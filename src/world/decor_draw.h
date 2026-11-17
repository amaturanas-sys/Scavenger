// El dibujo del decorado de OpenGameArt que siembra cada chunk (src/world/terrain.c, con las normas de
// src/sim/decor.h): cada modelo con props (su cache, el tinte de la hora), a la distancia que se ve.
#ifndef ESTEPA_DECOR_DRAW_H
#define ESTEPA_DECOR_DRAW_H

#include "raylib.h"
#include "sim/inventory.h"
#include "world/props.h"
#include "world/terrain.h"

void decor_draw(const Terrain *t, Props *p, const Inventory *inv, Camera3D cam);

#endif
