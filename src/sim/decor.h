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

#endif
