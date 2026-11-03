#include "game/world_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"
#include "sim/lang.h"
#include "world/body_draw.h"
#include "world/hearth.h"
#include "world/voxstruct.h"

#define SETTLE_ARRIVE 90.0f
#define DEN_DRAW 180.0f
#define TRIBE_DRAW 260.0f
#define TRIBE_ARRIVE 70.0f
#define TRIBE_RAID 120.0f  // las rivales salen al paso a esta distancia
#define TRIBE_REARM 600.0f // alejarse rearma la emboscada
#define SITE_DRAW 240.0f
#define SITE_SEEN 45.0f

static float dist_xz(float ax, float az, float bx, float bz) { return sqrtf((ax - bx) * (ax - bx) + (az - bz) * (az - bz)); }

static const char *ATTITUDE_TEXT[TRIBE_ATTITUDES] = { N_("rival"), N_("neutral"), N_("amiga") };

void wd_update(GameActions *ga, Combat *cb, const Terrain *t, const Player *p, MemoryMap *mem, char *log, size_t len) {
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
    // Una guarida, con su cueva (src/sim/caves.h): peligro, al mapa.
    for (int i = 0; i < w->den_count && i < 64; i++) {
        const Den *d = &w->dens[i];
        if ((ga->seen_dens >> i) & 1u || dist_xz(d->x, d->z, p->pos.x, p->pos.z) > 45.0f) continue;
        ga->seen_dens |= 1ull << i;
        memmap_toggle_marker(mem, d->x, d->z, MARKER_DANGER, 4.0f);
        char who[48];
        snprintf(who, sizeof(who), "%s", T(species_def((Species)d->species)->name));
        if (who[0] >= 'A' && who[0] <= 'Z') who[0] = (char)(who[0] - 'A' + 'a');
        CaveKind kind = i < w->cave_count ? w->caves[i].kind : CAVE_ANIMAL;
        if (kind >= CAVE_BANDITS)
            snprintf(log, len, T("Una cueva (%s): marcada como peligro. Se entra por la boca."), T(cave_kind_name(kind)));
        else if (kind == CAVE_ABANDONED)
            snprintf(log, len, T("Una cueva abandonada (huellas de %s): marcada como peligro."), who);
        else snprintf(log, len, T("Una guarida (%s): queda marcada como peligro en el mapa."), who);
    }

    // Las tribus nomadas.
    for (int i = 0; i < w->tribe_count && i < 32; i++) {
        const TribeCamp *tc = &w->tribes[i];
        float d = dist_xz(tc->x, tc->z, p->pos.x, p->pos.z);
        if (tc->attitude == TRIBE_RIVAL) {
            if (d > TRIBE_REARM) ga->raided_tribes &= ~(1u << i);
            if (d < TRIBE_RAID && !((ga->raided_tribes >> i) & 1u)) { // salen al paso
                ga->raided_tribes |= 1u << i;
                char tmp[160];
                cb_spawn_group(cb, ga, i % 3 == 2 ? "arqueros_caballo" : i % 2 ? "jinetes" : "bandidos", p, t, 45.0f, tmp, sizeof(tmp)); // nomadas: a caballo
                memmap_toggle_marker(mem, tc->x, tc->z, MARKER_DANGER, 4.0f);
                ga->seen_tribes |= 1u << i;
                snprintf(log, len, T("¡Los %s, tribu rival, te salen al paso!"), tc->name);
                continue;
            }
        }
        if ((ga->seen_tribes >> i) & 1u || d > TRIBE_ARRIVE) continue;
        ga->seen_tribes |= 1u << i;
        if (tc->attitude == TRIBE_FRIENDLY) { // comparten noticias: la estructura mas cercana sin ver, al mapa
            int best = -1;
            float bd = 1e18f;
            for (int k = 0; k < w->site_count && k < 64; k++) {
                float sd = dist_xz(w->sites[k].x, w->sites[k].z, tc->x, tc->z);
                if (!((ga->seen_sites >> k) & 1ull) && sd < bd) bd = sd, best = k;
            }
            memmap_toggle_marker(mem, tc->x, tc->z, MARKER_INTEREST, 4.0f);
            if (best >= 0) {
                memmap_toggle_marker(mem, w->sites[best].x, w->sites[best].z, MARKER_INTEREST, 4.0f);
                snprintf(log, len, T("Los %s, tribu amiga, te reciben y te señalan %s en el mapa."), tc->name, T(site_name(w->sites[best].kind)));
            } else {
                snprintf(log, len, T("Los %s, tribu amiga, te reciben junto a su fuego."), tc->name);
            }
        } else {
            memmap_toggle_marker(mem, tc->x, tc->z, tc->attitude == TRIBE_RIVAL ? MARKER_DANGER : MARKER_INTEREST, 4.0f);
            snprintf(log, len, T("El campamento de los %s (tribu %s): te observan, sin buscar pelea."), tc->name, T(ATTITUDE_TEXT[tc->attitude]));
        }
    }
    // Las estructuras: al verlas, al mapa.
    for (int i = 0; i < w->site_count && i < 64; i++) {
        const WorldSite *st = &w->sites[i];
        if ((ga->seen_sites >> i) & 1ull || dist_xz(st->x, st->z, p->pos.x, p->pos.z) > SITE_SEEN) continue;
        ga->seen_sites |= 1ull << i;
        memmap_toggle_marker(mem, st->x, st->z, MARKER_INTEREST, 4.0f);
        if (st->kind == SITE_WELL || st->kind == SITE_CARAVANSERAI)
            snprintf(log, len, T("Encuentras %s: agua limpia (L: llenar el odre · N: beber). Queda en el mapa."), T(site_name(st->kind)));
        else
            snprintf(log, len, site_is_ruin(st->kind) ? T("Encuentras %s, de otros tiempos: queda en el mapa.") : T("Encuentras %s: queda en el mapa."),
                     T(site_name(st->kind)));
    }
}

