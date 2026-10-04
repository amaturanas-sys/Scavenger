#include "game/inventory_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "game/talents_game.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "sim/lang.h"

#define CART_ID "vehiculo.tierra.carreta_bueyes"
#define CART_RANGE 5.0f  // m: para cargar la carreta
#define PACK_RANGE 8.0f  // m: para usar las alforjas de una montura
#define CARRY_BASE 20.0f // kg: con mas encima se anda mas lento (mas con tatuajes y joyas de carga)
#define CARRY_LIMIT (CARRY_BASE * (1.0f + ig_stat(ga, STAT_CARRY)))

static float dist_xz(Vector3 a, float x, float z) { return sqrtf((a.x - x) * (a.x - x) + (a.z - z) * (a.z - z)); }

void ig_init(GameActions *ga, Props *props, const Terrain *t) {
    bag_init(&ga->pockets, BAG_POCKETS, 0.0f);
    bag_init(&ga->backpack, BAG_BACKPACK, 0.0f);
    bag_init(&ga->cart, BAG_CART, 0.0f);
    bag_init(&ga->armory, BAG_CAMP, 0.0f);
    for (int i = 0; i < GA_PACKS; i++) ga->pack_animal[i] = -1;
    for (int i = 0; i < GA_LOOT; i++) ga->loot_age[i] = -1.0f;
    tg_init(ga);
    const Inventory *inv = ga->inv;
    // Lo que el jugador lleva al salir: municion, hierbas, algo de comer y dos amuletos.
    bag_add(&ga->pockets, inv, "utileria.consumible.hierbas", 1, 1.0f);
    bag_add(&ga->backpack, inv, "proyectil.flecha.comun", 12, 1.0f);
    bag_add(&ga->backpack, inv, "proyectil.virote.comun", 6, 1.0f);
    bag_add(&ga->backpack, inv, "proyectil.piedra.honda", 10, 1.0f);
    bag_add(&ga->backpack, inv, "utileria.consumible.hierbas", 2, 1.0f);
    bag_add(&ga->backpack, inv, "utileria.consumible.carne_seca", 2, 1.0f);
    bag_add(&ga->backpack, inv, "accesorio.amuleto.lobo", 1, 1.0f);
    bag_add(&ga->backpack, inv, "accesorio.amuleto.ciervo", 1, 1.0f);
    bag_add(&ga->pockets, inv, "utileria.gema.turquesa", 1, 1.0f); // para el orfebre
    bag_add(&ga->pockets, inv, "utileria.material.plata", 1, 1.0f);
    // La carreta de la tribu, junto al campamento, con algo de carga.
    props_add(props, CART_ID, (Vector3){ -12.0f, terrain_height(t, -12.0f, 24.0f), 24.0f }, 0.6f);
    bag_add(&ga->cart, inv, "utileria.material.troncos", 4, 1.0f);
    bag_add(&ga->cart, inv, "utileria.material.pieles", 4, 1.0f);
    bag_add(&ga->cart, inv, "utileria.material.cuerda", 2, 1.0f);
    // La armeria: piezas de repuesto (algunas gastadas).
    bag_add(&ga->armory, inv, "armadura.casco.escamas_hierro", 1, 0.6f);
    bag_add(&ga->armory, inv, "armadura.torso.malla", 1, 0.8f);
    bag_add(&ga->armory, inv, "armadura.hombreras.fieltro", 1, 1.0f);
    bag_add(&ga->armory, inv, "armadura.grebas.laminar_cuero", 1, 1.0f);
    bag_add(&ga->armory, inv, "accesorio.amuleto.aguila_ibice", 1, 1.0f);
}

bool ig_blocks_input(const GameActions *ga) { return ga->inv_open || ga->equip_open || ga->dlg.open; }

// ------------------------------------------------------------------ contenedores a mano
typedef enum { C_BAG, C_CAMP } ContType;

typedef struct {
    ContType type;
    Bag *bag;
    char label[64];
    IconId icon;
} Cont;

static bool near_cart(const Props *props, const Player *p) {
    if (!props) return false;
    for (int i = 0; i < props->count; i++)
        if (!strcmp(props->items[i].item->id, CART_ID) && dist_xz(p->pos, props->items[i].pos.x, props->items[i].pos.z) < CART_RANGE)
            return true;
    return false;
}

static bool near_camp(const Player *p) { return dist_xz(p->pos, 0.0f, 0.0f) < CAMP_STORE_RADIUS; }

static bool pack_near(const GameActions *ga, int k, const Player *p) {
    int a = ga->pack_animal[k];
    if (a < 0 || a >= ga->animal_count || !ga->animals[a].used) return false;
    return a == ga->mounted || dist_xz(p->pos, ga->animals[a].x, ga->animals[a].z) < PACK_RANGE;
}

static int containers(GameActions *ga, const Props *props, const Player *p, Cont out[10]) {
    int n = 0;
    out[n++] = (Cont){ C_BAG, &ga->pockets, "", ICON_BOLSILLO };
    snprintf(out[n - 1].label, sizeof(out[n - 1].label), "%s", T("Bolsillos"));
    out[n++] = (Cont){ C_BAG, &ga->backpack, "", ICON_MOCHILA };
    snprintf(out[n - 1].label, sizeof(out[n - 1].label), "%s", T("Mochila"));
    for (int k = 0; k < GA_PACKS; k++) {
        if (!pack_near(ga, k, p)) continue;
        out[n] = (Cont){ C_BAG, &ga->packs[k], "", ICON_ALFORJAS };
        snprintf(out[n].label, sizeof(out[n].label), T("Alforjas (%s)"), T(species_def(ga->animals[ga->pack_animal[k]].species)->name));
        n++;
    }
    if (near_cart(props, p)) {
        out[n++] = (Cont){ C_BAG, &ga->cart, "", ICON_CARRETA };
        snprintf(out[n - 1].label, sizeof(out[n - 1].label), "%s", T("Carreta de la tribu"));
    }
    if (near_camp(p)) {
        out[n++] = (Cont){ C_CAMP, NULL, "", ICON_CAMPAMENTO };
        snprintf(out[n - 1].label, sizeof(out[n - 1].label), "%s", T("Acopio y armería del campamento"));
    }
    return n;
}

int ig_count(const GameActions *ga, const Props *props, const Player *p, const char *id) {
    Cont c[10];
    int n = containers((GameActions *)ga, props, p, c), total = 0;
    for (int i = 0; i < n; i++)
        total += c[i].type == C_BAG ? bag_count(c[i].bag, id) : stock_count(&ga->stock, id) + bag_count(&ga->armory, id);
    return total;
}

int ig_use(GameActions *ga, const Props *props, const Player *p, const char *id, int n) {
    Cont c[10];
    int k = containers(ga, props, p, c), used = 0;
    for (int i = 0; i < k && used < n; i++) {
        if (c[i].type == C_BAG) {
            used += bag_take(c[i].bag, id, n - used, NULL);
        } else {
            int have = stock_count(&ga->stock, id), t = have < n - used ? have : n - used;
            if (t > 0) stock_take(&ga->stock, id, t), used += t;
            if (used < n) used += bag_take(&ga->armory, id, n - used, NULL);
        }
    }
    return used;
}

static int put_into(GameActions *ga, const Cont *to, const char *id, int n, float cond, unsigned short var) {
    if (to->type == C_CAMP) {
        if (item_is_gear(id) || !strncmp(id, "accesorio.", 10)) return bag_add_var(&ga->armory, ga->inv, id, n, cond, var);
        stock_add(&ga->stock, id, n);
        return n;
    }
    return bag_add_var(to->bag, ga->inv, id, n, cond, var);
}

