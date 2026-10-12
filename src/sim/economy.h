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
#define MAT_MAX 5 // ingredientes por receta, con el { NULL, 0 } que cierra la lista

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
#define FRESH_MEAT_ID "utileria.consumible.carne_fresca" // caza y sacrificio
#define MILK_ID "utileria.consumible.leche"              // ordeño
#define GRAIN_SACK_ID "utileria.objeto.saco_grano"        // de los campos saqueados o el mercado
#define GRAIN_SACK_RATIONS 10                             // raciones que da un saco de grano
#define HIDE_ID "utileria.material.pieles"

typedef struct {
    int gathered; // unidades recolectadas hoy
    int eaten;    // raciones comidas
    int hungry;   // integrantes sin racion
} UpkeepReport;

// Recoleccion por funcion y comida: cada integrante activo come una racion
// (carne fresca y leche antes que carne seca; lo fresco que sobra se seca o se pierde);
// un cocinero alarga la comida (cada 3 bocas, una racion menos). Si falta
// comida, la moral de todos cae.
UpkeepReport economy_daily_upkeep(Stockpile *s, Troop *t);

// ---------------------------------------------------------------- el trabajo de cada oficio
#define WORK_OUTPUTS_MAX 8
typedef struct {
    const char *id; // lo que hicieron
    int count;
} WorkOutput;

typedef struct {
    WorkOutput made[WORK_OUTPUTS_MAX];
    int n;
    int batches; // tandas: una por persona que trabajo
} WorkReport;

// Cada integrante con oficio hace una tanda por dia con el acopio de su campamento, si alcanza
// sin tocar la reserva: el cazador curte pieles (o hace puntas de hueso), el pastor abatana la
// lana en fieltro, el constructor labra troncos (o hace adobe), el cocinero hace velas, el
// curandero unguento y el orfebre peines de hueso.
WorkReport economy_daily_work(Stockpile *s, const Troop *t);

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
    // A mano, en cualquier sitio, con lo que se recolecta.
    CRAFT_ARROWS,
    CRAFT_BOLTS,
    CRAFT_SLING_STONES,
    CRAFT_ROPE,
    CRAFT_OINTMENT,
    CRAFT_LEATHER_CUIRASS,
    CRAFT_FELT_BOOTS,
    CRAFT_BRONZE_INGOT, // el bronce de las joyas
    // Ropa (src/sim/apparel.h): a mano, con pieles, lana y seda.
    CRAFT_FUR_HAT,
    CRAFT_FELT_HAT,
    CRAFT_SUN_HAT,
    CRAFT_WOLF_HAT,
    CRAFT_DESERT_SCARF,
    CRAFT_FUR_SCARF,
    CRAFT_SILK_TUNIC,
    CRAFT_WOOL_TUNIC,
    CRAFT_WINTER_DEEL,
    CRAFT_WOOL_CLOAK,
    CRAFT_WOLF_CLOAK,
    CRAFT_BEAR_COAT,
    CRAFT_TIGER_COAT,
    CRAFT_PUMA_CLOAK,
    CRAFT_HYENA_CLOAK,
    CRAFT_REINDEER_COAT,
    CRAFT_GOAT_COAT,
    CRAFT_WHITE_MANTLE,
    CRAFT_FUR_BOOTS,
    CRAFT_SANDALS,
    // Bebidas (src/sim/water.h): el alcohol purifica el agua.
    CRAFT_WATERED_WINE,
    CRAFT_AIRAG,
    CRAFT_BEER,
    // Las cadenas de produccion (fase 5): talabarteria, hueso y asta, grasa, leche, fieltro, madera y adobe.
    CRAFT_LEATHER,
    CRAFT_SADDLE,
    CRAFT_BRIDLE,
    CRAFT_SADDLEBAGS,
    CRAFT_QUIVER,
    CRAFT_LEATHER_HELMET,
    CRAFT_LEATHER_BRACERS,
    CRAFT_LEATHER_GREAVES,
    CRAFT_BONE_POINTS,
    CRAFT_BONE_ARROWS,
    CRAFT_COMB,
    CRAFT_BOW_PARTS,
    CRAFT_COMPOSITE_BOW,
    CRAFT_CANDLES,
    CRAFT_TORCH,
    CRAFT_CHEESE,
    CRAFT_FELT,
    CRAFT_WORKED_WOOD,
    CRAFT_ADOBE,
    // Las armas de la estepa (fase 7). El arco compuesto reflejo se hace en tres etapas: el alma de
    // madera, el asta y el tendon encolados (secan BOW_DRY_DAYS dias en el acopio) y la cola de
    // hueso que lo termina (CRAFT_COMPOSITE_BOW).
    CRAFT_SIMPLE_BOW,
    CRAFT_BONE_GLUE,
    CRAFT_BOW_CORE,
    CRAFT_BOW_GLUED,
    CRAFT_AKINAKES,
    CRAFT_SAGARIS,
    CRAFT_SAGARIS_HAMMER,
    CRAFT_MACE,
    CRAFT_RING_DAGGER,
    CRAFT_HEAVY_LANCE,
    // La forja (fase 7b, src/sim/forge.h): fundir el mineral, la forja movil y lo que se hace en
    // ella, y el taller (el wootz, las puntas de lanza de acero y el temple).
    CRAFT_SMELT_IRON,
    CRAFT_MOBILE_FORGE,
    CRAFT_IRON_POINTS,
    CRAFT_IRON_ARROWS,
    CRAFT_KNIFE,
    CRAFT_SABLE_FROM_BLADE,
    CRAFT_SABLE_WOOTZ,
    CRAFT_SPEAR_STEEL,
    CRAFT_TEMPER_SABLE,
    CRAFT_TEMPER_WOOTZ,
    CRAFT_TEMPER_DAGGER,
    CRAFT_COUNT
} CraftId;

