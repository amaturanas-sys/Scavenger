#include "clouds.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "raymath.h"

#define CELL 230.0f // m: una nube posible por celda (en el marco que arrastra el viento)
#define PUFFS_MAX 6
#define CAP_PUFFS 6

static const Vector2 WIND_DIR = { 0.86f, 0.5f }; // el mismo viento que la lluvia (weather.c)

static Model g_puff;
static bool g_ready;
static Vector2 g_drift; // cuanto arrastro el viento la capa
static float g_last_time = -1.0f;

static uint32_t hash3(int x, int z, uint32_t k) {
    uint32_t h = (uint32_t)x * 0x8DA6B343u ^ (uint32_t)z * 0xD8163841u ^ k * 0xCB1AB31Fu;
    h ^= h >> 13;
    h *= 0x5BD1E995u;
    return h ^ (h >> 15);
}

static float unit(uint32_t h, int shift) { return (float)((h >> shift) & 0xFF) / 255.0f; }

// Un bollo: esfera low-poly con el tope claro y la panza gris (sombra horneada).
static void ensure_puff(void) {
    if (g_ready) return;
    Mesh s = GenMeshSphere(1.0f, 4, 7);
    Mesh m = { 0 };
    m.vertexCount = s.vertexCount;
    m.triangleCount = s.triangleCount;
    m.vertices = MemAlloc(m.vertexCount * 3 * sizeof(float));
    memcpy(m.vertices, s.vertices, m.vertexCount * 3 * sizeof(float));
    if (s.indices) {
        m.indices = MemAlloc(m.triangleCount * 3 * sizeof(unsigned short));
        memcpy(m.indices, s.indices, m.triangleCount * 3 * sizeof(unsigned short));
    }
    m.colors = MemAlloc(m.vertexCount * 4);
    for (int i = 0; i < m.vertexCount; i++) {
        float y = m.vertices[i * 3 + 1], x = m.vertices[i * 3 + 0];
        float v = 0.62f + 0.3f * Clamp(y * 0.5f + 0.5f, 0.0f, 1.0f) + 0.08f * x; // luz desde arriba y el este
        unsigned char c = (unsigned char)(255.0f * Clamp(v, 0.0f, 1.0f));
        m.colors[i * 4 + 0] = c, m.colors[i * 4 + 1] = c, m.colors[i * 4 + 2] = (unsigned char)fminf(255.0f, c * 1.03f + 4.0f);
        m.colors[i * 4 + 3] = 255;
    }
    UnloadMesh(s);
    UploadMesh(&m, false);
    g_puff = LoadModelFromMesh(m);
    g_ready = true;
}

void clouds_unload(void) {
    if (g_ready) UnloadModel(g_puff);
    g_ready = false;
}

float clouds_base(const Terrain *t) { return t->plain + CLOUD_ABOVE_PLAIN; }

// El viento arrastra la capa: se integra la velocidad (asi un cambio de viento no la hace saltar).
static void advance(float time, float wind) {
    float dt = time - g_last_time;
    if (g_last_time < 0.0f || dt < 0.0f || dt > 30.0f) { // primera vez o salto (cargar partida)
        g_drift = Vector2Scale(WIND_DIR, 6.0f * time);
    } else {
        g_drift = Vector2Add(g_drift, Vector2Scale(WIND_DIR, (3.0f + 14.0f * wind) * dt));
    }
    g_last_time = time;
}

// La nube de una celda (en el marco del viento), o false si no hay. puffs: x, y, z, radio.
static int cell_cloud(int cx, int cz, uint32_t seed, float base, float cover, float out[PUFFS_MAX][4]) {
    uint32_t h = hash3(cx, cz, seed);
    float chance = 0.06f + 0.85f * cover;
    if (unit(h, 0) > chance) return 0;
    int n = 2 + (int)(unit(h, 8) * (PUFFS_MAX - 1.0f));
    float size = (0.55f + 0.45f * unit(h, 16)) * (0.7f + 0.5f * cover);
    float ox = (cx + 0.2f + 0.6f * unit(h, 24)) * CELL, oz = (cz + 0.2f + 0.6f * unit(hash3(cz, cx, seed), 0)) * CELL;
    for (int i = 0; i < n; i++) {
        uint32_t k = hash3(cx * 7 + i, cz * 13 - i, seed ^ 0x9E37u);
        float a = unit(k, 0) * 2.0f * PI, d = unit(k, 8) * 55.0f * size;
        out[i][0] = ox + cosf(a) * d;
        out[i][2] = oz + sinf(a) * d;
        out[i][3] = (26.0f + 26.0f * unit(k, 16)) * size;
        out[i][1] = base + out[i][3] * 0.2f + 6.0f * unit(k, 24);
    }
    return n;
}

static Color mixc(Color a, Color b, float k) {
    k = Clamp(k, 0.0f, 1.0f);
    return (Color){ (unsigned char)Lerp(a.r, b.r, k), (unsigned char)Lerp(a.g, b.g, k), (unsigned char)Lerp(a.b, b.b, k), 255 };
}

