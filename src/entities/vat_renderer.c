// VAT Renderer - Hybrid Animation System
// Vertex Animation Textures (VAT) with Multi-Backend Shaders
// ---------------------------------------------------------------------
// Supports both GLSL 330 (desktop) and GLSL ES 100 (Android).
// Renders static meshes with per-frame vertex animation using textures.
// ---------------------------------------------------------------------
#include "vat_renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "rlgl.h"

// Default configuration
#define VAT_MAX_INSTANCES 256
#define VAT_MAX_TEXTURES 64
#define VAT_MAX_MESHES 64
#define VAT_MAX_INSTANCE_TRANSFORMS 4096
#define VAT_DEFAULT_ANIM_SPEED 1.0f
#define VAT_DEFAULT_SCALE 1.0f

// Error reporting
static char g_shader_error[256] = { 0 };

const char *vat_renderer_get_shader_error(VatRenderer *vr) {
    if (vr && vr->gl100_shader.id == 0) {
        return g_shader_error;
    }
    return NULL;
}

bool vat_renderer_shader_loaded(VatRenderer *vr) {
    return (vr && vr->gl330_shader.id != 0 && vr->gl100_shader.id != 0);
}

bool vat_renderer_android_instancing_supported(VatRenderer *vr) {
    return vr && vr->android_instancing_supported;
}

void vat_renderer_init(VatRenderer *vr, int max_instances) {
    memset(vr, 0, sizeof(VatRenderer));
    
    vr->max_instances = max_instances > 0 ? max_instances : VAT_MAX_INSTANCES;
    vr->instance_count = 0;
    vr->max_textures = VAT_MAX_TEXTURES;
    vr->max_meshes = VAT_MAX_MESHES;
    vr->max_instance_transforms = VAT_MAX_INSTANCE_TRANSFORMS;
    vr->android_instancing_supported = false;
    vr->android_shader_loaded = false;
    
    // Initialize arrays
    vr->textures = (Texture2D *)calloc(vr->max_textures, sizeof(Texture2D));
    vr->meshes = (Model *)calloc(vr->max_meshes, sizeof(Model));
    vr->materials = (Material *)calloc(vr->max_meshes, sizeof(Material));
    vr->instanceTransforms = (Matrix *)calloc(vr->max_instance_transforms, sizeof(Matrix));
    
    // Initialize materials with default
    for (int i = 0; i < vr->max_meshes; i++) {
        vr->materials[i] = LoadMaterialDefault();
        vr->materials[i].shader = (Shader){ 0 };
    }
}

void vat_renderer_shutdown(VatRenderer *vr) {
    if (!vr) return;
    
    // Unload shaders
    if (vr->gl330_shader.id != 0) {
        UnloadShader(vr->gl330_shader);
    }
    if (vr->gl100_shader.id != 0) {
        UnloadShader(vr->gl100_shader);
    }
    
    // Unload textures
    for (int i = 0; i < vr->max_textures; i++) {
        if (vr->textures[i].id != 0) {
            UnloadTexture(vr->textures[i]);
        }
    }
    
    // Unload meshes
    for (int i = 0; i < vr->max_meshes; i++) {
        if (vr->meshes[i].meshCount > 0) {
            UnloadModel(vr->meshes[i]);
        }
    }
    
    // Unload materials
    for (int i = 0; i < vr->max_meshes; i++) {
        if (vr->materials[i].shader.id != 0) {
            UnloadMaterial(vr->materials[i]);
        }
    }
    
    // Free arrays
    free(vr->textures);
    free(vr->meshes);
    free(vr->materials);
    free(vr->instanceTransforms);
    
    memset(vr, 0, sizeof(VatRenderer));
}

