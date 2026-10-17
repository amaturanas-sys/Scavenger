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
#define CAMP_MAX_YURTS 5
#define CAMP_MAX_TREES 100

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
    // Camp site data (from camp_game.c)
    Vector3 fire;
    Vector3 yurts[CAMP_MAX_YURTS];
    int yurt_count;
    Vector3 trees[CAMP_MAX_TREES];
    int tree_count;
    float yurt_rot[CAMP_MAX_YURTS];
} Camp;

void camp_init(Camp *c, const Terrain *t, const char *yurt_path);
// Carga un layout desde JSON (cJSON). Devuelve false si falla.
bool LoadCampLayout(Camp *c, const char* filepath);
// Load procedural camp layout from assets/camp_data.json
bool LoadProceduralCamp(Camp *c, const char* filepath);
void DrawCamp(const Camp *c, float time, float snow, bool fire_lit);
void camp_unload(Camp *c);
// Animation update
void camp_update_animations(Camp *c, float dt);
// Drawing function (lowercase for compatibility with main.c)
static inline void camp_draw(const Camp *c, float time, float snow, bool fire_lit) {
    DrawCamp(c, time, snow, fire_lit);
}

#endif
