#include "sim/economy.h"

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------- acopio
void stock_init(Stockpile *s) { memset(s, 0, sizeof(*s)); }

static StockEntry *find(Stockpile *s, const char *id) {
    for (int i = 0; i < s->n; i++)
        if (!strcmp(s->e[i].id, id)) return &s->e[i];
    return NULL;
}

int stock_count(const Stockpile *s, const char *id) {
    for (int i = 0; i < s->n; i++)
        if (!strcmp(s->e[i].id, id)) return s->e[i].count;
    return 0;
}

bool stock_add(Stockpile *s, const char *id, int count) {
    if (!id || count <= 0) return false;
    StockEntry *e = find(s, id);
    if (!e) {
        if (s->n >= STOCK_MAX) return false;
        e = &s->e[s->n++];
        snprintf(e->id, sizeof(e->id), "%s", id);
        e->count = 0;
    }
    e->count += count;
    return true;
}

bool stock_take(Stockpile *s, const char *id, int count) {
    StockEntry *e = find(s, id);
    if (!e || e->count < count) return false;
    e->count -= count;
    return true;
}

const Ingredient *stock_first_missing(const Stockpile *s, const Ingredient *mats) {
    for (const Ingredient *m = mats; m && m->id; m++)
        if (stock_count(s, m->id) < m->count) return m;
    return NULL;
}

bool stock_has_all(const Stockpile *s, const Ingredient *mats) { return stock_first_missing(s, mats) == NULL; }

bool stock_take_all(Stockpile *s, const Ingredient *mats) {
    if (!stock_has_all(s, mats)) return false;
    for (const Ingredient *m = mats; m && m->id; m++) stock_take(s, m->id, m->count);
    return true;
}

void stock_seed_camp(Stockpile *s) {
    static const Ingredient START[] = {
        { FOOD_ID, 12 },
        { "utileria.objeto.lena", 10 },
        { "utileria.consumible.hierbas", 4 }, // para vendar heridas
        { "proyectil.flecha.comun", 24 },     // municion de arco, ballesta y honda
        { "proyectil.virote.comun", 10 },
        { "proyectil.piedra.honda", 15 },
        { "utileria.material.troncos", 16 },
        { "utileria.material.piedra", 10 },
        { "utileria.material.barro", 8 },
        { "utileria.material.pieles", 6 },
        { "utileria.material.cuerda", 6 },
        { "utileria.material.carbon", 4 },
        { "utileria.material.cobre", 3 },
        { "utileria.material.estano", 2 },
        { "utileria.material.hierro", 2 },
        { NULL, 0 },
    };
    for (const Ingredient *m = START; m->id; m++) stock_add(s, m->id, m->count);
}

// ---------------------------------------------------------------- dia a dia
#define HUNGER_MORALE (-6.0f)

UpkeepReport economy_daily_upkeep(Stockpile *s, Troop *t) {
    UpkeepReport r = { 0, 0, 0 };
    int mouths = 0, cooks = 0;
    for (int i = 0; i < t->count; i++) {
        Member *m = &t->members[i];
        if (m->status != STATUS_ACTIVE) continue;
        mouths++;
        // Lo que cada funcion trae al acopio.
        switch (m->role) {
        case ROLE_NONE:
            stock_add(s, "utileria.objeto.lena", 1);
            stock_add(s, "utileria.material.troncos", 1);
            r.gathered += 2;
            break;
        case ROLE_HUNTER:
            stock_add(s, FOOD_ID, 3);
            stock_add(s, "utileria.material.pieles", 1);
            r.gathered += 4;
            break;
        case ROLE_SCOUT:
            stock_add(s, "utileria.material.piedra", 1);
            stock_add(s, "utileria.material.barro", 1);
            r.gathered += 2;
            break;
        case ROLE_SMITH:
            stock_add(s, "utileria.material.carbon", 1);
            r.gathered += 1;
            break;
        case ROLE_BUILDER:
            stock_add(s, "utileria.material.cuerda", 1);
            r.gathered += 1;
            break;
        case ROLE_COOK: cooks++; break;
        default: break;
        }
    }
    // Comida: con cocinero, una racion menos cada 3 bocas (no se acumula).
    int need = mouths - (cooks ? mouths / 3 : 0);
    int have = stock_count(s, FOOD_ID);
    r.eaten = have < need ? have : need;
    stock_take(s, FOOD_ID, r.eaten);
    r.hungry = need - r.eaten;
    // Los que no comen: reparto simple, el hambre pesa en el animo de todos.
    if (r.hungry > 0) troop_adjust_morale(t, HUNGER_MORALE * (float)r.hungry / (float)(mouths ? mouths : 1));
    return r;
}

