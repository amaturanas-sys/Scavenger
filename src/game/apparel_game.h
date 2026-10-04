// La ropa en el juego (src/sim/apparel.h):
//  - el jugador se viste en la pestaña Ropa del equipo (P): lo que abriga, la sombra
//    contra el sol y el escarmiento de las pieles de depredador;
//  - los NPCs se cambian solos segun el tiempo, con lo que llevan en la mochila y, en su
//    campamento, con la ropa del acopio; si no tienen con que, pasan frio o calor (baja
//    la moral) y se les ve un aviso sobre la cabeza.
#ifndef ESTEPA_APPAREL_GAME_H
#define ESTEPA_APPAREL_GAME_H

#include <stddef.h>

#include "game/actions_game.h"
#include "sim/climate.h"
#include "sim/troop.h"

void ag_update(GameActions *ga, Troop *troop, const Climate *c, unsigned seed, float world_time, float dt, char *log, size_t len);
// Escarmiento de alguien cerca de un enemigo: el del jugador (member_id 0) o el de un integrante.
float ag_dread(const GameActions *ga, const Troop *troop, int member_id);
// Avisos de frio o calor sobre la cabeza de la gente. Despues de EndMode3D.
void ag_draw_overlay(const GameActions *ga, const Troop *troop, Camera3D cam, int w, int h);

#endif
