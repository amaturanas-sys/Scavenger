#include "decor.h"

#include <math.h>
#include <stddef.h>

#include "city.h"
#include "rng.h"

#define PI_D 3.14159265f

// Las chances van por region: estepa, bosque, altiplano, fiordos, desierto. Las medidas son las del inventario
// (las comprueba tests/test_main.c).
static const DecorNorm NORMS[DECOR_KINDS] = {
    // Crestas de roca en capas, de 77 y 112 m: en el altiplano y los fiordos, en terreno quebrado.
    [DECOR_CRESTA] = { "mapa.roca_acantilado.clif_01d", DECOR_CLASS_RIDGE, 76.82f, 22.79f, 48.44f, { 0, 0, 0.45f, 0.30f, 0 },
                       false, 0.06f, 0.6f, true, 0.9f, 1.1f, 0.2f },
    [DECOR_CRESTA_LARGA] = { "mapa.roca_acantilado.clif_01e", DECOR_CLASS_RIDGE, 112.41f, 34.39f, 46.02f, { 0, 0, 0.25f, 0.15f, 0 },
                             false, 0.06f, 0.6f, true, 0.9f, 1.1f, 0.2f },
    // Grandes.
    [DECOR_ROCA_CAPAS] = { "mapa.roca.rock_01c", DECOR_CLASS_BIG, 17.03f, 8.29f, 13.70f, { 0.03f, 0.03f, 0.10f, 0.08f, 0 },
                           false, 0.0f, 0.7f, true, 0.8f, 1.15f, 0.12f },
    [DECOR_ARENISCA_PARTIDA] = { "mapa.roca.rock_sand_01c", DECOR_CLASS_BIG, 15.43f, 9.92f, 7.09f, { 0.01f, 0, 0, 0, 0.10f },
                                 false, 0.0f, 0.6f, false, 0.8f, 1.1f, 0.10f },
    [DECOR_PENASCOS_PANTANO] = { "mapa.roca.swamp_rockbig_02c", DECOR_CLASS_BIG, 19.27f, 7.84f, 12.83f, { 0.06f, 0.08f, 0.04f, 0.08f, 0 },
                                 true, 0.0f, 0.5f, false, 0.8f, 1.1f, 0.15f },
    [DECOR_ACANTILADO] = { "mapa.roca_acantilado.clif_01c", DECOR_CLASS_BIG, 23.33f, 16.70f, 29.73f, { 0, 0.02f, 0.07f, 0.06f, 0.02f },
                           false, 0.15f, 0.9f, true, 0.8f, 1.1f, 0.12f },
    [DECOR_COLINA] = { "mapa.terreno_colina.hill_01a", DECOR_CLASS_BIG, 16.52f, 4.34f, 11.29f, { 0.035f, 0.01f, 0, 0, 0.015f },
                       false, 0.0f, 0.3f, false, 0.85f, 1.15f, 0.08f },
    [DECOR_COLINA_ANCHA] = { "mapa.terreno_colina.hill_01b", DECOR_CLASS_BIG, 20.19f, 4.88f, 15.82f, { 0.03f, 0.01f, 0, 0, 0.01f },
                             false, 0.0f, 0.3f, false, 0.85f, 1.15f, 0.08f },
    [DECOR_REPECHO] = { "mapa.terreno_colina.hill_half_01a", DECOR_CLASS_BIG, 22.32f, 5.43f, 8.73f, { 0.025f, 0.01f, 0.01f, 0, 0.01f },
                        false, 0.05f, 0.4f, false, 0.85f, 1.15f, 0.10f },
    [DECOR_LOMA] = { "mapa.terreno_colina.hill_half_01c", DECOR_CLASS_BIG, 19.06f, 4.52f, 9.42f, { 0.025f, 0.01f, 0.01f, 0, 0.01f },
                     false, 0.05f, 0.4f, false, 0.85f, 1.15f, 0.10f },
    // Medianos.
    [DECOR_LAJA] = { "mapa.roca.rock_flat_01c", DECOR_CLASS_MEDIUM, 7.88f, 1.88f, 6.88f, { 0.02f, 0.01f, 0.04f, 0.04f, 0.01f },
                     false, 0.0f, 0.4f, true, 0.8f, 1.2f, 0.20f },
    [DECOR_PENASCO_ARENISCA] = { "mapa.roca.rock_sand_01e", DECOR_CLASS_MEDIUM, 6.48f, 4.63f, 9.35f, { 0.005f, 0, 0, 0, 0.05f },
                                 false, 0.0f, 0.6f, false, 0.8f, 1.2f, 0.12f },
    [DECOR_LAJAS_PANTANO] = { "mapa.roca.swamp_rockbig_02a", DECOR_CLASS_MEDIUM, 6.48f, 4.63f, 9.35f, { 0.05f, 0.06f, 0.03f, 0.06f, 0 },
                              true, 0.0f, 0.5f, false, 0.8f, 1.2f, 0.15f },
    [DECOR_MONTICULO] = { "mapa.terreno_colina.hill_01d", DECOR_CLASS_MEDIUM, 6.26f, 3.71f, 6.47f, { 0.02f, 0.005f, 0.01f, 0, 0.01f },
                          false, 0.0f, 0.35f, false, 0.8f, 1.15f, 0.10f },
    [DECOR_ROCA_BAJA] = { "mapa.terreno_colina.hill_low_01c", DECOR_CLASS_MEDIUM, 6.04f, 1.62f, 5.86f, { 0.02f, 0.01f, 0.02f, 0.01f, 0.01f },
                          false, 0.0f, 0.35f, false, 0.8f, 1.2f, 0.15f },
    [DECOR_ACACIA] = { "mapa.vegetacion_arbol.tree_05d_1", DECOR_CLASS_MEDIUM, 6.06f, 9.00f, 7.38f, { 0.025f, 0, 0, 0, 0.02f },
                       false, 0.0f, 0.35f, false, 0.85f, 1.15f, 0.02f },
    [DECOR_SAUCE_MUERTO] = { "mapa.vegetacion_arbol.tree_06d_2", DECOR_CLASS_MEDIUM, 4.27f, 8.00f, 4.65f, { 0.04f, 0.03f, 0, 0.02f, 0 },
                             true, 0.0f, 0.4f, false, 0.85f, 1.15f, 0.02f },
    [DECOR_ARBOL_CAIDO] = { "mapa.vegetacion_arbol.tree_old_06_1", DECOR_CLASS_MEDIUM, 6.95f, 7.00f, 5.91f, { 0.005f, 0.04f, 0.01f, 0.01f, 0.02f },
                            false, 0.0f, 0.35f, false, 0.85f, 1.1f, 0.10f }, // las raices gruesas, 70 cm enterradas
    [DECOR_ARBUSTO_SECO] = { "mapa.vegetacion_arbusto.bush_dried_02", DECOR_CLASS_MEDIUM, 2.75f, 1.60f, 1.91f, { 0.05f, 0.005f, 0.01f, 0, 0.07f },
                             false, 0.0f, 0.6f, false, 0.8f, 1.25f, 0.03f },
    // Al pie de los arboles del bosque.
    [DECOR_HONGOS] = { "mapa.vegetacion_seta.mushroom_09", DECOR_CLASS_FOOT, 0.17f, 0.35f, 0.27f, { 0 }, false, 0.0f, 0.8f, false, 0.8f, 1.3f, 0.0f },
    [DECOR_RAMA] = { "utileria.objeto_generico.ea01_env_branch_04", DECOR_CLASS_FOOT, 1.54f, 0.62f, 1.71f, { 0 }, false, 0.0f, 0.8f, false, 0.8f, 1.2f, 0.1f },
    [DECOR_RAIZ] = { "utileria.objeto_generico.ea01_env_treeroot_01c", DECOR_CLASS_FOOT, 2.69f, 2.24f, 2.43f, { 0 }, false, 0.0f, 0.8f, false, 0.7f, 1.0f, 0.1f },
    // Las cercas.
    [DECOR_CERCA_A] = { "estructura.cerca.fence_wood_01a", DECOR_CLASS_HUMAN, 2.60f, 1.50f, 0.53f, { 0 }, false, 0.0f, 0.35f, false, 1, 1, 0.03f },
    [DECOR_CERCA_B] = { "estructura.cerca.fence_wood_01b", DECOR_CLASS_HUMAN, 3.03f, 1.60f, 0.46f, { 0 }, false, 0.0f, 0.35f, false, 1, 1, 0.03f },
    [DECOR_CERCA_ROTA] = { "estructura.cerca.fence_wood_01c", DECOR_CLASS_HUMAN, 2.59f, 1.40f, 0.60f, { 0 }, false, 0.0f, 0.35f, false, 1, 1, 0.03f },
    [DECOR_CERCA_LARGA] = { "estructura.cerca.fence_wood_02b", DECOR_CLASS_HUMAN, 4.43f, 1.80f, 0.43f, { 0 }, false, 0.0f, 0.35f, false, 1, 1, 0.03f },
    [DECOR_CERCA_QUEBRADA] = { "estructura.cerca.fence_wood_02c", DECOR_CLASS_HUMAN, 3.69f, 1.60f, 0.47f, { 0 }, false, 0.0f, 0.35f, false, 1, 1, 0.03f },
    [DECOR_PORTAL] = { "estructura.cerca.fence_wood_03a", DECOR_CLASS_HUMAN, 3.15f, 1.70f, 0.40f, { 0 }, false, 0.0f, 0.35f, false, 1, 1, 0.03f },
    [DECOR_MARCO] = { "estructura.cerca.fence_wood_03b", DECOR_CLASS_HUMAN, 2.44f, 1.60f, 0.37f, { 0 }, false, 0.0f, 0.35f, false, 1, 1, 0.03f },
    [DECOR_CERCA_CAIDA] = { "estructura.cerca.fence_wood_04a", DECOR_CLASS_HUMAN, 2.65f, 1.20f, 0.94f, { 0 }, false, 0.0f, 0.4f, false, 1, 1, 0.05f },
    [DECOR_CERCA_ALTA] = { "estructura.cerca.wood_fence3", DECOR_CLASS_HUMAN, 3.71f, 1.60f, 0.36f, { 0 }, false, 0.0f, 0.35f, false, 1, 1, 0.03f },
    // El agua: el puente sobre un arroyo y el muelle (la tarima en la orilla, un tramo y la punta).
    [DECOR_PUENTE] = { "estructura.puente.bridge_02_1", DECOR_CLASS_HUMAN, 5.91f, 2.28f, 4.30f, { 0 }, false, 0.0f, 0.5f, false, 1, 1, 0.0f },
    [DECOR_TARIMA] = { "estructura.puente.bridge_pier_04b", DECOR_CLASS_HUMAN, 2.24f, 0.48f, 2.06f, { 0 }, false, 0.0f, 0.5f, false, 1, 1, 0.0f },
    [DECOR_MUELLE] = { "estructura.puente.bridge_pier_04c", DECOR_CLASS_HUMAN, 3.58f, 2.06f, 3.35f, { 0 }, false, 0.0f, 1.0f, false, 1, 1, 0.0f },
    [DECOR_MUELLE_FIN] = { "estructura.puente.bridge_pier_end_04a", DECOR_CLASS_HUMAN, 3.54f, 2.05f, 3.35f, { 0 }, false, 0.0f, 1.0f, false, 1, 1, 0.0f },
    // Los enseres.
    [DECOR_CARRO] = { "utileria.generico.prop_wehicule_cart_01", DECOR_CLASS_HUMAN, 4.12f, 1.90f, 1.69f, { 0 }, false, 0.0f, 0.3f, false, 1, 1, 0.02f },
    [DECOR_BANCO] = { "utileria.generico.props_bench_02", DECOR_CLASS_HUMAN, 2.65f, 0.95f, 0.79f, { 0 }, false, 0.0f, 0.3f, false, 1, 1, 0.02f },
    [DECOR_BARRIL] = { "utileria.generico.props_container_barell_01a", DECOR_CLASS_HUMAN, 0.81f, 1.03f, 0.77f, { 0 }, false, 0.0f, 0.35f, false, 1, 1, 0.02f },
    [DECOR_COFRE] = { "utileria.generico.props_container_chest_01a", DECOR_CLASS_HUMAN, 1.02f, 0.75f, 0.66f, { 0 }, false, 0.0f, 0.35f, false, 1, 1, 0.02f },
    [DECOR_COFRE_ABIERTO] = { "utileria.generico.props_container_chest_01b", DECOR_CLASS_HUMAN, 0.97f, 0.90f, 0.90f, { 0 }, false, 0.0f, 0.35f, false, 1, 1, 0.02f },
    [DECOR_TABLAS] = { "utileria.objeto_generico.ea01_env_road_wooden_planks_03b", DECOR_CLASS_HUMAN, 5.34f, 0.25f, 3.07f, { 0 }, false, 0.0f, 0.3f, false, 1, 1, 0.0f },
};

