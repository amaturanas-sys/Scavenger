// ESTEPA (titulo provisional) — Fase 0: prototipo jugable.
//
// El mundo se dibuja en una textura de baja resolucion (640x360) que luego
// se escala sin suavizado: es el look pixel/low-res y, a la vez, la mayor
// optimizacion de rendimiento en moviles (9 veces menos pixeles que 1080p).
//
// Uso de escritorio:  estepa [--screenshot salida.png] [--frames N] [--sin-teclado] [--galeria]
//                     [--dia N] [--minuto M] [--pos X Z] [--trampa hielo|nieve|arena|rescate]
// --galeria muestra todos los objetos del inventario de assets (modelos o marcadores).
// --dia y --minuto ponen el reloj (minutos desde el amanecer), p. ej. para ver la noche.
// --pos lleva al jugador a otro lugar del mundo (p. ej. a la cordillera).
// --trampa arranca en un peligro con su minijuego (prueba).
// --enemigos bandidos|culto|lobos los pone delante; --heridas hiere al jugador y a la tribu (pruebas).
// --sin-teclado simula un dispositivo Android sin teclado (prueba del aviso).
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/actions_game.h"
#include "game/combat_game.h"
#include "game/hazards_game.h"
#include "game/player.h"
#include "platform.h"
#include "raylib.h"
#include "raymath.h"
#include "sim/champion.h"
#include "sim/climate.h"
#include "sim/clock.h"
#include "sim/inventory.h"
#include "sim/loadout.h"
#include "sim/memory_map.h"
#include "sim/troop.h"
#include "ui/minimap.h"
#include "ui/theme.h"
#include "world/camp.h"
#include "world/gallery.h"
#include "world/sky.h"
#include "world/weather.h"
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
static Inventory g_inventory; // assets/inventario.tsv: lo usan las acciones y los objetos del mundo
static Props g_props;
static GameActions g_actions;
static Hazards g_hazards;   // frio, barro, hielo, socavones y rescates
static Combat g_combat;     // salud, heridas, enemigos y combate
static Climate g_climate;   // clima del cuadro anterior (lo usan los peligros)

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
    } else if (IsKeyPressed(KEY_N)) {
        *world_time += 120.0f; // adelanta dos minutos (para ver el ocaso y la noche)
    }
    if (champ >= 0) *last_champion = champ;
}

// Avanza los dias que correspondan al reloj de juego.
static void advance_days(Troop *t, Rng *rng, int *day, float world_time, const Terrain *terrain, char *log,
                         size_t log_len) {
    while (*day < clock_day(world_time)) {
        DayReport r = troop_process_day(t, rng);
        (*day)++;
        // Comida, recoleccion, efectos del campamento y trabajos de los NPCs.
        ga_new_day(&g_actions, &g_props, terrain, t, &g_memory, world_time, *day, log, log_len);
        hz_new_day(&g_hazards, &g_climate, &g_actions, &g_props, t, log, log_len); // las noches heladas gastan lena
        cb_new_day(&g_combat, t); // el jugador tambien descansa y sana
        if (r.rebellion) {
            const Member *m = troop_find(t, r.rebellion_leader);
            snprintf(log, log_len, "Dia %d: REBELION encabezada por %s!", *day, m ? m->name : "?");
        } else if (r.champions_deserted) {
            snprintf(log, log_len, "Dia %d: %d desertores, %d grandes guerreros.", *day, r.deserted,
                     r.champions_deserted);
        } else if (r.deserted) {
            snprintf(log, log_len, "Dia %d: %d desertores.", *day, r.deserted);
        }
    }
}

