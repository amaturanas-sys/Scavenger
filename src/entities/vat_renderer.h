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

// VAT Renderer State
typedef struct {
    int max_instances;            // Maximum number of concurrent VAT instances
    int instance_count;           // Current number of active instances
    
    // Shader handles
    Shader gl330_shader;          // GLSL 330 shader for desktop
    Shader gl100_shader;          // GLSL ES 100 shader for Android
    
    // Shader uniform locations
    int loc_matModel;             // matModel uniform
    int loc_matView;              // matView uniform
    int loc_matProjection;        // matProjection uniform
    int loc_vatPositionMin;       // vatPositionMin uniform
    int loc_vatPositionExtent;    // vatPositionExtent uniform
    int loc_vatFrame;             // vatFrame uniform
    int loc_vatFrameCount;        // vatFrameCount uniform
    int loc_vatVertexCount;       // vatVertexCount uniform
    int loc_matDiffuseColor;      // matDiffuseColor uniform
    int loc_lightDir;             // lightDir uniform
    int loc_lightColor;           // lightColor uniform
    int loc_ambientColor;         // ambientColor uniform
    int loc_ambientIntensity;     // ambientIntensity uniform
    int loc_texture0;             // texture0 sampler uniform
    
    // Texture IDs
    Texture2D *textures;          // Array of VAT texture handles
    int max_textures;             // Maximum number of textures
    
    // Mesh handles
    Model *meshes;                // Array of static mesh handles
    int max_meshes;               // Maximum number of meshes
    
    // Material handles (one per mesh)
    Material *materials;          // Array of material handles
    int max_materials;            // Maximum number of materials
    
    // Instance transforms (application-owned array for instancing)
    Matrix *instanceTransforms;   // Pre-allocated instance transforms
    int max_instance_transforms;  // Maximum number of instance transforms
    
    // Android-specific
    bool android_instancing_supported;  // Whether Android backend supports instancing
    bool android_shader_loaded;          // Whether GLSL ES 100 shader loaded successfully
} VatRenderer;

// Initialization and shutdown
void vat_renderer_init(VatRenderer *vr, int max_instances);
void vat_renderer_shutdown(VatRenderer *vr);

// Asset loading (must be called during initialization, not in frame loop)
// Returns 0 on success, -1 on failure
int vat_renderer_load_shader_gl330(VatRenderer *vr, const char *vs_path, const char *fs_path);
int vat_renderer_load_shader_gl100(VatRenderer *vr, const char *vs_path, const char *fs_path);
int vat_renderer_load_mesh(VatRenderer *vr, const char *path);
int vat_renderer_load_vat_texture(VatRenderer *vr, const char *path, int frame_count);

// Instance management
// Returns instance index, or -1 if no available slot
int vat_renderer_add_instance(VatRenderer *vr, int archetype_id);
void vat_renderer_remove_instance(VatRenderer *vr, int instance_idx);
void vat_renderer_clear_instances(VatRenderer *vr);

// Update (must be called each frame)
void vat_renderer_update_instance(VatRenderer *vr, int instance_idx, float dt);
void vat_renderer_set_instance_frame(VatRenderer *vr, int instance_idx, int frame);
void vat_renderer_set_instance_animation(VatRenderer *vr, int instance_idx, float anim_speed);

// Render
void vat_renderer_draw(VatRenderer *vr);
void vat_renderer_draw_instance(VatRenderer *vr, int instance_idx);

// Utility functions
const char *vat_renderer_get_shader_error(VatRenderer *vr);
bool vat_renderer_shader_loaded(VatRenderer *vr);
bool vat_renderer_android_instancing_supported(VatRenderer *vr);

#endif
