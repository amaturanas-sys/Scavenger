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
    VB_KINDS
} VoxBuilding;

// Dibuja una estructura del mundo (su clase y region) en (x, y, z) con giro yaw (radianes).
void voxs_draw_site(SiteKind kind, Region region, Vector3 at, float yaw, Color tint);
// Dibuja una casa o torre; scale la agranda (la casa del jefe).
void voxs_draw_building(VoxBuilding b, Vector3 at, float yaw, float scale, Color tint);
// La casa tipica de una region.
VoxBuilding voxs_region_house(Region r);
void voxs_unload(void);

#endif
