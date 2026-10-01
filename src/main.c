// ESTEPA (titulo provisional) — Fase 0: prototipo jugable.
//
// El mundo se dibuja en una textura de baja resolucion (640x360) que luego
// se escala sin suavizado: es el look pixel/low-res y, a la vez, la mayor
// optimizacion de rendimiento en moviles (9 veces menos pixeles que 1080p).
//
// Uso de escritorio:  estepa [--screenshot salida.png] [--frames N] [--sin-teclado]
// --sin-teclado simula un dispositivo Android sin teclado (prueba del aviso).
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/player.h"
#include "platform.h"
#include "raylib.h"
#include "raymath.h"
#include "sim/loadout.h"
#include "sim/troop.h"
#include "world/camp.h"
#include "world/terrain.h"

#define VIRTUAL_W 640
#define VIRTUAL_H 360
#define WORLD_SEED 1206u // ano de la fundacion del Imperio mongol

typedef struct {
    float yaw, pitch, dist;
} CameraRig;

static void camera_update(CameraRig *rig, Camera3D *cam, const Player *p, float dt) {
    // Orbita: Q/E o arrastre con boton derecho; rueda para el zoom.
    if (IsKeyDown(KEY_Q)) rig->yaw += 1.8f * dt;
    if (IsKeyDown(KEY_E)) rig->yaw -= 1.8f * dt;
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 d = GetMouseDelta();
        rig->yaw -= d.x * 0.006f;
        rig->pitch = Clamp(rig->pitch + d.y * 0.004f, 0.05f, 1.25f);
    }
    rig->dist = Clamp(rig->dist - GetMouseWheelMove() * 0.8f, 3.0f, 16.0f);

    Vector3 target = { p->pos.x, p->pos.y + player_eye_height(p), p->pos.z };
    Vector3 back = { -sinf(rig->yaw) * cosf(rig->pitch), sinf(rig->pitch), -cosf(rig->yaw) * cosf(rig->pitch) };
    cam->position = Vector3Add(target, Vector3Scale(back, rig->dist));
    cam->target = target;
}

// Integrantes de ejemplo para la tropa inicial.
static void seed_troop(Troop *t) {
    int lt = troop_recruit(t, "Subotai", TRAIT_LOYALIST);
    troop_assign_role(t, lt, ROLE_LIEUTENANT);
    troop_assign_role(t, troop_recruit(t, "Borte", TRAIT_MERCIFUL | TRAIT_DEVOUT), ROLE_HEALER);
    troop_assign_role(t, troop_recruit(t, "Jebe", TRAIT_BLOODTHIRSTY), ROLE_HUNTER);
    troop_assign_role(t, troop_recruit(t, "Khasar", TRAIT_AMBITIOUS), ROLE_SCOUT);
    troop_assign_role(t, troop_recruit(t, "Temulun", 0), ROLE_COOK);
    troop_take_prisoner(t, "Explorador enemigo", 0);
}

static int first_with_status(const Troop *t, MemberStatus s, int skip_lieutenant) {
    for (int i = t->count - 1; i >= 0; i--) {
        const Member *m = &t->members[i];
        if (m->status == s && !(skip_lieutenant && m->role == ROLE_LIEUTENANT)) return m->id;
    }
    return -1;
}

