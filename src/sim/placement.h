// Ubicar una obra en el campamento (C puro): la fase 4b del plan grande
// (docs/PLAN_GRAN_ACTUALIZACION.md). La obra es una huella: un rectangulo girado con las medidas
// del inventario (largo en X local, ancho en Z local, como dibuja src/world/props.c).
//  - El sitio vale si queda dentro del radio del campamento, en seco, con poca pendiente y sin
//    pisar nada (yurtas, fuegos, otras estructuras, otras obras, arboles). Si no, se dice por que.
//  - Q/E la giran de a PLACE_ROT_STEP; con la rejilla, el centro se alinea cada PLACE_GRID m.
//  - Las cercas y los muros se trazan con dos puntos: tramos del ancho del modelo, de un extremo
//    al otro, sin huecos (si el largo no da justo, los tramos se enciman un poco).
#ifndef ESTEPA_PLACEMENT_H
#define ESTEPA_PLACEMENT_H

#include <stdbool.h>

#define PLACE_ROT_STEP 0.2617994f // 15 grados
#define PLACE_GRID 2.0f           // m
#define PLACE_MAX_SLOPE 0.3f      // desnivel / largo de la huella
#define PLACE_FLAT 0.4f           // m: un desnivel menor que esto siempre vale
#define PLACE_WET 0.05f           // m de agua: ya es «en el agua»

typedef struct {
    float x, z;   // centro
    float yaw;    // giro (rad): el frente local (+Z) mira a (sin yaw, cos yaw)
    float hl, hw; // medias medidas: largo (X local) y ancho (Z local)
} Footprint;

typedef enum {
    PLACE_OK,
    PLACE_OUTSIDE, // fuera del campamento
    PLACE_WATER,   // en el agua
    PLACE_STEEP,   // demasiado empinado
    PLACE_OVERLAP, // encima de algo (obstaculo hit)
    PLACE_KINDS
} PlaceResult;

typedef struct {
    PlaceResult res;
    int hit; // PLACE_OVERLAP: el obstaculo que pisa (indice); si no, -1
} PlaceCheck;

// El suelo: altura y profundidad del agua (<= 0 en seco) en (x, z). water puede ser NULL.
typedef struct {
    float (*height)(void *ud, float x, float z);
    float (*water)(void *ud, float x, float z);
    void *ud;
} PlaceGround;

// La huella de un objeto de medidas l (largo, X local) por w (ancho, Z local), con un margen.
Footprint footprint_make(float x, float z, float yaw, float l, float w, float margin);
// Las cuatro esquinas (x, z) en el mundo.
void footprint_corners(const Footprint *f, float out[4][2]);
// Se pisan (prueba de ejes separados de dos rectangulos girados).
bool footprints_overlap(const Footprint *a, const Footprint *b);
// ¿El sitio vale? (cx, cz, radius): el campamento (radius <= 0: no hay limite, se esta fundando
// uno). obs: lo que no se puede pisar. Se revisan, en orden: el radio, el agua, la pendiente y
// los obstaculos.
PlaceCheck place_check(const Footprint *f, float cx, float cz, float radius, const PlaceGround *g, const Footprint *obs, int nobs);
// Girar de a pasos de PLACE_ROT_STEP; el resultado queda en [0, 2pi).
float place_rotate(float yaw, int steps);
// Alinear v a la rejilla de paso grid que pasa por origin.
float place_snap(float v, float origin, float grid);
// Una cerca o un muro de (ax, az) a (bx, bz) con tramos de ancho seg_w (el ancho del modelo, Z
// local) y grosor seg_l: cuantos tramos (hasta max) y sus huellas, girados a lo largo de la linea.
int place_wall(float ax, float az, float bx, float bz, float seg_w, float seg_l, Footprint *out, int max);

#endif
