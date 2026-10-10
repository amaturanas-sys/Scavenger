// Entrada: cada accion global acepta varias teclas. Los teclados de tablet Android a
// menudo no tienen F1..F12 y su Esc llega como el boton Atras (KEY_BACK), o no llega:
// por eso cada accion tiene una alternativa con Ctrl y los botones tactiles de la esquina.
#ifndef ESTEPA_INPUT_H
#define ESTEPA_INPUT_H

#include <stdbool.h>

#include "sim/keymap.h"

typedef enum {
    IN_PAUSE,    // Esc · Atras · Ctrl+P · boton tactil de pausa
    IN_BACK,     // cerrar un menu: Esc · Atras · Retroceso
    IN_HELP,     // controles en pantalla: F1 · Ctrl+H
    IN_ORBITAL,  // vista orbital: F5 · Ctrl+M
    IN_ABILITY1, // habilidades activas: F2..F4 · Ctrl+1..3
    IN_ABILITY2,
    IN_ABILITY3,
    IN_DIAG,     // diagnostico (GPU, assets, teclado): Ctrl+D
    IN_COUNT
} InputAction;

// Ctrl apretado: las teclas simples (H, M, P, 1..3) no hacen lo suyo.
bool input_ctrl(void);
// Teclas de prueba (1..8 tropa, 9 enemigos): solo con el panel de diagnostico (Ctrl+D) abierto.
void input_set_debug(bool on);
bool input_debug(void);
bool input_pressed(InputAction a);
// Un toque o clic simulado (botones tactiles): se consume en el proximo input_pressed.
void input_inject(InputAction a);
// Que tecla se pulso por ultima vez (para «Probar teclado»): codigo raylib, o 0.
int input_last_key(void);
void input_update(void); // una vez por cuadro, antes de leer acciones
// Nombre de las teclas de una accion ("Esc / Atrás / Ctrl+P"), UTF-8.
const char *input_keys_text(InputAction a);

// Las acciones del juego (src/sim/keymap.h): pulsada en este cuadro, mantenida, soltada.
unsigned input_mods(void); // KM_SHIFT | KM_CTRL | KM_ALT apretados
bool input_action_pressed(KeyAction a);
bool input_action_down(KeyAction a);
bool input_action_released(KeyAction a);
// El boton derecho: mantenido sin arrastrar es cubrirse; arrastrado, gira la camara (dx, dy:
// lo que se movio el raton en este cuadro).
bool input_right_hold(void);
bool input_right_drag(float *dx, float *dy);

#endif
