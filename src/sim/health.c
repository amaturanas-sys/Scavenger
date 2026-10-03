#include "health.h"

#include <math.h>
#include <stdio.h>

static float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

void health_init(Health *h, float hp_max) {
    *h = (Health){ .hp = hp_max, .hp_max = hp_max, .blood = 1.0f };
}

void health_init_beast(Health *h, float hp_max) {
    health_init(h, hp_max);
    h->beast = true;
}

// Daño por zona: cuello y cabeza letales; el torso y el vientre, mucho; las
// extremidades, menos (y cuanto mas lejos del cuerpo, menos).
float part_damage_scale(BodyPart p) {
    static const float k[PART_COUNT] = { 1.8f, 2.2f, 1.15f, 1.25f, 1.0f, 0.7f, 0.55f, 0.7f, 0.55f, 0.85f, 0.6f, 0.85f, 0.6f };
    return (unsigned)p < PART_COUNT ? k[p] : 1.0f;
}

// Sangrado por zona: el cuello (yugular), el vientre y el muslo (femoral) sangran mas.
float part_bleed_scale(BodyPart p) {
    static const float k[PART_COUNT] = { 1.2f, 2.5f, 1.0f, 1.5f, 1.1f, 0.9f, 0.8f, 0.9f, 0.8f, 1.4f, 0.8f, 1.4f, 0.8f };
    return (unsigned)p < PART_COUNT ? k[p] : 1.0f;
}

bool part_is_arm(BodyPart p) { return p >= PART_UPPER_ARM_L && p <= PART_FOREARM_R; }
bool part_is_leg(BodyPart p) { return p >= PART_THIGH_L && p <= PART_SHIN_R; }

static BodyPart random_part(Rng *rng) {
    // Pesos (suman 100): cabeza 7, cuello 3, torax 22, abdomen 14, pelvis 8,
    // brazos 7+6 por lado, muslos 6, piernas 4.
    static const int weight[PART_COUNT] = { 7, 3, 22, 14, 8, 7, 6, 7, 6, 6, 4, 6, 4 };
    int r = rng_range(rng, 100);
    for (int p = 0; p < PART_COUNT; p++) {
        if (r < weight[p]) return (BodyPart)p;
        r -= weight[p];
    }
    return PART_THORAX;
}

int health_pick_part(Rng *rng) { return random_part(rng); }

int health_random_limb(Rng *rng) {
    static const BodyPart limbs[] = { PART_FOREARM_L, PART_FOREARM_R, PART_SHIN_L, PART_SHIN_R };
    return limbs[rng_range(rng, 4)];
}

static bool is_limb(BodyPart p) { return part_is_arm(p) || part_is_leg(p); }

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
    BodyPart bp = part == PART_RANDOM ? random_part(rng) : (BodyPart)part;
    damage *= part_damage_scale(bp);
    h->hp -= damage;
    float sev = clampf(damage / h->hp_max * 1.8f, 0.05f, 1.0f);
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
        bleed += w->severity * 0.004f * part_bleed_scale(w->part);
    }
    if (bleed > 0.0f) {
        h->blood -= bleed * dt;
        h->hp -= bleed * dt * h->hp_max * 0.5f;
    } else {
        h->blood = fminf(1.0f, h->blood + dt / (resting ? 200.0f : 600.0f));
    }
    // Veneno: quita vida poco a poco (la mitad en unos 14 s) y frena la recuperacion.
    if (h->venom > 0.0f) {
        float d = fminf(h->venom, h->venom * 0.05f * dt + 0.05f * dt);
        h->venom -= d;
        h->hp -= d;
    }
    // Vida: se recupera sin sangrado ni veneno, con sangre suficiente, hasta lo que permiten las heridas.
    float burden = 0.0f;
    for (int i = 0; i < h->wound_count; i++) burden += h->wounds[i].severity;
    float cap = h->hp_max * clampf(1.0f - burden * 0.25f, 0.3f, 1.0f);
    if (bleed == 0.0f && h->venom < 1.0f && h->blood > 0.5f && h->hp < cap)
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

void health_poison(Health *h, float amount) {
    if (!h->dead && amount > 0.0f) h->venom += amount;
}

bool health_poisoned(const Health *h) { return h->venom >= 1.0f; }

int health_treat(Health *h) {
    int n = 0;
    if (h->venom >= 1.0f) h->venom *= 0.5f, n++; // las hierbas cortan el veneno
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
    return n + (h->venom >= 2.0f);
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
    float p = limb_penalty(h, PART_THIGH_L, PART_THIGH_R, 0.7f, 0.35f) + limb_penalty(h, PART_SHIN_L, PART_SHIN_R, 0.6f, 0.3f);
    if (h->blood < 0.7f) p += 0.7f - h->blood;
    return clampf(1.0f - p, 0.25f, 1.0f);
}

float health_attack_scale(const Health *h) {
    if (h->down || h->dead) return 0.0f;
    float p = limb_penalty(h, PART_UPPER_ARM_L, PART_UPPER_ARM_R, 0.6f, 0.3f) +
              limb_penalty(h, PART_FOREARM_L, PART_FOREARM_R, 0.7f, 0.35f) + limb_penalty(h, PART_HEAD, PART_HEAD, 0.2f, 0.2f);
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

const char *part_name(BodyPart p, bool beast) {
    static const char *human[PART_COUNT] = {
        "la cabeza",         "el cuello",           "el tórax",          "el abdomen",           "la pelvis",
        "el brazo izquierdo", "el antebrazo izquierdo", "el brazo derecho", "el antebrazo derecho",
        "el muslo izquierdo", "la pierna izquierda",  "el muslo derecho",  "la pierna derecha",
    };
    static const char *animal[PART_COUNT] = {
        "la cabeza",       "el cuello",          "el pecho",         "el vientre",          "la grupa",
        "la paleta izquierda", "la pata delantera izquierda", "la paleta derecha", "la pata delantera derecha",
        "el anca izquierda", "la pata trasera izquierda", "el anca derecha", "la pata trasera derecha",
    };
    if ((unsigned)p >= PART_COUNT) return "?";
    return beast ? animal[p] : human[p];
}

const char *severity_name(float s) { return s < 0.3f ? "leve" : s < 0.6f ? "moderada" : "grave"; }

int wound_describe(const Wound *w, bool beast, char *out, int len) {
    return snprintf(out, (size_t)len, "%s en %s (%s%s%s)", wound_name(w->kind), part_name(w->part, beast),
                    severity_name(w->severity), w->bleeding ? ", sangra" : "",
                    w->treated ? (w->kind == WOUND_FRACTURE ? ", entablillada" : ", vendada") : "");
}

const char *health_state_name(const Health *h) {
    if (h->dead) return "muerto";
    if (h->down) return "abatido";
    if (h->venom >= 1.0f) return "envenenado";
    int worst = health_worst(h);
    if (h->hp < 0.35f * h->hp_max || (worst >= 0 && h->wounds[worst].severity >= 0.6f)) return "malherido";
    if (h->wound_count > 0 || h->hp < 0.9f * h->hp_max) return "herido";
    return "sano";
}
