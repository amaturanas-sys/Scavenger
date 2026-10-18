// El ejercito de los reinos y el asedio (C puro): la fase 8b del plan grande
// (docs/PLAN_GRAN_ACTUALIZACION.md). Sobre las ciudades de la 8a (src/sim/city.h):
//  - el ejercito que sale (army_compose): de la guarnicion (soldados y mercenarios) y del tesoro, que
//    paga la campaña. La infanteria pesada con paveses, los soldados, los arqueros, los ballesteros y
//    los carros de guerra (solo las capitales: dos caballos, conductor y arquero);
//  - la formacion cerrada (formation_slot): filas y columnas que se mueven juntas;
//  - el carro de guerra (chariot_speed): rapido en el llano, lento en la cuesta y vadeando;
//  - las expediciones de castigo (punitive_due): con la relacion por el suelo, el kanato manda una
//    contra el campamento (y una ciudad con mucho rencor, tambien); lo que se llevan (stock_plunder);
//  - el asedio: la guerra cierra las puertas (city_at_war); la muralla y sus puertas (city_wall_*);
//    el cerco y el hambre (city_siege_day), el fuego (city_burn_grain); la rendicion
//    (city_surrenders) y la ciudad tomada (city_take), que paga tributo a la tribu.
#ifndef ESTEPA_ARMY_H
#define ESTEPA_ARMY_H

#include <stdbool.h>
#include <stdint.h>

#include "city.h"
#include "economy.h"

// ---------------------------------------------------------------- el ejercito
// Las clases de tropa (crecen al final).
typedef enum {
    ARMY_HEAVY,    // infanteria pesada: pavés, lanza y escamas de hierro
    ARMY_SOLDIER,  // soldado: lanza y escudo (src/sim/combat.h: ENEMY_SOLDIER)
    ARMY_ARCHER,   // arquero del reino
    ARMY_CROSSBOW, // ballestero
    ARMY_CHARIOT,  // carro de guerra: dos caballos, conductor y arquero (dos hombres)
    ARMY_UNIT_KINDS
} ArmyUnit;

typedef struct {
    int n[ARMY_UNIT_KINDS];
} ArmyComp;

#define ARMY_MAN_COST 6.0f      // plata por hombre para una campaña (la paga y las raciones del camino)
#define ARMY_HEAVY_COST 9.0f    // la infanteria pesada: su armadura se gasta
#define ARMY_CHARIOT_COST 30.0f // un carro: los caballos, las ruedas y dos hombres
#define ARMY_BUDGET 0.5f        // la parte del tesoro que se gasta en una campaña

// Cuantas figuras salen al mundo (un carro es una) y cuantos hombres son (un carro, dos).
int army_units(const ArmyComp *a);
int army_men(const ArmyComp *a);
// Lo que cuesta al tesoro.
float army_cost(const ArmyComp *a);
// El ejercito que sale de la ciudad c (hasta max_units figuras). campaign: una campaña (la expedicion
// de castigo), que paga el tesoro (ARMY_BUDGET de lo que tiene): sin plata, no sale nadie; si no, una
// salida en defensa de la ciudad (sin carros y sin costo). Siempre queda en casa un tercio de la
// guarnicion. Las aldeas no tienen infanteria pesada ni carros. Devuelve cuantas figuras.
int army_compose(const City *c, SettleKind k, int max_units, bool campaign, ArmyComp *out);

// La formacion cerrada: el lugar del hombre i (de n) en filas de cols, a spacing m uno de otro, con
// el centro del frente en (cx, cz) y mirando a yaw (0: +z; como el jugador). La primera fila va
// delante; la ultima, si no esta llena, centrada.
void formation_slot(int i, int n, int cols, float spacing, float cx, float cz, float yaw, float *x, float *z);
#define FORMATION_SPACING 1.4f

// El carro de guerra: su velocidad (base, en el llano) segun la cuesta del terreno (slope: m que
// sube o baja por m) y el agua que vadea (wade: m de hondo). En el llano vuela; en terreno roto y
// en el agua, las ruedas lo frenan mucho (la infanteria casi no lo nota).
float chariot_speed(float base, float slope, float wade);
#define CHARIOT_SPEED 10.0f // m/s en el llano
#define CHARIOT_FLAT 0.06f  // hasta esta cuesta, el llano

// ---------------------------------------------------------------- las expediciones de castigo
#define PUNITIVE_RELATION (-40.0f)       // con la relacion asi de baja, el kanato manda su ejercito
#define PUNITIVE_GRUDGE 80.0f            // y una ciudad con tanto rencor, el suyo
#define PUNITIVE_DAYS DAYS_PER_SEASON    // entre una y otra de la misma ciudad
#define PUNITIVE_APPEASE 25.0f           // si saquean el campamento, el enojo se calma (relacion +)
#define PUNITIVE_MAX_UNITS 12            // figuras de una expedicion
// ¿Sale una expedicion de castigo de esa ciudad? overlord: es la capital del reino al que se rinde
// tributo (cuenta la relacion); grudge: el rencor de la ciudad; last: el dia de su ultima expedicion.
bool punitive_due(bool overlord, float relation, float grudge, int day, int last);
// La parte del acopio que se llevan del campamento: mas con mas hombres, menos con mas defensores.
float punitive_share(int men, int defenders);
// El saqueo: se llevan share de la comida, el grano y la plata del acopio. Devuelve cuanto.
int stock_plunder(Stockpile *s, float share);

