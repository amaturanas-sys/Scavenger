#include "game/cave_look.h"

#include <math.h>
#include <string.h>

#include "rlgl.h"
#include "sim/world.h"
#include "world/voxcave.h"

#define TEX 128 // px de cada textura

// Las texturas que se pintan: la roca de cada tipo, la sillería y el ladrillo del imperio, y los suelos.
typedef enum { TX_LIMESTONE, TX_MOSSY, TX_HEWN, TX_SMOOTH, TX_ASHLAR, TX_BRICK, TX_GRAVEL, TX_MUD, TX_EARTH, TX_RED_EARTH, TX_FLAGS } TexKind;

// El plan de cada tipo, la superficie y la luz (las paletas, de las referencias de docs/PLAN_CUEVAS_GRANDES.md):
//  - las de fieras: caliza gris azulada y mojada (la 10, estalactitas y lago), bruma azul oscura;
//  - las abandonadas: roca verdosa con musgo y la niebla verde del tunel de sillares (la 9);
//  - los forajidos: roca parda picada a medias, con hollin (el calabozo, la 6; la antorcha, la 3), tierra con paja;
//  - la tribu: roca ocre gastada (las pinturas, la 4 y la 5), tierra roja apisonada;
//  - el imperio: sillares y una cupula de ladrillo (la 7 y la 8), losas en el suelo.
static const struct {
    Color wall, vault, floor, moss;
    float region; // cuanto de la roca de la region
    float ambient[3];
    Color fog;
    float fog_near, fog_far, fog_max;
    TexKind tex[CL_SURFACES];
    float tile[CL_SURFACES];
    float jag, moss_h, soot;
} KIND_LOOK[CAVE_KINDS] = {
    { { 92, 104, 116, 255 }, { 112, 122, 132, 255 }, { 70, 76, 82, 255 }, { 52, 64, 78, 255 }, 0.35f, { 0.04f, 0.05f, 0.078f },
      { 16, 22, 32, 255 }, 10.0f, 40.0f, 0.75f, { TX_LIMESTONE, TX_LIMESTONE, TX_GRAVEL }, { 4.0f, 4.0f, 4.0f }, 1.0f, 1.3f, 0.0f },
    { { 100, 112, 102, 255 }, { 94, 106, 98, 255 }, { 74, 80, 66, 255 }, { 66, 104, 60, 255 }, 0.4f, { 0.034f, 0.054f, 0.05f },
      { 18, 40, 38, 255 }, 6.0f, 32.0f, 0.75f, { TX_MOSSY, TX_MOSSY, TX_MUD }, { 4.0f, 4.0f, 4.0f }, 0.8f, 1.9f, 0.0f },
    { { 146, 118, 90, 255 }, { 116, 94, 74, 255 }, { 122, 96, 70, 255 }, { 84, 66, 50, 255 }, 0.4f, { 0.042f, 0.032f, 0.025f },
      { 18, 12, 8, 255 }, 10.0f, 42.0f, 0.8f, { TX_HEWN, TX_HEWN, TX_EARTH }, { 4.0f, 4.0f, 4.0f }, 0.6f, 0.9f, 0.75f },
    { { 184, 140, 110, 255 }, { 168, 124, 96, 255 }, { 134, 88, 64, 255 }, { 168, 104, 72, 255 }, 0.35f, { 0.046f, 0.033f, 0.026f },
      { 22, 13, 8, 255 }, 10.0f, 42.0f, 0.8f, { TX_SMOOTH, TX_SMOOTH, TX_RED_EARTH }, { 4.0f, 4.0f, 4.0f }, 0.35f, 1.4f, 0.35f },
    { { 160, 140, 118, 255 }, { 128, 106, 96, 255 }, { 116, 110, 100, 255 }, { 100, 92, 82, 255 }, 0.15f, { 0.038f, 0.038f, 0.042f },
      { 16, 15, 16, 255 }, 12.0f, 46.0f, 0.75f, { TX_ASHLAR, TX_BRICK, TX_FLAGS }, { 4.0f, 3.0f, 4.0f }, 0.0f, 0.55f, 0.15f },
};

