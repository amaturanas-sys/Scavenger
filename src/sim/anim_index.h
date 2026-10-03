// Que clip de animacion corresponde a cada estado (indice: assets/animaciones.tsv).
//
// El juego pide el clip por nombre al modelo importado; si el modelo no trae
// ese clip, se usa "idle". Los nombres deben existir en el indice: un test
// lo comprueba.
#ifndef ESTEPA_ANIM_INDEX_H
#define ESTEPA_ANIM_INDEX_H

#include <stdbool.h>

#include "actions.h"
#include "animals.h"

#define ANIM_IDLE "idle"

typedef struct {
    bool moving, running, sneaking, grounded;
    bool hidden;      // agachado en la hierba alta
    bool climbing;    // subiendo por la cuerda de la trepa
    bool climb_top;   // pasando por encima del muro
    bool mounted;
    float mount_speed; // m/s de la montura (paso o galope)
    bool carrying;    // lleva un objeto en brazos
    bool sheathed;
    Grip grip;
    int doing;        // ActionId en curso, o -1
    int building;     // BuildId en el que trabaja, o -1
    bool forging;
    // Combate y salud.
    bool dead, down;  // muerto / abatido en el suelo
    bool hit;         // acaba de recibir un golpe
    int attacking;    // golpe en curso: 0 ninguno, 1..3 la serie de golpes
    bool spear;       // arma larga de asta (estocada)
    bool blocking;    // cubriendose con el escudo
    bool limping;     // herido en las piernas: cojea al caminar
} HumanoidState;

// Clip para un humano (jugador o NPC) segun lo que esta haciendo.
const char *anim_humanoid(const HumanoidState *s);
// Clip de una accion individual en curso.
const char *anim_for_action(ActionId a);
// Clip de un cuadrupedo segun su estado.
const char *anim_quadruped(const Animal *a);
// Clip de una fiera en pelea (lobos y otros depredadores enemigos).
const char *anim_quadruped_fight(float speed, bool attacking, bool hit, bool dead);
// Postura de reposo segun la empunadura.
const char *anim_grip(Grip g, bool sheathed);

#endif
