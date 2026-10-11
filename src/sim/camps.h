// Campamentos de la tribu (C puro):
//  - se funda uno al levantar una estructura basica (refugio o tienda) lejos de los
//    demas; hay que nombrar un guardian, que lo administra;
//  - cada campamento tiene su acopio y su gente; el guardian ordena obras, reune la
//    escolta, recluta gente nueva y capacita en oficios;
//  - las tareas gastan recursos (comida, herramientas, materiales) y gente: quien se
//    capacita o sale a reclutar no hace otra cosa, y los ociosos del campamento ayudan
//    (mas manos, mas rapido; un maestro del oficio, mucho mas);
//  - disolver: se queman las estructuras, se carga lo que quepa en las carretas que
//    haya y la gente sigue al jugador.
//  - la tribu (fase 4 del plan grande, docs/PLAN_GRAN_ACTUALIZACION.md): el guardian manda a
//    uno o dos a buscar reclutas, explorar recursos, pastorear, cazar o hacer guardia. Las
//    salidas (reclutas, exploracion, caza) se resuelven por tiempo, como los viajes
//    (src/sim/travel.h): los que van no se ven hasta que vuelven, con su suerte y lo que traen.
//    El pastoreo y la guardia se ven cerca del campamento. Ir acompañado y saber el oficio
//    bajan el riesgo y rinden mas.
#ifndef ESTEPA_CAMPS_H
#define ESTEPA_CAMPS_H

#include <stdbool.h>

#include "animals.h"
#include "clock.h"
#include "economy.h"
#include "rng.h"
#include "travel.h"
#include "troop.h"

#define CAMPS_MAX 6
#define CAMP_TASKS 8
#define CAMP_RADIUS_M 45.0f    // lo que abarca un campamento
#define CAMP_MIN_APART 70.0f   // un campamento nuevo, lejos de los demas
#define CAMP_NAME_LEN 32

// Se guarda: crece al final.
typedef enum {
    TASK_RECRUIT,         // explorar en busca de reclutas (salida)
    TASK_TRAIN,           // aprender un oficio, en el campamento
    TASK_SCOUT_RESOURCES, // explorar recursos: agua, pastos, campamentos rivales y ciudadelas (salida)
    TASK_HERD,            // pastorear el ganado cerca del campamento
    TASK_HUNT,            // cazar (salida)
    TASK_GUARD,           // un turno de guardia: la ronda de src/sim/squad.h
    TASK_KINDS
} CampTaskKind;

typedef struct {
    CampTaskKind kind;
    int member;  // quien la hace (id)
    Role role;   // capacitar: el oficio que aprende
    float work, done; // segundos de trabajo
    int member2; // el acompañante (id), o 0: va solo
} CampTask;

typedef struct {
    bool used;
    char name[CAMP_NAME_LEN];
    float x, z;
    int guardian; // id del integrante, o -1
    int founded_day;
    Stockpile stock;
    CampTask task[CAMP_TASKS];
    int ntask;
} CampSite;

void camps_init(CampSite *c, int n);
// Funda uno en (x, z); devuelve su indice o -1 (sin hueco o demasiado cerca de otro).
int camp_found(CampSite *c, int n, float x, float z, int day);
// El campamento que abarca (x, z), o -1.
int camp_at(const CampSite *c, int n, float x, float z);
int camp_nearest(const CampSite *c, int n, float x, float z, float *dist);
int camps_count(const CampSite *c, int n);
bool camp_far_enough(const CampSite *c, int n, float x, float z);

// Oficios que se pueden enseñar en el campamento (soldado, herrero, druida, orfebre,
// pastor, cazador, explorador), lo que cuesta y cuanto tarda.
bool role_trainable(Role r);
const Ingredient *train_cost(Role r);
float train_work(Role r); // segundos de trabajo, con una sola persona
const Ingredient *recruit_cost(void);
float recruit_work(void);
// Ritmo de una tarea: ayudantes ociosos (+25 % cada uno, hasta x2) y maestros del oficio (+60 % cada uno).
float task_rate(int helpers, int teachers);
// ¿Esta ocupado en alguna tarea de ese campamento (la hace o acompaña)?
bool camp_member_busy(const CampSite *c, int member);
// La tarea en la que esta ese integrante (la hace o acompaña), o NULL.
const CampTask *camp_task_of(const CampSite *c, int member);
// Pone una tarea en cola. false si no hay hueco o ese integrante ya esta ocupado.
bool camp_add_task(CampSite *c, CampTaskKind kind, int member, Role role);
// Igual, con un acompañante (member2: id, o 0 para ir solo; capacitar no lleva).
bool camp_add_task2(CampSite *c, CampTaskKind kind, int member, int member2, Role role);
// Avanza las tareas: devuelve cuantas terminaron y las copia en done_out (hasta max).
int camp_tick(CampSite *c, float dt, const float *rates, CampTask *done_out, int max);