// ---------------------------------------------------------------- el asedio
// La muralla de la capital (src/game/city_game.c la dibuja): doce torres a CITY_WALL_R y entre ellas
// doce tramos rectos; cada tercer tramo (0, 3, 6 y 9) es una puerta (a 15, 105, 195 y 285 grados;
// los angulos, con x = cos y z = sin, desde el centro).
#define CITY_WALL_SEGMENTS 12
#define CITY_GATES 4
#define CITY_WALL_HEIGHT 4.6f // m sobre el suelo: lo que se trepa
// La distancia del centro a la muralla en la direccion ang.
float city_wall_dist(float ang);
// El tramo de la muralla en la direccion ang (0..11), y su puerta (0..3; -1: muro).
int city_wall_segment(float ang);
int city_segment_gate(int seg);
float city_gate_angle(int gate);
// ¿El paso de (x0, z0) a (x1, z1) (relativos al centro) cruza la muralla? seg: por que tramo.
bool city_wall_crossed(float x0, float z0, float x1, float z1, int *seg);
// ¿La muralla corta ese paso? El muro siempre; en el tramo de una puerta, sus dos lienzos de muro
// (mas alla de CITY_GATE_HALF del centro de la puerta) y la puerta misma si esta cerrada (war: la
// ciudad esta en guerra; c NULL: abierta); y los antemuros de las puertas, siempre. gate: por que
// puerta pasa o choca (-1: muro o antemuro).
#define CITY_GATE_HALF 3.0f
bool city_wall_blocks(const City *c, bool war, float x0, float z0, float x1, float z1, int *gate);
// La puerta acodada: delante de cada puerta, un antemuro (el muro de adelante, a CITY_BARBICAN_OUT
// de la muralla y de 2 * CITY_BARBICAN_HALF de largo, cerrado de un costado). Se entra por el
// costado abierto, a lo largo de la muralla, y se dobla hacia la puerta: el que ataca le da el
// flanco a la muralla. En las coordenadas de la puerta: X hacia afuera desde la muralla, Z de
// costado, (-sin a, cos a) con a el angulo de la puerta.
#define CITY_BARBICAN_OUT 5.5f
#define CITY_BARBICAN_HALF 7.0f
// De que costado (Z: +1 o -1) esta cerrado el antemuro de la puerta gate (del otro, la boca).
int city_gate_closed_side(int gate);
// El camino por la puerta gate de afuera hacia adentro (relativo al centro de la ciudad): rodear el
// antemuro por afuera (la esquina cerrada y la de la boca), la boca, el patio delante de la puerta
// y adentro.
#define CITY_GATE_PATH 5
void city_gate_path(int gate, float out[CITY_GATE_PATH][2]);
// El rodeo por las puertas: si de (fx, fz) a (tx, tz) (relativos al centro) la muralla o un antemuro
// cortan el paso (con las puertas abiertas), el punto al que ir antes en (wx, wz): el primero del
// camino mas corto por un grafo chico (los caminos de las puertas, de puerta a puerta por adentro y
// de torre en torre por afuera), que se vea derecho desde aca. Con holgura: la muralla se cruza solo
// por el medio de una puerta, lejos de las puntas de los antemuros. Sin estado: llamado en cada
// cuadro, el camino no cambia mientras se lo sigue. false si se va derecho.
bool city_route(float fx, float fz, float tx, float tz, float *wx, float *wz);
// Lo primero que corta el paso de (x0, z0) a (x1, z1) (relativos al centro): la muralla (con sus
// puertas cerradas, como city_wall_blocks) o un antemuro (siempre). false si nada.
typedef struct {
    float t;      // donde choca, en [0, 1] del paso
    float tx, tz; // a lo largo de lo que choca (para deslizarse)
    int gate;     // la puerta de la muralla por la que pasa o en la que choca (-1: muro o antemuro)
} WallHit;
bool city_wall_hit(const City *c, bool war, float x0, float z0, float x1, float z1, WallHit *hit);

// ¿Esta en guerra con la tribu? Con mucho rencor (o, el reino al que se rinde tributo, con la
// relacion por el suelo): cierra el mercado y las puertas. Tomada, no.
bool city_at_war(const City *c, bool overlord, float relation);
// ¿Esta abierta esa puerta? En paz, si; en guerra, solo si la quemaron.
bool city_gate_open(const City *c, int gate, bool war);

// Un dia de cerco (despues de city_day con la cosecha en cero: los campos no se trabajan): la gente
// del campo se refugia adentro y come, no entra el comercio y crece el descontento.
#define CITY_SIEGE_EAT 0.5f // raciones de mas por alma y dia
void city_siege_day(City *c, SettleKind k);
// El fuego (una flecha encendida que cae adentro): quema una parte del granero y asusta a la gente.
// Devuelve las raciones quemadas.
#define CITY_FIRE_GRAIN 0.03f
int city_burn_grain(City *c);

// La rendicion: se rinde sin guarnicion (city_garrison_floor o menos), con hambre (menos de
// CITY_STARVING dias de grano) o en revuelta (descontento de CITY_REVOLT o mas).
#define CITY_STARVING 5.0f
#define CITY_REVOLT 0.9f
int city_garrison_floor(const City *c, SettleKind k);
bool city_surrenders(const City *c, SettleKind k);
// La tribu la toma el dia day: sin ejercito (lo licencian), sin rencor y abierta.
void city_take(City *c, int day);
// Tomada, paga tributo a la tribu cada estacion: una parte de lo que cobran sus escribas, de su
// tesoro. Devuelve la plata.
int city_vassal_tribute(City *c, SettleKind k);
// La paz con un reino en guerra se compra: tanta plata (segun la tribu).
int peace_price(int members);

#endif
