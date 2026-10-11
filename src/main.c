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
#include <time.h>
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
#include "game/apparel_game.h"
#include "game/water_game.h"
#include "game/world_game.h"
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
#include "world/hearth.h"
#include "game/death_game.h"
#include "game/hud_game.h"
#include "world/gallery.h"
#include "world/sky.h"
#include "world/clouds.h"
#include "world/voxstruct.h"
#include "game/input.h"
#include "rlgl.h"
#include "world/weather.h"
#include "world/terrain.h"
#include "world/worldview.h"
#include "sim/lang.h"

#ifndef ESTEPA_VERSION
#define ESTEPA_VERSION "dev"
#endif
#ifndef ESTEPA_BUILD_CODE
#define ESTEPA_BUILD_CODE 0
#endif

#define VIRTUAL_W 640
#define VIRTUAL_H 360
#define WORLD_SEED 1206u // ano de la fundacion del Imperio mongol: el mundo de las pruebas (--semilla cambia)
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

static void camera_update(CameraRig *rig, Camera3D *cam, const Player *p, const Terrain *t, float dt) {
    // Orbita: Q/E o arrastre con boton derecho (sin arrastrar, el derecho cubre); rueda para el zoom.
    if (IsKeyDown(KEY_Q)) rig->yaw += 1.8f * dt;
    if (IsKeyDown(KEY_E)) rig->yaw -= 1.8f * dt;
    float dx, dy;
    if (input_right_drag(&dx, &dy)) {
        rig->yaw -= dx * 0.006f;
        rig->pitch = Clamp(rig->pitch + dy * 0.004f, 0.05f, 1.25f);
    }
    rig->dist = Clamp(rig->dist - GetMouseWheelMove() * 0.8f, 3.0f, 16.0f);

    Vector3 target = { p->pos.x, p->pos.y + player_eye_height(p), p->pos.z };
    Vector3 back = { -sinf(rig->yaw) * cosf(rig->pitch), sinf(rig->pitch), -cosf(rig->yaw) * cosf(rig->pitch) };
    cam->position = Vector3Add(target, Vector3Scale(back, rig->dist));
    // Nunca bajo el suelo (las laderas, los muros): la camara sube sobre el terreno que tenga
    // detras y en el brazo que la une al jugador.
    float floor_y = -1e9f;
    for (int k = 1; k <= 4; k++) {
        Vector3 q = Vector3Add(target, Vector3Scale(back, rig->dist * k / 4.0f));
        floor_y = fmaxf(floor_y, terrain_height(t, q.x, q.z) + 1.2f * k / 4.0f);
    }
    if (cam->position.y < floor_y) cam->position.y = floor_y;
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
    if (input_ctrl()) return; // Ctrl+1..3: las habilidades activas
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
    } else if (IsKeyPressed(KEY_N) && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))) {
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
        ga_new_day(&g_actions, &g_props, terrain, t, &g_memory, world_time, *day, climate_at(world_time, terrain->seed).temp_mean, log, log_len);
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
        .snowline = terrain->plain + c->snowline, .flood = c->water_level, .ice = c->ice,
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
static void draw_weather_text(int right, int y, const Climate *c, Region rg) {
    const char *txt = TextFormat("%s · %s · %d °C", region_name(rg), weather_name(c->weather), (int)lroundf(c->temperature));
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
    const char *hint = T("F1 o Ctrl+H: controles · Esc o Ctrl+P: menú");
    ui_text(hint, VIRTUAL_W - 8 - MeasureText(hint, 10), VIRTUAL_H - 16, 10, Fade(UI_BONE_DIM, 0.8f));
}

// Cambia de mundo (otra semilla): el terreno y el campamento se rehacen.
static void world_reset(Terrain *terrain, Camp *camp, unsigned seed) {
    if (terrain->seed == seed) return;
    terrain_unload(terrain);
    TerrainLook look = terrain->look;
    terrain_init(terrain, seed);
    terrain_set_look(terrain, &look);
    camp_unload(camp);
    camp_init(camp, terrain, platform_asset_path("assets/models/estructura/vivienda/yurta_comun.glb"));
}

// Una semilla nueva para cada partida: el mismo mapa de regiones, otros rios, lagos y montañas.
static unsigned fresh_seed(void) {
    unsigned s = (unsigned)time(NULL) * 2654435761u ^ (unsigned)(GetTime() * 1000003.0);
    return s ? s : WORLD_SEED;
}

// Una partida nueva: la tribu inicial en el campamento, a media mañana del dia pedido.
static void game_new(GameState *g, Terrain *terrain, Camp *camp, unsigned seed, int start_day, float start_minute, char *log, size_t len) {
    world_reset(terrain, camp, seed);
    dg_reset(); // sin restos y con el campamento como lugar de descanso
    hud_reset(); // la barra rapida de serie
    player_init(g->player, terrain);
    kingdom_init_iron_khanate(g->overlord);
    troop_init(g->troop, g->overlord);
    seed_troop(g->troop);
    rng_seed(g->rng, seed);
    *g->world_time = (float)(start_day > 1 ? start_day - 1 : 0) * GAME_SECONDS_PER_DAY +
                     fmaxf(0.0f, fminf(start_minute * 60.0f, GAME_SECONDS_PER_DAY - 1.0f));
    *g->day = clock_day(*g->world_time);
    *g->last_champion = -1;
    *g->cam_yaw = PI;
    memmap_free(g->mem);
    memmap_init(g->mem);
    g->props->count = 0; // los modelos cargados se conservan
    // El clima primero: los animales iniciales miran donde hay agua.
    g_climate = climate_at(*g->world_time, seed);
    apply_climate_look(terrain, &g_climate);
    ga_init(g->ga, g->inv, g->props, terrain, seed);
    hz_init(g->hz, seed); // la semilla del mundo viaja en la partida guardada (Hazards.seed)
    cb_init(g->cb, seed);
    dz_init(g->dz, seed);
    g->hz->body = &g->cb->player; // caidas y congelacion hieren
    snprintf(log, len, T("Tu tropa acampa en la estepa (mundo %u)."), seed);
}

// Prueba: lleva al jugador a un sitio del mundo, mirando hacia lo que interesa. Lugares:
// estepa, bosque, altiplano, fiordos, desierto (en su region); canal, muro, mar, hielo (el
// borde); meandro, trenzado (los rios); capital, aldea, guarida.
static void go_to(const Terrain *t, Player *p, const char *what) {
    const World *w = t->world;
    static const char *REG[REGION_COUNT] = { "estepa", "bosque", "altiplano", "fiordos", "desierto" };
    float x = 0, z = 0, lx = 0, lz = 0; // donde y hacia donde mira
    bool ok = false;
    for (int r = 1; r < REGION_COUNT; r++)
        if (!strcmp(what, REG[r])) world_region_point(w, (Region)r, 0.62f * WORLD_RADIUS, &x, &z), lx = x * 1.1f, lz = z * 1.1f, ok = true;
    if (!strcmp(what, "estepa")) x = 260.0f, z = -180.0f, lx = 600.0f, lz = -400.0f, ok = true;
    if (!strcmp(what, "canal")) {
        float x0, z0;
        world_edge_point(w, REGION_FOREST, 0.0f, &x0, &z0);
        float a = atan2f(z0, x0), r = world_edge_radius(w, a) - world_canal_offset(w, a) - 62.0f;
        x = cosf(a) * r, z = sinf(a) * r, lx = x * 1.2f, lz = z * 1.2f, ok = true;
    }
    if (!strcmp(what, "muro")) world_edge_point(w, REGION_DESERT, 210.0f, &x, &z), lx = x * 1.2f, lz = z * 1.2f, ok = true;
    if (!strcmp(what, "mar")) { // en la orilla, mirando al mar abierto
        for (float in = 200.0f; in < 1200.0f; in += 20.0f) {
            world_edge_point(w, REGION_FJORD, in, &x, &z);
            if (world_height(w, x, z) > SEA_LEVEL + 1.5f && world_water(w, x, z, 0.0f, NULL) < -1e8f) break;
        }
        lx = x * 1.2f, lz = z * 1.2f, ok = true;
    }
    if (!strcmp(what, "fogata")) x = 1.8f, z = 0.0f, lx = 1.8f, lz = 10.0f, ok = true; // la del campamento, de cerca
    if (!strcmp(what, "hoguera")) x = 6.5f, z = -2.0f, lx = 6.5f, lz = 10.0f, ok = true; // una hoguera junto a la fogata
    if (!strcmp(what, "hielo")) world_edge_point(w, REGION_HIGHLAND, 230.0f, &x, &z), lx = x * 1.2f, lz = z * 1.2f, ok = true;
    static const struct { const char *name; SiteKind kind; } SITE_PLACES[] = {
        { "kurgan", SITE_KURGAN },       { "balbales", SITE_BALBALS },        { "piedra", SITE_DEER_STONE }, { "fortaleza", SITE_RUINED_FORT },
        { "ciudad", SITE_BURIED_CITY },  { "petroglifos", SITE_PETROGLYPHS }, { "caravasar", SITE_CARAVANSERAI },
        { "torre", SITE_WATCHTOWER },    { "ovoo", SITE_OVOO },               { "pozo", SITE_WELL },         { "embarcadero", SITE_HARBOR },
    };
    for (size_t k = 0; k < sizeof(SITE_PLACES) / sizeof(SITE_PLACES[0]) && !ok; k++) // una estructura: a unos pasos, mirandola
        for (int i = 0; i < w->site_count && !ok; i++)
            if (!strcmp(what, SITE_PLACES[k].name) && w->sites[i].kind == SITE_PLACES[k].kind)
                x = w->sites[i].x + 30.0f, z = w->sites[i].z + 12.0f, lx = w->sites[i].x, lz = w->sites[i].z, ok = true;
    for (int i = 0; i < w->tribe_count && !ok; i++) { // una tribu (tribu: amiga; rival; neutral), desde fuera de su alcance
        TribeAttitude want = !strcmp(what, "rival") ? TRIBE_RIVAL : !strcmp(what, "neutral") ? TRIBE_NEUTRAL : TRIBE_FRIENDLY;
        if ((!strcmp(what, "tribu") || !strcmp(what, "rival") || !strcmp(what, "neutral")) && w->tribes[i].attitude == want)
            x = w->tribes[i].x + 30.0f, z = w->tribes[i].z + 12.0f, lx = w->tribes[i].x, lz = w->tribes[i].z, ok = true;
    }
    if (!strcmp(what, "cumbre")) { // la cumbre mas alta (suele tocar las nubes)
        float best = -1e9f;
        for (int i = 0; i < w->peak_count; i++) {
            float h = world_height(w, w->peaks[i].x, w->peaks[i].z);
            if (h > best) best = h, x = w->peaks[i].x + 6.0f, z = w->peaks[i].z, lx = 0.0f, lz = 0.0f, ok = true;
        }
    }
    for (int i = 0; i < w->river_count && !ok; i++) { // rapidos: en la orilla de un arroyo, mirando aguas abajo
        const River *rv = &w->rivers[i];
        if (strcmp(what, "rapidos") || rv->kind != RIVER_CREEK || rv->n < 12) continue;
        int k = rv->n / 3;
        float dx = rv->x[k + 4] - rv->x[k], dz = rv->z[k + 4] - rv->z[k], l = sqrtf(dx * dx + dz * dz) + 1e-3f;
        x = rv->x[k] - dz / l * (rv->width + 5.0f), z = rv->z[k] + dx / l * (rv->width + 5.0f);
        lx = rv->x[k + 6], lz = rv->z[k + 6], ok = true;
    }
    for (int i = 0; i < w->river_count && !ok; i++) { // un rio: a su orilla, mirandolo
        const River *rv = &w->rivers[i];
        bool want = (!strcmp(what, "meandro") && rv->kind == RIVER_MEANDER) || (!strcmp(what, "trenzado") && rv->kind == RIVER_BRAIDED) ||
                    (!strcmp(what, "arroyo") && rv->kind == RIVER_CREEK);
        if (!want) continue;
        int k = rv->n / 2;
        float dx = rv->x[k + 1] - rv->x[k], dz = rv->z[k + 1] - rv->z[k], l = sqrtf(dx * dx + dz * dz) + 1e-3f;
        float off = rv->width + 25.0f;
        x = rv->x[k] - dz / l * off, z = rv->z[k] + dx / l * off, lx = rv->x[k], lz = rv->z[k], ok = true;
    }
    for (int i = 0; i < w->settlement_count && !ok; i++) {
        const Settlement *s = &w->settlements[i];
        bool want = (!strcmp(what, "capital") && s->kind == SETTLE_CAPITAL && s->region == REGION_FOREST) ||
                    (!strcmp(what, "aldea") && s->kind == SETTLE_VILLAGE && s->region == REGION_DESERT);
        if (want) x = s->x + 58.0f, z = s->z + 20.0f, lx = s->x, lz = s->z, ok = true;
    }
    for (int i = 0; i < w->den_count && !ok; i++)
        if (!strcmp(what, "guarida") && w->dens[i].region == REGION_FOREST)
            x = w->dens[i].x + 16.0f, z = w->dens[i].z + 6.0f, lx = w->dens[i].x, lz = w->dens[i].z, ok = true;
    if (!ok) return;
    // En seco: si cayo en el agua, el punto seco mas cercano (en espiral).
    for (float r = 0.0f; r < 400.0f && terrain_water(t, x, z) > terrain_height(t, x, z) - 0.2f; r += 6.0f) {
        bool found = false;
        for (int k = 0; k < 12 && !found; k++) {
            float a = (float)k / 12.0f * 2.0f * PI, nx = x + cosf(a) * r, nz = z + sinf(a) * r;
            if (terrain_water(t, nx, nz) <= terrain_height(t, nx, nz) - 0.2f) lx += nx - x, lz += nz - z, x = nx, z = nz, found = true;
        }
    }
    p->pos = (Vector3){ x, terrain_height(t, x, z), z };
    float water = terrain_water(t, x, z);
    if (water > p->pos.y) p->pos.y = water;
    p->yaw = atan2f(lx - x, lz - z);
}

static bool g_force_touch; // --tactil: los botones de Android tambien en escritorio (pruebas)

static void draw_keyboard_notice(void) {
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, (Color){ 8, 6, 5, 200 });
    const int w = 480, h = 84;
    ui_panel((Rectangle){ (VIRTUAL_W - w) / 2, (VIRTUAL_H - h) / 2, w, h }, UI_METAL_GOLD);
    ui_text_centered(T("Conecta un teclado para jugar"), VIRTUAL_W / 2, VIRTUAL_H / 2 - 18, 20, UI_GOLD_LIGHT);
    ui_text_centered(T("Scavengers Thrive se juega con teclado físico (ratón o mando opcionales)."), VIRTUAL_W / 2,
                     VIRTUAL_H / 2 + 8, 10, UI_BONE);
    ui_text_centered(T("Si ya tienes uno, pulsa una tecla o toca la pantalla para jugar igual."), VIRTUAL_W / 2,
                     VIRTUAL_H / 2 + 22, 10, UI_BONE_DIM);
}

