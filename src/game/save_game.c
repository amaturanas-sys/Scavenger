#include "game/save_game.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "game/death_game.h"
#include "game/hud_game.h"
#include "platform.h"
#include "sim/clock.h"
#include "sim/lang.h"
#include "sim/save_format.h"

#include "game/save_schema.inc" // el esquema de hoy (tools/save/esquema.py)
#include "game/save_v1.inc"     // el de las partidas planas (hasta la v0.4.3), congelado

// La minifoto de un hueco: se lee con fopen (en Android, raylib manda las lecturas al
// AssetManager y no veria el almacenamiento interno) y se decodifica desde memoria.
static Texture2D load_thumb(const char *path) {
    Texture2D t = { 0 };
    FILE *f = fopen(path, "rb");
    if (!f) return t;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = n > 0 && n < 8 * 1024 * 1024 ? malloc((size_t)n) : NULL;
    if (buf && fread(buf, 1, (size_t)n, f) == (size_t)n) {
        Image img = LoadImageFromMemory(".png", buf, (int)n);
        if (img.data) t = LoadTextureFromImage(img), UnloadImage(img);
    }
    free(buf);
    fclose(f);
    return t;
}

#define SAVE_MAGIC 0x50545345u     // "ESTP"
#define SAVE_FLAT 1u               // formato 1: los structs uno tras otro (hasta la v0.4.3)
#define SAVE_BLOCKS 2u             // formato 2: bloques con su version y su esquema
#define V1_DEATH_MAGIC 0x31544744u // "DGT1": en el formato 1, descanso y restos (v0.4.0)
#define V1_QUICK_MAGIC 0x31524251u // "QBR1": en el formato 1, la barra rapida (v0.4.0)

// El encabezado, igual en los dos formatos: la lista de huecos lo lee sin cargar la partida.
typedef struct {
    unsigned magic, version, layout; // layout: huella de los tamaños (solo en el formato 1)
    long long saved_at;
    int day, tribe;
    float world_time;
    char place[48];
} SaveHeader;

// Los bloques. Lo que cambia la forma de un struct (campos agregados, quitados o movidos,
// arreglos de otro tamaño) lo resuelve el esquema solo. La version de un bloque sube cuando un
// cambio necesita migrar los datos viejos (otro significado, otras unidades, un valor por defecto
// que no es cero): ver migrate(). Un bloque que falta arranca de cero; los obligatorios solo
// faltan si el archivo esta dañado.
typedef enum {
    B_MISC,
    B_PLAYER,
    B_KINGDOM,
    B_TROOP,
    B_ACTIONS,
    B_COMBAT,
    B_HAZARDS,
    B_DISASTERS,
    B_PROPS,
    B_MEMORY,
    B_MARKERS,
    B_DEATH,
    B_QUICKBAR,
    B_COUNT
} BlockId;

static const struct {
    char tag[5];
    uint32_t version;
    bool required;
} BLOCKS[B_COUNT] = {
    [B_MISC] = { "PART", 1, true },    [B_PLAYER] = { "JUGA", 1, true },   [B_KINGDOM] = { "REIN", 1, true },
    [B_TROOP] = { "TROP", 1, true },   [B_ACTIONS] = { "ACCI", 1, true },  [B_COMBAT] = { "COMB", 1, true },
    [B_HAZARDS] = { "PELI", 1, true }, [B_DISASTERS] = { "DESA", 1, true }, [B_PROPS] = { "OBJE", 1, false },
    [B_MEMORY] = { "MEMO", 1, false }, [B_MARKERS] = { "MARC", 1, false }, [B_DEATH] = { "MUER", 1, false },
    [B_QUICKBAR] = { "BARR", 1, false },
};

#if defined(_WIN32)
// En Windows, rename no pisa un archivo que ya existe: MoveFileEx si (declarada a mano, porque
// windows.h choca con los nombres de raylib).
__declspec(dllimport) int __stdcall MoveFileExA(const char *from, const char *to, unsigned long flags);
static int replace_file(const char *from, const char *to) { return MoveFileExA(from, to, 0x1u /* MOVEFILE_REPLACE_EXISTING */) ? 0 : -1; }
#else
static int replace_file(const char *from, const char *to) { return rename(from, to); }
#endif

