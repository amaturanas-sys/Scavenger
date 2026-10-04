// Iconos de los menus: siluetas en un atlas editable (assets/ui/iconos.png, celdas de
// 32x32; el orden lo da assets/ui/iconos.tsv; se regenera con tools/assets/iconos.py).
//  - botones de icono (ui_tile) con estado: normal, elegido, sin poder usarse;
//  - el puntero (raton o dedo) en coordenadas de la pantalla de 640x360;
//  - la leyenda discreta en la base de la pantalla: el nombre (y un detalle) de lo
//    que esta bajo el puntero o elegido con el teclado.
#ifndef ESTEPA_UI_ICONS_H
#define ESTEPA_UI_ICONS_H

#include <stdbool.h>

#include "raylib.h"

typedef enum {
    ICON_NUEVA,
    ICON_CARGAR,
    ICON_GUARDAR,
    ICON_INSTRUCTIVO,
    ICON_SALIR,
    ICON_CONTINUAR,
    ICON_TITULO,
    ICON_ACCIONES,
    ICON_OBRAS,
    ICON_FABRICAR,
    ICON_REPARAR,
    ICON_TOMAR,
    ICON_LANZAR,
    ICON_EMPUNAR,
    ICON_ENFUNDAR,
    ICON_FOGATA,
    ICON_TIENDA,
    ICON_TRINCHERA,
    ICON_TREPA,
    ICON_LAZO,
    ICON_ANTORCHA,
    ICON_ENSILLAR,
    ICON_REFUGIO,
    ICON_MURO_PIEDRA,
    ICON_HOGUERA,
    ICON_TOTEM,
    ICON_HORNO,
    ICON_FUNDICION,
    ICON_TORRE,
    ICON_CORRAL,
    ICON_BOLSILLO,
    ICON_MOCHILA,
    ICON_ALFORJAS,
    ICON_CARRETA,
    ICON_CAMPAMENTO,
    ICON_BOTIN,
    ICON_CASCO,
    ICON_CUELLO,
    ICON_TORSO,
    ICON_HOMBRERAS,
    ICON_BRAZALES,
    ICON_GUANTES,
    ICON_FALDAR,
    ICON_GREBAS,
    ICON_BOTAS,
    ICON_AMULETO,
    ICON_TATUAJE,
    ICON_SABLE,
    ICON_DAGA,
    ICON_HACHA,
    ICON_MAZA,
    ICON_LANZA,
    ICON_GUJA,
    ICON_ESPADA,
    ICON_ARCO,
    ICON_BALLESTA,
    ICON_HONDA,
    ICON_MOSQUETE,
    ICON_ESCUDO,
    ICON_PAVES,
    ICON_FLECHA,
    ICON_VIROTE,
    ICON_PIEDRAS_HONDA,
    ICON_BALA,
    ICON_LENA,
    ICON_PIEDRA,
    ICON_PIELES,
    ICON_PLUMAS,
    ICON_HUESO,
    ICON_TENDONES,
    ICON_PEDERNAL,
    ICON_CUERDA,
    ICON_MINERAL,
    ICON_LINGOTE,
    ICON_CARBON,
    ICON_CARNE,
    ICON_CARNE_SECA,
    ICON_HIERBAS,
    ICON_MIEL,
    ICON_LECHE,
    ICON_UNGUENTO,
    ICON_AGUA,
    ICON_HERRAMIENTA,
    ICON_MENSAJE,
    ICON_BULTO,
    ICON_ESTRUCTURA,
    ICON_ANIMAL,
    ICON_PERSONA,
    ICON_CORTE,
    ICON_GOLPE,
    ICON_PUNTA,
    ICON_COBERTURA,
    ICON_VIDA,
    ICON_SANGRE,
    ICON_VENENO,
    ICON_PESO,
    ICON_TIEMPO,
    ICON_OK,
    ICON_FALTA,
    ICON_TRIBU,
    ICON_VELOCIDAD,
    ICON_IDIOMA,
    ICON_DRUIDA,
    ICON_ORFEBRE,
    ICON_ANILLO,
    ICON_BRAZALETE,
    ICON_COLLAR,
    ICON_ARETE,
    ICON_HEBILLA,
    ICON_GEMA,
    ICON_GUARDIAN,
    ICON_RECLUTAR,
    ICON_ENTRENAR,
    ICON_DISOLVER,
    ICON_MOCHILA_GRANDE,
    ICON_SOLTAR,
    ICON_RECOGER,
    ICON_MENSAJERO,
    ICON_DESPACHAR,
    ICON_LUGAR,
    ICON_ALIMENTAR,
    ICON_PASTAR,
    ICON_SOLDADO,
    ICON_PASTOR,
    ICON_CAZADOR,
    ICON_EXPLORADOR,
    ICON_NIVEL,
    ICON_BLOQUEADO,
    ICON_DIALOGO,
    ICON_MAPA,
    ICON_COUNT
} IconId;

void icons_load(void);
void icons_unload(void);
const char *icon_name(IconId id);
// La silueta, teñida, en un cuadrado de size px (32: nitida; 16: reducida).
void ui_icon(IconId id, float x, float y, float size, Color tint);
void ui_icon_ex(IconId id, float x, float y, float size, Color tint, bool flip); // flip: espejo horizontal
// El icono de un objeto del inventario (por su id) y el de un hueco de armadura (ArmorSlot).
IconId icon_for_item(const char *inv_id);
IconId icon_for_slot(int armor_slot);

// ---------------------------------------------------------------- puntero
// main lo pone cada cuadro: posicion en la pantalla virtual y si se pulso.
void ui_pointer_frame(Vector2 virt, bool moved, bool pressed);
Vector2 ui_pointer(void);
bool ui_pointer_moved(void); // el raton se movio este cuadro (manda sobre el teclado)
bool ui_hover(Rectangle r);
bool ui_click(Rectangle r);  // pulsado este cuadro dentro de r

// ---------------------------------------------------------------- botones
// Boton de icono: placa de cuero con filete de metal; elegido, con turquesa.
// Devuelve true si el puntero esta encima.
bool ui_tile(Rectangle r, IconId icon, bool selected, bool enabled);
// Cifra en la esquina (cantidad) y barra de estado al pie del boton.
void ui_tile_badge(Rectangle r, const char *text, Color c);
void ui_tile_bar(Rectangle r, float value01, Color c);

// ---------------------------------------------------------------- leyenda
// Lo que dice la base de la pantalla este cuadro. ui_legend: lo que esta bajo el puntero
// (manda); ui_legend_default: lo elegido con el teclado (si nada esta bajo el puntero).
void ui_legend(const char *title, const char *detail);
void ui_legend_default(const char *title, const char *detail);
void ui_legend_draw(int w, int h); // al final del cuadro; la borra
bool ui_legend_active(void);

#endif