int vat_renderer_load_shader_gl330(VatRenderer *vr, const char *vs_path, const char *fs_path) {
    if (!vr || !vs_path || !fs_path) {
        strncpy(g_shader_error, "Invalid shader path", sizeof(g_shader_error) - 1);
        return -1;
    }
    
    // Unload existing shader
    if (vr->gl330_shader.id != 0) {
        UnloadShader(vr->gl330_shader);
    }
    
    const char *asset_vs = platform_asset_path(vs_path);
    const char *asset_fs = platform_asset_path(fs_path);
    
    vr->gl330_shader = LoadShader(asset_vs, asset_fs);
    
    if (vr->gl330_shader.id == 0) {
        snprintf(g_shader_error, sizeof(g_shader_error),
                 "Failed to load GLSL 330 shader: %s, %s", vs_path, fs_path);
        return -1;
    }
    
    // Cache uniform locations for GLSL 330
    vr->loc_matModel = GetShaderLocation(vr->gl330_shader, "matModel");
    vr->loc_matView = GetShaderLocation(vr->gl330_shader, "matView");
    vr->loc_matProjection = GetShaderLocation(vr->gl330_shader, "matProjection");
    vr->loc_vatPositionMin = GetShaderLocation(vr->gl330_shader, "vatPositionMin");
    vr->loc_vatPositionExtent = GetShaderLocation(vr->gl330_shader, "vatPositionExtent");
    vr->loc_vatFrame = GetShaderLocation(vr->gl330_shader, "vatFrame");
    vr->loc_vatFrameCount = GetShaderLocation(vr->gl330_shader, "vatFrameCount");
    vr->loc_vatVertexCount = GetShaderLocation(vr->gl330_shader, "vatVertexCount");
    vr->loc_matDiffuseColor = GetShaderLocation(vr->gl330_shader, "matDiffuseColor");
    vr->loc_lightDir = GetShaderLocation(vr->gl330_shader, "lightDir");
    vr->loc_lightColor = GetShaderLocation(vr->gl330_shader, "lightColor");
    vr->loc_ambientColor = GetShaderLocation(vr->gl330_shader, "ambientColor");
    vr->loc_ambientIntensity = GetShaderLocation(vr->gl330_shader, "ambientIntensity");
    vr->loc_texture0 = GetShaderLocation(vr->gl330_shader, "texture0");
    
    return 0;
}

int vat_renderer_load_shader_gl100(VatRenderer *vr, const char *vs_path, const char *fs_path) {
    if (!vr || !vs_path || !fs_path) {
        strncpy(g_shader_error, "Invalid shader path", sizeof(g_shader_error) - 1);
        return -1;
    }
    
    // Unload existing shader
    if (vr->gl100_shader.id != 0) {
        UnloadShader(vr->gl100_shader);
    }
    
    const char *asset_vs = platform_asset_path(vs_path);
    const char *asset_fs = platform_asset_path(fs_path);
    
    vr->gl100_shader = LoadShader(asset_vs, asset_fs);
    
    if (vr->gl100_shader.id == 0) {
        snprintf(g_shader_error, sizeof(g_shader_error),
                 "Failed to load GLSL ES 100 shader: %s, %s", vs_path, fs_path);
        return -1;
    }
    
    // Cache uniform locations for GLSL ES 100
    vr->loc_matModel = GetShaderLocation(vr->gl100_shader, "matModel");
    vr->loc_matView = GetShaderLocation(vr->gl100_shader, "matView");
    vr->loc_matProjection = GetShaderLocation(vr->gl100_shader, "matProjection");
    vr->loc_vatPositionMin = GetShaderLocation(vr->gl100_shader, "vatPositionMin");
    vr->loc_vatPositionExtent = GetShaderLocation(vr->gl100_shader, "vatPositionExtent");
    vr->loc_vatFrame = GetShaderLocation(vr->gl100_shader, "vatFrame");
    vr->loc_vatFrameCount = GetShaderLocation(vr->gl100_shader, "vatFrameCount");
    vr->loc_vatVertexCount = GetShaderLocation(vr->gl100_shader, "vatVertexCount");
    vr->loc_matDiffuseColor = GetShaderLocation(vr->gl100_shader, "matDiffuseColor");
    vr->loc_lightDir = GetShaderLocation(vr->gl100_shader, "lightDir");
    vr->loc_lightColor = GetShaderLocation(vr->gl100_shader, "lightColor");
    vr->loc_ambientColor = GetShaderLocation(vr->gl100_shader, "ambientColor");
    vr->loc_ambientIntensity = GetShaderLocation(vr->gl100_shader, "ambientIntensity");
    vr->loc_texture0 = GetShaderLocation(vr->gl100_shader, "texture0");
    
    vr->android_shader_loaded = true;
    
    return 0;
}