// El puntero en la pantalla virtual (raton o dedo).
static Vector2 virtual_pointer(void) {
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float k = fminf(sw / VIRTUAL_W, sh / VIRTUAL_H);
    Vector2 m = GetMousePosition();
    return (Vector2){ (m.x - (sw - VIRTUAL_W * k) * 0.5f) / k, (m.y - (sh - VIRTUAL_H * k) * 0.5f) / k };
}

// Android: dos botones tactiles arriba, junto al minimapa (pausa y controles), por si el teclado
// de la tablet no trae Esc ni F1. Abajo a la derecha van los botones de combate. Un toque
// inyecta la accion (se lee en el proximo cuadro).
static void draw_touch_buttons(void) {
    const int s = 22, y = 6, x = VIRTUAL_W - 2 * MINIMAP_RADIUS - 16 - s;
    const Rectangle pause = { (float)x, (float)y, (float)s, (float)s }, help = { (float)(x - 6 - s), (float)y, (float)s, (float)s };
    Vector2 m = virtual_pointer();
    bool tap = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    for (int i = 0; i < 2; i++) {
        Rectangle r = i ? help : pause;
        hud_hover(r); // un toque aqui no es un golpe
        bool over = CheckCollisionPointRec(m, r);
        DrawRectangleRec(r, Fade((Color){ 20, 16, 12, 255 }, over ? 0.85f : 0.6f));
        DrawRectangleLinesEx(r, 1.0f, UI_GOLD);
        if (i) ui_text_centered("?", (int)(r.x + r.width / 2), (int)r.y + 6, 10, UI_GOLD_LIGHT);
        else DrawRectangle((int)r.x + 7, (int)r.y + 6, 3, 10, UI_GOLD_LIGHT), DrawRectangle((int)r.x + 12, (int)r.y + 6, 3, 10, UI_GOLD_LIGHT);
        if (tap && over) input_inject(i ? IN_HELP : IN_PAUSE);
    }
}