int wd_den_near(const Terrain *t, float x, float z, float radius) {
    float d;
    int i = world_nearest_den(t->world, x, z, &d);
    return d < radius ? i : -1;
}

// ------------------------------------------------------------------ dibujo
// Las casas de cada region, del kit de la edad del hierro (src/sim/buildkit.h): la puerta mira al
// centro. variant: la 0 y la 1, chicas; la 2 y la 3, grandes.
static void building(BkType bt, Region rg, int variant, Vector3 at, float yaw) {
    voxs_draw_kit(bt, rg, variant, at, yaw + PI, 1.0f, WHITE); // la puerta (+x del modelo) hacia el centro
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

static bool g_fires_out;

// La boca de una cueva, desde afuera (el interior: src/game/cave_game.c). En las coordenadas de la cueva (la boca
// en el origen, +z hacia adentro): un peñasco con un tunel que se oscurece hacia el fondo, y lo de cada tipo: huesos
// en la de fieras, piedras caidas en la abandonada, antorchas en las habitadas, el portal de piedra labrada y los
// estandartes en la del imperio, la cortina de cuero en la de una tribu.
static void gradient_quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color front, Color back) { // a y b adelante; c y d al fondo
    rlBegin(RL_TRIANGLES);
    rlColor4ub(front.r, front.g, front.b, 255), rlVertex3f(a.x, a.y, a.z);
    rlColor4ub(front.r, front.g, front.b, 255), rlVertex3f(b.x, b.y, b.z);
    rlColor4ub(back.r, back.g, back.b, 255), rlVertex3f(c.x, c.y, c.z);
    rlColor4ub(front.r, front.g, front.b, 255), rlVertex3f(a.x, a.y, a.z);
    rlColor4ub(back.r, back.g, back.b, 255), rlVertex3f(c.x, c.y, c.z);
    rlColor4ub(back.r, back.g, back.b, 255), rlVertex3f(d.x, d.y, d.z);
    rlEnd();
}

static void mouth_torch(float x, float z, float base, float time, int k) {
    DrawCylinderEx((Vector3){ x, base, z }, (Vector3){ x, base + 1.7f, z }, 0.05f, 0.06f, 5, (Color){ 92, 66, 42, 255 });
    float fl = 0.85f + 0.15f * sinf(time * 17.0f + (float)k);
    DrawSphere((Vector3){ x, base + 1.85f, z }, 0.14f * fl, (Color){ 250, 150, 50, 255 });
    DrawSphere((Vector3){ x, base + 1.88f, z }, 0.07f * fl, (Color){ 255, 236, 170, 255 });
}

