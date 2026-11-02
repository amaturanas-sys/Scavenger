// Generador Nodal de Estructuras - Implementación con validaciones

#include "camp.h"

#include <math.h>
#include <string.h>

#include "../sim/rng.h"

void camp_clear(Camp *camp) {
    if (!camp) return;
    memset(camp, 0, sizeof(*camp));
}

const CampNode* camp_get_node(const Camp *camp, int index) {
    if (!camp || index < 0 || index >= camp->node_count) {
        return NULL;
    }
    return &camp->nodes[index];
}

int camp_find_nearest(const Camp *camp, float x, float z, float max_dist) {
    if (!camp || camp->node_count == 0) {
        return -1;
    }
    int nearest = -1;
    float min_dist = max_dist * max_dist;
    
    for (int i = 0; i < camp->node_count; i++) {
        const CampNode *node = &camp->nodes[i];
        float dx = node->pos.x - x;
        float dz = node->pos.z - z;
        float dist = dx * dx + dz * dz;
        
        if (dist < min_dist) {
            min_dist = dist;
            nearest = i;
        }
    }
    
    return nearest;
}

float camp_node_distance(const Camp *camp, int i, int j) {
    if (!camp || i < 0 || j < 0 || i >= camp->node_count || j >= camp->node_count) {
        return 0.0f;
    }
    const CampNode *a = &camp->nodes[i];
    const CampNode *b = &camp->nodes[j];
    float dx = a->pos.x - b->pos.x;
    float dz = a->pos.z - b->pos.z;
    return sqrtf(dx * dx + dz * dz);
}

bool camp_nodes_overlap(const Camp *camp, int i, int j, float min_spacing) {
    if (!camp || i < 0 || j < 0 || i >= camp->node_count || j >= camp->node_count) {
        return false;
    }
    float dist = camp_node_distance(camp, i, j);
    return dist < min_spacing;
}

bool camp_nodes_fit(const Camp *camp, int i, int j, float min_spacing) {
    if (!camp || i < 0 || j < 0 || i >= camp->node_count || j >= camp->node_count) {
        return false;
    }
    float dist = camp_node_distance(camp, i, j);
    return dist >= min_spacing;
}

void camp_generate(Camp *camp, const CampGenConfig *config, uint32_t node_seed) {
    if (!camp || !config) {
        return;
    }
    
    int yurt_count = config->yurt_count;
    int caravan_count = config->caravan_count;
    int palisade_count = config->palisade_count;
    int fort_count = config->fort_count;
    
    if (yurt_count < 0 || caravan_count < 0 || palisade_count < 0 || fort_count < 0) {
        return;
    }
    
    int total = yurt_count + caravan_count + palisade_count + fort_count;
    
    if (total == 0 || total > CAMP_MAX_NODES) {
        return;
    }
    
    if (config->circle_radius < 0.0f) {
        return;
    }
    
    camp_clear(camp);
    camp->node_count = total;
    camp->area_min_x = camp->area_max_x = 0.0f;
    camp->area_min_z = camp->area_max_z = 0.0f;
    
    Rng rng;
    rng_seed(&rng, node_seed);
    
    if (config->circular) {
        float angle_step = 2.0f * CAMP_PI / total;
        float radius = config->circle_radius;
        
        for (int i = 0; i < total; i++) {
            CampNode *node = &camp->nodes[i];
            node->seed = rng_next(&rng);
            
            float angle = i * angle_step;
            float offset = rng_float(&rng) * 0.3f;
            float r = radius * (1.0f + offset - 0.15f);
            
            node->pos.x = r * cosf(angle);
            node->pos.z = r * sinf(angle);
            node->pos.y = 0.0f;
            node->pos.yaw = angle + CAMP_PI;
            node->pos.pitch = 0.0f;
            node->scale = 1.0f;
            
            if (i < yurt_count) {
                node->type = CAMP_YURT;
            } else if (i < yurt_count + caravan_count) {
                node->type = CAMP_CARAVAN;
            } else if (i < yurt_count + caravan_count + palisade_count) {
                node->type = CAMP_PALISADE;
            } else {
                node->type = CAMP_FORT;
            }
        }
    } else {
        float spacing = 12.0f;
        float start_x = -((total - 1) * spacing) * 0.5f;
        
        for (int i = 0; i < total; i++) {
            CampNode *node = &camp->nodes[i];
            node->seed = rng_next(&rng);
            
            node->pos.x = start_x + i * spacing;
            node->pos.y = 0.0f;
            node->pos.z = 0.0f;
            node->pos.yaw = 0.0f;
            node->pos.pitch = 0.0f;
            node->scale = 1.0f;
            
            if (i < yurt_count) {
                node->type = CAMP_YURT;
            } else if (i < yurt_count + caravan_count) {
                node->type = CAMP_CARAVAN;
            } else if (i < yurt_count + caravan_count + palisade_count) {
                node->type = CAMP_PALISADE;
            } else {
                node->type = CAMP_FORT;
            }
        }
    }
    
    for (int i = 0; i < camp->node_count; i++) {
        const CampNode *node = &camp->nodes[i];
        camp->area_min_x = fminf(camp->area_min_x, node->pos.x);
        camp->area_max_x = fmaxf(camp->area_max_x, node->pos.x);
        camp->area_min_z = fminf(camp->area_min_z, node->pos.z);
        camp->area_max_z = fmaxf(camp->area_max_z, node->pos.z);
    }
}

void camp_serialize(const Camp *camp, CampNodeCompact *out) {
    if (!camp || !out) {
        return;
    }
    
    int count = camp->node_count;
    if (count > CAMP_MAX_NODES) {
        count = CAMP_MAX_NODES;
    }
    
    for (int i = 0; i < count; i++) {
        const CampNode *src = &camp->nodes[i];
        CampNodeCompact *dst = &out[i];
        
        dst->x = src->pos.x;
        dst->y = src->pos.y;
        dst->z = src->pos.z;
        dst->yaw = src->pos.yaw;
        dst->pitch = src->pos.pitch;
        dst->scale = src->scale;
        dst->type = (uint8_t)src->type;
        dst->seed = src->seed;
    }
}

int camp_deserialize(const CampNodeCompact *nodes, int count, Camp *out) {
    if (!out) {
        return 0;
    }
    
    camp_clear(out);
    
    if (count < 0) {
        return 0;
    }
    
    if (count > CAMP_MAX_NODES) {
        count = CAMP_MAX_NODES;
    }
    
    for (int i = 0; i < count; i++) {
        const CampNodeCompact *src = &nodes[i];
        
        if (src->type >= CAMP_KINDS) {
            continue;
        }
        
        CampNode *dst = &out->nodes[i];
        
        dst->pos.x = src->x;
        dst->pos.y = src->y;
        dst->pos.z = src->z;
        dst->pos.yaw = src->yaw;
        dst->pos.pitch = src->pitch;
        dst->scale = src->scale;
        dst->type = (CampStructType)src->type;
        dst->seed = src->seed;
    }
    
    out->node_count = count;
    
    // Recalcular bounding box
    for (int i = 0; i < out->node_count; i++) {
        const CampNode *node = &out->nodes[i];
        out->area_min_x = fminf(out->area_min_x, node->pos.x);
        out->area_max_x = fmaxf(out->area_max_x, node->pos.x);
        out->area_min_z = fminf(out->area_min_z, node->pos.z);
        out->area_max_z = fmaxf(out->area_max_z, node->pos.z);
    }
    
    return count;
}