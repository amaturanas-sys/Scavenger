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
        "assets/models/personajes/equipamento/cascos/armadura.casco.bronce.glb",
        "assets/models/personajes/equipamento/cascos/armadura.casco.cuero.glb",
        "assets/models/personajes/equipamento/cascos/armadura.casco.hierro.glb",
        "assets/models/personajes/equipamento/cascos/armadura.casco.acero.glb",
        "assets/models/personajes/equipamento/cascos/armadura.casco.oro.glb",
        "assets/models/personajes/equipamento/cascos/armadura.casco.fielto.glb",
    },
    [SLOT_NECK] = {
        "assets/models/personajes/equipamento/gorjales/armadura.gorjal.bronce.glb",
        "assets/models/personajes/equipamento/gorjales/armadura.gorjal.cuero.glb",
        "assets/models/personajes/equipamento/gorjales/armadura.gorjal.hierro.glb",
        "assets/models/personajes/equipamento/gorjales/armadura.gorjal.acero.glb",
        "assets/models/personajes/equipamento/gorjales/armadura.gorjal.oro.glb",
        "assets/models/personajes/equipamento/gorjales/armadura.gorjal.fielto.glb",
    },
    [SLOT_TORSO] = {
        "assets/models/personajes/equipamento/chalecos/armadura.chaleco.bronce.glb",
        "assets/models/personajes/equipamento/chalecos/armadura.chaleco.cuero.glb",
        "assets/models/personajes/equipamento/chalecos/armadura.chaleco.hierro.glb",
        "assets/models/personajes/equipamento/chalecos/armadura.chaleco.acero.glb",
        "assets/models/personajes/equipamento/chalecos/armadura.chaleco.oro.glb",
        "assets/models/personajes/equipamento/chalecos/armadura.chaleco.fielto.glb",
    },
    [SLOT_SHOULDERS] = {
        "assets/models/personajes/equipamento/hombreras/armadura.hombreras.bronce.glb",
        "assets/models/personajes/equipamento/hombreras/armadura.hombreras.cuero.glb",
        "assets/models/personajes/equipamento/hombreras/armadura.hombreras.hierro.glb",
        "assets/models/personajes/equipamento/hombreras/armadura.hombreras.acero.glb",
        "assets/models/personajes/equipamento/hombreras/armadura.hombreras.oro.glb",
        "assets/models/personajes/equipamento/hombreras/armadura.hombreras.fielto.glb",
    },
    [SLOT_BRACERS] = {
        "assets/models/personajes/equipamento/brazales/armadura.brazales.bronce.glb",
        "assets/models/personajes/equipamento/brazales/armadura.brazales.cuero.glb",
        "assets/models/personajes/equipamento/brazales/armadura.brazales.hierro.glb",
        "assets/models/personajes/equipamento/brazales/armadura.brazales.acero.glb",
        "assets/models/personajes/equipamento/brazales/armadura.brazales.oro.glb",
        "assets/models/personajes/equipamento/brazales/armadura.brazales.fielto.glb",
    },
    [SLOT_GLOVES] = {
        "assets/models/personajes/equipamento/manos/armadura.manos.bronce.glb",
        "assets/models/personajes/equipamento/manos/armadura.manos.cuero.glb",
        "assets/models/personajes/equipamento/manos/armadura.manos.hierro.glb",
        "assets/models/personajes/equipamento/manos/armadura.manos.acero.glb",
        "assets/models/personajes/equipamento/manos/armadura.manos.oro.glb",
        "assets/models/personajes/equipamento/manos/armadura.manos.fielto.glb",
    },
    [SLOT_SKIRT] = {
        "assets/models/personajes/equipamento/faldas/armadura.faldas.bronce.glb",
        "assets/models/personajes/equipamento/faldas/armadura.faldas.cuero.glb",
        "assets/models/personajes/equipamento/faldas/armadura.faldas.hierro.glb",
        "assets/models/personajes/equipamento/faldas/armadura.faldas.acero.glb",
        "assets/models/personajes/equipamento/faldas/armadura.faldas.oro.glb",
        "assets/models/personajes/equipamento/faldas/armadura.faldas.fielto.glb",
    },
    [SLOT_GREAVES] = {
        "assets/models/personajes/equipamento/grebas/armadura.grebas.bronce.glb",
        "assets/models/personajes/equipamento/grebas/armadura.grebas.cuero.glb",
        "assets/models/personajes/equipamento/grebas/armadura.grebas.hierro.glb",
        "assets/models/personajes/equipamento/grebas/armadura.grebas.acero.glb",
        "assets/models/personajes/equipamento/grebas/armadura.grebas.oro.glb",
        "assets/models/personajes/equipamento/grebas/armadura.grebas.fielto.glb",
    },
    [SLOT_BOOTS] = {
        "assets/models/personajes/equipamento/piernas/armadura.piernas.bronce.glb",
        "assets/models/personajes/equipamento/piernas/armadura.piernas.cuero.glb",
        "assets/models/personajes/equipamento/piernas/armadura.piernas.hierro.glb",
        "assets/models/personajes/equipamento/piernas/armadura.piernas.acero.glb",
        "assets/models/personajes/equipamento/piernas/armadura.piernas.oro.glb",
        "assets/models/personajes/equipamento/piernas/armadura.piernas.fielto.glb",
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
