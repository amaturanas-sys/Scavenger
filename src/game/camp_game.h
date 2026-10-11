// Los campamentos en el juego (src/sim/camps.h):
//  - fundar: al levantar una estructura basica (tienda o refugio) lejos de los demas
//    campamentos, hay que nombrar guardian a alguien de la escolta;
//  - el guardian (F junto a el): ordenar obras con el acopio del campamento, elegir
//    quien sale de escolta, la ventana de pobladores, capacitar en un oficio (soldado,
//    herrero, druida, orfebre, pastor, cazador, explorador), ver el estado y disolver;
//  - la ventana de pobladores (fase 4): la gente del campamento con su oficio, salud, animo
//    y tarea; se eligen uno o dos y salen a buscar reclutas, explorar recursos (marcan el
//    mapa), cazar, o se quedan a pastorear o hacer guardia. Al volver, el informe va al
//    registro y a la ventana;
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
// now: tiempo de juego (s); de el salen el dia, la estacion y la noche. mem: el mapa de memoria, donde
// los exploradores marcan lo que encuentran.
void cg_update(GameActions *ga, Troop *troop, Props *props, const Player *p, const Terrain *t, MemoryMap *mem, float now, float dt, char *log,
               size_t len);
// Para el HUD: el campamento donde esta el jugador ("" fuera).
const char *cg_here_name(const GameActions *ga);
// Pruebas (--pobladores, --tareas): abrir la ventana de pobladores con a y b elegidos (ids; 0:
// nadie), y mandar a a (y b) a una tarea del campamento k como desde la ventana.
void cg_show_people(GameActions *ga, int a, int b);
bool cg_start_task(GameActions *ga, Troop *troop, const Terrain *t, int k, CampTaskKind kind, int a, int b, char *log, size_t len);

#endif
