#include "game/title_menu.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/input.h"
#include "platform.h"
#include "sim/lang.h"
#include "ui/icons.h"
#include "ui/theme.h"

static const char *PLATE_FILES[MENU_PLATES] = {
    "assets/ui/lamina_reno.png",  "assets/ui/lamina_combate.png", "assets/ui/lamina_tigre.png",
    "assets/ui/lamina_grifo.png", "assets/ui/lamina_lobo.png",    "assets/ui/lamina_aguila.png",
    "assets/ui/lamina_tigre_dragon.png",
};

static const char *TITLE_ITEMS[] = { N_("Nueva partida"), N_("Cargar partida"), N_("Instructivo"), N_("Idioma"), N_("Salir") };
static const IconId TITLE_ICONS[] = { ICON_NUEVA, ICON_CARGAR, ICON_INSTRUCTIVO, ICON_IDIOMA, ICON_SALIR };
static const char *TITLE_HINTS[] = { N_("Una tribu nueva en la estepa"), N_("Las partidas guardadas, con su foto y su fecha"),
                                     N_("Controles y reglas del juego"), NULL, N_("Cerrar el juego") };
static const IconId PAUSE_ICONS[] = { ICON_CONTINUAR, ICON_GUARDAR, ICON_CARGAR, ICON_INSTRUCTIVO, ICON_TITULO, ICON_IDIOMA, ICON_SALIR };
static const char *PAUSE_HINTS[] = { N_("Volver al juego (Esc)"), N_("En uno de los tres huecos, con una foto de este momento"),
                                     N_("Volver a una partida guardada"), N_("Controles y reglas del juego"),
                                     N_("Dejar la partida (lo no guardado se pierde)"), NULL, N_("Cerrar el juego") };
static const char *PAUSE_ITEMS[] = { N_("Continuar"), N_("Guardar partida"), N_("Cargar partida"), N_("Instructivo"), N_("Salir al título"),
                                     N_("Idioma"), N_("Salir del juego") };
#define TITLE_COUNT 5
#define PAUSE_COUNT 7
#define TITLE_LANG 3 // el boton del idioma (sin pista: dice el idioma actual)
#define PAUSE_LANG 5

// Laminas del instructivo: un titulo, la lamina que la ilustra y sus lineas.
typedef struct {
    const char *title;
    int plate;
    const char *lines[12];
} HelpPage;

