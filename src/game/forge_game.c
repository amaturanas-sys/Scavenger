#include "game/forge_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/inventory_game.h"
#include "game/talents_game.h"
#include "sim/lang.h"
#include "ui/theme.h"

#define BRONZE_ID "estructura.campamento.horno_bronce"
#define STEEL_ID "estructura.campamento.horno_acero"
#define SEEN_RANGE 22.0f  // m: al pasar tan cerca, la veta queda marcada en el mapa
#define DRAW_RANGE 160.0f // m: hasta donde se dibujan

static IronSite g_sites[IRON_SITES_MAX];
static int g_count;
static const World *g_world; // de que mundo son (la semilla cambia en cada partida)
static uint32_t g_seed;

const IronSite *forge_sites(const Terrain *t, int *n) {
    const World *w = t ? t->world : NULL;
    if (!w) {
        if (n) *n = 0;
        return g_sites;
    }
    if (w != g_world || w->seed != g_seed) g_count = world_iron_sites(w, g_sites, IRON_SITES_MAX), g_world = w, g_seed = w->seed;
    if (n) *n = g_count;
    return g_sites;
}

const IronSite *forge_site_near(const Terrain *t, float x, float z) {
    int n;
    const IronSite *s = forge_sites(t, &n);
    int i = iron_site_near(s, n, x, z);
    return i >= 0 ? &s[i] : NULL;
}

void forge_mine(GameActions *ga, const Props *props, const Player *p, char *log, size_t len) {
    const IronSite *s = forge_site_near(ga->terrain, p->pos.x, p->pos.z);
    if (!s) return;
    int n = iron_ore_yield(s->kind), kept = ig_store(ga, props, p, IRON_ORE_ID, n, 1.0f);
    if (kept < n) stock_add(ga_stock(ga), IRON_ORE_ID, n - kept);
    snprintf(log, len, s->kind == IRON_VEIN ? T("Picas la veta: %d de mineral de hierro%s. Se funde en el horno de acero.")
                                            : T("Sacas hierro de pantano: %d de mineral%s. Se funde en el horno de acero."),
             n, kept < n ? T(" (lo que no cabe, al acopio)") : "");
    tg_xp(ga, XP_CRAFT, log, len);
}

void forge_update(const Terrain *t, const Player *p, MemoryMap *mem, float now, char *log, size_t len) {
    int n;
    const IronSite *s = forge_sites(t, &n);
    for (int i = 0; i < n && mem; i++) {
        float dx = s[i].x - p->pos.x, dz = s[i].z - p->pos.z;
        if (dx * dx + dz * dz > SEEN_RANGE * SEEN_RANGE) continue;
        bool marked = false;
        for (int m = 0; m < mem->marker_count && !marked; m++)
            marked = (mem->markers[m].x - s[i].x) * (mem->markers[m].x - s[i].x) + (mem->markers[m].z - s[i].z) * (mem->markers[m].z - s[i].z) <
                     60.0f * 60.0f;
        if (marked || memmap_add_marker(mem, s[i].x, s[i].z, MARKER_INTEREST, now) < 0) continue;
        snprintf(log, len, "%s", s[i].kind == IRON_VEIN ? T("Encuentras una veta de hierro a cielo abierto: la marcas en tu mapa (M). F para picarla.")
                                                       : T("Encuentras hierro de pantano en la orilla: lo marcas en tu mapa (M). F para sacarlo."));
    }
}

void forge_draw_world(const Terrain *t, Vector3 eye) {
    int n;
    const IronSite *s = forge_sites(t, &n);
    for (int i = 0; i < n; i++) {
        float dx = s[i].x - eye.x, dz = s[i].z - eye.z;
        if (dx * dx + dz * dz > DRAW_RANGE * DRAW_RANGE) continue;
        float y = terrain_height(t, s[i].x, s[i].z);
        if (s[i].kind == IRON_VEIN) { // rocas con la herrumbre a la vista
            static const float R[5][4] = { { 0.0f, 0.0f, 1.6f, 1.1f }, { 1.4f, 0.6f, 1.0f, 0.8f }, { -1.2f, 0.9f, 1.1f, 0.7f },
                                           { 0.5f, -1.4f, 0.9f, 0.6f }, { -0.6f, -0.9f, 0.7f, 0.5f } };
            for (int k = 0; k < 5; k++) {
                Vector3 c = { s[i].x + R[k][0], y + R[k][3] * 0.5f, s[i].z + R[k][1] };
                DrawCube(c, R[k][2], R[k][3], R[k][2] * 0.9f, k % 2 ? (Color){ 128, 64, 38, 255 } : (Color){ 92, 84, 78, 255 });
                DrawCube((Vector3){ c.x, c.y + R[k][3] * 0.5f + 0.02f, c.z }, R[k][2] * 0.6f, 0.04f, R[k][2] * 0.5f, (Color){ 168, 82, 40, 255 });
            }
        } else { // barro rojizo con charcos negros
            DrawCube((Vector3){ s[i].x, y + 0.03f, s[i].z }, 4.2f, 0.06f, 3.4f, (Color){ 120, 56, 30, 255 });
            DrawCube((Vector3){ s[i].x + 0.8f, y + 0.07f, s[i].z - 0.5f }, 1.2f, 0.04f, 0.9f, (Color){ 34, 30, 26, 255 });
            DrawCube((Vector3){ s[i].x - 1.1f, y + 0.07f, s[i].z + 0.7f }, 0.8f, 0.04f, 0.7f, (Color){ 150, 76, 36, 255 });
        }
    }
}

void forge_draw_hud(const Terrain *t, const Player *p, int w, int h) {
    const IronSite *s = forge_site_near(t, p->pos.x, p->pos.z);
    if (!s) return;
    ui_text_centered(s->kind == IRON_VEIN ? T("F: picar la veta de hierro") : T("F: sacar el hierro de pantano"), w / 2, h - 96, 10, UI_GOLD);
}

static bool has_building(const Props *props, const char *id, const Player *p, bool near) {
    for (int i = 0; props && i < props->count; i++) {
        if (strcmp(props->items[i].item->id, id) != 0) continue;
        float dx = props->items[i].pos.x - p->pos.x, dz = props->items[i].pos.z - p->pos.z;
        if (!near || dx * dx + dz * dz < CAMP_STORE_RADIUS * CAMP_STORE_RADIUS) return true;
    }
    return false;
}

bool forge_station(const GameActions *ga, const Props *props, const Player *p, const char *building, bool repair, bool near) {
    if (!building || !building[0]) return true;
    bool mobile = ig_count(ga, props, p, FORGE_MOBILE_ID) > 0;
    if (forge_station_ok(building, mobile, has_building(props, BRONZE_ID, p, near), has_building(props, STEEL_ID, p, near),
                         has_building(props, WORKSHOP_ID, p, near), repair))
        return true;
    return has_building(props, building, p, near); // otra obra (el horno de cocina...)
}

bool forge_temper_roll(GameActions *ga, const Troop *troop) {
    int smiths = 0;
    for (int i = 0; troop && i < troop->count; i++) smiths += troop->members[i].status == STATUS_ACTIVE && troop->members[i].role == ROLE_SMITH;
    return rng_float(&ga->rng) < temper_chance(smiths, fminf(1.0f, ig_stat(ga, STAT_CRAFT)));
}
