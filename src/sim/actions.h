// Acciones del jugador y de los NPCs.
//
// Tres piezas, todas en C puro (sin raylib) para poder testearlas:
//  1. Manos: que se empuna y como (una en cada mano, a dos manos, arma y
//     escudo...), enfundar y desenfundar, tomar y lanzar objetos.
//  2. Acciones individuales: las puede hacer una sola persona (instalar una
//     fogata o una tienda, cavar una trinchera, lanzar la trepa o el lazo,
//     encender una antorcha, ensillar una montura...).
//  3. Construcciones en grupo: necesitan varias personas de la tribu. El
//     ritmo de la obra depende de cuantos trabajan y de lo habiles que son:
//     las funciones adecuadas (constructor, herrero, cocinero...), los dones
//     de los grandes guerreros y la moral cuentan.
//
// Los objetos se nombran con los ids del inventario de assets
// (assets/inventario.tsv), asi cada accion sabe que modelo usar.
#ifndef ESTEPA_ACTIONS_H
#define ESTEPA_ACTIONS_H

#include <stdbool.h>

#include "economy.h"
#include "inventory.h"
#include "troop.h"

// ---------------------------------------------------------------- manos
typedef enum { HAND_RIGHT, HAND_LEFT } HandSlot;

typedef struct {
    char id[INV_ID_LEN]; // "" = mano vacia
    InvHands kind;
} Held;

typedef struct {
    Held right, left;
    bool sheathed;              // armas enfundadas: las manos quedan libres
    char carried[INV_ID_LEN];   // objeto tomado del suelo ("" = nada)
} Hands;

typedef enum {
    GRIP_EMPTY,         // desarmado
    GRIP_ONE_HANDED,    // un arma en una mano
    GRIP_DUAL,          // una en cada mano (p. ej. sable y daga, o arma y antorcha)
    GRIP_TWO_HANDED,    // un arma a dos manos
    GRIP_WEAPON_SHIELD, // arma y escudo
    GRIP_SHIELD,        // solo escudo
} Grip;

void hands_init(Hands *h);
// Empuna un objeto del inventario. Respeta las reglas: lo de dos manos ocupa
// ambas, el escudo va en la izquierda. Devuelve false si no se puede empunar.
bool hands_equip(Hands *h, const InvItem *item, HandSlot slot);
void hands_clear(Hands *h, HandSlot slot);
Grip hands_grip(const Hands *h);
const char *grip_name(Grip g);
// Enfundar/desenfundar. Devuelve false si no hay nada que enfundar.
bool hands_toggle_sheathe(Hands *h);
// Para tomar un objeto hace falta una mano libre (o las armas enfundadas).
bool hands_can_take(const Hands *h);
bool hands_take(Hands *h, const char *id);
// Suelta el objeto llevado (para lanzarlo). Copia su id en out. false si no lleva nada.
bool hands_throw(Hands *h, char *out, int out_len);
// true si el objeto esta empunado (y desenfundado) o se lleva en la mano.
bool hands_holding(const Hands *h, const char *id);

// ---------------------------------------------------------------- acciones individuales
enum { ACTOR_PLAYER = 1 << 0, ACTOR_NPC = 1 << 1 };

typedef enum {
    ACTION_TAKE,          // tomar un objeto
    ACTION_THROW,         // lanzar el objeto que se lleva
    ACTION_CHANGE_GRIP,   // cambiar de empunadura
    ACTION_SHEATHE,       // enfundar o desenfundar
    ACTION_PLACE_FIRE,    // instalar una fogata
    ACTION_PLACE_TENT,    // instalar una tienda de campana
    ACTION_DIG_TRENCH,    // cavar una trinchera
    ACTION_THROW_GRAPPLE, // lanzar la trepa (gancho con cuerda) a un muro
    ACTION_THROW_LASSO,   // lanzar el lazo a un animal salvaje
    ACTION_LIGHT_TORCH,   // encender una antorcha
    ACTION_SADDLE,        // instalar una montura (ensillar)
    ACTION_COUNT
} ActionId;

typedef struct {
    const char *name;     // UTF-8
    const char *desc;     // UTF-8
    unsigned actors;      // ACTOR_PLAYER | ACTOR_NPC
    float seconds;        // duracion (segundos de juego)
    const char *requires; // id de inventario que hay que tener a mano, o NULL
    const char *produces; // id de inventario que queda en el mundo, o NULL
    const char *target;   // a que se aplica ("muro", "animal salvaje"...), o NULL
    Ingredient mats[MAT_MAX]; // lo que consume del acopio
} ActionDef;

const ActionDef *action_def(ActionId a);

// ---------------------------------------------------------------- construcciones en grupo
typedef enum {
    BUILD_SHELTER,        // refugio
    BUILD_PALISADE,       // empalizada (muro de madera)
    BUILD_STONE_WALL,     // muro de piedra
    BUILD_BONFIRE,        // hoguera grande
    BUILD_TOTEM,          // totem de proteccion
    BUILD_OVEN,           // horno de cocina
    BUILD_FURNACE_BRONZE, // horno de fundicion de bronce
    BUILD_FURNACE_STEEL,  // horno de fundicion de acero
    BUILD_WATCHTOWER,     // torre de vigilancia
    BUILD_CORRAL,         // corral
    BUILD_COUNT
} BuildId;

typedef struct {
    const char *name;      // UTF-8
    const char *produces;  // id de inventario
    float work;            // trabajo total, en segundos-persona de juego
    int min_workers;       // por debajo, la obra no avanza
    int max_workers;       // mas gente no acelera (se estorban)
    Role required_role;    // sin alguien con esta funcion no se puede (ROLE_NONE: nadie en especial)
    Role skilled_role;     // quien tiene esta funcion trabaja el doble
    Ingredient mats[MAT_MAX]; // lo que consume del acopio al empezar
} BuildDef;

const BuildDef *build_def(BuildId b);

typedef enum { BUILD_READY, BUILD_FEW_WORKERS, BUILD_MISSING_ROLE } BuildCheck;

typedef struct {
    BuildCheck check;
    int workers; // cuantos trabajarian (incluido el jugador si ayuda)
    float rate;  // trabajo por segundo si todos estan en la obra; 0 si no se puede
    // La cuadrilla elegida, de mas a menos habil: id del integrante (-1 = el jugador) y su aporte.
    int ids[TROOP_MAX + 1];
    float skills[TROOP_MAX + 1];
} CrewPlan;

// Arma la cuadrilla con los integrantes activos mas habiles. player_helps:
// el jugador trabaja tambien (cuenta como un trabajador comun).
CrewPlan build_plan(const BuildDef *def, const Troop *t, bool player_helps);
// Igual, pero sin contar a los integrantes ocupados en otra obra (ids en busy).
CrewPlan build_plan_excluding(const BuildDef *def, const Troop *t, bool player_helps, const int *busy, int n_busy);
// Ritmo real con la parte de la cuadrilla que ya llego a la obra (present[i] para plan->ids[i]).
// Si los presentes no alcanzan el minimo, la obra espera (0).
float build_rate_present(const BuildDef *def, const CrewPlan *plan, const bool *present);
// Aporte de un integrante a una obra (1 = trabajador comun).
float build_worker_skill(const BuildDef *def, const Troop *t, const Member *m);
const char *build_check_text(const BuildDef *def, BuildCheck c);

typedef struct {
    BuildId def;
    float x, z;     // donde se levanta
    float progress; // [0, 1]
    bool done;
} BuildProject;

// Avanza la obra. Devuelve true en el paso en que se completa.
bool build_advance(BuildProject *p, float rate, float dt);

#endif
