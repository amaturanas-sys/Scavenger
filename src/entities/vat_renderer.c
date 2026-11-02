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
    if (!vr) return NULL;
    return g_shader_error;
}

bool vat_renderer_shader_loaded(VatRenderer *vr) {
    if (!vr) return false;
    return vr->state != VAT_RENDERER_STATE_UNINITIALIZED && vr->state != VAT_RENDERER_STATE_ERROR;
}

bool vat_renderer_android_instancing_supported(VatRenderer *vr) {
    if (!vr) return false;
    return vr->android_instancing_supported;
}

static int validate_shader_handle(Shader shader) {
    return (shader.id != 0);
}

static int validate_uniform_location(int location) {
    return (location >= 0);
}

static void set_shader_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_shader_error, sizeof(g_shader_error), fmt, args);
    va_end(args);
}

void vat_renderer_init(VatRenderer *vr, int max_instances) {
    if (!vr) return;

    memset(vr, 0, sizeof(VatRenderer));

    vr->max_instances = max_instances > 0 ? max_instances : VAT_MAX_INSTANCES;
    vr->instance_count = 0;
    vr->state = VAT_RENDERER_STATE_UNINITIALIZED;
    vr->is_android = false;
    vr->android_instancing_supported = false;
    vr->android_shader_loaded = false;
    vr->max_textures = VAT_MAX_TEXTURES;
    vr->max_meshes = VAT_MAX_MESHES;
    vr->max_instance_transforms = VAT_MAX_INSTANCE_TRANSFORMS;

    vr->textures = (Texture2D *)calloc(vr->max_textures, sizeof(Texture2D));
    vr->meshes = (Model *)calloc(vr->max_meshes, sizeof(Model));
    vr->materials = (Material *)calloc(vr->max_meshes, sizeof(Material));
    vr->instances = (VatInstance *)calloc(vr->max_instances, sizeof(VatInstance));
    vr->instanceTransforms = (Matrix *)calloc(vr->max_instance_transforms, sizeof(Matrix));

    for (int i = 0; i < vr->max_meshes; i++) {
        vr->materials[i] = LoadMaterialDefault();
    }
}

void vat_renderer_shutdown(VatRenderer *vr) {
    if (!vr) return;

    if (vr->gl330_shader.id != 0) {
        UnloadShader(vr->gl330_shader);
        vr->gl330_shader = (Shader){ 0 };
    }
    if (vr->gl100_shader.id != 0) {
        UnloadShader(vr->gl100_shader);
        vr->gl100_shader = (Shader){ 0 };
    }
    vr->active_shader = (Shader){ 0 };

    for (int i = 0; i < vr->max_textures; i++) {
        if (vr->textures[i].id != 0) {
            UnloadTexture(vr->textures[i]);
            vr->textures[i] = (Texture2D){ 0 };
        }
    }

    for (int i = 0; i < vr->max_meshes; i++) {
        if (vr->meshes[i].meshCount > 0) {
            UnloadModel(vr->meshes[i]);
            vr->meshes[i] = (Model){ 0 };
        }
    }

    for (int i = 0; i < vr->max_meshes; i++) {
        if (vr->materials[i].shader.id != 0) {
            UnloadMaterial(vr->materials[i]);
            vr->materials[i].shader = (Shader){ 0 };
        }
    }

    free(vr->textures);
    free(vr->meshes);
    free(vr->materials);
    free(vr->instances);
    free(vr->instanceTransforms);

    memset(vr, 0, sizeof(VatRenderer));
}

