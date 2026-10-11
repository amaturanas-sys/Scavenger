#include "game/hud_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/combat_game.h"
#include "game/input.h"
#include "game/inventory_game.h"
#include "game/talents_game.h"
#include "game/player.h"
#include "game/water_game.h"
#include "sim/lang.h"
#include "ui/theme.h"

#define VITAL_BAR 80   // px de cada barra de la placa
#define QUICK_TILE 26  // px de cada casilla de la barra rapida
#define COLUMN_TILE 22 // px de cada casilla de la columna
#define COLUMN_GAP 1
#define TILE_GAP 3

// Lo que ocupo el HUD en el cuadro anterior: un clic (o un toque, que llega sin pasar antes por
// encima) que cae ahi es del HUD y no un golpe.
#define HUD_RECTS 48
static Rectangle g_rects[2][HUD_RECTS];
static int g_nrects[2], g_cur;

void hud_frame_begin(void) {
    g_cur ^= 1;
    g_nrects[g_cur] = 0;
}

bool hud_pointer_over(void) {
    const int prev = g_cur ^ 1;
    for (int i = 0; i < g_nrects[prev]; i++)
        if (ui_hover(g_rects[prev][i])) return true;
    return false;
}

static bool hover(Rectangle r) {
    if (g_nrects[g_cur] < HUD_RECTS) g_rects[g_cur][g_nrects[g_cur]++] = r;
    return ui_hover(r);
}

bool hud_hover(Rectangle r) { return hover(r); }

// ------------------------------------------------------------------ placas
// Una placa de cuero oscuro con borde de oro y granulado en las esquinas, como las placas de
// cinturon de los kurganes.
static void plate(int x, int y, int w, int h) {
    hover((Rectangle){ (float)x, (float)y, (float)w, (float)h }); // entre casillas tampoco es un golpe
    DrawRectangle(x, y, w, h, (Color){ 20, 14, 10, 170 });
    DrawRectangleLines(x, y, w, h, Fade(UI_GOLD_DARK, 0.9f));
    ui_granule(x + 2, y + 2, UI_METAL_GOLD);
    ui_granule(x + w - 3, y + 2, UI_METAL_GOLD);
    ui_granule(x + 2, y + h - 3, UI_METAL_GOLD);
    ui_granule(x + w - 3, y + h - 3, UI_METAL_GOLD);
}

void hud_plate(int x, int y, int w, int h) { plate(x, y, w, h); }

void hud_vitals_frame(int right_x, int y, int rows) {
    const int w = VITAL_BAR + 40, h = rows * 22 + 6;
    plate(right_x - w + 4, y, w, h);
}

void hud_vital(IconId icon, const char *text, Color text_col, float value01, Color bar_col, int right_x, int y) {
    ui_text(text, right_x - MeasureText(text, 10), y, 10, text_col);
    ui_icon(icon, (float)(right_x - VITAL_BAR - 16), (float)(y + 8), 12, text_col);
    ui_bar(right_x - VITAL_BAR, y + 11, VITAL_BAR, fmaxf(0.0f, fminf(1.0f, value01)), bar_col, UI_METAL_SILVER);
}

void hud_stamina(int right_x, int y) {
    float s = player_stamina();
    bool winded = player_winded();
    const char *txt = winded ? T("agotado") : s < 0.35f ? T("cansado") : s < 0.8f ? T("con aliento") : T("descansado");
    Color col = winded ? UI_CARNELIAN : s < 0.35f ? UI_GOLD_LIGHT : UI_BONE;
    hud_vital(ICON_VELOCIDAD, txt, col, s, winded ? UI_CARNELIAN : UI_GOLD, right_x, y);
}

