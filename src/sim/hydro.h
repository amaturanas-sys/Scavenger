// La hidrologia (C puro, sin raylib): el agua baja, se junta y arma rios y lagos. Es la fase 1 del agua
// (docs/AGUA.md): src/sim/world.c la usa para trazar los rios y los lagos de cada semilla.
//
// Sobre una grilla de celdas cuadradas, con la altura del terreno sin agua en el centro de cada una:
//  1. Priority-Flood (Barnes, Lehman y Mulla, 2014; esta en RichDEM, github.com/r-barnes/richdem): desde
//     las salidas (el mar, el canal, un sumidero o afuera del mundo) se recorre el terreno de lo mas bajo a
//     lo mas alto. Cada depresion queda llena hasta su punto de desborde (el nivel de su lago) y cada celda
//     sabe a que vecina corre su agua: cuesta abajo, o hacia el desborde.
//  2. Se suma el agua que pasa por cada celda: la que le llueve (y el deshielo) y la de todas las de arriba.
//  3. Donde se junta bastante, nace un rio; los afluentes salen solos, como raices (el mismo principio que
//     mapgen4, github.com/redblobgames/mapgen4). Cada lago desborda por un solo punto, y de ahi sale su rio.
//
// Arreglos fijos: una grilla de hasta HYDRO_SIDE x HYDRO_SIDE celdas, de a una por vez (no es reentrante;
// se usa al generar el mundo, nunca por cuadro).
#ifndef ESTEPA_HYDRO_H
#define ESTEPA_HYDRO_H

#include <stdbool.h>

#define HYDRO_SIDE 300      // celdas por lado, como mucho
#define HYDRO_PATH_MAX 512  // celdas de un rio
#define HYDRO_BASINS 4096   // depresiones que se cuentan (las mas chicas no son lagos)
#define HYDRO_EPS 0.001f    // m que sube el agua de una celda a la siguiente en lo llano: asi siempre corre

// Que es cada celda (crece al final).
typedef enum {
    HCELL_LAND,  // tierra: le llueve y su agua corre
    HCELL_SEA,   // el mar: una salida
    HCELL_CANAL, // el canal del bosque: una salida
    HCELL_SINK,  // un sumidero (el cañon del desierto): una salida
    HCELL_OUT,   // afuera del mundo: una salida
    HCELL_KINDS
} HydroCell;

typedef struct {
    int side;     // celdas por lado
    float cell;   // m de lado de cada celda
    float x0, z0; // la esquina (x y z minimos) de la grilla
} HydroGrid;

// Que es la celda de centro (x, z), la altura de su terreno sin agua (height) y, si es tierra, cuanta agua
// recibe: rain, la de la lluvia, y melt, la del deshielo del glaciar (en celdas de lluvia).
typedef HydroCell (*HydroCellFn)(const void *ud, float x, float z, float *height, float *rain, float *melt);

// Los pasos 1 y 2: llena las depresiones, ordena las celdas y suma el agua. false si la grilla no cabe.
// Los bordes de la grilla son salidas aunque sean tierra.
bool hydro_run(const HydroGrid *g, HydroCellFn cell, const void *ud);

// Lo que dejo hydro_run, por celda (c = iz * side + ix).
int hydro_cells(void);
void hydro_center(int c, float *x, float *z);
int hydro_cell_at(float x, float z); // -1 afuera de la grilla
HydroCell hydro_kind(int c);
float hydro_ground(int c);  // el terreno
float hydro_surface(int c); // el terreno lleno: en una depresion, el agua hasta su desborde
int hydro_down(int c);      // la celda a la que corre su agua (-1 en las salidas)
float hydro_flow(int c);    // el agua que pasa por ella: la suya y la de todas las de arriba
float hydro_melt(int c);    // la parte de ese agua que viene del deshielo

// Las depresiones (los lagos posibles): las celdas que el llenado cubrio, con al menos min_depth m en lo
// mas hondo y min_cells celdas. Ordenadas como aparecen (por fila). level: el nivel del agua, el de su
// punto de desborde; spill: la primera celda de afuera, por donde sale el agua.
typedef struct {
    float x, z;  // el centro de las celdas cubiertas
    int cells;   // cuantas celdas cubre
    float level; // el nivel del agua
    float depth; // lo mas hondo
    float spread; // m: la distancia media de sus celdas al centro (una depresion larga y finita, mas)
    int spill;   // la celda del desborde
    int id;      // su numero (hydro_basin_of)
} HydroBasin;
int hydro_basins(float min_depth, int min_cells, HydroBasin *out, int max);
int hydro_basin_of(int c); // la depresion de una celda (su id), o -1

// Los rios: caminos de celdas desde la cabecera hasta la boca. Primero el de mas agua; cada afluente
// termina en la celda donde se junta con otro. Solo los que llevan al menos min_flow en la boca, y el rio
// de salida de cada lago (lakes: las depresiones que son lagos), lleve lo que lleve.
typedef enum {
    HEND_OUTLET, // en una salida: el mar, el canal, el sumidero o afuera
    HEND_RIVER,  // en otro rio (into: su indice)
    HEND_LAKE,   // en un lago (into: su indice en lakes)
    HEND_KINDS
} HydroEnd;
typedef struct {
    int cells[HYDRO_PATH_MAX];
    int n;
    float flow;     // el agua en la boca
    HydroEnd end;
    int into;       // el rio o el lago donde termina (-1 en una salida)
    int from_lake;  // el lago del que sale por su desborde (indice en lakes), o -1
} HydroPath;
int hydro_paths(float min_flow, const HydroBasin *lakes, int nlakes, HydroPath *out, int max);

#endif
