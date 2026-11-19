// El decorado del paisaje (C puro): los 41 modelos de OpenGameArt (FREE Remastered Meadows, EmacEArt, CC0)
// adaptados a la estetica del juego (tools/modelos/adaptar_oga.py) y las normas para sembrarlos. Es parte de la
// capa 4, la dispersion (docs/MUNDO.md): src/world/terrain.c los siembra en cada trozo, siempre los mismos para
// la misma semilla, y src/world/decor_draw.c los dibuja.
//  - Lo natural, por casillas fijas: una cosa o nada en cada una, segun la region, el agua cerca y la pendiente
//    (decor_pick). Las crestas de roca, en casillas de 96 m; las rocas, colinas y acantilados, de 24 m; las
//    rocas bajas, los arboles y los arbustos, de 12 m. Los hongos, las ramas y las raices, al pie de los arboles
//    del bosque (decor_pick_foot).
//  - Lo hecho por el hombre, solo donde tiene sentido (decor_layout): cercas, carros, bancos, barriles y tablas
//    en las afueras de los pueblos; un corral junto a los campamentos de las tribus; restos alrededor de las
//    ruinas. Y un puente sobre un arroyo angosto cerca de cada pueblo, y un muelle en la orilla de un lago o del
//    mar (src/world/terrain.c los busca).
#ifndef ESTEPA_DECOR_H
#define ESTEPA_DECOR_H

#include <stdbool.h>
#include <stdint.h>

#include "world.h"

typedef enum {
    DECOR_CLASS_RIDGE,  // crestas de roca: casillas de 96 m
    DECOR_CLASS_BIG,    // rocas, colinas y acantilados: casillas de 24 m
    DECOR_CLASS_MEDIUM, // rocas bajas, arboles y arbustos: casillas de 12 m
    DECOR_CLASS_FOOT,   // al pie de los arboles del bosque
    DECOR_CLASS_HUMAN,  // lo hecho por el hombre
    DECOR_CLASSES
} DecorClass;

#define DECOR_RIDGE_SLOT 96.0f
#define DECOR_BIG_SLOT 24.0f
#define DECOR_MEDIUM_SLOT 12.0f

typedef enum {
    DECOR_CRESTA, DECOR_CRESTA_LARGA, // crestas
    DECOR_ROCA_CAPAS, DECOR_ARENISCA_PARTIDA, DECOR_PENASCOS_PANTANO, DECOR_ACANTILADO, // grandes
    DECOR_COLINA, DECOR_COLINA_ANCHA, DECOR_REPECHO, DECOR_LOMA,
    DECOR_LAJA, DECOR_PENASCO_ARENISCA, DECOR_LAJAS_PANTANO, DECOR_MONTICULO, DECOR_ROCA_BAJA, // medianos
    DECOR_ACACIA, DECOR_SAUCE_MUERTO, DECOR_ARBOL_CAIDO, DECOR_ARBUSTO_SECO,
    DECOR_HONGOS, DECOR_RAMA, DECOR_RAIZ, // al pie de los arboles
    DECOR_CERCA_A, DECOR_CERCA_B, DECOR_CERCA_ROTA, DECOR_CERCA_LARGA, DECOR_CERCA_QUEBRADA, // las cercas
    DECOR_PORTAL, DECOR_MARCO, DECOR_CERCA_CAIDA, DECOR_CERCA_ALTA,
    DECOR_PUENTE, DECOR_TARIMA, DECOR_MUELLE, DECOR_MUELLE_FIN, // el agua
    DECOR_CARRO, DECOR_BANCO, DECOR_BARRIL, DECOR_COFRE, DECOR_COFRE_ABIERTO, DECOR_TABLAS, // los enseres
    DECOR_KINDS
} DecorKind;
#define DECOR_NONE (-1)

// Las normas de cada modelo.
typedef struct {
    const char *id;              // el id del inventario (assets/inventario.tsv)
    DecorClass cls;
    float length, height, width; // a escala 1 (m): largo (X), alto (Y) y ancho (Z), las del inventario
    float chance[REGION_COUNT];  // lo natural: la chance en cada casilla de su clase, con el peso de cada region
    bool wet;                    // solo junto al agua dulce (los peñascos del pantano, el sauce muerto); si no, en seco
    float slope_min, slope_max;  // la pendiente del suelo bajo la huella (desnivel / distancia)
    bool snow;                   // puede quedar sobre la linea de nieve (la roca, no lo que crece)
    float scale_min, scale_max;  // su variacion de tamaño
    float sink;                  // cuanto se entierra, en fraccion de su alto: no flota en la cuesta
} DecorNorm;

