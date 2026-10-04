// El agua en el juego (src/sim/water.h):
//  - N bebe lo mas seguro que tengas a mano (hervida, agua con vino, airag, cerveza, leche,
//    vino; la cruda al final); en la orilla, sin nada, bebe del rio (con riesgo);
//  - Mayus+N toma hierbas contra los espiritus malditos;
//  - el calor da mas sed; enfermo, fiebre; borracho, torpe, fatigado y se tambalea;
//  - con la sed a cero se desmaya y la tribu lo lleva al campamento;
//  - cada dia la tribu bebe del acopio o del rio (hervida si hay leña).
#ifndef ESTEPA_WATER_GAME_H
#define ESTEPA_WATER_GAME_H

#include <stddef.h>

#include "game/hazards_game.h"
#include "sim/water.h"

// Cuanta agua cruda cabe todavia en los odres que llevas (3 por odre, contando la hervida).
int wg_water_room(GameActions *ga, const Props *props, const Player *p);
void wg_update(GameActions *ga, Troop *troop, Player *p, const Props *props, const Hazards *hz, const Climate *c, Vector3 camp_fire,
               const Terrain *t, bool input_ok, float dt, char *log, size_t len);
// Efectos sobre el jugador: velocidad (sed, fiebre, borrachera) y torpeza [0, 1] (golpes y punteria).
float wg_speed_scale(const GameActions *ga);
float wg_clumsy(const GameActions *ga);
float wg_stamina(const GameActions *ga);
// ¿Hay agua (lago o rio) a menos de 60 m de (x, z)?
bool wg_water_near(const Terrain *t, float x, float z);
// Sed, borrachera y enfermedad bajo la salud (alineado a la derecha).
void wg_draw_hud(const GameActions *ga, int right_x, int y);

#endif
