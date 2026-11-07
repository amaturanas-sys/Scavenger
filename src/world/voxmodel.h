// Los modelos de voxeles del inventario (.vox de MagicaVoxel, con la etiqueta formato:voxel): se lee el archivo
// (sim/voxfile.h), se arma la malla con su paleta (voxs_grid_model, con oclusion y luz por cara) y se suelta la
// rejilla. Se dibujan como los GLB: con DrawModel, el origen en el centro de la base y el frente hacia +X.
#ifndef ESTEPA_VOXMODEL_H
#define ESTEPA_VOXMODEL_H

#include <stdbool.h>

#include "raylib.h"

// height: el alto del objeto en metros (las medidas del inventario); <= 0: 0,1 m por celda. false si el archivo
// falta o no se lee (y *out queda vacio). Se libera con UnloadModel.
bool vm_load(const char *path, float height, Model *out);

#endif
