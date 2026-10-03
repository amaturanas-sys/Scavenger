// Combate cuerpo a cuerpo (C puro): armas, enemigos y reglas de cada golpe.
// Las heridas que deja cada golpe las lleva src/sim/health.h.
#ifndef ESTEPA_COMBAT_H
#define ESTEPA_COMBAT_H

#include <stdbool.h>

#include "health.h"
#include "rng.h"

typedef struct {
    float damage;   // daño base por golpe
    float reach;    // alcance (m)
    float cooldown; // segundos entre golpes
    WoundKind wound;
    bool spear;     // arma de asta: estocada
} WeaponStats;

// Estadisticas del arma por id del inventario ("" o NULL: a puño limpio).
WeaponStats weapon_stats(const char *inv_id);

typedef enum { ENEMY_BANDIT, ENEMY_FANATIC, ENEMY_CAPTOR, ENEMY_WOLF, ENEMY_COUNT } EnemyKind;

typedef struct {
    const char *name;  // UTF-8
    const char *model; // id del inventario
    bool beast;        // fiera (cuadrupedo) o persona
    float hp, damage, reach, cooldown, speed, sight;
    WoundKind wound;
    float flee_at; // huye con menos de esta fraccion de vida (0: nunca)
} EnemyDef;

const EnemyDef *enemy_def(EnemyKind k);

// Probabilidad de parar un golpe con escudo segun de donde viene
// (facing: coseno entre el frente del defensor y la direccion al atacante).
float combat_block_chance(bool shield, float facing);
// Daño final de un golpe: base x fuerza del atacante x estado de sus brazos, con
// una variacion de +-20 %. Si se para con el escudo, queda el 15 %.
float combat_damage(float base, float strength, float attack_scale, bool blocked, Rng *rng);

#endif
