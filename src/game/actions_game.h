// Acciones en el juego: conecta el nucleo (sim/actions) con el jugador, el
// mundo y la interfaz. Menu de acciones (Tab), atajos (X empunadura,
// H enfundar, F tomar/soltar, T lanzar), acciones con duracion y obras en
// grupo que la tribu levanta mientras el juego corre.
#ifndef ESTEPA_ACTIONS_GAME_H
#define ESTEPA_ACTIONS_GAME_H

#include <stdbool.h>
#include <stddef.h>

#include "game/player.h"
#include "sim/actions.h"
#include "world/props.h"
#include "world/terrain.h"

#define GA_MAX_PROJECTS 8

typedef struct {
    const Inventory *inv;
    Hands hands;
    int preset;       // empunadura actual del equipo de prueba
    bool torch_lit;
    // Accion en curso (con duracion).
    int doing;        // ActionId, o -1
    float timer;
    // Menu de acciones.
    bool menu_open;
    int cursor;       // 0..ACTION_COUNT-1 acciones; luego BUILD_COUNT obras
    // Obras en grupo.
    BuildProject projects[GA_MAX_PROJECTS];
    int project_count;
} GameActions;

void ga_init(GameActions *ga, const Inventory *inv, Props *props, const Terrain *t);
// true si el menu esta abierto (el jugador no se mueve mientras tanto).
bool ga_menu_open(const GameActions *ga);
void ga_update(GameActions *ga, Props *props, const Terrain *t, const Player *p, const Troop *troop, float dt,
               char *log, size_t log_len);
void ga_draw_world(GameActions *ga, Props *props, const Terrain *t, const Player *p, float time);
// Linea de empunadura para el HUD.
const char *ga_hands_text(const GameActions *ga);
void ga_draw_hud(const GameActions *ga, const Troop *troop, int width, int height);

#endif
