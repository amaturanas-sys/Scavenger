// Fuego en el mundo (C puro): focos que arden, se propagan y se apagan.
//  - Cada foco quema un combustible (pasto, arbol, estructura) durante un tiempo y
//    calienta su entorno; si esta caliente, enciende puntos cercanos con combustible,
//    mas facil a favor del viento y con el pasto seco (verano).
//  - La lluvia enfria todos los focos hasta apagarlos.
//  - Lo quemado deja tierra calcinada que no vuelve a arder en unos dias.
// El juego dice que combustible hay en cada punto (fuel_at) y reacciona a los
// eventos (un arbol quemado, una estructura reducida a cenizas).
#ifndef ESTEPA_FIRE_H
#define ESTEPA_FIRE_H

#include <stdbool.h>

#include "rng.h"

#define FIRE_MAX 160
#define SCORCH_MAX 400

typedef enum { FUEL_NONE, FUEL_GRASS, FUEL_TREE, FUEL_STRUCTURE, FUEL_COUNT } FuelKind;

typedef struct {
    bool used;
    float x, z;
    float heat;  // [0, 1]: crece al prender, baja al consumirse o con la lluvia
    float fuel;  // s de combustible que le quedan
    FuelKind kind;
    int ref;     // arbol o estructura que quema (-1 pasto)
    float age;
} FireCell;

typedef struct {
    float x, z, r;
    float age; // s desde que se quemo
} Scorch;

typedef struct {
    FireCell cells[FIRE_MAX];
    Scorch scorch[SCORCH_MAX];
    int scorch_next;
    Rng rng;
    float spread_timer;
} FireField;

typedef struct {
    // Combustible en (x, z): tipo y referencia (arbol o estructura).
    FuelKind (*fuel_at)(void *ud, float x, float z, int *ref);
    void *ud;
    float dryness;         // [0, 1]: 1 pasto seco de verano, 0 mojado o nevado
    float rain;            // [0, 1]: intensidad de la lluvia
    float wind_x, wind_z;  // viento (m/s)
} FireEnv;

typedef enum { FIRE_EV_BURNED_OUT, FIRE_EV_EXTINGUISHED, FIRE_EV_SPREAD } FireEventKind;

typedef struct {
    FireEventKind kind;
    FuelKind fuel;
    int ref;
    float x, z;
} FireEvent;

#define SCORCH_DAYS_SECONDS (3.0f * 1800.0f) // lo calcinado no arde en 3 dias de juego

void fire_init(FireField *f, unsigned seed);
// Prende fuego en (x, z). false si ya arde ahi, esta calcinado o no queda sitio.
bool fire_ignite(FireField *f, float x, float z, FuelKind kind, int ref);
// Igual, ya con calor (un rayo prende de golpe).
bool fire_ignite_hot(FireField *f, float x, float z, FuelKind kind, int ref, float heat);
// Avanza: arde, se propaga, se apaga. Devuelve cuantos eventos escribio en out.
int fire_update(FireField *f, const FireEnv *env, float dt, FireEvent *out, int max);
// Calor en (x, z) dentro del radio r (para quemar a quien pise el fuego) [0, 1+].
float fire_heat_at(const FireField *f, float x, float z, float r);
// ¿Arde ese arbol o esa estructura?
bool fire_burning(const FireField *f, FuelKind kind, int ref);
int fire_count(const FireField *f);
bool fire_scorched(const FireField *f, float x, float z);
// Que tan facil arde [0, 1]: con calor, pasto seco y suelo seco; mojado o nevado, poco o nada.
float fire_dryness(float greenness, float wetness, float snow_cover, float temperature);

// ---------------------------------------------------------------- azar del clima
// Incendio forestal: solo en verano, con el pasto seco y sin lluvia (probabilidad en dt).
bool fire_wildfire_roll(bool summer, float dryness, float rain, float dt, Rng *rng);
// Rayo en una tormenta (probabilidad en dt).
bool fire_lightning_roll(float storm, float dt, Rng *rng);
// Probabilidad de que un rayo prenda lo que toca (la lluvia la baja, lo seco la sube).
float fire_lightning_ignite_chance(float dryness, float rain);

// Mantenimiento de las estructuras: cada dia se desgastan (mas si llovio); la lluvia
// torrencial derrumba las descuidadas (condicion baja).
float structure_daily_wear(bool rained);
float structure_collapse_chance(float condition, float rain, float dt); // probabilidad en dt
#define STRUCTURE_NEGLECTED 0.35f // por debajo, la lluvia torrencial puede derrumbarla
#define RAIN_TORRENTIAL 0.75f

#endif
