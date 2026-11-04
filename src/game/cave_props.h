// Como se ve cada objeto de la cueva (src/sim/caves.h: estalactitas, cofres, celdas, pinturas, antorchas...), con la
// luz de la escena (src/game/cave_scene.h). Las tareas de docs/PLAN_CUEVAS_GRANDES.md los pasan a voxeles
// (src/world/voxcave.h) o los refinan aqui, uno por uno. Lo que se toma, se choca o se guarda sigue en src/sim/caves.c y
// src/game/cave_game.c: aqui solo se ve.
#ifndef ESTEPA_CAVE_PROPS_H
#define ESTEPA_CAVE_PROPS_H

#include <stdbool.h>
#include <stdint.h>

#include "raylib.h"
#include "sim/caves.h"

// Los modelos de voxeles de los objetos de esta cueva (al cargarla, despues de cs_load).
void cp_preload(const Cave *c);
// Todos los objetos a menos de CS_VIEW del ojo (done: lo hecho en la cueva, GameActions.cave_done: tomado, encendido o
// liberado). Entre BeginMode3D y EndMode3D, despues de cs_gather_lights.
void cp_draw(const Cave *c, uint64_t done, Vector3 eye, float time);

#endif
