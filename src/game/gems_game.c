#include "gems_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/inventory_game.h"
#include "raymath.h"
#include "sim/jewelry.h"
#include "sim/lang.h"

#define CORAL_ID "mapa.agua.coral"
#define ROCK_SMALL "mapa.roca.pequena"
#define ROCK_MEDIUM "mapa.roca.mediana"

void gems_scatter(GameActions *ga, Props *props, const Terrain *t) {
    // Rocas sueltas por la estepa.
    for (int i = 0; i < 14; i++) {
        float a = rng_float(&ga->rng) * 6.2831853f, r = 25.0f + rng_float(&ga->rng) * 90.0f;
        float x = cosf(a) * r, z = sinf(a) * r, h = terrain_height(t, x, z);
        if (terrain_water(t, x, z) > h - 0.2f) continue;
        props_add(props, i % 3 ? ROCK_SMALL : ROCK_MEDIUM, (Vector3){ x, h, z }, rng_float(&ga->rng) * 6.28f);
    }
    // Arrecifes de coral en el agua poco honda de los lagos cercanos.
    int placed = 0;
    for (int tries = 0; tries < 2500 && placed < 10; tries++) {
        float x = (rng_float(&ga->rng) - 0.5f) * 500.0f, z = (rng_float(&ga->rng) - 0.5f) * 500.0f;
        float h = terrain_height(t, x, z), depth = terrain_water(t, x, z) - h;
        if (depth < 0.5f || depth > 2.5f) continue;
        props_add(props, CORAL_ID, (Vector3){ x, h, z }, rng_float(&ga->rng) * 6.28f);
        placed++;
    }
}

static const char *item_name(const GameActions *ga, const char *id) {
    const InvItem *it = inventory_find(ga->inv, id);
    return it ? T(it->name) : id;
}

// Guarda lo encontrado y lo cuenta en el registro.
static void found(GameActions *ga, const Props *props, const Player *p, const char *what, const char *id, int n, char *log, size_t len) {
    int kept = id ? ig_store(ga, props, p, id, n, 1.0f) : 0;
    if (!id) snprintf(log, len, "%s", what);
    else if (kept) snprintf(log, len, T("%s: %d x %s."), what, kept, item_name(ga, id));
    else snprintf(log, len, T("%s: %s, pero no te cabe."), what, item_name(ga, id));
}

bool gems_coral(GameActions *ga, Props *props, const Player *p, char *log, size_t len) {
    int best = -1;
    float bd = 2.5f;
    for (int i = 0; i < props->count; i++) {
        const Prop *pr = &props->items[i];
        if (strcmp(pr->item->id, CORAL_ID) != 0) continue;
        float d = Vector2Distance((Vector2){ pr->pos.x, pr->pos.z }, (Vector2){ p->pos.x, p->pos.z });
        if (d < bd) bd = d, best = i;
    }
    if (best < 0) return false;
    Prop *pr = &props->items[best];
    Gem g = gem_roll(GEMSRC_CORAL, &ga->rng);
    if (g == GEM_NONE) found(ga, props, p, T("Solo coral quebradizo, sin valor"), NULL, 0, log, len);
    else found(ga, props, p, T("Del arrecife sacas"), gem_id(g), 1, log, len);
    pr->condition -= 0.34f; // tres cosechas y el arrecife se agota
    if (pr->condition <= 0.0f) props_remove(props, best);
    return true;
}

bool gems_hit_rock(GameActions *ga, Props *props, const Player *p, float dmg, float reach, char *log, size_t len) {
    Vector3 f = { sinf(p->yaw), 0, cosf(p->yaw) };
    int best = -1;
    float bd = reach + 1.0f;
    for (int i = 0; i < props->count; i++) {
        const Prop *pr = &props->items[i];
        bool small = !strcmp(pr->item->id, ROCK_SMALL);
        if (!small && strcmp(pr->item->id, ROCK_MEDIUM) != 0) continue;
        Vector3 to = { pr->pos.x - p->pos.x, 0, pr->pos.z - p->pos.z };
        float d = Vector3Length(to);
        if (d < bd && (d < 1.0f || Vector3DotProduct(Vector3Normalize(to), f) > 0.4f)) bd = d, best = i;
    }
    if (best < 0) return false;
    Prop *pr = &props->items[best];
    bool small = !strcmp(pr->item->id, ROCK_SMALL);
    pr->condition -= dmg / (small ? 60.0f : 160.0f);
    if (pr->condition > 0.0f) {
        snprintf(log, len, T("La roca se agrieta (%d %%)."), (int)(pr->condition * 100.0f));
        return true;
    }
    // Rota: piedras, a veces mineral y a veces una gema.
    Vector3 at = pr->pos;
    props_remove(props, best);
    ig_store(ga, props, p, "utileria.material.piedra", small ? 2 : 4, 1.0f);
    float r = rng_float(&ga->rng);
    if (r < 0.25f) ig_store(ga, props, p, r < 0.15f ? "utileria.material.cobre" : "utileria.material.estano", 1, 1.0f);
    Gem g = gem_roll(GEMSRC_ROCK, &ga->rng);
    if (g != GEM_NONE) found(ga, props, p, T("¡La roca se parte y brilla algo!"), gem_id(g), 1, log, len);
    else snprintf(log, len, "%s", T("La roca se parte: solo piedras."));
    (void)at;
    return true;
}

void gems_dig(GameActions *ga, const Props *props, const Player *p, char *log, size_t len) {
    float r = rng_float(&ga->rng);
    Gem g = gem_roll(GEMSRC_DIG, &ga->rng);
    if (g != GEM_NONE) found(ga, props, p, T("Al cavar desentierras"), gem_id(g), 1, log, len);
    else if (r < 0.06f) found(ga, props, p, T("Al cavar desentierras"), "utileria.material.oro", 1, log, len);
    else if (r < 0.16f) found(ga, props, p, T("Al cavar desentierras"), "utileria.material.plata", 1, log, len);
}

bool gems_water_ahead(const Terrain *t, const Player *p) {
    for (float d = 0.0f; d <= 2.5f; d += 0.5f) {
        float x = p->pos.x + sinf(p->yaw) * d, z = p->pos.z + cosf(p->yaw) * d;
        if (terrain_water(t, x, z) > terrain_height(t, x, z) + 0.1f) return true;
    }
    return false;
}

void gems_pan(GameActions *ga, const Props *props, const Player *p, char *log, size_t len) {
    float r = rng_float(&ga->rng);
    Gem g = gem_roll(GEMSRC_RIVER, &ga->rng);
    if (g != GEM_NONE) found(ga, props, p, T("En la criba queda"), gem_id(g), 1, log, len);
    else if (r < 0.12f) found(ga, props, p, T("En la criba queda"), "utileria.material.oro", 1, log, len);
    else if (r < 0.2f) found(ga, props, p, T("En la criba queda"), "utileria.material.plata", 1, log, len);
    else snprintf(log, len, "%s", T("Solo arena y guijarros. Prueba otra vez."));
}
