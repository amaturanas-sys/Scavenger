// La forja en el juego (fase 7b, src/sim/forge.h): las vetas de hierro del mundo (se ven, se
// descubren al pasar y se pican), las estaciones de la forja (la forja movil a mano, los hornos y
// el taller) y el temple al terminar la receta.
#ifndef ESTEPA_FORGE_GAME_H
#define ESTEPA_FORGE_GAME_H

#include <stdbool.h>
#include <stddef.h>

#include "game/actions_game.h"
#include "game/player.h"
#include "raylib.h"
#include "sim/forge.h"
#include "sim/memory_map.h"
#include "world/props.h"
#include "world/terrain.h"

// Las vetas del mundo del terreno (calculadas una vez por semilla); n: cuantas.
const IronSite *forge_sites(const Terrain *t, int *n);
// La veta junto a (x, z) (a menos de IRON_NEAR), o NULL.
const IronSite *forge_site_near(const Terrain *t, float x, float z);
// Picar la veta (ACTION_MINE terminada): el mineral, a lo que llevas (o al acopio).
void forge_mine(GameActions *ga, const Props *props, const Player *p, char *log, size_t len);
// Cada cuadro: el jugador que pasa junto a una veta que no estaba en su mapa la marca (M).
void forge_update(const Terrain *t, const Player *p, MemoryMap *mem, float now, char *log, size_t len);
// Las vetas cercanas: rocas herrumbradas (cielo abierto) o barro rojizo (pantano). Y abajo, junto a
// una, lo que hace F.
void forge_draw_world(const Terrain *t, Vector3 eye);
void forge_draw_hud(const Terrain *t, const Player *p, int w, int h);
// ¿Esta la estacion que pide building? (una receta, o una reparacion si repair). La forja movil cuenta
// si esta a mano (src/game/inventory_game.h); los hornos y el taller, si los hay (near: cerca del jugador).
bool forge_station(const GameActions *ga, const Props *props, const Player *p, const char *building, bool repair, bool near);
// El temple (una receta con fails): ¿sale bien? Lo deciden los herreros de la tribu y las manos del clan.
bool forge_temper_roll(GameActions *ga, const Troop *troop);

#endif
