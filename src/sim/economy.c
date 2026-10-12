#include "sim/economy.h"
#include "sim/armor.h"
#include "sim/forge.h"
#include "sim/lang.h"

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
        { "utileria.consumible.agua_hervida", 6 }, // el agua: hervida o en bebidas (src/sim/water.h)
        { "utileria.consumible.cerveza", 4 },
        { "utileria.consumible.airag", 4 },
        { "utileria.consumible.vino", 2 },
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
        case ROLE_HERDER: // junta forraje para el ganado y las monturas
            stock_add(s, "utileria.consumible.forraje", 3);
            r.gathered += 3;
            break;
        default: break;
        }
    }
    // Comida: con cocinero, una racion menos cada 3 bocas (no se acumula).
    int need = mouths - (cooks ? mouths / 3 : 0);
    // Primero lo que no aguanta: carne fresca y leche; despues la carne seca.
    static const char *const FOODS[] = { FRESH_MEAT_ID, MILK_ID, FOOD_ID };
    for (int f = 0; f < 3 && r.eaten < need; f++) {
        int have = stock_count(s, FOODS[f]), take = have < need - r.eaten ? have : need - r.eaten;
        stock_take(s, FOODS[f], take);
        r.eaten += take;
    }
    // Despues, el grano (de los campos saqueados o del mercado): un saco da para GRAIN_SACK_RATIONS.
    int rest = need - r.eaten, sacks = stock_count(s, GRAIN_SACK_ID);
    if (rest > 0 && sacks > 0) {
        int use = (rest + GRAIN_SACK_RATIONS - 1) / GRAIN_SACK_RATIONS;
        if (use > sacks) use = sacks;
        stock_take(s, GRAIN_SACK_ID, use);
        r.eaten += use * GRAIN_SACK_RATIONS < rest ? use * GRAIN_SACK_RATIONS : rest;
    }
    r.hungry = need - r.eaten;
    // Lo fresco que sobra: la mitad de la carne se seca, la leche se cuaja en queso; el resto se pierde.
    int fresh = stock_count(s, FRESH_MEAT_ID), milk = stock_count(s, MILK_ID);
    stock_take(s, FRESH_MEAT_ID, fresh);
    stock_take(s, MILK_ID, milk);
    stock_add(s, FOOD_ID, fresh / 2);
    stock_add(s, "utileria.consumible.queso_seco", milk / 2);
    // Los que no comen: reparto simple, el hambre pesa en el animo de todos.
    if (r.hungry > 0) troop_adjust_morale(t, HUNGER_MORALE * (float)r.hungry / (float)(mouths ? mouths : 1));
    return r;
}

// ---------------------------------------------------------------- el trabajo de cada oficio
// Lo que hace cada oficio por dia con lo que hay en el acopio (sin tocar la reserva): una tanda.
typedef struct {
    Role role;
    Ingredient in[3];
    const char *out;
    int amount;
    int keep; // lo que no toca de cada insumo (los troncos de las reparaciones, por ejemplo)
} WorkRule;

static const WorkRule WORK[] = {
    { ROLE_HUNTER, { { "utileria.material.pieles", 2 }, { NULL, 0 } }, "utileria.material.cuero", 1, 2 },
    { ROLE_HUNTER, { { "utileria.material.hueso", 1 }, { NULL, 0 } }, "utileria.material.puntas_hueso", 4, 1 },
    { ROLE_HERDER, { { "utileria.piel.cabra", 2 }, { NULL, 0 } }, "utileria.material.fieltro", 1, 0 },
    { ROLE_BUILDER, { { "utileria.material.troncos", 2 }, { NULL, 0 } }, "utileria.material.madera_labrada", 1, 6 },
    { ROLE_BUILDER, { { "utileria.material.barro", 2 }, { "utileria.consumible.forraje", 1 }, { NULL, 0 } }, "utileria.material.adobe", 2, 2 },
    { ROLE_COOK, { { "utileria.material.grasa", 1 }, { NULL, 0 } }, "utileria.objeto.vela", 3, 0 },
    { ROLE_HEALER, { { "utileria.consumible.hierbas", 2 }, { "utileria.consumible.miel", 1 }, { NULL, 0 } }, "utileria.consumible.unguento", 1, 2 },
    { ROLE_GOLDSMITH, { { "utileria.material.hueso", 1 }, { NULL, 0 } }, "utileria.objeto.peine", 1, 1 },
};

