#include "game/death_game.h"

#include <math.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"
#include "sim/lang.h"

#define REST_NEAR_CAMP 10.0f // m del fuego o del centro de un campamento
#define REST_NEAR_HOME 4.0f  // m de una tienda, yurta o casa de la tribu
#define TAIL_MAGIC 0x31544744u // "DGT1"

typedef struct {
    float x, y, z, yaw;
} Remains;

static struct {
    bool has_rest;     // si no, el campamento de la tribu
    float rest_x, rest_z;
    char rest_name[48];
    int deaths;
    int count, next;   // restos en el suelo (anillo: los mas viejos se pierden)
    Remains remains[DG_REMAINS_MAX];
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
        g.has_rest = true;
        g.rest_x = x, g.rest_z = z;
        snprintf(g.rest_name, sizeof(g.rest_name), "%s", g.cand_name);
    }
}

Vector3 dg_rest_point(const Terrain *t, Vector3 camp_fire) {
    float x = g.has_rest ? g.rest_x : camp_fire.x, z = g.has_rest ? g.rest_z : camp_fire.z;
    // Junto al lugar (no encima del fuego ni de la tienda), y en seco.
    for (int k = 0; k < 16; k++) {
        float a = (float)k * PI / 8.0f, r = 2.5f + 0.25f * (float)k;
        float px = x - cosf(a) * r, pz = z + sinf(a) * r;
        if (terrain_water(t, px, pz) < terrain_height(t, px, pz) - 0.2f) return (Vector3){ px, terrain_height(t, px, pz), pz };
    }
    return (Vector3){ x - 2.5f, terrain_height(t, x - 2.5f, z), z };
}

const char *dg_rest_name(void) { return g.has_rest && g.rest_name[0] ? g.rest_name : T("el campamento"); }

void dg_leave_remains(Vector3 pos, float yaw) {
    g.remains[g.next] = (Remains){ pos.x, pos.y, pos.z, yaw };
    g.next = (g.next + 1) % DG_REMAINS_MAX;
    if (g.count < DG_REMAINS_MAX) g.count++;
    g.deaths++;
}

int dg_remains_count(void) { return g.count; }
int dg_deaths(void) { return g.deaths; }

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
    for (int i = 0; i < g.count; i++) {
        const Remains *r = &g.remains[i];
        draw_remains(r, terrain_height(t, r->x, r->z));
    }
}

// Bloque guardado: marca, lugar de descanso, muertes y restos.
typedef struct {
    unsigned magic;
    int has_rest;
    float rest_x, rest_z;
    char rest_name[48];
    int deaths, count, next;
    Remains remains[DG_REMAINS_MAX];
} Tail;

bool dg_write(FILE *f) {
    Tail tl;
    memset(&tl, 0, sizeof(tl));
    tl.magic = TAIL_MAGIC, tl.has_rest = g.has_rest, tl.rest_x = g.rest_x, tl.rest_z = g.rest_z;
    tl.deaths = g.deaths, tl.count = g.count, tl.next = g.next;
    memcpy(tl.rest_name, g.rest_name, sizeof(tl.rest_name));
    memcpy(tl.remains, g.remains, sizeof(tl.remains));
    return fwrite(&tl, sizeof(tl), 1, f) == 1;
}

void dg_read(FILE *f) {
    dg_reset();
    Tail tl;
    if (fread(&tl, sizeof(tl), 1, f) != 1 || tl.magic != TAIL_MAGIC) return;
    if (tl.count < 0 || tl.count > DG_REMAINS_MAX || tl.next < 0 || tl.next >= DG_REMAINS_MAX) return;
    g.has_rest = tl.has_rest != 0;
    g.rest_x = tl.rest_x, g.rest_z = tl.rest_z;
    memcpy(g.rest_name, tl.rest_name, sizeof(g.rest_name));
    g.rest_name[sizeof(g.rest_name) - 1] = '\0';
    g.deaths = tl.deaths, g.count = tl.count, g.next = tl.next;
    memcpy(g.remains, tl.remains, sizeof(g.remains));
}
