// Los arboles del mundo con sus modelos de voxeles del inventario (el pino, el sauce y el arce: TreeKind, src/sim/flora.h).
// Cada uno se lee una vez de su .vox y se arma en tres detalles: el de cerca (el .vox tal cual), el de media distancia
// (celdas del doble) y el de lejos (del cuadruple, vox_downsample), y en las estaciones que pida el paisaje:
//  - el verano (y la primavera): como en el .vox;
//  - el otoño: el arce rojo y naranja, el sauce amarillo; el pino, igual;
//  - pelado (de fines del otoño al deshielo): el arce y el sauce sin hojas, solo el tronco y las ramas;
//  - nevado: el pino con nieve encima de cada piso; el arce y el sauce, pelados.
// Se dibujan con DrawModelEx, uno por arbol, con el detalle segun la distancia al ojo y solo los que caen delante de la
// camara (sin memoria por cuadro: las mallas se arman al pedirlas y quedan).
#ifndef ESTEPA_TREES_H
#define ESTEPA_TREES_H

#include <stdbool.h>

#include "raylib.h"
#include "sim/flora.h"
#include "sim/inventory.h"

// El inventario, para el .vox y el alto de cada arbol (mapa.vegetacion.pino, .sauce, .arce).
void trees_init(const Inventory *inv);
// El cuadro: la camara (el detalle y lo que se ve). Despues, trees_draw por cada arbol.
void trees_begin(Camera3D cam);
// Un arbol en su estacion (flora_tree_season) con la base en at, su escala (1: el alto del inventario), su giro
// (radianes) y su tinte (WHITE; los quemados, oscuros). Sin su modelo, un cono o una bola de respaldo.
void trees_draw(TreeKind kind, TreeSeason season, Vector3 at, float scale, float yaw, Color tint);
// El alto del modelo a escala 1 (m), o 0 si no esta.
float trees_height(TreeKind kind);
// Cuantos se dibujaron desde trees_begin (para el diagnostico) y, si per_lod no es NULL, cuantos con cada detalle (de
// cerca, a media distancia y de lejos).
int trees_drawn(int per_lod[3]);
void trees_unload(void);
void trees_forget_gpu(void); // ver terrain_forget_gpu

#endif
