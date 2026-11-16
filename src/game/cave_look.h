// Como se ve cada tipo de cueva (docs/PLAN_CUEVAS_GRANDES.md, «El plan de cada tipo»): el color de la pared, de la
// boveda y del suelo (los de las referencias, mezclados con la roca de la region), sus texturas, la luz de fondo y la
// bruma. Lo que lleva cada sala sale de src/sim/caves.c (cave_decor); como se arma la roca, de src/game/cave_scene.c.
//
// Las texturas se pintan al cargar la cueva (cl_load: 128 x 128, en grises que el color de cada vertice tiñe con la
// luz, sin archivos) y se liberan al salir (cl_unload). Se repiten sin costura: cada una cubre tile[s] metros.
#ifndef ESTEPA_CAVE_LOOK_H
#define ESTEPA_CAVE_LOOK_H

#include <stdbool.h>

#include "raylib.h"
#include "sim/caves.h"

typedef enum { CL_WALL, CL_VAULT, CL_FLOOR, CL_SURFACES } ClSurface;

typedef struct {
    Color wall, vault, floor; // sin luz
    Color moss;               // el musgo (las abandonadas) o lo mojado (las de fieras), abajo de las paredes
    float ambient[3];         // la luz de fondo
    Color fog;                // la bruma: a lo lejos, todo va hacia este color (y el fondo es de este color)
    float fog_near, fog_far, fog_max;
    float tile[CL_SURFACES]; // m que cubre cada textura
    float jag;               // 0..1: lo desparejo de la roca (0: labrada)
    float moss_h;            // m: hasta donde sube el musgo (o lo mojado) por la pared
    float soot;              // 0..1: el hollin de las antorchas en la boveda
    float facet;             // 0..1: lo marcadas que se ven las facetas de la roca (las referencias, low poly)
    float warm;              // 0..1: lo naranja de las llamas (en las frias, mas blancas: la cueva sigue de su color)
} ClLook;

// Pinta las texturas y elige los colores de esta cueva (su tipo y su region). false si no hubo memoria: se ve sin textura.
bool cl_load(const Cave *c);
const ClLook *cl_look(void);
// La textura de una superficie (sin cargar: la blanca de raylib).
Texture2D cl_texture(ClSurface s);
void cl_unload(void);
void cl_forget_gpu(void); // ver terrain_forget_gpu

#endif
