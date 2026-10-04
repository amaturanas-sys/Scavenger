#include "jewelry.h"

#include <stdio.h>
#include <string.h>

#include "lang.h"

unsigned short jewel_var(Gem g, Enchant e) { return (unsigned short)(((unsigned)g & 15u) | (((unsigned)e & 3u) << 4)); }
Gem jewel_gem(unsigned short var) { return (Gem)(var & 15u); }
Enchant jewel_enchant(unsigned short var) { return (Enchant)((var >> 4) & 3u); }

static const char *TYPE_KEY[JT_COUNT] = { "anillo", "brazalete", "collar", "aretes", "hebilla" };
static const char *METAL_KEY[METAL_COUNT] = { "bronce", "plata", "oro" };
static const char *GEM_KEY[GEM_COUNT] = { "", "turquesa", "cornalina", "lapislazuli", "ambar", "jade", "granate", "perla", "coral" };

bool jewel_parse(const char *id, JewelType *type, JewelMetal *metal) {
    if (!id || strncmp(id, "accesorio.", 10) != 0) return false;
    for (int t = 0; t < JT_COUNT; t++) {
        size_t n = strlen(TYPE_KEY[t]);
        if (strncmp(id + 10, TYPE_KEY[t], n) || id[10 + n] != '.') continue;
        for (int m = 0; m < METAL_COUNT; m++)
            if (!strcmp(id + 11 + n, METAL_KEY[m])) {
                if (type) *type = (JewelType)t;
                if (metal) *metal = (JewelMetal)m;
                return true;
            }
    }
    return false;
}

const char *jewel_id(JewelType t, JewelMetal m) {
    static char buf[JT_COUNT][METAL_COUNT][INV_ID_LEN];
    if (t < 0 || t >= JT_COUNT || m < 0 || m >= METAL_COUNT) return "";
    if (!buf[t][m][0]) snprintf(buf[t][m], INV_ID_LEN, "accesorio.%s.%s", TYPE_KEY[t], METAL_KEY[m]);
    return buf[t][m];
}

const char *gem_id(Gem g) {
    static char buf[GEM_COUNT][INV_ID_LEN];
    if (g <= GEM_NONE || g >= GEM_COUNT) return "";
    if (!buf[g][0]) snprintf(buf[g], INV_ID_LEN, "utileria.gema.%s", GEM_KEY[g]);
    return buf[g];
}

Gem gem_from_id(const char *id) {
    if (!id || strncmp(id, "utileria.gema.", 14) != 0) return GEM_NONE;
    for (int g = 1; g < GEM_COUNT; g++)
        if (!strcmp(id + 14, GEM_KEY[g])) return (Gem)g;
    return GEM_NONE;
}

const char *metal_id(JewelMetal m) {
    static const char *I[METAL_COUNT] = { "utileria.material.bronce", "utileria.material.plata", "utileria.material.oro" };
    return m >= 0 && m < METAL_COUNT ? I[m] : "";
}

const char *gem_name(Gem g) {
    static const char *N[GEM_COUNT] = { N_("sin piedra"), N_("turquesa"), N_("cornalina"), N_("lapislázuli"), N_("ámbar"),
                                        N_("jade"),       N_("granate"),  N_("perla"),     N_("coral") };
    return g >= 0 && g < GEM_COUNT ? T(N[g]) : "?";
}

const char *jewel_type_name(JewelType t) {
    static const char *N[JT_COUNT] = { N_("anillo"), N_("brazalete"), N_("collar"), N_("aretes"), N_("hebilla") };
    return t >= 0 && t < JT_COUNT ? T(N[t]) : "?";
}

const char *jewel_slot_name(JewelSlot s) {
    static const char *N[JS_COUNT] = { N_("anillo, mano izquierda"), N_("anillo, mano izquierda"), N_("anillo, mano derecha"),
                                       N_("anillo, mano derecha"),   N_("brazalete izquierdo"),    N_("brazalete derecho"),
                                       N_("collar"),                 N_("aretes"),                 N_("hebilla del cinturón") };
    return s >= 0 && s < JS_COUNT ? T(N[s]) : "?";
}

JewelType slot_type(JewelSlot s) {
    if (s <= JS_RING_R2) return JT_RING;
    if (s <= JS_BRACELET_R) return JT_BRACELET;
    if (s == JS_NECKLACE) return JT_NECKLACE;
    if (s == JS_EARRINGS) return JT_EARRINGS;
    return JT_BUCKLE;
}

bool jewel_fits(const char *id, JewelSlot s) {
    JewelType t;
    if (jewel_parse(id, &t, NULL)) return t == slot_type(s);
    return s == JS_NECKLACE && id && !strncmp(id, "accesorio.amuleto.", 18); // los colgantes de animales
}

int jewel_metal_cost(JewelType t) {
    static const int C[JT_COUNT] = { 1, 2, 2, 1, 2 };
    return t >= 0 && t < JT_COUNT ? C[t] : 1;
}

int jewel_gem_cost(JewelType t) {
    static const int C[JT_COUNT] = { 1, 1, 2, 2, 1 };
    return t >= 0 && t < JT_COUNT ? C[t] : 1;
}

float jewel_potency(JewelType t, JewelMetal m) {
    static const float TP[JT_COUNT] = { 0.6f, 0.8f, 1.2f, 0.8f, 1.0f };
    static const float MP[METAL_COUNT] = { 1.0f, 1.3f, 1.6f };
    if (t < 0 || t >= JT_COUNT || m < 0 || m >= METAL_COUNT) return 0.0f;
    return TP[t] * MP[m];
}