// Teclas de prueba para la politica del campamento (solo Fase 0).
static void debug_camp_actions(Troop *t, Rng *rng, int *day, char *log, size_t log_len) {
    static const char *names[] = { "Arslan", "Toregene", "Chilaun", "Mukhali", "Sorghaghtani", "Bo'orchu" };
    if (IsKeyPressed(KEY_ONE)) {
        unsigned traits = 1u << rng_range(rng, 5);
        troop_recruit(t, names[rng_range(rng, 6)], traits);
        snprintf(log, log_len, "Reclutaste un nuevo guerrero.");
    } else if (IsKeyPressed(KEY_TWO)) {
        troop_take_prisoner(t, "Cautivo", 0);
        snprintf(log, log_len, "Tomaste un prisionero.");
    } else if (IsKeyPressed(KEY_THREE)) {
        int id = first_with_status(t, STATUS_PRISONER, 0);
        if (id >= 0 && troop_execute(t, id)) snprintf(log, log_len, "Ejecutaste a un prisionero.");
    } else if (IsKeyPressed(KEY_FOUR)) {
        int id = first_with_status(t, STATUS_ACTIVE, 1);
        if (id >= 0 && troop_execute(t, id)) snprintf(log, log_len, "Ejecutaste a uno de los tuyos.");
    } else if (IsKeyPressed(KEY_FIVE)) {
        int id = first_with_status(t, STATUS_ACTIVE, 1);
        if (id >= 0 && troop_banish(t, id)) snprintf(log, log_len, "Desterraste a un integrante.");
    } else if (IsKeyPressed(KEY_SIX)) {
        troop_share_loot(t);
        snprintf(log, log_len, "Repartiste el botin.");
    } else if (IsKeyPressed(KEY_SEVEN)) {
        int id = first_with_status(t, STATUS_PRISONER, 0);
        if (id >= 0 && troop_release_prisoner(t, id)) snprintf(log, log_len, "Liberaste a un prisionero.");
    } else if (IsKeyPressed(KEY_ENTER)) {
        DayReport r = troop_process_day(t, rng);
        (*day)++;
        if (r.rebellion) {
            const Member *m = troop_find(t, r.rebellion_leader);
            snprintf(log, log_len, "Dia %d: REBELION encabezada por %s!", *day, m ? m->name : "?");
        } else {
            snprintf(log, log_len, "Dia %d: %d desertores.", *day, r.deserted);
        }
    }
}

static void draw_bar(int x, int y, int w, float value, Color fill) {
    DrawRectangle(x, y, w, 5, (Color){ 30, 26, 22, 200 });
    DrawRectangle(x, y, (int)(w * Clamp(value / 100.0f, 0.0f, 1.0f)), 5, fill);
}

static void draw_hud(const Player *p, const Troop *t, const Kingdom *k, int day, const char *log) {
    DrawRectangle(4, 4, 214, 104, (Color){ 12, 10, 8, 170 });
    DrawText("ESTEPA - Fase 0", 10, 9, 10, (Color){ 240, 220, 170, 255 });
    DrawText(TextFormat("%s  |  %d fps", stance_name(p->stance), GetFPS()), 10, 22, 10, RAYWHITE);
    DrawText(TextFormat("Dia %d  Tropa: %d  Prisioneros: %d", day, troop_count_with_status(t, STATUS_ACTIVE),
                        troop_count_with_status(t, STATUS_PRISONER)), 10, 36, 10, RAYWHITE);
    float morale = troop_avg_morale(t), loyalty = troop_avg_loyalty(t);
    DrawText("Moral", 10, 50, 10, RAYWHITE);
    draw_bar(62, 53, 100, morale, (Color){ 120, 190, 90, 255 });
    DrawText("Lealtad", 10, 62, 10, RAYWHITE);
    draw_bar(62, 65, 100, loyalty, (Color){ 90, 150, 210, 255 });
    DrawText(TextFormat("Rebelion: %d%%", (int)(troop_rebellion_chance(t) * 100)), 168, 50, 10,
             troop_rebellion_chance(t) > 0.0f ? (Color){ 240, 90, 70, 255 } : RAYWHITE);
    DrawText(TextFormat("%s: %+d", k->name, (int)k->relation), 10, 76, 10, (Color){ 230, 200, 120, 255 });
    DrawText(log, 10, 92, 10, (Color){ 250, 240, 200, 255 });

    DrawRectangle(0, VIRTUAL_H - 26, VIRTUAL_W, 26, (Color){ 12, 10, 8, 170 });
    DrawText("WASD mover  Shift correr  C acechar  Espacio saltar  Q/E o clic der. camara  Rueda zoom",
             4, VIRTUAL_H - 24, 10, (Color){ 210, 200, 180, 255 });
    DrawText("1 reclutar  2 prisionero  3 ejecutar prisionero  4 ejecutar miembro  5 desterrar  6 botin  7 liberar  Enter: dia",
             4, VIRTUAL_H - 12, 10, (Color){ 210, 200, 180, 255 });
}

