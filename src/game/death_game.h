// La muerte del jugador y el regreso.
//  - El lugar de descanso: el ultimo campamento (o la ultima tienda o casa de la tribu) donde el
//    jugador se quedo unos segundos. Al empezar, el campamento de la tribu.
//  - Al morir (desangrado, o abatido sin nadie que lo socorra), en el suelo quedan una calavera y
//    un par de huesos, y el jugador vuelve a su lugar de descanso.
// El estado va en un bloque opcional al final de la partida guardada: las partidas de antes
// siguen cargando (sin restos, con el campamento como lugar de descanso).
#ifndef ESTEPA_DEATH_GAME_H
#define ESTEPA_DEATH_GAME_H

#include <stdbool.h>
#include <stdio.h>

#include "game/actions_game.h"
#include "raylib.h"
#include "world/props.h"
#include "world/terrain.h"

#define DG_REMAINS_MAX 16
#define DG_REST_SECONDS 5.0f // s quieto (o cerca) en un campamento o una tienda para descansar alli

void dg_reset(void);
// Sigue donde descansa el jugador (vivo): campamentos y viviendas de la tribu.
void dg_track_rest(const GameActions *ga, const Props *props, Vector3 pos, Vector3 camp_fire, float dt);
// Donde vuelve el jugador al morir (en seco, junto al lugar de descanso) y su nombre.
Vector3 dg_rest_point(const Terrain *t, Vector3 camp_fire);
const char *dg_rest_name(void);
// Deja una calavera y dos huesos en pos (sobre el suelo).
void dg_leave_remains(Vector3 pos, float yaw);
int dg_remains_count(void);
int dg_deaths(void);
void dg_draw_world(const Terrain *t);

// Bloque opcional de la partida guardada.
bool dg_write(FILE *f);
void dg_read(FILE *f); // si no esta (partida vieja), queda en dg_reset

#endif
