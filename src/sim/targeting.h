// El selector de objetivo (C puro): con que enemigo pelea el jugador. Tab pasa al siguiente,
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

#endif
