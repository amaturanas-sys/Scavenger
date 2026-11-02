// El mundo (C puro, sin raylib): una gran tierra casi circular (de borde fractal, no una
// circunferencia neta) con cinco regiones climaticas fijas y detalles que cambian en cada
// partida (la semilla).
//
// Reglas fijas (iguales en todas las partidas); el norte es -Z y el este +X:
//  - la estepa ocupa el centro (alli acampa la tribu, en una loma suave);
//  - al suroeste, la costa de fiordos, que termina en el mar;
//  - al noroeste, un bosque de coniferas muy tupido, que termina en un canal sigmoideo;
//  - al noreste, el desierto, que termina en un gran cañon y un muro de estratos;
//  - al sureste, el altiplano glaciar, que termina en un muro de hielo, entre dos cordilleras (hacia el
//    desierto y hacia los fiordos);
//  - las altitudes medias, de mayor a menor: altiplano, desierto, estepa, bosque y fiordos (el mar);
//  - el agua baja (docs/AGUA.md, src/sim/hydro.h): los rios nacen en el deshielo del glaciar y en las
//    lluvias, se juntan como raices y terminan en el canal del bosque, en el mar o en el cañon; trenzados
//    en el altiplano, de meandros en la estepa, tributarios en el bosque. Cada lago llena una depresion
//    hasta su punto de desborde, y de ahi sale su rio;
//  - los asentamientos, las guaridas y los reinos vecinos son de su region.
//
// Lo que cambia con la semilla: el contorno fractal del borde, el relieve (y con el, por donde bajan los
// rios y donde quedan los lagos), las montañas, las fronteras entre regiones (dentro de su cuadrante) y
// donde caen los asentamientos, las guaridas y las capitales.
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
#define RIVERS_MAX 192 // tramos: un rio largo se parte en varios (de hasta RIVER_PTS puntos, cada 20 m)
#define RIVER_PTS 96
#define PEAKS_MAX 64
#define FALLS_MAX 48
#define SETTLEMENTS_MAX 24
#define DENS_MAX 56
#define CANAL_SAMPLES 360

typedef struct {
    float x, z, r;  // centro y radio (la orilla se deforma con ruido)
    float level;    // nivel base del agua (absoluto): el de su punto de desborde
    bool oasis;     // en el desierto
    bool glacial;   // del altiplano: agua turquesa de deshielo
    float spill_x, spill_z; // el punto de desborde: por ahi sale su rio
    int outflow;    // el tramo de rio que sale por el desborde (indice en rivers), o -1 (los oasis)
} Lake;

// Arroyo: un cauce chico (2 a 3,5 m) que baja de lomas, cumbres y bosques hasta un rio.
typedef enum { RIVER_MEANDER, RIVER_TRIBUTARY, RIVER_BRAIDED, RIVER_CREEK, RIVER_KINDS } RiverKind;
// De donde viene el agua de un rio (crece al final).
typedef enum {
    RIVER_FROM_RAIN,    // las lluvias de su cuenca
    RIVER_FROM_GLACIER, // el deshielo del glaciar del altiplano (en invierno, menos: docs/AGUA.md)
    RIVER_FROM_LAKE,    // el desborde de un lago
    RIVER_SOURCES
} RiverSource;
// Donde termina un tramo (crece al final).
typedef enum {
    RIVER_TO_SEA,   // el mar de los fiordos
    RIVER_TO_CANAL, // el canal del bosque
    RIVER_TO_SINK,  // el cañon del desierto, que se la traga
    RIVER_TO_LAKE,  // un lago (into: su indice)
    RIVER_TO_RIVER, // otro rio, donde se junta (into: el tramo)
    RIVER_TO_NEXT,  // sigue en el tramo siguiente del mismo rio (into)
    RIVER_TO_EDGE,  // el borde del mundo
    RIVER_ENDS
} RiverEnd;

// Un tramo de rio, de la cabecera (punto 0) a la boca: el nivel del agua nunca sube.
typedef struct {
    float x[RIVER_PTS], z[RIVER_PTS], level[RIVER_PTS];
    float along[RIVER_PTS]; // metros recorridos hasta cada punto, desde la cabecera del rio
    int n;
    float width;            // trenzado: el ancho del lecho de grava
    RiverKind kind;
    float minx, minz, maxx, maxz; // caja (para descartar rapido)
    RiverSource source;
    RiverEnd end;
    int into;               // el lago o el tramo donde termina (-1 si no)
    float flow;             // el agua en la boca (celdas de lluvia de su cuenca; el deshielo cuenta mas)
    float melt;             // la parte de ese agua que es deshielo [0, 1]
} River;

// Una cascada: donde el rio cae sin pendiente que lo sostenga (docs/AGUA.md, pagina 4).
typedef struct {
    float x, z;        // el borde de arriba
    float top, bottom; // el nivel del agua arriba y abajo
    float yaw;         // hacia donde cae (como el jugador: 0 es +z)
    float width;
    int river;         // el tramo
    float bx, bz;      // el pie
} Waterfall;

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
    Waterfall falls[FALLS_MAX];
    int fall_count;
} World;