// ---------------------------------------------------------------- las tareas de la tribu
#define TASK_SCOUT_WORK 300.0f // s: explorar recursos
#define TASK_HUNT_WORK 300.0f  // s: cazar
#define TASK_HERD_WORK 300.0f  // s: un turno de pastoreo
#define TASK_GUARD_WORK 600.0f // s: un turno de guardia

// Lo que dura (s; capacitar, segun el oficio).
float task_work(CampTaskKind kind, Role role);
// Salidas: los que van no se ven hasta que vuelven (reclutas, exploracion, caza).
bool task_away(CampTaskKind kind);
// El oficio que mejor la hace: explorador (reclutas y recursos), pastor, cazador, guardia
// (y el soldado); ROLE_NONE si no hay (capacitar).
Role task_skill(CampTaskKind kind);
bool role_suits(CampTaskKind kind, Role r);
// Lo que lleva cada uno (provisiones); capacitar: el equipo del oficio. Lista terminada en {NULL, 0}.
const Ingredient *task_cost(CampTaskKind kind, Role role);
// Paga la tarea de people personas con el acopio. false (sin tocar nada) si no alcanza.
bool task_pay(Stockpile *s, CampTaskKind kind, Role role, int people);
// Probabilidad de un percance en la salida [0, 0.9]: cada uno que sabe el oficio (skilled) la
// baja, ir acompañado (people 2) tambien; de noche sube. 0 en lo que se hace en el campamento.
float task_risk(CampTaskKind kind, int skilled, int people, bool night);
// La suerte de los que salieron (fate[0..people)): si hay percance, cada uno vuelve herido o no
// vuelve; acompañados se cuidan y mueren menos.
void task_fates(CampTaskKind kind, int skilled, int people, bool night, Rng *rng, Fate *fate);
// Buscar reclutas: con cuantos vuelven (0..2). Un explorador y un acompañante ayudan.
int recruit_found(int skilled, int people, Rng *rng);

// Explorar recursos: lo que se marca en el mapa de memoria (src/sim/memory_map.h).
typedef enum { FIND_WATER, FIND_PASTURE, FIND_RIVAL, FIND_CITADEL, FIND_KINDS } FindKind;
typedef struct {
    float x, z;
    FindKind kind;
} ScoutCand;
// Hasta donde llegan (m desde el campamento) y cuantos hallazgos traen (1..4).
float scout_radius(int skilled, int people);
int scout_finds(int skilled, int people, Rng *rng);
// Los hallazgos: de los candidatos dentro del radio que aun no estan marcados (known[i]), primero
// el mas cercano de cada clase y despues los mas cercanos. Escribe los indices en out (hasta max).
int scout_pick(const ScoutCand *c, int n, const bool *known, float cx, float cz, float radius, int max, int *out);
// Campamentos rivales y ciudadelas: marca de peligro; agua y pastos: de interes.
bool find_danger(FindKind k);

// Cazar: la presa y lo que traen.
typedef struct {
    int kills; // presas abatidas
    int prey;  // la especie (Species de src/sim/animals.h), o -1 si no hay en la region
    int meat, hides, bones, sinew, fat;
} HuntBag;
// Una presa de la region (habitat: HAB_* de region_habitat), segun lo comunes que son; -1 si no hay.
int hunt_prey(int habitat, Rng *rng);
// Lo que traen: mas con cazadores y acompañados; en invierno escasea; en otoño, mas grasa.
HuntBag hunt_bag(int habitat, Season season, int skilled, int people, Rng *rng);

// Pastorear (cada paso dt del turno): el ganado de la tribu come y bebe en el campo; baja el
// hambre y la sed, mas rapido con dos pastores; sin pasto (nieve), come la mitad. false si no
// le toca (no es de la tribu o come carne).
bool herd_graze(Animal *a, float dt, int herders, bool grass);
// La leche de un animal al volver del pastoreo (los que no se ordeñaron hoy; con dos pastores,
// un poco mas). Lo deja ordeñado.
int herd_milk(Animal *a, int herders);
// Probabilidad de que las fieras se lleven un animal en el turno: mucho menos con dos pastores
// o con un pastor de oficio; de noche, mas.
float herd_loss_chance(int herders, int skilled, bool night);

#endif
