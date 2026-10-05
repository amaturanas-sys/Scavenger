// Peligros del clima y del terreno (C puro, sin raylib):
//  - frio: calor corporal que baja con el frio, el viento y la ropa mojada, y
//    sube junto al fuego;
//  - barro: la lluvia y el deshielo frenan la marcha en suelo blando;
//  - hielo: los lagos congelados se cruzan, pero la capa puede romperse;
//  - socavones ocultos (nieve en los glaciares, arena movediza en el desierto)
//    que cambian de lugar cada dia;
//  - minijuego de rescate: pulsar a tiempo una serie de teclas que elige el juego.
#ifndef ESTEPA_HAZARDS_H
#define ESTEPA_HAZARDS_H

#include <stdbool.h>
#include <stdint.h>

#include "rng.h"

// ------------------------------------------------------------------ biomas
// Desierto: el sector del desierto del mundo (src/sim/world.h), en [0, 1] (1 = arena plena).
float biome_desert(uint32_t seed, float x, float z);

// --------------------------------------------------------------------- frio
#define WARMTH_MAX 100.0f
#define WARMTH_COMFORT 8.0f // sensacion termica (grados) a partir de la cual se recupera calor

typedef struct {
    float heat; // calor corporal [0, 100]: 0 es hipotermia
    float wet;  // ropa mojada [0, 1]
} Warmth;

typedef enum { COLD_WARM, COLD_COOL, COLD_COLD, COLD_FREEZING, COLD_HYPOTHERMIA } ColdLevel;

// Sensacion termica: temperatura + abrigo + fuego cercano - viento - ropa mojada.
float hazard_feels_like(float temperature, float wind, float wet, float clothing, float fire);
void warmth_update(Warmth *w, float feels_like, float rain, bool near_fire, float dt);
ColdLevel warmth_level(const Warmth *w);
const char *cold_name(ColdLevel c); // UTF-8
// El frio entumece: multiplica la velocidad (1 abrigado, 0.6 en hipotermia).
float warmth_speed_scale(const Warmth *w);

// -------------------------------------------------------------------- barro
// Multiplicador de velocidad en suelo blando (no en roca, arena ni nieve).
float hazard_mud_scale(float wetness, float snow_cover);

// --------------------------------------------------------------------- hielo
// El hielo aguanta el peso desde este grado de congelamiento (climate.ice).
#define ICE_WALKABLE 0.5f
bool hazard_ice_walkable(float ice);
// Probabilidad por segundo de que la capa se rompa bajo alguien.
// speed: m/s; load: 1 a pie, ~2.5 a caballo.
float hazard_ice_break_chance(float ice, float speed, float load);

// --------------------------------------------------------------- socavones
typedef enum { SINK_NONE, SINK_SNOW, SINK_QUICKSAND } SinkKind;

#define SINK_CELL 40.0f    // metros por celda de la grilla de socavones
#define SINK_CHANCE 0.30f  // probabilidad de que una celda tenga uno (si el bioma lo permite)

typedef struct {
    float x, z, radius;
} Sinkhole;

// Socavon candidato de una celda para un dia (cambia cada dia). El juego decide
// si existe segun el bioma del lugar (glaciar o desierto). false si la celda no tiene.
bool hazard_sinkhole_cell(uint32_t seed, int day, int cx, int cz, Sinkhole *out);
const char *sink_name(SinkKind k); // UTF-8

// ------------------------------------------------------------- minijuego
#define QTE_KEYS 6   // teclas posibles (el juego las asigna: J K L U I O)
#define QTE_MAX 12

typedef enum { QTE_IDLE, QTE_RUNNING, QTE_WON, QTE_LOST } QteState;

typedef struct {
    int keys[QTE_MAX];
    int len, pos;
    float per_key;   // segundos para cada tecla
    float time_left; // de la tecla actual
    int mistakes, max_mistakes;
    QteState state;
} Qte;

// len teclas al azar; per_key segundos por tecla (se acorta un poco a cada acierto).
void qte_start(Qte *q, Rng *rng, int len, float per_key, int max_mistakes);
// Avanza el reloj: si se acaba el tiempo de una tecla cuenta como error.
void qte_update(Qte *q, float dt);
// Tecla pulsada (0..QTE_KEYS-1). Devuelve true si era la correcta.
bool qte_press(Qte *q, int key);
float qte_progress(const Qte *q); // [0, 1]

#endif
