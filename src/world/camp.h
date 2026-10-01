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
void camp_draw(const Camp *c, float time);
void camp_unload(Camp *c);

#endif
