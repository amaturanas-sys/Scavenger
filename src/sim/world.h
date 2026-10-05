// El mundo (C puro, sin raylib): un gran circulo con cinco regiones climaticas fijas y
// detalles que cambian en cada partida (la semilla).
//
// Reglas fijas (iguales en todas las partidas):
//  - la estepa ocupa el centro (alli acampa la tribu) y un sector hasta el borde; alrededor,
//    en este orden: bosque de coniferas, altiplano glaciar, costa de fiordos y desierto;
//  - cada region tiene su altitud media (de la costa, la mas baja, al altiplano, la mas alta);
//  - el borde del circulo: la costa termina en el mar (con fiordos que entran tierra
//    adentro), el desierto en un enorme muro escalonado tras un cañon, y la estepa, el
//    bosque y el altiplano en un gran canal del que salen rios tributarios hacia adentro;
//  - los asentamientos, las guaridas y los reinos vecinos son de su region.
//
// Lo que cambia con la semilla: el trazado de los rios, la forma y el sitio de los lagos y
// las montañas, las fronteras entre regiones (dentro de su sector), y donde caen los
// asentamientos, las guaridas y las capitales.
//
// Todo es una funcion pura del mundo y de (x, z): el render, la fisica y la IA consultan
// la altura y el agua sin estado propio. world_for_seed() guarda el mundo generado.
#ifndef ESTEPA_WORLD_H
#define ESTEPA_WORLD_H

#include <stdbool.h>
#include <stdint.h>

#define WORLD_RADIUS 2400.0f // m: radio del gran circulo
#define WORLD_LIMIT 2330.0f  // m: hasta donde se puede llegar (la frontera invisible)
#define SEA_LEVEL 0.0f

typedef enum { REGION_STEPPE, REGION_FOREST, REGION_HIGHLAND, REGION_FJORD, REGION_DESERT, REGION_COUNT } Region;

typedef enum { WATER_NONE, WATER_LAKE, WATER_RIVER, WATER_CANAL, WATER_SEA } WaterKind;

#define LAKES_MAX 20
#define RIVERS_MAX 12
#define RIVER_PTS 28
#define PEAKS_MAX 24
#define SETTLEMENTS_MAX 16
#define DENS_MAX 40
#define CANAL_SAMPLES 360

typedef struct {
    float x, z, r;  // centro y radio (la orilla se deforma con ruido)
    float level;    // nivel base del agua (absoluto)
    bool oasis;     // en el desierto
} Lake;

typedef struct {
    float x[RIVER_PTS], z[RIVER_PTS], level[RIVER_PTS]; // del canal hacia adentro
    int n;
    float width;
    float minx, minz, maxx, maxz; // caja (para descartar rapido)
} River;

typedef struct {
    float x, z, r, h; // macizo: centro, radio y altura sobre el terreno
} Peak;

typedef enum { SETTLE_VILLAGE, SETTLE_CAPITAL } SettleKind;

typedef struct {
    float x, z;
    Region region;
    SettleKind kind;
    int kingdom; // reino al que pertenece (indice en world_kingdom), o -1
    char name[32];
} Settlement;

typedef struct {
    float x, z;
    Region region;
    int species; // Species de src/sim/animals.h
} Den;

typedef struct {
    uint32_t seed;
    float warp_phase;    // giro de las fronteras entre regiones
    Lake lakes[LAKES_MAX];
    int lake_count;
    River rivers[RIVERS_MAX];
    int river_count;
    Peak peaks[PEAKS_MAX];
    int peak_count;
    Settlement settlements[SETTLEMENTS_MAX];
    int settlement_count;
    Den dens[DENS_MAX];
    int den_count;
    float canal_level[CANAL_SAMPLES]; // nivel del agua del gran canal por grado
    float camp_height;                // altura del llano del campamento (origen)
} World;

void world_generate(World *w, uint32_t seed);
// El mundo de una semilla (generado una vez y guardado).
const World *world_for_seed(uint32_t seed);

// Regiones: peso de cada una en (x, z) (suman 1) y la dominante.
Region world_region_weights(const World *w, float x, float z, float out[REGION_COUNT]);
Region world_region(const World *w, float x, float z);
float world_region_weight(const World *w, Region r, float x, float z);
const char *region_name(Region r);      // UTF-8 ("estepa", ...)
float region_altitude(Region r);        // altitud media (m sobre el mar)
// Habitat de la fauna de cada region (HAB_* de src/sim/animals.h).
int region_habitat(Region r);

// Altura del suelo (con lagos, rios, canal, mar y muro ya excavados).
float world_height(const World *w, float x, float z);
// Superficie del agua en (x, z), o -1e9 si no hay. flood: crecida estacional de lagos y
// rios (el mar no crece). kind (si no es NULL): que agua es.
float world_water(const World *w, float x, float z, float flood, WaterKind *kind);
// Distancia al borde del circulo (positiva dentro).
float world_edge_distance(float x, float z);
// Lleva (x, z) dentro de la frontera invisible. Devuelve true si lo movio.
bool world_clamp(float *x, float *z);

// Un punto del eje del sector de una region, a distancia r del centro.
void world_region_point(const World *w, Region rg, float r, float *x, float *z);

// Reinos vecinos: uno por region (la estepa es del kanato al que la tribu rinde tributo).
const char *world_kingdom_name(Region r); // UTF-8
// Asentamiento mas cercano (indice) o -1; dist (si no es NULL) en m.
int world_nearest_settlement(const World *w, float x, float z, float *dist);
int world_nearest_den(const World *w, float x, float z, float *dist);

#endif
