#include "talents_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/input.h"
#include "game/inventory_game.h"
#include "raylib.h"
#include "sim/lang.h"
#include "ui/icons.h"
#include "ui/theme.h"

#define TALK_RANGE 3.0f

typedef enum {
    TK_NONE,
    TK_DRUID,       // tatuar o encantar
    TK_TAT_NODE,    // que tatuaje
    TK_TAT_ZONE,    // donde
    TK_TAT_CONFIRM, // para siempre
    TK_ENCH_PICK,   // que joya encantar
    TK_ENCH_KIND,   // pasivo o activo
    TK_SMITH,       // hacer una joya
    TK_J_TYPE,
    TK_J_METAL,
    TK_J_GEM,
    TK_J_CONFIRM,
} TalkMode;

static const Ingredient ENCHANT_COST[] = { { "utileria.consumible.hierbas", 1 }, { "utileria.consumible.miel", 1 }, { NULL, 0 } };

void tg_init(GameActions *ga) {
    progress_init(&ga->prog);
    tattoo_body_init(&ga->tattoos);
    memset(&ga->jewels, 0, sizeof(ga->jewels));
    memset(ga->ab_timer, 0, sizeof(ga->ab_timer));
    memset(ga->ab_cd, 0, sizeof(ga->ab_cd));
    memset(ga->ab_pot, 0, sizeof(ga->ab_pot));
    ga->warmth_boost = 0.0f;
    memset(&ga->dlg, 0, sizeof(ga->dlg));
    ga->talk_mode = TK_NONE;
    ga->talk_member = -1;
}

bool tg_blocks_input(const GameActions *ga) { return ga->dlg.open; }

static float dist_xz(Vector3 a, Vector3 b) { return sqrtf((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z)); }

static const char *item_name(const GameActions *ga, const char *id) {
    const InvItem *it = inventory_find(ga->inv, id);
    return it ? T(it->name) : id;
}

static void lower_first(char *s) {
    if (s[0] >= 'A' && s[0] <= 'Z') s[0] = (char)(s[0] - 'A' + 'a');
}

// ---------------------------------------------------------------- experiencia
void tg_xp(GameActions *ga, XpSource s, char *log, size_t len) {
    int up = progress_add(&ga->prog, xp_reward(s));
    if (up > 0 && log) {
        bool tier = ga->prog.level == 3 || ga->prog.level == 6;
        snprintf(log, len, tier ? T("¡Subes al nivel %d! Un druida puede tatuarte un grado nuevo.") : T("¡Subes al nivel %d!"), ga->prog.level);
    }
}

// ---------------------------------------------------------------- habilidades activas
int tg_actives(const GameActions *ga, AbilityId *out, float *potency, int max) {
    int n = 0;
    AbilityId seen[ABIL_COUNT] = { 0 };
    float pot[ABIL_COUNT] = { 0 };
    for (int z = 0; z < TZ_COUNT; z++) { // tatuajes
        if (ga->tattoos.node[z] == TATTOO_NONE) continue;
        float mods[STAT_COUNT] = { 0 }, p = 1.0f;
        AbilityId a = tattoo_effect(ga->tattoos.node[z], (TattooZone)z, mods, &p);
        if (a != ABIL_NONE) seen[a] = a, pot[a] = fmaxf(pot[a], p);
    }
    for (int j = 0; j < JS_COUNT; j++) { // joyas encantadas
        const WornJewel *w = &ga->jewels.slot[j];
        if (!w->id[0]) continue;
        float mods[STAT_COUNT] = { 0 }, p = 1.0f;
        AbilityId a = jewel_effect(w->id, w->var, mods, &p);
        if (a != ABIL_NONE) seen[a] = a, pot[a] = fmaxf(pot[a], p);
    }
    for (int a = 1; a < ABIL_COUNT && n < max; a++)
        if (seen[a]) {
            out[n] = (AbilityId)a;
            if (potency) potency[n] = pot[a];
            n++;
        }
    return n;
}

