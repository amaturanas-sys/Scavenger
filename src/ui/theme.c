#include "ui/theme.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    Color base, light, dark;
} Metal;

static Metal metal_of(UiMetal m) {
    if (m == UI_METAL_SILVER) return (Metal){ UI_SILVER, UI_SILVER_LIGHT, UI_SILVER_DARK };
    return (Metal){ UI_GOLD, UI_GOLD_LIGHT, UI_GOLD_DARK };
}

static unsigned hash2(int x, int y) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static void px(int x, int y, Color c) { DrawRectangle(x, y, 1, 1, c); }

// Granulo de oro: 2x2 con un brillo arriba a la izquierda.
static void granule(int x, int y, Metal m) {
    DrawRectangle(x, y, 2, 2, m.base);
    px(x, y, m.light);
    px(x + 1, y + 1, m.dark);
}

// Turquesa engastada: rombo de 3x3 con brillo, rodeado de metal oscuro.
static void inlay(int x, int y, Metal m) {
    px(x + 1, y - 1, m.dark);
    px(x - 1, y + 1, m.dark);
    px(x + 3, y + 1, m.dark);
    px(x + 1, y + 3, m.dark);
    DrawRectangle(x, y + 1, 3, 1, UI_TURQUOISE);
    DrawRectangle(x + 1, y, 1, 3, UI_TURQUOISE);
    px(x + 1, y + 2, UI_TURQ_DARK);
    px(x + 1, y, UI_TURQ_LIGHT);
}

// Cuero curtido: base y grietas deterministas (dependen solo del rectangulo).
static void leather(Rectangle r) {
    DrawRectangleRec(r, UI_LEATHER);
    int x0 = (int)r.x, y0 = (int)r.y, w = (int)r.width, h = (int)r.height;
    int cracks = (w * h) / 900 + 1;
    for (int i = 0; i < cracks; i++) {
        unsigned a = hash2(x0 + i * 31, y0 + i * 17);
        int cx = x0 + 2 + (int)(a % (unsigned)(w > 4 ? w - 4 : 1));
        int cy = y0 + 2 + (int)((a >> 12) % (unsigned)(h > 4 ? h - 4 : 1));
        int len = 4 + (int)((a >> 22) % 9u);
        int dx = (int)((a >> 4) % 3u) - 1;
        for (int k = 0; k < len; k++) {
            int x = cx + k, y = cy + (k * dx) / 3;
            if (x < x0 + w - 1 && y > y0 && y < y0 + h - 1) px(x, y, UI_LEATHER_CRACK);
        }
    }
}

// Marco biselado de 3 px: contorno oscuro, cuerpo de metal, brillo arriba/izquierda.
static void bevel_frame(int x, int y, int w, int h, Metal m) {
    DrawRectangleLines(x, y, w, h, m.dark);
    DrawRectangleLines(x + 1, y + 1, w - 2, h - 2, m.base);
    DrawRectangleLines(x + 2, y + 2, w - 4, h - 4, m.base);
    DrawRectangle(x + 1, y + 1, w - 2, 1, m.light);
    DrawRectangle(x + 1, y + 1, 1, h - 2, m.light);
    DrawRectangle(x + 3, y + h - 3, w - 6, 1, m.dark);
    DrawRectangle(x + w - 3, y + 3, 1, h - 6, m.dark);
}

void ui_panel(Rectangle r, UiMetal metal) {
    Metal m = metal_of(metal);
    int x = (int)r.x, y = (int)r.y, w = (int)r.width, h = (int)r.height;
    leather(r);
    bevel_frame(x, y, w, h, m);

    // Banda de granulado entre el marco y un filete interior (como las placas
    // de cinturon: cuentas de oro alternadas con gotas de turquesa).
    const int band = 3, step = 6;
    for (int i = x + band + 3; i + 3 < x + w - band - 2; i += step) {
        int k = (i - x) / step;
        if (k % 2) {
            inlay(i, y + band + 1, m);
            inlay(i, y + h - band - 5, m);
        } else {
            granule(i + 1, y + band + 2, m);
            granule(i + 1, y + h - band - 4, m);
        }
    }
    for (int j = y + band + 9; j + 3 < y + h - band - 7; j += step) {
        int k = (j - y) / step;
        if (k % 2) {
            inlay(x + band + 1, j, m);
            inlay(x + w - band - 5, j, m);
        } else {
            granule(x + band + 2, j + 1, m);
            granule(x + w - band - 4, j + 1, m);
        }
    }
    DrawRectangleLines(x + band + 6, y + band + 6, w - 2 * (band + 6), h - 2 * (band + 6), m.dark);
}

