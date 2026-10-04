#include "icons.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "ui/theme.h"

#define CELL 32

static const char *ICON_NAMES[ICON_COUNT] = { "nueva", "cargar", "guardar", "instructivo", "salir", "continuar", "titulo", "acciones", "obras", "fabricar", "reparar", "tomar", "lanzar", "empunar", "enfundar", "fogata", "tienda", "trinchera", "trepa", "lazo", "antorcha", "ensillar", "refugio", "muro_piedra", "hoguera", "totem", "horno", "fundicion", "torre", "corral", "bolsillo", "mochila", "alforjas", "carreta", "campamento", "botin", "casco", "cuello", "torso", "hombreras", "brazales", "guantes", "faldar", "grebas", "botas", "amuleto", "tatuaje", "sable", "daga", "hacha", "maza", "lanza", "guja", "espada", "arco", "ballesta", "honda", "mosquete", "escudo", "paves", "flecha", "virote", "piedras_honda", "bala", "lena", "piedra", "pieles", "plumas", "hueso", "tendones", "pedernal", "cuerda", "mineral", "lingote", "carbon", "carne", "carne_seca", "hierbas", "miel", "leche", "unguento", "agua", "herramienta", "mensaje", "bulto", "estructura", "animal", "persona", "corte", "golpe", "punta", "cobertura", "vida", "sangre", "veneno", "peso", "tiempo", "ok", "falta", "tribu", "velocidad", "idioma", "druida", "orfebre", "anillo", "brazalete", "collar", "arete", "hebilla", "gema", "guardian", "reclutar", "entrenar", "disolver", "mochila_grande", "soltar", "recoger", "mensajero", "despachar", "lugar", "alimentar", "pastar", "soldado", "pastor", "cazador", "explorador", "nivel", "bloqueado", "dialogo", "mapa", "frio", "sol", "ropa", "sombrero", "capa", "escarmiento" };

static Texture2D g_atlas, g_atlas_small; // nitido (32 px y mas) y con mipmaps (16 px)
static int g_cell[ICON_COUNT]; // celda del atlas de cada icono
static int g_cols = 16;

void icons_load(void) {
    for (int i = 0; i < ICON_COUNT; i++) g_cell[i] = i; // sin TSV: el orden del enum
    const char *path = platform_asset_path("assets/ui/iconos.png");
    if (FileExists(path)) g_atlas = LoadTexture(path);
    if (g_atlas.id) {
        g_cols = g_atlas.width / CELL > 0 ? g_atlas.width / CELL : 1;
        SetTextureFilter(g_atlas, TEXTURE_FILTER_POINT); // a 32 o 64 px, pixeles nitidos
        // Reducido a 16 px se ve con el promedio de la celda (mipmaps).
        g_atlas_small = LoadTexture(path);
        GenTextureMipmaps(&g_atlas_small);
        SetTextureFilter(g_atlas_small, TEXTURE_FILTER_TRILINEAR);
    }
    char *tsv = LoadFileText(platform_asset_path("assets/ui/iconos.tsv"));
    if (!tsv) return;
    for (char *line = strtok(tsv, "\n"); line; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || !strncmp(line, "indice", 6)) continue;
        int idx = atoi(line);
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        char name[32];
        int k = 0;
        for (tab++; *tab && *tab != '\t' && *tab != '\r' && k < 31; tab++) name[k++] = *tab;
        name[k] = '\0';
        for (int i = 0; i < ICON_COUNT; i++)
            if (!strcmp(name, ICON_NAMES[i])) g_cell[i] = idx;
    }
    UnloadFileText(tsv);
}

void icons_unload(void) {
    if (g_atlas.id) UnloadTexture(g_atlas);
    if (g_atlas_small.id) UnloadTexture(g_atlas_small);
    g_atlas.id = g_atlas_small.id = 0;
}

const char *icon_name(IconId id) { return id >= 0 && id < ICON_COUNT ? ICON_NAMES[id] : ""; }

