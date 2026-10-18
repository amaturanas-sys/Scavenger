// Los materiales de las estructuras de voxeles: el valor de cada celda de una VoxGrid
// (src/sim/voxel.h), que src/world/voxstruct.c pinta con la paleta de su region. Los usan las
// recetas de src/world/voxstruct.c y el kit de edificios (src/sim/buildkit.c). Crecen al final.
#ifndef ESTEPA_VOXMAT_H
#define ESTEPA_VOXMAT_H

enum {
    M_NONE, M_STONE, M_STONE_DARK, M_EARTH, M_WOOD, M_WOOD_DARK, M_FELT, M_FELT_ROOF, M_GRASS, M_ADOBE, M_TURF,
    M_WATER, M_RED, M_BLUE, M_YELLOW, M_CARVE, M_STONE_LICHEN,
    M_BASALT, M_BASALT_L, M_GRANITE, M_GRANITE_D, M_SAND1, M_SAND2, M_SAND3, M_CHALK, M_CHALK_D, M_ICE, M_ICE_B, M_ICE_D, M_MOSS,
    // El kit de edificios de la edad del hierro (docs/ARQUITECTURA_EDAD_DEL_HIERRO.md):
    M_THATCH,    // paja del techo
    M_DAUB,      // zarzo con barro, encalado
    M_REED,      // la caña bajo el barro de los techos planos
    M_CLAY,      // el barro cocido del horno de cuba
    M_SLAG,      // la escoria y el carbon
    M_HIDE,      // el cuero (fuelles, pieles al sol, toldos)
    M_FELT_DARK, // el fieltro gris de las yurtas pobres y las crines de los estandartes
    M_SHADOW,    // lo de adentro de las casas (no se ve; detras de las puertas y ventanas, oscuro)
    M_COUNT
};

#endif
