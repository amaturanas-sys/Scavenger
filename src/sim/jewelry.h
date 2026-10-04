// Joyas (amuletos) que dan capacidades cambiables, a diferencia de los tatuajes (C puro):
//  - piezas: anillos (dos por mano), brazaletes (uno por muñeca), collar, aretes y
//    hebilla de cinturon; nueve huecos en el cuerpo;
//  - las hace un orfebre con metal (bronce, plata u oro: la potencia) y una piedra
//    preciosa (turquesa, cornalina, lapislazuli, ambar, jade, granate, perla, coral:
//    el tipo de efecto);
//  - sin encantar no hacen nada: un druida las encanta con un efecto pasivo
//    (un atributo) o activo (una habilidad con espera), segun la piedra;
//  - las piedras salen de romper rocas, de los corales del lago, de excavar y de
//    cribar en los rios.
// Las variantes (piedra y encantamiento) viajan en BagSlot.var (src/sim/storage.h).
#ifndef ESTEPA_JEWELRY_H
#define ESTEPA_JEWELRY_H

#include <stdbool.h>

#include "inventory.h"
#include "rng.h"
#include "talents.h"

typedef enum { JT_RING, JT_BRACELET, JT_NECKLACE, JT_EARRINGS, JT_BUCKLE, JT_COUNT } JewelType;
typedef enum { METAL_BRONZE, METAL_SILVER, METAL_GOLD, METAL_COUNT } JewelMetal;
typedef enum {
    GEM_NONE,
    GEM_TURQUOISE,
    GEM_CARNELIAN,
    GEM_LAPIS,
    GEM_AMBER,
    GEM_JADE,
    GEM_GARNET,
    GEM_PEARL,
    GEM_CORAL,
    GEM_COUNT
} Gem;
typedef enum { ENCH_NONE, ENCH_PASSIVE, ENCH_ACTIVE } Enchant;

typedef enum {
    JS_RING_L1,
    JS_RING_L2,
    JS_RING_R1,
    JS_RING_R2,
    JS_BRACELET_L,
    JS_BRACELET_R,
    JS_NECKLACE,
    JS_EARRINGS,
    JS_BUCKLE,
    JS_COUNT
} JewelSlot;

typedef struct {
    char id[INV_ID_LEN]; // "" = hueco vacio
    unsigned short var;  // piedra y encantamiento (jewel_var)
} WornJewel;

typedef struct {
    WornJewel slot[JS_COUNT];
} Jewelry;

// Variante: piedra (4 bits) y encantamiento (2 bits).
unsigned short jewel_var(Gem g, Enchant e);
Gem jewel_gem(unsigned short var);
Enchant jewel_enchant(unsigned short var);

// Ids del inventario: "accesorio.anillo.oro", "utileria.gema.turquesa"...
bool jewel_parse(const char *id, JewelType *type, JewelMetal *metal); // false si no es una joya
const char *jewel_id(JewelType t, JewelMetal m);
const char *gem_id(Gem g);
Gem gem_from_id(const char *id);
const char *metal_id(JewelMetal m); // el lingote o el metal del acopio
const char *gem_name(Gem g);        // T()
const char *jewel_type_name(JewelType t);
const char *jewel_slot_name(JewelSlot s);
JewelType slot_type(JewelSlot s);
// ¿Esa pieza va en ese hueco? (los amuletos de animales van en el collar)
bool jewel_fits(const char *id, JewelSlot s);

// Lo que pide el orfebre: metal y piedras.
int jewel_metal_cost(JewelType t);
int jewel_gem_cost(JewelType t);
float jewel_potency(JewelType t, JewelMetal m); // tipo x metal

// El efecto de una pieza encantada (mods: STAT_COUNT); devuelve su habilidad activa (o ABIL_NONE).
// Una pieza sin encantar no da nada. Los amuletos de animales (accesorio.amuleto.*) ya vienen encantados.
AbilityId jewel_effect(const char *id, unsigned short var, float *mods, float *potency);
// Lo que da cada encantamiento de una piedra (para elegir en el druida).
Stat gem_passive_stat(Gem g);
float gem_passive_value(Gem g);
AbilityId gem_active(Gem g);
float jewelry_stat(const Jewelry *j, Stat s);
int jewel_describe(const char *id, unsigned short var, char *out, int len);

// ---------------------------------------------------------------- piedras en el mundo
typedef enum { GEMSRC_ROCK, GEMSRC_CORAL, GEMSRC_DIG, GEMSRC_RIVER, GEMSRC_COUNT } GemSource;
// La piedra que sale (o GEM_NONE si no sale ninguna) segun de donde.
Gem gem_roll(GemSource src, Rng *rng);

#endif
