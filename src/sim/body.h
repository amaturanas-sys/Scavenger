// Cuerpo humano simple y articulado (C puro): cada zona de src/sim/health.h es
// una capsula (cabeza y cuello, torax, abdomen, pelvis, brazos y antebrazos,
// muslos y piernas) que se mueve con una pose (andar, golpear, apuntar,
// abatido). Sirve para dibujar a los humanos mientras no hay modelos y para
// saber en que zona impacta un proyectil.
//
// Espacio local del cuerpo: +Y arriba, +Z adelante (hacia donde mira), +X a su
// izquierda; los pies en Y = 0. Para un adulto de 1.7 m.
#ifndef ESTEPA_BODY_H
#define ESTEPA_BODY_H

#include <stdbool.h>

#include "health.h"

typedef struct {
    float x, y, z;
} V3;

typedef struct {
    V3 a, b;      // extremos de la capsula
    float radius;
} BodySeg;

typedef struct {
    BodySeg seg[PART_COUNT];
} BodyPose;

typedef struct {
    float walk_phase; // radianes: avanza al caminar
    float walk;       // [0, 1] cuanto camina (0 quieto)
    float attack;     // [0, 1] brazo derecho en el golpe
    bool aiming;      // brazos al frente (arco)
    bool down;        // tendido en el suelo
    float scale;      // talla (1 = 1.7 m)
} BodyPoseParams;

void body_pose(BodyPose *out, const BodyPoseParams *p);
// Rayo en espacio local (dir no necesita estar normalizada): zona impactada o -1.
// t_out: fraccion de dir hasta el impacto (si se pasa).
int body_raycast(const BodyPose *b, V3 origin, V3 dir, float max_t, float *t_out);
// Pasa un punto del mundo al espacio local de un cuerpo en pos con orientacion yaw
// (yaw: el frente es (sin yaw, cos yaw), como el jugador).
V3 body_to_local(V3 world, V3 pos, float yaw);
V3 body_dir_to_local(V3 dir, float yaw);
V3 body_to_world(V3 local, V3 pos, float yaw);

#endif