void ui_icon(IconId id, float x, float y, float size, Color tint) { ui_icon_ex(id, x, y, size, tint, false); }

void ui_icon_ex(IconId id, float x, float y, float size, Color tint, bool flip) {
    if (id < 0 || id >= ICON_COUNT) return;
    if (!g_atlas.id) { // sin atlas: un rombo, para que el boton se vea igual
        DrawPoly((Vector2){ x + size / 2, y + size / 2 }, 4, size * 0.35f, 0.0f, tint);
        return;
    }
    int c = g_cell[id];
    Rectangle src = { (float)(c % g_cols * CELL), (float)(c / g_cols * CELL), flip ? -CELL : CELL, CELL };
    DrawTexturePro(size < CELL && g_atlas_small.id ? g_atlas_small : g_atlas, src, (Rectangle){ floorf(x), floorf(y), size, size }, (Vector2){ 0, 0 }, 0.0f, tint);
}

// ------------------------------------------------------------------ iconos de objetos
typedef struct {
    const char *key; // trozo del id
    IconId icon;
} IconRule;

// La primera regla que encaja gana: lo concreto antes que lo general.
static const IconRule RULES[] = {
    { "amuleto", ICON_AMULETO },     { "tatuaje", ICON_TATUAJE },      { "mensaje", ICON_MENSAJE },
    { "anillo", ICON_ANILLO },       { "brazalete", ICON_BRAZALETE },  { "collar", ICON_COLLAR },
    { "aretes", ICON_ARETE },        { "hebilla", ICON_HEBILLA },      { "gema", ICON_GEMA },
    { "agua.coral", ICON_GEMA },     { "material.oro", ICON_LINGOTE }, { "material.plata", ICON_LINGOTE },
    { "material.bronce", ICON_LINGOTE },
    { "arreo.alforjas", ICON_ALFORJAS }, { "arreo", ICON_ENSILLAR },  { "armadura.montura", ICON_ENSILLAR },
    { "sombrero", ICON_SOMBRERO },   { "gorro", ICON_SOMBRERO },       { "espalda", ICON_CAPA },
    { "vestimenta.torso", ICON_ROPA }, { "piel.", ICON_PIELES },       { "material.seda", ICON_ROPA },
    { "casco", ICON_CASCO },         { "cabeza", ICON_CASCO },         { "cuello", ICON_CUELLO },
    { "hombreras", ICON_HOMBRERAS }, { "brazales", ICON_BRAZALES },    { "guantes", ICON_GUANTES },
    { "faldar", ICON_FALDAR },       { "cintura", ICON_FALDAR },       { "grebas", ICON_GREBAS },
    { "piernas", ICON_GREBAS },      { "botas", ICON_BOTAS },          { "pies", ICON_BOTAS },
    { "armadura.torso", ICON_TORSO }, { "vestimenta.torso", ICON_TORSO }, { "espalda", ICON_TORSO },
    { "escudo.grande", ICON_PAVES }, { "escudo", ICON_ESCUDO },
    { "proyectil.flecha", ICON_FLECHA }, { "virote", ICON_VIROTE },   { "proyectil.piedra", ICON_PIEDRAS_HONDA },
    { "proyectil.bala", ICON_BALA },
    { "daga", ICON_DAGA },           { "cuchillo", ICON_DAGA },        { "hacha", ICON_HACHA },
    { "maza", ICON_MAZA },           { "guja", ICON_GUJA },            { "alabarda", ICON_GUJA },
    { "espada", ICON_ESPADA },       { "sable", ICON_SABLE },          { "lanza", ICON_LANZA },
    { "pica", ICON_LANZA },          { "ballesta", ICON_BALLESTA },    { "arco", ICON_ARCO },
    { "honda", ICON_HONDA },         { "mosquete", ICON_MOSQUETE },    { "canon", ICON_MOSQUETE },
    { "lazo", ICON_LAZO },           { "boleadoras", ICON_LAZO },
    { "lena", ICON_LENA },           { "troncos", ICON_LENA },         { "material.piedra", ICON_PIEDRA },
    { "barro", ICON_PIEDRA },        { "pieles", ICON_PIELES },        { "plumas", ICON_PLUMAS },
    { "hueso", ICON_HUESO },         { "tendones", ICON_TENDONES },    { "pedernal", ICON_PEDERNAL },
    { "cuerda", ICON_CUERDA },       { "cadenas", ICON_CUERDA },       { "cobre", ICON_MINERAL },
    { "estano", ICON_MINERAL },      { "material.hierro", ICON_LINGOTE }, { "carbon", ICON_CARBON },
    { "carne_seca", ICON_CARNE_SECA }, { "carne", ICON_CARNE },       { "queso", ICON_CARNE_SECA },
    { "pan", ICON_CARNE_SECA },      { "hierbas", ICON_HIERBAS },      { "mata_hierbas", ICON_HIERBAS },
    { "miel", ICON_MIEL },           { "leche", ICON_LECHE },          { "airag", ICON_LECHE },
    { "unguento", ICON_UNGUENTO },   { "agua", ICON_AGUA },            { "odre", ICON_AGUA },
    { "antorcha", ICON_ANTORCHA },   { "pala", ICON_TRINCHERA },       { "gancho_trepa", ICON_TREPA },
    { "herramienta", ICON_HERRAMIENTA }, { "yunque", ICON_FABRICAR },  { "caldero", ICON_MIEL },
    { "saco", ICON_BOLSILLO },       { "cofre", ICON_GUARDAR },        { "fogata", ICON_FOGATA },
    { "hoguera", ICON_HOGUERA },     { "horno_bronce", ICON_FUNDICION }, { "horno_acero", ICON_FUNDICION },
    { "horno", ICON_HORNO },         { "forja", ICON_FABRICAR },       { "empalizada", ICON_OBRAS },
    { "atalaya", ICON_TORRE },       { "torre", ICON_TORRE },          { "corral", ICON_CORRAL },
    { "refugio", ICON_REFUGIO },     { "totem", ICON_TOTEM },          { "tienda", ICON_TIENDA },
    { "carreta", ICON_CARRETA },     { "vehiculo", ICON_CARRETA },     { "estructura", ICON_ESTRUCTURA },
    { "animal", ICON_ANIMAL },       { "personaje", ICON_PERSONA },
};