static ClLook g_look;
static Texture2D g_tex[CL_SURFACES];
static bool g_tex_ok[CL_SURFACES];
static Color g_px[TEX * TEX];

// ---------------------------------------------------------------- ruido que se repite sin costura
static uint32_t hash3(int x, int y, uint32_t seed) {
    uint32_t h = (uint32_t)x * 0x8DA6B343u ^ (uint32_t)y * 0xD8163841u ^ seed * 0xCB1AB31Fu;
    h ^= h >> 13, h *= 0x5BD1E995u, h ^= h >> 15;
    return h;
}

static float lattice(int x, int y, int px, int py, uint32_t seed) {
    x = ((x % px) + px) % px, y = ((y % py) + py) % py;
    return (float)(hash3(x, y, seed) & 0xFFFFu) / 65535.0f;
}

// Ruido de valor en (u, v) de la textura (0..1), con px x py celdas por vuelta: se repite al borde.
static float vnoise(float u, float v, int px, int py, uint32_t seed) {
    float x = u * (float)px, y = v * (float)py;
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - (float)ix, fy = y - (float)iy;
    fx = fx * fx * (3.0f - 2.0f * fx), fy = fy * fy * (3.0f - 2.0f * fy);
    float a = lattice(ix, iy, px, py, seed), b = lattice(ix + 1, iy, px, py, seed);
    float c = lattice(ix, iy + 1, px, py, seed), d = lattice(ix + 1, iy + 1, px, py, seed);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

static float fbm(float u, float v, int base, int octaves, uint32_t seed) {
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    for (int o = 0, p = base; o < octaves; o++, p *= 2, amp *= 0.5f) {
        sum += amp * vnoise(u, v, p, p, seed + (uint32_t)o * 31u);
        norm += amp;
    }
    return sum / norm;
}

// Las dos semillas mas cercanas de una rejilla de n x n celdas con una semilla corrida en cada una (Worley): f1 y f2 (en
// celdas) y el numero de la celda de la mas cercana.
static void worley(float u, float v, int n, uint32_t seed, float *f1, float *f2, int *id) {
    float x = u * (float)n, y = v * (float)n;
    int ix = (int)floorf(x), iy = (int)floorf(y);
    *f1 = *f2 = 9.0f, *id = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int cx = ix + dx, cy = iy + dy, wx = ((cx % n) + n) % n, wy = ((cy % n) + n) % n;
            float px = (float)cx + 0.15f + 0.7f * lattice(wx, wy, n, n, seed), py = (float)cy + 0.15f + 0.7f * lattice(wx, wy, n, n, seed + 7u);
            float d = sqrtf((x - px) * (x - px) + (y - py) * (y - py));
            if (d < *f1) *f2 = *f1, *f1 = d, *id = wx + wy * n;
            else if (d < *f2) *f2 = d;
        }
}

static float clamp01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
static float smooth(float a, float b, float x) {
    float t = clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

static Color gray(float k, float r, float g, float b) {
    return (Color){ (unsigned char)(255.0f * clamp01(k * r)), (unsigned char)(255.0f * clamp01(k * g)), (unsigned char)(255.0f * clamp01(k * b)), 255 };
}

// Un trazo corto (un golpe de pico, una brizna de paja) en cada celda de una rejilla de n x n: lo cerca que pasa de (u, v)
// (en celdas) y la celda.
static float strokes(float u, float v, int n, uint32_t seed, float len, float *cell) {
    float x = u * (float)n, y = v * (float)n, best = 9.0f;
    int ix = (int)floorf(x), iy = (int)floorf(y);
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int cx = ix + dx, cy = iy + dy, wx = ((cx % n) + n) % n, wy = ((cy % n) + n) % n;
            uint32_t h = hash3(wx, wy, seed);
            float px = (float)cx + (float)(h & 0xFF) / 255.0f, py = (float)cy + (float)((h >> 8) & 0xFF) / 255.0f;
            float a = (float)((h >> 16) & 0xFF) / 255.0f * 3.14159f, ex = cosf(a) * len, ey = sinf(a) * len;
            float t = clamp01(((x - px) * ex + (y - py) * ey) / (ex * ex + ey * ey + 1e-6f));
            float d = sqrtf((x - px - ex * t) * (x - px - ex * t) + (y - py - ey * t) * (y - py - ey * t));
            if (d < best) best = d, *cell = (float)((h >> 24) & 0xFF) / 255.0f;
        }
    return best;
}

