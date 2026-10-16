// Sistema de animaciones esqueléticas para entidades camp.
// Compatible con Raylib ModelAnimation (GLB con esqueleto).
// Optimizado para Android/móviles: memoria estática, sin leaks en el loop.
#ifndef ESTEPA_ANIM_SYSTEM_H
#define ESTEPA_ANIM_SYSTEM_H

#include <stdbool.h>

#include "raylib.h"

#define MAX_ANIM_ENTITIES 32
#define MAX_ANIM_TRACKS 16
#define MAX_ANIM_NAME 64

typedef struct {
    Model model;
    ModelAnimation *animations;
    int anim_count;
    int current_track;
    float current_frame;
    float anim_speed;
    bool loop;
    bool loaded;
    Vector3 position;
    Vector3 rotation_axis;
    float rotation_angle;
    Vector3 scale;
    float scale_factor;
} AnimatedEntity;

typedef struct {
    char name[MAX_ANIM_NAME];
    AnimatedEntity entities[MAX_ANIM_ENTITIES];
    int entity_count;
} AnimationSystem;

void anim_system_init(AnimationSystem *sys);
bool anim_system_load_entity(AnimationSystem *sys, int index, const char *model_path, bool animated);
bool anim_system_load_animation(AnimationSystem *sys, int entity_index, int track_index, const char *anim_path);
void anim_system_update(AnimationSystem *sys, float dt);
void anim_system_draw(const AnimationSystem *sys, int entity_index);
void anim_system_draw_all(const AnimationSystem *sys);
void anim_system_unload(AnimationSystem *sys, int entity_index);
void anim_system_unload_all(AnimationSystem *sys);
bool anim_system_set_track(AnimationSystem *sys, int entity_index, int track_index);
bool anim_system_play_track(AnimationSystem *sys, int entity_index, const char *anim_name);
bool anim_system_is_loaded(const AnimationSystem *sys, int entity_index);

#endif