// ------------------------------------------------------------------ barra rapida
// Cada casilla guarda una empuñadura (las de X), un objeto del inventario (arma, escudo,
// antorcha, agua, ungüento, hierbas) o una habilidad activa. Vacia, su tecla abre un selector.
static QbSlot g_slots[HUD_QUICK_SLOTS];
static bool g_slots_set;      // si no, las de serie
static int g_picking = -1;    // casilla que se esta eligiendo (-1: selector cerrado)
static int g_pick_cursor;

typedef struct {
    QbSlot s;
    IconId icon;
    const char *title;
    bool ok;
} QbOption;

static void default_slots(void) {
    memset(g_slots, 0, sizeof(g_slots));
    g_slots[0] = (QbSlot){ QB_GRIP, 0, "" }; // sable y escudo
    g_slots[1] = (QbSlot){ QB_GRIP, 3, "" }; // arco
    g_slots[2] = (QbSlot){ QB_GRIP, 6, "" }; // lanza y escudo
    g_slots[3] = (QbSlot){ QB_ITEM, 0, "utileria.objeto.antorcha" };
    g_slots_set = true;
}

void hud_reset(void) {
    default_slots();
    g_picking = -1;
}

const QbSlot *hud_slots(void) {
    if (!g_slots_set) default_slots();
    return g_slots;
}

// Para que sirve un objeto en la barra: beber, vendar, la antorcha o empuñarlo.
typedef enum { USE_NONE, USE_DRINK, USE_BANDAGE, USE_TORCH, USE_WIELD } ItemUse;
static ItemUse item_use(const GameActions *ga, const char *id) {
    if (!id || !id[0]) return USE_NONE;
    if (strstr(id, "antorcha")) return USE_TORCH;
    if (strstr(id, "agua") || strstr(id, "odre") || strstr(id, "vino") || strstr(id, "airag") || strstr(id, "cerveza")) return USE_DRINK;
    if (strstr(id, "unguento") || strstr(id, "hierbas")) return USE_BANDAGE;
    const InvItem *it = inventory_find(ga->inv, id);
    return it && it->hands != INV_HANDS_NONE ? USE_WIELD : USE_NONE;
}

static const char *item_label(const GameActions *ga, const char *id) {
    const InvItem *it = id ? inventory_find(ga->inv, id) : NULL;
    return it ? T(it->name) : "";
}

static int skill_index(const GameActions *ga, int ability) {
    AbilityId act[3];
    float pot[3];
    int n = tg_actives(ga, act, pot, 3);
    for (int i = 0; i < n; i++)
        if ((int)act[i] == ability) return i;
    return -1;
}

static bool has_carried(const GameActions *ga, const Props *props, const Player *p, const char *id) {
    return ga_has_item(ga, id) || ig_count(ga, props, p, id) > 0;
}

// Como se ve y si se puede usar ahora una casilla.
static QbOption describe(const GameActions *ga, const Props *props, const Player *p, const QbSlot *s) {
    QbOption o = { *s, ICON_FALTA, "", false };
    switch (s->kind) {
    case QB_GRIP: {
        GaGrip g = ga_grip(ga, s->grip);
        const char *main_id = g.right ? g.right : g.left;
        o.icon = main_id ? icon_for_item(main_id) : ICON_ENFUNDAR;
        o.title = main_id ? item_label(ga, main_id) : T("Manos libres");
        o.ok = g.available;
        break;
    }
    case QB_ITEM:
        o.icon = icon_for_item(s->id);
        o.title = item_label(ga, s->id);
        o.ok = has_carried(ga, props, p, s->id);
        break;
    case QB_SKILL:
        o.icon = ICON_NIVEL;
        o.title = T(ability_def((AbilityId)s->grip)->name);
        o.ok = skill_index(ga, s->grip) >= 0 && ga->ab_cd[s->grip] <= 0.0f;
        break;
    default: o.icon = ICON_FALTA, o.title = T("Casilla vacía");
    }
    return o;
}

