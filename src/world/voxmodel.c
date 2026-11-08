#include "voxmodel.h"

#include <string.h>

#include "sim/voxfile.h"
#include "voxstruct.h"

bool vm_load(const char *path, float height, Model *out) {
    memset(out, 0, sizeof(*out));
    int len = 0;
    unsigned char *data = LoadFileData(path, &len);
    if (!data) return false;
    VoxGrid g;
    static uint8_t pal[256][4]; // solo al cargar (nada por cuadro)
    bool ok = len > 0 && voxfile_read(data, (size_t)len, 0.1f, &g, pal);
    UnloadFileData(data);
    if (!ok) return false;
    if (height > 0.0f) g.size = height / (float)g.ny;
    g.on_ground = true; // lo del inventario va en el suelo: la cara de abajo de la base, solo en el borde
    // Lo de MagicaVoxel va como se pinto: sin refinar y con apenas variacion de tono por celda.
    ok = voxs_grid_model(&g, (const uint8_t(*)[4])pal, out, false, 6);
    vox_free(&g);
    return ok;
}
