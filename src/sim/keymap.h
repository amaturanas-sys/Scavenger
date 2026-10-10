// Mapa de teclas del juego (C puro): cada accion con su tecla y sus modificadores, en un solo
// lugar (el mapa de la fase 1 del plan grande, docs/PLAN_GRAN_ACTUALIZACION.md). El juego
// pregunta por acciones (input_action_pressed en src/game/input.c), no por teclas, y los tests
// comprueban que cada accion responde a su tecla y no a la vieja.
//
// Los codigos de tecla son los de raylib (las letras, su ASCII en mayuscula), copiados aqui para
// que el nucleo no dependa de raylib. Los numeros (barra rapida), las flechas y los menus no
// pasan por aqui.
#ifndef ESTEPA_KEYMAP_H
#define ESTEPA_KEYMAP_H

#include <stdbool.h>

#define KM_KEY_SPACE 32
#define KM_KEY_TAB 258

// Modificadores.
enum { KM_SHIFT = 1, KM_CTRL = 2, KM_ALT = 4 };

typedef enum {
    // Los cuatro botones de combate.
    KA_ATTACK, // H (y clic izquierdo): golpe, combo, mantener: pesado; a distancia, tensar y disparar
    KA_BLOCK,  // J (y clic derecho sin arrastrar), mantener: cubrirse con el escudo o el arma
    KA_PARRY,  // K: parry o contra en la ventana justa
    KA_CHARGE, // L: carga con escudo, o patada
    KA_TARGET, // Tab en combate: el enemigo siguiente (Mayus+Tab: el anterior)
    // Moverse.
    KA_SNEAK,  // C: sigilo
    KA_CROUCH, // X: agacharse
    KA_JUMP,   // Espacio
    // El mundo.
    KA_INTERACT,    // F: hablar, tomar, botin, animales, pozo, horno (Mayus+F: tomar todo o sacrificar)
    KA_GRAB,        // G: agarrar (la fase 2); por ahora, la mochila
    KA_BACKPACK,    // Mayus+G: dejar o recoger la mochila
    KA_LIGHT_ARROW, // Mayus+L: encender la flecha junto a un fuego
    KA_THROW,       // T: lanzar lo que se lleva en brazos
    KA_MOUNT,       // R: montar y desmontar
    KA_BANDAGE,     // B: vendar
    KA_DRINK,       // N: beber (Mayus+N: con hierbas)
    KA_ESCORT,      // Y: escolta (Mayus+Y: despachar)
    KA_MARK,        // M: marcar el mapa (Mayus+M: peligro)
    KA_MENU,        // Tab fuera de combate: acciones, obras, fabricar, reparar
    KA_INVENTORY,   // I
    KA_EQUIPMENT,   // P
    KA_CARD,        // U: la ficha del gran guerrero
    KA_COUNT
} KeyAction;

typedef struct {
    int key;         // codigo de raylib
    unsigned need;   // modificadores que hacen falta
    unsigned forbid; // modificadores que la anulan (Ctrl+H, Ctrl+P... son otras cosas)
} KeyBind;

const KeyBind *keymap_bind(KeyAction a);
// Pulsar key con los modificadores mods (KM_*) dispara la accion a.
bool keymap_matches(KeyAction a, int key, unsigned mods);
// La tecla para los textos: "H", "Mayús+G", "Tab" (UTF-8, en el idioma de la interfaz).
const char *keymap_text(KeyAction a);

#endif
