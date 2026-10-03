// Salud y heridas (C puro, sin raylib). Lo usan el jugador, los integrantes de
// la tribu, los NPCs y los enemigos (personas y fieras).
//
// Cada cuerpo tiene vida (puntos), sangre y una lista de heridas. Una herida
// tiene tipo (corte, golpe, fractura, mordida, congelacion), parte del cuerpo
// y gravedad. Las heridas abiertas sangran hasta que se vendan; la sangre que
// falta tumba (abatido) y, si se acaba, mata. Las heridas sanan con el tiempo,
// mas rapido tratadas y en reposo; las fracturas solo sanan entablilladas.
// Las heridas de piernas frenan, las de brazos debilitan los golpes.
#ifndef ESTEPA_HEALTH_H
#define ESTEPA_HEALTH_H

#include <stdbool.h>

#include "rng.h"

typedef enum { WOUND_CUT, WOUND_BRUISE, WOUND_FRACTURE, WOUND_BITE, WOUND_FROSTBITE, WOUND_COUNT } WoundKind;

typedef enum { PART_HEAD, PART_TORSO, PART_ARM_L, PART_ARM_R, PART_LEG_L, PART_LEG_R, PART_COUNT } BodyPart;

#define WOUNDS_MAX 8
#define PART_RANDOM (-1)

typedef struct {
    WoundKind kind;
    BodyPart part;
    float severity; // gravedad [0, 1]: leve < 0.3 < moderada < 0.6 < grave
    bool bleeding;
    bool treated;   // vendada o entablillada
} Wound;

typedef struct {
    float hp, hp_max;
    float blood; // [0, 1]
    Wound wounds[WOUNDS_MAX];
    int wound_count;
    bool down; // abatido: no puede moverse ni pelear
    bool dead;
} Health;

void health_init(Health *h, float hp_max);
// Recibe un golpe: resta vida y abre una herida (part = PART_RANDOM elige al azar,
// el torso es lo mas probable). Devuelve el indice de la herida, o -1 si no abrio ninguna.
int health_hit(Health *h, Rng *rng, float damage, WoundKind kind, int part);
// Avanza el tiempo: sangrado, coagulacion de cortes leves, regeneracion y cicatrizacion.
// resting: quieto o durmiendo (sana mas rapido). healer: habilidad de quien atiende [0, 1].
void health_update(Health *h, Rng *rng, float dt, bool resting, float healer);
// Venda las heridas que sangran y entablilla fracturas. Devuelve cuantas trato.
int health_treat(Health *h);
// Un dia de descanso en el campamento (con curandero, mas).
void health_daily(Health *h, float healer);
// Revive a alguien abatido (no muerto): queda con poca vida y sangre.
void health_revive(Health *h);

bool health_bleeding(const Health *h);
int health_untreated(const Health *h); // heridas sin tratar que lo necesitan
// Efectos sobre el cuerpo.
float health_speed_scale(const Health *h);  // piernas y sangre: 1 sano, 0 abatido
float health_attack_scale(const Health *h); // brazos y sangre
// La peor herida (indice) o -1.
int health_worst(const Health *h);

const char *wound_name(WoundKind k);  // UTF-8
const char *part_name(BodyPart p);    // UTF-8
const char *severity_name(float s);   // leve / moderada / grave
// "Corte en el brazo izquierdo (grave, sangra)"
int wound_describe(const Wound *w, char *out, int len);
// "sano", "herido", "malherido", "abatido", "muerto"
const char *health_state_name(const Health *h);

#endif