// La casilla guarda algo que se empuña (una empuñadura o un arma suelta).
static bool wields(const GameActions *ga, const QbSlot *s) {
    return s->kind == QB_GRIP || (s->kind == QB_ITEM && item_use(ga, s->id) == USE_WIELD);
}

static bool slot_active(const GameActions *ga, const QbSlot *s) {
    if (s->kind == QB_GRIP) return ga_grip(ga, s->grip).active;
    if (s->kind == QB_ITEM) {
        if (item_use(ga, s->id) == USE_TORCH) return ga->torch_lit;
        return !ga->hands.sheathed && (!strcmp(ga->hands.right.id, s->id) || !strcmp(ga->hands.left.id, s->id));
    }
    if (s->kind == QB_SKILL) return ga->ab_timer[s->grip] > 0.0f;
    return false;
}

// Lo que se puede poner en una casilla: empuñaduras, objetos a mano y habilidades activas.
#define QB_OPTIONS_MAX 48
static int options(const GameActions *ga, const Props *props, const Player *p, QbOption *out) {
    int n = 0;
    for (int k = 0; k < ga_grip_count() && n < QB_OPTIONS_MAX; k++) {
        QbSlot s = { QB_GRIP, k, "" };
        out[n] = describe(ga, props, p, &s);
        if (out[n].ok) n++;
    }
    Bag *bags[8];
    int nb = ig_bags((GameActions *)ga, props, p, bags, 8);
    static const char *TOOLS[] = { "utileria.objeto.antorcha" };
    for (int pass = 0; pass < 2; pass++) {
        int count = pass == 0 ? (int)(sizeof(TOOLS) / sizeof(TOOLS[0])) : 0;
        for (int b = 0; pass == 1 && b < nb; b++) count += bags[b]->n;
        for (int i = 0; i < count && n < QB_OPTIONS_MAX; i++) {
            const char *id = NULL;
            if (pass == 0) id = TOOLS[i];
            else {
                int k = i;
                for (int b = 0; b < nb; b++) {
                    if (k < bags[b]->n) {
                        id = bags[b]->s[k].id;
                        break;
                    }
                    k -= bags[b]->n;
                }
            }
            if (!id || item_use(ga, id) == USE_NONE || !has_carried(ga, props, p, id)) continue;
            bool dup = false;
            for (int j = 0; j < n && !dup; j++) dup = out[j].s.kind == QB_ITEM && !strcmp(out[j].s.id, id);
            if (dup) continue;
            QbSlot s = { QB_ITEM, 0, "" };
            snprintf(s.id, sizeof(s.id), "%s", id);
            out[n++] = describe(ga, props, p, &s);
        }
    }
    AbilityId act[3];
    float pot[3];
    int na = tg_actives(ga, act, pot, 3);
    for (int i = 0; i < na && n < QB_OPTIONS_MAX; i++) {
        QbSlot s = { QB_SKILL, (int)act[i], "" };
        out[n] = describe(ga, props, p, &s);
        out[n++].ok = true;
    }
    QbSlot none = { QB_EMPTY, 0, "" }; // vaciar la casilla
    out[n] = describe(ga, props, p, &none);
    out[n].ok = true;
    out[n++].title = T("Vaciar la casilla");
    return n;
}

static void use_slot(GameActions *ga, const Props *props, const Player *p, int k, char *log, size_t len) {
    const QbSlot *s = &g_slots[k];
    switch (s->kind) {
    case QB_GRIP: ga_quick_request_grip(s->grip); break;
    case QB_SKILL: {
        int i = skill_index(ga, s->grip);
        if (i >= 0) input_inject((InputAction)(IN_ABILITY1 + i));
        else snprintf(log, len, "%s", T("Esa habilidad ya no está activa."));
        break;
    }
    case QB_ITEM:
        if (!has_carried(ga, props, p, s->id)) {
            snprintf(log, len, T("No tienes %s."), item_label(ga, s->id));
            break;
        }
        switch (item_use(ga, s->id)) {
        case USE_TORCH: ga_quick_request_action(ACTION_LIGHT_TORCH); break;
        case USE_DRINK: wg_request_drink(); break;
        case USE_BANDAGE: cb_request_bandage(); break;
        case USE_WIELD:
            if (slot_active(ga, s)) ga_quick_request_action(ACTION_SHEATHE); // otra vez: enfunda
            else ga_quick_request_wield(s->id);
            break;
        default: break;
        }
        break;
    default: break;
    }
}

