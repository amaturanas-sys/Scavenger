// Inventario y equipo en el juego (src/sim/storage.h, src/sim/armor.h, src/sim/loadout.h):
//  - lo que se lleva encima: bolsillos (lo pequeño) y mochila; las alforjas de las
//    monturas ensilladas; la carreta de la tribu; el acopio y la armeria del campamento
//    (solo cerca de cada uno);
//  - I: menu de inventario, dos columnas para pasar cosas de un contenedor a otro;
//  - P: menu de equipo: las nueve piezas de armadura y los tres amuletos, con su estado
//    y su proteccion; poner, cambiar y quitar piezas; las heridas al lado;
//  - lo que se gasta en el campo (flechas, hierbas, carne) sale de lo que llevas encima
//    (o de lo que tengas cerca); lo que se consigue (caza, pesca, botin) va a la mochila.
#ifndef ESTEPA_INVENTORY_GAME_H
#define ESTEPA_INVENTORY_GAME_H

#include <stdbool.h>
#include <stddef.h>

#include "game/actions_game.h"
#include "game/combat_game.h"

#define CAMP_STORE_RADIUS 45.0f // m: el acopio y la armeria se alcanzan dentro del campamento

void ig_init(GameActions *ga, Props *props, const Terrain *t);
bool ig_blocks_input(const GameActions *ga); // un menu abierto
// Cuanto hay de algo a mano (encima, en las alforjas o la carreta cercanas, o en el campamento si estas en el).
int ig_count(const GameActions *ga, const Props *props, const Player *p, const char *id);
// Gasta n de lo que haya a mano (primero lo que llevas encima). Devuelve cuantas gasto.
int ig_use(GameActions *ga, const Props *props, const Player *p, const char *id, int n);
// Guarda n donde quepa (mochila, bolsillos, alforjas, carreta, campamento). Devuelve cuantas cupieron.
int ig_store(GameActions *ga, const Props *props, const Player *p, const char *id, int n, float condition);
float ig_carried_kg(const GameActions *ga);
float ig_speed_scale(const GameActions *ga); // la carga frena
// Suma de los amuletos colgados.
float ig_stat(const GameActions *ga, Stat s);
void ig_update(GameActions *ga, Combat *cb, Props *props, const Player *p, bool input_ok, char *log, size_t len);
void ig_draw(const GameActions *ga, const Combat *cb, const Props *props, const Player *p, int w, int h);

#endif
