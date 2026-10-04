// Tropa guerrillera: integrantes, funciones delegadas, prisioneros, moral y
// politica con el reino del que el jugador es tributario.
//
// Eje de diseno: lo que el jugador hace en su campamento mueve la moral de la
// tropa (riesgo de desercion y rebelion) y la relacion con su reino.
// Todo es logica pura (sin raylib) para poder testearla sin pantalla.
#ifndef ESTEPA_TROOP_H
#define ESTEPA_TROOP_H

#include <stdbool.h>
#include <stdint.h>

#include "champion.h"
#include "armor.h"
#include "storage.h"
#include "health.h"
#include "rng.h"

#define TROOP_MAX 64
#define NAME_LEN 32

typedef enum {
    STATUS_ACTIVE,
    STATUS_PRISONER,
    STATUS_BANISHED,
    STATUS_EXECUTED,
    STATUS_DESERTED,
    STATUS_DEAD, // murio (accidente, frio, combate)
} MemberStatus;

// Funciones que el lider puede delegar en sus subordinados.
typedef enum {
    ROLE_NONE,
    ROLE_SCOUT,      // explorador
    ROLE_HUNTER,     // cazador
    ROLE_COOK,       // cocinero
    ROLE_SMITH,      // herrero
    ROLE_GUARD,      // guardia del campamento
    ROLE_HEALER,     // curandero
    ROLE_LIEUTENANT, // lugarteniente
    ROLE_BUILDER,    // constructor: acelera las obras en grupo
    ROLE_SOLDIER,    // soldado: escolta y defensa
    ROLE_DRUID,      // druida: tatua y encanta joyas
    ROLE_GOLDSMITH,  // orfebre: hace joyas con metal y piedras
    ROLE_HERDER,     // pastor: cuida y alimenta el ganado
    ROLE_COUNT
} Role;

// Rasgos (combinables como banderas): modulan como cada integrante vive
// las decisiones del lider.
typedef enum {
    TRAIT_MERCIFUL = 1 << 0,     // compasivo: rechaza las ejecuciones
    TRAIT_BLOODTHIRSTY = 1 << 1, // sanguinario: las celebra
    TRAIT_AMBITIOUS = 1 << 2,    // ambicioso: candidato a liderar una rebelion
    TRAIT_DEVOUT = 1 << 3,       // devoto: valora el trato a los cautivos
    TRAIT_LOYALIST = 1 << 4,     // leal a sus camaradas
} Trait;

// Acciones del campamento con consecuencias politicas.
typedef enum {
    ACT_RECRUIT,
    ACT_TAKE_PRISONER,
    ACT_RELEASE_PRISONER,
    ACT_BANISH,
    ACT_EXECUTE_MEMBER,
    ACT_EXECUTE_PRISONER,
    ACT_SHARE_LOOT,
    ACT_COUNT
} CampAction;

typedef struct {
    int id;
    char name[NAME_LEN];
    Role role;
    MemberStatus status;
    unsigned traits;
    float morale;  // 0..100
    float loyalty; // 0..100, hacia el lider
    int champion;  // indice en Troop.champions si es un gran guerrero, o -1
    Health health; // vida, sangre y heridas (src/sim/health.h)
    Armor armor;   // piezas de armadura que lleva (src/sim/armor.h)
    bool outfitted; // ya recibio su equipo inicial
    int camp;       // campamento donde vive (src/sim/camps.h), o -1 si va con el jugador sin hogar
    int pack;       // su mochila (PackSize de src/sim/storage.h): mas grande, mas carga y mas lento
    Bag bag;        // lo que lleva en ella
    uint64_t known; // sitios que conoce (src/sim/travel.h: campamentos y marcas del mapa)
    int journey;    // viaje en curso + 1 (0: ninguno); mientras, no esta en ningun lado a la vista
} Member;

// Reino al que la tropa rinde tributo. `stance` dice cuanto sube o baja la
// relacion con cada accion: un reino de mano dura aplaude las ejecuciones,
// uno piadoso las castiga.
typedef struct {
    char name[NAME_LEN];
    float relation; // -100..100
    float stance[ACT_COUNT];
} Kingdom;

typedef struct {
    Member members[TROOP_MAX];
    int count;
    int next_id;
    Kingdom *overlord; // puede ser NULL (tropa independiente)
    Champion champions[TROOP_MAX];
    int champion_count;
    float rebellion_scale; // efectos del campamento (p. ej. el totem de proteccion la reduce); 1 por defecto
} Troop;

typedef struct {
    int deserted;        // cuantos desertaron hoy
    int champions_deserted; // cuantos de ellos eran grandes guerreros
    bool rebellion;      // estallo una rebelion
    int rebellion_leader; // id del instigador, o -1
} DayReport;

void troop_init(Troop *t, Kingdom *overlord);

// Devuelven el id del nuevo integrante, o -1 si la tropa esta llena.
int troop_recruit(Troop *t, const char *name, unsigned traits);
int troop_take_prisoner(Troop *t, const char *name, unsigned traits);

// Suma un gran guerrero como integrante (STATUS_ACTIVE) o como prisionero
// (STATUS_PRISONER). Despues sigue las mismas dinamicas que cualquier
// integrante (moral, desercion, rebelion, destierro, ejecucion), pero su
// desercion pesa en el animo de todos y tiende a encabezar las rebeliones.
int troop_add_champion(Troop *t, const Champion *c, MemberStatus status);
// Datos del gran guerrero con ese id de integrante, o NULL si no lo es.
const Champion *troop_champion(const Troop *t, int id);

Member *troop_find(Troop *t, int id);

// Solo los integrantes activos pueden recibir funciones.
bool troop_assign_role(Troop *t, int id, Role role);
bool troop_banish(Troop *t, int id);
bool troop_execute(Troop *t, int id); // integrante activo o prisionero
bool troop_release_prisoner(Troop *t, int id);
// Un integrante activo muere (accidente, frio...): sale de la tropa y el resto
// lo llora (baja la moral de todos). Devuelve false si no estaba activo.
bool troop_mourn(Troop *t, int id, float morale_loss);
void troop_share_loot(Troop *t);
// Suma (o resta) animo a todos los integrantes activos (comida, comodidades del campamento...).
void troop_adjust_morale(Troop *t, float delta);
// Habilidad de curar de la tropa [0, 1]: curandero (0.5) o el mejor gran guerrero sanador.
float troop_healer_skill(const Troop *t);

int troop_count_with_status(const Troop *t, MemberStatus status);
float troop_avg_morale(const Troop *t);
float troop_avg_loyalty(const Troop *t);

// Probabilidades diarias, en [0, 1].
float troop_desertion_chance(const Member *m);
float troop_rebellion_chance(const Troop *t);

// Avanza un dia: tira deserciones y rebelion con el RNG dado.
DayReport troop_process_day(Troop *t, Rng *rng);

// Reinos de ejemplo con valores opuestos.
void kingdom_init_iron_khanate(Kingdom *k);  // mano dura
void kingdom_init_jade_dynasty(Kingdom *k);  // piadoso y ordenado

const char *role_name(Role r);
const char *status_name(MemberStatus s);

#endif