void ui_strip(Rectangle r, UiMetal metal) {
    Metal m = metal_of(metal);
    leather(r);
    int x = (int)r.x, y = (int)r.y, w = (int)r.width;
    DrawRectangle(x, y, w, 1, m.dark);
    DrawRectangle(x, y + 1, w, 1, m.light);
    DrawRectangle(x, y + 2, w, 1, m.base);
    DrawRectangle(x, y + 3, w, 1, m.dark);
    for (int i = x + 4; i < x + w - 4; i += 12) granule(i, y + 5, m);
}

void ui_bar(int x, int y, int w, float value01, Color fill, UiMetal metal) {
    Metal m = metal_of(metal);
    const int h = 7;
    if (value01 < 0.0f) value01 = 0.0f;
    if (value01 > 1.0f) value01 = 1.0f;
    DrawRectangleLines(x, y, w, h, m.dark);
    DrawRectangle(x + 1, y + 1, w - 2, 1, m.light);
    DrawRectangle(x + 1, y + 1, 1, h - 2, m.light);
    DrawRectangle(x + 1, y + h - 2, w - 2, 1, m.base);
    DrawRectangle(x + w - 2, y + 1, 1, h - 2, m.base);
    int ix = x + 2, iw = w - 4, ih = h - 4;
    DrawRectangle(ix, y + 2, iw, ih, UI_VELVET);
    int fw = (int)(iw * value01 + 0.5f);
    if (fw > 0) {
        DrawRectangle(ix, y + 2, fw, ih, fill);
        Color hi = { (unsigned char)((fill.r + 255) / 2), (unsigned char)((fill.g + 255) / 2),
                     (unsigned char)((fill.b + 255) / 2), 255 };
        DrawRectangle(ix, y + 2, fw, 1, hi);
    }
    // Tabiques de metal: la incrustacion se ve como piezas engastadas.
    for (int i = ix + 6; i < ix + iw; i += 7) DrawRectangle(i, y + 2, 1, ih, m.dark);
}

void ui_divider(int x, int y, int w, UiMetal metal) {
    Metal m = metal_of(metal);
    DrawRectangle(x, y + 1, w, 1, m.dark);
    for (int i = x; i < x + w - 1; i += 4) px(i, y, m.base);
    for (int i = x + w / 2 - 12; i <= x + w / 2 + 12; i += 12) inlay(i - 1, y - 1, m);
}

void ui_text(const char *text, int x, int y, int size, Color color) {
    DrawText(text, x + 1, y + 1, size, (Color){ 10, 7, 5, 220 });
    DrawText(text, x, y, size, color);
}

void ui_text_centered(const char *text, int cx, int y, int size, Color color) {
    ui_text(text, cx - MeasureText(text, size) / 2, y, size, color);
}

void ui_granule(int x, int y, UiMetal metal) { granule(x, y, metal_of(metal)); }

void ui_inlay(int x, int y, UiMetal metal) { inlay(x, y, metal_of(metal)); }

static int wrap(const char *text, int x, int y, int width, int size, Color color, bool draw) {
    char line[256] = "", candidate[256];
    int lines = 0;
    const int line_h = size + 2;
    const char *p = text;
    while (*p) {
        size_t wlen = strcspn(p, " \n");
        if (line[0]) snprintf(candidate, sizeof(candidate), "%s %.*s", line, (int)wlen, p);
        else snprintf(candidate, sizeof(candidate), "%.*s", (int)wlen, p);
        if (line[0] && MeasureText(candidate, size) > width) {
            if (draw) ui_text(line, x, y + lines * line_h, size, color);
            lines++;
            snprintf(line, sizeof(line), "%.*s", (int)wlen, p);
        } else {
            memcpy(line, candidate, sizeof(line));
        }
        p += wlen;
        if (*p == '\n') { // salto de linea
            if (draw && line[0]) ui_text(line, x, y + lines * line_h, size, color);
            lines++;
            line[0] = '\0';
            p++;
        }
        while (*p == ' ') p++;
    }
    if (line[0]) {
        if (draw) ui_text(line, x, y + lines * line_h, size, color);
        lines++;
    }
    return lines * line_h;
}

int ui_text_wrapped(const char *text, int x, int y, int width, int size, Color color) {
    return wrap(text, x, y, width, size, color, true);
}

int ui_text_wrapped_height(const char *text, int width, int size) { return wrap(text, 0, 0, width, size, BLANK, false); }