// El aspecto del suelo, el agua y los glaciares sigue al clima.
static void apply_climate_look(Terrain *terrain, const Climate *c) {
    TerrainLook look = {
        .greenness = c->greenness, .autumn = c->autumn, .snow_cover = c->snow_cover, .wetness = c->wetness,
        .snowline = terrain->plain + c->snowline, .water_level = terrain->lake_base + c->water_level, .ice = c->ice,
    };
    terrain_set_look(terrain, &look);
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

// "Dia 3 · primavera · noche (8 min)": los minutos que faltan para el cambio de luz.
static const char *clock_text(float world_time) {
    int day = clock_day(world_time);
    float x = clock_seconds_into_day(world_time), light = clock_daylight_seconds(day);
    float left = x < light ? light - x : GAME_SECONDS_PER_DAY - x;
    return TextFormat("Día %d · %s · %s (%d min)", day, season_name(clock_season(day)),
                      phase_name(clock_phase(world_time)), (int)ceilf(left / 60.0f));
}

// Barra del ciclo bajo el minimapa: oro la luz, lapislazuli la noche; la marca es la hora.
static void draw_clock_bar(int cx, int y, int w, float world_time) {
    int day = clock_day(world_time), x0 = cx - w / 2;
    int lw = (int)(w * clock_daylight_fraction(day) + 0.5f);
    DrawRectangle(x0 - 1, y - 1, w + 2, 6, UI_LEATHER_CRACK);
    DrawRectangle(x0, y, lw, 4, UI_GOLD);
    DrawRectangle(x0 + lw, y, w - lw, 4, UI_LAPIS);
    int mx = x0 + (int)(w * clock_seconds_into_day(world_time) / GAME_SECONDS_PER_DAY);
    DrawRectangle(mx - 1, y - 2, 3, 8, clock_is_night(world_time) ? UI_BONE : UI_CARNELIAN);
}

// Tiempo y temperatura bajo el minimapa, alineados a la derecha.
static void draw_weather_text(int right, int y, const Climate *c) {
    const char *txt = TextFormat("%s · %d °C", weather_name(c->weather), (int)lroundf(c->temperature));
    Color col = c->snow > 0.3f || c->rain > 0.3f ? UI_TURQUOISE : UI_BONE;
    ui_text(txt, right - MeasureText(txt, 10), y, 10, col);
}

static void draw_hud(const Player *p, const Troop *t, const Kingdom *k, float world_time, const char *hands,
                     const char *log) {
    const int x = 4 + UI_PANEL_INSET, w = 256;
    ui_panel((Rectangle){ 4, 4, w, 152 }, UI_METAL_GOLD);
    ui_text("ESTEPA", x, 16, 10, UI_GOLD_LIGHT);
    ui_text(TextFormat("v%s (build %d) · %d fps", ESTEPA_VERSION, ESTEPA_BUILD_CODE, GetFPS()), x + 48, 16, 10,
            UI_BONE_DIM);
    ui_divider(x, 29, w - 2 * UI_PANEL_INSET, UI_METAL_GOLD);
    ui_text(clock_text(world_time), x, 34, 10, clock_is_night(world_time) ? UI_BONE_DIM : UI_GOLD_LIGHT);
    ui_text(TextFormat("%s · Tropa: %d · Prisioneros: %d", stance_name(p->stance),
                       troop_count_with_status(t, STATUS_ACTIVE), troop_count_with_status(t, STATUS_PRISONER)),
            x, 46, 10, UI_BONE);
    ui_text("Moral", x, 59, 10, UI_BONE);
    ui_bar(x + 46, 60, 100, troop_avg_morale(t) / 100.0f, UI_TURQUOISE, UI_METAL_GOLD);
    ui_text("Lealtad", x, 71, 10, UI_BONE);
    ui_bar(x + 46, 72, 100, troop_avg_loyalty(t) / 100.0f, UI_LAPIS, UI_METAL_GOLD);
    float rebellion = troop_rebellion_chance(t);
    ui_text(TextFormat("Rebelion %d%%", (int)(rebellion * 100)), x + 152, 65, 10,
            rebellion > 0.0f ? UI_CARNELIAN : UI_BONE_DIM);
    ui_text(hands, x, 85, 10, UI_TURQUOISE);
    ui_text(TextFormat("%s: %+d", k->name, (int)k->relation), x, 97, 10, UI_GOLD);
    ui_divider(x, 111, w - 2 * UI_PANEL_INSET, UI_METAL_GOLD);
    ui_text_wrapped(log, x, 116, w - 2 * UI_PANEL_INSET, 10, UI_BONE);

    ui_strip((Rectangle){ 0, VIRTUAL_H - 40, VIRTUAL_W, 40 }, UI_METAL_GOLD);
    ui_text("WASD mover  Shift correr  C acechar  Espacio saltar  Q/E cámara  M marcar  V golpear  Z cubrirse  B vendar  P salud",
            4, VIRTUAL_H - 32, 10, UI_BONE_DIM);
    ui_text("Tab acciones, obras y forja  X empuñadura  H enfundar  F tomar  T lanzar  R montar  I acopio  Y escolta", 4, VIRTUAL_H - 21, 10,
            UI_BONE);
    ui_text("1 reclutar 2 cautivo 3/4 ejecutar 5 desterrar 6 botín 7 liberar 8 guerrero G ficha Enter día N hora 9 enemigos",
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
    int start_day = 1;
    float start_minute = 4.0f; // la partida empieza a media manana
    bool start_pos = false;
    const char *start_trap = NULL, *start_enemies = NULL;
    bool start_wounds = false;
    float start_x = 0.0f, start_z = 0.0f;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) shot_path = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) shot_frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--sin-teclado")) simulate_no_keyboard = true;
        else if (!strcmp(argv[i], "--galeria")) gallery_mode = true;
        else if (!strcmp(argv[i], "--dia") && i + 1 < argc) start_day = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--minuto") && i + 1 < argc) start_minute = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--trampa") && i + 1 < argc) start_trap = argv[++i];
        else if (!strcmp(argv[i], "--enemigos") && i + 1 < argc) start_enemies = argv[++i];
        else if (!strcmp(argv[i], "--heridas")) start_wounds = true;
        else if (!strcmp(argv[i], "--pos") && i + 2 < argc) {
            start_pos = true;
            start_x = (float)atof(argv[++i]);
            start_z = (float)atof(argv[++i]);
        }
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
    // Segundos de juego (avanza solo si no hay pausa). Cada dia empieza al amanecer.
    float world_time = (float)(start_day > 1 ? start_day - 1 : 0) * GAME_SECONDS_PER_DAY +
                       fmaxf(0.0f, fminf(start_minute * 60.0f, GAME_SECONDS_PER_DAY - 1.0f));
    int day = clock_day(world_time);
    Sky sky;
    sky_init(&sky, WORLD_SEED);
    WeatherFx weather;
    weather_init(&weather, WORLD_SEED);
    if (start_pos) {
        player.pos = (Vector3){ start_x, terrain_height(&terrain, start_x, start_z), start_z };
    }
    int last_champion = -1;  // id del ultimo gran guerrero encontrado
    bool show_card = false;
    memmap_init(&g_memory);
    Minimap minimap;
    minimap_init(&minimap, MINIMAP_RADIUS, 2.0f);
    char log[128] = "Tu tropa acampa en la estepa.";
    char *inv_text = LoadFileText(platform_asset_path("assets/inventario.tsv"));
    if (inv_text) {
        inventory_parse(&g_inventory, inv_text);
        UnloadFileText(inv_text);
    }
    props_init(&g_props, &g_inventory);
    ga_init(&g_actions, &g_inventory, &g_props, &terrain, WORLD_SEED);
    hz_init(&g_hazards, WORLD_SEED);
    cb_init(&g_combat, WORLD_SEED);
    g_hazards.body = &g_combat.player; // caidas y congelacion hieren
    g_climate = climate_at(world_time, WORLD_SEED);
    if (!gallery_mode) apply_climate_look(&terrain, &g_climate);

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
            // Con el menu de acciones abierto el jugador no se mueve (las flechas eligen).
            bool menu = ga_menu_open(&g_actions);
            bool blocked = ga_blocks_input(&g_actions) || hz_blocks_input(&g_hazards) || cb_blocks_input(&g_combat);
            PlayerInput in = blocked ? (PlayerInput){ 0 } : player_read_input();
            player.speed_scale = ga_speed_scale(&g_actions) * hz_speed_scale(&g_hazards) * cb_speed_scale(&g_combat);
            player.draw_lift = g_actions.mounted >= 0 ? 1.1f : 0.0f;
            if (!g_actions.climbing) player_update(&player, &terrain, in, rig.yaw, dt);
            world_time += dt;
            int prev_champion = last_champion;
            if (!menu) debug_camp_actions(&troop, &rng, &world_time, &last_champion, log, sizeof(log));
            if (!gallery_mode) ga_update(&g_actions, &g_props, &terrain, &player, &troop, dt, log, sizeof(log));
            if (!gallery_mode) ga_after_player(&g_actions, &g_props, &terrain, &player);
            if (start_trap && frame == 3 && !gallery_mode) { // prueba: la tribu ya esta ubicada
                hz_force(&g_hazards, start_trap, &player, &g_actions, &troop, &terrain);
                start_trap = NULL;
            }
            if (start_enemies && frame == 3 && !gallery_mode) {
                cb_spawn_group(&g_combat, start_enemies, &player, &terrain, 7.0f, log, sizeof(log));
                start_enemies = NULL;
            }
            if (start_wounds && frame == 3 && !gallery_mode) { // prueba: heridas a la vista
                health_hit(&g_combat.player, &rng, 28.0f, WOUND_CUT, PART_LEG_L);
                health_hit(&g_combat.player, &rng, 14.0f, WOUND_BRUISE, PART_ARM_R);
                for (int i = 0; i < 2 && i < troop.count; i++)
                    health_hit(&troop.members[i].health, &rng, i ? 110.0f : 30.0f, WOUND_BITE, PART_TORSO);
                g_combat.show_panel = true;
                start_wounds = false;
            }
            if (!gallery_mode)
                hz_update(&g_hazards, &g_climate, &terrain, &player, &g_actions, &g_props, &troop, camp.fire, world_time,
                          dt, log, sizeof(log));
            if (!gallery_mode)
                cb_update(&g_combat, &player, &g_actions, &troop, &terrain, camp.fire,
                          !menu && !hz_blocks_input(&g_hazards), clock_is_night(world_time), g_climate.temp_mean < 0.0f,
                          dt, log, sizeof(log));
            if (last_champion != prev_champion) show_card = true; // ficha al conocerlo
            advance_days(&troop, &rng, &day, world_time, &terrain, log, sizeof(log));
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
        // Clima: estacion, tiempo, nieve, lagos y glaciares (la galeria se ve siempre igual).
        Climate climate = climate_at(world_time, WORLD_SEED);
        if (gallery_mode) climate = (Climate){ .clouds = 0.1f, .temperature = 20.0f };
        if (!gallery_mode) {
            apply_climate_look(&terrain, &climate);
            props_set_season(&g_props, clock_season(clock_day(world_time)));
            weather_update(&weather, &climate, dt);
        }
        g_climate = climate;

        BeginTextureMode(lowres);
        ClearBackground(sky_clear_color(climate.clouds));
        BeginMode3D(cam);
        terrain_draw(&terrain);
        camp_draw(&camp, (float)GetTime(), climate.snow_cover);
        if (gallery_mode) gallery_draw(&gallery);
        bool player_model = !gallery_mode && ga_draw_player(&g_actions, &g_props, &player, (float)GetTime());
        if (!player_model && !g_combat.player.down) player_draw(&player);
        if (!gallery_mode) cb_draw_world(&g_combat, &g_props, &g_actions, &player, player_model, (float)GetTime());
        if (!gallery_mode) ga_draw_world(&g_actions, &g_props, &terrain, &troop, &player, (float)GetTime());
        if (!gallery_mode) terrain_draw_water(&terrain, (float)GetTime()); // translucida: despues de lo opaco
        if (!gallery_mode) hz_draw_world(&g_hazards, &terrain, &g_actions, &troop, (float)GetTime());
        EndMode3D();
        if (!gallery_mode) { // la galeria se ve siempre de dia
            // Noche: se oscurece todo y se suman las estrellas, las llamas y el brillo de los fuegos.
            sky_apply_tint(world_time, climate.clouds, VIRTUAL_W, VIRTUAL_H);
            BeginMode3D(cam);
            sky_draw_stars(&sky, cam, world_time, climate.clouds);
            camp_draw_flame(&camp, (float)GetTime());
            weather_draw(&weather, &climate, cam, (float)GetTime(), clock_light(world_time));
            EndMode3D();
            Vector3 light_pos[SKY_MAX_LIGHTS];
            float light_radius[SKY_MAX_LIGHTS];
            light_pos[0] = (Vector3){ camp.fire.x, camp.fire.y + 0.4f, camp.fire.z };
            light_radius[0] = 9.0f;
            int lights = 1 + ga_lights(&g_actions, &g_props, &player, light_pos + 1, light_radius + 1, SKY_MAX_LIGHTS - 1);
            sky_draw_lights(cam, light_pos, light_radius, lights, world_time, (float)GetTime(), VIRTUAL_W, VIRTUAL_H);
            weather_draw_screen(&weather, &climate, VIRTUAL_W, VIRTUAL_H);
            cb_draw_overlay(&g_combat, &g_actions, &troop, cam, VIRTUAL_W, VIRTUAL_H);
        }
        if (gallery_mode) {
            gallery_draw_labels(&gallery, cam, player.pos, VIRTUAL_W, VIRTUAL_H);
        } else {
            draw_hud(&player, &troop, &overlord, world_time, ga_hands_text(&g_actions), log);
            minimap_draw(&minimap, &g_memory, (Vector2){ VIRTUAL_W - MINIMAP_RADIUS - 10, MINIMAP_RADIUS + 13 },
                         player.pos, rig.yaw, player.yaw, world_time);
            draw_clock_bar(VIRTUAL_W - MINIMAP_RADIUS - 10, 2 * MINIMAP_RADIUS + 19, 2 * MINIMAP_RADIUS - 8, world_time);
            draw_weather_text(VIRTUAL_W - 14, 2 * MINIMAP_RADIUS + 27, &climate);
            ga_draw_hud(&g_actions, &g_props, &troop, VIRTUAL_W, VIRTUAL_H);
            hz_draw_hud(&g_hazards, &troop, VIRTUAL_W - 14, 2 * MINIMAP_RADIUS + 40, VIRTUAL_W, VIRTUAL_H);
            cb_draw_hud(&g_combat, &g_actions, &troop, VIRTUAL_W - 14, 2 * MINIMAP_RADIUS + 63, VIRTUAL_W, VIRTUAL_H);
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
    props_unload(&g_props);
    inventory_free(&g_inventory);
    minimap_unload(&minimap);
    memmap_free(&g_memory);
    camp_unload(&camp);
    terrain_unload(&terrain);
    UnloadRenderTexture(lowres);
    CloseWindow();
    return 0;
}