const DecorNorm *decor_norm(DecorKind kind) { return kind >= 0 && kind < DECOR_KINDS ? &NORMS[kind] : NULL; }

float decor_radius(DecorKind kind, float scale) {
    const DecorNorm *n = decor_norm(kind);
    return n ? 0.5f * fmaxf(n->length, n->width) * scale : 0.0f;
}

int decor_pick(DecorClass cls, const float k[REGION_COUNT], bool wet, float r) {
    float acc = 0.0f;
    for (int i = 0; i < DECOR_KINDS; i++) {
        const DecorNorm *n = &NORMS[i];
        if (n->cls != cls || n->wet != wet) continue;
        for (int g = 0; g < REGION_COUNT; g++) acc += k[g] * n->chance[g];
        if (r < acc) return i;
    }
    return DECOR_NONE;
}

int decor_pick_foot(float forest, float r) {
    float p = 0.35f * fmaxf(0.0f, (forest - 0.4f) / 0.6f); // solo bosque adentro (v0.15.3: un suelo mas poblado)
    if (r >= p) return DECOR_NONE;
    float t = r / p;
    return t < 0.5f ? DECOR_HONGOS : t < 0.8f ? DECOR_RAMA : DECOR_RAIZ;
}

void decor_ring(DecorPlace place, float r_place, float *ring0, float *ring1) {
    // Las aldeas y las capitales: fuera de sus campos (los arboles empiezan 6 a 8 m despues, src/world/terrain.c);
    // los campamentos de las tribus: fuera de sus yurtas (26 m); las ruinas: fuera de su sitio.
    float r0 = place == DECOR_PLACE_VILLAGE ? VILLAGE_FIELDS_R1 + 8.0f
             : place == DECOR_PLACE_CAPITAL ? CITY_FIELDS_R1 + 8.0f
             : place == DECOR_PLACE_CAMP    ? 28.0f
                                            : r_place + 2.0f;
    float w = place == DECOR_PLACE_CAPITAL ? 14.0f : place == DECOR_PLACE_CAMP ? 7.0f : 10.0f;
    *ring0 = r0, *ring1 = r0 + w;
}

