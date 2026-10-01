// Grandes guerreros: personajes unicos que aparecen por azar.
//
// No hay un numero fijo: cada encuentro tiene una probabilidad baja de traer
// uno. Su historia es una mezcla azarosa de cuatro elementos (origen,
// circunstancia del exilio, modo de vida actual y aspiracion) y sus dones de
// combate tambien se sortean: mayor talla, rapidez, fuerza, aguante, un arma
// especial, talento para sanar, punteria o monta.
//
// La aspiracion define su rasgo de tropa: quien suena con un clan propio es
// ambicioso (y candidato a encabezar una rebelion), quien busca venganza es
// sanguinario, etc. Asi la historia pesa en las dinamicas de moral.
#ifndef ESTEPA_CHAMPION_H
#define ESTEPA_CHAMPION_H

#include <stdbool.h>
#include <stddef.h>

#include "rng.h"

#define CHAMPION_NAME_LEN 32
#define CHAMPION_DEFAULT_CHANCE 0.04f // probabilidad por encuentro: escasos

typedef enum {
    GIFT_TALL = 1 << 0,     // mayor talla
    GIFT_SWIFT = 1 << 1,    // mas rapido
    GIFT_STRONG = 1 << 2,   // mas fuerte
    GIFT_ENDURING = 1 << 3, // mas aguante
    GIFT_WEAPON = 1 << 4,   // arma especial
    GIFT_HEALER = 1 << 5,   // talento para sanar
    GIFT_MARKSMAN = 1 << 6, // punteria
    GIFT_RIDER = 1 << 7,    // jinete excepcional
    GIFT_COUNT = 8
} ChampionGift;

// Multiplicadores sobre un guerrero comun (1.0). healing: 0 = no sana.
typedef struct {
    float size, speed, strength, endurance, aim, riding, healing;
} CombatStats;

typedef struct {
    char name[CHAMPION_NAME_LEN];
    char epithet[CHAMPION_NAME_LEN];
    int origin, exile, livelihood, aspiration; // indices en las tablas de historia
    unsigned gifts;                            // ChampionGift combinables
    int weapon;                                // arma especial, o -1
    CombatStats stats;
    unsigned traits; // rasgos de tropa (Trait de troop.h)
} Champion;

// Tira el azar de un encuentro: true si aparece un gran guerrero.
bool champion_appears(Rng *rng, float chance);

// Genera un gran guerrero al azar (1 a 3 dones, historia completa).
void champion_generate(Champion *c, Rng *rng);

// Historia en una frase (UTF-8): "Nació ... . <Exilio>. Hoy ..., y sueña con ... ."
// Devuelve la longitud que tendria completa (como snprintf).
int champion_story(const Champion *c, char *buf, size_t len);

const char *gift_name(ChampionGift g);
const char *champion_weapon_name(int weapon);
int champion_gift_count(const Champion *c);

#endif