int ig_store(GameActions *ga, const Props *props, const Player *p, const char *id, int n, float condition) {
    Cont c[10];
    int k = containers(ga, props, p, c), stored = 0;
    // Primero la mochila, luego los bolsillos y lo demas.
    static const int order[] = { 1, 0 };
    for (int o = 0; o < 2 && stored < n; o++) stored += put_into(ga, &c[order[o]], id, n - stored, condition, 0);
    for (int i = 2; i < k && stored < n; i++) stored += put_into(ga, &c[i], id, n - stored, condition, 0);
    return stored;
}

float ig_carried_kg(const GameActions *ga) { return bag_kg(&ga->pockets, ga->inv) + bag_kg(&ga->backpack, ga->inv); }

float ig_speed_scale(const GameActions *ga) {
    return ga->mounted >= 0 ? 1.0f : bag_speed_scale(ig_carried_kg(ga), CARRY_LIMIT);
}

float ig_stat(const GameActions *ga, Stat s) { return tg_stat(ga, s); }

int ig_store_var(GameActions *ga, const Props *props, const Player *p, const char *id, int n, float condition, unsigned short var) {
    Cont c[10];
    int k = containers(ga, props, p, c), stored = 0;
    static const int order[] = { 1, 0 };
    for (int o = 0; o < 2 && stored < n; o++) stored += put_into(ga, &c[order[o]], id, n - stored, condition, var);
    for (int i = 2; i < k && stored < n; i++) stored += put_into(ga, &c[i], id, n - stored, condition, var);
    return stored;
}

int ig_bags(GameActions *ga, const Props *props, const Player *p, Bag **out, int max) {
    Cont c[10];
    int k = containers(ga, props, p, c), n = 0;
    for (int i = 0; i < k && n < max; i++) out[n++] = c[i].type == C_BAG ? c[i].bag : &ga->armory;
    return n;
}

// ------------------------------------------------------------------ filas de un contenedor
typedef struct {
    const char *id;
    int count;
    float cond;
    int src; // 0 hueco de bolsa, 1 acopio, 2 armeria
    int idx;
    unsigned short var; // variante (joyas)
} Row;

static int rows_of(const GameActions *ga, const Cont *c, Row *rows, int max) {
    int n = 0;
    if (c->type == C_BAG) {
        for (int i = 0; i < c->bag->n && n < max; i++) rows[n++] = (Row){ c->bag->s[i].id, c->bag->s[i].count, c->bag->s[i].condition, 0, i, c->bag->s[i].var };
        return n;
    }
    for (int i = 0; i < ga->armory.n && n < max; i++)
        rows[n++] = (Row){ ga->armory.s[i].id, ga->armory.s[i].count, ga->armory.s[i].condition, 2, i, ga->armory.s[i].var };
    for (int i = 0; i < ga->stock.n && n < max; i++)
        if (ga->stock.e[i].count > 0) rows[n++] = (Row){ ga->stock.e[i].id, ga->stock.e[i].count, 1.0f, 1, i, 0 };
    return n;
}

static void remove_row(GameActions *ga, const Cont *c, const Row *r, int n) {
    if (r->src == 0) bag_remove_slot(c->bag, r->idx, n);
    else if (r->src == 2) bag_remove_slot(&ga->armory, r->idx, n);
    else stock_take(&ga->stock, r->id, n);
}

static int move_row(GameActions *ga, const Cont *from, const Row *r, const Cont *to, int n) {
    if (from->type == to->type && from->bag == to->bag) return 0;
    if (n > r->count) n = r->count;
    char id[INV_ID_LEN];
    snprintf(id, sizeof(id), "%s", r->id); // la fila apunta al hueco, que puede cambiar
    Row copy = *r;
    copy.id = id;
    int moved = put_into(ga, to, id, n, r->cond, r->var);
    if (moved > 0) remove_row(ga, from, &copy, moved);
    return moved;
}

static const char *item_name(const GameActions *ga, const char *id) {
    const InvItem *it = inventory_find(ga->inv, id);
    return it ? T(it->name) : id;
}

// ------------------------------------------------------------------ equipo
// Las piezas que se pueden poner en el hueco s (armadura) o en el hueco de joya -(s + 1).
static int candidates(GameActions *ga, const Props *props, const Player *p, int s, Cont *conts, int nc, Row *out, int *cont_of,
                      int max) {
    int n = 0;
    for (int c = 0; c < nc; c++) {
        Row rows[64];
        int k = rows_of(ga, &conts[c], rows, 64);
        for (int i = 0; i < k && n < max; i++) {
            bool fits = s >= 0 ? armor_slot_for(rows[i].id) == s : jewel_fits(rows[i].id, (JewelSlot)(-s - 1));
            if (!fits) continue;
            out[n] = rows[i];
            cont_of[n++] = c;
        }
    }
    (void)props, (void)p;
    return n;
}

// Deja una pieza quitada en la mochila (o donde quepa). false si no cabe en ningun lado.
static bool stash(GameActions *ga, const Props *props, const Player *p, const char *id, float cond) {
    return ig_store(ga, props, p, id, 1, cond) == 1;
}

static void equip_armor(GameActions *ga, Combat *cb, const Props *props, const Player *p, int slot, bool remove, char *log,
                        size_t len) {
    ArmorPiece *cur = &cb->armor.slot[slot];
    if (remove) {
        if (!cur->id[0]) return;
        char id[INV_ID_LEN];
        snprintf(id, sizeof(id), "%s", cur->id);
        if (!stash(ga, props, p, id, cur->durability / cur->durability_max)) {
            snprintf(log, len, "%s", T("No hay sitio donde guardar la pieza."));
            return;
        }
        armor_unequip(&cb->armor, (ArmorSlot)slot);
        snprintf(log, len, T("Te quitas: %s."), item_name(ga, id));
        return;
    }
    Cont conts[10];
    int nc = containers(ga, props, p, conts);
    Row cand[16];
    int cont_of[16];
    int n = candidates(ga, props, p, slot, conts, nc, cand, cont_of, 16);
    if (!n) {
        snprintf(log, len, T("No tienes a mano otra pieza para %s."), slot_name((ArmorSlot)slot));
        return;
    }
    // Enter recorre las piezas a mano: la primera que no sea la que llevas.
    int pick = 0;
    for (int i = 0; i < n; i++)
        if (strcmp(cand[i].id, cur->id) != 0) {
            pick = i;
            break;
        }
    char id[INV_ID_LEN];
    snprintf(id, sizeof(id), "%s", cand[pick].id);
    float cond = cand[pick].cond;
    remove_row(ga, &conts[cont_of[pick]], &cand[pick], 1);
    if (cur->id[0] && !stash(ga, props, p, cur->id, cur->durability / cur->durability_max)) {
        put_into(ga, &conts[cont_of[pick]], id, 1, cond, 0); // la devuelve: no hay donde dejar la vieja
        snprintf(log, len, "%s", T("No hay sitio donde dejar la pieza que llevas."));
        return;
    }
    armor_equip(&cb->armor, id);
    cur->durability = cur->durability_max * cond;
    snprintf(log, len, T("Te pones: %s (%d %%)."), item_name(ga, id), (int)(cond * 100.0f));
}

