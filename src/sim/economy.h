// Economia del campamento: acopio de materiales, recoleccion y comida diaria,
// efectos de las construcciones y forja en los hornos. C puro, testeable.
//
// Todo se cuenta por ids del inventario de assets (assets/inventario.tsv).
#ifndef ESTEPA_ECONOMY_H
#define ESTEPA_ECONOMY_H

#include <stdbool.h>

#include "inventory.h"
#include "troop.h"

// ---------------------------------------------------------------- acopio
#define STOCK_MAX 64
#define MAT_MAX 4

typedef struct {
    char id[INV_ID_LEN];
    int count;
} StockEntry;

typedef struct {
    StockEntry e[STOCK_MAX];
    int n;
} Stockpile;

// Cantidad de un material para una receta. Las listas terminan en { NULL, 0 }.
typedef struct {
    const char *id;
    int count;
} Ingredient;

void stock_init(Stockpile *s);
int stock_count(const Stockpile *s, const char *id);
bool stock_add(Stockpile *s, const char *id, int count);
// Todo o nada: false (y sin cambios) si falta algo.
bool stock_take(Stockpile *s, const char *id, int count);
bool stock_has_all(const Stockpile *s, const Ingredient *mats);
bool stock_take_all(Stockpile *s, const Ingredient *mats);
// Primer material que falta (para avisar al jugador), o NULL.
const Ingredient *stock_first_missing(const Stockpile *s, const Ingredient *mats);
// Acopio inicial de la tribu.
void stock_seed_camp(Stockpile *s);

// ---------------------------------------------------------------- dia a dia
#define FOOD_ID "utileria.consumible.carne_seca"

typedef struct {
    int gathered; // unidades recolectadas hoy
    int eaten;    // raciones comidas
    int hungry;   // integrantes sin racion
} UpkeepReport;

// Recoleccion por funcion y comida: cada integrante activo come una racion;
// un cocinero alarga la comida (cada 3 bocas, una racion menos). Si falta
// comida, la moral de todos cae.
UpkeepReport economy_daily_upkeep(Stockpile *s, Troop *t);

// ---------------------------------------------------------------- efectos de las construcciones
typedef struct {
    float morale_per_day;  // animo que suma el campamento cada dia
    float rebellion_scale; // multiplica el riesgo de rebelion (totem: menos)
    float reveal_radius;   // m: torre de vigilancia, lo que la tribu ve alrededor
    int shelters;          // techos para dormir
} CampEffects;

// ids: lo construido en el campamento.
CampEffects camp_effects(const char *const *ids, int n);

// ---------------------------------------------------------------- forja
typedef enum {
    CRAFT_SABLE_BRONZE,
    CRAFT_SPEAR_BRONZE,
    CRAFT_HELMET_BRONZE,
    CRAFT_SABLE_STEEL,
    CRAFT_SCALE_ARMOR_STEEL,
    CRAFT_SHIELD_IRON,
    CRAFT_COUNT
} CraftId;

typedef struct {
    const char *name;     // UTF-8
    const char *produces; // id de inventario
    const char *building; // horno necesario (id de inventario)
    Role role;            // quien forja
    Ingredient mats[MAT_MAX];
    float work;           // segundos de juego para un artesano
} CraftDef;

const CraftDef *craft_def(CraftId c);
// Segundos que tarda con los artesanos activos de la tribu (0 artesanos: no se puede, devuelve -1).
float craft_seconds(const CraftDef *c, const Troop *t);

#endif