int vat_renderer_load_mesh(VatRenderer *vr, const char *path) {
    if (!vr || !path) {
        return -1;
    }
    
    // Find first free mesh slot
    int idx = -1;
    for (int i = 0; i < vr->max_meshes; i++) {
        if (vr->meshes[i].meshCount == 0) {
            idx = i;
            break;
        }
    }
    
    if (idx < 0) {
        return -1;
    }
    
    const char *asset_path = platform_asset_path(path);
    vr->meshes[idx] = LoadModel(asset_path);
    
    if (vr->meshes[idx].meshCount == 0) {
        return -1;
    }
    
    // Initialize material for this mesh
    vr->materials[idx] = LoadMaterialDefault();
    vr->materials[idx].shader = (Shader){ 0 };
    
    return idx;
}

int vat_renderer_load_vat_texture(VatRenderer *vr, const char *path, int frame_count) {
    if (!vr || !path || frame_count <= 0) {
        return -1;
    }
    
    // Find first free texture slot
    int idx = -1;
    for (int i = 0; i < vr->max_textures; i++) {
        if (vr->textures[i].id == 0) {
            idx = i;
            break;
        }
    }
    
    if (idx < 0) {
        return -1;
    }
    
    const char *asset_path = platform_asset_path(path);
    vr->textures[idx] = LoadTexture(asset_path);
    
    if (vr->textures[idx].id == 0) {
        return -1;
    }
    
    return idx;
}

int vat_renderer_add_instance(VatRenderer *vr, int archetype_id) {
    if (!vr || vr->instance_count >= vr->max_instances) {
        return -1;
    }
    
    int idx = vr->instance_count++;
    VatInstance *inst = (VatInstance *)&vr->instanceTransforms[idx];
    
    memset(inst, 0, sizeof(VatInstance));
    inst->enabled = 1;
    inst->archetype_id = archetype_id;
    inst->mesh_idx = -1;
    inst->texture_idx = -1;
    inst->frame_count = 1;
    inst->current_frame = 0;
    inst->anim_time = 0.0f;
    inst->anim_speed = VAT_DEFAULT_ANIM_SPEED;
    inst->position = (Vector3){ 0, 0, 0 };
    inst->yaw = 0.0f;
    inst->pitch = 0.0f;
    inst->roll = 0.0f;
    inst->scale = VAT_DEFAULT_SCALE;
    inst->dead = false;
    
    return idx;
}

void vat_renderer_remove_instance(VatRenderer *vr, int instance_idx) {
    if (!vr || instance_idx < 0 || instance_idx >= vr->instance_count) {
        return;
    }
    
    // Shift remaining instances
    for (int i = instance_idx; i < vr->instance_count - 1; i++) {
        vr->instanceTransforms[i] = vr->instanceTransforms[i + 1];
    }
    
    vr->instance_count--;
}

void vat_renderer_clear_instances(VatRenderer *vr) {
    vr->instance_count = 0;
}

void vat_renderer_update_instance(VatRenderer *vr, int instance_idx, float dt) {
    if (!vr || instance_idx < 0 || instance_idx >= vr->instance_count) {
        return;
    }
    
    VatInstance *inst = (VatInstance *)&vr->instanceTransforms[instance_idx];
    if (!inst->enabled) {
        return;
    }
    
    inst->anim_time += dt * inst->anim_speed;
    
    if (inst->frame_count > 0) {
        inst->current_frame = (int)inst->anim_time % inst->frame_count;
    }
}

void vat_renderer_set_instance_frame(VatRenderer *vr, int instance_idx, int frame) {
    if (!vr || instance_idx < 0 || instance_idx >= vr->instance_count) {
        return;
    }
    
    VatInstance *inst = (VatInstance *)&vr->instanceTransforms[instance_idx];
    inst->current_frame = frame;
    inst->anim_time = (float)frame;
}

void vat_renderer_set_instance_animation(VatRenderer *vr, int instance_idx, float anim_speed) {
    if (!vr || instance_idx < 0 || instance_idx >= vr->instance_count) {
        return;
    }
    
    VatInstance *inst = (VatInstance *)&vr->instanceTransforms[instance_idx];
    inst->anim_speed = anim_speed;
}

