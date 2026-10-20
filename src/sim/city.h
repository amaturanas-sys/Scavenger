// Las ciudades y aldeas de los reinos (C puro): la fase 8a del plan grande
// (docs/PLAN_GRAN_ACTUALIZACION.md). Cada asentamiento del mundo (src/sim/world.h) lleva su
// economia, un dia a la vez (city_day):
//  - la gente y el granero: los campos dan segun la estacion (la cosecha, en otoño; en invierno,
//    nada), la region y la suerte del año, y cada alma come una racion por dia. Pasada la tierra que
//    tienen (CITY_LAND), mas gente no da mas cosecha. Bien comida y en paz, la gente crece; con
//    hambre, se muere o se va, y crece el descontento;
//  - los impuestos: los escribas cobran una parte de lo que puede pagar cada alma (la capital tiene
//    mas escribas: la burocracia es un numero de eficiencia, CITY_SCRIBES) y van al tesoro;
//  - los talleres hacen armas y armaduras en cantidad (las paga el tesoro) para la armeria;
//  - el tesoro paga a los soldados, que se arman en la armeria, y, si sobra, a los mercenarios,
//    que llegan con la riqueza y se van en cuanto falta la paga;
//  - el saqueo de sus campos (city_raid_fields): arrasa la cosecha, vacia parte del granero y deja
//    rencor contra la tribu, que se olvida despacio.
// Ademas, el trazado (city_layout), que crece desde el centro con la gente: el palacio en la
// capital, los barrios (fundicion, cuartel y carpinteria), los anillos de casas y, afuera, los
// campos de cultivo, los corrales y las tiendas de los mercenarios.
#ifndef ESTEPA_CITY_H
#define ESTEPA_CITY_H

#include <stdbool.h>
#include <stdint.h>

#include "clock.h"
#include "economy.h"
#include "world.h"

#define CITY_FARMERS 0.55f     // parte de la gente que trabaja los campos
#define CITY_FARM_RATE 3.2f    // raciones por labrador y dia, con la cosecha entera (rinde 1)
#define CITY_GRANARY_DAYS 150  // el granero guarda hasta tantos dias de comida (lo demas se pudre)
#define CITY_TAX 0.05f         // plata por alma y dia que podrian cobrar los escribas
#define CITY_SOLDIER_PAY 0.5f  // plata por soldado y dia
#define CITY_MERC_PAY 1.2f     // por mercenario y dia
#define CITY_ARMS_COST 2.0f    // lo que cuesta un juego de armas y armadura en sus talleres
#define CITY_RAID_DAYS DAYS_PER_SEASON // arrasados, los campos tardan una estacion en dar de nuevo

typedef struct {
    float people;     // almas (0: todavia sin armar; city_start la arma); con decimales para crecer de a poco
    float grain;      // el granero, en raciones (una por alma y dia)
    float silver;     // el tesoro
    float arms;       // juegos de armas y armaduras en la armeria
    float fields;     // los campos [0, 1]: 1, sembrados enteros; el saqueo los arrasa y en paz se resiembran
    float unrest;     // el descontento [0, 1]: hambre, saqueos, soldados sin paga
    int32_t soldiers; // soldados de oficio
    int32_t mercs;    // mercenarios
    float grudge;     // rencor contra la tribu [0, 100]: saqueos y muertos; se olvida despacio
    int32_t raided;   // el ultimo dia en que le saquearon los campos (0: nunca)
    int32_t raided_lot; // que campo arrasaron (su numero entre los campos del trazado + 1; 0: ninguno): desde ahi se ven quemados
    // El ejercito y el asedio (fase 8b, src/sim/army.h):
    int32_t taken;       // el dia en que la tribu la tomo (0: libre): sin ejercito, paga tributo y su fundicion es tu taller
    int32_t gates_burnt; // las puertas quemadas (un bit por puerta): quedan abiertas
    int32_t besieged;    // el ultimo dia que paso cercada (0: nunca)
    int32_t ladders;     // las escalas apoyadas en la muralla (un bit por tramo)
    int32_t expedition;  // el dia en que salio su ultima expedicion de castigo (0: nunca)
} City;

// La tierra de cultivo (en almas que puede alimentar con la cosecha entera) y los escribas
// (la parte del impuesto que cobran) de una capital o una aldea.
int city_land(SettleKind k);
float city_scribes(SettleKind k);

// Cuanto rinde la region (el bosque, mas; el desierto, menos) y la estacion (otoño, la cosecha;
// invierno, nada).
float city_fertility(Region r);
float city_season_yield(Season s);
// La suerte de la cosecha de una estacion (0,8 .. 1,2): siempre la misma para esa ciudad y ese año.
float city_season_luck(uint32_t seed, int idx, int day);

