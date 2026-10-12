#include "sim/forge.h"

#include <math.h>
#include <string.h>

#include "sim/rng.h"

#define BRONZE_ID "estructura.campamento.horno_bronce"
#define STEEL_ID "estructura.campamento.horno_acero"
#define VEINS_WANTED 12

static bool dry(const World *w, float x, float z) { return world_water(w, x, z, 0.0f, NULL) < world_height(w, x, z); }

static bool far_from(const IronSite *s, int n, float x, float z, float d) {
    for (int i = 0; i < n; i++)
        if ((s[i].x - x) * (s[i].x - x) + (s[i].z - z) * (s[i].z - z) < d * d) return false;
    return true;
}

int world_iron_sites(const World *w, IronSite *out, int max) {
    if (!w || !out || max <= 0) return 0;
    Rng rng;
    rng_seed(&rng, w->seed * 2654435761u ^ 0x1E0F0E5u); // su propia racha: la del mundo no se toca
    int n = 0, veins = 0;
    // Vetas a cielo abierto: en el altiplano, en seco, lejos del campamento inicial y entre si.
    for (int tries = 0; tries < 600 && n < max && veins < VEINS_WANTED; tries++) {
        float x, z, a = rng_float(&rng) * 6.2831853f, d = rng_float(&rng) * 800.0f;
        world_region_point(w, REGION_HIGHLAND, 900.0f + rng_float(&rng) * 3300.0f, &x, &z);
        x += cosf(a) * d, z += sinf(a) * d;
        if (world_region(w, x, z) != REGION_HIGHLAND || world_edge_distance(w, x, z) < 120.0f || !dry(w, x, z) ||
            x * x + z * z < 700.0f * 700.0f || !far_from(out, n, x, z, 350.0f))
            continue;
        out[n++] = (IronSite){ x, z, IRON_VEIN };
        veins++;
    }
    // Hierro de pantano: en la orilla de los lagos del bosque y de los fiordos, uno por lago.
    for (int i = 0; i < w->lake_count && n < max; i++) {
        const Lake *lk = &w->lakes[i];
        Region rg = world_region(w, lk->x, lk->z);
        float a = rng_float(&rng) * 6.2831853f;
        if (rg != REGION_FOREST && rg != REGION_FJORD) continue;
        for (int k = 0; k < 8; k++, a += 0.785398f) { // la primera orilla seca alrededor del lago
            float x = lk->x + cosf(a) * (lk->r + 6.0f), z = lk->z + sinf(a) * (lk->r + 6.0f);
            if (!dry(w, x, z) || world_edge_distance(w, x, z) < 80.0f || !far_from(out, n, x, z, 200.0f)) continue;
            out[n++] = (IronSite){ x, z, IRON_BOG };
            break;
        }
    }
    return n;
}

int iron_site_near(const IronSite *s, int n, float x, float z) {
    int best = -1;
    float bd = IRON_NEAR * IRON_NEAR;
    for (int i = 0; i < n; i++) {
        float d = (s[i].x - x) * (s[i].x - x) + (s[i].z - z) * (s[i].z - z);
        if (d <= bd) bd = d, best = i;
    }
    return best;
}

int iron_ore_yield(IronKind k) { return k == IRON_VEIN ? 2 : 1; }

bool forge_station_ok(const char *building, bool mobile, bool bronze, bool steel, bool workshop, bool repair) {
    if (!building || !building[0]) return true;
    if (!strcmp(building, FORGE_MOBILE_ID)) return mobile || bronze || steel || workshop;
    if (!strcmp(building, WORKSHOP_ID)) return workshop;
    if (!strcmp(building, BRONZE_ID)) return bronze || (repair && (mobile || workshop));
    if (!strcmp(building, STEEL_ID)) return steel || (repair && (mobile || workshop));
    return false; // otra obra (el horno de cocina...): la ve quien llama
}

float temper_chance(int smiths, float skill) {
    if (smiths < 0) smiths = 0;
    if (smiths > 2) smiths = 2;
    float p = 0.5f + 0.15f * (float)smiths + 0.25f * fminf(fmaxf(skill, 0.0f), 1.0f);
    return fminf(0.95f, fmaxf(0.3f, p));
}
