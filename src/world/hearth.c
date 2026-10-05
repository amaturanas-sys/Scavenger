#include "hearth.h"

#include <math.h>
#include <stdlib.h>

#include "raymath.h"
#include "rlgl.h"

#define FIRE_MAX 48
#define PUFFS_PER_FIRE 12
#define FIRE_VIEW 600.0f // mas lejos ni llamas ni humo: el resplandor de sky_draw_lights basta

static const Vector2 WIND_DIR = { 0.86f, 0.5f }; // el viento de las nubes y la lluvia

typedef struct {
    Vector3 pos;
    float s;
    uint32_t seed;
} Fire;

static Fire g_fires[FIRE_MAX];
static int g_count;
static Texture2D g_puff;

static float hash(uint32_t seed, uint32_t k) {
    uint32_t h = seed * 2654435761u ^ (k + 0x9e3779b9u) * 2246822519u;
    h ^= h >> 15, h *= 2246822519u, h ^= h >> 13, h *= 3266489917u, h ^= h >> 16;
    return (float)(h & 0xffffff) / 16777216.0f;
}

static float fract(float x) { return x - floorf(x); }

// La bocanada: unas cuantas manchas redondas sumadas y cuantizadas en 4 niveles de opacidad,
// como las nubes pixeladas del cielo. Blanca, algo mas clara arriba; el tinte pone el gris.
void hearth_init(void) {
    if (g_puff.id) return;
    enum { N = 32 };
    Image img = GenImageColor(N, N, BLANK);
    Color *px = img.data;
    static const float blobs[6][3] = { { 16, 18, 9.5f }, { 10, 14, 6.5f }, { 22, 13, 7.0f }, { 15, 9, 6.0f }, { 8, 21, 5.0f }, { 24, 21, 5.5f } };
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float a = 0.0f;
            for (int b = 0; b < 6; b++) {
                float dx = (x + 0.5f - blobs[b][0]) / blobs[b][2], dy = (y + 0.5f - blobs[b][1]) / blobs[b][2];
                a += expf(-(dx * dx + dy * dy) * 1.6f);
            }
            float q = a > 0.9f ? 1.0f : a > 0.55f ? 0.7f : a > 0.3f ? 0.4f : a > 0.15f ? 0.15f : 0.0f;
            unsigned char v = (unsigned char)(255.0f * (0.78f + 0.22f * (1.0f - (float)y / N)));
            px[y * N + x] = (Color){ v, v, v, (unsigned char)(255.0f * q) };
        }
    g_puff = LoadTextureFromImage(img);
    UnloadImage(img);
}

void hearth_unload(void) {
    if (g_puff.id) UnloadTexture(g_puff);
    g_puff = (Texture2D){ 0 };
}

void hearth_frame_begin(void) { g_count = 0; }
int hearth_lit_count(void) { return g_count; }

// Una piedra: esfera de pocas caras, achatada y girada (como un canto).
static void stone(Vector3 p, Vector3 size, float yaw, float tilt, Color c) {
    rlPushMatrix();
    rlTranslatef(p.x, p.y, p.z);
    rlRotatef(yaw, 0, 1, 0);
    rlRotatef(tilt, 1, 0, 0);
    rlScalef(size.x, size.y, size.z);
    DrawSphereEx((Vector3){ 0, 0, 0 }, 1.0f, 3, 6, c);
    rlPopMatrix();
}

// Un leño: corteza abajo, carbonizado hacia la punta que da al fuego.
static void log_piece(Vector3 a, Vector3 b, float r, float burnt, Color bark) {
    Vector3 m = Vector3Lerp(a, b, 1.0f - burnt);
    DrawCylinderEx(a, m, r, r * 0.95f, 5, bark);
    DrawCylinderEx(m, b, r * 0.95f, r * 0.7f, 5, (Color){ 34, 28, 24, 255 });
}

