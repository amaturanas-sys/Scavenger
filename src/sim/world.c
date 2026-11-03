#include "world.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "animals.h"
#include "hazards.h"
#include "hydro.h"
#include "lang.h"
#include "noise.h"
#include "rng.h"

#define PI_F 3.14159265f
#define DEG (PI_F / 180.0f)

// Campamento inicial en el origen: el terreno se aplana a su alrededor.
#define CAMP_FLAT_INNER 18.0f
#define CAMP_FLAT_OUTER 42.0f
#define CAMP_DOME_H 6.0f   // el campamento, en una loma suave: el agua baja alrededor, nunca por el
#define CAMP_DOME_R 520.0f
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
#define WATER_MAX_DEPTH_LAKE 9.0f  // las cuencas tienen ~5,6 m; mas hondo es un cañon ajeno
#define WATER_MAX_DEPTH_RIVER 6.0f // los cauces, ~2,6 m
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
// De mayor a menor: el altiplano, el desierto, la estepa, el bosque y la costa (docs/AGUA.md, pagina 1): asi el
// agua del glaciar baja por la estepa hasta el canal del bosque y el mar.
static const float ALTITUDE[REGION_COUNT] = { 40.0f, 33.0f, 70.0f, 12.0f, 55.0f };
#define SECTOR_HALF (45.0f * DEG)
#define SECTOR_BLEND (7.0f * DEG)

float region_altitude(Region r) { return r >= 0 && r < REGION_COUNT ? ALTITUDE[r] : 0.0f; }

const char *region_name(Region r) {
    static const char *names[REGION_COUNT] = { N_("estepa"), N_("bosque de coníferas"), N_("altiplano glaciar"), N_("costa de fiordos"),
                                               N_("desierto") };
    return r >= 0 && r < REGION_COUNT ? T(names[r]) : "?";
}

