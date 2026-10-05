#include "worldview.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"
#include "sim/animals.h"
#include "sim/lang.h"

#define VIEW_N 255                       // vertices por lado (indices de 16 bits: n*n <= 65535)
#define VIEW_VSCALE 4.0f                 // el relieve, exagerado desde la orbita

static Color mixc(Color a, Color b, float k) {
    k = Clamp(k, 0.0f, 1.0f);
    return (Color){ (unsigned char)Lerp(a.r, b.r, k), (unsigned char)Lerp(a.g, b.g, k), (unsigned char)Lerp(a.b, b.b, k), 255 };
}

// Color del suelo visto desde lejos: la region, la roca en lo empinado, la nieve en lo alto y el agua.
static Color world_color(const World *w, float x, float z, float h, float slope, float snowline, float water, WaterKind wk) {
    static const Color REG[REGION_COUNT] = {
        { 176, 164, 96, 255 }, { 52, 92, 52, 255 }, { 142, 140, 112, 255 }, { 96, 122, 98, 255 }, { 214, 186, 130, 255 },
    };
    if (wk != WATER_NONE && water > h) {
        float depth = Clamp((water - h) / 6.0f, 0.0f, 1.0f);
        Color shallow = wk == WATER_SEA ? (Color){ 52, 96, 124, 255 } : (Color){ 72, 124, 150, 255 };
        Color deep = wk == WATER_SEA ? (Color){ 22, 48, 82, 255 } : (Color){ 40, 86, 124, 255 };
        return mixc(shallow, deep, depth);
    }
    float k[REGION_COUNT];
    world_region_weights(w, x, z, k);
    float r = 0, g = 0, b = 0;
    for (int i = 0; i < REGION_COUNT; i++) r += k[i] * REG[i].r, g += k[i] * REG[i].g, b += k[i] * REG[i].b;
    Color c = { (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
    if (slope > 0.6f) c = mixc(c, k[REGION_DESERT] > 0.5f ? (Color){ 172, 102, 70, 255 } : (Color){ 116, 112, 104, 255 }, (slope - 0.6f) * 2.0f);
    if (h > snowline) c = mixc(c, (Color){ 232, 238, 244, 255 }, Clamp((h - snowline) / 8.0f, 0.0f, 1.0f));
    return c;
}

// Lo que cubren la malla y el mapa: el borde fractal mas lejano y un margen (el muro, el hielo).
static float view_ext(const World *w) {
    float r = WORLD_RADIUS;
    for (int a = 0; a < 720; a++) r = fmaxf(r, world_edge_radius(w, (float)a / 720.0f * 2.0f * PI));
    return r + 320.0f;
}

void worldview_build(WorldView *v, const World *w, float snowline) {
    if (v->built && v->seed == w->seed) return;
    worldview_unload(v);
    const int n = VIEW_N;
    const float ext = view_ext(w), step = 2.0f * ext / (n - 1);
    float *hs = malloc(sizeof(float) * n * n), *ws = malloc(sizeof(float) * n * n);
    WaterKind *kinds = malloc(sizeof(WaterKind) * n * n);
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            float x = -ext + i * step, z = -ext + j * step;
            hs[j * n + i] = world_height(w, x, z);
            ws[j * n + i] = world_water(w, x, z, 0.0f, &kinds[j * n + i]);
        }
    Mesh m = { 0 };
    m.vertexCount = n * n;
    m.triangleCount = (n - 1) * (n - 1) * 2;
    m.vertices = MemAlloc(m.vertexCount * 3 * sizeof(float));
    m.normals = MemAlloc(m.vertexCount * 3 * sizeof(float));
    m.colors = MemAlloc(m.vertexCount * 4);
    m.indices = MemAlloc(m.triangleCount * 3 * sizeof(unsigned short));
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            int id = j * n + i;
            float x = -ext + i * step, z = -ext + j * step;
            float h = hs[id], top = fmaxf(h, kinds[id] != WATER_NONE ? ws[id] : -1e9f);
            bool outside = world_edge_distance(w, x, z) < -150.0f; // fuera del gran circulo: el vacio
            if (outside) top = -400.0f; // el disco flota: fuera, el vacio (del color del fondo)
            float hx = hs[j * n + (i < n - 1 ? i + 1 : i)] - hs[j * n + (i > 0 ? i - 1 : i)];
            float hz = hs[(j < n - 1 ? j + 1 : j) * n + i] - hs[(j > 0 ? j - 1 : j) * n + i];
            Vector3 nor = Vector3Normalize((Vector3){ -hx * VIEW_VSCALE, 2.0f * step, -hz * VIEW_VSCALE });
            float slope = sqrtf(hx * hx + hz * hz) / (2.0f * step) * VIEW_VSCALE * 0.6f;
            m.vertices[id * 3 + 0] = x, m.vertices[id * 3 + 1] = top * VIEW_VSCALE, m.vertices[id * 3 + 2] = z;
            m.normals[id * 3 + 0] = nor.x, m.normals[id * 3 + 1] = nor.y, m.normals[id * 3 + 2] = nor.z;
            Color c = world_color(w, x, z, h, slope, snowline, ws[id], kinds[id]);
            // Luz horneada (el sol del noroeste) y fuera del circulo, mas oscuro.
            const Vector3 sun = Vector3Normalize((Vector3){ -0.5f, 0.8f, -0.4f });
            float light = 0.55f + 0.55f * fmaxf(0.0f, Vector3DotProduct(nor, sun));
            if (world_edge_distance(w, x, z) < 0.0f) light *= 0.55f;
            if (outside) c = (Color){ 14, 18, 28, 255 }, light = 1.0f;
            m.colors[id * 4 + 0] = (unsigned char)fminf(255.0f, c.r * light);
            m.colors[id * 4 + 1] = (unsigned char)fminf(255.0f, c.g * light);
            m.colors[id * 4 + 2] = (unsigned char)fminf(255.0f, c.b * light);
            m.colors[id * 4 + 3] = 255;
        }
    int t = 0;
    for (int j = 0; j < n - 1; j++)
        for (int i = 0; i < n - 1; i++) {
            unsigned short a = (unsigned short)(j * n + i), b = (unsigned short)(a + 1), c = (unsigned short)(a + n), d = (unsigned short)(c + 1);
            unsigned short q[6] = { a, c, b, b, c, d };
            memcpy(&m.indices[t * 3], q, sizeof(q));
            t += 2;
        }
    UploadMesh(&m, false);
    v->model = LoadModelFromMesh(m);
    v->built = true;
    v->seed = w->seed;
    free(hs), free(ws), free(kinds);
}