float tg_stat(const GameActions *ga, Stat s) {
    float v = tattoo_stat(&ga->tattoos, s) + jewelry_stat(&ga->jewels, s);
    for (int a = 1; a < ABIL_COUNT; a++)
        if (ga->ab_timer[a] > 0.0f) v += ability_def((AbilityId)a)->mods[s] * ga->ab_pot[a];
    return v;
}

static void use_ability(GameActions *ga, Combat *cb, AbilityId a, float pot, char *log, size_t len) {
    const AbilityDef *d = ability_def(a);
    if (ga->ab_cd[a] > 0.0f) {
        snprintf(log, len, T("%s: espera %.0f s."), T(d->name), ga->ab_cd[a]);
        return;
    }
    ga->ab_cd[a] = d->cooldown;
    ga->ab_timer[a] = d->duration;
    ga->ab_pot[a] = pot;
    Health *h = &cb->player;
    if (d->heal > 0.0f) h->hp = fminf(h->hp_max, h->hp + h->hp_max * d->heal * pot);
    if (d->staunch) health_treat(h);
    if (d->warmth > 0.0f) ga->warmth_boost += d->warmth * 100.0f * pot;
    snprintf(log, len, T("%s: %s"), T(d->name), T(d->desc));
}

// ---------------------------------------------------------------- dialogos
static const Member *talk_member(const GameActions *ga, const Troop *troop) {
    for (int k = 0; k < troop->count && k < TROOP_MAX; k++)
        if (troop->members[k].id == ga->talk_member) return &troop->members[k];
    return NULL;
}

bool tg_try_talk(GameActions *ga, const Troop *troop, const Player *p, char *log, size_t len) {
    const Member *best = NULL;
    float bd = TALK_RANGE;
    for (int k = 0; k < troop->count && k < TROOP_MAX; k++) {
        const Member *m = &troop->members[k];
        const Npc *n = &ga->npcs[k];
        if (m->status != STATUS_ACTIVE || m->health.down || n->member_id != m->id) continue;
        if (m->role != ROLE_DRUID && m->role != ROLE_GOLDSMITH) continue;
        float d = dist_xz(n->pos, p->pos);
        if (d < bd) bd = d, best = m;
    }
    if (!best) return false;
    ga->talk_member = best->id;
    ga->talk_mode = best->role == ROLE_DRUID ? TK_DRUID : TK_SMITH;
    ga->dlg.open = false; // empieza con el cursor al principio
    ga->dlg.cursor = 0;
    ga->dlg.open = true;
    (void)log, (void)len;
    return true;
}

static void set_mode(GameActions *ga, int mode) {
    ga->talk_mode = mode;
    ga->dlg.cursor = 0;
}

static bool have_all(GameActions *ga, const Props *props, const Player *p, const Ingredient *mats) {
    return ig_first_missing(ga, props, p, mats) == NULL;
}

static const char *cost_text(GameActions *ga, const Props *props, const Player *p, const Ingredient *mats) {
    static char buf[160];
    int n = 0;
    buf[0] = '\0';
    for (const Ingredient *m = mats; m->id && n < (int)sizeof(buf); m++)
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s%s %d/%d", n ? ", " : "", item_name(ga, m->id), ig_count(ga, props, p, m->id), m->count);
    return buf;
}

// La tinta del druida para un tatuaje, como lista de ingredientes.
static void ink_of(int tier, Ingredient *out) {
    const TattooInk *ink = tattoo_ink(tier);
    int i = 0;
    for (; ink[i].id && i < MAT_MAX - 1; i++) out[i] = (Ingredient){ ink[i].id, ink[i].count };
    out[i] = (Ingredient){ NULL, 0 };
}

// Las joyas a mano con piedra y sin encantar.
typedef struct {
    Bag *bag;
    int slot;
} JewelRef;

static int unenchanted(GameActions *ga, const Props *props, const Player *p, JewelRef *out, int max) {
    Bag *bags[10];
    int nb = ig_bags(ga, props, p, bags, 10), n = 0;
    for (int b = 0; b < nb; b++)
        for (int i = 0; i < bags[b]->n && n < max; i++) {
            const BagSlot *s = &bags[b]->s[i];
            if (jewel_parse(s->id, NULL, NULL) && jewel_gem(s->var) != GEM_NONE && jewel_enchant(s->var) == ENCH_NONE)
                out[n++] = (JewelRef){ bags[b], i };
        }
    return n;
}