bool hud_picker_open(void) { return g_picking >= 0; }
void hud_open_picker(int slot) { g_picking = slot >= 0 && slot < HUD_QUICK_SLOTS ? slot : -1, g_pick_cursor = 0; }

void hud_update(GameActions *ga, const Props *props, const Player *p, bool input_ok, char *log, size_t len) {
    if (!g_slots_set) default_slots();
    if (g_picking >= 0) { // el selector: flechas y Enter, o un toque; Esc cierra
        QbOption opt[QB_OPTIONS_MAX + 1];
        int n = options(ga, props, p, opt);
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) g_pick_cursor = (g_pick_cursor + 1) % n;
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) g_pick_cursor = (g_pick_cursor + n - 1) % n;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) g_pick_cursor = (g_pick_cursor + 8) % n;
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) g_pick_cursor = (g_pick_cursor + n - 8 % n) % n;
        if (g_pick_cursor >= n) g_pick_cursor = n - 1;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ONE + g_picking)) {
            g_slots[g_picking] = opt[g_pick_cursor].s;
            snprintf(log, len, T("Casilla %d: %s."), g_picking + 1, opt[g_pick_cursor].title);
            g_picking = -1;
        } else if (input_pressed(IN_BACK)) g_picking = -1;
        return;
    }
    if (!input_ok || input_debug() || input_ctrl() || ga->menu_open) return;
    if (input_action_down(KA_ESCORT)) return; // Y+1..3: ordenes a la escolta (src/game/hazards_game.c)
    unsigned mods = input_mods();
    for (int k = 0; k < HUD_QUICK_SLOTS; k++) {
        if (!IsKeyPressed(KEY_ONE + k)) continue;
        const QbSlot *s = &g_slots[k];
        if ((mods & KM_ALT) || s->kind == QB_EMPTY) g_picking = k, g_pick_cursor = 0; // vacia (o Alt): elegir
        else if ((mods & KM_SHIFT) && wields(ga, s)) { // Mayús: el arma empuñada, a la otra mano
            if (slot_active(ga, s)) ga_quick_request_swap();
            else snprintf(log, len, T("Empuña primero el arma de la casilla %d (tecla %d) para pasarla de mano."), k + 1, k + 1);
        } else use_slot(ga, props, p, k, log, len);
    }
}