static const HelpPage PAGES[] = {
    { N_("La estepa"), 0,
      { N_("Guías una tropa nómada por la estepa: caza, construye, pelea y sobrevive a las estaciones. Un día dura 30 minutos; en invierno las noches son largas."),
        "",
        N_("WASD mover · Shift correr · C acechar (en la hierba alta te ocultas)"),
        N_("Espacio saltar · Q/E girar la cámara · M marcar el mapa"),
        N_("Esc, Atrás o Ctrl+P: menú (guardar, cargar) · F1 o Ctrl+H: controles en el juego"),
        N_("G deja la mochila en el suelo, en la carreta o en un animal de carga (más ligero para pelear) y la recoge · U en el inventario: cambiar de mochila"),
        N_("Los menús son iconos: pasa el ratón (o elige con las flechas) y su nombre aparece abajo; clic o Enter para usarlos."),
        "",
        N_("Abatido, tu escolta te levanta. Si mueres (desangrado o sin socorro), vuelves a tu último lugar de descanso (campamento o tienda) y tus huesos quedan donde caíste. Tus compañeros mueren para siempre.") } },
    { N_("La tribu y el campamento"), 6,
      { N_("Tab: acciones, obras, fabricar y reparar (Q/E cambia de pestaña) · I: inventario"),
        N_("Una tienda o un refugio lejos de los demás campamentos funda uno nuevo: nombra guardián a alguien de tu escolta."),
        N_("F junto al guardián: obras, escolta, reclutar, capacitar en un oficio, estado y disolver el campamento."),
        N_("Y: dos integrantes te escoltan · Mayús+Y: despachar o mandar un mensajero · G: ficha"),
        N_("La tribu come carne fresca y leche antes que la seca; con hambre baja la moral."),
        N_("Las estructuras se desgastan: la tribu las repara con troncos cada día. Sin mantenimiento, la lluvia torrencial las derrumba."),
        "",
        N_("La moral y la lealtad deciden si la tribu se rebela."),
        N_("Reclutar, liberar o ejecutar prisioneros cambia la relación con el reino.") } },
    { N_("Combate cuerpo a cuerpo"), 1,
      { N_("V golpe; tres seguidos: combo · mantener V: golpe pesado (rompe guardias)"),
        N_("J o botón central: patada · corriendo: patada con inercia (tumba aun con escudo)"),
        N_("Z cubrirse · con escudo: Z + V golpe de escudo · corriendo + Z: carga"),
        N_("U agarre y llave: arrancas el escudo, desarmas o tumbas"),
        N_("O engancha el escudo enemigo con hacha, guja o alabarda"),
        N_("1 a 9: la barra rápida (arma, objeto o habilidad; vacía o Mayús+número: elegir qué va) · X cambiar empuñadura · H enfundar"),
        N_("Correr, saltar y golpear gastan aguante (la barra de abajo a la derecha): agotado, no corres hasta recobrar el aliento."),
        N_("B venda heridas con hierbas (o ungüento) · P equipo y heridas · I inventario"),
        N_("A caballo: V golpe con más alcance y la inercia del galope; la lanza derriba; al galope arrollas. Un derribo te tira del caballo."),
        N_("Los enemigos abatidos dejan una bolsa de botín: F la recoge (lo que no cabe se queda)."),
        N_("Cada zona del cuerpo recibe distinto daño: cabeza y cuello son letales.") } },
    { N_("Arcos, flechas y armaduras"), 5,
      { N_("Arco, ballesta u honda: mantén V para tensar y suelta para disparar. La cámara alza la mira: la curva amarilla muestra dónde caerá; la caída depende del peso del proyectil y de la potencia del arma."),
        N_("L junto a un fuego enciende la flecha: quema y prende lo que toca."),
        "",
        N_("La armadura va por piezas (casco, gorjal, coraza, hombreras, brazales, guantes, faldar, grebas, botas). Cada material para distinto el corte, el golpe y la flecha, y se gasta con los impactos."),
        "",
        N_("Fabricar a mano: flechas (leña, plumas, pedernal), cuerda (tendones), ungüento (hierbas y miel), coraza y botas (pieles). Lo forjado pide herrero y horno."),
        N_("Reparar: el cuero y el fieltro con pieles; el bronce y el hierro, herrero, horno y metal.") } },
    { N_("Tatuajes y joyas"), 2,
      { N_("Subes de nivel peleando, cazando, fabricando, construyendo y domando."),
        N_("Tatuajes (para siempre): habla (F) con el druida, elige motivo y zona. Lobo, ciervo, grifo, tamga y olas suben en tres grados; el tercero se bifurca y elegir una rama cierra la otra. La zona del cuerpo cambia a qué va el bonus."),
        "",
        N_("Joyas (cambiables): el orfebre (F) hace anillos (dos por mano), brazaletes, collar, aretes y hebilla con metal y una piedra; el druida las encanta con un efecto pasivo o activo."),
        N_("Piedras: rompe rocas a golpes, F en los corales del lago, cava trincheras o criba en la orilla (Tab)."),
        N_("F2, F3, F4 (o Ctrl+1, 2, 3): habilidades activas · P equipo: armadura, joyas y tatuajes (Q/E)") } },
    { N_("Fauna y caza"), 4,
      { N_("Monturas (caballo, mula, burro, buey, camello, elefante): lazo y silla; R monta y desmonta."),
        N_("Lobos, perros, tigres, pumas, halcones y cuervos: debilítalos peleando, échales el lazo y dales carne (K) para domarlos."),
        N_("Osos, hienas, coyotes y jabalíes: siempre hostiles."),
        N_("K despieza un cadáver, ordeña la cabra · Mayús+K sacrifica el ganado."),
        N_("Camina cerca del ganado para llevarlo (pastoreo)."),
        N_("Los animales de la tribu comen: los herbívoros pastan solos si hay pasto; con nieve, forraje (F corta hierba alta). Los carnívoros, carne o cazan. K les da de comer; los pastores lo hacen en el campamento."),
        N_("Golpea hacia el agua o tira una flecha para pescar."),
        N_("Abejas: con la antorcha encendida, el humo las calma y K toma la miel.") } },
    { N_("Clima y peligros"), 3,
      { N_("El frío baja tu calor: busca el fuego y las yurtas; la ropa mojada enfría."),
        N_("Los lagos helados se cruzan, pero el hielo puede romperse."),
        N_("Socavones de nieve y arena movediza aparecen cada día en otro lugar."),
        N_("Al caer: pulsa la serie de teclas (J K L U I O) a tiempo."),
        "",
        N_("En verano el pasto seco arde: los incendios corren con el viento."),
        N_("Los rayos caen sobre lo alto durante las tormentas."),
        N_("La lluvia apaga todo fuego: antorchas, fogatas, hornos y flechas."),
        N_("Serpientes, escorpiones, arañas y avispas envenenan: véndate (B).") } },
    { N_("Ropa y calor"), 3,
      { N_("P equipo, pestaña Ropa: cabeza, cuello y cara, cuerpo, capa o abrigo y pies. Cada prenda abriga, da sombra o frena la lluvia."),
        N_("Contra el frío, pieles y abrigos: oso, reno, lobo, tigre, cabra. Contra el sol, sombrero, pañuelo del desierto, túnica de seda blanca y manto blanco."),
        N_("El desierto quema de día y hiela de noche. Con calor te sofocas: busca sombra (las yurtas) o el agua; con golpe de calor te desmayas."),
        N_("Las pieles de depredador (lobo, oso, tigre, puma, hiena) dan escarmiento: el enemigo se espanta o huye antes."),
        "",
        N_("Se fabrican a mano (Tab, Fabricar) con las pieles del despiece, la lana de las cabras (los pastores las esquilan) y la seda del botín."),
        N_("La tribu se cambia sola según el tiempo con la ropa de su mochila y del acopio; sin ropa adecuada pasa frío o calor y baja la moral.") } },
    { N_("El mundo"), 0,
      { N_("Una gran tierra casi redonda con cinco regiones: la estepa en el centro, rodeada de ríos que serpentean; el bosque al noroeste, el desierto al noreste, el altiplano al sureste y los fiordos al suroeste."),
        N_("Cada región tiene su altura: la costa, la más baja; el altiplano, el más alto. Hace más frío cuanto más alto."),
        N_("El borde: los fiordos dan al mar, el desierto termina en un cañón y un muro de estratos, el altiplano en un muro de hielo y el bosque en un canal sinuoso del que salen ríos."),
        N_("Cada partida nueva cambia los ríos, los lagos, las montañas y dónde están los pueblos y las guaridas."),
        "",
        N_("Cada región tiene su reino, sus aldeas y su capital amurallada, sus fieras y sus tribus nómadas: rivales, neutrales o amigas. Ruinas y estructuras quedan en el mapa al verlas."),
        N_("F5 o Ctrl+M: vista orbital del mundo entero (flechas para girar e inclinar). El sol, la luna y las estrellas cruzan el cielo; el viento lleva las nubes y las cumbres altas quedan entre ellas.") } },
    { N_("El agua"), 4,
      { N_("N bebe lo más seguro que tengas; en la orilla, sin nada, bebes del río. Con calor da más sed; sin agua te desmayas."),
        N_("El agua que corre por los suelos puede traer espíritus malditos: al rato, fiebre (más lento, más sed). Mayús+N: hierbas."),
        N_("Hervirla los echa (Tab, Hervir agua, junto a un fuego). El alcohol también: cerveza, vino, airag y agua con vino son seguros."),
        N_("Beber de más emborracha: torpe con las armas, cansado y te tambaleas."),
        "",
        N_("Tab, Llenar el odre en la orilla: agua cruda (3 tragos por odre). Fabricar: agua con vino, airag y cerveza de pan."),
        N_("La tribu bebe del acopio o del río cercano (hervida si hay leña). Los animales beben junto al agua; K con agua les da de beber.") } },
};
#define PAGE_COUNT ((int)(sizeof(PAGES) / sizeof(PAGES[0])))