int vat_renderer_load_shader_gl330(VatRenderer *vr, const char *vs_path, const char *fs_path) {
    if (!vr || !vs_path || !fs_path) {
        set_shader_error("Invalid shader paths");
        return -1;
    }

    if (vr->gl330_shader.id != 0) {
        UnloadShader(vr->gl330_shader);
    }

    const char *asset_vs = platform_asset_path(vs_path);
    const char *asset_fs = platform_asset_path(fs_path);

    vr->gl330_shader = LoadShader(asset_vs, asset_fs);

    if (!validate_shader_handle(vr->gl330_shader)) {
        set_shader_error("Failed to load GLSL 330 shader: %s / %s", vs_path, fs_path);
        return -1;
    }

    vr->gl330_loc_matModel = GetShaderLocation(vr->gl330_shader, "matModel");
    vr->gl330_loc_matView = GetShaderLocation(vr->gl330_shader, "matView");
    vr->gl330_loc_matProjection = GetShaderLocation(vr->gl330_shader, "matProjection");
    vr->gl330_loc_vatPositionMin = GetShaderLocation(vr->gl330_shader, "vatPositionMin");
    vr->gl330_loc_vatPositionExtent = GetShaderLocation(vr->gl330_shader, "vatPositionExtent");
    vr->gl330_loc_vatFrame = GetShaderLocation(vr->gl330_shader, "vatFrame");
    vr->gl330_loc_vatFrameCount = GetShaderLocation(vr->gl330_shader, "vatFrameCount");
    vr->gl330_loc_vatVertexCount = GetShaderLocation(vr->gl330_shader, "vatVertexCount");
    vr->gl330_loc_matDiffuseColor = GetShaderLocation(vr->gl330_shader, "matDiffuseColor");
    vr->gl330_loc_lightDir = GetShaderLocation(vr->gl330_shader, "lightDir");
    vr->gl330_loc_lightColor = GetShaderLocation(vr->gl330_shader, "lightColor");
    vr->gl330_loc_ambientColor = GetShaderLocation(vr->gl330_shader, "ambientColor");
    vr->gl330_loc_ambientIntensity = GetShaderLocation(vr->gl330_shader, "ambientIntensity");
    vr->gl330_loc_texture0 = GetShaderLocation(vr->gl330_shader, "texture0");

    if (!validate_uniform_location(vr->gl330_loc_matModel) ||
        !validate_uniform_location(vr->gl330_loc_matView) ||
        !validate_uniform_location(vr->gl330_loc_matProjection) ||
        !validate_uniform_location(vr->gl330_loc_vatPositionMin) ||
        !validate_uniform_location(vr->gl330_loc_vatPositionExtent) ||
        !validate_uniform_location(vr->gl330_loc_vatFrame) ||
        !validate_uniform_location(vr->gl330_loc_vatFrameCount) ||
        !validate_uniform_location(vr->gl330_loc_vatVertexCount) ||
        !validate_uniform_location(vr->gl330_loc_matDiffuseColor) ||
        !validate_uniform_location(vr->gl330_loc_lightDir) ||
        !validate_uniform_location(vr->gl330_loc_lightColor) ||
        !validate_uniform_location(vr->gl330_loc_ambientColor) ||
        !validate_uniform_location(vr->gl330_loc_ambientIntensity) ||
        !validate_uniform_location(vr->gl330_loc_texture0)) {
        set_shader_error("GLSL 330 shader missing required uniforms");
        UnloadShader(vr->gl330_shader);
        vr->gl330_shader = (Shader){ 0 };
        return -1;
    }

    return 0;
}

