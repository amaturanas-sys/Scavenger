// Capacidades permanentes del jugador (C puro):
//  - nivel y experiencia: se gana al pelear, cazar, fabricar, construir y explorar;
//  - habilidades activas (con duracion y espera) que dan tatuajes y joyas encantadas;
//  - el arbol de tatuajes: cinco motivos de la estepa (lobo, ciervo, grifo, tamga,
//    olas), cada uno una progresion de tres grados; el tercero se bifurca en dos
//    habilidades especificas que se excluyen. Un druida tatua una zona del cuerpo:
//    el motivo da el bonus y la zona cambia a que atributo va y cuanto pesa.
//
// Regla de diseno: un tatuaje no se puede quitar ni cambiar de zona. Por eso esta
// API no tiene ninguna funcion para retirarlo: cada decision es para siempre.
#ifndef ESTEPA_TALENTS_H
#define ESTEPA_TALENTS_H

#include <stdbool.h>

#include "loadout.h"

// ---------------------------------------------------------------- nivel
#define LEVEL_MAX 20

typedef struct {
    int level; // 1..LEVEL_MAX
    float xp;  // dentro del nivel actual
} Progress;

typedef enum { XP_KILL, XP_HUNT, XP_CRAFT, XP_BUILD, XP_DISCOVER, XP_TAME, XP_SOURCE_COUNT } XpSource;

void progress_init(Progress *p);
float xp_to_next(int level);         // la experiencia que pide el nivel siguiente
float xp_reward(XpSource s);
int progress_add(Progress *p, float xp); // devuelve cuantos niveles subio

// ---------------------------------------------------------------- habilidades activas
typedef enum {
    ABIL_NONE,
    ABIL_HOWL,    // aullido: furia en el cuerpo a cuerpo
    ABIL_GALLOP,  // galope: carrera
    ABIL_EAGLE,   // vuelo del grifo: punteria y vista
    ABIL_WAR_CRY, // grito del clan: la escolta pelea mejor
    ABIL_TIDE,    // marea: sana de golpe
    ABIL_SHADOW,  // sombra: casi invisible un rato
    ABIL_WARMTH,  // calor del ambar: devuelve el calor del cuerpo
    ABIL_STAUNCH, // sangre de granate: corta las hemorragias
    ABIL_COUNT
} AbilityId;

typedef struct {
    const char *name, *desc; // N_(): se muestran con T()
    float duration, cooldown; // s
    float mods[STAT_COUNT];   // mientras dura (por potencia 1)
    float heal;               // fraccion de la vida que devuelve al usarla
    bool staunch;             // venda las heridas que sangran
    float warmth;             // calor del cuerpo que devuelve [0, 1]
    bool allies;              // tambien alcanza a la escolta cercana
} AbilityDef;

const AbilityDef *ability_def(AbilityId a);

// ---------------------------------------------------------------- tatuajes
typedef enum { TZ_HEAD, TZ_NECK, TZ_CHEST, TZ_BACK, TZ_ARM_L, TZ_ARM_R, TZ_HANDS, TZ_LEGS, TZ_COUNT } TattooZone;
typedef enum { MOTIF_WOLF, MOTIF_DEER, MOTIF_GRIFFIN, MOTIF_TAMGA, MOTIF_WAVES, MOTIF_COUNT } Motif;

#define TATTOO_NODES (MOTIF_COUNT * 4)
#define TATTOO_NONE (-1)

typedef struct {
    const char *name, *desc; // N_()
    Motif motif;
    int tier;       // 1, 2 o 3
    int branch;     // 0 en los grados 1 y 2; 1 o 2 en el tercero (se excluyen)
    int level;      // nivel minimo del jugador
    Stat stat[2];   // lo que da (pasivo)
    float value[2];
    AbilityId active; // o ABIL_NONE
} TattooNode;

typedef struct {
    signed char node[TZ_COUNT]; // nodo tatuado en cada zona, o TATTOO_NONE
} TattooBody;

typedef enum {
    TAT_OK,
    TAT_ZONE_TAKEN,  // esa zona ya tiene un tatuaje (para siempre)
    TAT_HAVE_IT,     // ese tatuaje ya lo llevas
    TAT_LEVEL,       // nivel insuficiente
    TAT_NEEDS_PREV,  // falta el grado anterior del motivo
    TAT_OTHER_BRANCH // ya elegiste la otra rama del motivo
} TattooCheck;

const TattooNode *tattoo_node(int i);
const char *zone_name(TattooZone z);   // T()
const char *motif_name(Motif m);       // T()
const char *motif_item(Motif m);       // id del inventario (accesorio.tatuaje.*)
const char *tattoo_check_text(TattooCheck c); // T()

void tattoo_body_init(TattooBody *b);
bool tattoo_has(const TattooBody *b, int node);
int tattoo_count(const TattooBody *b);
TattooCheck tattoo_can(const TattooBody *b, int node, TattooZone zone, int level);
// Permanente: no hay vuelta atras.
TattooCheck tattoo_apply(TattooBody *b, int node, TattooZone zone, int level);
// Cuanto pesa un atributo en una zona: 1.5 si la zona lo favorece, 0.75 si no le va, 1 si no.
float zone_weight(TattooZone z, Stat s);
Stat zone_bonus_stat(TattooZone z); // lo que añade cualquier tatuaje en esa zona (+5 %)
// El efecto de un nodo hecho en una zona: suma en mods (STAT_COUNT); devuelve su habilidad (o ABIL_NONE)
// y en potency (si no es NULL) la potencia de la habilidad en esa zona.
AbilityId tattoo_effect(int node, TattooZone zone, float *mods, float *potency);
// Suma de todos los tatuajes del cuerpo para un atributo.
float tattoo_stat(const TattooBody *b, Stat s);
// "vista +22 %, carisma +5 %": lo que daria el nodo en la zona (para elegir antes de tatuar).
int tattoo_describe(int node, TattooZone zone, char *out, int len);
// Lo que pide el druida por un tatuaje del grado tier.
typedef struct {
    const char *id;
    int count;
} TattooInk;
const TattooInk *tattoo_ink(int tier); // terminado en { NULL, 0 }

#endif
