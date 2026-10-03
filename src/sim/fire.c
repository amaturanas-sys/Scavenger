#include "sim/fire.h"

#include <math.h>
#include <string.h>

// Por combustible: segundos que arde, probabilidad de prender a un vecino y radio de lo calcinado.
static const struct {
    float burn, spread, scorch;
} FUEL[FUEL_COUNT] = {
    [FUEL_NONE] = { 0.0f, 0.0f, 0.0f },
    [FUEL_GRASS] = { 10.0f, 0.55f, 2.2f },
    [FUEL_TREE] = { 40.0f, 0.7f, 2.8f },
    [FUEL_STRUCTURE] = { 55.0f, 0.6f, 3.0f },
};

#define SPREAD_STEP 0.5f  // s entre intentos de propagacion
#define SPREAD_DIST 3.0f  // m hasta el vecino que prende
#define CELL_GAP 1.8f     // m: no hay dos focos tan juntos

void fire_init(FireField *f, unsigned seed) {
    memset(f, 0, sizeof(*f));
    rng_seed(&f->rng, seed ^ 0xF17Eu);
}

static float d2(float ax, float az, float bx, float bz) { return (ax - bx) * (ax - bx) + (az - bz) * (az - bz); }

bool fire_scorched(const FireField *f, float x, float z) {
    for (int i = 0; i < SCORCH_MAX; i++) {
        const Scorch *s = &f->scorch[i];
        if (s->r > 0.0f && s->age < SCORCH_DAYS_SECONDS && d2(s->x, s->z, x, z) < s->r * s->r) return true;
    }
    return false;
}

static bool burning_near(const FireField *f, float x, float z, float gap) {
    for (int i = 0; i < FIRE_MAX; i++)
        if (f->cells[i].used && d2(f->cells[i].x, f->cells[i].z, x, z) < gap * gap) return true;
    return false;
}

bool fire_burning(const FireField *f, FuelKind kind, int ref) {
    for (int i = 0; i < FIRE_MAX; i++)
        if (f->cells[i].used && f->cells[i].kind == kind && f->cells[i].ref == ref) return true;
    return false;
}

bool fire_ignite(FireField *f, float x, float z, FuelKind kind, int ref) { return fire_ignite_hot(f, x, z, kind, ref, 0.3f); }

bool fire_ignite_hot(FireField *f, float x, float z, FuelKind kind, int ref, float heat) {
    if (kind == FUEL_NONE) return false;
    if (kind != FUEL_GRASS ? fire_burning(f, kind, ref) : (burning_near(f, x, z, CELL_GAP) || fire_scorched(f, x, z)))
        return false;
    for (int i = 0; i < FIRE_MAX; i++) {
        FireCell *c = &f->cells[i];
        if (c->used) continue;
        *c = (FireCell){ true, x, z, heat, FUEL[kind].burn, kind, ref, 0.0f };
        return true;
    }
    return false;
}

int fire_count(const FireField *f) {
    int n = 0;
    for (int i = 0; i < FIRE_MAX; i++) n += f->cells[i].used;
    return n;
}

float fire_heat_at(const FireField *f, float x, float z, float r) {
    float h = 0.0f;
    for (int i = 0; i < FIRE_MAX; i++) {
        const FireCell *c = &f->cells[i];
        if (!c->used) continue;
        float d = sqrtf(d2(c->x, c->z, x, z));
        if (d < r) h += c->heat * (1.0f - d / r);
    }
    return h;
}

float fire_dryness(float greenness, float wetness, float snow_cover, float temperature) {
    // El pasto de la estepa arde aun verde si hace calor; el suelo mojado y la nieve, poco o nada.
    float dry = (1.0f - 0.5f * greenness) * (1.0f - 0.8f * wetness) * (1.0f - snow_cover);
    float heat = fminf(1.0f, fmaxf(0.0f, (temperature - 5.0f) / 20.0f)); // con frio cuesta mas
    return fminf(1.0f, fmaxf(0.0f, dry * heat));
}

static void add_scorch(FireField *f, float x, float z, float r) {
    for (int i = 0; i < SCORCH_MAX; i++) { // ya calcinado: se renueva en vez de gastar otro
        Scorch *s = &f->scorch[i];
        if (s->r >= r * 0.8f && d2(s->x, s->z, x, z) < s->r * s->r * 0.25f) {
            s->age = 0.0f;
            return;
        }
    }
    f->scorch[f->scorch_next] = (Scorch){ x, z, r, 0.0f };
    f->scorch_next = (f->scorch_next + 1) % SCORCH_MAX;
}

