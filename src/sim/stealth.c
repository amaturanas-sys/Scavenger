#include "sim/stealth.h"

#include <math.h>

#define DEG2RAD_F 0.017453293f

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

float stealth_facing(float ex, float ez, float eyaw, float px, float pz) {
    float dx = px - ex, dz = pz - ez, d = sqrtf(dx * dx + dz * dz);
    if (d < 1e-4f) return 1.0f;
    return (sinf(eyaw) * dx + cosf(eyaw) * dz) / d;
}

bool stealth_behind(float ex, float ez, float eyaw, float px, float pz) {
    float dx = px - ex, dz = pz - ez;
    if (dx * dx + dz * dz > BACKSTAB_RANGE * BACKSTAB_RANGE) return false;
    // A la espalda: el angulo con la direccion contraria al frente, menor que medio cono.
    return stealth_facing(ex, ez, eyaw, px, pz) <= -cosf(BACKSTAB_CONE * 0.5f * DEG2RAD_F);
}

bool stealth_backstab(float ex, float ez, float eyaw, float px, float pz, bool noticed, bool stunned) {
    return stealth_behind(ex, ez, eyaw, px, pz) && (!noticed || stunned);
}

float stealth_sight_scale(float facing, float noise) {
    float behind = 0.03f + 0.5f * clampf(noise, 0.0f, 1.0f); // por detras solo oye
    float k = clampf((facing + 0.4f) / 0.7f, 0.0f, 1.0f);    // 0 a la espalda, 1 de frente
    return behind + (1.0f - behind) * k;
}

bool stealth_hears_kill(float dist) { return dist <= SILENT_ALERT_RANGE; }

bool hostage_blocks_shot(float sx, float sz, float tx, float tz, float hx, float hz) {
    float dx = tx - sx, dz = tz - sz, len2 = dx * dx + dz * dz;
    if (len2 < 1e-6f) return false;
    float t = ((hx - sx) * dx + (hz - sz) * dz) / len2; // donde cae el rehen sobre la linea
    if (t <= 0.0f || t >= 1.0f) return false;             // detras del arquero o del blanco
    float cx = sx + dx * t - hx, cz = sz + dz * t - hz;
    return cx * cx + cz * cz <= HOSTAGE_SHIELD_RADIUS * HOSTAGE_SHIELD_RADIUS;
}

float down_wake_seconds(Rng *rng) { return DOWN_WAKE_MIN + rng_float(rng) * (DOWN_WAKE_MAX - DOWN_WAKE_MIN); }

bool down_tick(float *wake, float dt) {
    if (*wake <= 0.0f) return false;
    *wake -= dt;
    return *wake <= 0.0f;
}

bool down_is_lethal(const Health *h) {
    if (h->dead || h->blood < 0.15f) return true;
    for (int i = 0; i < h->wound_count; i++) {
        const Wound *w = &h->wounds[i];
        bool open = w->kind == WOUND_CUT || w->kind == WOUND_BITE || w->kind == WOUND_BURN;
        if (!open) continue; // contundente: tumba, no mata
        if ((w->part == PART_HEAD || w->part == PART_NECK) && w->severity >= 0.6f) return true;
        if ((w->part == PART_THORAX || w->part == PART_ABDOMEN) && w->severity >= 0.85f) return true;
    }
    return false;
}