// Una cerca sana del estilo del lugar, o rota (una de cada cuatro).
static DecorKind fence(Rng *g, DecorKind style) {
    if (rng_float(g) < 0.25f) return rng_float(g) < 0.5f ? DECOR_CERCA_ROTA : DECOR_CERCA_QUEBRADA;
    return style;
}

// Un tramo de cerca de n trozos seguidos por el anillo, desde el angulo a, a la distancia r.
static int fence_run(Rng *g, DecorKind style, float a, float r, int n, DecorSpot *out, int count, int max) {
    for (int i = 0; i < n && count < max; i++) {
        DecorKind k = fence(g, style);
        float half = 0.5f * NORMS[k].length;
        a += half / r;
        out[count++] = (DecorSpot){ a, r, k, true };
        a += (half + 0.15f) / r;
    }
    return count;
}

static int add(DecorSpot *out, int count, int max, float a, float r, DecorKind k) {
    if (count < max) out[count++] = (DecorSpot){ a, r, k, false };
    return count;
}

int decor_layout(DecorPlace place, uint32_t seed, float r_place, DecorSpot *out, int max) {
    if (max > DECOR_SPOTS_MAX) max = DECOR_SPOTS_MAX;
    Rng g;
    rng_seed(&g, seed ^ 0xDEC0u ^ ((uint32_t)place << 24));
    float r0, r1;
    decor_ring(place, r_place, &r0, &r1);
    int n = 0;
#define ANG() (rng_float(&g) * 2.0f * PI_D)
#define RAD() (r0 + rng_float(&g) * (r1 - r0))
    if (place == DECOR_PLACE_VILLAGE || place == DECOR_PLACE_CAPITAL) {
        static const DecorKind STYLE[6] = { DECOR_CERCA_A, DECOR_CERCA_B, DECOR_CERCA_LARGA, DECOR_CERCA_ALTA, DECOR_PORTAL, DECOR_MARCO };
        DecorKind style = STYLE[rng_range(&g, 6)];
        int runs = place == DECOR_PLACE_CAPITAL ? 3 : 2;
        for (int i = 0; i < runs; i++) {
            float a = ANG(), r = r0 + 1.5f + rng_float(&g) * 3.0f; // las cercas, cerca del borde de los campos
            n = fence_run(&g, style, a, r, 3 + rng_range(&g, 3), out, n, max);
        }
        int carts = place == DECOR_PLACE_CAPITAL ? 2 : 1, benches = carts, barrels = place == DECOR_PLACE_CAPITAL ? 3 : 2;
        for (int i = 0; i < carts; i++)
            if (rng_float(&g) < 0.7f) n = add(out, n, max, ANG(), RAD(), DECOR_CARRO);
        for (int i = 0; i < benches; i++)
            if (rng_float(&g) < 0.5f) n = add(out, n, max, ANG(), RAD(), DECOR_BANCO);
        float a = ANG(), r = r0 + 0.5f + rng_float(&g) * (r1 - r0 - 1.0f); // los barriles, juntos (dentro del anillo)
        for (int i = 0; i < barrels; i++) n = add(out, n, max, a + (float)i * 1.3f / r, r + 0.4f * (float)(i & 1), DECOR_BARRIL);
        if (rng_float(&g) < 0.4f) n = add(out, n, max, ANG(), RAD(), DECOR_TABLAS);
        if (rng_float(&g) < 0.3f) n = add(out, n, max, ANG(), RAD(), DECOR_CERCA_CAIDA);
    } else if (place == DECOR_PLACE_CAMP) {
        float a = ANG(), r = 0.5f * (r0 + r1);
        n = fence_run(&g, rng_float(&g) < 0.5f ? DECOR_MARCO : DECOR_CERCA_A, a, r, 4, out, n, max); // el corral
        if (rng_float(&g) < 0.7f) n = add(out, n, max, a + 14.0f / r, r, DECOR_CARRO);
        n = add(out, n, max, a - 3.0f / r, r - 1.0f, DECOR_BARRIL);
        n = add(out, n, max, a - 4.4f / r, r - 0.6f, DECOR_BARRIL);
        if (rng_float(&g) < 0.2f) n = add(out, n, max, a - 6.0f / r, r - 1.2f, DECOR_COFRE);
    } else {
        // Restos alrededor de la ruina: tablas, una cerca caida, barriles, a veces un carro abandonado o un cofre.
        if (rng_float(&g) < 0.6f) n = add(out, n, max, ANG(), RAD(), DECOR_TABLAS);
        if (rng_float(&g) < 0.5f) n = add(out, n, max, ANG(), RAD(), DECOR_CERCA_CAIDA);
        if (rng_float(&g) < 0.4f) n = add(out, n, max, ANG(), RAD(), rng_float(&g) < 0.5f ? DECOR_CERCA_ROTA : DECOR_CERCA_QUEBRADA);
        if (rng_float(&g) < 0.4f) n = add(out, n, max, ANG(), RAD(), DECOR_BARRIL);
        if (rng_float(&g) < 0.25f) n = add(out, n, max, ANG(), RAD(), DECOR_CARRO);
        float c = rng_float(&g);
        if (c < 0.2f) n = add(out, n, max, ANG(), RAD(), DECOR_COFRE_ABIERTO);
        else if (c < 0.35f) n = add(out, n, max, ANG(), RAD(), DECOR_COFRE);
    }
#undef ANG
#undef RAD
    return n;
}

