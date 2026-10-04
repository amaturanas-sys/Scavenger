// De donde salen las piedras preciosas y los metales de las joyas (src/sim/jewelry.h):
//  - rocas pequeñas y medianas de la estepa: se rompen a golpes (mejor con maza o
//    golpe pesado) y sueltan piedras, mineral y a veces una gema;
//  - arrecifes de coral bajo el agua de los lagos: F junto a ellos (coral y perlas);
//  - cavar una trinchera desentierra a veces lapislazuli, ambar, plata u oro;
//  - cribar en el agua (accion del menu Tab) en la orilla: piedras rodadas y oro.
#ifndef ESTEPA_GEMS_GAME_H
#define ESTEPA_GEMS_GAME_H

#include <stdbool.h>
#include <stddef.h>

#include "game/actions_game.h"

void gems_scatter(GameActions *ga, Props *props, const Terrain *t);
// F junto a un coral (a menos de 2.5 m). true si habia uno.
bool gems_coral(GameActions *ga, Props *props, const Player *p, char *log, size_t len);
// Un golpe a una roca delante (dmg: daño del golpe). true si habia roca.
bool gems_hit_rock(GameActions *ga, Props *props, const Player *p, float dmg, float reach, char *log, size_t len);
void gems_dig(GameActions *ga, const Props *props, const Player *p, char *log, size_t len);
bool gems_water_ahead(const Terrain *t, const Player *p);
void gems_pan(GameActions *ga, const Props *props, const Player *p, char *log, size_t len);

#endif
