#include "sim/actions.h"

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------- manos
static void set_held(Held *h, const char *id, InvHands kind) {
    snprintf(h->id, sizeof(h->id), "%s", id ? id : "");
    h->kind = id ? kind : INV_HANDS_NONE;
}

void hands_init(Hands *h) { memset(h, 0, sizeof(*h)); }

void hands_clear(Hands *h, HandSlot slot) { set_held(slot == HAND_RIGHT ? &h->right : &h->left, NULL, INV_HANDS_NONE); }

bool hands_equip(Hands *h, const InvItem *item, HandSlot slot) {
    if (!item || item->hands == INV_HANDS_NONE) return false;
    switch (item->hands) {
    case INV_HANDS_TWO: // ocupa ambas manos
        set_held(&h->right, item->id, INV_HANDS_TWO);
        hands_clear(h, HAND_LEFT);
        break;
    case INV_HANDS_SHIELD: // siempre en la izquierda
        if (h->right.kind == INV_HANDS_TWO) hands_clear(h, HAND_RIGHT);
        set_held(&h->left, item->id, INV_HANDS_SHIELD);
        break;
    default: // una mano: la pedida; suelta lo de dos manos
        if (h->right.kind == INV_HANDS_TWO) hands_clear(h, HAND_RIGHT);
        set_held(slot == HAND_RIGHT ? &h->right : &h->left, item->id, INV_HANDS_ONE);
        break;
    }
    h->sheathed = false; // empunar es desenfundar
    return true;
}

Grip hands_grip(const Hands *h) {
    InvHands r = h->right.kind, l = h->left.kind;
    if (r == INV_HANDS_TWO) return GRIP_TWO_HANDED;
    if (r == INV_HANDS_ONE && l == INV_HANDS_SHIELD) return GRIP_WEAPON_SHIELD;
    if (r == INV_HANDS_ONE && l == INV_HANDS_ONE) return GRIP_DUAL;
    if (r == INV_HANDS_ONE || l == INV_HANDS_ONE) return GRIP_ONE_HANDED;
    if (l == INV_HANDS_SHIELD) return GRIP_SHIELD;
    return GRIP_EMPTY;
}

const char *grip_name(Grip g) {
    switch (g) {
    case GRIP_EMPTY: return "desarmado";
    case GRIP_ONE_HANDED: return "a una mano";
    case GRIP_DUAL: return "una en cada mano";
    case GRIP_TWO_HANDED: return "a dos manos";
    case GRIP_WEAPON_SHIELD: return "arma y escudo";
    case GRIP_SHIELD: return "solo escudo";
    }
    return "?";
}

bool hands_toggle_sheathe(Hands *h) {
    if (hands_grip(h) == GRIP_EMPTY) return false;
    h->sheathed = !h->sheathed;
    return true;
}

bool hands_can_take(const Hands *h) {
    if (h->carried[0]) return false;
    if (h->sheathed) return true;
    return h->right.kind == INV_HANDS_NONE || h->left.kind == INV_HANDS_NONE;
}

bool hands_take(Hands *h, const char *id) {
    if (!id || !id[0] || !hands_can_take(h)) return false;
    snprintf(h->carried, sizeof(h->carried), "%s", id);
    return true;
}

bool hands_throw(Hands *h, char *out, int out_len) {
    if (!h->carried[0]) return false;
    if (out && out_len > 0) snprintf(out, (size_t)out_len, "%s", h->carried);
    h->carried[0] = '\0';
    return true;
}

bool hands_holding(const Hands *h, const char *id) {
    if (!id || !id[0]) return false;
    if (!strcmp(h->carried, id)) return true;
    if (h->sheathed) return false;
    return !strcmp(h->right.id, id) || !strcmp(h->left.id, id);
}