typedef struct {
    const char *name;     // UTF-8
    const char *produces; // id de inventario
    const char *building; // horno necesario (id de inventario), o NULL: a mano, en cualquier sitio
    Role role;            // quien lo hace (ROLE_NONE: el propio jugador)
    Ingredient mats[MAT_MAX];
    float work;           // segundos de juego para un artesano
    int amount;           // cuantos salen (0 = 1)
    bool to_stock;        // lo hecho se cuelga en el acopio del campamento (a secar), no va a la mochila
    const char *fails;    // el temple (src/sim/forge.h): lo que sale si sale mal (fragil), o NULL
} CraftDef;

const CraftDef *craft_def(CraftId c);
// Segundos que tarda con los artesanos activos de la tribu (0 artesanos: no se puede, devuelve -1).
// Lo que se hace a mano (ROLE_NONE) lo hace el jugador: siempre se puede.
float craft_seconds(const CraftDef *c, const Troop *t);
bool craft_by_hand(const CraftDef *c);

// El secado: lo que se deja en el acopio de un campamento avanza una etapa cada dia (el arco
// encolado tarda BOW_DRY_DAYS en secarse). Devuelve cuantas piezas cambiaron.
#define BOW_DRY_DAYS 2
int stock_dry_day(Stockpile *s);

// ---------------------------------------------------------------- reparaciones
// Lo que cuesta reparar una pieza de armadura segun su material (src/sim/armor.h):
// fieltro y cuero, a mano con pieles; bronce, hierro, acero y oro, en el horno y con herrero.
typedef struct {
    Ingredient mats[MAT_MAX];
    const char *building; // horno necesario, o NULL
    Role role;            // artesano necesario (ROLE_NONE: cualquiera)
    float restore;        // cuanto devuelve (fraccion de la durabilidad)
} RepairDef;

const RepairDef *repair_def(int material);
// Untar con grasa: el fieltro y el cuero se mantienen con grasa cuando falta con que repararlos
// (devuelve menos). NULL para los demas materiales.
const RepairDef *repair_grease(int material);

#endif
