// Sistema de Personajes 3D con Equipamiento Modular
#include "character.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "sim/armor.h"
#include "rlgl.h"

// Configuración por defecto
#define CHAR_MAX 64
#define CACHE_SIZE 16
#define BONE_COUNT 14
#define SLOT_COUNT 9

static CharacterSystemConfig g_config = { CHAR_MAX, CACHE_SIZE };
static CharacterEquipment g_equipment[CHAR_MAX];
static int g_equipment_count = 0;

// Nombres de los huesos del modelo base (GLB)
static const char *BONE_NAMES[BONE_COUNT] = {
    "Head", "Neck", "Thorax", "Abdomen", "Pelvis",
    "UpperArm_L", "Forearm_L", "UpperArm_R", "Forearm_R",
    "Thigh_L", "Shin_L", "Thigh_R", "Shin_R"
};

// Mapeo de slots a índices de hueso
static const struct {
    ArmorSlot slot;
    BoneIndex bone;
} SLOT_BONE_MAP[SLOT_COUNT] = {
    [SLOT_HELMET] = { SLOT_HELMET, BONE_HEAD },
    [SLOT_NECK] = { SLOT_NECK, BONE_NECK },
    [SLOT_TORSO] = { SLOT_TORSO, BONE_THORAX },
    [SLOT_SHOULDERS] = { SLOT_SHOULDERS, BONE_UPPER_ARM_L },
    [SLOT_BRACERS] = { SLOT_BRACERS, BONE_FOREARM_L },
    [SLOT_GLOVES] = { SLOT_GLOVES, BONE_FOREARM_L },
    [SLOT_SKIRT] = { SLOT_SKIRT, BONE_PELVIS },
    [SLOT_GREAVES] = { SLOT_GREAVES, BONE_THIGH_L },
    [SLOT_BOOTS] = { SLOT_BOOTS, BONE_THIGH_L },
};

// Rutas de texturas por material
static const char *TEX_PATHS[6] = {
    [TEX_FELT] = "assets/models/personajes/texturas/felt_diffuse.png",
    [TEX_LEATHER] = "assets/models/personajes/texturas/leather_diffuse.png",
    [TEX_BRONZE] = "assets/models/personajes/texturas/bronze_diffuse.png",
    [TEX_IRON] = "assets/models/personajes/texturas/iron_diffuse.png",
    [TEX_STEEL] = "assets/models/personajes/texturas/steel_diffuse.png",
    [TEX_GOLD] = "assets/models/personajes/texturas/gold_diffuse.png",
};

void character_system_init(const CharacterSystemConfig *cfg) {
    if (cfg) {
        g_config = *cfg;
    }
    memset(g_equipment, 0, sizeof(g_equipment));
    g_equipment_count = 0;
}

void character_system_unload(void) {
    for (int i = 0; i < g_equipment_count; i++) {
        character_equip_unload(i);
    }
    g_equipment_count = 0;
}

int character_equip_load(const char *model_path, const char *tex_path, const char **anim_names) {
    if (g_equipment_count >= g_config.max_characters) return -1;
    if (!model_path) return -1;

    int idx = g_equipment_count++;
    CharacterEquipment *eq = &g_equipment[idx];

    const char *asset_path = platform_asset_path(model_path);
    eq->model = LoadModel(asset_path);
    eq->model.materialCount = 1;

    if (eq->model.meshCount == 0) {
        UnloadModel(eq->model);
        return -1;
    }

    eq->anims = LoadModelAnimations(asset_path, &eq->anim_count);

    if (anim_names && anim_names[0]) {
        for (int i = 0; i < eq->anim_count; i++) {
            for (int j = 0; anim_names[j]; j++) {
                if (strcmp(eq->anims[i].name, anim_names[j]) == 0) {
                    eq->current_anim = i;
                    break;
                }
            }
        }
    } else {
        for (int i = 0; i < eq->anim_count; i++) {
            if (strcmp(eq->anims[i].name, "idle") == 0) {
                eq->current_anim = i;
                break;
            }
        }
        if (eq->current_anim < 0 && eq->anim_count > 0) {
            eq->current_anim = 0;
        }
    }

    eq->anim_time = 0.0f;
    eq->anim_speed = 1.0f;
    eq->tex = TEX_FELT;

    for (int s = 0; s < SLOT_COUNT; s++) {
        eq->attachments_loaded[s] = false;
        if (eq->attachments[s].meshCount > 0) {
            UnloadModel(eq->attachments[s]);
        }
    }

    return idx;
}

void character_equip_unload(int equip_idx) {
    if (equip_idx < 0 || equip_idx >= g_equipment_count) return;
    CharacterEquipment *eq = &g_equipment[equip_idx];

    if (eq->anims) {
        UnloadModelAnimations(eq->anims, eq->anim_count);
    }

    if (eq->model.meshCount > 0) {
        UnloadModel(eq->model);
    }

    for (int s = 0; s < SLOT_COUNT; s++) {
        if (eq->attachments_loaded[s] && eq->attachments[s].meshCount > 0) {
            UnloadModel(eq->attachments[s]);
        }
    }
}