void hud_quickbar(GameActions *ga, const Props *props, const Player *p, int x, int y) {
    if (!g_slots_set) default_slots();
    plate(x - 3, y - 3, HUD_QUICK_SLOTS * (QUICK_TILE + TILE_GAP) + 3, QUICK_TILE + 6);
    char log[8];
    for (int k = 0; k < HUD_QUICK_SLOTS; k++) {
        Rectangle r = { (float)(x + k * (QUICK_TILE + TILE_GAP)), (float)y, QUICK_TILE, QUICK_TILE };
        const QbSlot *s = &g_slots[k];
        QbOption o = describe(ga, props, p, s);
        bool active = slot_active(ga, s), empty = s->kind == QB_EMPTY;
        hover(r);
        bool h;
        if (empty) { // vacia: un hueco con un +
            h = ui_hover(r);
            DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)r.height, (Color){ 30, 22, 16, 200 });
            DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height, h ? UI_GOLD_DARK : (Color){ 70, 54, 36, 255 });
            ui_text_centered("+", (int)(r.x + r.width / 2), (int)r.y + 8, 10, Fade(UI_GOLD_DARK, h ? 1.0f : 0.6f));
        } else {
            h = ui_tile(r, o.icon, active, o.ok);
            if (s->kind == QB_GRIP) { // la otra mano (escudo o daga), chica en la esquina
                GaGrip g = ga_grip(ga, s->grip);
                if (g.left && g.right) ui_icon(icon_for_item(g.left), r.x + r.width - 11, r.y + 1, 10, o.ok ? UI_BONE : UI_BONE_DIM);
            }
            if (s->kind == QB_ITEM && item_use(ga, s->id) != USE_WIELD && item_use(ga, s->id) != USE_TORCH)
                ui_tile_badge(r, TextFormat("%d", ig_count(ga, props, p, s->id)), UI_BONE);
            if (s->kind == QB_SKILL) ui_tile_bar(r, 1.0f - ga->ab_cd[s->grip] / fmaxf(1.0f, ability_def((AbilityId)s->grip)->cooldown), UI_TURQUOISE);
        }
        DrawRectangle((int)r.x + 1, (int)r.y + 1, 7, 9, (Color){ 16, 12, 9, 200 });
        DrawText(TextFormat("%d", k + 1), (int)r.x + 2, (int)r.y + 1, 10, active ? UI_TURQ_LIGHT : UI_GOLD_LIGHT);
        if (g_picking == k) DrawRectangleLines((int)r.x - 1, (int)r.y - 1, (int)r.width + 2, (int)r.height + 2, UI_TURQUOISE);
        if (h) {
            const char *how = empty          ? TextFormat(T("Tecla %d: elegir un arma, un objeto o una habilidad."), k + 1)
                              : wields(ga, s) ? TextFormat(T("Tecla %d: empuñar o enfundar. Mayús+%d: a la otra mano. Alt+%d o clic derecho: cambiar."), k + 1, k + 1, k + 1)
                                              : TextFormat(T("Tecla %d: usar. Alt+%d o clic derecho: cambiar."), k + 1, k + 1);
            ui_legend(empty ? T("Casilla vacía") : o.title, how);
        }
        bool right = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && ui_hover(r);
        if (ui_click(r) || right) {
            if (empty || right) g_picking = k, g_pick_cursor = 0;
            else use_slot(ga, props, p, k, log, sizeof(log));
        }
    }
}

// El selector: una rejilla de casillas sobre la barra rapida.
void hud_draw_picker(GameActions *ga, const Props *props, const Player *p, int w, int h) {
    if (g_picking < 0) return;
    QbOption opt[QB_OPTIONS_MAX + 1];
    int n = options(ga, props, p, opt);
    const int cols = 8, tile = 30, gap = 4, rows = (n + cols - 1) / cols;
    const int pw = cols * (tile + gap) + 20, ph = rows * (tile + gap) + 44;
    const int x0 = (w - pw) / 2, y0 = h - 70 - ph;
    ui_panel((Rectangle){ (float)x0, (float)y0, (float)pw, (float)ph }, UI_METAL_GOLD);
    ui_text_centered(TextFormat(T("Casilla %d: elige un arma, un objeto o una habilidad"), g_picking + 1), w / 2, y0 + 10, 10, UI_GOLD_LIGHT);
    for (int i = 0; i < n; i++) {
        Rectangle r = { (float)(x0 + 10 + (i % cols) * (tile + gap)), (float)(y0 + 26 + (i / cols) * (tile + gap)), tile, tile };
        hover(r);
        if (ui_pointer_moved() && ui_hover(r)) g_pick_cursor = i;
        bool sel = i == g_pick_cursor;
        if (opt[i].s.kind == QB_EMPTY) {
            ui_tile(r, ICON_FALTA, sel, true);
        } else {
            ui_tile(r, opt[i].icon, sel, true);
            if (opt[i].s.kind == QB_SKILL) ui_tile_badge(r, "★", UI_TURQ_LIGHT);
        }
        if (sel) ui_legend(opt[i].title, T("Enter o un toque: asignar · Esc: cerrar"));
        if (ui_click(r)) {
            g_slots[g_picking] = opt[i].s;
            g_picking = -1;
            return;
        }
    }
    ui_text_centered(T("Flechas: elegir · Enter: asignar · Esc: cerrar"), w / 2, y0 + ph - 14, 10, UI_BONE_DIM);
}

