// SCAVENGERS THRIVE - THEY COME FROM THE STEPPES (nombre interno: estepa).
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
// --enemigos bandidos|culto|lobos|arqueros los pone delante; --heridas hiere al jugador y a la tribu;
// --apuntar empuña el arco tenso (pruebas).
// --sin-teclado simula un dispositivo Android sin teclado (prueba del aviso).
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/actions_game.h"
#include "game/combat_game.h"
#include "game/disasters_game.h"
#include "game/fauna_game.h"
#include "game/inventory_game.h"
#include "game/save_game.h"
#include "game/settings.h"
#include "game/camp_game.h"
#include "game/talents_game.h"
#include "game/travel_game.h"
#include "game/title_menu.h"
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
#include "ui/icons.h"
#include "ui/theme.h"
#include "world/camp.h"
#include "world/gallery.h"
#include "world/sky.h"
#include "world/weather.h"
#include "world/terrain.h"
#include "sim/lang.h"

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
static Disasters g_dz; // fuego, rayos y lluvia torrencial
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
    troop_assign_role(t, troop_recruit(t, "Ulagan", TRAIT_DEVOUT), ROLE_DRUID);   // tatua y encanta
    troop_assign_role(t, troop_recruit(t, "Altani", 0), ROLE_GOLDSMITH);          // hace joyas
    troop_take_prisoner(t, T("Explorador enemigo"), 0);
    for (int i = 0; i < t->count; i++) { // cada uno con su mochila; el lugarteniente y el cazador, una mediana
        Member *m = &t->members[i];
        if (m->status != STATUS_ACTIVE) continue;
        m->pack = m->role == ROLE_LIEUTENANT || m->role == ROLE_HUNTER ? PACK_MEDIUM : PACK_SMALL;
        bag_init_pack(&m->bag, (PackSize)m->pack);
    }
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
        snprintf(log, log_len, status == STATUS_ACTIVE ? T("Un gran guerrero se une: %s, %s.")
                                                       : T("Capturaste a un gran guerrero: %s, %s."),
                 c.name, T(c.epithet));
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
            snprintf(log, log_len, "%s", T("Reclutaste un nuevo guerrero."));
        }
    } else if (IsKeyPressed(KEY_TWO)) {
        if (champion_appears(rng, CHAMPION_DEFAULT_CHANCE)) {
            champ = meet_champion(t, rng, STATUS_PRISONER, log, log_len);
        } else {
            troop_take_prisoner(t, T("Cautivo"), 0);
            snprintf(log, log_len, "%s", T("Tomaste un prisionero."));
        }
    } else if (IsKeyPressed(KEY_EIGHT)) {
        champ = meet_champion(t, rng, STATUS_ACTIVE, log, log_len); // prueba: fuerza el encuentro
    } else if (IsKeyPressed(KEY_THREE)) {
        int id = first_with_status(t, STATUS_PRISONER, 0);
        if (id >= 0 && troop_execute(t, id)) snprintf(log, log_len, "%s", T("Ejecutaste a un prisionero."));
    } else if (IsKeyPressed(KEY_FOUR)) {
        int id = first_with_status(t, STATUS_ACTIVE, 1);
        if (id >= 0 && troop_execute(t, id)) snprintf(log, log_len, "%s", T("Ejecutaste a uno de los tuyos."));
    } else if (IsKeyPressed(KEY_FIVE)) {
        int id = first_with_status(t, STATUS_ACTIVE, 1);
        if (id >= 0 && troop_banish(t, id)) snprintf(log, log_len, "%s", T("Desterraste a un integrante."));
    } else if (IsKeyPressed(KEY_SIX)) {
        troop_share_loot(t);
        snprintf(log, log_len, "%s", T("Repartiste el botin."));
    } else if (IsKeyPressed(KEY_SEVEN)) {
        int id = first_with_status(t, STATUS_PRISONER, 0);
        if (id >= 0 && troop_release_prisoner(t, id)) snprintf(log, log_len, "%s", T("Liberaste a un prisionero."));
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
        fg_new_day(&g_actions);   // el ganado se vuelve a ordeñar
        dz_new_day(&g_dz, &g_props, &g_actions, t, g_climate.wetness > 0.4f, log, log_len); // desgaste y reparaciones
        if (r.rebellion) {
            const Member *m = troop_find(t, r.rebellion_leader);
            snprintf(log, log_len, T("Dia %d: REBELION encabezada por %s!"), *day, m ? m->name : "?");
        } else if (r.champions_deserted) {
            snprintf(log, log_len, T("Dia %d: %d desertores, %d grandes guerreros."), *day, r.deserted,
                     r.champions_deserted);
        } else if (r.deserted) {
            snprintf(log, log_len, T("Dia %d: %d desertores."), *day, r.deserted);
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
    ui_text(TextFormat("%s, %s", c->name, T(c->epithet)), x, y, 20, UI_GOLD_LIGHT);
    const char *st = T(status_name(m->status));
    ui_text(st, x + iw - MeasureText(st, 10), y + 6, 10, UI_BONE_DIM);
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
        snprintf(gifts + n, sizeof(gifts) - n, "%s%s", n ? ", " : T("Dones: "), gift_name((ChampionGift)(1u << g)));
    }
    ui_text(gifts, x, y, 10, UI_TURQUOISE);
    y += 12;
    if (c->weapon >= 0) {
        ui_text(TextFormat(T("Arma: %s"), champion_weapon_name(c->weapon)), x, y, 10, UI_GOLD);
        y += 12;
    }
    const CombatStats *s = &c->stats;
    ui_text(TextFormat(T("Talla x%.2f  Rapidez x%.2f  Fuerza x%.2f  Aguante x%.2f"), s->size, s->speed, s->strength,
                       s->endurance), x, y, 10, UI_BONE_DIM);
    y += 12;
    ui_text(s->healing > 0.0f ? TextFormat(T("Puntería x%.2f  Monta x%.2f  Sanación %.0f%%"), s->aim, s->riding,
                                           s->healing * 100.0f)
                              : TextFormat(T("Puntería x%.2f  Monta x%.2f"), s->aim, s->riding),
            x, y, 10, UI_BONE_DIM);
}

