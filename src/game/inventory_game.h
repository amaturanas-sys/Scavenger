// Inventario y equipo en el juego (src/sim/storage.h, src/sim/armor.h, src/sim/loadout.h):
//  - lo que se lleva encima: bolsillos (lo pequeño) y mochila; las alforjas de las
//    monturas ensilladas; la carreta de la tribu; el acopio y la armeria del campamento
//    (solo cerca de cada uno);
//  - I: menu de inventario, dos columnas para pasar cosas de un contenedor a otro;
//  - P: menu de equipo: las nueve piezas de armadura y los tres amuletos, con su estado
//    y su proteccion; poner, cambiar y quitar piezas; las heridas al lado;
//  - lo que se gasta en el campo (flechas, hierbas, carne) sale de lo que llevas encima
//    (o de lo que tengas cerca); lo que se consigue (caza, pesca, botin) va a la mochila.
#ifndef ESTEPA_INVENTORY_GAME_H
#define ESTEPA_INVENTORY_GAME_H

#include <stdbool.h>
#include <stddef.h>

#include "game/actions_game.h"
#include "game/combat_game.h"

#define CAMP_STORE_RADIUS 45.0f // m: el acopio y la armeria se alcanzan dentro del campamento

void ig_init(GameActions *ga, Props *props, const Terrain *t);
bool ig_blocks_input(const GameActions *ga); // un menu abierto
// Cuanto hay de algo a mano (encima, en las alforjas o la carreta cercanas, o en el campamento si estas en el).
int ig_count(const GameActions *ga, const Props *props, const Player *p, const char *id);
// Gasta n de lo que haya a mano (primero lo que llevas encima). Devuelve cuantas gasto.
int ig_use(GameActions *ga, const Props *props, const Player *p, const char *id, int n);
// Guarda n donde quepa (mochila, bolsillos, alforjas, carreta, campamento). Devuelve cuantas cupieron.
int ig_store(GameActions *ga, const Props *props, const Player *p, const char *id, int n, float condition);
float ig_carried_kg(const GameActions *ga);
float ig_speed_scale(const GameActions *ga); // la carga frena
// Suma de los amuletos colgados.
float ig_stat(const GameActions *ga, Stat s);
// ---------------------------------------------------------------- reparaciones
typedef struct {
    bool worn;      // puesta (si no, en un contenedor a mano)
    int slot;       // hueco de armadura
    Bag *bag;       // contenedor (si no esta puesta)
    int bag_slot;
    char id[INV_ID_LEN];
    float cond;     // estado [0, 1]
    int material;   // ArmorMaterial
} RepairItem;

// Las piezas gastadas a mano: las que llevas puestas y las de tus contenedores cercanos.
int ig_repair_list(GameActions *ga, const Props *props, const Player *p, RepairItem *out, int max);
// Que falta para reparar (NULL si se puede). Escribe el motivo en why.
bool ig_can_repair(GameActions *ga, const Props *props, const Player *p, const Troop *troop, const RepairItem *it, char *why,
                   size_t len);
bool ig_repair(GameActions *ga, const Props *props, const Player *p, const Troop *troop, const RepairItem *it, char *log,
               size_t len);
// Ingredientes: ¿hay todo a mano (encima, cerca o en el campamento)? Devuelve el primero que falta.
const Ingredient *ig_first_missing(GameActions *ga, const Props *props, const Player *p, const Ingredient *mats);
void ig_use_all(GameActions *ga, const Props *props, const Player *p, const Ingredient *mats);

// ---------------------------------------------------------------- botin
// Deja una bolsa de botin en pos con esos objetos. false si no habia hueco.
bool ig_drop_loot(GameActions *ga, Vector3 pos, const LootItem *items, int n);
// Recoge la bolsa de botin mas cercana (a menos de 2.5 m): lo que no cabe se queda en ella.
bool ig_take_loot(GameActions *ga, const Props *props, const Player *p, char *log, size_t len);
void ig_draw_world(const GameActions *ga, const Terrain *t, float time);

void ig_update(GameActions *ga, Combat *cb, Props *props, const Player *p, bool input_ok, char *log, size_t len);
void ig_draw(const GameActions *ga, const Combat *cb, const Props *props, const Player *p, int w, int h);

#endif