IconId icon_for_item(const char *id) {
    if (!id || !id[0]) return ICON_BULTO;
    for (size_t i = 0; i < sizeof(RULES) / sizeof(RULES[0]); i++)
        if (strstr(id, RULES[i].key)) return RULES[i].icon;
    return ICON_BULTO;
}

IconId icon_for_slot(int s) {
    static const IconId SLOTS[] = { ICON_CASCO,    ICON_CUELLO, ICON_TORSO,  ICON_HOMBRERAS, ICON_BRAZALES,
                                    ICON_GUANTES,  ICON_FALDAR, ICON_GREBAS, ICON_BOTAS };
    return s >= 0 && s < (int)(sizeof(SLOTS) / sizeof(SLOTS[0])) ? SLOTS[s] : ICON_BULTO;
}

// ------------------------------------------------------------------ puntero
static Vector2 g_ptr = { -1000, -1000 };
static bool g_moved, g_pressed;

void ui_pointer_frame(Vector2 virt, bool moved, bool pressed) {
    g_ptr = virt;
    g_moved = moved;
    g_pressed = pressed;
}

Vector2 ui_pointer(void) { return g_ptr; }
bool ui_pointer_moved(void) { return g_moved; }
bool ui_hover(Rectangle r) { return CheckCollisionPointRec(g_ptr, r); }
bool ui_click(Rectangle r) { return g_pressed && CheckCollisionPointRec(g_ptr, r); }

