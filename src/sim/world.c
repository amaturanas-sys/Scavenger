#include "world.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "animals.h"
#include "lang.h"
#include "noise.h"
#include "rng.h"

#define PI_F 3.14159265f
#define DEG (PI_F / 180.0f)

// Campamento inicial en el origen: el terreno se aplana a su alrededor.
#define CAMP_FLAT_INNER 18.0f
#define CAMP_FLAT_OUTER 42.0f
// El borde: el gran canal (estepa, bosque, altiplano), el mar (fiordos), el muro (desierto).
#define CANAL_E 70.0f     // distancia del eje del canal al borde
#define CANAL_HALF 26.0f  // medio ancho del canal
#define CANAL_DEPTH 4.5f
#define SHORE_E 170.0f    // la costa de los fiordos (sin contar los fiordos que entran)
#define WALL_E 70.0f      // pie del muro del desierto
#define GORGE_E 118.0f    // eje del cañon al pie del muro

static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
static float smooth(float a, float b, float v) {
    float t = clamp01((v - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// ------------------------------------------------------------------ regiones
// Sectores fijos: la estepa al este (+x) y, girando hacia +z, bosque, altiplano, fiordos y desierto.
static const float SECTOR_ANGLE[REGION_COUNT] = { 0.0f, 72.0f * DEG, 144.0f * DEG, 216.0f * DEG, 288.0f * DEG };
static const float ALTITUDE[REGION_COUNT] = { 40.0f, 55.0f, 70.0f, 12.0f, 28.0f }; // la costa, la mas baja; el altiplano, el mas alto
#define SECTOR_HALF (36.0f * DEG)
#define SECTOR_BLEND (7.0f * DEG)

float region_altitude(Region r) { return r >= 0 && r < REGION_COUNT ? ALTITUDE[r] : 0.0f; }

const char *region_name(Region r) {
    static const char *names[REGION_COUNT] = { N_("estepa"), N_("bosque de coníferas"), N_("altiplano glaciar"), N_("costa de fiordos"),
                                               N_("desierto") };
    return r >= 0 && r < REGION_COUNT ? T(names[r]) : "?";
}

const char *world_kingdom_name(Region r) {
    static const char *names[REGION_COUNT] = { N_("Kanato de Hierro"), N_("Principado de los Pinos Negros"), N_("Señorío del Glaciar"),
                                               N_("Jarlazgo de la Costa Helada"), N_("Reino de los Oasis") };
    return r >= 0 && r < REGION_COUNT ? T(names[r]) : "?";
}

int region_habitat(Region r) {
    switch (r) {
    case REGION_FOREST: return HAB_FOREST;
    case REGION_HIGHLAND: return HAB_COLD;
    case REGION_FJORD: return HAB_COAST;
    case REGION_DESERT: return HAB_DESERT;
    default: return HAB_STEPPE;
    }
}

static float angdiff(float a, float b) {
    float d = fmodf(a - b, 2.0f * PI_F);
    if (d < -PI_F) d += 2.0f * PI_F;
    if (d > PI_F) d -= 2.0f * PI_F;
    return fabsf(d);
}

Region world_region_weights(const World *w, float x, float z, float out[REGION_COUNT]) {
    float r = sqrtf(x * x + z * z);
    // Las fronteras se tuercen con ruido (cambian con la semilla), pero el orden no.
    float th = atan2f(z, x) + w->warp_phase + 0.22f * fbm2d(x * 0.0009f, z * 0.0009f, w->seed + 11u, 3);
    float rr = r * (1.0f + 0.18f * fbm2d(x * 0.0012f + 5.0f, z * 0.0012f, w->seed + 13u, 3));
    float center = 1.0f - smooth(0.24f * WORLD_RADIUS, 0.34f * WORLD_RADIUS, rr); // la estepa del centro
    float ow[REGION_COUNT], sum = 0.0f;
    for (int i = 0; i < REGION_COUNT; i++) {
        float d = angdiff(th, SECTOR_ANGLE[i]);
        ow[i] = clamp01((SECTOR_HALF + SECTOR_BLEND - d) / (2.0f * SECTOR_BLEND));
        sum += ow[i];
    }
    Region best = REGION_STEPPE;
    for (int i = 0; i < REGION_COUNT; i++) {
        out[i] = (1.0f - center) * (sum > 0.0f ? ow[i] / sum : 0.0f) + (i == REGION_STEPPE ? center : 0.0f);
        if (out[i] > out[best]) best = (Region)i;
    }
    return best;
}

Region world_region(const World *w, float x, float z) {
    float k[REGION_COUNT];
    return world_region_weights(w, x, z, k);
}

float world_region_weight(const World *w, Region r, float x, float z) {
    float k[REGION_COUNT];
    world_region_weights(w, x, z, k);
    return r >= 0 && r < REGION_COUNT ? k[r] : 0.0f;
}

float world_edge_distance(float x, float z) { return WORLD_RADIUS - sqrtf(x * x + z * z); }

bool world_clamp(float *x, float *z) {
    float r = sqrtf(*x * *x + *z * *z);
    if (r <= WORLD_LIMIT) return false;
    *x *= WORLD_LIMIT / r;
    *z *= WORLD_LIMIT / r;
    return true;
}

// ------------------------------------------------------------------ relieve
static float region_detail(const World *w, Region rg, float x, float z) {
    uint32_t s = w->seed;
    switch (rg) {
    case REGION_FOREST: // lomas amplias y redondeadas
        return fbm2d(x * 0.005f, z * 0.005f, s + 31u, 5) * 15.0f + fbm2d(x * 0.03f, z * 0.03f, s + 32u, 3) * 2.0f;
    case REGION_HIGHLAND: { // meseta alta con cordilleras
        float plateau = fbm2d(x * 0.004f, z * 0.004f, s + 61u, 4) * 10.0f;
        float ridge = 1.0f - fabsf(fbm2d(x * 0.0045f, z * 0.0045f, s + 202u, 4));
        float mask = smooth(-0.15f, 0.35f, fbm2d(x * 0.0015f, z * 0.0015f, s + 101u, 3));
        return plateau + ridge * ridge * 40.0f * mask;
    }
    case REGION_FJORD: { // costa abrupta
        float rough = fbm2d(x * 0.007f, z * 0.007f, s + 41u, 4) * 14.0f;
        float ridge = 1.0f - fabsf(fbm2d(x * 0.006f, z * 0.006f, s + 43u, 4));
        return rough + ridge * ridge * 22.0f;
    }
    case REGION_DESERT: { // dunas y mesetas de bordes cortados
        float dunes = (1.0f - fabsf(noise2d(x * 0.03f + z * 0.012f, z * 0.02f, s + 505u))) * 2.4f;
        float m = fbm2d(x * 0.003f, z * 0.003f, s + 51u, 3);
        float mesa = smooth(0.22f, 0.27f, m) * 16.0f + smooth(0.42f, 0.46f, m) * 10.0f;
        return dunes + mesa + fbm2d(x * 0.008f, z * 0.008f, s + 52u, 3) * 3.0f;
    }
    default: // estepa: colinas suaves
        return fbm2d(x * 0.006f, z * 0.006f, s, 5) * 9.0f + fbm2d(x * 0.04f, z * 0.04f, s + 17u, 3) * 1.2f;
    }
}

// Fiordos: el mar entra tierra adentro en brazos estrechos (picos en el angulo).
static float fjord_inlet(const World *w, float th) {
    float f = noise2d(cosf(th) * 7.0f, sinf(th) * 7.0f, w->seed + 71u);
    float g = 1.0f - fabsf(f);
    float h = noise2d(cosf(th) * 23.0f, sinf(th) * 23.0f, w->seed + 72u) * 0.5f + 0.5f; // largo de cada fiordo
    return (220.0f + 300.0f * h) * powf(g, 16.0f);
}

// El terreno sin agua: regiones, montañas y el borde (mar, muro y cañon).
static float land(const World *w, float x, float z) {
    float k[REGION_COUNT];
    world_region_weights(w, x, z, k);
    float h = 0.0f;
    for (int i = 0; i < REGION_COUNT; i++)
        if (k[i] > 0.001f) h += k[i] * (ALTITUDE[i] + region_detail(w, (Region)i, x, z));
    for (int i = 0; i < w->peak_count; i++) { // macizos (su sitio cambia con la semilla)
        const Peak *p = &w->peaks[i];
        float dx = x - p->x, dz = z - p->z, d2 = (dx * dx + dz * dz) / (p->r * p->r);
        if (d2 >= 1.0f) continue;
        // Un cerro de laderas concavas y cresta quebrada (no una cupula).
        float f = 1.0f - sqrtf(d2);
        float crest = 1.0f - fabsf(fbm2d(x * 0.012f, z * 0.012f, w->seed + 81u + (uint32_t)i, 3));
        h += p->h * powf(f, 1.6f) * (0.55f + 0.6f * crest);
    }
    float e = world_edge_distance(x, z), th = atan2f(z, x);
    if (e < 520.0f) {
        float fj = 0.0f, de = 0.0f;
        { // pesos del borde por sector (sin el centro)
            float th2 = th + w->warp_phase + 0.22f * fbm2d(x * 0.0009f, z * 0.0009f, w->seed + 11u, 3);
            fj = clamp01((SECTOR_HALF + SECTOR_BLEND - angdiff(th2, SECTOR_ANGLE[REGION_FJORD])) / (2.0f * SECTOR_BLEND));
            de = clamp01((SECTOR_HALF + SECTOR_BLEND - angdiff(th2, SECTOR_ANGLE[REGION_DESERT])) / (2.0f * SECTOR_BLEND));
        }
        if (fj > 0.0f) { // la costa baja al mar; los fiordos entran
            float shore = SHORE_E + fjord_inlet(w, th);
            float t = smooth(shore + 60.0f, shore - 25.0f, e);
            float sea = -6.0f - 14.0f * smooth(shore, shore - 220.0f, e);
            h = lerpf(h, lerpf(h, sea, t), fj);
        }
        if (de > 0.0f) { // cañon al pie y un muro escalonado enorme
            float g = (e - GORGE_E) / 34.0f;
            float gorge = g * g < 1.0f ? (1.0f - g * g) * 30.0f : 0.0f;
            float t = smooth(WALL_E + 6.0f, WALL_E - 45.0f, e) * 6.0f; // seis estratos
            float step = floorf(t), frac = t - step;
            float wall = (step + smooth(0.75f, 1.0f, frac)) / 6.0f * 140.0f;
            h = lerpf(h, h - gorge + wall, de);
        }
    }
    return h;
}

// ------------------------------------------------------------------ agua
static float canal_weight(const World *w, float x, float z) {
    float th = atan2f(z, x) + w->warp_phase + 0.22f * fbm2d(x * 0.0009f, z * 0.0009f, w->seed + 11u, 3);
    float fj = clamp01((SECTOR_HALF + SECTOR_BLEND - angdiff(th, SECTOR_ANGLE[REGION_FJORD])) / (2.0f * SECTOR_BLEND));
    float de = clamp01((SECTOR_HALF + SECTOR_BLEND - angdiff(th, SECTOR_ANGLE[REGION_DESERT])) / (2.0f * SECTOR_BLEND));
    return 1.0f - fj - de;
}

static float canal_level(const World *w, float x, float z) {
    float a = atan2f(z, x) / DEG;
    if (a < 0.0f) a += 360.0f;
    int i = (int)a % CANAL_SAMPLES;
    float f = a - floorf(a);
    return lerpf(w->canal_level[i], w->canal_level[(i + 1) % CANAL_SAMPLES], f);
}

// Distancia de (x, z) al rio y nivel del agua en el punto mas cercano.
static float river_dist(const River *r, float x, float z, float *level) {
    float best = 1e9f;
    for (int i = 0; i + 1 < r->n; i++) {
        float ax = r->x[i], az = r->z[i], bx = r->x[i + 1], bz = r->z[i + 1];
        float vx = bx - ax, vz = bz - az, l2 = vx * vx + vz * vz;
        float t = l2 > 0.0f ? clamp01(((x - ax) * vx + (z - az) * vz) / l2) : 0.0f;
        float px = ax + vx * t - x, pz = az + vz * t - z, d = sqrtf(px * px + pz * pz);
        if (d < best) best = d, *level = lerpf(r->level[i], r->level[i + 1], t);
    }
    return best;
}

static float lake_dist(const World *w, const Lake *l, float x, float z) {
    float dx = x - l->x, dz = z - l->z;
    float wob = 1.0f + 0.28f * noise2d(atan2f(dz, dx) * 1.7f, l->r * 0.01f, w->seed + 91u); // orilla irregular
    return sqrtf(dx * dx + dz * dz) / (l->r * wob);
}

float world_height(const World *w, float x, float z) {
    float h = land(w, x, z);
    float e = world_edge_distance(x, z);
    // El gran canal: un cauce ancho a lo largo del borde (salvo en la costa y el desierto).
    if (e > CANAL_E - CANAL_HALF - 20.0f && e < CANAL_E + CANAL_HALF + 20.0f) {
        float cw = canal_weight(w, x, z);
        if (cw > 0.0f) {
            float d = fabsf(e - CANAL_E), lv = canal_level(w, x, z);
            float bed = d < CANAL_HALF ? lv - 0.5f - CANAL_DEPTH * (1.0f - (d / CANAL_HALF) * (d / CANAL_HALF))
                                       : lerpf(lv + 0.3f, fmaxf(h, lv + 0.3f), smooth(CANAL_HALF, CANAL_HALF + 20.0f, d));
            h = lerpf(h, fminf(h, bed), cw);
        }
    }
    for (int i = 0; i < w->lake_count; i++) { // lagos: cuencas con la orilla suave
        const Lake *l = &w->lakes[i];
        float dx = x - l->x, dz = z - l->z;
        if (dx * dx + dz * dz > l->r * l->r * 3.2f) continue;
        float d = lake_dist(w, l, x, z), depth = l->oasis ? 2.0f : 5.0f;
        if (d < 1.0f) h = fminf(h, l->level - 0.6f - depth * (1.0f - d * d));
        else if (d < 1.5f) h = fminf(h, lerpf(l->level - 0.6f, h, smooth(1.0f, 1.5f, d)));
    }
    for (int i = 0; i < w->river_count; i++) { // rios tributarios
        const River *r = &w->rivers[i];
        float m = r->width * 2.5f;
        if (x < r->minx - m || x > r->maxx + m || z < r->minz - m || z > r->maxz + m) continue;
        float lv = 0.0f, d = river_dist(r, x, z, &lv);
        if (d < r->width) h = fminf(h, lv - 0.4f - 2.2f * (1.0f - (d / r->width) * (d / r->width)));
        else if (d < r->width * 2.4f) h = fminf(h, lerpf(lv + 0.2f, fmaxf(h, lv + 0.2f), smooth(r->width, r->width * 2.4f, d)));
    }
    // El llano del campamento.
    float d = sqrtf(x * x + z * z);
    if (d < CAMP_FLAT_OUTER) h = lerpf(w->camp_height, h, smooth(CAMP_FLAT_INNER, CAMP_FLAT_OUTER, d));
    return h;
}

float world_water(const World *w, float x, float z, float flood, WaterKind *kind) {
    float best = -1e9f;
    WaterKind k = WATER_NONE;
    float e = world_edge_distance(x, z);
    if (e < SHORE_E + 460.0f && world_region_weight(w, REGION_FJORD, x, z) > 0.05f && land(w, x, z) < SEA_LEVEL + 1.0f)
        best = SEA_LEVEL, k = WATER_SEA;
    if (e > CANAL_E - CANAL_HALF - 4.0f && e < CANAL_E + CANAL_HALF + 4.0f && canal_weight(w, x, z) > 0.5f) {
        float lv = canal_level(w, x, z) + 0.5f * flood;
        if (lv > best) best = lv, k = WATER_CANAL;
    }
    for (int i = 0; i < w->lake_count; i++) {
        const Lake *l = &w->lakes[i];
        float dx = x - l->x, dz = z - l->z;
        if (dx * dx + dz * dz > l->r * l->r * 3.2f) continue;
        if (lake_dist(w, l, x, z) < 1.3f && l->level + flood > best) best = l->level + flood, k = WATER_LAKE;
    }
    for (int i = 0; i < w->river_count; i++) {
        const River *r = &w->rivers[i];
        float m = r->width * 1.5f;
        if (x < r->minx - m || x > r->maxx + m || z < r->minz - m || z > r->maxz + m) continue;
        float lv = 0.0f, d = river_dist(r, x, z, &lv);
        if (d < r->width * 1.25f && lv + 0.5f * flood > best) best = lv + 0.5f * flood, k = WATER_RIVER;
    }
    if (kind) *kind = k;
    return best;
}

// ------------------------------------------------------------------ generacion
static void polar(float r, float a, float *x, float *z) {
    *x = cosf(a) * r;
    *z = sinf(a) * r;
}

static bool far_from_settlements(const World *w, float x, float z, float min) {
    for (int i = 0; i < w->settlement_count; i++) {
        float dx = x - w->settlements[i].x, dz = z - w->settlements[i].z;
        if (dx * dx + dz * dz < min * min) return false;
    }
    return true;
}

static bool dry_spot(const World *w, float x, float z) {
    float h = world_height(w, x, z);
    if (world_water(w, x, z, 3.0f, NULL) > h - 0.5f) return false;
    // Ni en una pendiente fuerte.
    float hx = world_height(w, x + 6.0f, z), hz = world_height(w, x, z + 6.0f);
    return fabsf(hx - h) < 4.0f && fabsf(hz - h) < 4.0f;
}

// Un sitio de la region dominante (muy adentro de ella), seco y llano. false si no hay.
static bool find_spot(World *w, Rng *rng, Region rg, float rmin, float rmax, float *ox, float *oz) {
    for (int tries = 0; tries < 400; tries++) {
        float a = SECTOR_ANGLE[rg] - w->warp_phase + (rng_float(rng) - 0.5f) * 2.0f * SECTOR_HALF;
        float r = rmin + rng_float(rng) * (rmax - rmin);
        if (rg == REGION_STEPPE && rng_float(rng) < 0.5f) a = rng_float(rng) * 2.0f * PI_F, r = 300.0f + rng_float(rng) * 450.0f; // o en el centro
        float x, z;
        polar(r, a, &x, &z);
        if (world_region_weight(w, rg, x, z) < 0.85f || !dry_spot(w, x, z)) continue;
        *ox = x, *oz = z;
        return true;
    }
    return false;
}

static void make_name(Rng *rng, Region rg, char *out, int len) {
    static const char *A[REGION_COUNT][6] = {
        { "Ulaan", "Khar", "Bor", "Tem", "Sar", "Olon" },     { "Vel", "Drev", "Smol", "Bel", "Kor", "Les" },
        { "Tash", "Ak", "Mur", "Kok", "Ozen", "Dzhun" },     { "Hrafn", "Sker", "Vik", "Ulf", "Sigr", "Haf" },
        { "Qal", "Zar", "Mar", "Sab", "Tad", "Kas" },
    };
    static const char *B[REGION_COUNT][6] = {
        { "-Bulag", "-Tolgoi", "-Khot", "-Ger", "-Nuur", "-Ovoo" }, { "grad", "ovo", "insk", "ets", "ava", "ok" },
        { "-Tau", "-Kel", "-Bel", "-Su", "-Tor", "-Dag" },          { "nes", "vik", "fjord", "holm", "stad", "by" },
        { "abad", "kand", "ira", "esh", "oun", "ar" },
    };
    snprintf(out, (size_t)len, "%s%s", A[rg][rng_range(rng, 6)], B[rg][rng_range(rng, 6)]);
}

// Especies de las guaridas de cada region.
static int den_species(Rng *rng, Region rg) {
    static const int S[REGION_COUNT][4] = {
        { SPECIES_WOLF, SPECIES_TIGER, SPECIES_BOAR, SPECIES_COYOTE }, { SPECIES_BEAR, SPECIES_WOLF, SPECIES_PUMA, SPECIES_BOAR },
        { SPECIES_WOLF, SPECIES_PUMA, SPECIES_BEAR, SPECIES_WOLF },     { SPECIES_BEAR, SPECIES_WOLF, SPECIES_BEAR, SPECIES_WOLF },
        { SPECIES_HYENA, SPECIES_COYOTE, SPECIES_HYENA, SPECIES_DOG },
    };
    return S[rg][rng_range(rng, 4)];
}

static void add_lake(World *w, float x, float z, float r, bool oasis) {
    if (w->lake_count >= LAKES_MAX) return;
    Lake *l = &w->lakes[w->lake_count];
    l->x = x, l->z = z, l->r = r, l->oasis = oasis;
    // El nivel: algo por debajo del punto mas bajo de la orilla (no desborda).
    float rim = 1e9f;
    for (int k = 0; k < 16; k++) {
        float a = (float)k / 16.0f * 2.0f * PI_F;
        rim = fminf(rim, land(w, x + cosf(a) * r * 1.15f, z + sinf(a) * r * 1.15f));
    }
    l->level = rim - 0.8f;
    w->lake_count++;
}

static bool lake_clear(const World *w, float x, float z, float r) {
    for (int i = 0; i < w->lake_count; i++) {
        float dx = x - w->lakes[i].x, dz = z - w->lakes[i].z, min = r + w->lakes[i].r + 120.0f;
        if (dx * dx + dz * dz < min * min) return false;
    }
    return x * x + z * z > (r + 120.0f) * (r + 120.0f); // lejos del llano del campamento
}

void world_generate(World *w, uint32_t seed) {
    memset(w, 0, sizeof(*w));
    w->seed = seed;
    Rng rng;
    rng_seed(&rng, seed ^ 0x5EEDu);
    w->warp_phase = (rng_float(&rng) - 0.5f) * 10.0f * DEG; // las fronteras giran un poco
    // Montañas: el altiplano tiene las mas grandes; la costa y el bosque algunas; la estepa, lomas.
    static const struct { Region rg; int min, extra; float hmin, hmax, rmin, rmax; } PEAK_RULES[] = {
        { REGION_HIGHLAND, 4, 3, 55.0f, 95.0f, 120.0f, 230.0f }, // sus cumbres guardan el glaciar { REGION_FJORD, 3, 2, 30.0f, 55.0f, 90.0f, 150.0f },
        { REGION_FOREST, 2, 2, 22.0f, 40.0f, 100.0f, 170.0f },   { REGION_STEPPE, 1, 2, 14.0f, 26.0f, 110.0f, 160.0f },
    };
    for (size_t k = 0; k < sizeof(PEAK_RULES) / sizeof(PEAK_RULES[0]); k++) {
        int n = PEAK_RULES[k].min + rng_range(&rng, PEAK_RULES[k].extra + 1);
        for (int i = 0; i < n && w->peak_count < PEAKS_MAX; i++) {
            for (int tries = 0; tries < 60; tries++) {
                Region rg = PEAK_RULES[k].rg;
                float a = SECTOR_ANGLE[rg] - w->warp_phase + (rng_float(&rng) - 0.5f) * 1.6f * SECTOR_HALF;
                float r = (rg == REGION_STEPPE ? 0.3f : 0.4f) * WORLD_RADIUS + rng_float(&rng) * 0.42f * WORLD_RADIUS;
                float x, z;
                polar(r, a, &x, &z);
                if (x * x + z * z < 450.0f * 450.0f) continue; // ni junto al campamento
                Peak *p = &w->peaks[w->peak_count++];
                p->x = x, p->z = z;
                p->h = PEAK_RULES[k].hmin + rng_float(&rng) * (PEAK_RULES[k].hmax - PEAK_RULES[k].hmin);
                p->r = PEAK_RULES[k].rmin + rng_float(&rng) * (PEAK_RULES[k].rmax - PEAK_RULES[k].rmin);
                break;
            }
        }
    }
    w->camp_height = land(w, 0.0f, 0.0f);
    // El gran canal: su nivel sigue el terreno del borde (suavizado), un poco por debajo.
    float raw[CANAL_SAMPLES];
    for (int i = 0; i < CANAL_SAMPLES; i++) {
        float x, z;
        polar(WORLD_RADIUS - CANAL_E, (float)i * DEG, &x, &z);
        raw[i] = land(w, x, z) - 3.0f;
    }
    for (int i = 0; i < CANAL_SAMPLES; i++) {
        float s = 0.0f;
        for (int k = -6; k <= 6; k++) s += raw[(i + k + CANAL_SAMPLES) % CANAL_SAMPLES];
        w->canal_level[i] = fminf(s / 13.0f, raw[i]);
    }
    // Rios tributarios: nacen en el canal y entran hacia el centro, serpenteando.
    int rivers = 6 + rng_range(&rng, 4);
    for (int i = 0; i < rivers * 6 && w->river_count < rivers; i++) {
        float a = rng_float(&rng) * 2.0f * PI_F, x, z;
        polar(WORLD_RADIUS - CANAL_E, a, &x, &z);
        if (canal_weight(w, x, z) < 0.9f) continue;
        bool close = false; // separados entre si
        for (int k = 0; k < w->river_count; k++) close |= angdiff(a, atan2f(w->rivers[k].z[0], w->rivers[k].x[0])) < 14.0f * DEG;
        if (close) continue;
        River *rv = &w->rivers[w->river_count];
        rv->width = 5.0f + rng_float(&rng) * 5.0f;
        float len = 26.0f + rng_float(&rng) * 10.0f, r = WORLD_RADIUS - CANAL_E, ang = a;
        float stop = 520.0f + rng_float(&rng) * 700.0f;
        rv->n = 0;
        for (int k = 0; k < RIVER_PTS && r > stop; k++) {
            polar(r, ang, &x, &z);
            if (k > 0 && (world_region_weight(w, REGION_FJORD, x, z) > 0.3f || world_region_weight(w, REGION_DESERT, x, z) > 0.3f)) break;
            rv->x[rv->n] = x, rv->z[rv->n] = z;
            rv->level[rv->n] = land(w, x, z) - 1.4f;
            rv->n++;
            r -= len;
            ang += 0.55f * noise2d((float)k * 0.3f, (float)w->river_count * 3.1f, seed + 77u) * len / r; // meandros
        }
        if (rv->n < 6) continue;
        // El agua baja hacia el canal y nunca sube sobre la orilla: cada tramo, a lo sumo
        // tan alto como la orilla mas baja de alli hacia adentro.
        float m = 1e9f;
        for (int k = rv->n - 1; k >= 0; k--) {
            m = fminf(m, rv->level[k]);
            rv->level[k] = m;
        }
        rv->level[0] = fminf(rv->level[0], w->canal_level[(int)((a < 0 ? a + 2 * PI_F : a) / DEG) % CANAL_SAMPLES]);
        rv->minx = rv->maxx = rv->x[0], rv->minz = rv->maxz = rv->z[0];
        for (int k = 1; k < rv->n; k++) {
            rv->minx = fminf(rv->minx, rv->x[k]), rv->maxx = fmaxf(rv->maxx, rv->x[k]);
            rv->minz = fminf(rv->minz, rv->z[k]), rv->maxz = fmaxf(rv->maxz, rv->z[k]);
        }
        w->river_count++;
    }
    // Lagos: uno junto al campamento (siempre), otros donde caigan; oasis en el desierto.
    {
        float a = rng_float(&rng) * 2.0f * PI_F, x, z;
        polar(150.0f + rng_float(&rng) * 60.0f, a, &x, &z);
        add_lake(w, x, z, 32.0f + rng_float(&rng) * 18.0f, false);
    }
    // Algunos rios acaban en un lago.
    for (int i = 0; i < w->river_count; i++) {
        const River *rv = &w->rivers[i];
        float x = rv->x[rv->n - 1], z = rv->z[rv->n - 1], r = 45.0f + rng_float(&rng) * 40.0f;
        if (rng_float(&rng) < 0.5f && lake_clear(w, x, z, r)) add_lake(w, x, z, r, false);
    }
    int lakes = 7 + rng_range(&rng, 5);
    for (int i = 0; i < lakes * 30 && w->lake_count < LAKES_MAX - 2; i++) {
        float a = rng_float(&rng) * 2.0f * PI_F, rr = 260.0f + rng_float(&rng) * (WORLD_RADIUS - 600.0f), x, z;
        polar(rr, a, &x, &z);
        Region rg = world_region(w, x, z);
        if (rg == REGION_DESERT) continue;
        float r = 30.0f + rng_float(&rng) * 45.0f;
        if (!lake_clear(w, x, z, r) || world_water(w, x, z, 0.0f, NULL) > land(w, x, z)) continue;
        add_lake(w, x, z, r, false);
        if (--lakes <= 0) break;
    }
    for (int i = 0, oases = 2 + rng_range(&rng, 2); i < 200 && oases > 0; i++) {
        float x, z;
        if (!find_spot(w, &rng, REGION_DESERT, 0.4f * WORLD_RADIUS, 0.85f * WORLD_RADIUS, &x, &z) || !lake_clear(w, x, z, 18.0f)) continue;
        add_lake(w, x, z, 12.0f + rng_float(&rng) * 8.0f, true);
        oases--;
    }
    // Asentamientos: en cada region, una capital (de su reino) y aldeas.
    for (int rg = 0; rg < REGION_COUNT; rg++) {
        for (int k = 0; k < 3 && w->settlement_count < SETTLEMENTS_MAX; k++) {
            bool capital = k == 0;
            float x = 0, z = 0;
            bool ok = false;
            for (int tries = 0; tries < 20 && !ok; tries++) {
                ok = find_spot(w, &rng, (Region)rg, capital ? 0.55f * WORLD_RADIUS : 0.3f * WORLD_RADIUS, 0.85f * WORLD_RADIUS, &x, &z) &&
                     far_from_settlements(w, x, z, 380.0f) && x * x + z * z > 350.0f * 350.0f;
            }
            if (!ok) continue;
            Settlement *s = &w->settlements[w->settlement_count++];
            s->x = x, s->z = z, s->region = (Region)rg;
            s->kind = capital ? SETTLE_CAPITAL : SETTLE_VILLAGE;
            s->kingdom = rg;
            for (int tries = 0; tries < 12; tries++) { // sin nombres repetidos
                make_name(&rng, (Region)rg, s->name, (int)sizeof(s->name));
                bool dup = false;
                for (int o = 0; o < w->settlement_count - 1; o++) dup |= !strcmp(w->settlements[o].name, s->name);
                if (!dup) break;
            }
        }
    }
    // Guaridas de fieras: de las especies de cada region, lejos de la gente.
    for (int rg = 0; rg < REGION_COUNT; rg++) {
        int n = rg == REGION_STEPPE ? 5 : 7;
        for (int k = 0; k < n * 8 && n > 0 && w->den_count < DENS_MAX; k++) {
            float x, z;
            if (!find_spot(w, &rng, (Region)rg, 0.3f * WORLD_RADIUS, 0.9f * WORLD_RADIUS, &x, &z)) continue;
            if (!far_from_settlements(w, x, z, 200.0f) || x * x + z * z < 320.0f * 320.0f) continue;
            Den *d = &w->dens[w->den_count++];
            d->x = x, d->z = z, d->region = (Region)rg;
            d->species = den_species(&rng, (Region)rg);
            n--;
        }
    }
}

void world_region_point(const World *w, Region rg, float r, float *x, float *z) {
    float a = (rg >= 0 && rg < REGION_COUNT ? SECTOR_ANGLE[rg] : 0.0f) - w->warp_phase;
    polar(r, a, x, z);
}

const World *world_for_seed(uint32_t seed) {
    static World cache[2];
    static bool used[2];
    static int next;
    for (int i = 0; i < 2; i++)
        if (used[i] && cache[i].seed == seed) return &cache[i];
    int i = next;
    next = (next + 1) % 2;
    world_generate(&cache[i], seed);
    used[i] = true;
    return &cache[i];
}

int world_nearest_settlement(const World *w, float x, float z, float *dist) {
    int best = -1;
    float bd = 1e18f;
    for (int i = 0; i < w->settlement_count; i++) {
        float dx = x - w->settlements[i].x, dz = z - w->settlements[i].z, d = dx * dx + dz * dz;
        if (d < bd) bd = d, best = i;
    }
    if (dist) *dist = best >= 0 ? sqrtf(bd) : 1e9f;
    return best;
}

int world_nearest_den(const World *w, float x, float z, float *dist) {
    int best = -1;
    float bd = 1e18f;
    for (int i = 0; i < w->den_count; i++) {
        float dx = x - w->dens[i].x, dz = z - w->dens[i].z, d = dx * dx + dz * dz;
        if (d < bd) bd = d, best = i;
    }
    if (dist) *dist = best >= 0 ? sqrtf(bd) : 1e9f;
    return best;
}
