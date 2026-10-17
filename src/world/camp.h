// Campamento de la tropa: yurtas (modelo GLB generado con Kiln), fogata y
// arboles dispersos por la estepa.
#ifndef ESTEPA_CAMP_H
#define ESTEPA_CAMP_H

#include <stdbool.h>

#include "raylib.h"
#include "terrain.h"
#include "anim_system.h"

#define CAMP_MAX_INSTANCES 64
#define CAMP_MAX_ANIMATED 8

typedef enum {
    CAMP_TYPE_YURT,
    CAMP_TYPE_HEARTH,
    CAMP_TYPE_GER_CART,
    CAMP_TYPE_FORGE,
    CAMP_TYPE_PALISADE_WALL,
    CAMP_TYPE_TREE,
    CAMP_TYPE_STORAGE,
    CAMP_TYPE_ALTAR,
    CAMP_TYPE_TRAINING,
    CAMP_TYPE_TRADING
} CampInstanceType;

typedef struct {
    CampInstanceType type;
    Vector3 position;
    float rotation;
    float scale;
    // Animation support
    int anim_entity_index;  // Index in global AnimationSystem (-1 if not animated)
} CampInstance;

typedef struct {
    CampInstance instances[CAMP_MAX_INSTANCES];
    int instance_count;
    Model yurt_model;
    bool yurt_loaded;
    Model hearth_model;
    bool hearth_loaded;
    AnimationSystem anim_system;
} Camp;

void camp_init(Camp *c, const Terrain *t, const char *yurt_path);
// Carga un layout desde JSON (cJSON). Devuelve false si falla.
bool LoadCampLayout(Camp *c, const char* filepath);
void DrawCamp(const Camp *c, float time, float snow, bool fire_lit);
void camp_unload(Camp *c);
// Animation update
void camp_update_animations(Camp *c, float dt);

#endif
