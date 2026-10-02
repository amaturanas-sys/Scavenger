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
}

void props_unload(Props *p) {
    for (int i = 0; i < p->cached; i++)
        if (p->cache[i].loaded) UnloadModel(p->cache[i].model);
    p->cached = 0;
    p->count = 0;
}

int props_add(Props *p, const char *id, Vector3 pos, float yaw) {
    const InvItem *it = p->inv ? inventory_find(p->inv, id) : NULL;
    if (!it || p->count >= PROPS_MAX) return -1;
    p->items[p->count] = (Prop){ it, pos, yaw, false, { 0, 0, 0 } };
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

// Modelo del id, cargado una sola vez. NULL si todavia no fue importado.
static Model *cached_model(Props *p, const InvItem *item) {
    for (int i = 0; i < p->cached; i++)
        if (p->cache[i].item == item) return p->cache[i].loaded ? &p->cache[i].model : NULL;
    if (p->cached >= MODEL_CACHE_MAX) return NULL;
    char path[128];
    inventory_path(item, path, sizeof(path));
    const char *full = platform_asset_path(path);
    int slot = p->cached++;
    p->cache[slot].item = item;
    p->cache[slot].loaded = false;
#if !defined(__ANDROID__)
    if (item->texture || !FileExists(full)) return NULL; // en Android los assets viven en el APK
#endif
    if (item->texture) return NULL;
    p->cache[slot].model = LoadModel(full);
    p->cache[slot].loaded = p->cache[slot].model.meshCount > 0;
    return p->cache[slot].loaded ? &p->cache[slot].model : NULL;
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

void props_draw(Props *p) {
    for (int i = 0; i < p->count; i++) props_draw_item(p, p->items[i].item, p->items[i].pos, p->items[i].yaw, 1.0f);
}