void character_equip_swap_texture(int equip_idx, TextureMaterial tex) {
    if (equip_idx < 0 || equip_idx >= g_equipment_count) return;
    CharacterEquipment *eq = &g_equipment[equip_idx];

    if (tex >= 6 || tex == eq->tex) return;

    eq->tex = tex;

    if (eq->model.meshCount > 0 && eq->model.materialCount > 0) {
        Texture2D tex2d = LoadTexture(TEX_PATHS[tex]);
        if (tex2d.id > 0) {
            eq->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = tex2d;
            eq->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
        }
    }
}

void character_equip_clear(int equip_idx) {
    if (equip_idx < 0 || equip_idx >= g_equipment_count) return;
    CharacterEquipment *eq = &g_equipment[equip_idx];

    character_equip_unload(equip_idx);
    g_equipment_count--;
    if (equip_idx < g_equipment_count) {
        g_equipment[equip_idx] = g_equipment[g_equipment_count];
    }
}

void character_update_anim(int equip_idx, float dt) {
    if (equip_idx < 0 || equip_idx >= g_equipment_count) return;
    CharacterEquipment *eq = &g_equipment[equip_idx];

    eq->anim_time += dt * eq->anim_speed;

    if (eq->anim_count > 0 && eq->anims[eq->current_anim].frameCount > 0) {
        int frame = (int)(eq->anim_time * 30.0f) % eq->anims[eq->current_anim].frameCount;
        UpdateModelAnimation(eq->model, eq->anims[eq->current_anim], frame);
    }
}

void character_set_anim(int equip_idx, int anim_index) {
    if (equip_idx < 0 || equip_idx >= g_equipment_count) return;
    CharacterEquipment *eq = &g_equipment[equip_idx];

    if (anim_index >= 0 && anim_index < eq->anim_count) {
        eq->current_anim = anim_index;
        eq->anim_time = 0.0f;
    }
}

void character_set_anim_by_name(int equip_idx, const char *anim_name) {
    if (equip_idx < 0 || equip_idx >= g_equipment_count || !anim_name) return;
    CharacterEquipment *eq = &g_equipment[equip_idx];

    for (int i = 0; i < eq->anim_count; i++) {
        if (strcmp(eq->anims[i].name, anim_name) == 0) {
            eq->current_anim = i;
            eq->anim_time = 0.0f;
            return;
        }
    }
}

void character_draw(const CharacterPose *pose, int equip_idx) {
    if (equip_idx < 0 || equip_idx >= g_equipment_count) return;
    CharacterEquipment *eq = &g_equipment[equip_idx];

    if (eq->model.meshCount == 0) return;

    Vector3 world_pos = pose->pos;
    world_pos.y += 1.7f * pose->scale;

    rlPushMatrix();
    rlRotatef(-pose->yaw * RAD2DEG, 0, 1, 0);
    rlRotatef(-pose->yaw * RAD2DEG, 1, 0, 0);

    DrawModel(eq->model, (Vector3){ 0, 0, 0 }, pose->scale, WHITE);

    // Nota: Bone attachments requieren GetBoneTransform de raymath,
    // pero raylib 5.5 no lo expone directamente.
    // Se puede implementar manualmente si el modelo GLB tiene
    // transformaciones de huesos exportadas.

    rlPopMatrix();
}

void character_draw_dead(const CharacterPose *pose, int equip_idx) {
    if (equip_idx < 0 || equip_idx >= g_equipment_count) return;
    CharacterEquipment *eq = &g_equipment[equip_idx];

    if (eq->model.meshCount == 0) return;

    Vector3 world_pos = pose->pos;
    world_pos.y += 0.7f * pose->scale;

    rlPushMatrix();
    rlRotatef(180, 0, 0, 1);
    rlRotatef(-pose->yaw * RAD2DEG, 1, 0, 0);

    DrawModel(eq->model, (Vector3){ 0, 0, 0 }, pose->scale, WHITE);

    rlPopMatrix();
}

int character_get_bone_index(ArmorSlot slot) {
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (SLOT_BONE_MAP[i].slot == slot) {
            return (int)SLOT_BONE_MAP[i].bone;
        }
    }
    return -1;
}

const char *bone_name(BoneIndex bone) {
    if (bone >= 0 && bone < BONE_COUNT) {
        return BONE_NAMES[bone];
    }
    return NULL;
}

TextureMaterial armor_material_to_tex(ArmorMaterial mat) {
    switch (mat) {
        case MAT_FELT: return TEX_FELT;
        case MAT_LEATHER: return TEX_LEATHER;
        case MAT_BRONZE: return TEX_BRONZE;
        case MAT_IRON: return TEX_IRON;
        case MAT_STEEL: return TEX_STEEL;
        case MAT_GOLD: return TEX_GOLD;
        default: return TEX_FELT;
    }
}

void character_cache_textures(void) {
}

void character_unload_textures(void) {
}
