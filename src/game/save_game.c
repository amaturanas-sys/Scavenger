#include "game/save_game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "game/death_game.h"
#include "game/hud_game.h"
#include "platform.h"
#include "sim/clock.h"
#include "sim/lang.h"

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

#define SAVE_MAGIC 0x50545345u // "ESTP"
#define SAVE_VERSION 1u

typedef struct {
    unsigned magic, version, layout;
    long long saved_at;
    int day, tribe;
    float world_time;
    char place[48];
} SaveHeader;

typedef struct {
    char id[INV_ID_LEN];
    Vector3 pos;
    float yaw, condition;
} SavedProp;

// Huella del formato: si cambia el tamaño de algo de lo que se guarda, otra version.
static unsigned layout_hash(void) {
    unsigned sizes[] = {
        (unsigned)sizeof(Player),  (unsigned)sizeof(Kingdom), (unsigned)sizeof(Troop),     (unsigned)sizeof(Rng),
        (unsigned)sizeof(GameActions), (unsigned)sizeof(Combat), (unsigned)sizeof(Hazards), (unsigned)sizeof(Disasters),
        (unsigned)sizeof(MemoryPage), (unsigned)sizeof(MapMarker), (unsigned)sizeof(SavedProp),
    };
    unsigned h = 2166136261u;
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) h = (h ^ sizes[i]) * 16777619u;
    return h;
}

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

static bool put(FILE *f, const void *p, size_t n) { return fwrite(p, 1, n, f) == n; }
static bool get(FILE *f, void *p, size_t n) { return fread(p, 1, n, f) == n; }

bool save_write(int slot, const GameState *g, Image thumb, char *err, size_t len) {
    const char *path = save_path(slot, false);
    char tmp[1300];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path); // se escribe aparte y se renombra: nunca queda a medias
    FILE *f = fopen(tmp, "wb");
    if (!f) {
        snprintf(err, len, T("No se pudo escribir en %s."), platform_save_dir());
        return false;
    }
    SaveHeader h = { SAVE_MAGIC, SAVE_VERSION, layout_hash(), (long long)time(NULL), *g->day,
                     troop_count_with_status(g->troop, STATUS_ACTIVE), *g->world_time, "" };
    place_text(*g->world_time, h.place, sizeof(h.place));
    bool ok = put(f, &h, sizeof(h));
    ok = ok && put(f, g->last_champion, sizeof(int)) && put(f, g->cam_yaw, sizeof(float)) && put(f, g->rng, sizeof(Rng));
    ok = ok && put(f, g->player, sizeof(Player)) && put(f, g->overlord, sizeof(Kingdom)) && put(f, g->troop, sizeof(Troop));
    ok = ok && put(f, g->ga, sizeof(GameActions)) && put(f, g->cb, sizeof(Combat)) && put(f, g->hz, sizeof(Hazards));
    ok = ok && put(f, g->dz, sizeof(Disasters));
    // Objetos del mundo: por id (los punteros al inventario no se guardan).
    int n = g->props->count;
    ok = ok && put(f, &n, sizeof(n));
    for (int i = 0; ok && i < n; i++) {
        const Prop *pr = &g->props->items[i];
        SavedProp sp;
        memset(&sp, 0, sizeof(sp));
        snprintf(sp.id, sizeof(sp.id), "%s", pr->item->id);
        sp.pos = pr->pos, sp.yaw = pr->yaw, sp.condition = pr->condition;
        ok = put(f, &sp, sizeof(sp));
    }
    // Mapa de memoria: las paginas recorridas y las marcas.
    int pages = 0;
    for (int i = 0; i < g->mem->capacity; i++) pages += memmap_page_slot(g->mem, i) != NULL;
    ok = ok && put(f, &pages, sizeof(pages));
    for (int i = 0; ok && i < g->mem->capacity; i++) {
        const MemoryPage *p = memmap_page_slot(g->mem, i);
        if (p) ok = put(f, p, sizeof(*p));
    }
    ok = ok && put(f, &g->mem->marker_count, sizeof(int)) && put(f, g->mem->markers, sizeof(g->mem->markers));
    ok = ok && dg_write(f) && hud_write(f); // bloques opcionales al final: descanso, restos y barra rapida
    if (fclose(f) != 0) ok = false;
    if (!ok || rename(tmp, path) != 0) {
        remove(tmp);
        snprintf(err, len, "%s", T("No se pudo guardar la partida (disco lleno o sin permiso)."));
        return false;
    }
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

static bool read_header(FILE *f, SaveHeader *h) { return get(f, h, sizeof(*h)) && h->magic == SAVE_MAGIC; }

