// Combate cuerpo a cuerpo (C puro): golpes, combos, patadas, escudos, agarres,
// ganchos y parry. Las mismas reglas valen para el jugador, la tribu y los enemigos.
// El jugador pelea con cuatro botones (H ataque, J bloqueo, K parry, L carga o patada):
//  - golpe ligero (H) y combos: tres golpes seguidos en la ventana; con armas
//    cortas (daga, cuchillo, sable) los combos son mas rapidos; con largas (lanza,
//    espada, guja) llegan mas lejos y el remate puede tumbar;
//  - golpe pesado (mantener H): mas daño y rompe la guardia del escudo;
//  - bloqueo (J): con escudo cubre el frente; sin escudo para con el arma, que aguanta
//    la mitad del golpe y cansa (gasta aguante);
//  - parry (K): el rival anuncia su golpe (levanta el arma un momento); en la ventana
//    justa, antes de que caiga, el parry le gana: con hacha o guja le engancha el
//    escudo y se lo arranca, con las manos o una daga le hace una llave y lo tumba
//    (desarmado), con las demas armas desvia el golpe y lo deja abierto; a destiempo,
//    el que para queda expuesto;
//  - carga o patada (L): con escudo, carga que empuja y puede tumbar; sin escudo,
//    patada que aparta y desequilibra; a la carrera, con inercia, tumba incluso a
//    quien se cubre con escudo;
//  - golpe de escudo: cubriendose con el escudo (J) y atacando (H): aturde;
//  - agarre: del escudo (se lo arranca), del brazo armado (lo desarma) o de una
//    extremidad, y lo tumba con una llave; si falla, el que agarra queda expuesto;
//  - gancho: un arma con gancho (hacha, guja, alabarda) engancha el escudo enemigo
//    y se lo hace soltar.
// Quien esta en el suelo no se cubre y recibe mas daño hasta que se levanta.
#ifndef ESTEPA_MELEE_H
#define ESTEPA_MELEE_H

#include <stdbool.h>

#include "actions.h"
#include "combat.h"
#include "rng.h"

typedef enum {
    MOVE_LIGHT,        // golpe (y combo)
    MOVE_HEAVY,        // golpe pesado
    MOVE_KICK,         // patada
    MOVE_RUN_KICK,     // patada con inercia (a la carrera)
    MOVE_SHIELD_BASH,  // golpe de escudo
    MOVE_SHIELD_CHARGE,// carga con escudo
    MOVE_GRAPPLE,      // agarre y llave
    MOVE_HOOK,         // enganchar el escudo
    MOVE_PARRY,        // parry (K): el intento, hasta saber como sale
    MOVE_DEFLECT,      // desvio: el golpe del rival resbala por el arma
    MOVE_COUNT
} MeleeMove;

typedef struct {
    const char *name;   // UTF-8
    const char *clip;   // animacion (assets/animaciones.tsv)
    float reach;        // m (los golpes usan el alcance del arma)
    float damage;       // daño base (los golpes usan el del arma)
    float recovery;     // s hasta poder hacer otra cosa
    WoundKind wound;
} MoveDef;

// Quien pelea, visto por las reglas.
typedef struct {
    bool shield;     // lleva escudo (y no lo solto)
    bool blocking;   // se cubre
    bool down;       // en el suelo
    bool staggered;  // desequilibrado: no se puede cubrir
    bool attacking;  // en mitad de un golpe (se le puede agarrar el brazo)
    bool armed;      // tiene arma en la mano
    float facing;    // coseno entre su frente y la direccion al rival (1 de frente)
    float strength;  // fuerza (1 normal; grandes guerreros, mas)
    float weight;    // kg de armadura (pesa: cuesta mas tumbarlo)
    float health;    // [0, 1] vida restante (herido, se resiste peor)
} Fighter;

typedef enum { GRAB_NONE, GRAB_SHIELD, GRAB_WEAPON_ARM, GRAB_LIMB } GrabTarget;

typedef struct {
    bool landed;         // el movimiento conecto
    bool blocked;        // lo paro el escudo (o el arma)
    float damage;        // daño que pasa (antes de la armadura)
    WoundKind wound;
    bool knocked_down;   // al suelo
    bool staggered;      // desequilibrado un momento (sin guardia)
    bool shield_dropped; // solto el escudo
    bool disarmed;       // solto el arma
    bool attacker_staggered; // fallo el agarre o choco con otro escudo
    GrabTarget grab;
    float guard_cost;    // aguante que gasta el que se cubrio (parar con el arma cansa)
} MeleeResult;

const MoveDef *move_def(MeleeMove m);
// ¿El arma puede enganchar un escudo? (hacha, guja, alabarda, gancho)
bool weapon_can_hook(const char *inv_id);
// Arma corta (combos rapidos) o larga (alcance, remate que tumba).
bool weapon_is_short(const char *inv_id);
bool weapon_is_long(const char *inv_id);
// Escala de daño del paso del combo (1, 2, 3...) y tiempo hasta el siguiente golpe.
float melee_combo_scale(int step);
float melee_combo_cooldown(const char *inv_id, float base, int step);
#define COMBO_WINDOW 0.9f // s para encadenar el siguiente golpe

// Resuelve un movimiento de att contra def. weapon: el arma usada en los golpes.
MeleeResult melee_resolve(MeleeMove m, const Fighter *att, const Fighter *def, const char *weapon, int combo_step,
                          Rng *rng);

// Parry. El rival anuncia el golpe durante MELEE_WINDUP s (levanta el arma); el parry gana si
// llega cuando al golpe le quedan PARRY_WINDOW s o menos. A destiempo, el que para queda
// expuesto PARRY_EXPOSED s (ni se cubre ni ataca).
#define MELEE_WINDUP 0.4f
#define PARRY_WINDOW 0.25f
#define PARRY_EXPOSED 0.6f

typedef enum {
    PARRY_HOOK,    // hacha, guja, alabarda: engancha el escudo y se lo arranca
    PARRY_GRAPPLE, // manos o daga: le toma el brazo armado, lo desarma y lo tumba
    PARRY_DEFLECT, // las demas: desvia el golpe y el rival queda abierto
} ParryKind;

ParryKind melee_parry_kind(const char *weapon);
// El parry llega a tiempo: al golpe del rival le quedan windup_left s (0: no hay golpe en camino).
bool melee_parry_in_window(float windup_left);
// Un parry a tiempo de me contra foe (el que golpeaba): el resultado es para foe. Un agarre que
// no entra deja expuesto al que para (attacker_staggered).
MeleeResult melee_parry(ParryKind k, const Fighter *me, const Fighter *foe, Rng *rng);

// Manos: pasar el arma a la otra mano (no con escudo ni a dos manos).
bool hands_swap(Hands *h);
// El arma con la que se golpea en este paso del combo (con dos armas, alterna).
const char *hands_attack_weapon(const Hands *h, int combo_step, bool *off_hand);

#define KNOCKDOWN_SECONDS 2.5f
#define STAGGER_SECONDS 1.1f

#endif