static void draw_puff(Vector3 p, float r, Color c) {
    DrawModelEx(g_puff, p, (Vector3){ 0, 1, 0 }, 0.0f, (Vector3){ r, r * 0.42f, r }, c);
}

// Los gorros de las cumbres que llegan a la capa: bollos alrededor de la cima, que el viento
// mece pero no se lleva (la montaña los forma).
static int peak_caps(const Terrain *t, float time, float cover, float base, float out[][4], int max) {
    const World *w = t->world;
    int n = 0;
    for (int i = 0; i < w->peak_count; i++) {
        const Peak *pk = &w->peaks[i];
        float top = world_height(w, pk->x, pk->z);
        if (top < base - 12.0f) continue;
        float size = 0.8f + 0.5f * cover;
        for (int k = 0; k < CAP_PUFFS && n < max; k++, n++) {
            float a = k * (2.0f * PI / CAP_PUFFS) + 0.15f * sinf(time * 0.01f + i);
            float d = (30.0f + 18.0f * (k & 1)) * size;
            out[n][0] = pk->x + cosf(a) * d + WIND_DIR.x * 25.0f;
            out[n][2] = pk->z + sinf(a) * d + WIND_DIR.y * 25.0f;
            out[n][1] = fminf(top - 6.0f, base + 10.0f) + 5.0f * (k % 3);
            out[n][3] = (34.0f + 10.0f * (k % 3)) * size;
        }
    }
    return n;
}

void clouds_draw(const Terrain *t, Camera3D cam, float time, float cover, float wind, Color haze) {
    ensure_puff();
    advance(time, wind);
    float base = clouds_base(t);
    Color white = mixc((Color){ 246, 247, 250, 255 }, (Color){ 158, 162, 170, 255 }, (cover - 0.4f) / 0.6f);
    // La camara en el marco del viento.
    float lx = cam.position.x - g_drift.x, lz = cam.position.z - g_drift.y;
    int r = (int)(CLOUD_VIEW / CELL) + 1, c0x = (int)floorf(lx / CELL), c0z = (int)floorf(lz / CELL);
    float puffs[PUFFS_MAX][4];
    for (int dz = -r; dz <= r; dz++)
        for (int dx = -r; dx <= r; dx++) {
            int n = cell_cloud(c0x + dx, c0z + dz, t->seed, base, cover, puffs);
            for (int i = 0; i < n; i++) {
                Vector3 p = { puffs[i][0] + g_drift.x, puffs[i][1], puffs[i][2] + g_drift.y };
                float d = Vector2Distance((Vector2){ p.x, p.z }, (Vector2){ cam.position.x, cam.position.z });
                if (d > CLOUD_VIEW) continue;
                draw_puff(p, puffs[i][3], mixc(white, haze, (d - 500.0f) / (CLOUD_VIEW - 500.0f)));
            }
        }
    float caps[64][4];
    int n = peak_caps(t, time, cover, base, caps, 64);
    for (int i = 0; i < n; i++) {
        Vector3 p = { caps[i][0], caps[i][1], caps[i][2] };
        float d = Vector2Distance((Vector2){ p.x, p.z }, (Vector2){ cam.position.x, cam.position.z });
        if (d > CLOUD_VIEW * 1.4f) continue;
        draw_puff(p, caps[i][3], mixc(white, haze, (d - 700.0f) / (CLOUD_VIEW * 1.4f - 700.0f)));
    }
}

// Dentro de un bollo (elipsoide un poco agrandado): 1 en el centro, 0 en la orilla.
static float inside(Vector3 pos, const float pf[4]) {
    float r = pf[3] * 1.15f;
    float dx = (pos.x - pf[0]) / r, dy = (pos.y - pf[1]) / (r * 0.42f), dz = (pos.z - pf[2]) / r;
    return Clamp(1.4f * (1.0f - sqrtf(dx * dx + dy * dy + dz * dz)), 0.0f, 1.0f);
}

float clouds_mist(const Terrain *t, Vector3 pos, float time, float cover) {
    float base = clouds_base(t);
    if (pos.y < base - 30.0f || pos.y > base + 60.0f) return 0.0f;
    float mist = 0.0f, puffs[PUFFS_MAX][4];
    float lx = pos.x - g_drift.x, lz = pos.z - g_drift.y;
    int c0x = (int)floorf(lx / CELL), c0z = (int)floorf(lz / CELL);
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            int n = cell_cloud(c0x + dx, c0z + dz, t->seed, base, cover, puffs);
            for (int i = 0; i < n; i++) {
                float pf[4] = { puffs[i][0] + g_drift.x, puffs[i][1], puffs[i][2] + g_drift.y, puffs[i][3] };
                mist = fmaxf(mist, inside(pos, pf));
            }
        }
    float caps[64][4];
    int n = peak_caps(t, time, cover, base, caps, 64);
    for (int i = 0; i < n; i++) mist = fmaxf(mist, inside(pos, caps[i]));
    return mist;
}

void clouds_draw_mist(float mist, int w, int h) {
    if (mist <= 0.01f) return;
    DrawRectangle(0, 0, w, h, (Color){ 226, 230, 236, (unsigned char)(235.0f * mist) });
}
