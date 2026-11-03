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

#include "game/combat_game.h"
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
// ¿Junto a un pozo (o la fuente de un caravasar)? Su agua viene de hondo y es limpia: se bebe
// y se lleva sin hervir.
bool wg_at_well(const Terrain *t, float x, float z);
// La espuma de quien esta en el agua (docs/AGUA.md, fase 4): el jugador, la tribu, los enemigos y los animales
// que vadean o nadan a menos de 70 m, los mas cercanos primero, para terrain_set_ripples (x, z, radio, cuanta).
int wg_ripples(const GameActions *ga, const Troop *troop, const Combat *cb, const Player *p, const Terrain *t, Vector4 *out, int max);
// Sed, borrachera y enfermedad bajo la salud (alineado a la derecha).
void wg_request_drink(void); // como pulsar N (desde el HUD)
void wg_request_odre(void);  // la casilla del odre: junto al agua lo llena (ACTION_FILL_WATER), si no bebe
void wg_draw_hud(const GameActions *ga, int right_x, int y);

#endif
