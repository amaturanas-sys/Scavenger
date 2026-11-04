// Las cuevas (C puro, sin raylib ni memoria dinamica; docs/PLAN_TACTICO_Y_CUEVAS.md, B1 a B3): una por
// guarida del mundo, determinista por la semilla y su indice.
//
// La planta, como en la hoja: la Cisterna Magna abajo, en el centro; de 3 a 9 recamaras en anillo a su
// alrededor, mas altas, unidas una tras otra por pasillos; la bajada desde la boca hasta la Cisterna, la
// mas empinada y sin vuelta; los dos extremos del anillo (a e i) salen a la boca, y la Cisterna sube al
// anillo por el fondo. Bajar a la Cisterna obliga a salir recorriendo el anillo.
//
// Lo de adentro va en objetos, con su sala y su lugar: lo comun (estalactitas, estalagmitas, telarañas,
// insectos, vetas, sangre, huesos, charcos), lo de las fieras (nidos, huevos, presas, armas abandonadas,
// botin en la Cisterna) y lo de los habitados (antorchas, columnas, cofres, anforas, depositos de armas,
// celdas con prisioneros, pinturas, fogatas). Los que se toman dan un objeto del inventario
// (cave_object_loot); los prisioneros, al rescatarlos, se unen o traicionan (cave_prisoner_joins).
//
// Coordenadas de la cueva: la boca en (0, 0), con el suelo de la boca en y = 0 y +z hacia adentro. Tras la boca, el
// vestibulo (una camara chica, llana): de su borde salen los tres pasillos de la boca, cada uno por su lado. El
// interior es un espacio aparte (el terreno no se ahueca): se carga al explorar (src/game/cave_scene.c lo arma y lo
// dibuja; src/game/cave_game.c lo recorre).
//
// Las medidas (v0.14.0, docs/PLAN_CUEVAS_GRANDES.md): las recamaras de 6,5 a 9,5 m de radio y la Cisterna de 13 a 17;
// sobre todo su suelo, el techo a mas de tres personas (la pared recta hasta CAVE_WALL_HEIGHT y despues la boveda); los
// pasillos, de CAVE_CORRIDOR_MIN o mas y con vueltas (salvo la bajada, recta y empinada). La planta usa los mismos
// numeros al azar, en el mismo orden, que la de la v0.13: cada objeto sigue siendo el mismo (GameActions.cave_done).
#ifndef ESTEPA_CAVES_H
#define ESTEPA_CAVES_H

#include <stdbool.h>
#include <stdint.h>

#define CAVE_ROOMS_MAX 10      // la Cisterna y hasta 9 recamaras
#define CAVE_CORRIDORS_MAX 16
#define CAVE_OBJECTS_MAX 64    // los que se toman, primero; los adornos, hasta llenar
#define CAVE_CORRIDOR_HALF 1.2f // m: medio ancho de un pasillo
#define CAVE_PERSON_HEIGHT 1.8f // m: una persona (la medida de los techos: en las salas, al menos tres)
#define CAVE_ROOM_HEIGHT 9.0f   // m: del suelo a la clave de la boveda de una recamara (la Cisterna, el doble)
#define CAVE_WALL_HEIGHT 6.0f   // m: la pared recta de una recamara, hasta donde empieza la boveda (la Cisterna, el doble)
#define CAVE_CORRIDOR_HEIGHT 2.8f // m: del suelo al techo de un pasillo
#define CAVE_CORRIDOR_MIN 10.0f // m: lo menos que mide un pasillo de borde a borde de sus salas
#define CAVE_PATH_PTS 25        // los puntos del eje de un pasillo (24 tramos), del comienzo (from) al final (to)
#define CAVE_PROFILE_PTS 8      // los puntos del perfil de una sala (cave_room_profile)
#define CAVE_IN_CORRIDOR 100    // cave_floor: where >= esto es un pasillo (where - CAVE_IN_CORRIDOR)
#define CAVE_MOUTH -1           // CaveCorridor: el extremo en la boca (el vestibulo)
#define CAVE_VESTIBULE -2       // cave_floor: where del vestibulo
#define CAVE_VEST_Z 2.6f        // m: el centro del vestibulo, tras la boca
#define CAVE_VEST_R 3.0f        // m: su radio (de su borde salen la bajada y las dos salidas, sin pisarse)
#define CAVE_VEST_HEIGHT 3.6f   // m: su techo
#define CAVE_DECOR_MAX 96       // el decorado de una cueva (cave_decor)

