// La vista aerea para ubicar una obra (fase 4b; las reglas, en src/sim/placement.h):
//  - al elegir una obra (Tab -> Obras, o el guardian del campamento), la camara sube sobre el
//    campamento, unos 60 m de radio a la vista (fuera de todo campamento, sobre el jugador);
//  - la huella de la obra sigue al puntero (o al dedo, o a WASD y las flechas): verde si el
//    sitio vale; roja si no, con el motivo. Q/E la giran; Mayus la alinea a la rejilla;
//  - Enter, un clic sobre la huella o la casilla de aceptar la colocan (y se paga el acopio);
//    Esc cancela. Las cercas y los muros se trazan con dos puntos, de un extremo al otro.
// Los constructores caminan despues hasta la obra, como siempre.
#ifndef ESTEPA_PLACE_GAME_H
#define ESTEPA_PLACE_GAME_H

#include <stddef.h>

#include "game/actions_game.h"
#include "raylib.h"

// Empieza a ubicar la obra b. camp: el campamento donde va (su acopio y su radio), o -1 para
// ubicarla alrededor de (x, z) (fuera de todo campamento: fundando uno, con el acopio cercano).
void pg_begin(GameActions *ga, BuildId b, int camp, float x, float z);
bool pg_active(void);
void pg_cancel(void);
// Lo que no se puede pisar y no es un objeto del mundo: las yurtas y la fogata del campamento
// inicial y los arboles. Los punteros deben seguir validos (los del campamento).
void pg_set_world(const Vector3 *yurts, int nyurts, Vector3 fire, bool fire_used, const Vector3 *trees, int ntrees);
// La camara cenital (si esta ubicando una obra): reemplaza a la del jugador.
void pg_camera(Camera3D *cam);
// Cada cuadro, con la camara de pg_camera: entrada, validacion y, al confirmar, las obras nuevas.
void pg_update(GameActions *ga, const Props *props, const Terrain *t, Camera3D cam, int vw, int vh, float dt, char *log, size_t len);
void pg_draw_world(const GameActions *ga, Props *props, const Terrain *t, float time);
void pg_draw_hud(GameActions *ga, int vw, int vh);

#endif