static Texture2D load_tex(const char *rel) {
    Texture2D t = { 0 };
    const char *path = platform_asset_path(rel);
    if (platform_asset_exists(path)) t = LoadTexture(path);
    // El logo se achica mucho (de 320 a ~170 px): con filtro bilineal no se pixela mal.
    if (t.id) SetTextureFilter(t, strstr(rel, "logo") ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);
    platform_note_asset(rel, t.id != 0);
    return t;
}

void menu_init(TitleMenu *m) {
    memset(m, 0, sizeof(*m));
    m->background = load_tex("assets/ui/fondo_titulo.png");
    m->emblem = load_tex("assets/ui/logo.png"); // el logo: el lobo y el tigre en su placa de bronce
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

// ------------------------------------------------------------------ disposicion (dibujo y raton)
static Rectangle item_rect(const TitleMenu *m, int i, int n) {
    const int s = 44, gap = 12, total = n * s + (n - 1) * gap;
    int y = m->screen == MENU_TITLE ? 214 : 150;
    return (Rectangle){ (float)((m->w - total) / 2 + i * (s + gap)), (float)y, (float)s, (float)s };
}

static Rectangle slot_rect(const TitleMenu *m, int s) {
    const int pw = 380, ph = 84;
    return (Rectangle){ (float)((m->w - pw) / 2), (float)(56 + s * (ph + 6)), (float)pw, (float)ph };
}

// Botones del instructivo: pagina anterior, volver, pagina siguiente.
static Rectangle help_rect(const TitleMenu *m, int k) {
    return (Rectangle){ (float)(m->w / 2 - 50 + k * 36), (float)(m->h - 60), 28, 28 };
}

MenuAction menu_update(TitleMenu *m, float dt) {
    m->time += dt;
    m->message_timer = fmaxf(0.0f, m->message_timer - dt);
    if (m->screen == MENU_HIDDEN) return MENU_NONE;
    int n = item_count(m);
    bool row = m->screen == MENU_TITLE || m->screen == MENU_PAUSE; // los botones van en fila
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || (row && (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))))
        m->cursor = (m->cursor + 1) % n, m->slot = -1;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) || (row && (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))))
        m->cursor = (m->cursor + n - 1) % n, m->slot = -1;
    bool enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    bool back = input_pressed(IN_BACK);
    // Raton o dedo: pasar por encima elige; pulsar, entra.
    if (m->w > 0 && (row || m->screen == MENU_LOAD || m->screen == MENU_SAVE)) {
        for (int i = 0; i < n; i++) {
            Rectangle r = row ? item_rect(m, i, n) : slot_rect(m, i);
            if (ui_pointer_moved() && ui_hover(r) && m->cursor != i) m->cursor = i, m->slot = -1;
            if (ui_click(r)) m->cursor = i, enter = true;
        }
    }
    bool page_prev = false, page_next = false;
    if (m->w > 0 && m->screen == MENU_HELP) {
        page_prev = ui_click(help_rect(m, 0));
        back = back || ui_click(help_rect(m, 1));
        page_next = ui_click(help_rect(m, 2));
    }
    switch (m->screen) {
    case MENU_TITLE:
        if (!enter) return MENU_NONE;
        if (m->cursor == 0) return MENU_NEW_GAME;
        if (m->cursor == TITLE_LANG) return MENU_LANG;
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
        case PAUSE_LANG: return MENU_LANG;
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
            if (!si->used) menu_message(m, T("Ese hueco está vacío."));
            else if (!si->compatible) menu_message(m, T("Esa partida es de otra versión del juego."));
            else return m->slot = m->cursor, MENU_LOAD_SLOT;
            return MENU_NONE;
        }
        // Guardar: sobre un hueco ocupado hay que confirmar (segundo Enter).
        if (si->used && m->slot != m->cursor) {
            m->slot = m->cursor;
            menu_message(m, T("Ese hueco tiene una partida: Enter otra vez para sobrescribirla."));
            return MENU_NONE;
        }
        m->slot = m->cursor;
        return MENU_SAVE_SLOT;
    }
    case MENU_HELP:
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) || enter || page_next) m->page = (m->page + 1) % PAGE_COUNT;
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || page_prev) m->page = (m->page + PAGE_COUNT - 1) % PAGE_COUNT;
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

