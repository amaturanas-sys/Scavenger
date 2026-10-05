#include "game/world_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"
#include "sim/lang.h"
#include "world/body_draw.h"

#define SETTLE_DRAW 280.0f // m: se dibujan los asentamientos a esta distancia
#define SETTLE_ARRIVE 90.0f
#define DEN_DRAW 180.0f

static float dist_xz(float ax, float az, float bx, float bz) { return sqrtf((ax - bx) * (ax - bx) + (az - bz) * (az - bz)); }

void wd_update(GameActions *ga, const Terrain *t, const Player *p, MemoryMap *mem, char *log, size_t len) {
    const World *w = t->world;
    // Otra region: su nombre y quien la gobierna.
    Region rg = world_region(w, p->pos.x, p->pos.z);
    if (ga->last_region != (int)rg + 1) {
        bool first = ga->last_region == 0;
        ga->last_region = (int)rg + 1;
        if (!first) {
            char name[48];
            snprintf(name, sizeof(name), "%s", region_name(rg));
            if (name[0] >= 'a' && name[0] <= 'z') name[0] = (char)(name[0] - 'a' + 'A');
            snprintf(log, len, T("Entras en: %s (tierras del %s)."), name, world_kingdom_name(rg));
        }
    }
    // Un asentamiento nuevo: se anota en el mapa.
    for (int i = 0; i < w->settlement_count; i++) {
        const Settlement *s = &w->settlements[i];
        if ((ga->seen_settle >> i) & 1u || dist_xz(s->x, s->z, p->pos.x, p->pos.z) > SETTLE_ARRIVE) continue;
        ga->seen_settle |= 1u << i;
        memmap_toggle_marker(mem, s->x, s->z, MARKER_INTEREST, 4.0f);
        snprintf(log, len, s->kind == SETTLE_CAPITAL ? T("Llegas a %s, capital del %s.") : T("Llegas a %s, aldea del %s."), s->name,
                 world_kingdom_name(s->region));
    }
    // Una guarida: peligro, al mapa.
    for (int i = 0; i < w->den_count && i < 64; i++) {
        const Den *d = &w->dens[i];
        if ((ga->seen_dens >> i) & 1u || dist_xz(d->x, d->z, p->pos.x, p->pos.z) > 45.0f) continue;
        ga->seen_dens |= 1ull << i;
        memmap_toggle_marker(mem, d->x, d->z, MARKER_DANGER, 4.0f);
        char who[48];
        snprintf(who, sizeof(who), "%s", T(species_def((Species)d->species)->name));
        if (who[0] >= 'A' && who[0] <= 'Z') who[0] = (char)(who[0] - 'A' + 'a');
        snprintf(log, len, T("Una guarida (%s): queda marcada como peligro en el mapa."), who);
    }
}

int wd_den_near(const Terrain *t, float x, float z, float radius) {
    float d;
    int i = world_nearest_den(t->world, x, z, &d);
    return d < radius ? i : -1;
}

// ------------------------------------------------------------------ dibujo
static void gable(Vector3 c, float w, float l, float h, float roof, float yaw, Color wall, Color top) {
    // Caja con techo a dos aguas (cabaña, casa larga), girada yaw.
    rlPushMatrix();
    rlTranslatef(c.x, c.y, c.z);
    rlRotatef(yaw * RAD2DEG, 0, 1, 0);
    DrawCube((Vector3){ 0, h * 0.5f, 0 }, w, h, l, wall);
    DrawCubeWires((Vector3){ 0, h * 0.5f, 0 }, w, h, l, Fade(BLACK, 0.25f));
    for (int s = -1; s <= 1; s += 2) { // los dos faldones
        rlPushMatrix();
        rlTranslatef(s * w * 0.25f, h + roof * 0.5f, 0);
        rlRotatef(s * -atan2f(roof, w * 0.5f) * RAD2DEG, 0, 0, 1);
        DrawCube((Vector3){ 0, 0, 0 }, sqrtf(w * w * 0.25f + roof * roof) + 0.2f, 0.25f, l + 0.4f, top);
        rlPopMatrix();
    }
    rlPopMatrix();
}