typedef enum { CAVE_ANIMAL, CAVE_ABANDONED, CAVE_BANDITS, CAVE_TRIBE, CAVE_EMPIRE, CAVE_KINDS } CaveKind;

typedef struct {
    float x, z;    // centro, en la cueva
    float floor_y; // el suelo (la boca en 0; todo lo de adentro, mas abajo)
    float radius;
} CaveRoom;

typedef struct {
    int from, to;  // salas (CAVE_MOUTH: la boca)
    float slope;   // desnivel por metro, a lo largo de su eje
    bool one_way;  // la bajada: solo se recorre hacia la Cisterna
    float length;  // m: el largo del eje, con sus vueltas
    float px[CAVE_PATH_PTS], pz[CAVE_PATH_PTS]; // el eje, del borde de from al de to (el suelo baja parejo a lo largo)
} CaveCorridor;

typedef enum {
    // Lo comun a todas.
    COBJ_STALACTITE, COBJ_STALAGMITE, COBJ_WEB, COBJ_INSECTS, COBJ_VEIN, COBJ_BLOOD, COBJ_BONES, COBJ_PUDDLE,
    // Las de fieras (y las abandonadas).
    COBJ_NEST, COBJ_EGGS, COBJ_PREY, COBJ_WEAPON, COBJ_LOOT,
    // Las habitadas (y, en ruinas, las abandonadas).
    COBJ_TORCH, COBJ_COLUMN, COBJ_CHEST, COBJ_AMPHORA, COBJ_ARMS, COBJ_CELL, COBJ_PAINTING, COBJ_FIRE,
    COBJ_KINDS
} CaveObjectKind;

typedef enum { PRISONER_SHEPHERD, PRISONER_HUNTER, PRISONER_SMITH, PRISONER_OUTLAW, PRISONER_KINDS } PrisonerKind;

// El decorado: la roca misma (rocas, y lo que agreguen las tareas de docs/PLAN_CUEVAS_GRANDES.md). No se toma ni se
// guarda: sale de la cueva cada vez que se carga (cave_decor). Los tipos crecen al final.
typedef enum { CDEC_ROCK, CDEC_KINDS } CaveDecorKind;

typedef struct {
    uint8_t kind;    // CaveDecorKind
    uint8_t room;    // la sala
    uint8_t variant; // 0..3: el modelo de voxeles (src/world/voxcave.h)
    bool blocks;     // no se camina a traves (radius)
    float x, z, y;   // el pie, en el suelo de la sala
    float size;      // m: el alto (o el ancho) del modelo
    float yaw;
    float radius;    // m: lo que ocupa en el suelo
} CaveDecor;

typedef struct {
    uint8_t kind;     // CaveObjectKind
    uint8_t room;     // la sala
    uint8_t prisoner; // COBJ_CELL: 0 vacia, 1 + PrisonerKind
    bool lit;         // COBJ_TORCH y COBJ_FIRE: encendida (en las abandonadas, no)
    float x, z, y;    // en la cueva; y: el suelo (la estalactita, el techo)
    float size;
} CaveObject;

typedef struct {
    float x, z;        // la boca en el mundo
    float yaw;         // hacia adentro (el mundo: (sin yaw, cos yaw))
    int region;
    CaveKind kind;
    int animal_species; // la de la guarida (las de fieras y las abandonadas)
    int room_count;     // la Cisterna (0) y el anillo (1..room_count-1, de a a i)
    CaveRoom rooms[CAVE_ROOMS_MAX];
    int corridor_count; // el 0 es la bajada
    CaveCorridor corridors[CAVE_CORRIDORS_MAX];
    int object_count;
    CaveObject objects[CAVE_OBJECTS_MAX];
} Cave;

void cave_generate(Cave *c, uint32_t world_seed, int index, float x, float z, int region, int species);

// Las reglas de la planta (las pruebas): salas que no se pisan, todo alcanzable desde la boca, siempre
// una salida, la bajada sin vuelta y la mas empinada.
bool cave_layout_valid(const Cave *c);
// De una sala a otra por los pasillos (CAVE_MOUTH: la boca), respetando la bajada.
bool cave_room_reachable(const Cave *c, int from, int to);
// Los extremos del pasillo k, en el borde de sus salas (la boca: su punto), con la altura del suelo.
void cave_corridor_ends(const Cave *c, int k, float *ax, float *az, float *ay, float *bx, float *bz, float *by);
// Un punto del eje del pasillo k a t (0..1) de su largo, con la altura del suelo y la direccion (dx, dz, unitaria).
void cave_corridor_at(const Cave *c, int k, float t, float *x, float *z, float *y, float *dx, float *dz);
// Lo que dista (x, z) del eje del pasillo k (en el plano).
float cave_corridor_distance(const Cave *c, int k, float x, float z);

