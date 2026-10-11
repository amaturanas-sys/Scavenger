// El mundo (C puro, sin raylib): una gran tierra casi circular (de borde fractal, no una
// circunferencia neta) con cinco regiones climaticas fijas y detalles que cambian en cada
// partida (la semilla).
//
// Reglas fijas (iguales en todas las partidas); el norte es -Z y el este +X:
//  - la estepa ocupa el centro (alli acampa la tribu), rodeada de rios de trayecto
//    tortuoso (meandros) que la separan de las demas regiones;
//  - al suroeste, la costa de fiordos, que termina en el mar;
//  - al noroeste, un bosque de coniferas muy tupido, que termina en un canal sigmoideo del
//    que salen rios tributarios;
//  - al noreste, el desierto, que termina en un gran cañon y un muro de estratos;
//  - al sureste, el altiplano glaciar, que termina en un muro de hielo; sus rios bajan
//    trenzados por lechos de grava;
//  - cada region tiene su altitud media (de la costa, la mas baja, al altiplano, la mas alta);
//  - los asentamientos, las guaridas y los reinos vecinos son de su region.
//
// Lo que cambia con la semilla: el contorno fractal del borde, el trazado de los rios y sus
// meandros, la forma y el sitio de los lagos y las montañas, las fronteras entre regiones
// (dentro de su cuadrante) y donde caen los asentamientos, las guaridas y las capitales.
//
// Todo es una funcion pura del mundo y de (x, z): el render, la fisica y la IA consultan
// la altura y el agua sin estado propio. world_for_seed() guarda el mundo generado.
#ifndef ESTEPA_WORLD_H
#define ESTEPA_WORLD_H

#include <stdbool.h>
#include <stdint.h>

#define WORLD_RADIUS 4800.0f // m: radio medio de la tierra (el borde varia, fractal)
#define SEA_LEVEL 0.0f

typedef enum { REGION_STEPPE, REGION_FOREST, REGION_HIGHLAND, REGION_FJORD, REGION_DESERT, REGION_COUNT } Region;

typedef enum { WATER_NONE, WATER_LAKE, WATER_RIVER, WATER_CANAL, WATER_SEA } WaterKind;

#define LAKES_MAX 32
#define RIVERS_MAX 48
#define RIVER_PTS 160
#define PEAKS_MAX 48
#define SETTLEMENTS_MAX 24
#define DENS_MAX 56
#define CANAL_SAMPLES 360

typedef struct {
    float x, z, r;  // centro y radio (la orilla se deforma con ruido)
    float level;    // nivel base del agua (absoluto)
    bool oasis;     // en el desierto
    bool glacial;   // del altiplano: agua turquesa de deshielo
} Lake;

// Arroyo: un cauce chico (2 a 3,5 m) que baja de lomas, cumbres y bosques hasta un rio.
typedef enum { RIVER_MEANDER, RIVER_TRIBUTARY, RIVER_BRAIDED, RIVER_CREEK, RIVER_KINDS } RiverKind;

typedef struct {
    float x[RIVER_PTS], z[RIVER_PTS], level[RIVER_PTS];
    float along[RIVER_PTS]; // metros recorridos hasta cada punto (para los canales trenzados)
    int n;
    float width;            // trenzado: el ancho del lecho de grava
    RiverKind kind;
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

// Tribus nomadas (no son reinos): campamentos de yurtas con su actitud hacia la tuya.
typedef enum { TRIBE_RIVAL, TRIBE_NEUTRAL, TRIBE_FRIENDLY, TRIBE_ATTITUDES } TribeAttitude;
#define TRIBES_MAX 20
typedef struct {
    float x, z;
    Region region;
    TribeAttitude attitude;
    char name[32];
} TribeCamp;

// Estructuras sueltas: ruinas de otros tiempos y construcciones en uso, al estilo de su region.
typedef enum {
    SITE_KURGAN,       // tumulo funerario
    SITE_BALBALS,      // hilera de estelas de piedra
    SITE_DEER_STONE,   // piedra de ciervos (monolito grabado)
    SITE_RUINED_FORT,  // fortaleza derruida
    SITE_BURIED_CITY,  // ciudad enterrada en la arena
    SITE_PETROGLYPHS,  // rocas grabadas
    SITE_CARAVANSERAI, // posada amurallada de las caravanas (en uso)
    SITE_WATCHTOWER,   // torre de vigia (en uso)
    SITE_OVOO,         // altar de piedras con cintas (en uso)
    SITE_WELL,         // pozo (en uso)
    SITE_HARBOR,       // embarcadero de la costa (en uso)
    SITE_KINDS
} SiteKind;
#define SITES_MAX 64
typedef struct {
    float x, z, yaw;
    Region region;
    SiteKind kind;
} WorldSite;

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
    TribeCamp tribes[TRIBES_MAX];
    int tribe_count;
    WorldSite sites[SITES_MAX];
    int site_count;
    float canal_level[CANAL_SAMPLES]; // nivel del agua del canal del bosque por grado
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
// Hacia donde queda (dx, dz) desde donde uno esta: "norte", "noreste"... (-Z es el norte). UTF-8.
const char *world_compass(float dx, float dz);
float region_altitude(Region r);        // altitud media (m sobre el mar)
// Habitat de la fauna de cada region (HAB_* de src/sim/animals.h).
int region_habitat(Region r);

// Altura del suelo (con lagos, rios, canal, mar y muros ya excavados).
float world_height(const World *w, float x, float z);
// Superficie del agua en (x, z), o -1e9 si no hay. flood: crecida estacional de lagos y
// rios (el mar no crece). kind (si no es NULL): que agua es.
float world_water(const World *w, float x, float z, float flood, WaterKind *kind);
// Orilla de rio: cuanto es (x, z) lecho de cantos rodados que el agua lava (1 en el cauce y la
// orilla baja, 0 lejos), para rios de meandro, tributarios y arroyos; width (si no es NULL): el
// ancho de ese rio. Los trenzados ya son de grava.
float world_river_bank(const World *w, float x, float z, float *width);
// El borde fractal: su radio en un angulo, y la distancia de (x, z) a el (positiva dentro).
float world_edge_radius(const World *w, float angle);
float world_edge_distance(const World *w, float x, float z);
// Distancia del eje del canal del bosque al borde, en un angulo (el canal serpentea).
float world_canal_offset(const World *w, float angle);
// Lleva (x, z) dentro de la frontera invisible (antes del mar abierto, del muro, del canal
// o del hielo). Devuelve true si lo movio.
bool world_clamp(const World *w, float *x, float *z);

// Un punto del eje del cuadrante de una region (la estepa: el centro), a distancia r del
// centro; world_edge_point: a inset metros del borde, en el centro del cuadrante.
void world_region_point(const World *w, Region rg, float r, float *x, float *z);
void world_edge_point(const World *w, Region rg, float inset, float *x, float *z);

// Reinos vecinos: uno por region (la estepa es del kanato al que la tribu rinde tributo).
const char *world_kingdom_name(Region r); // UTF-8
// Nombre de la clase de estructura (UTF-8, para el texto) y si es una ruina.
const char *site_name(SiteKind k);
bool site_is_ruin(SiteKind k);
// Asentamiento mas cercano (indice) o -1; dist (si no es NULL) en m.
int world_nearest_settlement(const World *w, float x, float z, float *dist);
int world_nearest_den(const World *w, float x, float z, float *dist);

#endif
