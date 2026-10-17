// Sistema de vegetación procedimental para SCAVENGERS THRIVE
// Maneja pasto, maleza y follaje con instanced rendering, LOD y shaders de viento.

#include "vegetation.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"
#include "platform.h"
#include "raymath.h"

// Malla de cross-quad (cuatro vértices formando una cruz)
static const Vector3 CROSS_QUAD_VERTICES[VEGETATION_CROSS_QUAD_VERTICES] = {
    {0.0f, 0.0f, 0.0f},   // Centro
    {0.0f, 0.0f, 1.0f},   // Arriba (punta)
    {0.0f, 0.0f, -1.0f},  // Abajo (raíz)
    {1.0f, 0.0f, 0.0f},   // Derecha
};

// Índices de triángulos para el cross-quad
static const unsigned int CROSS_QUAD_INDICES[6] = {
    0, 1, 2,  // Triángulo superior
    0, 2, 3   // Triángulo inferior
};

// Colores base por tipo de vegetación
static const Color GRASS_COLORS[] = {
    {76, 175, 80, 255},  // Verde fresco
    {102, 187, 106, 255},
    {129, 199, 132, 255},
    {139, 195, 74, 255}
};

static const Color WEED_COLORS[] = {
    {120, 144, 156, 255},  // Gris verdoso
    {144, 164, 174, 255},
    {158, 158, 158, 255}
};

static const Color FOLIAGE_COLORS[] = {
    {34, 139, 34, 255},   // Verde bosque
    {50, 205, 50, 255},
    {60, 179, 113, 255},
    {46, 125, 50, 255}
};

// Colores estacionales
static const Color SEASON_COLORS[SEASON_COUNT][VEGETATION_COUNT][3] = {
    // Primavera
    {
        {100, 180, 100, 255},  // Grass
        {140, 150, 160, 255},  // Weed
        {60, 160, 60, 255},    // Foliage
    },
    // Verano
    {
        {76, 175, 80, 255},    // Grass
        {120, 144, 156, 255},  // Weed
        {34, 139, 34, 255},    // Foliage
    },
    // Otoño
    {
        {210, 180, 140, 255},  // Grass (dorado)
        {180, 160, 130, 255},  // Weed
        {160, 100, 50, 255},   // Foliage (naranja)
    },
    // Invierno
    {
        {245, 245, 255, 255},  // Grass (blanco)
        {220, 220, 230, 255},  // Weed
        {200, 200, 220, 255},  // Foliage (azulado)
    }
};

// Colores por bioma
static const Color BIOME_COLORS[BIOME_COUNT][VEGETATION_COUNT][3] = {
    // Estepa
    {
        {101, 67, 33, 255},    // Grass (marrón seco)
        {90, 70, 60, 255},     // Weed
        {34, 139, 34, 255},    // Foliage (limitado)
    },
    // Bosque
    {
        {76, 175, 80, 255},    // Grass
        {120, 144, 156, 255},  // Weed
        {34, 139, 34, 255},    // Foliage
    },
    // Altiplano
    {
        {139, 119, 101, 255},  // Grass (arena)
        {160, 140, 120, 255},  // Weed
        {80, 120, 80, 255},    // Foliage
    }
};

void vegetation_init(VegetationSystem *vs, int seed) {
    if (!vs) return;
    
    memset(vs, 0, sizeof(*vs));
    vs->seed = seed;
    vs->wind_speed = 0.5f;
    vs->enabled = true;
    
    // Crear malla del cross-quad
    vs->mesh = GenMeshFromVertices(CROSS_QUAD_VERTICES, VEGETATION_CROSS_QUAD_VERTICES,
                                    CROSS_QUAD_INDICES, 6);
    
    // Crear shader de viento
    vs->shader = LoadShaderFromMemory(
        NULL,
        "attribute vec3 position; attribute vec3 color; attribute float phase; "
        "uniform mat4 modelViewProjection; uniform float time; uniform float windSpeed; "
        "uniform vec3 color; varying vec3 vColor; varying float vPhase; "
        "void main() { "
        "    vec3 pos = position; "
        "    float tipY = pos.y; "
        "    float windOffset = sin(time * windSpeed + phase) * 0.1 * tipY; "
        "    pos.x += windOffset; "
        "    gl_Position = modelViewProjection * vec4(pos, 1.0); "
        "    vColor = color; "
        "    vPhase = phase; "
        "}"
    );
    
    vs->time_loc = GetShaderLocation(vs->shader, "time");
    vs->color_loc = GetShaderLocation(vs->shader, "color");
    vs->use_wind_loc = GetShaderLocation(vs->shader, "windSpeed");
    
    vs->initialized = true;
}

void vegetation_unload(VegetationSystem *vs) {
    if (!vs || !vs->initialized) return;
    
    if (vs->shader.id > 0) UnloadShader(vs->shader);
    if (vs->mesh.vertexCount > 0) UnloadMesh(vs->mesh);
    
    memset(vs, 0, sizeof(*vs));
}

