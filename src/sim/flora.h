// La vegetacion baja (C puro): las matas de pasto, la stipa (el pasto pluma de la estepa), las
// flores, los helechos del bosque y los juncos de las orillas, que src/world/vegetation.c dibuja
// alrededor del jugador. Las reglas:
//  - donde crecen (flora_cell): por celdas del suelo de 2 m, siempre las mismas para el mismo
//    mundo; cuantas y de que clase segun la region y el agua cerca (flora_density); ni en el agua
//    honda, ni en la tierra pelada y la roca de las cuestas, ni en el lecho de los rios, la playa
//    negra o el frente del glaciar; ni en el campamento (pisoteado), los pueblos con sus campos o
//    junto a las estructuras y los campamentos de otras tribus;
//  - cuanto crecen con la estacion (flora_height): verdes en primavera, altas a comienzos del
//    verano, secas de pie y ocres en otoño, aplastadas en invierno; las flores, en primavera; la
//    stipa echa sus plumas plateadas de fines de la primavera a mediados del verano (flora_plume);
//  - el viento (flora_lean, flora_sway): se inclinan a favor y se mecen en olas que cruzan el
//    pasto; y se apartan al pasar (flora_trample).
#ifndef ESTEPA_FLORA_H
#define ESTEPA_FLORA_H

#include <stdint.h>

#include "world.h"

// Las clases de mata (crecen al final).
typedef enum {
    FLORA_GRASS,  // pasto: tres hojas
    FLORA_STIPA,  // stipa: alta y fina; en verano, plumas plateadas
    FLORA_FLOWER, // flores de la primavera: tulipanes y lirios de la estepa, flores del altiplano
    FLORA_FERN,   // helechos del sotobosque
    FLORA_REED,   // juncos de las orillas de lagos y rios (tambien en el agua baja)
    FLORA_KINDS
} FloraKind;

#define FLORA_CELL 2.0f    // m: las celdas del suelo (src/world/terrain.h: CHUNK_SIZE / CHUNK_CELLS)
#define FLORA_PER_CELL 8   // matas como mucho por celda (dos por m2)
#define FLORA_CANDIDATE_M2 (FLORA_CELL * FLORA_CELL / (float)FLORA_PER_CELL) // lo que ocupa cada candidato: medio m2
#define FLORA_CAMP_R 18.0f // el campamento inicial, en el origen: el pasto pisoteado
#define FLORA_TRAMPLE_R 1.1f

// Matas por m2 de esa clase con los pesos de las regiones k y el agua cerca (near_water [0, 1]: 1
// en la orilla de un lago o un rio, 0 a 2,5 m sobre el agua o lejos; el mar no cuenta).
float flora_density(FloraKind kind, const float k[REGION_COUNT], float near_water);
// La clase de una mata candidata con su azar r en [0, 1), o -1 si ahi no crece nada (cada
// candidato ocupa FLORA_CANDIDATE_M2: su probabilidad es la densidad por esa superficie; todas
// juntas, como mucho 1).
int flora_pick(const float k[REGION_COUNT], float near_water, float r);

// Una mata sembrada.
typedef struct {
    float x, y, z;               // la base, sobre el facetado del suelo
    float size;                  // su alto respecto del de su clase [0,7 .. 1,3]
    float rnd;                   // su azar [0, 1): el giro, el color y el viento
    float patch;                 // las manchas de nieve del suelo de su celda (como src/world/terrain.c)
    unsigned char kind;          // FloraKind
    unsigned char forest, high, fjord, desert; // los pesos de region (0..255), para el color
} FloraTuft;

// Las matas de la celda (gx, gz): de (gx, gz) * FLORA_CELL a la siguiente; seed: la del terreno.
// Siempre las mismas para el mismo mundo. Devuelve cuantas (hasta FLORA_PER_CELL).
int flora_cell(const World *w, uint32_t seed, int gx, int gz, FloraTuft *out);

// El alto (m) de una mata de esa clase en ese momento del año (year [0, 1): 0, el comienzo de la
// primavera; greenness y autumn del clima, src/sim/climate.h): crece en primavera, llega a lo mas
// alto a comienzos del verano, se seca de pie y el invierno la aplasta (los juncos secos quedan de
// pie). Las flores, solo en primavera (si no, 0).
float flora_height(FloraKind kind, float year, float greenness, float autumn);
// Las plumas de la stipa [0, 1]: de fines de la primavera a mediados del verano.
float flora_plume(float year);

// El viento (wind [0, 1] del clima): cuanto se inclina la punta a favor (m por m de alto) y cuanto
// se mece alrededor en el instante t (phase: la de la mata; las olas cruzan el pasto si la fase
// avanza con la posicion a favor del viento).
float flora_lean(float wind);
float flora_sway(float t, float wind, float phase);
// Cuanto se aparta una mata de quien pasa a d m (1 encima, 0 desde FLORA_TRAMPLE_R).
float flora_trample(float d);

#endif
