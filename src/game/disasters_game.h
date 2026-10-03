// Clima con azar en el juego: fuego, rayos y lluvia torrencial (src/sim/fire.h).
//  - incendios forestales en verano con el pasto seco; el fuego se propaga por el
//    pasto, los arboles y las estructuras, mas rapido a favor del viento, y deja
//    tierra calcinada; quema a quien lo pisa;
//  - rayos en las tormentas: caen sobre lo mas alto (arboles, estructuras), las
//    queman o las prenden y hieren a quien este cerca;
//  - la lluvia torrencial derrumba las estructuras sin mantenimiento (la tribu
//    las repara cada dia con troncos; los constructores, mas);
//  - la lluvia apaga todo fuego: incendios, antorchas, fogatas y flechas encendidas.
#ifndef ESTEPA_DISASTERS_GAME_H
#define ESTEPA_DISASTERS_GAME_H

#include <stddef.h>

#include "game/actions_game.h"
#include "game/combat_game.h"
#include "sim/climate.h"
#include "sim/fire.h"
#include "world/camp.h"

typedef struct {
    FireField fire;
    Rng rng;
    float tree_burn[WORLD_MAX_TREES]; // 0 sano, 0.5 chamuscado, 1 calcinado
    float bolt_time;                  // s que le quedan al rayo dibujado
    Vector3 bolt_to;
    float bolt_seed;
    float relight_timer;
    float burn_tick, warn_timer;
    bool camp_warned;
} Disasters;

void dz_init(Disasters *dz, unsigned seed);
void dz_update(Disasters *dz, const Climate *c, Camp *camp, GameActions *ga, Props *props, Troop *troop, Combat *cb,
               Player *p, const Terrain *t, float now, float dt, char *log, size_t len);
// Cada dia: las estructuras se desgastan y la tribu repara las peores.
void dz_new_day(Disasters *dz, Props *props, GameActions *ga, const Troop *troop, bool rained, char *log, size_t len);
// Prende fuego en (x, z) si hay algo que arda (flechas encendidas). true si prendio.
bool dz_ignite_at(Disasters *dz, const Camp *camp, const Props *props, const Terrain *t, const Climate *c, float x, float z);
void dz_draw_world(const Disasters *dz, const Terrain *t, float time);

#endif
