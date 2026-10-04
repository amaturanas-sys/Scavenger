#include "talents.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "lang.h"

// ---------------------------------------------------------------- nivel
void progress_init(Progress *p) {
    p->level = 1;
    p->xp = 0.0f;
}

float xp_to_next(int level) { return 100.0f * powf((float)level, 1.4f); }

float xp_reward(XpSource s) {
    static const float R[XP_SOURCE_COUNT] = { [XP_KILL] = 30.0f, [XP_HUNT] = 12.0f, [XP_CRAFT] = 8.0f,
                                              [XP_BUILD] = 35.0f, [XP_DISCOVER] = 20.0f, [XP_TAME] = 25.0f };
    return s >= 0 && s < XP_SOURCE_COUNT ? R[s] : 0.0f;
}

int progress_add(Progress *p, float xp) {
    int up = 0;
    if (xp <= 0.0f || p->level >= LEVEL_MAX) return 0;
    p->xp += xp;
    while (p->level < LEVEL_MAX && p->xp >= xp_to_next(p->level)) {
        p->xp -= xp_to_next(p->level);
        p->level++;
        up++;
    }
    if (p->level >= LEVEL_MAX) p->xp = 0.0f;
    return up;
}

// ---------------------------------------------------------------- habilidades
static const AbilityDef ABILITIES[ABIL_COUNT] = {
    [ABIL_NONE] = { N_("Ninguna"), "", 0, 0, { 0 }, 0, false, 0, false },
    [ABIL_HOWL] = { N_("Aullido"), N_("La furia de la manada: más daño cuerpo a cuerpo y agarre."), 20.0f, 120.0f,
                   { [STAT_MELEE] = 0.35f, [STAT_GRAPPLE_POWER] = 0.25f }, 0, false, 0, false },
    [ABIL_GALLOP] = { N_("Galope"), N_("Corres como el ciervo, a pie o a caballo."), 12.0f, 90.0f,
                     { [STAT_SPEED] = 0.40f, [STAT_RIDING] = 0.30f }, 0, false, 0, false },
    [ABIL_EAGLE] = { N_("Vuelo del grifo"), N_("Ves de lejos y tu tiro no falla."), 15.0f, 100.0f,
                    { [STAT_ARCHERY] = 0.60f, [STAT_PERCEPTION] = 0.50f }, 0, false, 0, false },
    [ABIL_WAR_CRY] = { N_("Grito del clan"), N_("Tu escolta y tú peleáis con más fuerza."), 20.0f, 150.0f,
                      { [STAT_MELEE] = 0.20f, [STAT_CHARISMA] = 0.30f }, 0, false, 0, true },
    [ABIL_TIDE] = { N_("Marea"), N_("Recuperas de golpe parte de la vida."), 0.0f, 180.0f, { 0 }, 0.35f, false, 0, false },
    [ABIL_SHADOW] = { N_("Sombra"), N_("Casi nadie te ve un rato."), 10.0f, 120.0f, { [STAT_STEALTH] = 1.0f }, 0, false, 0, false },
    [ABIL_WARMTH] = { N_("Calor del ámbar"), N_("Devuelve el calor del cuerpo, aun en la nevada."), 0.0f, 150.0f, { 0 }, 0, false,
                     0.5f, false },
    [ABIL_STAUNCH] = { N_("Sangre de granate"), N_("Las heridas dejan de sangrar."), 0.0f, 160.0f, { 0 }, 0.05f, true, 0, false },
};

const AbilityDef *ability_def(AbilityId a) { return &ABILITIES[a > 0 && a < ABIL_COUNT ? a : 0]; }

