// Despachar seguidores y mandar mensajeros (src/sim/travel.h):
//  - Mayus+Y: ordenes a la escolta. Despachar a un campamento o a un sitio marcado en
//    el mapa (al menos uno de los elegidos tiene que conocerlo), o mandar un mensajero
//    a un campamento para que vuelva con refuerzos;
//  - los despachados se alejan hacia su destino (a pie o a caballo, como iban con el
//    jugador) y se pierden en el horizonte; aparecen en el destino al llegar, pero el
//    jugador solo se entera de como les fue al ir alli;
//  - la gente aprende los sitios por donde pasa (y su campamento).
#ifndef ESTEPA_TRAVEL_GAME_H
#define ESTEPA_TRAVEL_GAME_H

#include <stddef.h>

#include "game/actions_game.h"
#include "sim/memory_map.h"

void trv_update(GameActions *ga, Troop *troop, const MemoryMap *mem, const Player *p, const Terrain *t, bool night, bool input_ok,
                float dt, char *log, size_t len);
// ¿Esta de viaje (no se ve ni se le pueden dar ordenes)?
bool trv_away(const Member *m);
// La escolta que parte, a caballo si el jugador iba montado. Dentro de BeginMode3D.
void trv_draw_world(const GameActions *ga, const Troop *troop, float time);
int trv_count(const GameActions *ga); // viajes en curso

#endif