// ---------------------------------------------------------------- las texturas
static Color texel(TexKind k, float u, float v, int px, int py, uint32_t seed) {
    switch (k) {
    case TX_LIMESTONE: { // caliza: manchas, las capas (finas, horizontales), las coladas que bajan y alguna grieta suelta
        float base = 0.62f + 0.3f * fbm(u, v, 4, 4, seed), streak = vnoise(u, v, 24, 2, seed + 5u), f1, f2;
        float bed = 0.95f + 0.05f * sinf((v * 7.0f + 0.25f * vnoise(u, v, 4, 4, seed + 8u)) * 6.2832f);
        int id;
        worley(u, v, 5, seed + 9u, &f1, &f2, &id);
        float crack = f2 - f1 < 0.035f && vnoise(u, v, 6, 6, seed + 77u) > 0.56f ? 0.74f : 1.0f;
        return gray(base * bed * (0.82f + 0.3f * streak) * crack, 0.98f, 1.0f, 1.03f);
    }
    case TX_MOSSY: { // roca con grietas sueltas y manchas de musgo
        float base = 0.6f + 0.32f * fbm(u, v, 4, 4, seed), f1, f2, moss = smooth(0.52f, 0.66f, fbm(u, v, 3, 3, seed + 11u));
        int id;
        worley(u, v, 5, seed + 3u, &f1, &f2, &id);
        float k = base * (f2 - f1 < 0.04f && vnoise(u, v, 6, 6, seed + 71u) > 0.5f ? 0.66f : 1.0f);
        return gray(k * (1.0f - 0.08f * moss), 1.0f - 0.3f * moss, 1.0f - 0.05f * moss, 1.0f - 0.38f * moss);
    }
    case TX_HEWN: { // picada a mano: golpes cortos de pico, con su borde claro
        float base = 0.7f + 0.22f * fbm(u, v, 4, 3, seed), cell = 0.0f, d = strokes(u, v, 7, seed, 0.4f, &cell), cell2 = 0.0f;
        float d2 = strokes(u + 0.6f / (float)TEX, v + 0.6f / (float)TEX, 7, seed, 0.4f, &cell2);
        float k = base * (d < 0.05f && cell > 0.3f ? 0.8f : d2 < 0.05f && cell2 > 0.3f ? 1.08f : 1.0f);
        return gray(k, 1.0f, 0.98f, 0.95f);
    }
    case TX_SMOOTH: { // gastada: manchas suaves
        float base = 0.76f + 0.2f * fbm(u, v, 3, 2, seed) + 0.04f * vnoise(u, v, 32, 32, seed + 2u);
        return gray(base, 1.0f, 0.98f, 0.96f);
    }
    case TX_ASHLAR: { // sillares de 1 x 0,5 m (32 x 16 px), trabados; la junta, oscura; cada bloque con su tono y sus golpes
        int row = py / 16, x = px + (row & 1) * 16, bx = x / 32, lx = x % 32, ly = py % 16;
        float block = 0.78f + 0.16f * (float)(hash3(bx % (TEX / 32), row, seed) & 0xFF) / 255.0f;
        float edge = (lx < 1 || ly < 1) ? 0.5f : (lx < 2 || ly < 2 || lx > 30 || ly > 14) ? 0.84f : 1.0f;
        float chip = (lx < 4 || ly < 3 || lx > 28) && vnoise(u, v, 64, 64, seed + 4u) > 0.72f ? 0.82f : 1.0f;
        return gray(block * edge * chip * (0.9f + 0.1f * fbm(u, v, 8, 3, seed + 1u)), 1.0f, 0.98f, 0.95f);
    }
    case TX_BRICK: { // ladrillos de 16 x 8 px, trabados, con la junta clara de cal y algunos mas cocidos (oscuros)
        int row = py / 8, x = px + (row & 1) * 8, bx = x / 16, lx = x % 16, ly = py % 8;
        float hb = (float)(hash3(bx % (TEX / 16), row, seed) & 0xFF) / 255.0f;
        float brick = hb < 0.15f ? 0.62f : 0.76f + 0.2f * hb;
        if (lx < 1 || ly < 1) return gray(0.86f, 1.0f, 1.0f, 0.98f); // la junta
        return gray(brick * (0.92f + 0.08f * vnoise(u, v, 32, 32, seed + 6u)), 1.0f, 0.97f, 0.94f);
    }
    case TX_GRAVEL:
    case TX_MUD: { // grava: piedritas con su borde oscuro (el musgo del barro va aparte: texel_mud)
        float base = 0.62f + 0.24f * fbm(u, v, 4, 3, seed), f1, f2;
        int id;
        worley(u, v, 18, seed + 13u, &f1, &f2, &id);
        float k = f1 < 0.3f ? base * (0.98f + 0.2f * (float)(hash3(id, 1, seed) & 0xFF) / 255.0f) : f1 < 0.36f ? base * 0.72f : base;
        return gray(k, 1.0f, 1.0f, 1.0f);
    }
    case TX_EARTH: { // tierra apisonada con briznas de paja
        float base = 0.7f + 0.22f * fbm(u, v, 4, 3, seed), cell = 0.0f, d = strokes(u, v, 14, seed + 21u, 0.7f, &cell);
        if (d < 0.05f && cell > 0.35f) return gray(1.0f, 1.0f, 0.9f, 0.6f);
        return gray(base, 1.0f, 0.98f, 0.95f);
    }
    case TX_RED_EARTH: { // tierra roja apisonada, lisa, con alguna piedrita
        float base = 0.74f + 0.2f * fbm(u, v, 3, 3, seed), f1, f2;
        int id;
        worley(u, v, 10, seed + 17u, &f1, &f2, &id);
        return gray(f1 < 0.12f ? base * 1.1f : base, 1.0f, 0.97f, 0.95f);
    }
    default: { // TX_FLAGS: losas de piedra de casi un metro, desparejas, con la junta oscura y el medio gastado
        float f1, f2;
        int id;
        worley(u, v, 4, seed + 19u, &f1, &f2, &id);
        if (f2 - f1 < 0.06f) return gray(0.48f, 1.0f, 1.0f, 1.0f);
        float slab = 0.76f + 0.16f * (float)(hash3(id, 3, seed) & 0xFF) / 255.0f, worn = 1.0f + 0.06f * smooth(0.5f, 0.1f, f1);
        return gray(slab * worn * (0.92f + 0.08f * fbm(u, v, 16, 2, seed + 2u)), 1.0f, 0.99f, 0.97f);
    }
    }
}

