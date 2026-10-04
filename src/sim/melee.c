#include "sim/melee.h"
#include "sim/lang.h"

#include <math.h>
#include <string.h>

static const MoveDef MOVES[MOVE_COUNT] = {
    [MOVE_LIGHT] = { N_("Golpe"), "ataque_una_1", 0.0f, 0.0f, 0.0f, WOUND_CUT },
    [MOVE_HEAVY] = { N_("Golpe pesado"), "ataque_pesado", 0.0f, 0.0f, 1.3f, WOUND_CUT },
    [MOVE_KICK] = { N_("Patada"), "patada", 1.3f, 5.0f, 0.7f, WOUND_BRUISE },
    [MOVE_RUN_KICK] = { N_("Patada a la carrera"), "patada_carrera", 1.6f, 8.0f, 1.1f, WOUND_BRUISE },
    [MOVE_SHIELD_BASH] = { N_("Golpe de escudo"), "golpe_escudo", 1.2f, 4.0f, 0.8f, WOUND_BRUISE },
    [MOVE_SHIELD_CHARGE] = { N_("Carga con escudo"), "carga_escudo", 1.6f, 7.0f, 1.2f, WOUND_BRUISE },
    [MOVE_GRAPPLE] = { N_("Agarre"), "agarre", 1.1f, 6.0f, 1.2f, WOUND_BRUISE },
    [MOVE_HOOK] = { N_("Gancho al escudo"), "enganchar_escudo", 0.0f, 0.0f, 0.9f, WOUND_CUT },
};

const MoveDef *move_def(MeleeMove m) { return &MOVES[(unsigned)m < MOVE_COUNT ? m : 0]; }

static bool has(const char *id, const char *what) { return id && strstr(id, what); }

bool weapon_can_hook(const char *id) {
    return has(id, "hacha") || has(id, "guja") || has(id, "alabarda") || has(id, "gancho") || has(id, "bisarma");
}

bool weapon_is_short(const char *id) {
    return has(id, "daga") || has(id, "cuchillo") || has(id, "sable") || has(id, "maza") || !id || !id[0];
}

bool weapon_is_long(const char *id) {
    return has(id, "lanza") || has(id, "pica") || has(id, "guja") || has(id, "alabarda") || has(id, "espada");
}

float melee_combo_scale(int step) {
    static const float k[3] = { 1.0f, 1.15f, 1.45f }; // el tercero remata
    return k[(step < 1 ? 1 : step > 3 ? 3 : step) - 1];
}

float melee_combo_cooldown(const char *id, float base, int step) {
    float k = weapon_is_short(id) ? 0.65f : weapon_is_long(id) ? 1.0f : 0.85f;
    if (step >= 3) k *= 1.4f; // tras el remate, un respiro
    return base * k;
}

static float roll(Rng *rng) { return rng_float(rng); }