static void draw_cave(const Terrain *t, const Cave *c, float time) {
    // El pie del peñasco: lo mas bajo del terreno alrededor (en una ladera no queda en el aire).
    float ground = terrain_height(t, c->x, c->z), base = ground;
    for (int k = 0; k < 6; k++) {
        float wx, wz;
        cave_to_world(c, (k % 3 - 1) * 3.0f, (float)(k / 3) * 4.0f, &wx, &wz);
        base = fminf(base, terrain_height(t, wx, wz));
    }
    base -= 0.4f;
    Color rock = c->region == REGION_DESERT ? (Color){ 170, 120, 84, 255 } : c->region == REGION_HIGHLAND ? (Color){ 124, 126, 132, 255 } : (Color){ 112, 108, 100, 255 };
    Color dark = ColorBrightness(rock, -0.25f), dressed = { 196, 184, 160, 255 };
    bool empire = c->kind == CAVE_EMPIRE;
    float top = ground - base + 3.0f; // el alto de la abertura (sobre el pie)
    rlPushMatrix();
    rlTranslatef(c->x, base, c->z);
    rlRotatef(c->yaw * RAD2DEG, 0, 1, 0); // +z del dibujo: hacia adentro (cave_to_world)
    // El peñasco: dos jambas, el dintel y un monticulo encima; la abertura, de 2,8 m.
    DrawCube((Vector3){ -2.7f, top * 0.5f, 2.6f }, 2.6f, top, 4.6f, empire ? dressed : rock);
    DrawCube((Vector3){ 2.7f, top * 0.5f, 2.6f }, 2.6f, top, 4.6f, empire ? dressed : rock);
    DrawCube((Vector3){ 0.0f, top + 0.7f, 2.6f }, 8.0f, 1.4f, 4.6f, empire ? dressed : rock);
    DrawSphereEx((Vector3){ -1.4f, top + 1.2f, 3.6f }, 2.6f, 6, 8, dark);
    DrawSphereEx((Vector3){ 1.8f, top + 1.0f, 4.0f }, 2.9f, 6, 8, rock);
    DrawSphereEx((Vector3){ -3.6f, top * 0.6f, 4.2f }, 2.2f, 6, 8, dark);
    DrawSphereEx((Vector3){ 3.8f, top * 0.55f, 4.4f }, 2.3f, 6, 8, rock);
    DrawCube((Vector3){ 0.0f, top * 0.5f + 0.2f, 5.6f }, 9.0f, top + 1.8f, 2.4f, dark); // el fondo del peñasco
    // El tunel: se oscurece hacia adentro (a los 4 m ya no se ve nada). Sin descartar caras (vaciando el lote antes
    // y despues: raylib dibuja al final).
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    float g0 = ground - base + 0.02f;
    Color front = ColorBrightness(rock, -0.55f), back = { 6, 6, 7, 255 };
    gradient_quad((Vector3){ -1.4f, g0, 0.3f }, (Vector3){ 1.4f, g0, 0.3f }, (Vector3){ 1.4f, g0, 4.3f }, (Vector3){ -1.4f, g0, 4.3f }, front, back);
    gradient_quad((Vector3){ -1.4f, top, 0.3f }, (Vector3){ 1.4f, top, 0.3f }, (Vector3){ 1.4f, top, 4.3f }, (Vector3){ -1.4f, top, 4.3f }, front, back);
    gradient_quad((Vector3){ -1.4f, g0, 0.3f }, (Vector3){ -1.4f, top, 0.3f }, (Vector3){ -1.4f, top, 4.3f }, (Vector3){ -1.4f, g0, 4.3f }, front, back);
    gradient_quad((Vector3){ 1.4f, g0, 0.3f }, (Vector3){ 1.4f, top, 0.3f }, (Vector3){ 1.4f, top, 4.3f }, (Vector3){ 1.4f, g0, 4.3f }, front, back);
    DrawCube((Vector3){ 0.0f, top * 0.5f, 4.35f }, 2.8f, top, 0.1f, back);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    float t0 = ground - base;
    switch (c->kind) {
    case CAVE_ANIMAL: // huesos y un craneo a la entrada
        for (int k = 0; k < 4; k++) {
            float a = (float)k * 1.9f, x = -1.2f + (float)k * 0.8f, z = -1.4f + 0.3f * sinf(a);
            DrawCylinderEx((Vector3){ x - cosf(a) * 0.3f, t0 + 0.04f, z - sinf(a) * 0.3f }, (Vector3){ x + cosf(a) * 0.3f, t0 + 0.04f, z + sinf(a) * 0.3f },
                           0.035f, 0.035f, 4, (Color){ 222, 214, 192, 255 });
        }
        DrawSphere((Vector3){ 0.6f, t0 + 0.1f, -0.8f }, 0.13f, (Color){ 232, 224, 202, 255 });
        break;
    case CAVE_ABANDONED: // piedras caidas
        DrawCube((Vector3){ -0.9f, t0 + 0.25f, -0.6f }, 0.7f, 0.5f, 0.6f, dark);
        DrawCube((Vector3){ 1.1f, t0 + 0.18f, -1.3f }, 0.5f, 0.36f, 0.5f, rock);
        DrawCube((Vector3){ 0.3f, t0 + 0.12f, 0.6f }, 0.4f, 0.24f, 0.35f, dark);
        break;
    default: // las habitadas: antorchas a los lados
        mouth_torch(-1.7f, -0.4f, t0, time, 0);
        mouth_torch(1.7f, -0.4f, t0, time, 1);
        if (c->kind == CAVE_TRIBE) { // la cortina de cuero, corrida a un lado, y el sol pintado en el dintel
            DrawCube((Vector3){ -0.9f, t0 + top * 0.5f, 0.35f }, 1.0f, top - t0, 0.05f, (Color){ 120, 84, 52, 255 });
            DrawCylinderEx((Vector3){ 0.0f, top + 0.7f, 0.29f }, (Vector3){ 0.0f, top + 0.7f, 0.27f }, 0.35f, 0.35f, 10, (Color){ 168, 58, 36, 255 });
        } else if (empire) { // los estandartes del imperio
            for (int k = -1; k <= 1; k += 2) {
                DrawCube((Vector3){ 3.0f * (float)k, top - 0.6f, -0.05f }, 0.9f, 1.6f, 0.05f, (Color){ 150, 32, 30, 255 });
                DrawCube((Vector3){ 3.0f * (float)k, top - 0.6f, -0.08f }, 0.3f, 0.3f, 0.05f, (Color){ 220, 180, 70, 255 });
            }
        } else { // los forajidos: una estacada
            for (int k = 0; k < 5; k++) {
                float x = (k < 2 ? -3.8f : 2.2f) + (float)(k % 3) * 0.55f;
                DrawCylinderEx((Vector3){ x, t0, -1.2f }, (Vector3){ x, t0 + 1.3f, -1.25f }, 0.07f, 0.03f, 4, (Color){ 104, 78, 52, 255 });
            }
        }
        break;
    }
    rlPopMatrix();
}