int vat_renderer_load_shader_gl100(VatRenderer *vr, const char *vs_path, const char *fs_path) {
    if (!vr || !vs_path || !fs_path) {
        set_shader_error("Invalid shader paths");
        return -1;
    }

    if (vr->gl100_shader.id != 0) {
        UnloadShader(vr->gl100_shader);
    }

    const char *asset_vs = platform_asset_path(vs_path);
    const char *asset_fs = platform_asset_path(fs_path);

    vr->gl100_shader = LoadShader(asset_vs, asset_fs);

    if (!validate_shader_handle(vr->gl100_shader)) {
        set_shader_error("Failed to load GLSL ES 100 shader: %s / %s", vs_path, fs_path);
        return -1;
    }

    vr->gl100_loc_matModel = GetShaderLocation(vr->gl100_shader, "matModel");
    vr->gl100_loc_matView = GetShaderLocation(vr->gl100_shader, "matView");
    vr->gl100_loc_matProjection = GetShaderLocation(vr->gl100_shader, "matProjection");
    vr->gl100_loc_vatPositionMin = GetShaderLocation(vr->gl100_shader, "vatPositionMin");
    vr->gl100_loc_vatPositionExtent = GetShaderLocation(vr->gl100_shader, "vatPositionExtent");
    vr->gl100_loc_vatFrame = GetShaderLocation(vr->gl100_shader, "vatFrame");
    vr->gl100_loc_vatFrameCount = GetShaderLocation(vr->gl100_shader, "vatFrameCount");
    vr->gl100_loc_vatVertexCount = GetShaderLocation(vr->gl100_shader, "vatVertexCount");
    vr->gl100_loc_matDiffuseColor = GetShaderLocation(vr->gl100_shader, "matDiffuseColor");
    vr->gl100_loc_lightDir = GetShaderLocation(vr->gl100_shader, "lightDir");
    vr->gl100_loc_lightColor = GetShaderLocation(vr->gl100_shader, "lightColor");
    vr->gl100_loc_ambientColor = GetShaderLocation(vr->gl100_shader, "ambientColor");
    vr->gl100_loc_ambientIntensity = GetShaderLocation(vr->gl100_shader, "ambientIntensity");
    vr->gl100_loc_texture0 = GetShaderLocation(vr->gl100_shader, "texture0");

    if (!validate_uniform_location(vr->gl100_loc_matModel) ||
        !validate_uniform_location(vr->gl100_loc_matView) ||
        !validate_uniform_location(vr->gl100_loc_matProjection) ||
        !validate_uniform_location(vr->gl100_loc_vatPositionMin) ||
        !validate_uniform_location(vr->gl100_loc_vatPositionExtent) ||
        !validate_uniform_location(vr->gl100_loc_vatFrame) ||
        !validate_uniform_location(vr->gl100_loc_vatFrameCount) ||
        !validate_uniform_location(vr->gl100_loc_vatVertexCount) ||
        !validate_uniform_location(vr->gl100_loc_matDiffuseColor) ||
        !validate_uniform_location(vr->gl100_loc_lightDir) ||
        !validate_uniform_location(vr->gl100_loc_lightColor) ||
        !validate_uniform_location(vr->gl100_loc_ambientColor) ||
        !validate_uniform_location(vr->gl100_loc_ambientIntensity) ||
        !validate_uniform_location(vr->gl100_loc_texture0)) {
        set_shader_error("GLSL ES 100 shader missing required uniforms");
        UnloadShader(vr->gl100_shader);
        vr->gl100_shader = (Shader){ 0 };
        return -1;
    }

    vr->android_shader_loaded = true;
    return 0;
}

int vat_renderer_load_mesh(VatRenderer *vr, const char *path) {
    if (!vr || !path) {
        return -1;
    }

    if (vr->max_meshes <= 0) {
        return -1;
    }

    for (int i = 0; i < vr->max_meshes; i++) {
        if (vr->meshes[i].meshCount == 0) {
            const char *asset_path = platform_asset_path(path);
            vr->meshes[i] = LoadModel(asset_path);
            if (vr->meshes[i].meshCount > 0) {
                return i;
            }
            vr->meshes[i] = (Model){ 0 };
        }
    }

    return -1;
}

int vat_renderer_load_vat_texture(VatRenderer *vr, const char *path, int frame_count) {
    if (!vr || !path || frame_count <= 0) {
        return -1;
    }

    if (vr->max_textures <= 0) {
        return -1;
    }

    for (int i = 0; i < vr->max_textures; i++) {
        if (vr->textures[i].id == 0) {
            const char *asset_path = platform_asset_path(path);
            vr->textures[i] = LoadTexture(asset_path);
            if (vr->textures[i].id != 0) {
                vr->textures[i].width = frame_count;
                return i;
            }
            vr->textures[i] = (Texture2D){ 0 };
        }
    }

    return -1;
}