MeleeResult melee_resolve(MeleeMove m, const Fighter *att, const Fighter *def, const char *weapon, int combo_step,
                          Rng *rng) {
    MeleeResult r;
    memset(&r, 0, sizeof(r));
    const MoveDef *md = move_def(m);
    r.wound = md->wound;
    bool front = def->facing > 0.35f;
    bool guard = def->blocking && !def->down && !def->staggered && front;
    bool shield_guard = guard && def->shield;
    float mass = 1.0f + def->weight / 40.0f; // la armadura pesa: cuesta mas tumbarlo
    float down_k = def->down ? 1.5f : 1.0f;
    switch (m) {
    case MOVE_LIGHT:
    case MOVE_HEAVY: {
        WeaponStats w = weapon_stats(weapon);
        bool heavy = m == MOVE_HEAVY;
        r.wound = w.wound;
        float dmg = w.damage * att->strength * (heavy ? 1.8f : melee_combo_scale(combo_step));
        r.landed = true;
        if (shield_guard) {
            r.blocked = heavy || roll(rng) < 0.85f;
            if (heavy) r.staggered = true, dmg *= 0.3f; // el pesado rompe la guardia
            else if (r.blocked) dmg = 0.0f;
        } else if (guard) { // parada con el arma
            r.blocked = roll(rng) < (heavy ? 0.15f : 0.4f);
            if (r.blocked) dmg *= 0.15f;
        }
        r.damage = dmg * down_k;
        if (!r.blocked) {
            if (heavy) r.staggered = true;
            // El remate de un arma larga puede tumbar.
            if (!heavy && combo_step >= 3 && weapon_is_long(weapon) && roll(rng) < 0.3f / mass) r.knocked_down = true;
            if (heavy && roll(rng) < 0.2f / mass) r.knocked_down = true;
        }
        break;
    }
    case MOVE_KICK:
        r.landed = true;
        if (shield_guard) { // contra el escudo: lo aparta y le quita la guardia, sin tumbarlo
            r.blocked = true;
            r.staggered = true;
        } else {
            r.damage = md->damage * att->strength * down_k;
            r.staggered = true;
            r.knocked_down = roll(rng) < 0.35f / mass;
        }
        break;
    case MOVE_RUN_KICK: // la inercia tumba incluso con el escudo en guardia
        r.landed = true;
        r.damage = md->damage * att->strength * down_k * (shield_guard ? 0.5f : 1.0f);
        r.knocked_down = roll(rng) < (shield_guard ? 0.8f : 0.9f) / mass;
        r.staggered = !r.knocked_down;
        break;
    case MOVE_SHIELD_BASH:
        if (!att->shield) break;
        r.landed = true;
        if (shield_guard) { // escudo contra escudo: los dos se desequilibran
            r.blocked = r.staggered = r.attacker_staggered = true;
        } else {
            r.damage = md->damage * att->strength * down_k;
            r.staggered = true;
            r.knocked_down = roll(rng) < 0.15f / mass;
        }
        break;
    case MOVE_SHIELD_CHARGE:
        if (!att->shield) break;
        r.landed = true;
        r.damage = md->damage * att->strength * down_k * (shield_guard ? 0.4f : 1.0f);
        r.knocked_down = roll(rng) < (shield_guard ? 0.35f : 0.65f) * att->strength / mass;
        r.staggered = true;
        break;
    case MOVE_GRAPPLE: {
        // Que agarra: el escudo con el que se cubre, el brazo con el que golpea, o una extremidad.
        r.grab = def->shield && def->blocking ? GRAB_SHIELD : def->attacking && def->armed ? GRAB_WEAPON_ARM : GRAB_LIMB;
        float a = att->strength * (0.6f + 0.6f * roll(rng)) * (0.5f + 0.5f * att->health);
        float d = def->strength * (0.5f + 0.6f * roll(rng)) * (0.4f + 0.6f * def->health) * (1.0f + def->weight / 80.0f);
        if (def->staggered) d *= 0.6f;
        if (!def->down && a <= d) { // se zafa: el que agarra queda expuesto
            r.attacker_staggered = true;
            break;
        }
        r.landed = true;
        r.damage = md->damage * att->strength * down_k;
        if (r.grab == GRAB_SHIELD) {
            r.shield_dropped = true;
            r.staggered = true;
            r.knocked_down = roll(rng) < 0.3f;
        } else {
            r.disarmed = r.grab == GRAB_WEAPON_ARM;
            r.knocked_down = true; // la llave lo tumba
        }
        break;
    }
    case MOVE_HOOK: {
        if (!weapon_can_hook(weapon)) break;
        r.landed = true;
        if (!def->shield) { // sin escudo que enganchar: un tiron con el filo
            r.damage = weapon_stats(weapon).damage * 0.5f * att->strength * down_k;
            r.wound = WOUND_CUT;
            break;
        }
        float k = fminf(0.95f, 0.75f * att->strength / fmaxf(0.3f, def->strength));
        r.shield_dropped = roll(rng) < k;
        r.staggered = true;
        break;
    }
    default: break;
    }
    return r;
}

bool hands_swap(Hands *h) {
    if (h->right.kind == INV_HANDS_TWO || h->right.kind == INV_HANDS_SHIELD || h->left.kind == INV_HANDS_SHIELD) return false;
    if (!h->right.id[0] && !h->left.id[0]) return false;
    Held t = h->right;
    h->right = h->left;
    h->left = t;
    return true;
}

const char *hands_attack_weapon(const Hands *h, int combo_step, bool *off_hand) {
    if (off_hand) *off_hand = false;
    if (h->sheathed) return "";
    bool right_weapon = h->right.id[0] && h->right.kind != INV_HANDS_SHIELD;
    bool left_weapon = h->left.id[0] && h->left.kind == INV_HANDS_ONE;
    // Con un arma en cada mano, los golpes del combo alternan (derecha, izquierda, derecha).
    if (right_weapon && left_weapon && combo_step % 2 == 0) {
        if (off_hand) *off_hand = true;
        return h->left.id;
    }
    if (right_weapon) return h->right.id;
    if (left_weapon) {
        if (off_hand) *off_hand = true; // solo en la izquierda: la mano torpe
        return h->left.id;
    }
    return "";
}