// Fila de botones de icono; el elegido (teclado o raton) dice su nombre en la leyenda.
static void draw_items(const TitleMenu *m, const char **items, const IconId *icons, const char **hints, int n) {
    for (int i = 0; i < n; i++) {
        Rectangle r = item_rect(m, i, n);
        bool sel = i == m->cursor;
        if (sel) { // un brillo de turquesa bajo el elegido
            float k = 0.5f + 0.5f * sinf(m->time * 3.0f);
            DrawRectangle((int)r.x + 6, (int)(r.y + r.height) + 4, (int)r.width - 12, 2, Fade(UI_TURQUOISE, 0.5f + 0.5f * k));
        }
        ui_tile(r, icons[i], sel, true);
        if (icons[i] == ICON_IDIOMA) { // el codigo del idioma, en la esquina
            ui_tile_badge(r, lang_code(lang_get()), UI_TURQ_LIGHT);
            if (sel) ui_legend_default(T("Idioma"), TextFormat("%s -> %s", lang_name(lang_get()), lang_name((Lang)((lang_get() + 1) % LANG_COUNT))));
        } else if (sel) {
            ui_legend_default(T(items[i]), T(hints[i]));
        }
    }
}

// Tres huecos: la minifoto manda; al lado, solo la fecha y el dia. El resto, en la leyenda.
static void draw_slots(TitleMenu *m, int w, int h) {
    bool saving = m->screen == MENU_SAVE;
    ui_icon(saving ? ICON_GUARDAR : ICON_CARGAR, (float)(w / 2 - 16), 14, 32, UI_GOLD_LIGHT);
    for (int s = 0; s < SAVE_SLOTS; s++) {
        const SaveInfo *si = &m->slots[s];
        Rectangle r = slot_rect(m, s);
        int x = (int)r.x, y = (int)r.y, ph = (int)r.height;
        bool sel = s == m->cursor;
        ui_panel(r, sel ? UI_METAL_GOLD : UI_METAL_SILVER);
        Rectangle th = { (float)(x + UI_PANEL_INSET + 2), (float)(y + UI_PANEL_INSET), 110, (float)(ph - 2 * UI_PANEL_INSET) };
        if (si->used && si->thumb.id) draw_plate_fill(si->thumb, th);
        else draw_plate_fill(m->plates[(s + 2) % MENU_PLATES], th); // hueco vacio: una lamina
        int tx = (int)(th.x + th.width + 12), ty = y + UI_PANEL_INSET + 6;
        ui_text(TextFormat("%d", s + 1), (int)(r.x + r.width) - UI_PANEL_INSET - 12, ty, 20, sel ? UI_GOLD_LIGHT : UI_GOLD_DARK);
        if (!si->used) {
            ui_icon(ICON_FALTA, (float)tx, (float)ty + 8, 16, UI_BONE_DIM);
        } else {
            ui_text(save_date_text(si->saved_at), tx, ty, 10, sel ? UI_BONE : UI_BONE_DIM);
            ui_icon(ICON_TIEMPO, (float)tx, (float)ty + 18, 16, UI_GOLD);
            ui_text(TextFormat("%d", si->day), tx + 20, ty + 21, 10, UI_BONE);
            ui_icon(si->compatible ? ICON_TRIBU : ICON_FALTA, (float)tx + 60, (float)ty + 18, 16, si->compatible ? UI_TURQUOISE : UI_CARNELIAN);
            if (si->compatible) ui_text(TextFormat("%d", si->tribe), tx + 80, ty + 21, 10, UI_BONE);
        }
        if (sel) {
            DrawRectangleLinesEx((Rectangle){ r.x + 3, r.y + 3, r.width - 6, r.height - 6 }, 1, UI_TURQUOISE);
            const char *what = TextFormat(T("%s el hueco %d"), saving ? T("Guardar en") : T("Cargar"), s + 1);
            if (!si->used) ui_legend_default(what, saving ? T("Vacío") : T("Vacío: no hay partida"));
            else if (!si->compatible) ui_legend_default(what, T("Partida de otra versión del juego"));
            else ui_legend_default(what, TextFormat(T("Día %d · %s · tribu de %d"), si->day, si->place, si->tribe));
        }
    }
    (void)h;
}

