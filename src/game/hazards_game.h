// Peligros del clima en el juego (src/sim/hazards.h): conecta el nucleo con el
// jugador, la tribu, el terreno y la interfaz.
//  - frio: calor corporal del jugador (fuego, refugio, antorcha, viento, ropa
//    mojada); con hipotermia se desmaya y la tribu lo lleva al fuego; las noches
//    heladas gastan lena del acopio;
//  - barro: frena en suelo mojado; vadear y nadar en lagos sin hielo;
//  - lagos helados: se cruzan, pero la capa puede romperse;
//  - socavones ocultos (nieve en glaciares, arena movediza en el desierto) que
//    cambian de lugar cada dia;
//  - escolta (Y): dos integrantes acompanan al jugador y pueden caer tambien;
//  - minijuego: pulsar en orden las teclas que pide el juego (J K L U I O) para
//    salir o para sacar a un companero (F junto a el).
#ifndef ESTEPA_HAZARDS_GAME_H
#define ESTEPA_HAZARDS_GAME_H

#include <stddef.h>

#include "game/actions_game.h"
#include "game/player.h"
#include "sim/climate.h"
#include "sim/hazards.h"
#include "sim/troop.h"
#include "world/props.h"
#include "world/terrain.h"

#define HZ_MAX_HOLES 12
#define HZ_ESCORT 2

typedef enum { TRAP_NONE, TRAP_ICE, TRAP_SNOW, TRAP_QUICKSAND } TrapKind;

typedef struct {
    unsigned seed;
    Rng rng;
    Warmth warmth;
    float faint_timer; // segundos en hipotermia
    float feels;       // sensacion termica actual
    float fire;        // calor del fuego cercano (grados)
    // Terreno bajo los pies.
    bool on_ice, wading, swimming;
    float mud;   // multiplicador por barro
    bool danger; // cerca de un socavon oculto (pista sutil)
    Vector3 danger_pos;
    float danger_radius;
    SinkKind danger_kind;
    // Trampa del jugador y minijuego.
    TrapKind trap;
    Vector3 trap_pos;
    Qte qte;
    bool qte_rescue; // el minijuego es para sacar a un companero
    int helper;      // integrante que ayuda al jugador, o 0
    float sink_depth; // cuanto se hundio (arena movediza)
    // Agujeros abiertos en el hielo (se cierran al dia siguiente).
    Vector3 holes[HZ_MAX_HOLES];
    int hole_count, hole_day;
    // Socavon que ya atrapo a alguien hoy (no vuelve a hacerlo).
    float used_x, used_z;
    int used_day;
    // Companero atrapado.
    int victim; // id del integrante, o 0
    TrapKind victim_trap;
    float victim_timer;
    // Escolta.
    bool escort_on;
    float ice_roll; // acumulador para las tiradas de rotura
} Hazards;

void hz_init(Hazards *hz, unsigned seed);
// El jugador no se mueve (atrapado o en el minijuego).
bool hz_blocks_input(const Hazards *hz);
// Multiplica la velocidad del jugador (barro, frio, vadeo, nado).
float hz_speed_scale(const Hazards *hz);
// Despues de mover al jugador. camp_fire: la fogata del campamento.
void hz_update(Hazards *hz, const Climate *c, const Terrain *t, Player *p, GameActions *ga, const Props *props,
               Troop *troop, Vector3 camp_fire, float world_time, float dt, char *log, size_t log_len);
// Cada amanecer: las noches heladas gastan lena.
void hz_new_day(Hazards *hz, const Climate *c, GameActions *ga, const Props *props, Troop *troop, char *log,
                size_t log_len);
// Agujeros en el hielo, pistas de socavones y companero atrapado. Dentro de BeginMode3D.
void hz_draw_world(const Hazards *hz, const Terrain *t, const GameActions *ga, const Troop *troop, float time);
// Calor corporal (bajo el minimapa, en right_x/y), minijuego y avisos.
void hz_draw_hud(const Hazards *hz, const Troop *troop, int right_x, int y, int width, int height);
// Prueba: arranca en una trampa ("hielo", "nieve", "arena") o con un companero en el hielo ("rescate").
void hz_force(Hazards *hz, const char *what, Player *p, GameActions *ga, Troop *troop, const Terrain *t);

#endif