// ---------------------------------------------------------------- efectos de las construcciones
CampEffects camp_effects(const char *const *ids, int n) {
    CampEffects e = { 0.0f, 1.0f, 0.0f, 0 };
    bool bonfire = false, oven = false, totem = false;
    int fires = 0;
    for (int i = 0; i < n; i++) {
        const char *id = ids[i];
        if (!strcmp(id, "estructura.campamento.hoguera")) bonfire = true;
        else if (!strcmp(id, "estructura.campamento.fogata")) fires++;
        else if (!strcmp(id, "estructura.campamento.horno_cocina")) oven = true;
        else if (!strcmp(id, "totem.proteccion.guardian")) totem = true;
        else if (!strcmp(id, "estructura.campamento.atalaya")) e.reveal_radius = 80.0f;
        if (!strcmp(id, "estructura.campamento.refugio") || !strcmp(id, "estructura.vivienda.tienda_ligera")) e.shelters += 1;
        else if (!strncmp(id, "estructura.vivienda.yurta", 25)) e.shelters += 4;
    }
    if (bonfire) e.morale_per_day += 3.0f;        // calor y reunion
    e.morale_per_day += (float)(fires > 2 ? 2 : fires); // fogatas: hasta +2
    if (oven) e.morale_per_day += 2.0f;            // comida caliente
    if (totem) e.rebellion_scale = 0.5f;           // los espiritus protegen al clan
    return e;
}

// ---------------------------------------------------------------- forja
#define BRONZE "estructura.campamento.horno_bronce"
#define STEEL "estructura.campamento.horno_acero"

static const CraftDef CRAFTS[CRAFT_COUNT] = {
    [CRAFT_SABLE_BRONZE] = { "Sable de bronce", "arma.corta.sable_bronce", BRONZE, ROLE_SMITH,
                             { { "utileria.material.cobre", 2 }, { "utileria.material.estano", 1 },
                               { "utileria.material.carbon", 2 }, { NULL, 0 } }, 120.0f },
    [CRAFT_SPEAR_BRONZE] = { "Lanza de punta de bronce", "arma.larga.lanza_bronce", BRONZE, ROLE_SMITH,
                             { { "utileria.material.cobre", 1 }, { "utileria.material.estano", 1 },
                               { "utileria.material.troncos", 1 }, { NULL, 0 } }, 90.0f },
    [CRAFT_HELMET_BRONZE] = { "Casco de bronce", "armadura.casco.bronce", BRONZE, ROLE_SMITH,
                              { { "utileria.material.cobre", 3 }, { "utileria.material.estano", 1 },
                                { "utileria.material.carbon", 2 }, { NULL, 0 } }, 150.0f },
    [CRAFT_SABLE_STEEL] = { "Sable de acero", "arma.corta.sable_acero", STEEL, ROLE_SMITH,
                            { { "utileria.material.hierro", 2 }, { "utileria.material.carbon", 3 }, { NULL, 0 } },
                            200.0f },
    [CRAFT_SCALE_ARMOR_STEEL] = { "Coraza de escamas de acero", "armadura.torso.escamas_hierro", STEEL, ROLE_SMITH,
                                  { { "utileria.material.hierro", 4 }, { "utileria.material.carbon", 4 },
                                    { "utileria.material.pieles", 1 }, { NULL, 0 } }, 300.0f },
    [CRAFT_SHIELD_IRON] = { "Escudo de láminas de hierro", "escudo.mano.lamina", STEEL, ROLE_SMITH,
                            { { "utileria.material.hierro", 2 }, { "utileria.material.carbon", 2 },
                              { "utileria.material.troncos", 1 }, { NULL, 0 } }, 150.0f },
};

const CraftDef *craft_def(CraftId c) { return c >= 0 && c < CRAFT_COUNT ? &CRAFTS[c] : NULL; }

float craft_seconds(const CraftDef *c, const Troop *t) {
    int artisans = 0;
    for (int i = 0; i < t->count; i++)
        if (t->members[i].status == STATUS_ACTIVE && t->members[i].role == c->role) artisans++;
    return artisans ? c->work / (float)artisans : -1.0f;
}
