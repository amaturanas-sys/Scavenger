// Sistema de Equipamiento 3D para Bone Attachments
#include "equipment.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#include "platform.h"
#include "sim/armor.h"

#define EQUIP_CACHE_SIZE 64
#define BONE_COUNT 14
#define SLOT_COUNT 9
#define TEX_COUNT 6

static EquipCache g_equip_cache[EQUIP_CACHE_SIZE];
static int g_equip_cache_count = 0;

static const char *EQUIP_PATHS[SLOT_COUNT][TEX_COUNT] = {
    [SLOT_HELMET] = {
        "assets/models/personajes/equipamento/cascos/cascos_armadura_casco/bronze.glb",
        "assets/models/personajes/equipamento/cascos/cascos_armadura_casco/cuero.glb",
        "assets/models/personajes/equipamento/cascos/cascos_armadura_casco/hierro.glb",
        "assets/models/personajes/equipamento/cascos/cascos_armadura_casco/acero.glb",
        "assets/models/personajes/equipamento/cascos/cascos_armadura_casco/oro.glb",
        "assets/models/personajes/equipamento/cascos/cascos_armadura_casco/fielto.glb",
    },
    [SLOT_NECK] = {
        "assets/models/personajes/equipamento/gorjales/gorjales_armadura_gorjales/bronze.glb",
        "assets/models/personajes/equipamento/gorjales/gorjales_armadura_gorjales/cuero.glb",
        "assets/models/personajes/equipamento/gorjales/gorjales_armadura_gorjales/hierro.glb",
        "assets/models/personajes/equipamento/gorjales/gorjales_armadura_gorjales/acero.glb",
        "assets/models/personajes/equipamento/gorjales/gorjales_armadura_gorjales/oro.glb",
        "assets/models/personajes/equipamento/gorjales/gorjales_armadura_gorjales/fielto.glb",
    },
    [SLOT_TORSO] = {
        "assets/models/personajes/equipamento/chalecos/armadura_chaleco/bronze.glb",
        "assets/models/personajes/equipamento/chalecos/armadura_chaleco/cuero.glb",
        "assets/models/personajes/equipamento/chalecos/armadura_chaleco/hierro.glb",
        "assets/models/personajes/equipamento/chalecos/armadura_chaleco/acero.glb",
        "assets/models/personajes/equipamento/chalecos/armadura_chaleco/oro.glb",
        "assets/models/personajes/equipamento/chalecos/armadura_chaleco/fielto.glb",
    },
    [SLOT_SHOULDERS] = {
        "assets/models/personajes/equipamento/hombreras/hombreras_armadura_hombreras/bronze.glb",
        "assets/models/personajes/equipamento/hombreras/hombreras_armadura_hombreras/cuero.glb",
        "assets/models/personajes/equipamento/hombreras/hombreras_armadura_hombreras/hierro.glb",
        "assets/models/personajes/equipamento/hombreras/hombreras_armadura_hombreras/acero.glb",
        "assets/models/personajes/equipamento/hombreras/hombreras_armadura_hombreras/oro.glb",
        "assets/models/personajes/equipamento/hombreras/hombreras_armadura_hombreras/fielto.glb",
    },
    [SLOT_BRACERS] = {
        "assets/models/personajes/equipamento/brazales/brazales_armadura_brazales/bronze.glb",
        "assets/models/personajes/equipamento/brazales/brazales_armadura_brazales/cuero.glb",
        "assets/models/personajes/equipamento/brazales/brazales_armadura_brazales/hierro.glb",
        "assets/models/personajes/equipamento/brazales/brazales_armadura_brazales/acero.glb",
        "assets/models/personajes/equipamento/brazales/brazales_armadura_brazales/oro.glb",
        "assets/models/personajes/equipamento/brazales/brazales_armadura_brazales/fielto.glb",
    },
    [SLOT_GLOVES] = {
        "assets/models/personajes/equipamento/manos/manos_armadura_manos/bronze.glb",
        "assets/models/personajes/equipamento/manos/manos_armadura_manos/cuero.glb",
        "assets/models/personajes/equipamento/manos/manos_armadura_manos/hierro.glb",
        "assets/models/personajes/equipamento/manos/manos_armadura_manos/acero.glb",
        "assets/models/personajes/equipamento/manos/manos_armadura_manos/oro.glb",
        "assets/models/personajes/equipamento/manos/manos_armadura_manos/fielto.glb",
    },
    [SLOT_SKIRT] = {
        "assets/models/personajes/equipamento/faldas/faldas_armadura_faldas/bronze.glb",
        "assets/models/personajes/equipamento/faldas/faldas_armadura_faldas/cuero.glb",
        "assets/models/personajes/equipamento/faldas/faldas_armadura_faldas/hierro.glb",
        "assets/models/personajes/equipamento/faldas/faldas_armadura_faldas/acero.glb",
        "assets/models/personajes/equipamento/faldas/faldas_armadura_faldas/oro.glb",
        "assets/models/personajes/equipamento/faldas/faldas_armadura_faldas/fielto.glb",
    },
    [SLOT_GREAVES] = {
        "assets/models/personajes/equipamento/grebas/grebas_armadura_grebas/bronze.glb",
        "assets/models/personajes/equipamento/grebas/grebas_armadura_grebas/cuero.glb",
        "assets/models/personajes/equipamento/grebas/grebas_armadura_grebas/hierro.glb",
        "assets/models/personajes/equipamento/grebas/grebas_armadura_grebas/acero.glb",
        "assets/models/personajes/equipamento/grebas/grebas_armadura_grebas/oro.glb",
        "assets/models/personajes/equipamento/grebas/grebas_armadura_grebas/fielto.glb",
    },
    [SLOT_BOOTS] = {
        "assets/models/personajes/equipamento/piernas/piernas_armadura_piernas/bronze.glb",
        "assets/models/personajes/equipamento/piernas/piernas_armadura_piernas/cuero.glb",
        "assets/models/personajes/equipamento/piernas/piernas_armadura_piernas/hierro.glb",
        "assets/models/personajes/equipamento/piernas/piernas_armadura_piernas/acero.glb",
        "assets/models/personajes/equipamento/piernas/piernas_armadura_piernas/oro.glb",
        "assets/models/personajes/equipamento/piernas/piernas_armadura_piernas/fielto.glb",
    },
};

