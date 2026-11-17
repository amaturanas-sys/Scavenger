// Piezas sueltas con sombra por cara (cajas, cilindros y elipsoides, en rlgl), para lo que no es de voxeles: el decorado
// de las cuevas (src/game/cave_scene.c, con la luz de la cueva) y lo de cada tipo en las bocas (src/game/world_game.c).
// Cada cara lleva el tono de su normal, sobre lit: el color con la luz del lugar, ya calculada en el medio de la pieza.
//
// Los tonos: SHAPE_SOFT, como las facetas de la roca de las cuevas (mas clara la que mira arriba y de frente);
// SHAPE_VOXEL, como las caras de los voxeles (FACE_SHADE de src/sim/voxel.c), para que combine con ellos.
#ifndef ESTEPA_SHAPES_H
#define ESTEPA_SHAPES_H

#include "raylib.h"

typedef enum { SHAPE_SOFT, SHAPE_VOXEL } ShapeTone;

// El tono de una cara de normal n (de largo 1).
Color shape_tone(Color c, Vector3 n, ShapeTone t);
// Una caja de w de ancho, h de alto y d de largo (hacia yaw), con su centro en c.
void shape_box(Vector3 c, float w, float h, float d, float yaw, Color lit, ShapeTone t);
// Un cilindro (o un cono truncado) de a a b, de radio ra en a y rb en b, con sides lados; con la textura tex (0: sin
// textura) que cubre tile metros, y sus tapas (caps: 1, la de b; 2, la de a).
void shape_solid(Vector3 a, Vector3 b, float ra, float rb, int sides, Color lit, ShapeTone t, unsigned int tex, float tile, int caps);
// Un elipsoide de radios rl (hacia yaw), rh (arriba) y rw (de costado), con su centro en c.
void shape_ellipsoid(Vector3 c, float rl, float rh, float rw, float yaw, Color lit, ShapeTone t);

#endif
