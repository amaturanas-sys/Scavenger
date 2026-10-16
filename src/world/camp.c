#include "camp.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"

#include "../sim/rng.h"
#include "hearth.h"
#include "platform.h"
#include "raymath.h"

void camp_init(Camp *c, const Terrain *t, const char *yurt_path) {
    memset(c, 0, sizeof(*c));
    // Initialize animation system
    anim_system_init(&c->anim_system);

    // En Android los assets viven dentro del APK: platform_asset_exists pregunta al AssetManager.
    if (yurt_path && platform_asset_exists(yurt_path)) {
        c->yurt_model = LoadModel(yurt_path);
        c->yurt_loaded = c->yurt_model.meshCount > 0;
    }
    if (yurt_path) platform_note_asset(yurt_path, c->yurt_loaded);

    // We'll load a hearth model too for completeness
    c->hearth_model = LoadModel(platform_asset_path("assets/models/estructura/forja/forja_comun.glb"));
    c->hearth_loaded = c->hearth_model.meshCount > 0;
    if (platform_asset_path("assets/models/estructura/forja/forja_comun.glb"))
        platform_note_asset(platform_asset_path("assets/models/estructura/forja/forja_comun.glb"), c->hearth_loaded);

    // Initialize with default camp layout
    LoadCampLayout(c, platform_asset_path("assets/data/camp_layout.json"));
}

bool LoadCampLayout(Camp *c, const char* filepath) {
    char *text = LoadFileText(filepath);
    if (!text) return false;

    cJSON *root = cJSON_Parse(text);
    UnloadFileText(text);
    if (!root) return false;

    cJSON *camp_obj = cJSON_GetObjectItem(root, "camp");
    if (!camp_obj) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *inst_list = cJSON_GetObjectItem(camp_obj, "components");
    if (!inst_list) {
        cJSON_Delete(root);
        return false;
    }

    int inst_count = cJSON_GetArraySize(inst_list);
    if (inst_count > CAMP_MAX_INSTANCES) inst_count = CAMP_MAX_INSTANCES;

    c->instance_count = inst_count;

    for (int i = 0; i < inst_count; i++) {
        cJSON *comp = cJSON_GetArrayItem(inst_list, i);
        cJSON *type_comp = cJSON_GetObjectItem(comp, "type");
        cJSON *pos_comp = cJSON_GetObjectItem(comp, "position");
        cJSON *rot_comp = cJSON_GetObjectItem(comp, "rotation");
        cJSON *scale_comp = cJSON_GetObjectItem(comp, "scale");

        if (!type_comp || !pos_comp || !rot_comp || !scale_comp) continue;

        const char *type_str = cJSON_GetStringValue(type_comp);
        if (!type_str) continue;

        if (strcmp(type_str, "yurt") == 0) {
            c->instances[i].type = CAMP_TYPE_YURT;
        } else if (strcmp(type_str, "hearth") == 0) {
            c->instances[i].type = CAMP_TYPE_HEARTH;
        } else if (strcmp(type_str, "ger_cart") == 0) {
            c->instances[i].type = CAMP_TYPE_GER_CART;
        } else if (strcmp(type_str, "forge") == 0) {
            c->instances[i].type = CAMP_TYPE_FORGE;
        } else if (strcmp(type_str, "palisade_wall") == 0) {
            c->instances[i].type = CAMP_TYPE_PALISADE_WALL;
        } else if (strcmp(type_str, "tree") == 0) {
            c->instances[i].type = CAMP_TYPE_TREE;
        } else if (strcmp(type_str, "storage") == 0) {
            c->instances[i].type = CAMP_TYPE_STORAGE;
        } else if (strcmp(type_str, "altar") == 0) {
            c->instances[i].type = CAMP_TYPE_ALTAR;
        } else if (strcmp(type_str, "training") == 0) {
            c->instances[i].type = CAMP_TYPE_TRAINING;
        } else if (strcmp(type_str, "trading") == 0) {
            c->instances[i].type = CAMP_TYPE_TRADING;
        } else {
            continue;
        }

        c->instances[i].position.x = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(pos_comp, "x"));
        c->instances[i].position.y = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(pos_comp, "y"));
        c->instances[i].position.z = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(pos_comp, "z"));
        c->instances[i].rotation = (float)cJSON_GetNumberValue(rot_comp);
        c->instances[i].scale = (float)cJSON_GetNumberValue(scale_comp);
    }

    cJSON_Delete(root);
    return true;
}

