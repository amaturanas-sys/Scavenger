// Tests del nucleo de simulacion. Arnes minimo, sin dependencias.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/sim/actions.h"
#include "../src/sim/anim_index.h"
#include "../src/sim/animals.h"
#include "../src/sim/economy.h"
#include "../src/sim/champion.h"
#include "../src/sim/climate.h"
#include "../src/sim/hazards.h"
#include "../src/sim/clock.h"
#include "../src/sim/inventory.h"
#include "../src/sim/loadout.h"
#include "../src/sim/memory_map.h"
#include "../src/sim/noise.h"
#include "../src/sim/rng.h"
#include "../src/sim/troop.h"

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
    for (float x = -800; x <= 800; x += 40)
        for (float z = -800; z <= 800; z += 40) desert += biome_desert(seed, x, z) > 0.6f;
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
static void test_animals_flee_and_wander(void) {
    Rng r;
    rng_seed(&r, 3);
    Animal a;
    animal_init(&a, SPECIES_DEER, 0, 0);
    for (int i = 0; i < 100; i++) animal_update(&a, 0.1f, 1000.0f, 1000.0f, &r); // jugador lejos: deambula
    CHECK(!a.fleeing && sqrtf(a.x * a.x + a.z * a.z) < 20.0f);
    float before = sqrtf((a.x - 3) * (a.x - 3) + a.z * a.z);
    for (int i = 0; i < 10; i++) animal_update(&a, 0.1f, 3.0f, 0.0f, &r); // jugador encima: huye
    CHECK(a.fleeing && sqrtf((a.x - 3) * (a.x - 3) + a.z * a.z) > before);
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
    CHECK(!animal_try_tame(&horse, &r, 1.0f, 0, 0)); // ya domado
    CHECK(!animal_can_ride(&horse) && animal_saddle(&horse) && animal_can_ride(&horse));
    CHECK(animal_try_tame(&deer, &r, 1.0f, 0, 0) && !animal_saddle(&deer)); // un ciervo no se monta
    // Domado: sigue al jugador cercano.
    for (int i = 0; i < 50; i++) animal_update(&horse, 0.1f, 10.0f, 0.0f, &r);
    CHECK(fabsf(horse.x - 10.0f) < 6.0f);
    CHECK(species_def(SPECIES_HORSE)->ride_speed > 1.0f);
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
        CHECK(inventory_find(&inv, d->produces) != NULL && inventory_find(&inv, d->building) != NULL);
        for (const Ingredient *m = d->mats; m->id; m++) CHECK(inventory_find(&inv, m->id) != NULL);
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
    }
    for (int sp = 0; sp < SPECIES_COUNT; sp++) {
        Animal a;
        animal_init(&a, (Species)sp, 0, 0);
        for (int k = 0; k < 12; k++) {
            a.speed = (float)k;
            a.fleeing = k == 11;
            a.ridden = k >= 9 && k < 11;
            CHECK(clip_indexed(text, "cuadrupedo", anim_quadruped(&a)));
        }
    }
    free(text);
}

int main(void) {
    RUN(test_recruit_and_roles);
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
    RUN(test_animals_flee_and_wander);
    RUN(test_animals_tame_saddle_ride);
    RUN(test_inventory_parses_and_maps_paths);
    RUN(test_anim_index_states);
    RUN(test_anim_index_names_exist);
    RUN(test_inventory_repo_file_is_valid);
    RUN(test_clock_seasonal_day_night);
    RUN(test_climate_seasons_and_weather);
    RUN(test_hazards_cold_mud_ice);
    RUN(test_hazards_sinkholes_and_desert);
    RUN(test_hazards_qte);
    printf("\n%d comprobaciones, %d fallos\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
