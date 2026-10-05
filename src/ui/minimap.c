#include "ui/minimap.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ui/theme.h"

#define MINIMAP_CACHE 128      // celdas por lado de la cache (512 m)
#define MINIMAP_CELL 4.0f      // m por celda (las del mapa de memoria)
#define MINIMAP_BUDGET 300     // celdas nuevas por cuadro (~1,4 us cada una): el mundo se consulta de a poco

void minimap_set_world(Minimap *mm, const World *w, float plain) {
    if (mm->world == w && mm->cache) return;
    mm->world = w;
    mm->plain = plain;
    if (!mm->cache) mm->cache = calloc(MINIMAP_CACHE * MINIMAP_CACHE, sizeof(MinimapCell));
    else memset(mm->cache, 0, MINIMAP_CACHE * MINIMAP_CACHE * sizeof(MinimapCell));
}

// El color de una celda: la mezcla de las regiones, el agua, la nieve de lo alto.
static void sample_cell(const Minimap *mm, MinimapCell *c, int cx, int cz) {
    static const Color REGION_COL[REGION_COUNT] = {
        { 150, 146, 84, 255 },  // estepa: pasto seco
        { 58, 92, 56, 255 },    // bosque de coniferas
        { 170, 178, 184, 255 }, // altiplano glaciar: roca y hielo
        { 96, 116, 98, 255 },   // fiordos: musgo y roca
        { 206, 172, 112, 255 }, // desierto
    };
    const World *w = mm->world;
    float x = ((float)cx + 0.5f) * MINIMAP_CELL, z = ((float)cz + 0.5f) * MINIMAP_CELL;
    float k[REGION_COUNT], r = 0, g = 0, b = 0;
    world_region_weights(w, x, z, k);
    for (int i = 0; i < REGION_COUNT; i++) r += REGION_COL[i].r * k[i], g += REGION_COL[i].g * k[i], b += REGION_COL[i].b * k[i];
    float h = world_height(w, x, z);
    WaterKind wk;
    float lv = world_water(w, x, z, 0.0f, &wk);
    c->cx = cx, c->cz = cz, c->h = h, c->ready = true, c->water = wk != WATER_NONE && lv > h;
    if (c->water) {
        c->c = wk == WATER_SEA ? (Color){ 38, 66, 104, 255 } : k[REGION_HIGHLAND] > 0.5f ? (Color){ 78, 158, 168, 255 }
               : wk == WATER_LAKE ? (Color){ 56, 102, 140, 255 } : (Color){ 70, 124, 168, 255 };
        return;
    }
    float snow = fminf(1.0f, fmaxf(0.0f, (h - mm->plain - 40.0f) / 25.0f)); // las cumbres, blancas
    c->c = (Color){ (unsigned char)(r + (236 - r) * snow), (unsigned char)(g + (240 - g) * snow), (unsigned char)(b + (244 - b) * snow), 255 };
}

static MinimapCell *cell_at(Minimap *mm, int cx, int cz, int *budget) {
    MinimapCell *c = &mm->cache[(cz & (MINIMAP_CACHE - 1)) * MINIMAP_CACHE + (cx & (MINIMAP_CACHE - 1))];
    if (c->ready && c->cx == cx && c->cz == cz) return c;
    if (*budget <= 0) return NULL;
    (*budget)--;
    sample_cell(mm, c, cx, cz);
    return c;
}

