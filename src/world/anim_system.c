#include "anim_system.h"

#include <string.h>

#include "platform.h"
#include "raymath.h"

void anim_system_init(AnimationSystem *sys) {
    memset(sys, 0, sizeof(*sys));
    sys->entity_count = 0;
}

bool anim_system_load_entity(AnimationSystem *sys, int index, const char *model_path, bool animated) {
    if (index < 0 || index >= MAX_ANIM_ENTITIES) return false;

    AnimatedEntity *ent = &sys->entities[index];
    memset(ent, 0, sizeof(*ent));

    ent->model = LoadModel(model_path);
    ent->loaded = ent->model.meshCount > 0;

    if (!ent->loaded) {
        UnloadModel(ent->model);
        return false;
    }

    if (animated) {
        ent->animations = LoadModelAnimations(model_path, &ent->anim_count);
        if (ent->anim_count > 0 && ent->animations) {
            ent->current_track = 0;
            ent->current_frame = 0.0f;
            ent->anim_speed = 1.0f;
            ent->loop = true;
        }
    }

    if (index >= sys->entity_count) sys->entity_count = index + 1;

    return true;
}

bool anim_system_load_animation(AnimationSystem *sys, int entity_index, int track_index,
                                const char *anim_path) {
    if (entity_index < 0 || entity_index >= MAX_ANIM_ENTITIES) return false;

    AnimatedEntity *ent = &sys->entities[entity_index];
    if (!ent->loaded) return false;

    if (track_index < 0 || track_index >= MAX_ANIM_TRACKS) return false;

    // For Raylib, animations are loaded with the model. Track replacement:
    // unload existing animations and reload with combined set
    if (ent->animations) {
        UnloadModelAnimations(ent->animations, ent->anim_count);
        ent->animations = NULL;
        ent->anim_count = 0;
    }

    ent->animations = LoadModelAnimations(anim_path, &ent->anim_count);
    if (ent->anim_count > 0 && ent->animations) {
        ent->current_track = 0;
        return true;
    }

    return false;
}

void anim_system_update(AnimationSystem *sys, float dt) {
    for (int i = 0; i < sys->entity_count; i++) {
        AnimatedEntity *ent = &sys->entities[i];
        if (!ent->loaded || !ent->animations || ent->anim_count == 0) continue;

        // Advance frame
        ent->current_frame += ent->anim_speed * dt * 30.0f; // 30 FPS base

        if (ent->loop) {
            if (ent->current_frame >= (float)ent->animations[ent->current_track].frameCount) {
                ent->current_frame = 0.0f;
            }
        } else {
            if (ent->current_frame >= (float)ent->animations[ent->current_track].frameCount) {
                ent->current_frame =
                    (float)ent->animations[ent->current_track].frameCount - 1.0f;
            }
        }

        UpdateModelAnimation(ent->model, ent->animations[ent->current_track],
                             (int)ent->current_frame);
    }
}

void anim_system_draw(const AnimationSystem *sys, int entity_index) {
    if (entity_index < 0 || entity_index >= MAX_ANIM_ENTITIES) return;

    const AnimatedEntity *ent = &sys->entities[entity_index];
    if (!ent->loaded) return;

    // Apply transform using model properties with DrawModelEx
    DrawModelEx(ent->model, ent->position, ent->rotation_axis, ent->rotation_angle,
                ent->scale, WHITE);
}

void anim_system_draw_all(const AnimationSystem *sys) {
    for (int i = 0; i < sys->entity_count; i++) {
        anim_system_draw(sys, i);
    }
}

void anim_system_unload(AnimationSystem *sys, int entity_index) {
    if (entity_index < 0 || entity_index >= MAX_ANIM_ENTITIES) return;

    AnimatedEntity *ent = &sys->entities[entity_index];

    if (ent->animations) {
        UnloadModelAnimations(ent->animations, ent->anim_count);
        ent->animations = NULL;
        ent->anim_count = 0;
    }

    if (ent->loaded) {
        UnloadModel(ent->model);
        ent->loaded = false;
    }

    memset(ent, 0, sizeof(*ent));
}

void anim_system_unload_all(AnimationSystem *sys) {
    for (int i = 0; i < sys->entity_count; i++) {
        anim_system_unload(sys, i);
    }
    sys->entity_count = 0;
}

bool anim_system_set_track(AnimationSystem *sys, int entity_index, int track_index) {
    if (entity_index < 0 || entity_index >= MAX_ANIM_ENTITIES) return false;

    AnimatedEntity *ent = &sys->entities[entity_index];
    if (!ent->loaded || !ent->animations) return false;
    if (track_index < 0 || track_index >= ent->anim_count) return false;

    ent->current_track = track_index;
    ent->current_frame = 0.0f;
    return true;
}

bool anim_system_play_track(AnimationSystem *sys, int entity_index, const char *anim_name) {
    // Raylib does not store animation names by default; match by index.
    // Clients should use anim_system_set_track with known indices.
    (void)sys;
    (void)entity_index;
    (void)anim_name;
    return false;
}

bool anim_system_is_loaded(const AnimationSystem *sys, int entity_index) {
    if (entity_index < 0 || entity_index >= MAX_ANIM_ENTITIES) return false;
    return sys->entities[entity_index].loaded;
}