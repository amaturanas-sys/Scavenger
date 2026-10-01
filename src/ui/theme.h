// Estetica de la interfaz: orfebreria de la estepa (ver docs/ESTILO_VISUAL.md).
//
// Paneles como placas de cinturon de estilo animal: marco de oro (o plata)
// con biselado, banda de granulado con incrustaciones de turquesa y fondo de
// cuero curtido. Todo se dibuja con primitivas, a nivel de pixel, sobre la
// textura de 640x360: sin texturas que cargar y nitido al escalar.
#ifndef ESTEPA_UI_THEME_H
#define ESTEPA_UI_THEME_H

#include "raylib.h"

// Paleta.
#define UI_GOLD        (Color){ 212, 166,  72, 255 }
#define UI_GOLD_LIGHT  (Color){ 248, 218, 132, 255 }
#define UI_GOLD_DARK   (Color){ 120,  82,  28, 255 }
#define UI_SILVER      (Color){ 192, 192, 184, 255 }
#define UI_SILVER_LIGHT (Color){ 236, 236, 228, 255 }
#define UI_SILVER_DARK (Color){  92,  92,  90, 255 }
#define UI_TURQUOISE   (Color){  68, 198, 196, 255 }
#define UI_TURQ_LIGHT  (Color){ 150, 232, 224, 255 }
#define UI_TURQ_DARK   (Color){  26, 110, 116, 255 }
#define UI_CARNELIAN   (Color){ 184,  62,  40, 255 }
#define UI_LAPIS       (Color){  44,  66, 136, 255 }
#define UI_LEATHER     (Color){  54,  37,  26, 244 }
#define UI_LEATHER_CRACK (Color){ 33, 22, 15, 244 }
#define UI_VELVET      (Color){  16,  15,  18, 255 }
#define UI_BONE        (Color){ 242, 228, 192, 255 }
#define UI_BONE_DIM    (Color){ 200, 186, 152, 255 }

// Oro para lo principal; plata para lo secundario o inactivo.
typedef enum { UI_METAL_GOLD, UI_METAL_SILVER } UiMetal;

// Grosor del marco + banda de granulado: el contenido empieza a esta distancia del borde.
#define UI_PANEL_INSET 11

// Placa completa: cuero, marco biselado y banda de granulado con turquesas.
void ui_panel(Rectangle r, UiMetal metal);
// Franja simple (sin banda): cuero con un filete de metal y granulado arriba.
void ui_strip(Rectangle r, UiMetal metal);
// Barra con incrustacion tabicada (cloisonne): value01 en [0, 1].
void ui_bar(int x, int y, int w, float value01, Color inlay, UiMetal metal);
// Filete de granulado horizontal, para separar secciones.
void ui_divider(int x, int y, int w, UiMetal metal);
// Texto con sombra de grabado.
void ui_text(const char *text, int x, int y, int size, Color color);
void ui_text_centered(const char *text, int cx, int y, int size, Color color);

#endif
