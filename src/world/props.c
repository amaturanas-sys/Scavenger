#include "props.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "raymath.h"
#include "rlgl.h"

void props_init(Props *p, const Inventory *inv) {
    memset(p, 0, sizeof(*p));
    p->inv = inv;
    p->season = -1;
}

void props_set_season(Props *p, int season) { p->season = season; }

const char *props_season_suffix(int season) {
    static const char *names[] = { "primavera", "verano", "otono", "invierno" };
    return season >= 0 && season < 4 ? names[season] : NULL;
}

void props_unload(Props *p) {
    for (int i = 0; i < p->cached; i++) {
        if (p->cache[i].anims) UnloadModelAnimations(p->cache[i].anims, p->cache[i].anim_count);
        if (p->cache[i].loaded) UnloadModel(p->cache[i].model);
    }
    p->cached = 0;
    p->count = 0;
}

int props_add(Props *p, const char *id, Vector3 pos, float yaw) {
    const InvItem *it = p->inv ? inventory_find(p->inv, id) : NULL;
    if (!it || p->count >= PROPS_MAX) return -1;
    p->items[p->count] = (Prop){ it, pos, yaw, false, { 0, 0, 0 }, 1.0f };
    return p->count++;
}

void props_remove(Props *p, int index) {
    if (index < 0 || index >= p->count) return;
    p->items[index] = p->items[--p->count];
}

bool props_takeable(const InvItem *item) { return fmaxf(fmaxf(item->w, item->h), item->l) <= 1.2f && !item->texture; }