// Joyas: Enter pone (o cambia por la siguiente que haya a mano); quitar la guarda.
static void equip_jewel(GameActions *ga, const Props *props, const Player *p, int slot, bool remove, char *log, size_t len) {
    WornJewel *cur = &ga->jewels.slot[slot];
    Cont conts[10];
    int nc = containers(ga, props, p, conts);
    Row cand[16];
    int cont_of[16];
    int n = remove ? 0 : candidates(ga, props, p, -(slot + 1), conts, nc, cand, cont_of, 16);
    if (!remove && !n) {
        snprintf(log, len, T("No tienes a mano una joya para: %s."), jewel_slot_name((JewelSlot)slot));
        return;
    }
    if (cur->id[0]) { // quitar la que hay
        if (ig_store_var(ga, props, p, cur->id, 1, 1.0f, cur->var) != 1) {
            snprintf(log, len, "%s", T("No hay sitio donde guardar la joya."));
            return;
        }
        snprintf(log, len, T("Te quitas: %s."), item_name(ga, cur->id));
        memset(cur, 0, sizeof(*cur));
        if (remove) return;
        n = candidates(ga, props, p, -(slot + 1), conts, nc, cand, cont_of, 16); // las filas cambiaron
        if (!n) return;
    }
    char id[INV_ID_LEN];
    snprintf(id, sizeof(id), "%s", cand[0].id);
    unsigned short var = cand[0].var;
    remove_row(ga, &conts[cont_of[0]], &cand[0], 1);
    snprintf(cur->id, sizeof(cur->id), "%s", id);
    cur->var = var;
    char d[128];
    jewel_describe(id, var, d, sizeof(d));
    snprintf(log, len, T("Te pones: %s (%s)."), item_name(ga, id), d[0] ? d : T("sin efecto"));
}

// ------------------------------------------------------------------ reparaciones
const Ingredient *ig_first_missing(GameActions *ga, const Props *props, const Player *p, const Ingredient *mats) {
    for (const Ingredient *m = mats; m->id; m++)
        if (ig_count(ga, props, p, m->id) < m->count) return m;
    return NULL;
}

void ig_use_all(GameActions *ga, const Props *props, const Player *p, const Ingredient *mats) {
    for (const Ingredient *m = mats; m->id; m++) ig_use(ga, props, p, m->id, m->count);
}

static int material_of(const char *id) {
    Armor tmp;
    memset(&tmp, 0, sizeof(tmp));
    int s = armor_slot_for(id);
    if (s < 0 || !armor_equip(&tmp, id)) return 0;
    return (int)tmp.slot[s].material;
}

int ig_repair_list(GameActions *ga, const Props *props, const Player *p, RepairItem *out, int max) {
    int n = 0;
    if (ga->player_armor)
        for (int s = 0; s < SLOT_COUNT && n < max; s++) {
            const ArmorPiece *pc = &ga->player_armor->slot[s];
            if (!pc->id[0] || pc->durability >= pc->durability_max * 0.999f) continue;
            RepairItem *r = &out[n++];
            memset(r, 0, sizeof(*r));
            r->worn = true, r->slot = s, r->cond = pc->durability / pc->durability_max, r->material = (int)pc->material;
            snprintf(r->id, sizeof(r->id), "%s", pc->id);
        }
    Cont c[10];
    int nc = containers(ga, props, p, c);
    for (int k = 0; k < nc && n < max; k++) {
        Bag *b = c[k].type == C_BAG ? c[k].bag : &ga->armory;
        for (int i = 0; i < b->n && n < max; i++) {
            if (b->s[i].condition >= 0.999f || armor_slot_for(b->s[i].id) < 0) continue;
            RepairItem *r = &out[n++];
            memset(r, 0, sizeof(*r));
            r->bag = b, r->bag_slot = i, r->cond = b->s[i].condition, r->slot = armor_slot_for(b->s[i].id);
            r->material = material_of(b->s[i].id);
            snprintf(r->id, sizeof(r->id), "%s", b->s[i].id);
        }
    }
    return n;
}

static bool has_role(const Troop *t, Role r) {
    for (int i = 0; i < t->count; i++)
        if (t->members[i].status == STATUS_ACTIVE && t->members[i].role == r && !t->members[i].health.down) return true;
    return false;
}

static bool building_near(const Props *props, const char *id, const Player *p) {
    for (int i = 0; props && i < props->count; i++)
        if (!strcmp(props->items[i].item->id, id) && dist_xz(p->pos, props->items[i].pos.x, props->items[i].pos.z) < CAMP_STORE_RADIUS)
            return true;
    return false;
}

bool ig_can_repair(GameActions *ga, const Props *props, const Player *p, const Troop *troop, const RepairItem *it, char *why,
                   size_t len) {
    const RepairDef *d = repair_def(it->material);
    if (d->building && !building_near(props, d->building, p)) {
        snprintf(why, len, T("Hace falta: %s (cerca)."), item_name(ga, d->building));
        return false;
    }
    if (d->role != ROLE_NONE && !has_role(troop, d->role)) {
        snprintf(why, len, T("Hace falta un %s en la tribu."), T(role_name(d->role)));
        return false;
    }
    const Ingredient *miss = ig_first_missing(ga, props, p, d->mats);
    if (miss) {
        snprintf(why, len, T("Falta: %s (%d de %d)."), item_name(ga, miss->id), ig_count(ga, props, p, miss->id), miss->count);
        return false;
    }
    if (why && len) why[0] = '\0';
    return true;
}

bool ig_repair(GameActions *ga, const Props *props, const Player *p, const Troop *troop, const RepairItem *it, char *log,
               size_t len) {
    char why[128];
    if (!ig_can_repair(ga, props, p, troop, it, why, sizeof(why))) {
        snprintf(log, len, T("No se puede reparar: %s"), why);
        return false;
    }
    const RepairDef *d = repair_def(it->material);
    ig_use_all(ga, props, p, d->mats);
    float cond = fminf(1.0f, it->cond + d->restore);
    if (it->worn && ga->player_armor) {
        ArmorPiece *pc = &ga->player_armor->slot[it->slot];
        pc->durability = pc->durability_max * cond;
    } else if (it->bag) { // los materiales pueden haber movido los huecos: se busca la pieza otra vez
        for (int i = 0; i < it->bag->n; i++)
            if (!strcmp(it->bag->s[i].id, it->id) && fabsf(it->bag->s[i].condition - it->cond) < 1e-4f) {
                it->bag->s[i].condition = cond;
                break;
            }
    }
    snprintf(log, len, T("Reparas: %s (%d %% -> %d %%)."), item_name(ga, it->id), (int)(it->cond * 100), (int)(cond * 100));
    return true;
}

// ------------------------------------------------------------------ botin
#define LOOT_RANGE 2.5f
#define LOOT_SECONDS 900.0f // media jornada en el suelo

bool ig_drop_loot(GameActions *ga, Vector3 pos, const LootItem *items, int n) {
    if (n <= 0) return false;
    int k = -1;
    for (int i = 0; i < GA_LOOT && k < 0; i++)
        if (ga->loot_age[i] < 0.0f) k = i;
    if (k < 0) { // sin hueco: se pisa la bolsa mas vieja
        k = 0;
        for (int i = 1; i < GA_LOOT; i++)
            if (ga->loot_age[i] > ga->loot_age[k]) k = i;
    }
    bag_init(&ga->loot[k], BAG_CAMP, 0.0f);
    for (int i = 0; i < n; i++) bag_add(&ga->loot[k], ga->inv, items[i].id, items[i].count, items[i].condition);
    if (!ga->loot[k].n) return false;
    ga->loot_pos[k] = pos;
    ga->loot_age[k] = 0.0f;
    return true;
}