void DrawCamp(const Camp *c, float time, float snow, bool fire_lit) {
    (void)time;

    for (int i = 0; i < c->instance_count; i++) {
        const CampInstance *inst = &c->instances[i];
        Color base_color = WHITE;

        switch (inst->type) {
            case CAMP_TYPE_YURT:
                if (c->yurt_loaded) {
                    DrawModelEx(c->yurt_model, inst->position, (Vector3){ 0, 1, 0 },
                        inst->rotation * RAD2DEG, (Vector3){ inst->scale, inst->scale, inst->scale }, base_color);
                } else {
                    // Fallback cylinder for missing model
                    DrawCylinder(inst->position, 2.2f, 2.2f, 1.7f * inst->scale, 10, (Color){ 233, 228, 214, 255 });
                }
                if (snow > 0.3f) {
                    Vector3 p = inst->position;
                    p.y += 1.7f;
                    DrawCylinder(p, 0.5f, 2.4f, 0.92f * inst->scale, 10, (Color){ 238, 241, 246, 255 });
                }
                break;
            case CAMP_TYPE_HEARTH:
                hearth_draw_base(inst->position, 1.3f * inst->scale, 7u, fire_lit);
                break;
            case CAMP_TYPE_GER_CART:
            case CAMP_TYPE_FORGE:
            case CAMP_TYPE_PALISADE_WALL:
                DrawCubeWiresV(inst->position,
                    (Vector3){ 2.0f * inst->scale, 2.0f * inst->scale, 2.0f * inst->scale },
                    (Color){ 200, 200, 200, 255 });
                break;
            case CAMP_TYPE_TREE:
                DrawCylinder(inst->position, 0.15f * inst->scale, 0.25f * inst->scale,
                    3.0f * inst->scale, 5, (Color){ 96, 70, 46, 255 });
                break;
            case CAMP_TYPE_STORAGE:
                DrawCube(inst->position,
                    3.0f * inst->scale, 2.5f * inst->scale, 3.0f * inst->scale,
                    (Color){ 150, 100, 50, 255 });
                if (snow > 0.3f) {
                    Vector3 p = inst->position;
                    p.y += 1.25f * inst->scale;
                    DrawCube(p,
                        3.1f * inst->scale, 2.6f * inst->scale, 3.1f * inst->scale,
                        (Color){ 238, 241, 246, 128 });
                }
                break;
            case CAMP_TYPE_ALTAR:
                DrawCylinder((Vector3){ inst->position.x, inst->position.y + 0.5f * inst->scale, inst->position.z },
                    1.5f * inst->scale, 1.5f * inst->scale, 0.2f * inst->scale, 8, (Color){ 139, 69, 19, 255 });
                DrawCylinder((Vector3){ inst->position.x, inst->position.y + 1.5f * inst->scale, inst->position.z },
                    0.8f * inst->scale, 0.8f * inst->scale, 1.0f * inst->scale, 8, (Color){ 255, 215, 0, 200 });
                break;
            case CAMP_TYPE_TRAINING:
                // Draw a simple obstacle course marker
                DrawSphere(inst->position, 0.5f * inst->scale, 8, 8, (Color){ 255, 165, 0, 200 });
                DrawLine3D((Vector3){ inst->position.x - 2.0f * inst->scale, inst->position.y, inst->position.z },
                    (Vector3){ inst->position.x + 2.0f * inst->scale, inst->position.y, inst->position.z },
                    (Color){ 255, 255, 255, 100 });
                DrawLine3D((Vector3){ inst->position.x, inst->position.y, inst->position.z - 2.0f * inst->scale },
                    (Vector3){ inst->position.x, inst->position.y, inst->position.z + 2.0f * inst->scale },
                    (Color){ 255, 255, 255, 100 });
                break;
            case CAMP_TYPE_TRADING:
                DrawCube(inst->position,
                    2.0f * inst->scale, 1.5f * inst->scale, 2.0f * inst->scale,
                    (Color){ 210, 180, 140, 255 });
                // Simple trading post marker - a small cube on top
                DrawCube((Vector3){ inst->position.x, inst->position.y + 2.0f * inst->scale, inst->position.z },
                    0.5f * inst->scale, 0.3f * inst->scale, 0.5f * inst->scale,
                    (Color){ 255, 215, 0, 255 });
                break;
        }
    }
}

void camp_unload(Camp *c) {
    if (c->yurt_loaded) UnloadModel(c->yurt_model);
    c->yurt_loaded = false;
    if (c->hearth_loaded) UnloadModel(c->hearth_model);
    c->hearth_loaded = false;
    anim_system_unload_all(&c->anim_system);
}

void camp_update_animations(Camp *c, float dt) {
    anim_system_update(&c->anim_system, dt);
}
