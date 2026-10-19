#include "sim/flora.h"

#include <math.h>
#include <string.h>

#include "sim/city.h"
#include "sim/noise.h"

static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

static float smooth(float a, float b, float x) {
    float t = clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

static uint32_t hash3(int x, int z, uint32_t s) {
    uint32_t h = (uint32_t)x * 0x8DA6B343u ^ (uint32_t)z * 0xD8163841u ^ s * 0xCB1AB31Fu;
    h ^= h >> 13;
    h *= 0x5BD1E995u;
    h ^= h >> 15;
    return h;
}

// ---------------------------------------------------------------- donde crecen
float flora_density(FloraKind kind, const float k[REGION_COUNT], float near_water) {
    float nw = clamp01(near_water);
    switch (kind) {
    case FLORA_GRASS: // en el desierto, solo junto al agua (los oasis)
        return k[REGION_STEPPE] * 0.55f + k[REGION_FOREST] * 0.22f + k[REGION_HIGHLAND] * 0.32f + k[REGION_FJORD] * 0.4f +
               k[REGION_DESERT] * (0.015f + 0.35f * nw);
    case FLORA_STIPA: // la estepa seca: lejos del agua
        return (k[REGION_STEPPE] * 0.24f + k[REGION_HIGHLAND] * 0.04f + k[REGION_DESERT] * 0.02f) * (1.0f - 0.7f * nw);
    case FLORA_FLOWER:
        return k[REGION_STEPPE] * 0.05f + k[REGION_HIGHLAND] * 0.04f + k[REGION_FOREST] * 0.015f + k[REGION_FJORD] * 0.02f;
    case FLORA_FERN:
        return k[REGION_FOREST] * 0.22f + k[REGION_FJORD] * 0.07f;
    case FLORA_REED: // las orillas
        return nw * nw *
               (k[REGION_STEPPE] * 0.5f + k[REGION_FOREST] * 0.35f + k[REGION_HIGHLAND] * 0.12f + k[REGION_FJORD] * 0.2f +
                k[REGION_DESERT] * 0.45f);
    default: return 0.0f;
    }
}

int flora_pick(const float k[REGION_COUNT], float near_water, float r) {
    float d[FLORA_KINDS], total = 0.0f;
    for (int i = 0; i < FLORA_KINDS; i++) total += d[i] = flora_density((FloraKind)i, k, near_water);
    float scale = total > 1.0f ? 1.0f / total : 1.0f; // como mucho una mata por candidato
    for (int i = 0; i < FLORA_KINDS; i++) {
        r -= d[i] * scale;
        if (r < 0.0f) return i;
    }
    return -1;
}

// Lejos de los campamentos de otras tribus, de las estructuras y de los pueblos con sus campos.
static bool cleared(const World *w, float x, float z) {
    float dist;
    int si = world_nearest_settlement(w, x, z, &dist);
    if (si >= 0 && dist < (w->settlements[si].kind == SETTLE_CAPITAL ? CITY_FIELDS_R1 : VILLAGE_FIELDS_R1) + 4.0f) return true;
    for (int i = 0; i < w->tribe_count; i++) {
        float dx = x - w->tribes[i].x, dz = z - w->tribes[i].z;
        if (dx * dx + dz * dz < 20.0f * 20.0f) return true;
    }
    for (int i = 0; i < w->site_count; i++) {
        float dx = x - w->sites[i].x, dz = z - w->sites[i].z;
        if (dx * dx + dz * dz < 10.0f * 10.0f) return true;
    }
    return false;
}

int flora_cell(const World *w, uint32_t seed, int gx, int gz, FloraTuft *out) {
    const float c = FLORA_CELL, x0 = (float)gx * c, z0 = (float)gz * c, mx = x0 + c * 0.5f, mz = z0 + c * 0.5f;
    float r_camp = sqrtf(mx * mx + mz * mz);
    if (r_camp < FLORA_CAMP_R) return 0;
    float edge = world_edge_distance(w, mx, mz);
    if (edge < 20.0f || cleared(w, mx, mz)) return 0;
    float k[REGION_COUNT];
    world_region_weights(w, mx, mz, k);
    if (k[REGION_HIGHLAND] > 0.5f && edge < 150.0f) return 0;     // el muro de hielo del altiplano
    if (world_river_bank(w, mx, mz, NULL) > 0.45f) return 0;      // el lecho de cantos que el rio lava
    // El suelo es un facetado de dos triangulos por celda (src/world/terrain.c: de (x1, z0) a
    // (x0, z1)): la base de cada mata, sobre el triangulo que la tiene.
    float h00 = world_height(w, x0, z0), h10 = world_height(w, x0 + c, z0);
    float h01 = world_height(w, x0, z0 + c), h11 = world_height(w, x0 + c, z0 + c);
    if (k[REGION_FJORD] > 0.5f && (h00 + h10 + h01 + h11) * 0.25f < SEA_LEVEL + 4.0f) return 0; // la playa negra
    WaterKind wk;
    float wl = world_water(w, mx, mz, 0.0f, &wk);
    bool lake = wl > -1e8f && wk != WATER_SEA;
    if (wk == WATER_SEA && wl > -1e8f && (h00 + h10 + h01 + h11) * 0.25f < wl + 1.0f) return 0;
    // La tierra pelada y la roca de las cuestas (como el suelo: 1 - la normal hacia arriba).
    float steep = 0.45f + 0.15f * k[REGION_HIGHLAND];
    float ya = c / sqrtf(c * c + (h10 - h00) * (h10 - h00) + (h01 - h00) * (h01 - h00));
    float yb = c / sqrtf(c * c + (h11 - h01) * (h11 - h01) + (h11 - h10) * (h11 - h10));
    bool bare_a = 1.0f - ya > steep, bare_b = 1.0f - yb > steep;
    if (bare_a && bare_b) return 0;
    float patch = 0.5f + 0.5f * fbm2d(mx * 0.08f, mz * 0.08f, seed + 303u, 2);
    float trodden = smooth(FLORA_CAMP_R, FLORA_CAMP_R + 8.0f, r_camp); // al borde del campamento, menos
    int n = 0;
    for (int i = 0; i < FLORA_PER_CELL; i++) {
        uint32_t h = hash3(gx * FLORA_PER_CELL + i, gz, seed ^ 0xF10A5u);
        float u = 0.04f + 0.92f * (float)(h & 0xFFu) / 255.0f, v = 0.04f + 0.92f * (float)((h >> 8) & 0xFFu) / 255.0f;
        float r = (float)(h >> 16) / 65536.0f;
        bool tri_b = u + v > 1.0f;
        if (tri_b ? bare_b : bare_a) continue;
        float y = tri_b ? h11 + (1.0f - u) * (h01 - h11) + (1.0f - v) * (h10 - h11) : h00 + u * (h10 - h00) + v * (h01 - h00);
        float above = lake ? y - wl : 99.0f;
        if (above < -0.5f) continue; // el agua honda
        if (trodden < 1.0f) r /= fmaxf(trodden, 0.001f);
        int kind = flora_pick(k, lake ? clamp01(1.0f - above / 2.5f) : 0.0f, r);
        if (kind < 0 || (above < 0.0f && kind != FLORA_REED)) continue; // en el agua baja, solo juncos
        uint32_t h2 = hash3(gz, gx * FLORA_PER_CELL + i, seed ^ 0x5EED5u);
        FloraTuft *t = &out[n++];
        memset(t, 0, sizeof(*t)); // tambien el relleno: dos siembras de la misma celda son identicas byte a byte
        t->x = x0 + u * c, t->y = y, t->z = z0 + v * c;
        t->size = 0.7f + 0.6f * (float)(h2 & 0xFFu) / 255.0f;
        t->rnd = (float)(h2 >> 16) / 65536.0f;
        t->patch = patch;
        t->kind = (unsigned char)kind;
        t->forest = (unsigned char)(k[REGION_FOREST] * 255.0f + 0.5f), t->high = (unsigned char)(k[REGION_HIGHLAND] * 255.0f + 0.5f);
        t->fjord = (unsigned char)(k[REGION_FJORD] * 255.0f + 0.5f), t->desert = (unsigned char)(k[REGION_DESERT] * 255.0f + 0.5f);
    }
    return n;
}

// ---------------------------------------------------------------- la estacion
static const float FULL_HEIGHT[FLORA_KINDS] = { 0.5f, 0.85f, 0.32f, 0.65f, 1.6f };

float flora_height(FloraKind kind, float year, float greenness, float autumn) {
    if ((unsigned)kind >= FLORA_KINDS) return 0.0f;
    float g = clamp01(greenness), a = clamp01(autumn), grow;
    switch (kind) {
    case FLORA_FLOWER: // florecen en primavera (con helada, no)
        grow = smooth(0.05f, 0.13f, year) * (1.0f - smooth(0.3f, 0.4f, year)) * clamp01(g * 2.0f);
        break;
    case FLORA_FERN: // se secan en otoño y el invierno los tumba
        grow = 0.12f + 0.88f * clamp01(g * 1.5f + a * 0.6f);
        break;
    case FLORA_REED: // los tallos secos quedan de pie todo el invierno
        grow = 0.7f + 0.3f * clamp01(g + a);
        break;
    default: // el pasto y la stipa: crecen en primavera, se secan de pie y el invierno los aplasta
        grow = 0.3f + 0.7f * clamp01(g * 1.6f + a * 0.9f);
        break;
    }
    return FULL_HEIGHT[kind] * grow;
}

float flora_plume(float year) { return smooth(0.15f, 0.24f, year) * (1.0f - smooth(0.42f, 0.52f, year)); }

// ---------------------------------------------------------------- el viento
float flora_lean(float wind) {
    wind = clamp01(wind);
    return 0.06f + 0.5f * wind * wind;
}

float flora_sway(float t, float wind, float phase) {
    wind = clamp01(wind);
    float amp = 0.05f + 0.22f * wind, freq = 1.4f + 2.4f * wind;
    float gust = 0.6f + 0.4f * sinf(t * 0.45f + phase * 0.37f); // las rafagas: una ola lenta
    return amp * gust * sinf(t * freq + phase);
}

float flora_trample(float d) { return 1.0f - smooth(0.0f, FLORA_TRAMPLE_R, d); }