static void building(Region rg, Vector3 at, float yaw, float scale) {
    switch (rg) {
    case REGION_FOREST: gable(at, 4.0f * scale, 5.0f * scale, 2.4f, 1.6f, yaw, (Color){ 118, 82, 52, 255 }, (Color){ 72, 58, 44, 255 }); break;
    case REGION_FJORD: gable(at, 5.0f * scale, 12.0f * scale, 2.2f, 2.6f, yaw, (Color){ 84, 64, 48, 255 }, (Color){ 96, 110, 82, 255 }); break;
    case REGION_HIGHLAND:
        DrawCube((Vector3){ at.x, at.y + 1.1f, at.z }, 4.0f * scale, 2.2f, 4.0f * scale, (Color){ 128, 126, 120, 255 });
        DrawCube((Vector3){ at.x, at.y + 2.35f, at.z }, 4.4f * scale, 0.3f, 4.4f * scale, (Color){ 96, 80, 64, 255 });
        break;
    case REGION_DESERT:
        DrawCube((Vector3){ at.x, at.y + 1.3f, at.z }, 4.5f * scale, 2.6f, 4.5f * scale, (Color){ 196, 160, 112, 255 });
        DrawSphere((Vector3){ at.x, at.y + 2.6f, at.z }, 1.4f * scale, (Color){ 210, 180, 132, 255 });
        break;
    default: // yurta
        DrawCylinder(at, 2.2f * scale, 2.2f * scale, 1.7f, 10, (Color){ 233, 228, 214, 255 });
        DrawCylinder((Vector3){ at.x, at.y + 1.7f, at.z }, 0.45f * scale, 2.35f * scale, 0.9f, 10, (Color){ 216, 208, 189, 255 });
        break;
    }
}

// Ropa de la gente de cada region.
static Color folk_color(Region rg, int k) {
    static const Color C[REGION_COUNT][2] = {
        { { 60, 90, 140, 255 }, { 150, 60, 50, 255 } },  { { 70, 96, 58, 255 }, { 120, 90, 60, 255 } },
        { { 130, 110, 80, 255 }, { 90, 80, 110, 255 } }, { { 90, 110, 130, 255 }, { 140, 130, 110, 255 } },
        { { 230, 226, 210, 255 }, { 180, 120, 70, 255 } },
    };
    return C[rg][k & 1];
}

