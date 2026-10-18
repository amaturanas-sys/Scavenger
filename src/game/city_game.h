// Las ciudades y aldeas en el juego (fase 8a, src/sim/city.h): la economia de cada asentamiento,
// dia a dia; el tributo al kanato de la estepa (se paga en su capital: src/game/trade_game.c); las
// bajas de la guardia que sale a defender los campos; y el dibujo, que crece con la gente: el
// palacio y su corte, las casas, la fundicion, el cuartel y la carpinteria, los campos con sus
// surcos (segun la estacion, y negros si los arrasaron), los corrales, los labradores de dia, la
// guardia de las puertas y las tiendas de los mercenarios.
// El ejercito y el asedio (fase 8b, src/sim/army.h):
//  - las expediciones de castigo: con la relacion por el suelo (o mucho rencor), sale un ejercito
//    contra el campamento: en el mundo si el jugador esta cerca, si no lo cuenta el guardian;
//  - la guerra cierra las puertas de la ciudad; la muralla corta el paso (al jugador, a las flechas y
//    a los enemigos, que van por las puertas). Se entra trepando (el gancho o las escalas), quemando
//    una puerta (flechas encendidas o la antorcha) o rindiendola;
//  - la guarnicion sale a pelear cuando te acercas; el cerco (un campamento de la tribu cerca) corta el
//    grano; las flechas encendidas que caen adentro queman el granero;
//  - tomada, la ciudad paga tributo a la tribu cada estacion y su fundicion es tu taller; si es la
//    capital del kanato, ya no le debes tributo.
#ifndef ESTEPA_CITY_GAME_H
#define ESTEPA_CITY_GAME_H

#include <stdbool.h>
#include <stddef.h>

#include "game/actions_game.h"
#include "game/combat_game.h"
#include "game/player.h"
#include "sim/army.h"
#include "sim/city.h"
#include "sim/troop.h"
#include "world/terrain.h"

#define LADDER_ID "utileria.herramienta.escala_asedio"

// Un dia nuevo: la economia de todas las ciudades (las cercadas, sin cosecha); al empezar una
// estacion, si la anterior paso sin tributo, la relacion con el kanato baja, y las ciudades tomadas
// pagan el suyo. Y las expediciones de castigo que salen.
void ct_new_day(GameActions *ga, const Terrain *t, Troop *troop, int day, char *log, size_t len);
// Cada cuadro (despues del combate): arma las ciudades que falten (partida nueva o de antes de la
// v0.9.0), descuenta los soldados que caen, la expedicion en marcha, la guarnicion que sale, la
// muralla que corta el paso, las puertas que arden y las flechas encendidas que caen adentro.
void ct_update(GameActions *ga, Combat *cb, Troop *troop, const Props *props, const Terrain *t, Player *p, int day, float dt, char *log,
               size_t len);
// El trazado del asentamiento idx (se rehace cuando cambia su gente); n: cuantos lotes.
const CityLot *ct_lots(const GameActions *ga, const Terrain *t, int idx, int *n);
// El asentamiento cuyo campo pisa (x, z), o -1; field (si no es NULL): que campo (su numero entre los
// campos del trazado).
int ct_field_at(const GameActions *ga, const Terrain *t, float x, float z, int *field);
// Donde sale la guardia de la ciudad idx para ir hacia (x, z): la puerta mas cercana (la aldea, su
// centro).
Vector3 ct_gate_toward(const Terrain *t, int idx, float x, float z);
// ¿Esta en guerra con la tribu la ciudad idx? (src/sim/army.h: city_at_war)
bool ct_at_war(const GameActions *ga, const Terrain *t, const Troop *troop, int idx);
// ¿Tomo la tribu la capital del kanato? Entonces ya no se le debe tributo ni cuenta la relacion.
bool ct_khanate_fallen(const GameActions *ga, const Terrain *t);
// La puerta cerrada de una capital en guerra a menos de r de (x, z) (por fuera o por dentro), o -1;
// city: de que ciudad.
int ct_gate_near(const GameActions *ga, const Terrain *t, const Troop *troop, float x, float z, float r, int *city);
// La muralla de una capital en el camino de from a to: el rodeo por la puerta que conviene (los
// enemigos entran y salen por ahi); si no hay muralla en medio, to.
Vector3 ct_route(const GameActions *ga, const Terrain *t, Vector3 from, Vector3 to);
// Una flecha de from a to: ¿se clava en una muralla o en una puerta cerrada? at: donde. Encendida,
// prende la puerta.
bool ct_shot_wall(const GameActions *ga, const Terrain *t, Vector3 from, Vector3 to, bool burning, Vector3 *at);
// La trepa contra la muralla de una capital: ¿hay muralla delante, a tiro? top, over y to: por donde
// sube, pasa y baja (src/game/actions_game.h: ga_climb).
bool ct_wall_ahead(const GameActions *ga, const Terrain *t, const Player *p, Vector3 *top, Vector3 *over, Vector3 *to);
// F junto a la muralla de una ciudad en guerra: subir por una escala apoyada, apoyar la tuya o
// prender la puerta con la antorcha. true si hizo algo.
bool ct_try_interact(GameActions *ga, const Props *props, const Terrain *t, Player *p, char *log, size_t len);
// ¿Esta (x, z) dentro de una ciudad tomada por la tribu? Su fundicion es tu taller (src/game/forge_game.c).
bool ct_workshop_at(const GameActions *ga, const Terrain *t, float x, float z);
// Exigir la rendicion (el dialogo de la ciudad, src/game/trade_game.c): si no le queda guarnicion,
// grano o paciencia, se rinde y es de la tribu; si no, dice por que no.
void ct_demand_surrender(GameActions *ga, Combat *cb, const Terrain *t, Troop *troop, int idx, int day, char *log, size_t len);
// Como esta la ciudad (para el dialogo del mercado): su gente, el granero, el tesoro y los escribas,
// la armeria y el ejercito, el rencor y, en el kanato, la relacion y el tributo; en guerra, el asedio.
// UTF-8.
void ct_report(const GameActions *ga, const Terrain *t, const Troop *troop, int idx, int day, char *out, size_t len);
// Lo que hace F junto a la muralla (la escala, la puerta), o "".
const char *ct_hint(void);
void ct_draw_hud(int w, int h);
// Las ciudades cercanas. world_time: la estacion y si es de dia (los labradores).
void ct_draw_world(const GameActions *ga, const Terrain *t, const Player *p, float time, float world_time);
// Pruebas: --asedio (puerta, trepa, escala, fuego, rendicion, tomada) y --expedicion.
void ct_test(GameActions *ga, Combat *cb, Troop *troop, const Props *props, const Terrain *t, Player *p, const char *what, int day, char *log,
             size_t len);

#endif
