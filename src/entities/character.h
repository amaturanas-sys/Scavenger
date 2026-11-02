// Sistema de Personajes 3D con Equipamiento Modular
// Soporta skinned meshes con bone attachments y texture swapping para Android.
// ANDAMIAJE SIN CONECTAR (v0.10.8): nada del juego lo llama todavia. Los personajes se dibujan con
// src/world/body_draw.c y los modelos con src/world/props.c; esto no los reemplaza. Antes de
// conectarlo o de crecerlo, ver docs/ARQUITECTURA.md («src/entities»).
#ifndef ESTEPA_CHARACTER_H
#define ESTEPA_CHARACTER_H

#include <stdbool.h>
#include <stddef.h>

#include "raylib.h"
#include "sim/body.h"
#include "sim/armor.h"
#include "sim/apparel.h"
#include "sim/champion.h"
#include "equipment.h"

// BoneIndex y TextureMaterial: en equipment.h (un tipo vive en un solo header).

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