// ---------------------------------------------------------------- lo que se camina
// Las medidas de los modelos, a escala 1 (rayos sobre sus GLB): el tablero del puente va de x -2,35 a 2,35 en arco,
// de 0,55 m en las puntas a 0,95 en el medio, entre las barandas (|z| < 1,6); el de los muelles, a 0,7 m; el de la
// tarima, a 0,44.
#define BRIDGE_END 0.55f
#define BRIDGE_RISE 0.40f

float decor_deck(DecorKind kind, float scale, float stretch, float lx, float lz) {
    float hl, hw;
    switch (kind) {
    case DECOR_PUENTE: hl = DECOR_BRIDGE_HALF * stretch, hw = 1.6f; break;
    case DECOR_MUELLE:
    case DECOR_MUELLE_FIN: hl = 1.78f, hw = 1.55f; break;
    case DECOR_TARIMA: hl = 1.1f, hw = 1.0f; break;
    default: return -1.0f;
    }
    lx /= scale, lz /= scale;
    if (fabsf(lx) > hl || fabsf(lz) > hw) return -1.0f;
    float u = lx / hl;
    return scale * (kind == DECOR_PUENTE ? BRIDGE_END + BRIDGE_RISE * (1.0f - u * u) : kind == DECOR_TARIMA ? 0.44f : 0.70f);
}

