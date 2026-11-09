// Los vestigios de los asentamientos antiguos: la planta de un pueblo de otros tiempos, como las de las
// excavaciones (Ras Ibn Hani, Elefantina): un palacio de cuartos alrededor de su patio y casas de dos a
// cinco cuartos, separadas por calles; muros de ladrillo sobre zocalo de piedra, de los que el tiempo
// dejo poco: casi todos a ras del suelo, con brechas, y unos pocos tramos de pie (el muro en ruinas del
// inventario, estructura.ruina.muro). C puro: lo arma en voxeles el kit de edificios (BK_RUIN_TOWN,
// src/sim/buildkit.c) y src/game/world_game.c pone los tramos de pie donde dice la planta.
#ifndef ESTEPA_ANCIENT_H
#define ESTEPA_ANCIENT_H

#include <stdbool.h>
#include <stdint.h>

#define ANC_HALF 22.0f          // m: la planta entra en un cuadrado de 44 m de lado, centrado en el sitio
#define ANC_BUILDINGS_MAX 8
#define ANC_WALLS_MAX 160
#define ANC_DOORS_MAX 96
#define ANC_STANDING_MAX 4
#define ANC_STANDING_LEN 3.42f  // m: el largo del muro en ruinas del inventario (lo que tapa de la pared)
#define ANC_HEIGHT_MAX 1.6f     // m: lo mas alto que queda de un muro (fuera de los tramos de pie)

// Un rectangulo de la planta en m (x0 < x1, z0 < z1), con el sitio en el origen.
typedef struct {
    float x0, z0, x1, z1;
} AncRect;

typedef struct {
    AncRect r;  // la huella del muro
    float h;    // m: lo que le quedo de alto, en promedio (src/sim/ancient.c ancient_height: a lo largo, varia)
    bool outer; // de afuera (mas grueso) o de entre cuartos
} AncWall;

typedef struct {
    float x, z; // m: el centro del tramo de pie
    float yaw;  // radianes: el giro del muro en ruinas (su largo va a lo largo de la pared)
    AncRect r;  // lo que tapa de la pared (ahi el kit no pone muro)
} AncStanding;

typedef struct {
    uint32_t seed;                        // la del ruido de lo que queda de alto (ancient_height)
    AncRect buildings[ANC_BUILDINGS_MAX]; // el primero, el palacio
    int building_count;
    AncRect court;                        // el patio del palacio (sin cuartos)
    AncWall walls[ANC_WALLS_MAX];
    int wall_count;
    AncRect doors[ANC_DOORS_MAX];         // los vanos de las puertas (cortan los muros)
    int door_count;
    AncStanding standing[ANC_STANDING_MAX];
    int standing_count;
    int rooms;
} AncientPlan;

// La planta de una semilla (siempre la misma).
void ancient_plan(AncientPlan *p, uint32_t seed);
// ¿Hay muro en (x, z) en la planta entera (antes del tiempo: sin las puertas)?
bool ancient_wall_at(const AncientPlan *p, float x, float z);
// Lo que queda de alto en (x, z) (m; 0: sin muro, una puerta, una brecha o un tramo de pie).
float ancient_height(const AncientPlan *p, float x, float z);

#endif
