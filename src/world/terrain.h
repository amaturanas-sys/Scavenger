// El terreno por chunks con streaming alrededor del jugador. La forma sale del mundo
// (src/sim/world.h: cinco regiones, rios, lagos, mar, muro y canal): la altura y el agua
// son funciones puras de (x, z) que cualquier sistema consulta sin tocar las mallas.
// Cada chunk lleva su suelo (colores de la region y la estacion), sus arboles y su agua.
#ifndef ESTEPA_TERRAIN_H
#define ESTEPA_TERRAIN_H

#include <stdbool.h>

#include "raylib.h"
#include "sim/hazards.h"
#include "sim/world.h"

#define CHUNK_SIZE 48.0f   // metros por lado
#define CHUNK_CELLS 24     // celdas por lado (2 m/celda: facetado low-poly)
#define CHUNK_RADIUS 2     // chunks cargados alrededor: (2r+1)^2 = 25
#define CHUNK_SLOTS ((2 * CHUNK_RADIUS + 1) * (2 * CHUNK_RADIUS + 1))

// La crecida maxima de lagos y rios sobre su nivel base (deshielo + lluvias), para la orilla.
#define TERRAIN_LAKE_FLOOD 3.0f
#define TERRAIN_NO_WATER (-1e9f)

// Aspecto estacional del suelo y del agua (sale de Climate; alturas absolutas).
typedef struct {
    float greenness, autumn, snow_cover, wetness;
    float snowline;    // altura absoluta de la nieve permanente (glaciares)
    float glacier;     // cuanto avanzo el frente del glaciar [0, 1] (Climate.glacier)
    // El agua (docs/AGUA.md, fase 2: src/sim/world.h, WaterSeason): cada lago y cada rio, con su hielo.
    float flood;       // crecida de lagos y rios sobre su nivel base (m; el mar no crece)
    float temp, temp_late; // temperatura media de hoy y de dos dias antes (Climate.temp_mean, temp_late)
    float melt;        // el deshielo del glaciar que corre ahora [0, 1]
    float sea_ice;     // el hielo del mar en la costa [0, 1]
    float floes;       // los tempanos en el mar [0, 1]
} TerrainLook;

// Formaciones de cada region, de voxeles (src/world/voxstruct.h), sembradas por chunk.
#define CHUNK_FEATS 28
typedef struct {
    float x, y, z, yaw, scale;
    unsigned char kind, variant;
} ChunkFeature;

#define TERRAIN_WATER_MESHES 3 // el agua, el hielo y la espuma de cada chunk
typedef struct {
    bool loaded;
    int cx, cz;
    ChunkFeature feat[CHUNK_FEATS];
    int feat_count;
    Model model;
    float *tri;             // por triangulo: lo que hace falta para recolorear con la estacion
    Model water[TERRAIN_WATER_MESHES];
    bool has_water[TERRAIN_WATER_MESHES];
    // Las aguas que llegan a cada vertice (no cambian con la estacion; NULL si el chunk no tiene agua) y el
    // suelo: con eso, las mallas del agua se rehacen al cambiar la estacion sin volver a preguntarle al mundo.
    WaterBodies *wbody;
    float *wground;
    unsigned char *wcell; // por celda: deshielo turquesa del altiplano, rapidos con espuma
    bool water_dirty;     // la estacion cambio: rehacer sus mallas del agua
} Chunk;

typedef struct {
    unsigned seed;
    Chunk slots[CHUNK_SLOTS];
    int center_cx, center_cz;
    bool has_center;
    TerrainLook look;
    WaterSeason meshed; // la estacion de las mallas del agua (cambia de a saltos: la consulta, en cada cuadro)
    float plain;        // altura del llano del campamento
    const World *world; // la forma del mundo (de la semilla)
} Terrain;