bool save_read(int slot, GameState *g, char *err, size_t len) {
    FILE *f = fopen(save_path(slot, false), "rb");
    if (!f) {
        snprintf(err, len, "%s", T("Ese hueco está vacío."));
        return false;
    }
    SaveHeader h;
    if (!read_header(f, &h) || h.version != SAVE_VERSION || h.layout != layout_hash()) {
        fclose(f);
        snprintf(err, len, "%s", T("La partida es de otra versión del juego y no se puede cargar."));
        return false;
    }
    // Se lee todo aparte y solo si esta completo se pisa el estado.
    struct Blob {
        int last_champion;
        float cam_yaw;
        Rng rng;
        Player player;
        Kingdom overlord;
        Troop troop;
        GameActions ga;
        Combat cb;
        Hazards hz;
        Disasters dz;
    } *b = calloc(1, sizeof(struct Blob));
    if (!b) {
        fclose(f);
        snprintf(err, len, "%s", T("Sin memoria para cargar la partida."));
        return false;
    }
    bool ok = get(f, &b->last_champion, sizeof(int)) && get(f, &b->cam_yaw, sizeof(float)) && get(f, &b->rng, sizeof(Rng));
    ok = ok && get(f, &b->player, sizeof(Player)) && get(f, &b->overlord, sizeof(Kingdom)) && get(f, &b->troop, sizeof(Troop));
    ok = ok && get(f, &b->ga, sizeof(GameActions)) && get(f, &b->cb, sizeof(Combat)) && get(f, &b->hz, sizeof(Hazards));
    ok = ok && get(f, &b->dz, sizeof(Disasters));
    int n = 0;
    ok = ok && get(f, &n, sizeof(n)) && n >= 0 && n <= PROPS_MAX;
    SavedProp *props = ok ? calloc((size_t)(n > 0 ? n : 1), sizeof(SavedProp)) : NULL;
    for (int i = 0; ok && i < n; i++) ok = props && get(f, &props[i], sizeof(SavedProp));
    int pages = 0;
    ok = ok && get(f, &pages, sizeof(pages)) && pages >= 0 && pages < 1000000;
    MemoryPage *pg = ok ? calloc((size_t)(pages > 0 ? pages : 1), sizeof(MemoryPage)) : NULL;
    for (int i = 0; ok && i < pages; i++) ok = pg && get(f, &pg[i], sizeof(MemoryPage));
    int markers = 0;
    MapMarker mk[MEMMAP_MAX_MARKERS];
    ok = ok && get(f, &markers, sizeof(int)) && get(f, mk, sizeof(mk)) && markers >= 0 && markers <= MEMMAP_MAX_MARKERS;
    if (ok) dg_read(f), hud_read(f); // las partidas de antes no los traen: sin restos, barra de serie
    fclose(f);
    if (!ok) {
        free(b), free(props), free(pg);
        snprintf(err, len, "%s", T("La partida está dañada (archivo incompleto)."));
        return false;
    }
    // Completo: al estado, rehaciendo los punteros.
    *g->world_time = h.world_time;
    *g->day = h.day;
    *g->last_champion = b->last_champion;
    *g->cam_yaw = b->cam_yaw;
    *g->rng = b->rng;
    *g->player = b->player;
    *g->overlord = b->overlord;
    *g->troop = b->troop;
    g->troop->overlord = g->overlord;
    *g->ga = b->ga;
    g->ga->inv = g->inv;
    *g->cb = b->cb;
    Health *body = g->hz->body;
    *g->hz = b->hz;
    g->hz->body = body;
    *g->dz = b->dz;
    g->props->count = 0;
    for (int i = 0; i < n; i++) {
        int k = props_add(g->props, props[i].id, props[i].pos, props[i].yaw);
        if (k >= 0) g->props->items[k].condition = props[i].condition;
    }
    memmap_free(g->mem);
    memmap_init(g->mem);
    for (int i = 0; i < pages; i++) memmap_restore_page(g->mem, &pg[i]);
    memcpy(g->mem->markers, mk, sizeof(mk));
    g->mem->marker_count = markers;
    free(b), free(props), free(pg);
    return true;
}

void save_list(SaveInfo out[SAVE_SLOTS]) {
    for (int s = 0; s < SAVE_SLOTS; s++) {
        SaveInfo *si = &out[s];
        memset(si, 0, sizeof(*si));
        FILE *f = fopen(save_path(s, false), "rb");
        if (!f) continue;
        SaveHeader h;
        if (read_header(f, &h)) {
            si->used = true;
            si->compatible = h.version == SAVE_VERSION && h.layout == layout_hash();
            si->saved_at = h.saved_at;
            si->day = h.day;
            si->tribe = h.tribe;
            place_text(h.world_time, si->place, sizeof(si->place)); // en el idioma de ahora
        }
        fclose(f);
        if (si->used) si->thumb = load_thumb(save_path(s, true));
    }
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