static bool work_ready(const Stockpile *s, const WorkRule *w) {
    for (const Ingredient *m = w->in; m->id; m++)
        if (stock_count(s, m->id) < m->count + w->keep) return false;
    return true;
}

WorkReport economy_daily_work(Stockpile *s, const Troop *t) {
    WorkReport r;
    memset(&r, 0, sizeof(r));
    for (int i = 0; i < t->count; i++) {
        const Member *m = &t->members[i];
        if (m->status != STATUS_ACTIVE) continue;
        for (size_t k = 0; k < sizeof(WORK) / sizeof(WORK[0]); k++) {
            const WorkRule *w = &WORK[k];
            if (w->role != m->role || !work_ready(s, w)) continue;
            for (const Ingredient *in = w->in; in->id; in++) stock_take(s, in->id, in->count);
            stock_add(s, w->out, w->amount);
            r.batches++;
            int j = 0;
            while (j < r.n && strcmp(r.made[j].id, w->out) != 0) j++;
            if (j == r.n && r.n < WORK_OUTPUTS_MAX) r.made[r.n++] = (WorkOutput){ w->out, 0 };
            if (j < r.n) r.made[j].count += w->amount;
            break; // una tanda por persona y por dia
        }
    }
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
        else if (!strcmp(id, "estructura.vivienda.choza_fieltro")) e.shelters += 2;
        else if (!strcmp(id, "estructura.vivienda.casa_adobe") || !strcmp(id, "estructura.vivienda.cabana_troncos")) e.shelters += 3;
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
    [CRAFT_SABLE_BRONZE] = { N_("Sable de bronce"), "arma.corta.sable_bronce", BRONZE, ROLE_SMITH,
                             { { "utileria.material.cobre", 2 }, { "utileria.material.estano", 1 },
                               { "utileria.material.carbon", 2 }, { NULL, 0 } }, 120.0f },
    [CRAFT_SPEAR_BRONZE] = { N_("Lanza de punta de bronce"), "arma.larga.lanza_bronce", BRONZE, ROLE_SMITH,
                             { { "utileria.material.cobre", 1 }, { "utileria.material.estano", 1 },
                               { "utileria.material.troncos", 1 }, { NULL, 0 } }, 90.0f },
    [CRAFT_HELMET_BRONZE] = { N_("Casco de bronce"), "armadura.casco.bronce", BRONZE, ROLE_SMITH,
                              { { "utileria.material.cobre", 3 }, { "utileria.material.estano", 1 },
                                { "utileria.material.carbon", 2 }, { NULL, 0 } }, 150.0f },
    [CRAFT_SABLE_STEEL] = { N_("Sable de acero"), "arma.corta.sable_acero", STEEL, ROLE_SMITH,
                            { { "utileria.material.hierro", 2 }, { "utileria.material.carbon", 3 }, { NULL, 0 } },
                            200.0f },
    [CRAFT_SCALE_ARMOR_STEEL] = { N_("Coraza de escamas de acero"), "armadura.torso.escamas_hierro", STEEL, ROLE_SMITH,
                                  { { "utileria.material.hierro", 4 }, { "utileria.material.carbon", 4 },
                                    { "utileria.material.pieles", 1 }, { NULL, 0 } }, 300.0f },
    [CRAFT_SHIELD_IRON] = { N_("Escudo de láminas de hierro"), "escudo.mano.lamina", STEEL, ROLE_SMITH,
                            { { "utileria.material.hierro", 2 }, { "utileria.material.carbon", 2 },
                              { "utileria.material.troncos", 1 }, { NULL, 0 } }, 150.0f },
    // A mano: con lo que da la estepa (leña, pedernal, plumas y huesos de la caza, hierbas, miel).
    [CRAFT_ARROWS] = { N_("Flechas (5)"), "proyectil.flecha.comun", NULL, ROLE_NONE,
                       { { "utileria.objeto.lena", 1 }, { "utileria.material.plumas", 1 }, { "utileria.material.pedernal", 1 },
                         { NULL, 0 } }, 20.0f, 5 },
    [CRAFT_BOLTS] = { N_("Virotes (4)"), "proyectil.virote.comun", NULL, ROLE_NONE,
                      { { "utileria.objeto.lena", 1 }, { "utileria.material.hueso", 1 }, { NULL, 0 } }, 20.0f, 4 },
    [CRAFT_SLING_STONES] = { N_("Piedras de honda (8)"), "proyectil.piedra.honda", NULL, ROLE_NONE,
                             { { "utileria.material.piedra", 1 }, { NULL, 0 } }, 10.0f, 8 },
    [CRAFT_ROPE] = { N_("Cuerda de tendones"), "utileria.material.cuerda", NULL, ROLE_NONE,
                     { { "utileria.material.tendones", 2 }, { NULL, 0 } }, 25.0f, 1 },
    [CRAFT_OINTMENT] = { N_("Ungüento de hierbas y miel"), "utileria.consumible.unguento", NULL, ROLE_NONE,
                         { { "utileria.consumible.hierbas", 2 }, { "utileria.consumible.miel", 1 }, { NULL, 0 } }, 15.0f, 1 },
    [CRAFT_LEATHER_CUIRASS] = { N_("Coraza laminar de cuero"), "armadura.torso.laminar_cuero", NULL, ROLE_NONE,
                                { { "utileria.material.pieles", 3 }, { "utileria.material.cuerda", 1 }, { NULL, 0 } }, 90.0f, 1 },
    [CRAFT_FELT_BOOTS] = { N_("Botas de fieltro"), "armadura.botas.fieltro", NULL, ROLE_NONE,
                           { { "utileria.material.fieltro", 2 }, { NULL, 0 } }, 45.0f, 1 },
    [CRAFT_BRONZE_INGOT] = { N_("Lingote de bronce"), "utileria.material.bronce", BRONZE, ROLE_SMITH,
                             { { "utileria.material.cobre", 2 }, { "utileria.material.estano", 1 }, { "utileria.material.carbon", 1 }, { NULL, 0 } },
                             60.0f, 1 },
    [CRAFT_FUR_HAT] = { N_("Gorro de piel"), "vestimenta.cabeza.gorro_piel", NULL, ROLE_NONE, { { "utileria.material.pieles", 1 }, { NULL, 0 } }, 20.0f, 1 },
    [CRAFT_FELT_HAT] = { N_("Gorro de fieltro de punta"), "vestimenta.cabeza.gorro_punta", NULL, ROLE_NONE, { { "utileria.material.fieltro", 1 }, { NULL, 0 } }, 20.0f, 1 },
    [CRAFT_SUN_HAT] = { N_("Sombrero de ala ancha"), "vestimenta.cabeza.sombrero", NULL, ROLE_NONE, { { "utileria.piel.cabra", 2 }, { NULL, 0 } }, 25.0f, 1 },
    [CRAFT_WOLF_HAT] = { N_("Gorro de cabeza de lobo"), "vestimenta.cabeza.gorro_lobo", NULL, ROLE_NONE, { { "utileria.piel.lobo", 1 }, { "utileria.material.tendones", 1 }, { NULL, 0 } }, 30.0f, 1 },
    [CRAFT_DESERT_SCARF] = { N_("Pañuelo del desierto"), "vestimenta.cuello.panuelo_desierto", NULL, ROLE_NONE, { { "utileria.material.seda", 1 }, { NULL, 0 } }, 15.0f, 1 },
    [CRAFT_FUR_SCARF] = { N_("Bufanda de piel de zorro"), "vestimenta.cuello.bufanda_piel", NULL, ROLE_NONE, { { "utileria.piel.coyote", 1 }, { NULL, 0 } }, 20.0f, 1 },
    [CRAFT_SILK_TUNIC] = { N_("Túnica de seda blanca"), "vestimenta.torso.tunica_seda", NULL, ROLE_NONE, { { "utileria.material.seda", 3 }, { NULL, 0 } }, 40.0f, 1 },
    [CRAFT_WOOL_TUNIC] = { N_("Túnica de lana"), "vestimenta.torso.tunica_lana", NULL, ROLE_NONE, { { "utileria.piel.cabra", 3 }, { NULL, 0 } }, 40.0f, 1 },
    [CRAFT_WINTER_DEEL] = { N_("Deel de invierno con piel"), "vestimenta.torso.deel_invierno", NULL, ROLE_NONE, { { "utileria.material.pieles", 2 }, { "utileria.piel.cabra", 2 }, { NULL, 0 } }, 60.0f, 1 },
    [CRAFT_WOOL_CLOAK] = { N_("Capa de lana"), "vestimenta.espalda.capa", NULL, ROLE_NONE, { { "utileria.piel.cabra", 2 }, { NULL, 0 } }, 30.0f, 1 },
    [CRAFT_WOLF_CLOAK] = { N_("Capa de piel de lobo"), "vestimenta.espalda.capa_piel", NULL, ROLE_NONE, { { "utileria.piel.lobo", 2 }, { "utileria.material.cuerda", 1 }, { NULL, 0 } }, 45.0f, 1 },
    [CRAFT_BEAR_COAT] = { N_("Abrigo de piel de oso"), "vestimenta.espalda.abrigo_oso", NULL, ROLE_NONE, { { "utileria.piel.oso", 2 }, { "utileria.material.cuerda", 1 }, { NULL, 0 } }, 60.0f, 1 },
    [CRAFT_TIGER_COAT] = { N_("Abrigo de piel de tigre"), "vestimenta.espalda.abrigo_tigre", NULL, ROLE_NONE, { { "utileria.piel.tigre", 2 }, { "utileria.material.cuerda", 1 }, { NULL, 0 } }, 60.0f, 1 },
    [CRAFT_PUMA_CLOAK] = { N_("Capa de piel de puma"), "vestimenta.espalda.capa_puma", NULL, ROLE_NONE, { { "utileria.piel.puma", 2 }, { NULL, 0 } }, 45.0f, 1 },
    [CRAFT_HYENA_CLOAK] = { N_("Capa de piel de hiena"), "vestimenta.espalda.capa_hiena", NULL, ROLE_NONE, { { "utileria.piel.hiena", 2 }, { NULL, 0 } }, 45.0f, 1 },
    [CRAFT_REINDEER_COAT] = { N_("Abrigo de piel de reno"), "vestimenta.espalda.abrigo_reno", NULL, ROLE_NONE, { { "utileria.piel.reno", 3 }, { "utileria.material.cuerda", 1 }, { NULL, 0 } }, 60.0f, 1 },
    [CRAFT_GOAT_COAT] = { N_("Abrigo de lana de cabra"), "vestimenta.espalda.abrigo_cabra", NULL, ROLE_NONE, { { "utileria.piel.cabra", 4 }, { NULL, 0 } }, 50.0f, 1 },
    [CRAFT_WHITE_MANTLE] = { N_("Manto blanco del desierto"), "vestimenta.espalda.manto_blanco", NULL, ROLE_NONE, { { "utileria.material.seda", 2 }, { NULL, 0 } }, 30.0f, 1 },
    [CRAFT_FUR_BOOTS] = { N_("Botas de piel de reno"), "vestimenta.pies.botas_piel", NULL, ROLE_NONE, { { "utileria.piel.reno", 1 }, { "utileria.material.tendones", 1 }, { NULL, 0 } }, 35.0f, 1 },
    [CRAFT_WATERED_WINE] = { N_("Agua con vino (3)"), "utileria.consumible.agua_vino", NULL, ROLE_NONE,
                             { { "utileria.consumible.agua", 2 }, { "utileria.consumible.vino", 1 }, { NULL, 0 } }, 10.0f, 3 },
    [CRAFT_AIRAG] = { N_("Airag (leche de yegua fermentada)"), "utileria.consumible.airag", NULL, ROLE_NONE,
                      { { "utileria.consumible.leche", 2 }, { NULL, 0 } }, 30.0f, 1 },
    [CRAFT_BEER] = { N_("Cerveza de pan (2)"), "utileria.consumible.cerveza", NULL, ROLE_NONE,
                     { { "utileria.consumible.pan", 1 }, { "utileria.consumible.agua", 1 }, { NULL, 0 } }, 40.0f, 2 },
    [CRAFT_SANDALS] = { N_("Sandalias de cuero"), "vestimenta.pies.sandalias", NULL, ROLE_NONE, { { "utileria.material.pieles", 1 }, { NULL, 0 } }, 15.0f, 1 },
    // Talabarteria: las pieles se curten y se trabajan.
    [CRAFT_LEATHER] = { N_("Cuero (curtir pieles)"), "utileria.material.cuero", NULL, ROLE_NONE, { { "utileria.material.pieles", 2 }, { NULL, 0 } }, 40.0f, 1 },
    [CRAFT_SADDLE] = { N_("Silla de montar"), "accesorio.arreo.silla_montar", NULL, ROLE_NONE,
                       { { "utileria.material.cuero", 3 }, { "utileria.material.madera_labrada", 1 }, { NULL, 0 } }, 120.0f, 1 },
    [CRAFT_BRIDLE] = { N_("Brida y riendas"), "accesorio.arreo.brida", NULL, ROLE_NONE,
                       { { "utileria.material.cuero", 1 }, { "utileria.material.cuerda", 1 }, { NULL, 0 } }, 50.0f, 1 },
    [CRAFT_SADDLEBAGS] = { N_("Alforjas"), "accesorio.arreo.alforjas", NULL, ROLE_NONE, { { "utileria.material.cuero", 2 }, { NULL, 0 } }, 60.0f, 1 },
    [CRAFT_QUIVER] = { N_("Aljaba de cuero"), "utileria.objeto.aljaba", NULL, ROLE_NONE, { { "utileria.material.cuero", 1 }, { NULL, 0 } }, 35.0f, 1 },
    [CRAFT_LEATHER_HELMET] = { N_("Casco laminar de cuero"), "armadura.casco.laminar_cuero", NULL, ROLE_NONE,
                               { { "utileria.material.cuero", 2 }, { "utileria.material.cuerda", 1 }, { NULL, 0 } }, 60.0f, 1 },
    [CRAFT_LEATHER_BRACERS] = { N_("Brazales laminares de cuero"), "armadura.brazales.laminar_cuero", NULL, ROLE_NONE,
                                { { "utileria.material.cuero", 1 }, { NULL, 0 } }, 40.0f, 1 },
    [CRAFT_LEATHER_GREAVES] = { N_("Grebas laminares de cuero"), "armadura.grebas.laminar_cuero", NULL, ROLE_NONE,
                                { { "utileria.material.cuero", 1 }, { "utileria.material.cuerda", 1 }, { NULL, 0 } }, 45.0f, 1 },
    // Hueso y asta.
    [CRAFT_BONE_POINTS] = { N_("Puntas de hueso (4)"), "utileria.material.puntas_hueso", NULL, ROLE_NONE, { { "utileria.material.hueso", 1 }, { NULL, 0 } }, 15.0f, 4 },
    [CRAFT_BONE_ARROWS] = { N_("Flechas con punta de hueso (5)"), "proyectil.flecha.comun", NULL, ROLE_NONE,
                            { { "utileria.objeto.lena", 1 }, { "utileria.material.plumas", 1 }, { "utileria.material.puntas_hueso", 2 }, { NULL, 0 } }, 20.0f, 5 },
    [CRAFT_COMB] = { N_("Peine de hueso"), "utileria.objeto.peine", NULL, ROLE_NONE, { { "utileria.material.hueso", 1 }, { NULL, 0 } }, 25.0f, 1 },
    [CRAFT_BOW_PARTS] = { N_("Piezas de arco (asta y tendón)"), "utileria.material.piezas_arco", NULL, ROLE_NONE,
                          { { "utileria.material.hueso", 2 }, { "utileria.material.tendones", 1 }, { NULL, 0 } }, 40.0f, 1 },
    // La tercera etapa del arco reflejo: el arco ya seco, con la cola de hueso y la cuerda de tendon.
    [CRAFT_COMPOSITE_BOW] = { N_("Arco compuesto reflejo (cola de hueso)"), "arma.distancia.arco_compuesto", NULL, ROLE_NONE,
                              { { "utileria.material.arco_seco", 1 }, { "utileria.material.cola_hueso", 1 }, { "utileria.material.tendones", 1 }, { NULL, 0 } },
                              60.0f, 1 },
    // Grasa: velas y antorchas.
    [CRAFT_CANDLES] = { N_("Velas de sebo (3)"), "utileria.objeto.vela", NULL, ROLE_NONE, { { "utileria.material.grasa", 1 }, { NULL, 0 } }, 20.0f, 3 },
    [CRAFT_TORCH] = { N_("Antorchas (2)"), "utileria.objeto.antorcha", NULL, ROLE_NONE,
                      { { "utileria.objeto.lena", 1 }, { "utileria.material.grasa", 1 }, { NULL, 0 } }, 15.0f, 2 },
    // Leche, lana, madera y barro.
    [CRAFT_CHEESE] = { N_("Queso seco"), "utileria.consumible.queso_seco", NULL, ROLE_NONE, { { "utileria.consumible.leche", 2 }, { NULL, 0 } }, 20.0f, 1 },
    [CRAFT_FELT] = { N_("Fieltro"), "utileria.material.fieltro", NULL, ROLE_NONE, { { "utileria.piel.cabra", 2 }, { NULL, 0 } }, 40.0f, 1 },
    [CRAFT_WORKED_WOOD] = { N_("Madera labrada"), "utileria.material.madera_labrada", NULL, ROLE_NONE, { { "utileria.material.troncos", 2 }, { NULL, 0 } }, 30.0f, 1 },
    [CRAFT_ADOBE] = { N_("Ladrillos de adobe (2)"), "utileria.material.adobe", NULL, ROLE_NONE,
                      { { "utileria.material.barro", 2 }, { "utileria.consumible.forraje", 1 }, { NULL, 0 } }, 30.0f, 2 },
    // Las armas de la estepa (fase 7).
    [CRAFT_SIMPLE_BOW] = { N_("Arco simple"), "arma.distancia.arco_simple", NULL, ROLE_NONE,
                           { { "utileria.material.madera_labrada", 1 }, { "utileria.material.tendones", 1 }, { NULL, 0 } }, 60.0f, 1 },
    [CRAFT_BONE_GLUE] = { N_("Cola de hueso (2)"), "utileria.material.cola_hueso", NULL, ROLE_NONE,
                          { { "utileria.material.hueso", 2 }, { "utileria.objeto.lena", 1 }, { NULL, 0 } }, 30.0f, 2 },
    [CRAFT_BOW_CORE] = { N_("Alma de arco (madera)"), "utileria.material.alma_arco", NULL, ROLE_NONE,
                         { { "utileria.material.madera_labrada", 1 }, { NULL, 0 } }, 50.0f, 1 },
    [CRAFT_BOW_GLUED] = { N_("Arco encolado (asta y tendón)"), "utileria.material.arco_encolado", NULL, ROLE_NONE,
                          { { "utileria.material.alma_arco", 1 }, { "utileria.material.piezas_arco", 2 }, { "utileria.material.tendones", 2 },
                            { "utileria.material.cola_hueso", 1 } },
                          90.0f, 1, true },
    [CRAFT_AKINAKES] = { N_("Akinakes de bronce"), "arma.corta.akinakes", BRONZE, ROLE_SMITH,
                         { { "utileria.material.cobre", 1 }, { "utileria.material.estano", 1 }, { "utileria.material.carbon", 1 }, { NULL, 0 } }, 90.0f },
    [CRAFT_SAGARIS] = { N_("Sagaris de pico"), "arma.corta.sagaris", BRONZE, ROLE_SMITH,
                        { { "utileria.material.cobre", 2 }, { "utileria.material.estano", 1 }, { "utileria.material.troncos", 1 }, { NULL, 0 } }, 100.0f },
    [CRAFT_SAGARIS_HAMMER] = { N_("Sagaris de martillo"), "arma.corta.sagaris_martillo", BRONZE, ROLE_SMITH,
                               { { "utileria.material.cobre", 2 }, { "utileria.material.estano", 1 }, { "utileria.material.troncos", 1 }, { NULL, 0 } },
                               100.0f },
    [CRAFT_MACE] = { N_("Maza de bronce"), "arma.corta.maza", BRONZE, ROLE_SMITH,
                     { { "utileria.material.cobre", 2 }, { "utileria.material.estano", 1 }, { "utileria.material.troncos", 1 }, { NULL, 0 } }, 90.0f },
    [CRAFT_RING_DAGGER] = { N_("Daga de anillo"), "arma.corta.daga_anillo", STEEL, ROLE_SMITH,
                            { { "utileria.material.hierro", 1 }, { "utileria.material.carbon", 2 }, { NULL, 0 } }, 90.0f },
    [CRAFT_HEAVY_LANCE] = { N_("Lanza pesada"), "arma.larga.lanza_pesada", BRONZE, ROLE_SMITH,
                            { { "utileria.material.troncos", 2 }, { "utileria.material.cobre", 1 }, { "utileria.material.estano", 1 }, { NULL, 0 } },
                            120.0f },
    // La forja (fase 7b). El mineral se funde en el horno de acero.
    [CRAFT_SMELT_IRON] = { N_("Hierro (fundir el mineral)"), "utileria.material.hierro", STEEL, ROLE_SMITH,
                           { { IRON_ORE_ID, 3 }, { "utileria.material.carbon", 2 }, { NULL, 0 } }, 120.0f },
    [CRAFT_MOBILE_FORGE] = { N_("Forja móvil (fuelle y hogar)"), FORGE_MOBILE_ID, STEEL, ROLE_SMITH,
                             { { "utileria.material.cuero", 2 }, { "utileria.material.hierro", 2 }, { "utileria.material.piedra", 2 }, { NULL, 0 } },
                             150.0f },
    // En la forja movil (o en un horno o el taller), con tus manos.
    [CRAFT_IRON_POINTS] = { N_("Puntas de flecha de hierro (6)"), "utileria.material.puntas_hierro", FORGE_MOBILE_ID, ROLE_NONE,
                            { { "utileria.material.hierro", 1 }, { "utileria.material.carbon", 1 }, { NULL, 0 } }, 40.0f, 6 },
    [CRAFT_IRON_ARROWS] = { N_("Flechas de punta de hierro (8)"), "proyectil.flecha.comun", NULL, ROLE_NONE,
                            { { "utileria.objeto.lena", 1 }, { "utileria.material.plumas", 1 }, { "utileria.material.puntas_hierro", 2 }, { NULL, 0 } },
                            25.0f, 8 },
    [CRAFT_KNIFE] = { N_("Cuchillo de hierro"), "arma.corta.cuchillo", FORGE_MOBILE_ID, ROLE_NONE,
                      { { "utileria.material.hierro", 1 }, { "utileria.material.hueso", 1 }, { NULL, 0 } }, 45.0f, 1 },
    [CRAFT_SABLE_FROM_BLADE] = { N_("Sable de acero (con una hoja)"), "arma.corta.sable_acero", FORGE_MOBILE_ID, ROLE_NONE,
                                 { { "utileria.material.hoja_acero", 1 }, { "utileria.material.cuero", 1 }, { "utileria.material.madera_labrada", 1 },
                                   { NULL, 0 } },
                                 60.0f, 1 },
    // En el taller del herrero, de un campamento grande.
    [CRAFT_SABLE_WOOTZ] = { N_("Sable de acero wootz"), "arma.corta.sable_wootz", WORKSHOP_ID, ROLE_SMITH,
                            { { "utileria.material.acero_wootz", 1 }, { "utileria.material.carbon", 2 }, { "utileria.material.cuero", 1 }, { NULL, 0 } },
                            240.0f },
    [CRAFT_SPEAR_STEEL] = { N_("Lanza de punta de acero"), "arma.larga.lanza_acero", WORKSHOP_ID, ROLE_SMITH,
                            { { "utileria.material.hierro", 1 }, { "utileria.material.carbon", 2 }, { "utileria.material.troncos", 1 }, { NULL, 0 } },
                            150.0f },
    // El temple: sale templada o, si sale mal, fragil (fails).
    [CRAFT_TEMPER_SABLE] = { N_("Templar un sable de acero"), "arma.corta.sable_acero_templado", WORKSHOP_ID, ROLE_SMITH,
                             { { "arma.corta.sable_acero", 1 }, { "utileria.material.carbon", 1 }, { NULL, 0 } }, 120.0f, 1, false,
                             "arma.corta.sable_acero_fragil" },
    [CRAFT_TEMPER_WOOTZ] = { N_("Templar un sable wootz"), "arma.corta.sable_wootz_templado", WORKSHOP_ID, ROLE_SMITH,
                             { { "arma.corta.sable_wootz", 1 }, { "utileria.material.carbon", 1 }, { NULL, 0 } }, 150.0f, 1, false,
                             "arma.corta.sable_wootz_fragil" },
    [CRAFT_TEMPER_DAGGER] = { N_("Templar una daga de anillo"), "arma.corta.daga_anillo_templada", WORKSHOP_ID, ROLE_SMITH,
                              { { "arma.corta.daga_anillo", 1 }, { "utileria.material.carbon", 1 }, { NULL, 0 } }, 90.0f, 1, false,
                              "arma.corta.daga_anillo_fragil" },
};

