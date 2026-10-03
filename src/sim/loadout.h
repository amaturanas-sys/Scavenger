// Amuletos (intercambiables) y tatuajes (permanentes) que otorgan buffs
// activos y pasivos, y desbloquean nodos del arbol de habilidades.
//
// Regla de diseno: un tatuaje no se puede quitar. Por eso esta API no
// ofrece ninguna funcion para retirarlo.
#ifndef ESTEPA_LOADOUT_H
#define ESTEPA_LOADOUT_H

#include <stdbool.h>

#define CHARM_ID_LEN 24
#define CHARM_MAX_BUFFS 4
#define AMULET_SLOTS 3
#define TATTOO_MAX 8

typedef enum {
    STAT_STAMINA_REGEN,
    STAT_GRAPPLE_POWER, // fuerza en el grappling
    STAT_STEALTH,       // sigilo al acechar/esconderse
    STAT_ARCHERY,       // precision a distancia
    STAT_CHARISMA,      // reclutar y sostener la moral
    STAT_RIDING,        // control de monturas
    STAT_COUNT
} Stat;

typedef enum { BUFF_PASSIVE, BUFF_ACTIVE } BuffKind;

typedef struct {
    char id[CHARM_ID_LEN];
    BuffKind kind;
    float cooldown_s;      // solo para activos
    float mods[STAT_COUNT];
} Buff;

// Un amuleto o un tatuaje: un portador de buffs.
typedef struct {
    char id[CHARM_ID_LEN];
    Buff buffs[CHARM_MAX_BUFFS];
    int buff_count;
} Charm;

typedef struct {
    Charm amulets[AMULET_SLOTS];
    bool amulet_equipped[AMULET_SLOTS];
    Charm tattoos[TATTOO_MAX];
    int tattoo_count;
} Loadout;

// Nodo del arbol: se desbloquea si algun amuleto equipado o tatuaje
// coincide con `requires_charm`.
typedef struct {
    char id[CHARM_ID_LEN];
    char requires_charm[CHARM_ID_LEN];
} SkillNode;

void loadout_init(Loadout *l);

bool loadout_equip_amulet(Loadout *l, int slot, const Charm *amulet);
bool loadout_unequip_amulet(Loadout *l, int slot);

// Permanente. Falla si no hay espacio o si ese tatuaje ya esta.
bool loadout_apply_tattoo(Loadout *l, const Charm *tattoo);

// Suma de modificadores pasivos de amuletos equipados y tatuajes.
float loadout_stat(const Loadout *l, Stat stat);

// Copia en `out` los buffs activos disponibles; devuelve cuantos.
int loadout_active_buffs(const Loadout *l, const Buff **out, int max);

bool skill_unlocked(const Loadout *l, const SkillNode *node);

// Los amuletos del inventario (accesorio.amuleto.*): sus buffs pasivos.
// false si el id no es un amuleto conocido.
bool amulet_charm(const char *inv_id, Charm *out);
// "sigilo +30 %, agarre +10 %" (UTF-8)
int charm_describe(const Charm *c, char *out, int len);
const char *stat_name(Stat s); // UTF-8

// Ayudante para armar charms en codigo y tests.
Charm charm_make(const char *id);
bool charm_add_buff(Charm *c, const char *buff_id, BuffKind kind, Stat stat, float value);

#endif