DecorHit decor_collider(DecorKind kind, float *radius, float *half) {
    const DecorNorm *n = decor_norm(kind);
    *radius = *half = 0.0f;
    if (!n || n->cls == DECOR_CLASS_FOOT || n->height < 0.45f) return DECOR_HIT_NONE;
    switch (kind) {
    case DECOR_PUENTE:
    case DECOR_TARIMA:
    case DECOR_MUELLE:
    case DECOR_MUELLE_FIN:
    case DECOR_TABLAS:
    case DECOR_CERCA_CAIDA:
    case DECOR_ARBUSTO_SECO: return DECOR_HIT_NONE; // el arbusto se atraviesa, como el pasto (v0.15.3)
    case DECOR_ACACIA:
    case DECOR_SAUCE_MUERTO:
    case DECOR_ARBOL_CAIDO: *radius = 0.45f; return DECOR_HIT_CIRCLE; // el tronco (el arbol seco, de pie desde la v0.15.1)
    case DECOR_PORTAL:
    case DECOR_MARCO: *radius = 0.2f, *half = 0.5f * n->length - 0.2f; return DECOR_HIT_POSTS;
    default: break;
    }
    *radius = fmaxf(0.45f * fminf(n->width, n->length), 0.15f);
    *half = fmaxf(0.45f * n->length - *radius, 0.0f);
    return DECOR_HIT_CAPSULE;
}