const char *save_path(int slot, bool thumb) {
    static char buf[2][600];
    char *b = buf[thumb];
    snprintf(b, sizeof(buf[0]), "%s/partida_%d.%s", platform_save_dir(), slot + 1, thumb ? "png" : "sav");
    return b;
}

static void place_text(float world_time, char *out, size_t len) {
    int day = clock_day(world_time);
    snprintf(out, len, "%s, %s", season_name(clock_season(day)), phase_name(clock_phase(world_time)));
}

// ---------------------------------------------------------------- guardar
bool save_write_file(const char *path, const GameState *g, long long saved_at, char *err, size_t len) {
    char tmp[1300];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path); // se escribe aparte y se renombra: nunca queda a medias
    FILE *f = fopen(tmp, "wb");
    if (!f) {
        snprintf(err, len, T("No se pudo escribir en %s."), platform_save_dir());
        return false;
    }
    SaveHeader h = { SAVE_MAGIC, SAVE_BLOCKS, 0, saved_at, *g->day, troop_count_with_status(g->troop, STATUS_ACTIVE), *g->world_time, "" };
    place_text(*g->world_time, h.place, sizeof(h.place));
    SfWriter w;
    sf_write_begin(&w, fwrite(&h, sizeof(h), 1, f) == 1 ? f : NULL);
    SavedMisc misc = { *g->last_champion, *g->cam_yaw, *g->rng };
    sf_block(&w, BLOCKS[B_MISC].tag, BLOCKS[B_MISC].version, &SF_T_SavedMisc, &misc, 1);
    sf_block(&w, BLOCKS[B_PLAYER].tag, BLOCKS[B_PLAYER].version, &SF_T_Player, g->player, 1);
    sf_block(&w, BLOCKS[B_KINGDOM].tag, BLOCKS[B_KINGDOM].version, &SF_T_Kingdom, g->overlord, 1);
    sf_block(&w, BLOCKS[B_TROOP].tag, BLOCKS[B_TROOP].version, &SF_T_Troop, g->troop, 1);
    sf_block(&w, BLOCKS[B_ACTIONS].tag, BLOCKS[B_ACTIONS].version, &SF_T_GameActions, g->ga, 1);
    sf_block(&w, BLOCKS[B_COMBAT].tag, BLOCKS[B_COMBAT].version, &SF_T_Combat, g->cb, 1);
    sf_block(&w, BLOCKS[B_HAZARDS].tag, BLOCKS[B_HAZARDS].version, &SF_T_Hazards, g->hz, 1);
    sf_block(&w, BLOCKS[B_DISASTERS].tag, BLOCKS[B_DISASTERS].version, &SF_T_Disasters, g->dz, 1);
    // Objetos del mundo: por id (los punteros al inventario no se guardan).
    sf_block_begin(&w, BLOCKS[B_PROPS].tag, BLOCKS[B_PROPS].version, &SF_T_SavedProp);
    for (int i = 0; i < g->props->count; i++) {
        const Prop *pr = &g->props->items[i];
        SavedProp sp;
        memset(&sp, 0, sizeof(sp));
        snprintf(sp.id, sizeof(sp.id), "%s", pr->item->id);
        sp.pos = pr->pos, sp.yaw = pr->yaw, sp.condition = pr->condition;
        sf_put(&w, &sp);
    }
    sf_block_end(&w);
    // Mapa de memoria: las paginas recorridas y las marcas.
    sf_block_begin(&w, BLOCKS[B_MEMORY].tag, BLOCKS[B_MEMORY].version, &SF_T_MemoryPage);
    for (int i = 0; i < g->mem->capacity; i++)
        if (memmap_page_slot(g->mem, i)) sf_put(&w, memmap_page_slot(g->mem, i));
    sf_block_end(&w);
    sf_block(&w, BLOCKS[B_MARKERS].tag, BLOCKS[B_MARKERS].version, &SF_T_MapMarker, g->mem->markers, g->mem->marker_count);
    sf_block(&w, BLOCKS[B_DEATH].tag, BLOCKS[B_DEATH].version, &SF_T_DeathSave, dg_state(), 1);
    sf_block(&w, BLOCKS[B_QUICKBAR].tag, BLOCKS[B_QUICKBAR].version, &SF_T_QbSlot, hud_slots(), HUD_QUICK_SLOTS);
    bool ok = sf_write_end(&w);
    if (fclose(f) != 0) ok = false;
    if (!ok || replace_file(tmp, path) != 0) {
        remove(tmp);
        snprintf(err, len, "%s", T("No se pudo guardar la partida (disco lleno o sin permiso)."));
        return false;
    }
    return true;
}

