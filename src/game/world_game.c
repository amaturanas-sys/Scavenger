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

    // Las tribus nomadas.
    for (int i = 0; i < w->tribe_count && i < 32; i++) {
        const TribeCamp *tc = &w->tribes[i];
        float d = dist_xz(tc->x, tc->z, p->pos.x, p->pos.z);
        if (tc->attitude == TRIBE_RIVAL) {
            if (d > TRIBE_REARM) ga->raided_tribes &= ~(1u << i);
            if (d < TRIBE_RAID && !((ga->raided_tribes >> i) & 1u)) { // salen al paso
                ga->raided_tribes |= 1u << i;
                char tmp[160];
                cb_spawn_group(cb, ga, i % 2 ? "jinetes" : "bandidos", p, t, 45.0f, tmp, sizeof(tmp));
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

// Un campamento de tribu: yurtas (o las casas de su region) en corro, el estandarte del color
// de su actitud (rojo rival, crudo neutral, azul amiga), fogata, gente y caballos.
static void draw_tribe(const Terrain *t, const TribeCamp *tc, int idx, float time) {
    static const Color BANNER[TRIBE_ATTITUDES] = { { 168, 36, 32, 255 }, { 214, 206, 186, 255 }, { 52, 98, 176, 255 } };
    float ground = terrain_height(t, tc->x, tc->z);
    for (int i = 0; i < 5; i++) {
        float a = (float)i / 5.0f * 2.0f * PI + idx, x = tc->x + cosf(a) * 13.0f, z = tc->z + sinf(a) * 13.0f;
        building(tc->region == REGION_DESERT ? REGION_STEPPE : tc->region, (Vector3){ x, terrain_height(t, x, z), z }, -a, 0.9f);
    }
    Vector3 pole = { tc->x + 3.0f, ground, tc->z };
    DrawCylinder(pole, 0.1f, 0.1f, 6.5f, 5, (Color){ 90, 70, 50, 255 });
    float wave = sinf(time * 2.3f + idx) * 0.25f;
    DrawCube((Vector3){ pole.x + 0.8f, ground + 5.8f + wave * 0.2f, pole.z }, 1.6f, 1.0f, 0.08f, BANNER[tc->attitude]);
    DrawCylinder((Vector3){ tc->x, ground + 0.1f, tc->z }, 0.0f, 0.4f, 0.8f, 5, (Color){ 240, 140, 40, 255 });
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
    for (int k = 0; k < 2; k++) { // caballos atados
        float x = tc->x - 8.0f + (float)k * 2.5f, z = tc->z + 9.0f;
        cb_draw_horse((Vector3){ x, terrain_height(t, x, z), z }, 0.3f * (float)k, 0.0f, time + k, false);
    }
}

static void slab(Vector3 at, float w, float h, float d, float yaw, Color c) {
    rlPushMatrix();
    rlTranslatef(at.x, at.y, at.z);
    rlRotatef(yaw * RAD2DEG, 0, 1, 0);
    DrawCube((Vector3){ 0, h * 0.5f, 0 }, w, h, d, c);
    rlPopMatrix();
}

// Una estructura, con piezas simples (low-poly) al estilo de su region.
static void draw_site(const Terrain *t, const WorldSite *st, float time) {
    float y = terrain_height(t, st->x, st->z);
    Vector3 c = { st->x, y, st->z };
    const Color stone = st->region == REGION_DESERT ? (Color){ 186, 150, 104, 255 } : (Color){ 132, 128, 120, 255 };
    const Color dark = { 96, 92, 86, 255 }, wood = { 108, 80, 54, 255 };
    float cs = cosf(st->yaw), sn = sinf(st->yaw);
#define AT(dx, dz) ((Vector3){ c.x + (dx) * cs - (dz) * sn, terrain_height(t, c.x + (dx) * cs - (dz) * sn, c.z + (dx) * sn + (dz) * cs), c.z + (dx) * sn + (dz) * cs })
    switch (st->kind) {
    case SITE_KURGAN: // tumulo de tierra con piedras alrededor
        rlPushMatrix();
        rlTranslatef(c.x, y - 1.0f, c.z);
        rlScalef(1.0f, 0.35f, 1.0f);
        DrawSphereEx((Vector3){ 0 }, 9.0f, 6, 10, st->region == REGION_FJORD ? (Color){ 92, 118, 80, 255 } : (Color){ 122, 128, 76, 255 });
        rlPopMatrix();
        for (int i = 0; i < 10; i++) {
            float a = (float)i / 10.0f * 2.0f * PI;
            DrawSphereEx(AT(cosf(a) * 10.5f, sinf(a) * 10.5f), 0.6f, 3, 5, stone);
        }
        break;
    case SITE_BALBALS: // hilera de estelas mirando al este
        for (int i = 0; i < 6; i++) slab(AT((float)i * 2.2f - 5.5f, 0.0f), 0.5f, 1.4f + 0.3f * (float)(i % 3), 0.35f, st->yaw, stone);
        break;
    case SITE_DEER_STONE: // monolito alto con bandas grabadas
        slab(c, 0.8f, 3.6f, 0.45f, st->yaw, dark);
        for (int i = 0; i < 3; i++) slab((Vector3){ c.x, y + 0.9f + i * 0.9f, c.z }, 0.84f, 0.12f, 0.5f, st->yaw, (Color){ 150, 146, 136, 255 });
        break;
    case SITE_RUINED_FORT: // muros derruidos y una torre rota
        for (int i = 0; i < 4; i++) {
            float a = (float)i * PI * 0.5f, h = 1.5f + 2.0f * (float)((i * 7) % 3) / 2.0f;
            Vector3 m = AT(cosf(a) * 11.0f, sinf(a) * 11.0f);
            slab(m, i % 2 ? 1.2f : 16.0f, h, i % 2 ? 16.0f : 1.2f, st->yaw, stone);
        }
        DrawCylinder(AT(11.0f, 11.0f), 2.2f, 2.4f, 6.0f, 7, stone);
        break;
    case SITE_BURIED_CITY: // cupulas y muros asomando de la arena
        for (int i = 0; i < 5; i++) {
            Vector3 m = AT((float)(i % 3) * 9.0f - 9.0f, (float)(i / 3) * 10.0f - 5.0f);
            slab((Vector3){ m.x, m.y - 1.0f, m.z }, 5.0f, 2.2f, 5.0f, st->yaw, stone);
            if (i % 2 == 0) DrawSphereEx((Vector3){ m.x, m.y + 1.0f, m.z }, 2.0f, 5, 8, (Color){ 200, 166, 118, 255 });
        }
        break;
    case SITE_PETROGLYPHS: // peñas con grabados claros
        for (int i = 0; i < 3; i++) {
            Vector3 m = AT((float)i * 3.0f - 3.0f, (float)(i % 2) * 2.0f);
            DrawSphereEx((Vector3){ m.x, m.y + 0.8f, m.z }, 1.6f, 4, 6, dark);
            slab((Vector3){ m.x, m.y + 1.0f, m.z - 1.45f }, 1.0f, 0.5f, 0.06f, st->yaw, (Color){ 210, 196, 160, 255 });
        }
        break;
    case SITE_CARAVANSERAI: // patio amurallado con portada y camellos
        for (int i = 0; i < 4; i++) {
            float a = (float)i * PI * 0.5f;
            slab(AT(cosf(a) * 12.0f, sinf(a) * 12.0f), i % 2 ? 1.0f : 24.0f, 4.0f, i % 2 ? 24.0f : 1.0f, st->yaw, stone);
        }
        slab(AT(12.5f, 0.0f), 2.0f, 6.0f, 6.0f, st->yaw, (Color){ 168, 132, 92, 255 }); // portada
        slab(AT(0.0f, 0.0f), 3.0f, 0.4f, 3.0f, st->yaw, (Color){ 60, 90, 120, 255 });     // la fuente
        break;
    case SITE_WATCHTOWER: // torre de madera (o piedra) con techo
        if (st->region == REGION_FOREST || st->region == REGION_STEPPE) {
            for (int i = 0; i < 4; i++) DrawCylinder(AT(i % 2 ? 1.4f : -1.4f, i / 2 ? 1.4f : -1.4f), 0.15f, 0.15f, 8.0f, 4, wood);
            slab((Vector3){ c.x, y + 8.0f, c.z }, 3.6f, 0.3f, 3.6f, st->yaw, wood);
            DrawCylinder((Vector3){ c.x, y + 8.3f, c.z }, 0.0f, 2.6f, 1.6f, 4, (Color){ 84, 66, 48, 255 });
        } else {
            DrawCylinder(c, 2.0f, 2.3f, 9.0f, 8, stone);
            DrawCylinder((Vector3){ c.x, y + 9.0f, c.z }, 2.5f, 2.5f, 0.8f, 8, dark);
        }
        break;
    case SITE_OVOO: { // cono de piedras con un palo y cintas que ondean
        DrawCylinder(c, 0.4f, 2.2f, 1.8f, 7, stone);
        DrawCylinder((Vector3){ c.x, y + 1.6f, c.z }, 0.06f, 0.06f, 2.6f, 4, wood);
        static const Color RIB[3] = { { 60, 110, 200, 255 }, { 230, 230, 220, 255 }, { 220, 190, 60, 255 } };
        for (int i = 0; i < 3; i++) {
            float wv = sinf(time * 3.0f + i) * 0.3f;
            DrawCube((Vector3){ c.x + 0.4f + wv * 0.2f, y + 3.6f - i * 0.3f, c.z + (float)i * 0.1f }, 0.8f, 0.12f, 0.03f, RIB[i]);
        }
        break;
    }
    case SITE_WELL: // brocal de piedra con travesaño y cubo
        DrawCylinder(c, 1.1f, 1.1f, 0.9f, 8, stone);
        DrawCylinder((Vector3){ c.x, y + 0.85f, c.z }, 0.8f, 0.8f, 0.08f, 8, (Color){ 40, 60, 80, 255 });
        DrawCylinder(AT(-1.0f, 0.0f), 0.08f, 0.08f, 2.2f, 4, wood);
        DrawCylinder(AT(1.0f, 0.0f), 0.08f, 0.08f, 2.2f, 4, wood);
        slab((Vector3){ c.x, y + 2.1f, c.z }, 2.2f, 0.12f, 0.12f, st->yaw, wood);
        break;
    case SITE_HARBOR: // muelle de tablas hacia el agua y una barca
        for (int i = 0; i < 5; i++) {
            Vector3 m = AT(0.0f, (float)i * 3.0f);
            slab((Vector3){ m.x, y + 0.6f, m.z }, 2.4f, 0.2f, 3.0f, st->yaw, wood);
            DrawCylinder((Vector3){ m.x + 1.1f * cs, m.y - 1.0f, m.z + 1.1f * sn }, 0.12f, 0.12f, 1.9f, 4, (Color){ 80, 60, 44, 255 });
        }
        slab(AT(3.0f, 10.0f), 1.6f, 0.6f, 6.0f, st->yaw, (Color){ 96, 70, 48, 255 });
        break;
    default: break;
    }
#undef AT
}

void wd_draw_world(const GameActions *ga, const Terrain *t, const Player *p, float time) {
    (void)ga;
    const World *w = t->world;
    for (int i = 0; i < w->settlement_count; i++)
        if (dist_xz(w->settlements[i].x, w->settlements[i].z, p->pos.x, p->pos.z) < SETTLE_DRAW) draw_settlement(t, &w->settlements[i], i, time);
    for (int i = 0; i < w->den_count; i++)
        if (dist_xz(w->dens[i].x, w->dens[i].z, p->pos.x, p->pos.z) < DEN_DRAW) draw_den(t, &w->dens[i]);
    for (int i = 0; i < w->tribe_count; i++)
        if (dist_xz(w->tribes[i].x, w->tribes[i].z, p->pos.x, p->pos.z) < TRIBE_DRAW) draw_tribe(t, &w->tribes[i], i, time);
    for (int i = 0; i < w->site_count; i++)
        if (dist_xz(w->sites[i].x, w->sites[i].z, p->pos.x, p->pos.z) < SITE_DRAW) draw_site(t, &w->sites[i], time);
}
