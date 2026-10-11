#include "sim/riding.h"

#include <math.h>

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float wrap_angle(float a) { return atan2f(sinf(a), cosf(a)); }

// ---------------------------------------------------------------- arqueria montada
float mounted_shot_spread(float speed, float rel_yaw, float riding, float archery) {
    riding = clampf(riding, 0.0f, 1.0f), archery = clampf(archery, 0.0f, 1.0f);
    speed = clampf(speed, 0.0f, 12.0f);
    float s = 1.0f - 0.6f * archery;                         // la punteria (amuleto del aguila...)
    s *= 1.0f + speed / RIDE_GALLOP * (1.0f - 0.6f * riding); // la carrera; la monta la corrige
    // Hacia atras, girado en la montura: cuanto mas rapido, mas cuesta (quieto, se gira el caballo).
    if (fabsf(wrap_angle(rel_yaw)) > PARTHIAN_ANGLE) s *= 1.0f + 0.4f * fminf(1.0f, speed / RIDE_GALLOP) * (1.0f - 0.5f * riding);
    return s;
}

bool shot_is_parthian(float speed, float rel_yaw) { return speed >= RIDE_GALLOP && fabsf(wrap_angle(rel_yaw)) > PARTHIAN_ANGLE; }

// ---------------------------------------------------------------- carga con lanza
float lance_charge_damage(float base, float speed) {
    if (speed < LANCE_MIN_SPEED) return 0.0f;
    return base * fminf(2.5f, 1.0f + (speed - LANCE_MIN_SPEED) / 3.0f);
}

float lance_charge_knock(float speed, bool blocked) {
    if (speed < LANCE_MIN_SPEED) return 0.0f;
    float p = clampf(0.55f + 0.08f * (speed - LANCE_MIN_SPEED), 0.0f, 0.95f);
    return blocked ? p * 0.35f : p;
}

// ---------------------------------------------------------------- el cansancio del caballo
float mount_stamina_step(float stamina, bool galloping, bool moving, float hunger, float thirst, float grit, float dt) {
    float worn = 1.0f + (hunger > 1.0f ? 0.5f : 0.0f) + (thirst > 1.0f ? 0.5f : 0.0f); // con hambre o sed, peor
    grit = clampf(grit, -0.5f, 0.5f);
    if (galloping) stamina -= MOUNT_GALLOP_COST * worn * (1.0f - grit) * dt;
    else stamina += (moving ? MOUNT_REGEN_WALK : MOUNT_REGEN_REST) / worn * (1.0f + grit) * dt;
    return clampf(stamina, 0.0f, 1.0f);
}

bool mount_winded(float stamina, bool was_winded) { return stamina <= 0.0f || (was_winded && stamina < MOUNT_RECOVER); }

float mount_speed_scale(float pace, float hunger, float thirst) {
    return (1.0f + clampf(pace, -0.5f, 0.5f)) * (hunger > 1.0f ? 0.85f : 1.0f) * (thirst > 1.0f ? 0.85f : 1.0f);
}

// ---------------------------------------------------------------- arqueros a caballo
void horse_archer_goal(float ex, float ez, float tx, float tz, float dir, bool *retreat, float *gx, float *gz) {
    float dx = ex - tx, dz = ez - tz, d = sqrtf(dx * dx + dz * dz);
    if (d < 1e-3f) dx = 1.0f, dz = 0.0f, d = 1.0f;
    float ux = dx / d, uz = dz / d; // del blanco hacia el arquero
    if (*retreat && d > HA_FAR) *retreat = false;
    if (!*retreat && d < HA_CLOSE) *retreat = true;
    if (*retreat) { // al galope, lejos del blanco
        *gx = ex + ux * 10.0f, *gz = ez + uz * 10.0f;
        return;
    }
    // Rodear: de costado, corrigiendo hacia la orbita (lejos, se acerca en espiral).
    float s = dir < 0.0f ? -1.0f : 1.0f;
    float k = (HA_ORBIT - d) / HA_ORBIT * 2.0f;
    float mx = -uz * s + ux * k, mz = ux * s + uz * k, ml = sqrtf(mx * mx + mz * mz);
    *gx = ex + mx / ml * 8.0f, *gz = ez + mz / ml * 8.0f;
}