bool ig_take_loot(GameActions *ga, const Props *props, const Player *p, char *log, size_t len) {
    int k = -1;
    float bd = LOOT_RANGE;
    for (int i = 0; i < GA_LOOT; i++) {
        float d = ga->loot_age[i] >= 0.0f ? dist_xz(p->pos, ga->loot_pos[i].x, ga->loot_pos[i].z) : 1e9f;
        if (d < bd) bd = d, k = i;
    }
    if (k < 0) return false;
    Bag *b = &ga->loot[k];
    int taken = 0;
    char first[96] = "";
    for (int i = b->n - 1; i >= 0; i--) {
        BagSlot s = b->s[i];
        int got = ig_store(ga, props, p, s.id, s.count, s.condition);
        if (got > 0 && !first[0]) snprintf(first, sizeof(first), "%s", item_name(ga, s.id));
        taken += got;
        bag_remove_slot(b, i, got);
    }
    if (!b->n) ga->loot_age[k] = -1.0f;
    if (taken) snprintf(log, len, T("Recoges el botín: %s%s%s"), first, taken > 1 ? TextFormat(T(" y %d más"), taken - 1) : "",
                        b->n ? T(" (lo demás no te cabe).") : ".");
    else snprintf(log, len, "%s", T("No te cabe nada del botín (I: inventario)."));
    return true;
}

void ig_draw_world(const GameActions *ga, const Terrain *t, float time) {
    for (int i = 0; i < GA_LOOT; i++) {
        if (ga->loot_age[i] < 0.0f) continue;
        Vector3 p = ga->loot_pos[i];
        p.y = terrain_height(t, p.x, p.z);
        float bob = 0.04f * sinf(time * 3.0f + (float)i);
        DrawSphere((Vector3){ p.x, p.y + 0.18f, p.z }, 0.22f, (Color){ 120, 86, 52, 255 }); // el saco
        DrawCylinder((Vector3){ p.x, p.y + 0.36f, p.z }, 0.05f, 0.09f, 0.1f, 5, (Color){ 90, 62, 38, 255 });
        DrawCube((Vector3){ p.x, p.y + 0.62f + bob, p.z }, 0.1f, 0.1f, 0.1f, UI_GOLD); // una marca para verlo de lejos
    }
}

// ------------------------------------------------------------------ actualizacion
static void sync_packs(GameActions *ga, char *log, size_t len) {
    for (int k = 0; k < GA_PACKS; k++) { // las alforjas de una montura que ya no esta
        int a = ga->pack_animal[k];
        if (a < 0) continue;
        if (a >= ga->animal_count || !ga->animals[a].used || ga->animals[a].state != ANIMAL_SADDLED) {
            if (ga->packs[k].n) snprintf(log, len, "%s", T("Se perdieron las alforjas de una montura."));
            ga->pack_animal[k] = -1;
            ga->packs[k].n = 0;
        }
    }
    for (int i = 0; i < ga->animal_count; i++) { // una montura ensillada recibe sus alforjas
        if (!ga->animals[i].used || ga->animals[i].state != ANIMAL_SADDLED) continue;
        bool has = false;
        for (int k = 0; k < GA_PACKS; k++) has |= ga->pack_animal[k] == i;
        for (int k = 0; k < GA_PACKS && !has; k++)
            if (ga->pack_animal[k] < 0) {
                ga->pack_animal[k] = i;
                bag_init(&ga->packs[k], BAG_MOUNT, mount_capacity_kg(ga->animals[i].species));
                has = true;
            }
    }
}

// ------------------------------------------------------------------ disposicion (dibujo y raton)
#define PANE_W 300
#define PANE_H 296
#define PANE_Y 12
#define INV_COLS 8
#define INV_ROWS 6
#define INV_TILE 32
#define INV_GAP 3

static int pane_x(int k) { return 14 + k * (PANE_W + 12); }

static Rectangle cont_tab_rect(int k, int c) {
    return (Rectangle){ (float)(pane_x(k) + UI_PANEL_INSET + 2 + c * 28), (float)(PANE_Y + UI_PANEL_INSET), 24, 24 };
}

static int inv_first_row(int cur) {
    int row = cur / INV_COLS;
    return row >= INV_ROWS ? row - INV_ROWS + 1 : 0;
}

static Rectangle inv_tile_rect(int k, int i, int first_row) {
    int c = i % INV_COLS, r = i / INV_COLS - first_row;
    return (Rectangle){ (float)(pane_x(k) + UI_PANEL_INSET + 2 + c * (INV_TILE + INV_GAP)),
                        (float)(PANE_Y + UI_PANEL_INSET + 46 + r * (INV_TILE + INV_GAP)), INV_TILE, INV_TILE };
}

// Equipo, en tres pestañas: armadura (nueve huecos alrededor de la figura), joyas
// (cuatro anillos, dos brazaletes, collar, aretes y hebilla) y tatuajes (las ocho
// zonas del cuerpo y, a la derecha, el arbol de los cinco motivos).
#define FIG_X 170
#define EQUIP_TABS 3
static Rectangle equip_tab_rect(int t) { return (Rectangle){ (float)(26 + t * 32), 20, 28, 28 }; }

static Rectangle equip_rect(int i) {
    static const int pos[SLOT_COUNT][2] = {
        { -84, 56 }, { 52, 76 }, { 52, 116 }, { -84, 96 }, { -84, 136 }, { 52, 156 }, { -84, 176 }, { 52, 196 }, { -16, 234 },
    };
    return (Rectangle){ (float)(FIG_X + pos[i][0]), (float)pos[i][1], 32, 32 };
}

static Rectangle jewel_rect(int i) {
    static const int pos[JS_COUNT][2] = {
        { -84, 156 }, { -84, 196 }, { 52, 156 }, { 52, 196 }, { -84, 116 }, { 52, 116 }, { 52, 76 }, { -84, 56 }, { -16, 234 },
    };
    return (Rectangle){ (float)(FIG_X + pos[i][0]), (float)pos[i][1], 32, 32 };
}

static Rectangle zone_rect(int z) {
    static const int pos[TZ_COUNT][2] = { { -84, 56 }, { 52, 76 }, { 52, 116 }, { -84, 96 }, { -84, 136 }, { 52, 156 }, { -84, 176 }, { 52, 196 } };
    return (Rectangle){ (float)(FIG_X + pos[z][0]), (float)pos[z][1], 32, 32 };
}

// El arbol: una columna por motivo; filas: grado 1, grado 2, rama A y rama B del tercero.
static Rectangle node_rect(int i) {
    const TattooNode *n = tattoo_node(i);
    int row = n->tier == 3 ? 1 + n->branch : n->tier - 1;
    return (Rectangle){ (float)(338 + n->motif * 54), (float)(70 + row * 46), 32, 32 };
}

static int equip_count(const GameActions *ga) { return ga->equip_tab == 0 ? SLOT_COUNT : ga->equip_tab == 1 ? JS_COUNT : TATTOO_NODES; }
static int *equip_cur(GameActions *ga) { return ga->equip_tab == 0 ? &ga->equip_cursor : ga->equip_tab == 1 ? &ga->jewel_cursor : &ga->tattoo_cursor; }
static Rectangle equip_item_rect(const GameActions *ga, int i) {
    return ga->equip_tab == 0 ? equip_rect(i) : ga->equip_tab == 1 ? jewel_rect(i) : node_rect(i);
}

