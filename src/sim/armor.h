// Armadura por piezas (C puro). Cada pieza cubre unas zonas del cuerpo
// (src/sim/health.h) y reduce el daño que llega a ellas segun su material:
// el fieltro amortigua, el cuero laminar y el bronce cortan el filo, el hierro
// y el acero aguantan mucho mas. Cada impacto gasta su durabilidad (mas rapido
// en materiales blandos); rota, ya no protege. Las piezas pesan: frenan.
#ifndef ESTEPA_ARMOR_H
#define ESTEPA_ARMOR_H

#include <stdbool.h>

#include "health.h"
#include "inventory.h"
#include "rng.h"

typedef enum { MAT_FELT, MAT_LEATHER, MAT_BRONZE, MAT_IRON, MAT_STEEL, MAT_GOLD, MAT_COUNT } ArmorMaterial;

typedef struct {
    const char *name;  // UTF-8
    float vs_cut;      // fraccion del daño que para contra filo y mordida
    float vs_blunt;    // contra golpe
    float vs_pierce;   // contra proyectiles
    float durability;  // durabilidad de una pieza nueva
    float hardness;    // resistencia al desgaste por impacto
    float weight;      // cuanto frena (fraccion de la velocidad)
} MaterialDef;

const MaterialDef *material_def(ArmorMaterial m);

// Ranuras: una pieza por ranura.
typedef enum {
    SLOT_HELMET,    // casco: cabeza
    SLOT_NECK,      // gorjal: cuello
    SLOT_TORSO,     // coraza: torax y abdomen
    SLOT_SHOULDERS, // hombreras: brazos
    SLOT_BRACERS,   // brazales: antebrazos
    SLOT_GLOVES,    // guanteletes: antebrazos (parcial)
    SLOT_SKIRT,     // faldar: pelvis y muslos (parcial)
    SLOT_GREAVES,   // grebas: piernas
    SLOT_BOOTS,     // botas: piernas (parcial)
    SLOT_COUNT
} ArmorSlot;

typedef struct {
    char id[INV_ID_LEN]; // "" = ranura vacia
    ArmorMaterial material;
    unsigned zones;   // banderas 1 << BodyPart
    float coverage;   // probabilidad de que un impacto en sus zonas de con la pieza
    bool mail;        // malla: buena contra el filo, peor contra la punta
    float durability, durability_max;
} ArmorPiece;

typedef struct {
    ArmorPiece slot[SLOT_COUNT];
} Armor;

// Equipa una pieza por id del inventario ("armadura.<tipo>.<material>"). false si no es armadura.
bool armor_equip(Armor *a, const char *inv_id);
void armor_unequip(Armor *a, ArmorSlot s);
// Hueco de una pieza por su id ("armadura.casco.*" -> SLOT_HELMET), o -1.
int armor_slot_for(const char *inv_id);
// Lo que queda de un impacto en una zona tras la armadura. kind puede cambiar:
// un corte que la armadura para casi entero llega como golpe. broke: si alguna pieza se rompio.
float armor_absorb(Armor *a, BodyPart part, WoundKind *kind, bool projectile, float damage, Rng *rng, bool *broke);
// Proteccion contra filo en una zona [0, 1] (para la interfaz).
float armor_protection(const Armor *a, BodyPart part);
float armor_speed_scale(const Armor *a);
// Repara un poco cada pieza (herrero): fraccion de la durabilidad maxima.
void armor_repair(Armor *a, float fraction);
const char *slot_name(ArmorSlot s); // UTF-8

#endif
