#include "game/title_menu.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "ui/theme.h"

static const char *PLATE_FILES[MENU_PLATES] = {
    "assets/ui/lamina_reno.png",  "assets/ui/lamina_combate.png", "assets/ui/lamina_tigre.png",
    "assets/ui/lamina_grifo.png", "assets/ui/lamina_lobo.png",    "assets/ui/lamina_aguila.png",
    "assets/ui/lamina_tigre_dragon.png",
};

static const char *TITLE_ITEMS[] = { "Nueva partida", "Cargar partida", "Instructivo", "Salir" };
static const char *PAUSE_ITEMS[] = { "Continuar", "Guardar partida", "Cargar partida", "Instructivo", "Salir al título",
                                     "Salir del juego" };
#define TITLE_COUNT 4
#define PAUSE_COUNT 6

// Laminas del instructivo: un titulo, la lamina que la ilustra y sus lineas.
typedef struct {
    const char *title;
    int plate;
    const char *lines[12];
} HelpPage;

static const HelpPage PAGES[] = {
    { "La estepa", 0,
      { "Guías una tropa nómada por la estepa: caza, construye, pelea y sobrevive a las estaciones. Un día dura 30 minutos; en invierno las noches son largas.",
        "",
        "WASD mover · Shift correr · C acechar (en la hierba alta te ocultas)",
        "Espacio saltar · Q/E girar la cámara · M marcar el mapa",
        "Esc menú (guardar, cargar) · F1 controles en el juego",
        "",
        "Tu personaje no muere: abatido, tu escolta te levanta o despiertas en el campamento. Tus compañeros sí pueden morir." } },
    { "La tribu y el campamento", 6,
      { "Tab: acciones, obras en grupo y forja · I: acopio de la tribu",
        "Y: dos integrantes te escoltan · G: ficha del gran guerrero",
        "La tribu come carne fresca y leche antes que la seca; con hambre baja la moral.",
        "Las estructuras se desgastan: la tribu las repara con troncos cada día. Sin mantenimiento, la lluvia torrencial las derrumba.",
        "",
        "La moral y la lealtad deciden si la tribu se rebela.",
        "Reclutar, liberar o ejecutar prisioneros cambia la relación con el reino." } },
    { "Combate cuerpo a cuerpo", 1,
      { "V golpe; tres seguidos: combo · mantener V: golpe pesado (rompe guardias)",
        "J o botón central: patada · corriendo: patada con inercia (tumba aun con escudo)",
        "Z cubrirse · con escudo: Z + V golpe de escudo · corriendo + Z: carga",
        "U agarre y llave: arrancas el escudo, desarmas o tumbas",
        "O engancha el escudo enemigo con hacha, guja o alabarda",
        "X cambiar empuñadura · Mayús+X pasar el arma de mano · H enfundar",
        "",
        "B venda heridas con hierbas · P panel de heridas y armadura",
        "Cada zona del cuerpo recibe distinto daño: cabeza y cuello son letales." } },
    { "Arcos, flechas y armaduras", 5,
      { "Arco, ballesta u honda: mantén V para tensar y suelta para disparar. La cámara alza la mira: la curva amarilla muestra dónde caerá; la caída depende del peso del proyectil y de la potencia del arma.",
        "L junto a un fuego enciende la flecha: quema y prende lo que toca.",
        "",
        "La armadura va por piezas (casco, gorjal, coraza, hombreras, brazales, guantes, faldar, grebas, botas). Cada material para distinto el corte, el golpe y la flecha, y se gasta con los impactos." } },
    { "Fauna y caza", 4,
      { "Monturas (caballo, mula, burro, buey, camello, elefante): lazo y silla; R monta y desmonta.",
        "Lobos, perros, tigres, pumas, halcones y cuervos: debilítalos peleando, échales el lazo y dales carne (K) para domarlos.",
        "Osos, hienas, coyotes y jabalíes: siempre hostiles.",
        "K despieza un cadáver, ordeña la cabra · Mayús+K sacrifica el ganado.",
        "Camina cerca del ganado para llevarlo (pastoreo).",
        "Golpea hacia el agua o tira una flecha para pescar.",
        "Abejas: con la antorcha encendida, el humo las calma y K toma la miel." } },
    { "Clima y peligros", 3,
      { "El frío baja tu calor: busca el fuego y las yurtas; la ropa mojada enfría.",
        "Los lagos helados se cruzan, pero el hielo puede romperse.",
        "Socavones de nieve y arena movediza aparecen cada día en otro lugar.",
        "Al caer: pulsa la serie de teclas (J K L U I O) a tiempo.",
        "",
        "En verano el pasto seco arde: los incendios corren con el viento.",
        "Los rayos caen sobre lo alto durante las tormentas.",
        "La lluvia apaga todo fuego: antorchas, fogatas, hornos y flechas.",
        "Serpientes, escorpiones, arañas y avispas envenenan: véndate (B)." } },
};
#define PAGE_COUNT ((int)(sizeof(PAGES) / sizeof(PAGES[0])))