void ig_update(GameActions *ga, Combat *cb, Props *props, const Player *p, bool input_ok, char *log, size_t len) {
    sync_packs(ga, log, len);
    for (int i = 0; i < GA_LOOT; i++) // el botin no se queda para siempre
        if (ga->loot_age[i] >= 0.0f && (ga->loot_age[i] += GetFrameTime()) > LOOT_SECONDS) ga->loot_age[i] = -1.0f;
    if (input_ok && IsKeyPressed(KEY_I) && !ga->equip_open) ga->inv_open = !ga->inv_open;
    if (input_ok && IsKeyPressed(KEY_P) && !ga->inv_open) ga->equip_open = !ga->equip_open;
    if ((ga->inv_open || ga->equip_open) && IsKeyPressed(KEY_ESCAPE)) ga->inv_open = ga->equip_open = false;
    Cont conts[10];
    int nc = containers(ga, props, p, conts);
    if (ga->inv_open) {
        int *pane = &ga->inv_pane;
        for (int k = 0; k < 2; k++) { // si un contenedor quedo lejos, el ultimo a mano
            if (ga->inv_cont[k] >= nc) ga->inv_cont[k] = k ? nc - 1 : 0;
            for (int c = 0; c < nc; c++)
                if (ui_click(cont_tab_rect(k, c))) ga->inv_cont[k] = c, ga->inv_cursor[k] = 0, *pane = k;
        }
        int *cont = &ga->inv_cont[*pane];
        // Q/E: contenedor de la columna activa.
        if (IsKeyPressed(KEY_Q)) *cont = (*cont + nc - 1) % nc, ga->inv_cursor[*pane] = 0;
        if (IsKeyPressed(KEY_E)) *cont = (*cont + 1) % nc, ga->inv_cursor[*pane] = 0;
        Row rows[64];
        int nr = rows_of(ga, &conts[*cont], rows, 64);
        int *cur = &ga->inv_cursor[*pane];
        if (*cur >= nr) *cur = nr ? nr - 1 : 0;
        // Flechas (o WASD) por la cuadricula; por el borde se pasa a la otra columna.
        bool right = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D), left = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A);
        if (right && (nr == 0 || *cur % INV_COLS == INV_COLS - 1 || *cur == nr - 1) && *pane == 0) *pane = 1;
        else if (left && (nr == 0 || *cur % INV_COLS == 0) && *pane == 1) *pane = 0;
        else if (right && *cur + 1 < nr) (*cur)++;
        else if (left && *cur > 0) (*cur)--;
        if ((IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) && *cur + INV_COLS < nr) *cur += INV_COLS;
        if ((IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) && *cur >= INV_COLS) *cur -= INV_COLS;
        bool all = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool move = IsKeyPressed(KEY_ENTER);
        for (int k = 0; k < 2; k++) { // raton: pasar elige, un clic sobre lo elegido lo pasa
            Row rk[64];
            int nk = rows_of(ga, &conts[ga->inv_cont[k]], rk, 64), first = inv_first_row(ga->inv_cursor[k]);
            for (int i = first * INV_COLS; i < nk && i < (first + INV_ROWS) * INV_COLS; i++) {
                Rectangle r = inv_tile_rect(k, i, first);
                if (ui_pointer_moved() && ui_hover(r)) *pane = k, ga->inv_cursor[k] = i;
                if (ui_click(r)) {
                    if (*pane == k && ga->inv_cursor[k] == i) move = true;
                    *pane = k, ga->inv_cursor[k] = i;
                }
            }
        }
        cont = &ga->inv_cont[*pane];
        cur = &ga->inv_cursor[*pane];
        nr = rows_of(ga, &conts[*cont], rows, 64);
        if (move && nr && *cur < nr) {
            const Cont *to = &conts[ga->inv_cont[1 - *pane]];
            int moved = move_row(ga, &conts[*cont], &rows[*cur], to, all ? rows[*cur].count : 1);
            if (moved) snprintf(log, len, "%d x %s a: %s.", moved, item_name(ga, rows[*cur].id), to->label);
            else snprintf(log, len, T("No cabe en %s (peso, talla o huecos)."), to->label);
        }
    }
    if (ga->equip_open) {
        // Pestañas: Q/E o clic.
        if (IsKeyPressed(KEY_E)) ga->equip_tab = (ga->equip_tab + 1) % EQUIP_TABS;
        if (IsKeyPressed(KEY_Q)) ga->equip_tab = (ga->equip_tab + EQUIP_TABS - 1) % EQUIP_TABS;
        for (int t = 0; t < EQUIP_TABS; t++)
            if (ui_click(equip_tab_rect(t))) ga->equip_tab = t;
        const int total = equip_count(ga);
        int *cur = equip_cur(ga);
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_D)) *cur = (*cur + 1) % total;
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_A)) *cur = (*cur + total - 1) % total;
        if (*cur >= total) *cur = 0;
        bool put = IsKeyPressed(KEY_ENTER), take = IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE);
        for (int i = 0; i < total; i++) { // raton: pasar elige; clic sobre lo elegido cambia; clic derecho quita
            Rectangle r = equip_item_rect(ga, i);
            if (ui_pointer_moved() && ui_hover(r)) *cur = i;
            if (ui_click(r)) {
                if (*cur == i) put = true;
                *cur = i;
            }
            if (ui_hover(r) && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) *cur = i, take = true;
        }
        if (put || take) { // los tatuajes no se ponen ni se quitan aqui: los hace el druida, para siempre
            if (ga->equip_tab == 0) equip_armor(ga, cb, props, p, *cur, take, log, len);
            else if (ga->equip_tab == 1) equip_jewel(ga, props, p, *cur, take, log, len);
            else snprintf(log, len, "%s", take ? T("Un tatuaje no se puede quitar.") : T("Los tatuajes los hace un druida: háblale (F)."));
        }
    }
}

// ------------------------------------------------------------------ dibujo
static Color mat_color(const ArmorPiece *pc) {
    static const Color mats[MAT_COUNT] = {
        [MAT_FELT] = { 150, 128, 98, 255 },  [MAT_LEATHER] = { 140, 96, 58, 255 }, [MAT_BRONZE] = { 192, 140, 70, 255 },
        [MAT_IRON] = { 128, 130, 136, 255 }, [MAT_STEEL] = { 182, 188, 198, 255 }, [MAT_GOLD] = { 222, 182, 64, 255 },
    };
    return pc->id[0] ? mats[pc->material] : (Color){ 60, 50, 42, 255 };
}