bool save_write(int slot, const GameState *g, Image thumb, char *err, size_t len) {
    if (!save_write_file(save_path(slot, false), g, (long long)time(NULL), err, len)) return false;
    if (thumb.data) {
        Image t = ImageCopy(thumb);
        ImageResize(&t, SAVE_THUMB_W, SAVE_THUMB_H);
        // A memoria y con fopen: ExportImage en Android escribiria en una ruta doble.
        int n = 0;
        unsigned char *png = ExportImageToMemory(t, ".png", &n);
        FILE *f = png ? fopen(save_path(slot, true), "wb") : NULL;
        if (f) fwrite(png, 1, (size_t)n, f), fclose(f);
        MemFree(png);
        UnloadImage(t);
    }
    return true;
}

// ---------------------------------------------------------------- cargar
// Lo leido, aparte: solo si todo esta bien se pisa el estado del juego.
typedef struct {
    SavedMisc misc;
    Player player;
    Kingdom overlord;
    Troop troop;
    GameActions ga;
    Combat cb;
    Hazards hz;
    Disasters dz;
    SavedProp *props;
    int nprops;
    MemoryPage *pages;
    int npages;
    MapMarker markers[MEMMAP_MAX_MARKERS];
    int nmarkers;
    DeathSave death;
    bool has_death;
    QbSlot slots[HUD_QUICK_SLOTS];
    int nslots;
    uint32_t version[B_COUNT]; // la version de cada bloque leido (0: no estaba)
} Loaded;

typedef enum { LOAD_OK, LOAD_DAMAGED, LOAD_OTHER, LOAD_NEWER, LOAD_MEMORY } LoadResult;

static const char *load_error(LoadResult r) {
    switch (r) {
    case LOAD_DAMAGED: return T("La partida está dañada (archivo incompleto).");
    case LOAD_NEWER: return T("La partida es de una versión más nueva del juego.");
    case LOAD_MEMORY: return T("Sin memoria para cargar la partida.");
    default: return T("La partida es de otra versión del juego y no se puede cargar.");
    }
}

// Un arreglo de un bloque, del largo que traiga (malloc en *out). -1 si esta dañado.
static int read_array(const SfBlock *b, const SfType *type, size_t elem, void **out) {
    int n = sf_count(b);
    if (n < 0) return -1;
    *out = calloc((size_t)(n > 0 ? n : 1), elem);
    if (!*out) return -2;
    return sf_read(b, type, *out, n);
}