const DecorNorm *decor_norm(DecorKind kind);
// El radio de su huella (m): la mitad de su lado mas largo, a esa escala.
float decor_radius(DecorKind kind, float scale);
// Lo natural de esa clase para una casilla: con los pesos de las regiones k, junto al agua dulce o no, y r en
// [0, 1) (el azar de la casilla). DECOR_NONE casi siempre: la suma de las chances es chica.
int decor_pick(DecorClass cls, const float k[REGION_COUNT], bool wet, float r);
// Al pie de un arbol: con el peso del bosque, r en [0, 1). Hongos, una rama caida o una raiz, o nada.
int decor_pick_foot(float forest, float r);

// Lo hecho por el hombre, alrededor de un lugar: cada cosa en un angulo y una distancia del centro.
typedef enum {
    DECOR_PLACE_VILLAGE, // las afueras de una aldea (fuera de sus campos)
    DECOR_PLACE_CAPITAL, // las afueras de una capital
    DECOR_PLACE_CAMP,    // junto al campamento de una tribu: un corral
    DECOR_PLACE_RUIN,    // alrededor de una ruina: restos
    DECOR_PLACES
} DecorPlace;
typedef struct {
    float angle, radius; // desde el centro del lugar
    DecorKind kind;
    bool along;          // alineado con el anillo (las cercas); si no, girado al azar
} DecorSpot;
#define DECOR_SPOTS_MAX 24
// El anillo donde va lo de ese lugar: entre ring0 y ring1 m del centro (r_place: el radio del lugar, el de su
// sitio para las ruinas).
void decor_ring(DecorPlace place, float r_place, float *ring0, float *ring1);
// Lo que va alrededor de un lugar, siempre lo mismo para la misma semilla. Devuelve cuantas cosas (<= max).
int decor_layout(DecorPlace place, uint32_t seed, float r_place, DecorSpot *out, int max);

// Un puente sobre un arroyo (no los trenzados, de hasta 4 m de agua) a menos de 220 m de (px, pz), de orilla alta a
// orilla alta: las dos orillas suben a DECOR_BRIDGE_CLEAR m o mas sobre el agua, y el tablero, en arco, apoya sus
// puntas 40 cm adentro de cada una (la mas baja, justo en el suelo; la otra, hasta 35 cm hundida), sin tocar el suelo
// en el medio, y de cada punta se sale a una orilla pareja. El modelo se escala (scale, 1 a 1,2) y se estira a lo
// largo (stretch, su X: de 0,9 a DECOR_BRIDGE_STRETCH) para cubrir la luz. Fuera de los pueblos, sus campos y los
// demas lugares. El cruce mas corto; siempre el mismo.
#define DECOR_BRIDGE_CLEAR 1.2f
#define DECOR_BRIDGE_STRETCH 2.2f
typedef struct {
    float x, z, y, yaw;   // el centro, la base del modelo y su giro (su X, de orilla a orilla)
    float scale, stretch; // la escala del modelo y lo que se estira a lo largo
    float water;          // el agua debajo
} DecorBridge;
bool decor_bridge(const World *w, float px, float pz, DecorBridge *out);
// El tablero de lo que se camina (el puente, los muelles, la tarima) en (lx, lz): su alto sobre la base del modelo, en
// sus ejes (lx a lo largo, su X; lz a lo ancho) y en metros del mundo. Negativo si (lx, lz) cae fuera del tablero.
float decor_deck(DecorKind kind, float scale, float stretch, float lx, float lz);
// Lo que choca de cada cosa del decorado, lo mas sencillo posible (las colisiones: src/world/terrain.c): nada (lo que
// se pisa: lo de menos de 45 cm de alto, lo del pie de los arboles, las tablas y la cerca caida; y lo que se camina:
// el puente, los muelles y la tarima), un circulo (el tronco de los arboles), dos postes (el portal y el marco, en las
// puntas de su X) o una capsula a lo largo de su X (lo demas: del 90 % de su ancho y su largo). En metros, a escala 1:
// el radio y el medio largo del eje.
// Las rocas (de la cresta a la roca baja): se caminan y se trepan (src/world/terrain.c) y toman el color de su region.
static inline bool decor_climbable(DecorKind kind) { return kind <= DECOR_ROCA_BAJA; }
typedef enum { DECOR_HIT_NONE, DECOR_HIT_CIRCLE, DECOR_HIT_POSTS, DECOR_HIT_CAPSULE } DecorHit;
DecorHit decor_collider(DecorKind kind, float *radius, float *half);
// Medio largo del tablero del puente (a escala 1, sin estirar): sus puntas, a 0,55 m de la base; el medio, a 0,95.
#define DECOR_BRIDGE_HALF 2.35f

#endif
