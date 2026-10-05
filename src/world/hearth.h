// Fuegos: la fogata y la hoguera (la misma, mas grande) al modo de las fotos de referencia:
// corro de piedras, leños en tipi con las puntas carbonizadas, lecho de brasas, lenguas de
// llama en capas (rojo afuera, amarillo y blanco en el corazon), chispas que suben y humo:
// bocanadas oscuras que suben en hilera, se inclinan con el viento, crecen, se aclaran y se
// desvanecen en la altura.
//
// Por cuadro: hearth_frame_begin, luego hearth_draw_base (opaco, en la pasada normal; si arde,
// queda en la cola), hearth_draw_smoke (pasada normal, tras lo opaco: el humo se oscurece de
// noche como todo) y hearth_draw_flames (tras el tinte nocturno: las llamas no se apagan).
#ifndef ESTEPA_HEARTH_H
#define ESTEPA_HEARTH_H

#include <stdbool.h>
#include <stdint.h>

#include "raylib.h"

#define HEARTH_SCALE_CAMPFIRE 1.0f // fogata: corro de ~1.2 m
#define HEARTH_SCALE_BONFIRE 2.0f  // hoguera: el doble

void hearth_init(void); // la textura de las bocanadas (necesita la ventana)
void hearth_unload(void);
void hearth_forget_gpu(void); // ver terrain_forget_gpu

void hearth_frame_begin(void);
// Piedras, leños y ceniza en pos (el suelo). seed: variacion de cada fuego. lit: arde.
void hearth_draw_base(Vector3 pos, float scale, uint32_t seed, bool lit);
// Humo de los fuegos encendidos de este cuadro. wind: fuerza del viento (0..1); light: luz del
// dia (0 noche, 1 mediodia), que oscurece el humo contra el cielo nocturno.
void hearth_draw_smoke(Camera3D cam, float time, float wind, float light);
// Llamas, brasas y chispas de los fuegos encendidos de este cuadro.
void hearth_draw_flames(float time, float wind);
int hearth_lit_count(void);

#endif