void worldview_unload(WorldView *v) {
    if (v->built) UnloadModel(v->model);
    v->built = false;
}

static Vector3 at_world(const World *w, float x, float z, float lift) {
    float h = world_height(w, x, z), water = world_water(w, x, z, 0.0f, NULL);
    return (Vector3){ x, (fmaxf(h, water) + lift) * VIEW_VSCALE, z };
}

void worldview_draw(WorldView *v, const World *w, float angle, float tilt, Vector3 player, float time) {
    if (!v->built) return;
    tilt = Clamp(tilt, 0.0f, 1.0f);
    float radial = 5200.0f * (1.2f - 0.7f * tilt), height = 1300.0f + 4600.0f * tilt;
    Camera3D cam = { 0 };
    cam.position = (Vector3){ cosf(angle) * radial, height, sinf(angle) * radial };
    cam.target = (Vector3){ 0, 120.0f, 0 };
    cam.up = (Vector3){ 0, 1, 0 };
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    rlSetClipPlanes(10.0, 30000.0);
    BeginMode3D(cam);
    DrawModel(v->model, (Vector3){ 0 }, 1.0f, WHITE);
    // Asentamientos: torres doradas las capitales, blancas las aldeas; guaridas en rojo.
    for (int i = 0; i < w->settlement_count; i++) {
        const Settlement *s = &w->settlements[i];
        Vector3 p = at_world(w, s->x, s->z, 0.0f);
        bool cap = s->kind == SETTLE_CAPITAL;
        DrawCylinder(p, cap ? 26.0f : 14.0f, cap ? 30.0f : 16.0f, cap ? 260.0f : 120.0f, 6, cap ? (Color){ 232, 190, 70, 255 } : (Color){ 236, 232, 220, 255 });
        if (cap) DrawCylinder((Vector3){ p.x, p.y + 260.0f, p.z }, 0.0f, 34.0f, 60.0f, 6, (Color){ 250, 220, 120, 255 });
    }
    for (int i = 0; i < w->den_count; i++) {
        Vector3 p = at_world(w, w->dens[i].x, w->dens[i].z, 0.0f);
        DrawCylinder(p, 0.0f, 16.0f, 70.0f, 4, (Color){ 196, 50, 40, 255 });
    }
    Vector3 camp = at_world(w, 0.0f, 0.0f, 0.0f);
    DrawCylinder(camp, 22.0f, 22.0f, 90.0f, 8, (Color){ 240, 140, 40, 255 });
    float pulse = 0.6f + 0.4f * sinf(time * 4.0f);
    Vector3 pp = at_world(w, player.x, player.z, 0.0f);
    DrawCylinder(pp, 12.0f, 12.0f, 520.0f * pulse, 8, (Color){ 90, 220, 230, 255 }); // el jugador
    EndMode3D();
    rlSetClipPlanes(RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
}

// ------------------------------------------------------------------ mapa general
static void mark(Image *img, int x, int y, int r, Color fill, Color edge) {
    ImageDrawCircle(img, x, y, r + 1, edge);
    ImageDrawCircle(img, x, y, r, fill);
}

bool worldmap_export(const World *w, const char *path, int size, float snowline) {
    Image img = GenImageColor(size, size, (Color){ 18, 22, 30, 255 });
    float *hs = malloc(sizeof(float) * size * size);
    const float ext = view_ext(w), px = 2.0f * ext / size;
    for (int j = 0; j < size; j++)
        for (int i = 0; i < size; i++) hs[j * size + i] = world_height(w, -ext + (i + 0.5f) * px, -ext + (j + 0.5f) * px);
    for (int j = 0; j < size; j++)
        for (int i = 0; i < size; i++) {
            float x = -ext + (i + 0.5f) * px, z = -ext + (j + 0.5f) * px, h = hs[j * size + i];
            float hx = (i > 0 ? h - hs[j * size + i - 1] : 0.0f), hz = (j > 0 ? h - hs[(j - 1) * size + i] : 0.0f);
            WaterKind k;
            float water = world_water(w, x, z, 0.0f, &k);
            float slope = sqrtf(hx * hx + hz * hz) / px;
            Color c = world_color(w, x, z, h, slope, snowline, water, k);
            float light = k != WATER_NONE && water > h ? 1.0f : Clamp(1.0f + (hx + hz) / px * 1.4f, 0.45f, 1.45f);
            if (world_edge_distance(w, x, z) < 0.0f) light *= 0.45f;
            ImageDrawPixel(&img, i, j, (Color){ (unsigned char)fminf(255, c.r * light), (unsigned char)fminf(255, c.g * light), (unsigned char)fminf(255, c.b * light), 255 });
        }
    free(hs);
    // El borde fractal (la frontera de lo transitable).
    int cx = size / 2, cy = size / 2;
    for (int a = 0; a < 2880; a++) {
        float t = (float)a / 2880.0f * 2.0f * PI, rr = world_edge_radius(w, t) / px;
        ImageDrawPixel(&img, cx + (int)(cosf(t) * rr), cy + (int)(sinf(t) * rr), (Color){ 240, 220, 160, 140 });
    }
    int fs = size >= 900 ? 10 : 8;
    // Guaridas, aldeas, capitales y el campamento.
    for (int i = 0; i < w->den_count; i++) {
        int x = (int)((w->dens[i].x + ext) / px), y = (int)((w->dens[i].z + ext) / px);
        ImageDrawTriangle(&img, (Vector2){ (float)x, (float)y - 5 }, (Vector2){ (float)x - 4, (float)y + 3 }, (Vector2){ (float)x + 4, (float)y + 3 }, (Color){ 200, 40, 36, 255 });
    }
    static const Color TRIBE_COL[TRIBE_ATTITUDES] = { { 200, 44, 40, 255 }, { 226, 220, 200, 255 }, { 70, 120, 210, 255 } };
    for (int i = 0; i < w->site_count; i++) { // estructuras: rombo pardo (ruina) o gris (en uso)
        int x = (int)((w->sites[i].x + ext) / px), y = (int)((w->sites[i].z + ext) / px);
        Color c = site_is_ruin(w->sites[i].kind) ? (Color){ 150, 110, 70, 255 } : (Color){ 200, 200, 196, 255 };
        ImageDrawTriangle(&img, (Vector2){ (float)x, (float)y - 4 }, (Vector2){ (float)x - 4, (float)y }, (Vector2){ (float)x + 4, (float)y }, c);
        ImageDrawTriangle(&img, (Vector2){ (float)x - 4, (float)y }, (Vector2){ (float)x, (float)y + 4 }, (Vector2){ (float)x + 4, (float)y }, c);
    }
    for (int i = 0; i < w->tribe_count; i++) { // tribus: banderin del color de su actitud
        int x = (int)((w->tribes[i].x + ext) / px), y = (int)((w->tribes[i].z + ext) / px);
        ImageDrawRectangle(&img, x, y - 8, 1, 10, (Color){ 30, 20, 10, 255 });
        ImageDrawRectangle(&img, x + 1, y - 8, 6, 4, TRIBE_COL[w->tribes[i].attitude]);
    }
    for (int i = 0; i < w->settlement_count; i++) {
        const Settlement *s = &w->settlements[i];
        int x = (int)((s->x + ext) / px), y = (int)((s->z + ext) / px);
        bool cap = s->kind == SETTLE_CAPITAL;
        if (cap) {
            ImageDrawRectangle(&img, x - 6, y - 6, 13, 13, (Color){ 30, 20, 10, 255 });
            ImageDrawRectangle(&img, x - 5, y - 5, 11, 11, (Color){ 236, 196, 70, 255 });
        } else {
            mark(&img, x, y, 3, (Color){ 246, 242, 230, 255 }, (Color){ 30, 20, 10, 255 });
        }
        const char *label = cap ? TextFormat("%s (%s)", s->name, world_kingdom_name(s->region)) : s->name;
        ImageDrawText(&img, label, x + 8, y - fs / 2, fs, (Color){ 20, 14, 10, 255 });
        ImageDrawText(&img, label, x + 7, y - fs / 2 - 1, fs, cap ? (Color){ 255, 230, 150, 255 } : (Color){ 245, 240, 230, 255 });
    }
    mark(&img, cx, cy, 5, (Color){ 240, 140, 40, 255 }, (Color){ 30, 20, 10, 255 });
    ImageDrawText(&img, T("campamento"), cx + 8, cy - 4, fs, (Color){ 255, 200, 140, 255 });
    // Leyenda.
    static const Color REG[REGION_COUNT] = {
        { 176, 164, 96, 255 }, { 52, 92, 52, 255 }, { 142, 140, 112, 255 }, { 96, 122, 98, 255 }, { 214, 186, 130, 255 },
    };
    int ly = 12;
    ImageDrawRectangle(&img, 8, 6, 250, 20 + REGION_COUNT * 16 + 98, (Color){ 12, 10, 8, 210 });
    ImageDrawText(&img, TextFormat(T("Mundo %u"), w->seed), 16, ly, fs + 4, (Color){ 255, 230, 150, 255 });
    ly += 22;
    for (int r = 0; r < REGION_COUNT; r++, ly += 16) {
        ImageDrawRectangle(&img, 16, ly, 12, 12, REG[r]);
        ImageDrawText(&img, TextFormat("%s · %.0f m", region_name((Region)r), region_altitude((Region)r)), 34, ly + 1, fs, (Color){ 235, 230, 220, 255 });
    }
    ImageDrawRectangle(&img, 16, ly + 2, 12, 8, (Color){ 72, 124, 150, 255 });
    ImageDrawText(&img, T("agua: lagos, ríos, canal y mar"), 34, ly + 1, fs, (Color){ 235, 230, 220, 255 });
    ly += 16;
    ImageDrawRectangle(&img, 17, ly, 10, 10, (Color){ 236, 196, 70, 255 });
    ImageDrawText(&img, T("capital · aldea"), 34, ly + 1, fs, (Color){ 235, 230, 220, 255 });
    ly += 16;
    ImageDrawTriangle(&img, (Vector2){ 22, (float)ly - 1 }, (Vector2){ 18, (float)ly + 8 }, (Vector2){ 26, (float)ly + 8 }, (Color){ 200, 40, 36, 255 });
    ImageDrawText(&img, T("guarida de fieras"), 34, ly + 1, fs, (Color){ 235, 230, 220, 255 });
    ly += 16;
    for (int k = 0; k < TRIBE_ATTITUDES; k++) ImageDrawRectangle(&img, 17 + k * 5, ly + 1, 4, 8, TRIBE_COL[k]);
    ImageDrawText(&img, T("tribu rival · neutral · amiga"), 34, ly + 1, fs, (Color){ 235, 230, 220, 255 });
    ly += 16;
    ImageDrawRectangle(&img, 17, ly + 2, 5, 6, (Color){ 150, 110, 70, 255 });
    ImageDrawRectangle(&img, 23, ly + 2, 5, 6, (Color){ 200, 200, 196, 255 });
    ImageDrawText(&img, T("ruina · estructura en uso"), 34, ly + 1, fs, (Color){ 235, 230, 220, 255 });
    bool ok = ExportImage(img, path);
    UnloadImage(img);
    return ok;
}