// El suelo en (x, z): true dentro de una sala, el vestibulo o un pasillo, con su altura. where: la sala (0..),
// CAVE_VESTIBULE o CAVE_IN_CORRIDOR + el pasillo (donde se cruzan, el que no es la bajada).
bool cave_floor(const Cave *c, float x, float z, float *y, int *where);
// Un paso de (x, z) a (nx, nz): false si da contra la roca o si sube por la bajada.
bool cave_step_ok(const Cave *c, float x, float z, float nx, float nz);
// Donde se aparece al entrar (al pie de la boca, mirando adentro) y si (x, z) ya salio por la boca.
void cave_entry(const Cave *c, float *x, float *z, float *yaw);
bool cave_at_exit(const Cave *c, float x, float z);
// De la cueva al mundo y del mundo a la cueva (solo para la boca: el interior no esta en el mapa).
void cave_to_world(const Cave *c, float lx, float lz, float *wx, float *wz);
void cave_from_world(const Cave *c, float wx, float wz, float *lx, float *lz);
// El techo sobre un punto del suelo (where y floor_y, de cave_floor): la clave de la boveda (en un pasillo, su techo).
float cave_ceiling(const Cave *c, int where, float floor_y);
// La pared de una sala (room; CAVE_VESTIBULE: el vestibulo) en el angulo a (atan2 de x y z desde su centro): siempre mas
// alla de donde se camina, despareja.
float cave_wall_radius(const Cave *c, int room, float a);
// Si en el angulo a de la pared de una sala (o del vestibulo) hay una puerta: un pasillo del otro lado.
bool cave_door_at(const Cave *c, int room, float a);
// El perfil de una sala, de abajo arriba (j de 0 a CAVE_PROFILE_PTS - 1): la fraccion del radio de su pared y la altura
// sobre su suelo. La pared recta hasta el techo de los pasillos (las puertas) y hasta CAVE_WALL_HEIGHT; despues, la
// boveda hasta la clave (cave_ceiling). Lo que se dibuja y lo que se choca salen de aqui.
void cave_room_profile(const Cave *c, int room, int j, float *frac, float *height);
// La roca de verdad sobre (x, z) (where y floor_y, de cave_floor): en una sala, la boveda tal como es (mas baja junto
// a la pared que en el centro); en un pasillo, su techo. La camara no pasa de aqui.
float cave_rock_above(const Cave *c, float x, float z, int where, float floor_y);
// Donde va un objeto: lo de la pared (antorchas, vetas, telarañas, pinturas, depositos, celdas), sobre la roca; las
// estalactitas, colgadas de la boveda (y: de donde cuelgan); yaw: mirando al centro de su sala.
void cave_object_at(const Cave *c, int obj, float *x, float *y, float *z, float *yaw);
bool cave_object_on_wall(int kind);

// El decorado de la cueva (determinista, desde su lugar en el mundo): out tiene lugar para max. Cada cosa en el suelo
// de su sala, lejos de las puertas y de los objetos; lo que tapa el paso, solo junto a la pared (del centro de cada sala
// se llega a todas sus puertas). Devuelve cuantas.
int cave_decor(const Cave *c, CaveDecor *out, int max);
// Si (x, z) da contra algo del decorado que tapa el paso.
bool cave_decor_blocked(const CaveDecor *d, int n, float x, float z);

// Lo que da un objeto al tomarlo (F): el id del inventario y cuantos; NULL si no se toma (un adorno,
// una celda, una antorcha).
const char *cave_object_loot(const Cave *c, int obj, int *count);
// Un prisionero (COBJ_CELL con prisoner): la probabilidad (0..100) de que traicione al rescatarlo. Un
// forajido traiciona mas que un pastor; en una cueva de forajidos, mas.
int cave_betray_percent(const Cave *c, int obj);
// Rescatarlo (roll en [0, 1)): true se une a la tropa; false te traiciona e intenta saquearte.
bool cave_prisoner_joins(const Cave *c, int obj, float roll);

const char *cave_kind_name(CaveKind k);         // UTF-8, para T()
const char *cave_prisoner_name(PrisonerKind k); // UTF-8, para T()

#endif
