#include "health.h"

#include <math.h>
#include <stdio.h>

static float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

void health_init(Health *h, float hp_max) {
    *h = (Health){ .hp = hp_max, .hp_max = hp_max, .blood = 1.0f };
}

static BodyPart random_part(Rng *rng) {
    // Pesos: cabeza 6, torso 40, brazos 14 y 14, piernas 13 y 13.
    static const int weight[PART_COUNT] = { 6, 40, 14, 14, 13, 13 };
    int r = rng_range(rng, 100);
    for (int p = 0; p < PART_COUNT; p++) {
        if (r < weight[p]) return (BodyPart)p;
        r -= weight[p];
    }
    return PART_TORSO;
}

static bool is_limb(BodyPart p) { return p >= PART_ARM_L; }

static void update_state(Health *h) {
    if (h->dead) return;
    if (h->blood <= 0.05f || h->hp <= -0.5f * h->hp_max) {
        h->dead = true;
        h->down = true;
        return;
    }
    if (h->hp <= 0.0f || h->blood < 0.35f) h->down = true;
    else if (h->down && h->hp > 0.25f * h->hp_max && h->blood >= 0.45f) h->down = false;
}

int health_hit(Health *h, Rng *rng, float damage, WoundKind kind, int part) {
    if (h->dead || damage <= 0.0f) return -1;
    h->hp -= damage;
    float sev = clampf(damage / h->hp_max * 1.8f, 0.05f, 1.0f);
    BodyPart bp = part == PART_RANDOM ? random_part(rng) : (BodyPart)part;
    if (kind == WOUND_BRUISE && sev > 0.55f && is_limb(bp)) kind = WOUND_FRACTURE; // un golpe fuerte rompe el hueso
    bool bleeds = ((kind == WOUND_CUT || kind == WOUND_BITE) && sev >= 0.15f) || (kind == WOUND_FRACTURE && sev > 0.8f);
    // La misma herida sin tratar empeora en vez de duplicarse.
    int idx = -1;
    for (int i = 0; i < h->wound_count; i++) {
        Wound *w = &h->wounds[i];
        if (w->part == bp && w->kind == kind && !w->treated) {
            w->severity = fminf(1.0f, w->severity + sev * 0.7f);
            w->bleeding |= bleeds;
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (h->wound_count < WOUNDS_MAX) {
            idx = h->wound_count++;
        } else { // sin lugar: empeora la herida menos grave
            idx = 0;
            for (int i = 1; i < WOUNDS_MAX; i++)
                if (h->wounds[i].severity < h->wounds[idx].severity) idx = i;
        }
        h->wounds[idx] = (Wound){ kind, bp, sev, bleeds, false };
    }
    update_state(h);
    return idx;
}

static void remove_wound(Health *h, int i) { h->wounds[i] = h->wounds[--h->wound_count]; }

void health_update(Health *h, Rng *rng, float dt, bool resting, float healer) {
    if (h->dead) return;
    // Sangrado: las heridas abiertas vacian la sangre; los cortes leves coagulan solos.
    float bleed = 0.0f;
    for (int i = 0; i < h->wound_count; i++) {
        Wound *w = &h->wounds[i];
        if (!w->bleeding) continue;
        if (w->severity < 0.3f && rng_float(rng) < dt * 0.02f) {
            w->bleeding = false;
            continue;
        }
        bleed += w->severity * 0.004f;
    }
    if (bleed > 0.0f) {
        h->blood -= bleed * dt;
        h->hp -= bleed * dt * h->hp_max * 0.5f;
    } else {
        h->blood = fminf(1.0f, h->blood + dt / (resting ? 200.0f : 600.0f));
    }
    // Vida: se recupera sin sangrado y con sangre suficiente, hasta lo que permiten las heridas.
    float burden = 0.0f;
    for (int i = 0; i < h->wound_count; i++) burden += h->wounds[i].severity;
    float cap = h->hp_max * clampf(1.0f - burden * 0.25f, 0.3f, 1.0f);
    if (bleed == 0.0f && h->blood > 0.5f && h->hp < cap)
        h->hp = fminf(cap, h->hp + h->hp_max * dt / (resting ? 120.0f : 400.0f));
    // Cicatrizacion: tratadas y en reposo, mas rapido; las fracturas solo entablilladas.
    for (int i = 0; i < h->wound_count; i++) {
        Wound *w = &h->wounds[i];
        if (w->kind == WOUND_FRACTURE && !w->treated) continue;
        float rate = (w->treated ? 1.0f / 400.0f : 1.0f / 1600.0f) * (resting ? 2.0f : 1.0f) * (1.0f + healer);
        if (w->kind == WOUND_FRACTURE) rate *= 0.3f;
        w->severity -= rate * dt;
        if (w->severity <= 0.0f) remove_wound(h, i--);
    }
    update_state(h);
}

int health_treat(Health *h) {
    int n = 0;
    for (int i = 0; i < h->wound_count; i++) {
        Wound *w = &h->wounds[i];
        bool needs = w->bleeding || (w->kind == WOUND_FRACTURE && !w->treated) || (!w->treated && w->severity >= 0.3f);
        if (!needs) continue;
        w->bleeding = false;
        w->treated = true;
        n++;
    }
    return n;
}

void health_daily(Health *h, float healer) {
    if (h->dead) return;
    if (healer > 0.0f) health_treat(h);
    for (int i = 0; i < h->wound_count; i++) {
        Wound *w = &h->wounds[i];
        if (w->kind == WOUND_FRACTURE && !w->treated) continue;
        w->severity -= w->treated ? 0.35f * (1.0f + healer) : 0.12f;
        if (w->severity <= 0.0f) remove_wound(h, i--);
    }
    if (!health_bleeding(h)) h->blood = fminf(1.0f, h->blood + 0.4f);
    h->hp = fminf(h->hp_max, fmaxf(h->hp, 0.0f) + h->hp_max * 0.5f);
    update_state(h);
}

void health_revive(Health *h) {
    if (h->dead) return;
    h->hp = fmaxf(h->hp, 0.25f * h->hp_max + 1.0f);
    h->blood = fmaxf(h->blood, 0.46f);
    h->down = false;
}

bool health_bleeding(const Health *h) {
    for (int i = 0; i < h->wound_count; i++)
        if (h->wounds[i].bleeding) return true;
    return false;
}

int health_untreated(const Health *h) {
    int n = 0;
    for (int i = 0; i < h->wound_count; i++) {
        const Wound *w = &h->wounds[i];
        n += w->bleeding || (w->kind == WOUND_FRACTURE && !w->treated) || (!w->treated && w->severity >= 0.3f);
    }
    return n;
}

static float limb_penalty(const Health *h, BodyPart a, BodyPart b, float fracture, float other) {
    float p = 0.0f;
    for (int i = 0; i < h->wound_count; i++) {
        const Wound *w = &h->wounds[i];
        if (w->part != a && w->part != b) continue;
        p += w->severity * (w->kind == WOUND_FRACTURE ? fracture : other) * (w->treated ? 0.6f : 1.0f);
    }
    return p;
}

float health_speed_scale(const Health *h) {
    if (h->down || h->dead) return 0.0f;
    float p = limb_penalty(h, PART_LEG_L, PART_LEG_R, 0.7f, 0.35f);
    if (h->blood < 0.7f) p += 0.7f - h->blood;
    return clampf(1.0f - p, 0.25f, 1.0f);
}

float health_attack_scale(const Health *h) {
    if (h->down || h->dead) return 0.0f;
    float p = limb_penalty(h, PART_ARM_L, PART_ARM_R, 0.7f, 0.35f) + limb_penalty(h, PART_HEAD, PART_HEAD, 0.2f, 0.2f);
    if (h->blood < 0.7f) p += 0.7f - h->blood;
    return clampf(1.0f - p, 0.3f, 1.0f);
}

int health_worst(const Health *h) {
    int best = -1;
    for (int i = 0; i < h->wound_count; i++)
        if (best < 0 || h->wounds[i].severity > h->wounds[best].severity) best = i;
    return best;
}

const char *wound_name(WoundKind k) {
    static const char *names[WOUND_COUNT] = { "Corte", "Golpe", "Fractura", "Mordida", "Congelación" };
    return (unsigned)k < WOUND_COUNT ? names[k] : "?";
}

const char *part_name(BodyPart p) {
    static const char *names[PART_COUNT] = { "la cabeza", "el torso", "el brazo izquierdo", "el brazo derecho",
                                             "la pierna izquierda", "la pierna derecha" };
    return (unsigned)p < PART_COUNT ? names[p] : "?";
}

const char *severity_name(float s) { return s < 0.3f ? "leve" : s < 0.6f ? "moderada" : "grave"; }

int wound_describe(const Wound *w, char *out, int len) {
    return snprintf(out, (size_t)len, "%s en %s (%s%s%s)", wound_name(w->kind), part_name(w->part),
                    severity_name(w->severity), w->bleeding ? ", sangra" : "",
                    w->treated ? (w->kind == WOUND_FRACTURE ? ", entablillada" : ", vendada") : "");
}

const char *health_state_name(const Health *h) {
    if (h->dead) return "muerto";
    if (h->down) return "abatido";
    int worst = health_worst(h);
    if (h->hp < 0.35f * h->hp_max || (worst >= 0 && h->wounds[worst].severity >= 0.6f)) return "malherido";
    if (h->wound_count > 0 || h->hp < 0.9f * h->hp_max) return "herido";
    return "sano";
}