// Adentro del peñasco de una boca (draw_cave), fuera del tunel: la roca.
static bool in_outcrop(const Cave *c, float ground, Vector3 q) {
    float lx, lz;
    cave_from_world(c, q.x, q.z, &lx, &lz);
    if (fabsf(lx) > 5.0f || lz < 0.2f || lz > 7.0f || q.y > ground + 6.5f) return false;
    return !(fabsf(lx) < 1.3f && lz < 4.2f && q.y < ground + 2.8f);
}

void wd_camera_clear(const Terrain *t, Vector3 target, Vector3 *cam) {
    const World *w = t->world;
    for (int i = 0; i < w->cave_count; i++) {
        const Cave *c = &w->caves[i];
        if (dist_xz(c->x, c->z, target.x, target.z) > 25.0f) continue;
        float ground = terrain_height(t, c->x, c->z);
        for (int k = 0; k < 40 && in_outcrop(c, ground, *cam); k++) *cam = Vector3Lerp(*cam, target, 0.12f); // hacia el jugador
    }
}

int wd_cave_lights(const Terrain *t, const Player *p, Vector3 *pos, float *radius, int max) {
    const World *w = t->world;
    int n = 0;
    for (int i = 0; i < w->cave_count && n + 1 < max; i++) {
        const Cave *c = &w->caves[i];
        if (c->kind < CAVE_BANDITS || dist_xz(c->x, c->z, p->pos.x, p->pos.z) > 120.0f) continue;
        float ground = terrain_height(t, c->x, c->z);
        for (int k = -1; k <= 1 && n < max; k += 2) {
            float wx, wz;
            cave_to_world(c, 1.7f * (float)k, -0.4f, &wx, &wz);
            pos[n] = (Vector3){ wx, ground + 1.9f, wz };
            radius[n++] = 6.0f;
        }
    }
    return n;
}

