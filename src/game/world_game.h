// El mundo en el juego (src/sim/world.h): las regiones, los asentamientos de los reinos
// vecinos y las guaridas de las fieras.
//  - al cruzar a otra region, un aviso con su nombre y el reino que la gobierna;
//  - los asentamientos (aldeas y capitales) se dibujan al estilo de su region: yurtas en la
//    estepa, cabañas de troncos en el bosque, casas de piedra en el altiplano, casas largas
//    en la costa, adobe en el desierto; las capitales, amuralladas. Al llegar quedan
//    marcados en el mapa;
//  - las guaridas: una cueva entre rocas; cerca, la fauna que aparece es la suya.
#ifndef ESTEPA_WORLD_GAME_H
#define ESTEPA_WORLD_GAME_H

#include <stddef.h>

#include "game/actions_game.h"
#include "game/combat_game.h"
#include "sim/memory_map.h"

//  - las tribus nomadas: las rivales salen al paso con sus jinetes; las neutrales observan;
//    las amigables comparten noticias (marcan en el mapa una estructura cercana);
//  - las estructuras (kurganes, balbales, piedras de ciervos, fortalezas, ciudades enterradas,
//    petroglifos, caravasares, torres, ovoos, pozos, embarcaderos): al verlas, al mapa.
void wd_update(GameActions *ga, Combat *cb, const Terrain *t, const Player *p, MemoryMap *mem, char *log, size_t len);
// Asentamientos y guaridas cercanos. Dentro de BeginMode3D.
void wd_draw_world(const GameActions *ga, const Terrain *t, const Player *p, float time);
// La guarida cercana a (x, z) (a menos de radius), o -1: su especie aparece alli.
int wd_den_near(const Terrain *t, float x, float z, float radius);

#endif
