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
// La estepa del centro: su radio medio (el contorno se deforma con ruido).
#define STEPPE_R (0.36f * WORLD_RADIUS)
// Los bordes, en metros desde el borde fractal hacia adentro.
#define CANAL_E 120.0f    // eje medio del canal del bosque (serpentea +-CANAL_SWING)
#define CANAL_SWING 60.0f
#define CANAL_HALF 30.0f  // medio ancho del canal
#define CANAL_DEPTH 5.0f
#define SHORE_E 230.0f    // la costa de los fiordos (sin contar los fiordos que entran)
#define WALL_E 95.0f      // pie del muro de estratos del desierto
#define GORGE_E 150.0f    // eje del cañon al pie del muro
#define ICE_E 120.0f      // pie del muro de hielo del altiplano

static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
static float smooth(float a, float b, float v) {
    float t = clamp01((v - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// ------------------------------------------------------------------ regiones
// Cuadrantes fijos (angulo de atan2(z, x); +Z es el sur): la estepa al centro; el altiplano
// al sureste, la costa al suroeste, el bosque al noroeste y el desierto al noreste.
static const float SECTOR_ANGLE[REGION_COUNT] = { 0.0f, 225.0f * DEG, 45.0f * DEG, 135.0f * DEG, 315.0f * DEG };
static const float ALTITUDE[REGION_COUNT] = { 40.0f, 55.0f, 70.0f, 12.0f, 28.0f }; // la costa, la mas baja; el altiplano, el mas alto
#define SECTOR_HALF (45.0f * DEG)
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

static void polar(float r, float a, float *x, float *z) {
    *x = cosf(a) * r;
    *z = sinf(a) * r;
}

// El borde fractal: varias octavas de ruido sobre el angulo.
float world_edge_radius(const World *w, float th) {
    float c = cosf(th), s = sinf(th);
    float big = fbm2d(c * 2.2f + 13.0f, s * 2.2f, w->seed + 601u, 6);
    float fine = fbm2d(c * 11.0f, s * 11.0f + 7.0f, w->seed + 602u, 5);
    return WORLD_RADIUS * (1.0f + 0.06f * big + 0.012f * fine);
}

float world_edge_distance(const World *w, float x, float z) { return world_edge_radius(w, atan2f(z, x)) - sqrtf(x * x + z * z); }

static float steppe_radius(const World *w, float th) {
    return STEPPE_R * (1.0f + 0.22f * fbm2d(cosf(th) * 1.6f, sinf(th) * 1.6f + 3.0f, w->seed + 611u, 5));
}

// El angulo de las fronteras entre cuadrantes, torcido con ruido (cambia con la semilla).
static float warped_angle(const World *w, float x, float z) {
    return atan2f(z, x) + w->warp_phase + 0.18f * fbm2d(x * 0.00045f, z * 0.00045f, w->seed + 11u, 3);
}

// Pesos de los cuatro cuadrantes de afuera (sin la estepa del centro).
static void quadrant_weights(float th, float out[REGION_COUNT]) {
    float sum = 0.0f;
    out[REGION_STEPPE] = 0.0f;
    for (int i = 1; i < REGION_COUNT; i++) {
        out[i] = clamp01((SECTOR_HALF + SECTOR_BLEND - angdiff(th, SECTOR_ANGLE[i])) / (2.0f * SECTOR_BLEND));
        sum += out[i];
    }
    for (int i = 1; i < REGION_COUNT; i++) out[i] = sum > 0.0f ? out[i] / sum : 0.0f;
}

Region world_region_weights(const World *w, float x, float z, float out[REGION_COUNT]) {
    float r = sqrtf(x * x + z * z), rs = steppe_radius(w, atan2f(z, x));
    float center = 1.0f - smooth(rs - 70.0f, rs + 70.0f, r); // la estepa del centro
    float q[REGION_COUNT];
    quadrant_weights(warped_angle(w, x, z), q);
    Region best = REGION_STEPPE;
    for (int i = 0; i < REGION_COUNT; i++) {
        out[i] = (1.0f - center) * q[i] + (i == REGION_STEPPE ? center : 0.0f);
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

float world_canal_offset(const World *w, float th) { return CANAL_E + CANAL_SWING * sinf(th * 7.0f + w->warp_phase * 9.0f); }

bool world_clamp(const World *w, float *x, float *z) {
    float th = atan2f(*z, *x), r = sqrtf(*x * *x + *z * *z);
    float q[REGION_COUNT];
    quadrant_weights(th + w->warp_phase, q); // solo del angulo: llevar el punto hacia adentro no cambia el limite
    // Hasta donde se llega: el mar (un poco), el pie del muro, del hielo, o la orilla interior del canal.
    float need = q[REGION_FJORD] * 35.0f + q[REGION_DESERT] * (WALL_E - 15.0f) + q[REGION_HIGHLAND] * (ICE_E - 25.0f) +
                 q[REGION_FOREST] * (world_canal_offset(w, th) + CANAL_HALF + 8.0f);
    float limit = world_edge_radius(w, th) - need;
    if (r <= limit + 0.05f) return false;
    *x *= limit / r;
    *z *= limit / r;
    return true;
}

void world_region_point(const World *w, Region rg, float r, float *x, float *z) {
    float a = (rg >= 0 && rg < REGION_COUNT ? SECTOR_ANGLE[rg] : 0.0f) - w->warp_phase;
    polar(r, a, x, z);
}

void world_edge_point(const World *w, Region rg, float inset, float *x, float *z) {
    float a = (rg >= 0 && rg < REGION_COUNT ? SECTOR_ANGLE[rg] : 0.0f) - w->warp_phase;
    polar(world_edge_radius(w, a) - inset, a, x, z);
}

// ------------------------------------------------------------------ relieve
static float region_detail(const World *w, Region rg, float x, float z) {
    uint32_t s = w->seed;
    switch (rg) {
    case REGION_FOREST: // lomas amplias y redondeadas
        return fbm2d(x * 0.004f, z * 0.004f, s + 31u, 5) * 16.0f + fbm2d(x * 0.03f, z * 0.03f, s + 32u, 3) * 2.0f;
    case REGION_HIGHLAND: { // meseta alta con cordilleras de agujas
        float plateau = fbm2d(x * 0.003f, z * 0.003f, s + 61u, 4) * 10.0f;
        float ridge = 1.0f - fabsf(fbm2d(x * 0.0035f, z * 0.0035f, s + 202u, 5));
        float mask = smooth(-0.15f, 0.35f, fbm2d(x * 0.0011f, z * 0.0011f, s + 101u, 3));
        return plateau + ridge * ridge * ridge * 55.0f * mask;
    }
    case REGION_FJORD: { // costa abrupta
        float rough = fbm2d(x * 0.006f, z * 0.006f, s + 41u, 4) * 14.0f;
        float ridge = 1.0f - fabsf(fbm2d(x * 0.005f, z * 0.005f, s + 43u, 4));
        return rough + ridge * ridge * 24.0f;
    }
    case REGION_DESERT: { // dunas y mesetas de bordes cortados
        float dunes = (1.0f - fabsf(noise2d(x * 0.03f + z * 0.012f, z * 0.02f, s + 505u))) * 2.4f;
        float m = fbm2d(x * 0.0022f, z * 0.0022f, s + 51u, 3);
        float mesa = smooth(0.22f, 0.27f, m) * 16.0f + smooth(0.42f, 0.46f, m) * 10.0f;
        return dunes + mesa + fbm2d(x * 0.008f, z * 0.008f, s + 52u, 3) * 3.0f;
    }
    default: { // estepa: colinas suaves y matas (los montículos de pasto)
        float tussock = fmaxf(0.0f, noise2d(x * 0.33f, z * 0.33f, s + 18u)) * 0.45f;
        return fbm2d(x * 0.005f, z * 0.005f, s, 5) * 9.0f + fbm2d(x * 0.04f, z * 0.04f, s + 17u, 3) * 1.2f + tussock;
    }
    }
}

// Fiordos: el mar entra tierra adentro en brazos estrechos (picos en el angulo).
static float fjord_inlet(const World *w, float th) {
    float f = noise2d(cosf(th) * 11.0f, sinf(th) * 11.0f, w->seed + 71u);
    float g = 1.0f - fabsf(f);
    float len = noise2d(cosf(th) * 31.0f, sinf(th) * 31.0f, w->seed + 72u) * 0.5f + 0.5f; // largo de cada fiordo
    return (380.0f + 600.0f * len) * powf(g, 16.0f);
}

// El terreno sin agua: regiones, montañas y los bordes (mar, cañon y muro, hielo).
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
        float crest = 1.0f - fabsf(fbm2d(x * 0.01f, z * 0.01f, w->seed + 81u + (uint32_t)i, 3));
        h += p->h * powf(f, 1.6f) * (0.55f + 0.6f * crest);
    }
    float th = atan2f(z, x), e = world_edge_distance(w, x, z);
    if (e < 1200.0f) {
        float q[REGION_COUNT];
        quadrant_weights(warped_angle(w, x, z), q);
        if (q[REGION_FJORD] > 0.0f) { // la costa baja al mar; los fiordos entran
            float shore = SHORE_E + fjord_inlet(w, th);
            float t = smooth(shore + 70.0f, shore - 30.0f, e);
            float sea = -6.0f - 16.0f * smooth(shore, shore - 260.0f, e);
            h = lerpf(h, lerpf(h, sea, t), q[REGION_FJORD]);
        }
        if (q[REGION_DESERT] > 0.0f) { // un cañon al pie y un muro de siete estratos
            float g = (e - GORGE_E) / 45.0f;
            float gorge = g * g < 1.0f ? (1.0f - g * g) * 36.0f : 0.0f;
            float t = smooth(WALL_E + 8.0f, WALL_E - 60.0f, e) * 7.0f;
            float step = floorf(t), frac = t - step;
            float wall = (step + smooth(0.75f, 1.0f, frac)) / 7.0f * 160.0f;
            h = lerpf(h, h - gorge + wall, q[REGION_DESERT]);
        }
        if (q[REGION_HIGHLAND] > 0.0f) { // el muro de hielo del glaciar, con grietas
            float t = smooth(ICE_E + 10.0f, ICE_E - 70.0f, e);
            float cracks = (1.0f - fabsf(noise2d(x * 0.05f, z * 0.05f, w->seed + 91u))) * 5.0f;
            h = lerpf(h, h + t * (135.0f + cracks), q[REGION_HIGHLAND]);
        }
    }
    return h;
}

// ------------------------------------------------------------------ agua
static float canal_level(const World *w, float th) {
    float a = th / DEG;
    while (a < 0.0f) a += 360.0f;
    int i = (int)a % CANAL_SAMPLES;
    float f = a - floorf(a);
    return lerpf(w->canal_level[i], w->canal_level[(i + 1) % CANAL_SAMPLES], f);
}

// Distancia de (x, z) al rio, nivel del agua en el punto mas cercano, y (si se piden) los
// metros recorridos y el lado (la distancia con signo).
static float river_dist(const River *r, float x, float z, float *level, float *along, float *side) {
    float best = 1e9f;
    for (int i = 0; i + 1 < r->n; i++) {
        float ax = r->x[i], az = r->z[i], bx = r->x[i + 1], bz = r->z[i + 1];
        float vx = bx - ax, vz = bz - az, l2 = vx * vx + vz * vz;
        float t = l2 > 0.0f ? clamp01(((x - ax) * vx + (z - az) * vz) / l2) : 0.0f;
        float px = x - (ax + vx * t), pz = z - (az + vz * t), d = sqrtf(px * px + pz * pz);
        if (d < best) {
            best = d;
            *level = lerpf(r->level[i], r->level[i + 1], t);
            if (along) *along = lerpf(r->along[i], r->along[i + 1], t);
            if (side) *side = (vx * pz - vz * px) >= 0.0f ? d : -d;
        }
    }
    return best;
}

// Rio trenzado: canales que se separan y se juntan por el lecho de grava (las curvas de
// nivel cero de un ruido estirado a lo largo del rio).
static bool braid_channel(const World *w, const River *r, float along, float side) {
    float v = side / r->width;
    float n = noise2d(along * 0.010f, v * 2.6f, w->seed + 700u) + 0.45f * noise2d(along * 0.031f, v * 5.0f, w->seed + 701u);
    return fabsf(n) < 0.12f + 0.06f * (1.0f - fabsf(v));
}

static float lake_dist(const World *w, const Lake *l, float x, float z) {
    float dx = x - l->x, dz = z - l->z;
    float wob = 1.0f + 0.28f * noise2d(atan2f(dz, dx) * 1.7f, l->r * 0.01f, w->seed + 91u); // orilla irregular
    return sqrtf(dx * dx + dz * dz) / (l->r * wob);
}

static bool river_box(const River *r, float x, float z, float m) {
    return x >= r->minx - m && x <= r->maxx + m && z >= r->minz - m && z <= r->maxz + m;
}

float world_height(const World *w, float x, float z) {
    float h = land(w, x, z);
    float th = atan2f(z, x), e = world_edge_distance(w, x, z);
    // El canal del bosque: un cauce ancho que serpentea junto al borde.
    float ce = world_canal_offset(w, th);
    if (e > ce - CANAL_HALF - 25.0f && e < ce + CANAL_HALF + 25.0f) {
        float q[REGION_COUNT];
        quadrant_weights(warped_angle(w, x, z), q);
        float cw = q[REGION_FOREST];
        if (cw > 0.0f) {
            float d = fabsf(e - ce), lv = canal_level(w, th);
            float bed = d < CANAL_HALF ? lv - 0.5f - CANAL_DEPTH * (1.0f - (d / CANAL_HALF) * (d / CANAL_HALF))
                                       : lerpf(lv + 0.3f, fmaxf(h, lv + 0.3f), smooth(CANAL_HALF, CANAL_HALF + 25.0f, d));
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
    for (int i = 0; i < w->river_count; i++) { // rios: meandros, tributarios y trenzados
        const River *r = &w->rivers[i];
        if (!river_box(r, x, z, r->width * 2.5f)) continue;
        float lv = 0.0f, along = 0.0f, side = 0.0f, d = river_dist(r, x, z, &lv, &along, &side);
        if (r->kind == RIVER_BRAIDED) {
            if (d < r->width) h = fminf(h, braid_channel(w, r, along, side) ? lv - 0.6f : lv + 0.12f); // grava y canales
            else if (d < r->width * 1.6f) h = fminf(h, lerpf(lv + 0.12f, fmaxf(h, lv + 0.12f), smooth(r->width, r->width * 1.6f, d)));
        } else {
            float deep = 2.2f * fminf(1.0f, r->width / 5.0f); // los arroyos, menos hondos
            if (d < r->width) h = fminf(h, lv - 0.4f - deep * (1.0f - (d / r->width) * (d / r->width)));
            else if (d < r->width * 2.4f) h = fminf(h, lerpf(lv + 0.2f, fmaxf(h, lv + 0.2f), smooth(r->width, r->width * 2.4f, d)));
        }
    }
    // El llano del campamento.
    float d = sqrtf(x * x + z * z);
    if (d < CAMP_FLAT_OUTER) h = lerpf(w->camp_height, h, smooth(CAMP_FLAT_INNER, CAMP_FLAT_OUTER, d));
    return h;
}

float world_water(const World *w, float x, float z, float flood, WaterKind *kind) {
    float best = -1e9f;
    WaterKind k = WATER_NONE;
    float th = atan2f(z, x), e = world_edge_distance(w, x, z);
    if (e < SHORE_E + 1100.0f) {
        float q[REGION_COUNT];
        quadrant_weights(warped_angle(w, x, z), q);
        if (q[REGION_FJORD] > 0.05f && land(w, x, z) < SEA_LEVEL + 1.0f) best = SEA_LEVEL, k = WATER_SEA;
        float ce = world_canal_offset(w, th);
        if (q[REGION_FOREST] > 0.5f && fabsf(e - ce) < CANAL_HALF + 4.0f) {
            float lv = canal_level(w, th) + 0.5f * flood;
            if (lv > best) best = lv, k = WATER_CANAL;
        }
    }
    for (int i = 0; i < w->lake_count; i++) {
        const Lake *l = &w->lakes[i];
        float dx = x - l->x, dz = z - l->z;
        if (dx * dx + dz * dz > l->r * l->r * 3.2f) continue;
        if (lake_dist(w, l, x, z) < 1.3f && l->level + flood > best) best = l->level + flood, k = WATER_LAKE;
    }
    for (int i = 0; i < w->river_count; i++) {
        const River *r = &w->rivers[i];
        if (!river_box(r, x, z, r->width * 1.5f)) continue;
        float lv = 0.0f, d = river_dist(r, x, z, &lv, NULL, NULL);
        float reach = r->kind == RIVER_BRAIDED ? r->width : r->width * 1.25f;
        if (d < reach && lv + 0.5f * flood > best) best = lv + 0.5f * flood, k = WATER_RIVER;
    }
    if (kind) *kind = k;
    return best;
}

// ------------------------------------------------------------------ generacion
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

// Un punto al azar de una region: la estepa, dentro de su anillo; las demas, en su cuadrante
// entre la estepa y el borde (inset metros antes del borde).
static void random_point(const World *w, Rng *rng, Region rg, float inset, float *x, float *z) {
    if (rg == REGION_STEPPE) {
        float a = rng_float(rng) * 2.0f * PI_F;
        polar(sqrtf(rng_float(rng)) * (steppe_radius(w, a) - 180.0f), a, x, z);
        return;
    }
    float a = SECTOR_ANGLE[rg] - w->warp_phase + (rng_float(rng) - 0.5f) * 1.7f * SECTOR_HALF;
    float rmin = steppe_radius(w, a) + 160.0f, rmax = world_edge_radius(w, a) - inset;
    polar(rmin + rng_float(rng) * fmaxf(0.0f, rmax - rmin), a, x, z);
}

// Un sitio de la region (muy adentro de ella), seco y llano. false si no hay.
static bool find_spot(World *w, Rng *rng, Region rg, float inset, float *ox, float *oz) {
    for (int tries = 0; tries < 400; tries++) {
        float x, z;
        random_point(w, rng, rg, inset, &x, &z);
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
    l->glacial = world_region(w, x, z) == REGION_HIGHLAND;
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
        float dx = x - w->lakes[i].x, dz = z - w->lakes[i].z, min = r + w->lakes[i].r + 160.0f;
        if (dx * dx + dz * dz < min * min) return false;
    }
    return x * x + z * z > (r + 120.0f) * (r + 120.0f); // lejos del llano del campamento
}

static void river_finish(River *rv) {
    rv->minx = rv->maxx = rv->x[0], rv->minz = rv->maxz = rv->z[0];
    rv->along[0] = 0.0f;
    for (int k = 1; k < rv->n; k++) {
        rv->minx = fminf(rv->minx, rv->x[k]), rv->maxx = fmaxf(rv->maxx, rv->x[k]);
        rv->minz = fminf(rv->minz, rv->z[k]), rv->maxz = fmaxf(rv->maxz, rv->z[k]);
        rv->along[k] = rv->along[k - 1] + sqrtf((rv->x[k] - rv->x[k - 1]) * (rv->x[k] - rv->x[k - 1]) + (rv->z[k] - rv->z[k - 1]) * (rv->z[k] - rv->z[k - 1]));
    }
}

// El agua baja hacia el punto 0 y nunca sube sobre la orilla: cada tramo, a lo sumo tan alto
// como la orilla mas baja de alli hacia arriba (from_end: el agua baja hacia el final).
static void river_levels(const World *w, River *rv, float below, bool from_end) {
    for (int k = 0; k < rv->n; k++) rv->level[k] = land(w, rv->x[k], rv->z[k]) - below;
    float m = 1e9f;
    if (!from_end)
        for (int k = rv->n - 1; k >= 0; k--) m = fminf(m, rv->level[k]), rv->level[k] = m;
    else
        for (int k = 0; k < rv->n; k++) m = fminf(m, rv->level[k]), rv->level[k] = m;
}

// Los meandros de la estepa: arcos que siguen su contorno dando vueltas cerradas. El rumbo
// oscila como una curva generada por un seno (los meandros reales): theta(s) = w sin(2 pi s / L),
// y se corrige despacio hacia el contorno de la estepa para no alejarse.
static void make_meanders(World *w, Rng *rng) {
    int arcs = 5;
    float start = rng_float(rng) * 2.0f * PI_F;
    for (int a = 0; a < arcs && w->river_count < RIVERS_MAX; a++) {
        River *rv = &w->rivers[w->river_count];
        memset(rv, 0, sizeof(*rv));
        rv->kind = RIVER_MEANDER;
        rv->width = 7.0f + rng_float(rng) * 4.0f;
        float span = 2.0f * PI_F / arcs - (4.0f + 4.0f * rng_float(rng)) * DEG; // huecos entre arcos
        float th0 = start + (float)a * 2.0f * PI_F / arcs, th_end = th0 + span;
        float x, z;
        polar(steppe_radius(w, th0), th0, &x, &z);
        float ph = 0.0f;
        float step = 14.0f, s = 0.0f;
        for (int k = 0; k < RIVER_PTS * 3 && rv->n < RIVER_PTS; k++) {
            float th = atan2f(z, x), r = sqrtf(x * x + z * z);
            if (angdiff(th, th0) > span || (k > 10 && angdiff(th, th_end) < 1.0f * DEG)) break;
            if (k % 3 == 0) rv->x[rv->n] = x, rv->z[rv->n] = z, rv->n++; // un punto cada 42 m
            // Rumbo: la tangente del contorno, corregida hacia el, mas la oscilacion del meandro.
            // Cada vuelta distinta: la amplitud y el largo del meandro cambian por el camino, y el
            // cauce entero deriva lejos del contorno y vuelve.
            float rs = steppe_radius(w, th) + 160.0f * noise2d(s * 0.0011f, (float)a * 2.3f, w->seed + 78u);
            float omega = (70.0f + 45.0f * (0.5f + 0.5f * noise2d(s * 0.0025f, (float)a, w->seed + 76u))) * DEG;
            float wave = 380.0f + 360.0f * (0.5f + 0.5f * noise2d(s * 0.0017f, (float)a + 9.0f, w->seed + 75u));
            ph += 2.0f * PI_F * step / wave;
            float tangent = th + PI_F * 0.5f, pull = fmaxf(-0.7f, fminf(0.7f, (r - rs) / 220.0f));
            float heading = tangent + pull + omega * sinf(ph) + 0.3f * noise2d(s * 0.006f, (float)a, w->seed + 77u);
            x += cosf(heading) * step, z += sinf(heading) * step;
            s += step;
        }
        (void)th_end;
        if (rv->n < 12) continue;
        river_finish(rv);
        river_levels(w, rv, 1.6f, false);
        for (int k = 0; k < rv->n; k++) { // sin escalones bruscos: el minimo de los vecinos
            float m = rv->level[k];
            for (int j = k - 4; j <= k + 4; j++)
                if (j >= 0 && j < rv->n) m = fminf(m, land(w, rv->x[j], rv->z[j]) - 1.6f);
            rv->level[k] = m;
        }
        w->river_count++;
    }
}

// Un rio que avanza en linea serpenteante desde (x, z) en el angulo radial ang, hacia adentro
// (inward) o hacia afuera, hasta stop_r (o hasta salir de su region).
static void make_radial(World *w, Rng *rng, RiverKind kind, Region rg, float ang, float r0, float stop_r, bool inward, float width) {
    if (w->river_count >= RIVERS_MAX) return;
    River *rv = &w->rivers[w->river_count];
    memset(rv, 0, sizeof(*rv));
    rv->kind = kind;
    rv->width = width;
    float r = r0, a = ang, step = kind == RIVER_BRAIDED ? 34.0f : 28.0f;
    float wiggle = kind == RIVER_TRIBUTARY ? 0.9f : 0.45f;
    for (int k = 0; k < RIVER_PTS; k++) {
        float x, z;
        polar(r, a, &x, &z);
        if (k > 2 && world_region_weight(w, rg, x, z) < 0.35f && world_region(w, x, z) != REGION_STEPPE) break;
        rv->x[rv->n] = x, rv->z[rv->n] = z;
        rv->n++;
        if (inward ? r < stop_r : r > stop_r) break;
        r += inward ? -step : step;
        a += wiggle * noise2d((float)k * 0.22f, (float)w->river_count * 3.1f, w->seed + 79u) * step / r;
    }
    (void)rng;
    if (rv->n < 8) return;
    river_finish(rv);
    river_levels(w, rv, kind == RIVER_BRAIDED ? 0.6f : 1.4f, kind != RIVER_TRIBUTARY);
    if (kind == RIVER_TRIBUTARY) { // nace en el canal: alli, su nivel
        float lv0 = canal_level(w, atan2f(rv->z[0], rv->x[0]));
        rv->level[0] = fminf(rv->level[0], lv0);
    }
    w->river_count++;
}

// Un arroyo: de (x, z) hacia el punto mas cercano de los rios ya trazados (los primeros
// `mains`), serpenteando; el agua baja hacia la junta.
static void make_creek(World *w, Rng *rng, int mains, float x, float z) {
    if (w->river_count >= RIVERS_MAX) return;
    float best = 1e18f, tx = 0.0f, tz = 0.0f;
    for (int i = 0; i < mains; i++)
        for (int k = 0; k < w->rivers[i].n; k += 2) {
            float dx = w->rivers[i].x[k] - x, dz = w->rivers[i].z[k] - z, d2 = dx * dx + dz * dz;
            if (d2 < best) best = d2, tx = w->rivers[i].x[k], tz = w->rivers[i].z[k];
        }
    float dist = sqrtf(best);
    if (dist < 120.0f || dist > 1400.0f) return;
    River *rv = &w->rivers[w->river_count];
    memset(rv, 0, sizeof(*rv));
    rv->kind = RIVER_CREEK;
    rv->width = 2.0f + rng_float(rng) * 1.5f;
    const float step = 16.0f;
    float px = x, pz = z, phase = rng_float(rng) * 50.0f;
    for (int k = 0; k < RIVER_PTS; k++) {
        rv->x[rv->n] = px, rv->z[rv->n] = pz;
        rv->n++;
        float dx = tx - px, dz = tz - pz, d = sqrtf(dx * dx + dz * dz);
        if (d < step) {
            rv->x[rv->n] = tx, rv->z[rv->n] = tz; // la junta
            rv->n++;
            break;
        }
        float a = atan2f(dz, dx) + 0.7f * noise2d(phase + k * 0.18f, (float)w->river_count, w->seed + 83u);
        px += cosf(a) * step, pz += sinf(a) * step;
        if (rv->n >= RIVER_PTS - 1) break;
    }
    if (rv->n < 6) return;
    river_finish(rv);
    river_levels(w, rv, 0.9f, true);
    w->river_count++;
}

void world_generate(World *w, uint32_t seed) {
    memset(w, 0, sizeof(*w));
    w->seed = seed;
    Rng rng;
    rng_seed(&rng, seed ^ 0x5EEDu);
    w->warp_phase = (rng_float(&rng) - 0.5f) * 10.0f * DEG; // las fronteras giran un poco
    // Montañas: el altiplano tiene las mas grandes; la costa y el bosque algunas; la estepa, lomas.
    static const struct { Region rg; int min, extra; float hmin, hmax, rmin, rmax; } PEAK_RULES[] = {
        { REGION_HIGHLAND, 8, 4, 60.0f, 110.0f, 150.0f, 320.0f }, // sus cumbres guardan el glaciar
        { REGION_FJORD, 5, 3, 40.0f, 70.0f, 110.0f, 200.0f },
        { REGION_FOREST, 4, 2, 25.0f, 45.0f, 140.0f, 240.0f },
        { REGION_STEPPE, 2, 1, 12.0f, 24.0f, 160.0f, 240.0f },
    };
    for (size_t k = 0; k < sizeof(PEAK_RULES) / sizeof(PEAK_RULES[0]); k++) {
        int n = PEAK_RULES[k].min + rng_range(&rng, PEAK_RULES[k].extra + 1);
        for (int i = 0; i < n && w->peak_count < PEAKS_MAX; i++) {
            for (int tries = 0; tries < 60; tries++) {
                float x, z;
                random_point(w, &rng, PEAK_RULES[k].rg, 450.0f, &x, &z);
                if (x * x + z * z < 600.0f * 600.0f) continue; // ni junto al campamento
                Peak *p = &w->peaks[w->peak_count++];
                p->x = x, p->z = z;
                p->h = PEAK_RULES[k].hmin + rng_float(&rng) * (PEAK_RULES[k].hmax - PEAK_RULES[k].hmin);
                p->r = PEAK_RULES[k].rmin + rng_float(&rng) * (PEAK_RULES[k].rmax - PEAK_RULES[k].rmin);
                break;
            }
        }
    }
    w->camp_height = land(w, 0.0f, 0.0f);
    // El canal del bosque: su nivel sigue el terreno de su eje (suavizado), un poco por debajo.
    float raw[CANAL_SAMPLES];
    for (int i = 0; i < CANAL_SAMPLES; i++) {
        float th = (float)i * DEG, x, z;
        polar(world_edge_radius(w, th) - world_canal_offset(w, th), th, &x, &z);
        raw[i] = land(w, x, z) - 3.0f;
    }
    for (int i = 0; i < CANAL_SAMPLES; i++) {
        float s = 0.0f;
        for (int k = -6; k <= 6; k++) s += raw[(i + k + CANAL_SAMPLES) % CANAL_SAMPLES];
        w->canal_level[i] = fminf(s / 13.0f, raw[i]);
    }
    // Rios: los meandros que rodean la estepa; los tributarios del canal, por el bosque; los
    // trenzados del altiplano (del hielo hacia la estepa) y de la costa (de la estepa al mar).
    make_meanders(w, &rng);
    int tribs = 4 + rng_range(&rng, 3);
    for (int i = 0; i < tribs * 4 && tribs > 0; i++) {
        float a = SECTOR_ANGLE[REGION_FOREST] - w->warp_phase + (rng_float(&rng) - 0.5f) * 1.6f * SECTOR_HALF;
        bool close = false;
        for (int k = 0; k < w->river_count; k++)
            close |= w->rivers[k].kind == RIVER_TRIBUTARY && angdiff(a, atan2f(w->rivers[k].z[0], w->rivers[k].x[0])) < 9.0f * DEG;
        if (close) continue;
        float r0 = world_edge_radius(w, a) - world_canal_offset(w, a);
        make_radial(w, &rng, RIVER_TRIBUTARY, REGION_FOREST, a, r0, steppe_radius(w, a) + 40.0f, true, 5.0f + rng_float(&rng) * 4.0f);
        tribs--;
    }
    int braids = 3 + rng_range(&rng, 2);
    for (int i = 0; i < braids; i++) {
        float a = SECTOR_ANGLE[REGION_HIGHLAND] - w->warp_phase + ((float)i + 0.5f - braids * 0.5f) / braids * 1.5f * SECTOR_HALF;
        make_radial(w, &rng, RIVER_BRAIDED, REGION_HIGHLAND, a, world_edge_radius(w, a) - ICE_E - 60.0f, steppe_radius(w, a) + 30.0f, true,
                    40.0f + rng_float(&rng) * 40.0f);
    }
    for (int i = 0, n = 1 + rng_range(&rng, 2); i < n; i++) {
        float a = SECTOR_ANGLE[REGION_FJORD] - w->warp_phase + (rng_float(&rng) - 0.5f) * 1.2f * SECTOR_HALF;
        make_radial(w, &rng, RIVER_BRAIDED, REGION_FJORD, a, steppe_radius(w, a) + 60.0f, world_edge_radius(w, a) - 80.0f, false,
                    35.0f + rng_float(&rng) * 30.0f);
    }
    // Arroyos: bajan de las lomas de la estepa, del bosque, del altiplano y de la costa hasta un rio.
    {
        static const Region CREEK_REGIONS[] = { REGION_STEPPE, REGION_FOREST, REGION_HIGHLAND, REGION_FJORD };
        int mains = w->river_count, creeks = 14 + rng_range(&rng, 7);
        for (int i = 0; i < creeks * 6 && creeks > 0 && w->river_count < RIVERS_MAX - 2; i++) {
            float x, z;
            random_point(w, &rng, CREEK_REGIONS[i % 4], 260.0f, &x, &z);
            if (x * x + z * z < 300.0f * 300.0f || !dry_spot(w, x, z)) continue;
            int before = w->river_count;
            make_creek(w, &rng, mains, x, z);
            if (w->river_count > before) creeks--;
        }
    }
    // Lagos: uno junto al campamento (siempre), otros donde caigan; oasis en el desierto.
    {
        float a = rng_float(&rng) * 2.0f * PI_F, x, z;
        polar(150.0f + rng_float(&rng) * 60.0f, a, &x, &z);
        add_lake(w, x, z, 32.0f + rng_float(&rng) * 18.0f, false);
    }
    for (int i = 0; i < w->river_count; i++) { // algunos tributarios acaban en un lago
        const River *rv = &w->rivers[i];
        if (rv->kind != RIVER_TRIBUTARY || rng_float(&rng) > 0.5f) continue;
        float x = rv->x[rv->n - 1], z = rv->z[rv->n - 1], r = 50.0f + rng_float(&rng) * 50.0f;
        if (lake_clear(w, x, z, r)) add_lake(w, x, z, r, false);
    }
    int lakes = 14 + rng_range(&rng, 6);
    for (int i = 0; i < lakes * 30 && w->lake_count < LAKES_MAX - 4 && lakes > 0; i++) {
        float a = rng_float(&rng) * 2.0f * PI_F, rr = 300.0f + rng_float(&rng) * (WORLD_RADIUS - 700.0f), x, z;
        polar(rr, a, &x, &z);
        Region rg = world_region(w, x, z);
        if (rg == REGION_DESERT || world_edge_distance(w, x, z) < 400.0f) continue;
        float r = 35.0f + rng_float(&rng) * 60.0f;
        if (!lake_clear(w, x, z, r) || world_water(w, x, z, 0.0f, NULL) > land(w, x, z)) continue;
        add_lake(w, x, z, r, false);
        lakes--;
    }
    for (int i = 0, oases = 3 + rng_range(&rng, 2); i < 200 && oases > 0; i++) {
        float x, z;
        if (!find_spot(w, &rng, REGION_DESERT, 450.0f, &x, &z) || !lake_clear(w, x, z, 18.0f)) continue;
        add_lake(w, x, z, 12.0f + rng_float(&rng) * 8.0f, true);
        oases--;
    }
    // Asentamientos: en cada region, una capital (de su reino) y tres aldeas.
    for (int rg = 0; rg < REGION_COUNT; rg++) {
        for (int k = 0; k < 4 && w->settlement_count < SETTLEMENTS_MAX; k++) {
            float x = 0, z = 0;
            bool ok = false;
            for (int tries = 0; tries < 20 && !ok; tries++)
                ok = find_spot(w, &rng, (Region)rg, 500.0f, &x, &z) && far_from_settlements(w, x, z, 600.0f) && x * x + z * z > 450.0f * 450.0f;
            if (!ok) continue;
            Settlement *s = &w->settlements[w->settlement_count++];
            s->x = x, s->z = z, s->region = (Region)rg;
            s->kind = k == 0 ? SETTLE_CAPITAL : SETTLE_VILLAGE;
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
        int n = rg == REGION_STEPPE ? 7 : 11;
        for (int k = 0; k < n * 8 && n > 0 && w->den_count < DENS_MAX; k++) {
            float x, z;
            if (!find_spot(w, &rng, (Region)rg, 400.0f, &x, &z)) continue;
            if (!far_from_settlements(w, x, z, 260.0f) || x * x + z * z < 400.0f * 400.0f) continue;
            Den *d = &w->dens[w->den_count++];
            d->x = x, d->z = z, d->region = (Region)rg;
            d->species = den_species(&rng, (Region)rg);
            n--;
        }
    }
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
