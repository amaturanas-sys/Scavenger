#include "player.h"

#include <math.h>

#include "game/input.h"
#include "raymath.h"
#include "sim/lang.h"

#define WALK_SPEED 4.0f
#define RUN_SPEED 8.5f
#define SNEAK_SPEED 1.8f
#define CROUCH_SPEED 2.2f
#define CROUCH_SNEAK_SPEED 1.2f // agachado y en sigilo
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
    in.toggle_sneak |= input_action_pressed(KA_SNEAK);
    in.toggle_crouch |= input_action_pressed(KA_CROUCH);
    in.jump |= input_action_pressed(KA_JUMP);
    return in;
}

// Aguante [0, 1]: correr lo gasta (~12 s a la carrera), saltar tambien; se recupera al paso y
// mas rapido quieto. Agotado no se corre hasta recobrar un 30 %. A caballo corre la montura.
// No se guarda: al cargar una partida empieza lleno.
#define STAMINA_RUN_COST (1.0f / 12.0f)
#define STAMINA_JUMP_COST 0.08f
#define STAMINA_REGEN_WALK (1.0f / 14.0f)
#define STAMINA_REGEN_REST (1.0f / 6.0f)
#define STAMINA_RECOVER 0.3f
static float g_stamina = 1.0f;
static bool g_winded;

float player_stamina(void) { return g_stamina; }
bool player_winded(void) { return g_winded; }
void player_stamina_spend(float amount) {
    g_stamina = fmaxf(0.0f, g_stamina - amount);
    if (g_stamina <= 0.0f) g_winded = true;
}

void player_init(Player *p, const Terrain *t) {
    g_stamina = 1.0f, g_winded = false;
    *p = (Player){ 0 };
    p->pos = (Vector3){ 0.0f, 0.0f, 22.0f }; // en la entrada sur del campamento
    p->pos.y = terrain_height(t, p->pos.x, p->pos.z);
    p->yaw = PI; // mirando hacia el campamento
    p->grounded = true;
    p->speed_scale = 1.0f;
}

void player_update(Player *p, const Terrain *t, PlayerInput in, float cam_yaw, float dt) {
    if (in.toggle_sneak) p->sneaking = !p->sneaking;
    if (in.toggle_crouch) p->crouching = !p->crouching;

    Vector2 dir = { in.move_x, in.move_z };
    float len = Vector2Length(dir);
    if (len > 1.0f) dir = Vector2Scale(dir, 1.0f / len);
    p->moving = len > 0.05f;

    // Correr cancela el sigilo y levanta al agachado; los dos son lentos. Sin aguante no se corre.
    // A caballo no se agacha.
    bool mounted = p->draw_lift > 0.0f;
    if (g_winded && g_stamina >= STAMINA_RECOVER) g_winded = false;
    if (g_winded && !mounted) in.run = false;
    if (in.run && p->moving) p->sneaking = false, p->crouching = false;
    if (mounted) p->crouching = false;
    p->stance = p->sneaking ? STANCE_SNEAK : (in.run && p->moving ? STANCE_RUN : STANCE_WALK);
    float speed = p->stance == STANCE_SNEAK ? SNEAK_SPEED : (p->stance == STANCE_RUN ? RUN_SPEED : WALK_SPEED);
    if (p->crouching) speed = p->sneaking ? CROUCH_SNEAK_SPEED : CROUCH_SPEED;
    speed *= p->speed_scale > 0.0f ? p->speed_scale : 1.0f;

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

    if (!mounted && p->stance == STANCE_RUN) player_stamina_spend(STAMINA_RUN_COST * dt);
    else g_stamina = fminf(1.0f, g_stamina + (p->moving ? STAMINA_REGEN_WALK : STAMINA_REGEN_REST) * dt);

    float ground = terrain_height(t, p->pos.x, p->pos.z);
    if (p->grounded && in.jump && p->crouching) { // agachado, Espacio lo levanta
        p->crouching = false;
        in.jump = false;
    }
    if (p->grounded && in.jump && p->stance != STANCE_SNEAK && (mounted || g_stamina > 0.0f)) {
        if (!mounted) player_stamina_spend(STAMINA_JUMP_COST);
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
    else p->noise = p->stance == STANCE_RUN ? 1.0f : (p->stance == STANCE_SNEAK ? 0.15f : 0.5f) * (p->crouching ? 0.6f : 1.0f);
}

float player_eye_height(const Player *p) { return p->crouching ? 1.0f : p->sneaking ? 1.4f : 1.6f; }

float player_visibility(const Player *p, bool cover) {
    float v = p->crouching && p->sneaking ? 0.35f : p->crouching ? 0.6f : p->sneaking ? 0.55f : 1.0f;
    return cover ? v * 0.4f : v; // a cubierto: tras la hierba alta, una roca o una mata
}

void player_draw(const Player *p) {
    // Placeholder hasta tener el modelo del personaje (ver docs/ROADMAP.md).
    float h = p->crouching ? 1.1f : 1.7f;
    Color body = p->stance == STANCE_SNEAK ? (Color){ 70, 80, 70, 255 } : (Color){ 150, 52, 40, 255 };
    float y = p->pos.y + p->draw_lift;
    Vector3 base = { p->pos.x, y + 0.35f, p->pos.z };
    Vector3 top = { p->pos.x, y + h - 0.35f, p->pos.z };
    DrawCapsule(base, top, 0.35f, 6, 3, body);
    // Nariz: indica hacia donde mira.
    Vector3 nose = { p->pos.x + sinf(p->yaw) * 0.4f, y + h - 0.35f, p->pos.z + cosf(p->yaw) * 0.4f };
    DrawCube(nose, 0.15f, 0.15f, 0.15f, (Color){ 230, 200, 160, 255 });
}

const char *stance_name(Stance s) {
    switch (s) {
    case STANCE_WALK: return T("caminando");
    case STANCE_RUN: return T("corriendo");
    case STANCE_SNEAK: return T("en sigilo");
    }
    return "?";
}
