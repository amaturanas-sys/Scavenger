#include "dialog.h"

#include <stdio.h>
#include <string.h>

#include "game/input.h"
#include "raylib.h"
#include "sim/lang.h"
#include "ui/theme.h"

#define DLG_W 520
#define DLG_X ((640 - DLG_W) / 2)
#define DLG_COLS 12
#define DLG_TILE 34
#define DLG_GAP 5

void dlg_begin(Dialog *d, const char *speaker, IconId portrait, const char *text) {
    int cursor = d->open ? d->cursor : 0;
    d->open = true;
    snprintf(d->speaker, sizeof(d->speaker), "%s", speaker ? speaker : "");
    snprintf(d->text, sizeof(d->text), "%s", text ? text : "");
    d->portrait = portrait;
    d->n = 0;
    d->cursor = cursor;
}

DlgOption *dlg_option(Dialog *d, IconId icon, const char *label, const char *hint, bool enabled, int value) {
    if (d->n >= DLG_OPTIONS) return NULL;
    DlgOption *o = &d->opt[d->n++];
    memset(o, 0, sizeof(*o));
    o->icon = icon;
    snprintf(o->label, sizeof(o->label), "%s", label ? label : "");
    snprintf(o->hint, sizeof(o->hint), "%s", hint ? hint : "");
    o->enabled = enabled;
    o->value = value;
    return o;
}

void dlg_close(Dialog *d) { d->open = false; }

// Alto del panel segun el texto y las filas de opciones.
static int text_h(const Dialog *d) { return ui_text_wrapped_height(d->text, DLG_W - 2 * UI_PANEL_INSET - 52, 10); }

static int panel_y(const Dialog *d, int h) {
    int rows = (d->n + DLG_COLS - 1) / DLG_COLS;
    int ph = 2 * UI_PANEL_INSET + 16 + (text_h(d) > 36 ? text_h(d) : 36) + 10 + rows * (DLG_TILE + DLG_GAP);
    return h - 22 - ph;
}

static Rectangle opt_rect(const Dialog *d, int i, int h) {
    int y0 = panel_y(d, h) + UI_PANEL_INSET + 16 + (text_h(d) > 36 ? text_h(d) : 36) + 10;
    int c = i % DLG_COLS, r = i / DLG_COLS;
    return (Rectangle){ (float)(DLG_X + UI_PANEL_INSET + 4 + c * (DLG_TILE + DLG_GAP)), (float)(y0 + r * (DLG_TILE + DLG_GAP)), DLG_TILE,
                        DLG_TILE };
}

int dlg_update(Dialog *d) {
    if (!d->open) return DLG_NONE;
    if (input_pressed(IN_BACK)) return DLG_BACK;
    if (d->n <= 0) return DLG_NONE;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) d->cursor = (d->cursor + 1) % d->n;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) d->cursor = (d->cursor + d->n - 1) % d->n;
    if ((IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) && d->cursor + DLG_COLS < d->n) d->cursor += DLG_COLS;
    if ((IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) && d->cursor >= DLG_COLS) d->cursor -= DLG_COLS;
    if (d->cursor >= d->n) d->cursor = d->n - 1;
    bool accept = IsKeyPressed(KEY_ENTER);
    for (int i = 0; i < d->n; i++) {
        Rectangle r = opt_rect(d, i, 360);
        if (ui_pointer_moved() && ui_hover(r)) d->cursor = i;
        if (ui_click(r)) d->cursor = i, accept = true;
    }
    if (accept && d->opt[d->cursor].enabled) return d->opt[d->cursor].value;
    return DLG_NONE;
}

void dlg_draw(const Dialog *d, int w, int h) {
    if (!d->open) return;
    (void)w;
    int y = panel_y(d, h), ph = h - 22 - y;
    ui_panel((Rectangle){ DLG_X, (float)y, DLG_W, (float)ph }, UI_METAL_GOLD);
    int x = DLG_X + UI_PANEL_INSET + 4, ty = y + UI_PANEL_INSET;
    ui_tile((Rectangle){ (float)x, (float)ty + 2, 40, 40 }, d->portrait, true, true);
    ui_text(d->speaker, x + 50, ty, 10, UI_GOLD_LIGHT);
    ui_text_wrapped(d->text, x + 50, ty + 14, DLG_W - 2 * UI_PANEL_INSET - 52, 10, UI_BONE);
    for (int i = 0; i < d->n; i++) {
        const DlgOption *o = &d->opt[i];
        Rectangle r = opt_rect(d, i, h);
        bool hover = ui_tile(r, o->icon, i == d->cursor, o->enabled);
        if (o->marked) DrawRectangleLinesEx((Rectangle){ r.x - 2, r.y - 2, r.width + 4, r.height + 4 }, 1, UI_TURQUOISE);
        if (o->badge[0]) ui_tile_badge(r, o->badge, o->enabled ? UI_BONE : UI_BONE_DIM);
        if (hover) ui_legend(o->label, o->hint);
        else if (i == d->cursor) ui_legend_default(o->label, o->hint);
    }
    if (!d->n) ui_legend_default(T("Esc"), T("volver"));
}