static void draw_pane(const GameActions *ga, const Cont *conts, int nc, int k) {
    bool active = ga->inv_pane == k;
    int x = pane_x(k), y = PANE_Y;
    int c = ga->inv_cont[k] < nc ? ga->inv_cont[k] : 0, cursor = ga->inv_cursor[k];
    const Cont *ct = &conts[c];
    ui_panel((Rectangle){ (float)x, (float)y, PANE_W, PANE_H }, active ? UI_METAL_GOLD : UI_METAL_SILVER);
    // Contenedores a mano: un icono cada uno (Q/E o clic).
    for (int i = 0; i < nc; i++) {
        Rectangle r = cont_tab_rect(k, i);
        if (ui_tile(r, conts[i].icon, i == c, true)) ui_legend(conts[i].label, T("Q / E o clic: cambiar de contenedor"));
    }
    int ix = x + UI_PANEL_INSET + 2, iw = PANE_W - 2 * UI_PANEL_INSET - 4, hy = y + UI_PANEL_INSET + 30;
    if (ct->type == C_BAG) { // peso y huecos
        float kg = bag_kg(ct->bag, ga->inv);
        bool full = kg > ct->bag->cap_kg * 0.9f;
        ui_icon(ICON_PESO, (float)ix, (float)hy - 4, 16, full ? UI_CARNELIAN : UI_GOLD);
        ui_bar(ix + 20, hy + 2, iw - 92, kg / ct->bag->cap_kg, UI_TURQUOISE, UI_METAL_SILVER);
        const char *cap = TextFormat("%.1f/%.0f", kg, ct->bag->cap_kg);
        ui_text(cap, ix + iw - MeasureText(cap, 10), hy, 10, full ? UI_CARNELIAN : UI_BONE_DIM);
        if (ui_hover((Rectangle){ (float)ix, (float)hy - 4, (float)iw, 14 }))
            ui_legend(ct->label, TextFormat(T("%.1f de %.0f kg · %d de %d huecos"), kg, ct->bag->cap_kg, ct->bag->n, ct->bag->slots));
    } else {
        ui_text(ct->label, ix, hy, 10, UI_BONE_DIM);
    }
    Row rows[64];
    int n = rows_of(ga, ct, rows, 64);
    if (!n) ui_icon(ICON_BOTIN, (float)ix + 4, (float)(y + UI_PANEL_INSET + 52), 32, (Color){ 120, 108, 92, 140 });
    int first = inv_first_row(cursor);
    for (int i = first * INV_COLS; i < n && i < (first + INV_ROWS) * INV_COLS; i++) {
        Rectangle r = inv_tile_rect(k, i, first);
        bool sel = active && i == cursor;
        bool hover = ui_tile(r, icon_for_item(rows[i].id), sel, true);
        if (rows[i].count > 1) ui_tile_badge(r, TextFormat("%d", rows[i].count), UI_BONE);
        bool gear = item_is_gear(rows[i].id);
        if (gear && rows[i].cond < 0.999f) ui_tile_bar(r, rows[i].cond, rows[i].cond < 0.3f ? UI_CARNELIAN : UI_GOLD);
        if (hover || sel) {
            const InvItem *it = inventory_find(ga->inv, rows[i].id);
            const char *d = TextFormat(T("%d · %.1f kg%s · Enter o clic: pasar (Mayús: todo)"), rows[i].count, item_kg(it) * (float)rows[i].count,
                                       gear ? TextFormat(T(" · estado %d %%"), (int)(rows[i].cond * 100)) : "");
            if (hover) ui_legend(it ? T(it->name) : rows[i].id, d);
            else ui_legend_default(it ? T(it->name) : rows[i].id, d);
        }
    }
    if (n > (first + INV_ROWS) * INV_COLS || first > 0) ui_text(TextFormat("%d/%d", cursor + 1, n), ix, y + PANE_H - UI_PANEL_INSET - 10, 10, UI_BONE_DIM);
}

static void draw_inventory(const GameActions *ga, const Props *props, const Player *p, int w, int h) {
    Cont conts[10];
    int nc = containers((GameActions *)ga, props, p, conts);
    DrawRectangle(0, 0, w, h, (Color){ 10, 7, 5, 150 });
    for (int k = 0; k < 2; k++) draw_pane(ga, conts, nc, k);
    // Entre las columnas: hacia donde pasa lo elegido.
    ui_icon_ex(ICON_CONTINUAR, (float)(pane_x(1) - 12), (float)(PANE_Y + PANE_H / 2 - 8), 16, UI_TURQUOISE, ga->inv_pane == 1);
    // Lo que llevas encima (frena si pesa).
    float kg = ig_carried_kg(ga);
    bool heavy = kg > CARRY_LIMIT;
    Rectangle wr = { 14, (float)(PANE_Y + PANE_H + 4), 90, 18 };
    ui_icon(ICON_PERSONA, wr.x, wr.y + 1, 16, UI_GOLD);
    ui_text(TextFormat(T("%.1f kg"), kg), (int)wr.x + 20, (int)wr.y + 5, 10, heavy ? UI_CARNELIAN : UI_BONE);
    if (heavy) ui_icon(ICON_VELOCIDAD, wr.x + 70, wr.y + 1, 16, UI_CARNELIAN);
    if (ui_hover(wr)) ui_legend(T("Llevas encima"), heavy ? TextFormat(T("%.1f kg: pesado, andas más lento (más de %.0f kg)"), kg, CARRY_LIMIT)
                                                    : TextFormat(T("%.1f kg (hasta %.0f sin frenarte)"), kg, CARRY_LIMIT));
    ui_legend_default(T("Inventario"), T("Flechas: elegir · Q/E: contenedor · Enter: pasar · I: cerrar"));
}

static IconId motif_icon(Motif m) {
    static const IconId I[MOTIF_COUNT] = { ICON_ANIMAL, ICON_VELOCIDAD, ICON_EXPLORADOR, ICON_TATUAJE, ICON_AGUA };
    return m >= 0 && m < MOTIF_COUNT ? I[m] : ICON_TATUAJE;
}

static IconId zone_icon(TattooZone z) {
    static const IconId I[TZ_COUNT] = { ICON_CASCO, ICON_CUELLO, ICON_TORSO, ICON_MOCHILA, ICON_BRAZALES, ICON_SABLE, ICON_GUANTES, ICON_GREBAS };
    return z >= 0 && z < TZ_COUNT ? I[z] : ICON_TATUAJE;
}

static IconId jewel_slot_icon(int s) {
    JewelType t = slot_type((JewelSlot)s);
    return t == JT_RING ? ICON_ANILLO : t == JT_BRACELET ? ICON_BRAZALETE : t == JT_NECKLACE ? ICON_COLLAR : t == JT_EARRINGS ? ICON_ARETE : ICON_HEBILLA;
}

static void draw_jewel_tab(const GameActions *ga, int w, int h) {
    (void)h;
    for (int i = 0; i < JS_COUNT; i++) {
        const WornJewel *j = &ga->jewels.slot[i];
        Rectangle r = jewel_rect(i);
        bool sel = i == ga->jewel_cursor;
        bool hover = ui_tile(r, j->id[0] ? icon_for_item(j->id) : jewel_slot_icon(i), sel, j->id[0] != '\0');
        Gem g = jewel_gem(j->var);
        if (j->id[0] && g != GEM_NONE) DrawCircle((int)r.x + 5, (int)r.y + 5, 2, jewel_enchant(j->var) ? UI_TURQUOISE : UI_BONE_DIM);
        if (hover || sel) {
            char d[160] = "";
            if (j->id[0]) jewel_describe(j->id, j->var, d, sizeof(d));
            const char *title = j->id[0] ? item_name(ga, j->id) : jewel_slot_name((JewelSlot)i);
            const char *det = j->id[0] ? TextFormat(T("%s · Supr: quitar"), d) : T("vacío · Enter: ponerte una joya que tengas a mano");
            if (hover) ui_legend(title, det);
            else ui_legend_default(title, det);
        }
    }
    // Detalle: icono grande, la piedra, el encantamiento y lo que da.
    const int dx = 330, dw = w - dx - 26;
    const WornJewel *j = &ga->jewels.slot[ga->jewel_cursor];
    Rectangle big = { (float)dx, 60, 68, 68 };
    ui_tile(big, j->id[0] ? icon_for_item(j->id) : jewel_slot_icon(ga->jewel_cursor), true, j->id[0] != '\0');
    int tx = dx + 78;
    if (!j->id[0]) {
        ui_text(jewel_slot_name((JewelSlot)ga->jewel_cursor), tx, 62, 10, UI_BONE_DIM);
    } else {
        ui_text_wrapped(item_name(ga, j->id), tx, 62, dw - 78, 10, UI_GOLD_LIGHT);
        Gem g = jewel_gem(j->var);
        if (g != GEM_NONE) {
            ui_icon(ICON_GEMA, (float)tx, 84, 16, UI_TURQ_LIGHT);
            ui_text(gem_name(g), tx + 20, 88, 10, UI_BONE);
        }
        char d[160];
        jewel_describe(j->id, j->var, d, sizeof(d));
        ui_text_wrapped(d, dx, 140, dw, 10, UI_TURQUOISE);
        if (jewel_parse(j->id, NULL, NULL) && jewel_enchant(j->var) == ENCH_NONE)
            ui_text_wrapped(T("Sin encantar no hace nada: llévasela a un druida."), dx, 170, dw, 10, UI_CARNELIAN);
    }
    // Suma de todas las joyas.
    int y = 206, x = dx;
    ui_divider(dx, y - 8, dw, UI_METAL_GOLD);
    for (int s = 0; s < STAT_COUNT; s++) {
        float v = jewelry_stat(&ga->jewels, (Stat)s);
        if (v == 0.0f) continue;
        const char *t = TextFormat("%s %+d%%", stat_name((Stat)s), (int)(v * 100.0f + 0.5f));
        if (x + MeasureText(t, 10) > dx + dw) x = dx, y += 12;
        ui_text(t, x, y, 10, UI_BONE);
        x += MeasureText(t, 10) + 12;
    }
}

