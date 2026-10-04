// Viajes de la tribu fuera de la vista del jugador (C puro):
//  - despachar seguidores a un campamento o a un sitio marcado: al menos uno tiene
//    que conocerlo (haber estado alli); cuantos lo conocen decide el riesgo;
//  - el riesgo crece con la distancia y de noche, y baja con el grupo: si algo sale
//    mal, algunos llegan heridos y otros no llegan;
//  - mensajero: va a un campamento y vuelve con refuerzos (ida y vuelta).
// Para no gastar en simular el camino, el viaje es un temporizador: los que salen se
// pierden en el horizonte y aparecen en el destino al llegar.
#ifndef ESTEPA_TRAVEL_H
#define ESTEPA_TRAVEL_H

#include <stdbool.h>
#include <stdint.h>

#include "rng.h"

#define JOURNEYS_MAX 8
#define JOURNEY_PEOPLE 8

typedef enum { TRAVEL_FOOT, TRAVEL_MOUNTED } TravelMode;
typedef enum { JOURNEY_DISPATCH, JOURNEY_MESSENGER, JOURNEY_REINFORCE } JourneyKind;
typedef enum { FATE_OK, FATE_WOUNDED, FATE_DEAD } Fate;

typedef struct {
    bool used;
    JourneyKind kind;
    int people[JOURNEY_PEOPLE]; // ids de los integrantes
    int n;
    int site;            // destino (id de sitio: campamento 0..7, marca 8+)
    float x0, z0, x1, z1;
    float t, eta;        // s de camino y lo que dura
    TravelMode mode;
    float knowledge;     // [0, 1]: fraccion del grupo que conoce el destino
    int request;         // mensajero: cuantos refuerzos pide
    bool arrived;        // ya llego (el jugador se entera al ir alli)
    int fate[JOURNEY_PEOPLE];
} Journey;

float travel_speed(TravelMode m);                       // m/s
float journey_eta(float dist, TravelMode m);
// Probabilidad de que el viaje sufra un percance [0, 0.9].
float journey_risk(float dist, float knowledge, int group, bool night);
// Tira la suerte de cada uno (fate[i]); devuelve cuantos llegan con vida.
int journey_resolve(Journey *j, Rng *rng, bool night);
int journey_start(Journey *list, int max, JourneyKind kind, const int *people, int n, int site, float x0, float z0, float x1, float z1,
                  TravelMode mode, float knowledge);

// Sitios conocidos: un bit por sitio (campamentos 0..7, marcas del mapa 8..63).
#define SITE_CAMP(k) (k)
#define SITE_MARK(i) (8 + (i))
#define SITES_MAX 64
static inline bool site_known(uint64_t known, int site) { return site >= 0 && site < SITES_MAX && (known >> site) & 1u; }
static inline uint64_t site_learn(uint64_t known, int site) { return site >= 0 && site < SITES_MAX ? known | ((uint64_t)1 << site) : known; }

#endif
