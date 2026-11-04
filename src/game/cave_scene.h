// La escena de una cueva: lo que se carga al explorarla y se libera al salir (docs/PLAN_CUEVAS_GRANDES.md). Mientras
// tanto el mundo de afuera no se dibuja: la roca, los tuneles y las salas de la cueva no se cruzan con el terreno.
//
// Al cargar (cs_load) se arman las mallas de la roca (el vestibulo, cada sala y cada pasillo, en trozos para no dibujar
// lo que esta lejos), el decorado (src/sim/caves.h, cave_decor) y los modelos de voxeles que usan (src/world/voxcave.h).
// Cada cuadro, la luz (cs_gather_lights: las llamas, la boca y la antorcha del jugador) se calcula por vertice en lo
// que esta a menos de CS_VIEW del ojo. Lo de cada objeto se ve en src/game/cave_props.c; lo que se juega, en
// src/game/cave_game.c.
#ifndef ESTEPA_CAVE_SCENE_H
#define ESTEPA_CAVE_SCENE_H

#include <stdbool.h>
#include <stdint.h>

#include "raylib.h"
#include "sim/caves.h"

#define CS_VIEW 48.0f // m: lo que se dibuja alrededor del ojo (mas lejos ya esta oscuro)

// Arma la escena de la cueva (su indice en World.caves). false si no hubo memoria: no se entra.
bool cs_load(const Cave *c, int cave);
void cs_unload(void);
bool cs_loaded(void);
int cs_cave(void);        // la cargada, o -1
void cs_forget_gpu(void); // ver terrain_forget_gpu

// La luz de este cuadro: la de la boca (de dia, clara), la que se ve siempre alrededor del jugador, su antorcha y las
// llamas encendidas de la cueva (done: lo hecho en ella, GameActions.cave_done).
void cs_gather_lights(const Cave *c, uint64_t done, Vector3 player, float player_yaw, bool torch_lit, float world_time, float time);
// Un color con la luz de este cuadro en ese punto (WHITE: la luz misma, para teñir un modelo).
Color cs_shade(Color base, Vector3 p);
// Si una antorcha o una fogata de la cueva arde (encendida de siempre o por el jugador).
bool cs_object_lit(const Cave *c, uint64_t done, int obj);

// La roca (salas, vestibulo y pasillos), la boca vista desde adentro y el decorado, con la luz de este cuadro: solo lo que
// esta a menos de CS_VIEW de eye. Entre BeginMode3D y EndMode3D.
void cs_draw(const Cave *c, Vector3 eye, float world_time);
// El decorado de la cueva cargada (lo que tapa el paso: cave_decor_blocked).
const CaveDecor *cs_decor(int *n);

typedef struct {
    int chunks, vertices, triangles; // la roca
    int decor, models, model_tris;   // el decorado y los modelos de voxeles armados
    int drawn_chunks;                // los trozos del ultimo cuadro
    float load_ms;                   // lo que tardo la carga
} CsStats;
CsStats cs_stats(void);

#endif