// ---------------------------------------------------------------- acciones individuales
static const ActionDef ACTIONS[ACTION_COUNT] = {
    [ACTION_TAKE] = { "Tomar", "Recoger un objeto del suelo. Hace falta una mano libre.",
                      ACTOR_PLAYER | ACTOR_NPC, 0.5f, NULL, NULL, "objeto cercano" },
    [ACTION_THROW] = { "Lanzar", "Arrojar el objeto que se lleva en la mano.", ACTOR_PLAYER | ACTOR_NPC, 0.6f, NULL,
                       NULL, NULL },
    [ACTION_CHANGE_GRIP] = { "Cambiar empuñadura",
                             "Una en cada mano, a dos manos, arma y escudo...", ACTOR_PLAYER | ACTOR_NPC, 0.8f,
                             NULL, NULL, NULL },
    [ACTION_SHEATHE] = { "Enfundar / desenfundar", "Guardar las armas libera las manos.",
                         ACTOR_PLAYER | ACTOR_NPC, 0.6f, NULL, NULL, NULL },
    [ACTION_PLACE_FIRE] = { "Instalar fogata", "Una fogata pequeña para una persona o dos.",
                            ACTOR_PLAYER | ACTOR_NPC, 4.0f, NULL, "estructura.campamento.fogata", NULL },
    [ACTION_PLACE_TENT] = { "Instalar tienda", "Tienda ligera de viaje.", ACTOR_PLAYER | ACTOR_NPC, 8.0f, NULL,
                            "estructura.vivienda.tienda_ligera", NULL },
    [ACTION_DIG_TRENCH] = { "Cavar trinchera", "Cobertura contra flechas y cargas. Requiere pala.",
                            ACTOR_PLAYER | ACTOR_NPC, 12.0f, "utileria.herramienta.pala",
                            "estructura.defensa.trinchera", NULL },
    [ACTION_THROW_GRAPPLE] = { "Lanzar trepa", "Gancho con cuerda para escalar muros en un asedio.",
                               ACTOR_PLAYER | ACTOR_NPC, 1.2f, "utileria.herramienta.gancho_trepa", NULL, "muro" },
    [ACTION_THROW_LASSO] = { "Lanzar lazo", "Atrapar un animal salvaje para intentar domarlo.",
                             ACTOR_PLAYER | ACTOR_NPC, 1.0f, "arma.distancia.lazo", NULL, "animal salvaje" },
    [ACTION_LIGHT_TORCH] = { "Encender antorcha", "Luz en la noche; ahuyenta fieras.", ACTOR_PLAYER | ACTOR_NPC,
                             1.5f, "utileria.objeto.antorcha", NULL, NULL },
    [ACTION_SADDLE] = { "Instalar montura", "Ensillar un animal para montarlo.", ACTOR_PLAYER | ACTOR_NPC, 5.0f,
                        "accesorio.arreo.silla_montar", NULL, "montura" },
};

const ActionDef *action_def(ActionId a) { return a >= 0 && a < ACTION_COUNT ? &ACTIONS[a] : NULL; }

// ---------------------------------------------------------------- construcciones en grupo
static const BuildDef BUILDS[BUILD_COUNT] = {
    [BUILD_SHELTER] = { "Refugio", "estructura.campamento.refugio", 900.0f, 2, 4, ROLE_NONE, ROLE_BUILDER,
                        "ramas, pieles, cuerda" },
    [BUILD_PALISADE] = { "Muro de empalizada", "estructura.campamento.empalizada", 1200.0f, 3, 6, ROLE_NONE,
                         ROLE_BUILDER, "troncos, cuerda" },
    [BUILD_STONE_WALL] = { "Muro de piedra", "estructura.defensa.muro_piedra", 2400.0f, 4, 8, ROLE_NONE,
                           ROLE_BUILDER, "piedras" },
    [BUILD_BONFIRE] = { "Hoguera", "estructura.campamento.hoguera", 300.0f, 2, 4, ROLE_NONE, ROLE_HUNTER,
                        "leña" },
    [BUILD_TOTEM] = { "Tótem de protección", "totem.proteccion.guardian", 900.0f, 2, 4, ROLE_NONE, ROLE_HEALER,
                      "tronco, pigmentos, crines" },
    [BUILD_OVEN] = { "Horno de cocina", "estructura.campamento.horno_cocina", 600.0f, 2, 3, ROLE_NONE, ROLE_COOK,
                     "barro, piedras" },
    [BUILD_FURNACE_BRONZE] = { "Horno de fundición (bronce)", "estructura.campamento.horno_bronce", 1800.0f, 3, 5,
                               ROLE_SMITH, ROLE_SMITH, "barro, piedras, carbón, cobre y estaño" },
    [BUILD_FURNACE_STEEL] = { "Horno de fundición (acero)", "estructura.campamento.horno_acero", 3000.0f, 4, 6,
                              ROLE_SMITH, ROLE_SMITH, "piedra refractaria, fuelles, carbón, hierro" },
    [BUILD_WATCHTOWER] = { "Torre de vigilancia", "estructura.campamento.atalaya", 1500.0f, 3, 6, ROLE_NONE,
                           ROLE_BUILDER, "troncos, cuerda" },
    [BUILD_CORRAL] = { "Corral", "estructura.campamento.corral", 600.0f, 2, 4, ROLE_NONE, ROLE_HUNTER,
                       "estacas, cuerda" },
};

