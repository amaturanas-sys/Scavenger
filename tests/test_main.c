// Tests del nucleo de simulacion. Arnes minimo, sin dependencias.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/sim/actions.h"
#include "../src/sim/anim_index.h"
#include "../src/sim/animals.h"
#include "../src/sim/voxel.h"
#include "../src/sim/economy.h"
#include "../src/sim/champion.h"
#include "../src/sim/climate.h"
#include "../src/sim/hazards.h"
#include "../src/sim/health.h"
#include "../src/sim/combat.h"
#include "../src/sim/apparel.h"
#include "../src/sim/armor.h"
#include "../src/sim/ballistics.h"
#include "../src/sim/body.h"
#include "../src/sim/clock.h"
#include "../src/sim/inventory.h"
#include "../src/sim/loadout.h"
#include "../src/sim/camps.h"
#include "../src/sim/jewelry.h"
#include "../src/sim/lang.h"
#include "../src/sim/talents.h"
#include "../src/sim/memory_map.h"
#include "../src/sim/noise.h"
#include "../src/sim/rng.h"
#include "../src/sim/swarms.h"
#include "../src/sim/fire.h"
#include "../src/sim/melee.h"
#include "../src/sim/storage.h"
#include "../src/sim/travel.h"
#include "../src/sim/troop.h"
#include "../src/sim/water.h"
#include "../src/sim/world.h"

