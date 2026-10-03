#include "game/inventory_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
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
    out[n++] = (Cont){ C_BAG, &ga->pockets, "Bolsillos" };
    out[n++] = (Cont){ C_BAG, &ga->backpack, "Mochila" };
    for (int k = 0; k < GA_PACKS; k++) {
        if (!pack_near(ga, k, p)) continue;
        out[n] = (Cont){ C_BAG, &ga->packs[k], "" };
        snprintf(out[n].label, sizeof(out[n].label), "Alforjas (%s)", species_def(ga->animals[ga->pack_animal[k]].species)->name);
        n++;
    }
    if (near_cart(props, p)) out[n++] = (Cont){ C_BAG, &ga->cart, "Carreta de la tribu" };
    if (near_camp(p)) out[n++] = (Cont){ C_CAMP, NULL, "Acopio y armería del campamento" };
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

void ig_update(GameActions *ga, Combat *cb, Props *props, const Player *p, bool input_ok, char *log, size_t len) {
    sync_packs(ga, log, len);
    if (input_ok && IsKeyPressed(KEY_I) && !ga->equip_open) ga->inv_open = !ga->inv_open;
    if (input_ok && IsKeyPressed(KEY_P) && !ga->inv_open) ga->equip_open = !ga->equip_open;
    if ((ga->inv_open || ga->equip_open) && IsKeyPressed(KEY_ESCAPE)) ga->inv_open = ga->equip_open = false;
    Cont conts[10];
    int nc = containers(ga, props, p, conts);
    if (ga->inv_open) {
        int *pane = &ga->inv_pane;
        if (IsKeyPressed(KEY_LEFT)) *pane = 0;
        if (IsKeyPressed(KEY_RIGHT)) *pane = 1;
        int *cont = &ga->inv_cont[*pane];
        if (IsKeyPressed(KEY_A)) *cont = (*cont + nc - 1) % nc, ga->inv_cursor[*pane] = 0;
        if (IsKeyPressed(KEY_D)) *cont = (*cont + 1) % nc, ga->inv_cursor[*pane] = 0;
        for (int k = 0; k < 2; k++) // si un contenedor quedo lejos, el ultimo a mano
            if (ga->inv_cont[k] >= nc) ga->inv_cont[k] = k ? nc - 1 : 0;
        Row rows[64];
        int nr = rows_of(ga, &conts[*cont], rows, 64);
        int *cur = &ga->inv_cursor[*pane];
        if (IsKeyPressed(KEY_DOWN) && nr) *cur = (*cur + 1) % nr;
        if (IsKeyPressed(KEY_UP) && nr) *cur = (*cur + nr - 1) % nr;
        if (*cur >= nr) *cur = nr ? nr - 1 : 0;
        if (IsKeyPressed(KEY_ENTER) && nr) {
            const Cont *to = &conts[ga->inv_cont[1 - *pane]];
            bool all = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            int moved = move_row(ga, &conts[*cont], &rows[*cur], to, all ? rows[*cur].count : 1);
            if (moved) snprintf(log, len, "%d x %s a: %s.", moved, item_name(ga, rows[*cur].id), to->label);
            else snprintf(log, len, "No cabe en %s (peso, talla o huecos).", to->label);
        }
    }
    if (ga->equip_open) {
        const int total = SLOT_COUNT + AMULET_SLOTS;
        if (IsKeyPressed(KEY_DOWN)) ga->equip_cursor = (ga->equip_cursor + 1) % total;
        if (IsKeyPressed(KEY_UP)) ga->equip_cursor = (ga->equip_cursor + total - 1) % total;
        bool put = IsKeyPressed(KEY_ENTER), take = IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE);
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

static void draw_cont(const GameActions *ga, const Cont *c, int x, int y, int w, int h, int cursor, bool active) {
    ui_panel((Rectangle){ (float)x, (float)y, (float)w, (float)h }, active ? UI_METAL_GOLD : UI_METAL_SILVER);
    int ix = x + UI_PANEL_INSET + 2, iw = w - 2 * UI_PANEL_INSET - 4;
    ui_text(c->label, ix, y + UI_PANEL_INSET, 10, active ? UI_GOLD_LIGHT : UI_BONE);
    if (c->type == C_BAG) {
        float kg = bag_kg(c->bag, ga->inv);
        const char *cap = TextFormat("%.1f / %.0f kg · %d/%d", kg, c->bag->cap_kg, c->bag->n, c->bag->slots);
        ui_text(cap, ix + iw - MeasureText(cap, 10), y + UI_PANEL_INSET, 10, kg > c->bag->cap_kg * 0.9f ? UI_CARNELIAN : UI_BONE_DIM);
        ui_bar(ix, y + UI_PANEL_INSET + 13, iw, kg / c->bag->cap_kg, UI_TURQUOISE, UI_METAL_SILVER);
    }
    Row rows[64];
    int n = rows_of(ga, c, rows, 64);
    const int row_h = 11, top = y + UI_PANEL_INSET + 24, visible = (h - 2 * UI_PANEL_INSET - 26) / row_h;
    int first = cursor - visible / 2;
    if (first > n - visible) first = n - visible;
    if (first < 0) first = 0;
    if (!n) ui_text("(vacío)", ix, top, 10, UI_BONE_DIM);
    for (int i = first; i < n && i < first + visible; i++) {
        int ry = top + (i - first) * row_h;
        bool sel = active && i == cursor;
        if (sel) DrawRectangle(ix - 2, ry - 1, iw + 4, row_h, Fade(UI_TURQ_DARK, 0.9f));
        const InvItem *it = inventory_find(ga->inv, rows[i].id);
        ui_text(TextFormat("%3d  %s", rows[i].count, it ? it->name : rows[i].id), ix, ry, 10, sel ? UI_BONE : UI_BONE_DIM);
        if (item_is_gear(rows[i].id) && rows[i].cond < 0.999f) { // estado de la pieza
            DrawRectangle(ix + iw - 30, ry + 3, 28, 4, UI_LEATHER_CRACK);
            DrawRectangle(ix + iw - 30, ry + 3, (int)(28 * rows[i].cond), 4, rows[i].cond < 0.3f ? UI_CARNELIAN : UI_GOLD);
        } else {
            const char *kg = TextFormat("%.1f", item_kg(it) * (float)rows[i].count);
            ui_text(kg, ix + iw - MeasureText(kg, 10), ry, 10, UI_BONE_DIM);
        }
    }
}

static void draw_inventory(const GameActions *ga, const Props *props, const Player *p, int w, int h) {
    Cont conts[10];
    int nc = containers((GameActions *)ga, props, p, conts);
    DrawRectangle(0, 0, w, h, (Color){ 10, 7, 5, 150 });
    const int pw = 300, ph = h - 70, y = 16;
    for (int k = 0; k < 2; k++) {
        int c = ga->inv_cont[k] < nc ? ga->inv_cont[k] : 0;
        draw_cont(ga, &conts[c], 14 + k * (pw + 12), y, pw, ph, ga->inv_cursor[k], ga->inv_pane == k);
    }
    // Detalle de lo elegido y teclas.
    int c = ga->inv_cont[ga->inv_pane] < nc ? ga->inv_cont[ga->inv_pane] : 0;
    Row rows[64];
    int nr = rows_of(ga, &conts[c], rows, 64), cur = ga->inv_cursor[ga->inv_pane];
    ui_strip((Rectangle){ 0, (float)(h - 48), (float)w, 48 }, UI_METAL_GOLD);
    if (nr && cur < nr) {
        const InvItem *it = inventory_find(ga->inv, rows[cur].id);
        ui_text(TextFormat("%s · %.2f kg c/u%s", it ? it->name : rows[cur].id, item_kg(it),
                           item_is_gear(rows[cur].id) ? TextFormat(" · estado %d %%", (int)(rows[cur].cond * 100)) : ""),
                10, h - 40, 10, UI_GOLD_LIGHT);
    }
    ui_text(TextFormat("Llevas encima %.1f kg%s", ig_carried_kg(ga), ig_carried_kg(ga) > CARRY_LIMIT ? " (pesado: andas más lento)" : ""),
            10, h - 27, 10, ig_carried_kg(ga) > CARRY_LIMIT ? UI_CARNELIAN : UI_BONE_DIM);
    const char *k2 = "Izq/Der columna · A/D contenedor · Arriba/Abajo · Enter pasar 1 · Mayús+Enter todos · I cerrar";
    ui_text(k2, w - 10 - MeasureText(k2, 10), h - 14, 10, UI_BONE_DIM);
}

static void draw_equipment(const GameActions *ga, const Combat *cb, int w, int h) {
    DrawRectangle(0, 0, w, h, (Color){ 10, 7, 5, 150 });
    ui_panel((Rectangle){ 10, 10, (float)(w - 20), (float)(h - 20) }, UI_METAL_GOLD);
    ui_text("Equipo", 26, 24, 20, UI_GOLD_LIGHT);
    ui_text("Arriba/Abajo elegir · Enter poner o cambiar · Supr quitar · P cerrar", 120, 30, 10, UI_BONE_DIM);
    // La figura: un guerrero esquematico con sus piezas coloreadas por material.
    const int fx = 150, fy = 70;
    const ArmorPiece *s = cb->armor.slot;
    DrawCircle(fx, fy + 12, 11, mat_color(&s[SLOT_HELMET]));                                     // cabeza
    DrawRectangle(fx - 5, fy + 23, 10, 7, mat_color(&s[SLOT_NECK]));                              // cuello
    DrawRectangle(fx - 18, fy + 30, 36, 42, mat_color(&s[SLOT_TORSO]));                           // torso
    DrawRectangle(fx - 30, fy + 30, 12, 22, mat_color(&s[SLOT_SHOULDERS]));                       // hombros
    DrawRectangle(fx + 18, fy + 30, 12, 22, mat_color(&s[SLOT_SHOULDERS]));
    DrawRectangle(fx - 32, fy + 52, 10, 22, mat_color(&s[SLOT_BRACERS]));                         // antebrazos
    DrawRectangle(fx + 22, fy + 52, 10, 22, mat_color(&s[SLOT_BRACERS]));
    DrawRectangle(fx - 33, fy + 74, 12, 8, mat_color(&s[SLOT_GLOVES]));                           // manos
    DrawRectangle(fx + 21, fy + 74, 12, 8, mat_color(&s[SLOT_GLOVES]));
    DrawRectangle(fx - 18, fy + 72, 36, 16, mat_color(&s[SLOT_SKIRT]));                           // faldar
    DrawRectangle(fx - 16, fy + 88, 13, 32, mat_color(&s[SLOT_GREAVES]));                         // piernas
    DrawRectangle(fx + 3, fy + 88, 13, 32, mat_color(&s[SLOT_GREAVES]));
    DrawRectangle(fx - 17, fy + 120, 15, 9, mat_color(&s[SLOT_BOOTS]));                           // pies
    DrawRectangle(fx + 2, fy + 120, 15, 9, mat_color(&s[SLOT_BOOTS]));
    // Lista de huecos.
    int lx = 250, ly = 60;
    for (int i = 0; i < SLOT_COUNT + AMULET_SLOTS; i++) {
        int ry = ly + i * 14 + (i >= SLOT_COUNT ? 8 : 0);
        bool sel = i == ga->equip_cursor;
        if (i == SLOT_COUNT) ui_divider(lx, ry - 7, 340, UI_METAL_GOLD);
        if (sel) DrawRectangle(lx - 4, ry - 2, 344, 13, Fade(UI_TURQ_DARK, 0.9f));
        if (i < SLOT_COUNT) {
            const ArmorPiece *pc = &s[i];
            const InvItem *it = pc->id[0] ? inventory_find(ga->inv, pc->id) : NULL;
            char sn[32];
            snprintf(sn, sizeof(sn), "%s", slot_name((ArmorSlot)i));
            if (sn[0] >= 'a' && sn[0] <= 'z') sn[0] = (char)(sn[0] - 'a' + 'A');
            ui_text(sn, lx, ry, 10, sel ? UI_GOLD_LIGHT : UI_GOLD);
            ui_text(it ? it->name : (pc->id[0] ? pc->id : "(nada)"), lx + 80, ry, 10, sel ? UI_BONE : UI_BONE_DIM);
            if (pc->id[0]) {
                float f = pc->durability / pc->durability_max;
                DrawRectangle(lx + 300, ry + 3, 36, 5, UI_LEATHER_CRACK);
                DrawRectangle(lx + 300, ry + 3, (int)(36 * f), 5, f < 0.3f ? UI_CARNELIAN : mat_color(pc));
            }
        } else {
            const char *id = ga->amulet_id[i - SLOT_COUNT];
            ui_text(TextFormat("Amuleto %d", i - SLOT_COUNT + 1), lx, ry, 10, sel ? UI_GOLD_LIGHT : UI_GOLD);
            ui_text(id[0] ? item_name(ga, id) : "(nada)", lx + 80, ry, 10, sel ? UI_BONE : UI_BONE_DIM);
        }
    }
    // Detalle del hueco elegido.
    int dy = ly + (SLOT_COUNT + AMULET_SLOTS) * 14 + 16;
    ui_divider(26, dy - 6, w - 52, UI_METAL_GOLD);
    if (ga->equip_cursor < SLOT_COUNT) {
        const ArmorPiece *pc = &s[ga->equip_cursor];
        if (pc->id[0]) {
            const MaterialDef *m = material_def(pc->material);
            ui_text(TextFormat("%s · contra corte %d %% · golpe %d %% · flechas %d %% · cubre %d %% · estado %d %%",
                               material_def(pc->material)->name, (int)(m->vs_cut * 100), (int)(m->vs_blunt * 100),
                               (int)(m->vs_pierce * 100 * (pc->mail ? 0.7f : 1.0f)), (int)(pc->coverage * 100),
                               (int)(100.0f * pc->durability / pc->durability_max)),
                    26, dy, 10, UI_BONE);
        } else {
            ui_text("Hueco vacío: Enter para ponerte una pieza que tengas a mano.", 26, dy, 10, UI_BONE_DIM);
        }
    } else {
        Charm c;
        const char *id = ga->amulet_id[ga->equip_cursor - SLOT_COUNT];
        char d[96] = "";
        if (id[0] && amulet_charm(id, &c)) charm_describe(&c, d, sizeof(d));
        ui_text(id[0] ? TextFormat("Efecto: %s", d) : "Sin amuleto: Enter para colgarte uno de los que lleves.", 26, dy, 10,
                id[0] ? UI_TURQUOISE : UI_BONE_DIM);
    }
    // Heridas.
    const Health *ph = &cb->player;
    ui_text(TextFormat("%s · vida %d/%d · sangre %d %%%s", health_state_name(ph), (int)fmaxf(0, ph->hp), (int)ph->hp_max,
                       (int)(ph->blood * 100), ph->venom >= 1.0f ? " · veneno" : ""),
            26, dy + 16, 10, health_bleeding(ph) ? UI_CARNELIAN : UI_BONE);
    for (int i = 0; i < ph->wound_count && i < 4; i++) {
        char d[96];
        wound_describe(&ph->wounds[i], false, d, sizeof(d));
        ui_text(d, 26 + (i % 2) * 290, dy + 30 + (i / 2) * 12, 10, UI_BONE_DIM);
    }
    // Totales.
    float speed = armor_speed_scale(&cb->armor);
    ui_text(TextFormat("La armadura frena un %d %%", (int)((1.0f - speed) * 100.0f + 0.5f)), 60, fy + 140, 10, UI_BONE_DIM);
}

void ig_draw(const GameActions *ga, const Combat *cb, const Props *props, const Player *p, int w, int h) {
    if (ga->inv_open) draw_inventory(ga, props, p, w, h);
    if (ga->equip_open) draw_equipment(ga, cb, w, h);
}