Color vegetation_get_season_color(Season season, VegetationType type) {
    if (season < 0 || season >= SEASON_COUNT || type < 0 || type >= VEGETATION_COUNT) {
        return WHITE;
    }
    return (Color){
        SEASON_COLORS[season][type][0],
        SEASON_COLORS[season][type][1],
        SEASON_COLORS[season][type][2],
        255
    };
}

Color vegetation_get_biome_color(Biome biome, VegetationType type) {
    if (biome < 0 || biome >= BIOME_COUNT || type < 0 || type >= VEGETATION_COUNT) {
        return WHITE;
    }
    return (Color){
        BIOME_COLORS[biome][type][0],
        BIOME_COLORS[biome][type][1],
        BIOME_COLORS[biome][type][2],
        255
    };
}

void vegetation_set_wind_speed(VegetationSystem *vs, float speed) {
    if (!vs) return;
    vs->wind_speed = fminf(fmaxf(speed, 0.0f), 1.0f);
}

void vegetation_set_season_tint(VegetationSystem *vs, Season season) {
    if (!vs) return;
    for (int i = 0; i < vs->instance_count && i < VEGETATION_MAX_INSTANCES; i++) {
        VegetationType type = vs->instances[i].type;
        vs->instances[i].color = vegetation_get_season_color(season, type);
    }
}

void vegetation_set_biome(VegetationSystem *vs, Biome biome) {
    if (!vs) return;
    for (int i = 0; i < vs->instance_count && i < VEGETATION_MAX_INSTANCES; i++) {
        VegetationType type = vs->instances[i].type;
        vs->instances[i].color = vegetation_get_biome_color(biome, type);
    }
}

// Generación procedimental simple usando ruido pseudo-aleatorio
static float pseudo_noise(float x, float z, int seed) {
    int n = (int)(x * 3711.0f + z * 6243.0f + (float)seed);
    n = (n << 13) ^ n;
    return ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / (float)0x7fffffff;
}

static float noise_2d(float x, float z, int seed, float scale) {
    float ix = floorf(x * scale);
    float iz = floorf(z * scale);
    float fx = x * scale - ix;
    float fz = z * scale - iz;
    
    float a = pseudo_noise(ix, iz, seed);
    float b = pseudo_noise(ix + 1, iz, seed);
    float c = pseudo_noise(ix, iz + 1, seed);
    float d = pseudo_noise(ix + 1, iz + 1, seed);
    
    float u = fx * fx * (3.0f - 2.0f * fx);
    float v = fz * fz * (3.0f - 2.0f * fz);
    
    return vegetation_lerp(vegetation_lerp(a, b, u), vegetation_lerp(c, d, u), v);
}

void vegetation_populate(VegetationSystem *vs, const Vector3 *center, float radius,
                          int count, VegetationType type, int seed) {
    if (!vs || vs->instance_count >= VEGETATION_MAX_INSTANCES) return;
    
    int remaining = count;
    int offset = vs->instance_count;
    
    for (int i = 0; i < remaining && vs->instance_count < VEGETATION_MAX_INSTANCES; i++) {
        float angle = pseudo_noise(i, 0, seed) * 2.0f * PI;
        float r = sqrtf(pseudo_noise(i, 1, seed)) * radius;
        
        Vector3 pos = {
            center->x + cosf(angle) * r,
            center->y,
            center->z + sinf(angle) * r
        };
        
        float height = 0.5f + noise_2d(pos.x, pos.z, seed + i, 0.1f) * 0.5f;
        
        vs->instances[vs->instance_count] = (VegetationInstance){
            .position = pos,
            .scale = 0.5f + noise_2d(pos.x, pos.z, seed + i + 1000, 0.5f) * 0.5f,
            .rotation_y = pseudo_noise(i, 2, seed) * 360.0f,
            .wind_phase = pseudo_noise(i, 3, seed) * 2.0f * PI,
            .color = (type == VEGETATION_GRASS) ? GRASS_COLORS[0] :
                     (type == VEGETATION_WEED) ? WEED_COLORS[0] : FOLIAGE_COLORS[0],
            .type = type,
            .height = height
        };
        
        vs->instance_count++;
    }
}

