// Estructuras de voxeles (src/sim/voxel.h): ruinas y construcciones en uso, murallas y torres, y
// los edificios del kit de la edad del hierro (src/sim/buildkit.h: casas, palacios, talleres,
// graneros, templos y carretas). Cada modelo se arma una vez (con su paleta y su erosion, si es
// ruina) y se dibuja girado donde haga falta. Las ruinas pierden lo alto primero (erosion por ruido).
#ifndef ESTEPA_VOXSTRUCT_H
#define ESTEPA_VOXSTRUCT_H

#include <stdbool.h>

#include "raylib.h"
#include "sim/buildkit.h"
#include "sim/world.h"

typedef enum {
    VB_TOWER_STONE,   // torre de muralla (capitales)
    VB_TOWER_WOOD,
    VB_WALL_STONE,    // tramo de muralla de 8 m (a lo largo de x), con almenas
    VB_PALISADE,      // tramo de empalizada de troncos de 8 m
    VB_KINDS
} VoxBuilding;

// Dibuja una estructura del mundo (su clase y region) en (x, y, z) con giro yaw (radianes).
void voxs_draw_site(SiteKind kind, Region region, Vector3 at, float yaw, Color tint);
// Dibuja una torre o un tramo de muralla.
void voxs_draw_building(VoxBuilding b, Vector3 at, float yaw, float scale, Color tint);
// Un edificio del kit de la edad del hierro (src/sim/buildkit.h): su tipo, su region y su variante
// (0..BK_VARIANTS-1; la 0 y la 1, chicas). Cada uno se arma la primera vez que se dibuja: a menos
// de VOXS_KIT_NEAR m del ojo (voxs_set_lod), refinado; mas lejos, con la mitad de detalle.
void voxs_draw_kit(BkType t, Region region, int variant, Vector3 at, float yaw, float scale, Color tint);
// Sus medidas y su puerta (m, desde el punto donde se dibuja; ver BkInfo); false si no existe.
bool voxs_kit_info(BkType t, Region region, int variant, BkInfo *out);
// El ojo de la camara de este cuadro y hasta donde los edificios del kit van refinados.
#define VOXS_KIT_NEAR 32.0f
void voxs_set_lod(Vector3 eye, float near_m);
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
void voxs_forget_gpu(void); // ver terrain_forget_gpu

#endif