const BuildDef *build_def(BuildId b) { return b >= 0 && b < BUILD_COUNT ? &BUILDS[b] : NULL; }

#define LOW_MORALE 30.0f

float build_worker_skill(const BuildDef *def, const Troop *t, const Member *m) {
    float s = 1.0f;
    if (m->role != ROLE_NONE && (m->role == def->skilled_role || m->role == def->required_role)) s *= 2.0f;
    if (m->champion >= 0) { // los dones de un gran guerrero tambien sirven en la obra
        unsigned g = t->champions[m->champion].gifts;
        if (g & GIFT_STRONG) s *= 1.5f;
        if (g & GIFT_ENDURING) s *= 1.25f;
        if (g & GIFT_TALL) s *= 1.2f;
    }
    if (m->morale < LOW_MORALE) s *= 0.5f; // desganado
    return s;
}

CrewPlan build_plan(const BuildDef *def, const Troop *t, bool player_helps) {
    CrewPlan plan = { BUILD_READY, 0, 0.0f };
    float skills[TROOP_MAX + 1];
    int n = 0;
    bool has_required = def->required_role == ROLE_NONE;
    for (int i = 0; i < t->count; i++) {
        const Member *m = &t->members[i];
        if (m->status != STATUS_ACTIVE) continue;
        if (m->role == def->required_role) has_required = true;
        skills[n++] = build_worker_skill(def, t, m);
    }
    if (player_helps) skills[n++] = 1.0f;
    // Los mas habiles primero (insercion: n es pequeno).
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && skills[j] > skills[j - 1]; j--) {
            float tmp = skills[j];
            skills[j] = skills[j - 1];
            skills[j - 1] = tmp;
        }
    plan.workers = n < def->max_workers ? n : def->max_workers;
    if (!has_required) plan.check = BUILD_MISSING_ROLE;
    else if (plan.workers < def->min_workers) plan.check = BUILD_FEW_WORKERS;
    if (plan.check != BUILD_READY) return plan;
    for (int i = 0; i < plan.workers; i++) plan.rate += skills[i];
    return plan;
}

const char *build_check_text(const BuildDef *def, BuildCheck c) {
    static char buf[96];
    switch (c) {
    case BUILD_READY: return "lista";
    case BUILD_FEW_WORKERS: snprintf(buf, sizeof(buf), "faltan manos (mínimo %d)", def->min_workers); return buf;
    case BUILD_MISSING_ROLE: snprintf(buf, sizeof(buf), "falta un %s en la tribu", role_name(def->required_role)); return buf;
    }
    return "?";
}

bool build_advance(BuildProject *p, float rate, float dt) {
    if (p->done || rate <= 0.0f) return false;
    const BuildDef *def = build_def(p->def);
    p->progress += rate * dt / def->work;
    if (p->progress >= 1.0f) {
        p->progress = 1.0f;
        p->done = true;
        return true;
    }
    return false;
}