const char *world_compass(float dx, float dz) {
    static const char *names[8] = { N_("este"), N_("noreste"), N_("norte"), N_("noroeste"), N_("oeste"), N_("suroeste"), N_("sur"),
                                    N_("sureste") };
    float a = atan2f(-dz, dx); // -Z es el norte
    int o = (int)floorf(a / (PI_F / 4.0f) + 0.5f);
    return T(names[(o % 8 + 8) % 8]);
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

// Donde empieza el muro del otro lado del canal del bosque: unos 20 m mas alla de su orilla de afuera.
#define WALL_OUTER(w, th) (world_canal_offset((w), (th)) - CANAL_HALF - 20.0f)

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

static float land(const World *w, float x, float z);

float world_open_sea(const World *w, float x, float z) {
    float q[REGION_COUNT];
    quadrant_weights(warped_angle(w, x, z), q);
    if (q[REGION_FJORD] < 0.5f) return -1e9f;
    float beyond = WORLD_SHARK_E - world_edge_distance(w, x, z);
    if (beyond <= 0.0f || land(w, x, z) > SEA_LEVEL - 2.0f) return fminf(beyond, 0.0f); // aca no, o sobre una roca
    return beyond;
}

float world_forest_wall(const World *w, float x, float z) {
    float q[REGION_COUNT];
    quadrant_weights(warped_angle(w, x, z), q);
    if (q[REGION_FOREST] <= 0.0f) return 0.0f;
    float th = atan2f(z, x);
    return q[REGION_FOREST] * smooth(WALL_OUTER(w, th) + 4.0f, WALL_OUTER(w, th) - 12.0f, world_edge_distance(w, x, z));
}

bool world_canal_near(const World *w, float x, float z, float margin) {
    float q[REGION_COUNT];
    quadrant_weights(warped_angle(w, x, z), q);
    if (q[REGION_FOREST] < 0.5f) return false;
    return fabsf(world_edge_distance(w, x, z) - world_canal_offset(w, atan2f(z, x))) < CANAL_HALF + margin;
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
            // Playas de arena volcanica que alternan con roquerios: donde el ruido lo dice, la
            // franja cerca del nivel del mar se aplana (una playa ancha); en lo demas, la costa cae.
            float beach = smooth(-0.05f, 0.25f, noise2d(x * 0.004f, z * 0.004f, w->seed + 97u)) * q[REGION_FJORD];
            if (beach > 0.0f && h > SEA_LEVEL - 3.0f && h < SEA_LEVEL + 9.0f) {
                float flat = SEA_LEVEL - 0.3f + (h - SEA_LEVEL) * 0.25f; // agua baja de rompiente y arena suave
                h = lerpf(h, lerpf(flat, h, smooth(SEA_LEVEL + 3.0f, SEA_LEVEL + 9.0f, h)), beach);
            }
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
        if (q[REGION_FOREST] > 0.0f) { // del otro lado del canal, un muro de roca de estratos levantados (fase 5)
            float t = smooth(WALL_OUTER(w, th), WALL_OUTER(w, th) - 75.0f, e);
            if (t > 0.0f) {
                // Ocho estratos, cada uno con su escalon; las capas suben y bajan a lo largo del muro (levantadas).
                float layers = t * 8.0f, step = floorf(layers), frac = layers - step;
                float tilt = 38.0f * sinf(th * 40.0f + w->warp_phase * 7.0f);
                float wall = (step + smooth(0.6f, 1.0f, frac)) / 8.0f * 120.0f + tilt * t;
                h = lerpf(h, h + fmaxf(0.0f, wall), q[REGION_FOREST]);
            }
        }
    }
    float d = sqrtf(x * x + z * z);
    if (d < CAMP_DOME_R) h += CAMP_DOME_H * (1.0f - smooth(0.0f, CAMP_DOME_R, d));
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

// Lo mas bajo de la orilla de un lago: el relieve en el anillo donde se ve su agua (lake_dist de 1 a 1,3,
// con la misma orilla deformada). La grilla del agua es de 40 m: su desborde puede quedar mas alto que un
// bajo entre dos celdas, y el lago no tiene que pasarse por ahi.
static float lake_rim(const World *w, const Lake *l) {
    float lo = 1e9f;
    for (int k = 0; k < 48; k++) {
        float a = atan2f(sinf((float)k * PI_F / 24.0f), cosf((float)k * PI_F / 24.0f));
        float wob = 1.0f + 0.28f * noise2d(a * 1.7f, l->r * 0.01f, w->seed + 91u);
        for (float t = 1.0f; t < 1.35f; t += 0.1f) lo = fminf(lo, land(w, l->x + cosf(a) * t * l->r * wob, l->z + sinf(a) * t * l->r * wob));
    }
    return lo;
}

float world_land(const World *w, float x, float z) { return land(w, x, z); }

float world_lake_shore(const World *w, int i) { return i >= 0 && i < w->lake_count ? lake_rim(w, &w->lakes[i]) : 0.0f; }

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

void world_water_bodies(const World *w, float x, float z, WaterBodies *b) {
    b->ground = b->sea_floor = b->canal = -1e9f;
    b->lake = -1;
    for (int k = 0; k < WATER_RIVERS_HERE; k++) b->river[k] = -1, b->river_level[k] = -1e9f, b->river_along[k] = 0.0f;
    float th = atan2f(z, x), e = world_edge_distance(w, x, z);
    if (e < SHORE_E + 1100.0f) {
        float q[REGION_COUNT];
        quadrant_weights(warped_angle(w, x, z), q);
        if (q[REGION_FJORD] > 0.05f) {
            float h = land(w, x, z);
            if (h < SEA_LEVEL + 1.0f) b->sea_floor = h;
        }
        float ce = world_canal_offset(w, th);
        if (q[REGION_FOREST] > 0.5f && fabsf(e - ce) < CANAL_HALF + 4.0f) b->canal = canal_level(w, th);
    }
    // Un lago o un rio no flota: donde el suelo cae muy por debajo de su nivel (un cañon que
    // pasa junto a la cuenca) no hay agua suya, y manda la de abajo. Sin esto, el agua quedaba
    // en el aire con paredes hasta el fondo (blancas de hielo en invierno, turquesa en verano).
    float lake_lv = -1e9f;
    for (int i = 0; i < w->lake_count; i++) {
        const Lake *l = &w->lakes[i];
        float dx = x - l->x, dz = z - l->z;
        if (dx * dx + dz * dz > l->r * l->r * 3.2f || l->level <= lake_lv || lake_dist(w, l, x, z) >= 1.3f) continue;
        if (b->ground < -1e8f) b->ground = world_height(w, x, z);
        if (l->level - b->ground < WATER_MAX_DEPTH_LAKE) b->lake = i, lake_lv = l->level;
    }
    for (int i = 0; i < w->river_count; i++) {
        const River *r = &w->rivers[i];
        if (!river_box(r, x, z, r->width * 1.5f)) continue;
        float lv = 0.0f, along = 0.0f, d = river_dist(r, x, z, &lv, &along, NULL);
        float reach = r->kind == RIVER_BRAIDED ? r->width : r->width * 1.25f;
        if (d >= reach || lv <= b->river_level[WATER_RIVERS_HERE - 1]) continue;
        if (b->ground < -1e8f) b->ground = world_height(w, x, z);
        if (lv - b->ground >= WATER_MAX_DEPTH_RIVER) continue;
        int k = WATER_RIVERS_HERE - 1; // los mas altos primero
        for (; k > 0 && lv > b->river_level[k - 1]; k--)
            b->river[k] = b->river[k - 1], b->river_level[k] = b->river_level[k - 1], b->river_along[k] = b->river_along[k - 1];
        b->river[k] = i, b->river_level[k] = lv, b->river_along[k] = along;
    }
}

float world_water(const World *w, float x, float z, float flood, WaterKind *kind) {
    WaterBodies b;
    world_water_bodies(w, x, z, &b);
    float best = -1e9f;
    WaterKind k = WATER_NONE;
    if (b.sea_floor > -1e8f) best = SEA_LEVEL, k = WATER_SEA;
    if (b.canal > -1e8f && b.canal + 0.5f * flood > best) best = b.canal + 0.5f * flood, k = WATER_CANAL;
    if (b.lake >= 0 && w->lakes[b.lake].level + flood > best) best = w->lakes[b.lake].level + flood, k = WATER_LAKE;
    if (b.river[0] >= 0 && b.river_level[0] + 0.5f * flood > best) best = b.river_level[0] + 0.5f * flood, k = WATER_RIVER;
    if (kind) *kind = k;
    return best;
}

// ------------------------------------------------------------------ las estaciones del agua (docs/AGUA.md, fase 2)
#define ICE_LAPSE 0.08f    // grados menos por metro sobre el llano del campamento (en el juego, la altura enfria mas)
#define ICE_CANAL 1.5f     // el canal, ancho y hondo, guarda el calor: se hiela al final
#define ICE_PUDDLE -1.5f   // un charco se hiela antes que su rio
#define DRY_PUDDLES 0.3f   // en un cauce seco hay charco donde el ruido pasa de esto
#define MOUTH_BLEND 80.0f  // m antes de la boca en que un rio baja hasta el agua que lo recibe
#define GLACIER_ADVANCE 260.0f // m que avanza la lengua del glaciar sobre su pie (por el medio de sus lobulos)

WaterSeason world_full_season(float flood) { return (WaterSeason){ flood, 30.0f, 30.0f, 1.0f, 0.0f }; }

static float ice_of(float temp) { return clamp01((-temp - 1.0f) / 5.0f); } // como Climate.ice

float world_river_share(const World *w, int i, const WaterSeason *s) {
    if (i < 0 || i >= w->river_count) return 0.0f;
    const River *r = &w->rivers[i];
    return clamp01(1.0f - r->melt + r->melt * clamp01(s->melt));
}

ChannelState world_channel_state(const World *w, int i, const WaterSeason *s) {
    if (world_river_share(w, i, s) < WORLD_DRY_SHARE) return CHANNEL_DRY;
    return hazard_ice_walkable(world_river_ice(w, i, s)) ? CHANNEL_FROZEN : CHANNEL_WET;
}

static float river_deep(const River *r) { return 2.2f * fminf(1.0f, r->width / 5.0f); } // como world_height

float world_river_offset(const World *w, int i, const WaterSeason *s) {
    if (i < 0 || i >= w->river_count) return 0.0f;
    const River *r = &w->rivers[i];
    float full = clamp01(world_river_share(w, i, s) / WORLD_FULL_SHARE);
    // Con menos agua, el nivel baja hacia lo hondo del cauce (en el trenzado, hasta sus canales).
    float low = r->kind == RIVER_BRAIDED ? 0.55f : 0.4f + 0.85f * river_deep(r);
    float off = 0.5f * s->flood - (1.0f - full) * low;
    return r->source == RIVER_FROM_LAKE ? fminf(off, s->flood) : off;
}

// La temperatura de un tramo: la de su altura, mas lo que guarda segun su clase (los rios grandes van un poco
// detras del aire; los arroyos, no) y menos si lleva poca agua.
static float river_temp(const World *w, int i, const WaterSeason *s) {
    static const float KIND[RIVER_KINDS] = { 1.5f, 1.5f, 0.5f, -0.5f }; // meandro, tributario, trenzado, arroyo
    const River *r = &w->rivers[i];
    float full = clamp01(world_river_share(w, i, s) / WORLD_FULL_SHARE), slow = r->kind == RIVER_CREEK ? 0.0f : 0.5f;
    return lerpf(s->temp, s->temp_late, slow) - ICE_LAPSE * (r->level[r->n / 2] - w->camp_height) + KIND[r->kind] - 1.5f * (1.0f - full);
}

float world_river_ice(const World *w, int i, const WaterSeason *s) {
    return i < 0 || i >= w->river_count ? 0.0f : ice_of(river_temp(w, i, s));
}

float world_lake_ice(const World *w, int i, const WaterSeason *s) {
    if (i < 0 || i >= w->lake_count) return 0.0f;
    const Lake *l = &w->lakes[i];
    float big = smooth(20.0f, 150.0f, l->r); // los chicos (y los oasis) siguen el aire de hoy
    return ice_of(lerpf(s->temp, s->temp_late, big) - ICE_LAPSE * (l->level - w->camp_height));
}

float world_sea_ice(const World *w, float x, float z, float sea_floor, const WaterSeason *s) {
    if (s->sea_ice <= 0.0f || sea_floor < -1e8f) return 0.0f;
    // Crece desde la orilla, donde es bajo, y con mas frio llega a lo mas hondo; el borde, irregular.
    float n = 0.5f + 0.5f * noise2d(x * 0.012f, z * 0.012f, w->seed + 741u);
    float reach = (1.0f + 11.0f * s->sea_ice) * (0.6f + 0.8f * n); // hasta 17 m de hondo: mar adentro sigue abierto
    return clamp01((reach - (SEA_LEVEL - sea_floor)) / 3.0f) * smooth(0.0f, 0.4f, s->sea_ice);
}

float world_glacier_reach(const World *w, float x, float z) {
    float e = world_edge_distance(w, x, z);
    if (e > ICE_E + 30.0f + 1.5f * GLACIER_ADVANCE) return 2.0f;
    float q[REGION_COUNT];
    quadrant_weights(warped_angle(w, x, z), q);
    if (q[REGION_HIGHLAND] < 0.5f) return 2.0f;
    // Lenguas: el frente avanza mas por unos lados que por otros, y menos hacia las cordilleras de los costados.
    float th = atan2f(z, x);
    float lobe = 0.55f + 0.9f * (0.5f + 0.5f * noise2d(cosf(th) * 9.0f, sinf(th) * 9.0f, w->seed + 743u));
    lobe *= fmaxf(0.05f, smooth(0.5f, 0.9f, q[REGION_HIGHLAND]));
    return (e - ICE_E - 30.0f) / (GLACIER_ADVANCE * lobe);
}

// Cuanto cambia el nivel de un tramo en (along): lo suyo, y cerca de la boca baja con el agua que lo recibe si
// esa baja mas (sin un escalon en la junta; esa agua, a su vez, puede bajar con la que la recibe a ella).
static float river_offset_at(const World *w, int i, float along, const WaterSeason *s, int depth) {
    const River *r = &w->rivers[i];
    float off = world_river_offset(w, i, s), end = r->along[r->n - 1], k = smooth(end - MOUTH_BLEND, end, along);
    if (k <= 0.0f) return off;
    float to = off;
    if (r->end == RIVER_TO_LAKE) to = s->flood;
    else if (r->end == RIVER_TO_CANAL) to = 0.5f * s->flood;
    else if (r->end == RIVER_TO_SEA) to = 0.0f;
    else if ((r->end == RIVER_TO_NEXT || r->end == RIVER_TO_RIVER) && r->into >= 0 && r->into < w->river_count && depth < 16) {
        const River *t = &w->rivers[r->into];
        int j = 0; // la junta: el primer punto del tramo siguiente, o el punto del rio donde llega
        for (int q = 0; r->end == RIVER_TO_RIVER && q < t->n; q++)
            if (t->x[q] == r->x[r->n - 1] && t->z[q] == r->z[r->n - 1]) {
                j = q;
                break;
            }
        to = river_offset_at(w, r->into, t->along[j], s, depth + 1);
    }
    return to < off ? lerpf(off, to, k) : off;
}

// El nivel de un tramo en (along), con su hielo. Seco: solo unos charcos en lo hondo del cauce.
static float river_now(const World *w, int i, float lv, float along, const WaterSeason *s, float *ice) {
    const River *r = &w->rivers[i];
    float t = river_temp(w, i, s);
    *ice = ice_of(t);
    if (world_river_share(w, i, s) < WORLD_DRY_SHARE) {
        if (noise2d(along * 0.035f, (float)i * 3.7f, w->seed + 733u) < DRY_PUDDLES) return -1e9f;
        *ice = ice_of(t + ICE_PUDDLE);
        return lv - (r->kind == RIVER_BRAIDED ? 0.5f : 0.4f + 0.8f * river_deep(r));
    }
    return lv + river_offset_at(w, i, along, s, 0);
}

float world_river_level(const World *w, int i, int k, const WaterSeason *s, float *ice) {
    float dummy;
    if (!ice) ice = &dummy;
    *ice = 0.0f;
    if (i < 0 || i >= w->river_count || k < 0 || k >= w->rivers[i].n) return -1e9f;
    return river_now(w, i, w->rivers[i].level[k], w->rivers[i].along[k], s, ice);
}

float world_water_resolve(const World *w, const WaterBodies *b, float x, float z, const WaterSeason *s, WaterNow *out) {
    WaterNow n = { -1e9f, WATER_NONE, 0.0f, -1, -1 };
    if (b->sea_floor > -1e8f) n.level = SEA_LEVEL, n.kind = WATER_SEA, n.ice = world_sea_ice(w, x, z, b->sea_floor, s);
    if (b->canal > -1e8f && b->canal + 0.5f * s->flood > n.level) {
        n.level = b->canal + 0.5f * s->flood, n.kind = WATER_CANAL;
        n.ice = ice_of(s->temp_late - ICE_LAPSE * (b->canal - w->camp_height) + ICE_CANAL);
    }
    if (b->lake >= 0 && w->lakes[b->lake].level + s->flood > n.level) {
        n.level = w->lakes[b->lake].level + s->flood, n.kind = WATER_LAKE;
        n.ice = world_lake_ice(w, b->lake, s), n.lake = b->lake;
    }
    for (int k = 0; k < WATER_RIVERS_HERE && b->river[k] >= 0; k++) {
        float ice, lv = river_now(w, b->river[k], b->river_level[k], b->river_along[k], s, &ice);
        if (lv > n.level) n.level = lv, n.kind = WATER_RIVER, n.ice = ice, n.lake = -1, n.river = b->river[k];
    }
    if (out) *out = n;
    return n.level;
}

float world_water_now(const World *w, float x, float z, const WaterSeason *s, WaterNow *out) {
    WaterBodies b;
    world_water_bodies(w, x, z, &b);
    return world_water_resolve(w, &b, x, z, s, out);
}

bool world_river_flow(const World *w, int river, float along, float *dx, float *dz, float *slope) {
    if (river < 0 || river >= w->river_count) return false;
    const River *r = &w->rivers[river];
    if (r->n < 2) return false;
    int lo = 0, hi = r->n - 2; // el trecho [lo, lo + 1] donde cae along
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (r->along[mid] <= along) lo = mid;
        else hi = mid - 1;
    }
    float vx = r->x[lo + 1] - r->x[lo], vz = r->z[lo + 1] - r->z[lo], l = sqrtf(vx * vx + vz * vz);
    if (l < 1e-4f) return false;
    *dx = vx / l, *dz = vz / l;
    if (slope) *slope = fmaxf(0.0f, (r->level[lo] - r->level[lo + 1]) / l);
    return true;
}

