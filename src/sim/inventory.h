// Inventario de assets (assets/inventario.tsv): la lista de todos los modelos
// que necesita el juego, con su id, medidas y estado de produccion.
//
// Cada id "categoria.subcategoria.nombre" tiene una ruta fija:
//   assets/models/categoria/subcategoria/nombre.glb   (modelos)
//   assets/textures/categoria/subcategoria/nombre.png (formato:textura)
// Mientras el archivo no exista, el juego dibuja un marcador provisional con
// las medidas del inventario; al importar el modelo, lo reemplaza solo.
//
// Parser en C puro (sin raylib) para poder testearlo.
#ifndef ESTEPA_INVENTORY_H
#define ESTEPA_INVENTORY_H

#include <stdbool.h>
#include <stddef.h>

#define INV_ID_LEN 72
#define INV_CATEGORY_LEN 24

typedef enum { INV_PENDIENTE, INV_KILN, INV_IMPORTADO, INV_REFINADO, INV_ESTADO_COUNT } InvState;

// Como se empuna (etiqueta manos:una|dos|escudo). INV_HANDS_NONE: no se empuna.
typedef enum { INV_HANDS_NONE, INV_HANDS_ONE, INV_HANDS_TWO, INV_HANDS_SHIELD } InvHands;

typedef struct {
    char id[INV_ID_LEN];
    char name[96]; // UTF-8
    char category[INV_CATEGORY_LEN];
    float w, h, l; // medidas en metros: ancho (Z), alto (Y), largo (X, hacia el frente)
    int tris_max;
    InvState state;
    bool texture; // formato:textura (calcomania o textura, no modelo)
    InvHands hands;
} InvItem;

typedef struct {
    InvItem *items;
    int count;
} Inventory;

// Lee el TSV (texto completo). Ignora comentarios (#), la cabecera y lineas
// mal formadas. Devuelve la cantidad de objetos leidos.
int inventory_parse(Inventory *inv, const char *text);
void inventory_free(Inventory *inv);

const InvItem *inventory_find(const Inventory *inv, const char *id);

// Ruta relativa del archivo del objeto. Devuelve la longitud (como snprintf).
int inventory_path(const InvItem *item, char *buf, size_t len);

const char *inventory_state_name(InvState s);

#endif
