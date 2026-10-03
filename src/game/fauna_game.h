// Fauna en el juego. Conecta src/sim/animals.h con el mundo, el combate y la tribu:
//  - aparicion por bioma (estepa, desierto, frio) alrededor del jugador; de noche,
//    mas cazadores; los lejanos desaparecen; los cadaveres se quedan un rato;
//  - los animales atacan a las personas (jugador, tribu, enemigos) con armadura y
//    zonas del cuerpo; los domados defienden al jugador y pelean con los enemigos;
//  - el cuervo domado explora (revela el mapa) y el halcon caza liebres;
//  - K: dar de comer a un animal atado (domarlo), despiezar un cadaver, ordeñar
//    el ganado (Mayús+K: sacrificarlo);
//  - el ganado (cabras, becerros) sigue al jugador que camina cerca: pastoreo;
//  - enjambres (src/sim/swarms.h): bancos de peces en los lagos (se pescan con un
//    golpe o una flecha), colmenas (miel con humo) y avisperos, mosquitos junto al
//    agua al anochecer y moscas sobre los cadaveres; las picaduras envenenan.
#ifndef ESTEPA_FAUNA_GAME_H
#define ESTEPA_FAUNA_GAME_H

#include <stddef.h>

#include "game/actions_game.h"
#include "game/combat_game.h"
#include "sim/animals.h"
#include "sim/memory_map.h"

// Animales iniciales: el ganado del campamento y algunas manadas cerca.
void fg_init(GameActions *ga, const Terrain *t);
// Un grupo de la especie en (x, z); devuelve cuantos aparecieron.
int fg_spawn_group(GameActions *ga, Species s, float x, float z);
// Prueba: aparece un grupo por nombre ("lobos" o el final del id: "tigre", "jabali"...).
bool fg_spawn_named(GameActions *ga, const char *what, const Player *p, float dist, char *log, size_t len);
// temp: °C ahora (los insectos y mosquitos dependen del calor); la noche y el atardecer salen de now.
void fg_update(GameActions *ga, Combat *cb, Player *p, Troop *troop, const Terrain *t, MemoryMap *mem, float now,
               float temp, bool input_ok, float dt, char *log, size_t len);
void fg_new_day(GameActions *ga);

// Para el combate: el animal al alcance de un golpe delante de pos (o -1).
int fg_melee_target(const GameActions *ga, const Terrain *t, Vector3 pos, float yaw, float reach, float *dist);
// Un proyectil (de a a a + d) contra los animales: indice o -1, con la fraccion y la zona.
int fg_raycast(const GameActions *ga, const Terrain *t, V3 a, V3 d, float *tt, int *part);
// Herir un animal. attacker: 0 jugador, >0 id del integrante, -1 otro. Escribe el registro si lo hirio el jugador.
void fg_hurt(GameActions *ga, int idx, float dmg, WoundKind w, int part, int attacker, char *log, size_t len);
// Pescar con un golpe delante (lanza: mas facil). true si habia peces (pescara o no).
bool fg_fish(GameActions *ga, const Terrain *t, Vector3 pos, float yaw, float reach, bool spear, char *log, size_t len);
// Un proyectil que cae al agua en at puede atravesar un pez.
bool fg_shot_water(GameActions *ga, const Terrain *t, Vector3 at, char *log, size_t len);
// Un golpe a un nido (colmena, avispero) a menos de reach: se enfadan. true si habia uno.
bool fg_poke_nest(GameActions *ga, Vector3 pos, float reach);
// Animal hostil (salvaje que ataca o puede atacar) mas cercano a pos, o -1.
int fg_threat_near(const GameActions *ga, Vector3 pos, float range, float *dist);

void fg_draw_world(const GameActions *ga, Props *props, const Terrain *t, float time);
void fg_draw_overlay(const GameActions *ga, const Terrain *t, Camera3D cam, int w, int h);
void fg_draw_hud(const GameActions *ga, int w, int h);

#endif