static void draw_settlement(const Terrain *t, const Settlement *s, int idx, float time) {
    float ground = terrain_height(t, s->x, s->z);
    int n = s->kind == SETTLE_CAPITAL ? 9 : 5;
    float ring = s->kind == SETTLE_CAPITAL ? 26.0f : 15.0f;
    for (int i = 0; i < n; i++) { // casas en anillo, mirando al centro
        float a = (float)i / n * 2.0f * PI + idx * 0.7f;
        float x = s->x + cosf(a) * ring, z = s->z + sinf(a) * ring;
        building(s->region, (Vector3){ x, terrain_height(t, x, z), z }, -a, s->kind == SETTLE_CAPITAL && i == 0 ? 1.6f : 1.0f);
    }
    if (s->kind == SETTLE_CAPITAL) { // muralla de torres y estandarte del reino
        Color stone = s->region == REGION_STEPPE || s->region == REGION_FOREST ? (Color){ 120, 92, 60, 255 } : (Color){ 140, 134, 124, 255 };
        for (int i = 0; i < 12; i++) {
            float a = (float)i / 12.0f * 2.0f * PI, x = s->x + cosf(a) * 42.0f, z = s->z + sinf(a) * 42.0f;
            float y = terrain_height(t, x, z);
            DrawCylinder((Vector3){ x, y, z }, 1.6f, 1.8f, 7.0f, 6, stone);
            float a2 = (float)(i + 1) / 12.0f * 2.0f * PI, x2 = s->x + cosf(a2) * 42.0f, z2 = s->z + sinf(a2) * 42.0f;
            Vector3 m = { (x + x2) * 0.5f, (y + terrain_height(t, x2, z2)) * 0.5f + 2.2f, (z + z2) * 0.5f };
            rlPushMatrix();
            rlTranslatef(m.x, m.y, m.z);
            rlRotatef(-atan2f(z2 - z, x2 - x) * RAD2DEG, 0, 1, 0);
            if (i % 3) DrawCube((Vector3){ 0 }, dist_xz(x, z, x2, z2), 4.4f, 1.0f, stone); // deja puertas
            rlPopMatrix();
        }
        Vector3 pole = { s->x, ground, s->z };
        DrawCylinder(pole, 0.12f, 0.12f, 9.0f, 5, (Color){ 90, 70, 50, 255 });
        float wave = sinf(time * 2.0f) * 0.3f;
        DrawCube((Vector3){ pole.x + 1.0f, ground + 8.0f + wave * 0.2f, pole.z }, 2.0f, 1.2f, 0.08f, folk_color(s->region, idx + 1));
    }
    // Fogata y gente.
    DrawCylinder((Vector3){ s->x, ground + 0.1f, s->z }, 0.0f, 0.4f, 0.8f, 5, (Color){ 240, 140, 40, 255 });
    for (int k = 0; k < 4; k++) {
        float a = time * 0.05f + (float)k * 1.6f + idx, r = 5.0f + 1.5f * (float)(k % 2);
        Vector3 pos = { s->x + cosf(a) * r, 0, s->z + sinf(a) * r };
        pos.y = terrain_height(t, pos.x, pos.z);
        BodyPose pose;
        BodyPoseParams bp = { .walk_phase = time * 6.0f + k, .walk = 0.0f, .scale = 1.0f };
        body_pose(&pose, &bp);
        BodyColors bc = { { 200, 160, 120, 255 }, folk_color(s->region, k), { 84, 64, 46, 255 } };
        body_draw(&pose, pos, a + PI, bc, NULL);
    }
}

static void draw_den(const Terrain *t, const Den *d) {
    float y = terrain_height(t, d->x, d->z);
    Color rock = d->region == REGION_DESERT ? (Color){ 170, 120, 84, 255 } : (Color){ 112, 108, 100, 255 };
    for (int i = 0; i < 6; i++) { // un monticulo de peñas
        float a = (float)i * 1.05f, r = 2.4f + (float)(i % 2);
        DrawSphere((Vector3){ d->x + cosf(a) * r, y + 0.6f, d->z + sinf(a) * r }, 1.6f + 0.4f * (float)(i % 3), rock);
    }
    DrawSphere((Vector3){ d->x, y + 1.6f, d->z }, 2.6f, rock);
    DrawCylinder((Vector3){ d->x + 2.4f, y, d->z }, 1.0f, 0.9f, 1.4f, 8, (Color){ 20, 16, 14, 255 }); // la boca
    for (int i = 0; i < 4; i++) // huesos
        DrawCube((Vector3){ d->x + 4.0f + (float)i * 0.5f, y + 0.05f, d->z - 1.0f + (float)i * 0.7f }, 0.6f, 0.08f, 0.12f, (Color){ 226, 220, 200, 255 });
}

void wd_draw_world(const GameActions *ga, const Terrain *t, const Player *p, float time) {
    (void)ga;
    const World *w = t->world;
    for (int i = 0; i < w->settlement_count; i++)
        if (dist_xz(w->settlements[i].x, w->settlements[i].z, p->pos.x, p->pos.z) < SETTLE_DRAW) draw_settlement(t, &w->settlements[i], i, time);
    for (int i = 0; i < w->den_count; i++)
        if (dist_xz(w->dens[i].x, w->dens[i].z, p->pos.x, p->pos.z) < DEN_DRAW) draw_den(t, &w->dens[i]);
}