// Valores por persona: casas y campos que caben en una estructura.
#define CITY_MAX_ALMAS_HOGAR_CAPITAL 110   // capital: 110 almas por casa
#define CITY_MAX_ALMAS_HOGAR_ALDEA 45      // aldea: 45 almas por casa
#define CITY_MAX_ALMAS_CAMPO_CAPITAL 180   // capital: 180 almas por campo
#define CITY_MAX_ALMAS_CAMPO_ALDEA 40      // aldea: 40 almas por campo
// Cuanto rinde la region (el bosque, mas; el desierto, menos) y la estacion (otoño, la cosecha;
// invierno, nada).
float city_fertility(Region r);
float city_season_yield(Season s);
// La suerte de la cosecha de una estacion (0,8 .. 1,2): siempre la misma para esa ciudad y ese año.
float city_season_luck(uint32_t seed, int idx, int day);

// Arma la ciudad al empezar (o al cargar una partida de antes de la v0.9.0): su gente, su granero,
// su tesoro y su guarnicion, con algo de azar de la semilla.
void city_start(City *c, SettleKind k, Region r, uint32_t seed);
// Lo que cobran sus escribas por dia (con descontento o la cosecha arrasada, menos).
float city_taxes(const City *c, SettleKind k);
// Los soldados que quiere tener (una parte de la gente; tomada, ninguno) y el tesoro que guarda
// antes de gastar.
int city_soldier_target(const City *c, SettleKind k);
float city_reserve(const City *c);

// Un dia: cosecha, comida, gente, impuestos, talleres, paga del ejercito y mercenarios.
// luck: la suerte de la cosecha (city_season_luck).
void city_day(City *c, SettleKind k, Region r, Season s, float luck);

// El saqueo de los campos en la estacion s, el dia day: devuelve las raciones que se llevan, o -1 si
// ya estaban arrasados (hace menos de CITY_RAID_DAYS). Arrasa la cosecha y suben el descontento y el
// rencor.
int city_raid_fields(City *c, Season s, int day);
// Los soldados que la tribu le mata (o toma presos): primero los de oficio, despues los mercenarios.
void city_lose_soldiers(City *c, int n);
// Cuantos salen a defender los campos (de la guarnicion, entre 2 y max).
int city_guard_party(const City *c, int max);

// Lo que cambia el precio de un objeto en su mercado: la comida, segun el granero; las armas,
// segun la armeria; con mucho descontento, todo un poco mas caro.
float city_price_factor(const City *c, const char *id);
// ¿Comercia con la tribu? No, con mucho rencor (o, en el reino al que se rinde tributo, con la
// relacion por el suelo). Tomada por la tribu, si.
bool city_trades(const City *c, bool overlord, float relation);
#define CITY_GRUDGE_NO_TRADE 60.0f
#define OVERLORD_NO_TRADE (-30.0f)

// El tributo al reino de la estepa (el kanato): lo que se paga por estacion (segun la tribu) y lo
// que sube la relacion; si una estacion pasa sin pagarlo, baja.
int tribute_due(int members);
#define TRIBUTE_RELATION 10.0f
#define TRIBUTE_MISSED 6.0f
// La estacion de un dia como numero corrido (año * 4 + estacion), para recordar si ya se pago.
int season_index(int day);

// ---------------------------------------------------------------- el trazado
typedef enum {
    LOT_PALACE,    // el palacio del rey o gran jefe, en el centro de la capital
    LOT_HOUSE,     // una casa (la de la region)
    LOT_FORGE,     // la fundicion
    LOT_BARRACKS,  // el cuartel
    LOT_CARPENTRY, // la carpinteria
    LOT_FIELD,     // un campo de cultivo (w x l)
    LOT_CORRAL,    // un corral
    LOT_MERC_TENT, // una tienda de mercenarios, junto a la muralla
    LOT_KINDS
} LotKind;

typedef struct {
    float x, z, yaw; // centro y giro (radianes): las casas miran al centro; los campos, a lo largo del radio
    float w, l;      // medidas (m), de los campos y los corrales
    LotKind kind;
} CityLot;

#define CITY_LOTS_MAX 72
#define CITY_WALL_R 42.0f // la muralla de la capital
// Donde estan los campos: entre estas distancias del centro.
#define CITY_FIELDS_R0 50.0f
#define CITY_FIELDS_R1 78.0f
#define VILLAGE_FIELDS_R0 24.0f
#define VILLAGE_FIELDS_R1 46.0f

// El trazado del asentamiento idx del mundo con c (la gente y los mercenarios): siempre el mismo
// para la misma semilla y la misma gente, y mas grande cuanta mas gente. Los campos y los corrales
// no caen en el agua ni en una cuesta. Devuelve cuantos lotes escribio.
int city_layout(const World *w, int idx, const City *c, CityLot *out, int max);
// El lote de esa clase que tiene (x, z) adentro (margin: m de mas alrededor), o -1.
int city_lot_at(const CityLot *lots, int n, LotKind kind, float x, float z, float margin);

#endif