static void draw_help(const TitleMenu *m, int w, int h) {
    const HelpPage *pg = &PAGES[m->page];
    ui_panel((Rectangle){ 20, 24, (float)(w - 40), (float)(h - 56) }, UI_METAL_GOLD);
    ui_text(T(pg->title), 40, 40, 20, UI_GOLD_LIGHT);
    ui_text(TextFormat("%d / %d", m->page + 1, PAGE_COUNT), w - 80, 46, 10, UI_BONE_DIM);
    ui_divider(40, 64, w - 80, UI_METAL_GOLD);
    // La lamina entera, centrada en su marco (sin deformarla).
    Texture2D pl = m->plates[pg->plate];
    float pw = 150.0f, ph = pl.id ? pw * (float)pl.height / (float)pl.width : 100.0f;
    if (ph > 200.0f) ph = 200.0f, pw = pl.id ? ph * (float)pl.width / (float)pl.height : 150.0f;
    draw_plate(pl, (Rectangle){ 44 + (150 - pw) / 2, 80 + (200 - ph) / 2, pw, ph });
    int y = 80;
    for (int i = 0; i < 12 && pg->lines[i]; i++) {
        if (pg->lines[i][0]) y += ui_text_wrapped(T(pg->lines[i]), 210, y, w - 260, 10, UI_BONE);
        else y += 8;
    }
    // Pagina anterior, volver y siguiente.
    static const char *names[3] = { N_("Página anterior"), N_("Volver"), N_("Página siguiente") };
    for (int k = 0; k < 3; k++) {
        Rectangle r = help_rect(m, k);
        bool hover = ui_tile(r, k == 1 ? ICON_TITULO : ICON_CONTINUAR, false, true);
        if (k == 0) { // la flecha de la izquierda, en espejo, encima del boton
            DrawRectangle((int)r.x + 1, (int)r.y + 1, (int)r.width - 2, (int)r.height - 2, hover ? (Color){ 70, 50, 34, 236 } : UI_LEATHER);
            ui_icon_ex(ICON_CONTINUAR, r.x + 6, r.y + 6, 16, hover ? UI_BONE : UI_GOLD, true);
        }
        if (hover) ui_legend(T(names[k]), k == 1 ? "Esc" : T("A / D o flechas"));
    }
}