// El giro que pone el largo del modelo (su X) a lo largo de (dx, dz).
static float yaw_along(float dx, float dz) { return atan2f(-dz, dx); }

// La distancia desde el centro del cauce, en la direccion (ux, uz), al hombro de la orilla: donde el suelo ya esta a
// DECOR_BRIDGE_CLEAR m o mas sobre el agua y deja de subir (en los 2 m siguientes sube menos de BRIDGE_SHOULDER, y no
// baja de la orilla). -1 si no hay antes de BRIDGE_REACH m.
#define BRIDGE_REACH 6.5f
#define BRIDGE_SHOULDER 1.0f
static float bank_top(const World *w, float x, float z, float ux, float uz, float wl) {
    for (float d = 0.5f; d <= BRIDGE_REACH; d += 0.25f) {
        float h = world_height(w, x + ux * d, z + uz * d);
        if (h < wl + DECOR_BRIDGE_CLEAR) continue;
        float far = world_height(w, x + ux * (d + 2.0f), z + uz * (d + 2.0f)), mid = world_height(w, x + ux * (d + 1.0f), z + uz * (d + 1.0f));
        if (far - h < BRIDGE_SHOULDER && fminf(far, mid) >= wl + DECOR_BRIDGE_CLEAR - 0.1f) return d;
    }
    return -1.0f;
}

