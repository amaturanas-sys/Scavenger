// El kit de edificios de la temprana edad del hierro (C puro), la parte B del plan de herramientas
// (docs/PLAN_HERRAMIENTAS_IA.md; la investigacion: docs/ARQUITECTURA_EDAD_DEL_HIERRO.md). Cada
// edificio se arma con piezas de una rejilla de un metro (cuatro celdas de voxel por metro):
//  - muros de adobe sobre zocalo de piedra, de piedra abajo y adobe arriba, de postes con zarzo y
//    barro, de turba sobre piedra o de fieltro;
//  - postes, vigas que asoman, aleros;
//  - techos planos de vigas, caña y barro con pretil, a dos aguas de paja o de turba, conicos;
//  - puertas, ventanucos, escaleras, patios;
// y detalles al azar (cantaros, leña, el horno de pan, pieles al sol, franjas del fieltro). Las
// reglas eligen con la semilla la planta (bloque, L o patio; rectangular, redonda o larga), las
// medidas y lo demas, al estilo de su region: cada semilla da otra casa de la misma familia. La
// puerta mira siempre al frente (+X), a la calle.
// El resultado es una rejilla de voxeles (src/sim/voxel.h, materiales de src/sim/voxmat.h) con el
// cimiento bajo el suelo; src/world/voxstruct.c la malla y la dibuja.
#ifndef ESTEPA_BUILDKIT_H
#define ESTEPA_BUILDKIT_H

#include <stdbool.h>
#include <stdint.h>

#include "voxel.h"
#include "world.h"

// Los tipos (crecen al final).
typedef enum {
    BK_HOUSE,     // la vivienda de la region: yurta, casa de postes o redonda, de piedra y adobe, larga de turba, de adobe con patio
    BK_PALACE,    // el palacio: la orda del kan sobre su tarima, la sala larga, la sala de columnas sobre su terraza
    BK_FORGE,     // la fundicion: el horno de cuba de barro, el fuelle, la escoria, el carbon y el yunque, bajo un cobertizo
    BK_BARRACKS,  // el cuartel, con el armero de lanzas
    BK_CARPENTRY, // la carpinteria: cobertizo, troncos apilados y un caballete
    BK_GRANARY,   // el granero sobre postes (en el desierto, de adobe con silos)
    BK_TEMPLE,    // el templo del fuego, de planta en cruz (solo en el desierto y el altiplano)
    BK_WAGON,     // la carreta-casa de la estepa: cuatro o seis ruedas y una casilla de fieltro
    // La muralla de la capital (src/sim/army.h): adobe sobre zocalo, piedra, el murus gallicus del
    // bosque, la empalizada sobre el terraplen de la estepa, piedra seca y turba en los fiordos.
    BK_WALL,      // un tercio del tramo entre dos torres
    BK_TOWER,     // la torre de cada esquina
    BK_GATE,      // la puerta con su antemuro (la puerta acodada); la semilla par, cerrado hacia +Z
    // Las ruinas de los sitios del mundo (src/world/voxstruct.c): las mismas piezas, comidas por el
    // tiempo (lo alto cae primero) y tapadas de pasto, de musgo o de arena. Todas grandes.
    BK_RUIN_FORT, // la fortaleza derruida: el recinto de muralla de la region, su puerta y sus torres rotas
    BK_RUIN_CITY, // la ciudad enterrada: casas de adobe y cupulas asomando de la arena (solo en el desierto)
    // Las obras de la tribu (src/sim/actions.h) y lo que queda de ellas, a la medida de su id del
    // inventario (bk_for_item): src/world/props.c las dibuja con el kit mientras no haya un modelo
    // importado. Al estilo de la estepa (la tribu es nomada); las chicas y las grandes son dos obras.
    BK_SHELTER,    // el refugio de ramas y pieles
    BK_PALISADE,   // un tramo de empalizada
    BK_DRYSTONE,   // un tramo de muro de piedra seca
    BK_OVEN,       // el horno de cocina de barro
    BK_FURNACE,    // el horno de fundicion: las chicas, el del bronce; las grandes, el del acero
    BK_WATCHTOWER, // la atalaya de madera
    BK_CORRAL,     // el corral de cuerdas y estacas
    BK_SMITHY,     // el taller del herrero
    BK_TENT,       // las chicas, la tienda ligera de cuero; las grandes, la choza de fieltro
    BK_RUBBLE,     // las chicas, los escombros; las grandes, un muro en ruinas
    BK_ASHES,      // las chicas, las cenizas de una obra; las grandes, una yurta quemada
    BK_KINDS
} BkType;

// Las variantes de cada tipo y region que el juego tiene a la vez: la 0 y la 1, chicas (las aldeas
// y las tribus: caben en 7 x 7 m); la 2 y la 3, grandes (las capitales).
#define BK_VARIANTS 4
#define BK_SMALL 7.0f        // m: la huella de las chicas
#define BK_FOUND 3           // celdas de cimiento bajo el suelo

// Las medidas en m van desde el origen del modelo: el centro de la base de la rejilla, a ras del
// suelo (X hacia el frente, Z de lado, Y arriba).
typedef struct {
    float l, w, h;           // m: el largo (X, hacia el frente), el ancho (Z) y el alto sobre el suelo
    float found;             // m de cimiento bajo el suelo: se dibuja hundido
    float front;             // m: hasta donde llega lo construido hacia el frente (+X)
    float door_along, door_across, door_up; // m: el centro del umbral, por fuera (door_up: la altura del piso)
    int door_x, door_y;      // la puerta: la columna x de su cara de afuera y la celda del umbral (-1: abierto, sin puerta)
    int door_z0, door_z1;    // y su ancho (celdas)
    bool enclosed;           // muros cerrados (los cobertizos y las carretas, no); adentro, relleno de M_SHADOW
} BkInfo;

// ¿Tiene ese tipo en esa region? (el templo del fuego, solo en el desierto y el altiplano; la ciudad
// enterrada, solo en el desierto; las obras de la tribu, solo al estilo de la estepa)
bool bk_available(BkType t, Region r);
// La semilla de cada variante de un tipo y una region (siempre la misma). La de la puerta tiene la
// paridad de la variante: la par cierra el antemuro hacia +Z, como la puerta par de la muralla.
uint32_t bk_variant_seed(BkType t, Region r, int variant);
// Arma el edificio en g (que reserva; vox_free al terminar). info (o NULL): sus medidas y su
// puerta. small: que quepa en BK_SMALL x BK_SMALL. false si no lo tiene la region o no hay memoria.
bool bk_build(BkType t, Region r, uint32_t seed, bool small, VoxGrid *g, BkInfo *info);
// El nombre del tipo en esa region (UTF-8, para traducir con T()).
const char *bk_name(BkType t, Region r);
// Con que se dibuja un id del inventario de las obras y las viviendas de la tribu (y lo que queda de
// ellas) mientras no tenga modelo: el tipo, la region y la primera de las dos variantes de su tamaño
// (la otra, variant + 1). Las casas se escalan a su lugar; lo demas ya tiene sus medidas. false si no
// es una de ellas.
bool bk_for_item(const char *id, BkType *t, Region *r, int *variant);

#endif