void menu_draw_controls(int x, int y, int w, int h) {
    ui_panel((Rectangle){ (float)x, (float)y, (float)w, (float)h }, UI_METAL_GOLD);
    static const char *cols[2][9] = {
        { N_("WASD mover · Shift correr"), N_("C acechar · Espacio saltar"), N_("Q/E cámara · M marcar"), N_("V golpe (mantener: pesado)"),
          N_("J patada · Z cubrirse"), N_("U agarre · O gancho"), N_("B vendar · P equipo"), N_("L encender flecha"), N_("X/H empuñar/enfundar") },
        { N_("Tab acciones/fabricar"), N_("I inventario · Y escolta"), N_("F tomar/botín · T lanzar"), N_("R montar · K animal"), N_("G ficha · M mapa"),
          N_("Mayús+X cambiar de mano"), N_("Esc/Atrás/Ctrl+P menú"), N_("F1/Ctrl+H cerrar esta ayuda"), N_("F5/Ctrl+M vista orbital") },
    };
    ui_text(T("Controles"), x + UI_PANEL_INSET + 2, y + UI_PANEL_INSET, 10, UI_GOLD_LIGHT);
    for (int c = 0; c < 2; c++)
        for (int i = 0; i < 9; i++)
            ui_text(T(cols[c][i]), x + UI_PANEL_INSET + 2 + c * (w / 2), y + UI_PANEL_INSET + 16 + i * 12, 10, UI_BONE);
}

void menu_draw(TitleMenu *m, bool in_game, const char *version, int w, int h) {
    if (m->screen == MENU_HIDDEN) return;
    m->w = w, m->h = h;
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
        if (m->emblem.id) { // el logo (con el nombre del juego), con un brillo que va y viene
            float k = 0.9f + 0.1f * sinf(m->time * 1.3f);
            Color tint = { (unsigned char)(255 * k), (unsigned char)(255 * k), (unsigned char)(245 * k), 255 };
            float ew = title ? 168.0f : 96.0f, eh = ew * (float)m->emblem.height / (float)m->emblem.width;
            DrawTexturePro(m->emblem, (Rectangle){ 0, 0, (float)m->emblem.width, (float)m->emblem.height },
                           (Rectangle){ w / 2.0f - ew / 2.0f, title ? 14.0f : 18.0f, ew, eh }, (Vector2){ 0, 0 }, 0.0f, tint);
        }
        // El nombre del juego va en el logo; sin logo, en letras (no se traduce).
        if (title && !m->emblem.id) ui_text_centered(GAME_TITLE, w / 2, 156, 30, UI_GOLD_LIGHT);
        if (!title) ui_text_centered(T("Pausa"), w / 2, 120, 20, UI_GOLD_LIGHT);
        if (title) ui_text_centered(GAME_SUBTITLE, w / 2, 190, 10, UI_TURQ_LIGHT);
        if (title) draw_items(m, TITLE_ITEMS, TITLE_ICONS, TITLE_HINTS, TITLE_COUNT);
        else draw_items(m, PAUSE_ITEMS, PAUSE_ICONS, PAUSE_HINTS, PAUSE_COUNT);
        if (version) ui_text(version, 16, h - 26, 10, UI_BONE_DIM);
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