void hearth_draw_base(Vector3 pos, float s, uint32_t seed, bool lit) {
    // Ceniza y tierra quemada.
    DrawCylinder((Vector3){ pos.x, pos.y - 0.02f, pos.z }, 0.62f * s, 0.66f * s, 0.05f, 9, (Color){ 52, 48, 45, 255 });
    DrawCylinder((Vector3){ pos.x, pos.y, pos.z }, 0.34f * s, 0.4f * s, 0.05f, 8, (Color){ 74, 70, 66, 255 });
    // El corro de piedras: cantos grises de tamaños distintos, los de dentro tiznados.
    int n = 9 + (int)(s * 2.5f);
    float big = powf(s, 0.6f);
    for (int i = 0; i < n; i++) {
        float a = ((float)i + 0.4f * hash(seed, i)) / (float)n * 2.0f * PI;
        float r = 0.62f * s * (0.95f + 0.12f * hash(seed, i + 40));
        Vector3 size = { (0.13f + 0.07f * hash(seed, i + 80)) * big, (0.08f + 0.06f * hash(seed, i + 120)) * big,
                         (0.1f + 0.05f * hash(seed, i + 160)) * big };
        float g = 92.0f + 50.0f * hash(seed, i + 200), soot = hash(seed, i + 240) < 0.35f ? 0.55f : 1.0f;
        Color c = { (unsigned char)(g * soot + 4), (unsigned char)(g * soot), (unsigned char)(g * soot - 4), 255 };
        stone((Vector3){ pos.x + cosf(a) * r, pos.y + size.y * 0.45f, pos.z + sinf(a) * r }, size, -a * RAD2DEG + 90.0f,
              (hash(seed, i + 280) - 0.5f) * 30.0f, c);
    }
    // Leños en tipi, apoyados unos en otros sobre el centro.
    int m = 4 + (int)(s * 1.5f);
    float lr = 0.045f * s, top = (0.55f + 0.1f * hash(seed, 300)) * s;
    for (int j = 0; j < m; j++) {
        float a = ((float)j + 0.3f * hash(seed, j + 320)) / (float)m * 2.0f * PI + hash(seed, 1) * PI;
        float rb = 0.4f * s * (0.85f + 0.3f * hash(seed, j + 340));
        Vector3 b = { pos.x + cosf(a) * rb, pos.y + 0.03f, pos.z + sinf(a) * rb };
        Vector3 t = { pos.x + cosf(a + 2.6f) * 0.06f * s, pos.y + top * (0.8f + 0.25f * hash(seed, j + 360)), pos.z + sinf(a + 2.6f) * 0.06f * s };
        float tone = 0.8f + 0.4f * hash(seed, j + 380);
        log_piece(b, t, lr * (0.8f + 0.4f * hash(seed, j + 400)), lit ? 0.45f : 0.6f,
                  (Color){ (unsigned char)(92 * tone), (unsigned char)(66 * tone), (unsigned char)(44 * tone), 255 });
    }
    // Dos troncos cruzados en el suelo (las brasas de abajo).
    for (int j = 0; j < 2; j++) {
        float a = hash(seed, 5) * PI + j * 1.9f;
        Vector3 d = { cosf(a) * 0.38f * s, 0, sinf(a) * 0.38f * s };
        Vector3 c = { pos.x, pos.y + lr * 1.2f + j * lr, pos.z };
        log_piece(Vector3Subtract(c, d), Vector3Add(c, d), lr * 1.15f, 0.5f, (Color){ 78, 58, 40, 255 });
    }
    if (lit && g_count < FIRE_MAX) g_fires[g_count++] = (Fire){ pos, s, seed };
}

// ---- Humo ----

typedef struct {
    Vector3 p;
    float size, rot, dist;
    Color c;
} Puff;

static int by_dist(const void *a, const void *b) {
    float da = ((const Puff *)a)->dist, db = ((const Puff *)b)->dist;
    return da < db ? 1 : da > db ? -1 : 0; // de lejos a cerca
}

