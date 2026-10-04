#include "sim/swarms.h"
#include "sim/lang.h"

#include <math.h>
#include <string.h>

// clang-format off
static const SwarmDef DEFS[SWARM_KIND_COUNT] = {
    //                    nombre                   modelo                         nido                          radio vel   daño venen. cd    provoca enfado correa n
    [SWARM_FISH]       = { N_("Banco de peces"),        "animal.acuatico.peces",        NULL,                         1.6f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f,  0.0f,  0.0f, 10 },
    [SWARM_BEES]       = { N_("Enjambre de abejas"),    "animal.insecto.abejas",        "animal.insecto.colmena",     2.2f, 4.0f, 0.5f, 1.0f, 0.6f, 1.5f, 25.0f, 30.0f, 14 },
    [SWARM_WASPS]      = { N_("Avispas"),               "animal.insecto.avispas",       "animal.insecto.avispero",    1.6f, 5.0f, 1.0f, 2.5f, 0.5f, 4.0f, 18.0f, 25.0f, 10 },
    [SWARM_MOSQUITOES] = { N_("Mosquitos"),             "animal.insecto.mosquitos",     NULL,                         1.4f, 2.2f, 0.0f, 0.3f, 1.5f, 0.0f,  3.0f, 25.0f, 14 },
    [SWARM_FLIES]      = { N_("Moscas"),                "animal.insecto.moscas",        NULL,                         0.9f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  0.0f,  0.0f, 10 },
};
// clang-format on

#define MOSQUITO_SENSE 14.0f // m: los mosquitos encuentran a quien pase cerca
#define SMOKE_RANGE 3.0f
#define SMOKE_SECONDS 12.0f
#define FISH_FLEE 5.0f

const SwarmDef *swarm_def(SwarmKind k) { return &DEFS[(unsigned)k < SWARM_KIND_COUNT ? k : 0]; }

static float depth_at(const SwarmCtx *c, float x, float z) {
    return c->water_depth ? c->water_depth(c->water_ud, x, z) : 0.0f;
}

void swarm_init(Swarm *s, SwarmKind k, float x, float y, float z, Rng *rng) {
    memset(s, 0, sizeof(*s));
    const SwarmDef *d = &DEFS[k];
    s->used = true;
    s->kind = k;
    s->hx = s->cx = s->tx = x;
    s->hy = y;
    s->hz = s->cz = s->tz = z;
    s->cy = k == SWARM_FISH ? y - 0.35f : y + 1.0f;
    s->active = true;
    s->n = d->members;
    for (int i = 0; i < s->n; i++) {
        SwarmBody *b = &s->b[i];
        b->alive = true;
        b->x = s->cx + (rng_float(rng) - 0.5f) * d->radius * 2.0f;
        b->y = s->cy + (rng_float(rng) - 0.5f) * (k == SWARM_FISH ? 0.4f : d->radius);
        b->z = s->cz + (rng_float(rng) - 0.5f) * d->radius * 2.0f;
    }
}

void swarm_provoke(Swarm *s) {
    const SwarmDef *d = &DEFS[s->kind];
    if (d->chase > 0.0f && s->kind != SWARM_MOSQUITOES) s->anger = d->chase, s->calm = 0.0f;
}

void swarm_smoke(Swarm *s) {
    s->anger = 0.0f;
    s->calm = SMOKE_SECONDS;
}

int swarm_alive(const Swarm *s) {
    int n = 0;
    for (int i = 0; i < s->n; i++) n += s->b[i].alive;
    return n;
}

int swarm_catch(Swarm *s, float x, float y, float z, float r, int max) {
    int caught = 0;
    for (int i = 0; i < s->n && caught < max; i++) {
        SwarmBody *b = &s->b[i];
        if (!b->alive) continue;
        float dx = b->x - x, dy = b->y - y, dz = b->z - z;
        if (dx * dx + dy * dy + dz * dz > r * r) continue;
        b->alive = false;
        caught++;
    }
    return caught;
}