// ---------------------------------------------------------------- arbol de tatuajes
// Por motivo: grado 1 (nivel 1), grado 2 (nivel 3), y el tercero (nivel 6) en dos ramas.
static const TattooNode NODES[TATTOO_NODES] = {
    // Lobo: la manada y la sombra.
    { N_("Huella del lobo"), N_("Pisas sin ruido."), MOTIF_WOLF, 1, 0, 1, { STAT_STEALTH, STAT_COUNT }, { 0.15f, 0 }, ABIL_NONE },
    { N_("Paso de la manada"), N_("Rondas como el lobo: sigilo y paso."), MOTIF_WOLF, 2, 0, 3, { STAT_STEALTH, STAT_SPEED }, { 0.15f, 0.05f }, ABIL_NONE },
    { N_("Aullido"), N_("Habilidad activa: la furia de la manada."), MOTIF_WOLF, 3, 1, 6, { STAT_MELEE, STAT_COUNT }, { 0.05f, 0 }, ABIL_HOWL },
    { N_("Sombra de la estepa"), N_("Te pierdes en la hierba."), MOTIF_WOLF, 3, 2, 6, { STAT_STEALTH, STAT_COUNT }, { 0.30f, 0 }, ABIL_NONE },
    // Ciervo: aguante y carrera.
    { N_("Astas del ciervo"), N_("Te repones antes."), MOTIF_DEER, 1, 0, 1, { STAT_STAMINA_REGEN, STAT_COUNT }, { 0.15f, 0 }, ABIL_NONE },
    { N_("Carrera larga"), N_("Corres más y te cansas menos."), MOTIF_DEER, 2, 0, 3, { STAT_SPEED, STAT_STAMINA_REGEN }, { 0.08f, 0.10f }, ABIL_NONE },
    { N_("Galope del ciervo"), N_("Habilidad activa: una carrera que nadie alcanza."), MOTIF_DEER, 3, 1, 6, { STAT_SPEED, STAT_COUNT }, { 0.04f, 0 }, ABIL_GALLOP },
    { N_("Sangre de la tierra"), N_("Más vida y más aguante."), MOTIF_DEER, 3, 2, 6, { STAT_HEALTH, STAT_STAMINA_REGEN }, { 0.20f, 0.10f }, ABIL_NONE },
    // Grifo: vista y punteria.
    { N_("Ojo del grifo"), N_("Ves más lejos."), MOTIF_GRIFFIN, 1, 0, 1, { STAT_PERCEPTION, STAT_COUNT }, { 0.15f, 0 }, ABIL_NONE },
    { N_("Garra del grifo"), N_("Tu tiro se afina."), MOTIF_GRIFFIN, 2, 0, 3, { STAT_ARCHERY, STAT_PERCEPTION }, { 0.15f, 0.05f }, ABIL_NONE },
    { N_("Vuelo del grifo"), N_("Habilidad activa: vista de águila y tiro certero."), MOTIF_GRIFFIN, 3, 1, 6, { STAT_ARCHERY, STAT_COUNT }, { 0.05f, 0 }, ABIL_EAGLE },
    { N_("Pluma certera"), N_("Cada flecha va donde miras."), MOTIF_GRIFFIN, 3, 2, 6, { STAT_ARCHERY, STAT_COUNT }, { 0.25f, 0 }, ABIL_NONE },
    // Tamga: el sello del clan, mando y oficio.
    { N_("Sello del clan"), N_("La tribu te escucha."), MOTIF_TAMGA, 1, 0, 1, { STAT_CHARISMA, STAT_COUNT }, { 0.15f, 0 }, ABIL_NONE },
    { N_("Manos del clan"), N_("Fabricas y reparas más rápido."), MOTIF_TAMGA, 2, 0, 3, { STAT_CRAFT, STAT_CHARISMA }, { 0.15f, 0.05f }, ABIL_NONE },
    { N_("Grito del clan"), N_("Habilidad activa: la escolta pelea a tu lado con más fuerza."), MOTIF_TAMGA, 3, 1, 6, { STAT_CHARISMA, STAT_COUNT }, { 0.05f, 0 }, ABIL_WAR_CRY },
    { N_("Señor de la estepa"), N_("Mandas y cargas como un jefe."), MOTIF_TAMGA, 3, 2, 6, { STAT_CHARISMA, STAT_CARRY }, { 0.25f, 0.10f }, ABIL_NONE },
    // Olas: el rio y la vida.
    { N_("Agua del río"), N_("Más vida."), MOTIF_WAVES, 1, 0, 1, { STAT_HEALTH, STAT_COUNT }, { 0.10f, 0 }, ABIL_NONE },
    { N_("Corriente"), N_("Cargas más sin frenarte."), MOTIF_WAVES, 2, 0, 3, { STAT_CARRY, STAT_STAMINA_REGEN }, { 0.15f, 0.05f }, ABIL_NONE },
    { N_("Marea que sana"), N_("Habilidad activa: recuperas de golpe parte de la vida."), MOTIF_WAVES, 3, 1, 6, { STAT_HEALTH, STAT_COUNT }, { 0.05f, 0 }, ABIL_TIDE },
    { N_("Piel de río"), N_("Golpeas y aguantas como la crecida."), MOTIF_WAVES, 3, 2, 6, { STAT_MELEE, STAT_HEALTH }, { 0.15f, 0.10f }, ABIL_NONE },
};