static Texture2D load_tex(const char *rel) {
    Texture2D t = { 0 };
    const char *path = platform_asset_path(rel);
    if (FileExists(path)) t = LoadTexture(path);
    if (t.id) SetTextureFilter(t, TEXTURE_FILTER_POINT);
    return t;
}

void menu_init(TitleMenu *m) {
    memset(m, 0, sizeof(*m));
    m->background = load_tex("assets/ui/fondo_titulo.png");
    m->emblem = load_tex("assets/ui/emblema_ciervo.png");
    for (int i = 0; i < MENU_PLATES; i++) m->plates[i] = load_tex(PLATE_FILES[i]);
}

void menu_unload(TitleMenu *m) {
    if (m->slots_loaded) save_list_unload(m->slots);
    if (m->background.id) UnloadTexture(m->background);
    if (m->emblem.id) UnloadTexture(m->emblem);
    for (int i = 0; i < MENU_PLATES; i++)
        if (m->plates[i].id) UnloadTexture(m->plates[i]);
}

static void refresh_slots(TitleMenu *m) {
    if (m->slots_loaded) save_list_unload(m->slots);
    save_list(m->slots);
    m->slots_loaded = true;
}

void menu_open(TitleMenu *m, MenuScreen s) {
    if (s == MENU_LOAD || s == MENU_SAVE || s == MENU_HELP) m->back = m->screen == MENU_PAUSE ? MENU_PAUSE : MENU_TITLE;
    m->screen = s;
    m->cursor = 0;
    m->page = 0;
    m->slot = -1;
    if (s == MENU_LOAD || s == MENU_SAVE) refresh_slots(m);
}

bool menu_visible(const TitleMenu *m) { return m->screen != MENU_HIDDEN; }

void menu_message(TitleMenu *m, const char *text) {
    snprintf(m->message, sizeof(m->message), "%s", text);
    m->message_timer = 4.0f;
}

static int item_count(const TitleMenu *m) {
    switch (m->screen) {
    case MENU_TITLE: return TITLE_COUNT;
    case MENU_PAUSE: return PAUSE_COUNT;
    case MENU_LOAD:
    case MENU_SAVE: return SAVE_SLOTS;
    default: return 1;
    }
}

MenuAction menu_update(TitleMenu *m, float dt) {
    m->time += dt;
    m->message_timer = fmaxf(0.0f, m->message_timer - dt);
    if (m->screen == MENU_HIDDEN) return MENU_NONE;
    int n = item_count(m);
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) m->cursor = (m->cursor + 1) % n, m->slot = -1;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) m->cursor = (m->cursor + n - 1) % n, m->slot = -1;
    bool enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    bool back = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE);
    switch (m->screen) {
    case MENU_TITLE:
        if (!enter) return MENU_NONE;
        if (m->cursor == 0) return MENU_NEW_GAME;
        if (m->cursor == 1) menu_open(m, MENU_LOAD);
        else if (m->cursor == 2) menu_open(m, MENU_HELP);
        else return MENU_QUIT;
        return MENU_NONE;
    case MENU_PAUSE:
        if (back) return MENU_CONTINUE;
        if (!enter) return MENU_NONE;
        switch (m->cursor) {
        case 0: return MENU_CONTINUE;
        case 1: menu_open(m, MENU_SAVE); return MENU_NONE;
        case 2: menu_open(m, MENU_LOAD); return MENU_NONE;
        case 3: menu_open(m, MENU_HELP); return MENU_NONE;
        case 4: return MENU_TO_TITLE;
        default: return MENU_QUIT;
        }
    case MENU_LOAD:
    case MENU_SAVE: {
        if (back) { // vuelve al menu, con el cursor en la opcion de la que vino
            bool saving = m->screen == MENU_SAVE;
            MenuScreen to = m->back;
            menu_open(m, to);
            m->cursor = to == MENU_PAUSE ? (saving ? 1 : 2) : 1;
            return MENU_NONE;
        }
        if (!enter) return MENU_NONE;
        const SaveInfo *si = &m->slots[m->cursor];
        if (m->screen == MENU_LOAD) {
            if (!si->used) menu_message(m, "Ese hueco está vacío.");
            else if (!si->compatible) menu_message(m, "Esa partida es de otra versión del juego.");
            else return m->slot = m->cursor, MENU_LOAD_SLOT;
            return MENU_NONE;
        }
        // Guardar: sobre un hueco ocupado hay que confirmar (segundo Enter).
        if (si->used && m->slot != m->cursor) {
            m->slot = m->cursor;
            menu_message(m, "Ese hueco tiene una partida: Enter otra vez para sobrescribirla.");
            return MENU_NONE;
        }
        m->slot = m->cursor;
        return MENU_SAVE_SLOT;
    }
    case MENU_HELP:
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) || enter) m->page = (m->page + 1) % PAGE_COUNT;
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) m->page = (m->page + PAGE_COUNT - 1) % PAGE_COUNT;
        if (back) {
            MenuScreen to = m->back;
            menu_open(m, to);
            m->cursor = to == MENU_PAUSE ? 3 : 2;
        }
        return MENU_NONE;
    default: return MENU_NONE;
    }
}

