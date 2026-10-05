#include "game/hud_game.h"

#include <math.h>
#include <stdio.h>

#include "game/combat_game.h"
#include "game/player.h"
#include "game/water_game.h"
#include "sim/lang.h"
#include "ui/theme.h"

#define VITAL_BAR 80   // px de cada barra de la placa
#define QUICK_TILE 26  // px de cada casilla de la barra rapida
#define COLUMN_TILE 24 // px de cada casilla de la columna
#define TILE_GAP 3

static bool g_over, g_over_next; // el puntero sobre el HUD (el del cuadro anterior)

void hud_frame_begin(void) {
    g_over = g_over_next;
    g_over_next = false;
}

bool hud_pointer_over(void) { return g_over; }

static bool hover(Rectangle r) {
    bool h = ui_hover(r);
    g_over_next |= h;
    return h;
}

// ------------------------------------------------------------------ placas
// Una placa de cuero oscuro con borde de oro y granulado en las esquinas, como las placas de
// cinturon de los kurganes.
static void plate(int x, int y, int w, int h) {
    DrawRectangle(x, y, w, h, (Color){ 20, 14, 10, 170 });
    DrawRectangleLines(x, y, w, h, Fade(UI_GOLD_DARK, 0.9f));
    ui_granule(x + 2, y + 2, UI_METAL_GOLD);
    ui_granule(x + w - 3, y + 2, UI_METAL_GOLD);
    ui_granule(x + 2, y + h - 3, UI_METAL_GOLD);
    ui_granule(x + w - 3, y + h - 3, UI_METAL_GOLD);
}

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
static const char *item_label(const GameActions *ga, const char *id) {
    const InvItem *it = id ? inventory_find(ga->inv, id) : NULL;
    return it ? T(it->name) : "";
}

void hud_quickbar(const GameActions *ga, int x, int y) {
    GaQuickSlot q[GA_QUICK_SLOTS];
    ga_quick_slots(ga, q);
    plate(x - 3, y - 3, GA_QUICK_SLOTS * (QUICK_TILE + TILE_GAP) + 3, QUICK_TILE + 6);
    for (int k = 0; k < GA_QUICK_SLOTS; k++) {
        Rectangle r = { (float)(x + k * (QUICK_TILE + TILE_GAP)), (float)y, QUICK_TILE, QUICK_TILE };
        const char *main_id = q[k].right ? q[k].right : q[k].left;
        IconId icon = main_id ? icon_for_item(main_id) : ICON_ENFUNDAR; // sin nada: manos libres
        hover(r);
        bool h = ui_tile(r, icon, q[k].active, q[k].available);
        if (q[k].left && q[k].right) // la otra mano (escudo o daga), chica en la esquina
            ui_icon(icon_for_item(q[k].left), r.x + r.width - 11, r.y + 1, 10, q[k].available ? UI_BONE : UI_BONE_DIM);
        // El numero de la tecla, arriba a la izquierda.
        DrawRectangle((int)r.x + 1, (int)r.y + 1, 7, 9, (Color){ 16, 12, 9, 200 });
        DrawText(TextFormat("%d", k + 1), (int)r.x + 2, (int)r.y + 1, 10, q[k].active ? UI_TURQ_LIGHT : UI_GOLD_LIGHT);
        if (h) {
            const char *title = main_id ? item_label(ga, main_id) : T("Manos libres");
            const char *detail = q[k].right && q[k].left ? TextFormat(T("Con %s en la izquierda. Tecla %d."), item_label(ga, q[k].left), k + 1)
                                 : !q[k].available         ? T("No lo tienes (ni en el acopio).")
                                 : q[k].active             ? TextFormat(T("Lo empuñas. Tecla %d otra vez: enfundar."), k + 1)
                                                           : TextFormat(T("Tecla %d: empuñar."), k + 1);
            ui_legend(title, detail);
        }
        if (ui_click(r)) ga_quick_request_slot(k);
    }
}

// ------------------------------------------------------------------ columna de acciones
typedef enum { COL_MENU, COL_DRINK, COL_BANDAGE, COL_ACTION } ColumnKind;
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
};
#define COLUMN_COUNT ((int)(sizeof(COLUMN) / sizeof(COLUMN[0])))

void hud_action_column(const GameActions *ga, int x, int y) {
    plate(x - 3, y - 3, COLUMN_TILE + 6, COLUMN_COUNT * (COLUMN_TILE + TILE_GAP) + 3);
    for (int i = 0; i < COLUMN_COUNT; i++) {
        const ColumnEntry *e = &COLUMN[i];
        Rectangle r = { (float)x, (float)(y + i * (COLUMN_TILE + TILE_GAP)), COLUMN_TILE, COLUMN_TILE };
        bool busy = ga->doing >= 0 && e->kind == COL_ACTION;
        bool selected = e->kind == COL_MENU ? ga->menu_open : e->kind == COL_ACTION && ga->doing == (int)e->action;
        hover(r);
        bool h = ui_tile(r, e->icon, selected, !busy || selected);
        if (e->key[0]) ui_tile_badge(r, e->key, UI_GOLD_LIGHT);
        if (h) {
            const char *title = e->kind == COL_MENU ? T("Acciones, obras y fabricar") : e->kind == COL_DRINK ? T("Beber")
                                : e->kind == COL_BANDAGE ? T("Vendar")
                                                         : T(action_def(e->action)->name);
            const char *detail = e->kind == COL_MENU    ? T("Tab: el menú de acciones, obras, fabricar y reparar.")
                                 : e->kind == COL_DRINK ? T("N: bebe lo más seguro que tengas, o del río en la orilla.")
                                 : e->kind == COL_BANDAGE ? T("B: venda tus heridas (o levanta a un compañero abatido).")
                                                          : T(action_def(e->action)->desc);
            ui_legend(title, detail);
        }
        if (!ui_click(r)) continue;
        switch (e->kind) {
        case COL_MENU: ga_quick_request_menu(); break;
        case COL_DRINK: wg_request_drink(); break;
        case COL_BANDAGE: cb_request_bandage(); break;
        default: ga_quick_request_action(e->action); break;
        }
    }
}
