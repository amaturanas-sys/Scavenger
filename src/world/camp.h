// Campamento de la tropa: yurtas (modelo GLB generado con Kiln), fogata y
// arboles dispersos por la estepa.
#ifndef ESTEPA_CAMP_H
#define ESTEPA_CAMP_H

#include <stdbool.h>

#include "raylib.h"
#include "terrain.h"

#define CAMP_MAX_YURTS 8
#define WORLD_MAX_TREES 80

typedef struct {
    Model yurt;
    bool yurt_loaded;
    Vector3 yurts[CAMP_MAX_YURTS];
    float yurt_rot[CAMP_MAX_YURTS];
    int yurt_count;
    Vector3 fire;
    Vector3 trees[WORLD_MAX_TREES];
    float tree_h[WORLD_MAX_TREES];
    int tree_count;
} Camp;

void camp_init(Camp *c, const Terrain *t, const char *yurt_path);
// snow en [0, 1]: nieve sobre los techos de las yurtas y las copas. fire_lit: la fogata
// arde (la lluvia la apaga). tree_burn (o NULL): por arbol, 0 sano, 0.5 chamuscado, 1 calcinado.
void camp_draw(const Camp *c, float time, float snow, bool fire_lit, const float *tree_burn);
// Solo la llama (la luz de la noche la vuelve a dibujar sin oscurecer).
void camp_draw_flame(const Camp *c, float time);
void camp_unload(Camp *c);

#endif
