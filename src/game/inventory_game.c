#include "game/inventory_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "ui/icons.h"
#include "ui/theme.h"

#define CART_ID "vehiculo.tierra.carreta_bueyes"
#define CART_RANGE 5.0f  // m: para cargar la carreta
#define PACK_RANGE 8.0f  // m: para usar las alforjas de una montura
#define CARRY_LIMIT 20.0f // kg: con mas encima se anda mas lento

static float dist_xz(Vector3 a, float x, float z) { return sqrtf((a.x - x) * (a.x - x) + (a.z - z) * (a.z - z)); }

void ig_init(GameActions *ga, Props *props, const Terrain *t) {
    bag_init(&ga->pockets, BAG_POCKETS, 0.0f);
    bag_init(&ga->backpack, BAG_BACKPACK, 0.0f);
    bag_init(&ga->cart, BAG_CART, 0.0f);
    bag_init(&ga->armory, BAG_CAMP, 0.0f);
    for (int i = 0; i < GA_PACKS; i++) ga->pack_animal[i] = -1;
    for (int i = 0; i < GA_LOOT; i++) ga->loot_age[i] = -1.0f;
    loadout_init(&ga->loadout);
    memset(ga->amulet_id, 0, sizeof(ga->amulet_id));
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

bool ig_blocks_input(const GameActions *ga) { return ga->inv_open || ga->equip_open; }

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
    out[n++] = (Cont){ C_BAG, &ga->pockets, "Bolsillos", ICON_BOLSILLO };
    out[n++] = (Cont){ C_BAG, &ga->backpack, "Mochila", ICON_MOCHILA };
    for (int k = 0; k < GA_PACKS; k++) {
        if (!pack_near(ga, k, p)) continue;
        out[n] = (Cont){ C_BAG, &ga->packs[k], "", ICON_ALFORJAS };
        snprintf(out[n].label, sizeof(out[n].label), "Alforjas (%s)", species_def(ga->animals[ga->pack_animal[k]].species)->name);
        n++;
    }
    if (near_cart(props, p)) out[n++] = (Cont){ C_BAG, &ga->cart, "Carreta de la tribu", ICON_CARRETA };
    if (near_camp(p)) out[n++] = (Cont){ C_CAMP, NULL, "Acopio y armería del campamento", ICON_CAMPAMENTO };
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

static int put_into(GameActions *ga, const Cont *to, const char *id, int n, float cond) {
    if (to->type == C_CAMP) {
        if (item_is_gear(id) || !strncmp(id, "accesorio.", 10)) return bag_add(&ga->armory, ga->inv, id, n, cond);
        stock_add(&ga->stock, id, n);
        return n;
    }
    return bag_add(to->bag, ga->inv, id, n, cond);
}

int ig_store(GameActions *ga, const Props *props, const Player *p, const char *id, int n, float condition) {
    Cont c[10];
    int k = containers(ga, props, p, c), stored = 0;
    // Primero la mochila, luego los bolsillos y lo demas.
    static const int order[] = { 1, 0 };
    for (int o = 0; o < 2 && stored < n; o++) stored += put_into(ga, &c[order[o]], id, n - stored, condition);
    for (int i = 2; i < k && stored < n; i++) stored += put_into(ga, &c[i], id, n - stored, condition);
    return stored;
}

float ig_carried_kg(const GameActions *ga) { return bag_kg(&ga->pockets, ga->inv) + bag_kg(&ga->backpack, ga->inv); }

float ig_speed_scale(const GameActions *ga) {
    return ga->mounted >= 0 ? 1.0f : bag_speed_scale(ig_carried_kg(ga), CARRY_LIMIT);
}

float ig_stat(const GameActions *ga, Stat s) { return loadout_stat(&ga->loadout, s); }

// ------------------------------------------------------------------ filas de un contenedor
typedef struct {
    const char *id;
    int count;
    float cond;
    int src; // 0 hueco de bolsa, 1 acopio, 2 armeria
    int idx;
} Row;

static int rows_of(const GameActions *ga, const Cont *c, Row *rows, int max) {
    int n = 0;
    if (c->type == C_BAG) {
        for (int i = 0; i < c->bag->n && n < max; i++) rows[n++] = (Row){ c->bag->s[i].id, c->bag->s[i].count, c->bag->s[i].condition, 0, i };
        return n;
    }
    for (int i = 0; i < ga->armory.n && n < max; i++)
        rows[n++] = (Row){ ga->armory.s[i].id, ga->armory.s[i].count, ga->armory.s[i].condition, 2, i };
    for (int i = 0; i < ga->stock.n && n < max; i++)
        if (ga->stock.e[i].count > 0) rows[n++] = (Row){ ga->stock.e[i].id, ga->stock.e[i].count, 1.0f, 1, i };
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
    int moved = put_into(ga, to, id, n, r->cond);
    if (moved > 0) remove_row(ga, from, &copy, moved);
    return moved;
}

static const char *item_name(const GameActions *ga, const char *id) {
    const InvItem *it = inventory_find(ga->inv, id);
    return it ? it->name : id;
}

// ------------------------------------------------------------------ equipo
// Las piezas que se pueden poner en el hueco s (armadura) o como amuleto (s < 0).
static int candidates(GameActions *ga, const Props *props, const Player *p, int s, Cont *conts, int nc, Row *out, int *cont_of,
                      int max) {
    int n = 0;
    for (int c = 0; c < nc; c++) {
        Row rows[64];
        int k = rows_of(ga, &conts[c], rows, 64);
        for (int i = 0; i < k && n < max; i++) {
            bool fits = s >= 0 ? armor_slot_for(rows[i].id) == s : !strncmp(rows[i].id, "accesorio.amuleto.", 18);
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
            snprintf(log, len, "No hay sitio donde guardar la pieza.");
            return;
        }
        armor_unequip(&cb->armor, (ArmorSlot)slot);
        snprintf(log, len, "Te quitas: %s.", item_name(ga, id));
        return;
    }
    Cont conts[10];
    int nc = containers(ga, props, p, conts);
    Row cand[16];
    int cont_of[16];
    int n = candidates(ga, props, p, slot, conts, nc, cand, cont_of, 16);
    if (!n) {
        snprintf(log, len, "No tienes a mano otra pieza para %s.", slot_name((ArmorSlot)slot));
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
        put_into(ga, &conts[cont_of[pick]], id, 1, cond); // la devuelve: no hay donde dejar la vieja
        snprintf(log, len, "No hay sitio donde dejar la pieza que llevas.");
        return;
    }
    armor_equip(&cb->armor, id);
    cur->durability = cur->durability_max * cond;
    snprintf(log, len, "Te pones: %s (%d %%).", item_name(ga, id), (int)(cond * 100.0f));
}

static void equip_amulet(GameActions *ga, const Props *props, const Player *p, int slot, bool remove, char *log, size_t len) {
    char *cur = ga->amulet_id[slot];
    if (remove || cur[0]) { // quitar el que hay (y, si no era quitar, poner otro despues)
        if (cur[0]) {
            if (!stash(ga, props, p, cur, 1.0f)) {
                snprintf(log, len, "No hay sitio donde guardar el amuleto.");
                return;
            }
            snprintf(log, len, "Te quitas: %s.", item_name(ga, cur));
            loadout_unequip_amulet(&ga->loadout, slot);
            cur[0] = '\0';
        }
        if (remove) return;
    }
    Cont conts[10];
    int nc = containers(ga, props, p, conts);
    Row cand[16];
    int cont_of[16];
    int n = candidates(ga, props, p, -1, conts, nc, cand, cont_of, 16);
    for (int i = 0; i < n; i++) {
        bool worn = false;
        for (int k = 0; k < AMULET_SLOTS; k++) worn |= !strcmp(ga->amulet_id[k], cand[i].id);
        if (worn) continue;
        Charm c;
        if (!amulet_charm(cand[i].id, &c)) continue;
        snprintf(cur, INV_ID_LEN, "%s", cand[i].id);
        remove_row(ga, &conts[cont_of[i]], &cand[i], 1);
        loadout_equip_amulet(&ga->loadout, slot, &c);
        char d[96];
        charm_describe(&c, d, sizeof(d));
        snprintf(log, len, "Te cuelgas: %s (%s).", item_name(ga, cur), d);
        return;
    }
    if (!remove) snprintf(log, len, "No tienes a mano otro amuleto.");
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
        snprintf(why, len, "Hace falta: %s (cerca).", item_name(ga, d->building));
        return false;
    }
    if (d->role != ROLE_NONE && !has_role(troop, d->role)) {
        snprintf(why, len, "Hace falta un %s en la tribu.", role_name(d->role));
        return false;
    }
    const Ingredient *miss = ig_first_missing(ga, props, p, d->mats);
    if (miss) {
        snprintf(why, len, "Falta: %s (%d de %d).", item_name(ga, miss->id), ig_count(ga, props, p, miss->id), miss->count);
        return false;
    }
    if (why && len) why[0] = '\0';
    return true;
}

