#include "gallery.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "raymath.h"
#include "ui/theme.h"

#define ROW_GAP 8.0f  // separacion entre filas (categorias)
#define ITEM_GAP 2.0f // separacion entre objetos de una fila

static Color category_color(const char *cat) {
    static const struct {
        const char *cat;
        Color c;
    } COLORS[] = {
        { "mapa", { 120, 140, 90, 255 } },      { "estructura", { 200, 180, 140, 255 } },
        { "vehiculo", { 150, 100, 60, 255 } },  { "animal", { 170, 120, 80, 255 } },
        { "arma", { 180, 180, 190, 255 } },     { "proyectil", { 150, 150, 160, 255 } },
        { "escudo", { 140, 90, 50, 255 } },     { "totem", { 120, 70, 40, 255 } },
        { "armadura", { 110, 110, 120, 255 } }, { "vestimenta", { 90, 110, 150, 255 } },
        { "accesorio", { 212, 166, 72, 255 } }, { "asedio", { 100, 70, 50, 255 } },
        { "utileria", { 160, 130, 100, 255 } }, { "personaje", { 190, 140, 110, 255 } },
    };
    for (size_t i = 0; i < sizeof(COLORS) / sizeof(COLORS[0]); i++)
        if (!strcmp(COLORS[i].cat, cat)) return COLORS[i].c;
    return GRAY;
}

bool gallery_init(Gallery *g, const Terrain *t, Vector3 origin) {
    memset(g, 0, sizeof(*g));
    char *text = LoadFileText(platform_asset_path("assets/inventario.tsv"));
    if (!text) return false;
    inventory_parse(&g->inv, text);
    UnloadFileText(text);
    if (!g->inv.count) return false;
    g->entries = calloc((size_t)g->inv.count, sizeof(*g->entries));
    if (!g->entries) return false;

    // Una fila por categoria, a lo largo de +X; las filas se alejan hacia -Z.
    float x = origin.x, z = origin.z, row_depth = 0.0f;
    const char *cat = NULL;
    for (int i = 0; i < g->inv.count; i++) {
        const InvItem *it = &g->inv.items[i];
        if (cat && strcmp(cat, it->category)) {
            z -= row_depth + ROW_GAP;
            x = origin.x;
            row_depth = 0.0f;
        }
        cat = it->category;
        float footprint = fmaxf(it->l, 0.4f);
        x += footprint * 0.5f;
        GalleryEntry *e = &g->entries[g->count++];
        e->item = it;
        e->pos = (Vector3){ x, terrain_height(t, x, z), z };
        x += footprint * 0.5f + ITEM_GAP;
        row_depth = fmaxf(row_depth, it->w);

        char path[128];
        inventory_path(it, path, sizeof(path));
        const char *full = platform_asset_path(path);
        if (!it->texture && FileExists(full)) {
            e->model = LoadModel(full);
            e->loaded = e->model.meshCount > 0;
            if (e->loaded) g->loaded++;
        }
    }
    g->start = (Vector3){ origin.x + 6.0f, 0.0f, origin.z + 10.0f };
    return true;
}

void gallery_draw(const Gallery *g) {
    for (int i = 0; i < g->count; i++) {
        const GalleryEntry *e = &g->entries[i];
        const InvItem *it = e->item;
        if (e->loaded) {
            DrawModel(e->model, e->pos, 1.0f, WHITE);
            continue;
        }
        // Marcador: caja con las medidas del inventario (largo en X, ancho en Z).
        Color c = category_color(it->category);
        float h = it->texture || it->h <= 0.0f ? 0.03f : it->h;
        Vector3 size = { fmaxf(it->l, 0.05f), h, fmaxf(it->w, 0.05f) };
        Vector3 center = { e->pos.x, e->pos.y + h * 0.5f, e->pos.z };
        DrawCubeV(center, size, c);
        DrawCubeWiresV(center, size, ColorBrightness(c, -0.45f));
        // Flecha de frente (+X): la direccion en la que debe mirar el modelo.
        Vector3 front = { e->pos.x + size.x * 0.5f, e->pos.y + h * 0.5f, e->pos.z };
        DrawLine3D(front, Vector3Add(front, (Vector3){ 0.4f, 0, 0 }), UI_CARNELIAN);
    }
}

void gallery_draw_labels(const Gallery *g, Camera3D cam, Vector3 viewer, int width, int height) {
    const GalleryEntry *nearest = NULL;
    float best = 1e9f;
    for (int i = 0; i < g->count; i++) {
        const GalleryEntry *e = &g->entries[i];
        float d = Vector3Distance(viewer, e->pos);
        if (d < best) {
            best = d;
            nearest = e;
        }
        if (d > 14.0f) continue;
        Vector3 top = { e->pos.x, e->pos.y + fmaxf(e->item->h, 0.2f) + 0.3f, e->pos.z };
        Vector3 to = Vector3Subtract(top, cam.position), fwd = Vector3Subtract(cam.target, cam.position);
        if (Vector3DotProduct(to, fwd) <= 0.0f) continue; // detras de la camara
        Vector2 s = GetWorldToScreenEx(top, cam, width, height);
        const char *label = strchr(e->item->id, '.') + 1; // subcategoria.nombre
        int w = MeasureText(label, 10);
        DrawRectangle((int)s.x - w / 2 - 2, (int)s.y - 1, w + 4, 11, (Color){ 16, 15, 18, 170 });
        DrawText(label, (int)s.x - w / 2, (int)s.y, 10, e->loaded ? UI_TURQUOISE : UI_BONE_DIM);
    }
    // Ficha del objeto mas cercano.
    ui_strip((Rectangle){ 0, 0, (float)width, 30 }, UI_METAL_GOLD);
    ui_text(TextFormat("GALERIA  %d objetos  |  %d con modelo  |  %d pendientes", g->count, g->loaded,
                       g->count - g->loaded), 6, 7, 10, UI_GOLD_LIGHT);
    if (nearest && best < 20.0f) {
        const InvItem *it = nearest->item;
        ui_text(TextFormat("%s  -  %s  -  %.2gx%.2gx%.2g m  -  %s", it->id, it->name, it->w, it->h, it->l,
                           nearest->loaded ? "modelo cargado" : inventory_state_name(it->state)),
                6, 18, 10, nearest->loaded ? UI_TURQUOISE : UI_BONE);
    }
}

void gallery_unload(Gallery *g) {
    for (int i = 0; i < g->count; i++)
        if (g->entries[i].loaded) UnloadModel(g->entries[i].model);
    free(g->entries);
    inventory_free(&g->inv);
    memset(g, 0, sizeof(*g));
}
