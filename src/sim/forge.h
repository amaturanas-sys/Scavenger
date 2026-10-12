// La forja (C puro): la fase 7b del plan grande (docs/PLAN_GRAN_ACTUALIZACION.md).
//  - El hierro: vetas a cielo abierto en el altiplano y hierro de pantano en la orilla de los lagos
//    del bosque y de los fiordos. Salen de la semilla del mundo sin tocar su generacion (con su
//    propia racha de azar): los exploradores las marcan en el mapa (src/sim/camps.h) y alli se pica
//    el mineral, que se funde en el horno de acero.
//  - La forja movil (fuelle de cuero y hogar de carbon, en la mochila, las alforjas, la carreta o el acopio):
//    repara lo que pide un horno y hace puntas de flecha, cuchillos y sables con hojas compradas en
//    cualquier sitio, tambien en la migracion. Los hornos y el taller sirven para lo mismo.
//  - El taller del herrero: una obra de campamento grande (WORKSHOP_MIN_PEOPLE). En el se templa,
//    se forja el acero wootz y se hacen puntas de lanza de acero.
//  - El temple: un paso con riesgo. El arma sale templada (mas daño) o fragil (puede quebrarse,
//    src/sim/combat.h); la probabilidad depende de los herreros de la tribu y de las manos del clan.
#ifndef ESTEPA_FORGE_H
#define ESTEPA_FORGE_H

#include <stdbool.h>

#include "world.h"

#define FORGE_MOBILE_ID "utileria.herramienta.forja_movil"
#define WORKSHOP_ID "estructura.campamento.taller_herrero"
#define IRON_ORE_ID "utileria.material.mineral_hierro"
#define WORKSHOP_MIN_PEOPLE 8 // un campamento grande: gente que vive en el
#define IRON_SITES_MAX 32
#define IRON_NEAR 6.0f        // m: a esta distancia se pica la veta

typedef enum { IRON_VEIN, IRON_BOG, IRON_KINDS } IronKind;

typedef struct {
    float x, z;
    IronKind kind;
} IronSite;

// Las vetas de un mundo: siempre las mismas para su semilla. Devuelve cuantas escribio en out.
int world_iron_sites(const World *w, IronSite *out, int max);
// La veta a menos de IRON_NEAR de (x, z) (la mas cercana), o -1.
int iron_site_near(const IronSite *s, int n, float x, float z);
// Mineral que da una tanda de trabajo: la veta a cielo abierto, 2; el pantano, 1.
int iron_ore_yield(IronKind k);

// ¿Alcanza esa estacion para lo que pide building (de una receta, o de una reparacion si repair)?
// mobile: la forja movil a mano; bronze, steel: los hornos; workshop: el taller. La forja movil
// sirve para lo suyo y para reparar lo que pide un horno; los hornos y el taller, para lo de ella.
bool forge_station_ok(const char *building, bool mobile, bool bronze, bool steel, bool workshop, bool repair);

// El temple sale bien con esta probabilidad: smiths, herreros de la tribu (cuentan hasta 2); skill,
// las manos del clan [0, 1] (tatuajes y joyas). Entre 0,3 y 0,95.
float temper_chance(int smiths, float skill);

#endif
