#include "ui/minimap.h"

#include <math.h>
#include <stdlib.h>

#include "ui/theme.h"

void minimap_init(Minimap *mm, int radius, float meters_per_px) {
    mm->radius = radius;
    mm->meters_per_px = meters_per_px;
    int size = radius * 2;
    mm->pixels = calloc((size_t)(size * size), sizeof(Color));
    Image img = { mm->pixels, size, size, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    mm->tex = LoadTextureFromImage(img);
    SetTextureFilter(mm->tex, TEXTURE_FILTER_POINT);
}

void minimap_unload(Minimap *mm) {
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

void minimap_draw(Minimap *mm, const MemoryMap *map, Vector2 center, Vector3 player_pos,
                  float cam_yaw, float player_yaw, float now) {
    const int r = mm->radius, size = r * 2;
    const float mpp = mm->meters_per_px;
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
            *px = memory_color(memmap_light(map, wx, wz, now));
        }
    }
    UpdateTexture(mm->tex, mm->pixels);
    int cx = (int)center.x, cy = (int)center.y;
    DrawTexture(mm->tex, cx - r, cy - r, WHITE);

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