static float smooth(float e0, float e1, float x) {
    float t = Clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void hearth_draw_smoke(Camera3D cam, float time, float wind, float light) {
    static Puff puffs[FIRE_MAX * PUFFS_PER_FIRE];
    int np = 0;
    float push = 0.35f + 2.2f * Clamp(wind, 0.0f, 1.0f);
    float dim = Clamp(light, 0.0f, 1.0f); // de noche, contra el cielo oscuro, el humo tambien
    for (int f = 0; f < g_count; f++) {
        const Fire *fi = &g_fires[f];
        if (Vector3Distance(fi->pos, cam.position) > FIRE_VIEW) continue;
        float s = fi->s, height = 6.0f + 4.0f * s, period = 5.5f + 2.5f * s;
        int n = 8 + (int)(s * 2.0f);
        if (n > PUFFS_PER_FIRE) n = PUFFS_PER_FIRE;
        for (int i = 0; i < n; i++) {
            // Cada bocanada nace sobre las llamas y sube; la hilera es la misma bocanada en fases.
            float ph = fract(time / period + (float)i / (float)n + 0.03f * hash(fi->seed, i + 500));
            float rise = 1.0f - powf(1.0f - ph, 1.5f); // sube rapido y se frena arriba
            float lean = powf(ph, 1.3f) * height * 0.45f * push;
            float wob = sinf(ph * 5.0f + time * 0.4f + (float)f) * 0.25f * s * ph;
            Vector3 p = { fi->pos.x + WIND_DIR.x * lean + WIND_DIR.y * wob, fi->pos.y + 0.8f * s + rise * height,
                          fi->pos.z + WIND_DIR.y * lean - WIND_DIR.x * wob };
            float v = (36.0f + 64.0f * ph) * (0.3f + 0.7f * dim); // oscuro al salir, gris al desvanecerse
            float a = 215.0f * smooth(0.0f, 0.07f, ph) * powf(1.0f - ph, 1.5f);
            puffs[np++] = (Puff){ p, s * (0.32f + 1.25f * ph) * (0.85f + 0.3f * hash(fi->seed, i + 520)),
                                  hash(fi->seed, i + 540) * 360.0f + ph * 50.0f, Vector3Distance(p, cam.position),
                                  (Color){ (unsigned char)v, (unsigned char)(v * 0.97f), (unsigned char)(v * 0.94f), (unsigned char)a } };
        }
    }
    if (!np) return;
    qsort(puffs, (size_t)np, sizeof(Puff), by_dist);
    rlDrawRenderBatchActive();
    rlDisableDepthMask(); // translucido: no tapa lo que viene detras
    for (int i = 0; i < np; i++) {
        const Puff *pf = &puffs[i];
        if (g_puff.id) {
            Rectangle src = { 0, 0, (float)g_puff.width, (float)g_puff.height };
            DrawBillboardPro(cam, g_puff, src, pf->p, (Vector3){ 0, 1, 0 }, (Vector2){ pf->size, pf->size },
                             (Vector2){ pf->size * 0.5f, pf->size * 0.5f }, pf->rot, pf->c);
        } else {
            DrawSphereEx(pf->p, pf->size * 0.4f, 3, 6, pf->c);
        }
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

// ---- Llamas, brasas y chispas ----

void hearth_draw_flames(float time, float wind) {
    if (!g_count) return;
    static const Color LAYER[4] = { { 214, 64, 22, 210 }, { 246, 132, 34, 230 }, { 255, 200, 74, 245 }, { 255, 244, 200, 255 } };
    static const float WIDTH[4] = { 0.13f, 0.1f, 0.07f, 0.04f }, TALL[4] = { 1.0f, 0.8f, 0.58f, 0.32f };
    float push = 0.15f + 0.5f * Clamp(wind, 0.0f, 1.0f);
    // Brasas: cubitos encendidos en el lecho, que laten.
    for (int f = 0; f < g_count; f++) {
        const Fire *fi = &g_fires[f];
        int n = 6 + (int)(fi->s * 3.0f);
        for (int i = 0; i < n; i++) {
            float a = hash(fi->seed, i + 600) * 2.0f * PI, r = 0.3f * fi->s * sqrtf(hash(fi->seed, i + 620));
            float k = 0.5f + 0.5f * sinf(time * (1.5f + hash(fi->seed, i + 640)) + (float)i);
            Color c = { 255, (unsigned char)(60 + 90 * k), (unsigned char)(20 + 30 * k), 255 };
            float sz = (0.05f + 0.04f * hash(fi->seed, i + 660)) * fi->s;
            DrawCube((Vector3){ fi->pos.x + cosf(a) * r, fi->pos.y + 0.04f, fi->pos.z + sinf(a) * r }, sz, sz * 0.6f, sz, c);
        }
    }
    // Lenguas de llama: conos que tiemblan y se mecen; por capas, de la roja de fuera a la
    // blanca del corazon, sin escribir profundidad para que cada capa se vea sobre la anterior.
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    for (int layer = 0; layer < 4; layer++) {
        for (int f = 0; f < g_count; f++) {
            const Fire *fi = &g_fires[f];
            float s = fi->s;
            int tongues = 5 + (int)(s * 1.5f);
            for (int k = 0; k < tongues; k++) {
                if (layer == 3 && k % 2) continue;
                float ha = hash(fi->seed, k + 700), hb = hash(fi->seed, k + 720);
                float a = ha * 2.0f * PI, rad = 0.15f * s * hb * (layer == 3 ? 0.4f : 1.0f);
                Vector3 b = { fi->pos.x + cosf(a) * rad, fi->pos.y + 0.06f * s, fi->pos.z + sinf(a) * rad };
                float flick = 0.72f + 0.28f * sinf(time * (8.0f + 3.0f * ha) + k * 1.7f) * sinf(time * (5.1f + 2.0f * hb) + (float)k);
                float h = s * (0.6f + 0.5f * hash(fi->seed, k + 740)) * flick * (k == 0 ? 1.25f : 1.0f) * TALL[layer];
                float sway = 0.1f * s;
                Vector3 tip = { b.x + sinf(time * 3.1f + k * 2.3f) * sway + WIND_DIR.x * push * h,
                                b.y + h, b.z + cosf(time * 2.7f + k * 1.9f) * sway + WIND_DIR.y * push * h };
                DrawCylinderEx(b, tip, WIDTH[layer] * s * (0.8f + 0.4f * hb), 0.0f, 5, LAYER[layer]);
            }
        }
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    // Chispas: suben girando, se enfrian (amarillo, naranja, rojo) y se apagan.
    for (int f = 0; f < g_count; f++) {
        const Fire *fi = &g_fires[f];
        float s = fi->s;
        int n = (int)(9.0f * s);
        for (int i = 0; i < n; i++) {
            float period = 1.1f + 1.2f * hash(fi->seed, i + 800);
            float ph = fract(time / period + hash(fi->seed, i + 820));
            float up = (0.4f + 2.6f * ph) * s;
            float side = (hash(fi->seed, i + 840) - 0.5f) * 0.35f * s + sinf(ph * 7.0f + i) * 0.18f * s * ph;
            float drift = ph * (0.3f + 1.5f * push) * s;
            Vector3 p = { fi->pos.x + side + WIND_DIR.x * drift, fi->pos.y + up, fi->pos.z + side * 0.6f + WIND_DIR.y * drift };
            Color c = ph < 0.3f ? (Color){ 255, 226, 130, 255 } : ph < 0.65f ? (Color){ 255, 146, 44, 255 } : (Color){ 210, 70, 26, 255 };
            c.a = (unsigned char)(255.0f * (1.0f - ph));
            DrawLine3D(p, (Vector3){ p.x - WIND_DIR.x * 0.05f * s, p.y - 0.12f * s, p.z - WIND_DIR.y * 0.05f * s }, c);
            float sz = 0.035f * sqrtf(s);
            DrawCube(p, sz, sz, sz, c);
        }
    }
}
