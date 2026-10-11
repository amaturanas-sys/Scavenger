// El comercio, las caravanas y el saqueo en el juego (src/sim/trade.h): la fase 5 del plan grande
// (docs/PLAN_GRAN_ACTUALIZACION.md).
//  - F junto a una tribu amiga o neutral, una aldea, una capital, un caravasar o una caravana:
//    vender lo que llevas (encima, en las alforjas, la carreta o las mochilas de la escolta) y
//    comprar lo suyo, en plata. Los precios cambian con la region;
//  - las caravanas van y vuelven, al paso, entre cada caravasar y la capital mas cercana, con
//    camellos cargados y dos jinetes de escolta. Se detienen a comerciar;
//  - asaltar una caravana: su escolta pelea y, vencida, la carga queda en el suelo; el reino
//    se entera. La caravana tarda dos dias en volver a salir;
//  - un campamento rival cuyos guerreros ya salieron al paso, sin nadie en pie alrededor, se
//    saquea con F (se rearma al alejarse).
#ifndef ESTEPA_TRADE_GAME_H
#define ESTEPA_TRADE_GAME_H

#include <stdbool.h>
#include <stddef.h>

#include "game/actions_game.h"
#include "game/combat_game.h"

// ¿Hay con quien comerciar (o algo que saquear) al alcance de F? Lo pide al proximo td_update.
bool td_try_talk(GameActions *ga, const Terrain *t, const Player *p);
void td_update(GameActions *ga, Combat *cb, Troop *troop, const Props *props, const Terrain *t, const Player *p, float now, float dt,
               char *log, size_t len);
// La pista de F («F: comerciar con los...»), sobre la de la fauna.
void td_draw_hud(int w, int h);
// Las caravanas. Dentro de BeginMode3D.
void td_draw_world(const GameActions *ga, Props *props, const Terrain *t, const Player *p, float time);
// Prueba (--comerciar): what = "raiz", "vender" o "comprar" (el mercado mas cercano, con algo que
// vender en la mochila), "cerca" (a su lado, sin hablar), "caravana" (a su costado, mirandola pasar) o "asaltar" (la asalta), las dos
// con la de la ruta mas larga, o "saquear" (un campamento rival cuyos guerreros ya salieron).
void td_test(GameActions *ga, Combat *cb, Troop *troop, const Props *props, const Terrain *t, Player *p, const char *what, char *log,
             size_t len);

#endif
