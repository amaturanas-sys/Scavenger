// El HUD de juego, al modo de un juego de rol de la estepa (cuero, oro y turquesa de los
// kurganes escitas):
//  - arriba a la derecha, el minimapa y bajo el, la placa de las constantes: vida, calor del
//    cuerpo, sed y aguante (cada una con su icono, su estado y su barra);
//  - abajo a la izquierda, la barra rapida: casillas 1..9 con armas, objetos o habilidades;
//  - en el borde izquierdo, una columna de acciones rapidas (menu, beber, vendar, fogata,
//    tienda, lazo, trepa, hervir agua, mochila y, con arco, encender la flecha);
//  - abajo a la derecha, los cuatro botones de combate (src/game/combat_game.c) y, encima, las
//    habilidades activas (src/game/talents_game.c).
// Las casillas responden al raton y a los toques (tableta): no hace falta teclado para usarlas.
#ifndef ESTEPA_HUD_GAME_H
#define ESTEPA_HUD_GAME_H

#include <stdbool.h>
#include <stddef.h>

#include "game/actions_game.h"
#include "raylib.h"
#include "ui/icons.h"

// La placa de las constantes bajo el minimapa (right_x: su borde derecho).
void hud_vitals_frame(int right_x, int y, int rows);
// Una fila de la placa: estado a la derecha, icono y barra debajo.
void hud_vital(IconId icon, const char *text, Color text_col, float value01, Color bar_col, int right_x, int y);
// La fila del aguante (src/game/player.c).
void hud_stamina(int right_x, int y);

// Barra rapida (abajo a la izquierda): 9 casillas con una empuñadura, un objeto del inventario
// o una habilidad. Su tecla (1..9) o un toque la usa (un arma: empuñar; otra vez, enfundar);
// Mayus+tecla pasa el arma empuñada a la otra mano; vacia (o Alt+tecla, o clic derecho) abre
// el selector. hud_update atiende el teclado; hud_quickbar dibuja y atiende los toques.
#define HUD_QUICK_SLOTS 9
typedef enum { QB_EMPTY, QB_GRIP, QB_ITEM, QB_SKILL } QbKind;
typedef struct {
    int kind;            // QbKind
    int grip;            // QB_GRIP: indice de la empuñadura; QB_SKILL: AbilityId
    char id[INV_ID_LEN]; // QB_ITEM
} QbSlot;
void hud_reset(void); // las casillas de serie (partida nueva)
const QbSlot *hud_slots(void); // las casillas, para guardarlas (src/game/save_game.c)
void hud_update(GameActions *ga, const Props *props, const Player *p, bool input_ok, char *log, size_t len);
void hud_quickbar(GameActions *ga, const Props *props, const Player *p, int x, int y);
bool hud_picker_open(void);
void hud_open_picker(int slot); // 0..8 (prueba: --selector N)
void hud_draw_picker(GameActions *ga, const Props *props, const Player *p, int w, int h);
// Una partida cargada: sus n casillas (NULL o dañadas: las de serie).
void hud_load(const QbSlot *s, int n);
// Columna de acciones (borde izquierdo): dibuja y atiende los clics o toques.
void hud_action_column(const GameActions *ga, int x, int y);
// El puntero esta sobre una casilla del HUD (ese clic no es un golpe): lo que el HUD ocupo en
// el cuadro anterior.
bool hud_pointer_over(void);
void hud_frame_begin(void);
// Para otras piezas del HUD (los botones de combate, src/game/combat_game.c): una placa y una
// casilla que cuentan como HUD (true: el puntero esta encima).
void hud_plate(int x, int y, int w, int h);
bool hud_hover(Rectangle r);

#endif