int main(int argc, char **argv) {
    const char *shot_path = NULL;
    int shot_frames = 90;
    bool simulate_no_keyboard = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) shot_path = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) shot_frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--sin-teclado")) simulate_no_keyboard = true;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
#if defined(__ANDROID__)
    InitWindow(0, 0, "Estepa"); // pantalla completa del dispositivo
#else
    InitWindow(VIRTUAL_W * 2, VIRTUAL_H * 2, "Estepa");
#endif
    SetTargetFPS(60);

    RenderTexture2D lowres = LoadRenderTexture(VIRTUAL_W, VIRTUAL_H);
    SetTextureFilter(lowres.texture, TEXTURE_FILTER_POINT); // pixeles nitidos al escalar

    Terrain terrain;
    terrain_init(&terrain, WORLD_SEED);
    Camp camp;
    camp_init(&camp, &terrain, platform_asset_path("assets/models/yurt.glb"));
    Player player;
    player_init(&player, &terrain);

    Kingdom overlord;
    kingdom_init_iron_khanate(&overlord);
    Troop troop;
    troop_init(&troop, &overlord);
    seed_troop(&troop);
    Rng rng;
    rng_seed(&rng, WORLD_SEED);
    int day = 1;
    char log[96] = "Tu tropa acampa en la estepa.";

    Camera3D cam = { .up = { 0, 1, 0 }, .fovy = 55.0f, .projection = CAMERA_PERSPECTIVE };
    CameraRig rig = { .yaw = PI, .pitch = 0.38f, .dist = 11.0f };

    int frame = 0;
    bool has_keyboard = platform_has_keyboard() && !simulate_no_keyboard;
    while (!WindowShouldClose()) {
        float dt = fminf(GetFrameTime(), 0.05f);
        if (frame % 30 == 0) has_keyboard = platform_has_keyboard() && !simulate_no_keyboard; // conexion en caliente
        frame++;
        // Sin teclado el juego queda en pausa (Android: tablets sin teclado conectado).
        if (has_keyboard) {
            PlayerInput in = player_read_input();
            player_update(&player, &terrain, in, rig.yaw, dt);
            debug_camp_actions(&troop, &rng, &day, log, sizeof(log));
        }
        camera_update(&rig, &cam, &player, dt);
        terrain_update(&terrain, player.pos);

        BeginTextureMode(lowres);
        ClearBackground((Color){ 168, 196, 214, 255 }); // cielo de estepa
        BeginMode3D(cam);
        terrain_draw(&terrain);
        camp_draw(&camp, (float)GetTime());
        player_draw(&player);
        EndMode3D();
        draw_hud(&player, &troop, &overlord, day, log);
        if (!has_keyboard) {
            DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, (Color){ 0, 0, 0, 190 });
            const char *msg = "Conecta un teclado para jugar";
            DrawText(msg, (VIRTUAL_W - MeasureText(msg, 20)) / 2, VIRTUAL_H / 2 - 16, 20, (Color){ 240, 220, 170, 255 });
            const char *sub = "Estepa se juega con teclado fisico (raton o mando opcionales).";
            DrawText(sub, (VIRTUAL_W - MeasureText(sub, 10)) / 2, VIRTUAL_H / 2 + 12, 10, RAYWHITE);
        }
        EndTextureMode();

        // Escala a la ventana conservando la proporcion (con bandas si hace falta).
        float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
        float scale = fminf(sw / VIRTUAL_W, sh / VIRTUAL_H);
        Rectangle src = { 0, 0, (float)VIRTUAL_W, -(float)VIRTUAL_H };
        Rectangle dst = { (sw - VIRTUAL_W * scale) * 0.5f, (sh - VIRTUAL_H * scale) * 0.5f, VIRTUAL_W * scale, VIRTUAL_H * scale };
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(lowres.texture, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
        EndDrawing();

        if (shot_path && frame >= shot_frames) {
            Image img = LoadImageFromTexture(lowres.texture);
            ImageFlipVertical(&img);
            ExportImage(img, shot_path);
            UnloadImage(img);
            break;
        }
    }

    camp_unload(&camp);
    terrain_unload(&terrain);
    UnloadRenderTexture(lowres);
    CloseWindow();
    return 0;
}