// El suelo de las abandonadas: grava con manchas de musgo (aparte: el musgo va en el color, no en la grava).
static Color texel_mud(float u, float v, uint32_t seed) {
    Color g = texel(TX_GRAVEL, u, v, 0, 0, seed);
    float moss = smooth(0.5f, 0.68f, fbm(u, v, 3, 3, seed + 29u));
    return (Color){ (unsigned char)((float)g.r * (1.0f - 0.32f * moss)), (unsigned char)((float)g.g * (1.0f - 0.04f * moss)),
                    (unsigned char)((float)g.b * (1.0f - 0.42f * moss)), 255 };
}

static bool paint(TexKind k, uint32_t seed, Texture2D *out) {
    for (int py = 0; py < TEX; py++)
        for (int px = 0; px < TEX; px++) {
            float u = ((float)px + 0.5f) / (float)TEX, v = ((float)py + 0.5f) / (float)TEX;
            g_px[py * TEX + px] = k == TX_MUD ? texel_mud(u, v, seed) : texel(k, u, v, px, py, seed);
        }
    Image img = { .data = g_px, .width = TEX, .height = TEX, .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    *out = LoadTextureFromImage(img);
    if (!out->id) return false;
    SetTextureFilter(*out, TEXTURE_FILTER_POINT); // pixeles nitidos, como el terreno
    SetTextureWrap(*out, TEXTURE_WRAP_REPEAT);
    return true;
}

// ---------------------------------------------------------------- cargar
static Color mixc(Color a, Color b, float t) {
    return (Color){ (unsigned char)((float)a.r + ((float)b.r - (float)a.r) * t), (unsigned char)((float)a.g + ((float)b.g - (float)a.g) * t),
                    (unsigned char)((float)a.b + ((float)b.b - (float)a.b) * t), 255 };
}

bool cl_load(const Cave *c) {
    cl_unload();
    int k = (int)c->kind < 0 || c->kind >= CAVE_KINDS ? CAVE_ANIMAL : (int)c->kind;
    Color region = vc_rock_color((Region)c->region);
    // La roca de la region tiñe un poco: la de una cueva del desierto, mas calida que la de los fiordos. El color de la
    // roca se lleva mas alto que el de las referencias: la textura lo oscurece (su media, ~0,82).
    float w = KIND_LOOK[k].region;
    g_look = (ClLook){ .wall = mixc(KIND_LOOK[k].wall, region, w), .vault = mixc(KIND_LOOK[k].vault, region, w),
                       .floor = mixc(KIND_LOOK[k].floor, ColorBrightness(region, -0.2f), w * 0.7f), .moss = KIND_LOOK[k].moss,
                       .ambient = { KIND_LOOK[k].ambient[0], KIND_LOOK[k].ambient[1], KIND_LOOK[k].ambient[2] }, .fog = KIND_LOOK[k].fog,
                       .fog_near = KIND_LOOK[k].fog_near, .fog_far = KIND_LOOK[k].fog_far, .fog_max = KIND_LOOK[k].fog_max,
                       .jag = KIND_LOOK[k].jag, .moss_h = KIND_LOOK[k].moss_h, .soot = KIND_LOOK[k].soot };
    for (int s = 0; s < CL_SURFACES; s++) g_look.tile[s] = KIND_LOOK[k].tile[s];
    bool ok = true;
    uint32_t seed = 0xCA7Eu + (uint32_t)k * 101u + (uint32_t)c->region * 13u;
    for (int s = 0; s < CL_SURFACES; s++) {
        TexKind t = KIND_LOOK[k].tex[s];
        bool same = false; // la boveda de las naturales es la misma roca: se comparte
        for (int p = 0; p < s && !same; p++)
            if (g_tex_ok[p] && KIND_LOOK[k].tex[p] == t) g_tex[s] = g_tex[p], same = true;
        if (same) continue;
        g_tex_ok[s] = paint(t, seed + (uint32_t)s * 7u, &g_tex[s]);
        ok = ok && g_tex_ok[s];
    }
    return ok;
}

const ClLook *cl_look(void) { return &g_look; }

Texture2D cl_texture(ClSurface s) {
    if ((unsigned)s < CL_SURFACES && g_tex[s].id) return g_tex[s];
    return (Texture2D){ rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
}

void cl_unload(void) {
    for (int s = 0; s < CL_SURFACES; s++) {
        bool shared = false;
        for (int p = 0; p < s; p++) shared = shared || (g_tex[p].id && g_tex[p].id == g_tex[s].id);
        if (g_tex_ok[s] && !shared) UnloadTexture(g_tex[s]);
    }
    memset(g_tex, 0, sizeof(g_tex));
    memset(g_tex_ok, 0, sizeof(g_tex_ok));
}

void cl_forget_gpu(void) {
    memset(g_tex, 0, sizeof(g_tex));
    memset(g_tex_ok, 0, sizeof(g_tex_ok));
}