static void draw_tattoo_tab(const GameActions *ga, int w, int h) {
    (void)h;
    // El cuerpo: lo tatuado en cada zona (para siempre).
    for (int z = 0; z < TZ_COUNT; z++) {
        Rectangle r = zone_rect(z);
        int node = ga->tattoos.node[z];
        bool hover = ui_tile(r, node >= 0 ? motif_icon(tattoo_node(node)->motif) : zone_icon((TattooZone)z), false, node >= 0);
        if (node >= 0) ui_tile_badge(r, tattoo_node(node)->tier == 3 ? "III" : tattoo_node(node)->tier == 2 ? "II" : "I", UI_TURQ_LIGHT);
        if (hover) {
            char d[160] = "";
            if (node >= 0) tattoo_describe(node, (TattooZone)z, d, sizeof(d));
            char zn[48];
            snprintf(zn, sizeof(zn), "%s", zone_name((TattooZone)z));
            if (zn[0] >= 'a' && zn[0] <= 'z') zn[0] = (char)(zn[0] - 'a' + 'A');
            ui_legend(node >= 0 ? TextFormat("%s: %s", zn, T(tattoo_node(node)->name)) : zn, node >= 0 ? d : T("sin tatuar"));
        }
    }
    // Nivel y experiencia.
    const int dx = 330, dw = w - dx - 26;
    ui_icon(ICON_NIVEL, (float)dx, 24, 16, UI_GOLD_LIGHT);
    ui_text(TextFormat(T("Nivel %d"), ga->prog.level), dx + 20, 28, 10, UI_GOLD_LIGHT);
    ui_bar(dx + 80, 30, dw - 80, ga->prog.level >= LEVEL_MAX ? 1.0f : ga->prog.xp / xp_to_next(ga->prog.level), UI_TURQUOISE, UI_METAL_GOLD);
    // El arbol: una columna por motivo.
    for (int m = 0; m < MOTIF_COUNT; m++) {
        int cx = 338 + m * 54;
        ui_icon(motif_icon((Motif)m), (float)cx + 8, 48, 16, UI_BONE_DIM);
        DrawLine(cx + 16, 102, cx + 16, 116, UI_GOLD_DARK);
        DrawLine(cx + 16, 148, cx + 16, 162, UI_GOLD_DARK);
    }
    for (int i = 0; i < TATTOO_NODES; i++) {
        const TattooNode *n = tattoo_node(i);
        Rectangle r = node_rect(i);
        bool have = tattoo_has(&ga->tattoos, i);
        TattooCheck c = have ? TAT_OK : tattoo_can(&ga->tattoos, i, TZ_COUNT, ga->prog.level);
        // tattoo_can con una zona invalida contesta "zona ocupada": miramos lo demas aparte.
        bool open = !have && ga->prog.level >= n->level;
        for (int k = 0; k < TATTOO_NODES && open; k++) {
            const TattooNode *o = tattoo_node(k);
            if (o->motif == n->motif && o->tier == 3 && n->tier == 3 && o->branch != n->branch && tattoo_has(&ga->tattoos, k)) open = false;
        }
        if (open && n->tier > 1) {
            bool prev = false;
            for (int k = 0; k < TATTOO_NODES; k++)
                if (tattoo_node(k)->motif == n->motif && tattoo_node(k)->tier == n->tier - 1 && tattoo_has(&ga->tattoos, k)) prev = true;
            open = prev;
        }
        (void)c;
        bool sel = i == ga->tattoo_cursor;
        bool hover = ui_tile(r, motif_icon(n->motif), sel || have, open || have);
        if (have) DrawRectangleLinesEx((Rectangle){ r.x - 2, r.y - 2, r.width + 4, r.height + 4 }, 1, UI_TURQUOISE);
        if (!open && !have) ui_icon(ICON_BLOQUEADO, r.x + r.width - 12, r.y + r.height - 12, 16, UI_BONE_DIM);
        if (n->active != ABIL_NONE) DrawCircle((int)r.x + 5, (int)r.y + 5, 2, UI_CARNELIAN); // habilidad activa
        if (hover || sel) {
            const char *det = TextFormat(T("%s · nivel %d%s"), T(n->desc), n->level, have ? T(" · tatuado") : open ? T(" · pídeselo a un druida") : "");
            if (hover) ui_legend(T(n->name), det);
            else ui_legend_default(T(n->name), det);
        }
    }
    ui_text_wrapped(T("Un tatuaje es para siempre: la zona del cuerpo cambia a qué va el bonus. El tercer grado se bifurca: elegir una rama cierra la otra."),
                    dx, 262, dw, 10, UI_BONE_DIM);
}

