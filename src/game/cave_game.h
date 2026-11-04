// El interior de las cuevas (docs/PLAN_TACTICO_Y_CUEVAS.md, B1 a B3, y docs/PLAN_CUEVAS_GRANDES.md; la planta y las
// reglas, en src/sim/caves.h).
//
// Junto a la boca, F explora: la cueva se carga aparte (src/game/cave_scene.c arma su roca, su decorado y sus modelos;
// src/game/cave_props.c dibuja lo de adentro) y se entra; se sale caminando por la boca, y la escena se libera. El
// interior es un espacio aparte, en las coordenadas de la cueva, con su camara, su oscuridad y su luz (las antorchas y
// la fogata encendidas, la boca de dia y la antorcha del jugador). Mientras tanto el mundo de afuera se detiene (el
// combate, la fauna, los peligros, los desastres) y no se dibuja; el reloj, las necesidades, el inventario, la barra
// rapida y la antorcha siguen. Para el mundo, el jugador queda en la boca: alli se guarda la partida y alli aparece al
// cargarla.
//
// Adentro, F: tomar lo que se lleva (cofres, anforas, armas, botin, vetas, huesos, nidos), mirar las pinturas,
// encender las antorchas apagadas con la propia y liberar a los prisioneros (se unen a la tribu o te traicionan).
// Lo hecho queda en GameActions.cave_done, que se guarda. Las cuevas habitadas tienen guardias: salen a la boca una
// vez por dia, al acercarte, y mientras sigan en pie junto a la boca no te dejan entrar.
#ifndef ESTEPA_CAVE_GAME_H
#define ESTEPA_CAVE_GAME_H

#include <stdbool.h>
#include <stddef.h>

#include "game/actions_game.h"
#include "game/combat_game.h"
#include "game/player.h"
#include "raylib.h"
#include "sim/troop.h"
#include "world/props.h"
#include "world/terrain.h"

bool cv_inside(void);
int cv_cave(void); // la cueva en la que esta (indice de World.caves), o -1
// Explorar: entre que se pulso F en la boca y que la escena quedo armada (un cuadro con «Explorando...»).
bool cv_loading(void);
// La pantalla de la carga (en lugar del mundo, mientras cv_loading).
void cv_draw_loading(const Terrain *t, int w, int h);
// Si en este cuadro F fue para explorar (src/game/actions_game.c no la usa para otra cosa).
bool cv_claimed_interact(void);

// Al cargar una partida o empezar otra: afuera, con la luz de siempre.
void cv_reset(void);
// Cada cuadro, despues de player_update (afuera) o en su lugar (adentro). Afuera: los guardias de las cuevas
// habitadas cercanas y, en la boca, F para explorar. Adentro: moverse, F, el dialogo con un prisionero y salir.
// cam_yaw: el de la camara (al entrar mira adentro; al salir, afuera).
void cv_update(GameActions *ga, Combat *cb, Troop *troop, const Props *props, Player *p, const Terrain *t, PlayerInput in,
               float *cam_yaw, float world_time, bool input_ok, float dt, char *log, size_t len);
// La camara adentro (yaw, pitch y dist: los de la camara de siempre), sin salirse de la roca.
void cv_camera(const Terrain *t, Camera3D *cam, float yaw, float pitch, float dist);
// Lo de adentro, entre BeginMode3D y EndMode3D: la roca, lo que hay, los prisioneros y el jugador, con su luz.
void cv_draw(GameActions *ga, const Combat *cb, Props *props, const Terrain *t, float world_time, float time);
// El plano de la cueva (en lugar del minimapa: centro cx, cy y radio r) y lo que hace F.
void cv_draw_plan(const Terrain *t, int cx, int cy, int r);
void cv_draw_hud(const GameActions *ga, int w, int h);
// Pruebas (--cueva tipo[:lugar]): entra en la cueva de ese tipo mas cercana al campamento (animal, abandonada, forajidos,
// tribu o imperio) y se para en el lugar: una sala (su numero), un pasillo (pasilloN: a su mitad, mirando a lo largo) o
// ante un objeto (celda, dialogo: y le habla, nido, huevos, presa, botin, arma, cofre, anfora, deposito, pintura, fogata,
// antorcha, veta, columna, estalactita, estalagmita). false si no hay de ese tipo.
bool cv_test_enter(GameActions *ga, Player *p, const Terrain *t, const char *spec, float *cam_yaw);
// Pruebas (--explorar): se para en la boca de la cueva mas cercana (a menos de 30 m) y pulsa F; false si no hay.
bool cv_test_explore(Player *p, const Terrain *t);
// Pruebas (--cueva-f): como pulsar F (o, con el dialogo de un prisionero abierto, elegir romper las cadenas).
void cv_test_press(GameActions *ga, Combat *cb, Troop *troop, const Props *props, Player *p, const Terrain *t, char *log, size_t len);

#endif