static void push(FireEvent *out, int max, int *n, FireEventKind k, const FireCell *c) {
    if (*n < max && out) out[(*n)++] = (FireEvent){ k, c->kind, c->ref, c->x, c->z };
}

int fire_update(FireField *f, const FireEnv *env, float dt, FireEvent *out, int max) {
    int n = 0;
    for (int i = 0; i < SCORCH_MAX; i++)
        if (f->scorch[i].r > 0.0f) f->scorch[i].age += dt;
    // Arder: el foco se aviva, consume su combustible y la lluvia lo enfria.
    for (int i = 0; i < FIRE_MAX; i++) {
        FireCell *c = &f->cells[i];
        if (!c->used) continue;
        c->age += dt;
        c->fuel -= dt;
        if (c->fuel > 0.0f) c->heat = fminf(1.0f, c->heat + dt * 0.4f * (1.0f - env->rain)); // mojado no se aviva
        else c->heat -= dt * 0.5f;
        c->heat -= env->rain * dt * (c->kind == FUEL_GRASS ? 1.2f : 0.7f); // la lluvia apaga todo fuego
        if (c->heat > 0.0f) continue;
        bool consumed = c->fuel <= 0.0f;
        add_scorch(f, c->x, c->z, FUEL[c->kind].scorch * (consumed ? 1.0f : 0.6f));
        push(out, max, &n, consumed ? FIRE_EV_BURNED_OUT : FIRE_EV_EXTINGUISHED, c);
        c->used = false;
    }
    // Propagarse a puntos cercanos con combustible, a favor del viento y con el pasto seco.
    f->spread_timer += dt;
    if (f->spread_timer < SPREAD_STEP) return n;
    f->spread_timer = 0.0f;
    float wl = sqrtf(env->wind_x * env->wind_x + env->wind_z * env->wind_z);
    for (int i = 0; i < FIRE_MAX; i++) {
        FireCell *c = &f->cells[i];
        if (!c->used || c->heat < 0.5f || !env->fuel_at) continue;
        float ang = rng_float(&f->rng) * 6.2831853f, dx = cosf(ang), dz = sinf(ang);
        float with_wind = wl > 0.01f ? (dx * env->wind_x + dz * env->wind_z) / wl : 0.0f; // [-1, 1]
        float dist = SPREAD_DIST * (1.0f + 0.5f * fmaxf(0.0f, with_wind) * fminf(1.0f, wl / 6.0f));
        float x = c->x + dx * dist, z = c->z + dz * dist;
        int ref = -1;
        FuelKind k = env->fuel_at(env->ud, x, z, &ref);
        if (k == FUEL_NONE) continue;
        float p = FUEL[k].spread * env->dryness * (1.0f - env->rain) * c->heat * (1.0f + 0.8f * with_wind * fminf(1.0f, wl / 6.0f));
        if (rng_float(&f->rng) >= p) continue;
        if (fire_ignite(f, x, z, k, ref)) push(out, max, &n, FIRE_EV_SPREAD, c);
    }
    return n;
}

// ---------------------------------------------------------------- azar del clima
bool fire_wildfire_roll(bool summer, float dryness, float rain, float dt, Rng *rng) {
    if (!summer || dryness < 0.35f || rain > 0.05f) return false;
    float per_s = 0.0012f * (dryness - 0.35f) / 0.65f; // con todo seco, uno cada ~14 min de juego
    return rng_float(rng) < per_s * dt;
}

bool fire_lightning_roll(float storm, float dt, Rng *rng) {
    if (storm < 0.5f) return false;
    return rng_float(rng) < dt / 20.0f; // un rayo que toca tierra cada ~20 s
}

float fire_lightning_ignite_chance(float dryness, float rain) {
    return fminf(0.9f, 0.45f * (1.0f - 0.6f * rain) + 0.4f * dryness);
}

float structure_daily_wear(bool rained) { return rained ? 0.09f : 0.05f; }

float structure_collapse_chance(float condition, float rain, float dt) {
    if (rain < RAIN_TORRENTIAL || condition >= STRUCTURE_NEGLECTED) return 0.0f;
    float neglect = (STRUCTURE_NEGLECTED - condition) / STRUCTURE_NEGLECTED; // (0, 1]
    float storm = (rain - RAIN_TORRENTIAL) / (1.0f - RAIN_TORRENTIAL);    // [0, 1]
    return fminf(1.0f, (0.002f + 0.01f * neglect) * (0.4f + 0.6f * storm) * dt);
}