int props_nearest(const Props *p, Vector3 from, float radius, bool takeable_only) {
    int best = -1;
    float best_d = radius;
    for (int i = 0; i < p->count; i++) {
        const Prop *pr = &p->items[i];
        if (pr->flying || (takeable_only && !props_takeable(pr->item))) continue;
        float d = Vector2Distance((Vector2){ from.x, from.z }, (Vector2){ pr->pos.x, pr->pos.z });
        if (d <= best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

// Indice en la cache del id y la variante (cargando el modelo y sus clips una sola vez), o -1.
static int load_slot(Props *p, const InvItem *item, int variant) {
    for (int i = 0; i < p->cached; i++)
        if (p->cache[i].item == item && p->cache[i].variant == variant) return i;
    if (p->cached >= MODEL_CACHE_MAX) return -1;
    char path[160];
    inventory_path(item, path, sizeof(path));
    if (variant >= 0) { // "dir/nombre.glb" -> "dir/nombre@invierno.glb"
        char *dot = strrchr(path, '.');
        if (!dot) return -1;
        char ext[8];
        snprintf(ext, sizeof(ext), "%s", dot);
        snprintf(dot, sizeof(path) - (size_t)(dot - path), "@%s%s", props_season_suffix(variant), ext);
    }
    const char *full = platform_asset_path(path);
    int slot = p->cached++;
    p->cache[slot].item = item;
    p->cache[slot].variant = variant;
    p->cache[slot].loaded = false;
    p->cache[slot].anims = NULL;
    p->cache[slot].anim_count = 0;
    if (item->texture) return slot;
    if (!platform_asset_exists(full)) return slot; // en Android, dentro del APK
    p->cache[slot].model = LoadModel(full);
    p->cache[slot].loaded = p->cache[slot].model.meshCount > 0;
    platform_note_asset(full, p->cache[slot].loaded);
    if (p->cache[slot].loaded) p->cache[slot].anims = LoadModelAnimations(full, &p->cache[slot].anim_count);
    return slot;
}

// Entrada de cache a usar: la variante de la estacion si existe, si no el modelo base.
static int cache_slot(Props *p, const InvItem *item) {
    if (props_season_suffix(p->season)) {
        int v = load_slot(p, item, p->season);
        if (v >= 0 && p->cache[v].loaded) return v;
    }
    return load_slot(p, item, -1);
}

// Modelo del id, cargado una sola vez. NULL si todavia no fue importado.
static Model *cached_model(Props *p, const InvItem *item) {
    int slot = cache_slot(p, item);
    return slot >= 0 && p->cache[slot].loaded ? &p->cache[slot].model : NULL;
}

bool props_has_model(Props *p, const InvItem *item) { return item && cached_model(p, item) != NULL; }

void props_draw_item_anim(Props *p, const InvItem *item, Vector3 pos, float yaw, const char *clip, float time) {
    Model *m = cached_model(p, item);
    int slot = cache_slot(p, item);
    if (m && slot >= 0 && p->cache[slot].anim_count > 0) {
        ModelAnimation *anims = p->cache[slot].anims, *chosen = NULL, *idle = NULL;
        for (int i = 0; i < p->cache[slot].anim_count; i++) {
            if (!strcmp(anims[i].name, clip)) chosen = &anims[i];
            if (!strcmp(anims[i].name, "idle")) idle = &anims[i];
        }
        if (!chosen) chosen = idle ? idle : &anims[0];
        if (chosen->frameCount > 0) {
            int frame = (int)(time * 30.0f) % chosen->frameCount; // los GLB se exportan a 30 fps
            UpdateModelAnimation(*m, *chosen, frame);
        }
    }
    props_draw_item(p, item, pos, yaw, 1.0f);
}

// Color del marcador segun la categoria del inventario (igual que en la galeria).
static Color category_color(const char *cat) {
    if (!strcmp(cat, "mapa")) return (Color){ 120, 140, 80, 255 };
    if (!strcmp(cat, "animal")) return (Color){ 160, 115, 75, 255 };
    if (!strcmp(cat, "utileria")) return (Color){ 165, 135, 100, 255 };
    if (!strcmp(cat, "totem")) return (Color){ 125, 75, 45, 255 };
    return (Color){ 190, 160, 120, 255 };
}

void props_draw_item(Props *p, const InvItem *item, Vector3 pos, float yaw, float grow) {
    Model *m = cached_model(p, item);
    if (m && grow >= 1.0f) {
        DrawModelEx(*m, pos, (Vector3){ 0, 1, 0 }, yaw * RAD2DEG, (Vector3){ 1, 1, 1 }, WHITE);
        return;
    }
    // Marcador: caja con las medidas del inventario (largo en X local, ancho en Z), girada con yaw.
    float h = (item->texture || item->h <= 0.0f ? 0.03f : item->h) * fmaxf(grow, 0.05f);
    Color c = grow < 1.0f ? (Color){ 150, 120, 80, 255 } : category_color(item->category);
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(yaw * RAD2DEG, 0, 1, 0);
    Vector3 size = { fmaxf(item->l, 0.05f), h, fmaxf(item->w, 0.05f) };
    Vector3 center = { 0, h * 0.5f, 0 };
    DrawCubeV(center, size, c);
    DrawCubeWiresV(center, size, ColorBrightness(c, -0.5f));
    if (grow < 1.0f) { // andamio: el volumen final, en alambre
        Vector3 full = { size.x, fmaxf(item->h, 0.05f), size.z };
        DrawCubeWiresV((Vector3){ 0, full.y * 0.5f, 0 }, full, (Color){ 90, 70, 50, 255 });
    }
    rlPopMatrix();
}

bool props_is_fire(const InvItem *item, float *scale) {
    if (!item) return false;
    bool bon = !strcmp(item->id, "estructura.campamento.hoguera");
    if (!bon && strcmp(item->id, "estructura.campamento.fogata")) return false;
    if (scale) *scale = bon ? 2.0f : 1.0f;
    return true;
}

void props_draw(Props *p) {
    for (int i = 0; i < p->count; i++) {
        const InvItem *it = p->items[i].item;
        if (props_is_fire(it, NULL) && !props_has_model(p, it)) continue; // los dibuja hearth.c
        props_draw_item(p, it, p->items[i].pos, p->items[i].yaw, 1.0f);
    }
}