// ------------------------------------------------------------------ ornamentos
// Espiral de la orfebreria de la estepa (como los rizos de las placas).
static void draw_spiral(float cx, float cy, float r, Color c) {
    Vector2 prev = { cx, cy };
    for (int i = 1; i <= 22; i++) {
        float t = (float)i / 22.0f, a = t * 2.6f * PI;
        Vector2 p = { cx + cosf(a) * r * t, cy + sinf(a) * r * t };
        DrawLineEx(prev, p, 1.0f, c);
        prev = p;
    }
}

// Friso: triangulos escalonados y espirales alternos, como el borde de los escudos.
static void draw_frieze(float x, float y, float w, float h, Color c, Color dark) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, dark);
    DrawRectangleLines((int)x, (int)y, (int)w, (int)h, c);
    float step = h * 1.6f;
    for (float px = x + 2.0f; px + step <= x + w; px += step) {
        bool tri = (int)((px - x) / step) % 2 == 0;
        if (tri) {
            DrawTriangle((Vector2){ px + step * 0.5f, y + 2.0f }, (Vector2){ px + 2.0f, y + h - 2.0f },
                         (Vector2){ px + step - 2.0f, y + h - 2.0f }, c);
        } else {
            draw_spiral(px + step * 0.5f, y + h * 0.5f, h * 0.42f, c);
        }
    }
}

static void draw_frame(int w, int h, float alpha) {
    Color gold = Fade(UI_GOLD, alpha), dark = Fade((Color){ 30, 20, 12, 255 }, alpha * 0.85f);
    draw_frieze(0, 0, (float)w, 12, gold, dark);
    draw_frieze(0, (float)h - 12, (float)w, 12, gold, dark);
    // Esquinas: una turquesa engastada en un circulo de oro.
    int cx[2] = { 10, w - 10 }, cy[2] = { 6, h - 6 };
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++) {
            DrawCircle(cx[i], cy[j], 7, gold);
            DrawCircle(cx[i], cy[j], 4, Fade(UI_TURQUOISE, alpha));
        }
}

static void draw_plate(Texture2D t, Rectangle r) {
    DrawRectangleRec((Rectangle){ r.x - 3, r.y - 3, r.width + 6, r.height + 6 }, UI_GOLD_DARK);
    DrawRectangleLinesEx((Rectangle){ r.x - 3, r.y - 3, r.width + 6, r.height + 6 }, 1, UI_GOLD);
    if (t.id) DrawTexturePro(t, (Rectangle){ 0, 0, (float)t.width, (float)t.height }, r, (Vector2){ 0, 0 }, 0.0f, WHITE);
    else DrawRectangleRec(r, UI_VELVET);
}

// Una lamina recortada para que llene el rectangulo sin deformarse.
static void draw_plate_fill(Texture2D t, Rectangle r) {
    if (!t.id) {
        draw_plate(t, r);
        return;
    }
    float sa = (float)t.width / (float)t.height, ra = r.width / r.height;
    Rectangle src = { 0, 0, (float)t.width, (float)t.height };
    if (sa > ra) src.width = t.height * ra, src.x = (t.width - src.width) * 0.5f;
    else src.height = t.width / ra, src.y = (t.height - src.height) * 0.5f;
    DrawRectangleRec((Rectangle){ r.x - 3, r.y - 3, r.width + 6, r.height + 6 }, UI_GOLD_DARK);
    DrawRectangleLinesEx((Rectangle){ r.x - 3, r.y - 3, r.width + 6, r.height + 6 }, 1, UI_GOLD);
    DrawTexturePro(t, src, r, (Vector2){ 0, 0 }, 0.0f, WHITE);
}

