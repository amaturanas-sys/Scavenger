// Animales del mundo: deambulan, huyen del jugador si son salvajes, se doman
// con el lazo, se ensillan y se montan. C puro (posiciones en el plano XZ),
// el juego pone la altura del terreno y el modelo del inventario.
#ifndef ESTEPA_ANIMALS_H
#define ESTEPA_ANIMALS_H

#include <stdbool.h>

#include "rng.h"

typedef enum {
    SPECIES_HORSE, // caballo estepario
    SPECIES_CAMEL, // camello bactriano
    SPECIES_DEER,  // ciervo
    SPECIES_WOLF,  // lobo
    SPECIES_IBEX,  // ibice
    SPECIES_COUNT
} Species;

typedef struct {
    const char *name;   // UTF-8
    const char *model;  // id del inventario de assets
    float speed;        // m/s al huir
    float flee_dist;    // m: a esta distancia del jugador, un animal salvaje huye
    float tame_chance;  // probabilidad de domarlo con un lazo
    bool rideable;      // se puede ensillar y montar
    float ride_speed;   // multiplica la velocidad del jinete
} SpeciesDef;

typedef enum { ANIMAL_WILD, ANIMAL_TAMED, ANIMAL_SADDLED } AnimalState;

typedef struct {
    Species species;
    AnimalState state;
    float x, z, yaw;
    float home_x, home_z; // centro de su territorio (o el campamento, si esta domado)
    float tx, tz;         // destino actual
    float timer;          // hasta elegir otro destino
    bool fleeing;
    bool ridden;
    float speed;          // m/s en el ultimo paso (para elegir la animacion)
} Animal;

const SpeciesDef *species_def(Species s);
void animal_init(Animal *a, Species s, float x, float z);
// px, pz: posicion del jugador.
void animal_update(Animal *a, float dt, float px, float pz, Rng *rng);
// Lazo: true si lo doma. bonus suma a la probabilidad (p. ej., un jinete habil).
bool animal_try_tame(Animal *a, Rng *rng, float bonus, float camp_x, float camp_z);
// Ensillar: solo animales domados y montables.
bool animal_saddle(Animal *a);
bool animal_can_ride(const Animal *a);

#endif
