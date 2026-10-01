// ESTEPA (titulo provisional) — Fase 0: prototipo jugable.
//
// El mundo se dibuja en una textura de baja resolucion (640x360) que luego
// se escala sin suavizado: es el look pixel/low-res y, a la vez, la mayor
// optimizacion de rendimiento en moviles (9 veces menos pixeles que 1080p).
//
// Uso de escritorio:  estepa [--screenshot salida.png] [--frames N] [--sin-teclado] [--galeria]
// --galeria muestra todos los objetos del inventario de assets (modelos o marcadores).
// --sin-teclado simula un dispositivo Android sin teclado (prueba del aviso).
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/player.h"
#include "platform.h"
#include "raylib.h"
#include "raymath.h"
#include "sim/champion.h"
#include "sim/clock.h"
#include "sim/loadout.h"
#include "sim/memory_map.h"
#include "sim/troop.h"
#include "ui/minimap.h"
#include "ui/theme.h"
#include "world/camp.h"
#include "world/gallery.h"
#include "world/terrain.h"

#ifndef ESTEPA_VERSION
#define ESTEPA_VERSION "dev"
#endif
#ifndef ESTEPA_BUILD_CODE
#define ESTEPA_BUILD_CODE 0
#endif

#define VIRTUAL_W 640
#define VIRTUAL_H 360
#define WORLD_SEED 1206u // ano de la fundacion del Imperio mongol
#define MINIMAP_RADIUS 50

static MemoryMap g_memory;

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

// Un encuentro con un gran guerrero: se une a la tropa o llega como prisionero.
static int meet_champion(Troop *t, Rng *rng, MemberStatus status, char *log, size_t log_len) {
    Champion c;
    champion_generate(&c, rng);
    int id = troop_add_champion(t, &c, status);
    if (id >= 0) {
        snprintf(log, log_len, status == STATUS_ACTIVE ? "Un gran guerrero se une: %s, %s."
                                                       : "Capturaste a un gran guerrero: %s, %s.",
                 c.name, c.epithet);
    }
    return id;
}

// Teclas de prueba para la politica del campamento (solo Fase 0).
static void debug_camp_actions(Troop *t, Rng *rng, float *world_time, int *last_champion, char *log,
                               size_t log_len) {
    static const char *names[] = { "Arslan", "Toregene", "Chilaun", "Mukhali", "Sorghaghtani", "Bo'orchu" };
    int champ = -1;
    if (IsKeyPressed(KEY_ONE)) {
        // Por azar, quien se acerca puede ser un gran guerrero.
        if (champion_appears(rng, CHAMPION_DEFAULT_CHANCE)) {
            champ = meet_champion(t, rng, STATUS_ACTIVE, log, log_len);
        } else {
            unsigned traits = 1u << rng_range(rng, 5);
            troop_recruit(t, names[rng_range(rng, 6)], traits);
            snprintf(log, log_len, "Reclutaste un nuevo guerrero.");
        }
    } else if (IsKeyPressed(KEY_TWO)) {
        if (champion_appears(rng, CHAMPION_DEFAULT_CHANCE)) {
            champ = meet_champion(t, rng, STATUS_PRISONER, log, log_len);
        } else {
            troop_take_prisoner(t, "Cautivo", 0);
            snprintf(log, log_len, "Tomaste un prisionero.");
        }
    } else if (IsKeyPressed(KEY_EIGHT)) {
        champ = meet_champion(t, rng, STATUS_ACTIVE, log, log_len); // prueba: fuerza el encuentro
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
        *world_time = (float)clock_day(*world_time) * GAME_SECONDS_PER_DAY; // salta al amanecer siguiente
    }
    if (champ >= 0) *last_champion = champ;
}

// Avanza los dias que correspondan al reloj de juego.
static void advance_days(Troop *t, Rng *rng, int *day, float world_time, char *log, size_t log_len) {
    while (*day < clock_day(world_time)) {
        DayReport r = troop_process_day(t, rng);
        (*day)++;
        if (r.rebellion) {
            const Member *m = troop_find(t, r.rebellion_leader);
            snprintf(log, log_len, "Dia %d: REBELION encabezada por %s!", *day, m ? m->name : "?");
        } else if (r.champions_deserted) {
            snprintf(log, log_len, "Dia %d: %d desertores, %d grandes guerreros.", *day, r.deserted,
                     r.champions_deserted);
        } else {
            snprintf(log, log_len, "Dia %d: %d desertores.", *day, r.deserted);
        }
    }
}