static IconId motif_icon(Motif m) {
    static const IconId I[MOTIF_COUNT] = { ICON_ANIMAL, ICON_VELOCIDAD, ICON_EXPLORADOR, ICON_TATUAJE, ICON_AGUA };
    return m >= 0 && m < MOTIF_COUNT ? I[m] : ICON_TATUAJE;
}

static IconId zone_icon(TattooZone z) {
    static const IconId I[TZ_COUNT] = { ICON_CASCO, ICON_CUELLO, ICON_TORSO, ICON_MOCHILA, ICON_BRAZALES, ICON_SABLE, ICON_GUANTES, ICON_GREBAS };
    return z >= 0 && z < TZ_COUNT ? I[z] : ICON_TATUAJE;
}

static IconId type_icon(JewelType t) {
    static const IconId I[JT_COUNT] = { ICON_ANILLO, ICON_BRAZALETE, ICON_COLLAR, ICON_ARETE, ICON_HEBILLA };
    return t >= 0 && t < JT_COUNT ? I[t] : ICON_ANILLO;
}

static const char *roman(int tier) { return tier == 3 ? "III" : tier == 2 ? "II" : "I"; }

// Rehace las opciones del dialogo segun el paso en que va.
static void build(GameActions *ga, const Troop *troop, const Props *props, const Player *p) {
    Dialog *d = &ga->dlg;
    const Member *m = talk_member(ga, troop);
    const char *who = m ? m->name : "";
    char back[32];
    snprintf(back, sizeof(back), "%s", T("Volver"));
    switch (ga->talk_mode) {
    case TK_DRUID:
        dlg_begin(d, TextFormat(T("%s, druida"), who), ICON_DRUIDA, T("Los espíritus de la estepa escuchan. ¿Quieres llevar un motivo en la piel, o que despierte una joya?"));
        dlg_option(d, ICON_TATUAJE, T("Tatuarme"), T("Para siempre: elige motivo y zona del cuerpo"), true, 1);
        dlg_option(d, ICON_GEMA, T("Encantar una joya"), T("Pasivo o activo, según su piedra (hierbas y miel)"), true, 2);
        dlg_option(d, ICON_SALIR, T("Adiós"), "", true, 0);
        break;
    case TK_TAT_NODE: {
        dlg_begin(d, TextFormat(T("%s, druida"), who), ICON_DRUIDA,
                  TextFormat(T("¿Qué motivo? Cada uno sube en tres grados; el tercero se bifurca. Tu nivel: %d."), ga->prog.level));
        for (int i = 0; i < TATTOO_NODES; i++) {
            const TattooNode *n = tattoo_node(i);
            bool have = tattoo_has(&ga->tattoos, i);
            // ¿Hay alguna zona libre donde se pueda?
            TattooCheck best = TAT_ZONE_TAKEN;
            for (int z = 0; z < TZ_COUNT && best != TAT_OK; z++) {
                TattooCheck c = tattoo_can(&ga->tattoos, i, (TattooZone)z, ga->prog.level);
                if (c != TAT_ZONE_TAKEN) best = c;
            }
            DlgOption *o = dlg_option(d, motif_icon(n->motif), TextFormat("%s (%s %s)", T(n->name), motif_name(n->motif), roman(n->tier)),
                                      have ? T("Ya lo llevas") : best == TAT_OK ? TextFormat(T("%s · nivel %d"), T(n->desc), n->level) : tattoo_check_text(best),
                                      best == TAT_OK && !have, 100 + i);
            if (o) {
                snprintf(o->badge, sizeof(o->badge), "%s", roman(n->tier));
                o->marked = have;
            }
        }
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    }
    case TK_TAT_ZONE: {
        const TattooNode *n = tattoo_node(ga->talk_a);
        dlg_begin(d, TextFormat(T("%s, druida"), who), ICON_DRUIDA,
                  TextFormat(T("%s: ¿dónde? La zona cambia a qué va el bonus y cuánto pesa."), T(n->name)));
        for (int z = 0; z < TZ_COUNT; z++) {
            char eff[160];
            tattoo_describe(ga->talk_a, (TattooZone)z, eff, sizeof(eff));
            TattooCheck c = tattoo_can(&ga->tattoos, ga->talk_a, (TattooZone)z, ga->prog.level);
            char zn[48];
            snprintf(zn, sizeof(zn), "%s", zone_name((TattooZone)z));
            DlgOption *o = dlg_option(d, zone_icon((TattooZone)z), zn, c == TAT_OK ? eff : tattoo_check_text(c), c == TAT_OK, 200 + z);
            if (o) o->marked = ga->tattoos.node[z] != TATTOO_NONE;
        }
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    }
    case TK_TAT_CONFIRM: {
        const TattooNode *n = tattoo_node(ga->talk_a);
        Ingredient ink[MAT_MAX];
        ink_of(n->tier, ink);
        char eff[160];
        tattoo_describe(ga->talk_a, (TattooZone)ga->talk_b, eff, sizeof(eff));
        bool ok = have_all(ga, props, p, ink);
        dlg_begin(d, TextFormat(T("%s, druida"), who), ICON_DRUIDA,
                  TextFormat(T("%s en %s: %s. No se podrá quitar nunca. Tinta: %s."), T(n->name), zone_name((TattooZone)ga->talk_b), eff,
                             cost_text(ga, props, p, ink)));
        dlg_option(d, ICON_OK, T("Tatuar, para siempre"), ok ? eff : T("Falta tinta: carbón, hierbas (y miel en el tercer grado)"), ok, 1);
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    }
    case TK_ENCH_PICK: {
        JewelRef refs[DLG_OPTIONS - 1];
        int n = unenchanted(ga, props, p, refs, DLG_OPTIONS - 1);
        dlg_begin(d, TextFormat(T("%s, druida"), who), ICON_DRUIDA,
                  n ? T("¿Qué joya despierto? La piedra decide qué puede dar.") : T("No llevas joyas con piedra sin encantar. Un orfebre las hace."));
        for (int i = 0; i < n; i++) {
            const BagSlot *s = &refs[i].bag->s[refs[i].slot];
            JewelType t;
            jewel_parse(s->id, &t, NULL);
            DlgOption *o = dlg_option(d, type_icon(t), item_name(ga, s->id), gem_name(jewel_gem(s->var)), true, 300 + i);
            (void)o;
        }
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    }
    case TK_ENCH_KIND: {
        JewelRef refs[DLG_OPTIONS];
        int n = unenchanted(ga, props, p, refs, DLG_OPTIONS);
        if (ga->talk_a >= n) {
            set_mode(ga, TK_ENCH_PICK);
            build(ga, troop, props, p);
            return;
        }
        const BagSlot *s = &refs[ga->talk_a].bag->s[refs[ga->talk_a].slot];
        Gem g = jewel_gem(s->var);
        JewelType t;
        JewelMetal mt;
        jewel_parse(s->id, &t, &mt);
        float k = jewel_potency(t, mt);
        bool ok = have_all(ga, props, p, ENCHANT_COST);
        dlg_begin(d, TextFormat(T("%s, druida"), who), ICON_DRUIDA,
                  TextFormat(T("%s con %s. ¿Qué despierto en ella? Cuesta: %s."), item_name(ga, s->id), gem_name(g), cost_text(ga, props, p, ENCHANT_COST)));
        dlg_option(d, ICON_NIVEL, T("Efecto pasivo"),
                   TextFormat("%s %+d%%", stat_name(gem_passive_stat(g)), (int)(gem_passive_value(g) * k * 100.0f + 0.5f)), ok, 1);
        AbilityId a = gem_active(g);
        dlg_option(d, ICON_VELOCIDAD, T("Habilidad activa"), TextFormat(T("%s (x%.2f): %s"), T(ability_def(a)->name), k, T(ability_def(a)->desc)), ok, 2);
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    }
    case TK_SMITH:
        dlg_begin(d, TextFormat(T("%s, orfebre"), who), ICON_ORFEBRE,
                  T("Con metal y una piedra te hago una joya. Sin encantar no hace nada: eso es cosa del druida."));
        dlg_option(d, ICON_ANILLO, T("Hacer una joya"), T("Anillo, brazalete, collar, aretes o hebilla"), true, 1);
        dlg_option(d, ICON_SALIR, T("Adiós"), "", true, 0);
        break;
    case TK_J_TYPE:
        dlg_begin(d, TextFormat(T("%s, orfebre"), who), ICON_ORFEBRE, T("¿Qué pieza? Cada una va en su hueco y pesa distinto."));
        for (int t = 0; t < JT_COUNT; t++)
            dlg_option(d, type_icon((JewelType)t), jewel_type_name((JewelType)t),
                       TextFormat(T("%d de metal, %d piedra(s) · potencia x%.1f"), jewel_metal_cost((JewelType)t), jewel_gem_cost((JewelType)t),
                                  jewel_potency((JewelType)t, METAL_BRONZE)),
                       true, 400 + t);
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    case TK_J_METAL:
        dlg_begin(d, TextFormat(T("%s, orfebre"), who), ICON_ORFEBRE, T("¿En qué metal? El oro rinde más que la plata, y la plata más que el bronce."));
        for (int mt = 0; mt < METAL_COUNT; mt++) {
            int have = ig_count(ga, props, p, metal_id((JewelMetal)mt)), need = jewel_metal_cost((JewelType)ga->talk_a);
            DlgOption *o = dlg_option(d, ICON_LINGOTE, item_name(ga, metal_id((JewelMetal)mt)),
                                      TextFormat(T("tienes %d, hacen falta %d · potencia x%.2f"), have, need,
                                                 jewel_potency((JewelType)ga->talk_a, (JewelMetal)mt)),
                                      have >= need, 500 + mt);
            if (o) snprintf(o->badge, sizeof(o->badge), "%d", have);
        }
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    case TK_J_GEM:
        dlg_begin(d, TextFormat(T("%s, orfebre"), who), ICON_ORFEBRE, T("¿Con qué piedra? Decide qué podrá despertar el druida."));
        for (int g = 1; g < GEM_COUNT; g++) {
            int have = ig_count(ga, props, p, gem_id((Gem)g)), need = jewel_gem_cost((JewelType)ga->talk_a);
            DlgOption *o = dlg_option(d, ICON_GEMA, gem_name((Gem)g),
                                      TextFormat(T("tienes %d, hacen falta %d · %s o %s"), have, need, stat_name(gem_passive_stat((Gem)g)),
                                                 T(ability_def(gem_active((Gem)g))->name)),
                                      have >= need, 600 + g);
            if (o) snprintf(o->badge, sizeof(o->badge), "%d", have);
        }
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    case TK_J_CONFIRM: {
        const char *id = jewel_id((JewelType)ga->talk_a, (JewelMetal)ga->talk_b);
        dlg_begin(d, TextFormat(T("%s, orfebre"), who), ICON_ORFEBRE,
                  TextFormat(T("%s con %s. Pondré %d de metal y %d piedra(s)."), item_name(ga, id), gem_name((Gem)ga->talk_c),
                             jewel_metal_cost((JewelType)ga->talk_a), jewel_gem_cost((JewelType)ga->talk_a)));
        dlg_option(d, ICON_OK, T("Hazla"), T("Va a tu mochila (o donde quepa)"), true, 1);
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    }
    default: dlg_close(d); break;
    }
}