// Diagnostico (Ctrl+D): version de OpenGL, pantalla, teclado y assets que no cargaron.
static void draw_diagnostics(bool has_keyboard) {
    static const char *GL[] = { "?", "1.1", "2.1", "3.3", "4.3", "ES 2.0", "ES 3.0" };
    int v = rlGetVersion();
    const char *fails[8];
    int nf = platform_asset_failures(fails, 8);
    int h = 70 + 11 * (nf < 8 ? nf : 8);
    DrawRectangle(8, 60, 300, h, (Color){ 8, 6, 5, 220 });
    DrawRectangleLines(8, 60, 300, h, UI_GOLD);
    int y = 66;
    ui_text(TextFormat(T("Diagnóstico · OpenGL %s · %dx%d · %d fps"), (v >= 0 && v < 7) ? GL[v] : "?", GetScreenWidth(), GetScreenHeight(), GetFPS()), 14, y, 10, UI_GOLD_LIGHT);
    y += 12;
    ui_text(TextFormat(T("Teclado: %s · última tecla: %d"), has_keyboard ? T("sí") : T("no"), input_last_key()), 14, y, 10, UI_BONE);
    y += 12;
    ui_text(TextFormat(T("Assets cargados: %d · fallidos: %d"), platform_asset_loaded(), nf), 14, y, 10, UI_BONE);
    y += 12;
    for (int i = 0; i < nf && i < 8; i++, y += 11) ui_text(fails[i], 20, y, 10, (Color){ 230, 120, 100, 255 });
    ui_text(T("Ctrl+D: cerrar"), 14, y + 2, 10, UI_BONE_DIM);
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
    bool start_crouch = false, start_target = false;
    int start_back = 0; // prueba: 1 a la espalda del primer enemigo, 2 ademas con el de rehen
    MenuScreen start_menu_screen = MENU_TITLE;
    int start_help_page = 0; // prueba: --pagina N del instructivo
    unsigned start_seed = WORLD_SEED; // --semilla N: el mundo (sin ella, cada partida nueva del menu sortea uno)
    const char *map_path = NULL;      // --mapa-mundo archivo.png: el mapa general del mundo, y sale
    const char *start_goto = NULL;    // --ir lugar: a un sitio del mundo (ver go_to)
    bool start_diag = false;          // --diagnostico: el panel de Ctrl+D abierto
    int start_picker = -1;
    float start_heading = -1.0f;      // --rumbo grados: hacia donde mira la camara (0 sur, 90 este, 180 norte, 270 oeste)
    float start_pitch = -1.0f;        // --camara inclinacion: 0.05 casi de canto (se ve el cielo), 1.25 desde arriba
    int map_size = 1024;
    bool start_orbital = false;       // --orbital [grados] [inclinacion]: la vista orbital
    float start_orbit_deg = 30.0f, start_orbit_tilt = 0.55f;
    bool seed_given = false;
    int start_save = -1, start_load = -1;
    const char *selftest_dir = NULL; // --probar-partidas dir: las partidas de referencia (tests/partidas), y sale
    bool selftest_rewrite = false;   // --rehacer-resumenes: reescribe los resumenes esperados
    const char *convert_from = NULL, *convert_to = NULL; // --convertir-partida a b: la reescribe en el formato de hoy, y sale
    int start_pause = 0;  // prueba: 1 pausa, 2 guardar, 3 controles (F1), sobre la partida
    int start_tab = -1;
    int start_talk = 0;      // prueba: 1 hablar con el druida, 2 con el orfebre
    int start_equip_tab = -1; // prueba: pestaña del equipo (1 ropa, 2 joyas, 3 tatuajes)   // prueba: menu Tab abierto en esa pestaña
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
        else if (!strcmp(argv[i], "--agachado")) start_crouch = true;
        else if (!strcmp(argv[i], "--objetivo")) start_target = true;
        else if (!strcmp(argv[i], "--espalda")) start_back = 1;
        else if (!strcmp(argv[i], "--rehen")) start_back = 2;
        else if (!strcmp(argv[i], "--lago")) start_lake = true;
        else if (!strcmp(argv[i], "--mapa-mundo") && i + 1 < argc) map_path = argv[++i];
        else if (!strcmp(argv[i], "--ir") && i + 1 < argc) start_goto = argv[++i];
        else if (!strcmp(argv[i], "--diagnostico")) start_diag = true;
        else if (!strcmp(argv[i], "--tactil")) g_force_touch = true;
        else if (!strcmp(argv[i], "--selector") && i + 1 < argc) start_picker = atoi(argv[++i]) - 1; // prueba: el selector de una casilla
        else if (!strcmp(argv[i], "--rumbo") && i + 1 < argc) start_heading = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--camara") && i + 1 < argc) start_pitch = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--mapa-tam") && i + 1 < argc) map_size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--orbital")) {
            start_orbital = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') start_orbit_deg = (float)atof(argv[++i]);
            if (i + 1 < argc && argv[i + 1][0] != '-') start_orbit_tilt = (float)atof(argv[++i]);
        }
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
        else if (!strcmp(argv[i], "--ropa")) start_equip = true, start_equip_tab = 1;
        else if (!strcmp(argv[i], "--joyas")) start_equip = true, start_equip_tab = 2;
        else if (!strcmp(argv[i], "--tatuajes")) start_equip = true, start_equip_tab = 3;
        else if (!strcmp(argv[i], "--pausa")) start_pause = 1;
        else if (!strcmp(argv[i], "--guardar")) start_pause = 2;
        else if (!strcmp(argv[i], "--controles")) start_pause = 3;
        else if (!strcmp(argv[i], "--equipo")) start_equip = true;
        else if (!strcmp(argv[i], "--autoguardar") && i + 1 < argc) start_save = atoi(argv[++i]) - 1;
        else if (!strcmp(argv[i], "--cargar") && i + 1 < argc) start_load = atoi(argv[++i]) - 1;
        else if (!strcmp(argv[i], "--probar-partidas") && i + 1 < argc) selftest_dir = argv[++i];
        else if (!strcmp(argv[i], "--rehacer-resumenes")) selftest_rewrite = true;
        else if (!strcmp(argv[i], "--convertir-partida") && i + 2 < argc) convert_from = argv[++i], convert_to = argv[++i];
        else if (!strcmp(argv[i], "--instructivo")) start_menu = true, start_menu_screen = MENU_HELP;
        else if (!strcmp(argv[i], "--semilla") && i + 1 < argc) start_seed = (unsigned)strtoul(argv[++i], NULL, 10), seed_given = true;
        else if (!strcmp(argv[i], "--pagina") && i + 1 < argc) start_menu = true, start_menu_screen = MENU_HELP, start_help_page = atoi(argv[++i]) - 1;
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
    { // el logo en la barra de la ventana (en Android, el icono va en el APK)
        Image icon = LoadImage(platform_asset_path("assets/ui/icono.png"));
        if (icon.data) SetWindowIcon(icon), UnloadImage(icon);
    }
