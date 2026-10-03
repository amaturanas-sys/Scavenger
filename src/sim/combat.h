// Combate cuerpo a cuerpo (C puro): armas, enemigos y reglas de cada golpe.
// Las heridas que deja cada golpe las lleva src/sim/health.h.
#ifndef ESTEPA_COMBAT_H
#define ESTEPA_COMBAT_H

#include <stdbool.h>

#include "armor.h"
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

typedef enum { ENEMY_BANDIT, ENEMY_FANATIC, ENEMY_CAPTOR, ENEMY_ARCHER, ENEMY_RIDER, ENEMY_COUNT } EnemyKind;

typedef struct {
    const char *name;  // UTF-8
    const char *model; // id del inventario
    bool beast;        // fiera (las fieras viven en src/sim/animals.h; hoy, siempre false)
    float hp, damage, reach, cooldown, speed, sight;
    WoundKind wound;
    float flee_at; // huye con menos de esta fraccion de vida (0: nunca)
    const char *ranged;    // arma a distancia (src/sim/ballistics.h), o NULL
    const char *armor[4];  // piezas de armadura que lleva (ids del inventario)
    const char *weapon;    // arma de mano (src/sim/melee.h la usa para golpes, combos y ganchos)
    const char *shield;    // escudo que puede llevar (o NULL)
    float shield_chance;   // probabilidad de llevarlo
    bool mounted;          // a caballo: rapido, pega desde arriba, cuesta tumbarlo (y si cae, pierde el caballo)
} EnemyDef;

const EnemyDef *enemy_def(EnemyKind k);

// Botin: lo que deja un enemigo al caer (ademas de las piezas de su armadura, que pone el juego).
typedef struct {
    const char *id;
    int count;
    float condition; // estado de una pieza de equipo [0, 1]
} LootItem;

// armed: aun tiene su arma; shield: aun tiene su escudo. Devuelve cuantos objetos escribio.
int enemy_loot(EnemyKind k, bool armed, bool shield, Rng *rng, LootItem *out, int max);
// Bono de un golpe a caballo segun la velocidad (m/s): 1 parado, hasta 1.8 al galope.
float mounted_momentum(float speed);

// Probabilidad de parar un golpe con escudo segun de donde viene
// (facing: coseno entre el frente del defensor y la direccion al atacante).
float combat_block_chance(bool shield, float facing);
// Daño final de un golpe: base x fuerza del atacante x estado de sus brazos, con
// una variacion de +-20 %. Si se para con el escudo, queda el 15 %.
float combat_damage(float base, float strength, float attack_scale, bool blocked, Rng *rng);

// Un impacto completo: elige la zona (si part = PART_RANDOM), pasa por la armadura
// (a puede ser NULL) y hiere. Devuelve el indice de la herida o -1 (parado del todo).
// absorbed: daño que paro la armadura; broke: si una pieza se rompio.
int combat_apply_hit(Health *h, Armor *a, Rng *rng, float damage, WoundKind kind, int part, bool projectile,
                     float *absorbed, bool *broke);

#endif