static void choose(GameActions *ga, Troop *troop, Props *props, const Player *p, int v, char *log, size_t len) {
    const Member *m = talk_member(ga, troop);
    switch (ga->talk_mode) {
    case TK_DRUID:
    case TK_SMITH:
        if (v == 0) dlg_close(&ga->dlg);
        else if (ga->talk_mode == TK_DRUID) set_mode(ga, v == 1 ? TK_TAT_NODE : TK_ENCH_PICK);
        else set_mode(ga, TK_J_TYPE);
        break;
    case TK_TAT_NODE:
        if (v == 0) set_mode(ga, TK_DRUID);
        else ga->talk_a = v - 100, set_mode(ga, TK_TAT_ZONE);
        break;
    case TK_TAT_ZONE:
        if (v == 0) set_mode(ga, TK_TAT_NODE);
        else ga->talk_b = v - 200, set_mode(ga, TK_TAT_CONFIRM);
        break;
    case TK_TAT_CONFIRM: {
        if (v == 0) {
            set_mode(ga, TK_TAT_ZONE);
            break;
        }
        const TattooNode *n = tattoo_node(ga->talk_a);
        Ingredient ink[MAT_MAX];
        ink_of(n->tier, ink);
        if (!have_all(ga, props, p, ink)) break;
        if (tattoo_apply(&ga->tattoos, ga->talk_a, (TattooZone)ga->talk_b, ga->prog.level) == TAT_OK) {
            ig_use_all(ga, props, p, ink);
            char zn[48];
            snprintf(zn, sizeof(zn), "%s", zone_name((TattooZone)ga->talk_b));
            snprintf(log, len, T("%s te tatúa %s en %s. Ya es parte de ti."), m ? m->name : "", T(n->name), zn);
        }
        dlg_close(&ga->dlg);
        break;
    }
    case TK_ENCH_PICK:
        if (v == 0) set_mode(ga, TK_DRUID);
        else ga->talk_a = v - 300, set_mode(ga, TK_ENCH_KIND);
        break;
    case TK_ENCH_KIND: {
        if (v == 0) {
            set_mode(ga, TK_ENCH_PICK);
            break;
        }
        JewelRef refs[DLG_OPTIONS];
        int n = unenchanted(ga, props, p, refs, DLG_OPTIONS);
        if (ga->talk_a >= n || !have_all(ga, props, p, ENCHANT_COST)) break;
        BagSlot *s = &refs[ga->talk_a].bag->s[refs[ga->talk_a].slot];
        // Sacarla, encantarla y volver a guardarla (puede apilarse con otra igual).
        char id[INV_ID_LEN];
        snprintf(id, sizeof(id), "%s", s->id);
        unsigned short var = jewel_var(jewel_gem(s->var), v == 1 ? ENCH_PASSIVE : ENCH_ACTIVE);
        float cond = s->condition;
        bag_remove_slot(refs[ga->talk_a].bag, refs[ga->talk_a].slot, 1);
        ig_use_all(ga, props, p, ENCHANT_COST);
        ig_store_var(ga, props, p, id, 1, cond, var);
        char d[160];
        jewel_describe(id, var, d, sizeof(d));
        snprintf(log, len, T("%s despierta la joya: %s."), m ? m->name : "", d);
        dlg_close(&ga->dlg);
        break;
    }
    case TK_J_TYPE:
        if (v == 0) set_mode(ga, TK_SMITH);
        else ga->talk_a = v - 400, set_mode(ga, TK_J_METAL);
        break;
    case TK_J_METAL:
        if (v == 0) set_mode(ga, TK_J_TYPE);
        else ga->talk_b = v - 500, set_mode(ga, TK_J_GEM);
        break;
    case TK_J_GEM:
        if (v == 0) set_mode(ga, TK_J_METAL);
        else ga->talk_c = v - 600, set_mode(ga, TK_J_CONFIRM);
        break;
    case TK_J_CONFIRM: {
        if (v == 0) {
            set_mode(ga, TK_J_GEM);
            break;
        }
        JewelType t = (JewelType)ga->talk_a;
        JewelMetal mt = (JewelMetal)ga->talk_b;
        Gem g = (Gem)ga->talk_c;
        Ingredient cost[3] = { { metal_id(mt), jewel_metal_cost(t) }, { gem_id(g), jewel_gem_cost(t) }, { NULL, 0 } };
        if (!have_all(ga, props, p, cost)) {
            snprintf(log, len, "%s", T("Falta metal o piedra."));
            break;
        }
        const char *id = jewel_id(t, mt);
        if (ig_store_var(ga, props, p, id, 1, 1.0f, jewel_var(g, ENCH_NONE)) != 1) {
            snprintf(log, len, "%s", T("No hay sitio donde guardar la joya."));
            break;
        }
        ig_use_all(ga, props, p, cost);
        char name[64];
        snprintf(name, sizeof(name), "%s", item_name(ga, id));
        lower_first(name);
        snprintf(log, len, T("%s te entrega un %s con %s. Llévaselo a un druida para que despierte."), m ? m->name : "", name, gem_name(g));
        tg_xp(ga, XP_CRAFT, NULL, 0);
        dlg_close(&ga->dlg);
        break;
    }
    default: dlg_close(&ga->dlg); break;
    }
}