static int g_failed = 0, g_checks = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        g_checks++;                                                                 \
        if (!(cond)) {                                                              \
            g_failed++;                                                             \
            fprintf(stderr, "  FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                           \
    } while (0)

#define RUN(fn)                     \
    do {                            \
        int before = g_failed;      \
        fn();                       \
        printf("%s %s\n", g_failed == before ? "ok  " : "FAIL", #fn); \
    } while (0)

// ---------------------------------------------------------------- tropa
static void test_recruit_and_roles(void) {
    Troop t;
    troop_init(&t, NULL);
    int a = troop_recruit(&t, "Batu", 0);
    int p = troop_take_prisoner(&t, "Cautivo", 0);
    CHECK(a > 0 && p > 0 && a != p);
    CHECK(troop_count_with_status(&t, STATUS_ACTIVE) == 1);
    CHECK(troop_count_with_status(&t, STATUS_PRISONER) == 1);
    CHECK(troop_assign_role(&t, a, ROLE_SCOUT));
    CHECK(troop_find(&t, a)->role == ROLE_SCOUT);
    CHECK(!troop_assign_role(&t, p, ROLE_GUARD)); // un prisionero no recibe funciones
    CHECK(!troop_assign_role(&t, 999, ROLE_GUARD));
}

static void test_banish_removes_from_active(void) {
    Troop t;
    troop_init(&t, NULL);
    int a = troop_recruit(&t, "Batu", 0);
    troop_assign_role(&t, a, ROLE_COOK);
    CHECK(troop_banish(&t, a));
    CHECK(troop_find(&t, a)->status == STATUS_BANISHED);
    CHECK(troop_find(&t, a)->role == ROLE_NONE);
    CHECK(!troop_banish(&t, a)); // no se destierra dos veces
}

static void test_execution_hits_morale_by_trait(void) {
    Troop t;
    troop_init(&t, NULL);
    int merciful = troop_recruit(&t, "Compasiva", TRAIT_MERCIFUL);
    int brute = troop_recruit(&t, "Sanguinario", TRAIT_BLOODTHIRSTY);
    int victim = troop_recruit(&t, "Condenado", 0);
    float m0 = troop_find(&t, merciful)->morale;
    float b0 = troop_find(&t, brute)->morale;
    CHECK(troop_execute(&t, victim));
    CHECK(troop_find(&t, victim)->status == STATUS_EXECUTED);
    CHECK(troop_find(&t, merciful)->morale < m0 - 10.0f); // la compasiva lo sufre mucho
    CHECK(troop_find(&t, brute)->morale < b0);            // aun asi pesa...
    CHECK(b0 - troop_find(&t, brute)->morale < m0 - troop_find(&t, merciful)->morale); // ...mucho menos
    CHECK(!troop_execute(&t, victim)); // ya ejecutado
}

static void test_kingdom_reacts_to_its_values(void) {
    Kingdom iron, jade;
    kingdom_init_iron_khanate(&iron);
    kingdom_init_jade_dynasty(&jade);
    Troop a, b;
    troop_init(&a, &iron);
    troop_init(&b, &jade);
    int pa = troop_take_prisoner(&a, "X", 0);
    int pb = troop_take_prisoner(&b, "X", 0);
    float iron0 = iron.relation, jade0 = jade.relation;
    troop_execute(&a, pa);
    troop_execute(&b, pb);
    CHECK(iron.relation > iron0); // el kanato de mano dura lo aprueba
    CHECK(jade.relation < jade0); // la dinastia piadosa lo castiga
}

static void test_release_prisoner(void) {
    Kingdom jade;
    kingdom_init_jade_dynasty(&jade);
    Troop t;
    troop_init(&t, &jade);
    int p = troop_take_prisoner(&t, "X", 0);
    float r0 = jade.relation;
    CHECK(troop_release_prisoner(&t, p));
    CHECK(jade.relation > r0);
    CHECK(!troop_release_prisoner(&t, p));
}

static void test_desertion_only_when_unhappy(void) {
    Member m = { .status = STATUS_ACTIVE, .morale = 80, .loyalty = 80 };
    CHECK(troop_desertion_chance(&m) == 0.0f);
    m.morale = 10;
    m.loyalty = 10;
    float p = troop_desertion_chance(&m);
    CHECK(p > 0.2f && p <= 0.9f);
    m.status = STATUS_PRISONER;
    CHECK(troop_desertion_chance(&m) == 0.0f); // los prisioneros no "desertan"
}

static void test_rebellion_grows_as_morale_falls(void) {
    Troop t;
    troop_init(&t, NULL);
    for (int i = 0; i < 4; i++) troop_recruit(&t, "G", i == 0 ? TRAIT_AMBITIOUS : 0);
    CHECK(troop_rebellion_chance(&t) == 0.0f); // tropa contenta
    for (int i = 0; i < t.count; i++) t.members[i].morale = 30.0f;
    float mid = troop_rebellion_chance(&t);
    for (int i = 0; i < t.count; i++) t.members[i].morale = 5.0f;
    float low = troop_rebellion_chance(&t);
    CHECK(mid > 0.0f && low > mid);
    t.members[0].loyalty = 10.0f; // ambicioso desleal: atiza la revuelta
    CHECK(troop_rebellion_chance(&t) > low);
}

static void test_process_day_is_deterministic(void) {
    Troop a, b;
    troop_init(&a, NULL);
    for (int i = 0; i < 10; i++) troop_recruit(&a, "G", i % 3 == 0 ? TRAIT_AMBITIOUS : 0);
    for (int i = 0; i < a.count; i++) { a.members[i].morale = 15; a.members[i].loyalty = 15; }
    b = a;
    Rng r1, r2;
    rng_seed(&r1, 42);
    rng_seed(&r2, 42);
    DayReport d1 = troop_process_day(&a, &r1);
    DayReport d2 = troop_process_day(&b, &r2);
    CHECK(d1.deserted == d2.deserted && d1.rebellion == d2.rebellion);
    CHECK(d1.deserted > 0); // con moral y lealtad tan bajas, alguien se va
    CHECK(troop_count_with_status(&a, STATUS_DESERTED) == d1.deserted);
}

static void test_share_loot_lifts_spirits(void) {
    Troop t;
    troop_init(&t, NULL);
    int g = troop_recruit(&t, "G", 0);
    float m0 = troop_find(&t, g)->morale, l0 = troop_find(&t, g)->loyalty;
    troop_share_loot(&t);
    CHECK(troop_find(&t, g)->morale > m0 && troop_find(&t, g)->loyalty > l0);
}

// ---------------------------------------------------------------- equipo
static void test_amulets_swap_and_stack(void) {
    Loadout l;
    loadout_init(&l);
    Charm wolf = charm_make("amuleto_lobo");
    charm_add_buff(&wolf, "sigilo", BUFF_PASSIVE, STAT_STEALTH, 0.2f);
    Charm bear = charm_make("amuleto_oso");
    charm_add_buff(&bear, "agarre", BUFF_PASSIVE, STAT_GRAPPLE_POWER, 0.3f);
    CHECK(loadout_equip_amulet(&l, 0, &wolf));
    CHECK(fabsf(loadout_stat(&l, STAT_STEALTH) - 0.2f) < 1e-6f);
    CHECK(loadout_equip_amulet(&l, 0, &bear)); // reemplaza en el mismo espacio
    CHECK(loadout_stat(&l, STAT_STEALTH) == 0.0f);
    CHECK(fabsf(loadout_stat(&l, STAT_GRAPPLE_POWER) - 0.3f) < 1e-6f);
    CHECK(loadout_unequip_amulet(&l, 0));
    CHECK(loadout_stat(&l, STAT_GRAPPLE_POWER) == 0.0f);
    CHECK(!loadout_equip_amulet(&l, AMULET_SLOTS, &wolf)); // espacio inexistente
}

static void test_tattoos_are_permanent(void) {
    Loadout l;
    loadout_init(&l);
    Charm eagle = charm_make("tatuaje_aguila");
    charm_add_buff(&eagle, "vista", BUFF_PASSIVE, STAT_ARCHERY, 0.15f);
    CHECK(loadout_apply_tattoo(&l, &eagle));
    CHECK(!loadout_apply_tattoo(&l, &eagle)); // no se duplica
    // Cambiar amuletos no toca los tatuajes.
    Charm any = charm_make("amuleto_x");
    loadout_equip_amulet(&l, 1, &any);
    loadout_unequip_amulet(&l, 1);
    CHECK(fabsf(loadout_stat(&l, STAT_ARCHERY) - 0.15f) < 1e-6f);
    CHECK(l.tattoo_count == 1);
}

static void test_active_buffs_and_skill_tree(void) {
    Loadout l;
    loadout_init(&l);
    Charm horse = charm_make("amuleto_caballo");
    charm_add_buff(&horse, "carga", BUFF_ACTIVE, STAT_RIDING, 1.0f);
    charm_add_buff(&horse, "jinete", BUFF_PASSIVE, STAT_RIDING, 0.1f);
    SkillNode charge = { .id = "carga_montada", .requires_charm = "amuleto_caballo" };
    SkillNode base = { .id = "golpe_basico", .requires_charm = "" };
    CHECK(skill_unlocked(&l, &base));
    CHECK(!skill_unlocked(&l, &charge));
    loadout_equip_amulet(&l, 2, &horse);
    CHECK(skill_unlocked(&l, &charge));
    const Buff *act[4];
    CHECK(loadout_active_buffs(&l, act, 4) == 1);
    CHECK(strcmp(act[0]->id, "carga") == 0);
}

// ---------------------------------------------------------------- terreno
static void test_noise_is_deterministic_and_bounded(void) {
    float a = fbm2d(12.3f, -4.5f, 7, 5);
    float b = fbm2d(12.3f, -4.5f, 7, 5);
    CHECK(a == b);
    CHECK(fbm2d(12.3f, -4.5f, 8, 5) != a); // otra semilla, otro mundo
    for (int i = 0; i < 1000; i++) {
        float v = fbm2d(i * 0.37f, i * -0.21f, 1, 5);
        CHECK(v >= -1.0f && v <= 1.0f);
    }
    // Continuidad: puntos vecinos dan alturas vecinas (sin costuras entre chunks).
    CHECK(fabsf(noise2d(10.0f, 10.0f, 3) - noise2d(10.001f, 10.0f, 3)) < 0.01f);
}

// ---------------------------------------------------------------- mapa de memoria
static void walk(MemoryMap *m, float x, float z, float seconds, float *now) {
    for (float t = 0.0f; t < seconds; t += 0.1f) {
        *now += 0.1f;
        memmap_visit(m, x, z, 0.1f, *now);
    }
}

static void test_memmap_starts_dark_and_lights_gradually(void) {
    MemoryMap m;
    memmap_init(&m);
    float now = 0.0f;
    CHECK(memmap_light(&m, 0, 0, now) == 0.0f);
    walk(&m, 0, 0, 1.0f, &now);
    float early = memmap_light(&m, 0, 0, now);
    CHECK(early > 0.0f && early < 0.3f); // se ilumina poco a poco
    walk(&m, 0, 0, 10.0f, &now);
    CHECK(memmap_light(&m, 0, 0, now) > early);
    CHECK(memmap_light(&m, 100.0f, 0, now) == 0.0f); // lo no visto sigue negro
    // El borde de la vista se memoriza menos que donde se pisa.
    CHECK(memmap_light(&m, 18.0f, 0, now) < memmap_light(&m, 0, 0, now));
    memmap_free(&m);
}

static void test_memmap_frequent_places_are_brighter(void) {
    MemoryMap m;
    memmap_init(&m);
    float now = 0.0f;
    walk(&m, -200.0f, 0, 20.0f, &now);  // una pasada breve
    walk(&m, 200.0f, 0, 600.0f, &now);  // diez minutos en el mismo lugar
    float rare = memmap_light(&m, -200.0f, 0, now + 1.0f);
    float frequent = memmap_light(&m, 200.0f, 0, now + 1.0f);
    CHECK(frequent > rare);
    CHECK(frequent > 0.6f);
    CHECK(memmap_familiarity(&m, 200.0f, 0) > memmap_familiarity(&m, -200.0f, 0));
    memmap_free(&m);
}

static void test_memmap_forgets_over_days(void) {
    MemoryMap m;
    memmap_init(&m);
    float now = 0.0f;
    walk(&m, -200.0f, 0, 20.0f, &now);
    walk(&m, 200.0f, 0, 600.0f, &now);
    float rare0 = memmap_light(&m, -200.0f, 0, now);
    float freq0 = memmap_light(&m, 200.0f, 0, now);
    // Unas horas de juego apenas cambian nada: el olvido se mide en dias.
    CHECK(memmap_light(&m, -200.0f, 0, now + GAME_SECONDS_PER_DAY * 0.1f) > rare0 * 0.95f);
    float later = now + 3.0f * GAME_SECONDS_PER_DAY;
    float rare1 = memmap_light(&m, -200.0f, 0, later);
    float freq1 = memmap_light(&m, 200.0f, 0, later);
    CHECK(rare1 < rare0 * 0.6f && freq1 < freq0); // todo se olvida
    CHECK(freq1 / freq0 > rare1 / rare0);          // lo familiar, mas despacio
    CHECK(memmap_light(&m, -200.0f, 0, now + 60.0f * GAME_SECONDS_PER_DAY) < 0.01f);
    memmap_free(&m);
}

static void test_clock_seasonal_day_night(void) {
    // Ciclo completo de 30 minutos; dia y noche suman siempre el ciclo.
    CHECK(GAME_SECONDS_PER_DAY == 1800.0f);
    for (int d = 1; d <= 2 * DAYS_PER_YEAR; d++) {
        float light = clock_daylight_seconds(d), night = clock_night_seconds(d);
        CHECK(fabsf(light + night - GAME_SECONDS_PER_DAY) < 0.01f);
        CHECK(light >= 600.0f - 0.5f && light <= 1200.0f + 0.5f);
    }
    // Estaciones: primavera, verano, otono, invierno; el ano se repite.
    CHECK(clock_season(1) == SEASON_SPRING);
    CHECK(clock_season(1 + DAYS_PER_SEASON) == SEASON_SUMMER);
    CHECK(clock_season(1 + 2 * DAYS_PER_SEASON) == SEASON_AUTUMN);
    CHECK(clock_season(1 + 3 * DAYS_PER_SEASON) == SEASON_WINTER);
    CHECK(clock_season(1 + DAYS_PER_YEAR) == SEASON_SPRING);
    CHECK(clock_day_of_season(DAYS_PER_SEASON) == DAYS_PER_SEASON && clock_day_of_season(DAYS_PER_SEASON + 1) == 1);
    // Pleno verano: 20 min de dia y 10 de noche. Pleno invierno: al reves.
    int mid_summer = 1 + DAYS_PER_SEASON + DAYS_PER_SEASON / 2;
    int mid_winter = 1 + 3 * DAYS_PER_SEASON + DAYS_PER_SEASON / 2;
    CHECK(clock_season(mid_summer) == SEASON_SUMMER && clock_season(mid_winter) == SEASON_WINTER);
    CHECK(fabsf(clock_daylight_seconds(mid_summer) - 1200.0f) < 1.0f);
    CHECK(fabsf(clock_night_seconds(mid_winter) - 1200.0f) < 1.0f);
    // Equinoccios (mitad de primavera y de otono): parejo.
    int mid_spring = 1 + DAYS_PER_SEASON / 2, mid_autumn = 1 + 2 * DAYS_PER_SEASON + DAYS_PER_SEASON / 2;
    CHECK(fabsf(clock_daylight_seconds(mid_spring) - 900.0f) < 60.0f);
    CHECK(fabsf(clock_daylight_seconds(mid_autumn) - 900.0f) < 60.0f);
    // El verano es mas largo de dia que la primavera, y la primavera que el invierno.
    CHECK(clock_daylight_seconds(mid_summer) > clock_daylight_seconds(mid_spring));
    CHECK(clock_daylight_seconds(mid_spring) > clock_daylight_seconds(mid_winter));

    // Dentro de un dia de invierno: amanece al empezar, mediodia a 5 min, noche cerrada despues.
    float t0 = (float)(mid_winter - 1) * GAME_SECONDS_PER_DAY, dl = clock_daylight_seconds(mid_winter);
    CHECK(clock_day(t0 + 1.0f) == mid_winter);
    CHECK(clock_phase(t0 + 1.0f) == PHASE_DAWN);
    CHECK(clock_phase(t0 + dl * 0.5f) == PHASE_DAY && clock_light(t0 + dl * 0.5f) == 1.0f);
    CHECK(fabsf(clock_sun_height(t0 + dl * 0.5f) - 1.0f) < 1e-3f);
    CHECK(clock_phase(t0 + dl) == PHASE_DUSK && fabsf(clock_light(t0 + dl) - 0.5f) < 1e-3f);
    float midnight = t0 + dl + 0.5f * (GAME_SECONDS_PER_DAY - dl);
    CHECK(clock_phase(midnight) == PHASE_NIGHT && clock_light(midnight) == 0.0f && clock_is_night(midnight));
    CHECK(fabsf(clock_sun_height(midnight) + 1.0f) < 1e-3f);
    CHECK(clock_phase(t0 + GAME_SECONDS_PER_DAY - 1.0f) == PHASE_DAWN); // antes del alba siguiente
    CHECK(!clock_is_night(t0 + dl * 0.5f));
    // La luz es continua: sin saltos bruscos de un segundo al siguiente.
    float prev = clock_light(t0);
    for (float t = t0 + 1.0f; t < t0 + 2.0f * GAME_SECONDS_PER_DAY; t += 1.0f) {
        float l = clock_light(t);
        CHECK(fabsf(l - prev) < 0.05f);
        prev = l;
    }
    CHECK(fabsf(clock_phase_progress(t0 + dl * 0.25f) - 0.25f) < 1e-3f);
    CHECK(!strcmp(season_name(SEASON_AUTUMN), "otoño") && !strcmp(phase_name(PHASE_DAY), "día"));
}

static float day_time(int day, float minute) { return (float)(day - 1) * GAME_SECONDS_PER_DAY + minute * 60.0f; }

static void test_climate_seasons_and_weather(void) {
    const uint32_t seed = 1206u;
    int mid_spring = 1 + DAYS_PER_SEASON / 2, mid_summer = mid_spring + DAYS_PER_SEASON;
    int mid_autumn = mid_summer + DAYS_PER_SEASON, mid_winter = mid_autumn + DAYS_PER_SEASON;
    int late_summer = 2 * DAYS_PER_SEASON; // ultimo dia del verano
    // Determinista: misma semilla, mismo clima.
    Climate a = climate_at(12345.0f, seed), b = climate_at(12345.0f, seed);
    CHECK(a.weather == b.weather && a.snow_cover == b.snow_cover && a.water_level == b.water_level);

    Climate sp = climate_at(day_time(mid_spring, 5), seed), su = climate_at(day_time(mid_summer, 5), seed);
    Climate au = climate_at(day_time(mid_autumn, 5), seed), wi = climate_at(day_time(mid_winter, 5), seed);
    Climate ls = climate_at(day_time(late_summer, 5), seed);
    // Temperatura: calor en verano, helada en invierno.
    CHECK(su.temp_mean > 18.0f && wi.temp_mean < -12.0f);
    // Nieve en el suelo todo el invierno; nada en pleno verano.
    CHECK(wi.snow_cover > 0.95f && su.snow_cover < 0.01f);
    // Lagos congelados en invierno, agua libre en verano; crecida de deshielo en primavera.
    CHECK(wi.ice > 0.9f && su.ice == 0.0f);
    CHECK(climate_at(day_time(mid_spring + 2, 5), seed).water_level > ls.water_level + 2.0f);
    // Glaciares: la nieve permanente baja en invierno.
    CHECK(su.snowline - wi.snowline > 20.0f);
    // Vegetacion: verde en primavera, seca al final del verano, ocre solo en otono.
    CHECK(climate_at(day_time(DAYS_PER_SEASON, 5), seed).greenness > ls.greenness + 0.3f); // fin de la primavera
    CHECK(au.autumn > 0.5f && sp.autumn == 0.0f && su.autumn < 0.2f);

    // Tiempo por estacion, sobre varios anos de bloques.
    int counts[SEASON_COUNT][WEATHER_COUNT] = { { 0 } };
    const int blocks = (int)(4.0f * DAYS_PER_YEAR * GAME_SECONDS_PER_DAY / WEATHER_BLOCK_SECONDS);
    for (int bl = 0; bl < blocks; bl++) {
        int day = clock_day(((float)bl + 0.5f) * WEATHER_BLOCK_SECONDS);
        counts[clock_season(day)][climate_block_weather(bl, seed)]++;
    }
    CHECK(counts[SEASON_WINTER][WEATHER_RAIN] + counts[SEASON_WINTER][WEATHER_STORM] == 0); // en invierno no llueve
    CHECK(counts[SEASON_WINTER][WEATHER_SNOW] + counts[SEASON_WINTER][WEATHER_BLIZZARD] > 0);
    CHECK(counts[SEASON_SUMMER][WEATHER_SNOW] + counts[SEASON_SUMMER][WEATHER_BLIZZARD] == 0);
    CHECK(counts[SEASON_SUMMER][WEATHER_STORM] > 0 && counts[SEASON_AUTUMN][WEATHER_RAIN] > 0);
    for (int s = 0; s < SEASON_COUNT; s++) CHECK(counts[s][WEATHER_CLEAR] > 0);
    // Con lluvia o nieve fuertes, el cielo esta cubierto.
    for (int bl = 0; bl < blocks; bl += 7) {
        Climate c = climate_at(((float)bl + 0.2f) * WEATHER_BLOCK_SECONDS, seed);
        if (c.rain > 0.4f || c.snow > 0.4f) CHECK(c.clouds > 0.6f);
        CHECK(c.snow_cover >= 0.0f && c.snow_cover <= 1.0f && c.wetness >= 0.0f && c.wetness <= 1.0f);
    }
    // Transiciones suaves: sin saltos de un segundo al siguiente.
    float prev_clouds = climate_at(0.0f, seed).clouds, prev_level = climate_at(0.0f, seed).water_level;
    for (float t = 1.0f; t < 3.0f * GAME_SECONDS_PER_DAY; t += 1.0f) {
        Climate c = climate_at(t, seed);
        CHECK(fabsf(c.clouds - prev_clouds) < 0.05f);
        CHECK(fabsf(c.water_level - prev_level) < 0.05f);
        prev_clouds = c.clouds;
        prev_level = c.water_level;
    }
    CHECK(!strcmp(weather_name(WEATHER_BLIZZARD), "ventisca"));
}

static void test_hazards_cold_mud_ice(void) {
    // Frio: a -18 con viento se pierde calor; junto al fuego se recupera.
    Warmth w = { .heat = 100.0f, .wet = 0.0f };
    float feels = hazard_feels_like(-18.0f, 0.5f, 0.0f, 10.0f, 0.0f);
    CHECK(feels < -10.0f);
    float t = 0.0f;
    while (w.heat > 0.0f && t < 3600.0f) {
        warmth_update(&w, feels, 0.0f, false, 1.0f);
        t += 1.0f;
    }
    CHECK(t > 120.0f && t < 600.0f); // entre 2 y 10 minutos a la intemperie
    CHECK(warmth_level(&w) == COLD_HYPOTHERMIA && warmth_speed_scale(&w) < 0.65f);
    float warm = hazard_feels_like(-18.0f, 0.5f, 0.0f, 10.0f, 30.0f);
    for (int i = 0; i < 120; i++) warmth_update(&w, warm, 0.0f, true, 1.0f);
    CHECK(w.heat > 45.0f && warmth_speed_scale(&w) == 1.0f);
    // Mojarse enfria: la misma temperatura se siente peor.
    CHECK(hazard_feels_like(2.0f, 0.2f, 1.0f, 10.0f, 0.0f) < hazard_feels_like(2.0f, 0.2f, 0.0f, 10.0f, 0.0f) - 9.0f);
    Warmth rain = { 80.0f, 0.0f };
    for (int i = 0; i < 60; i++) warmth_update(&rain, 15.0f, 1.0f, false, 1.0f);
    CHECK(rain.wet > 0.9f);
    for (int i = 0; i < 30; i++) warmth_update(&rain, 15.0f, 0.0f, true, 1.0f);
    CHECK(rain.wet < 0.1f); // el fuego seca
    CHECK(!strcmp(cold_name(COLD_COLD), "frío"));

    // Barro: frena con suelo mojado, no bajo la nieve.
    CHECK(hazard_mud_scale(0.0f, 0.0f) == 1.0f);
    CHECK(hazard_mud_scale(1.0f, 0.0f) < 0.65f && hazard_mud_scale(1.0f, 1.0f) == 1.0f);

    // Hielo: no aguanta si no esta congelado; correr y montar rompen mas.
    CHECK(!hazard_ice_walkable(0.3f) && hazard_ice_break_chance(0.3f, 0.0f, 1.0f) == 1.0f);
    float walk = hazard_ice_break_chance(1.0f, 4.0f, 1.0f), run = hazard_ice_break_chance(1.0f, 7.5f, 1.0f);
    float ride = hazard_ice_break_chance(1.0f, 4.0f, 2.5f), thin = hazard_ice_break_chance(0.55f, 4.0f, 1.0f);
    CHECK(walk > 0.0f && walk < 0.01f && run > walk * 2.0f && ride > walk * 2.0f && thin > walk * 3.0f);
    CHECK(hazard_ice_break_chance(1.0f, 0.0f, 1.0f) < walk); // quieto, menos riesgo
}

static void test_hazards_sinkholes_and_desert(void) {
    const uint32_t seed = 1206u;
    // El desierto nunca toca el campamento, pero existe lejos.
    CHECK(biome_desert(seed, 0, 0) == 0.0f && biome_desert(seed, 150.0f, 100.0f) == 0.0f);
    int desert = 0;
    for (float x = -2000; x <= 2000; x += 80)
        for (float z = -2000; z <= 2000; z += 80) desert += biome_desert(seed, x, z) > 0.6f;
    CHECK(desert > 20);
    // Socavones: deterministas, dentro de su celda, y cambian de un dia al siguiente.
    int found = 0, moved = 0;
    for (int cx = -10; cx < 10; cx++)
        for (int cz = -10; cz < 10; cz++) {
            Sinkhole a, b, c;
            bool ha = hazard_sinkhole_cell(seed, 3, cx, cz, &a), hb = hazard_sinkhole_cell(seed, 3, cx, cz, &b);
            CHECK(ha == hb);
            if (!ha) continue;
            found++;
            CHECK(a.x == b.x && a.z == b.z && a.radius >= 1.8f && a.radius <= 3.2f);
            CHECK(a.x > cx * SINK_CELL && a.x < (cx + 1) * SINK_CELL && a.z > cz * SINK_CELL && a.z < (cz + 1) * SINK_CELL);
            if (!hazard_sinkhole_cell(seed, 4, cx, cz, &c) || fabsf(c.x - a.x) > 0.5f) moved++;
        }
    CHECK(found > 400 * SINK_CHANCE * 0.6f && found < 400 * SINK_CHANCE * 1.4f);
    CHECK(moved > found * 3 / 4);
    CHECK(!strcmp(sink_name(SINK_QUICKSAND), "arena movediza"));
}

static void test_hazards_qte(void) {
    Rng rng;
    rng_seed(&rng, 77u);
    Qte q;
    qte_start(&q, &rng, 7, 1.2f, 2);
    CHECK(q.state == QTE_RUNNING && q.len == 7);
    for (int i = 1; i < q.len; i++) CHECK(q.keys[i] != q.keys[i - 1] && q.keys[i] >= 0 && q.keys[i] < QTE_KEYS);
    // Acertar todo a tiempo gana.
    for (int i = 0; i < 7; i++) {
        qte_update(&q, 0.5f);
        CHECK(qte_press(&q, q.keys[q.pos]));
    }
    CHECK(q.state == QTE_WON && qte_progress(&q) == 1.0f && q.per_key < 1.2f);
    // Errores y demoras: con mas de max_mistakes se pierde.
    qte_start(&q, &rng, 7, 1.2f, 2);
    CHECK(!qte_press(&q, (q.keys[0] + 1) % QTE_KEYS) && q.mistakes == 1 && q.pos == 0);
    qte_update(&q, 1.3f); // se acabo el tiempo de la tecla
    CHECK(q.mistakes == 2 && q.state == QTE_RUNNING);
    qte_press(&q, (q.keys[0] + 1) % QTE_KEYS);
    CHECK(q.state == QTE_LOST && !qte_press(&q, q.keys[q.pos]));
}

static bool is_limb_part(int p) { return part_is_arm((BodyPart)p) || part_is_leg((BodyPart)p); }

static void test_health_wounds_bleeding_healing(void) {
    Rng rng;
    rng_seed(&rng, 42u);
    Health h;
    health_init(&h, 100.0f);
    CHECK(!strcmp(health_state_name(&h), "sano") && health_speed_scale(&h) == 1.0f);
    // Un corte serio en el muslo: sangra (femoral), frena y resta vida segun la zona.
    int w = health_hit(&h, &rng, 30.0f, WOUND_CUT, PART_THIGH_L);
    CHECK(w >= 0 && h.wounds[w].bleeding && fabsf(h.hp - (100.0f - 30.0f * part_damage_scale(PART_THIGH_L))) < 0.01f);
    CHECK(health_bleeding(&h) && health_speed_scale(&h) < 1.0f && health_attack_scale(&h) == 1.0f);
    char desc[96];
    wound_describe(&h.wounds[w], false, desc, sizeof(desc));
    CHECK(strstr(desc, "Corte en el muslo izquierdo") && strstr(desc, "sangra"));
    // Sin vendar, la sangre se va.
    float blood0 = h.blood;
    for (int i = 0; i < 60; i++) health_update(&h, &rng, 1.0f, false, 0.0f);
    CHECK(h.blood < blood0);
    // Vendar detiene el sangrado; en reposo la sangre y la vida vuelven y la herida cierra.
    CHECK(health_treat(&h) == 1 && !health_bleeding(&h) && h.wounds[w].treated);
    float blood1 = h.blood, hp1 = h.hp;
    for (int i = 0; i < 1200 && h.wound_count; i++) health_update(&h, &rng, 1.0f, true, 0.5f);
    CHECK(h.wound_count == 0 && h.blood > blood1 && h.hp > hp1);

    // Un golpe fuerte en el antebrazo lo rompe; la fractura no sana sin entablillar.
    Health f;
    health_init(&f, 100.0f);
    int fr = health_hit(&f, &rng, 60.0f, WOUND_BRUISE, PART_FOREARM_R);
    CHECK(f.wounds[fr].kind == WOUND_FRACTURE && health_attack_scale(&f) < 0.7f);
    float sev = f.wounds[fr].severity;
    for (int i = 0; i < 600; i++) health_update(&f, &rng, 1.0f, true, 0.0f);
    CHECK(f.wound_count == 1 && f.wounds[0].severity == sev);
    health_daily(&f, 0.0f);
    CHECK(f.wounds[0].severity == sev); // ni con un dia de descanso
    health_daily(&f, 0.5f);              // el curandero la entablilla
    CHECK(f.wounds[0].treated && f.wounds[0].severity < sev);

    // Zonas: el mismo golpe pesa distinto segun donde cae.
    CHECK(part_damage_scale(PART_NECK) > part_damage_scale(PART_HEAD));
    CHECK(part_damage_scale(PART_HEAD) > part_damage_scale(PART_THORAX));
    CHECK(part_damage_scale(PART_THORAX) > part_damage_scale(PART_THIGH_L));
    CHECK(part_damage_scale(PART_UPPER_ARM_L) > part_damage_scale(PART_FOREARM_L));
    CHECK(part_bleed_scale(PART_NECK) > part_bleed_scale(PART_FOREARM_R));
    Health hn, ha;
    health_init(&hn, 100.0f);
    health_init(&ha, 100.0f);
    health_hit(&hn, &rng, 20.0f, WOUND_CUT, PART_NECK);
    health_hit(&ha, &rng, 20.0f, WOUND_CUT, PART_FOREARM_L);
    CHECK(hn.hp < ha.hp - 25.0f && hn.wounds[0].severity > ha.wounds[0].severity);
    for (int i = 0; i < 20; i++) health_update(&hn, &rng, 1.0f, false, 0.0f), health_update(&ha, &rng, 1.0f, false, 0.0f);
    CHECK(hn.blood < ha.blood); // el cuello sangra mucho mas
    CHECK(part_is_arm(PART_FOREARM_R) && part_is_leg(PART_SHIN_L) && !part_is_arm(PART_NECK));
    for (int i = 0; i < 20; i++) CHECK(is_limb_part(health_random_limb(&rng)));

    // La misma herida sin tratar empeora en vez de duplicarse.
    Health g;
    health_init(&g, 100.0f);
    health_hit(&g, &rng, 10.0f, WOUND_BITE, PART_THORAX);
    health_hit(&g, &rng, 10.0f, WOUND_BITE, PART_THORAX);
    CHECK(g.wound_count == 1 && g.wounds[0].severity > 0.18f);
    // Fieras: las mismas zonas con nombres de animal.
    Health b;
    health_init_beast(&b, 60.0f);
    int bw = health_hit(&b, &rng, 10.0f, WOUND_CUT, PART_SHIN_R);
    wound_describe(&b.wounds[bw], b.beast, desc, sizeof(desc));
    CHECK(strstr(desc, "pata trasera derecha") != NULL);

    // Abatido al quedarse sin vida; muerto si se desangra.
    Health d;
    health_init(&d, 50.0f);
    health_hit(&d, &rng, 50.0f, WOUND_CUT, PART_THORAX);
    CHECK(d.down && !d.dead && health_speed_scale(&d) == 0.0f && !strcmp(health_state_name(&d), "abatido"));
    for (int i = 0; i < 2000 && !d.dead; i++) health_update(&d, &rng, 1.0f, false, 0.0f);
    CHECK(d.dead && !strcmp(health_state_name(&d), "muerto"));
    // Revivir a un abatido que no murio.
    Health r;
    health_init(&r, 100.0f);
    health_hit(&r, &rng, 90.0f, WOUND_BRUISE, PART_THORAX);
    CHECK(r.down && !r.dead);
    health_revive(&r);
    CHECK(!r.down && r.hp > 25.0f);
    // Las zonas al azar caen sobre todo en el tronco.
    int trunk = 0;
    for (int i = 0; i < 400; i++) {
        Health x;
        health_init(&x, 100.0f);
        int k = health_hit(&x, &rng, 5.0f, WOUND_BRUISE, PART_RANDOM);
        BodyPart pp = x.wounds[k].part;
        trunk += pp == PART_THORAX || pp == PART_ABDOMEN || pp == PART_PELVIS;
    }
    CHECK(trunk > 140 && trunk < 240);
}

static void test_troop_health_daily(void) {
    Kingdom k;
    kingdom_init_iron_khanate(&k);
    Troop t;
    troop_init(&t, &k);
    int a = troop_recruit(&t, "Herido", 0);
    int healer = troop_recruit(&t, "Curandera", 0);
    Rng rng;
    rng_seed(&rng, 9u);
    CHECK(troop_healer_skill(&t) == 0.0f);
    troop_assign_role(&t, healer, ROLE_HEALER);
    CHECK(troop_healer_skill(&t) == 0.5f);
    Member *m = troop_find(&t, a);
    CHECK(m->health.hp_max == 100.0f && !m->health.down);
    health_hit(&m->health, &rng, 40.0f, WOUND_CUT, PART_UPPER_ARM_L);
    CHECK(health_bleeding(&m->health));
    troop_process_day(&t, &rng);
    m = troop_find(&t, a);
    CHECK(!health_bleeding(&m->health) && m->health.hp > 70.0f); // el curandero lo vendo
    // Un gran guerrero aguanta mas.
    Champion c;
    champion_generate(&c, &rng);
    int ch = troop_add_champion(&t, &c, STATUS_ACTIVE);
    CHECK(fabsf(troop_find(&t, ch)->health.hp_max - 100.0f * c.stats.size * c.stats.endurance) < 0.01f);
}

static void test_combat_weapons_and_enemies(void) {
    WeaponStats fist = weapon_stats(""), sable = weapon_stats("arma.corta.sable");
    WeaponStats steel = weapon_stats("arma.corta.sable_acero"), spear = weapon_stats("arma.larga.lanza");
    WeaponStats bow = weapon_stats("arma.distancia.arco_compuesto"), mace = weapon_stats("arma.corta.maza");
    CHECK(fist.wound == WOUND_BRUISE && fist.damage < sable.damage && steel.damage > sable.damage);
    CHECK(spear.spear && spear.reach > sable.reach && mace.wound == WOUND_BRUISE && bow.damage == fist.damage);
    CHECK(weapon_stats("utileria.objeto.antorcha").damage == fist.damage);
    // Enemigos: todos con modelo del inventario; los fanaticos no huyen (las fieras son fauna: src/sim/animals.h).
    for (int k = 0; k < ENEMY_COUNT; k++) {
        const EnemyDef *d = enemy_def((EnemyKind)k);
        CHECK(d->hp > 0 && d->damage > 0 && d->model && d->model[0]);
    }
    CHECK(enemy_def(ENEMY_FANATIC)->flee_at == 0.0f && !enemy_def(ENEMY_BANDIT)->beast);
    // Escudo: solo de frente.
    CHECK(combat_block_chance(true, 0.9f) > 0.5f && combat_block_chance(true, -0.5f) == 0.0f);
    CHECK(combat_block_chance(false, 1.0f) == 0.0f);
    Rng rng;
    rng_seed(&rng, 5u);
    for (int i = 0; i < 50; i++) {
        float d = combat_damage(20.0f, 1.0f, 1.0f, false, &rng);
        CHECK(d >= 16.0f && d <= 24.0f);
        CHECK(combat_damage(20.0f, 1.0f, 1.0f, true, &rng) <= 24.0f * 0.15f);
        CHECK(combat_damage(20.0f, 1.0f, 0.5f, false, &rng) <= 12.0f); // brazo herido: golpea menos
    }
}

static void test_body_zones_raycast(void) {
    BodyPose b;
    BodyPoseParams pp = { .scale = 1.0f };
    body_pose(&b, &pp);
    // Un rayo horizontal de frente (desde +Z hacia -Z) a cada altura da en su zona.
    struct { float y, x; int part; } shots[] = {
        { 1.64f, 0.0f, PART_HEAD },        { 1.49f, 0.0f, PART_NECK },        { 1.30f, 0.0f, PART_THORAX },
        { 1.06f, 0.0f, PART_ABDOMEN },     { 0.90f, 0.0f, PART_PELVIS },      { 1.25f, 0.22f, PART_UPPER_ARM_L },
        { 0.95f, 0.24f, PART_FOREARM_L },  { 0.65f, 0.10f, PART_THIGH_L },    { 0.25f, -0.10f, PART_SHIN_R },
    };
    for (size_t i = 0; i < sizeof(shots) / sizeof(shots[0]); i++) {
        float t = 0.0f;
        int hit = body_raycast(&b, (V3){ shots[i].x, shots[i].y, 5.0f }, (V3){ 0, 0, -10.0f }, 1.0f, &t);
        CHECK(hit == shots[i].part);
        CHECK(t > 0.4f && t < 0.5f); // el cuerpo esta en z ~ 0
    }
    // Por encima de la cabeza o al costado no da en nada.
    CHECK(body_raycast(&b, (V3){ 0, 2.0f, 5.0f }, (V3){ 0, 0, -10.0f }, 1.0f, NULL) == -1);
    CHECK(body_raycast(&b, (V3){ 1.0f, 1.3f, 5.0f }, (V3){ 0, 0, -10.0f }, 1.0f, NULL) == -1);
    // Un rayo corto que no llega tampoco.
    CHECK(body_raycast(&b, (V3){ 0, 1.3f, 5.0f }, (V3){ 0, 0, -1.0f }, 1.0f, NULL) == -1);
    // Tendido: a la altura del pecho de pie no hay nada; a ras de suelo, si.
    BodyPoseParams down = { .down = true, .scale = 1.0f };
    BodyPose d;
    body_pose(&d, &down);
    CHECK(body_raycast(&d, (V3){ 0, 1.3f, 5.0f }, (V3){ 0, 0, -10.0f }, 1.0f, NULL) == -1);
    CHECK(body_raycast(&d, (V3){ 0, 3.0f, 0.0f }, (V3){ 0, -5.0f, 0 }, 1.0f, NULL) >= 0);
    // Andar mueve las piernas; golpear sube el brazo derecho.
    BodyPoseParams walk = { .walk = 1.0f, .walk_phase = 1.57f, .scale = 1.0f };
    BodyPose w;
    body_pose(&w, &walk);
    CHECK(w.seg[PART_SHIN_L].b.z > 0.2f && w.seg[PART_SHIN_R].b.z < -0.1f);
    BodyPoseParams atk = { .attack = 1.0f, .scale = 1.0f };
    BodyPose a;
    body_pose(&a, &atk);
    CHECK(a.seg[PART_FOREARM_R].b.y > b.seg[PART_FOREARM_R].b.y + 0.5f);
    // Mundo <-> local: un punto delante del cuerpo queda en +Z local.
    V3 l = body_to_local((V3){ 10.0f + sinf(0.7f) * 2.0f, 1.0f, 5.0f + cosf(0.7f) * 2.0f }, (V3){ 10, 0, 5 }, 0.7f);
    CHECK(fabsf(l.z - 2.0f) < 1e-3f && fabsf(l.x) < 1e-3f && fabsf(l.y - 1.0f) < 1e-3f);
    V3 back = body_to_world(l, (V3){ 10, 0, 5 }, 0.7f);
    CHECK(fabsf(back.x - (10.0f + sinf(0.7f) * 2.0f)) < 1e-3f);
}

static void test_armor_pieces_materials(void) {
    Rng rng;
    rng_seed(&rng, 3u);
    Armor a;
    memset(&a, 0, sizeof(a));
    CHECK(!armor_equip(&a, "arma.corta.sable") && !armor_equip(&a, "armadura.montura.barda_cuero"));
    CHECK(armor_equip(&a, "armadura.casco.escamas_hierro") && a.slot[SLOT_HELMET].material == MAT_IRON);
    CHECK(armor_equip(&a, "armadura.torso.fieltro") && a.slot[SLOT_TORSO].material == MAT_FELT);
    CHECK(armor_equip(&a, "armadura.grebas.culto") && a.slot[SLOT_GREAVES].material == MAT_BRONZE);
    CHECK(armor_equip(&a, "armadura.cuello.malla") && a.slot[SLOT_NECK].mail);
    // Solo protege las zonas que cubre.
    CHECK(armor_protection(&a, PART_HEAD) > 0.4f && armor_protection(&a, PART_FOREARM_L) == 0.0f);
    CHECK(armor_protection(&a, PART_HEAD) > armor_protection(&a, PART_THORAX)); // hierro > fieltro
    // Un golpe en el casco de hierro: llega mucho menos, y el corte llega como golpe.
    float total = 0.0f;
    int bruised = 0;
    for (int i = 0; i < 100; i++) {
        Armor fresh;
        memset(&fresh, 0, sizeof(fresh));
        armor_equip(&fresh, "armadura.casco.escamas_hierro");
        WoundKind k = WOUND_CUT;
        total += armor_absorb(&fresh, PART_HEAD, &k, false, 20.0f, &rng, NULL);
        bruised += k == WOUND_BRUISE;
    }
    CHECK(total / 100.0f < 11.0f && bruised > 60);
    // Sin pieza en la zona: pasa todo.
    WoundKind k = WOUND_CUT;
    CHECK(armor_absorb(&a, PART_FOREARM_R, &k, false, 20.0f, &rng, NULL) == 20.0f && k == WOUND_CUT);
    // Durabilidad: los golpes la gastan y al final se rompe; rota no protege.
    Armor w;
    memset(&w, 0, sizeof(w));
    armor_equip(&w, "armadura.casco.fieltro");
    bool broke = false;
    for (int i = 0; i < 200 && !broke; i++) {
        WoundKind kk = WOUND_CUT;
        armor_absorb(&w, PART_HEAD, &kk, false, 25.0f, &rng, &broke);
    }
    CHECK(broke && w.slot[SLOT_HELMET].durability == 0.0f && armor_protection(&w, PART_HEAD) == 0.0f);
    armor_repair(&w, 0.5f);
    CHECK(w.slot[SLOT_HELMET].durability == 30.0f && armor_protection(&w, PART_HEAD) > 0.0f);
    // El hierro dura mas que el fieltro con los mismos golpes.
    Armor fe, fl;
    memset(&fe, 0, sizeof(fe));
    memset(&fl, 0, sizeof(fl));
    armor_equip(&fe, "armadura.torso.escamas_hierro");
    armor_equip(&fl, "armadura.torso.fieltro");
    for (int i = 0; i < 6; i++) {
        WoundKind k1 = WOUND_CUT, k2 = WOUND_CUT;
        rng_seed(&rng, 100u + (unsigned)i);
        armor_absorb(&fe, PART_THORAX, &k1, false, 20.0f, &rng, NULL);
        rng_seed(&rng, 100u + (unsigned)i);
        armor_absorb(&fl, PART_THORAX, &k2, false, 20.0f, &rng, NULL);
    }
    CHECK(fe.slot[SLOT_TORSO].durability / fe.slot[SLOT_TORSO].durability_max >
          fl.slot[SLOT_TORSO].durability / fl.slot[SLOT_TORSO].durability_max);
    // La malla para menos las flechas que el filo.
    CHECK(armor_speed_scale(&a) < 1.0f && armor_speed_scale(&a) > 0.6f);
    // Impacto completo con armadura: menos vida perdida que sin ella.
    Health h1, h2;
    health_init(&h1, 100.0f);
    health_init(&h2, 100.0f);
    Armor full;
    memset(&full, 0, sizeof(full));
    armor_equip(&full, "armadura.torso.escamas_hierro");
    float absorbed = 0.0f;
    float lost1 = 0.0f, lost2 = 0.0f;
    for (int i = 0; i < 20; i++) {
        float hp1 = h1.hp, hp2 = h2.hp;
        combat_apply_hit(&h1, &full, &rng, 10.0f, WOUND_CUT, PART_THORAX, false, &absorbed, NULL);
        combat_apply_hit(&h2, NULL, &rng, 10.0f, WOUND_CUT, PART_THORAX, false, NULL, NULL);
        lost1 += hp1 - h1.hp, lost2 += hp2 - h2.hp;
        armor_repair(&full, 1.0f);
    }
    CHECK(lost1 < lost2 * 0.7f);
}

static void test_ballistics_trajectories(void) {
    const RangedDef *bow = ranged_def("arma.distancia.arco_compuesto");
    const RangedDef *xbow = ranged_def("arma.distancia.ballesta");
    const RangedDef *sling = ranged_def("arma.distancia.honda");
    CHECK(bow && xbow && sling && !ranged_def("arma.corta.sable") && !ranged_def(""));
    // v = sqrt(2E/m): mas potencia, mas rapido; mas masa, mas lento.
    float vb = ranged_muzzle_speed(bow, 1.0f), vx = ranged_muzzle_speed(xbow, 1.0f), vs = ranged_muzzle_speed(sling, 1.0f);
    CHECK(fabsf(vb - sqrtf(2.0f * 75.0f / 0.030f)) < 0.01f);
    CHECK(vx < vb && vs < vx);                                   // el virote pesa el doble
    CHECK(ranged_muzzle_speed(bow, 0.3f) < vb * 0.6f);            // poco tenso, poca velocidad
    CHECK(ranged_muzzle_speed(xbow, 0.0f) == vx);                 // la ballesta no se tensa a mano
    // Curva: un tiro horizontal cae con la distancia, y mas cuanto mas lento.
    V3 pts[400];
    int n = ballistic_trace(PROJ_ARROW, (V3){ 0, 1.5f, 0 }, 0.0f, 0.0f, vb, 0.02f, -50.0f, pts, 400);
    CHECK(n > 10);
    float drop30 = 0.0f, drop60 = 0.0f;
    for (int i = 1; i < n; i++) {
        if (pts[i - 1].z < 30.0f && pts[i].z >= 30.0f) drop30 = 1.5f - pts[i].y;
        if (pts[i - 1].z < 60.0f && pts[i].z >= 60.0f) drop60 = 1.5f - pts[i].y;
    }
    CHECK(drop30 > 0.5f && drop60 > drop30 * 3.0f);
    // La resistencia del aire frena: llega con menos energia de la que salio.
    Projectile p;
    projectile_launch(&p, PROJ_ARROW, (V3){ 0, 1.5f, 0 }, 0.0f, 0.05f, vb);
    float e0 = projectile_energy(&p), d0 = projectile_damage(&p);
    for (int i = 0; i < 100; i++) projectile_step(&p, 0.01f);
    CHECK(projectile_energy(&p) < e0 && projectile_damage(&p) < d0);
    CHECK(d0 > 20.0f && d0 < 35.0f); // una flecha tensa hiere como un sable
    // El mosquete pega mucho mas fuerte.
    Projectile m;
    projectile_launch(&m, PROJ_BALL, (V3){ 0, 0, 0 }, 0.0f, 0.0f, ranged_muzzle_speed(ranged_def("arma.distancia.mosquete"), 1));
    CHECK(projectile_damage(&m) > 100.0f);
    // A igual velocidad, el proyectil pesado conserva mas su velocidad.
    Projectile light, heavy;
    projectile_launch(&light, PROJ_ARROW, (V3){ 0, 0, 0 }, 0.0f, 0.0f, 60.0f);
    projectile_launch(&heavy, PROJ_BOLT, (V3){ 0, 0, 0 }, 0.0f, 0.0f, 60.0f);
    for (int i = 0; i < 100; i++) projectile_step(&light, 0.01f), projectile_step(&heavy, 0.01f);
    CHECK(heavy.vel.z > light.vel.z);
    // Puntería balistica: el angulo resuelto da en el blanco.
    V3 from = { 0, 1.5f, 0 }, target = { 20.0f, 1.2f, 30.0f };
    float pitch = 0.0f;
    CHECK(ballistic_solve(PROJ_ARROW, from, target, vb, &pitch));
    float yaw = atan2f(target.x - from.x, target.z - from.z);
    Projectile s;
    projectile_launch(&s, PROJ_ARROW, from, yaw, pitch, vb);
    float best = 1e9f, dist_t = sqrtf(20.0f * 20.0f + 30.0f * 30.0f);
    for (int i = 0; i < 400; i++) {
        projectile_step(&s, 0.005f);
        float dd = sqrtf(s.pos.x * s.pos.x + s.pos.z * s.pos.z);
        if (fabsf(dd - dist_t) < 0.5f) best = fminf(best, fabsf(s.pos.y - target.y));
    }
    CHECK(best < 0.3f);
    // Fuera de alcance no hay solucion.
    CHECK(!ballistic_solve(PROJ_STONE, from, (V3){ 0, 0, 2000.0f }, vs, &pitch));
}

static void test_memmap_markers_toggle(void) {
    MemoryMap m;
    memmap_init(&m);
    CHECK(memmap_toggle_marker(&m, 10.0f, 5.0f, MARKER_INTEREST, 6.0f));
    CHECK(m.marker_count == 1);
    CHECK(!memmap_toggle_marker(&m, 12.0f, 6.0f, MARKER_INTEREST, 6.0f)); // cerca: la quita
    CHECK(m.marker_count == 0);
    for (int i = 0; i < MEMMAP_MAX_MARKERS; i++) memmap_toggle_marker(&m, i * 50.0f, 0, MARKER_DANGER, 6.0f);
    CHECK(m.marker_count == MEMMAP_MAX_MARKERS);
    CHECK(!memmap_toggle_marker(&m, 0, 900.0f, MARKER_DANGER, 6.0f)); // lleno
    memmap_free(&m);
}

static void test_memmap_covers_whole_world_sparsely(void) {
    MemoryMap m;
    memmap_init(&m);
    float now = 0.0f;
    // Muy lejos del origen y en coordenadas negativas: funciona igual.
    walk(&m, 50000.0f, -80000.0f, 2.0f, &now);
    walk(&m, -123456.0f, 98765.0f, 2.0f, &now);
    CHECK(memmap_light(&m, 50000.0f, -80000.0f, now) > 0.0f);
    CHECK(memmap_light(&m, -123456.0f, 98765.0f, now) > 0.0f);
    CHECK(memmap_light(&m, 0, 0, now) == 0.0f);
    // Solo se guardan las zonas visitadas (como mucho 4 paginas por visita).
    CHECK(m.page_count >= 2 && m.page_count <= 8);
    // Un recorrido largo agranda la tabla sin perder lo aprendido.
    for (int i = 0; i < 200; i++) walk(&m, i * 130.0f, 0, 0.2f, &now);
    CHECK(m.page_count > 64);
    CHECK(memmap_light(&m, 50000.0f, -80000.0f, now) > 0.0f);
    memmap_free(&m);
}

// ---------------------------------------------------------------- grandes guerreros
static void test_champions_are_scarce(void) {
    Rng r;
    rng_seed(&r, 99);
    int hits = 0;
    for (int i = 0; i < 20000; i++) hits += champion_appears(&r, CHAMPION_DEFAULT_CHANCE);
    CHECK(hits > 20000 * 0.03f && hits < 20000 * 0.05f); // ~4 %: escasos, pero sin limite
}

static void test_champion_generation_is_deterministic(void) {
    Rng a, b;
    rng_seed(&a, 7);
    rng_seed(&b, 7);
    Champion ca, cb;
    champion_generate(&ca, &a);
    champion_generate(&cb, &b);
    CHECK(memcmp(&ca, &cb, sizeof(ca)) == 0);
}

static void test_champion_gifts_shape_stats(void) {
    Rng r;
    rng_seed(&r, 2024);
    unsigned seen_gifts = 0;
    int origins[8] = { 0 }, aspirations[8] = { 0 };
    for (int i = 0; i < 400; i++) {
        Champion c;
        champion_generate(&c, &r);
        int n = champion_gift_count(&c);
        CHECK(n >= 1 && n <= 3);
        CHECK(c.name[0] && c.epithet[0]);
        CHECK(c.traits != 0); // la aspiracion fija un rasgo de tropa
        seen_gifts |= c.gifts;
        origins[c.origin]++;
        aspirations[c.aspiration]++;
        if (c.gifts & GIFT_TALL) CHECK(c.stats.size >= 1.2f);
        else CHECK(c.stats.size == 1.0f);
        if (c.gifts & GIFT_SWIFT) CHECK(c.stats.speed > 1.1f);
        if (c.gifts & GIFT_STRONG) CHECK(c.stats.strength >= 1.3f);
        if (c.gifts & GIFT_ENDURING) CHECK(c.stats.endurance >= 1.3f);
        if (c.gifts & GIFT_MARKSMAN) CHECK(c.stats.aim >= 1.3f);
        if (c.gifts & GIFT_RIDER) CHECK(c.stats.riding >= 1.3f);
        CHECK((c.gifts & GIFT_HEALER) ? c.stats.healing > 0.0f : c.stats.healing == 0.0f);
        CHECK((c.gifts & GIFT_WEAPON) ? champion_weapon_name(c.weapon)[0] != 0 : c.weapon == -1);
        char story[256];
        int len = champion_story(&c, story, sizeof(story));
        CHECK(len > 40 && len < (int)sizeof(story));
        CHECK(strncmp(story, "Nació ", 7) == 0);
    }
    CHECK(seen_gifts == (1u << GIFT_COUNT) - 1); // todos los dones aparecen
    for (int i = 0; i < 8; i++) CHECK(origins[i] > 0 && aspirations[i] > 0);
}

static void test_champion_in_troop(void) {
    Troop t;
    troop_init(&t, NULL);
    Rng r;
    rng_seed(&r, 5);
    Champion c;
    champion_generate(&c, &r);
    int a = troop_recruit(&t, "Comun", 0);
    int g = troop_add_champion(&t, &c, STATUS_ACTIVE);
    int p = troop_add_champion(&t, &c, STATUS_PRISONER);
    CHECK(g > 0 && p > 0);
    CHECK(troop_champion(&t, a) == NULL);
    CHECK(troop_champion(&t, g) != NULL && strcmp(troop_champion(&t, g)->epithet, c.epithet) == 0);
    CHECK(troop_find(&t, p)->status == STATUS_PRISONER);
    CHECK(troop_add_champion(&t, &c, STATUS_DESERTED) == -1);
}

static void test_champion_desertion_weighs_on_troop(void) {
    Troop t;
    troop_init(&t, NULL);
    Rng r;
    rng_seed(&r, 11);
    Champion c;
    champion_generate(&c, &r);
    int a = troop_recruit(&t, "Fiel", 0);
    int b = troop_recruit(&t, "Fiel 2", 0);
    int g = troop_add_champion(&t, &c, STATUS_ACTIVE);
    troop_find(&t, a)->morale = troop_find(&t, b)->morale = 80.0f;
    troop_find(&t, a)->loyalty = troop_find(&t, b)->loyalty = 80.0f;
    troop_find(&t, g)->morale = troop_find(&t, g)->loyalty = 0.0f; // a punto de irse
    DayReport rep = { 0 };
    for (int day = 0; day < 50 && !rep.champions_deserted; day++) rep = troop_process_day(&t, &r);
    CHECK(rep.champions_deserted == 1 && rep.deserted == 1);
    CHECK(troop_find(&t, g)->status == STATUS_DESERTED);
    CHECK(troop_find(&t, a)->morale == 75.0f); // su perdida pesa en los demas
}

// ---------------------------------------------------------------- acciones
static InvItem make_item(const char *id, InvHands hands) {
    InvItem it;
    memset(&it, 0, sizeof(it));
    snprintf(it.id, sizeof(it.id), "%s", id);
    it.hands = hands;
    return it;
}

static void test_hands_grips(void) {
    InvItem sable = make_item("arma.corta.sable", INV_HANDS_ONE), daga = make_item("arma.corta.daga", INV_HANDS_ONE);
    InvItem guja = make_item("arma.larga.guja", INV_HANDS_TWO), escudo = make_item("escudo.mano.mimbre", INV_HANDS_SHIELD);
    InvItem roca = make_item("mapa.roca.pequena", INV_HANDS_NONE);
    Hands h;
    hands_init(&h);
    CHECK(hands_grip(&h) == GRIP_EMPTY);
    CHECK(!hands_equip(&h, &roca, HAND_RIGHT)); // no se empuna
    CHECK(hands_equip(&h, &sable, HAND_RIGHT) && hands_grip(&h) == GRIP_ONE_HANDED);
    CHECK(hands_equip(&h, &daga, HAND_LEFT) && hands_grip(&h) == GRIP_DUAL);
    CHECK(hands_equip(&h, &escudo, HAND_RIGHT) && hands_grip(&h) == GRIP_WEAPON_SHIELD); // el escudo va a la izquierda
    CHECK(!strcmp(h.left.id, "escudo.mano.mimbre") && !strcmp(h.right.id, "arma.corta.sable"));
    CHECK(hands_equip(&h, &guja, HAND_RIGHT) && hands_grip(&h) == GRIP_TWO_HANDED);
    CHECK(h.left.id[0] == '\0'); // a dos manos suelta el escudo
    CHECK(hands_equip(&h, &escudo, HAND_LEFT) && hands_grip(&h) == GRIP_SHIELD); // y el escudo suelta la guja
    CHECK(h.right.id[0] == '\0');
}

static void test_hands_sheathe_take_throw(void) {
    InvItem sable = make_item("arma.corta.sable", INV_HANDS_ONE), escudo = make_item("escudo.mano.mimbre", INV_HANDS_SHIELD);
    Hands h;
    hands_init(&h);
    CHECK(!hands_toggle_sheathe(&h)); // nada que enfundar
    hands_equip(&h, &sable, HAND_RIGHT);
    hands_equip(&h, &escudo, HAND_LEFT);
    CHECK(hands_holding(&h, "arma.corta.sable"));
    CHECK(!hands_can_take(&h)); // ambas manos ocupadas
    CHECK(!hands_take(&h, "utileria.objeto.cofre"));
    CHECK(hands_toggle_sheathe(&h) && h.sheathed);
    CHECK(!hands_holding(&h, "arma.corta.sable")); // enfundada
    CHECK(hands_take(&h, "utileria.objeto.antorcha"));
    CHECK(!hands_take(&h, "utileria.objeto.cofre")); // ya lleva algo
    char thrown[INV_ID_LEN];
    CHECK(hands_throw(&h, thrown, sizeof(thrown)) && !strcmp(thrown, "utileria.objeto.antorcha"));
    CHECK(!hands_throw(&h, thrown, sizeof(thrown)));
    CHECK(hands_toggle_sheathe(&h) && !h.sheathed);
    hands_equip(&h, &sable, HAND_RIGHT);
    hands_clear(&h, HAND_LEFT);
    CHECK(hands_can_take(&h)); // la izquierda quedo libre
}

static void test_action_catalog(void) {
    for (int a = 0; a < ACTION_COUNT; a++) {
        const ActionDef *d = action_def((ActionId)a);
        CHECK(d && d->name && d->seconds > 0.0f);
        CHECK(d->actors & ACTOR_PLAYER);
        CHECK(d->actors & ACTOR_NPC); // todas sirven tambien a los NPCs
    }
    CHECK(action_def(ACTION_COUNT) == NULL);
    CHECK(!strcmp(action_def(ACTION_THROW_LASSO)->requires, "arma.distancia.lazo"));
    for (int b = 0; b < BUILD_COUNT; b++) {
        const BuildDef *d = build_def((BuildId)b);
        CHECK(d && d->work > 0.0f && d->min_workers >= 2 && d->max_workers >= d->min_workers); // siempre en grupo
    }
}

static int add_worker(Troop *t, Role role, float morale) {
    int id = troop_recruit(t, "Obrero", 0);
    troop_assign_role(t, id, role);
    troop_find(t, id)->morale = morale;
    return id;
}

static void test_build_needs_crew_and_skills(void) {
    Troop t;
    troop_init(&t, NULL);
    const BuildDef *oven = build_def(BUILD_OVEN), *furnace = build_def(BUILD_FURNACE_BRONZE);
    add_worker(&t, ROLE_NONE, 60.0f);
    CrewPlan p = build_plan(oven, &t, false);
    CHECK(p.check == BUILD_FEW_WORKERS && p.rate == 0.0f); // una persona no basta
    p = build_plan(oven, &t, true);                         // el jugador ayuda
    CHECK(p.check == BUILD_READY && p.workers == 2 && p.rate == 2.0f);
    add_worker(&t, ROLE_NONE, 60.0f);
    add_worker(&t, ROLE_NONE, 60.0f);
    p = build_plan(furnace, &t, true);
    CHECK(p.check == BUILD_MISSING_ROLE && p.rate == 0.0f); // sin herrero no hay fundicion
    add_worker(&t, ROLE_SMITH, 60.0f);
    p = build_plan(furnace, &t, false);
    CHECK(p.check == BUILD_READY && p.workers == 4 && p.rate == 5.0f); // el herrero trabaja el doble
    // La habilidad cuenta: un cocinero acelera el horno de cocina.
    float before = build_plan(oven, &t, false).rate;
    add_worker(&t, ROLE_COOK, 60.0f);
    CHECK(build_plan(oven, &t, false).rate > before);
    // Tope de trabajadores: mas gente no acelera.
    for (int i = 0; i < 10; i++) add_worker(&t, ROLE_NONE, 60.0f);
    CHECK(build_plan(oven, &t, false).workers == oven->max_workers);
}

static void test_build_skill_morale_and_champions(void) {
    Troop t;
    troop_init(&t, NULL);
    const BuildDef *wall = build_def(BUILD_PALISADE);
    int sad = add_worker(&t, ROLE_NONE, 10.0f);
    CHECK(build_worker_skill(wall, &t, troop_find(&t, sad)) == 0.5f); // desganado
    int builder = add_worker(&t, ROLE_BUILDER, 60.0f);
    CHECK(build_worker_skill(wall, &t, troop_find(&t, builder)) == 2.0f);
    Champion c;
    memset(&c, 0, sizeof(c));
    snprintf(c.name, sizeof(c.name), "Fuerte");
    c.gifts = GIFT_STRONG;
    c.weapon = -1;
    int g = troop_add_champion(&t, &c, STATUS_ACTIVE);
    troop_find(&t, g)->morale = 60.0f;
    CHECK(build_worker_skill(wall, &t, troop_find(&t, g)) == 1.5f);
    int p = troop_add_champion(&t, &c, STATUS_PRISONER);
    (void)p;
    CHECK(build_plan(wall, &t, false).workers == 3); // los prisioneros no trabajan
}

static void test_build_advance(void) {
    BuildProject p = { BUILD_BONFIRE, 0, 0, 0.0f, false };
    float work = build_def(BUILD_BONFIRE)->work;
    CHECK(!build_advance(&p, 0.0f, 10.0f) && p.progress == 0.0f); // sin cuadrilla no avanza
    CHECK(!build_advance(&p, 2.0f, work / 4.0f) && p.progress > 0.49f && p.progress < 0.51f);
    CHECK(build_advance(&p, 2.0f, work / 4.0f) && p.done && p.progress == 1.0f);
    CHECK(!build_advance(&p, 2.0f, 1.0f)); // ya terminada
}

// ---------------------------------------------------------------- economia
static void test_stockpile(void) {
    Stockpile s;
    stock_init(&s);
    CHECK(stock_count(&s, "utileria.material.piedra") == 0);
    CHECK(stock_add(&s, "utileria.material.piedra", 5) && stock_add(&s, "utileria.material.piedra", 2));
    CHECK(stock_count(&s, "utileria.material.piedra") == 7);
    CHECK(!stock_take(&s, "utileria.material.piedra", 8) && stock_count(&s, "utileria.material.piedra") == 7);
    const Ingredient need[] = { { "utileria.material.piedra", 4 }, { "utileria.material.barro", 2 }, { NULL, 0 } };
    CHECK(!stock_has_all(&s, need));
    CHECK(!strcmp(stock_first_missing(&s, need)->id, "utileria.material.barro"));
    CHECK(!stock_take_all(&s, need) && stock_count(&s, "utileria.material.piedra") == 7); // todo o nada
    stock_add(&s, "utileria.material.barro", 2);
    CHECK(stock_take_all(&s, need) && stock_count(&s, "utileria.material.piedra") == 3 &&
          stock_count(&s, "utileria.material.barro") == 0);
}

static void test_daily_upkeep_food_and_gathering(void) {
    Troop t;
    troop_init(&t, NULL);
    Stockpile s;
    stock_init(&s);
    for (int i = 0; i < 3; i++) troop_recruit(&t, "Comun", 0);
    int hunter = troop_recruit(&t, "Cazador", 0);
    troop_assign_role(&t, hunter, ROLE_HUNTER);
    // Sin comida: el cazador trae 3, comen 4 -> uno se queda sin racion.
    float before = troop_avg_morale(&t);
    UpkeepReport r = economy_daily_upkeep(&s, &t);
    CHECK(r.eaten == 3 && r.hungry == 1);
    CHECK(troop_avg_morale(&t) < before); // el hambre baja la moral
    CHECK(stock_count(&s, "utileria.objeto.lena") == 3); // los que no tienen funcion recolectan
    CHECK(stock_count(&s, "utileria.material.pieles") == 1);
    // Con cocinero se come menos.
    stock_add(&s, FOOD_ID, 20);
    int cook = troop_recruit(&t, "Cocinero", 0);
    troop_assign_role(&t, cook, ROLE_COOK);
    int food = stock_count(&s, FOOD_ID);
    r = economy_daily_upkeep(&s, &t);
    CHECK(r.hungry == 0 && r.eaten == 5 - 5 / 3);
    CHECK(stock_count(&s, FOOD_ID) == food + 3 - r.eaten);
    // La carne fresca y la leche se comen antes; lo fresco que sobra se seca o se cuaja.
    int dried = stock_count(&s, FOOD_ID), cheese = stock_count(&s, "utileria.consumible.queso_seco");
    stock_add(&s, FRESH_MEAT_ID, 6);
    stock_add(&s, MILK_ID, 4);
    r = economy_daily_upkeep(&s, &t); // comen 4: carne fresca; sobran 2 de carne y 4 de leche
    CHECK(r.eaten == 4 && stock_count(&s, FRESH_MEAT_ID) == 0 && stock_count(&s, MILK_ID) == 0);
    CHECK(stock_count(&s, FOOD_ID) == dried + 3 + 1);
    CHECK(stock_count(&s, "utileria.consumible.queso_seco") == cheese + 2);
}

static void test_camp_effects(void) {
    const char *none[] = { "estructura.vivienda.yurta_comun" };
    CampEffects e = camp_effects(none, 1);
    CHECK(e.morale_per_day == 0.0f && e.rebellion_scale == 1.0f && e.shelters == 4 && e.reveal_radius == 0.0f);
    const char *all[] = { "estructura.campamento.hoguera", "estructura.campamento.horno_cocina",
                          "totem.proteccion.guardian", "estructura.campamento.atalaya",
                          "estructura.campamento.fogata", "estructura.campamento.fogata", "estructura.campamento.fogata",
                          "estructura.campamento.refugio" };
    e = camp_effects(all, 8);
    CHECK(e.morale_per_day == 3.0f + 2.0f + 2.0f); // hoguera + horno + fogatas (tope 2)
    CHECK(e.rebellion_scale == 0.5f && e.reveal_radius > 0.0f && e.shelters == 1);
    // El totem baja el riesgo de rebelion de la tropa.
    Troop t;
    troop_init(&t, NULL);
    for (int i = 0; i < 4; i++) troop_find(&t, troop_recruit(&t, "Triste", TRAIT_AMBITIOUS))->morale = 10.0f;
    float p = troop_rebellion_chance(&t);
    t.rebellion_scale = e.rebellion_scale;
    CHECK(p > 0.0f && troop_rebellion_chance(&t) < p);
}

static void test_crafting(void) {
    Troop t;
    troop_init(&t, NULL);
    const CraftDef *c = craft_def(CRAFT_SABLE_BRONZE);
    CHECK(craft_seconds(c, &t) < 0.0f); // sin herrero no se forja
    troop_assign_role(&t, troop_recruit(&t, "Herrero", 0), ROLE_SMITH);
    CHECK(craft_seconds(c, &t) == c->work);
    troop_assign_role(&t, troop_recruit(&t, "Herrero 2", 0), ROLE_SMITH);
    CHECK(craft_seconds(c, &t) == c->work / 2.0f);
    for (int i = 0; i < CRAFT_COUNT; i++) CHECK(craft_def((CraftId)i)->mats[0].id != NULL);
}

static void test_build_materials_and_crew_presence(void) {
    for (int b = 0; b < BUILD_COUNT; b++) CHECK(build_def((BuildId)b)->mats[0].id != NULL); // toda obra cuesta algo
    Troop t;
    troop_init(&t, NULL);
    int a = add_worker(&t, ROLE_NONE, 60.0f), b = add_worker(&t, ROLE_BUILDER, 60.0f);
    add_worker(&t, ROLE_NONE, 60.0f);
    const BuildDef *wall = build_def(BUILD_PALISADE);
    CrewPlan plan = build_plan(wall, &t, false);
    CHECK(plan.check == BUILD_READY && plan.workers == 3 && plan.ids[0] == b); // el constructor primero
    bool present[TROOP_MAX + 1] = { true, true, false };
    CHECK(build_rate_present(wall, &plan, present) == 0.0f); // faltan manos en la obra: espera
    present[2] = true;
    CHECK(build_rate_present(wall, &plan, present) == plan.rate);
    // Un integrante ocupado en otra obra no cuenta.
    int busy[] = { a };
    CHECK(build_plan_excluding(wall, &t, false, busy, 1).check == BUILD_FEW_WORKERS);
    CHECK(build_plan_excluding(wall, &t, true, busy, 1).ids[2] == -1); // el jugador completa la cuadrilla
}

// ---------------------------------------------------------------- animales
static float an_dist(const Animal *a, float x, float z) { return sqrtf((a->x - x) * (a->x - x) + (a->z - z) * (a->z - z)); }

// Las clases: quien se monta, quien se doma peleando, quien nunca, quien se caza, quien se pastorea.
static void test_animals_classes(void) {
    const Species mounts[] = { SPECIES_HORSE, SPECIES_MULE, SPECIES_DONKEY, SPECIES_OX, SPECIES_CAMEL, SPECIES_ELEPHANT };
    const Species tame[] = { SPECIES_WOLF, SPECIES_DOG, SPECIES_TIGER, SPECIES_PUMA, SPECIES_FALCON, SPECIES_RAVEN };
    const Species hostile[] = { SPECIES_BEAR, SPECIES_HYENA, SPECIES_COYOTE, SPECIES_BOAR };
    const Species prey[] = { SPECIES_ANTELOPE, SPECIES_REINDEER, SPECIES_GAZELLE, SPECIES_DEER, SPECIES_HARE };
    for (int i = 0; i < 6; i++) {
        const SpeciesDef *d = species_def(mounts[i]);
        CHECK(d->cls == CLASS_MOUNT && d->rideable && d->ride_speed > 1.0f && d->tame_chance > 0.0f);
        CHECK(species_def(tame[i])->cls == CLASS_TAMEABLE && !species_def(tame[i])->rideable);
    }
    for (int i = 0; i < 4; i++) CHECK(species_def(hostile[i])->cls == CLASS_HOSTILE && species_def(hostile[i])->damage > 0);
    for (int i = 0; i < 5; i++)
        CHECK(species_def(prey[i])->cls == CLASS_PREY && species_is_prey(prey[i]) && species_def(prey[i])->meat > 0);
    CHECK(species_def(SPECIES_GOAT)->cls == CLASS_LIVESTOCK && species_def(SPECIES_GOAT)->milk > 0);
    CHECK(species_def(SPECIES_CALF)->cls == CLASS_LIVESTOCK && species_def(SPECIES_CALF)->meat > 0);
    CHECK(species_def(SPECIES_FALCON)->flier && species_def(SPECIES_RAVEN)->flier && !species_def(SPECIES_WOLF)->flier);
    // Sociales y solitarios.
    CHECK(species_def(SPECIES_WOLF)->social && species_def(SPECIES_HYENA)->social && species_def(SPECIES_DEER)->social);
    CHECK(!species_def(SPECIES_TIGER)->social && !species_def(SPECIES_PUMA)->social && !species_def(SPECIES_BEAR)->social);
    CHECK(species_find("lobo") == SPECIES_WOLF && species_find("jabali") == SPECIES_BOAR && species_find("nada") < 0);
    for (int s = 0; s < SPECIES_COUNT; s++) {
        const SpeciesDef *d = species_def((Species)s);
        CHECK(d->hp > 0 && d->speed > d->walk && d->group_min >= 1 && d->group_max >= d->group_min && d->habitat);
    }
}

static void test_animals_flee_and_wander(void) {
    Rng r;
    rng_seed(&r, 3);
    Animal a;
    animal_init(&a, SPECIES_DEER, 0, 0);
    for (int i = 0; i < 100; i++) animal_update(&a, 0.1f, 1000.0f, 1000.0f, &r); // jugador lejos: deambula
    CHECK(!a.fleeing && sqrtf(a.x * a.x + a.z * a.z) < 20.0f);
    // Una persona cerca que no le hizo nada: no huye.
    for (int i = 0; i < 10; i++) animal_update(&a, 0.1f, a.x + 3.0f, a.z, &r);
    CHECK(!a.fleeing);
    // Herida por una persona: huye de ella, y su grupo tambien.
    Animal herd[3];
    for (int i = 0; i < 3; i++) animal_init(&herd[i], SPECIES_DEER, (float)i * 2.0f, 0), herd[i].group = 7;
    FaunaHuman hu = { 5.0f, 0.0f, false, false, false };
    FaunaCtx c = { &hu, 1, 5.0f, 0.0f, false, 100.0f, 100.0f, false, NULL, NULL, false };
    animal_hurt(herd, 3, 2, &r, 10.0f, WOUND_CUT, PART_THORAX, true, 0);
    CHECK(herd[2].fear_humans > 0.0f && herd[0].fear_humans > 0.0f && herd[1].fear_humans > 0.0f);
    float before = an_dist(&herd[0], 5.0f, 0.0f);
    for (int i = 0; i < 20; i++) fauna_update(herd, 3, &c, &r, 0.1f, NULL);
    CHECK(herd[0].fleeing && herd[2].fleeing && an_dist(&herd[0], 5.0f, 0.0f) > before);
}

static void test_animals_tame_saddle_ride(void) {
    Rng r;
    rng_seed(&r, 9);
    Animal deer, horse;
    animal_init(&deer, SPECIES_DEER, 0, 0);
    animal_init(&horse, SPECIES_HORSE, 0, 0);
    CHECK(!animal_saddle(&horse)); // salvaje: no se ensilla
    int tries = 0;
    while (!animal_try_tame(&horse, &r, 0.0f, 50.0f, 60.0f) && tries < 100) tries++;
    CHECK(horse.state == ANIMAL_TAMED && horse.home_x == 50.0f && tries < 100);
    CHECK(animal_lasso(&horse, &r, 1.0f, 0, 0) == TAME_ALREADY);
    CHECK(!animal_can_ride(&horse) && animal_saddle(&horse) && animal_can_ride(&horse));
    CHECK(animal_lasso(&deer, &r, 1.0f, 0, 0) == TAME_NEVER && deer.state == ANIMAL_WILD); // presa: no se doma
    Animal bear;
    animal_init(&bear, SPECIES_BEAR, 0, 0);
    bear.h.hp = 1.0f;
    CHECK(animal_lasso(&bear, &r, 1.0f, 0, 0) == TAME_NEVER); // hostil: nunca, ni debilitado
    // Todas las monturas se doman con el lazo y se ensillan.
    for (int s = SPECIES_HORSE; s <= SPECIES_ELEPHANT; s++) {
        Animal m;
        animal_init(&m, (Species)s, 0, 0);
        CHECK(animal_lasso(&m, &r, 1.0f, 0, 0) == TAME_OK && animal_saddle(&m) && animal_can_ride(&m));
    }
    // Domado: sigue al jugador cercano.
    for (int i = 0; i < 50; i++) animal_update(&horse, 0.1f, 10.0f, 0.0f, &r);
    CHECK(fabsf(horse.x - 10.0f) < 6.0f);
}

// Depredador domable: pelear hasta debilitarlo, lazo, darle de comer.
static void test_animals_tame_predator(void) {
    Rng r;
    rng_seed(&r, 21);
    Animal w;
    animal_init(&w, SPECIES_WOLF, 0, 0);
    CHECK(animal_lasso(&w, &r, 1.0f, 0, 0) == TAME_TOO_STRONG);
    animal_hurt(&w, 1, 0, &r, 30.0f, WOUND_CUT, PART_ABDOMEN, true, 0);
    CHECK(w.weakened && w.state == ANIMAL_WILD);
    CHECK(!animal_feed(&w, 0, 0)); // suelto no se le da de comer
    CHECK(animal_lasso(&w, &r, 1.0f, 0, 0) == TAME_OK && w.state == ANIMAL_BOUND);
    // Sin comer, se suelta.
    Animal w2 = w;
    FaunaEvents ev = { .n = 0 };
    FaunaHuman hu = { 50.0f, 50.0f, false, false, false };
    FaunaCtx c = { &hu, 1, 50.0f, 50.0f, false, 0, 0, false, NULL, NULL, false };
    for (int i = 0; i < 650 && w2.state == ANIMAL_BOUND; i++) fauna_update(&w2, 1, &c, &r, 0.1f, &ev);
    CHECK(w2.state == ANIMAL_WILD && ev.n > 0 && ev.ev[ev.n - 1].kind == FEV_BREAK_FREE);
    // Con carne: de la tribu.
    CHECK(animal_feed(&w, 0, 0) && w.state == ANIMAL_TAMED);
    // Domado, defiende al jugador: va a por un enemigo de la tribu y lo ataca.
    Animal pack[1] = { w };
    pack[0].x = 0, pack[0].z = 0;
    FaunaHuman people[2] = { { 0.0f, 0.0f, false, false, false }, { 6.0f, 0.0f, false, false, true } };
    FaunaCtx c2 = { people, 2, 0.0f, 0.0f, false, 0, 0, false, NULL, NULL, false };
    ev.n = 0;
    bool bit_enemy = false;
    for (int i = 0; i < 60; i++) {
        fauna_update(pack, 1, &c2, &r, 0.1f, &ev);
        for (int k = 0; k < ev.n; k++) bit_enemy |= ev.ev[k].kind == FEV_BITE_HUMAN && ev.ev[k].other == 1;
        ev.n = 0;
    }
    CHECK(bit_enemy);
}

// Sociales: la manada comparte la presa y la caza; la presa nota al cazador y su grupo huye.
static void test_animals_pack_hunt(void) {
    Rng r;
    rng_seed(&r, 5);
    Animal a[7];
    for (int i = 0; i < 3; i++) {
        animal_init(&a[i], SPECIES_WOLF, -25.0f + (float)i, 0.0f);
        a[i].group = 1, a[i].hunger = 1.0f;
    }
    for (int i = 3; i < 7; i++) animal_init(&a[i], SPECIES_DEER, (float)(i - 3) * 2.0f, 3.0f), a[i].group = 2;
    FaunaHuman hu = { 500.0f, 500.0f, false, false, false };
    FaunaCtx c = { &hu, 1, 500.0f, 500.0f, false, 0, 0, false, NULL, NULL, false };
    bool shared = false, alarmed = false, killed = false, ate = false;
    for (int step = 0; step < 1800 && !(killed && ate); step++) {
        FaunaEvents ev = { .n = 0 };
        fauna_update(a, 7, &c, &r, 0.1f, &ev);
        if (a[0].tkind == TGT_ANIMAL && a[1].tkind == TGT_ANIMAL && a[0].target == a[1].target && a[0].target >= 3)
            shared = true;
        for (int i = 3; i < 7; i++) alarmed |= a[i].fleeing;
        for (int k = 0; k < ev.n; k++) killed |= ev.ev[k].kind == FEV_KILL;
        for (int i = 0; i < 3; i++) ate |= a[i].mode == MODE_EAT;
    }
    CHECK(shared && alarmed && killed && ate);
    // Una presa no huye de un cazador que no la amenaza (demasiado chico para ella).
    Animal b[2];
    animal_init(&b[0], SPECIES_FALCON, 0, 0);
    animal_init(&b[1], SPECIES_ELEPHANT, 5, 0);
    b[0].hunger = 1.0f;
    for (int i = 0; i < 20; i++) fauna_update(b, 2, &c, &r, 0.1f, NULL);
    CHECK(!b[1].fleeing);
}

// Solitarios: acecho sin ser notado y rivalidad dentro de la especie.
static void test_animals_stalk_and_rivals(void) {
    Rng r;
    rng_seed(&r, 13);
    FaunaHuman hu = { 500.0f, 500.0f, false, false, false };
    FaunaCtx c = { &hu, 1, 500.0f, 500.0f, false, 0, 0, false, NULL, NULL, false };
    Animal a[2];
    animal_init(&a[0], SPECIES_TIGER, 0, 0);
    animal_init(&a[1], SPECIES_DEER, 25.0f, 0);
    a[0].hunger = 1.0f;
    for (int i = 0; i < 10; i++) fauna_update(a, 2, &c, &r, 0.1f, NULL);
    CHECK(a[0].mode == MODE_STALK && a[0].tkind == TGT_ANIMAL && a[0].target == 1);
    CHECK(a[1].alert <= 0.0f && !a[1].fleeing); // no lo nota mientras acecha
    CHECK(a[0].speed < species_def(SPECIES_TIGER)->speed * 0.5f);
    // Rivales: dos pumas sin hambre en el mismo territorio pelean; el perdedor se va.
    Animal p[2];
    animal_init(&p[0], SPECIES_PUMA, 0, 0);
    animal_init(&p[1], SPECIES_PUMA, 8.0f, 0);
    p[0].hunger = p[1].hunger = 0.0f;
    p[0].rival_timer = p[1].rival_timer = -100.0f;
    p[1].h.hp = 50.0f; // el segundo, ya tocado: pierde
    bool rival = false, won = false;
    for (int i = 0; i < 200 && !won; i++) {
        FaunaEvents ev = { .n = 0 };
        fauna_update(p, 2, &c, &r, 0.1f, &ev);
        rival |= p[0].mode == MODE_RIVAL && p[1].mode == MODE_RIVAL;
        for (int k = 0; k < ev.n; k++)
            if (ev.ev[k].kind == FEV_RIVAL_WON) {
                won = true;
                CHECK(ev.ev[k].animal == 0 && ev.ev[k].other == 1);
            }
    }
    CHECK(rival && won && an_dist(&p[1], p[1].home_x, p[1].home_z) > 20.0f && p[1].alert > 0.0f);
    CHECK(p[0].h.hp > 0.0f && p[1].h.hp > 0.0f); // los golpes de advertencia no matan
}

// Cazadores y hostiles: no huyen (contraatacan) salvo con la vida critica.
static void test_animals_flee_only_critical(void) {
    Rng r;
    rng_seed(&r, 17);
    FaunaHuman hu = { 3.0f, 0.0f, false, false, false };
    FaunaCtx c = { &hu, 1, 3.0f, 0.0f, false, 50, 50, false, NULL, NULL, false };
    Animal w;
    animal_init(&w, SPECIES_BEAR, 0, 0);
    animal_hurt(&w, 1, 0, &r, 60.0f, WOUND_CUT, PART_THORAX, true, 0);
    for (int i = 0; i < 5; i++) fauna_update(&w, 1, &c, &r, 0.1f, NULL);
    CHECK(!w.fleeing && w.tkind == TGT_HUMAN);
    w.h.hp = w.h.hp_max * 0.1f;
    for (int i = 0; i < 5; i++) fauna_update(&w, 1, &c, &r, 0.1f, NULL);
    CHECK(w.fleeing && w.mode == MODE_FLEE);
    // Los hostiles atacan a las personas que ven.
    Animal h;
    animal_init(&h, SPECIES_HYENA, 0, 0);
    FaunaEvents ev = { .n = 0 };
    bool bit = false;
    for (int i = 0; i < 40; i++) {
        fauna_update(&h, 1, &c, &r, 0.1f, &ev);
        for (int k = 0; k < ev.n; k++) bit |= ev.ev[k].kind == FEV_BITE_HUMAN && ev.ev[k].damage > 0.0f;
        ev.n = 0;
    }
    CHECK(bit);
    // Una presa que se defiende (buey) embiste a quien la hirio en vez de huir.
    Animal ox;
    animal_init(&ox, SPECIES_OX, 0, 0);
    animal_hurt(&ox, 1, 0, &r, 20.0f, WOUND_CUT, PART_THORAX, true, 0);
    for (int i = 0; i < 5; i++) fauna_update(&ox, 1, &c, &r, 0.1f, NULL);
    CHECK(!ox.fleeing && ox.tkind == TGT_HUMAN);
}

// Ganado: pastoreo, leche, sacrificio y despiece.
static void test_animals_livestock(void) {
    Rng r;
    rng_seed(&r, 4);
    Animal g[2];
    animal_init(&g[0], SPECIES_GOAT, 0, 0);
    animal_init(&g[1], SPECIES_CALF, 2, 0);
    CHECK(g[0].state == ANIMAL_TAMED && g[1].state == ANIMAL_TAMED); // de la tribu desde el principio
    // El pastor camina cerca: lo siguen; se para: se quedan donde estan.
    FaunaHuman hu = { 4.0f, 0.0f, false, false, false };
    FaunaCtx c = { &hu, 1, 4.0f, 0.0f, true, 0, 0, false, NULL, NULL, false };
    for (int i = 0; i < 300; i++) {
        hu.x = c.px = 4.0f + (float)i * 0.1f; // 1 m/s
        fauna_update(g, 2, &c, &r, 0.1f, NULL);
    }
    CHECK(g[0].mode == MODE_FOLLOW && g[0].x > 20.0f && g[1].x > 20.0f);
    c.player_moving = false;
    fauna_update(g, 2, &c, &r, 0.1f, NULL);
    CHECK(g[0].mode == MODE_GRAZE && g[0].home_x > 20.0f);
    // Leche: una vez al dia; el becerro no da.
    CHECK(animal_milk(&g[0]) > 0 && animal_milk(&g[0]) == 0 && animal_milk(&g[1]) == 0);
    animal_new_day(&g[0]);
    CHECK(animal_milk(&g[0]) > 0);
    // Sacrificio: carne y piel; despiezar solo una vez.
    int meat = 0, hide = 0;
    CHECK(!animal_butcher(&g[1], &meat, &hide)); // vivo, no
    CHECK(animal_slaughter(&g[1]) && g[1].state == ANIMAL_DEAD);
    CHECK(animal_butcher(&g[1], &meat, &hide) && meat == species_def(SPECIES_CALF)->meat && hide > 0);
    CHECK(!animal_butcher(&g[1], &meat, &hide));
    Animal deer;
    animal_init(&deer, SPECIES_DEER, 0, 0);
    CHECK(!animal_slaughter(&deer)); // la caza se abate, no se sacrifica
    animal_hurt(&deer, 1, 0, &r, 500.0f, WOUND_CUT, PART_NECK, true, 0);
    CHECK(deer.state == ANIMAL_DEAD && deer.killed_by_human);
    deer.eaten = 0.75f; // se lo comieron los lobos: queda poco
    CHECK(animal_butcher(&deer, &meat, &hide) && meat < species_def(SPECIES_DEER)->meat && hide == 0);
}

// Cuerpo de los animales: los proyectiles impactan en zonas de fiera, a la altura de su talla.
static void test_animals_body(void) {
    Animal h, hare, f;
    animal_init(&h, SPECIES_HORSE, 0, 0);
    animal_init(&hare, SPECIES_HARE, 0, 0);
    animal_init(&f, SPECIES_FALCON, 0, 0);
    BodyPose b;
    float t;
    animal_body(&h, &b);
    CHECK(body_raycast(&b, (V3){ 5, 0.95f, 0.3f }, (V3){ -10, 0, 0 }, 1.0f, &t) == PART_THORAX);
    animal_body(&hare, &b);
    CHECK(body_raycast(&b, (V3){ 5, 0.95f, 0.0f }, (V3){ -10, 0, 0 }, 1.0f, &t) < 0); // por encima de una liebre
    f.alt = 6.0f;
    animal_body(&f, &b);
    CHECK(body_raycast(&b, (V3){ 5, 6.1f, 0.0f }, (V3){ -10, 0, 0 }, 1.0f, &t) >= 0); // un ave, en el aire
}


// Agua para las pruebas: un lago de 3 m de hondo donde x < 0; seco donde x >= 0.
static float test_lake(void *ud, float x, float z) {
    (void)ud, (void)z;
    return x < 0.0f ? 3.0f : -1.0f;
}

// Veneno: quita vida poco a poco, frena la recuperacion y las hierbas lo cortan.
static void test_health_venom(void) {
    Rng r;
    rng_seed(&r, 2);
    Health h;
    health_init(&h, 100.0f);
    health_poison(&h, 20.0f);
    CHECK(health_poisoned(&h) && !strcmp(health_state_name(&h), "envenenado") && health_untreated(&h) == 1);
    for (int i = 0; i < 100; i++) health_update(&h, &r, 0.1f, false, 0.0f); // 10 s
    CHECK(h.hp < 95.0f && h.hp > 80.0f && h.venom < 15.0f);
    float before = h.venom;
    CHECK(health_treat(&h) >= 1 && h.venom < before * 0.6f);
    for (int i = 0; i < 1200; i++) health_update(&h, &r, 0.1f, false, 0.0f);
    CHECK(!health_poisoned(&h) && h.hp > 80.0f); // se pasa y vuelve a sanar
}

// Acuaticos y venenosos: el cocodrilo no se aleja de su orilla; la tortuga se salva
// en el agua; la vibora muerde con veneno pero no persigue.
// Tribus (rivales, neutrales y amigables) y estructuras, cada una en su region y en seco.
static void test_world_tribes_and_sites(void) {
    static const uint32_t SEEDS[] = { 1206u, 7u, 42u, 2024u };
    for (int si = 0; si < 4; si++) {
        const World *w = world_for_seed(SEEDS[si]);
        int att[TRIBE_ATTITUDES] = { 0 }, ruins = 0, used = 0;
        CHECK(w->tribe_count >= 12);
        for (int i = 0; i < w->tribe_count; i++) {
            const TribeCamp *tc = &w->tribes[i];
            att[tc->attitude]++;
            CHECK(world_region(w, tc->x, tc->z) == tc->region && tc->name[0]);
            CHECK(world_water(w, tc->x, tc->z, 0.0f, NULL) < world_height(w, tc->x, tc->z));
        }
        for (int k = 0; k < TRIBE_ATTITUDES; k++) CHECK(att[k] >= 3);
        CHECK(w->site_count >= 35);
        for (int i = 0; i < w->site_count; i++) {
            const WorldSite *st = &w->sites[i];
            CHECK(world_region(w, st->x, st->z) == st->region);
            CHECK(world_water(w, st->x, st->z, 0.0f, NULL) < world_height(w, st->x, st->z));
            if (site_is_ruin(st->kind)) ruins++;
            else used++;
        }
        CHECK(ruins >= 15 && used >= 15);
    }
}

// Voxeles: figuras, caras expuestas (lo de adentro no se malla), erosion, y nubes al modo de
// Nubis (densidad por perfil y ruido) con la luz que baja hacia la sombra del sol.
static void test_voxels(void) {
    VoxGrid g;
    CHECK(vox_init(&g, 8, 8, 8, 0.5f));
    vox_box(&g, 0, 0, 0, 2, 2, 2, 1); // un cubo de 3x3x3: 54 caras expuestas
    CHECK(vox_count(&g) == 27 && vox_exposed_faces(&g) == 54);
    VoxMesh m;
    static const uint8_t PAL[2][4] = { { 0, 0, 0, 0 }, { 200, 100, 50, 255 } };
    CHECK(vox_mesh(&g, PAL, &m) && m.tris == 108);
    float miny = 1e9f, maxy = -1e9f;
    for (int i = 0; i < m.tris * 3; i++) miny = fminf(miny, m.pos[i * 3 + 1]), maxy = fmaxf(maxy, m.pos[i * 3 + 1]);
    CHECK(fabsf(miny) < 1e-4f && fabsf(maxy - 1.5f) < 1e-4f); // 3 celdas de 0,5 m desde la base
    vox_mesh_free(&m);
    vox_free(&g);
    // Erosion: las ruinas pierden celdas, mas arriba que abajo.
    CHECK(vox_init(&g, 12, 12, 12, 0.5f));
    vox_box(&g, 0, 0, 0, 11, 11, 11, 1);
    vox_erode(&g, 7u, 0.6f);
    int low = 0, high = 0;
    for (int z = 0; z < 12; z++)
        for (int x = 0; x < 12; x++) low += vox_get(&g, x, 1, z) != 0, high += vox_get(&g, x, 10, z) != 0;
    CHECK(vox_count(&g) < 12 * 12 * 12 && low > high);
    vox_free(&g);
    // Nubes: densidad dentro del perfil (nada en el borde), luz del lado del sol > del lado opuesto.
    for (int k = 0; k < VCLOUD_KINDS; k++) {
        CHECK(vox_init(&g, 20, 8, 20, 8.0f));
        vox_cloud_shape(&g, (VoxCloudKind)k, 11u + (uint32_t)k);
        int n = vox_count(&g);
        CHECK(n > 60 && n < 20 * 8 * 20);
        CHECK(!vox_get(&g, 0, 4, 0) && !vox_get(&g, 19, 4, 19)); // las esquinas, vacias
        vox_free(&g);
    }
    CHECK(vox_init(&g, 20, 8, 20, 8.0f));
    vox_cloud_shape(&g, VCLOUD_CUMULUS, 5u);
    vox_cloud_light(&g, 1.0f, 0.3f, 0.0f); // sol al este (+x)
    float east = 0.0f, west = 0.0f;
    int ne = 0, nw = 0;
    for (int y = 0; y < 8; y++)
        for (int z = 0; z < 20; z++)
            for (int x = 0; x < 20; x++) {
                if (!vox_get(&g, x, y, z)) continue;
                if (x >= 13) east += g.light[x + 20 * (z + 20 * y)], ne++;
                if (x <= 6) west += g.light[x + 20 * (z + 20 * y)], nw++;
            }
    CHECK(ne > 0 && nw > 0 && east / ne > west / nw);
    vox_free(&g);
}

// Un lago redondo de 20 m de radio y 2 m de hondo en el origen (orilla con pendiente).
static float test_round_lake(void *ud, float x, float z) {
    (void)ud;
    float d = sqrtf(x * x + z * z);
    return d < 20.0f ? fminf(2.0f, (20.0f - d) * 0.5f) : -1.0f;
}

// Los de tierra rodean el agua honda y, con sed, beben desde la orilla sin entrar.
static void test_animals_keep_out_of_lakes(void) {
    Rng r;
    rng_seed(&r, 77);
    FaunaHuman hu = { 500.0f, 500.0f, false, false, false };
    FaunaCtx c = { &hu, 1, 500.0f, 500.0f, false, 100, 100, false, test_round_lake, NULL, false };
    Animal herd[4];
    for (int i = 0; i < 4; i++) {
        animal_init(&herd[i], i < 2 ? SPECIES_DEER : SPECIES_WOLF, 24.0f + i, 0.0f);
        herd[i].home_x = herd[i].home_z = 0.0f; // su territorio esta centrado en el lago
    }
    float worst = 0.0f;
    for (int t = 0; t < 6000; t++) { // 10 minutos
        fauna_update(herd, 4, &c, &r, 0.1f, NULL);
        for (int i = 0; i < 4; i++) worst = fmaxf(worst, test_round_lake(NULL, herd[i].x, herd[i].z));
    }
    CHECK(worst <= ANIMAL_WADE_MAX + 1e-3f);
    // Con sed: va a la orilla, bebe y no entra.
    Animal deer;
    animal_init(&deer, SPECIES_DEER, 60.0f, 10.0f);
    deer.thirst = 1.0f;
    bool drank = false;
    worst = 0.0f;
    for (int t = 0; t < 1200 && !drank; t++) {
        fauna_update(&deer, 1, &c, &r, 0.1f, NULL);
        worst = fmaxf(worst, test_round_lake(NULL, deer.x, deer.z));
        drank = deer.thirst < 0.1f;
    }
    CHECK(drank && worst <= 0.0f);
    CHECK(sqrtf(deer.x * deer.x + deer.z * deer.z) < 26.0f); // en la orilla
}

static void test_animals_water_and_venom(void) {
    Rng r;
    rng_seed(&r, 31);
    FaunaHuman hu = { 6.0f, 0.0f, false, false, false };
    FaunaCtx c = { &hu, 1, 6.0f, 0.0f, false, 100, 100, false, test_lake, NULL, false };
    Animal croc;
    animal_init(&croc, SPECIES_CROCODILE, -2.0f, 0.0f); // en el agua, junto a la orilla
    FaunaEvents ev = { .n = 0 };
    bool bit = false;
    for (int i = 0; i < 60; i++) {
        fauna_update(&croc, 1, &c, &r, 0.1f, &ev);
        for (int k = 0; k < ev.n; k++) bit |= ev.ev[k].kind == FEV_BITE_HUMAN;
        ev.n = 0;
    }
    CHECK(bit); // embosca a quien se acerca a la orilla
    hu.x = c.px = 30.0f; // lejos de su guarida: no lo persigue
    for (int i = 0; i < 100; i++) fauna_update(&croc, 1, &c, &r, 0.1f, NULL);
    CHECK(croc.tkind == TGT_NONE && an_dist(&croc, croc.home_x, croc.home_z) < 12.0f);
    // Tortuga en el agua: el lobo no la caza; en tierra, si.
    Animal a[2];
    animal_init(&a[0], SPECIES_WOLF, 4.0f, 0.0f);
    animal_init(&a[1], SPECIES_TURTLE, -3.0f, 0.0f);
    a[0].hunger = 1.0f;
    hu.x = c.px = 500.0f;
    for (int i = 0; i < 5; i++) fauna_update(a, 2, &c, &r, 0.1f, NULL);
    CHECK(a[0].tkind == TGT_NONE);
    animal_init(&a[1], SPECIES_TURTLE, 10.0f, 0.0f);
    a[1].alert = 3.0f, a[1].flee_x = 4.0f, a[1].flee_z = 0.0f; // asustada en tierra: huye hacia el agua
    for (int i = 0; i < 5; i++) fauna_update(a, 2, &c, &r, 0.1f, NULL);
    CHECK(a[0].tkind == TGT_ANIMAL && a[0].target == 1);
    // Vibora: muerde con veneno si te acercas, y no te sigue lejos.
    Animal v;
    animal_init(&v, SPECIES_SNAKE, 20.0f, 0.0f);
    hu.x = c.px = 21.5f;
    float venom = 0.0f;
    for (int i = 0; i < 30; i++) {
        fauna_update(&v, 1, &c, &r, 0.1f, &ev);
        for (int k = 0; k < ev.n; k++) venom += ev.ev[k].kind == FEV_BITE_HUMAN ? ev.ev[k].venom : 0.0f;
        ev.n = 0;
    }
    CHECK(venom >= species_def(SPECIES_SNAKE)->venom);
    hu.x = c.px = 40.0f;
    for (int i = 0; i < 100; i++) fauna_update(&v, 1, &c, &r, 0.1f, NULL);
    CHECK(an_dist(&v, 20.0f, 0.0f) < 6.0f);
}

// Enjambres: abejas que defienden la colmena, humo, agua; mosquitos con calor al
// anochecer; peces que huyen, no salen del agua y se pescan.
static void test_swarms(void) {
    Rng r;
    rng_seed(&r, 8);
    SwarmCtx c = { 30.0f, 0.0f, 0.0f, false, false, false, 22.0f, false, false, test_lake, NULL };
    Swarm bees;
    swarm_init(&bees, SWARM_BEES, 20.0f, 0.0f, 0.0f, &r);
    for (int i = 0; i < 50; i++) swarm_update(&bees, &c, &r, 0.1f);
    CHECK(bees.anger == 0.0f); // de lejos, tranquilas
    swarm_provoke(&bees);      // golpeaste la colmena
    int stings = 0;
    for (int i = 0; i < 100; i++) {
        c.px = 22.0f + (float)i * 0.02f;
        stings += swarm_update(&bees, &c, &r, 0.1f).stings;
    }
    CHECK(stings >= 3);
    c.torch = true; // humo: se calman
    swarm_update(&bees, &c, &r, 0.1f);
    CHECK(bees.anger == 0.0f && bees.calm > 0.0f);
    c.torch = false;
    for (int i = 0; i < 200; i++) swarm_update(&bees, &c, &r, 0.1f); // pasa el humo
    swarm_provoke(&bees);
    c.submerged = true; // bajo el agua te pierden
    swarm_update(&bees, &c, &r, 0.1f);
    CHECK(bees.anger == 0.0f);
    c.submerged = false;
    c.night = true; // de noche, en la colmena
    swarm_update(&bees, &c, &r, 0.1f);
    CHECK(!bees.active);
    // Avispas: acercarse al nido basta.
    Swarm w;
    swarm_init(&w, SWARM_WASPS, 0.0f, 0.0f, 50.0f, &r);
    c.night = false, c.px = 1.0f, c.pz = 52.0f;
    swarm_update(&w, &c, &r, 0.1f);
    CHECK(w.anger > 0.0f);
    // Mosquitos: con calor al atardecer, no a mediodia ni con frio.
    Swarm m;
    swarm_init(&m, SWARM_MOSQUITOES, -2.0f, 0.2f, 0.0f, &r);
    c.px = 1.0f, c.pz = 0.0f;
    swarm_update(&m, &c, &r, 0.1f);
    CHECK(!m.active);
    c.dusk = true;
    swarm_update(&m, &c, &r, 0.1f);
    CHECK(m.active && m.anger > 0.0f);
    c.temp = 5.0f;
    swarm_update(&m, &c, &r, 0.1f);
    CHECK(!m.active);
    // Peces: bajo la superficie, dentro del lago; huyen; se pescan.
    Swarm f;
    swarm_init(&f, SWARM_FISH, -6.0f, 0.0f, 0.0f, &r);
    c.px = 50.0f, c.pz = 50.0f;
    for (int i = 0; i < 300; i++) swarm_update(&f, &c, &r, 0.1f);
    bool under = true;
    for (int i = 0; i < f.n; i++) under &= f.b[i].y < 0.0f && f.b[i].y > -3.0f;
    CHECK(under && f.cx < 0.0f);
    float cx = f.cx;
    c.px = f.cx + 2.0f, c.pz = f.cz;
    for (int i = 0; i < 10; i++) swarm_update(&f, &c, &r, 0.1f);
    CHECK(f.cx < cx); // se alejan de quien se mete
    int alive = swarm_alive(&f);
    CHECK(swarm_catch(&f, f.b[0].x, f.b[0].y, f.b[0].z, 0.5f, 1) == 1 && swarm_alive(&f) == alive - 1);
}


// Fuego de prueba: pasto en todas partes salvo donde x > 50 (un rio); un arbol en (0, 10).
static FuelKind test_fuel(void *ud, float x, float z, int *ref) {
    (void)ud;
    *ref = -1;
    if (x > 50.0f) return FUEL_NONE;
    if (fabsf(x) < 1.5f && fabsf(z - 10.0f) < 1.5f) return *ref = 0, FUEL_TREE;
    return FUEL_GRASS;
}

static void test_fire_spread_and_rain(void) {
    FireField f;
    fire_init(&f, 4);
    FireEnv dry = { test_fuel, NULL, 1.0f, 0.0f, 6.0f, 0.0f }; // seco, viento hacia +x
    CHECK(fire_ignite(&f, 0.0f, 0.0f, FUEL_GRASS, -1));
    CHECK(!fire_ignite(&f, 0.5f, 0.0f, FUEL_GRASS, -1)); // ya arde ahi
    FireEvent ev[64];
    bool burned_out = false;
    for (int i = 0; i < 300; i++) { // 30 s
        int n = fire_update(&f, &dry, 0.1f, ev, 64);
        for (int k = 0; k < n; k++) burned_out |= ev[k].kind == FIRE_EV_BURNED_OUT;
    }
    CHECK(fire_count(&f) > 5 && burned_out); // se propago y lo primero ya se consumio
    // A favor del viento llega mas lejos.
    float max_x = -1e9f, min_x = 1e9f;
    for (int i = 0; i < FIRE_MAX; i++)
        if (f.cells[i].used) max_x = fmaxf(max_x, f.cells[i].x), min_x = fminf(min_x, f.cells[i].x);
    CHECK(max_x > -min_x);
    CHECK(fire_scorched(&f, 0.0f, 0.0f) && !fire_ignite(&f, 0.0f, 0.0f, FUEL_GRASS, -1)); // lo calcinado no vuelve a arder
    CHECK(fire_heat_at(&f, max_x, 0.0f, 3.0f) > 0.0f);
    // La lluvia lo apaga todo.
    FireEnv rain = dry;
    rain.rain = 0.5f;
    bool out = false;
    for (int i = 0; i < 200; i++) {
        int n = fire_update(&f, &rain, 0.1f, ev, 64);
        for (int k = 0; k < n; k++) out |= ev[k].kind == FIRE_EV_EXTINGUISHED;
    }
    CHECK(fire_count(&f) == 0 && out);
    // Mojado o verde no se propaga.
    FireField g;
    fire_init(&g, 5);
    FireEnv wet = { test_fuel, NULL, fire_dryness(0.9f, 0.8f, 0.0f, 15.0f), 0.0f, 0.0f, 0.0f };
    fire_ignite(&g, -20.0f, -20.0f, FUEL_GRASS, -1);
    for (int i = 0; i < 300; i++) fire_update(&g, &wet, 0.1f, NULL, 0);
    CHECK(fire_count(&g) <= 1);
    CHECK(fire_dryness(0.1f, 0.0f, 0.0f, 30.0f) > 0.8f && fire_dryness(0.1f, 0.0f, 1.0f, 30.0f) == 0.0f);
    // Un arbol tarda mas en consumirse que el pasto; un rayo lo prende ya caliente.
    FireField h;
    fire_init(&h, 6);
    CHECK(fire_ignite_hot(&h, 0.0f, 10.0f, FUEL_TREE, 0, 1.0f) && fire_burning(&h, FUEL_TREE, 0));
    CHECK(!fire_ignite(&h, 0.0f, 10.0f, FUEL_TREE, 0));
}

static void test_weather_hazards_random(void) {
    Rng r;
    rng_seed(&r, 12);
    // Incendios solo en verano, con pasto seco y sin lluvia.
    int summer = 0, winter = 0, wet = 0;
    for (int i = 0; i < 20000; i++) {
        summer += fire_wildfire_roll(true, 1.0f, 0.0f, 1.0f, &r);
        winter += fire_wildfire_roll(false, 1.0f, 0.0f, 1.0f, &r);
        wet += fire_wildfire_roll(true, 1.0f, 0.5f, 1.0f, &r);
    }
    CHECK(summer > 5 && winter == 0 && wet == 0);
    // Rayos solo en tormenta.
    int storm = 0, calm = 0;
    for (int i = 0; i < 1000; i++) storm += fire_lightning_roll(1.0f, 1.0f, &r), calm += fire_lightning_roll(0.0f, 1.0f, &r);
    CHECK(storm > 20 && calm == 0);
    CHECK(fire_lightning_ignite_chance(1.0f, 0.0f) > fire_lightning_ignite_chance(0.0f, 1.0f));
    // Derrumbes: solo con lluvia torrencial y estructuras descuidadas.
    CHECK(structure_collapse_chance(0.1f, 1.0f, 1.0f) > 0.0f);
    CHECK(structure_collapse_chance(0.9f, 1.0f, 1.0f) == 0.0f && structure_collapse_chance(0.1f, 0.5f, 1.0f) == 0.0f);
    CHECK(structure_collapse_chance(0.0f, 1.0f, 1.0f) > structure_collapse_chance(0.3f, 1.0f, 1.0f));
    CHECK(structure_daily_wear(true) > structure_daily_wear(false));
    // Quemaduras: no sangran y la armadura de metal casi no las para.
    Health hh;
    health_init(&hh, 100.0f);
    int w = health_hit(&hh, &r, 30.0f, WOUND_BURN, PART_THORAX);
    CHECK(w >= 0 && hh.wounds[w].kind == WOUND_BURN && !hh.wounds[w].bleeding && !strcmp(wound_name(WOUND_BURN), "Quemadura"));
    Armor a;
    memset(&a, 0, sizeof(a));
    CHECK(armor_equip(&a, "armadura.torso.escamas_hierro"));
    WoundKind k = WOUND_BURN;
    bool broke;
    CHECK(armor_absorb(&a, PART_THORAX, &k, false, 20.0f, &r, &broke) > 15.0f);
}


// Cuerpo a cuerpo: escudos, patadas, agarres, ganchos, combos y manos.
static void test_melee_moves(void) {
    Rng r;
    rng_seed(&r, 77);
    Fighter att = { .strength = 1.0f, .health = 1.0f, .armed = true, .facing = 1.0f };
    Fighter guard = { .shield = true, .blocking = true, .strength = 1.0f, .health = 1.0f, .armed = true, .facing = 1.0f };
    Fighter open = { .strength = 1.0f, .health = 1.0f, .armed = true, .facing = 1.0f };
    int blocked = 0, knocked_kick = 0, knocked_run = 0, heavy_stagger = 0;
    float heavy_dmg = 0.0f;
    for (int i = 0; i < 400; i++) {
        blocked += melee_resolve(MOVE_LIGHT, &att, &guard, "arma.corta.sable", 1, &r).blocked;
        MeleeResult h = melee_resolve(MOVE_HEAVY, &att, &guard, "arma.corta.sable", 1, &r);
        heavy_stagger += h.staggered && h.blocked;
        heavy_dmg += h.damage;
        knocked_kick += melee_resolve(MOVE_KICK, &att, &guard, "", 1, &r).knocked_down;
        knocked_run += melee_resolve(MOVE_RUN_KICK, &att, &guard, "", 1, &r).knocked_down;
    }
    CHECK(blocked > 300);                     // el escudo para casi todos los golpes ligeros
    CHECK(heavy_stagger == 400 && heavy_dmg > 0.0f); // el pesado rompe la guardia y algo pasa
    CHECK(knocked_kick == 0);                 // una patada contra el escudo no tumba...
    CHECK(knocked_run > 250);                 // ...con inercia, si
    // De espaldas, el escudo no sirve.
    Fighter back = guard;
    back.facing = -1.0f;
    CHECK(!melee_resolve(MOVE_LIGHT, &att, &back, "arma.corta.sable", 1, &r).blocked);
    // En el suelo, mas daño.
    Fighter down = open;
    down.down = true;
    CHECK(melee_resolve(MOVE_KICK, &att, &down, "", 1, &r).damage > melee_resolve(MOVE_KICK, &att, &open, "", 1, &r).damage);
    // Escudo: golpe y carga solo con escudo; escudo contra escudo, los dos se tambalean.
    CHECK(!melee_resolve(MOVE_SHIELD_BASH, &att, &open, "", 1, &r).landed);
    MeleeResult bash = melee_resolve(MOVE_SHIELD_BASH, &guard, &guard, "", 1, &r);
    CHECK(bash.landed && bash.blocked && bash.attacker_staggered && bash.staggered);
    int charge_down = 0;
    for (int i = 0; i < 200; i++) charge_down += melee_resolve(MOVE_SHIELD_CHARGE, &guard, &open, "", 1, &r).knocked_down;
    CHECK(charge_down > 80);
    // Agarre: del escudo (lo arranca), del brazo armado (desarma y tumba), de una extremidad (llave).
    Fighter strong = att;
    strong.strength = 3.0f;
    MeleeResult g = melee_resolve(MOVE_GRAPPLE, &strong, &guard, "", 1, &r);
    CHECK(g.grab == GRAB_SHIELD && g.shield_dropped);
    Fighter swinging = open;
    swinging.attacking = true;
    g = melee_resolve(MOVE_GRAPPLE, &strong, &swinging, "", 1, &r);
    CHECK(g.grab == GRAB_WEAPON_ARM && g.disarmed && g.knocked_down);
    g = melee_resolve(MOVE_GRAPPLE, &strong, &open, "", 1, &r);
    CHECK(g.grab == GRAB_LIMB && g.knocked_down);
    Fighter weak = att;
    weak.strength = 0.2f;
    g = melee_resolve(MOVE_GRAPPLE, &weak, &strong, "", 1, &r);
    CHECK(!g.landed && g.attacker_staggered); // se zafa: el que agarra queda expuesto
    // Gancho: solo armas con gancho; hace soltar el escudo.
    CHECK(weapon_can_hook("arma.corta.hacha") && weapon_can_hook("arma.larga.guja") && !weapon_can_hook("arma.corta.sable"));
    CHECK(!melee_resolve(MOVE_HOOK, &att, &guard, "arma.corta.sable", 1, &r).landed);
    int dropped = 0;
    for (int i = 0; i < 100; i++) dropped += melee_resolve(MOVE_HOOK, &att, &guard, "arma.larga.guja", 1, &r).shield_dropped;
    CHECK(dropped > 50);
    // Combos: el remate pega mas; las armas cortas encadenan mas rapido; el remate largo puede tumbar.
    CHECK(melee_combo_scale(3) > melee_combo_scale(2) && melee_combo_scale(2) > melee_combo_scale(1));
    CHECK(melee_combo_cooldown("arma.corta.daga", 1.0f, 1) < melee_combo_cooldown("arma.larga.lanza", 1.0f, 1));
    int finisher = 0;
    for (int i = 0; i < 300; i++) finisher += melee_resolve(MOVE_LIGHT, &att, &open, "arma.larga.lanza", 3, &r).knocked_down;
    CHECK(finisher > 30 && weapon_is_short("arma.corta.daga") && weapon_is_long("arma.larga.lanza"));
    // La armadura pesada cuesta mas tumbarla.
    Fighter heavy_armor = open;
    heavy_armor.weight = 60.0f;
    int k_light = 0, k_heavy = 0;
    for (int i = 0; i < 400; i++) {
        k_light += melee_resolve(MOVE_KICK, &att, &open, "", 1, &r).knocked_down;
        k_heavy += melee_resolve(MOVE_KICK, &att, &heavy_armor, "", 1, &r).knocked_down;
    }
    CHECK(k_heavy < k_light);
}

static char *read_file(const char *path);

static void test_hands_swap_and_dual(void) {
    Inventory inv;
    char *text = read_file(ESTEPA_SOURCE_DIR "/assets/inventario.tsv");
    CHECK(text != NULL);
    if (!text) return;
    inventory_parse(&inv, text);
    Hands h;
    hands_init(&h);
    hands_equip(&h, inventory_find(&inv, "arma.corta.sable"), HAND_RIGHT);
    CHECK(hands_swap(&h) && !strcmp(h.left.id, "arma.corta.sable") && !h.right.id[0]); // a la izquierda
    bool off = false;
    CHECK(!strcmp(hands_attack_weapon(&h, 1, &off), "arma.corta.sable") && off); // con la mano torpe
    CHECK(hands_swap(&h) && !strcmp(h.right.id, "arma.corta.sable"));
    // Dos armas: el combo alterna derecha, izquierda, derecha.
    hands_equip(&h, inventory_find(&inv, "arma.corta.daga"), HAND_LEFT);
    CHECK(!strcmp(hands_attack_weapon(&h, 1, &off), "arma.corta.sable") && !off);
    CHECK(!strcmp(hands_attack_weapon(&h, 2, &off), "arma.corta.daga") && off);
    CHECK(hands_swap(&h) && !strcmp(h.right.id, "arma.corta.daga"));
    // Con escudo o a dos manos no se cambia.
    hands_clear(&h, HAND_LEFT);
    hands_equip(&h, inventory_find(&inv, "escudo.mano.mimbre"), HAND_LEFT);
    CHECK(!hands_swap(&h));
    hands_clear(&h, HAND_LEFT), hands_clear(&h, HAND_RIGHT);
    hands_equip(&h, inventory_find(&inv, "arma.larga.guja"), HAND_RIGHT);
    CHECK(!hands_swap(&h));
    // Enfundadas: a puño limpio.
    h.sheathed = true;
    CHECK(!hands_attack_weapon(&h, 1, NULL)[0]);
    free(text);
}


// Contenedores: bolsillos, mochila, alforjas; peso, talla y estado de las piezas.
static char *read_file(const char *path);

static void test_storage_bags(void) {
    Inventory inv;
    char *text = read_file(ESTEPA_SOURCE_DIR "/assets/inventario.tsv");
    CHECK(text != NULL);
    if (!text) return;
    inventory_parse(&inv, text);
    Bag pockets, pack, mule, camp;
    bag_init(&pockets, BAG_POCKETS, 0.0f);
    bag_init(&pack, BAG_BACKPACK, 0.0f);
    bag_init(&mule, BAG_MOUNT, mount_capacity_kg(SPECIES_MULE));
    bag_init(&camp, BAG_CAMP, 0.0f);
    CHECK(mule.cap_kg > pack.cap_kg && pack.cap_kg > pockets.cap_kg);
    CHECK(mount_capacity_kg(SPECIES_ELEPHANT) > mount_capacity_kg(SPECIES_HORSE));
    // En los bolsillos solo lo pequeño.
    CHECK(bag_add(&pockets, &inv, "utileria.consumible.hierbas", 2, 1.0f) == 2);
    CHECK(bag_add(&pockets, &inv, "arma.corta.sable", 1, 1.0f) == 0);
    CHECK(bag_add(&pack, &inv, "arma.corta.sable", 1, 1.0f) == 1);
    // Lo que no es transportable no entra en ninguno.
    CHECK(!item_storable(inventory_find(&inv, "estructura.vivienda.yurta_comun")));
    CHECK(bag_add(&camp, &inv, "estructura.vivienda.yurta_comun", 1, 1.0f) == 0);
    // Peso: la mochila se llena de troncos (15 kg c/u): cabe uno.
    CHECK(item_kg(inventory_find(&inv, "utileria.material.troncos")) > item_kg(inventory_find(&inv, "proyectil.flecha.comun")));
    CHECK(bag_add(&pack, &inv, "utileria.material.troncos", 1, 1.0f) == 0); // un tronco no entra en la mochila
    int stones = bag_add(&pack, &inv, "utileria.material.piedra", 5, 1.0f);  // 10 kg cada piedra
    CHECK(stones > 0 && stones < 5 && bag_kg(&pack, &inv) <= pack.cap_kg);
    bag_take(&pack, "utileria.material.piedra", stones, NULL);
    CHECK(bag_add(&mule, &inv, "utileria.material.troncos", 9, 1.0f) == 5); // la mula, cinco (80 kg)
    bag_take(&mule, "utileria.material.troncos", 5, NULL);
    // Lo apilable se junta; el equipo gastado va aparte y sale primero.
    int slots = pack.n;
    CHECK(bag_add(&pack, &inv, "proyectil.flecha.comun", 10, 1.0f) == 10 && bag_add(&pack, &inv, "proyectil.flecha.comun", 5, 1.0f) == 5);
    CHECK(pack.n == slots + 1 && bag_count(&pack, "proyectil.flecha.comun") == 15);
    CHECK(bag_add(&mule, &inv, "armadura.casco.fieltro", 1, 1.0f) == 1 && bag_add(&mule, &inv, "armadura.casco.fieltro", 1, 0.4f) == 1);
    CHECK(mule.n == 2);
    float cond = 1.0f;
    CHECK(bag_take(&mule, "armadura.casco.fieltro", 1, &cond) == 1 && cond < 0.5f);
    // Mover de un contenedor a otro conserva el estado.
    bag_add(&pack, &inv, "armadura.casco.fieltro", 1, 0.3f);
    int k = -1;
    for (int i = 0; i < pack.n; i++)
        if (!strcmp(pack.s[i].id, "armadura.casco.fieltro")) k = i;
    CHECK(k >= 0 && bag_move_slot(&pack, k, &mule, &inv, 1) == 1);
    CHECK(bag_count(&pack, "armadura.casco.fieltro") == 0 && bag_count(&mule, "armadura.casco.fieltro") == 2);
    bool worn = false;
    for (int i = 0; i < mule.n; i++) worn |= mule.s[i].condition < 0.35f;
    CHECK(worn);
    // Cargado de mas, se anda mas lento.
    CHECK(bag_speed_scale(10.0f, 20.0f) == 1.0f && bag_speed_scale(40.0f, 20.0f) < 1.0f && bag_speed_scale(400.0f, 20.0f) >= 0.5f);
    CHECK(armor_slot_for("armadura.grebas.malla") == SLOT_GREAVES && armor_slot_for("arma.corta.sable") < 0);
    inventory_free(&inv);
    free(text);
}

// Amuletos: cada uno con sus efectos; colgados, suman.
static void test_amulets(void) {
    Charm c;
    CHECK(amulet_charm("accesorio.amuleto.lobo", &c) && c.buff_count == 2);
    CHECK(!amulet_charm("accesorio.amuleto.inexistente", &c) && !amulet_charm("arma.corta.sable", &c));
    Loadout l;
    loadout_init(&l);
    CHECK(loadout_stat(&l, STAT_STEALTH) == 0.0f);
    amulet_charm("accesorio.amuleto.lobo", &c);
    CHECK(loadout_equip_amulet(&l, 0, &c));
    CHECK(loadout_stat(&l, STAT_STEALTH) > 0.25f);
    amulet_charm("accesorio.amuleto.aguila_ibice", &c);
    loadout_equip_amulet(&l, 1, &c);
    CHECK(loadout_stat(&l, STAT_STEALTH) > 0.3f && loadout_stat(&l, STAT_ARCHERY) > 0.3f);
    char d[96];
    CHECK(charm_describe(&c, d, sizeof(d)) > 0 && strstr(d, "puntería"));
    CHECK(loadout_unequip_amulet(&l, 0) && loadout_stat(&l, STAT_STEALTH) < 0.1f);
    const char *ids[] = { "lobo", "ciervo", "aguila_ibice", "tigre_dragon", "oso", "caballo" };
    for (int i = 0; i < 6; i++) {
        char id[64];
        snprintf(id, sizeof(id), "accesorio.amuleto.%s", ids[i]);
        CHECK(amulet_charm(id, &c));
    }
}

// ---------------------------------------------------------------- inventario de assets
static void test_inventory_parses_and_maps_paths(void) {
    const char *tsv =
        "# comentario\n"
        "id\tnombre\tetiquetas\tmedidas_m\ttris_max\testado\tnotas\n"
        "estructura.vivienda.yurta_comun\tYurta\tfaccion:nomada\t4.5x2.6x4.5\t300\tkiln\tnota\r\n"
        "accesorio.tatuaje.lobo\tTatuaje\tuso:equipable,formato:textura\t0.25x0.25x0\t0\tpendiente\t\n"
        "mapa.suelo.estepa\tSuelo\tformato:textura\t4x0x4\t0\tpendiente\t\n"
        "Mal.Id.Con.Mayusculas\tx\tx\t1x1x1\t1\tpendiente\t\n"
        "arma.corta.sable\tSable\t\tsin-medidas\t120\trefinado\t\n";
    Inventory inv;
    CHECK(inventory_parse(&inv, tsv) == 4); // cabecera, comentario e id invalido fuera
    const InvItem *suelo = inventory_find(&inv, "mapa.suelo.estepa");
    CHECK(suelo && suelo->w == 4.0f && suelo->h == 0.0f && suelo->l == 4.0f); // "0x4" no es hexadecimal
    const InvItem *y = inventory_find(&inv, "estructura.vivienda.yurta_comun");
    CHECK(y && !strcmp(y->category, "estructura") && y->state == INV_KILN && !y->texture);
    CHECK(y && !strcmp(y->name, "Yurta"));
    CHECK(y && y->w == 4.5f && y->h == 2.6f && y->tris_max == 300);
    char path[128];
    inventory_path(y, path, sizeof(path));
    CHECK(!strcmp(path, "assets/models/estructura/vivienda/yurta_comun.glb"));
    const InvItem *t = inventory_find(&inv, "accesorio.tatuaje.lobo");
    CHECK(t && t->texture);
    inventory_path(t, path, sizeof(path));
    CHECK(!strcmp(path, "assets/textures/accesorio/tatuaje/lobo.png"));
    const InvItem *s = inventory_find(&inv, "arma.corta.sable");
    CHECK(s && s->state == INV_REFINADO && s->w == 1.0f); // medidas invalidas: 1 m
    CHECK(inventory_find(&inv, "no.existe.nada") == NULL);
    inventory_free(&inv);
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) n = 0;
    if (buf) buf[n] = '\0';
    fclose(f);
    return buf;
}

// El inventario real del repo: ids unicos, categorias conocidas y medidas positivas.
static void test_inventory_repo_file_is_valid(void) {
    char *text = read_file(ESTEPA_SOURCE_DIR "/assets/inventario.tsv");
    CHECK(text != NULL);
    if (!text) return;
    static const char *CATS[] = { "mapa", "estructura", "vehiculo", "animal", "arma", "proyectil", "escudo", "totem",
                                  "armadura", "vestimenta", "accesorio", "asedio", "utileria", "personaje" };
    Inventory inv;
    CHECK(inventory_parse(&inv, text) >= 250);
    for (int i = 0; i < inv.count; i++) {
        const InvItem *it = &inv.items[i];
        int known = 0;
        for (size_t c = 0; c < sizeof(CATS) / sizeof(CATS[0]); c++) known |= !strcmp(it->category, CATS[c]);
        CHECK(known);
        CHECK(it->h >= 0.0f && it->w > 0.0f);
        for (int j = i + 1; j < inv.count; j++) CHECK(strcmp(it->id, inv.items[j].id) != 0);
    }
    CHECK(inventory_find(&inv, "estructura.vivienda.yurta_comun") != NULL);
    // Todo lo que nombran las acciones existe en el inventario.
    for (int a = 0; a < ACTION_COUNT; a++) {
        const ActionDef *d = action_def((ActionId)a);
        if (d->requires) CHECK(inventory_find(&inv, d->requires) != NULL);
        if (d->produces) CHECK(inventory_find(&inv, d->produces) != NULL);
    }
    for (int b = 0; b < BUILD_COUNT; b++) {
        const BuildDef *d = build_def((BuildId)b);
        CHECK(inventory_find(&inv, d->produces) != NULL);
        for (const Ingredient *m = d->mats; m->id; m++) CHECK(inventory_find(&inv, m->id) != NULL);
    }
    for (int a = 0; a < ACTION_COUNT; a++)
        for (const Ingredient *m = action_def((ActionId)a)->mats; m->id; m++) CHECK(inventory_find(&inv, m->id) != NULL);
    for (int c = 0; c < CRAFT_COUNT; c++) {
        const CraftDef *d = craft_def((CraftId)c);
        CHECK(inventory_find(&inv, d->produces) != NULL && (craft_by_hand(d) || inventory_find(&inv, d->building) != NULL));
        for (const Ingredient *m = d->mats; m->id; m++) CHECK(inventory_find(&inv, m->id) != NULL);
    }
    for (int mat = 0; mat < MAT_COUNT; mat++) { // reparaciones: materiales y horno existen
        const RepairDef *r = repair_def(mat);
        CHECK(!r->building || inventory_find(&inv, r->building) != NULL);
        for (const Ingredient *m = r->mats; m->id; m++) CHECK(inventory_find(&inv, m->id) != NULL);
    }
    { // el botin de los enemigos: todo existe en el inventario
        Rng lr;
        rng_seed(&lr, 3u);
        for (int i = 0; i < 300; i++) {
            LootItem it[16];
            int n = enemy_loot((EnemyKind)(i % ENEMY_COUNT), true, true, &lr, it, 16);
            for (int j = 0; j < n; j++) CHECK(inventory_find(&inv, it[j].id) != NULL);
        }
    }
    for (int sp = 0; sp < SPECIES_COUNT; sp++) CHECK(inventory_find(&inv, species_def((Species)sp)->model) != NULL);
    Stockpile seed;
    stock_init(&seed);
    stock_seed_camp(&seed);
    for (int i = 0; i < seed.n; i++) CHECK(inventory_find(&inv, seed.e[i].id) != NULL);
    CHECK(inventory_find(&inv, "arma.larga.guja")->hands == INV_HANDS_TWO);
    CHECK(inventory_find(&inv, "arma.corta.sable")->hands == INV_HANDS_ONE);
    CHECK(inventory_find(&inv, "escudo.grande.paves")->hands == INV_HANDS_SHIELD);
    CHECK(inventory_find(&inv, "mapa.roca.pequena")->hands == INV_HANDS_NONE);
    inventory_free(&inv);
    free(text);
}

// ---------------------------------------------------------------- indice de animaciones
// true si el clip (rig, nombre) esta en assets/animaciones.tsv.
static bool clip_indexed(const char *text, const char *rig, const char *clip) {
    char needle[96];
    snprintf(needle, sizeof(needle), "\n%s\t%s\t", rig, clip);
    return strstr(text, needle) != NULL;
}

static void test_anim_index_states(void) {
    HumanoidState s;
    memset(&s, 0, sizeof(s));
    s.doing = s.building = -1;
    s.grounded = true;
    CHECK(!strcmp(anim_humanoid(&s), "idle"));
    s.grip = GRIP_WEAPON_SHIELD;
    CHECK(!strcmp(anim_humanoid(&s), "empunar_escudo"));
    s.sheathed = true;
    CHECK(!strcmp(anim_humanoid(&s), "idle"));
    s.moving = true;
    CHECK(!strcmp(anim_humanoid(&s), "caminar"));
    s.running = true;
    CHECK(!strcmp(anim_humanoid(&s), "correr"));
    s.carrying = true;
    CHECK(!strcmp(anim_humanoid(&s), "llevar"));
    s.doing = ACTION_DIG_TRENCH;
    CHECK(!strcmp(anim_humanoid(&s), "cavar")); // la accion manda sobre el movimiento
    s.mounted = true;
    s.mount_speed = 10.0f;
    CHECK(!strcmp(anim_humanoid(&s), "jinete_galope"));
    s.climbing = true;
    CHECK(!strcmp(anim_humanoid(&s), "trepar_cuerda"));
    memset(&s, 0, sizeof(s));
    s.doing = -1;
    s.grounded = true;
    s.building = BUILD_BONFIRE;
    CHECK(!strcmp(anim_humanoid(&s), "construir"));
    s.hidden = true;
    CHECK(!strcmp(anim_humanoid(&s), "acechar_idle"));

    Animal a;
    animal_init(&a, SPECIES_HORSE, 0, 0);
    CHECK(!strcmp(anim_quadruped(&a), "pastar"));
    a.speed = 1.0f;
    CHECK(!strcmp(anim_quadruped(&a), "caminar"));
    a.fleeing = true;
    CHECK(!strcmp(anim_quadruped(&a), "galopar"));
    a.ridden = true;
    a.speed = 0.0f;
    CHECK(!strcmp(anim_quadruped(&a), "montado_idle"));
}

// Todo clip que el juego puede pedir existe en el indice, con el rig correcto.
static void test_anim_index_names_exist(void) {
    char *text = read_file(ESTEPA_SOURCE_DIR "/assets/animaciones.tsv");
    CHECK(text != NULL);
    if (!text) return;
    for (int a = 0; a < ACTION_COUNT; a++) CHECK(clip_indexed(text, "humanoide", anim_for_action((ActionId)a)));
    for (int g = 0; g <= GRIP_SHIELD; g++) CHECK(clip_indexed(text, "humanoide", anim_grip((Grip)g, false)));
    // Recorre todas las combinaciones de banderas del estado humano.
    for (int bits = 0; bits < (1 << 10); bits++) {
        HumanoidState s;
        memset(&s, 0, sizeof(s));
        s.moving = bits & 1, s.running = bits & 2, s.sneaking = bits & 4, s.grounded = bits & 8;
        s.hidden = bits & 16, s.climbing = bits & 32, s.climb_top = bits & 64, s.mounted = bits & 128;
        s.carrying = bits & 256, s.forging = bits & 512;
        s.mount_speed = (float)(bits % 3) * 4.0f;
        s.doing = (bits % 5 == 0) ? ACTION_SADDLE : -1;
        s.building = (bits % 7 == 0) ? BUILD_OVEN : -1;
        s.grip = (Grip)(bits % 6);
        CHECK(clip_indexed(text, "humanoide", anim_humanoid(&s)));
        // Combate y salud.
        s.limping = bits & 1, s.blocking = bits & 2, s.attacking = bits % 4, s.spear = bits & 4;
        s.hit = bits % 11 == 0, s.down = bits % 13 == 0, s.dead = bits % 17 == 0, s.ranged = bits % 4;
        CHECK(clip_indexed(text, "humanoide", anim_humanoid(&s)));
        // Cuerpo a cuerpo: cada movimiento, derribado, cambiar de mano.
        s.move = bits % (MOVE_COUNT + 1), s.knocked = bits % 19 == 0, s.swapping = bits % 23 == 0;
        CHECK(clip_indexed(text, "humanoide", anim_humanoid(&s)));
    }
    for (int sp = 0; sp < SPECIES_COUNT; sp++) {
        Animal a;
        animal_init(&a, (Species)sp, 0, 0);
        const char *rig = species_def((Species)sp)->flier ? "ave" : "cuadrupedo";
        for (int k = 0; k < 24; k++) {
            a.speed = (float)(k % 12);
            a.fleeing = k == 11;
            a.ridden = k >= 9 && k < 11;
            a.mode = (AnimalMode)(k % 7);
            a.alt = (float)(k % 5) * 1.5f;
            a.attack_anim = k == 13 ? 0.2f : 0.0f;
            a.hit_anim = k == 14 ? 0.2f : 0.0f;
            a.state = k == 15 ? ANIMAL_BOUND : k == 16 ? ANIMAL_DEAD : ANIMAL_WILD;
            a.clock = (float)k;
            CHECK(clip_indexed(text, rig, anim_animal(&a)));
            CHECK(clip_indexed(text, "cuadrupedo", anim_quadruped_fight((float)k, k == 3, k == 5, k == 7)));
        }
    }
    free(text);
}

static void test_hand_crafting_and_repairs(void) {
    // A mano, sin taller: flechas por tandas, cuerda, ungüento, coraza de cuero.
    const CraftDef *ar = craft_def(CRAFT_ARROWS);
    CHECK(craft_by_hand(ar) && ar->amount == 5);
    CHECK(craft_by_hand(craft_def(CRAFT_ROPE)) && craft_by_hand(craft_def(CRAFT_OINTMENT)));
    CHECK(!craft_by_hand(craft_def(CRAFT_SABLE_BRONZE)));
    Troop t;
    troop_init(&t, NULL);
    CHECK(craft_seconds(ar, &t) == ar->work); // lo hace el jugador: no hace falta oficio
    // Reparar: el cuero con pieles; el bronce y el hierro piden herrero y su horno.
    for (int m = 0; m < MAT_COUNT; m++) CHECK(repair_def(m) && repair_def(m)->mats[0].id && repair_def(m)->restore > 0.0f);
    CHECK(repair_def(MAT_LEATHER)->role == ROLE_NONE && repair_def(MAT_IRON)->role == ROLE_SMITH);
    CHECK(repair_def(MAT_IRON)->building != NULL && repair_def(-1) == repair_def(MAT_FELT)); // fuera de rango: fieltro
}

static void test_enemy_loot_and_mounted_momentum(void) {
    Rng rng;
    rng_seed(&rng, 41u);
    int weapons = 0, total = 0;
    for (int i = 0; i < 400; i++) {
        EnemyKind k = (EnemyKind)(i % ENEMY_COUNT);
        LootItem it[16];
        bool armed = i % 3 != 0;
        int n = enemy_loot(k, armed, true, &rng, it, 16);
        CHECK(n >= 0 && n <= 16);
        total += n;
        for (int j = 0; j < n; j++) {
            CHECK(it[j].id && it[j].id[0] && it[j].count > 0 && it[j].condition > 0.0f && it[j].condition <= 1.0f);
            bool weapon = enemy_def(k)->weapon && !strcmp(it[j].id, enemy_def(k)->weapon);
            CHECK(armed || !weapon); // desarmado no deja el arma que ya solto
            weapons += weapon;
        }
    }
    CHECK(weapons > 50 && total > 400);
    LootItem one[1];
    CHECK(enemy_loot(ENEMY_ARCHER, true, false, &rng, one, 1) <= 1); // respeta el maximo
    // Jinete: a caballo, rapido; la inercia crece con la velocidad y tiene techo.
    CHECK(enemy_def(ENEMY_RIDER)->mounted && !enemy_def(ENEMY_BANDIT)->mounted);
    CHECK(enemy_def(ENEMY_RIDER)->speed > enemy_def(ENEMY_BANDIT)->speed);
    CHECK(mounted_momentum(0.0f) == 1.0f && mounted_momentum(6.0f) > 1.3f && mounted_momentum(100.0f) <= 1.81f);
    CHECK(mounted_momentum(-3.0f) == 1.0f);
}

static char *read_file(const char *path);

static void test_progress_levels(void) {
    Progress p;
    progress_init(&p);
    CHECK(p.level == 1 && p.xp == 0.0f);
    CHECK(progress_add(&p, xp_to_next(1) - 1.0f) == 0 && p.level == 1);
    CHECK(progress_add(&p, 2.0f) == 1 && p.level == 2);
    CHECK(xp_to_next(5) > xp_to_next(2)); // cada nivel cuesta mas
    CHECK(progress_add(&p, 1e9f) > 0 && p.level == LEVEL_MAX && p.xp == 0.0f);
    CHECK(progress_add(&p, 100.0f) == 0); // tope
    for (int s = 0; s < XP_SOURCE_COUNT; s++) CHECK(xp_reward((XpSource)s) > 0.0f);
}

static void test_tattoo_tree_is_permanent_and_branches(void) {
    TattooBody b;
    tattoo_body_init(&b);
    CHECK(tattoo_count(&b) == 0);
    // Cada motivo: grados 1, 2 y dos ramas del 3.
    for (int m = 0; m < MOTIF_COUNT; m++) {
        int tiers[4] = { 0 };
        for (int i = 0; i < TATTOO_NODES; i++)
            if ((int)tattoo_node(i)->motif == m) tiers[tattoo_node(i)->tier]++;
        CHECK(tiers[1] == 1 && tiers[2] == 1 && tiers[3] == 2);
    }
    int w1 = 0, w2 = 1, w3a = 2, w3b = 3; // lobo
    CHECK(tattoo_node(w1)->motif == MOTIF_WOLF && tattoo_node(w3b)->branch == 2);
    CHECK(tattoo_can(&b, w2, TZ_LEGS, 10) == TAT_NEEDS_PREV); // primero el grado 1
    CHECK(tattoo_apply(&b, w1, TZ_LEGS, 1) == TAT_OK);
    CHECK(tattoo_apply(&b, w1, TZ_ARM_R, 1) == TAT_HAVE_IT);
    CHECK(tattoo_can(&b, w2, TZ_LEGS, 10) == TAT_ZONE_TAKEN); // la zona ya esta tatuada
    CHECK(tattoo_can(&b, w2, TZ_BACK, 2) == TAT_LEVEL);
    CHECK(tattoo_apply(&b, w2, TZ_BACK, 3) == TAT_OK);
    CHECK(tattoo_apply(&b, w3a, TZ_ARM_R, 6) == TAT_OK);
    CHECK(tattoo_can(&b, w3b, TZ_HEAD, 20) == TAT_OTHER_BRANCH); // la otra rama queda cerrada
    CHECK(tattoo_count(&b) == 3);
    // La zona cambia el bonus: el sigilo pesa mas en las piernas que en la espalda.
    float legs[STAT_COUNT] = { 0 }, chest[STAT_COUNT] = { 0 };
    tattoo_effect(w1, TZ_LEGS, legs, NULL);
    tattoo_effect(w1, TZ_CHEST, chest, NULL);
    CHECK(legs[STAT_STEALTH] > chest[STAT_STEALTH]);
    CHECK(legs[STAT_SPEED] > 0.0f && chest[STAT_HEALTH] > 0.0f); // y cada zona suma lo suyo
    CHECK(tattoo_stat(&b, STAT_STEALTH) > 0.3f);
    float pot = 0.0f, mods[STAT_COUNT] = { 0 };
    CHECK(tattoo_effect(w3a, TZ_ARM_R, mods, &pot) == ABIL_HOWL && pot >= 1.0f);
    char d[160];
    CHECK(tattoo_describe(w1, TZ_LEGS, d, sizeof(d)) > 0 && strstr(d, "sigilo"));
    for (int t = 1; t <= 3; t++) CHECK(tattoo_ink(t)[0].id != NULL);
    for (int a = 1; a < ABIL_COUNT; a++) CHECK(ability_def((AbilityId)a)->cooldown > 0.0f);
}

static void test_jewelry_crafted_and_enchanted(void) {
    JewelType t;
    JewelMetal m;
    CHECK(jewel_parse("accesorio.anillo.oro", &t, &m) && t == JT_RING && m == METAL_GOLD);
    CHECK(!jewel_parse("accesorio.amuleto.lobo", &t, &m) && !jewel_parse("arma.corta.sable", NULL, NULL));
    CHECK(!strcmp(jewel_id(JT_BUCKLE, METAL_SILVER), "accesorio.hebilla.plata"));
    for (int g = 1; g < GEM_COUNT; g++) CHECK((int)gem_from_id(gem_id((Gem)g)) == g && gem_active((Gem)g) != ABIL_NONE);
    unsigned short v = jewel_var(GEM_JADE, ENCH_ACTIVE);
    CHECK(jewel_gem(v) == GEM_JADE && jewel_enchant(v) == ENCH_ACTIVE);
    // Sin encantar no hace nada; encantada, el oro rinde mas que el bronce.
    float mods[STAT_COUNT] = { 0 };
    CHECK(jewel_effect("accesorio.collar.oro", jewel_var(GEM_LAPIS, ENCH_NONE), mods, NULL) == ABIL_NONE && mods[STAT_PERCEPTION] == 0.0f);
    float gold[STAT_COUNT] = { 0 }, bronze[STAT_COUNT] = { 0 };
    jewel_effect("accesorio.collar.oro", jewel_var(GEM_LAPIS, ENCH_PASSIVE), gold, NULL);
    jewel_effect("accesorio.collar.bronce", jewel_var(GEM_LAPIS, ENCH_PASSIVE), bronze, NULL);
    CHECK(gold[STAT_PERCEPTION] > bronze[STAT_PERCEPTION] && bronze[STAT_PERCEPTION] > 0.0f);
    float pot = 0.0f;
    CHECK(jewel_effect("accesorio.anillo.plata", v, mods, &pot) == ABIL_SHADOW && pot > 0.0f);
    // Huecos: cuatro anillos, dos brazaletes; los colgantes de animales van al collar.
    CHECK(jewel_fits("accesorio.anillo.oro", JS_RING_R2) && !jewel_fits("accesorio.anillo.oro", JS_NECKLACE));
    CHECK(jewel_fits("accesorio.amuleto.lobo", JS_NECKLACE) && !jewel_fits("accesorio.amuleto.lobo", JS_BUCKLE));
    Jewelry j;
    memset(&j, 0, sizeof(j));
    snprintf(j.slot[JS_RING_L1].id, INV_ID_LEN, "accesorio.anillo.bronce");
    j.slot[JS_RING_L1].var = jewel_var(GEM_CARNELIAN, ENCH_PASSIVE);
    snprintf(j.slot[JS_NECKLACE].id, INV_ID_LEN, "accesorio.amuleto.lobo");
    CHECK(jewelry_stat(&j, STAT_MELEE) > 0.0f && jewelry_stat(&j, STAT_STEALTH) > 0.25f);
    // Las joyas con piedras distintas no se apilan en la mochila.
    Inventory inv;
    char *text = read_file(ESTEPA_SOURCE_DIR "/assets/inventario.tsv");
    CHECK(text != NULL);
    if (!text) return;
    inventory_parse(&inv, text);
    Bag bag;
    bag_init(&bag, BAG_BACKPACK, 25.0f);
    CHECK(bag_add_var(&bag, &inv, "accesorio.anillo.oro", 1, 1.0f, jewel_var(GEM_JADE, ENCH_NONE)) == 1);
    CHECK(bag_add_var(&bag, &inv, "accesorio.anillo.oro", 1, 1.0f, jewel_var(GEM_PEARL, ENCH_NONE)) == 1);
    CHECK(bag.n == 2);
    CHECK(bag_add_var(&bag, &inv, "accesorio.anillo.oro", 1, 1.0f, jewel_var(GEM_JADE, ENCH_NONE)) == 1 && bag.n == 2);
    for (int g = 1; g < GEM_COUNT; g++) CHECK(inventory_find(&inv, gem_id((Gem)g)) != NULL);
    for (int a = 0; a < JT_COUNT; a++)
        for (int b = 0; b < METAL_COUNT; b++) CHECK(inventory_find(&inv, jewel_id((JewelType)a, (JewelMetal)b)) != NULL);
    for (int b = 0; b < METAL_COUNT; b++) CHECK(inventory_find(&inv, metal_id((JewelMetal)b)) != NULL);
    inventory_free(&inv);
    free(text);
    // Las piedras: los corales dan mas que el rio.
    Rng rng;
    rng_seed(&rng, 9u);
    int coral = 0, river = 0;
    for (int i = 0; i < 2000; i++) {
        coral += gem_roll(GEMSRC_CORAL, &rng) != GEM_NONE;
        river += gem_roll(GEMSRC_RIVER, &rng) != GEM_NONE;
    }
    CHECK(coral > river && river > 0);
}

static void test_lang_table(void) {
    CHECK(lang_get() == LANG_ES && !strcmp(T("Vacío"), "Vacío"));
    CHECK(lang_load(LANG_EN, "# comentario\nVacío\tEmpty\nDía %d\tDay %d\nDos\\nlineas\tTwo\\nlines\n") == 3);
    lang_set(LANG_EN);
    CHECK(!strcmp(T("Vacío"), "Empty") && !strcmp(T("Día %d"), "Day %d") && !strcmp(T("Dos\nlineas"), "Two\nlines"));
    CHECK(!strcmp(T("Sin traducir"), "Sin traducir")); // si falta, el español
    lang_set(LANG_ES);
    CHECK(!strcmp(T("Vacío"), "Vacío"));
    lang_free();
}

static void test_camps_found_tasks_and_rates(void) {
    CampSite c[CAMPS_MAX];
    camps_init(c, CAMPS_MAX);
    CHECK(camps_count(c, CAMPS_MAX) == 0 && camp_at(c, CAMPS_MAX, 0, 0) < 0);
    int a = camp_found(c, CAMPS_MAX, 0.0f, 0.0f, 1);
    CHECK(a == 0 && c[0].guardian == -1 && camp_at(c, CAMPS_MAX, 10.0f, 5.0f) == 0);
    CHECK(camp_found(c, CAMPS_MAX, 30.0f, 0.0f, 1) < 0); // demasiado cerca de otro
    int b = camp_found(c, CAMPS_MAX, 200.0f, 0.0f, 2);
    CHECK(b == 1 && camps_count(c, CAMPS_MAX) == 2);
    float d;
    CHECK(camp_nearest(c, CAMPS_MAX, 150.0f, 0.0f, &d) == 1 && d < 60.0f);
    CHECK(camp_at(c, CAMPS_MAX, 100.0f, 0.0f) < 0); // entre los dos: fuera de ambos
    // Oficios que se enseñan: cuestan algo y tardan; los del fuego, mas.
    int trainable = 0;
    for (int r = 0; r < ROLE_COUNT; r++) {
        if (!role_trainable((Role)r)) continue;
        trainable++;
        CHECK(train_work((Role)r) > 0.0f && train_cost((Role)r)[0].id != NULL);
    }
    CHECK(trainable == 7 && train_work(ROLE_SMITH) > train_work(ROLE_HERDER) && !role_trainable(ROLE_LIEUTENANT));
    CHECK(recruit_cost()[0].id && recruit_work() > 0.0f);
    // Mas manos, mas rapido (con tope); un maestro, mucho mas.
    CHECK(task_rate(0, 0) == 1.0f && task_rate(2, 0) > task_rate(1, 0) && task_rate(20, 0) <= 2.0f && task_rate(0, 1) > task_rate(2, 0));
    CHECK(camp_add_task(&c[0], TASK_TRAIN, 7, ROLE_DRUID) && !camp_add_task(&c[0], TASK_RECRUIT, 7, ROLE_NONE)); // ya ocupado
    CHECK(camp_member_busy(&c[0], 7) && !camp_member_busy(&c[0], 8));
    CHECK(camp_add_task(&c[0], TASK_RECRUIT, 8, ROLE_NONE));
    CampTask done[CAMP_TASKS];
    float rates[CAMP_TASKS] = { 1.0f, 100.0f };
    CHECK(camp_tick(&c[0], 3.0f, rates, done, CAMP_TASKS) == 1 && done[0].kind == TASK_RECRUIT && c[0].ntask == 1);
    CHECK(camp_tick(&c[0], train_work(ROLE_DRUID), NULL, done, CAMP_TASKS) == 1 && done[0].role == ROLE_DRUID && c[0].ntask == 0);
}

static void test_backpacks_and_feeding(void) {
    // Mochilas: mas grandes, mas caben, pero frenan.
    Bag b;
    memset(&b, 0, sizeof(b));
    bag_init_pack(&b, PACK_SMALL);
    float small = b.cap_kg;
    int small_slots = b.slots;
    bag_init_pack(&b, PACK_LARGE);
    CHECK(b.cap_kg > small && b.slots > small_slots && b.kind == BAG_BACKPACK);
    CHECK(pack_speed(PACK_LARGE) < pack_speed(PACK_MEDIUM) && pack_speed(PACK_MEDIUM) <= pack_speed(PACK_SMALL) && pack_speed(PACK_NONE) == 1.0f);
    CHECK(pack_size_of("utileria.mochila.grande") == PACK_LARGE && pack_size_of("utileria.mochila.pequena") == PACK_SMALL);
    CHECK(pack_size_of("arma.corta.sable") == PACK_NONE && !strcmp(pack_id(PACK_MEDIUM), "utileria.mochila.mediana"));
    bag_init_pack(&b, PACK_NONE);
    CHECK(b.cap_kg == 0.0f && b.slots == 0);
    // Animales de la tribu: el hambre sube; los herbivoros pastan si hay pasto; se les da forraje o carne.
    Rng rng;
    rng_seed(&rng, 12u);
    Animal herd[2];
    memset(herd, 0, sizeof(herd));
    animal_init(&herd[0], SPECIES_HORSE, 0.0f, 0.0f);
    herd[0].state = ANIMAL_TAMED;
    animal_init(&herd[1], SPECIES_DOG, 3.0f, 0.0f);
    herd[1].state = ANIMAL_TAMED;
    FaunaHuman hu = { 200.0f, 200.0f, false, false, false };
    FaunaCtx bare = { &hu, 1, 200.0f, 200.0f, false, 0.0f, 0.0f, false, NULL, NULL, false };
    herd[0].hunger = herd[1].hunger = 0.5f;
    for (int i = 0; i < 600; i++) fauna_update(herd, 2, &bare, &rng, 0.5f, NULL); // 5 minutos sin pasto
    CHECK(herd[0].hunger > 0.6f);
    float h0 = herd[0].hunger;
    FaunaCtx green = bare;
    green.grass = true;
    for (int i = 0; i < 600; i++) fauna_update(herd, 1, &green, &rng, 0.5f, NULL);
    CHECK(herd[0].hunger < h0); // con pasto, pasta y se le pasa
    CHECK(!species_eats_meat(SPECIES_HORSE) && species_eats_meat(SPECIES_DOG) && animal_domestic(&herd[1]));
    herd[1].hunger = 0.9f;
    CHECK(!animal_feed_tamed(&herd[1], false) && animal_feed_tamed(&herd[1], true) && herd[1].hunger == 0.0f);
    // Mucha hambre: se va (vuelve a ser salvaje).
    herd[0].hunger = 1.99f;
    FaunaEvents ev = { 0 };
    for (int i = 0; i < 400 && herd[0].state != ANIMAL_WILD; i++) fauna_update(herd, 1, &bare, &rng, 0.5f, &ev);
    CHECK(herd[0].state == ANIMAL_WILD);
    bool starved = false;
    for (int i = 0; i < ev.n; i++) starved |= ev.ev[i].kind == FEV_STARVED;
    CHECK(starved);
}

static void test_apparel_and_climate(void) {
    // Prendas por capa: poner una devuelve la que habia.
    Outfit o;
    outfit_clear(&o);
    int deel = garment_find("vestimenta.torso.deel"), silk = garment_find("vestimenta.torso.tunica_seda");
    int bear = garment_find("vestimenta.espalda.abrigo_oso"), hat = garment_find("vestimenta.cabeza.sombrero");
    int fur_hat = garment_find("vestimenta.cabeza.gorro_piel"), scarf = garment_find("vestimenta.cuello.panuelo_desierto");
    CHECK(deel >= 0 && silk >= 0 && bear >= 0 && hat >= 0 && fur_hat >= 0 && scarf >= 0 && garment_find("arma.corta.sable") < 0);
    CHECK(outfit_wear(&o, deel) == -1 && outfit_wear(&o, silk) == deel && o.g[WEAR_BODY] == silk);
    CHECK(outfit_warmth(&o) == garment(silk)->warmth);
    // La sombra se combina sin pasar de 1; el escarmiento tiene tope.
    outfit_wear(&o, hat);
    outfit_wear(&o, scarf);
    float shade = outfit_shade(&o);
    CHECK(shade > garment(hat)->shade && shade < 1.0f);
    Outfit fierce;
    outfit_clear(&fierce);
    outfit_wear(&fierce, garment_find("vestimenta.espalda.abrigo_tigre"));
    outfit_wear(&fierce, garment_find("vestimenta.cabeza.gorro_lobo"));
    outfit_wear(&fierce, garment_find("vestimenta.cuello.bufanda_piel"));
    CHECK(outfit_dread(&fierce) > 0.4f && outfit_dread(&fierce) <= 0.5f && outfit_dread(&o) == 0.0f);
    CHECK(outfit_speed_scale(&fierce) < 1.0f && outfit_speed_scale(&o) == 1.0f);
    // Las pieles de los depredadores (y del reno y la cabra) tienen nombre; las demas, curtidas.
    CHECK(!strcmp(species_pelt(SPECIES_WOLF), "utileria.piel.lobo") && !strcmp(species_pelt(SPECIES_BEAR), "utileria.piel.oso"));
    CHECK(species_pelt(SPECIES_HARE) == NULL && species_pelt(SPECIES_GOAT) != NULL);
    // El desierto quema de dia y hiela de noche.
    CHECK(local_temperature(25.0f, 1.0f, 1.0f) > local_temperature(25.0f, 1.0f, 0.0f) + 8.0f);
    CHECK(local_temperature(5.0f, -1.0f, 1.0f) < local_temperature(5.0f, -1.0f, 0.0f) - 5.0f);
    CHECK(sun_strength(1.0f, 0.0f, 1.0f) > sun_strength(1.0f, 1.0f, 1.0f) && sun_strength(-0.5f, 0.0f, 1.0f) == 0.0f);
    // Al sol del desierto, la seda blanca y el sombrero refrescan; el deel y el abrigo de oso sofocan.
    Outfit heavy;
    outfit_clear(&heavy);
    outfit_wear(&heavy, deel);
    outfit_wear(&heavy, bear);
    float sun = sun_strength(1.0f, 0.0f, 1.0f), hot_temp = local_temperature(28.0f, 1.0f, 1.0f);
    float cool = apparel_feels_like(hot_temp, 0.1f, 0.0f, sun, &o, 0.0f, 0.0f);
    float stifling = apparel_feels_like(hot_temp, 0.1f, 0.0f, sun, &heavy, 0.0f, 0.0f);
    CHECK(cool < stifling - 15.0f);
    // En el frio, al reves; la ropa de lluvia moja menos.
    CHECK(apparel_feels_like(-15.0f, 0.6f, 0.0f, 0.0f, &heavy, 0.0f, 0.0f) > apparel_feels_like(-15.0f, 0.6f, 0.0f, 0.0f, &o, 0.0f, 0.0f) + 15.0f);
    CHECK(apparel_wet_scale(&heavy) < apparel_wet_scale(&o) && apparel_wet_scale(NULL) == 1.0f);
    // Calor: sube al sol y baja a la sombra; con golpe de calor se va mas lento y da mas sed.
    HeatStress h = { 0.0f };
    for (int i = 0; i < 1200; i++) heat_update(&h, stifling, 0.0f, false, 0.5f);
    CHECK(heat_level(&h) == HEAT_STROKE && heat_speed_scale(&h) < 0.7f && heat_thirst_scale(&h) > 2.0f);
    for (int i = 0; i < 240; i++) heat_update(&h, 20.0f, 0.0f, true, 0.5f);
    CHECK(heat_level(&h) == HEAT_FINE && heat_speed_scale(&h) == 1.0f);
    HeatStress dry = { 60.0f }, wet = { 60.0f };
    heat_update(&dry, 38.0f, 0.0f, false, 10.0f);
    heat_update(&wet, 38.0f, 1.0f, false, 10.0f);
    CHECK(wet.load < dry.load);
    // Los NPCs eligen: con frio, el abrigo de oso; con sol fuerte, el sombrero y nada de abrigo.
    int cloaks[3] = { garment_find("vestimenta.espalda.capa"), bear, garment_find("vestimenta.espalda.manto_blanco") };
    float cold_need = comfort_need(-20.0f, 0.5f, 0.0f), hot_need = comfort_need(36.0f, 0.1f, 1.0f);
    CHECK(cold_need > 20.0f && hot_need < 0.0f);
    CHECK(garment_pick(WEAR_CLOAK, cold_need, 0.0f, cloaks, 3) == 1);
    CHECK(garment_pick(WEAR_CLOAK, hot_need, 1.0f, cloaks, 3) == 2); // el manto blanco da sombra
    int hats[2] = { fur_hat, hat };
    CHECK(garment_pick(WEAR_HEAD, hot_need, 1.0f, hats, 2) == 1 && garment_pick(WEAR_HEAD, cold_need, 0.0f, hats, 2) == 0);
    int only_bear[1] = { bear };
    CHECK(garment_pick(WEAR_CLOAK, hot_need, 1.0f, only_bear, 1) == -1); // mejor nada que el abrigo
    CHECK(garment_pick(WEAR_HEAD, cold_need, 0.0f, only_bear, 1) == -1); // no es de esa capa
    // Toda la ropa existe en el inventario y se fabrica (o ya existia) con materiales que existen.
    Inventory inv;
    char *text = read_file(ESTEPA_SOURCE_DIR "/assets/inventario.tsv");
    CHECK(text != NULL);
    if (!text) return;
    inventory_parse(&inv, text);
    for (int i = 0; i < garment_count(); i++) CHECK(inventory_find(&inv, garment(i)->id) != NULL);
    int clothing = 0;
    for (int c = 0; c < CRAFT_COUNT; c++) {
        const CraftDef *d = craft_def((CraftId)c);
        if (garment_find(d->produces) < 0) continue;
        clothing++;
        CHECK(craft_by_hand(d));
        for (const Ingredient *m = d->mats; m->id; m++) CHECK(inventory_find(&inv, m->id) != NULL);
    }
    CHECK(clothing >= 18);
    inventory_free(&inv);
}

static float test_water_everywhere(void *ud, float x, float z) {
    (void)ud, (void)x, (void)z;
    return 1.0f;
}

static void test_water_spirits_and_drink(void) {
    Rng rng;
    rng_seed(&rng, 7u);
    Hydration h;
    hydration_init(&h);
    CHECK(h.water == THIRST_MAX && thirst_level(&h) == THIRST_FINE && drunk_level(&h) == DRUNK_SOBER);
    // La sed baja, mas rapido con calor.
    Hydration cool = h, hot = h;
    for (int i = 0; i < 600; i++) hydration_update(&cool, 1.0f, false, 1.0f), hydration_update(&hot, 2.5f, false, 1.0f);
    CHECK(hot.water < cool.water && cool.water < THIRST_MAX && thirst_level(&hot) >= THIRST_THIRSTY);
    CHECK(hydration_speed_scale(&hot) < 1.0f && hydration_stamina_scale(&hot) < 1.0f);
    for (int i = 0; i < 3000; i++) hydration_update(&hot, 2.5f, false, 1.0f);
    CHECK(hot.water == 0.0f && thirst_level(&hot) == THIRST_DRY);
    // Lo hervido y el alcohol no traen espiritus; lo crudo, a veces (cuanto mas calor, mas).
    int sick = 0;
    for (int i = 0; i < 200; i++) {
        Hydration b;
        hydration_init(&b);
        b.water = 10.0f;
        CHECK(!hydration_drink(&b, DRINK_BOILED, 1.0f, &rng) && !hydration_drink(&b, DRINK_BEER, 1.0f, &rng));
        CHECK(!hydration_drink(&b, DRINK_WATERED_WINE, 1.0f, &rng) && b.curse == 0.0f);
        Hydration r;
        hydration_init(&r);
        sick += hydration_drink(&r, DRINK_RAW, water_spirit_chance(20.0f, true), &rng);
    }
    CHECK(sick > 40 && sick < 160);
    CHECK(water_spirit_chance(25.0f, true) > water_spirit_chance(0.0f, false));
    CHECK(water_spirit_chance(0.0f, false) > 0.0f);
    // Los espiritus se incuban, dan fiebre y se van (antes con hierbas).
    Hydration c;
    hydration_init(&c);
    CHECK(hydration_drink(&c, DRINK_RAW, 1.0f, &rng) && !hydration_sick(&c) && c.incubate >= 60.0f);
    for (int i = 0; i < 130; i++) hydration_update(&c, 1.0f, false, 1.0f);
    CHECK(hydration_sick(&c) && hydration_speed_scale(&c) < 1.0f);
    Hydration d = c;
    hydration_herbs(&d);
    CHECK(d.curse < c.curse);
    for (int i = 0; i < 1300; i++) hydration_update(&c, 1.0f, true, 1.0f);
    CHECK(!hydration_sick(&c));
    // Las hierbas durante la incubacion casi la cortan.
    Hydration e;
    hydration_init(&e);
    hydration_drink(&e, DRINK_RAW, 1.0f, &rng);
    hydration_herbs(&e);
    hydration_herbs(&e);
    CHECK(e.curse == 0.0f);
    // Beber mucho vino emborracha: torpe, fatigado; se pasa con el tiempo.
    Hydration w;
    hydration_init(&w);
    for (int i = 0; i < 4; i++) hydration_drink(&w, DRINK_WINE, 0.0f, &rng);
    CHECK(drunk_level(&w) >= DRUNK_WASTED && hydration_clumsy(&w) > 0.9f && hydration_stamina_scale(&w) < 0.6f);
    Hydration one;
    hydration_init(&one);
    hydration_drink(&one, DRINK_BEER, 0.0f, &rng);
    CHECK(drunk_level(&one) == DRUNK_SOBER && hydration_clumsy(&one) == 0.0f); // una cerveza: nada
    for (int i = 0; i < 800; i++) hydration_update(&w, 1.0f, true, 1.0f);
    CHECK(drunk_level(&w) == DRUNK_SOBER);
    CHECK(drink_find("utileria.consumible.agua") == DRINK_RAW && drink_find("utileria.consumible.vino") == DRINK_WINE && drink_find("arma.corta.sable") < 0);
    // La tribu: del acopio lo seguro; si falta, del rio hervida con leña; sin leña, cruda; sin agua, sed.
    Troop t;
    troop_init(&t, NULL);
    for (int i = 0; i < 8; i++) troop_recruit(&t, "Bebedor", 0);
    Stockpile s;
    stock_init(&s);
    stock_add(&s, "utileria.consumible.agua_hervida", 2);
    stock_add(&s, "utileria.consumible.cerveza", 1);
    stock_add(&s, "utileria.objeto.lena", 1);
    WaterReport r = water_daily(&s, &t, true, 1.0f, &rng);
    CHECK(r.safe == 3 && r.boiled == WATER_WOOD_PER && r.raw == 8 - 3 - WATER_WOOD_PER && r.sick == r.raw && r.dry == 0);
    CHECK(stock_count(&s, "utileria.objeto.lena") == 0 && stock_count(&s, "utileria.consumible.cerveza") == 0);
    float morale = t.members[7].morale;
    WaterReport none = water_daily(&s, &t, false, 1.0f, &rng);
    CHECK(none.dry == 8 && t.members[7].morale < morale);
    for (int i = 0; i < 20; i++) water_daily(&s, &t, false, 1.0f, &rng);
    CHECK(t.members[0].health.hp >= t.members[0].health.hp_max * 0.3f - 0.01f); // nadie muere de esto
    // Los animales de la tribu tienen sed; junto al agua beben solos.
    Animal a;
    memset(&a, 0, sizeof(a));
    animal_init(&a, SPECIES_HORSE, 0.0f, 0.0f);
    a.state = ANIMAL_TAMED;
    CHECK(!animal_give_water(&a));
    a.thirst = 1.2f;
    CHECK(animal_give_water(&a) && a.thirst == 0.0f);
    // En seco la sed sube (y avisa); junto al agua bebe solo.
    FaunaHuman hu = { 300.0f, 300.0f, false, false, false };
    FaunaCtx dry;
    memset(&dry, 0, sizeof(dry));
    dry.humans = &hu, dry.human_count = 1, dry.px = dry.pz = 300.0f, dry.grass = true;
    FaunaEvents ev = { 0 };
    a.thirst = 0.95f;
    for (int i = 0; i < 40; i++) fauna_update(&a, 1, &dry, &rng, 0.5f, &ev);
    bool told = false;
    for (int i = 0; i < ev.n; i++) told |= ev.ev[i].kind == FEV_THIRSTY;
    CHECK(a.thirst > 1.0f && told);
    FaunaCtx wet = dry;
    wet.water_depth = test_water_everywhere;
    for (int i = 0; i < 40; i++) fauna_update(&a, 1, &wet, &rng, 0.5f, NULL);
    CHECK(a.thirst < 0.5f);
}

static void test_world_regions_borders_and_sites(void) {
    static World a, b, c;
    world_generate(&a, 1206u);
    world_generate(&b, 1206u);
    world_generate(&c, 77u);
    // Determinista por semilla; otra semilla mueve rios, lagos, montañas y el borde.
    CHECK(a.lake_count == b.lake_count && a.river_count == b.river_count && world_height(&a, 333.0f, -777.0f) == world_height(&b, 333.0f, -777.0f));
    CHECK(a.lakes[1].x != c.lakes[1].x || a.rivers[0].x[0] != c.rivers[0].x[0]);
    CHECK(world_edge_radius(&a, 1.0f) != world_edge_radius(&c, 1.0f));
    const World *ws[2] = { &a, &c };
    for (int k = 0; k < 2; k++) {
        const World *w = ws[k];
        // El borde es fractal: varia alrededor del radio medio, sin saltos.
        float rmin = 1e9f, rmax = 0.0f;
        for (int i = 0; i < 720; i++) {
            float r = world_edge_radius(w, (float)i * 0.5f * 0.0174533f);
            rmin = fminf(rmin, r), rmax = fmaxf(rmax, r);
        }
        CHECK(rmax - rmin > 0.05f * WORLD_RADIUS && rmin > 0.8f * WORLD_RADIUS && rmax < 1.2f * WORLD_RADIUS);
        // La estepa al centro; el bosque al noroeste, el desierto al noreste, el altiplano al
        // sureste y la costa al suroeste (el norte es -Z).
        CHECK(world_region(w, 0.0f, 0.0f) == REGION_STEPPE && world_region(w, 500.0f, -400.0f) == REGION_STEPPE);
        CHECK(world_region(w, -2600.0f, -2600.0f) == REGION_FOREST && world_region(w, 2600.0f, -2600.0f) == REGION_DESERT);
        CHECK(world_region(w, 2600.0f, 2600.0f) == REGION_HIGHLAND && world_region(w, -2600.0f, 2600.0f) == REGION_FJORD);
        // Altitud media estandarizada: la costa abajo, el altiplano arriba.
        double sum[REGION_COUNT] = { 0 };
        int n[REGION_COUNT] = { 0 };
        for (float x = -4400.0f; x < 4400.0f; x += 120.0f)
            for (float z = -4400.0f; z < 4400.0f; z += 120.0f) {
                if (world_edge_distance(w, x, z) < 600.0f) continue;
                float wt[REGION_COUNT];
                Region r = world_region_weights(w, x, z, wt);
                if (wt[r] < 0.9f) continue;
                sum[r] += world_height(w, x, z), n[r]++;
            }
        float mean[REGION_COUNT];
        for (int r = 0; r < REGION_COUNT; r++) {
            CHECK(n[r] > 20);
            mean[r] = n[r] ? (float)(sum[r] / n[r]) : 0.0f;
        }
        CHECK(mean[REGION_FJORD] < mean[REGION_DESERT] && mean[REGION_DESERT] < mean[REGION_STEPPE]);
        CHECK(mean[REGION_STEPPE] < mean[REGION_FOREST] && mean[REGION_FOREST] < mean[REGION_HIGHLAND]);
        // Los bordes: el mar en la costa, el cañon y el muro de estratos en el desierto, el muro
        // de hielo en el altiplano y el canal en el bosque.
        float x, z;
        WaterKind wk;
        world_edge_point(w, REGION_FJORD, 40.0f, &x, &z);
        CHECK(world_height(w, x, z) < SEA_LEVEL && world_water(w, x, z, 0.0f, &wk) == SEA_LEVEL && wk == WATER_SEA);
        world_edge_point(w, REGION_DESERT, 20.0f, &x, &z);
        float wall = world_height(w, x, z);
        CHECK(wall > mean[REGION_DESERT] + 100.0f);
        world_edge_point(w, REGION_DESERT, 150.0f, &x, &z);
        CHECK(world_height(w, x, z) < wall - 120.0f);
        world_edge_point(w, REGION_HIGHLAND, 20.0f, &x, &z);
        CHECK(world_height(w, x, z) > mean[REGION_HIGHLAND] + 80.0f);
        float fa = 225.0f * 0.0174533f - w->warp_phase;
        float cr = world_edge_radius(w, fa) - world_canal_offset(w, fa);
        float lv = world_water(w, cosf(fa) * cr, sinf(fa) * cr, 0.0f, &wk);
        CHECK(wk == WATER_CANAL && lv > world_height(w, cosf(fa) * cr, sinf(fa) * cr) + 1.0f);
        // Los rios: meandros alrededor de la estepa; tributarios que nacen en el canal; trenzados
        // en el altiplano; el agua siempre baja.
        int kinds[RIVER_KINDS] = { 0 };
        for (int i = 0; i < w->river_count; i++) {
            const River *rv = &w->rivers[i];
            kinds[rv->kind]++;
            CHECK(rv->n >= 8);
            if (rv->kind == RIVER_TRIBUTARY) {
                float th = atan2f(rv->z[0], rv->x[0]);
                CHECK(fabsf(world_edge_distance(w, rv->x[0], rv->z[0]) - world_canal_offset(w, th)) < 3.0f);
                for (int j = 1; j < rv->n; j++) CHECK(rv->level[j] >= rv->level[j - 1]);
            }
            if (rv->kind == RIVER_BRAIDED || rv->kind == RIVER_CREEK)
                for (int j = 1; j < rv->n; j++) CHECK(rv->level[j] <= rv->level[j - 1]);
            if (rv->kind == RIVER_MEANDER) { // cerca del contorno de la estepa
                float r0 = sqrtf(rv->x[rv->n / 2] * rv->x[rv->n / 2] + rv->z[rv->n / 2] * rv->z[rv->n / 2]);
                CHECK(r0 > 0.2f * WORLD_RADIUS && r0 < 0.55f * WORLD_RADIUS);
            }
        }
        CHECK(kinds[RIVER_MEANDER] >= 4 && kinds[RIVER_TRIBUTARY] >= 3 && kinds[RIVER_BRAIDED] >= 3 && kinds[RIVER_CREEK] >= 8);
        // Agua cerca del campamento; el campamento, en seco.
        CHECK(sqrtf(w->lakes[0].x * w->lakes[0].x + w->lakes[0].z * w->lakes[0].z) < 260.0f);
        CHECK(world_water(w, 0.0f, 0.0f, 3.0f, NULL) < world_height(w, 0.0f, 0.0f));
        // Asentamientos y guaridas: de su region; una capital por reino; las fieras, de alli.
        int capitals = 0;
        for (int i = 0; i < w->settlement_count; i++) {
            const Settlement *st = &w->settlements[i];
            CHECK(world_region(w, st->x, st->z) == st->region && st->name[0]);
            capitals += st->kind == SETTLE_CAPITAL;
            for (int o = 0; o < i; o++) CHECK(strcmp(st->name, w->settlements[o].name) != 0);
        }
        CHECK(capitals == REGION_COUNT && w->settlement_count >= 15);
        CHECK(w->den_count >= 30);
        for (int i = 0; i < w->den_count; i++) {
            const Den *d = &w->dens[i];
            CHECK(world_region(w, d->x, d->z) == d->region);
            CHECK(species_def((Species)d->species)->habitat & region_habitat(d->region));
        }
        // La frontera invisible: no se pasa el mar abierto, el muro, el hielo ni el canal.
        for (int r = REGION_FOREST; r < REGION_COUNT; r++) {
            world_edge_point(w, (Region)r, -300.0f, &x, &z); // afuera
            CHECK(world_clamp(w, &x, &z) && !world_clamp(w, &x, &z));
            CHECK(world_edge_distance(w, x, z) > 30.0f);
        }
        float px = 100.0f, pz = 100.0f;
        CHECK(!world_clamp(w, &px, &pz));
    }
    CHECK(world_for_seed(1206u) == world_for_seed(1206u) && world_for_seed(1206u)->seed == 1206u);
}

static void test_travel_dispatch_and_messengers(void) {
    // Riesgo: sin nadie que conozca el destino, casi seguro se pierden.
    CHECK(journey_risk(300.0f, 0.0f, 3, false) >= 0.89f);
    float some = journey_risk(300.0f, 0.34f, 3, false), all = journey_risk(300.0f, 1.0f, 3, false);
    CHECK(all < some && some < 0.9f);
    CHECK(journey_risk(300.0f, 1.0f, 6, false) < all);  // en grupo se cuidan
    CHECK(journey_risk(300.0f, 1.0f, 3, true) > all);   // de noche, peor
    CHECK(journey_risk(900.0f, 1.0f, 3, false) > all);  // mas lejos, peor
    CHECK(travel_speed(TRAVEL_MOUNTED) > travel_speed(TRAVEL_FOOT));
    CHECK(journey_eta(400.0f, TRAVEL_MOUNTED) < journey_eta(400.0f, TRAVEL_FOOT));
    // Viajes: hay lugar para JOURNEYS_MAX a la vez.
    Journey list[JOURNEYS_MAX];
    memset(list, 0, sizeof(list));
    int ppl[3] = { 4, 7, 9 };
    int k = journey_start(list, JOURNEYS_MAX, JOURNEY_DISPATCH, ppl, 3, SITE_CAMP(1), 0, 0, 300, 0, TRAVEL_FOOT, 1.0f);
    CHECK(k == 0 && list[0].used && list[0].n == 3 && list[0].people[2] == 9 && list[0].eta > 80.0f);
    CHECK(journey_start(list, JOURNEYS_MAX, JOURNEY_MESSENGER, ppl, 0, 0, 0, 0, 1, 1, TRAVEL_FOOT, 1.0f) == -1);
    for (int i = 1; i < JOURNEYS_MAX; i++) journey_start(list, JOURNEYS_MAX, JOURNEY_MESSENGER, ppl, 1, 0, 0, 0, 10, 0, TRAVEL_FOOT, 1.0f);
    CHECK(journey_start(list, JOURNEYS_MAX, JOURNEY_MESSENGER, ppl, 1, 0, 0, 0, 10, 0, TRAVEL_FOOT, 1.0f) == -1);
    // Suerte: con quien conozca el camino la mayoria llega; sin nadie, muchos no llegan.
    Rng rng;
    rng_seed(&rng, 99u);
    int safe = 0, lost = 0;
    for (int t = 0; t < 400; t++) {
        Journey j = list[0];
        j.knowledge = 1.0f;
        safe += journey_resolve(&j, &rng, false);
        j.knowledge = 0.0f;
        lost += journey_resolve(&j, &rng, false);
        for (int i = 0; i < j.n; i++) CHECK(j.fate[i] >= FATE_OK && j.fate[i] <= FATE_DEAD);
    }
    CHECK(safe > lost && safe > 400 * 3 * 9 / 10);
    // Sitios conocidos.
    uint64_t known = 0;
    CHECK(!site_known(known, SITE_MARK(3)));
    known = site_learn(known, SITE_MARK(3));
    known = site_learn(known, SITE_CAMP(0));
    CHECK(site_known(known, SITE_MARK(3)) && site_known(known, 0) && !site_known(known, 1));
    CHECK(site_learn(known, SITES_MAX) == known && !site_known(known, -1));
}

int main(void) {
    RUN(test_recruit_and_roles);
    RUN(test_progress_levels);
    RUN(test_tattoo_tree_is_permanent_and_branches);
    RUN(test_jewelry_crafted_and_enchanted);
    RUN(test_lang_table);
    RUN(test_camps_found_tasks_and_rates);
    RUN(test_backpacks_and_feeding);
    RUN(test_travel_dispatch_and_messengers);
    RUN(test_apparel_and_climate);
    RUN(test_water_spirits_and_drink);
    RUN(test_world_regions_borders_and_sites);
    RUN(test_hand_crafting_and_repairs);
    RUN(test_enemy_loot_and_mounted_momentum);
    RUN(test_banish_removes_from_active);
    RUN(test_execution_hits_morale_by_trait);
    RUN(test_kingdom_reacts_to_its_values);
    RUN(test_release_prisoner);
    RUN(test_desertion_only_when_unhappy);
    RUN(test_rebellion_grows_as_morale_falls);
    RUN(test_process_day_is_deterministic);
    RUN(test_share_loot_lifts_spirits);
    RUN(test_amulets_swap_and_stack);
    RUN(test_tattoos_are_permanent);
    RUN(test_active_buffs_and_skill_tree);
    RUN(test_noise_is_deterministic_and_bounded);
    RUN(test_memmap_starts_dark_and_lights_gradually);
    RUN(test_memmap_frequent_places_are_brighter);
    RUN(test_memmap_forgets_over_days);
    RUN(test_memmap_markers_toggle);
    RUN(test_memmap_covers_whole_world_sparsely);
    RUN(test_champions_are_scarce);
    RUN(test_champion_generation_is_deterministic);
    RUN(test_champion_gifts_shape_stats);
    RUN(test_champion_in_troop);
    RUN(test_champion_desertion_weighs_on_troop);
    RUN(test_hands_grips);
    RUN(test_hands_sheathe_take_throw);
    RUN(test_action_catalog);
    RUN(test_build_needs_crew_and_skills);
    RUN(test_build_skill_morale_and_champions);
    RUN(test_build_advance);
    RUN(test_stockpile);
    RUN(test_daily_upkeep_food_and_gathering);
    RUN(test_camp_effects);
    RUN(test_crafting);
    RUN(test_build_materials_and_crew_presence);
    RUN(test_animals_classes);
    RUN(test_animals_flee_and_wander);
    RUN(test_animals_tame_saddle_ride);
    RUN(test_animals_tame_predator);
    RUN(test_animals_pack_hunt);
    RUN(test_animals_stalk_and_rivals);
    RUN(test_animals_flee_only_critical);
    RUN(test_animals_livestock);
    RUN(test_animals_body);
    RUN(test_health_venom);
    RUN(test_animals_water_and_venom);
    RUN(test_animals_keep_out_of_lakes);
    RUN(test_world_tribes_and_sites);
    RUN(test_voxels);
    RUN(test_swarms);
    RUN(test_fire_spread_and_rain);
    RUN(test_weather_hazards_random);
    RUN(test_melee_moves);
    RUN(test_hands_swap_and_dual);
    RUN(test_storage_bags);
    RUN(test_amulets);
    RUN(test_inventory_parses_and_maps_paths);
    RUN(test_anim_index_states);
    RUN(test_anim_index_names_exist);
    RUN(test_inventory_repo_file_is_valid);
    RUN(test_clock_seasonal_day_night);
    RUN(test_climate_seasons_and_weather);
    RUN(test_hazards_cold_mud_ice);
    RUN(test_hazards_sinkholes_and_desert);
    RUN(test_hazards_qte);
    RUN(test_health_wounds_bleeding_healing);
    RUN(test_troop_health_daily);
    RUN(test_combat_weapons_and_enemies);
    RUN(test_body_zones_raycast);
    RUN(test_armor_pieces_materials);
    RUN(test_ballistics_trajectories);
    printf("\n%d comprobaciones, %d fallos\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