void minimap_init(Minimap *mm, int radius, float meters_per_px) {
    mm->world = NULL;
    mm->cache = NULL;
    mm->radius = radius;
    mm->meters_per_px = meters_per_px;
    int size = radius * 2;
    mm->pixels = calloc((size_t)(size * size), sizeof(Color));
    Image img = { mm->pixels, size, size, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    mm->tex = LoadTextureFromImage(img);
    SetTextureFilter(mm->tex, TEXTURE_FILTER_POINT);
}

void minimap_unload(Minimap *mm) {
    free(mm->cache);
    mm->cache = NULL;
    UnloadTexture(mm->tex);
    free(mm->pixels);
    mm->pixels = NULL;
}

static Color lerp_color(Color a, Color b, float t) {
    return (Color){ (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                    (unsigned char)(a.b + (b.b - a.b) * t), 255 };
}

// Lo olvidado es negro; lo recordado va del cuero al hueso, como tinta sobre piel.
static Color memory_color(float light) {
    static const Color ramp[] = {
        { 10, 8, 7, 255 }, { 58, 40, 26, 255 }, { 150, 112, 62, 255 }, { 232, 214, 170, 255 },
    };
    float t = light * 3.0f;
    int i = (int)t;
    if (i >= 3) return ramp[3];
    return lerp_color(ramp[i], ramp[i + 1], t - (float)i);
}

// Vector del mundo (plano XZ) -> desplazamiento en pantalla, con arriba = vista.
static Vector2 to_screen(float wx, float wz, float cam_yaw) {
    float fx = sinf(cam_yaw), fz = cosf(cam_yaw); // adelante
    float rx = -fz, rz = fx;                        // derecha
    return (Vector2){ wx * rx + wz * rz, -(wx * fx + wz * fz) };
}

static void diamond(int x, int y, Color fill, Color edge) {
    DrawRectangle(x - 1, y - 2, 3, 5, edge);
    DrawRectangle(x - 2, y - 1, 5, 3, edge);
    DrawRectangle(x, y - 1, 1, 3, fill);
    DrawRectangle(x - 1, y, 3, 1, fill);
}

static void draw_frame(int cx, int cy, int r, float cam_yaw) {
    Vector2 c = { (float)cx, (float)cy };
    DrawRing(c, (float)r, (float)r + 1.0f, 0, 360, 64, UI_GOLD_DARK);
    DrawRing(c, (float)r + 1.0f, (float)r + 4.0f, 0, 360, 64, UI_GOLD);
    DrawRing(c, (float)r + 1.0f, (float)r + 2.0f, 180, 360, 64, UI_GOLD_LIGHT); // brillo arriba
    DrawRing(c, (float)r + 4.0f, (float)r + 5.0f, 0, 360, 64, UI_GOLD_DARK);
    // Granulado fijo en el aro: 24 cuentas, con turquesas en los puntos cardinales del marco.
    for (int i = 0; i < 24; i++) {
        float a = (float)i * (2.0f * PI / 24.0f);
        int gx = cx + (int)lroundf(cosf(a) * ((float)r + 2.5f));
        int gy = cy + (int)lroundf(sinf(a) * ((float)r + 2.5f));
        if (i % 6 == 0) ui_inlay(gx - 1, gy - 1, UI_METAL_GOLD);
        else ui_granule(gx - 1, gy - 1, UI_METAL_GOLD);
    }
    // Norte (-Z del mundo) gira con la vista, como una brujula.
    Vector2 n = to_screen(0.0f, -1.0f, cam_yaw);
    int nx = cx + (int)lroundf(n.x * ((float)r + 3.0f));
    int ny = cy + (int)lroundf(n.y * ((float)r + 3.0f));
    DrawCircle(nx, ny, 6.0f, UI_GOLD_DARK);
    DrawCircle(nx, ny, 5.0f, UI_VELVET);
    DrawText("N", nx - 2, ny - 4, 10, UI_CARNELIAN);
}

static void draw_player_arrow(int cx, int cy, float cam_yaw, float player_yaw) {
    Vector2 d = to_screen(sinf(player_yaw), cosf(player_yaw), cam_yaw);
    Vector2 side = { -d.y, d.x };
    Vector2 c = { (float)cx + 0.5f, (float)cy + 0.5f };
    Vector2 tip = { c.x + d.x * 5.0f, c.y + d.y * 5.0f };
    Vector2 l = { c.x - d.x * 3.0f + side.x * 3.5f, c.y - d.y * 3.0f + side.y * 3.5f };
    Vector2 rr = { c.x - d.x * 3.0f - side.x * 3.5f, c.y - d.y * 3.0f - side.y * 3.5f };
    // raylib exige orden antihorario en pantalla; se dibuja en ambos sentidos.
    DrawTriangle(tip, l, rr, UI_BONE);
    DrawTriangle(tip, rr, l, UI_BONE);
    DrawTriangleLines(tip, l, rr, UI_VELVET);
}

// Pueblos, tribus, guaridas y ruinas ya vistos (con memoria), dentro del circulo.
static void draw_places(const Minimap *mm, const MemoryMap *map, int cx, int cy, Vector3 pos, float cam_yaw, float now) {
    const World *w = mm->world;
    const float lim = (float)mm->radius - 3.0f;
    for (int kind = 0; kind < 4; kind++) {
        int n = kind == 0 ? w->settlement_count : kind == 1 ? w->tribe_count : kind == 2 ? w->den_count : w->site_count;
        for (int i = 0; i < n; i++) {
            float x = kind == 0 ? w->settlements[i].x : kind == 1 ? w->tribes[i].x : kind == 2 ? w->dens[i].x : w->sites[i].x;
            float z = kind == 0 ? w->settlements[i].z : kind == 1 ? w->tribes[i].z : kind == 2 ? w->dens[i].z : w->sites[i].z;
            Vector2 s = to_screen(x - pos.x, z - pos.z, cam_yaw);
            s.x /= mm->meters_per_px, s.y /= mm->meters_per_px;
            if (s.x * s.x + s.y * s.y > lim * lim || memmap_light(map, x, z, now) < 0.05f) continue;
            int px = cx + (int)lroundf(s.x), py = cy + (int)lroundf(s.y);
            if (kind == 0) { // pueblo: casitas de oro; la capital, con muralla
                bool cap = w->settlements[i].kind == SETTLE_CAPITAL;
                DrawRectangle(px - 2 - cap, py - 2 - cap, 5 + 2 * cap, 5 + 2 * cap, UI_VELVET);
                DrawRectangle(px - 1 - cap, py - 1 - cap, 3 + 2 * cap, 3 + 2 * cap, UI_GOLD_LIGHT);
            } else if (kind == 1) { // tribu: un estandarte del color de su actitud
                static const Color ATT[TRIBE_ATTITUDES] = { { 168, 36, 32, 255 }, { 214, 206, 186, 255 }, { 52, 98, 176, 255 } };
                DrawRectangle(px, py - 4, 1, 6, UI_VELVET);
                DrawRectangle(px + 1, py - 4, 3, 3, ATT[w->tribes[i].attitude]);
            } else if (kind == 2) { // guarida: una mancha roja oscura
                DrawRectangle(px - 1, py - 1, 3, 3, (Color){ 120, 24, 20, 255 });
            } else { // ruina o estructura
                DrawRectangle(px - 1, py, 3, 1, UI_TURQ_LIGHT);
                DrawRectangle(px, py - 1, 1, 3, UI_TURQ_LIGHT);
            }
        }
    }
}

void minimap_draw(Minimap *mm, const MemoryMap *map, Vector2 center, Vector3 player_pos,
                  float cam_yaw, float player_yaw, float now) {
    const int r = mm->radius, size = r * 2;
    const float mpp = mm->meters_per_px;
    int budget = MINIMAP_BUDGET;
    float fx = sinf(cam_yaw), fz = cosf(cam_yaw);
    float rx = -fz, rz = fx;
    for (int j = 0; j < size; j++) {
        float dy = (float)j - (float)r + 0.5f;
        for (int i = 0; i < size; i++) {
            float dx = (float)i - (float)r + 0.5f;
            Color *px = &mm->pixels[j * size + i];
            if (dx * dx + dy * dy > (float)(r * r)) {
                *px = BLANK;
                continue;
            }
            // Pixel -> mundo: derecha * dx + adelante * (-dy).
            float wx = player_pos.x + (rx * dx - fx * dy) * mpp;
            float wz = player_pos.z + (rz * dx - fz * dy) * mpp;
            float light = memmap_light(map, wx, wz, now);
            MinimapCell *cell = NULL;
            if (mm->world && mm->cache)
                cell = cell_at(mm, (int)floorf(wx / MINIMAP_CELL), (int)floorf(wz / MINIMAP_CELL), &budget);
            if (!cell) { // sin mundo (o la celda aun sin calcular): la tinta de la memoria
                *px = memory_color(light);
                continue;
            }
            // Relieve: la ladera que mira al noroeste, clara; la otra, en sombra.
            Color c = cell->c;
            if (!cell->water) {
                const MinimapCell *nw = cell_at(mm, cell->cx - 1, cell->cz - 1, &budget);
                float shade = nw ? fminf(1.3f, fmaxf(0.65f, 1.0f + (nw->h - cell->h) * -0.06f)) : 1.0f;
                c = (Color){ (unsigned char)fminf(255.0f, c.r * shade), (unsigned char)fminf(255.0f, c.g * shade),
                             (unsigned char)fminf(255.0f, c.b * shade), 255 };
            }
            // La tribu conoce su tierra: la geografia se ve siempre, apagada donde no estuviste y
            // viva donde la recuerdas.
            *px = lerp_color(memory_color(0.0f), c, 0.6f + 0.4f * fminf(1.0f, light * 1.4f));
        }
    }
    UpdateTexture(mm->tex, mm->pixels);
    int cx = (int)center.x, cy = (int)center.y;
    DrawTexture(mm->tex, cx - r, cy - r, WHITE);

    if (mm->world) draw_places(mm, map, cx, cy, player_pos, cam_yaw, now);
    // Marcas: fuera del alcance quedan en el borde, senalando su direccion.
    for (int k = 0; k < map->marker_count; k++) {
        const MapMarker *mk = &map->markers[k];
        Vector2 s = to_screen(mk->x - player_pos.x, mk->z - player_pos.z, cam_yaw);
        s.x /= mpp;
        s.y /= mpp;
        float len = sqrtf(s.x * s.x + s.y * s.y), lim = (float)r - 4.0f;
        if (len > lim) {
            s.x *= lim / len;
            s.y *= lim / len;
        }
        Color fill = mk->kind == MARKER_DANGER ? UI_CARNELIAN : (mk->kind == MARKER_CAMP ? UI_GOLD_LIGHT : UI_TURQUOISE);
        diamond(cx + (int)lroundf(s.x), cy + (int)lroundf(s.y), fill, UI_VELVET);
    }
    draw_player_arrow(cx, cy, cam_yaw, player_yaw);
    draw_frame(cx, cy, r, cam_yaw);
}
