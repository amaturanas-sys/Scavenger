// Ropa y clima (C puro, sin raylib):
//  - prendas por capas (cabeza, cuello, cuerpo, capa o abrigo, pies) que abrigan, dan
//    sombra contra el sol, frenan la lluvia y pesan;
//  - las pieles de depredadores (lobo, oso, tigre, puma, hiena) dan escarmiento: el
//    enemigo se lo piensa (se espanta o huye antes);
//  - el sol: el desierto quema de dia y hiela de noche; la sombra (sombrero, tunica de
//    seda blanca, pañuelo del desierto) lo corta;
//  - calor: como el frio de src/sim/hazards.h, pero al reves (agotamiento y golpe de calor);
//  - los NPCs eligen su ropa segun el tiempo con garment_pick.
#ifndef ESTEPA_APPAREL_H
#define ESTEPA_APPAREL_H

#include <stdbool.h>

typedef enum { WEAR_HEAD, WEAR_FACE, WEAR_BODY, WEAR_CLOAK, WEAR_FEET, WEAR_COUNT } WearSlot;

typedef struct {
    const char *id; // del inventario (vestimenta.*)
    WearSlot slot;
    float warmth;   // grados de sensacion termica que suma
    float shade;    // [0, 1] cuanto corta el sol
    float rain;     // [0, 1] cuanto frena el mojarse
    float weight;   // fraccion de velocidad que quita
    float dread;    // escarmiento [0, 0.35]: pieles de depredador
    unsigned char r, g, b; // color (para dibujarla)
} GarmentDef;

// Lo que lleva puesto: indice de prenda por capa, o -1.
typedef struct {
    signed char g[WEAR_COUNT];
} Outfit;

int garment_count(void);
const GarmentDef *garment(int i);  // NULL si no existe
int garment_find(const char *inv_id); // -1 si no es ropa
const char *wear_slot_name(WearSlot s); // UTF-8

void outfit_clear(Outfit *o);
// Pone la prenda en su capa; devuelve la que habia (-1 si nada).
int outfit_wear(Outfit *o, int garment_idx);
float outfit_warmth(const Outfit *o);
float outfit_shade(const Outfit *o);  // 1 - prod(1 - sombra)
float outfit_rain(const Outfit *o);   // 1 - prod(1 - lluvia)
float outfit_dread(const Outfit *o);  // suma, hasta 0.5
float outfit_speed_scale(const Outfit *o);

// La piel con nombre que deja un animal al despiezarlo (utileria.piel.*), o NULL si solo
// da pieles curtidas comunes. species: SpeciesId de src/sim/animals.h.
const char *species_pelt(int species);

// ------------------------------------------------------------------- sol
// Fuerza del sol [0, 1]: alto, sin nubes; en el desierto pega mas.
float sun_strength(float sun_height, float clouds, float desert);
// Temperatura del lugar: el desierto suma de dia (hasta +11 a mediodia) y resta de noche (hasta -7).
float local_temperature(float temperature, float sun_height, float desert);
// Sensacion termica con la ropa: temperatura + abrigo + fuego - viento - mojado + sol sin sombra.
float apparel_feels_like(float temperature, float wind, float wet, float sun, const Outfit *o, float extra, float fire);
// Cuanto se moja por la lluvia con esa ropa (multiplica la lluvia).
float apparel_wet_scale(const Outfit *o);

// ----------------------------------------------------------------- calor
#define HEAT_COMFORT 27.0f // sensacion termica desde la que el cuerpo se recalienta
#define HEAT_MAX 100.0f

typedef enum { HEAT_FINE, HEAT_WARM, HEAT_HOT, HEAT_EXHAUSTED, HEAT_STROKE } HeatLevel;

typedef struct {
    float load; // calor acumulado [0, 100]: 100 es golpe de calor
} HeatStress;

// wet: estar mojado refresca. shade_here: a la sombra (tienda, arbol, agua).
void heat_update(HeatStress *h, float feels_like, float wet, bool shade_here, float dt);
HeatLevel heat_level(const HeatStress *h);
const char *heat_name(HeatLevel l); // UTF-8
float heat_speed_scale(const HeatStress *h); // 1 bien, 0.65 con golpe de calor
float heat_thirst_scale(const HeatStress *h); // cuanto mas rapido da sed (1 a 2.5)

// ------------------------------------------------------------------ NPCs
// Abrigo que le falta a alguien sin ropa para estar a gusto (negativo: le sobra calor).
float comfort_need(float temperature, float wind, float sun);
// La mejor prenda para la capa s entre los candidatos (indices de prenda); need: el abrigo
// que todavia falta. Devuelve el indice en cands, o -1 si mejor nada.
int garment_pick(WearSlot s, float need, float sun, const int *cands, int n);
// Sensacion termica de un NPC con su ropa (sin fuego ni mojado).
float npc_feels_like(float temperature, float wind, float sun, const Outfit *o);

#endif
