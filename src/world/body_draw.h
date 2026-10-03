// Dibuja el cuerpo humano simple y articulado de src/sim/body.h (mientras no
// hay modelos importados), con las piezas de armadura encima de cada zona.
#ifndef ESTEPA_BODY_DRAW_H
#define ESTEPA_BODY_DRAW_H

#include "raylib.h"
#include "sim/armor.h"
#include "sim/body.h"

typedef struct {
    Color skin, cloth, legs;
} BodyColors;

void body_draw(const BodyPose *b, Vector3 pos, float yaw, BodyColors c, const Armor *armor);
// Color de un material de armadura (gastado se oscurece; roto, gris).
Color armor_color(const ArmorPiece *pc);

#endif