// Lejos de los pueblos (y sus campos), las estructuras, los campamentos y las bocas de las cuevas.
static bool bridge_spot_free(const World *w, float x, float z) {
    for (int i = 0; i < w->settlement_count; i++) {
        float dx = x - w->settlements[i].x, dz = z - w->settlements[i].z;
        float r = (w->settlements[i].kind == SETTLE_CAPITAL ? CITY_FIELDS_R1 : VILLAGE_FIELDS_R1) + 8.0f;
        if (dx * dx + dz * dz < r * r) return false;
    }
    for (int i = 0; i < w->site_count; i++) {
        float dx = x - w->sites[i].x, dz = z - w->sites[i].z, r = w->sites[i].kind == SITE_ANCIENT_TOWN ? 40.0f : 34.0f;
        if (dx * dx + dz * dz < r * r) return false;
    }
    for (int i = 0; i < w->tribe_count; i++) {
        float dx = x - w->tribes[i].x, dz = z - w->tribes[i].z;
        if (dx * dx + dz * dz < 30.0f * 30.0f) return false;
    }
    for (int i = 0; i < w->cave_count; i++) {
        float dx = x - w->caves[i].x, dz = z - w->caves[i].z;
        if (dx * dx + dz * dz < 16.0f * 16.0f) return false;
    }
    return true;
}

bool decor_bridge(const World *w, float px, float pz, DecorBridge *out) {
    float best = 1e9f;
    for (int i = 0; i < w->river_count; i++) {
        const River *rv = &w->rivers[i];
        if (rv->kind == RIVER_BRAIDED || rv->width > 4.0f || rv->n < 3) continue;
        for (int k = 1; k + 1 < rv->n; k++) {
            float x = rv->x[k], z = rv->z[k], dx = x - px, dz = z - pz;
            if (dx * dx + dz * dz > 220.0f * 220.0f || !bridge_spot_free(w, x, z)) continue;
            WaterKind wk;
            float wl = world_water(w, x, z, 0.0f, &wk);
            if (wl < -1e8f || wk != WATER_RIVER) continue;
            float fx = rv->x[k + 1] - rv->x[k - 1], fz = rv->z[k + 1] - rv->z[k - 1], l = sqrtf(fx * fx + fz * fz) + 1e-6f;
            float ux = -fz / l, uz = fx / l; // de orilla a orilla
            float d1 = bank_top(w, x, z, ux, uz, wl), d2 = d1 < 0.0f ? -1.0f : bank_top(w, x, z, -ux, -uz, wl);
            if (d2 < 0.0f || d1 + d2 >= best) continue;
            float half = 0.5f * (d1 + d2) + 0.4f, sk = half / DECOR_BRIDGE_HALF; // las puntas, 40 cm adentro de cada orilla
            float sc = fminf(fmaxf(sk / 1.4f, 1.0f), 1.2f), st = sk / sc;
            if (st > DECOR_BRIDGE_STRETCH) continue;
            if (st < 0.9f) st = 0.9f, half = DECOR_BRIDGE_HALF * sc * st;
            float cx = x + ux * 0.5f * (d1 - d2), cz = z + uz * 0.5f * (d1 - d2); // entre las dos orillas
            float g1 = world_height(w, cx + ux * half, cz + uz * half), g2 = world_height(w, cx - ux * half, cz - uz * half);
            if (fabsf(g1 - g2) > 0.35f) continue;
            // Y se sale a una orilla pareja: 1,5 m mas alla de cada punta, el suelo no sube ni baja mas de 0,8 m.
            float a1 = world_height(w, cx + ux * (half + 1.5f), cz + uz * (half + 1.5f)), a2 = world_height(w, cx - ux * (half + 1.5f), cz - uz * (half + 1.5f));
            if (fabsf(a1 - g1) > 1.0f || fabsf(a2 - g2) > 1.0f) continue;
            float base = fminf(g1, g2) - BRIDGE_END * sc;
            bool clear = true; // y en el medio, nada del suelo toca el tablero
            for (float t = -half + 0.6f; t <= half - 0.6f && clear; t += 0.5f) {
                float u = t / half;
                clear = world_height(w, cx + ux * t, cz + uz * t) < base + sc * (BRIDGE_END + BRIDGE_RISE * (1.0f - u * u)) - 0.05f;
            }
            if (!clear) continue;
            best = d1 + d2;
            *out = (DecorBridge){ cx, cz, base, yaw_along(ux, uz), sc, st, wl };
        }
    }
    return best < 1e8f;
}