// Lo que ocupa cada cosa del mundo (m de radio alrededor de su centro): nada de eso cae en el agua.
#define WORLD_FOOT_CAPITAL 50.0f // la muralla de la capital (CITY_WALL_R, src/sim/city.h) y sus torres
#define WORLD_FOOT_VILLAGE 24.0f // el anillo de casas de la aldea
#define WORLD_FOOT_TRIBE 20.0f   // el corro de casas o yurtas de una tribu
#define WORLD_FOOT_SITE 18.0f    // una estructura (la fortaleza en ruinas y la ciudad enterrada, las mas grandes)
#define WORLD_FOOT_DEN 8.0f      // una guarida
// La linea de nieve del verano, en m sobre el llano del campamento (src/sim/climate.c: la mas alta
// del año): mas arriba hay nieve todo el año y glaciar. Los pueblos y las tribus, siempre por debajo
// de WORLD_SNOW_FREE.
#define WORLD_SNOWLINE_MAX 40.0f
#define WORLD_SNOW_FREE (WORLD_SNOWLINE_MAX - 4.0f)

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
// El relieve sin el agua (sin lagos, rios ni canal excavados): por donde bajaria el agua.
float world_land(const World *w, float x, float z);
// Lo mas bajo de la orilla del lago i (el relieve donde termina su agua): su nivel queda por debajo, asi el
// agua sale solo por su rio de desborde (docs/AGUA.md, pagina 9).
float world_lake_shore(const World *w, int i);
// Superficie del agua en (x, z), o -1e9 si no hay. flood: crecida estacional de lagos y
// rios (el mar no crece). kind (si no es NULL): que agua es. Es el agua completa, la de los cauces llenos:
// sirve para no construir en un cauce. La de cada estacion (seca o helada) es world_water_now.
float world_water(const World *w, float x, float z, float flood, WaterKind *kind);

// ------------------------------------------------------------------ las estaciones del agua
// docs/AGUA.md, fase 2. Lo que hace falta del clima (src/sim/climate.h) para saber como esta cada agua:
//  - los rios llevan la lluvia de su cuenca y el deshielo del glaciar (River.melt). Con helada el glaciar no
//    suelta agua, y los que dependen de el se secan: queda el cauce de grava, con unos charcos;
//  - con frio el agua se hiela, primero la de lo alto y la chica (charcos, arroyos, lagos chicos); al final,
//    los rios grandes y el canal. El mar se hiela desde la costa, donde es bajo;
//  - el frente del glaciar avanza en invierno y retrocede en verano.
typedef struct {
    float flood;     // crecida de los lagos sobre su nivel base (m); los rios y el canal, la mitad; el mar no crece
    float temp;      // temperatura media del dia en el llano del campamento (grados; Climate.temp_mean)
    float temp_late; // la de dos dias antes (Climate.temp_late): la que sienten los lagos grandes y el canal
    float melt;      // el deshielo del glaciar que corre ahora [0, 1] (Climate.melt)
    float sea_ice;   // el hielo del mar en la costa [0, 1] (Climate.sea_ice)
} WaterSeason;
// La estacion del agua completa (la de world_water): todo el deshielo y nada de hielo.
WaterSeason world_full_season(float flood);

// Como esta un cauce (crece al final).
typedef enum {
    CHANNEL_WET,    // con agua
    CHANNEL_DRY,    // seco: el cauce de grava, con unos charcos
    CHANNEL_FROZEN, // helado: el hielo aguanta (ICE_WALKABLE, src/sim/hazards.h)
    CHANNEL_STATES
} ChannelState;
#define WORLD_DRY_SHARE 0.2f  // un rio se seca si corre menos de esta parte de su agua...
#define WORLD_FULL_SHARE 0.6f // ... y desde esta parte, su cauce va lleno
// La parte del agua de un tramo que corre ahora [0, 1]: la de la lluvia y los lagos siempre, la del deshielo
// segun la estacion.
float world_river_share(const World *w, int river, const WaterSeason *s);
ChannelState world_channel_state(const World *w, int river, const WaterSeason *s);
// Cuanto sube (o baja) el nivel de un tramo sobre su nivel base (River.level), en m: la mitad de la crecida,
// menos lo que baja con menos agua. El que sale de un lago nunca queda mas alto que el lago.
float world_river_offset(const World *w, int river, const WaterSeason *s);
// El nivel del agua en el punto k de un tramo en esa estacion (cerca de su boca baja con el agua que lo recibe:
// sin escalones en la junta), o -1e9 si esta seco y ahi no queda un charco; ice (si no es NULL): su hielo.
float world_river_level(const World *w, int river, int k, const WaterSeason *s, float *ice);
// El hielo [0, 1] de un lago y de un tramo (con ICE_WALKABLE, aguanta). Pesa la temperatura de su altura
// (mas fria arriba) y, en los lagos, su tamaño: los chicos siguen el aire de hoy y los grandes, el de dos dias
// antes (se hielan despues y se deshielan despues).
float world_lake_ice(const World *w, int lake, const WaterSeason *s);
float world_river_ice(const World *w, int river, const WaterSeason *s);
// El hielo del mar en (x, z), con el fondo del mar ahi (sea_floor, el relieve bajo el agua).
float world_sea_ice(const World *w, float x, float z, float sea_floor, const WaterSeason *s);
// Cuanto tiene que avanzar el glaciar (Climate.glacier) para cubrir (x, z): 0 o menos junto al muro de hielo
// (siempre), 1 en lo mas lejos que llega su lengua; mas de 1, nunca (fuera del altiplano, 2).
float world_glacier_reach(const World *w, float x, float z);