// Una partida cargada (src/game/save_game.c): las casillas guardadas, si tienen sentido.
void hud_load(const QbSlot *s, int n) {
    hud_reset();
    if (!s || n < 1) return;
    QbSlot slots[HUD_QUICK_SLOTS];
    memcpy(slots, g_slots, sizeof(slots));
    for (int k = 0; k < HUD_QUICK_SLOTS && k < n; k++) {
        if (s[k].kind < QB_EMPTY || s[k].kind > QB_SKILL) return;
        if (s[k].kind == QB_GRIP && (s[k].grip < 0 || s[k].grip >= ga_grip_count())) return;
        if (s[k].kind == QB_SKILL && (s[k].grip < 0 || s[k].grip >= ABIL_COUNT)) return;
        slots[k] = s[k];
        slots[k].id[INV_ID_LEN - 1] = '\0';
    }
    memcpy(g_slots, slots, sizeof(g_slots));
}

// ------------------------------------------------------------------ columna de acciones
typedef enum { COL_MENU, COL_DRINK, COL_BANDAGE, COL_ACTION, COL_PACK, COL_ARROW } ColumnKind;
typedef struct {
    IconId icon;
    ColumnKind kind;
    ActionId action;
    const char *key; // tecla (o "" si solo desde el menu o la columna)
} ColumnEntry;

static const ColumnEntry COLUMN[] = {
    { ICON_ACCIONES, COL_MENU, 0, "Tab" },
    { ICON_BEBER, COL_DRINK, 0, "N" },
    { ICON_UNGUENTO, COL_BANDAGE, 0, "B" },
    { ICON_FOGATA, COL_ACTION, ACTION_PLACE_FIRE, "" },
    { ICON_TIENDA, COL_ACTION, ACTION_PLACE_TENT, "" },
    { ICON_LAZO, COL_ACTION, ACTION_THROW_LASSO, "" },
    { ICON_TREPA, COL_ACTION, ACTION_THROW_GRAPPLE, "" },
    { ICON_HERVIR, COL_ACTION, ACTION_BOIL, "" },
    { ICON_MOCHILA, COL_PACK, 0, "" },           // Mayus+G (no cabe en la casilla: va en la leyenda)
    { ICON_FLECHA_ENCENDIDA, COL_ARROW, 0, "" }, // Mayus+L; solo con arco o ballesta en la mano
};
#define COLUMN_COUNT ((int)(sizeof(COLUMN) / sizeof(COLUMN[0])))

// Con un arma que dispara flechas o virotes en la mano (se pueden encender).
static bool arrows_in_hand(const GameActions *ga) {
    const RangedDef *rd = ga->hands.sheathed ? NULL : ranged_def(ga->hands.right.id);
    return rd && (rd->projectile == PROJ_ARROW || rd->projectile == PROJ_BOLT);
}

