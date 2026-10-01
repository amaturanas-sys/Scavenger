#include "player.h"

#include <math.h>

#include "raymath.h"

#define WALK_SPEED 4.0f
#define RUN_SPEED 8.5f
#define SNEAK_SPEED 1.8f
#define JUMP_SPEED 6.0f
#define GRAVITY 18.0f
#define TURN_RATE 10.0f

PlayerInput player_read_input(void) {
    PlayerInput in = { 0 };
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) in.move_z += 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) in.move_z -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) in.move_x += 1.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) in.move_x -= 1.0f;
    if (IsGamepadAvailable(0)) {
        float gx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float gy = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        if (fabsf(gx) > 0.2f) in.move_x += gx;
        if (fabsf(gy) > 0.2f) in.move_z -= gy;
        in.run |= IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
        in.toggle_sneak |= IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_THUMB);
        in.jump |= IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    }
    in.run |= IsKeyDown(KEY_LEFT_SHIFT);
    in.toggle_sneak |= IsKeyPressed(KEY_C) || IsKeyPressed(KEY_LEFT_CONTROL);
    in.jump |= IsKeyPressed(KEY_SPACE);
    return in;
}

void player_init(Player *p, const Terrain *t) {
    *p = (Player){ 0 };
    p->pos = (Vector3){ 0.0f, 0.0f, 22.0f }; // en la entrada sur del campamento
    p->pos.y = terrain_height(t, p->pos.x, p->pos.z);
    p->yaw = PI; // mirando hacia el campamento
    p->grounded = true;
}

void player_update(Player *p, const Terrain *t, PlayerInput in, float cam_yaw, float dt) {
    if (in.toggle_sneak) p->sneaking = !p->sneaking;

    Vector2 dir = { in.move_x, in.move_z };
    float len = Vector2Length(dir);
    if (len > 1.0f) dir = Vector2Scale(dir, 1.0f / len);
    p->moving = len > 0.05f;

    // Correr cancela el sigilo; el sigilo es lento y silencioso.
    if (in.run && p->moving) p->sneaking = false;
    p->stance = p->sneaking ? STANCE_SNEAK : (in.run && p->moving ? STANCE_RUN : STANCE_WALK);
    float speed = p->stance == STANCE_SNEAK ? SNEAK_SPEED : (p->stance == STANCE_RUN ? RUN_SPEED : WALK_SPEED);

    if (p->moving) {
        // Adelante = direccion a la que mira la camara (proyectada al suelo).
        Vector3 fwd = { sinf(cam_yaw), 0.0f, cosf(cam_yaw) };
        Vector3 right = { -fwd.z, 0.0f, fwd.x };
        Vector3 move = Vector3Add(Vector3Scale(fwd, dir.y), Vector3Scale(right, dir.x));
        p->pos.x += move.x * speed * dt;
        p->pos.z += move.z * speed * dt;
        float target = atan2f(move.x, move.z);
        float diff = atan2f(sinf(target - p->yaw), cosf(target - p->yaw));
        p->yaw += diff * fminf(1.0f, TURN_RATE * dt);
    }

    float ground = terrain_height(t, p->pos.x, p->pos.z);
    if (p->grounded && in.jump && p->stance != STANCE_SNEAK) {
        p->vy = JUMP_SPEED;
        p->grounded = false;
    }
    p->vy -= GRAVITY * dt;
    p->pos.y += p->vy * dt;
    if (p->pos.y <= ground) {
        p->pos.y = ground;
        p->vy = 0.0f;
        p->grounded = true;
    }

    // Ruido emitido: base para la deteccion de enemigos al acechar.
    if (!p->moving) p->noise = 0.0f;
    else p->noise = p->stance == STANCE_RUN ? 1.0f : (p->stance == STANCE_SNEAK ? 0.15f : 0.5f);
}

float player_eye_height(const Player *p) { return p->sneaking ? 1.0f : 1.6f; }

void player_draw(const Player *p) {
    // Placeholder hasta tener el modelo del personaje (ver docs/ROADMAP.md).
    float h = p->sneaking ? 1.1f : 1.7f;
    Color body = p->stance == STANCE_SNEAK ? (Color){ 70, 80, 70, 255 } : (Color){ 150, 52, 40, 255 };
    Vector3 base = { p->pos.x, p->pos.y + 0.35f, p->pos.z };
    Vector3 top = { p->pos.x, p->pos.y + h - 0.35f, p->pos.z };
    DrawCapsule(base, top, 0.35f, 6, 3, body);
    // Nariz: indica hacia donde mira.
    Vector3 nose = { p->pos.x + sinf(p->yaw) * 0.4f, p->pos.y + h - 0.35f, p->pos.z + cosf(p->yaw) * 0.4f };
    DrawCube(nose, 0.15f, 0.15f, 0.15f, (Color){ 230, 200, 160, 255 });
}

const char *stance_name(Stance s) {
    switch (s) {
    case STANCE_WALK: return "caminando";
    case STANCE_RUN: return "corriendo";
    case STANCE_SNEAK: return "acechando";
    }
    return "?";
}