// El agua de un punto en una estacion.
typedef struct {
    float level;     // la superficie del agua o del hielo, o -1e9 si no hay (un cauce seco, sin charco)
    WaterKind kind;  // WATER_NONE si no hay
    float ice;       // [0, 1]
    int lake, river; // el lago o el tramo de rio de esa agua, o -1
} WaterNow;
float world_water_now(const World *w, float x, float z, const WaterSeason *s, WaterNow *out);
// Lo mismo en dos pasos, para quien lo pide muchas veces en el mismo lugar (las mallas del agua): las aguas
// que llegan a (x, z), que no cambian con la estacion, y como estan en una estacion.
#define WATER_RIVERS_HERE 2
typedef struct {
    float ground;    // world_height (si hay un lago o un rio cerca; si no, -1e9)
    float sea_floor; // el relieve bajo el mar, o -1e9 si no hay mar
    float canal;     // el nivel base del canal, o -1e9
    int lake;        // el lago que llega (el mas alto), o -1
    int river[WATER_RIVERS_HERE];  // los tramos que llegan (los mas altos primero), o -1
    float river_level[WATER_RIVERS_HERE], river_along[WATER_RIVERS_HERE];
} WaterBodies;
void world_water_bodies(const World *w, float x, float z, WaterBodies *b);
float world_water_resolve(const World *w, const WaterBodies *b, float x, float z, const WaterSeason *s, WaterNow *out);
// Hacia donde corre un tramo en un punto de su recorrido (along: los metros desde la cabecera, como
// WaterBodies.river_along): la direccion aguas abajo (dx, dz, unitaria) y lo que baja el agua por metro (slope,
// si no es NULL). false si el tramo no existe (docs/AGUA.md, fase 4: la corriente que se ve).
bool world_river_flow(const World *w, int river, float along, float *dx, float *dz, float *slope);
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
// Los bordes (docs/AGUA.md, fase 5). El mar abierto: en la costa de los fiordos, cuantos metros mas alla de la
// linea del tiburon (WORLD_SHARK_E m del borde) esta (x, z), sobre el mar; 0 o menos, todavia no (o no es el
// mar). Hasta ahi se nada; mas alla, el tiburon.
#define WORLD_SHARK_E 140.0f
float world_open_sea(const World *w, float x, float z);
// El muro de roca del otro lado del canal del bosque, de estratos levantados: cuanto es (x, z) muro [0, 1] (0 de
// la orilla de afuera del canal hacia adentro). Se ve y no se alcanza.
float world_forest_wall(const World *w, float x, float z);
// ¿Esta (x, z) a menos de margin m del agua del canal del bosque? (su orilla queda despejada: se ve el muro)
bool world_canal_near(const World *w, float x, float z, float margin);

// Un punto del eje del cuadrante de una region (la estepa: el centro), a distancia r del
// centro; world_edge_point: a inset metros del borde, en el centro del cuadrante.
void world_region_point(const World *w, Region rg, float r, float *x, float *z);
void world_edge_point(const World *w, Region rg, float inset, float *x, float *z);

// Reinos vecinos: uno por region (la estepa es del kanato al que la tribu rinde tributo).
const char *world_kingdom_name(Region r); // UTF-8
// Nombre de la clase de estructura (UTF-8, para el texto) y si es una ruina.
const char *site_name(SiteKind k);
bool site_is_ruin(SiteKind k);
// ¿La huella de radio r alrededor de (x, z) esta en seco? Ni un lago (con su orilla), ni un rio,
// ni el canal, ni el mar (sea_ok: el embarcadero, que da al mar), con la crecida de la primavera.
bool world_footprint_dry(const World *w, float x, float z, float r, bool sea_ok);
// Asentamiento mas cercano (indice) o -1; dist (si no es NULL) en m.
int world_nearest_settlement(const World *w, float x, float z, float *dist);
int world_nearest_den(const World *w, float x, float z, float *dist);

#endif
