// Los modelos de voxeles en archivo (C puro): el formato .vox de MagicaVoxel, el editor gratis, que tambien
// escribe tools/voxeles/voxeles.py (desde tres vistas, un splat de Idol o un GLB). Un solo modelo: el primero.
//
// Los ejes: en el .vox, z hacia arriba y el frente del objeto mirando a -y (la vista de frente de MagicaVoxel);
// en la rejilla del juego (sim/voxel.h), y hacia arriba y el frente hacia +X, como los GLB del inventario. Cada
// celda guarda su indice de color (1..255; 0, vacia) y la paleta va aparte, como la de los materiales: el color
// de la celda es palette[indice].
#ifndef ESTEPA_VOXFILE_H
#define ESTEPA_VOXFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sim/voxel.h"

#define VOXFILE_MAX 128 // celdas por eje, como mucho (un objeto, no un paisaje)

// Lee el primer modelo. size: metros por celda. Reserva la rejilla (vox_free). Sin paleta en el archivo, grises.
// false (y nada reservado) si no es un .vox, esta roto o pasa de VOXFILE_MAX por eje.
bool voxfile_read(const uint8_t *data, size_t len, float size, VoxGrid *g, uint8_t palette[256][4]);
// La rejilla como .vox (para las pruebas y para exportar lo que arma el juego). Los bytes escritos, o 0 si no
// cupo en cap o la rejilla pasa de VOXFILE_MAX por eje.
size_t voxfile_write(const VoxGrid *g, const uint8_t palette[256][4], uint8_t *out, size_t cap);

#endif
