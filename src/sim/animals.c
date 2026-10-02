#include "sim/animals.h"

#include <math.h>
#include <string.h>

static const SpeciesDef SPECIES[SPECIES_COUNT] = {
    [SPECIES_HORSE] = { "Caballo estepario", "animal.montura.caballo_estepario", 7.0f, 14.0f, 0.45f, true, 2.4f },
    [SPECIES_CAMEL] = { "Camello bactriano", "animal.montura.camello_bactriano", 4.5f, 10.0f, 0.55f, true, 1.7f },
    [SPECIES_DEER] = { "Ciervo", "animal.salvaje.ciervo", 8.0f, 18.0f, 0.25f, false, 1.0f },
    [SPECIES_WOLF] = { "Lobo", "animal.salvaje.lobo", 7.5f, 12.0f, 0.12f, false, 1.0f },
    [SPECIES_IBEX] = { "Íbice", "animal.salvaje.ibice", 6.5f, 16.0f, 0.2f, false, 1.0f },
};

#define WANDER_RADIUS 18.0f
#define WALK_SPEED 1.2f
#define FOLLOW_GAP 4.0f

const SpeciesDef *species_def(Species s) { return s >= 0 && s < SPECIES_COUNT ? &SPECIES[s] : NULL; }

void animal_init(Animal *a, Species s, float x, float z) {
    memset(a, 0, sizeof(*a));
    a->species = s;
    a->state = ANIMAL_WILD;
    a->x = a->home_x = a->tx = x;
    a->z = a->home_z = a->tz = z;
}

static void move_towards(Animal *a, float tx, float tz, float speed, float dt) {
    float dx = tx - a->x, dz = tz - a->z, d = sqrtf(dx * dx + dz * dz);
    if (d < 0.05f) return;
    float step = fminf(speed * dt, d);
    a->x += dx / d * step;
    a->z += dz / d * step;
    a->yaw = atan2f(dx, dz);
}

static void update_inner(Animal *a, float dt, float px, float pz, Rng *rng);

void animal_update(Animal *a, float dt, float px, float pz, Rng *rng) {
    if (a->ridden) return; // lo mueve el jinete (y fija su velocidad)
    float x0 = a->x, z0 = a->z;
    update_inner(a, dt, px, pz, rng);
    a->speed = dt > 0.0f ? sqrtf((a->x - x0) * (a->x - x0) + (a->z - z0) * (a->z - z0)) / dt : 0.0f;
}

static void update_inner(Animal *a, float dt, float px, float pz, Rng *rng) {
    const SpeciesDef *d = &SPECIES[a->species];
    float dx = a->x - px, dz = a->z - pz, dist = sqrtf(dx * dx + dz * dz);
    if (a->state == ANIMAL_WILD && dist < d->flee_dist) {
        // Huye en direccion contraria al jugador.
        a->fleeing = true;
        float inv = dist > 0.01f ? 1.0f / dist : 1.0f;
        move_towards(a, a->x + dx * inv * 5.0f, a->z + dz * inv * 5.0f, d->speed, dt);
        return;
    }
    a->fleeing = false;
    if (a->state != ANIMAL_WILD && dist > FOLLOW_GAP && dist < 30.0f) {
        // Domado: sigue al jugador si esta cerca; si no, pasta en el campamento.
        move_towards(a, px - dx / dist * FOLLOW_GAP, pz - dz / dist * FOLLOW_GAP, d->speed * 0.6f, dt);
        return;
    }
    // Deambular por su territorio.
    a->timer -= dt;
    if (a->timer <= 0.0f) {
        float ang = rng_float(rng) * 6.2831853f, r = rng_float(rng) * WANDER_RADIUS;
        a->tx = a->home_x + cosf(ang) * r;
        a->tz = a->home_z + sinf(ang) * r;
        a->timer = 4.0f + rng_float(rng) * 6.0f;
    }
    move_towards(a, a->tx, a->tz, WALK_SPEED, dt);
}

bool animal_try_tame(Animal *a, Rng *rng, float bonus, float camp_x, float camp_z) {
    if (a->state != ANIMAL_WILD) return false;
    if (rng_float(rng) >= SPECIES[a->species].tame_chance + bonus) return false;
    a->state = ANIMAL_TAMED;
    a->fleeing = false;
    a->home_x = a->tx = camp_x; // su territorio pasa a ser el campamento
    a->home_z = a->tz = camp_z;
    return true;
}

bool animal_saddle(Animal *a) {
    if (a->state != ANIMAL_TAMED || !SPECIES[a->species].rideable) return false;
    a->state = ANIMAL_SADDLED;
    return true;
}

bool animal_can_ride(const Animal *a) { return a->state == ANIMAL_SADDLED; }