static void draw_equipment(const GameActions *ga, const Combat *cb, int w, int h) {
    DrawRectangle(0, 0, w, h, (Color){ 10, 7, 5, 150 });
    ui_panel((Rectangle){ 10, 10, (float)(w - 20), (float)(h - 34) }, UI_METAL_GOLD);
    const ArmorPiece *s = cb->armor.slot;
    // La figura: un guerrero esquematico con sus piezas coloreadas por material.
    const int fx = FIG_X, fy = 68;
    DrawCircle(fx, fy + 12, 11, mat_color(&s[SLOT_HELMET]));
    DrawRectangle(fx - 5, fy + 23, 10, 7, mat_color(&s[SLOT_NECK]));
    DrawRectangle(fx - 18, fy + 30, 36, 42, mat_color(&s[SLOT_TORSO]));
    DrawRectangle(fx - 30, fy + 30, 12, 22, mat_color(&s[SLOT_SHOULDERS]));
    DrawRectangle(fx + 18, fy + 30, 12, 22, mat_color(&s[SLOT_SHOULDERS]));
    DrawRectangle(fx - 32, fy + 52, 10, 22, mat_color(&s[SLOT_BRACERS]));
    DrawRectangle(fx + 22, fy + 52, 10, 22, mat_color(&s[SLOT_BRACERS]));
    DrawRectangle(fx - 33, fy + 74, 12, 8, mat_color(&s[SLOT_GLOVES]));
    DrawRectangle(fx + 21, fy + 74, 12, 8, mat_color(&s[SLOT_GLOVES]));
    DrawRectangle(fx - 18, fy + 72, 36, 16, mat_color(&s[SLOT_SKIRT]));
    DrawRectangle(fx - 16, fy + 88, 13, 60, mat_color(&s[SLOT_GREAVES]));
    DrawRectangle(fx + 3, fy + 88, 13, 60, mat_color(&s[SLOT_GREAVES]));
    DrawRectangle(fx - 17, fy + 148, 15, 9, mat_color(&s[SLOT_BOOTS]));
    DrawRectangle(fx + 2, fy + 148, 15, 9, mat_color(&s[SLOT_BOOTS]));
    // Pestañas: armadura, joyas, tatuajes.
    static const IconId TAB_ICON[EQUIP_TABS] = { ICON_TORSO, ICON_ANILLO, ICON_TATUAJE };
    static const char *TAB_NAME[EQUIP_TABS] = { N_("Armadura"), N_("Joyas"), N_("Tatuajes") };
    for (int t = 0; t < EQUIP_TABS; t++)
        if (ui_tile(equip_tab_rect(t), TAB_ICON[t], t == ga->equip_tab, true)) ui_legend(T(TAB_NAME[t]), T("Q / E cambia de pestaña"));
    ui_text(T(TAB_NAME[ga->equip_tab]), 26 + EQUIP_TABS * 32 + 6, 26, 20, UI_GOLD_LIGHT);
    if (ga->equip_tab == 1) {
        draw_jewel_tab(ga, w, h);
        return;
    }
    if (ga->equip_tab == 2) {
        draw_tattoo_tab(ga, w, h);
        return;
    }
    // Los huecos: el icono del hueco; puesto, teñido del material.
    for (int i = 0; i < SLOT_COUNT; i++) {
        Rectangle r = equip_rect(i);
        bool sel = i == ga->equip_cursor, hover;
        if (i < SLOT_COUNT) {
            const ArmorPiece *pc = &s[i];
            hover = ui_tile(r, icon_for_slot(i), sel, pc->id[0] != '\0');
            if (pc->id[0]) {
                DrawRectangle((int)r.x + 2, (int)r.y + 2, 4, 4, mat_color(pc)); // el material, en la esquina
                float f = pc->durability / pc->durability_max;
                ui_tile_bar(r, f, f < 0.3f ? UI_CARNELIAN : UI_GOLD);
            }
            if (hover || sel) {
                const InvItem *it = pc->id[0] ? inventory_find(ga->inv, pc->id) : NULL;
                char sn[32];
                snprintf(sn, sizeof(sn), "%s", slot_name((ArmorSlot)i));
                if (sn[0] >= 'a' && sn[0] <= 'z') sn[0] = (char)(sn[0] - 'a' + 'A');
                const char *title = it ? T(it->name) : sn;
                const char *d = pc->id[0] ? TextFormat(T("%s · Enter: cambiar · Supr o clic derecho: quitar"), sn) : T("vacío · Enter: ponerte una pieza a mano");
                if (hover) ui_legend(title, d);
                else ui_legend_default(title, d);
            }
        }
    }
    // Detalle del hueco elegido: icono grande y cifras con iconos.
    const int dx = 330, dw = w - dx - 26;
    int dy = 28;
    Rectangle big = { (float)dx, (float)dy, 68, 68 };
    int tx = dx + 78;
    {
        const ArmorPiece *pc = &s[ga->equip_cursor];
        ui_tile(big, icon_for_slot(ga->equip_cursor), true, pc->id[0] != '\0');
        if (pc->id[0]) {
            const MaterialDef *m = material_def(pc->material);
            const InvItem *it = inventory_find(ga->inv, pc->id);
            ui_text_wrapped(it ? T(it->name) : pc->id, tx, dy + 2, dw - 78, 10, UI_GOLD_LIGHT);
            ui_text(T(m->name), tx, dy + 28, 10, mat_color(pc));
            float f = pc->durability / pc->durability_max;
            ui_bar(tx, dy + 46, dw - 78, f, f < 0.3f ? UI_CARNELIAN : UI_GOLD, UI_METAL_SILVER);
            struct { IconId icon; int pct; const char *name; } st[4] = {
                { ICON_CORTE, (int)(m->vs_cut * 100), T("Contra el corte") },
                { ICON_GOLPE, (int)(m->vs_blunt * 100), T("Contra el golpe") },
                { ICON_PUNTA, (int)(m->vs_pierce * 100 * (pc->mail ? 0.7f : 1.0f)), T("Contra flechas y puntas") },
                { ICON_COBERTURA, (int)(pc->coverage * 100), T("Cubre su zona") },
            };
            for (int k = 0; k < 4; k++) {
                int cx = dx + (k % 2) * 130, cy = dy + 80 + (k / 2) * 22;
                ui_icon(st[k].icon, (float)cx, (float)cy, 16, UI_GOLD);
                ui_text(TextFormat("%d %%", st[k].pct), cx + 20, cy + 4, 10, UI_BONE);
                if (ui_hover((Rectangle){ (float)cx, (float)cy, 120, 18 })) ui_legend(st[k].name, TextFormat("%d %%", st[k].pct));
            }
        } else {
            ui_text(T("Vacío"), tx, dy + 2, 10, UI_BONE_DIM);
        }
    }
    // Salud y heridas, con iconos.
    const Health *ph = &cb->player;
    int hy = 160;
    ui_divider(dx, hy - 8, dw, UI_METAL_GOLD);
    int cx = dx;
    ui_icon(ICON_VIDA, (float)cx, (float)hy, 16, UI_CARNELIAN);
    ui_text(TextFormat("%d/%d", (int)fmaxf(0, ph->hp), (int)ph->hp_max), cx + 20, hy + 4, 10, UI_BONE);
    if (ui_hover((Rectangle){ (float)cx, (float)hy, 80, 18 })) ui_legend(T("Vida"), health_state_name(ph));
    cx += 86;
    ui_icon(ICON_SANGRE, (float)cx, (float)hy, 16, health_bleeding(ph) ? UI_CARNELIAN : UI_GOLD);
    ui_text(TextFormat("%d %%", (int)(ph->blood * 100)), cx + 20, hy + 4, 10, health_bleeding(ph) ? UI_CARNELIAN : UI_BONE);
    if (ui_hover((Rectangle){ (float)cx, (float)hy, 70, 18 })) ui_legend(T("Sangre"), health_bleeding(ph) ? T("sangras: véndate (B)") : T("no sangras"));
    cx += 76;
    if (ph->venom >= 1.0f) {
        ui_icon(ICON_VENENO, (float)cx, (float)hy, 16, UI_CARNELIAN);
        if (ui_hover((Rectangle){ (float)cx, (float)hy, 18, 18 })) ui_legend(T("Envenenado"), T("el ungüento corta el veneno (B)"));
        cx += 24;
    }
    float speed = armor_speed_scale(&cb->armor);
    ui_icon(ICON_VELOCIDAD, (float)cx, (float)hy, 16, UI_BONE_DIM);
    ui_text(TextFormat("-%d %%", (int)((1.0f - speed) * 100.0f + 0.5f)), cx + 20, hy + 4, 10, UI_BONE_DIM);
    if (ui_hover((Rectangle){ (float)cx, (float)hy, 70, 18 })) ui_legend(T("La armadura frena"), TextFormat(T("un %d %%"), (int)((1.0f - speed) * 100.0f + 0.5f)));
    for (int i = 0; i < ph->wound_count && i < 6; i++) { // las heridas: un icono por herida; el detalle, al pasar
        Rectangle r = { (float)(dx + i * 22), (float)(hy + 26), 18, 18 };
        DrawRectangleRec(r, (Color){ 70, 24, 18, 236 });
        ui_icon(ICON_SANGRE, r.x + 1, r.y + 1, 16, ph->wounds[i].treated ? UI_BONE_DIM : UI_CARNELIAN);
        if (ui_hover(r)) {
            char d[96];
            wound_describe(&ph->wounds[i], false, d, sizeof(d));
            ui_legend(T("Herida"), d);
        }
    }
}

void ig_draw(const GameActions *ga, const Combat *cb, const Props *props, const Player *p, int w, int h) {
    if (ga->inv_open) draw_inventory(ga, props, p, w, h);
    if (ga->equip_open) draw_equipment(ga, cb, w, h);
}