void hud_escort_orders(const GameActions *ga, const Troop *troop, int x, int y) {
    bool out = false;
    for (int i = 0; i < troop->count && i < TROOP_MAX && !out; i++) out = ga->npcs[i].escort && troop->members[i].status == STATUS_ACTIVE;
    if (!out) return;
    static const IconId ICONS[ORDER_COUNT] = { ICON_SOLDADO, ICON_ESCUDO, ICON_TRIBU };
    plate(x - 3, y - 3, ORDER_COUNT * (COLUMN_TILE + TILE_GAP) - TILE_GAP + 6, COLUMN_TILE + 6);
    for (int k = 0; k < ORDER_COUNT; k++) {
        Rectangle r = { (float)(x + k * (COLUMN_TILE + TILE_GAP)), (float)y, COLUMN_TILE, COLUMN_TILE };
        hover(r);
        bool h = ui_tile(r, ICONS[k], ga->escort_order == k, true);
        ui_tile_badge(r, TextFormat("%d", k + 1), UI_GOLD_LIGHT);
        if (h)
            ui_legend(k == ORDER_ATTACK ? T("Escolta: atacar") : k == ORDER_DEFEND ? T("Escolta: defender") : T("Escolta: seguir"),
                      k == ORDER_ATTACK   ? T("Y+1: van a tu objetivo o al enemigo más cercano.")
                      : k == ORDER_DEFEND ? T("Y+2: se quedan a tu lado y paran a quien se te acerque.")
                                          : T("Y+3: te siguen sin pelear."));
        if (ui_click(r)) ga_request_escort_order(k);
    }
}

void hud_action_column(const GameActions *ga, int x, int y) {
    const ColumnEntry *shown[COLUMN_COUNT];
    int n = 0;
    for (int i = 0; i < COLUMN_COUNT; i++)
        if (COLUMN[i].kind != COL_ARROW || arrows_in_hand(ga)) shown[n++] = &COLUMN[i];
    plate(x - 3, y - 3, COLUMN_TILE + 6, n * (COLUMN_TILE + COLUMN_GAP) - COLUMN_GAP + 6);
    for (int i = 0; i < n; i++) {
        const ColumnEntry *e = shown[i];
        Rectangle r = { (float)x, (float)(y + i * (COLUMN_TILE + COLUMN_GAP)), COLUMN_TILE, COLUMN_TILE };
        bool busy = ga->doing >= 0 && e->kind == COL_ACTION;
        bool selected = e->kind == COL_MENU ? ga->menu_open : e->kind == COL_ACTION && ga->doing == (int)e->action;
        bool usable = e->kind == COL_ARROW ? ga->fire_near && !ga->raining : !busy || selected;
        hover(r);
        bool h = ui_tile(r, e->icon, selected, usable);
        if (e->key[0]) ui_tile_badge(r, e->key, UI_GOLD_LIGHT);
        if (h) {
            const char *title = e->kind == COL_MENU      ? T("Acciones, obras y fabricar")
                                : e->kind == COL_DRINK   ? T("Beber")
                                : e->kind == COL_BANDAGE ? T("Vendar")
                                : e->kind == COL_PACK    ? T("Mochila")
                                : e->kind == COL_ARROW   ? T("Encender la flecha")
                                                         : T(action_def(e->action)->name);
            const char *detail = e->kind == COL_MENU      ? T("Tab: el menú de acciones, obras, fabricar y reparar.")
                                 : e->kind == COL_DRINK   ? T("N: bebe lo más seguro que tengas, o del río en la orilla.")
                                 : e->kind == COL_BANDAGE ? T("B: venda tus heridas (o levanta a un compañero abatido).")
                                 : e->kind == COL_PACK    ? T("Mayús+G: deja la mochila en el suelo para moverte ligero, o recógela.")
                                 : e->kind == COL_ARROW   ? T("Mayús+L: enciende la flecha en un fuego cercano (no con lluvia).")
                                                          : T(action_def(e->action)->desc);
            ui_legend(title, detail);
        }
        if (!ui_click(r)) continue;
        switch (e->kind) {
        case COL_MENU: ga_quick_request_menu(); break;
        case COL_DRINK: wg_request_drink(); break;
        case COL_BANDAGE: cb_request_bandage(); break;
        case COL_PACK: ig_request_pack(); break;
        case COL_ARROW: cb_request_light_arrow(); break;
        default: ga_quick_request_action(e->action); break;
        }
    }
}