void tg_update(GameActions *ga, Combat *cb, Troop *troop, Props *props, const Player *p, bool input_ok, float dt, char *log,
               size_t len) {
    for (int a = 1; a < ABIL_COUNT; a++) {
        ga->ab_timer[a] = fmaxf(0.0f, ga->ab_timer[a] - dt);
        ga->ab_cd[a] = fmaxf(0.0f, ga->ab_cd[a] - dt);
    }
    if (ga->dlg.open && ga->talk_mode >= 100) return; // un dialogo del campamento (src/game/camp_game.c)
    if (ga->dlg.open) {
        const Member *m = talk_member(ga, troop);
        bool near = false;
        for (int k = 0; m && k < troop->count && k < TROOP_MAX; k++)
            if (ga->npcs[k].member_id == m->id) near = dist_xz(ga->npcs[k].pos, p->pos) < TALK_RANGE + 2.0f;
        if (!m || !near || m->status != STATUS_ACTIVE) { // se fue (o lo hirieron)
            dlg_close(&ga->dlg);
            return;
        }
        build(ga, troop, props, p);
        int v = dlg_update(&ga->dlg);
        if (v == DLG_BACK) {
            if (ga->talk_mode == TK_DRUID || ga->talk_mode == TK_SMITH) dlg_close(&ga->dlg);
            else choose(ga, troop, props, p, 0, log, len); // Esc: un paso atras
        } else if (v >= 0) {
            choose(ga, troop, props, p, v, log, len);
        }
        if (ga->dlg.open) build(ga, troop, props, p);
        return;
    }
    if (!input_ok) return;
    AbilityId act[3];
    float pot[3];
    int n = tg_actives(ga, act, pot, 3);
    for (int i = 0; i < n; i++)
        if (input_pressed((InputAction)(IN_ABILITY1 + i))) use_ability(ga, cb, act[i], pot[i], log, len);
}