// Un campamento de tribu: yurtas (o las casas de su region) en corro, la del jefe mas grande, el
// estandarte del color de su actitud (rojo rival, crudo neutral, azul amiga), fogata, gente y
// caballos. Los de la estepa (y los nomadas del desierto) tienen carretas; los demas, un granero.
static void draw_tribe(const Terrain *t, const TribeCamp *tc, int idx, float time) {
    static const Color BANNER[TRIBE_ATTITUDES] = { { 168, 36, 32, 255 }, { 214, 206, 186, 255 }, { 52, 98, 176, 255 } };
    float ground = terrain_height(t, tc->x, tc->z);
    Region rg = tc->region == REGION_DESERT ? REGION_STEPPE : tc->region;
    for (int i = 0; i < 5; i++) {
        float a = (float)i / 5.0f * 2.0f * PI + idx, x = tc->x + cosf(a) * 13.0f, z = tc->z + sinf(a) * 13.0f;
        BkType bt = i == 4 ? (rg == REGION_STEPPE ? BK_WAGON : BK_GRANARY) : i == 2 && rg == REGION_STEPPE ? BK_WAGON : BK_HOUSE;
        building(bt, rg, i == 0 ? 2 + (idx & 1) : (i + idx) & 1, (Vector3){ x, terrain_height(t, x, z), z }, -a);
    }
    Vector3 pole = { tc->x + 3.0f, ground, tc->z };
    DrawCylinder(pole, 0.1f, 0.1f, 6.5f, 5, (Color){ 90, 70, 50, 255 });
    float wave = sinf(time * 2.3f + idx) * 0.25f;
    DrawCube((Vector3){ pole.x + 0.8f, ground + 5.8f + wave * 0.2f, pole.z }, 1.6f, 1.0f, 0.08f, BANNER[tc->attitude]);
    hearth_draw_base((Vector3){ tc->x, ground, tc->z }, 1.2f, 200u + (uint32_t)idx, !g_fires_out);
    for (int k = 0; k < 3; k++) {
        float a = time * 0.04f + (float)k * 2.1f + idx, r = 5.0f;
        Vector3 pos = { tc->x + cosf(a) * r, 0, tc->z + sinf(a) * r };
        pos.y = terrain_height(t, pos.x, pos.z);
        BodyPose pose;
        BodyPoseParams bp = { .walk_phase = time * 6.0f + k, .walk = 0.0f, .scale = 1.0f };
        body_pose(&pose, &bp);
        BodyColors bc = { { 200, 160, 120, 255 }, k == 0 ? BANNER[tc->attitude] : folk_color(tc->region, k), { 84, 64, 46, 255 } };
        body_draw(&pose, pos, a + PI, bc, NULL);
    }
    for (int k = 0; k < 2; k++) { // caballos atados, adentro del corro
        float x = tc->x - 4.0f + (float)k * 2.5f, z = tc->z + 6.5f;
        cb_draw_horse((Vector3){ x, terrain_height(t, x, z), z }, 0.3f * (float)k, 0.0f, time + k, false);
    }
}

// Una estructura: el modelo de voxeles de su clase, al estilo de su region (src/world/voxstruct.c).
static void draw_site(const Terrain *t, const WorldSite *st, float time) {
    (void)time;
    voxs_draw_site(st->kind, st->region, (Vector3){ st->x, terrain_height(t, st->x, st->z), st->z }, st->yaw, WHITE);
}

void wd_draw_world(const GameActions *ga, const Terrain *t, const Player *p, float time) {
    g_fires_out = ga->fires_out; // la lluvia apaga tambien los fuegos de los demas
    const World *w = t->world;
    for (int i = 0; i < w->cave_count; i++)
        if (dist_xz(w->caves[i].x, w->caves[i].z, p->pos.x, p->pos.z) < DEN_DRAW) draw_cave(t, &w->caves[i], time);
    for (int i = 0; i < w->tribe_count; i++)
        if (dist_xz(w->tribes[i].x, w->tribes[i].z, p->pos.x, p->pos.z) < TRIBE_DRAW) draw_tribe(t, &w->tribes[i], i, time);
    for (int i = 0; i < w->site_count; i++)
        if (dist_xz(w->sites[i].x, w->sites[i].z, p->pos.x, p->pos.z) < SITE_DRAW) draw_site(t, &w->sites[i], time);
}