bool ig_repair(GameActions *ga, const Props *props, const Player *p, const Troop *troop, const RepairItem *it, char *log,
               size_t len) {
    char why[128];
    if (!ig_can_repair(ga, props, p, troop, it, why, sizeof(why))) {
        snprintf(log, len, "No se puede reparar: %s", why);
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
    snprintf(log, len, "Reparas: %s (%d %% -> %d %%).", item_name(ga, it->id), (int)(it->cond * 100), (int)(cond * 100));
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
    if (taken) snprintf(log, len, "Recoges el botín: %s%s%s", first, taken > 1 ? TextFormat(" y %d más", taken - 1) : "",
                        b->n ? " (lo demás no te cabe)." : ".");
    else snprintf(log, len, "No te cabe nada del botín (I: inventario).");
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
            if (ga->packs[k].n) snprintf(log, len, "Se perdieron las alforjas de una montura.");
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

// Equipo: los nueve huecos alrededor de la figura y los tres amuletos debajo.
#define FIG_X 170
static Rectangle equip_rect(int i) {
    static const int pos[SLOT_COUNT][2] = {
        { -84, 44 }, { 52, 64 }, { 52, 104 }, { -84, 84 }, { -84, 124 }, { 52, 144 }, { -84, 164 }, { 52, 184 }, { -16, 222 },
    };
    if (i < SLOT_COUNT) return (Rectangle){ (float)(FIG_X + pos[i][0]), (float)pos[i][1], 32, 32 };
    return (Rectangle){ (float)(FIG_X - 58 + (i - SLOT_COUNT) * 42), 278, 32, 32 };
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
            else snprintf(log, len, "No cabe en %s (peso, talla o huecos).", to->label);
        }
    }
    if (ga->equip_open) {
        const int total = SLOT_COUNT + AMULET_SLOTS;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_D))
            ga->equip_cursor = (ga->equip_cursor + 1) % total;
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_A))
            ga->equip_cursor = (ga->equip_cursor + total - 1) % total;
        bool put = IsKeyPressed(KEY_ENTER), take = IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE);
        for (int i = 0; i < total; i++) { // raton: pasar elige; clic sobre lo elegido cambia; clic derecho quita
            Rectangle r = equip_rect(i);
            if (ui_pointer_moved() && ui_hover(r)) ga->equip_cursor = i;
            if (ui_click(r)) {
                if (ga->equip_cursor == i) put = true;
                ga->equip_cursor = i;
            }
            if (ui_hover(r) && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) ga->equip_cursor = i, take = true;
        }
        if (put || take) {
            if (ga->equip_cursor < SLOT_COUNT) equip_armor(ga, cb, props, p, ga->equip_cursor, take, log, len);
            else equip_amulet(ga, props, p, ga->equip_cursor - SLOT_COUNT, take, log, len);
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
        if (ui_tile(r, conts[i].icon, i == c, true)) ui_legend(conts[i].label, "Q / E o clic: cambiar de contenedor");
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
            ui_legend(ct->label, TextFormat("%.1f de %.0f kg · %d de %d huecos", kg, ct->bag->cap_kg, ct->bag->n, ct->bag->slots));
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
            const char *d = TextFormat("%d · %.1f kg%s · Enter o clic: pasar (Mayús: todo)", rows[i].count, item_kg(it) * (float)rows[i].count,
                                       gear ? TextFormat(" · estado %d %%", (int)(rows[i].cond * 100)) : "");
            if (hover) ui_legend(it ? it->name : rows[i].id, d);
            else ui_legend_default(it ? it->name : rows[i].id, d);
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
    ui_text(TextFormat("%.1f kg", kg), (int)wr.x + 20, (int)wr.y + 5, 10, heavy ? UI_CARNELIAN : UI_BONE);
    if (heavy) ui_icon(ICON_VELOCIDAD, wr.x + 70, wr.y + 1, 16, UI_CARNELIAN);
    if (ui_hover(wr)) ui_legend("Llevas encima", heavy ? TextFormat("%.1f kg: pesado, andas más lento (más de %.0f kg)", kg, CARRY_LIMIT)
                                                    : TextFormat("%.1f kg (hasta %.0f sin frenarte)", kg, CARRY_LIMIT));
    ui_legend_default("Inventario", "Flechas: elegir · Q/E: contenedor · Enter: pasar · I: cerrar");
}