static void draw_items(const TitleMenu *m, const char **items, int n, int cx, int y) {
    const int w = 190, row = 22;
    ui_panel((Rectangle){ (float)(cx - w / 2), (float)y, (float)w, (float)(n * row + 2 * UI_PANEL_INSET) }, UI_METAL_GOLD);
    for (int i = 0; i < n; i++) {
        int ty = y + UI_PANEL_INSET + i * row + 5;
        bool sel = i == m->cursor;
        if (sel) {
            DrawRectangle(cx - w / 2 + UI_PANEL_INSET, ty - 4, w - 2 * UI_PANEL_INSET, row - 2, Fade(UI_TURQ_DARK, 0.85f));
            DrawCircle(cx - w / 2 + UI_PANEL_INSET + 8, ty + 5, 3, UI_TURQUOISE);
        }
        ui_text_centered(items[i], cx, ty, 10, sel ? UI_GOLD_LIGHT : UI_BONE_DIM);
    }
}

static void draw_slots(TitleMenu *m, int w, int h) {
    bool saving = m->screen == MENU_SAVE;
    ui_text_centered(saving ? "Guardar partida" : "Cargar partida", w / 2, 26, 20, UI_GOLD_LIGHT);
    const int pw = 380, ph = 84, x = (w - pw) / 2;
    for (int s = 0; s < SAVE_SLOTS; s++) {
        const SaveInfo *si = &m->slots[s];
        int y = 56 + s * (ph + 6);
        bool sel = s == m->cursor;
        ui_panel((Rectangle){ (float)x, (float)y, (float)pw, (float)ph }, sel ? UI_METAL_GOLD : UI_METAL_SILVER);
        Rectangle th = { (float)(x + UI_PANEL_INSET + 2), (float)(y + UI_PANEL_INSET), 110, (float)(ph - 2 * UI_PANEL_INSET) };
        if (si->used && si->thumb.id) draw_plate_fill(si->thumb, th);
        else draw_plate_fill(m->plates[(s + 2) % MENU_PLATES], th); // hueco vacio: una lamina
        int tx = (int)(th.x + th.width + 12), ty = y + UI_PANEL_INSET + 2;
        ui_text(TextFormat("Hueco %d", s + 1), tx, ty, 10, sel ? UI_GOLD_LIGHT : UI_GOLD);
        if (!si->used) {
            ui_text("Vacío", tx, ty + 16, 10, UI_BONE_DIM);
            continue;
        }
        ui_text(save_date_text(si->saved_at), tx, ty + 16, 10, UI_BONE);
        ui_text(TextFormat("Día %d · %s", si->day, si->place), tx, ty + 30, 10, UI_BONE_DIM);
        ui_text(si->compatible ? TextFormat("Tribu: %d", si->tribe) : "Otra versión del juego", tx, ty + 44, 10,
                si->compatible ? UI_TURQUOISE : UI_CARNELIAN);
        if (sel) DrawRectangleLinesEx((Rectangle){ (float)x + 3, (float)y + 3, (float)pw - 6, (float)ph - 6 }, 1, UI_TURQUOISE);
    }
    ui_text_centered(saving ? "Flechas: elegir · Enter: guardar · Esc: volver" : "Flechas: elegir · Enter: cargar · Esc: volver",
                     w / 2, h - 30, 10, UI_BONE_DIM);
}

static void draw_help(const TitleMenu *m, int w, int h) {
    const HelpPage *pg = &PAGES[m->page];
    ui_panel((Rectangle){ 20, 24, (float)(w - 40), (float)(h - 56) }, UI_METAL_GOLD);
    ui_text(pg->title, 40, 40, 20, UI_GOLD_LIGHT);
    ui_text(TextFormat("%d / %d", m->page + 1, PAGE_COUNT), w - 80, 46, 10, UI_BONE_DIM);
    ui_divider(40, 64, w - 80, UI_METAL_GOLD);
    // La lamina entera, centrada en su marco (sin deformarla).
    Texture2D pl = m->plates[pg->plate];
    float pw = 150.0f, ph = pl.id ? pw * (float)pl.height / (float)pl.width : 100.0f;
    if (ph > 200.0f) ph = 200.0f, pw = pl.id ? ph * (float)pl.width / (float)pl.height : 150.0f;
    draw_plate(pl, (Rectangle){ 44 + (150 - pw) / 2, 80 + (200 - ph) / 2, pw, ph });
    int y = 80;
    for (int i = 0; i < 12 && pg->lines[i]; i++) {
        if (pg->lines[i][0]) y += ui_text_wrapped(pg->lines[i], 210, y, w - 260, 10, UI_BONE);
        else y += 8;
    }
    ui_text_centered("A/D o flechas: página · Esc: volver", w / 2, h - 30, 10, UI_BONE_DIM);
}

