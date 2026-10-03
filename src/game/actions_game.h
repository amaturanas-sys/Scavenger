// Acciones y vida del campamento en el juego. Conecta el nucleo (sim/actions,
// sim/economy, sim/animals) con el jugador, el mundo y la interfaz:
//  - menu de acciones (Tab), atajos (X empunadura, H enfundar, F tomar,
//    T lanzar, R montar, I acopio) y acciones con duracion;
//  - acopio de la tribu: materiales, comida, recoleccion diaria;
//  - obras en grupo: los NPCs elegidos caminan a la obra y trabajan alli;
//  - NPCs que hacen acciones individuales (instalar una fogata o una tienda);
//  - animales: se doman con el lazo, se ensillan y se montan (la fauna, en
//    src/game/fauna_game.c);
//  - trepar muros con la trepa, esconderse en la hierba alta;
//  - forja en los hornos y efectos diarios de las construcciones.
#ifndef ESTEPA_ACTIONS_GAME_H
#define ESTEPA_ACTIONS_GAME_H

#include <stdbool.h>
#include <stddef.h>

#include "game/player.h"
#include "sim/actions.h"
#include "sim/animals.h"
#include "sim/economy.h"
#include "sim/memory_map.h"
#include "sim/swarms.h"
#include "world/props.h"
#include "world/terrain.h"

#define GA_MAX_PROJECTS 8
#define GA_MAX_ANIMALS 64
#define GA_MAX_SWARMS 24

typedef struct {
    int member_id;     // integrante de la tropa (0 = libre)
    Vector3 pos, home;
    float yaw;
    int project;       // obra a la que va, o -1
    int job;           // accion individual (ActionId) que esta haciendo, o -1
    bool moving;
    Vector3 job_pos;
    float job_timer;
    bool escort;       // acompana al jugador (src/game/hazards_game.c lo mueve)
    bool fighting;     // peleando (src/game/combat_game.c lo mueve)
    float fight_anim;  // segundos que quedan del golpe en curso
    float hurt_anim;   // segundos que quedan de la reaccion a un golpe
} Npc;

typedef struct {
    const Inventory *inv;
    Rng rng;
    // Manos y equipo.
    Hands hands;
    int preset;
    bool torch_lit;
    // Accion en curso.
    int doing;          // ActionId, o -1
    float timer;
    int target;         // animal o muro objetivo, o -1
    // Interfaz.
    bool menu_open, stock_open;
    int cursor;
    // Campamento.
    Stockpile stock;
    BuildProject projects[GA_MAX_PROJECTS];
    int project_count;
    int crew_present[GA_MAX_PROJECTS], crew_size[GA_MAX_PROJECTS];
    int crafting;       // CraftId en la forja, o -1
    float craft_timer, craft_total;
    Npc npcs[TROOP_MAX];
    // Animales.
    Animal animals[GA_MAX_ANIMALS]; // huecos libres: used = false
    int animal_count;   // hasta donde hay animales en el arreglo
    int mounted;        // animal montado, o -1
    int next_group;     // id del proximo grupo (manada, rebaño)
    float fauna_timer, reveal_timer;
    Swarm swarms[GA_MAX_SWARMS]; // peces, abejas, avispas, mosquitos, moscas
    float swarm_timer, sting_timer;
    // Trepar.
    bool climbing;
    float climb_t;
    Vector3 climb_from, climb_top, climb_over, climb_to;
    bool hidden;
    // Estado de combate y salud del jugador para la animacion (lo pone src/game/combat_game.c).
    bool pl_down, pl_hit, pl_blocking, pl_limping, pl_spear;
    int pl_attacking, pl_ranged;
} GameActions;

void ga_init(GameActions *ga, const Inventory *inv, Props *props, const Terrain *t, unsigned seed);
// El jugador no controla el movimiento (menu abierto o trepando).
bool ga_blocks_input(const GameActions *ga);
bool ga_menu_open(const GameActions *ga);
// Multiplicador de velocidad del jugador (montado: el de la montura).
float ga_speed_scale(const GameActions *ga);
void ga_update(GameActions *ga, Props *props, const Terrain *t, Player *p, Troop *troop, float dt, char *log,
               size_t log_len);
// Despues de mover al jugador: trepar, montura, escondite.
void ga_after_player(GameActions *ga, Props *props, const Terrain *t, Player *p);
// Un dia nuevo: comida y recoleccion, efectos de las construcciones, trabajos de los NPCs.
void ga_new_day(GameActions *ga, Props *props, const Terrain *t, Troop *troop, MemoryMap *mem, float now, int day,
                char *log, size_t log_len);
void ga_draw_world(GameActions *ga, Props *props, const Terrain *t, const Troop *troop, const Player *p, float time);
// Dibuja al jugador con el modelo del protagonista y su animacion, si ya fue
// importado. Devuelve false si no (el juego dibuja el marcador de siempre).
bool ga_draw_player(GameActions *ga, Props *props, const Player *p, float time);
const char *ga_hands_text(const GameActions *ga);
void ga_draw_hud(const GameActions *ga, const Props *props, const Troop *troop, int width, int height);

// Fuentes de luz para la noche (fogatas, hogueras, hornos y la antorcha encendida).
// Llena pos y radius (metros de alcance); devuelve cuantas hay.
int ga_lights(const GameActions *ga, const Props *props, const Player *p, Vector3 *pos, float *radius, int max);

#endif