static void move_center(Swarm *s, float gx, float gy, float gz, float speed, float dt) {
    float dx = gx - s->cx, dz = gz - s->cz, d = sqrtf(dx * dx + dz * dz);
    if (d > 0.05f) {
        float step = fminf(speed * dt, d);
        s->cx += dx / d * step;
        s->cz += dz / d * step;
    }
    s->cy += (gy - s->cy) * fminf(1.0f, dt * 2.0f);
}

static bool is_active(const Swarm *s, const SwarmCtx *c) {
    switch (s->kind) {
    case SWARM_BEES:
    case SWARM_WASPS: return !c->night && c->temp > 4.0f;
    case SWARM_MOSQUITOES: return c->temp > 12.0f && (c->dusk || c->night);
    case SWARM_FLIES: return c->temp > 2.0f;
    default: return true;
    }
}

SwarmHit swarm_update(Swarm *s, const SwarmCtx *c, Rng *rng, float dt) {
    SwarmHit hit = { 0, 0.0f, 0.0f };
    const SwarmDef *d = &DEFS[s->kind];
    s->cooldown = fmaxf(0.0f, s->cooldown - dt);
    s->anger = fmaxf(0.0f, s->anger - dt);
    s->calm = fmaxf(0.0f, s->calm - dt);
    s->active = is_active(s, c);
    float pdx = c->px - s->cx, pdz = c->pz - s->cz, dp = sqrtf(pdx * pdx + pdz * pdz);
    float dnest = sqrtf((c->px - s->hx) * (c->px - s->hx) + (c->pz - s->hz) * (c->pz - s->hz));

    float gx = s->hx, gy = s->hy + 1.0f, gz = s->hz, speed = 1.0f;
    if (!s->active) {
        s->anger = 0.0f;
        gy = s->hy + 0.3f; // dentro del nido o escondidos
    } else if (s->kind == SWARM_FISH) {
        gy = s->hy - 0.35f; // cerca de la superficie: se ven desde la orilla
        if (dp < FISH_FLEE && !c->player_down) { // huyen de quien se acerca
            float l = dp > 0.01f ? dp : 1.0f;
            gx = s->cx - pdx / l * 4.0f, gz = s->cz - pdz / l * 4.0f, speed = d->speed;
        } else {
            s->timer -= dt;
            if (s->timer <= 0.0f) {
                for (int k = 0; k < 6; k++) { // un sitio con agua bastante honda
                    float a = rng_float(rng) * 6.2831853f, r = rng_float(rng) * 8.0f;
                    float x = s->hx + cosf(a) * r, z = s->hz + sinf(a) * r;
                    if (depth_at(c, x, z) > 0.9f) {
                        s->tx = x, s->tz = z;
                        break;
                    }
                }
                s->timer = 3.0f + rng_float(rng) * 4.0f;
            }
            gx = s->tx, gz = s->tz, speed = 0.8f;
        }
    } else {
        // Provocar: acercarse al nido (avispas) o pasar cerca de los mosquitos.
        if (!c->player_down && s->calm <= 0.0f) {
            if (d->provoke > 0.0f && dnest < d->provoke) s->anger = fmaxf(s->anger, d->chase);
            if (s->kind == SWARM_MOSQUITOES && dp < MOSQUITO_SENSE) s->anger = fmaxf(s->anger, d->chase);
        }
        // Humo y fuego: las abejas y avispas se calman; los mosquitos se espantan.
        bool fire_near = c->torch && dp < SMOKE_RANGE + (s->kind == SWARM_MOSQUITOES ? 1.5f : 0.0f);
        if (fire_near && s->kind != SWARM_FLIES) swarm_smoke(s);
        if (c->submerged) s->anger = 0.0f; // bajo el agua te pierden
        if (s->anger > 0.0f && dnest < d->leash && !c->player_down) {
            gx = c->px, gy = c->py + 1.3f, gz = c->pz, speed = d->speed;
        } else if (s->calm > 0.0f && s->kind == SWARM_MOSQUITOES && dp < 6.0f) {
            float l = dp > 0.01f ? dp : 1.0f;
            gx = s->cx - pdx / l * 4.0f, gz = s->cz - pdz / l * 4.0f, speed = d->speed;
        } else if (s->kind == SWARM_BEES) { // pecorean alrededor de la colmena
            s->timer -= dt;
            if (s->timer <= 0.0f) {
                float a = rng_float(rng) * 6.2831853f, r = rng_float(rng) * 5.0f;
                s->tx = s->hx + cosf(a) * r, s->tz = s->hz + sinf(a) * r;
                s->timer = 4.0f + rng_float(rng) * 4.0f;
            }
            gx = s->tx, gz = s->tz;
        }
    }
    float ox = s->cx, oz = s->cz;
    move_center(s, gx, gy, gz, speed, dt);
    if (s->kind == SWARM_FISH && c->water_depth && depth_at(c, s->cx, s->cz) < 0.6f) { // no salen del agua
        s->cx = ox, s->cz = oz;
        s->tx = s->hx, s->tz = s->hz;
        s->timer = 0.0f;
    }

    // Individuos: bandada alrededor del centro.
    float rad = s->active ? d->radius : d->radius * 0.3f;
    float jitter = s->kind == SWARM_FISH ? 1.5f : 9.0f;
    for (int i = 0; i < s->n; i++) {
        SwarmBody *b = &s->b[i];
        if (!b->alive) continue;
        float ax = (s->cx - b->x), ay = (s->cy - b->y), az = (s->cz - b->z);
        float dist = sqrtf(ax * ax + ay * ay + az * az);
        float pull = dist > rad ? 4.0f : 1.0f; // cohesion: fuera del radio, vuelven con fuerza
        b->vx += (ax * pull + (rng_float(rng) - 0.5f) * jitter) * dt;
        b->vy += (ay * pull + (rng_float(rng) - 0.5f) * jitter * 0.4f) * dt;
        b->vz += (az * pull + (rng_float(rng) - 0.5f) * jitter) * dt;
        const SwarmBody *o = &s->b[(i + 1) % s->n]; // separacion del vecino
        float sx = b->x - o->x, sy = b->y - o->y, sz = b->z - o->z, sd = sx * sx + sy * sy + sz * sz;
        if (sd < 0.04f && sd > 1e-6f) b->vx += sx * 2.0f * dt, b->vy += sy * 2.0f * dt, b->vz += sz * 2.0f * dt;
        float vmax = speed + (s->kind == SWARM_FISH ? 1.0f : 3.0f);
        float v = sqrtf(b->vx * b->vx + b->vy * b->vy + b->vz * b->vz);
        if (v > vmax) b->vx *= vmax / v, b->vy *= vmax / v, b->vz *= vmax / v;
        b->x += b->vx * dt, b->y += b->vy * dt, b->z += b->vz * dt;
        if (s->kind == SWARM_FISH) { // bajo la superficie, sobre el fondo
            float floor = s->hy - fmaxf(0.3f, depth_at(c, b->x, b->z)) + 0.1f;
            if (b->y > s->hy - 0.08f) b->y = s->hy - 0.08f, b->vy = -fabsf(b->vy);
            if (b->y < floor) b->y = floor, b->vy = fabsf(b->vy);
        }
    }

    // Picaduras.
    if (s->active && s->anger > 0.0f && !c->player_down && s->cooldown <= 0.0f && (d->sting > 0.0f || d->venom > 0.0f) &&
        dp < 1.6f && fabsf(s->cy - (c->py + 1.3f)) < 2.0f && swarm_alive(s) > 0) {
        hit.stings = 1;
        hit.damage = d->sting;
        hit.venom = d->venom;
        s->cooldown = d->sting_cd;
    }
    return hit;
}