static const char *EQUIP_TEX_PATHS[6] = {
    [TEX_FELT] = "assets/models/personajes/texturas/felt_diffuse.png",
    [TEX_LEATHER] = "assets/models/personajes/texturas/leather_diffuse.png",
    [TEX_BRONZE] = "assets/models/personajes/texturas/bronze_diffuse.png",
    [TEX_IRON] = "assets/models/personajes/texturas/iron_diffuse.png",
    [TEX_STEEL] = "assets/models/personajes/texturas/steel_diffuse.png",
    [TEX_GOLD] = "assets/models/personajes/texturas/gold_diffuse.png",
};

int equipment_load_for_slot(ArmorSlot slot, TextureMaterial tex) {
    if (slot >= SLOT_COUNT || slot < 0) return -1;
    if (tex >= TEX_COUNT || tex < 0) return -1;

    for (int i = 0; i < g_equip_cache_count; i++) {
        if (g_equip_cache[i].loaded && g_equip_cache[i].bone_index == (int)slot) {
            return i;
        }
    }

    if (g_equip_cache_count >= EQUIP_CACHE_SIZE) return -1;

    int idx = g_equip_cache_count++;
    EquipCache *cache = &g_equip_cache[idx];

    const char *path = EQUIP_PATHS[slot][tex];
    const char *asset_path = platform_asset_path(path);

    cache->model = LoadModel(asset_path);
    cache->bone_index = (int)slot;
    cache->loaded = (cache->model.meshCount > 0);

    return idx;
}

void equipment_unload_for_slot(ArmorSlot slot) {
    for (int i = 0; i < g_equip_cache_count; i++) {
        if (g_equip_cache[i].loaded && g_equip_cache[i].bone_index == (int)slot) {
            if (g_equip_cache[i].model.meshCount > 0) {
                UnloadModel(g_equip_cache[i].model);
            }
            g_equip_cache[i].loaded = 0;
        }
    }
}

void equipment_unload_all(void) {
    for (int i = 0; i < g_equip_cache_count; i++) {
        if (g_equip_cache[i].loaded && g_equip_cache[i].model.meshCount > 0) {
            UnloadModel(g_equip_cache[i].model);
        }
        g_equip_cache[i].loaded = 0;
    }
    g_equip_cache_count = 0;
}

Model *equipment_get_model(int cache_idx) {
    if (cache_idx < 0 || cache_idx >= g_equip_cache_count) return NULL;
    if (!g_equip_cache[cache_idx].loaded) return NULL;
    return &g_equip_cache[cache_idx].model;
}

int equipment_get_bone_index(int cache_idx) {
    if (cache_idx < 0 || cache_idx >= g_equip_cache_count) return -1;
    return g_equip_cache[cache_idx].bone_index;
}

void equipment_swap_texture_for_slot(ArmorSlot slot, TextureMaterial tex) {
    if (slot >= SLOT_COUNT || slot < 0) return;
    if (tex >= TEX_COUNT || tex < 0) return;

    for (int i = 0; i < g_equip_cache_count; i++) {
        if (g_equip_cache[i].loaded && g_equip_cache[i].bone_index == (int)slot) {
            Model *m = &g_equip_cache[i].model;
            if (m->meshCount > 0 && m->materialCount > 0) {
                Texture2D tex2d = LoadTexture(EQUIP_TEX_PATHS[tex]);
                if (tex2d.id > 0) {
                    m->materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = tex2d;
                    m->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
                }
            }
            return;
        }
    }
}