int vat_renderer_add_instance(VatRenderer *vr, int archetype_id) {
    if (!vr) return -1;

    if (vr->instance_count >= vr->max_instances) {
        return -1;
    }

    int idx = vr->instance_count;
    memset(&vr->instances[idx], 0, sizeof(VatInstance));
    vr->instances[idx].enabled = 1;
    vr->instances[idx].archetype_id = archetype_id;
    vr->instances[idx].mesh_idx = -1;
    vr->instances[idx].texture_idx = -1;
    vr->instances[idx].frame_count = 1;
    vr->instances[idx].current_frame = 0;
    vr->instances[idx].anim_time = 0.0f;
    vr->instances[idx].anim_speed = VAT_DEFAULT_ANIM_SPEED;
    vr->instances[idx].position = (Vector3){ 0, 0, 0 };
    vr->instances[idx].yaw = 0.0f;
    vr->instances[idx].pitch = 0.0f;
    vr->instances[idx].roll = 0.0f;
    vr->instances[idx].scale = VAT_DEFAULT_SCALE;
    vr->instances[idx].dead = false;

    vr->instance_count++;
    return idx;
}

void vat_renderer_remove_instance(VatRenderer *vr, int instance_idx) {
    if (!vr || instance_idx < 0 || instance_idx >= vr->instance_count) {
        return;
    }

    if (instance_idx < vr->instance_count - 1) {
        VatInstance last = vr->instances[vr->instance_count - 1];
        memcpy(&vr->instances[instance_idx], &last, sizeof(VatInstance));
    }

    memset(&vr->instances[vr->instance_count - 1], 0, sizeof(VatInstance));
    vr->instance_count--;
}

void vat_renderer_clear_instances(VatRenderer *vr) {
    if (!vr) return;

    for (int i = 0; i < vr->instance_count; i++) {
        memset(&vr->instances[i], 0, sizeof(VatInstance));
    }
    vr->instance_count = 0;
}

void vat_renderer_update_instance(VatRenderer *vr, int instance_idx, float dt) {
    if (!vr || instance_idx < 0 || instance_idx >= vr->instance_count) {
        return;
    }

    VatInstance *inst = &vr->instances[instance_idx];
    if (!inst->enabled) {
        return;
    }

    inst->anim_time += dt * inst->anim_speed;
    if (inst->anim_time >= inst->frame_count) {
        inst->anim_time -= inst->frame_count;
    }
    inst->current_frame = (int)inst->anim_time;
}

void vat_renderer_set_instance_frame(VatRenderer *vr, int instance_idx, int frame) {
    if (!vr || instance_idx < 0 || instance_idx >= vr->instance_count) {
        return;
    }

    vr->instances[instance_idx].current_frame = frame;
    vr->instances[instance_idx].anim_time = (float)frame;
}

void vat_renderer_set_instance_animation(VatRenderer *vr, int instance_idx, float anim_speed) {
    if (!vr || instance_idx < 0 || instance_idx >= vr->instance_count) {
        return;
    }

    vr->instances[instance_idx].anim_speed = anim_speed;
}

static int select_active_shader(VatRenderer *vr) {
    if (!vr) return -1;

#if defined(PLATFORM_ANDROID)
    vr->is_android = true;
    if (vr->gl100_shader.id != 0) {
        vr->active_shader = vr->gl100_shader;
        return 0;
    }
    vr->active_shader = (Shader){ 0 };
    vr->state = VAT_RENDERER_STATE_ERROR;
    return -1;
#else
    vr->is_android = false;
    if (vr->gl330_shader.id != 0) {
        vr->active_shader = vr->gl330_shader;
        return 0;
    }
    vr->active_shader = (Shader){ 0 };
    vr->state = VAT_RENDERER_STATE_ERROR;
    return -1;
#endif
}

