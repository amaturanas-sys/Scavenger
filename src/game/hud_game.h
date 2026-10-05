// El HUD de juego, al modo de un juego de rol de la estepa (cuero, oro y turquesa de los
// kurganes escitas):
//  - arriba a la derecha, el minimapa y bajo el, la placa de las constantes: vida, calor del
//    cuerpo, sed y aguante (cada una con su icono, su estado y su barra);
//  - abajo a la izquierda, la barra rapida: casillas 1..9 con armas, objetos o habilidades;
//  - en el borde izquierdo, una columna de acciones rapidas (menu, beber, vendar, fogata,
//    tienda, lazo, trepa, hervir agua).
// Las casillas responden al raton y a los toques (tableta): no hace falta teclado para usarlas.
#ifndef ESTEPA_HUD_GAME_H
#define ESTEPA_HUD_GAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

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
// o una habilidad. Su tecla (1..9) o un toque la usa; vacia (o Mayus+tecla, o clic derecho)
// abre el selector. hud_update atiende el teclado; hud_quickbar dibuja y atiende los toques.
#define HUD_QUICK_SLOTS 9
void hud_reset(void); // las casillas de serie (partida nueva)
void hud_update(GameActions *ga, const Props *props, const Player *p, bool input_ok, char *log, size_t len);
void hud_quickbar(GameActions *ga, const Props *props, const Player *p, int x, int y);
bool hud_picker_open(void);
void hud_open_picker(int slot); // 0..8 (prueba: --selector N)
void hud_draw_picker(GameActions *ga, const Props *props, const Player *p, int w, int h);
// Bloque opcional de la partida guardada (las casillas).
bool hud_write(FILE *f);
void hud_read(FILE *f);
// Columna de acciones (borde izquierdo): dibuja y atiende los clics o toques.
void hud_action_column(const GameActions *ga, int x, int y);
// El puntero esta sobre una casilla del HUD (ese clic no es un golpe).
bool hud_pointer_over(void);
void hud_frame_begin(void);

#endif