static void draw_champion_card(const Troop *t, int id) {
    const Champion *c = troop_champion(t, id);
    const Member *m = troop_find((Troop *)t, id);
    if (!c || !m) return;
    const int w = 380, pad = UI_PANEL_INSET + 4, x0 = (VIRTUAL_W - w) / 2, y0 = 92;
    const int x = x0 + pad, iw = w - 2 * pad;
    char story[256];
    champion_story(c, story, sizeof(story));
    // Alto segun el contenido: titulo, historia, dones, arma (si hay) y dos lineas de atributos.
    int h = 2 * pad + 30 + ui_text_wrapped_height(story, iw, 10) + 10 + 12 * (c->weapon >= 0 ? 4 : 3);
    ui_panel((Rectangle){ x0, y0, w, h }, UI_METAL_GOLD);
    int y = y0 + pad;
    ui_text(TextFormat("%s, %s", c->name, c->epithet), x, y, 20, UI_GOLD_LIGHT);
    ui_text(status_name(m->status), x + iw - MeasureText(status_name(m->status), 10), y + 6, 10, UI_BONE_DIM);
    y += 24;
    ui_divider(x, y, iw, UI_METAL_GOLD);
    y += 6;
    y += ui_text_wrapped(story, x, y, iw, 10, UI_BONE) + 4;
    ui_divider(x, y, iw, UI_METAL_GOLD);
    y += 6;
    char gifts[160] = "";
    for (int g = 0; g < GIFT_COUNT; g++) {
        if (!(c->gifts & (1u << g))) continue;
        size_t n = strlen(gifts);
        snprintf(gifts + n, sizeof(gifts) - n, "%s%s", n ? ", " : "Dones: ", gift_name((ChampionGift)(1u << g)));
    }
    ui_text(gifts, x, y, 10, UI_TURQUOISE);
    y += 12;
    if (c->weapon >= 0) {
        ui_text(TextFormat("Arma: %s", champion_weapon_name(c->weapon)), x, y, 10, UI_GOLD);
        y += 12;
    }
    const CombatStats *s = &c->stats;
    ui_text(TextFormat("Talla x%.2f  Rapidez x%.2f  Fuerza x%.2f  Aguante x%.2f", s->size, s->speed, s->strength,
                       s->endurance), x, y, 10, UI_BONE_DIM);
    y += 12;
    ui_text(s->healing > 0.0f ? TextFormat("Puntería x%.2f  Monta x%.2f  Sanación %.0f%%", s->aim, s->riding,
                                           s->healing * 100.0f)
                              : TextFormat("Puntería x%.2f  Monta x%.2f", s->aim, s->riding),
            x, y, 10, UI_BONE_DIM);
}

static void draw_hud(const Player *p, const Troop *t, const Kingdom *k, int day, const char *log) {
    const int x = 4 + UI_PANEL_INSET, w = 256;
    ui_panel((Rectangle){ 4, 4, w, 140 }, UI_METAL_GOLD);
    ui_text("ESTEPA", x, 16, 10, UI_GOLD_LIGHT);
    ui_text(TextFormat("v%s (build %d)", ESTEPA_VERSION, ESTEPA_BUILD_CODE), x + 48, 16, 10, UI_BONE_DIM);
    ui_divider(x, 29, w - 2 * UI_PANEL_INSET, UI_METAL_GOLD);
    ui_text(TextFormat("%s  |  %d fps", stance_name(p->stance), GetFPS()), x, 34, 10, UI_BONE);
    ui_text(TextFormat("Dia %d  Tropa: %d  Prisioneros: %d", day, troop_count_with_status(t, STATUS_ACTIVE),
                       troop_count_with_status(t, STATUS_PRISONER)), x, 46, 10, UI_BONE);
    ui_text("Moral", x, 59, 10, UI_BONE);
    ui_bar(x + 46, 60, 100, troop_avg_morale(t) / 100.0f, UI_TURQUOISE, UI_METAL_GOLD);
    ui_text("Lealtad", x, 71, 10, UI_BONE);
    ui_bar(x + 46, 72, 100, troop_avg_loyalty(t) / 100.0f, UI_LAPIS, UI_METAL_GOLD);
    float rebellion = troop_rebellion_chance(t);
    ui_text(TextFormat("Rebelion %d%%", (int)(rebellion * 100)), x + 152, 65, 10,
            rebellion > 0.0f ? UI_CARNELIAN : UI_BONE_DIM);
    ui_text(TextFormat("%s: %+d", k->name, (int)k->relation), x, 85, 10, UI_GOLD);
    ui_divider(x, 99, w - 2 * UI_PANEL_INSET, UI_METAL_GOLD);
    ui_text_wrapped(log, x, 104, w - 2 * UI_PANEL_INSET, 10, UI_BONE);

    ui_strip((Rectangle){ 0, VIRTUAL_H - 29, VIRTUAL_W, 29 }, UI_METAL_GOLD);
    ui_text("WASD mover  Shift correr  C acechar  Espacio saltar  Q/E o clic der. camara  Rueda zoom  M marcar",
            4, VIRTUAL_H - 21, 10, UI_BONE_DIM);
    ui_text("1 reclutar 2 prisionero 3 ejec. prisionero 4 ejec. miembro 5 desterrar 6 botin 7 liberar 8 gran guerrero G ficha Enter dia",
            4, VIRTUAL_H - 11, 10, UI_BONE_DIM);
}

