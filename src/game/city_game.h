// Las ciudades y aldeas en el juego (fase 8a, src/sim/city.h): la economia de cada asentamiento,
// dia a dia; el tributo al kanato de la estepa (se paga en su capital: src/game/trade_game.c); las
// bajas de la guardia que sale a defender los campos; y el dibujo, que crece con la gente: el
// palacio y su corte, las casas, la fundicion, el cuartel y la carpinteria, los campos con sus
// surcos (segun la estacion, y negros si los arrasaron), los corrales, los labradores de dia, la
// guardia de las puertas y las tiendas de los mercenarios.
#ifndef ESTEPA_CITY_GAME_H
#define ESTEPA_CITY_GAME_H

#include <stdbool.h>
#include <stddef.h>

#include "game/actions_game.h"
#include "game/combat_game.h"
#include "game/player.h"
#include "sim/city.h"
#include "sim/troop.h"
#include "world/terrain.h"

// Un dia nuevo: la economia de todas las ciudades; al empezar una estacion, si la anterior paso sin
// tributo, la relacion con el kanato baja.
void ct_new_day(GameActions *ga, const Terrain *t, Troop *troop, int day, char *log, size_t len);
// Cada cuadro: arma las ciudades que falten (partida nueva o de antes de la v0.9.0) y descuenta los
// soldados de su guardia que caen.
void ct_update(GameActions *ga, Combat *cb, const Terrain *t);
// El trazado del asentamiento idx (se rehace cuando cambia su gente); n: cuantos lotes.
const CityLot *ct_lots(const GameActions *ga, const Terrain *t, int idx, int *n);
// El asentamiento cuyo campo pisa (x, z), o -1; field (si no es NULL): que campo (su numero entre los
// campos del trazado).
int ct_field_at(const GameActions *ga, const Terrain *t, float x, float z, int *field);
// Donde sale la guardia de la ciudad idx para ir hacia (x, z): la puerta mas cercana (la aldea, su
// centro).
Vector3 ct_gate_toward(const Terrain *t, int idx, float x, float z);
// Como esta la ciudad (para el dialogo del mercado): su gente, el granero, el tesoro y los escribas,
// la armeria y el ejercito, el rencor y, en el kanato, la relacion y el tributo. UTF-8.
void ct_report(const GameActions *ga, const Terrain *t, const Troop *troop, int idx, int day, char *out, size_t len);
// Las ciudades cercanas. world_time: la estacion y si es de dia (los labradores).
void ct_draw_world(const GameActions *ga, const Terrain *t, const Player *p, float time, float world_time);

#endif
