// Salud y heridas (C puro, sin raylib). Lo usan el jugador, los integrantes de
// la tribu, los NPCs y los enemigos (personas y fieras).
//
// Cada cuerpo tiene vida (puntos), sangre y una lista de heridas. Una herida
// tiene tipo (corte, golpe, fractura, mordida, congelacion), zona del cuerpo
// y gravedad. Cada zona recibe el daño de forma distinta: el cuello y la
// cabeza son letales, el torso aguanta, un antebrazo poco (part_damage_scale).
// Las heridas abiertas sangran hasta que se vendan; la sangre que falta tumba
// (abatido) y, si se acaba, mata. Las heridas sanan con el tiempo, mas rapido
// tratadas y en reposo; las fracturas solo sanan entablilladas. El veneno
// (serpientes, escorpiones, arañas, avispas) quita vida poco a poco; las
// hierbas lo cortan a la mitad.
// Las heridas de piernas frenan, las de brazos debilitan los golpes.
#ifndef ESTEPA_HEALTH_H
#define ESTEPA_HEALTH_H

#include <stdbool.h>

#include "rng.h"

typedef enum { WOUND_CUT, WOUND_BRUISE, WOUND_FRACTURE, WOUND_BITE, WOUND_FROSTBITE, WOUND_BURN, WOUND_COUNT } WoundKind;

// Zonas del cuerpo humano (src/sim/body.h les da forma); las fieras usan las
// mismas con otros nombres (antebrazos = patas delanteras, piernas = traseras).
typedef enum {
    PART_HEAD,
    PART_NECK,
    PART_THORAX,
    PART_ABDOMEN,
    PART_PELVIS,
    PART_UPPER_ARM_L, // brazo (del hombro al codo)
    PART_FOREARM_L,   // antebrazo y mano
    PART_UPPER_ARM_R,
    PART_FOREARM_R,
    PART_THIGH_L, // muslo
    PART_SHIN_L,  // pierna (de la rodilla al pie)
    PART_THIGH_R,
    PART_SHIN_R,
    PART_COUNT
} BodyPart;

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
    bool beast; // fiera: nombres de zonas de animal
    float venom; // veneno en el cuerpo: la vida que todavia va a quitar
    float sequela; // secuelas [0, 1] (la mordida del tiburon, docs/AGUA.md fase 5): menos aguante y velocidad; no
                   // se curan solas, solo con un curandero o un druida del campamento
} Health;

void health_init(Health *h, float hp_max);
void health_init_beast(Health *h, float hp_max);

// Cuanto multiplica el daño un impacto en cada zona, y cuanto sangra.
float part_damage_scale(BodyPart p);
float part_bleed_scale(BodyPart p);
bool part_is_arm(BodyPart p);
bool part_is_leg(BodyPart p);
// Una extremidad al azar (congelacion, caidas).
int health_random_limb(Rng *rng);
// Una zona al azar con los pesos de un golpe cuerpo a cuerpo (el tronco, lo mas probable).
int health_pick_part(Rng *rng);
// Recibe un golpe: resta vida (segun la zona) y abre una herida (part = PART_RANDOM
// elige al azar; el torso es lo mas probable). damage es el daño antes de la zona.
// Devuelve el indice de la herida, o -1 si no abrio ninguna.
int health_hit(Health *h, Rng *rng, float damage, WoundKind kind, int part);
// Avanza el tiempo: sangrado, coagulacion de cortes leves, regeneracion y cicatrizacion.
// resting: quieto o durmiendo (sana mas rapido). healer: habilidad de quien atiende [0, 1].
void health_update(Health *h, Rng *rng, float dt, bool resting, float healer);
// Venda las heridas que sangran y entablilla fracturas. Devuelve cuantas trato.
int health_treat(Health *h);
// Veneno de una picadura o mordedura (puntos de vida que quitara poco a poco).
void health_poison(Health *h, float amount);
bool health_poisoned(const Health *h);
// Un dia de descanso en el campamento (con curandero, mas).
void health_daily(Health *h, float healer);
// Revive a alguien abatido (no muerto): queda con poca vida y sangre.
void health_revive(Health *h);

// Las secuelas: lo que deja una herida terrible y no se va con el descanso (docs/AGUA.md, fase 5). Cada
// mordida del tiburon suma SEQUELA_BITE (hasta 1). Con todas: un cuarto menos de velocidad y el aguante rinde
// algo mas de la mitad. Solo las curan el curandero del campamento (de a poco, health_sequela_tend) o el
// druida (de una vez, health_sequela_cure).
#define SEQUELA_BITE 0.5f
#define SEQUELA_TEND_SECONDS 90.0f // lo que tarda el curandero en curar unas secuelas completas
void health_add_sequela(Health *h, float amount);
float health_endurance_scale(const Health *h); // cuanto rinde el aguante: 1 sin secuelas, 0,55 con todas
void health_sequela_tend(Health *h, float dt);  // el curandero: las va curando (dt en s)
void health_sequela_cure(Health *h);            // el druida: de una vez

bool health_bleeding(const Health *h);
int health_untreated(const Health *h); // heridas sin tratar que lo necesitan
// Efectos sobre el cuerpo.
float health_speed_scale(const Health *h);  // piernas y sangre: 1 sano, 0 abatido
float health_attack_scale(const Health *h); // brazos y sangre
// La peor herida (indice) o -1.
int health_worst(const Health *h);

const char *wound_name(WoundKind k);  // UTF-8
const char *part_name(BodyPart p, bool beast); // UTF-8
const char *severity_name(float s);   // leve / moderada / grave
// "Corte en el antebrazo izquierdo (grave, sangra)"
int wound_describe(const Wound *w, bool beast, char *out, int len);
// "sano", "herido", "malherido", "abatido", "muerto"
const char *health_state_name(const Health *h);

#endif
