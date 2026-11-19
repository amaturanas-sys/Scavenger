// El terreno por chunks con streaming alrededor del jugador. La forma sale del mundo
// (src/sim/world.h: cinco regiones, rios, lagos, mar, muro y canal): la altura y el agua
// son funciones puras de (x, z) que cualquier sistema consulta sin tocar las mallas.
// Cada chunk lleva su suelo (colores de la region y la estacion), sus arboles y su agua.
#ifndef ESTEPA_TERRAIN_H
#define ESTEPA_TERRAIN_H

#include <stdbool.h>

#include "raylib.h"
#include "sim/hazards.h"
#include "sim/decor.h"
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

// Los arboles de cada chunk (los modelos de voxeles del pino, el sauce y el arce: src/world/trees.h), por casillas de 4 m.
#define CHUNK_TREES 144
typedef struct {
    float x, y, z, scale, yaw; // la base, la escala del modelo (1: el alto del inventario) y el giro
    unsigned char kind;        // TreeKind (src/sim/flora.h)
} ChunkTree;

// El decorado de cada chunk: los modelos de OpenGameArt, sembrados con sus normas (src/sim/decor.h). Cada cosa
// la guarda el chunk donde cae su centro (o la punta de tierra de un muelle).
#define CHUNK_DECOR 64
typedef struct {
    float x, y, z, yaw, scale; // la base, el giro y la escala
    float stretch;             // lo que se estira a lo largo (su X) sobre la escala: el puente, de orilla a orilla
    unsigned char kind;        // DecorKind (src/sim/decor.h)
    unsigned char biome;       // la region de mas peso: la variante de las rocas (props_draw_item_biome)
} ChunkDecor;

// Las rocas de cada chunk que se alzan (de 60 cm o mas, de prop_slot): para las colisiones (terrain_collide).
#define CHUNK_ROCKS 96
typedef struct {
    float x, y, z, r, top; // el centro (y: su base), el radio que choca y el alto de la punta
} ChunkRock;