const TattooNode *tattoo_node(int i) { return i >= 0 && i < TATTOO_NODES ? &NODES[i] : NULL; }

const char *zone_name(TattooZone z) {
    static const char *N[TZ_COUNT] = { N_("cabeza"), N_("cuello"), N_("pecho"),  N_("espalda"),
                                       N_("brazo izquierdo"), N_("brazo derecho"), N_("manos"), N_("piernas") };
    return z >= 0 && z < TZ_COUNT ? T(N[z]) : "?";
}

const char *motif_name(Motif m) {
    static const char *N[MOTIF_COUNT] = { N_("Lobo"), N_("Ciervo"), N_("Grifo"), N_("Tamga"), N_("Olas") };
    return m >= 0 && m < MOTIF_COUNT ? T(N[m]) : "?";
}

const char *motif_item(Motif m) {
    static const char *I[MOTIF_COUNT] = { "accesorio.tatuaje.lobo", "accesorio.tatuaje.ciervo", "accesorio.tatuaje.grifo",
                                          "accesorio.tatuaje.tamga", "accesorio.tatuaje.olas" };
    return m >= 0 && m < MOTIF_COUNT ? I[m] : "";
}

const char *tattoo_check_text(TattooCheck c) {
    switch (c) {
    case TAT_OK: return T("Se puede.");
    case TAT_ZONE_TAKEN: return T("Esa zona ya está tatuada (para siempre).");
    case TAT_HAVE_IT: return T("Ya llevas ese tatuaje.");
    case TAT_LEVEL: return T("Te falta nivel.");
    case TAT_NEEDS_PREV: return T("Primero el grado anterior de ese motivo.");
    default: return T("Ya elegiste la otra rama de ese motivo.");
    }
}

void tattoo_body_init(TattooBody *b) { memset(b->node, TATTOO_NONE, sizeof(b->node)); }

bool tattoo_has(const TattooBody *b, int node) {
    for (int z = 0; z < TZ_COUNT; z++)
        if (b->node[z] == node) return true;
    return false;
}

int tattoo_count(const TattooBody *b) {
    int n = 0;
    for (int z = 0; z < TZ_COUNT; z++) n += b->node[z] != TATTOO_NONE;
    return n;
}

TattooCheck tattoo_can(const TattooBody *b, int node, TattooZone zone, int level) {
    const TattooNode *n = tattoo_node(node);
    if (!n || zone < 0 || zone >= TZ_COUNT) return TAT_ZONE_TAKEN;
    if (b->node[zone] != TATTOO_NONE) return TAT_ZONE_TAKEN;
    if (tattoo_has(b, node)) return TAT_HAVE_IT;
    for (int i = 0; i < TATTOO_NODES; i++) { // la otra rama del mismo motivo cierra esta
        const TattooNode *o = &NODES[i];
        if (o->motif == n->motif && o->tier == 3 && n->tier == 3 && o->branch != n->branch && tattoo_has(b, i)) return TAT_OTHER_BRANCH;
    }
    if (level < n->level) return TAT_LEVEL;
    if (n->tier > 1) { // el grado anterior del motivo, hecho
        bool prev = false;
        for (int i = 0; i < TATTOO_NODES; i++)
            if (NODES[i].motif == n->motif && NODES[i].tier == n->tier - 1 && tattoo_has(b, i)) prev = true;
        if (!prev) return TAT_NEEDS_PREV;
    }
    return TAT_OK;
}

TattooCheck tattoo_apply(TattooBody *b, int node, TattooZone zone, int level) {
    TattooCheck c = tattoo_can(b, node, zone, level);
    if (c == TAT_OK) b->node[zone] = (signed char)node;
    return c;
}