static void draw_equipment(const GameActions *ga, const Combat *cb, int w, int h) {
    DrawRectangle(0, 0, w, h, (Color){ 10, 7, 5, 150 });
    ui_panel((Rectangle){ 10, 10, (float)(w - 20), (float)(h - 34) }, UI_METAL_GOLD);
    const ArmorPiece *s = cb->armor.slot;
    // La figura: un guerrero esquematico con sus piezas coloreadas por material.
    const int fx = FIG_X, fy = 56;
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
    ui_divider(fx - 70, 266, 140, UI_METAL_GOLD);
    // Los huecos: el icono del hueco; puesto, teñido del material.
    for (int i = 0; i < SLOT_COUNT + AMULET_SLOTS; i++) {
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
                const char *title = it ? it->name : sn;
                const char *d = pc->id[0] ? TextFormat("%s · Enter: cambiar · Supr o clic derecho: quitar", sn) : "vacío · Enter: ponerte una pieza a mano";
                if (hover) ui_legend(title, d);
                else ui_legend_default(title, d);
            }
        } else {
            const char *id = ga->amulet_id[i - SLOT_COUNT];
            hover = ui_tile(r, id[0] ? icon_for_item(id) : ICON_AMULETO, sel, id[0] != '\0');
            if (hover || sel) {
                Charm c;
                char d[96] = "";
                if (id[0] && amulet_charm(id, &c)) charm_describe(&c, d, sizeof(d));
                const char *title = id[0] ? item_name(ga, id) : TextFormat("Amuleto %d", i - SLOT_COUNT + 1);
                const char *det = id[0] ? d : "vacío · Enter: colgarte uno de los que lleves";
                if (hover) ui_legend(title, det);
                else ui_legend_default(title, det);
            }
        }
    }
    // Detalle del hueco elegido: icono grande y cifras con iconos.
    const int dx = 330, dw = w - dx - 26;
    int dy = 28;
    Rectangle big = { (float)dx, (float)dy, 68, 68 };
    int tx = dx + 78;
    if (ga->equip_cursor < SLOT_COUNT) {
        const ArmorPiece *pc = &s[ga->equip_cursor];
        ui_tile(big, icon_for_slot(ga->equip_cursor), true, pc->id[0] != '\0');
        if (pc->id[0]) {
            const MaterialDef *m = material_def(pc->material);
            const InvItem *it = inventory_find(ga->inv, pc->id);
            ui_text_wrapped(it ? it->name : pc->id, tx, dy + 2, dw - 78, 10, UI_GOLD_LIGHT);
            ui_text(m->name, tx, dy + 28, 10, mat_color(pc));
            float f = pc->durability / pc->durability_max;
            ui_bar(tx, dy + 46, dw - 78, f, f < 0.3f ? UI_CARNELIAN : UI_GOLD, UI_METAL_SILVER);
            struct { IconId icon; int pct; const char *name; } st[4] = {
                { ICON_CORTE, (int)(m->vs_cut * 100), "Contra el corte" },
                { ICON_GOLPE, (int)(m->vs_blunt * 100), "Contra el golpe" },
                { ICON_PUNTA, (int)(m->vs_pierce * 100 * (pc->mail ? 0.7f : 1.0f)), "Contra flechas y puntas" },
                { ICON_COBERTURA, (int)(pc->coverage * 100), "Cubre su zona" },
            };
            for (int k = 0; k < 4; k++) {
                int cx = dx + (k % 2) * 130, cy = dy + 80 + (k / 2) * 22;
                ui_icon(st[k].icon, (float)cx, (float)cy, 16, UI_GOLD);
                ui_text(TextFormat("%d %%", st[k].pct), cx + 20, cy + 4, 10, UI_BONE);
                if (ui_hover((Rectangle){ (float)cx, (float)cy, 120, 18 })) ui_legend(st[k].name, TextFormat("%d %%", st[k].pct));
            }
        } else {
            ui_text("Vacío", tx, dy + 2, 10, UI_BONE_DIM);
        }
    } else {
        Charm c;
        const char *id = ga->amulet_id[ga->equip_cursor - SLOT_COUNT];
        ui_tile(big, id[0] ? icon_for_item(id) : ICON_AMULETO, true, id[0] != '\0');
        if (id[0] && amulet_charm(id, &c)) {
            char d[96];
            charm_describe(&c, d, sizeof(d));
            ui_text_wrapped(item_name(ga, id), tx, dy + 2, dw - 78, 10, UI_GOLD_LIGHT);
            ui_text_wrapped(d, dx, dy + 80, dw, 10, UI_TURQUOISE);
        } else {
            ui_text("Vacío", tx, dy + 2, 10, UI_BONE_DIM);
        }
    }
    // Salud y heridas, con iconos.
    const Health *ph = &cb->player;
    int hy = 160;
    ui_divider(dx, hy - 8, dw, UI_METAL_GOLD);
    int cx = dx;
    ui_icon(ICON_VIDA, (float)cx, (float)hy, 16, UI_CARNELIAN);
    ui_text(TextFormat("%d/%d", (int)fmaxf(0, ph->hp), (int)ph->hp_max), cx + 20, hy + 4, 10, UI_BONE);
    if (ui_hover((Rectangle){ (float)cx, (float)hy, 80, 18 })) ui_legend("Vida", health_state_name(ph));
    cx += 86;
    ui_icon(ICON_SANGRE, (float)cx, (float)hy, 16, health_bleeding(ph) ? UI_CARNELIAN : UI_GOLD);
    ui_text(TextFormat("%d %%", (int)(ph->blood * 100)), cx + 20, hy + 4, 10, health_bleeding(ph) ? UI_CARNELIAN : UI_BONE);
    if (ui_hover((Rectangle){ (float)cx, (float)hy, 70, 18 })) ui_legend("Sangre", health_bleeding(ph) ? "sangras: véndate (B)" : "no sangras");
    cx += 76;
    if (ph->venom >= 1.0f) {
        ui_icon(ICON_VENENO, (float)cx, (float)hy, 16, UI_CARNELIAN);
        if (ui_hover((Rectangle){ (float)cx, (float)hy, 18, 18 })) ui_legend("Envenenado", "el ungüento corta el veneno (B)");
        cx += 24;
    }
    float speed = armor_speed_scale(&cb->armor);
    ui_icon(ICON_VELOCIDAD, (float)cx, (float)hy, 16, UI_BONE_DIM);
    ui_text(TextFormat("-%d %%", (int)((1.0f - speed) * 100.0f + 0.5f)), cx + 20, hy + 4, 10, UI_BONE_DIM);
    if (ui_hover((Rectangle){ (float)cx, (float)hy, 70, 18 })) ui_legend("La armadura frena", TextFormat("un %d %%", (int)((1.0f - speed) * 100.0f + 0.5f)));
    for (int i = 0; i < ph->wound_count && i < 6; i++) { // las heridas: un icono por herida; el detalle, al pasar
        Rectangle r = { (float)(dx + i * 22), (float)(hy + 26), 18, 18 };
        DrawRectangleRec(r, (Color){ 70, 24, 18, 236 });
        ui_icon(ICON_SANGRE, r.x + 1, r.y + 1, 16, ph->wounds[i].treated ? UI_BONE_DIM : UI_CARNELIAN);
        if (ui_hover(r)) {
            char d[96];
            wound_describe(&ph->wounds[i], false, d, sizeof(d));
            ui_legend("Herida", d);
        }
    }
}

void ig_draw(const GameActions *ga, const Combat *cb, const Props *props, const Player *p, int w, int h) {
    if (ga->inv_open) draw_inventory(ga, props, p, w, h);
    if (ga->equip_open) draw_equipment(ga, cb, w, h);
}
