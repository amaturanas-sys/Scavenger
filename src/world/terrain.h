// Estepa procedural por chunks con streaming alrededor del jugador.
// La altura es una funcion pura del mundo (x, z): cualquier sistema
// (fisica, IA, colocacion de props) la consulta sin tocar las mallas.
#ifndef ESTEPA_TERRAIN_H
#define ESTEPA_TERRAIN_H

#include <stdbool.h>

#include "raylib.h"

#define CHUNK_SIZE 48.0f   // metros por lado
#define CHUNK_CELLS 24     // celdas por lado (2 m/celda: facetado low-poly)
#define CHUNK_RADIUS 2     // chunks cargados alrededor: (2r+1)^2 = 25
#define CHUNK_SLOTS ((2 * CHUNK_RADIUS + 1) * (2 * CHUNK_RADIUS + 1))

typedef struct {
    bool loaded;
    int cx, cz;
    Model model;
} Chunk;

typedef struct {
    unsigned seed;
    Chunk slots[CHUNK_SLOTS];
    int center_cx, center_cz;
    bool has_center;
} Terrain;

float terrain_height(const Terrain *t, float x, float z);

void terrain_init(Terrain *t, unsigned seed);
// Carga los chunks cercanos a `pos` y descarga los que quedaron lejos.
void terrain_update(Terrain *t, Vector3 pos);
void terrain_draw(const Terrain *t);
void terrain_unload(Terrain *t);

#endif
