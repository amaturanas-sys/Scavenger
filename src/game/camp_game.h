// Los campamentos en el juego (src/sim/camps.h):
//  - fundar: al levantar una estructura basica (tienda o refugio) lejos de los demas
//    campamentos, hay que nombrar guardian a alguien de la escolta;
//  - el guardian (F junto a el): ordenar obras con el acopio del campamento, elegir
//    quien sale de escolta, reclutar gente nueva, capacitar en un oficio (soldado,
//    herrero, druida, orfebre, pastor, cazador, explorador), ver el estado y disolver;
//  - las tareas avanzan con la gente ociosa del campamento (y los maestros del oficio);
//  - disolver: se queman las estructuras, se carga en la carreta (si esta cerca) lo que
//    quepa y la gente sigue al jugador.
#ifndef ESTEPA_CAMP_GAME_H
#define ESTEPA_CAMP_GAME_H

#include <stddef.h>

#include "game/actions_game.h"

// F: hablar con el guardian mas cercano (a menos de 3 m). true si habia uno.
bool cg_try_talk(GameActions *ga, const Troop *troop, const Player *p);
// Una estructura basica quedo en (x, z): si esta lejos de todo campamento, se ofrece fundar uno.
void cg_basic_structure(GameActions *ga, float x, float z);
void cg_update(GameActions *ga, Troop *troop, Props *props, const Player *p, int day, float dt, char *log, size_t len);
// Para el HUD: el campamento donde esta el jugador ("" fuera).
const char *cg_here_name(const GameActions *ga);

#endif
