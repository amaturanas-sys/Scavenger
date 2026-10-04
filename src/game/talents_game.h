// Capacidades del jugador en el juego (src/sim/talents.h, src/sim/jewelry.h):
//  - F junto al druida: tatuar (motivo, zona y confirmacion: es para siempre) o
//    encantar una joya (efecto pasivo o activo segun su piedra);
//  - F junto al orfebre: hacer una joya (tipo, metal y piedra);
//  - F2, F3 y F4: las habilidades activas (de tatuajes y joyas encantadas);
//  - la experiencia (pelear, cazar, fabricar, construir, domar) sube el nivel,
//    que abre los grados del arbol de tatuajes.
#ifndef ESTEPA_TALENTS_GAME_H
#define ESTEPA_TALENTS_GAME_H

#include <stddef.h>

#include "game/actions_game.h"
#include "game/combat_game.h"

void tg_init(GameActions *ga);
bool tg_blocks_input(const GameActions *ga); // un dialogo abierto
// F: hablar con el druida o el orfebre mas cercano (a menos de 3 m). true si habia con quien.
bool tg_try_talk(GameActions *ga, const Troop *troop, const Player *p, char *log, size_t len);
void tg_update(GameActions *ga, Combat *cb, Troop *troop, Props *props, const Player *p, bool input_ok, float dt, char *log,
               size_t len);
// Suma experiencia y avisa si sube de nivel.
void tg_xp(GameActions *ga, XpSource s, char *log, size_t len);
// Tatuajes + joyas + habilidades en curso.
float tg_stat(const GameActions *ga, Stat s);
// Las habilidades activas a mano (de tatuajes y joyas), en el orden de F2, F3, F4.
int tg_actives(const GameActions *ga, AbilityId *out, float *potency, int max);
void tg_draw_hud(const GameActions *ga, int w, int h);
void tg_draw(const GameActions *ga, int w, int h);

#endif