static void draw_keyboard_notice(void) {
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, (Color){ 8, 6, 5, 200 });
    const int w = 400, h = 76;
    ui_panel((Rectangle){ (VIRTUAL_W - w) / 2, (VIRTUAL_H - h) / 2, w, h }, UI_METAL_GOLD);
    ui_text_centered("Conecta un teclado para jugar", VIRTUAL_W / 2, VIRTUAL_H / 2 - 18, 20, UI_GOLD_LIGHT);
    ui_text_centered("Estepa se juega con teclado fisico (raton o mando opcionales).", VIRTUAL_W / 2,
                     VIRTUAL_H / 2 + 8, 10, UI_BONE);
}

int main(int argc, char **argv) {
    const char *shot_path = NULL;
    int shot_frames = 90;
    bool simulate_no_keyboard = false;
    bool gallery_mode = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) shot_path = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) shot_frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--sin-teclado")) simulate_no_keyboard = true;
        else if (!strcmp(argv[i], "--galeria")) gallery_mode = true;
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
    camp_init(&camp, &terrain, platform_asset_path("assets/models/estructura/vivienda/yurta_comun.glb"));
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
    float world_time = 0.0f; // segundos de juego (avanza solo si no hay pausa)
    int last_champion = -1;  // id del ultimo gran guerrero encontrado
    bool show_card = false;
    memmap_init(&g_memory);
    Minimap minimap;
    minimap_init(&minimap, MINIMAP_RADIUS, 2.0f);
    char log[128] = "Tu tropa acampa en la estepa.";

    Gallery gallery = { 0 };
    if (gallery_mode && gallery_init(&gallery, &terrain, (Vector3){ 100.0f, 0.0f, 60.0f })) {
        player.pos = gallery.start;
        player.pos.y = terrain_height(&terrain, player.pos.x, player.pos.z);
        player.yaw = PI; // mirando a -Z, hacia las filas
        snprintf(log, sizeof(log), "Galeria: %d objetos (%d con modelo).", gallery.count, gallery.loaded);
    } else {
        gallery_mode = false;
    }

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
            world_time += dt;
            int prev_champion = last_champion;
            debug_camp_actions(&troop, &rng, &world_time, &last_champion, log, sizeof(log));
            if (last_champion != prev_champion) show_card = true; // ficha al conocerlo
            advance_days(&troop, &rng, &day, world_time, log, sizeof(log));
            if (IsKeyPressed(KEY_G)) show_card = !show_card && last_champion >= 0;
            memmap_visit(&g_memory, player.pos.x, player.pos.z, dt, world_time);
            if (IsKeyPressed(KEY_M)) {
                MarkerKind kind = IsKeyDown(KEY_LEFT_SHIFT) ? MARKER_DANGER : MARKER_INTEREST;
                bool placed = memmap_toggle_marker(&g_memory, player.pos.x, player.pos.z, kind, 6.0f);
                snprintf(log, sizeof(log), placed ? "Marcaste este lugar en el mapa." : "Quitaste la marca.");
            }
        }
        camera_update(&rig, &cam, &player, dt);
        terrain_update(&terrain, player.pos);

        BeginTextureMode(lowres);
        ClearBackground((Color){ 168, 196, 214, 255 }); // cielo de estepa
        BeginMode3D(cam);
        terrain_draw(&terrain);
        camp_draw(&camp, (float)GetTime());
        if (gallery_mode) gallery_draw(&gallery);
        player_draw(&player);
        EndMode3D();
        if (gallery_mode) {
            gallery_draw_labels(&gallery, cam, player.pos, VIRTUAL_W, VIRTUAL_H);
        } else {
            draw_hud(&player, &troop, &overlord, day, log);
            minimap_draw(&minimap, &g_memory, (Vector2){ VIRTUAL_W - MINIMAP_RADIUS - 10, MINIMAP_RADIUS + 13 },
                         player.pos, rig.yaw, player.yaw, world_time);
            if (show_card) draw_champion_card(&troop, last_champion);
        }
        if (!has_keyboard) draw_keyboard_notice();
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

    if (gallery_mode) gallery_unload(&gallery);
    minimap_unload(&minimap);
    memmap_free(&g_memory);
    camp_unload(&camp);
    terrain_unload(&terrain);
    UnloadRenderTexture(lowres);
    CloseWindow();
    return 0;
}
