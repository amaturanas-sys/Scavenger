// Sistema de Personajes 3D con Equipamiento Modular
// Soporta skinned meshes con bone attachments y texture swapping para Android.
// TODO: Migrar de body_draw a este sistema para personajes con modelos 3D.
#ifndef ESTEPA_CHARACTER_H
#define ESTEPA_CHARACTER_H

#include <stdbool.h>
#include <stddef.h>

#include "raylib.h"
#include "sim/body.h"
#include "sim/armor.h"
#include "sim/apparel.h"
#include "sim/champion.h"

// Mapeo de huesos del modelo base a slots de armadura
// Los índices son los nombres de los huesos en el GLB base_personaje.glb
typedef enum {
    BONE_HEAD = 0,
    BONE_NECK,
    BONE_THORAX,
    BONE_ABDOMEN,
    BONE_PELVIS,
    BONE_UPPER_ARM_L,
    BONE_FOREARM_L,
    BONE_UPPER_ARM_R,
    BONE_FOREARM_R,
    BONE_THIGH_L,
    BONE_SHIN_L,
    BONE_THIGH_R,
    BONE_SHIN_R
} BoneIndex;

// Material de equipamiento para texture swapping
typedef enum {
    TEX_FELT = 0,
    TEX_LEATHER,
    TEX_BRONZE,
    TEX_IRON,
    TEX_STEEL,
    TEX_GOLD
} TextureMaterial;

// Equipamiento de un personaje (modelo 3D + texturas)
typedef struct {
    Model model;           // Modelo base (GLB) con skeleton animado
    ModelAnimation *anims; // Clips de animación
    int anim_count;        // Número de clips
    int current_anim;      // Índice del clip actual
    float anim_time;       // Tiempo en el clip actual
    float anim_speed;      // Velocidad de reproducción
    TextureMaterial tex;   // Textura actual (swappable)
    // Bone attachments: modelos de equipamiento por slot
    Model attachments[SLOT_COUNT];
    bool attachments_loaded[SLOT_COUNT];
} CharacterEquipment;

// Pose del personaje para renderizado
typedef struct {
    BodyPose body;         // Pose de cápsulas (C puro)
    Vector3 pos;           // Posición en mundo
    float yaw;             // Orientación (radianes)
    float scale;           // Escala (talla)
    bool dead;             // Muerto (pose diferente)
} CharacterPose;

// Configuración del sistema
typedef struct {
    int max_characters;    // Máximo de personajes simultáneos
    int cache_size;        // Tamaño de caché de modelos
} CharacterSystemConfig;

// Inicialización del sistema de personajes
void character_system_init(const CharacterSystemConfig *cfg);
void character_system_unload(void);

// Carga de equipamiento para un personaje
// model_path: ruta al modelo base con skeleton (GLB)
// tex_path: ruta a la textura diffuse para swapping
// anim_names: array de nombres de clips a cargar (NULL-terminated)
// return: índice del equipamiento, o -1 si falla
int character_equip_load(const char *model_path, const char *tex_path, const char **anim_names);
void character_equip_unload(int equip_idx);
void character_equip_swap_texture(int equip_idx, TextureMaterial tex);
void character_equip_clear(int equip_idx);

// Actualización de animación
void character_update_anim(int equip_idx, float dt);
void character_set_anim(int equip_idx, int anim_index);
void character_set_anim_by_name(int equip_idx, const char *anim_name);

// Renderizado
void character_draw(const CharacterPose *pose, int equip_idx);
void character_draw_dead(const CharacterPose *pose, int equip_idx);

// Utilidades
int character_get_bone_index(ArmorSlot slot);
const char *bone_name(BoneIndex bone);
TextureMaterial armor_material_to_tex(ArmorMaterial mat);

// Caché de texturas
void character_cache_textures(void);
void character_unload_textures(void);

#endif