#include "game/death_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"
#include "sim/lang.h"

#define REST_NEAR_CAMP 10.0f // m del fuego o del centro de un campamento
#define REST_NEAR_HOME 4.0f  // m de una tienda, yurta o casa de la tribu

static struct {
    DeathSave s;
    // Sin guardar: el candidato a lugar de descanso y cuanto lleva el jugador alli.
    float cand_x, cand_z, cand_t;
    char cand_name[48];
} g;

void dg_reset(void) { memset(&g, 0, sizeof(g)); }

static bool is_home(const InvItem *it) { return it && !strncmp(it->id, "estructura.vivienda.", 20); }

void dg_track_rest(const GameActions *ga, const Props *props, Vector3 pos, Vector3 camp_fire, float dt) {
    // Donde esta ahora: el campamento de la tribu, otro campamento suyo o una vivienda.
    float x = 0.0f, z = 0.0f;
    const char *name = NULL;
    if (Vector2Distance((Vector2){ pos.x, pos.z }, (Vector2){ camp_fire.x, camp_fire.z }) < REST_NEAR_CAMP)
        x = camp_fire.x, z = camp_fire.z, name = T("el campamento");
    for (int k = 0; k < CAMPS_MAX && !name; k++) {
        const CampSite *c = &ga->camps[k];
        if (c->used && Vector2Distance((Vector2){ pos.x, pos.z }, (Vector2){ c->x, c->z }) < REST_NEAR_CAMP) x = c->x, z = c->z, name = c->name;
    }
    for (int i = 0; i < props->count && !name; i++) {
        const Prop *pr = &props->items[i];
        if (is_home(pr->item) && Vector2Distance((Vector2){ pos.x, pos.z }, (Vector2){ pr->pos.x, pr->pos.z }) < REST_NEAR_HOME)
            x = pr->pos.x, z = pr->pos.z, name = T(pr->item->name);
    }
    if (!name) {
        g.cand_t = 0.0f;
        return;
    }
    if (fabsf(x - g.cand_x) > 0.5f || fabsf(z - g.cand_z) > 0.5f) g.cand_t = 0.0f; // otro sitio: empieza la cuenta
    g.cand_x = x, g.cand_z = z;
    snprintf(g.cand_name, sizeof(g.cand_name), "%s", name);
    g.cand_t += dt;
    if (g.cand_t >= DG_REST_SECONDS) {
        g.s.has_rest = true;
        g.s.rest_x = x, g.s.rest_z = z;
        snprintf(g.s.rest_name, sizeof(g.s.rest_name), "%s", g.cand_name);
    }
}

Vector3 dg_rest_point(const Terrain *t, Vector3 camp_fire) {
    float x = g.s.has_rest ? g.s.rest_x : camp_fire.x, z = g.s.has_rest ? g.s.rest_z : camp_fire.z;
    // Junto al lugar (no encima del fuego ni de la tienda), y en seco.
    for (int k = 0; k < 16; k++) {
        float a = (float)k * PI / 8.0f, r = 2.5f + 0.25f * (float)k;
        float px = x - cosf(a) * r, pz = z + sinf(a) * r;
        if (terrain_water(t, px, pz) < terrain_height(t, px, pz) - 0.2f) return (Vector3){ px, terrain_height(t, px, pz), pz };
    }
    return (Vector3){ x - 2.5f, terrain_height(t, x - 2.5f, z), z };
}

const char *dg_rest_name(void) { return g.s.has_rest && g.s.rest_name[0] ? g.s.rest_name : T("el campamento"); }

void dg_leave_remains(Vector3 pos, float yaw) {
    g.s.remains[g.s.next] = (Remains){ pos.x, pos.y, pos.z, yaw };
    g.s.next = (g.s.next + 1) % DG_REMAINS_MAX;
    if (g.s.count < DG_REMAINS_MAX) g.s.count++;
    g.s.deaths++;
}

int dg_remains_count(void) { return g.s.count; }
int dg_deaths(void) { return g.s.deaths; }
const DeathSave *dg_state(void) { return &g.s; }

// Una calavera (cráneo, cuencas y mandibula) y dos huesos largos cruzados.
static void draw_remains(const Remains *r, float ground) {
    const Color bone = { 222, 214, 190, 255 }, shade = { 176, 166, 140, 255 }, hole = { 34, 28, 24, 255 };
    rlPushMatrix();
    rlTranslatef(r->x, ground, r->z);
    rlRotatef(r->yaw * RAD2DEG, 0, 1, 0);
    rlScalef(1.5f, 1.5f, 1.5f); // algo mayores que en la vida: se ven a 640x360
    // Huesos: dos fémures en aspa, con sus cabezas.
    for (int k = 0; k < 2; k++) {
        float a = (k ? -0.6f : 0.6f) + 0.3f;
        Vector3 d = { sinf(a) * 0.24f, 0.0f, cosf(a) * 0.24f };
        Vector3 p0 = { -d.x + 0.12f, 0.035f + 0.02f * k, -d.z }, p1 = { d.x + 0.12f, 0.035f + 0.02f * k, d.z };
        DrawCylinderEx(p0, p1, 0.022f, 0.022f, 5, bone);
        DrawSphereEx(p0, 0.04f, 3, 5, shade);
        DrawSphereEx(p1, 0.04f, 3, 5, shade);
    }
    // La calavera: un poco ladeada, mirando hacia +Z.
    Vector3 c = { -0.12f, 0.1f, 0.02f };
    DrawSphereEx(c, 0.1f, 4, 7, bone);
    DrawCube((Vector3){ c.x, c.y - 0.06f, c.z + 0.05f }, 0.1f, 0.05f, 0.08f, bone); // mandibula
    DrawCube((Vector3){ c.x - 0.035f, c.y, c.z + 0.085f }, 0.035f, 0.03f, 0.03f, hole);
    DrawCube((Vector3){ c.x + 0.035f, c.y, c.z + 0.085f }, 0.035f, 0.03f, 0.03f, hole);
    DrawCube((Vector3){ c.x, c.y - 0.035f, c.z + 0.09f }, 0.015f, 0.02f, 0.02f, hole); // la nariz
    rlPopMatrix();
}

void dg_draw_world(const Terrain *t) {
    for (int i = 0; i < g.s.count; i++) {
        const Remains *r = &g.s.remains[i];
        draw_remains(r, terrain_height(t, r->x, r->z));
    }
}

// Una partida cargada (src/game/save_game.c): lo guardado, si tiene sentido; si no, de cero.
void dg_load(const DeathSave *s) {
    dg_reset();
    if (!s || s->count < 0 || s->count > DG_REMAINS_MAX || s->next < 0 || s->next >= DG_REMAINS_MAX) return;
    g.s = *s;
    g.s.has_rest = s->has_rest != 0;
    g.s.rest_name[sizeof(g.s.rest_name) - 1] = '\0';
}
