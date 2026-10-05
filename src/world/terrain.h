// El terreno por chunks con streaming alrededor del jugador. La forma sale del mundo
// (src/sim/world.h: cinco regiones, rios, lagos, mar, muro y canal): la altura y el agua
// son funciones puras de (x, z) que cualquier sistema consulta sin tocar las mallas.
// Cada chunk lleva su suelo (colores de la region y la estacion), sus arboles y su agua.
#ifndef ESTEPA_TERRAIN_H
#define ESTEPA_TERRAIN_H

#include <stdbool.h>

#include "raylib.h"
#include "sim/world.h"

#define CHUNK_SIZE 48.0f   // metros por lado
#define CHUNK_CELLS 24     // celdas por lado (2 m/celda: facetado low-poly)
#define CHUNK_RADIUS 2     // chunks cargados alrededor: (2r+1)^2 = 25
#define CHUNK_SLOTS ((2 * CHUNK_RADIUS + 1) * (2 * CHUNK_RADIUS + 1))

// La crecida maxima de lagos y rios sobre su nivel base (deshielo + lluvias), para la orilla.
#define TERRAIN_LAKE_FLOOD 3.0f
#define TERRAIN_NO_WATER (-1e9f)

// Aspecto estacional del suelo (sale de Climate; alturas absolutas).
typedef struct {
    float greenness, autumn, snow_cover, wetness;
    float snowline;    // altura absoluta de la nieve permanente (glaciares)
    float flood;       // crecida de lagos y rios sobre su nivel base (m; el mar no crece)
    float ice;         // lagos y rios congelados (el mar no se hiela)
} TerrainLook;

typedef struct {
    bool loaded;
    int cx, cz;
    Model model;
    float *tri;             // por triangulo: lo que hace falta para recolorear con la estacion
    Model water[3];         // lagos, rios y canal, mar (cada uno sube distinto con la crecida)
    bool has_water[3];
} Chunk;

typedef struct {
    unsigned seed;
    Chunk slots[CHUNK_SLOTS];
    int center_cx, center_cz;
    bool has_center;
    TerrainLook look;
    float plain;        // altura del llano del campamento
    const World *world; // la forma del mundo (de la semilla)
} Terrain;

float terrain_height(const Terrain *t, float x, float z);
// Altura del llano del campamento (referencia del clima y de los lagos).
float terrain_plain_height(const Terrain *t);
// Cambia el aspecto estacional; recolorea los chunks si cambio lo suficiente.
void terrain_set_look(Terrain *t, const TerrainLook *look);
// Superficie del agua en (x, z) (lagos y rios con la crecida, canal, mar), o TERRAIN_NO_WATER.
float terrain_water(const Terrain *t, float x, float z);
// Hielo del agua en (x, z) [0, 1]: lagos, rios y canal se hielan; el mar no.
float terrain_ice(const Terrain *t, float x, float z);
// ¿Agua (sin hielo que aguante) a mas de depth de profundidad en (x, z)?
bool terrain_deep_water(const Terrain *t, float x, float z, float depth);
Region terrain_region(const Terrain *t, float x, float z);
// Agua (o hielo) en el area cargada. Despues de lo opaco.
void terrain_draw_water(const Terrain *t, float time);

void terrain_init(Terrain *t, unsigned seed);
// Carga los chunks cercanos a `pos` y descarga los que quedaron lejos.
void terrain_update(Terrain *t, Vector3 pos);
void terrain_draw(const Terrain *t);
void terrain_unload(Terrain *t);

#endif