bool craft_by_hand(const CraftDef *c) { return c->role == ROLE_NONE; }

int stock_dry_day(Stockpile *s) {
    // De la ultima etapa a la primera: cada pieza avanza una sola por dia.
    static const char *const STAGES[][2] = {
        { "utileria.material.arco_oreado", "utileria.material.arco_seco" },
        { "utileria.material.arco_encolado", "utileria.material.arco_oreado" },
    };
    int moved = 0;
    for (size_t i = 0; i < sizeof(STAGES) / sizeof(STAGES[0]); i++) {
        int n = stock_count(s, STAGES[i][0]);
        if (n <= 0 || !stock_take(s, STAGES[i][0], n)) continue;
        stock_add(s, STAGES[i][1], n);
        moved += n;
    }
    return moved;
}

// Mismo orden que ArmorMaterial: fieltro, cuero, bronce, hierro, acero, oro.
static const RepairDef REPAIRS[] = {
    { { { "utileria.material.pieles", 1 }, { NULL, 0 } }, NULL, ROLE_NONE, 0.6f },
    { { { "utileria.material.pieles", 1 }, { "utileria.material.cuerda", 1 }, { NULL, 0 } }, NULL, ROLE_NONE, 0.6f },
    { { { "utileria.material.cobre", 1 }, { "utileria.material.carbon", 1 }, { NULL, 0 } }, BRONZE, ROLE_SMITH, 0.8f },
    { { { "utileria.material.hierro", 1 }, { "utileria.material.carbon", 1 }, { NULL, 0 } }, STEEL, ROLE_SMITH, 0.8f },
    { { { "utileria.material.hierro", 1 }, { "utileria.material.carbon", 2 }, { NULL, 0 } }, STEEL, ROLE_SMITH, 0.8f },
    { { { "utileria.material.carbon", 1 }, { NULL, 0 } }, BRONZE, ROLE_SMITH, 0.8f },
};

static const RepairDef GREASE = { { { "utileria.material.grasa", 1 }, { NULL, 0 } }, NULL, ROLE_NONE, 0.3f };

const RepairDef *repair_grease(int material) { return material == MAT_FELT || material == MAT_LEATHER ? &GREASE : NULL; }

const RepairDef *repair_def(int material) {
    return &REPAIRS[material >= 0 && material < (int)(sizeof(REPAIRS) / sizeof(REPAIRS[0])) ? material : 0];
}

const CraftDef *craft_def(CraftId c) { return c >= 0 && c < CRAFT_COUNT ? &CRAFTS[c] : NULL; }

float craft_seconds(const CraftDef *c, const Troop *t) {
    if (c->role == ROLE_NONE) return c->work; // a mano: lo hace el jugador
    int artisans = 0;
    for (int i = 0; i < t->count; i++)
        if (t->members[i].status == STATUS_ACTIVE && t->members[i].role == c->role) artisans++;
    return artisans ? c->work / (float)artisans : -1.0f;
}