void vegetation_populate_area(VegetationSystem *vs, const BoundingBox *area,
                               int count, VegetationType type, int seed) {
    if (!vs || !area || vs->instance_count >= VEGETATION_MAX_INSTANCES) return;
    
    int remaining = count;
    int offset = vs->instance_count;
    
    for (int i = 0; i < remaining && vs->instance_count < VEGETATION_MAX_INSTANCES; i++) {
        Vector3 pos = {
            area->min.x + noise_2d(i, 0, seed) * (area->max.x - area->min.x),
            area->min.y + noise_2d(i, 1, seed) * (area->max.y - area->min.y),
            area->min.z + noise_2d(i, 2, seed) * (area->max.z - area->min.z)
        };
        
        float height = 0.5f + noise_2d(pos.x, pos.z, seed + i, 0.1f) * 0.5f;
        
        vs->instances[vs->instance_count] = (VegetationInstance){
            .position = pos,
            .scale = 0.5f + noise_2d(pos.x, pos.z, seed + i + 1000, 0.5f) * 0.5f,
            .rotation_y = noise_2d(pos.x, pos.z, seed + i + 2000, 1.0f) * 360.0f,
            .wind_phase = noise_2d(pos.x, pos.z, seed + i + 3000, 1.0f) * 2.0f * PI,
            .color = (type == VEGETATION_GRASS) ? GRASS_COLORS[0] :
                     (type == VEGETATION_WEED) ? WEED_COLORS[0] : FOLIAGE_COLORS[0],
            .type = type,
            .height = height
        };
        
        vs->instance_count++;
    }
}
void vegetation_draw(VegetationSystem *vs, Camera camera, float time,
                     float wind_intensity, Color tint_override) {
    if (!vs || !vs->initialized || !vs->enabled || vs->instance_count == 0) return;
    
    Vector3 cam_pos = camera.position;
    int visible_count = 0;
    Matrix transforms[VEGETATION_MAX_INSTANCES];
    
    for (int i = 0; i < vs->instance_count; i++) {
        const VegetationInstance *inst = &vs->instances[i];
        
        float dist = Vector3Distance(cam_pos, inst->position);
        
        if (dist < VEGETATION_LOD_FAR) {
            Matrix scale_m = MatrixScale(inst->scale, inst->height, inst->scale);
            Matrix rot_m = MatrixRotateY(inst->rotation_y * DEG2RAD);
            Matrix trans_m = MatrixTranslate(inst->position.x, inst->position.y, inst->position.z);
            
            transforms[visible_count] = MatrixMultiply(scale_m, trans_m);
            transforms[visible_count] = MatrixMultiply(transforms[visible_count], rot_m);
            
            visible_count++;
        }
    }
    
    if (visible_count == 0) return;
    
    if (vs->shader.id > 0 && wind_intensity > 0.01f) {
        BeginShaderMode(vs->shader);
        SetShaderValue(vs->shader, vs->time_loc, &time, SHADER_UNIFORM_FLOAT);
        SetShaderValue(vs->shader, vs->use_wind_loc, &wind_intensity, SHADER_UNIFORM_FLOAT);
        
        for (int i = 0; i < visible_count; i++) {
            Color col = vs->instances[i].color;
            if (tint_override.a > 0) {
                col = vegetation_color_lerp(col, tint_override, 0.5f);
            }
            SetShaderValue(vs->shader, vs->color_loc, &col, SHADER_UNIFORM_VEC3);
            DrawMeshInstanced(vs->mesh, vs->shader, &transforms[i], 1);
        }
        
        EndShaderMode();
    } else {
        for (int i = 0; i < visible_count; i++) {
            DrawMesh(vs->mesh, vs->instances[i].color, transforms[i]);
        }
    }
}

void vegetation_draw_all(VegetationSystem *vs, Camera camera, float time,
                          float wind_intensity, Color tint_override) {
    if (!vs || !vs->initialized || !vs->enabled || vs->instance_count == 0) return;
    
    Vector3 cam_pos = camera.position;
    
    for (int type = 0; type < VEGETATION_COUNT; type++) {
        int visible_count = 0;
        Matrix transforms[VEGETATION_MAX_INSTANCES];
        
        for (int i = 0; i < vs->instance_count; i++) {
            if (vs->instances[i].type != type) continue;
            
            float dist = Vector3Distance(cam_pos, vs->instances[i].position);
            
            if (dist < VEGETATION_LOD_FAR) {
                Matrix scale_m = MatrixScale(vs->instances[i].scale, vs->instances[i].height, vs->instances[i].scale);
                Matrix rot_m = MatrixRotateY(vs->instances[i].rotation_y * DEG2RAD);
                Matrix trans_m = MatrixTranslate(vs->instances[i].position.x, vs->instances[i].position.y, vs->instances[i].position.z);
                
                transforms[visible_count] = MatrixMultiply(scale_m, trans_m);
                transforms[visible_count] = MatrixMultiply(transforms[visible_count], rot_m);
                
                visible_count++;
            }
        }
        
        if (visible_count == 0) continue;
        
        if (vs->shader.id > 0 && wind_intensity > 0.01f) {
            BeginShaderMode(vs->shader);
            SetShaderValue(vs->shader, vs->time_loc, &time, SHADER_UNIFORM_FLOAT);
            SetShaderValue(vs->shader, vs->use_wind_loc, &wind_intensity, SHADER_UNIFORM_FLOAT);
            
            for (int i = 0; i < visible_count; i++) {
                Color col = vs->instances[i].color;
                if (tint_override.a > 0) {
                    col = vegetation_color_lerp(col, tint_override, 0.5f);
                }
                SetShaderValue(vs->shader, vs->color_loc, &col, SHADER_UNIFORM_VEC3);
                DrawMeshInstanced(vs->mesh, vs->shader, &transforms[i], 1);
            }
            
            EndShaderMode();
        } else {
            for (int i = 0; i < visible_count; i++) {
                DrawMesh(vs->mesh, vs->instances[i].color, transforms[i]);
            }
        }
    }
}