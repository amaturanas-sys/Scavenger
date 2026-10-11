// La escolta, los guardias y los pobladores en una pelea (C puro): la fase 3 del plan grande
// (docs/PLAN_GRAN_ACTUALIZACION.md). Deciden a quien va cada uno; el juego los mueve y aplica
// los golpes con las mismas reglas que el jugador (src/sim/melee.h).
//  - Ordenes a la escolta: atacar (al objetivo del jugador o al mas cercano), defender (se
//    quedan junto al jugador y paran a quien se le acerque) o seguir sin atacar. Herido, se
//    retira detras del jugador.
//  - Guardias: hacen la ronda en un anillo de SQUAD_RING_MIN a SQUAD_RING_MAX m alrededor del
//    campamento, con puntos que rotan, y salen al encuentro de quien cruza el anillo.
//  - Pobladores: los que no pelean se esconden en las yurtas cuando hay enemigos cerca.
#ifndef ESTEPA_SQUAD_H
#define ESTEPA_SQUAD_H

#include <stdbool.h>

#include "targeting.h"

typedef enum { ORDER_ATTACK, ORDER_DEFEND, ORDER_FOLLOW, ORDER_COUNT } SquadOrder; // se guarda: crece al final

#define SQUAD_ATTACK_RANGE 14.0f // m: atacar, al mas cercano a el o al jugador
#define SQUAD_LOCK_RANGE 30.0f   // m: el objetivo del jugador, si no esta mas lejos de el
#define SQUAD_DEFEND_RADIUS 4.0f // m alrededor del jugador: defender para a quien entra
#define SQUAD_RETREAT_HP 0.3f    // con menos vida, se retira
#define SQUAD_RING_MIN 40.0f     // m: la ronda de los guardias
#define SQUAD_RING_MAX 60.0f
#define SQUAD_GUARD_REACT 35.0f  // m: hasta donde sale un guardia a interceptar
#define SQUAD_ALARM 30.0f        // m: un enemigo asi de cerca del campamento (o del poblador) da la alarma

typedef struct {
    int foe;      // el enemigo al que va (indice en los candidatos), o -1
    bool retreat; // herido: se aparta, detras del jugador
} SquadChoice;

// A quien va un integrante de la escolta. (nx, nz): el; (px, pz): el jugador; hp01: su vida
// [0, 1]; foes: los enemigos (alive: en pie); player_target: el elegido del jugador (-1: ninguno).
SquadChoice squad_escort_choose(SquadOrder order, float nx, float nz, float px, float pz, float hp01, const TargetCand *foes,
                                int n, int player_target);
// El punto numero step de la ronda de un guardia que empezo en el angulo start (radianes):
// alterna entre los dos bordes del anillo y avanza 30 grados por punto.
void squad_patrol_point(float cx, float cz, float start, int step, float *x, float *z);
// A quien sale a interceptar un guardia en (gx, gz) del campamento en (cx, cz): el mas cercano
// de los que cruzaron el anillo (a menos de SQUAD_RING_MAX + 10 m del campamento) y estan a
// menos de SQUAD_GUARD_REACT m del guardia; o, si ya entraron al campamento (a menos de
// SQUAD_RING_MIN m del centro), vuelve a defenderlo desde donde este. -1 si ninguno.
int squad_guard_intercept(float gx, float gz, float cx, float cz, const TargetCand *foes, int n);
// Hay que esconderse: algun enemigo a menos de SQUAD_ALARM m del campamento o del poblador.
bool squad_villager_alarm(float vx, float vz, float cx, float cz, const TargetCand *foes, int n);

#endif
