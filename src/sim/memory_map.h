// Mapa de memoria: lo que el personaje recuerda del mundo.
//
// El mapa empieza negro. Cada celda se "ilumina" poco a poco mientras el
// jugador la recorre, y se apaga con el paso de los dias (olvido). Las zonas
// que se recorren con frecuencia ganan familiaridad: brillan mas y se olvidan
// mas despacio, como la memoria espacial real (repeticion espaciada).
//
// Cubre el mundo entero: las celdas se guardan en paginas que solo se crean
// al visitar una zona (tabla hash), asi que la memoria crece con lo explorado,
// no con el tamano del mundo. El olvido se calcula de forma perezosa al leer
// o visitar una celda: el coste no depende de cuanto se haya explorado.
#ifndef ESTEPA_MEMORY_MAP_H
#define ESTEPA_MEMORY_MAP_H

#include <stdbool.h>

#define MEMMAP_CELL_SIZE 4.0f // metros por celda
#define MEMMAP_PAGE 32        // celdas por lado de pagina (128 m)
#define MEMMAP_MAX_MARKERS 32

typedef struct {
    float light;       // brillo recordado en t_last, [0, 1]
    float familiarity; // minutos acumulados en la zona (ponderados); no se olvida
    float t_last;      // instante (s de juego) de la ultima actualizacion de light
} MemoryCell;

typedef struct {
    int px, pz; // coordenadas de la pagina
    MemoryCell cells[MEMMAP_PAGE * MEMMAP_PAGE];
} MemoryPage;

typedef enum { MARKER_INTEREST = 0, MARKER_DANGER, MARKER_CAMP } MarkerKind;

typedef struct {
    float x, z;
    MarkerKind kind;
} MapMarker;

// Parametros de balance (editables).
typedef struct {
    float sight_radius; // m: radio que se memoriza alrededor del jugador
    float gain;         // 1/s: velocidad con la que una zona se ilumina
    float forget_days;  // dias de juego: constante de olvido de una zona sin familiaridad
    float base_cap;     // brillo maximo de una zona vista una sola vez
} MemoryParams;

typedef struct {
    MemoryPage **slots; // tabla hash con sondeo lineal
    int capacity, page_count;
    MapMarker markers[MEMMAP_MAX_MARKERS];
    int marker_count;
    MemoryParams params;
} MemoryMap;

MemoryParams memmap_default_params(void);
void memmap_init(MemoryMap *m);
void memmap_free(MemoryMap *m);
// Guardado: recorrer las paginas (i en [0, capacity), NULL si el hueco esta vacio) y
// restaurar una pagina leida (la crea si no existe).
const MemoryPage *memmap_page_slot(const MemoryMap *m, int i);
bool memmap_restore_page(MemoryMap *m, const MemoryPage *page);

// El jugador esta en (x, z) durante dt segundos; now = tiempo de juego (s).
void memmap_visit(MemoryMap *m, float x, float z, float dt, float now);

// Revela de golpe un circulo (p. ej., lo que se ve desde una torre de vigilancia)
// como si se hubiera recorrido durante `seconds`.
void memmap_reveal(MemoryMap *m, float x, float z, float radius, float seconds, float now);

// Brillo recordado en (x, z) en el instante now, [0, 1]. Lo no visitado: 0.
float memmap_light(const MemoryMap *m, float x, float z, float now);
float memmap_familiarity(const MemoryMap *m, float x, float z);

// Marca un sitio. Si ya hay una marca a menos de radius metros, la quita
// (alternar). Devuelve true si la marca queda puesta.
bool memmap_toggle_marker(MemoryMap *m, float x, float z, MarkerKind kind, float radius);
// Pone una marca sin quitar ninguna (lo que traen los exploradores, src/sim/camps.h): si ya hay
// una a menos de radius metros, deja esa. Devuelve el indice de la marca, o -1 si no hay hueco.
int memmap_add_marker(MemoryMap *m, float x, float z, MarkerKind kind, float radius);

#endif