void vat_renderer_draw(VatRenderer *vr) {
    if (!vr || vr->instance_count == 0) {
        return;
    }
    
    // Select shader based on platform
    Shader shader = vr->gl330_shader;
    #if defined(PLATFORM_ANDROID)
    if (vr->gl100_shader.id != 0) {
        shader = vr->gl100_shader;
    }
    #endif
    
    if (shader.id == 0) {
        return;
    }
    
    // Set up lighting uniforms
    SetShaderValue(shader, vr->loc_lightDir, (float[3]){ 0.5, 1.0, 0.5 }, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, vr->loc_lightColor, (float[3]){ 1.0, 0.95, 0.9 }, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, vr->loc_ambientColor, (float[3]){ 0.3, 0.3, 0.35 }, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, vr->loc_ambientIntensity, &(float){ 0.6f }, SHADER_UNIFORM_FLOAT);
    
    // Draw all enabled instances
    for (int i = 0; i < vr->instance_count; i++) {
        vat_renderer_draw_instance(vr, i);
    }
}

void vat_renderer_draw_instance(VatRenderer *vr, int instance_idx) {
    if (!vr || instance_idx < 0 || instance_idx >= vr->instance_count) {
        return;
    }
    
    VatInstance *inst = (VatInstance *)&vr->instanceTransforms[instance_idx];
    if (!inst->enabled) {
        return;
    }
    
    if (inst->mesh_idx < 0 || inst->mesh_idx >= vr->max_meshes) {
        return;
    }
    
    Model *mesh = &vr->meshes[inst->mesh_idx];
    if (mesh->meshCount == 0) {
        return;
    }
    
    // Select shader
    Shader shader = vr->gl330_shader;
    #if defined(PLATFORM_ANDROID)
    if (vr->gl100_shader.id != 0) {
        shader = vr->gl100_shader;
    }
    #endif
    
    if (shader.id == 0) {
        return;
    }
    
    // Set shader
    vr->materials[inst->mesh_idx].shader = shader;
    
    // Build model matrix using raylib's transformation API
    rlPushMatrix();
    rlRotatef(inst->yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
    rlRotatef(inst->pitch * RAD2DEG, 1.0f, 0.0f, 0.0f);
    rlRotatef(inst->roll * RAD2DEG, 0.0f, 0.0f, 1.0f);
    rlScalef(inst->scale, inst->scale, inst->scale);
    rlTranslatef(inst->position.x, inst->position.y, inst->position.z);
    Matrix model = rlGetMatrixModelview();
    rlPopMatrix();
    
    // Get view and projection matrices from rlgl
    Matrix view = rlGetMatrixModelview();
    Matrix projection = rlGetMatrixProjection();
    
    // Set uniforms
    SetShaderValueMatrix(shader, vr->loc_matModel, model);
    SetShaderValueMatrix(shader, vr->loc_matView, view);
    SetShaderValueMatrix(shader, vr->loc_matProjection, projection);
    SetShaderValue(shader, vr->loc_vatPositionMin, (float[3]){ 0, 0, 0 }, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, vr->loc_vatPositionExtent, (float[3]){ 1, 1, 1 }, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, vr->loc_vatFrame, &(float){ (float)inst->current_frame }, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, vr->loc_vatFrameCount, &(float){ (float)inst->frame_count }, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, vr->loc_vatVertexCount, &(float){ (float)mesh->meshes[0].vertexCount }, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, vr->loc_matDiffuseColor, (float[3]){ 1, 1, 1 }, SHADER_UNIFORM_VEC3);
    
    // Set texture (if available)
    if (inst->texture_idx >= 0 && inst->texture_idx < vr->max_textures) {
        if (vr->textures[inst->texture_idx].id != 0) {
            SetShaderValueTexture(shader, vr->loc_texture0, vr->textures[inst->texture_idx]);
        }
    }
    
    // Draw mesh with instance transform
    Matrix transforms[1];
    transforms[0] = model;
    DrawMeshInstanced(mesh->meshes[0], vr->materials[inst->mesh_idx], transforms, 1);
}