// "Dia 3 · primavera · noche (8 min)": los minutos que faltan para el cambio de luz.
static const char *clock_text(float world_time) {
    int day = clock_day(world_time);
    float x = clock_seconds_into_day(world_time), light = clock_daylight_seconds(day);
    float left = x < light ? light - x : GAME_SECONDS_PER_DAY - x;
    return TextFormat(T("Día %d · %s · %s (%d min)"), day, season_name(clock_season(day)),
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

// HUD limpio: arriba a la izquierda, lo esencial (hora, tribu, animo, manos); el registro
// abajo, solo mientras es reciente; los controles, con F1.
static void draw_hud(const Troop *t, float world_time, const char *hands, const char *log, float log_age, bool menu) {
    const int x = 4 + 8, w = 236;
    DrawRectangle(4, 4, w, 46, (Color){ 20, 14, 10, 170 });
    DrawRectangleLines(4, 4, w, 46, Fade(UI_GOLD_DARK, 0.9f));
    ui_text(clock_text(world_time), x, 8, 10, clock_is_night(world_time) ? UI_BONE_DIM : UI_GOLD_LIGHT);
    ui_text(TextFormat(T("Tropa %d"), troop_count_with_status(t, STATUS_ACTIVE)), x, 22, 10, UI_BONE);
    int prisoners = troop_count_with_status(t, STATUS_PRISONER);
    if (prisoners) ui_text(TextFormat(T("· %d cautivo%s"), prisoners, prisoners == 1 ? "" : "s"), x + 46, 22, 10, UI_BONE_DIM);
    // Animo (turquesa) y lealtad (lapislazuli), dos barras cortas.
    ui_bar(x + 128, 23, 46, troop_avg_morale(t) / 100.0f, UI_TURQUOISE, UI_METAL_GOLD);
    ui_bar(x + 178, 23, 46, troop_avg_loyalty(t) / 100.0f, UI_LAPIS, UI_METAL_GOLD);
    ui_text(hands, x, 36, 10, UI_TURQUOISE);
    float rebellion = troop_rebellion_chance(t);
    if (rebellion > 0.0f) ui_text(TextFormat(T("Riesgo de rebelión %d%%"), (int)(rebellion * 100)), x, 54, 10, UI_CARNELIAN);
    // Registro: abajo a la izquierda, se desvanece a los 8 s.
    if (log[0] && log_age < 8.0f) {
        float a = log_age < 6.0f ? 1.0f : (8.0f - log_age) / 2.0f;
        int lw = MeasureText(log, 10);
        if (lw > VIRTUAL_W - 180) lw = VIRTUAL_W - 180;
        // Con un menu abierto, la leyenda ocupa la base: el registro sube encima de ella.
        int ly = menu ? VIRTUAL_H - 40 : VIRTUAL_H - 22, lx = menu ? (VIRTUAL_W - lw - 14) / 2 : 4;
        DrawRectangle(lx, ly, lw + 14, 18, Fade((Color){ 20, 14, 10, 255 }, 0.7f * a));
        DrawRectangle(lx, ly, 2, 18, Fade(UI_GOLD, a));
        BeginScissorMode(lx, ly, lw + 14, 18);
        ui_text(log, lx + 7, ly + 4, 10, Fade(UI_BONE, a));
        EndScissorMode();
    }
    if (menu) return;
    const char *hint = T("F1 controles · Esc menú");
    ui_text(hint, VIRTUAL_W - 8 - MeasureText(hint, 10), VIRTUAL_H - 16, 10, Fade(UI_BONE_DIM, 0.8f));
}

// Una partida nueva: la tribu inicial en el campamento, a media mañana del dia pedido.
static void game_new(GameState *g, Terrain *terrain, int start_day, float start_minute, char *log, size_t len) {
    player_init(g->player, terrain);
    kingdom_init_iron_khanate(g->overlord);
    troop_init(g->troop, g->overlord);
    seed_troop(g->troop);
    rng_seed(g->rng, WORLD_SEED);
    *g->world_time = (float)(start_day > 1 ? start_day - 1 : 0) * GAME_SECONDS_PER_DAY +
                     fmaxf(0.0f, fminf(start_minute * 60.0f, GAME_SECONDS_PER_DAY - 1.0f));
    *g->day = clock_day(*g->world_time);
    *g->last_champion = -1;
    *g->cam_yaw = PI;
    memmap_free(g->mem);
    memmap_init(g->mem);
    g->props->count = 0; // los modelos cargados se conservan
    // El clima primero: los animales iniciales miran donde hay agua.
    g_climate = climate_at(*g->world_time, WORLD_SEED);
    apply_climate_look(terrain, &g_climate);
    ga_init(g->ga, g->inv, g->props, terrain, WORLD_SEED);
    hz_init(g->hz, WORLD_SEED);
    cb_init(g->cb, WORLD_SEED);
    dz_init(g->dz, WORLD_SEED);
    g->hz->body = &g->cb->player; // caidas y congelacion hieren
    snprintf(log, len, "%s", T("Tu tropa acampa en la estepa."));
}

static void draw_keyboard_notice(void) {
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, (Color){ 8, 6, 5, 200 });
    const int w = 400, h = 76;
    ui_panel((Rectangle){ (VIRTUAL_W - w) / 2, (VIRTUAL_H - h) / 2, w, h }, UI_METAL_GOLD);
    ui_text_centered(T("Conecta un teclado para jugar"), VIRTUAL_W / 2, VIRTUAL_H / 2 - 18, 20, UI_GOLD_LIGHT);
    ui_text_centered(T("Scavengers Thrive se juega con teclado físico (ratón o mando opcionales)."), VIRTUAL_W / 2,
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
    bool start_wounds = false, start_aim = false, start_lake = false, start_fire = false, start_menu = false;
    MenuScreen start_menu_screen = MENU_TITLE;
    int start_save = -1, start_load = -1;
    int start_pause = 0;  // prueba: 1 pausa, 2 guardar, 3 controles (F1), sobre la partida
    int start_tab = -1;
    int start_talk = 0;      // prueba: 1 hablar con el druida, 2 con el orfebre
    int start_equip_tab = -1; // prueba: pestaña del equipo (1 joyas, 2 tatuajes)   // prueba: menu Tab abierto en esa pestaña
    bool start_loot = false; // prueba: bolsas de botin delante
    bool start_inv = false, start_equip = false; // pruebas: guardar al final en ese hueco / cargar al empezar
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
        else if (!strcmp(argv[i], "--apuntar")) start_aim = true;
        else if (!strcmp(argv[i], "--lago")) start_lake = true;
        else if (!strcmp(argv[i], "--incendio")) start_fire = true;
        else if (!strcmp(argv[i], "--menu")) start_menu = true;
        else if (!strcmp(argv[i], "--inventario")) start_inv = true;
        else if (!strcmp(argv[i], "--pestana") && i + 1 < argc) start_tab = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--botin")) start_loot = true;
        else if (!strcmp(argv[i], "--hablar") && i + 1 < argc) {
            i++;
            start_talk = !strcmp(argv[i], "orfebre") ? 2 : !strcmp(argv[i], "guardian") ? 3 : 1;
        } else if (!strcmp(argv[i], "--fundar")) start_talk = 4;
        else if (!strcmp(argv[i], "--ordenes")) start_talk = 5;
        else if (!strcmp(argv[i], "--despachar")) start_talk = 6;
        else if (!strcmp(argv[i], "--joyas")) start_equip = true, start_equip_tab = 1;
        else if (!strcmp(argv[i], "--tatuajes")) start_equip = true, start_equip_tab = 2;
        else if (!strcmp(argv[i], "--pausa")) start_pause = 1;
        else if (!strcmp(argv[i], "--guardar")) start_pause = 2;
        else if (!strcmp(argv[i], "--controles")) start_pause = 3;
        else if (!strcmp(argv[i], "--equipo")) start_equip = true;
        else if (!strcmp(argv[i], "--autoguardar") && i + 1 < argc) start_save = atoi(argv[++i]) - 1;
        else if (!strcmp(argv[i], "--cargar") && i + 1 < argc) start_load = atoi(argv[++i]) - 1;
        else if (!strcmp(argv[i], "--instructivo")) start_menu = true, start_menu_screen = MENU_HELP;
        else if (!strcmp(argv[i], "--huecos")) start_menu = true, start_menu_screen = MENU_LOAD;
        else if (!strcmp(argv[i], "--pos") && i + 2 < argc) {
            start_pos = true;
            start_x = (float)atof(argv[++i]);
            start_z = (float)atof(argv[++i]);
        }
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
#if defined(__ANDROID__)
    InitWindow(0, 0, GAME_TITLE " - " GAME_SUBTITLE); // pantalla completa del dispositivo
#else
    InitWindow(VIRTUAL_W * 2, VIRTUAL_H * 2, GAME_TITLE " - " GAME_SUBTITLE);
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
    Troop troop;
    Rng rng;
    float world_time = 0.0f;
    int day = 1;
    Sky sky;
    sky_init(&sky, WORLD_SEED);
    WeatherFx weather;
    weather_init(&weather, WORLD_SEED);
    int last_champion = -1;  // id del ultimo gran guerrero encontrado
    bool show_card = false;
    memmap_init(&g_memory);
    Minimap minimap;
    minimap_init(&minimap, MINIMAP_RADIUS, 2.0f);
    char log[128] = "";
    char *inv_text = LoadFileText(platform_asset_path("assets/inventario.tsv"));
    if (inv_text) {
        inventory_parse(&g_inventory, inv_text);
        UnloadFileText(inv_text);
    }
    props_init(&g_props, &g_inventory);
    CameraRig rig = { .yaw = PI, .pitch = 0.38f, .dist = 11.0f };
    settings_init(); // idioma (y sus traducciones), antes del primer texto de la partida
    GameState gs = { &world_time, &day, &last_champion, &rig.yaw, &rng, &player, &overlord, &troop, &g_actions,
                     &g_props, &g_combat, &g_hazards, &g_dz, &g_memory, &g_inventory };
    game_new(&gs, &terrain, start_day, start_minute, log, sizeof(log));
    if (gallery_mode) world_time = 4.0f * 60.0f;
    if (start_pos) {
        player.pos = (Vector3){ start_x, terrain_height(&terrain, start_x, start_z), start_z };
    }
    if (start_lake && !gallery_mode) { // prueba: en la orilla del lago mas cercano, mirando al agua
        for (float r = 20.0f; r < 400.0f; r += 6.0f) {
            bool found = false;
            for (int k = 0; k < 24 && !found; k++) {
                float a = (float)k * PI / 12.0f, x = cosf(a) * r, z = sinf(a) * r;
                if (terrain.look.water_level < terrain_height(&terrain, x, z) + 2.0f) continue;
                for (float back = 0.0f; back < r; back += 1.0f) { // hacia el campamento, hasta la orilla
                    float bx = cosf(a) * (r - back), bz = sinf(a) * (r - back);
                    if (terrain.look.water_level < terrain_height(&terrain, bx, bz) - 0.1f) {
                        player.pos = (Vector3){ bx, terrain_height(&terrain, bx, bz), bz };
                        player.yaw = atan2f(cosf(a), sinf(a));
                        found = true;
                        break;
                    }
                }
            }
            if (found) break;
        }
    }

    Gallery gallery = { 0 };
    if (gallery_mode && gallery_init(&gallery, &terrain, (Vector3){ 100.0f, 0.0f, 60.0f })) {
        player.pos = gallery.start;
        player.pos.y = terrain_height(&terrain, player.pos.x, player.pos.z);
        player.yaw = PI; // mirando a -Z, hacia las filas
        snprintf(log, sizeof(log), T("Galeria: %d objetos (%d con modelo)."), gallery.count, gallery.loaded);
    } else {
        gallery_mode = false;
    }

    Camera3D cam = { .up = { 0, 1, 0 }, .fovy = 55.0f, .projection = CAMERA_PERSPECTIVE };
    if (start_lake) rig.yaw = player.yaw; // mirando al agua

    // Menu de entrada: al arrancar normalmente; las pruebas (--screenshot, --galeria...) van
    // directo a la partida salvo que pidan el menu (--menu, --instructivo, --huecos).
    TitleMenu menu;
    menu_init(&menu);
    icons_load();
    bool in_game = gallery_mode || (shot_path && !start_menu);
    if (!in_game) menu_open(&menu, MENU_TITLE);
    if (start_menu && start_menu_screen != MENU_TITLE) menu_open(&menu, start_menu_screen);
    bool show_controls = false, quit = false;
    char last_log[128] = "";
    float log_age = 99.0f;
    Image pause_shot = { 0 }; // la partida al pausar (minifoto de la partida guardada)
    char err[120];

    int frame = 0;
    bool has_keyboard = platform_has_keyboard() && !simulate_no_keyboard;
    SetExitKey(KEY_NULL); // Esc abre el menu
    while (!WindowShouldClose() && !quit) {
        float dt = fminf(GetFrameTime(), 0.05f);
        { // el puntero (raton o dedo) en la pantalla virtual de 640x360
            float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
            float k = fminf(sw / VIRTUAL_W, sh / VIRTUAL_H);
            Vector2 m = GetMousePosition(), d = GetMouseDelta();
            Vector2 v = { (m.x - (sw - VIRTUAL_W * k) * 0.5f) / k, (m.y - (sh - VIRTUAL_H * k) * 0.5f) / k };
            ui_pointer_frame(v, d.x != 0.0f || d.y != 0.0f, IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
        }
        if (frame % 30 == 0) has_keyboard = platform_has_keyboard() && !simulate_no_keyboard; // conexion en caliente
        frame++;
        if (start_pause && frame == 10 && in_game) { // prueba: la pausa (o guardar) sobre la partida
            if (start_pause == 3) {
                show_controls = true;
            } else {
                pause_shot = LoadImageFromTexture(lowres.texture);
                ImageFlipVertical(&pause_shot);
                menu_open(&menu, MENU_PAUSE);
                if (start_pause == 2) menu_open(&menu, MENU_SAVE);
            }
            start_pause = 0;
        }
        // Esc en la partida: pausa (antes, una foto para la partida guardada).
        if (in_game && !gallery_mode && !menu_visible(&menu) && !ig_blocks_input(&g_actions) && IsKeyPressed(KEY_ESCAPE)) {
            if (ga_menu_open(&g_actions)) {
                g_actions.menu_open = false;
            } else {
                if (pause_shot.data) UnloadImage(pause_shot);
                pause_shot = LoadImageFromTexture(lowres.texture);
                ImageFlipVertical(&pause_shot);
                menu_open(&menu, MENU_PAUSE);
            }
        } else if (menu_visible(&menu) && has_keyboard) {
            switch (menu_update(&menu, dt)) {
            case MENU_NEW_GAME:
                game_new(&gs, &terrain, 1, 4.0f, log, sizeof(log));
                in_game = true;
                menu.screen = MENU_HIDDEN;
                break;
            case MENU_CONTINUE: menu.screen = MENU_HIDDEN; break;
            case MENU_LOAD_SLOT:
                if (save_read(menu.slot, &gs, err, sizeof(err))) {
                    in_game = true;
                    menu.screen = MENU_HIDDEN;
                    snprintf(log, sizeof(log), T("Partida cargada (hueco %d)."), menu.slot + 1);
                } else {
                    menu_message(&menu, err);
                }
                break;
            case MENU_SAVE_SLOT:
                if (save_write(menu.slot, &gs, pause_shot, err, sizeof(err))) {
                    menu_open(&menu, MENU_SAVE);
                    menu_message(&menu, TextFormat(T("Partida guardada en el hueco %d."), menu.cursor + 1));
                } else {
                    menu_message(&menu, err);
                }
                break;
            case MENU_TO_TITLE:
                in_game = false;
                menu_open(&menu, MENU_TITLE);
                break;
            case MENU_QUIT: quit = true; break;
            case MENU_LANG: settings_next_lang(); break;
            default: break;
            }
        }
        if (in_game && IsKeyPressed(KEY_F1)) show_controls = !show_controls;
        if ((start_inv || start_equip) && frame == 3) { // prueba: menus de inventario y equipo
            g_actions.inv_open = start_inv;
            g_actions.inv_cont[1] = 99; // a la derecha, el ultimo a mano (el acopio, en el campamento)
            g_actions.equip_open = start_equip;
            start_inv = start_equip = false;
        }
        if (start_equip_tab >= 0 && frame == 4) {
            g_actions.equip_tab = start_equip_tab;
            start_equip_tab = -1;
        }
        if (start_talk == 4 && frame == 6) { // prueba: con dos de escolta, lejos, y una tienda: fundar un campamento
            for (int k = 1; k <= 2 && k < troop.count; k++) g_actions.npcs[k].escort = true;
            player.pos = (Vector3){ 120.0f, terrain_height(&terrain, 120.0f, 40.0f), 40.0f };
            cg_basic_structure(&g_actions, 122.0f, 40.0f);
            start_talk = 0;
        }
        if ((start_talk == 5 || start_talk == 6) && frame == 6) { // prueba: ordenes a la escolta (o elegir destino)
            for (int k = 1; k <= 3 && k < troop.count; k++) g_actions.npcs[k].escort = true;
            g_actions.travel_npick = 0;
            for (int k = 1; k <= 2 && k < troop.count; k++) g_actions.travel_pick[g_actions.travel_npick++] = troop.members[k].id;
            memmap_toggle_marker(&g_memory, 140.0f, -60.0f, MARKER_INTEREST, 5.0f); // un sitio marcado que nadie conoce
            g_actions.talk_mode = start_talk == 5 ? 200 : 202;
            g_actions.talk_member = -1;
            g_actions.dlg.cursor = 0;
            g_actions.dlg.open = true;
            start_talk = 0;
        }
        if (start_talk == 3 && frame == 6) { // prueba: hablando con el guardian del campamento inicial
            for (int k = 0; k < troop.count && k < TROOP_MAX; k++)
                if (troop.members[k].id == g_actions.camps[0].guardian) {
                    Vector3 at = g_actions.npcs[k].pos;
                    player.pos = (Vector3){ at.x + 1.5f, terrain_height(&terrain, at.x + 1.5f, at.z), at.z };
                }
            cg_try_talk(&g_actions, &troop, &player);
            start_talk = 0;
        }
        if (start_talk && frame == 6) { // prueba: al lado del druida (u orfebre), hablando
            Role want = start_talk == 2 ? ROLE_GOLDSMITH : ROLE_DRUID;
            for (int k = 0; k < troop.count && k < TROOP_MAX; k++)
                if (troop.members[k].role == want) {
                    Vector3 at = g_actions.npcs[k].pos;
                    player.pos = (Vector3){ at.x + 1.5f, terrain_height(&terrain, at.x + 1.5f, at.z), at.z };
                }
            tg_try_talk(&g_actions, &troop, &player, log, sizeof(log));
            start_talk = 0;
        }
        if (start_tab >= 0 && frame == 3) { // prueba: menu Tab (0 acciones, 1 obras, 2 fabricar, 3 reparar)
            g_actions.menu_open = true;
            g_actions.menu_tab = start_tab;
            start_tab = -1;
        }
        if (start_loot && frame == 3) { // prueba: un par de bolsas de botin delante del jugador
            LootItem a[] = { { "arma.corta.sable", 1, 0.6f }, { "proyectil.flecha.comun", 6, 1.0f }, { "armadura.casco.laminar_cuero", 1, 0.4f } };
            LootItem b[] = { { "utileria.consumible.carne_seca", 2, 1.0f }, { "accesorio.amuleto.caballo", 1, 1.0f } };
            float fx = sinf(player.yaw), fz = cosf(player.yaw);
            ig_drop_loot(&g_actions, (Vector3){ player.pos.x + fx * 3.0f, player.pos.y, player.pos.z + fz * 3.0f }, a, 3);
            ig_drop_loot(&g_actions, (Vector3){ player.pos.x + fx * 4.5f + fz * 1.5f, player.pos.y, player.pos.z + fz * 4.5f - fx * 1.5f }, b, 2);
            start_loot = false;
        }
        if (start_load >= 0 && frame == 2) { // prueba: cargar un hueco al empezar
            if (save_read(start_load, &gs, err, sizeof(err))) snprintf(log, sizeof(log), T("Partida cargada (hueco %d)."), start_load + 1);
            else snprintf(log, sizeof(log), "%s", err);
            start_load = -1;
        }
        if (strcmp(log, last_log) != 0) snprintf(last_log, sizeof(last_log), "%s", log), log_age = 0.0f;
        log_age += dt;
        // Sin teclado el juego queda en pausa (Android: tablets sin teclado conectado); con el menu, tambien.
        if (has_keyboard && in_game && !menu_visible(&menu)) {
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
            g_actions.player_armor = &g_combat.armor; // para reparar lo que llevas puesto
            if (!gallery_mode) ga_update(&g_actions, &g_props, &terrain, &player, &troop, dt, log, sizeof(log));
            if (!gallery_mode) ga_after_player(&g_actions, &g_props, &terrain, &player);
            if (start_trap && frame == 3 && !gallery_mode) { // prueba: la tribu ya esta ubicada
                hz_force(&g_hazards, start_trap, &player, &g_actions, &troop, &terrain);
                start_trap = NULL;
            }
            if (start_enemies && frame == 3 && !gallery_mode) {
                cb_spawn_group(&g_combat, &g_actions, start_enemies, &player, &terrain, 7.0f, log, sizeof(log));
                start_enemies = NULL;
            }
            if (start_fire && frame == 3 && !gallery_mode) { // prueba: fuego en el pasto, 12 m delante
                float fx = player.pos.x + sinf(player.yaw) * 12.0f, fz = player.pos.z + cosf(player.yaw) * 12.0f;
                fire_ignite_hot(&g_dz.fire, fx, fz, FUEL_GRASS, -1, 1.0f);
                start_fire = false;
            }
            if (start_wounds && frame == 3 && !gallery_mode) { // prueba: heridas a la vista
                health_hit(&g_combat.player, &rng, 28.0f, WOUND_CUT, PART_THIGH_L);
                health_hit(&g_combat.player, &rng, 14.0f, WOUND_BRUISE, PART_FOREARM_R);
                for (int i = 0; i < 2 && i < troop.count; i++)
                    health_hit(&troop.members[i].health, &rng, i ? 110.0f : 30.0f, WOUND_BITE, PART_ABDOMEN);
                g_actions.equip_open = true;
                start_wounds = false;
            }
            if (!gallery_mode)
                hz_update(&g_hazards, &g_climate, &terrain, &player, &g_actions, &g_props, &troop, camp.fire, world_time,
                          dt, log, sizeof(log));
            if (!gallery_mode)
                ig_update(&g_actions, &g_combat, &g_props, &player, !menu && !hz_blocks_input(&g_hazards) && !g_actions.dlg.open, log,
                          sizeof(log));
            if (!gallery_mode) cg_update(&g_actions, &troop, &g_props, &player, day, dt, log, sizeof(log));
            if (!gallery_mode)
                trv_update(&g_actions, &troop, &g_memory, &player, &terrain, clock_is_night(world_time),
                           !menu && !hz_blocks_input(&g_hazards) && !ig_blocks_input(&g_actions), dt, log, sizeof(log));
            if (!g_actions.camps[0].used && camp.yurt_count) { // el campamento inicial se disolvio: sus yurtas arden
                for (int i = 0; i < camp.yurt_count; i++) {
                    bool ruin = false;
                    for (int k = 0; k < g_props.count; k++)
                        if (!strcmp(g_props.items[k].item->id, "estructura.ruina.yurta_quemada") &&
                            Vector3Distance(g_props.items[k].pos, camp.yurts[i]) < 1.0f)
                            ruin = true;
                    if (!ruin) props_add(&g_props, "estructura.ruina.yurta_quemada", camp.yurts[i], camp.yurt_rot[i] * DEG2RAD);
                    if (g_actions.ignite_n < 8) g_actions.ignite_at[g_actions.ignite_n++] = camp.yurts[i];
                }
                camp.yurt_count = 0;
            }
            if (!gallery_mode)
                tg_update(&g_actions, &g_combat, &troop, &g_props, &player,
                          !menu && !hz_blocks_input(&g_hazards) && !ig_blocks_input(&g_actions), dt, log, sizeof(log));
            if (!gallery_mode)
                cb_update(&g_combat, &player, &g_actions, &troop, &g_props, &terrain, camp.fire,
                          !menu && !hz_blocks_input(&g_hazards) && !ig_blocks_input(&g_actions), clock_is_night(world_time),
                          g_climate.temp_mean < 0.0f,
                          rig.yaw, rig.pitch, dt, log, sizeof(log));
            g_actions.grass = g_climate.snow_cover < 0.3f; // con nieve no hay pasto
            if (!gallery_mode)
                fg_update(&g_actions, &g_combat, &player, &troop, &terrain, &g_memory, world_time, g_climate.temperature,
                          !menu && !hz_blocks_input(&g_hazards) && !ig_blocks_input(&g_actions), dt, log, sizeof(log));
            if (!gallery_mode)
                dz_update(&g_dz, &g_climate, &camp, &g_actions, &g_props, &troop, &g_combat, &player, &terrain, world_time,
                          dt, log, sizeof(log));
            if (start_aim && !gallery_mode) { // prueba: arco tenso, para ver la curva de la mira
                snprintf(g_actions.hands.right.id, sizeof(g_actions.hands.right.id), "arma.distancia.arco_compuesto");
                g_actions.hands.right.kind = INV_HANDS_TWO;
                g_actions.hands.left.id[0] = '\0';
                g_actions.hands.sheathed = false;
                g_combat.aiming = true;
                g_combat.draw = 0.8f;
                g_combat.aim_pitch = 0.12f;
                player.yaw = rig.yaw;
            }
            if (last_champion != prev_champion) show_card = true; // ficha al conocerlo
            advance_days(&troop, &rng, &day, world_time, &terrain, log, sizeof(log));
            if (IsKeyPressed(KEY_G) && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))) show_card = !show_card && last_champion >= 0;
            memmap_visit(&g_memory, player.pos.x, player.pos.z, dt, world_time);
            { // la vista (tatuajes del grifo, lapislazuli): el mapa se descubre mas lejos
                static float reveal_t = 0.0f;
                float per = ig_stat(&g_actions, STAT_PERCEPTION);
                if (per > 0.0f && (reveal_t += dt) > 1.0f) {
                    reveal_t = 0.0f;
                    memmap_reveal(&g_memory, player.pos.x, player.pos.z, 30.0f * per, 30.0f, world_time);
                }
            }
            if (IsKeyPressed(KEY_M)) {
                MarkerKind kind = IsKeyDown(KEY_LEFT_SHIFT) ? MARKER_DANGER : MARKER_INTEREST;
                bool placed = memmap_toggle_marker(&g_memory, player.pos.x, player.pos.z, kind, 6.0f);
                snprintf(log, sizeof(log), "%s", placed ? T("Marcaste este lugar en el mapa.") : T("Quitaste la marca."));
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
        if (in_game) {
        BeginMode3D(cam);
        terrain_draw(&terrain);
        camp_draw(&camp, (float)GetTime(), climate.snow_cover, !g_actions.fires_out && g_actions.camps[0].used, g_dz.tree_burn);
        if (gallery_mode) gallery_draw(&gallery);
        bool player_model = !gallery_mode && ga_draw_player(&g_actions, &g_props, &player, (float)GetTime());
        if (gallery_mode) player_draw(&player);
        else cb_draw_world(&g_combat, &g_props, &g_actions, &terrain, &player, player_model, (float)GetTime());
        if (!gallery_mode) ga_draw_world(&g_actions, &g_props, &terrain, &troop, &player, (float)GetTime());
        if (!gallery_mode) fg_draw_world(&g_actions, &g_props, &terrain, (float)GetTime());
        if (!gallery_mode) ig_draw_world(&g_actions, &terrain, (float)GetTime());
        if (!gallery_mode) trv_draw_world(&g_actions, &troop, (float)GetTime());
        if (!gallery_mode) dz_draw_world(&g_dz, &terrain, (float)GetTime());
        if (!gallery_mode) terrain_draw_water(&terrain, (float)GetTime()); // translucida: despues de lo opaco
        if (!gallery_mode) hz_draw_world(&g_hazards, &terrain, &g_actions, &troop, (float)GetTime());
        EndMode3D();
        if (!gallery_mode) { // la galeria se ve siempre de dia
            // Noche: se oscurece todo y se suman las estrellas, las llamas y el brillo de los fuegos.
            sky_apply_tint(world_time, climate.clouds, VIRTUAL_W, VIRTUAL_H);
            BeginMode3D(cam);
            sky_draw_stars(&sky, cam, world_time, climate.clouds);
            if (!g_actions.fires_out) camp_draw_flame(&camp, (float)GetTime());
            weather_draw(&weather, &climate, cam, (float)GetTime(), clock_light(world_time));
            EndMode3D();
            Vector3 light_pos[SKY_MAX_LIGHTS];
            float light_radius[SKY_MAX_LIGHTS];
            light_pos[0] = (Vector3){ camp.fire.x, camp.fire.y + 0.4f, camp.fire.z };
            light_radius[0] = g_actions.fires_out ? 0.0f : 9.0f; // la lluvia apaga la fogata
            int lights = 1 + ga_lights(&g_actions, &g_props, &player, light_pos + 1, light_radius + 1, SKY_MAX_LIGHTS - 1);
            sky_draw_lights(cam, light_pos, light_radius, lights, world_time, (float)GetTime(), VIRTUAL_W, VIRTUAL_H);
            weather_draw_screen(&weather, &climate, VIRTUAL_W, VIRTUAL_H);
            cb_draw_overlay(&g_combat, &g_actions, &troop, cam, VIRTUAL_W, VIRTUAL_H);
            fg_draw_overlay(&g_actions, &terrain, cam, VIRTUAL_W, VIRTUAL_H);
        }
        if (gallery_mode) {
            gallery_draw_labels(&gallery, cam, player.pos, VIRTUAL_W, VIRTUAL_H);
        } else if (!menu_visible(&menu)) {
            draw_hud(&troop, world_time, ga_hands_text(&g_actions), log, log_age,
                     g_actions.menu_open || g_actions.inv_open || g_actions.equip_open);
            minimap_draw(&minimap, &g_memory, (Vector2){ VIRTUAL_W - MINIMAP_RADIUS - 10, MINIMAP_RADIUS + 13 },
                         player.pos, rig.yaw, player.yaw, world_time);
            draw_clock_bar(VIRTUAL_W - MINIMAP_RADIUS - 10, 2 * MINIMAP_RADIUS + 19, 2 * MINIMAP_RADIUS - 8, world_time);
            draw_weather_text(VIRTUAL_W - 14, 2 * MINIMAP_RADIUS + 27, &climate);
            ga_draw_hud(&g_actions, &g_props, &troop, &player, VIRTUAL_W, VIRTUAL_H);
            hz_draw_hud(&g_hazards, &troop, VIRTUAL_W - 14, 2 * MINIMAP_RADIUS + 40, VIRTUAL_W, VIRTUAL_H);
            cb_draw_hud(&g_combat, &g_actions, &troop, VIRTUAL_W - 14, 2 * MINIMAP_RADIUS + 63, VIRTUAL_W, VIRTUAL_H);
            fg_draw_hud(&g_actions, VIRTUAL_W, VIRTUAL_H);
            ig_draw(&g_actions, &g_combat, &g_props, &player, VIRTUAL_W, VIRTUAL_H);
            if (!ig_blocks_input(&g_actions) && !ga_menu_open(&g_actions)) tg_draw_hud(&g_actions, VIRTUAL_W, VIRTUAL_H);
            tg_draw(&g_actions, VIRTUAL_W, VIRTUAL_H);
            if (show_card) draw_champion_card(&troop, last_champion);
            if (show_controls) menu_draw_controls(VIRTUAL_W / 2 - 200, 90, 400, 140);
        }
        }
        menu_draw(&menu, in_game,
                  TextFormat("v%s (build %d) · %d fps%s", ESTEPA_VERSION, ESTEPA_BUILD_CODE, GetFPS(),
                             in_game ? TextFormat(" · %s: %+d", T(overlord.name), (int)overlord.relation) : ""),
                  VIRTUAL_W, VIRTUAL_H);
        ui_legend_draw(VIRTUAL_W, VIRTUAL_H); // la leyenda de lo que esta bajo el puntero
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

        if (start_save >= 0 && frame >= shot_frames) { // prueba: guardar con la foto del ultimo cuadro
            Image shot = LoadImageFromTexture(lowres.texture);
            ImageFlipVertical(&shot);
            if (!save_write(start_save, &gs, shot, err, sizeof(err))) TraceLog(LOG_WARNING, "%s", err);
            UnloadImage(shot);
            start_save = -1;
        }
        if (shot_path && frame >= shot_frames) {
            Image img = LoadImageFromTexture(lowres.texture);
            ImageFlipVertical(&img);
            ExportImage(img, shot_path);
            UnloadImage(img);
            break;
        }
    }

    if (pause_shot.data) UnloadImage(pause_shot);
    icons_unload();
    settings_free();
    menu_unload(&menu);
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
