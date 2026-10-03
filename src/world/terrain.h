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

// Los lagos ocupan las hondonadas mas bajas del mundo (TERRAIN_LAKE_SHARE del
// terreno queda bajo su nivel base), siempre al menos TERRAIN_LAKE_DEPTH por
// debajo del campamento. El nivel sube o baja con la estacion (src/sim/climate.h).
#define TERRAIN_LAKE_SHARE 0.07f
#define TERRAIN_LAKE_DEPTH 4.0f
// La crecida maxima sobre el nivel base (deshielo + lluvias), para la orilla.
#define TERRAIN_LAKE_FLOOD 3.0f

// Aspecto estacional del suelo (sale de Climate; alturas absolutas).
typedef struct {
    float greenness, autumn, snow_cover, wetness;
    float snowline;    // altura absoluta de la nieve permanente (glaciares)
    float water_level; // altura absoluta del agua de los lagos
    float ice;         // lagos congelados
} TerrainLook;

typedef struct {
    bool loaded;
    int cx, cz;
    Model model;
    float *tri; // por triangulo: altura media, pendiente, luz, mancha, desierto (para recolorear)
} Chunk;

typedef struct {
    unsigned seed;
    Chunk slots[CHUNK_SLOTS];
    int center_cx, center_cz;
    bool has_center;
    TerrainLook look;
    float plain;     // altura del llano del campamento
    float lake_base; // nivel base absoluto de los lagos
} Terrain;

float terrain_height(const Terrain *t, float x, float z);
// Altura del llano del campamento (referencia del clima y de los lagos).
float terrain_plain_height(const Terrain *t);
// Cambia el aspecto estacional; recolorea los chunks si cambio lo suficiente.
void terrain_set_look(Terrain *t, const TerrainLook *look);
// Agua (o hielo) de los lagos en el area cargada. Despues de lo opaco.
void terrain_draw_water(const Terrain *t, float time);

void terrain_init(Terrain *t, unsigned seed);
// Carga los chunks cercanos a `pos` y descarga los que quedaron lejos.
void terrain_update(Terrain *t, Vector3 pos);
void terrain_draw(const Terrain *t);
void terrain_unload(Terrain *t);

#endif
