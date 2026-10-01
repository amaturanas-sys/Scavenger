// Tests del nucleo de simulacion. Arnes minimo, sin dependencias.
#include <math.h>
#include <stdio.h>
#include <string.h>

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
static MemoryMap g_map; // ~1,8 MB: fuera de la pila

static void walk(MemoryMap *m, float x, float z, float seconds, float *now) {
    for (float t = 0.0f; t < seconds; t += 0.1f) {
        *now += 0.1f;
        memmap_visit(m, x, z, 0.1f, *now);
    }
}

static void test_memmap_starts_dark_and_lights_gradually(void) {
    memmap_init(&g_map);
    float now = 0.0f;
    CHECK(memmap_light(&g_map, 0, 0, now) == 0.0f);
    walk(&g_map, 0, 0, 1.0f, &now);
    float early = memmap_light(&g_map, 0, 0, now);
    CHECK(early > 0.0f && early < 0.3f); // se ilumina poco a poco
    walk(&g_map, 0, 0, 10.0f, &now);
    CHECK(memmap_light(&g_map, 0, 0, now) > early);
    CHECK(memmap_light(&g_map, 100.0f, 0, now) == 0.0f); // lo no visto sigue negro
    // El borde de la vista se memoriza menos que donde se pisa.
    CHECK(memmap_light(&g_map, 18.0f, 0, now) < memmap_light(&g_map, 0, 0, now));
}

static void test_memmap_frequent_places_are_brighter(void) {
    memmap_init(&g_map);
    float now = 0.0f;
    walk(&g_map, -200.0f, 0, 20.0f, &now);  // una pasada breve
    walk(&g_map, 200.0f, 0, 600.0f, &now);  // diez minutos en el mismo lugar
    float rare = memmap_light(&g_map, -200.0f, 0, now + 1.0f);
    float frequent = memmap_light(&g_map, 200.0f, 0, now + 1.0f);
    CHECK(frequent > rare);
    CHECK(frequent > 0.6f);
    CHECK(memmap_familiarity(&g_map, 200.0f, 0) > memmap_familiarity(&g_map, -200.0f, 0));
}

static void test_memmap_fades_and_familiar_fades_slower(void) {
    memmap_init(&g_map);
    float now = 0.0f;
    walk(&g_map, -200.0f, 0, 20.0f, &now);
    walk(&g_map, 200.0f, 0, 600.0f, &now);
    float rare0 = memmap_light(&g_map, -200.0f, 0, now);
    float freq0 = memmap_light(&g_map, 200.0f, 0, now);
    float later = now + 900.0f; // 15 minutos sin volver
    float rare1 = memmap_light(&g_map, -200.0f, 0, later);
    float freq1 = memmap_light(&g_map, 200.0f, 0, later);
    CHECK(rare1 < rare0 && freq1 < freq0); // todo se olvida
    CHECK(freq1 / freq0 > rare1 / rare0);   // lo familiar, mas despacio
    CHECK(memmap_light(&g_map, -200.0f, 0, now + 36000.0f) < 0.01f);
}

static void test_memmap_markers_toggle(void) {
    memmap_init(&g_map);
    CHECK(memmap_toggle_marker(&g_map, 10.0f, 5.0f, MARKER_INTEREST, 6.0f));
    CHECK(g_map.marker_count == 1);
    CHECK(!memmap_toggle_marker(&g_map, 12.0f, 6.0f, MARKER_INTEREST, 6.0f)); // cerca: la quita
    CHECK(g_map.marker_count == 0);
    for (int i = 0; i < MEMMAP_MAX_MARKERS; i++) memmap_toggle_marker(&g_map, i * 50.0f, 0, MARKER_DANGER, 6.0f);
    CHECK(g_map.marker_count == MEMMAP_MAX_MARKERS);
    CHECK(!memmap_toggle_marker(&g_map, 0, 900.0f, MARKER_DANGER, 6.0f)); // lleno
}

static void test_memmap_outside_bounds_is_safe(void) {
    memmap_init(&g_map);
    float now = 0.0f;
    walk(&g_map, 5000.0f, -5000.0f, 2.0f, &now);
    CHECK(memmap_light(&g_map, 5000.0f, -5000.0f, now) == 0.0f);
    walk(&g_map, 760.0f, 760.0f, 2.0f, &now); // en el borde: visita parcial
    CHECK(memmap_light(&g_map, 760.0f, 760.0f, now) > 0.0f);
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
    RUN(test_memmap_fades_and_familiar_fades_slower);
    RUN(test_memmap_markers_toggle);
    RUN(test_memmap_outside_bounds_is_safe);
    printf("\n%d comprobaciones, %d fallos\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