void menu_draw_controls(int x, int y, int w, int h) {
    ui_panel((Rectangle){ (float)x, (float)y, (float)w, (float)h }, UI_METAL_GOLD);
    static const char *cols[2][9] = {
        { "WASD mover · Shift correr", "C acechar · Espacio saltar", "Q/E cámara · M marcar", "V golpe (mantener: pesado)",
          "J patada · Z cubrirse", "U agarre · O gancho", "B vendar · P salud", "L encender flecha", "X/H empuñar/enfundar" },
        { "Tab acciones y forja", "I acopio · Y escolta", "F tomar · T lanzar", "R montar · K animal", "G ficha · M mapa",
          "Mayús+X cambiar de mano", "Esc menú: guardar/cargar", "F1 cerrar esta ayuda", "" },
    };
    ui_text("Controles", x + UI_PANEL_INSET + 2, y + UI_PANEL_INSET, 10, UI_GOLD_LIGHT);
    for (int c = 0; c < 2; c++)
        for (int i = 0; i < 9; i++)
            ui_text(cols[c][i], x + UI_PANEL_INSET + 2 + c * (w / 2), y + UI_PANEL_INSET + 16 + i * 12, 10, UI_BONE);
}

void menu_draw(TitleMenu *m, bool in_game, const char *version, int w, int h) {
    if (m->screen == MENU_HIDDEN) return;
    // Fondo: las estelas en la estepa (o la partida oscurecida, en la pausa).
    if (in_game) {
        DrawRectangle(0, 0, w, h, (Color){ 10, 7, 5, 190 });
    } else if (m->background.id) {
        float drift = sinf(m->time * 0.05f) * 6.0f; // la camara respira apenas
        DrawTexturePro(m->background, (Rectangle){ 8.0f + drift, 4, (float)m->background.width - 16, (float)m->background.height - 8 },
                       (Rectangle){ 0, 0, (float)w, (float)h }, (Vector2){ 0, 0 }, 0.0f, WHITE);
        DrawRectangleGradientV(0, 0, w, h, (Color){ 10, 7, 5, 60 }, (Color){ 10, 7, 5, 200 });
    } else {
        ClearBackground(UI_VELVET);
    }
    draw_frame(w, h, 1.0f);
    switch (m->screen) {
    case MENU_TITLE:
    case MENU_PAUSE: {
        bool title = m->screen == MENU_TITLE;
        if (m->emblem.id) { // el ciervo de oro de astas de ave, con un brillo que va y viene
            float k = 0.85f + 0.15f * sinf(m->time * 1.3f);
            Color tint = { (unsigned char)(255 * k), (unsigned char)(255 * k), (unsigned char)(235 * k), 255 };
            float ew = title ? 180.0f : 130.0f, eh = ew * (float)m->emblem.height / (float)m->emblem.width;
            DrawTexturePro(m->emblem, (Rectangle){ 0, 0, (float)m->emblem.width, (float)m->emblem.height },
                           (Rectangle){ w / 2.0f - ew / 2.0f, title ? 18.0f : 16.0f, ew, eh }, (Vector2){ 0, 0 }, 0.0f, tint);
        }
        int ty = title ? 160 : 118;
        ui_text_centered(title ? "ESTEPA" : "Pausa", w / 2, ty, title ? 40 : 20, UI_GOLD_LIGHT);
        if (title) ui_text_centered("La tribu de las estelas", w / 2, ty + 40, 10, UI_BONE_DIM);
        if (title) draw_items(m, TITLE_ITEMS, TITLE_COUNT, w / 2, ty + 58);
        else draw_items(m, PAUSE_ITEMS, PAUSE_COUNT, w / 2, ty + 28);
        if (version) ui_text(version, 16, h - 26, 10, UI_BONE_DIM);
        ui_text("Flechas y Enter", w - 16 - MeasureText("Flechas y Enter", 10), h - 26, 10, UI_BONE_DIM);
        break;
    }
    case MENU_LOAD:
    case MENU_SAVE: draw_slots(m, w, h); break;
    case MENU_HELP: draw_help(m, w, h); break;
    default: break;
    }
    if (m->message_timer > 0.0f) {
        int mw = MeasureText(m->message, 10) + 24;
        ui_strip((Rectangle){ (float)(w - mw) / 2, (float)h - 52, (float)mw, 20 }, UI_METAL_GOLD);
        ui_text_centered(m->message, w / 2, h - 46, 10, UI_GOLD_LIGHT);
    }
}