float world_river_bank(const World *w, float x, float z, float *width) {
    float best = 0.0f;
    for (int i = 0; i < w->river_count; i++) {
        const River *r = &w->rivers[i];
        if (r->kind == RIVER_BRAIDED || !river_box(r, x, z, r->width * 3.0f)) continue;
        float lv = 0.0f, d = river_dist(r, x, z, &lv, NULL, NULL);
        // Barras de cantos: el cauce y una orilla de hasta 2,6 anchos, mas ancha en los rios grandes.
        float b = 1.0f - smooth(r->width * 1.3f, r->width * 2.6f + 3.0f, d);
        if (b > best) {
            best = b;
            if (width) *width = r->width;
        }
    }
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

// Hasta donde puede llegar el agua de un lago desde su centro: 1,3 radios (world_water) por la
// orilla irregular (hasta 1,28).
#define LAKE_REACH (1.3f * 1.28f)

bool world_footprint_dry(const World *w, float x, float z, float r, bool sea_ok) {
    for (int i = 0; i < w->lake_count; i++) { // los lagos, por su forma
        const Lake *l = &w->lakes[i];
        float reach = r + l->r * LAKE_REACH, dx = x - l->x, dz = z - l->z;
        if (dx * dx + dz * dz < reach * reach) return false;
    }
    for (int i = 0; i < w->river_count; i++) { // los rios: su eje lejos de la huella
        const River *rv = &w->rivers[i];
        float lv;
        if (river_box(rv, x, z, r + rv->width * 2.0f) && river_dist(rv, x, z, &lv, NULL, NULL) < r + rv->width * 1.5f) return false;
    }
    // El canal y el mar (grandes): el centro y dos anillos de 12 puntos.
    for (int ring = 0; ring <= 2; ring++)
        for (int k = 0; k < (ring ? 12 : 1); k++) {
            float a = (float)k * (PI_F / 6.0f) + (float)ring * 0.26f, rr = r * 0.5f * (float)ring;
            float px = x + cosf(a) * rr, pz = z + sinf(a) * rr;
            WaterKind wk;
            if (world_water(w, px, pz, 3.0f, &wk) > world_height(w, px, pz) - 0.5f && !(sea_ok && wk == WATER_SEA)) return false;
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
    l->spill_x = x, l->spill_z = z;
    l->outflow = -1; // los oasis no desbordan; el del campamento, lo arma make_water
    w->lake_count++;
}

static bool lake_clear(const World *w, float x, float z, float r) {
    for (int i = 0; i < w->lake_count; i++) {
        float dx = x - w->lakes[i].x, dz = z - w->lakes[i].z, min = r + w->lakes[i].r + 160.0f;
        if (dx * dx + dz * dz < min * min) return false;
    }
    return x * x + z * z > (r + 120.0f) * (r + 120.0f); // lejos del llano del campamento
}

// El tramo listo: su caja y los metros recorridos desde la cabecera de su rio (along0 en su punto 0).
static void piece_finish(River *rv, float along0) {
    rv->minx = rv->maxx = rv->x[0], rv->minz = rv->maxz = rv->z[0];
    rv->along[0] = along0;
    for (int k = 1; k < rv->n; k++) {
        rv->minx = fminf(rv->minx, rv->x[k]), rv->maxx = fmaxf(rv->maxx, rv->x[k]);
        rv->minz = fminf(rv->minz, rv->z[k]), rv->maxz = fmaxf(rv->maxz, rv->z[k]);
        rv->along[k] = rv->along[k - 1] + sqrtf((rv->x[k] - rv->x[k - 1]) * (rv->x[k] - rv->x[k - 1]) + (rv->z[k] - rv->z[k - 1]) * (rv->z[k] - rv->z[k - 1]));
    }
}

// ------------------------------------------------------------------ el agua baja (docs/AGUA.md, fase 1)
// La hidrologia (src/sim/hydro.h) sobre una grilla de 40 m: lo que llueve en cada region, el deshielo del
// glaciar, las salidas (el mar, el canal, el cañon del desierto) y el relieve. De ahi salen los lagos (cada
// depresion, llena hasta su punto de desborde) y los rios (por donde se junta el agua), y cada lago desborda
// en un rio.
#define HYDRO_CELL_M 40.0f
#define HYDRO_GRID_SIDE 288   // 288 x 40 m: todo el mundo y un poco de afuera
#define HYDRO_PATHS 96
#define GLACIER_MELT 14.0f    // el deshielo: el agua de catorce celdas de lluvia por celda de glaciar
#define FLOW_CREEK 450.0f     // desde aca, un arroyo (celdas de lluvia de su cuenca)
#define FLOW_RIVER 2600.0f    // un rio: de meandros en la estepa y el desierto, tributario en el bosque
#define FLOW_BRAID 9000.0f    // un rio trenzado (el deshielo, en el altiplano y la costa)
#define LAKE_MIN_DEPTH 1.5f   // una depresion es un lago con al menos este hondo...
#define LAKE_MIN_CELLS 4      // ... y estas celdas
#define STREAM_STEP 20.0f     // m entre los puntos de un rio
#define STREAM_PTS 1600
#define FALL_DROP 3.0f        // una cascada: el agua cae al menos esto...
#define FALL_SLOPE 0.25f      // ... con mas pendiente que esto (m por m)
// Cuanto llueve en cada region (estepa, bosque, altiplano, fiordos, desierto), en celdas de lluvia. En el
// desierto no nace ningun rio: solo lo cruzan los que vienen de afuera (y tiene sus oasis).
static const float REGION_RAIN[REGION_COUNT] = { 0.55f, 1.0f, 0.75f, 1.0f, 0.0f };

// El suelo para el agua: el relieve con las cuencas de los lagos ya puestos (el del campamento).
static float hydro_ground_at(const World *w, float x, float z) {
    float h = land(w, x, z);
    for (int i = 0; i < w->lake_count; i++) {
        const Lake *l = &w->lakes[i];
        float dx = x - l->x, dz = z - l->z;
        if (dx * dx + dz * dz > l->r * l->r * 3.2f) continue;
        float d = lake_dist(w, l, x, z), depth = l->oasis ? 2.0f : 5.0f;
        if (d < 1.0f) h = fminf(h, l->level - 0.6f - depth * (1.0f - d * d));
    }
    return h;
}

static HydroCell hydro_cell_cb(const void *ud, float x, float z, float *height, float *rain, float *melt) {
    const World *w = (const World *)ud;
    float e = world_edge_distance(w, x, z);
    *rain = 0.0f, *melt = 0.0f;
    if (e < -80.0f) { // lejos, afuera: un sumidero sin relieve (el agua del borde cae del mundo)
        *height = -1000.0f;
        return HCELL_OUT;
    }
    float h = hydro_ground_at(w, x, z);
    *height = h;
    if (e < 0.0f) return HCELL_OUT;
    float q[REGION_COUNT];
    quadrant_weights(warped_angle(w, x, z), q);
    if (e < SHORE_E + 1100.0f && q[REGION_FJORD] > 0.05f && h < SEA_LEVEL + 1.0f) return HCELL_SEA; // como world_water
    if (q[REGION_FOREST] > 0.5f && fabsf(e - world_canal_offset(w, atan2f(z, x))) < CANAL_HALF) return HCELL_CANAL;
    if (q[REGION_DESERT] > 0.5f && e < GORGE_E + 45.0f) return HCELL_SINK; // el cañon se traga el agua
    float k[REGION_COUNT];
    world_region_weights(w, x, z, k);
    for (int i = 0; i < REGION_COUNT; i++) *rain += k[i] * REGION_RAIN[i];
    // El deshielo: el pie del muro de hielo y lo que queda sobre la nieve de todo el año.
    float ice = q[REGION_HIGHLAND] * (1.0f - smooth(ICE_E + 80.0f, ICE_E + 420.0f, e));
    float top = w->camp_height + WORLD_SNOWLINE_MAX;
    float snow = k[REGION_HIGHLAND] * smooth(top - 5.0f, top + 10.0f, h);
    *melt = GLACIER_MELT * fmaxf(ice, snow);
    return HCELL_LAND;
}

// Un rio entero (antes de partirlo en tramos): los puntos de la cabecera a la boca.
typedef struct {
    float x[STREAM_PTS], z[STREAM_PTS], level[STREAM_PTS], flow[STREAM_PTS], melt[STREAM_PTS];
    unsigned char kind[STREAM_PTS];
    int n;
} Stream;
static Stream g_stream, g_stream2;

static void stream_add(Stream *s, float x, float z, float flow, float melt) {
    if (s->n >= STREAM_PTS) return;
    s->x[s->n] = x, s->z[s->n] = z, s->flow[s->n] = flow, s->melt[s->n] = melt;
    s->n++;
}

// Puntos cada STREAM_STEP m por el mismo camino.
static void stream_resample(Stream *s) {
    Stream *o = &g_stream2;
    o->n = 0;
    if (s->n < 2) return;
    stream_add(o, s->x[0], s->z[0], s->flow[0], s->melt[0]);
    float carry = 0.0f;
    for (int i = 0; i + 1 < s->n; i++) {
        float dx = s->x[i + 1] - s->x[i], dz = s->z[i + 1] - s->z[i], len = sqrtf(dx * dx + dz * dz);
        float t = STREAM_STEP - carry;
        while (len > 0.0f && t <= len) {
            float f = t / len;
            stream_add(o, s->x[i] + dx * f, s->z[i] + dz * f, lerpf(s->flow[i], s->flow[i + 1], f), lerpf(s->melt[i], s->melt[i + 1], f));
            t += STREAM_STEP;
        }
        carry = len - (t - STREAM_STEP);
    }
    // La boca, justa (la junta con su rio, punto por punto): si quedo a menos de un paso, la reemplaza.
    float lx = o->x[o->n - 1] - s->x[s->n - 1], lz = o->z[o->n - 1] - s->z[s->n - 1];
    if (lx * lx + lz * lz < 0.25f * STREAM_STEP * STREAM_STEP && o->n > 1) o->n--;
    stream_add(o, s->x[s->n - 1], s->z[s->n - 1], s->flow[s->n - 1], s->melt[s->n - 1]);
    *s = *o;
}

// Las esquinas de la grilla, redondeadas: cada punto, hacia el medio de sus vecinos (las puntas quedan).
static void stream_smooth(Stream *s, int passes) {
    for (int p = 0; p < passes; p++) {
        float px = s->x[0], pz = s->z[0];
        for (int i = 1; i + 1 < s->n; i++) {
            float nx = 0.25f * px + 0.5f * s->x[i] + 0.25f * s->x[i + 1], nz = 0.25f * pz + 0.5f * s->z[i] + 0.25f * s->z[i + 1];
            px = s->x[i], pz = s->z[i];
            s->x[i] = nx, s->z[i] = nz;
        }
    }
}

static RiverKind point_kind(const World *w, float x, float z, float flow) {
    if (flow < FLOW_RIVER) return RIVER_CREEK;
    Region r = world_region(w, x, z);
    if ((r == REGION_HIGHLAND || r == REGION_FJORD) && flow >= FLOW_BRAID) return RIVER_BRAIDED;
    return r == REGION_FOREST ? RIVER_TRIBUTARY : RIVER_MEANDER;
}

static float kind_below(RiverKind k) { return k == RIVER_BRAIDED ? 0.6f : k == RIVER_CREEK ? 0.9f : 1.4f; }

static float kind_width(RiverKind k, float flow) {
    if (k == RIVER_CREEK) return 2.0f + 1.5f * clamp01(flow / FLOW_RIVER);
    if (k == RIVER_BRAIDED) return 30.0f + 50.0f * clamp01(sqrtf(flow / 40000.0f));
    return 6.0f + 6.0f * clamp01(sqrtf(flow / 24000.0f));
}

// Los meandros: en lo llano, el rio de la estepa (y el tributario del bosque) va y viene a los lados de su
// camino, como una curva de seno que cambia de largo y de ancho por el camino; nunca en las puntas (sus juntas
// y su boca quedan en su lugar) ni subiendo una loma.
static void stream_meanders(const World *w, Stream *s, int seed) {
    float phase = 0.0f, along = 0.0f;
    static float ox[STREAM_PTS], oz[STREAM_PTS];
    for (int i = 0; i < s->n; i++) ox[i] = s->x[i], oz[i] = s->z[i];
    for (int i = 1; i + 1 < s->n; i++) {
        along += STREAM_STEP;
        RiverKind k = (RiverKind)s->kind[i];
        float wave = 300.0f + 160.0f * (0.5f + 0.5f * noise2d(along * 0.0013f, (float)seed * 1.7f, w->seed + 75u));
        phase += 2.0f * PI_F * STREAM_STEP / wave;
        if (k != RIVER_MEANDER && k != RIVER_TRIBUTARY) continue;
        float ends = smooth(0.0f, 140.0f, along) * smooth(0.0f, 140.0f, (float)(s->n - 1 - i) * STREAM_STEP);
        float tx = ox[i + 1] - ox[i - 1], tz = oz[i + 1] - oz[i - 1], tl = sqrtf(tx * tx + tz * tz);
        if (tl < 1e-3f) continue;
        float nx = -tz / tl, nz = tx / tl;
        float amp = (24.0f + 18.0f * (0.5f + 0.5f * noise2d(along * 0.002f, (float)seed, w->seed + 76u))) * ends;
        float off = amp * sinf(phase);
        float h0 = land(w, ox[i], oz[i]);
        for (int tries = 0; tries < 4 && fabsf(off) > 1.0f; tries++) { // sin subir la loma de al lado
            if (land(w, ox[i] + nx * off, oz[i] + nz * off) < h0 + 1.5f) break;
            off *= 0.5f;
        }
        s->x[i] = ox[i] + nx * off, s->z[i] = oz[i] + nz * off;
    }
}

// Los tramos de cada camino de la hidrologia (para que un afluente llegue a su rio).
static int g_path_first[HYDRO_PATHS], g_path_last[HYDRO_PATHS];

// Un rio, en tramos de hasta RIVER_PTS puntos y de una sola clase. Devuelve el primero (-1 si no hubo lugar).
static int stream_store(World *w, const Stream *s, RiverSource src, RiverEnd end, int into) {
    int first = -1, prev = -1, i = 0;
    float along = 0.0f;
    while (i < s->n - 1 && w->river_count < RIVERS_MAX) {
        RiverKind k = (RiverKind)s->kind[i];
        int j = i;
        while (j + 1 < s->n && j - i + 1 < RIVER_PTS && (RiverKind)s->kind[j + 1] == k) j++;
        if (j == i) j++; // al menos un tramo de dos puntos
        River *rv = &w->rivers[w->river_count];
        memset(rv, 0, sizeof(*rv));
        rv->kind = k;
        for (int t = i; t <= j; t++) rv->x[rv->n] = s->x[t], rv->z[rv->n] = s->z[t], rv->level[rv->n] = s->level[t], rv->n++;
        piece_finish(rv, along);
        along = rv->along[rv->n - 1];
        float flow = s->flow[j];
        rv->width = kind_width(k, 0.5f * (s->flow[i] + flow));
        rv->flow = flow;
        rv->melt = flow > 0.0f ? clamp01(s->melt[j] / flow) : 0.0f;
        rv->source = src;
        rv->end = end, rv->into = into;
        if (prev >= 0) w->rivers[prev].end = RIVER_TO_NEXT, w->rivers[prev].into = w->river_count;
        if (first < 0) first = w->river_count;
        prev = w->river_count++;
        i = j;
    }
    return first;
}

// Las cascadas de un rio: donde su nivel cae FALL_DROP m o mas, empinado. Lo que cae debajo del agua donde
// termina (floor_lv: el lago o el rio al que llega) no cuenta: ahi ya no cae, se hunde.
static void stream_falls(World *w, const Stream *s, float floor_lv) {
    int i = 0;
    while (i + 1 < s->n && w->fall_count < FALLS_MAX) {
        float dx = s->x[i + 1] - s->x[i], dz = s->z[i + 1] - s->z[i], d = sqrtf(dx * dx + dz * dz);
        if (d < 1e-3f || (s->level[i] - s->level[i + 1]) / d < FALL_SLOPE) {
            i++;
            continue;
        }
        int j = i + 1;
        while (j + 1 < s->n) {
            float ex = s->x[j + 1] - s->x[j], ez = s->z[j + 1] - s->z[j], e = sqrtf(ex * ex + ez * ez);
            if (e < 1e-3f || (s->level[j] - s->level[j + 1]) / e < FALL_SLOPE) break;
            j++;
        }
        float bottom = fmaxf(s->level[j], floor_lv);
        if (s->level[i] - bottom >= FALL_DROP) {
            Waterfall *f = &w->falls[w->fall_count++];
            f->x = s->x[i], f->z = s->z[i], f->bx = s->x[j], f->bz = s->z[j];
            f->top = s->level[i], f->bottom = bottom;
            f->yaw = atan2f(s->x[j] - s->x[i], s->z[j] - s->z[i]);
            f->width = kind_width((RiverKind)s->kind[i], s->flow[i]);
            f->river = -1;
            for (int r = 0; r < w->river_count; r++) // el tramo de arriba de la cascada
                for (int k = 0; k < w->rivers[r].n && f->river < 0; k++)
                    if (w->rivers[r].x[k] == s->x[i] && w->rivers[r].z[k] == s->z[i]) f->river = r;
        }
        i = j;
    }
}

// El punto de un rio (sus tramos, de first a last) mas cerca de (x, z): donde se junta un afluente.
static int nearest_on(const World *w, int first, int last, float x, float z, float *ox, float *oz) {
    int best = -1;
    float bd = 1e18f;
    for (int r = first; r >= 0 && r <= last && r < w->river_count; r++)
        for (int k = 0; k < w->rivers[r].n; k++) {
            float dx = w->rivers[r].x[k] - x, dz = w->rivers[r].z[k] - z, d = dx * dx + dz * dz;
            if (d < bd) bd = d, best = r, *ox = w->rivers[r].x[k], *oz = w->rivers[r].z[k];
        }
    return best;
}

static HydroBasin g_basins[2048];
static HydroBasin g_lake_basins[LAKES_MAX];
static int g_lake_world[LAKES_MAX]; // el lago de w->lakes de cada uno de g_lake_basins
static HydroPath g_paths[HYDRO_PATHS];

// Los lagos y los rios de la semilla, con el lago del campamento ya puesto (su cuenca entra en la grilla).
static void make_water(World *w) {
    HydroGrid grid = { HYDRO_GRID_SIDE, HYDRO_CELL_M, -0.5f * HYDRO_GRID_SIDE * HYDRO_CELL_M, -0.5f * HYDRO_GRID_SIDE * HYDRO_CELL_M };
    if (!hydro_run(&grid, hydro_cell_cb, w)) return;
    int nb = hydro_basins(LAKE_MIN_DEPTH, 1, g_basins, (int)(sizeof(g_basins) / sizeof(g_basins[0])));
    int nl = 0;
    // El lago del campamento: su cuenca es la de su centro (su nivel, el de su desborde).
    {
        Lake *l = &w->lakes[0];
        int id = hydro_basin_of(hydro_cell_at(l->x, l->z));
        HydroBasin hb = { l->x, l->z, 0, l->level, 5.0f, 0.0f, -1, -1 };
        for (int i = 0; i < nb && id >= 0; i++)
            if (g_basins[i].id == id) hb = g_basins[i];
        if (hb.spill < 0) { // ninguna celda cae adentro: el desborde es lo mas bajo de su orilla
            float lowest = 1e9f, sx = l->x, sz = l->z;
            for (int k = 0; k < 32; k++) {
                float a = (float)k / 32.0f * 2.0f * PI_F, x = l->x + cosf(a) * l->r * 1.25f, z = l->z + sinf(a) * l->r * 1.25f;
                float h = land(w, x, z);
                if (h < lowest) lowest = h, sx = x, sz = z;
            }
            hb.spill = hydro_cell_at(sx, sz);
            hb.level = fminf(l->level, lowest - 0.2f);
        }
        l->level = fminf(hb.level, lake_rim(w, l) - 0.4f);
        hydro_center(hb.spill, &l->spill_x, &l->spill_z);
        g_lake_basins[nl] = hb, g_lake_world[nl] = 0, nl++;
    }
    // Las depresiones grandes y redondas (no un barranco largo), lejos del campamento y del desierto.
    static int order[2048];
    int no = 0;
    for (int i = 0; i < nb; i++)
        if (g_basins[i].cells >= LAKE_MIN_CELLS && g_basins[i].id != g_lake_basins[0].id) order[no++] = i;
    for (int i = 1; i < no; i++) // las mas grandes primero (a igual tamaño, el orden de la grilla)
        for (int j = i; j > 0 && g_basins[order[j]].cells > g_basins[order[j - 1]].cells; j--) {
            int t = order[j];
            order[j] = order[j - 1], order[j - 1] = t;
        }
    for (int o = 0; o < no && w->lake_count < LAKES_MAX - 6; o++) {
        const HydroBasin *b = &g_basins[order[o]];
        float req = sqrtf((float)b->cells * HYDRO_CELL_M * HYDRO_CELL_M / PI_F);
        float r = fminf(150.0f, fmaxf(24.0f, 0.85f * req));
        if (b->spread > 0.85f * req) continue;                                   // larga y finita
        if (b->x * b->x + b->z * b->z < 420.0f * 420.0f) continue;               // el campamento
        if (world_region_weight(w, REGION_DESERT, b->x, b->z) > 0.4f) continue;  // el desierto solo tiene oasis
        if (world_edge_distance(w, b->x, b->z) < 350.0f) continue;
        bool clear = true;
        for (int i = 0; i < w->lake_count && clear; i++) {
            float dx = b->x - w->lakes[i].x, dz = b->z - w->lakes[i].z, min = r + w->lakes[i].r + 40.0f;
            clear = dx * dx + dz * dz > min * min;
        }
        if (!clear) continue;
        Lake *l = &w->lakes[w->lake_count];
        memset(l, 0, sizeof(*l));
        l->x = b->x, l->z = b->z, l->r = r, l->level = b->level;
        // La depresion es irregular y el lago, redondo: crece hasta cubrirla (su orilla, sobre el agua) y, si
        // no alcanza, el agua baja hasta lo mas bajo de su orilla (el rio de salida corta hasta el desborde).
        float rim = lake_rim(w, l);
        for (int g = 0; g < 4 && rim < l->level - 0.3f && l->r < 150.0f; g++) {
            l->r = fminf(150.0f, l->r * 1.12f);
            rim = lake_rim(w, l);
        }
        l->level = fminf(l->level, rim - 0.4f);
        l->glacial = world_region(w, b->x, b->z) == REGION_HIGHLAND;
        hydro_center(b->spill, &l->spill_x, &l->spill_z);
        l->outflow = -1;
        g_lake_basins[nl] = *b, g_lake_world[nl] = w->lake_count, nl++;
        w->lake_count++;
    }
    w->lakes[0].outflow = -1;
    // Los rios: del de mas agua al de menos; cada afluente llega al punto de su rio. Los arroyos sueltos,
    // mientras quede lugar: el que sale de un lago y el que le entra van siempre.
    int np = hydro_paths(FLOW_CREEK, g_lake_basins, nl, g_paths, HYDRO_PATHS);
    int lake_paths = 0;
    for (int p = 0; p < np; p++) lake_paths += g_paths[p].from_lake >= 0 || g_paths[p].end == HEND_LAKE;
    for (int p = 0; p < np; p++) {
        HydroPath *hp = &g_paths[p];
        Stream *s = &g_stream;
        s->n = 0;
        g_path_first[p] = g_path_last[p] = -1;
        bool lake_path = hp->from_lake >= 0 || hp->end == HEND_LAKE;
        lake_paths -= lake_path;
        if (hp->n < 2) continue;
        if (!lake_path && hp->flow < FLOW_RIVER && w->river_count + 2 * lake_paths + 4 >= RIVERS_MAX) continue;
        // Si el rio donde termina no se trazo (un arroyo que no entro), sigue por su cauce hasta uno que si.
        for (int guard = 0; hp->end == HEND_RIVER && hp->into >= 0 && hp->into < p && g_path_first[hp->into] < 0 && guard < 8; guard++) {
            const HydroPath *t = &g_paths[hp->into];
            int junction = hp->cells[hp->n - 1], at = -1;
            for (int i = 0; i < t->n && at < 0; i++)
                if (t->cells[i] == junction) at = i;
            if (at < 0) break;
            for (int i = at + 1; i < t->n && hp->n < HYDRO_PATH_MAX; i++) hp->cells[hp->n++] = t->cells[i];
            hp->end = t->end, hp->into = t->into;
        }
        float fx, fz;
        hydro_center(hp->cells[0], &fx, &fz);
        if (hp->from_lake >= 0) { // nace adentro del lago y sale por su desborde
            const Lake *l = &w->lakes[g_lake_world[hp->from_lake]];
            float dx = fx - l->x, dz = fz - l->z, d = sqrtf(dx * dx + dz * dz);
            if (d > 1.0f) stream_add(s, l->x + dx / d * l->r * 0.6f, l->z + dz / d * l->r * 0.6f, hydro_flow(hp->cells[0]), hydro_melt(hp->cells[0]));
        }
        for (int i = 0; i < hp->n; i++) {
            float x, z;
            hydro_center(hp->cells[i], &x, &z);
            stream_add(s, x, z, hydro_flow(hp->cells[i]), hydro_melt(hp->cells[i]));
        }
        RiverEnd end = RIVER_TO_EDGE;
        int into = -1;
        if (hp->end == HEND_LAKE && hp->into >= 0) { // termina adentro del lago
            const Lake *l = &w->lakes[g_lake_world[hp->into]];
            float dx = s->x[s->n - 1] - l->x, dz = s->z[s->n - 1] - l->z, d = sqrtf(dx * dx + dz * dz);
            if (d > l->r * 0.5f) stream_add(s, l->x + dx / d * l->r * 0.5f, l->z + dz / d * l->r * 0.5f, s->flow[s->n - 1], s->melt[s->n - 1]);
            end = RIVER_TO_LAKE, into = g_lake_world[hp->into];
        } else if (hp->end == HEND_RIVER && hp->into >= 0 && hp->into < p && g_path_first[hp->into] >= 0) {
            float jx = s->x[s->n - 1], jz = s->z[s->n - 1];
            int r = nearest_on(w, g_path_first[hp->into], g_path_last[hp->into], jx, jz, &jx, &jz);
            s->x[s->n - 1] = jx, s->z[s->n - 1] = jz; // justo en el punto de su rio
            end = RIVER_TO_RIVER, into = r;
        } else if (hp->end == HEND_OUTLET) {
            HydroCell k = hydro_kind(hp->cells[hp->n - 1]);
            end = k == HCELL_SEA ? RIVER_TO_SEA : k == HCELL_CANAL ? RIVER_TO_CANAL : k == HCELL_SINK ? RIVER_TO_SINK : RIVER_TO_EDGE;
        }
        stream_resample(s);
        if (s->n < 3) continue;
        stream_smooth(s, 3);
        // La clase de cada punto, sin tramos cortitos: una clase dura al menos 8 puntos (160 m).
        for (int i = 0; i < s->n; i++) s->kind[i] = (unsigned char)point_kind(w, s->x[i], s->z[i], s->flow[i]);
        for (int i = 1; i < s->n; i++) {
            int j = i;
            while (j < s->n && s->kind[j] == s->kind[i]) j++;
            if (s->kind[i] != s->kind[i - 1] && j - i < 8 && j < s->n)
                for (int t = i; t < j; t++) s->kind[t] = s->kind[i - 1];
        }
        stream_meanders(w, s, p);
        stream_smooth(s, 1);
        // El nivel: un poco abajo de la orilla, y nunca sube (el que sale de un lago, desde su nivel). Tampoco
        // baja del mar ni del canal donde termina. En su lago o en el rio al que llega puede hundirse un poco
        // (lo tapa esa agua), pero eso no es una cascada; en el cañon, si: cae.
        float m = hp->from_lake >= 0 ? w->lakes[g_lake_world[hp->from_lake]].level : 1e9f, floor_lv = -1e9f;
        if (end == RIVER_TO_SEA) floor_lv = SEA_LEVEL;
        else if (end == RIVER_TO_CANAL) floor_lv = canal_level(w, atan2f(s->z[s->n - 1], s->x[s->n - 1]));
        else if (end == RIVER_TO_LAKE) floor_lv = w->lakes[into].level;
        else if (end == RIVER_TO_RIVER && into >= 0) {
            const River *t = &w->rivers[into];
            for (int k = 0; k < t->n; k++)
                if (t->x[k] == s->x[s->n - 1] && t->z[k] == s->z[s->n - 1]) floor_lv = t->level[k];
        }
        if (hp->from_lake >= 0) floor_lv = fminf(floor_lv, m); // el que sale de un lago nunca arranca mas alto que el
        bool surface = end == RIVER_TO_SEA || end == RIVER_TO_CANAL;
        for (int i = 0; i < s->n; i++) {
            m = fminf(m, land(w, s->x[i], s->z[i]) - kind_below((RiverKind)s->kind[i]));
            s->level[i] = surface ? fmaxf(m, floor_lv) : m;
        }
        RiverSource src = hp->from_lake >= 0 ? RIVER_FROM_LAKE : s->melt[0] > 0.5f * s->flow[0] ? RIVER_FROM_GLACIER : RIVER_FROM_RAIN;
        int first = stream_store(w, s, src, end, into);
        if (first < 0) break; // ya no hay lugar
        g_path_first[p] = first, g_path_last[p] = w->river_count - 1;
        if (hp->from_lake >= 0) { // su rio de salida: el del desborde mas bajo (si el lago tiene dos)
            Lake *l = &w->lakes[g_lake_world[hp->from_lake]];
            if (l->outflow < 0 || hp->cells[0] == g_lake_basins[hp->from_lake].spill) {
                l->outflow = first;
                hydro_center(hp->cells[0], &l->spill_x, &l->spill_z);
            }
        }
        stream_falls(w, s, floor_lv);
    }
}

// ------------------------------------------------------------------ nada en el agua
typedef enum { PLACE_SETTLEMENT, PLACE_TRIBE, PLACE_SITE, PLACE_DEN } PlaceKind;

static bool near_any(float x, float z, float ox, float oz, float min) { return (x - ox) * (x - ox) + (z - oz) * (z - oz) < min * min; }

// ¿Toda la huella queda por debajo de la nieve de todo el año? (los pueblos y las tribus)
static bool snow_free(const World *w, float x, float z, float r) {
    float top = w->camp_height + WORLD_SNOW_FREE;
    if (world_height(w, x, z) > top) return false;
    for (int k = 0; k < 8; k++) {
        float a = (float)k * (PI_F / 4.0f);
        if (world_height(w, x + cosf(a) * r, z + sinf(a) * r) > top) return false;
    }
    return true;
}

// ¿Sirve (x, z) para la cosa idx de esa clase? Las mismas reglas que al sembrarla (su region muy
// adentro, lo seco y llano, las distancias a lo demas y al campamento) y, ademas, la huella en seco
// y, los pueblos y las tribus, por debajo de la nieve de todo el año.
static bool place_ok(const World *w, PlaceKind kind, int idx, float x, float z) {
    Region rg;
    float inset, foot, home, d2 = x * x + z * z;
    bool sea_ok = false;
    switch (kind) {
    case PLACE_SETTLEMENT: {
        const Settlement *s = &w->settlements[idx];
        rg = s->region, inset = 500.0f, home = 450.0f, foot = s->kind == SETTLE_CAPITAL ? WORLD_FOOT_CAPITAL : WORLD_FOOT_VILLAGE;
        for (int o = 0; o < w->settlement_count; o++)
            if (o != idx && near_any(x, z, w->settlements[o].x, w->settlements[o].z, 600.0f)) return false;
        for (int o = 0; o < w->tribe_count; o++)
            if (near_any(x, z, w->tribes[o].x, w->tribes[o].z, 350.0f)) return false;
        for (int o = 0; o < w->site_count; o++)
            if (near_any(x, z, w->sites[o].x, w->sites[o].z, 140.0f)) return false;
        for (int o = 0; o < w->den_count; o++)
            if (near_any(x, z, w->dens[o].x, w->dens[o].z, 260.0f)) return false;
        break;
    }
    case PLACE_TRIBE:
        rg = w->tribes[idx].region, inset = 420.0f, home = 600.0f, foot = WORLD_FOOT_TRIBE;
        for (int o = 0; o < w->settlement_count; o++)
            if (near_any(x, z, w->settlements[o].x, w->settlements[o].z, 350.0f)) return false;
        for (int o = 0; o < w->tribe_count; o++)
            if (o != idx && near_any(x, z, w->tribes[o].x, w->tribes[o].z, 450.0f)) return false;
        for (int o = 0; o < w->site_count; o++)
            if (near_any(x, z, w->sites[o].x, w->sites[o].z, 120.0f)) return false;
        break;
    case PLACE_SITE: {
        const WorldSite *st = &w->sites[idx];
        rg = st->region, home = 250.0f, foot = WORLD_FOOT_SITE;
        sea_ok = st->kind == SITE_HARBOR;
        inset = sea_ok ? 0.0f : 300.0f;
        if (sea_ok) { // el embarcadero, en la costa
            float e = world_edge_distance(w, x, z);
            if (e < SHORE_E - 30.0f || e > SHORE_E + 160.0f) return false;
        }
        for (int o = 0; o < w->settlement_count; o++)
            if (near_any(x, z, w->settlements[o].x, w->settlements[o].z, 140.0f)) return false;
        for (int o = 0; o < w->site_count; o++)
            if (o != idx && near_any(x, z, w->sites[o].x, w->sites[o].z, 160.0f)) return false;
        for (int o = 0; o < w->tribe_count; o++)
            if (near_any(x, z, w->tribes[o].x, w->tribes[o].z, 120.0f)) return false;
        break;
    }
    default:
        rg = w->dens[idx].region, inset = 400.0f, home = 400.0f, foot = WORLD_FOOT_DEN;
        for (int o = 0; o < w->settlement_count; o++)
            if (near_any(x, z, w->settlements[o].x, w->settlements[o].z, 260.0f)) return false;
        break;
    }
    if (d2 < home * home || world_region_weight(w, rg, x, z) < 0.85f) return false;
    if (rg != REGION_STEPPE && world_edge_distance(w, x, z) < inset) return false;
    if ((kind == PLACE_SETTLEMENT || kind == PLACE_TRIBE) && !snow_free(w, x, z, foot)) return false;
    return dry_spot(w, x, z) && world_footprint_dry(w, x, z, foot, sea_ok);
}

// Si la cosa no sirve donde cayo, el lugar valido mas cercano (en anillos de 20 m, hasta 2 km).
// false si no hay: queda donde estaba.
static bool settle(const World *w, PlaceKind kind, int idx, float *x, float *z) {
    if (place_ok(w, kind, idx, *x, *z)) return true;
    for (float r = 20.0f; r <= 2000.0f; r += 20.0f) {
        int n = 8 + (int)(r * 0.3f);
        for (int k = 0; k < n; k++) {
            float a = ((float)k + 0.5f) / (float)n * 2.0f * PI_F, px = *x + cosf(a) * r, pz = *z + sinf(a) * r;
            if (place_ok(w, kind, idx, px, pz)) {
                *x = px, *z = pz;
                return true;
            }
        }
    }
    return false;
}

// Nada sobre el agua ni dentro de ella: lo que cayo sobre un lago (o un rio, el canal o el mar)
// o, los pueblos y las tribus, sobre la nieve de todo el año, se corre al lugar valido mas cercano.
// Va al final y sin tocar el azar: lo demas del mundo (sus nombres, lo que esta en seco) queda igual.
static void settle_all(World *w) {
    for (int i = 0; i < w->settlement_count; i++) settle(w, PLACE_SETTLEMENT, i, &w->settlements[i].x, &w->settlements[i].z);
    for (int i = 0; i < w->tribe_count; i++) settle(w, PLACE_TRIBE, i, &w->tribes[i].x, &w->tribes[i].z);
    for (int i = 0; i < w->site_count; i++) {
        WorldSite *st = &w->sites[i];
        if (settle(w, PLACE_SITE, i, &st->x, &st->z) && st->kind == SITE_HARBOR) st->yaw = atan2f(st->x, st->z); // mirando al mar
    }
    for (int i = 0; i < w->den_count; i++) settle(w, PLACE_DEN, i, &w->dens[i].x, &w->dens[i].z);
    for (int i = 0; i < w->den_count && i < CAVES_MAX; i++) {
        const Den *d = &w->dens[i];
        cave_generate(&w->caves[w->cave_count++], w->seed, i, d->x, d->z, d->region, d->species);
    }
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
    // Las cordilleras de frontera (docs/AGUA.md, la transicion entre biomas): del altiplano al desierto (al
    // este) y del altiplano a los fiordos (al sur). Entre ellas, el agua del glaciar baja a la estepa.
    // Una sierra, no una fila: cerros anchos que se tocan, a distancias distintas, y que se corren a los lados.
    for (int c = 0; c < 2; c++) {
        float a = (c == 0 ? 0.0f : 90.0f * DEG) - w->warp_phase, wander = rng_float(&rng) * 6.0f;
        for (float r = steppe_radius(w, a) + 260.0f; r < world_edge_radius(w, a) - 420.0f && w->peak_count < PEAKS_MAX;
             r += 240.0f + rng_float(&rng) * 180.0f) {
            float aa = a + (sinf(r * 0.0016f + wander) * 3.5f + (rng_float(&rng) - 0.5f) * 4.0f) * DEG, x, z;
            polar(r + (rng_float(&rng) - 0.5f) * 120.0f, aa, &x, &z);
            Peak *p = &w->peaks[w->peak_count++];
            p->x = x, p->z = z;
            p->h = 26.0f + rng_float(&rng) * 30.0f;
            p->r = 190.0f + rng_float(&rng) * 120.0f;
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
    // El agua (docs/AGUA.md): el lago junto al campamento (siempre) y, con su cuenca, la hidrologia: los lagos
    // de las depresiones y los rios por donde se junta el agua. Despues, los oasis del desierto.
    {
        float a = rng_float(&rng) * 2.0f * PI_F, x, z;
        polar(150.0f + rng_float(&rng) * 60.0f, a, &x, &z);
        add_lake(w, x, z, 32.0f + rng_float(&rng) * 18.0f, false);
    }
    make_water(w);
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
    // Tribus nomadas: tres o cuatro por region, rivales, neutrales y amigables (de todo en cada
    // region), lejos de los pueblos, del campamento y entre si.
    for (int rg = 0; rg < REGION_COUNT; rg++) {
        int n = rg == REGION_STEPPE ? 4 : 3, first = rng_range(&rng, TRIBE_ATTITUDES);
        for (int k = 0, made = 0; k < n * 12 && made < n && w->tribe_count < TRIBES_MAX; k++) {
            float x, z;
            if (!find_spot(w, &rng, (Region)rg, 420.0f, &x, &z) || !far_from_settlements(w, x, z, 350.0f) || x * x + z * z < 600.0f * 600.0f) continue;
            bool near = false;
            for (int o = 0; o < w->tribe_count; o++) near |= (w->tribes[o].x - x) * (w->tribes[o].x - x) + (w->tribes[o].z - z) * (w->tribes[o].z - z) < 450.0f * 450.0f;
            if (near) continue;
            TribeCamp *tc = &w->tribes[w->tribe_count++];
            tc->x = x, tc->z = z, tc->region = (Region)rg;
            tc->attitude = (TribeAttitude)((first + made) % TRIBE_ATTITUDES);
            make_name(&rng, (Region)rg, tc->name, (int)sizeof(tc->name));
            made++;
        }
    }
    // Estructuras: ruinas y construcciones en uso de cada region.
    {
        static const struct { Region rg; SiteKind kind; int n; } RULES[] = {
            { REGION_STEPPE, SITE_KURGAN, 4 },       { REGION_STEPPE, SITE_BALBALS, 3 },     { REGION_STEPPE, SITE_DEER_STONE, 2 },
            { REGION_STEPPE, SITE_OVOO, 2 },         { REGION_STEPPE, SITE_WATCHTOWER, 1 },  { REGION_STEPPE, SITE_WELL, 2 },
            { REGION_STEPPE, SITE_CARAVANSERAI, 1 }, { REGION_FOREST, SITE_RUINED_FORT, 2 }, { REGION_FOREST, SITE_OVOO, 1 },
            { REGION_FOREST, SITE_WATCHTOWER, 2 },   { REGION_FOREST, SITE_PETROGLYPHS, 1 }, { REGION_HIGHLAND, SITE_OVOO, 3 },
            { REGION_HIGHLAND, SITE_DEER_STONE, 2 }, { REGION_HIGHLAND, SITE_RUINED_FORT, 1 }, { REGION_HIGHLAND, SITE_PETROGLYPHS, 2 },
            { REGION_HIGHLAND, SITE_WATCHTOWER, 1 }, { REGION_FJORD, SITE_HARBOR, 2 },       { REGION_FJORD, SITE_KURGAN, 2 },
            { REGION_FJORD, SITE_RUINED_FORT, 1 },   { REGION_FJORD, SITE_WATCHTOWER, 1 },   { REGION_FJORD, SITE_PETROGLYPHS, 1 },
            { REGION_DESERT, SITE_BURIED_CITY, 2 },  { REGION_DESERT, SITE_CARAVANSERAI, 2 }, { REGION_DESERT, SITE_WELL, 3 },
            { REGION_DESERT, SITE_RUINED_FORT, 1 },
        };
        for (size_t r = 0; r < sizeof(RULES) / sizeof(RULES[0]); r++) {
            for (int k = 0, made = 0; k < RULES[r].n * 30 && made < RULES[r].n && w->site_count < SITES_MAX; k++) {
                float x, z;
                // El embarcadero, en la costa (junto al mar); lo demas, tierra adentro.
                if (!find_spot(w, &rng, RULES[r].rg, RULES[r].kind == SITE_HARBOR ? 0.0f : 300.0f, &x, &z)) continue;
                if (RULES[r].kind == SITE_HARBOR) {
                    float e = world_edge_distance(w, x, z);
                    if (e < SHORE_E - 30.0f || e > SHORE_E + 160.0f) continue;
                }
                if (!far_from_settlements(w, x, z, 140.0f) || x * x + z * z < 250.0f * 250.0f) continue;
                bool near = false;
                for (int o = 0; o < w->site_count; o++) near |= (w->sites[o].x - x) * (w->sites[o].x - x) + (w->sites[o].z - z) * (w->sites[o].z - z) < 160.0f * 160.0f;
                for (int o = 0; o < w->tribe_count; o++) near |= (w->tribes[o].x - x) * (w->tribes[o].x - x) + (w->tribes[o].z - z) * (w->tribes[o].z - z) < 120.0f * 120.0f;
                if (near) continue;
                WorldSite *st = &w->sites[w->site_count++];
                st->x = x, st->z = z, st->region = RULES[r].rg, st->kind = RULES[r].kind;
                st->yaw = rng_float(&rng) * 2.0f * PI_F;
                if (RULES[r].kind == SITE_HARBOR) st->yaw = atan2f(x, z); // mirando al mar
                made++;
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
    settle_all(w);
}

const char *site_name(SiteKind k) {
    static const char *N[SITE_KINDS] = { N_("un kurgán"), N_("una hilera de balbales"), N_("una piedra de ciervos"),
                                         N_("una fortaleza en ruinas"), N_("una ciudad enterrada en la arena"), N_("rocas grabadas"),
                                         N_("un caravasar"), N_("una torre de vigía"), N_("un ovoo"), N_("un pozo"), N_("un embarcadero") };
    return (unsigned)k < SITE_KINDS ? N[k] : "?";
}

bool site_is_ruin(SiteKind k) { return k <= SITE_PETROGLYPHS; }

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
