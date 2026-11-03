// El selector de objetivo (C puro): con que enemigo pelea el jugador. O pasa al siguiente,
// en orden alrededor del jugador empezando por el que tiene delante; se pierde a TARGET_RANGE
// metros o si muere. Sin objetivo, los golpes van al mas cercano delante (como siempre).
#ifndef ESTEPA_TARGETING_H
#define ESTEPA_TARGETING_H

#include <stdbool.h>

#define TARGET_RANGE 25.0f // m: mas lejos, el objetivo se pierde

typedef struct {
    float x, z;
    bool alive;
} TargetCand;

// El siguiente (dir > 0) o el anterior (dir < 0) despues de current (-1: el que mejor se ve
// delante). yaw: hacia donde mira el jugador. -1 si no hay ninguno a tiro.
int target_cycle(const TargetCand *c, int n, float px, float pz, float yaw, int current, int dir);
// El elegido sigue valiendo: vivo y a menos de TARGET_RANGE.
bool target_keep(const TargetCand *c, int n, int current, float px, float pz);

// Hacia que lado pasa target_cycle: el que esta a la izquierda del jugador (el angulo crece hacia su
// izquierda, mirando a (sin yaw, cos yaw)) o a la derecha. El clic izquierdo y Ctrl+Q van a la
// izquierda; el derecho y Ctrl+E, a la derecha (las hojas, A3).
#define TARGET_LEFT 1
#define TARGET_RIGHT -1

// Sin verlo (el relieve, una roca), el objetivo dura TARGET_UNSEEN_S s; cubierto (C, A4) no se pierde.
// Muerto o mas lejos de TARGET_RANGE, se pierde en el acto. Lleva la cuenta en *unseen; false: soltarlo.
#define TARGET_UNSEEN_S 6.0f
bool target_hold(float *unseen, bool alive, float dist, bool visible, bool covered, float dt);

// El clic con arma a distancia (las hojas): un clic corto elige (o cambia de objetivo); mantenerlo
// TARGET_RELEASE_HOLD s lo suelta. held: s apretado; moved: se arrastro (la camara); up: se solto en
// este cuadro; done: esta pulsacion ya solto el objetivo.
#define TARGET_RELEASE_HOLD 1.0f
typedef enum { TCLICK_NONE, TCLICK_SELECT, TCLICK_RELEASE } TargetClick;
TargetClick target_click(float held, bool moved, bool up, bool done);

#endif