#define TERRAIN_WATER_MESHES 3 // el agua, el hielo y la espuma de cada chunk
typedef struct {
    bool loaded;
    int cx, cz;
    ChunkFeature feat[CHUNK_FEATS];
    int feat_count;
    ChunkTree trees[CHUNK_TREES];
    int tree_count;
    ChunkDecor decor[CHUNK_DECOR];
    int decor_count;
    ChunkRock rocks[CHUNK_ROCKS];
    int rock_count;
    Model model;
    float *tri;             // por triangulo: lo que hace falta para recolorear con la estacion
    Model water[TERRAIN_WATER_MESHES];
    bool has_water[TERRAIN_WATER_MESHES];
    // Las aguas que llegan a cada vertice (no cambian con la estacion; NULL si el chunk no tiene agua) y el
    // suelo: con eso, las mallas del agua se rehacen al cambiar la estacion sin volver a preguntarle al mundo.
    WaterBodies *wbody;
    float *wground;
    unsigned char *wcell; // por celda: deshielo turquesa del altiplano, rapidos con espuma
    unsigned char *wfoam; // por vertice: la espuma de las rocas y los rapidos [0, 255] (el shader del agua)
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
    // El puente de cada pueblo sobre un arroyo cercano (src/sim/decor.h, decor_bridge), si tiene: se buscan una vez.
    DecorBridge bridge[SETTLEMENTS_MAX];
    bool has_bridge[SETTLEMENTS_MAX];
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
// Agua (o hielo) en el area cargada. Despues de lo opaco. Con el shader del agua (docs/AGUA.md, fase 4; GLSL 330,
// o ES 100 en Android): el vaiven de los lagos, la corriente de los rios por su cauce, el oleaje del mar con las
// olas que entran y se retiran en la playa, y la espuma de las orillas, las rocas, los rapidos y de quien esta en
// el agua. Si el shader no compila, el agua de antes (las ondas que respiran y la malla de espuma).
void terrain_draw_water(const Terrain *t, float time);
// Quien esta en el agua este cuadro (x, z del mundo; z: radio de su espuma en m; w: cuanta [0, 1]): hasta
// TERRAIN_RIPPLES, los mas cercanos primero (src/game/water_game.c).
#define TERRAIN_RIPPLES 8
void terrain_set_ripples(const Vector4 *bodies, int count);
// ¿Hay una formacion de roca o hielo (cantos, basalto, farallones, tempanos, mesas...) a menos de margin m de (x, z)?
// Solo en los chunks cargados (no tienen colision: para no dejar a nadie adentro de una).
// Cuantas cosas del decorado hay en los trozos cargados, y cuantas de cada clase (DecorClass, src/sim/decor.h).
int terrain_decor_count(const Terrain *t, int per_class[DECOR_CLASSES]);
// El censo del decorado natural en todo el mundo: cuantos de cada DecorKind y, por clase, cuantas intenciones caen
// por zona, ceden, no tienen suelo, pisan un arbol, o van (--censo-decorado).
void terrain_decor_census(const Terrain *t, int per_kind[DECOR_KINDS], int why[DECOR_CLASSES][5]);
// El primer puente (pier: muelle) del decorado, de pueblo en pueblo: su lugar y su direccion (--ir puente, muelle).
bool terrain_find_water_decor(const Terrain *t, bool pier, float *x, float *z, float *dx, float *dz);
bool terrain_near_feature(const Terrain *t, float x, float z, float margin);
// Las colisiones, lo mas sencillo posible: circulos y capsulas en el suelo, sin fisica. Saca a (x, z) (un cuerpo de
// radius m con los pies a y) de los troncos de los arboles, las rocas que se alzan y el decorado que choca
// (decor_collider, src/sim/decor.h) de los trozos cargados de alrededor. Lo que no pasa de las rodillas (45 cm sobre
// los pies) se pisa. Las obras de voxeles (edificios, ruinas, formaciones) frenan aparte: voxs_solid_at (src/main.c).
// reach: lo que sube de un paso (caminando) o, saltando, el borde que alcanza a agarrar: las rocas mas bajas que eso se
// trepan (terrain_ground lo sube encima); las mas altas lo frenan.
void terrain_collide(const Terrain *t, float *x, float *z, float y, float radius, float reach);
// El suelo que se pisa en (x, z): el terreno o, si alli hay un tablero (el puente, un muelle, la tarima) y quien
// camina venia a su altura (y_hint, sus pies: a lo sumo 60 cm por debajo), el tablero.
// Tambien las rocas (las de prop_slot y las del decorado que se trepan): una cupula sobre su planta, para quien llega a
// menos de reach de su superficie.
float terrain_ground(const Terrain *t, float x, float z, float y_hint, float reach);
// Lo que cubre en el mundo a menos de reach m del borde: el tronco de un arbol, una roca de un metro o mas,
// una mata alta o una formacion de voxeles (los mismos que se dibujan). true con el mas cercano: su centro
// (en el suelo) y su radio. La cobertura del jugador (C, A4) lo usa.
bool terrain_cover_near(const Terrain *t, float x, float z, float reach, Vector3 *center, float *radius);
// ¿Se dibuja el agua con su shader? (falso antes del primer chunk con agua, o si no compilo)
bool terrain_water_shader(void);
// Sin el shader del agua (--sin-shader: prueba del respaldo). Antes del primer chunk.
void terrain_water_shader_off(void);
// Las cascadas del area cargada (docs/AGUA.md, pagina 4 y fase 4): la cortina de agua, que baja, y la espuma y
// la bruma al pie (helada si su rio esta helado: sin espuma ni bruma; ninguna si esta seco). Despues del agua.
void terrain_draw_falls(const Terrain *t, float time);

void terrain_init(Terrain *t, unsigned seed);
// Carga los chunks cercanos a `pos` y descarga los que quedaron lejos.
void terrain_update(Terrain *t, Vector3 pos);
// Dibuja los chunks y, debajo, el horizonte lejano (malla gruesa hasta ~2,4 km).
void terrain_draw(const Terrain *t);
// Los arboles de los chunks cargados, en su estacion (despues de terrain_draw, dentro de BeginMode3D): los de cerca con
// todo su detalle y solo los que caen delante de la camara (src/world/trees.h).
void terrain_draw_trees(const Terrain *t, Camera3D cam);
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