// Lo que favorece (1.5) o no le va (0.75) a cada zona del cuerpo.
static const struct {
    Stat fav[3];
    Stat unfav[2];
    Stat bonus;
} ZONES[TZ_COUNT] = {
    [TZ_HEAD] = { { STAT_PERCEPTION, STAT_ARCHERY, STAT_CHARISMA }, { STAT_MELEE, STAT_CARRY }, STAT_PERCEPTION },
    [TZ_NECK] = { { STAT_CHARISMA, STAT_STAMINA_REGEN, STAT_COUNT }, { STAT_SPEED, STAT_COUNT }, STAT_CHARISMA },
    [TZ_CHEST] = { { STAT_HEALTH, STAT_STAMINA_REGEN, STAT_COUNT }, { STAT_STEALTH, STAT_ARCHERY }, STAT_HEALTH },
    [TZ_BACK] = { { STAT_CARRY, STAT_HEALTH, STAT_COUNT }, { STAT_PERCEPTION, STAT_COUNT }, STAT_CARRY },
    [TZ_ARM_L] = { { STAT_GRAPPLE_POWER, STAT_HEALTH, STAT_COUNT }, { STAT_ARCHERY, STAT_COUNT }, STAT_GRAPPLE_POWER },
    [TZ_ARM_R] = { { STAT_MELEE, STAT_ARCHERY, STAT_COUNT }, { STAT_STEALTH, STAT_COUNT }, STAT_MELEE },
    [TZ_HANDS] = { { STAT_CRAFT, STAT_ARCHERY, STAT_GRAPPLE_POWER }, { STAT_SPEED, STAT_COUNT }, STAT_CRAFT },
    [TZ_LEGS] = { { STAT_SPEED, STAT_STEALTH, STAT_RIDING }, { STAT_CRAFT, STAT_COUNT }, STAT_SPEED },
};

float zone_weight(TattooZone z, Stat s) {
    if (z < 0 || z >= TZ_COUNT) return 1.0f;
    for (int i = 0; i < 3; i++)
        if (ZONES[z].fav[i] == s) return 1.5f;
    for (int i = 0; i < 2; i++)
        if (ZONES[z].unfav[i] == s) return 0.75f;
    return 1.0f;
}

Stat zone_bonus_stat(TattooZone z) { return z >= 0 && z < TZ_COUNT ? ZONES[z].bonus : STAT_COUNT; }

AbilityId tattoo_effect(int node, TattooZone zone, float *mods, float *potency) {
    const TattooNode *n = tattoo_node(node);
    if (!n) return ABIL_NONE;
    for (int k = 0; k < 2; k++)
        if (n->stat[k] < STAT_COUNT) mods[n->stat[k]] += n->value[k] * zone_weight(zone, n->stat[k]);
    Stat b = zone_bonus_stat(zone);
    if (b < STAT_COUNT) mods[b] += 0.05f;
    if (potency) { // la habilidad pesa segun lo que la zona favorece de ella
        const AbilityDef *a = ability_def(n->active);
        float best = 1.0f;
        for (int s = 0; s < STAT_COUNT; s++)
            if (a->mods[s] > 0.0f) best = fmaxf(best, zone_weight(zone, (Stat)s));
        if (n->active == ABIL_TIDE) best = zone_weight(zone, STAT_HEALTH);
        *potency = best;
    }
    return n->active;
}

float tattoo_stat(const TattooBody *b, Stat s) {
    float total = 0.0f;
    for (int z = 0; z < TZ_COUNT; z++) {
        if (b->node[z] == TATTOO_NONE) continue;
        float mods[STAT_COUNT] = { 0 };
        tattoo_effect(b->node[z], (TattooZone)z, mods, NULL);
        total += mods[s];
    }
    return total;
}

int tattoo_describe(int node, TattooZone zone, char *out, int len) {
    float mods[STAT_COUNT] = { 0 };
    float pot = 1.0f;
    AbilityId a = tattoo_effect(node, zone, mods, &pot);
    int n = 0;
    out[0] = '\0';
    for (int s = 0; s < STAT_COUNT && n < len; s++)
        if (mods[s] != 0.0f) n += snprintf(out + n, (size_t)(len - n), "%s%s %+d %%", n ? ", " : "", stat_name((Stat)s), (int)(mods[s] * 100.0f + 0.5f));
    if (a != ABIL_NONE && n < len)
        n += snprintf(out + n, (size_t)(len - n), T("%s%s (activa, x%.2f)"), n ? ", " : "", T(ability_def(a)->name), pot);
    return n;
}

const TattooInk *tattoo_ink(int tier) {
    static const TattooInk I1[] = { { "utileria.material.carbon", 1 }, { "utileria.consumible.hierbas", 1 }, { NULL, 0 } };
    static const TattooInk I2[] = { { "utileria.material.carbon", 2 }, { "utileria.consumible.hierbas", 2 }, { NULL, 0 } };
    static const TattooInk I3[] = { { "utileria.material.carbon", 2 }, { "utileria.consumible.hierbas", 2 }, { "utileria.consumible.miel", 1 }, { NULL, 0 } };
    return tier >= 3 ? I3 : tier == 2 ? I2 : I1;
}
