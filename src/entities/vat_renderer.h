// VAT Renderer - Hybrid Animation System
// Vertex Animation Textures (VAT) with Multi-Backend Shaders
// ---------------------------------------------------------------------
// Supports both GLSL 330 (desktop) and GLSL ES 100 (Android).
// Renders static meshes with per-frame vertex animation using textures.
// ---------------------------------------------------------------------

#ifndef ESTEPA_VAT_RENDERER_H
#define ESTEPA_VAT_RENDERER_H

#include <stdbool.h>
#include <stddef.h>

#include "raylib.h"
#include "raymath.h"

// VAT Instance data for a single rendered mesh
typedef struct {
    int enabled;                  // 1 = enabled for rendering, 0 = disabled
    int archetype_id;             // ID of the NPC archetype using this VAT
    int mesh_idx;                 // Index of the static mesh
    int texture_idx;              // Index of the VAT texture
    int frame_count;              // Number of frames in the VAT animation
    int current_frame;            // Current frame (0 to frame_count-1)
    float anim_time;              // Animation time accumulator
    float anim_speed;             // Animation speed multiplier
    Vector3 position;             // World position
    float yaw;                    // Yaw angle (radians)
    float pitch;                  // Pitch angle (radians)
    float roll;                   // Roll angle (radians)
    float scale;                  // Scale factor
    bool dead;                    // Dead pose variant
} VatInstance;

// Renderer state flags
typedef enum {
    VAT_RENDERER_STATE_UNINITIALIZED = 0,
    VAT_RENDERER_STATE_DESKTOP_ONLY,      // GLSL 330 available, GLSL ES 100 not used
    VAT_RENDERER_STATE_ANDROID_ONLY,      // GLSL ES 100 available, GLSL 330 not used
    VAT_RENDERER_STATE_BOTH_SHADERS,      // Both shaders loaded
    VAT_RENDERER_STATE_ERROR               // Initialization failed
} VatRendererState;

// VAT Renderer State
typedef struct {
    int max_instances;
    int instance_count;
    VatRendererState state;
    bool is_android;
    Shader active_shader;
    Shader gl330_shader;
    Shader gl100_shader;
    int gl330_loc_matModel;
    int gl330_loc_matView;
    int gl330_loc_matProjection;
    int gl330_loc_vatPositionMin;
    int gl330_loc_vatPositionExtent;
    int gl330_loc_vatFrame;
    int gl330_loc_vatFrameCount;
    int gl330_loc_vatVertexCount;
    int gl330_loc_matDiffuseColor;
    int gl330_loc_lightDir;
    int gl330_loc_lightColor;
    int gl330_loc_ambientColor;
    int gl330_loc_ambientIntensity;
    int gl330_loc_texture0;
    int gl100_loc_matModel;
    int gl100_loc_matView;
    int gl100_loc_matProjection;
    int gl100_loc_vatPositionMin;
    int gl100_loc_vatPositionExtent;
    int gl100_loc_vatFrame;
    int gl100_loc_vatFrameCount;
    int gl100_loc_vatVertexCount;
    int gl100_loc_matDiffuseColor;
    int gl100_loc_lightDir;
    int gl100_loc_lightColor;
    int gl100_loc_ambientColor;
    int gl100_loc_ambientIntensity;
    int gl100_loc_texture0;
    Texture2D *textures;
    int max_textures;
    Model *meshes;
    int max_meshes;
    Material *materials;
    int max_materials;
    VatInstance *instances;
    Matrix *instanceTransforms;
    int max_instance_transforms;
    bool android_instancing_supported;
    bool android_shader_loaded;
} VatRenderer;

void vat_renderer_init(VatRenderer *vr, int max_instances);
void vat_renderer_shutdown(VatRenderer *vr);
int vat_renderer_load_shader_gl330(VatRenderer *vr, const char *vs_path, const char *fs_path);
int vat_renderer_load_shader_gl100(VatRenderer *vr, const char *vs_path, const char *fs_path);
int vat_renderer_load_mesh(VatRenderer *vr, const char *path);
int vat_renderer_load_vat_texture(VatRenderer *vr, const char *path, int frame_count);
int vat_renderer_add_instance(VatRenderer *vr, int archetype_id);
void vat_renderer_remove_instance(VatRenderer *vr, int instance_idx);
void vat_renderer_clear_instances(VatRenderer *vr);
void vat_renderer_update_instance(VatRenderer *vr, int instance_idx, float dt);
void vat_renderer_set_instance_frame(VatRenderer *vr, int instance_idx, int frame);
void vat_renderer_set_instance_animation(VatRenderer *vr, int instance_idx, float anim_speed);
void vat_renderer_draw(VatRenderer *vr);
void vat_renderer_draw_instance(VatRenderer *vr, int instance_idx);
const char *vat_renderer_get_shader_error(VatRenderer *vr);
bool vat_renderer_shader_loaded(VatRenderer *vr);
bool vat_renderer_android_instancing_supported(VatRenderer *vr);

#endif
