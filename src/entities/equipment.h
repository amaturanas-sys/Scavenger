// Sistema de Equipamiento 3D para Bone Attachments
// Carga y gestiona modelos de equipamiento que se adjuntan a huesos
// del skeleton principal para rendering eficiente.
#ifndef ESTEPA_EQUIPMENT_H
#define ESTEPA_EQUIPMENT_H

#include <stdbool.h>
#include <stddef.h>

#include "raylib.h"
#include "sim/armor.h"

// Mapeo de slots de armadura a índices de hueso del modelo base
// Estos índices deben coincidir con los nombres de los huesos en el GLB
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

// Caché de modelos de equipamiento
// Cada entrada representa un modelo cargado que se adjunta a un hueso
typedef struct {
    Model model;           // Modelo 3D del equipamiento
    int bone_index;        // Índice del hueso al que está adjunto
    bool loaded;           // Si el modelo está cargado
} EquipCache;

// Funciones de carga y gestión
// Carga un modelo de equipamiento para un slot específico
// return: índice en la caché, o -1 si falla
int equipment_load_for_slot(ArmorSlot slot, TextureMaterial tex);

// Libera el modelo de equipamiento para un slot específico
void equipment_unload_for_slot(ArmorSlot slot);

// Libera todos los modelos de equipamiento
void equipment_unload_all(void);

// Obtiene el modelo cargado para un índice de caché
Model *equipment_get_model(int cache_idx);

// Obtiene el índice del hueso al que está adjunto un modelo
int equipment_get_bone_index(int cache_idx);

// Intercambia la textura de un equipamiento por material
void equipment_swap_texture_for_slot(ArmorSlot slot, TextureMaterial tex);

// Utilidades
// Mapeo de slot a índice de hueso
int equipment_bone_index_for_slot(ArmorSlot slot);

// Nombre del material
const char *texture_material_name(TextureMaterial tex);

#endif
