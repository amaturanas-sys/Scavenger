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
#include "sim/apparel.h"
#include "sim/water.h"
#include "sim/economy.h"
#include "sim/memory_map.h"
#include "game/dialog.h"
#include "sim/camps.h"
#include "sim/jewelry.h"
#include "sim/loadout.h"
#include "sim/talents.h"
#include "sim/travel.h"
#include "sim/storage.h"
#include "sim/swarms.h"
#include "world/props.h"
#include "world/terrain.h"

#define GA_MAX_PROJECTS 8
#define GA_MAX_ANIMALS 64
#define GA_MAX_SWARMS 24
#define GA_PACKS 4 // monturas con alforjas
#define GA_LOOT 8  // bolsas de botin en el suelo

enum { PACKW_WORN, PACKW_GROUND, PACKW_CART, PACKW_ANIMAL };

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
    float stagger, knock; // desequilibrado; derribado (src/sim/melee.h)
    int move, combo;   // movimiento cuerpo a cuerpo en curso (MeleeMove + 1) y paso del combo
    float move_anim;
    int home_camp;     // campamento de su casa + 1 (0: sin calcular)
    float leave_t;     // despachado: s que lleva alejandose hacia el horizonte (luego desaparece)
    Vector3 leave_dir;
    signed char comfort; // con su ropa: -1 pasa frio, 1 pasa calor, 0 a gusto (src/game/apparel_game.c)
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
    // Campamentos (src/sim/camps.h): cada uno con su acopio y su guardian. El 0 es el
    // campamento con el que empieza la tribu. ga_stock() da el acopio de donde esta el jugador.
    CampSite camps[CAMPS_MAX];
    int here;           // campamento donde esta el jugador, o -1
    Vector3 player_pos; // donde esta el jugador (lo pone ga_update)
    bool found_pending; // se levanto una estructura basica lejos de todo: fundar campamento
    float found_x, found_z;
    int burn_camp, burn_next; // disolviendo: campamento que arde y la proxima estructura a prender (-1: nada)
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
    // Fuego y lluvia (los pone src/game/disasters_game.c).
    bool fires_out;     // la lluvia apago las fogatas y los hornos
    bool raining;       // llueve: no se enciende nada
    bool grass;         // hay pasto (sin nieve): los herbivoros de la tribu pastan solos (lo pone main)
    bool fire_near;     // hay un fuego al lado del jugador (para encender una flecha)
    Vector3 ignite_at[8]; // donde cayeron flechas encendidas este paso
    int ignite_n;
    // Lo que se lleva (src/game/inventory_game.c): bolsillos, mochila, alforjas, carreta,
    // y la armeria del campamento (el equipo que se deja alli; lo demas va al acopio).
    Bag pockets, backpack, cart, armory;
    Bag packs[GA_PACKS];
    int pack_animal[GA_PACKS]; // animal que lleva cada alforja, o -1
    // Capacidades del jugador (src/game/talents_game.c): nivel, tatuajes (para siempre),
    // joyas puestas y habilidades activas (en curso y en espera).
    Progress prog;
    TattooBody tattoos;
    Jewelry jewels;
    Outfit outfit;   // la ropa (src/sim/apparel.h): abriga, da sombra, escarmienta
    int wear_cursor; // hueco elegido en la pestaña de ropa
    float dress_t;   // reloj de los NPCs que se cambian de ropa (src/game/apparel_game.c)
    Hydration hydro; // sed, borrachera y espiritus malditos del agua (src/game/water_game.c)
    float dry_t;     // segundos con la sed a cero
    float ab_timer[ABIL_COUNT], ab_cd[ABIL_COUNT], ab_pot[ABIL_COUNT];
    float warmth_boost; // calor que devuelve una habilidad (lo consume src/game/hazards_game.c)
    // Dialogo con el druida o el orfebre (y luego el guardian del campamento).
    Dialog dlg;
    int talk_mode, talk_member, talk_a, talk_b, talk_c;
    int equip_tab, jewel_cursor, tattoo_cursor; // menu de equipo: armadura, joyas, tatuajes
    // Viajes fuera de la vista (src/game/travel_game.c): despachos, mensajeros y refuerzos.
    Journey journeys[JOURNEYS_MAX];
    int travel_pick[JOURNEY_PEOPLE], travel_npick; // los elegidos para despachar
    // Menus de inventario y de equipo.
    bool inv_open, equip_open;
    int inv_pane, inv_cont[2], inv_cursor[2], equip_cursor;
    // Menu de acciones (Tab) por pestañas: acciones, obras, fabricar, reparar.
    int menu_tab, tab_cursor[4];
    bool take_all; // F con Mayus: tomar todo lo de alrededor
    // Botin que dejan los enemigos al caer (F para recogerlo).
    Bag loot[GA_LOOT];
    Vector3 loot_pos[GA_LOOT];
    float loot_age[GA_LOOT]; // s en el suelo; < 0: hueco libre
    Armor *player_armor; // la armadura que lleva el jugador (la pone main cada cuadro; no se guarda)
    const Terrain *terrain; // el terreno (lo pone ga_update cada cuadro; no se guarda)
    Troop *troop_ref;       // la tribu (lo pone ga_update cada cuadro; no se guarda)
    // La mochila del jugador: su tamaño y donde esta (puesta, en el suelo, en la carreta o en un animal).
    int pack_size, pack_where, pack_anchor; // PackSize; PACKW_*; animal que la lleva
    Vector3 pack_pos;
    // Trepar.
    bool climbing;
    float climb_t;
    Vector3 climb_from, climb_top, climb_over, climb_to;
    bool hidden;
    // Estado de combate y salud del jugador para la animacion (lo pone src/game/combat_game.c).
    bool pl_down, pl_hit, pl_blocking, pl_limping, pl_spear, pl_knocked;
    int pl_attacking, pl_ranged, pl_move;
    float swap_anim;    // pasando el arma de mano
} GameActions;

void ga_init(GameActions *ga, const Inventory *inv, Props *props, const Terrain *t, unsigned seed);
// El acopio del campamento donde esta el jugador (o del mas cercano; uno vacio si no queda ninguno).
Stockpile *ga_stock(GameActions *ga);
const Stockpile *ga_stock_c(const GameActions *ga);
Stockpile *ga_stock_at(GameActions *ga, float x, float z);
// El guardian ordena una obra en su campamento (con su acopio), en un sitio libre.
bool ga_order_build(GameActions *ga, BuildId b, int camp, const Props *props, char *log, size_t len);
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
void ga_new_day(GameActions *ga, Props *props, const Terrain *t, Troop *troop, MemoryMap *mem, float now, int day, float temp_mean,
                char *log, size_t log_len);
void ga_draw_world(GameActions *ga, Props *props, const Terrain *t, const Troop *troop, const Player *p, float time);
// Dibuja al jugador con el modelo del protagonista y su animacion, si ya fue
// importado. Devuelve false si no (el juego dibuja el marcador de siempre).
bool ga_draw_player(GameActions *ga, Props *props, const Player *p, float time);
const char *ga_hands_text(const GameActions *ga);
void ga_draw_hud(const GameActions *ga, const Props *props, const Troop *troop, const Player *p, int width, int height);

// Fuentes de luz para la noche (fogatas, hogueras, hornos y la antorcha encendida).
// Llena pos y radius (metros de alcance); devuelve cuantas hay.
int ga_lights(const GameActions *ga, const Props *props, const Player *p, Vector3 *pos, float *radius, int max);

#endif