void tg_draw_hud(const GameActions *ga, int w, int h) {
    AbilityId act[3];
    float pot[3];
    int n = tg_actives(ga, act, pot, 3);
    for (int i = 0; i < n; i++) { // abajo a la derecha, sobre la ayuda de teclas
        Rectangle r = { (float)(w - 8 - (n - i) * 30), (float)(h - 52), 26, 26 };
        AbilityId a = act[i];
        bool on = ga->ab_timer[a] > 0.0f, ready = ga->ab_cd[a] <= 0.0f;
        bool hover = ui_tile(r, ICON_NIVEL, on, ready);
        if (!ready) { // la espera: se vacia de arriba abajo
            float f = ga->ab_cd[a] / fmaxf(1.0f, ability_def(a)->cooldown);
            DrawRectangle((int)r.x + 1, (int)r.y + 1, (int)r.width - 2, (int)((r.height - 2) * f), (Color){ 10, 8, 6, 170 });
        }
        DrawText(TextFormat("F%d", i + 2), (int)r.x + 2, (int)r.y + 1, 10, ready ? UI_GOLD_LIGHT : UI_BONE_DIM);
        if (hover) ui_legend(T(ability_def(a)->name), ready ? T(ability_def(a)->desc) : TextFormat(T("espera %.0f s"), ga->ab_cd[a]));
    }
}

void tg_draw(const GameActions *ga, int w, int h) { dlg_draw(&ga->dlg, w, h); }
