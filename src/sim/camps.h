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
#ifndef ESTEPA_CAMPS_H
#define ESTEPA_CAMPS_H

#include <stdbool.h>

#include "economy.h"
#include "troop.h"

#define CAMPS_MAX 6
#define CAMP_TASKS 6
#define CAMP_RADIUS_M 45.0f    // lo que abarca un campamento
#define CAMP_MIN_APART 70.0f   // un campamento nuevo, lejos de los demas
#define CAMP_NAME_LEN 32

typedef enum { TASK_RECRUIT, TASK_TRAIN } CampTaskKind;

typedef struct {
    CampTaskKind kind;
    int member;  // quien sale a reclutar o se capacita (id)
    Role role;   // el oficio que aprende
    float work, done; // segundos de trabajo
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
// ¿Esta ocupado en alguna tarea de ese campamento?
bool camp_member_busy(const CampSite *c, int member);
// Pone una tarea en cola. false si no hay hueco o ese integrante ya esta ocupado.
bool camp_add_task(CampSite *c, CampTaskKind kind, int member, Role role);
// Avanza las tareas: devuelve cuantas terminaron y las copia en done_out (hasta max).
int camp_tick(CampSite *c, float dt, const float *rates, CampTask *done_out, int max);

#endif
