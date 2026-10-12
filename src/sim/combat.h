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
    // Las armas de la estepa (fase 7):
    bool pierce;       // pico: contra la armadura entra como una punta (el sagaris)
    bool crush;        // cabeza pesada: abate mas y el casco para la mitad (la maza)
    bool stun;         // el golpe que entra aturde (el martillo del sagaris)
    bool clean_cut;    // corte limpio a caballo: no se traba y suma inercia (el sable)
    bool mounted_only; // solo sirve a caballo (la lanza pesada)
    bool ring;         // daga de anillo: sin castigo en la mano torpe; mejor en llaves, rehenes y ejecuciones
    bool brittle;      // mal templada (fase 7b): pega un poco mas, pero puede quebrarse
} WeaponStats;

// Estadisticas del arma por id del inventario ("" o NULL: a puño limpio).
WeaponStats weapon_stats(const char *inv_id);
// A caballo, cuanto multiplica el golpe la inercia de la carrera (m/s): el sable (corte limpio)
// la aprovecha entera y algo mas, las de asta entera, las demas la mitad.
float weapon_mounted_scale(const char *inv_id, float speed);
// Probabilidad de que un golpe al galope se trabe en el rival (se pierde el ritmo): las hachas,
// las espadas, las dagas y las mazas; el sable y las de asta, nunca.
float weapon_snag_chance(const char *inv_id, float speed);
#define SNAG_SECONDS 0.9f // s de mas hasta el proximo golpe si se traba
// Un arma fragil (mal templada, src/sim/forge.h) se quiebra a veces en un golpe que entra, y mas
// si se lo paran. Las demas, nunca.
float weapon_break_chance(const char *inv_id, bool blocked);

// Crecen al final (se guardan como numeros, docs/PARTIDAS.md).
typedef enum {
    ENEMY_BANDIT,
    ENEMY_FANATIC,
    ENEMY_CAPTOR,
    ENEMY_ARCHER,
    ENEMY_RIDER,
    ENEMY_HORSE_ARCHER, // arquero a caballo: hostiga, rodea y se aleja tirando (src/sim/riding.h)
    ENEMY_COUNT
} EnemyKind;

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
// Un golpe cuerpo a cuerpo con los rasgos de su arma: con pierce (el pico) la armadura lo para como
// a una punta (peor la malla); con crush (la maza), en la cabeza pasa la mitad de lo que pararia el
// casco. Sin rasgos, es combat_apply_hit.
int combat_apply_strike(Health *h, Armor *a, Rng *rng, float damage, WoundKind kind, int part, bool pierce, bool crush,
                        float *absorbed, bool *broke);

#endif
