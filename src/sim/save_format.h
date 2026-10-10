// Formato de la partida guardada por bloques (docs/PARTIDAS.md).
//
// La partida es una lista de bloques {etiqueta de 4 letras, version, tamaño, datos}, uno por
// modulo. Cada bloque lleva, ademas de sus datos, el esquema de su struct: el nombre, el tipo, la
// posicion y el tamaño de cada campo (src/game/save_schema.inc, generado por
// tools/save/esquema.py). Al leer, si el struct cambio desde que se guardo (campos agregados,
// quitados o movidos, arreglos mas grandes o mas chicos, un int que paso a float), los campos se
// copian por nombre: lo nuevo queda con su valor por defecto y lo que ya no existe se ignora.
// La version del bloque es para lo que el esquema no resuelve solo (otro significado, otras
// unidades, un valor por defecto que no es cero): el modulo migra los datos viejos.
//
// C puro, sin raylib: se testea en tests/test_main.c.
#ifndef ESTEPA_SAVE_FORMAT_H
#define ESTEPA_SAVE_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    SF_INT,    // entero con signo (int, enums, signed char...)
    SF_UINT,   // entero sin signo (unsigned, uint32_t, uint64_t...)
    SF_FLOAT,  // float o double
    SF_BOOL,
    SF_TEXT,   // char[]: texto terminado en cero
    SF_BYTES,  // opaco (Vector3, Color...): se copia tal cual si el tipo es el mismo
    SF_STRUCT, // otro struct, con su esquema
    SF_KIND_COUNT
} SfKind;

typedef struct SfType SfType;

typedef struct {
    const char *name;  // nombre del campo
    const char *type;  // su tipo (float, Role, Vector3, Health...)
    SfKind kind;
    unsigned offset;   // bytes desde el comienzo del struct
    unsigned size;     // bytes de un elemento
    unsigned count;    // elementos (1 si no es un arreglo)
    const SfType *sub; // SF_STRUCT: el esquema del elemento
} SfField;

struct SfType {
    const char *name;
    unsigned size; // sizeof
    unsigned nfields;
    const SfField *fields;
};

// Para escribir esquemas: un campo, un arreglo (de una o dos dimensiones) y el tipo.
#define SF_ONE(T, f, ty, kind, sub) { #f, ty, kind, (unsigned)offsetof(T, f), (unsigned)sizeof(((T *)0)->f), 1u, sub }
#define SF_ARR(T, f, ty, kind, sub)                                                                    \
    { #f, ty, kind, (unsigned)offsetof(T, f), (unsigned)sizeof(((T *)0)->f[0]),                        \
      (unsigned)(sizeof(((T *)0)->f) / sizeof(((T *)0)->f[0])), sub }
#define SF_ARR2(T, f, ty, kind, sub)                                                                   \
    { #f, ty, kind, (unsigned)offsetof(T, f), (unsigned)sizeof(((T *)0)->f[0][0]),                     \
      (unsigned)(sizeof(((T *)0)->f) / sizeof(((T *)0)->f[0][0])), sub }
#define SF_TYPE(T, fields) { #T, (unsigned)sizeof(T), (unsigned)(sizeof(fields) / sizeof((fields)[0])), fields }

// ---------------------------------------------------------------- escritura
// Los bloques van uno tras otro en un archivo abierto en binario.
typedef struct {
    FILE *f;
    bool ok;            // false si algo fallo al escribir
    long start;         // donde empiezan los datos del bloque abierto (-1: ninguno)
    long count_at;      // donde va la cantidad de elementos
    uint32_t count;
    const SfType *type;
    unsigned char *buf; // un elemento empaquetado (sin relleno ni punteros)
} SfWriter;

void sf_write_begin(SfWriter *w, FILE *f);
// Un bloque de elementos de un tipo: abrir, agregar los elementos y cerrar.
void sf_block_begin(SfWriter *w, const char *tag, uint32_t version, const SfType *type);
void sf_put(SfWriter *w, const void *elem);
void sf_block_end(SfWriter *w);
// Atajo: un bloque con n elementos contiguos.
void sf_block(SfWriter *w, const char *tag, uint32_t version, const SfType *type, const void *elems, int n);
// La marca de fin. false si algo fallo al escribir.
bool sf_write_end(SfWriter *w);

// ---------------------------------------------------------------- lectura
typedef struct {
    char tag[5]; // con el cero al final
    uint32_t version;
    uint32_t size;
    const unsigned char *data;
} SfBlock;

#define SF_MAX_BLOCKS 64

typedef struct {
    int count;
    SfBlock blocks[SF_MAX_BLOCKS];
} SfFile;

// Indexa los bloques de buf (lo que sigue al encabezado de la partida). false si esta dañado:
// un bloque cortado o sin la marca de fin.
bool sf_parse(SfFile *file, const void *buf, size_t len);
const SfBlock *sf_find(const SfFile *file, const char *tag); // NULL si no esta
// Cuantos elementos trae el bloque, o -1 si esta dañado.
int sf_count(const SfBlock *b);
// Lee hasta max elementos del bloque en dst, un arreglo del tipo live. Campo por campo y por
// nombre: lo que no esta en el bloque queda como estaba en dst (los valores por defecto) y lo que
// ya no existe se ignora. Devuelve cuantos elementos trae el bloque (aunque sean mas que max), o
// -1 si esta dañado (y entonces dst queda sin tocar).
int sf_read(const SfBlock *b, const SfType *live, void *dst, int max);

// Convierte n elementos guardados con el esquema old al esquema live (como sf_read, pero los
// datos y el esquema viejo vienen de otro lado: las partidas planas de antes de los bloques).
void sf_convert(const SfType *old, const void *src, int n, const SfType *live, void *dst);
// Los dos esquemas describen la misma memoria (los datos se pueden copiar tal cual).
bool sf_same(const SfType *a, const SfType *b);

// Pruebas: convierte campo por campo aunque los esquemas sean iguales.
void sf_set_slow(bool slow);
// Vuelca los esquemas como codigo C con numeros fijos: un esquema congelado, para seguir leyendo
// archivos de un formato que ya no se escribe. Los nombres llevan el prefijo (p. ej. "V1").
void sf_dump_c(FILE *out, const SfType *const *roots, int n, const char *prefix);

#endif