float terrain_height(const Terrain *t, float x, float z);
// Altura del llano del campamento (referencia del clima y de los lagos).
float terrain_plain_height(const Terrain *t);
// Cambia el aspecto estacional; recolorea los chunks si cambio lo suficiente.
void terrain_set_look(Terrain *t, const TerrainLook *look);
// El agua de la estacion (src/sim/world.h): la de los lagos, los rios, el canal y el mar, segun el aspecto.
WaterSeason terrain_season(const Terrain *t);
// Superficie del agua (o del hielo) en (x, z) en esta estacion (un cauce seco no tiene), o TERRAIN_NO_WATER.
float terrain_water(const Terrain *t, float x, float z);
// El agua con los cauces llenos (aunque hoy esten secos o helados): para no construir ni sembrar en un cauce.
float terrain_water_full(const Terrain *t, float x, float z);
// Hielo del agua en (x, z) [0, 1]: el de su lago, su rio, el canal o el mar (en la costa).
float terrain_ice(const Terrain *t, float x, float z);
// ¿Agua (sin hielo que aguante) a mas de depth de profundidad en (x, z)?
bool terrain_deep_water(const Terrain *t, float x, float z, float depth);
Region terrain_region(const Terrain *t, float x, float z);
// Un paso de (x0, z0) a (*x, *z) que no entre en agua honda (mas de 0,6 m, o hielo que no
// aguanta): si entra, prueba a rodearla girando a uno y otro lado; si no puede, se queda.
// Quien ya esta en el agua puede salir. Para lo que no nada: los carros de guerra.
void terrain_dry_step(const Terrain *t, float x0, float z0, float *x, float *z);
// El paso de la gente (la tribu, los enemigos) y sus monturas (docs/AGUA.md, fase 3): vadea hasta WADE_PERSON
// (src/sim/hazards.h); mas hondo, cruza nadando un agua angosta (un rio: hay donde hacer pie a menos de
// TERRAIN_NARROW_WATER m en esa direccion) y rodea una ancha (un lago, el mar) como terrain_dry_step. El agua frena
// el paso: vadeando y nadando avanza menos. El hielo que aguanta se camina.
#define TERRAIN_NARROW_WATER 40.0f
void terrain_wade_step(const Terrain *t, float x0, float z0, float *x, float *z);
// Donde van los pies de alguien en (x, z): el suelo; sobre el hielo que aguanta, encima; nadando, con la cabeza
// afuera (1,25 m bajo la superficie).
float terrain_body_y(const Terrain *t, float x, float z);
// Como esta alguien en (x, z): en seco, vadeando, nadando o sobre el hielo (src/sim/hazards.h).
WaterStep terrain_water_step(const Terrain *t, float x, float z);
// Agua (o hielo) en el area cargada. Despues de lo opaco.
void terrain_draw_water(const Terrain *t, float time);
// Las cascadas del area cargada (docs/AGUA.md, pagina 4): la cortina de agua y la espuma al pie (helada si su
// rio esta helado; ninguna si esta seco). Despues del agua.
void terrain_draw_falls(const Terrain *t, float time);

void terrain_init(Terrain *t, unsigned seed);
// Carga los chunks cercanos a `pos` y descarga los que quedaron lejos.
void terrain_update(Terrain *t, Vector3 pos);
// Dibuja los chunks y, debajo, el horizonte lejano (malla gruesa hasta ~2,4 km).
void terrain_draw(const Terrain *t);
// Color de la bruma del horizonte (el del cielo); recolorea si cambio.
void terrain_set_haze(Terrain *t, Color haze);
void terrain_unload(Terrain *t);
// Las texturas compartidas (atlas del suelo y agua): al cerrar, se liberan y se vuelven a pedir
// en el proximo terrain_init. terrain_forget_gpu solo las olvida, sin llamar a OpenGL: en
// Android, android_main puede volver a correr en el mismo proceso con un contexto nuevo, y las
// variables estaticas guardarian ids que ya no existen (el suelo se veia negro).
void terrain_unload_gpu(void);
void terrain_forget_gpu(void);

#endif
