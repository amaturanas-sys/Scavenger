// El valor de las cosas, el comercio y las caravanas (C puro): la fase 5 del plan grande
// (docs/PLAN_GRAN_ACTUALIZACION.md).
//  - Cada bien tiene un valor de referencia en plata, la moneda de la estepa (SILVER_ID).
//  - El precio cambia con la region: lo que alli abunda vale menos (PRICE_CHEAP) y lo que
//    escasea, mas (PRICE_DEAR). Quien vende cobra TRADE_SELL_CUT del precio y quien compra paga
//    TRADE_BUY_MARKUP: lo que se queda el mercader.
//  - Cada mercado vende lo suyo (una tribu, una aldea, una capital, una caravana) y lo de su region.
//  - Las caravanas van y vuelven, al paso, entre un caravasar y la capital mas cercana.
#ifndef ESTEPA_TRADE_H
#define ESTEPA_TRADE_H

#include <stdbool.h>

#include "economy.h"
#include "world.h"

#define SILVER_ID "utileria.material.plata"
#define TRADE_SELL_CUT 0.8f
#define TRADE_BUY_MARKUP 1.25f
#define PRICE_CHEAP 0.7f
#define PRICE_DEAR 1.5f

// Plata por unidad (0: no se comercia: estructuras, animales, tatuajes...).
float item_value(const char *id);
// Lo que vale todo el acopio, en plata.
float stock_value(const Stockpile *s);
// Cuanto cambia el precio en la region: PRICE_CHEAP, 1 o PRICE_DEAR.
float price_factor(const char *id, Region r);
// Lo que pagan por una unidad (0: no la compran) y lo que cuesta comprarla, en plata.
int trade_sell_price(const char *id, Region r);
int trade_buy_price(const char *id, Region r);

typedef enum { MARKET_TRIBE, MARKET_VILLAGE, MARKET_CAPITAL, MARKET_CARAVAN, MARKET_KINDS } MarketKind;
// Lo que vende un mercado: lo suyo y lo de su region, sin repetir. Devuelve cuantos (hasta max).
int market_goods(MarketKind k, Region r, const char **out, int max);

// ---------------------------------------------------------------- caravanas
#define CARAVANS_MAX 4
#define CARAVAN_SPEED 1.1f // m/s: al paso de los camellos

typedef struct {
    float ax, az; // el caravasar
    float bx, bz; // la capital
    float offset; // m: cada una sale a otra hora
} CaravanRoute;

// Una ruta por caravasar, hasta la capital mas cercana.
int caravan_routes(const World *w, CaravanRoute *out, int max);
// Donde va la caravana en el instante t (s de juego), y hacia donde mira: va y vuelve.
void caravan_at(const CaravanRoute *r, float t, float *x, float *z, float *yaw);

#endif