static LoadResult read_blocks(const unsigned char *p, size_t n, Loaded *L) {
    SfFile file;
    if (!sf_parse(&file, p, n)) return LOAD_DAMAGED;
    const SfBlock *b[B_COUNT];
    for (int i = 0; i < B_COUNT; i++) {
        b[i] = sf_find(&file, BLOCKS[i].tag);
        if (b[i] && b[i]->version > BLOCKS[i].version) return LOAD_NEWER;
        if (!b[i] && BLOCKS[i].required) return LOAD_DAMAGED;
        L->version[i] = b[i] ? b[i]->version : 0;
    }
    const struct {
        BlockId id;
        const SfType *type;
        void *dst;
    } one[] = {
        { B_MISC, &SF_T_SavedMisc, &L->misc }, { B_PLAYER, &SF_T_Player, &L->player },   { B_KINGDOM, &SF_T_Kingdom, &L->overlord },
        { B_TROOP, &SF_T_Troop, &L->troop },   { B_ACTIONS, &SF_T_GameActions, &L->ga }, { B_COMBAT, &SF_T_Combat, &L->cb },
        { B_HAZARDS, &SF_T_Hazards, &L->hz },  { B_DISASTERS, &SF_T_Disasters, &L->dz },
    };
    for (size_t i = 0; i < sizeof(one) / sizeof(one[0]); i++)
        if (sf_read(b[one[i].id], one[i].type, one[i].dst, 1) != 1) return LOAD_DAMAGED;
    if (b[B_PROPS]) {
        void *arr = NULL;
        L->nprops = read_array(b[B_PROPS], &SF_T_SavedProp, sizeof(SavedProp), &arr);
        L->props = arr;
        if (L->nprops < 0) return L->nprops == -2 ? LOAD_MEMORY : LOAD_DAMAGED;
    }
    if (b[B_MEMORY]) {
        void *arr = NULL;
        L->npages = read_array(b[B_MEMORY], &SF_T_MemoryPage, sizeof(MemoryPage), &arr);
        L->pages = arr;
        if (L->npages < 0) return L->npages == -2 ? LOAD_MEMORY : LOAD_DAMAGED;
    }
    if (b[B_MARKERS]) {
        L->nmarkers = sf_read(b[B_MARKERS], &SF_T_MapMarker, L->markers, MEMMAP_MAX_MARKERS);
        if (L->nmarkers < 0) return LOAD_DAMAGED;
        if (L->nmarkers > MEMMAP_MAX_MARKERS) L->nmarkers = MEMMAP_MAX_MARKERS;
    }
    if (b[B_DEATH]) {
        if (sf_read(b[B_DEATH], &SF_T_DeathSave, &L->death, 1) != 1) return LOAD_DAMAGED;
        L->has_death = true;
    }
    if (b[B_QUICKBAR]) {
        L->nslots = sf_read(b[B_QUICKBAR], &SF_T_QbSlot, L->slots, HUD_QUICK_SLOTS);
        if (L->nslots < 0) return LOAD_DAMAGED;
        if (L->nslots > HUD_QUICK_SLOTS) L->nslots = HUD_QUICK_SLOTS;
    }
    return LOAD_OK;
}

// Las partidas planas (formato 1, hasta la v0.4.3): los structs uno tras otro, tal como estaban en
// memoria. Se leen con el esquema congelado de entonces, campo por campo como cualquier bloque.
// Si la huella de los tamaños no es la del esquema congelado pero si la de este build (seria otra
// plataforma con otra disposicion), se leen con el esquema de este build.
typedef struct {
    const SfType *rng, *player, *kingdom, *troop, *ga, *cb, *hz, *dz, *prop, *page, *marker, *death, *slot;
    int markers, slots; // las capacidades de entonces
} FlatLayout;

static const FlatLayout FLAT_V1 = { &V1_T_Rng,         &V1_T_Player,   &V1_T_Kingdom,   &V1_T_Troop,     &V1_T_GameActions,
                                    &V1_T_Combat,      &V1_T_Hazards,  &V1_T_Disasters, &V1_T_SavedProp, &V1_T_MemoryPage,
                                    &V1_T_MapMarker,   &V1_T_DeathSave, &V1_T_QbSlot,   V1_MAX_MARKERS,  V1_QUICK_SLOTS };
static const FlatLayout FLAT_LIVE = { &SF_T_Rng,       &SF_T_Player,    &SF_T_Kingdom,   &SF_T_Troop,     &SF_T_GameActions,
                                      &SF_T_Combat,    &SF_T_Hazards,   &SF_T_Disasters, &SF_T_SavedProp, &SF_T_MemoryPage,
                                      &SF_T_MapMarker, &SF_T_DeathSave, &SF_T_QbSlot,    MEMMAP_MAX_MARKERS, HUD_QUICK_SLOTS };

