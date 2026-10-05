// El mundo entero de un vistazo:
//  - vista orbital en el juego (F5): una malla gruesa de todo el gran circulo (relieve,
//    regiones, rios, lagos, canal, mar y el muro del desierto) con los asentamientos, las
//    guaridas y el jugador, vista desde lo alto mientras gira;
//  - mapa general (--mapa-mundo archivo.png): el mismo mundo visto desde arriba, con
//    sombreado del relieve, leyenda y nombres, para afinar la generacion.
#ifndef ESTEPA_WORLDVIEW_H
#define ESTEPA_WORLDVIEW_H

#include <stdbool.h>

#include "raylib.h"
#include "sim/world.h"

typedef struct {
    bool built;
    uint32_t seed;
    Model model;
} WorldView;

// La malla del mundo (se rehace si cambia la semilla). snowline: altura de la nieve permanente.
void worldview_build(WorldView *v, const World *w, float snowline);
// Dibuja la vista orbital: angle gira la camara alrededor del centro, tilt la inclina
// (0 de canto, 1 desde arriba). player: donde esta el jugador. Hace su propio BeginMode3D.
void worldview_draw(WorldView *v, const World *w, float angle, float tilt, Vector3 player, float time);
void worldview_unload(WorldView *v);
// Mapa general del mundo en un PNG de size x size. Devuelve true si se escribio.
bool worldmap_export(const World *w, const char *path, int size, float snowline);

#endif
