// Estructuras de voxeles (src/sim/voxel.h): ruinas y construcciones en uso, casas de cada region
// y yurtas. Cada modelo se arma una vez (con su paleta y su erosion, si es ruina) y se dibuja
// girado donde haga falta. Las ruinas pierden lo alto primero (erosion por ruido).
#ifndef ESTEPA_VOXSTRUCT_H
#define ESTEPA_VOXSTRUCT_H

#include <stdbool.h>

#include "raylib.h"
#include "sim/world.h"

typedef enum {
    VB_YURT,          // yurta de fieltro (estepa; tribus)
    VB_LOG_CABIN,     // cabaña de troncos (bosque)
    VB_STONE_HOUSE,   // casa de piedra con techo de tablas (altiplano)
    VB_LONGHOUSE,     // casa larga con techo de turba (costa)
    VB_ADOBE,         // casa de adobe con cupula (desierto)
    VB_TOWER_STONE,   // torre de muralla (capitales)
    VB_TOWER_WOOD,
    VB_WALL_STONE,    // tramo de muralla de 8 m (a lo largo de x), con almenas
    VB_PALISADE,      // tramo de empalizada de troncos de 8 m
    VB_KINDS
} VoxBuilding;

// Dibuja una estructura del mundo (su clase y region) en (x, y, z) con giro yaw (radianes).
void voxs_draw_site(SiteKind kind, Region region, Vector3 at, float yaw, Color tint);
// Dibuja una casa o torre; scale la agranda (la casa del jefe).
void voxs_draw_building(VoxBuilding b, Vector3 at, float yaw, float scale, Color tint);
// La casa tipica de una region.
VoxBuilding voxs_region_house(Region r);
// Formaciones del paisaje (de las fotos de referencia de cada region), en 4 variantes:
typedef enum {
    VF_BASALT,    // roquerio de columnas de basalto en la playa negra y las rompientes (fiordos)
    VF_SEA_STACK, // farallon de basalto en el mar, con un gorro de hierba
    VF_BOULDER,   // canto rodado claro en el cauce y la orilla de los rios
    VF_HOODOO,    // torre de arenisca en estratos con sombrero mas duro (desierto)
    VF_MESA,      // farallon estratificado con alero y cueva (cañones del desierto)
    VF_CHALK,     // formacion de creta blanca erosionada por el viento (desierto)
    VF_SERAC,     // bloques de hielo del frente del glaciar, con grietas azules
    VF_ICEBERG,   // tempano en los lagos de deshielo
    VF_KINDS
} VoxFeature;
#define VF_VARIANTS 4
void voxs_draw_feature(VoxFeature f, int variant, Vector3 at, float yaw, float scale, Color tint);
void voxs_unload(void);

#endif