// La huella que llevaba el encabezado: los tamaños de lo que se guardaba.
static unsigned flat_hash(const FlatLayout *l) {
    unsigned sizes[] = { l->player->size, l->kingdom->size, l->troop->size,  l->rng->size,    l->ga->size,  l->cb->size,
                         l->hz->size,     l->dz->size,      l->page->size,   l->marker->size, l->prop->size };
    unsigned h = 2166136261u;
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) h = (h ^ sizes[i]) * 16777619u;
    return h;
}

static const FlatLayout *flat_layout(unsigned layout) {
    return layout == flat_hash(&FLAT_V1) ? &FLAT_V1 : layout == flat_hash(&FLAT_LIVE) ? &FLAT_LIVE : NULL;
}

typedef struct {
    const unsigned char *p, *end;
} Flat;

static const unsigned char *take(Flat *f, size_t n) {
    if ((size_t)(f->end - f->p) < n) return NULL;
    const unsigned char *r = f->p;
    f->p += n;
    return r;
}

static bool take_value(Flat *f, void *dst, size_t n) {
    const unsigned char *s = take(f, n);
    if (s) memcpy(dst, s, n);
    return s != NULL;
}

static bool take_struct(Flat *f, const SfType *old, const SfType *live, void *dst) {
    const unsigned char *s = take(f, old->size);
    if (s) sf_convert(old, s, 1, live, dst);
    return s != NULL;
}

// count elementos de un arreglo (malloc en *out). false si no estan todos.
static bool take_array(Flat *f, int count, const SfType *old, const SfType *live, size_t elem, void **out) {
    if (count < 0 || (size_t)count > (size_t)(f->end - f->p) / old->size) return false;
    *out = calloc((size_t)(count > 0 ? count : 1), elem);
    if (*out) sf_convert(old, take(f, (size_t)count * old->size), count, live, *out);
    return *out != NULL;
}

static LoadResult read_flat(const unsigned char *p, size_t n, unsigned layout, Loaded *L) {
    const FlatLayout *fl = flat_layout(layout);
    if (!fl) return LOAD_OTHER;
    Flat f = { p, p + n };
    bool ok = take_value(&f, &L->misc.last_champion, sizeof(int)) && take_value(&f, &L->misc.cam_yaw, sizeof(float));
    ok = ok && take_struct(&f, fl->rng, &SF_T_Rng, &L->misc.rng) && take_struct(&f, fl->player, &SF_T_Player, &L->player);
    ok = ok && take_struct(&f, fl->kingdom, &SF_T_Kingdom, &L->overlord) && take_struct(&f, fl->troop, &SF_T_Troop, &L->troop);
    ok = ok && take_struct(&f, fl->ga, &SF_T_GameActions, &L->ga) && take_struct(&f, fl->cb, &SF_T_Combat, &L->cb);
    ok = ok && take_struct(&f, fl->hz, &SF_T_Hazards, &L->hz) && take_struct(&f, fl->dz, &SF_T_Disasters, &L->dz);
    void *arr = NULL;
    ok = ok && take_value(&f, &L->nprops, sizeof(int)) && take_array(&f, L->nprops, fl->prop, &SF_T_SavedProp, sizeof(SavedProp), &arr);
    L->props = arr;
    arr = NULL;
    ok = ok && take_value(&f, &L->npages, sizeof(int)) && take_array(&f, L->npages, fl->page, &SF_T_MemoryPage, sizeof(MemoryPage), &arr);
    L->pages = arr;
    const unsigned char *s = NULL;
    ok = ok && take_value(&f, &L->nmarkers, sizeof(int)) && L->nmarkers >= 0 && L->nmarkers <= fl->markers;
    ok = ok && (s = take(&f, (size_t)fl->markers * fl->marker->size)) != NULL;
    if (!ok) return LOAD_DAMAGED;
    if (L->nmarkers > MEMMAP_MAX_MARKERS) L->nmarkers = MEMMAP_MAX_MARKERS;
    sf_convert(fl->marker, s, L->nmarkers, &SF_T_MapMarker, L->markers);
    // Al final, opcionales (desde la v0.4.0): descanso y restos, y la barra rapida.
    uint32_t magic = 0;
    if (take_value(&f, &magic, 4) && magic == V1_DEATH_MAGIC && take_struct(&f, fl->death, &SF_T_DeathSave, &L->death)) L->has_death = true;
    if (take_value(&f, &magic, 4) && magic == V1_QUICK_MAGIC && (s = take(&f, (size_t)fl->slots * fl->slot->size)) != NULL) {
        L->nslots = fl->slots < HUD_QUICK_SLOTS ? fl->slots : HUD_QUICK_SLOTS;
        sf_convert(fl->slot, s, L->nslots, &SF_T_QbSlot, L->slots);
    }
    for (int i = 0; i < B_COUNT; i++) L->version[i] = 1; // lo plano es lo mismo que la version 1 de cada bloque
    return LOAD_OK;
}

