// Agua y bebidas (C puro, sin raylib):
//  - la sed baja con el tiempo, mas con calor (src/sim/apparel.h) y enfermo;
//  - el agua que corre por los suelos (rios, lagos, charcos) puede traer espiritus
//    malditos: se incuban un rato y luego dan fiebre (frena, cansa, da mas sed);
//  - hervir el agua los elimina; el alcohol tambien: la cerveza, el vino, el airag y el
//    agua con vino son seguros;
//  - beber alcohol en exceso emborracha: torpe (golpes y punteria) y fatigado;
//  - las hierbas curan antes. El jugador no muere: con la sed a cero se desmaya.
#ifndef ESTEPA_WATER_H
#define ESTEPA_WATER_H

#include <stdbool.h>

#include "economy.h"
#include "rng.h"
#include "troop.h"

typedef enum {
    DRINK_RAW,          // agua cruda (del rio o del lago)
    DRINK_BOILED,       // agua hervida
    DRINK_WATERED_WINE, // agua con vino
    DRINK_MILK,         // leche
    DRINK_AIRAG,        // leche de yegua fermentada (poco alcohol)
    DRINK_BEER,         // cerveza
    DRINK_WINE,         // vino
    DRINK_COUNT
} DrinkKind;

typedef struct {
    const char *id; // del inventario
    float water;    // cuanta sed quita (de 100)
    float alcohol;  // cuanto emborracha
    bool raw;       // puede traer espiritus malditos
} DrinkDef;

const DrinkDef *drink_def(DrinkKind k);
int drink_find(const char *inv_id); // DrinkKind, o -1 si no se bebe
// Probabilidad de que el agua cruda traiga espiritus: mas con calor y en agua quieta.
float water_spirit_chance(float temp_mean, bool still);

#define THIRST_MAX 100.0f

typedef enum { THIRST_FINE, THIRST_THIRSTY, THIRST_PARCHED, THIRST_DRY } ThirstLevel;
typedef enum { DRUNK_SOBER, DRUNK_MERRY, DRUNK_DRUNK, DRUNK_WASTED } DrunkLevel;

typedef struct {
    float water;    // [0, 100]: 100 sin sed
    float drunk;    // alcohol en el cuerpo [0, 2]
    float curse;    // espiritus malditos [0, 1]: lo graves que son
    float incubate; // segundos hasta que se manifiestan (> 0: incubando, aun sin sintomas)
} Hydration;

void hydration_init(Hydration *h);
// thirst_scale: el calor da mas sed (heat_thirst_scale). resting: quieto o durmiendo.
void hydration_update(Hydration *h, float thirst_scale, bool resting, float dt);
// Bebe un trago. spirit_chance: la del agua cruda (water_spirit_chance). Devuelve true si
// ese trago trajo espiritus malditos (aun no se notan: se incuban).
bool hydration_drink(Hydration *h, DrinkKind k, float spirit_chance, Rng *rng);
// Las hierbas: cortan los espiritus a la mitad (y los que se incuban, casi del todo).
void hydration_herbs(Hydration *h);
bool hydration_sick(const Hydration *h); // con sintomas
ThirstLevel thirst_level(const Hydration *h);
DrunkLevel drunk_level(const Hydration *h);
const char *thirst_name(ThirstLevel l); // UTF-8
const char *drunk_name(DrunkLevel l);   // UTF-8
// Efectos: velocidad, aguante (recuperar el aliento) y torpeza [0, 1] (golpes y punteria).
float hydration_speed_scale(const Hydration *h);
float hydration_stamina_scale(const Hydration *h);
float hydration_clumsy(const Hydration *h);

// ------------------------------------------------------------------ la tribu
typedef struct {
    int safe;   // bebieron algo seguro del acopio (hervida, bebidas o leche)
    int boiled; // hirvieron agua del rio para ellos (con leña)
    int raw;    // bebieron agua cruda del rio
    int sick;   // les cayeron los espiritus malditos
    int dry;    // sin agua: pasan sed
} WaterReport;

#define WATER_WOOD_PER 4 // una leña hierve el agua de cuatro

// El agua del dia de un campamento: cada integrante activo bebe una racion del acopio (lo
// seguro primero). Si falta y hay agua cerca, se saca del rio: hervida mientras haya leña,
// cruda si no (cada uno se arriesga a los espiritus: fiebre, baja la vida y el animo). Sin
// agua, sed: baja el animo y la vida. Nadie muere de esto (la vida no baja del 30 %).
WaterReport water_daily(Stockpile *s, Troop *t, bool water_near, float spirit_chance, Rng *rng);

// ----------------------------------------------------------------- animales
// La sed de un animal de la tribu sube con el tiempo (THIRST_ANIMAL_RATE por segundo);
// junto al agua bebe solo. Con mucha sed (> 1) adelgaza; con > 2 se va.
#define THIRST_ANIMAL_RATE (1.0f / 360.0f)

#endif