// Cada piedra: un atributo (pasivo) o una habilidad (activo).
static const struct {
    Stat stat;
    float value;
    AbilityId active;
} GEMS[GEM_COUNT] = {
    [GEM_TURQUOISE] = { STAT_STAMINA_REGEN, 0.20f, ABIL_TIDE },
    [GEM_CARNELIAN] = { STAT_MELEE, 0.15f, ABIL_HOWL },
    [GEM_LAPIS] = { STAT_PERCEPTION, 0.20f, ABIL_EAGLE },
    [GEM_AMBER] = { STAT_CARRY, 0.20f, ABIL_WARMTH },
    [GEM_JADE] = { STAT_STEALTH, 0.20f, ABIL_SHADOW },
    [GEM_GARNET] = { STAT_HEALTH, 0.15f, ABIL_STAUNCH },
    [GEM_PEARL] = { STAT_CHARISMA, 0.20f, ABIL_WAR_CRY },
    [GEM_CORAL] = { STAT_RIDING, 0.20f, ABIL_GALLOP },
};

Stat gem_passive_stat(Gem g) { return g > GEM_NONE && g < GEM_COUNT ? GEMS[g].stat : STAT_COUNT; }
float gem_passive_value(Gem g) { return g > GEM_NONE && g < GEM_COUNT ? GEMS[g].value : 0.0f; }
AbilityId gem_active(Gem g) { return g > GEM_NONE && g < GEM_COUNT ? GEMS[g].active : ABIL_NONE; }

AbilityId jewel_effect(const char *id, unsigned short var, float *mods, float *potency) {
    if (potency) *potency = 1.0f;
    if (id && !strncmp(id, "accesorio.amuleto.", 18)) { // colgante de animal: ya encantado
        Charm c;
        if (!amulet_charm(id, &c)) return ABIL_NONE;
        for (int b = 0; b < c.buff_count; b++)
            for (int s = 0; s < STAT_COUNT; s++) mods[s] += c.buffs[b].mods[s];
        return ABIL_NONE;
    }
    JewelType t;
    JewelMetal m;
    if (!jewel_parse(id, &t, &m)) return ABIL_NONE;
    Gem g = jewel_gem(var);
    Enchant e = jewel_enchant(var);
    if (g == GEM_NONE || e == ENCH_NONE) return ABIL_NONE; // sin encantar: solo es una joya
    float k = jewel_potency(t, m);
    if (e == ENCH_PASSIVE) {
        mods[GEMS[g].stat] += GEMS[g].value * k;
        return ABIL_NONE;
    }
    if (potency) *potency = k;
    return GEMS[g].active;
}

float jewelry_stat(const Jewelry *j, Stat s) {
    float total = 0.0f;
    for (int i = 0; i < JS_COUNT; i++) {
        if (!j->slot[i].id[0]) continue;
        float mods[STAT_COUNT] = { 0 };
        jewel_effect(j->slot[i].id, j->slot[i].var, mods, NULL);
        total += mods[s];
    }
    return total;
}

int jewel_describe(const char *id, unsigned short var, char *out, int len) {
    float mods[STAT_COUNT] = { 0 };
    float pot = 1.0f;
    AbilityId a = jewel_effect(id, var, mods, &pot);
    int n = 0;
    out[0] = '\0';
    JewelType t;
    if (jewel_parse(id, &t, NULL)) {
        Gem g = jewel_gem(var);
        n += snprintf(out, (size_t)len, "%s", gem_name(g));
        if (g != GEM_NONE && jewel_enchant(var) == ENCH_NONE && n < len) n += snprintf(out + n, (size_t)(len - n), "%s", T(", sin encantar"));
    }
    for (int s = 0; s < STAT_COUNT && n < len; s++)
        if (mods[s] != 0.0f) n += snprintf(out + n, (size_t)(len - n), "%s%s %+d %%", n ? ", " : "", stat_name((Stat)s), (int)(mods[s] * 100.0f + 0.5f));
    if (a != ABIL_NONE && n < len)
        n += snprintf(out + n, (size_t)(len - n), T("%s%s (activa, x%.2f)"), n ? ", " : "", T(ability_def(a)->name), pot);
    return n;
}

Gem gem_roll(GemSource src, Rng *rng) {
    // Probabilidad de que salga alguna piedra, y cuales (pesos) segun el sitio.
    static const float CHANCE[GEMSRC_COUNT] = { 0.30f, 0.55f, 0.18f, 0.12f };
    static const float W[GEMSRC_COUNT][GEM_COUNT] = {
        [GEMSRC_ROCK] = { 0, 3, 3, 2, 0, 2, 3, 0, 0 },  // en la roca: turquesa, cornalina, granate, jade
        [GEMSRC_CORAL] = { 0, 0, 0, 0, 0, 0, 0, 2, 6 }, // coral y alguna perla
        [GEMSRC_DIG] = { 0, 2, 2, 3, 3, 1, 2, 0, 0 },   // excavando: lapislazuli y ambar
        [GEMSRC_RIVER] = { 0, 2, 3, 1, 2, 3, 2, 1, 0 }, // en el rio: jade y cornalina rodados
    };
    if (src < 0 || src >= GEMSRC_COUNT || rng_float(rng) >= CHANCE[src]) return GEM_NONE;
    float total = 0.0f;
    for (int g = 0; g < GEM_COUNT; g++) total += W[src][g];
    float r = rng_float(rng) * total;
    for (int g = 0; g < GEM_COUNT; g++) {
        if (r < W[src][g]) return (Gem)g;
        r -= W[src][g];
    }
    return GEM_COUNT - 1;
}