// Los datos viejos, al dia: lo que el esquema no resuelve solo. Cuando un cambio lo necesite, subir
// la version del bloque en BLOCKS y agregar aca su caso; p. ej.:
//   if (L->version[B_COMBAT] < 2) L->cb.lock_target = -1; // el objetivo fijado (v0.5.0)
static void migrate(Loaded *L) { (void)L; }

// Al estado del juego, rehaciendo los punteros (no se guardan).
static void apply(const Loaded *L, const SaveHeader *h, GameState *g) {
    *g->world_time = h->world_time;
    *g->day = h->day;
    *g->last_champion = L->misc.last_champion;
    *g->cam_yaw = L->misc.cam_yaw;
    *g->rng = L->misc.rng;
    *g->player = L->player;
    *g->overlord = L->overlord;
    *g->troop = L->troop;
    g->troop->overlord = g->overlord;
    Armor *armor = g->ga->player_armor;
    const Terrain *terrain = g->ga->terrain;
    Troop *troop_ref = g->ga->troop_ref;
    *g->ga = L->ga;
    g->ga->inv = g->inv, g->ga->player_armor = armor, g->ga->terrain = terrain, g->ga->troop_ref = troop_ref;
    *g->cb = L->cb;
    Health *body = g->hz->body;
    *g->hz = L->hz;
    g->hz->body = body;
    *g->dz = L->dz;
    g->props->count = 0;
    for (int i = 0; i < L->nprops; i++) {
        char id[INV_ID_LEN];
        snprintf(id, sizeof(id), "%.*s", INV_ID_LEN - 1, L->props[i].id);
        int k = props_add(g->props, id, L->props[i].pos, L->props[i].yaw);
        if (k >= 0) g->props->items[k].condition = L->props[i].condition;
    }
    memmap_free(g->mem);
    memmap_init(g->mem);
    for (int i = 0; i < L->npages; i++) memmap_restore_page(g->mem, &L->pages[i]);
    memcpy(g->mem->markers, L->markers, sizeof(L->markers));
    g->mem->marker_count = L->nmarkers;
    dg_load(L->has_death ? &L->death : NULL); // las partidas que no lo traen: sin restos
    hud_load(L->nslots ? L->slots : NULL, L->nslots); // y la barra de serie
}

static unsigned char *read_whole(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = n >= 0 ? malloc((size_t)n + 1) : NULL;
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) free(buf), buf = NULL;
    fclose(f);
    *len = buf ? (size_t)n : 0;
    return buf;
}