void vat_renderer_draw_instance(VatRenderer *vr, int instance_idx) {
    if (!vr || instance_idx < 0 || instance_idx >= vr->instance_count) {
        return;
    }

    VatInstance *inst = &vr->instances[instance_idx];
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

    if (select_active_shader(vr) != 0) {
        return;
    }

    Shader shader = vr->active_shader;
    if (shader.id == 0) {
        return;
    }

    vr->materials[inst->mesh_idx].shader = shader;

    Matrix model = MatrixIdentity();
    Matrix view = rlGetMatrixModelview();
    Matrix projection = rlGetMatrixProjection();

    if (vr->is_android) {
        SetShaderValueMatrix(shader, vr->gl100_loc_matModel, model);
        SetShaderValueMatrix(shader, vr->gl100_loc_matView, view);
        SetShaderValueMatrix(shader, vr->gl100_loc_matProjection, projection);
        SetShaderValue(shader, vr->gl100_loc_vatPositionMin, (float[3]){ 0, 0, 0 }, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, vr->gl100_loc_vatPositionExtent, (float[3]){ 1, 1, 1 }, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, vr->gl100_loc_vatFrame, &(float){ (float)inst->current_frame }, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, vr->gl100_loc_vatFrameCount, &(float){ (float)inst->frame_count }, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, vr->gl100_loc_vatVertexCount, &(float){ (float)mesh->meshes[0].vertexCount }, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, vr->gl100_loc_matDiffuseColor, (float[3]){ 1, 1, 1 }, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, vr->gl100_loc_lightDir, (float[3]){ 0.5, 1.0, 0.5 }, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, vr->gl100_loc_lightColor, (float[3]){ 1.0, 0.95, 0.9 }, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, vr->gl100_loc_ambientColor, (float[3]){ 0.3, 0.3, 0.35 }, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, vr->gl100_loc_ambientIntensity, &(float){ 0.6f }, SHADER_UNIFORM_FLOAT);
    } else {
        SetShaderValueMatrix(shader, vr->gl330_loc_matModel, model);
        SetShaderValueMatrix(shader, vr->gl330_loc_matView, view);
        SetShaderValueMatrix(shader, vr->gl330_loc_matProjection, projection);
        SetShaderValue(shader, vr->gl330_loc_vatPositionMin, (float[3]){ 0, 0, 0 }, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, vr->gl330_loc_vatPositionExtent, (float[3]){ 1, 1, 1 }, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, vr->gl330_loc_vatFrame, &(float){ (float)inst->current_frame }, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, vr->gl330_loc_vatFrameCount, &(float){ (float)inst->frame_count }, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, vr->gl330_loc_vatVertexCount, &(float){ (float)mesh->meshes[0].vertexCount }, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, vr->gl330_loc_matDiffuseColor, (float[3]){ 1, 1, 1 }, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, vr->gl330_loc_lightDir, (float[3]){ 0.5, 1.0, 0.5 }, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, vr->gl330_loc_lightColor, (float[3]){ 1.0, 0.95, 0.9 }, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, vr->gl330_loc_ambientColor, (float[3]){ 0.3, 0.3, 0.35 }, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, vr->gl330_loc_ambientIntensity, &(float){ 0.6f }, SHADER_UNIFORM_FLOAT);
    }

    Matrix transforms[1];
    transforms[0] = MatrixIdentity();

    DrawMeshInstanced(mesh->meshes[0], vr->materials[inst->mesh_idx], transforms, 1);
}

void vat_renderer_draw(VatRenderer *vr) {
    if (!vr) return;

    if (select_active_shader(vr) != 0) {
        return;
    }

    Shader shader = vr->active_shader;
    if (shader.id == 0) {
        return;
    }

    for (int i = 0; i < vr->instance_count; i++) {
        vat_renderer_draw_instance(vr, i);
    }
}