#endif
    SetTargetFPS(60);

    // Contexto nuevo: lo que los modulos guardaban de una corrida anterior (Android) ya no vale.
    terrain_forget_gpu();
    clouds_forget_gpu();
    voxs_forget_gpu();
    hearth_forget_gpu();
    RenderTexture2D lowres = LoadRenderTexture(VIRTUAL_W, VIRTUAL_H);
    SetTextureFilter(lowres.texture, TEXTURE_FILTER_POINT); // pixeles nitidos al escalar

    Terrain terrain;
    terrain_init(&terrain, start_seed);
    if (map_path) { // el mapa general del mundo (con la nieve de verano) y nada mas
        bool ok = worldmap_export(terrain.world, map_path, map_size > 64 ? map_size : 1024, terrain.plain + 40.0f);
        printf("%s %s\n", ok ? "Mapa escrito:" : "No se pudo escribir", map_path);
        CloseWindow();
        return ok ? 0 : 1;
    }
    Camp camp;
    camp_init(&camp, &terrain, platform_asset_path("assets/models/estructura/vivienda/yurta_comun.glb"));
    WorldView view = { 0 };
    bool orbital = start_orbital;
    float orbit_angle = start_orbit_deg * DEG2RAD, orbit_tilt = start_orbit_tilt;
    Player player;
    player_init(&player, &terrain);

    Kingdom overlord;
    Troop troop;
    Rng rng;
    float world_time = 0.0f;
    int day = 1;
    Sky sky;
    sky_init(&sky, WORLD_SEED);
    hearth_init();
    WeatherFx weather;
    weather_init(&weather, WORLD_SEED);
    int last_champion = -1;  // id del ultimo gran guerrero encontrado
    bool show_card = false;
    memmap_init(&g_memory);
    Minimap minimap;
    minimap_init(&minimap, MINIMAP_RADIUS, 3.0f); // 3 m por pixel: 150 m de radio
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
    game_new(&gs, &terrain, &camp, start_seed, start_day, start_minute, log, sizeof(log));
    if (selftest_dir) { // prueba: las partidas de referencia cargan igual que siempre
        int fails = save_selftest(selftest_dir, &gs, selftest_rewrite);
        CloseWindow();
        return fails ? 1 : 0;
    }
    if (convert_from) { // una partida (de cualquier version que este build lea) al formato de hoy
        SaveInfo info;
        char err[256] = "";
        bool ok = save_peek(convert_from, &info) && save_read_file(convert_from, &gs, err, sizeof(err)) &&
                  save_write_file(convert_to, &gs, info.saved_at, err, sizeof(err));
        printf("%s %s -> %s %s\n", ok ? "Convertida:" : "No se pudo convertir:", convert_from, convert_to, err);
        CloseWindow();
        return ok ? 0 : 1;
    }
    if (gallery_mode) world_time = 4.0f * 60.0f;
    if (start_pos) {
        player.pos = (Vector3){ start_x, terrain_height(&terrain, start_x, start_z), start_z };
    }
    if (start_lake && !gallery_mode) { // prueba: en la orilla del lago mas cercano, mirando al agua
        for (float r = 20.0f; r < 400.0f; r += 6.0f) {
            bool found = false;
            for (int k = 0; k < 24 && !found; k++) {
                float a = (float)k * PI / 12.0f, x = cosf(a) * r, z = sinf(a) * r;
                if (terrain_water(&terrain, x, z) < terrain_height(&terrain, x, z) + 2.0f) continue;
                for (float back = 0.0f; back < r; back += 1.0f) { // hacia el campamento, hasta la orilla
                    float bx = cosf(a) * (r - back), bz = sinf(a) * (r - back);
                    if (terrain_water(&terrain, bx, bz) < terrain_height(&terrain, bx, bz) - 0.1f) {
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

    if (start_goto && !gallery_mode) go_to(&terrain, &player, start_goto);
    if (start_goto && !gallery_mode && !strcmp(start_goto, "hoguera"))
        props_add(&g_props, "estructura.campamento.hoguera", (Vector3){ 3.0f, terrain_height(&terrain, 3.0f, -2.0f), -2.0f }, 0.0f);

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
    if (start_lake || start_goto) rig.yaw = player.yaw; // mirando al agua (o a lo que interesa)
    if (start_pitch >= 0.0f) rig.pitch = start_pitch;
    if (start_heading >= 0.0f) rig.yaw = player.yaw = start_heading * DEG2RAD;

    // Menu de entrada: al arrancar normalmente; las pruebas (--screenshot, --galeria...) van
    // directo a la partida salvo que pidan el menu (--menu, --instructivo, --huecos).
    TitleMenu menu;
    menu_init(&menu);
    icons_load();
    bool in_game = gallery_mode || (shot_path && !start_menu);
    if (!in_game) menu_open(&menu, MENU_TITLE);
    if (start_menu && start_menu_screen != MENU_TITLE) menu_open(&menu, start_menu_screen);
    if (start_help_page > 0) menu.page = start_help_page;
    bool show_controls = false, quit = false;
    char last_log[128] = "";
    float log_age = 99.0f;
    Image pause_shot = { 0 }; // la partida al pausar (minifoto de la partida guardada)
    char err[120];

    int frame = 0;
    bool has_keyboard = platform_has_keyboard() && !simulate_no_keyboard;
    bool keyboard_override = false; // «jugar igual»: un toque sobre el aviso, o cualquier tecla que llegue
    bool show_diag = start_diag;
    SetExitKey(KEY_NULL); // Esc abre el menu
    while (!WindowShouldClose() && !quit) {
        float dt = fminf(GetFrameTime(), 0.05f);
        input_update();
        if (input_last_key() && !simulate_no_keyboard) keyboard_override = true; // llego una tecla: hay teclado
        if (!has_keyboard && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) keyboard_override = true;
        if (input_pressed(IN_DIAG)) show_diag = !show_diag;
        input_set_debug(show_diag);
        { // el puntero (raton o dedo) en la pantalla virtual de 640x360
            float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
            float k = fminf(sw / VIRTUAL_W, sh / VIRTUAL_H);
            Vector2 m = GetMousePosition();
            Vector2 v = { (m.x - (sw - VIRTUAL_W * k) * 0.5f) / k, (m.y - (sh - VIRTUAL_H * k) * 0.5f) / k };
            // Se movio si cambio de sitio desde el cuadro anterior (al menos medio pixel). No sirve
            // GetMouseDelta: en Android raylib solo la renueva con cada evento tactil y queda fija
            // en lo ultimo, asi que los menus creian que el puntero se movia siempre y le
            // devolvian el cursor a lo que tenia debajo (las flechas parecian enloquecidas).
            static Vector2 last_v = { -1000.0f, -1000.0f };
            bool moved = fabsf(v.x - last_v.x) + fabsf(v.y - last_v.y) >= 0.5f && last_v.x > -999.0f;
            last_v = v;
            ui_pointer_frame(v, moved, IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
        }
        if (frame % 30 == 0) has_keyboard = (platform_has_keyboard() && !simulate_no_keyboard) || keyboard_override; // conexion en caliente
        frame++;
        hud_frame_begin();
        if (start_picker >= 0 && frame == 10) hud_open_picker(start_picker), start_picker = -1;
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
        if (in_game && !gallery_mode && !orbital && !menu_visible(&menu) && !ig_blocks_input(&g_actions) && input_pressed(IN_PAUSE)) {
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
                game_new(&gs, &terrain, &camp, seed_given ? start_seed : fresh_seed(), 1, 4.0f, log, sizeof(log));
                in_game = true;
                menu.screen = MENU_HIDDEN;
                break;
            case MENU_CONTINUE: menu.screen = MENU_HIDDEN; break;
            case MENU_LOAD_SLOT:
                if (save_read(menu.slot, &gs, err, sizeof(err))) {
                    world_reset(&terrain, &camp, g_hazards.seed); // el mundo de esa partida
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
        if (in_game && input_pressed(IN_HELP)) show_controls = !show_controls;
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
            if (save_read(start_load, &gs, err, sizeof(err))) {
                world_reset(&terrain, &camp, g_hazards.seed);
                snprintf(log, sizeof(log), T("Partida cargada (hueco %d)."), start_load + 1);
            }
            else snprintf(log, sizeof(log), "%s", err);
            start_load = -1;
        }
        if (strcmp(log, last_log) != 0) snprintf(last_log, sizeof(last_log), "%s", log), log_age = 0.0f;
        log_age += dt;
        // Sin teclado el juego queda en pausa (Android: tablets sin teclado conectado); con el menu, tambien.
        // F5: la vista orbital del mundo (las flechas la giran y la inclinan).
        if (in_game && !menu_visible(&menu) && input_pressed(IN_ORBITAL)) orbital = !orbital;
        if (orbital) {
            orbit_angle += (IsKeyDown(KEY_RIGHT) ? 1.0f : IsKeyDown(KEY_LEFT) ? -1.0f : 0.06f) * dt;
            if (IsKeyDown(KEY_UP)) orbit_tilt = fminf(1.0f, orbit_tilt + 0.5f * dt);
            if (IsKeyDown(KEY_DOWN)) orbit_tilt = fmaxf(0.0f, orbit_tilt - 0.5f * dt);
            if (input_pressed(IN_BACK)) orbital = false;
        }
        if (has_keyboard && in_game && !menu_visible(&menu)) {
            // Con el menu de acciones abierto el jugador no se mueve (las flechas eligen).
            bool menu = ga_menu_open(&g_actions) || hud_picker_open();
            bool blocked = hud_picker_open() || ga_blocks_input(&g_actions) || hz_blocks_input(&g_hazards) || cb_blocks_input(&g_combat) || orbital;
            PlayerInput in = blocked ? (PlayerInput){ 0 } : player_read_input();
            player.speed_scale = ga_speed_scale(&g_actions) * hz_speed_scale(&g_hazards) * cb_speed_scale(&g_combat);
            player.draw_lift = g_actions.mounted >= 0 ? 1.1f : 0.0f;
            if (!g_actions.climbing) player_update(&player, &terrain, in, rig.yaw, dt);
            if (world_clamp(terrain.world, &player.pos.x, &player.pos.z)) { // la frontera invisible del gran circulo
                static float told = -100.0f;
                if (world_time - told > 30.0f) {
                    told = world_time;
                    Region rg = terrain_region(&terrain, player.pos.x, player.pos.z);
                    snprintf(log, sizeof(log), "%s",
                             rg == REGION_FJORD    ? T("Más allá solo hay mar abierto: la tropa no se aventura.")
                             : rg == REGION_DESERT ? T("El gran muro del cañón cierra el desierto: no hay paso.")
                             : rg == REGION_HIGHLAND ? T("El muro de hielo del glaciar cierra el altiplano: no hay paso.")
                                                   : T("El gran canal marca el fin de estas tierras: no hay vado."));
                }
            }
            world_time += dt;
            int prev_champion = last_champion;
            if (!menu && show_diag) debug_camp_actions(&troop, &rng, &world_time, &last_champion, log, sizeof(log)); // teclas de prueba: con Ctrl+D
            g_actions.player_armor = &g_combat.armor; // para reparar lo que llevas puesto
            if (!gallery_mode) // la barra rapida (1..9) y su selector, antes que las acciones
                hud_update(&g_actions, &g_props, &player, !hz_blocks_input(&g_hazards) && !ig_blocks_input(&g_actions), log, sizeof(log));
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
            if (!gallery_mode) ag_update(&g_actions, &troop, &g_climate, g_hazards.seed, world_time, dt, log, sizeof(log));
            if (!gallery_mode) wd_update(&g_actions, &g_combat, &terrain, &player, &g_memory, log, sizeof(log));
            if (!gallery_mode)
                wg_update(&g_actions, &troop, &player, &g_props, &g_hazards, &g_climate, camp.fire, &terrain,
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
            // Un clic o un toque sobre un enemigo (no sobre el HUD) lo elige como objetivo.
            if (!gallery_mode && !menu && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !hud_pointer_over() &&
                !ig_blocks_input(&g_actions) && !g_actions.dlg.open)
                cb_pick_target(&g_combat, cam, virtual_pointer(), VIRTUAL_W, VIRTUAL_H, log, sizeof(log));
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
            if (start_target && frame == 30 && !gallery_mode) // prueba: como pulsar Tab con enemigos cerca
                cb_cycle_target(&g_combat, &player, 1, log, sizeof(log));
            if (start_back && frame == 30 && !gallery_mode) // prueba: la espalda ganada (y el rehen)
                cb_test_back(&g_combat, &player, start_back == 2, log, sizeof(log));
            if (start_crouch && !gallery_mode) { // prueba: agachado (X), de perfil para ver la postura
                player.crouching = true;
                if (frame == 3) player.yaw = rig.yaw + PI / 2;
            }
            if (last_champion != prev_champion) show_card = true; // ficha al conocerlo
            advance_days(&troop, &rng, &day, world_time, &terrain, log, sizeof(log));
            if (input_action_pressed(KA_CARD) && !ig_blocks_input(&g_actions)) // U: la ficha del gran guerrero (en el inventario, U cambia la mochila)
                show_card = !show_card && last_champion >= 0;
            memmap_visit(&g_memory, player.pos.x, player.pos.z, dt, world_time);
            { // la vista (tatuajes del grifo, lapislazuli): el mapa se descubre mas lejos
                static float reveal_t = 0.0f;
                float per = ig_stat(&g_actions, STAT_PERCEPTION);
                if (per > 0.0f && (reveal_t += dt) > 1.0f) {
                    reveal_t = 0.0f;
                    memmap_reveal(&g_memory, player.pos.x, player.pos.z, 30.0f * per, 30.0f, world_time);
                }
            }
            if (input_action_pressed(KA_MARK)) { // M: marcar el lugar (Ctrl+M es la vista orbital)
                MarkerKind kind = IsKeyDown(KEY_LEFT_SHIFT) ? MARKER_DANGER : MARKER_INTEREST;
                bool placed = memmap_toggle_marker(&g_memory, player.pos.x, player.pos.z, kind, 6.0f);
                snprintf(log, sizeof(log), "%s", placed ? T("Marcaste este lugar en el mapa.") : T("Quitaste la marca."));
            }
        }
        camera_update(&rig, &cam, &player, &terrain, dt);
        terrain_update(&terrain, player.pos);
        // Clima: estacion, tiempo, nieve, lagos y glaciares (la galeria se ve siempre igual).
        Climate climate = climate_at(world_time, terrain.seed);
        if (gallery_mode) climate = (Climate){ .clouds = 0.1f, .temperature = 20.0f };
        if (!gallery_mode) {
            apply_climate_look(&terrain, &climate);
            props_set_season(&g_props, clock_season(clock_day(world_time)));
            weather_update(&weather, &climate, dt);
        }
        g_climate = climate;
        terrain_set_haze(&terrain, sky_clear_color(climate.clouds)); // la bruma del horizonte, del color del cielo

        BeginTextureMode(lowres);
        ClearBackground(sky_clear_color(climate.clouds));
        if (in_game && orbital) { // el mundo entero desde lo alto
            ClearBackground((Color){ 14, 18, 28, 255 });
            worldview_build(&view, terrain.world, terrain.plain + 40.0f);
            worldview_draw(&view, terrain.world, orbit_angle, orbit_tilt, player.pos, (float)GetTime());
            char rg[48];
            snprintf(rg, sizeof(rg), "%s", region_name(terrain_region(&terrain, player.pos.x, player.pos.z)));
            ui_text(TextFormat(T("Vista orbital · mundo %u · estás en: %s"), terrain.seed, rg), 10, 10, 10, UI_GOLD_LIGHT);
            ui_text(T("Flechas: girar e inclinar · F5 o Esc: volver"), 10, VIRTUAL_H - 18, 10, UI_BONE_DIM);
        } else if (in_game) {
        // El firmamento: degradado, resplandor y sol (la galeria, siempre a media mañana).
        float sky_time = gallery_mode ? 400.0f : world_time;
        sky_draw_background(cam, sky_time, climate.clouds, VIRTUAL_W, VIRTUAL_H);
        rlSetClipPlanes(0.1, 3300.0); // el horizonte lejano y el cielo, a ~3 km
        hearth_frame_begin();
        BeginMode3D(cam);
        terrain_draw(&terrain);
        camp_draw(&camp, (float)GetTime(), climate.snow_cover, !g_actions.fires_out && g_actions.camps[0].used, g_dz.tree_burn);
        if (gallery_mode) gallery_draw(&gallery);
        bool player_model = !gallery_mode && ga_draw_player(&g_actions, &g_props, &player, (float)GetTime());
        if (gallery_mode) player_draw(&player);
        else cb_draw_world(&g_combat, &g_props, &g_actions, &terrain, &player, player_model, (float)GetTime());
        if (!gallery_mode) ga_draw_world(&g_actions, &g_props, &terrain, &troop, &player, (float)GetTime());
        if (!gallery_mode) wd_draw_world(&g_actions, &terrain, &player, (float)GetTime());
        if (!gallery_mode) dg_draw_world(&terrain); // calaveras y huesos donde murio el jugador
        if (!gallery_mode) fg_draw_world(&g_actions, &g_props, &terrain, (float)GetTime());
        if (!gallery_mode) ig_draw_world(&g_actions, &terrain, (float)GetTime());
        if (!gallery_mode) trv_draw_world(&g_actions, &troop, (float)GetTime());
        if (!gallery_mode) dz_draw_world(&g_dz, &terrain, (float)GetTime());
        if (!gallery_mode) terrain_draw_water(&terrain, (float)GetTime()); // translucida: despues de lo opaco
        hearth_draw_smoke(cam, (float)GetTime(), climate.wind, gallery_mode ? 1.0f : clock_light(world_time)); // el humo, oscurecido de noche como todo
        if (gallery_mode) hearth_draw_flames((float)GetTime(), climate.wind);
        if (!gallery_mode) hz_draw_world(&g_hazards, &terrain, &g_actions, &troop, (float)GetTime());
        if (!gallery_mode) clouds_draw(&terrain, cam, world_time, climate.clouds, climate.wind, sky_clear_color(climate.clouds));
        EndMode3D();
        if (!gallery_mode) { // la galeria se ve siempre de dia
            // Dentro de una nube (en una cumbre alta): niebla.
            clouds_draw_mist(clouds_mist(&terrain, cam.position, world_time, climate.clouds), VIRTUAL_W, VIRTUAL_H);
            // Noche: se oscurece todo y se suman las estrellas, las llamas y el brillo de los fuegos.
            sky_apply_tint(world_time, climate.clouds, VIRTUAL_W, VIRTUAL_H);
            BeginMode3D(cam);
            sky_draw_stars(&sky, cam, world_time, climate.clouds);
            sky_draw_moon(cam, world_time, climate.clouds);
            hearth_draw_flames((float)GetTime(), climate.wind); // los fuegos encendidos de este cuadro
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
            ag_draw_overlay(&g_actions, &troop, cam, VIRTUAL_W, VIRTUAL_H);
        }
        rlSetClipPlanes(RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
        if (gallery_mode) {
            gallery_draw_labels(&gallery, cam, player.pos, VIRTUAL_W, VIRTUAL_H);
        } else if (!menu_visible(&menu)) {
            draw_hud(&troop, world_time, ga_hands_text(&g_actions), log, log_age,
                     g_actions.menu_open || g_actions.inv_open || g_actions.equip_open);
            minimap_set_world(&minimap, terrain.world, terrain.plain);
            minimap_draw(&minimap, &g_memory, (Vector2){ VIRTUAL_W - MINIMAP_RADIUS - 10, MINIMAP_RADIUS + 13 },
                         player.pos, rig.yaw, player.yaw, world_time);
            draw_clock_bar(VIRTUAL_W - MINIMAP_RADIUS - 10, 2 * MINIMAP_RADIUS + 19, 2 * MINIMAP_RADIUS - 8, world_time);
            draw_weather_text(VIRTUAL_W - 14, 2 * MINIMAP_RADIUS + 27, &climate, terrain_region(&terrain, player.pos.x, player.pos.z));
            ga_draw_hud(&g_actions, &g_props, &troop, &player, VIRTUAL_W, VIRTUAL_H);
            // La placa de las constantes bajo el minimapa: vida, calor del cuerpo, sed y aguante.
            const int vy = 2 * MINIMAP_RADIUS + 40;
            hud_vitals_frame(VIRTUAL_W - 10, vy - 3, 4);
            cb_draw_hud(&g_combat, &g_actions, &troop, VIRTUAL_W - 14, vy, VIRTUAL_W, VIRTUAL_H);
            hz_draw_hud(&g_hazards, &troop, VIRTUAL_W - 14, vy + 22, VIRTUAL_W, VIRTUAL_H);
            wg_draw_hud(&g_actions, VIRTUAL_W - 14, vy + 44);
            hud_stamina(VIRTUAL_W - 14, vy + 66);
            // Barra rapida (1..9) abajo a la izquierda y la columna de acciones en el borde.
            if (!ig_blocks_input(&g_actions) && !ga_menu_open(&g_actions) && !g_actions.dlg.open) {
                hud_quickbar(&g_actions, &g_props, &player, 6, VIRTUAL_H - 54);
                hud_action_column(&g_actions, 6, 68);
                cb_draw_buttons(&g_combat, &g_actions, VIRTUAL_W, VIRTUAL_H); // H J K L, abajo a la derecha
            }
            fg_draw_hud(&g_actions, VIRTUAL_W, VIRTUAL_H);
            ig_draw(&g_actions, &g_combat, &g_props, &player, VIRTUAL_W, VIRTUAL_H);
            if (!ig_blocks_input(&g_actions) && !ga_menu_open(&g_actions)) tg_draw_hud(&g_actions, VIRTUAL_W, VIRTUAL_H);
            hud_draw_picker(&g_actions, &g_props, &player, VIRTUAL_W, VIRTUAL_H); // elegir que va en una casilla
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
        if (in_game && !menu_visible(&menu) && (platform_touch_ui() || g_force_touch)) draw_touch_buttons();
        if (show_diag) draw_diagnostics(has_keyboard);
        if (!has_keyboard) draw_keyboard_notice();
        // Lo translucido (humo, agua, nubes) baja el alfa del cuadro: se deja opaco para que la
        // captura y la minifoto de la partida se vean como en pantalla.
        rlDrawRenderBatchActive();
        rlColorMask(false, false, false, true);
        DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, WHITE);
        rlDrawRenderBatchActive();
        rlColorMask(true, true, true, true);
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
    clouds_unload();
    hearth_unload();
    terrain_unload_gpu();
    voxs_unload();
    UnloadRenderTexture(lowres);
    CloseWindow();
    return 0;
}