bool save_read_file(const char *path, GameState *g, char *err, size_t len) {
    size_t n = 0;
    unsigned char *buf = read_whole(path, &n);
    if (!buf) {
        snprintf(err, len, "%s", T("Ese hueco está vacío."));
        return false;
    }
    SaveHeader h;
    memset(&h, 0, sizeof(h));
    if (n >= sizeof(h)) memcpy(&h, buf, sizeof(h));
    Loaded *L = NULL;
    LoadResult r;
    if (n < sizeof(h) || h.magic != SAVE_MAGIC) r = LOAD_OTHER;
    else if (h.version > SAVE_BLOCKS) r = LOAD_NEWER;
    else if (!(L = calloc(1, sizeof(*L)))) r = LOAD_MEMORY;
    else if (h.version == SAVE_FLAT) r = read_flat(buf + sizeof(h), n - sizeof(h), h.layout, L);
    else if (h.version == SAVE_BLOCKS) r = read_blocks(buf + sizeof(h), n - sizeof(h), L);
    else r = LOAD_OTHER;
    if (r == LOAD_OK) {
        migrate(L);
        apply(L, &h, g);
    } else {
        snprintf(err, len, "%s", load_error(r));
    }
    if (L) free(L->props), free(L->pages);
    free(L);
    free(buf);
    return r == LOAD_OK;
}

bool save_read(int slot, GameState *g, char *err, size_t len) { return save_read_file(save_path(slot, false), g, err, len); }

// ---------------------------------------------------------------- huecos
// Los bloques de un archivo, sin leerlos: ninguno de una version mas nueva que las de este build.
static bool blocks_known(FILE *f) {
    unsigned char hd[12];
    while (fread(hd, 1, sizeof(hd), f) == sizeof(hd)) {
        uint32_t version = (uint32_t)hd[4] | (uint32_t)hd[5] << 8 | (uint32_t)hd[6] << 16 | (uint32_t)hd[7] << 24;
        uint32_t size = (uint32_t)hd[8] | (uint32_t)hd[9] << 8 | (uint32_t)hd[10] << 16 | (uint32_t)hd[11] << 24;
        if (!memcmp(hd, "FIN.", 4)) return true;
        for (int i = 0; i < B_COUNT; i++)
            if (!memcmp(hd, BLOCKS[i].tag, 4) && version > BLOCKS[i].version) return false;
        if (fseek(f, (long)size, SEEK_CUR) != 0) return false;
    }
    return false; // cortado
}

bool save_peek(const char *path, SaveInfo *si) {
    memset(si, 0, sizeof(*si));
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    SaveHeader h;
    if (fread(&h, sizeof(h), 1, f) == 1 && h.magic == SAVE_MAGIC) {
        si->used = true;
        si->compatible = h.version == SAVE_FLAT ? flat_layout(h.layout) != NULL : h.version == SAVE_BLOCKS && blocks_known(f);
        si->saved_at = h.saved_at;
        si->day = h.day;
        si->tribe = h.tribe;
        place_text(h.world_time, si->place, sizeof(si->place)); // en el idioma de ahora
    }
    fclose(f);
    return si->used;
}

void save_list(SaveInfo out[SAVE_SLOTS]) {
    for (int s = 0; s < SAVE_SLOTS; s++)
        if (save_peek(save_path(s, false), &out[s])) out[s].thumb = load_thumb(save_path(s, true));
}

void save_list_unload(SaveInfo out[SAVE_SLOTS]) {
    for (int s = 0; s < SAVE_SLOTS; s++)
        if (out[s].thumb.id) UnloadTexture(out[s].thumb), out[s].thumb.id = 0;
}

const char *save_date_text(long long when) {
    static const char *months[12] = { N_("ene"), N_("feb"), N_("mar"), N_("abr"), N_("may"), N_("jun"), N_("jul"), N_("ago"), N_("sep"), N_("oct"), N_("nov"), N_("dic") };
    static char buf[48];
    time_t t = (time_t)when;
    struct tm *tm = localtime(&t);
    if (!tm) return "?";
    snprintf(buf, sizeof(buf), "%d %s %d, %02d:%02d", tm->tm_mday, T(months[tm->tm_mon % 12]), tm->tm_year + 1900, tm->tm_hour,
             tm->tm_min);
    return buf;
}
