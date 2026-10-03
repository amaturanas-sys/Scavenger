#include "hazards.h"

#include <math.h>

#include "noise.h"

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static float smoothstep(float a, float b, float x) {
    float t = clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

float biome_desert(uint32_t seed, float x, float z) {
    float d = sqrtf(x * x + z * z);
    if (d < 220.0f) return 0.0f; // el campamento esta en la estepa
    float region = smoothstep(0.12f, 0.42f, fbm2d(x * 0.0011f, z * 0.0011f, seed + 404u, 3));
    return region * smoothstep(220.0f, 380.0f, d);
}

// --------------------------------------------------------------------- frio
float hazard_feels_like(float temperature, float wind, float wet, float clothing, float fire) {
    return temperature + clothing + fire - wind * 10.0f - wet * 10.0f;
}

void warmth_update(Warmth *w, float feels_like, float rain, bool near_fire, float dt) {
    // La lluvia empapa; el fuego (y, mas despacio, el aire) seca.
    if (rain > 0.05f && !near_fire) w->wet += rain * dt / 60.0f;
    else w->wet -= dt / (near_fire ? 20.0f : 120.0f);
    w->wet = clamp01(w->wet);
    if (feels_like >= WARMTH_COMFORT) w->heat += fminf(1.5f, (feels_like - WARMTH_COMFORT) * 0.06f + 0.2f) * dt;
    else w->heat -= (WARMTH_COMFORT - feels_like) * 0.012f * dt;
    if (w->heat < 0.0f) w->heat = 0.0f;
    if (w->heat > WARMTH_MAX) w->heat = WARMTH_MAX;
}

ColdLevel warmth_level(const Warmth *w) {
    if (w->heat > 70.0f) return COLD_WARM;
    if (w->heat > 45.0f) return COLD_COOL;
    if (w->heat > 25.0f) return COLD_COLD;
    if (w->heat > 0.0f) return COLD_FREEZING;
    return COLD_HYPOTHERMIA;
}

const char *cold_name(ColdLevel c) {
    static const char *names[] = { "abrigado", "fresco", "frío", "helado", "hipotermia" };
    return (unsigned)c <= COLD_HYPOTHERMIA ? names[c] : "?";
}

float warmth_speed_scale(const Warmth *w) { return w->heat >= 35.0f ? 1.0f : 0.6f + 0.4f * w->heat / 35.0f; }

// -------------------------------------------------------------------- barro
float hazard_mud_scale(float wetness, float snow_cover) {
    return 1.0f - 0.4f * clamp01(wetness) * (1.0f - clamp01(snow_cover));
}

// --------------------------------------------------------------------- hielo
bool hazard_ice_walkable(float ice) { return ice >= ICE_WALKABLE; }

float hazard_ice_break_chance(float ice, float speed, float load) {
    if (!hazard_ice_walkable(ice)) return 1.0f;
    float thin = 1.0f - clamp01(ice);
    float base = 0.002f + 0.06f * thin * thin;          // hielo recien formado: mas fragil
    float motion = 0.3f + (speed / 4.0f) * (speed / 4.0f); // correr golpea la capa
    return fminf(1.0f, base * motion * load);
}

// --------------------------------------------------------------- socavones
bool hazard_sinkhole_cell(uint32_t seed, int day, int cx, int cz, Sinkhole *out) {
    Rng r;
    uint32_t h = seed ^ 0x51C4u;
    h = h * 0x9E3779B1u ^ (uint32_t)day;
    h = h * 0x85EBCA6Bu ^ (uint32_t)cx;
    h = h * 0xC2B2AE35u ^ (uint32_t)cz;
    rng_seed(&r, h);
    rng_next(&r);
    if (rng_float(&r) >= SINK_CHANCE) return false;
    // Dentro de la celda, lejos de sus bordes; de 1.8 a 3.2 m de radio.
    out->x = ((float)cx + 0.15f + 0.7f * rng_float(&r)) * SINK_CELL;
    out->z = ((float)cz + 0.15f + 0.7f * rng_float(&r)) * SINK_CELL;
    out->radius = 1.8f + 1.4f * rng_float(&r);
    return true;
}

const char *sink_name(SinkKind k) {
    switch (k) {
    case SINK_SNOW: return "socavón de nieve";
    case SINK_QUICKSAND: return "arena movediza";
    default: return "";
    }
}

// ------------------------------------------------------------- minijuego
void qte_start(Qte *q, Rng *rng, int len, float per_key, int max_mistakes) {
    if (len > QTE_MAX) len = QTE_MAX;
    if (len < 1) len = 1;
    for (int i = 0; i < len; i++) {
        int k;
        do k = rng_range(rng, QTE_KEYS);
        while (i > 0 && k == q->keys[i - 1]); // nunca la misma dos veces seguidas
        q->keys[i] = k;
    }
    q->len = len;
    q->pos = 0;
    q->per_key = per_key;
    q->time_left = per_key;
    q->mistakes = 0;
    q->max_mistakes = max_mistakes;
    q->state = QTE_RUNNING;
}

static void mistake(Qte *q) {
    q->mistakes++;
    q->time_left = q->per_key;
    if (q->mistakes > q->max_mistakes) q->state = QTE_LOST;
}

void qte_update(Qte *q, float dt) {
    if (q->state != QTE_RUNNING) return;
    q->time_left -= dt;
    if (q->time_left <= 0.0f) mistake(q); // se acabo el tiempo de esta tecla
}

bool qte_press(Qte *q, int key) {
    if (q->state != QTE_RUNNING) return false;
    if (key != q->keys[q->pos]) {
        mistake(q);
        return false;
    }
    q->pos++;
    q->per_key *= 0.96f; // cada vez mas rapido
    q->time_left = q->per_key;
    if (q->pos >= q->len) q->state = QTE_WON;
    return true;
}

float qte_progress(const Qte *q) { return q->len > 0 ? (float)q->pos / (float)q->len : 0.0f; }
