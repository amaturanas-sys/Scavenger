// Lo que se lleva y donde (C puro): bolsillos, mochila, alforjas de una montura,
// la carreta y el acopio del campamento. Cada contenedor tiene un peso maximo,
// un numero de huecos y una talla maxima de objeto (en los bolsillos solo entra
// lo pequeño). Las piezas de equipo (armas, escudos, armaduras) guardan su estado
// (gastadas o rotas) y no se apilan con las nuevas.
#ifndef ESTEPA_STORAGE_H
#define ESTEPA_STORAGE_H

#include <stdbool.h>

#include "inventory.h"

typedef enum { BAG_POCKETS, BAG_BACKPACK, BAG_MOUNT, BAG_CART, BAG_CAMP, BAG_KIND_COUNT } BagKind;

#define BAG_SLOTS 32

typedef struct {
    char id[INV_ID_LEN];
    int count;
    float condition; // [0, 1]: estado de la pieza (1 nueva)
    unsigned short var; // variante: piedra y encantamiento de una joya (src/sim/jewelry.h); 0 = ninguna
} BagSlot;

typedef struct {
    BagKind kind;
    float cap_kg;   // peso maximo
    float max_size; // m: la medida mayor de lo que cabe
    int slots;      // huecos (<= BAG_SLOTS)
    BagSlot s[BAG_SLOTS];
    int n;
} Bag;

// Mochilas de tres tamaños: mas grandes, mas caben, pero frenan (aun vacias).
typedef enum { PACK_NONE, PACK_SMALL, PACK_MEDIUM, PACK_LARGE, PACK_COUNT } PackSize;
void bag_init_pack(Bag *b, PackSize s);      // conserva lo que habia si cabe (no: hay que vaciarla antes)
float pack_speed(PackSize s);                // 1, 0.97, 0.9
PackSize pack_size_of(const char *inv_id);   // "utileria.mochila.grande" -> PACK_LARGE (o PACK_NONE)
const char *pack_id(PackSize s);
const char *pack_name(PackSize s);           // T()

// Peso aproximado de un objeto (kg), por su tipo, sus medidas y su material.
float item_kg(const InvItem *it);
// ¿Se puede llevar en un contenedor? (no las construcciones ni los vehiculos)
bool item_storable(const InvItem *it);
// Pieza de equipo: guarda su estado y no se apila.
bool item_is_gear(const char *id);
// Lo que cabe segun el tipo: bolsillos (2 kg, lo pequeño), mochila (25 kg),
// alforjas (segun la montura: cap_kg), carreta (400 kg), acopio (sin limite practico).
void bag_init(Bag *b, BagKind kind, float cap_kg);
const char *bag_kind_name(BagKind k); // UTF-8
float bag_kg(const Bag *b, const Inventory *inv);
int bag_count(const Bag *b, const char *id);
// Mete hasta n (las que quepan); devuelve cuantas. condition: estado de una pieza de equipo.
int bag_add(Bag *b, const Inventory *inv, const char *id, int n, float condition);
// Igual, con variante (no se apila con otras variantes del mismo id).
int bag_add_var(Bag *b, const Inventory *inv, const char *id, int n, float condition, unsigned short var);
// Saca hasta n; devuelve cuantas. condition (si no es NULL): el estado de la ultima sacada.
int bag_take(Bag *b, const char *id, int n, float *condition);
// Quita n del hueco slot (lo vacia si llega a 0).
void bag_remove_slot(Bag *b, int slot, int n);
// Mueve hasta n del hueco slot de from a to; devuelve cuantas se movieron.
int bag_move_slot(Bag *from, int slot, Bag *to, const Inventory *inv, int n);
// Cuanto frena la carga: 1 hasta el limite, luego baja (hasta 0.5).
float bag_speed_scale(float carried_kg, float limit_kg);
// Alforjas segun la especie de la montura (kg): mula 80, camello 120, elefante 200...
float mount_capacity_kg(int species);

#endif