// ------------------------------------------------------------------ botones
bool ui_tile(Rectangle r, IconId icon, bool selected, bool enabled) {
    bool hover = ui_hover(r);
    int x = (int)r.x, y = (int)r.y, w = (int)r.width, h = (int)r.height;
    DrawRectangle(x, y, w, h, selected ? (Color){ 26, 70, 72, 236 } : hover ? (Color){ 70, 50, 34, 236 } : UI_LEATHER);
    DrawRectangleLines(x, y, w, h, selected ? UI_GOLD : hover ? UI_GOLD_DARK : (Color){ 80, 62, 40, 255 });
    if (selected) DrawRectangleLines(x + 1, y + 1, w - 2, h - 2, UI_TURQUOISE);
    else DrawLine(x + 1, y + 1, x + w - 1, y + 1, Fade(UI_GOLD_LIGHT, 0.18f)); // biselado
    float size = fminf(r.width, r.height) >= 36.0f ? 32.0f : 16.0f;
    if (fminf(r.width, r.height) >= 60.0f) size = 64.0f;
    Color tint = !enabled ? (Color){ 120, 108, 92, 200 } : selected ? UI_GOLD_LIGHT : hover ? UI_BONE : UI_GOLD;
    ui_icon(icon, r.x + (r.width - size) / 2, r.y + (r.height - size) / 2, size, tint);
    return hover;
}

void ui_tile_badge(Rectangle r, const char *text, Color c) {
    if (!text || !text[0]) return;
    int tw = MeasureText(text, 10);
    int x = (int)(r.x + r.width) - tw - 2, y = (int)(r.y + r.height) - 10;
    DrawRectangle(x - 1, y, tw + 2, 9, (Color){ 16, 12, 9, 200 });
    DrawText(text, x, y, 10, c);
}

void ui_tile_bar(Rectangle r, float v, Color c) {
    int w = (int)r.width - 6;
    DrawRectangle((int)r.x + 3, (int)(r.y + r.height) - 4, w, 2, UI_LEATHER_CRACK);
    DrawRectangle((int)r.x + 3, (int)(r.y + r.height) - 4, (int)(w * fmaxf(0.0f, fminf(1.0f, v))), 2, c);
}

// ------------------------------------------------------------------ leyenda
static char g_title[96], g_detail[192];
static float g_alpha;
static bool g_strong; // lo que esta bajo el puntero manda sobre lo elegido

void ui_legend(const char *title, const char *detail) {
    snprintf(g_title, sizeof(g_title), "%s", title ? title : "");
    snprintf(g_detail, sizeof(g_detail), "%s", detail ? detail : "");
    g_strong = true;
}

void ui_legend_default(const char *title, const char *detail) {
    if (g_strong) return;
    snprintf(g_title, sizeof(g_title), "%s", title ? title : "");
    snprintf(g_detail, sizeof(g_detail), "%s", detail ? detail : "");
}

bool ui_legend_active(void) { return g_title[0] != '\0'; }

void ui_legend_draw(int w, int h) {
    bool on = g_title[0] != '\0';
    g_alpha = on ? fminf(1.0f, g_alpha + 0.25f) : 0.0f; // aparece en unos cuadros
    if (!on) {
        g_strong = false;
        return;
    }
    int tw = MeasureText(g_title, 10), dw = g_detail[0] ? MeasureText(g_detail, 10) : 0;
    int total = tw + (dw ? dw + 14 : 0);
    if (total > w - 24) dw = 0, total = tw; // si no cabe, solo el nombre
    int x = (w - total) / 2, y = h - 15;
    DrawRectangle(x - 10, y - 3, total + 20, 15, Fade((Color){ 14, 10, 8, 255 }, 0.72f * g_alpha));
    DrawLine(x - 10, y - 3, x + total + 10, y - 3, Fade(UI_GOLD_DARK, g_alpha));
    ui_text(g_title, x, y, 10, Fade(UI_GOLD_LIGHT, g_alpha));
    if (dw) {
        DrawCircle(x + tw + 7, y + 5, 1.5f, Fade(UI_TURQUOISE, g_alpha));
        ui_text(g_detail, x + tw + 14, y, 10, Fade(UI_BONE_DIM, g_alpha));
    }
    g_title[0] = g_detail[0] = '\0';
    g_strong = false;
}
