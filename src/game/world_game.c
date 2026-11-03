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
// en el origen, +z hacia adentro): un peñasco natural con un tunel que se oscurece hacia el fondo, y lo de cada tipo:
// huesos en la de fieras; en la abandonada, piedras caidas, una escalera rota y la fachada de una civilizacion
// desaparecida; en las habitadas, antorchas y una escalera hasta un saliente, con la cortina y el bastidor de cuero
// en la de una tribu, los estandartes en la del imperio y la estacada en la de los forajidos.
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

// La boca natural sobre el suelo de la boca (g: el suelo menos el pie del peñasco). El contorno de adentro deja el paso
// del tunel (2,8 m de ancho y 3 de alto) y el de afuera, la silueta de la roca. Sus extremos bajan hasta el pie, asi
// que en una ladera la abertura conserva su forma.
#define MOUTH_POINTS 8
static const float MOUTH_IN_X[MOUTH_POINTS] = { -1.42f, -1.42f, -1.30f, -0.85f, 0.15f, 0.95f, 1.38f, 1.42f };
static const float MOUTH_IN_Y[MOUTH_POINTS] = { 0.00f, 1.00f, 2.00f, 2.65f, 3.00f, 2.62f, 1.85f, 0.00f };
static const float MOUTH_OUT_X[MOUTH_POINTS] = { -4.30f, -4.40f, -3.75f, -2.40f, 0.20f, 2.60f, 3.90f, 4.55f };
static const float MOUTH_OUT_Y[MOUTH_POINTS] = { 0.00f, 1.40f, 3.35f, 4.75f, 5.60f, 4.85f, 3.20f, 0.00f };
// Las dos rocas bajas a los costados del umbral (x, z y radio, en el plano de la cueva). Asoman delante de la huella
// del peñasco, asi que wd_keep_out_of_rock tambien las rodea.
static const float MOUTH_BOULDERS[2][3] = { { -3.15f, 0.55f, 1.35f }, { 3.30f, 0.70f, 1.45f } };

static void draw_natural_mouth(Color rock, float g) {
    Color facets[MOUTH_POINTS - 1] = {
        ColorBrightness(rock, -0.18f), rock, ColorBrightness(rock, 0.10f), ColorBrightness(rock, -0.08f),
        ColorBrightness(rock, 0.04f), ColorBrightness(rock, -0.22f), ColorBrightness(rock, -0.04f),
    };
    Vector3 in[MOUTH_POINTS], out[MOUTH_POINTS], deep[MOUTH_POINTS];
    for (int i = 0; i < MOUTH_POINTS; i++) {
        bool foot = i == 0 || i == MOUTH_POINTS - 1; // los extremos, hasta el pie del peñasco
        in[i] = (Vector3){ MOUTH_IN_X[i], foot ? 0.0f : g + MOUTH_IN_Y[i], 0.28f };
        out[i] = (Vector3){ MOUTH_OUT_X[i], foot ? 0.0f : g + MOUTH_OUT_Y[i], 0.28f };
        deep[i] = (Vector3){ out[i].x, out[i].y, 4.25f };
    }
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < MOUTH_POINTS - 1; i++) {
        Color c = facets[i];
        rlColor4ub(c.r, c.g, c.b, 255);
        rlVertex3f(out[i].x, out[i].y, out[i].z);
        rlVertex3f(out[i + 1].x, out[i + 1].y, out[i + 1].z);
        rlVertex3f(in[i + 1].x, in[i + 1].y, in[i + 1].z);
        rlVertex3f(out[i].x, out[i].y, out[i].z);
        rlVertex3f(in[i + 1].x, in[i + 1].y, in[i + 1].z);
        rlVertex3f(in[i].x, in[i].y, in[i].z);
        // La cara exterior se mete en la ladera para que el borde no sea una lámina plana.
        c = ColorBrightness(c, -0.20f);
        rlColor4ub(c.r, c.g, c.b, 255);
        rlVertex3f(out[i].x, out[i].y, out[i].z);
        rlVertex3f(deep[i].x, deep[i].y, deep[i].z);
        rlVertex3f(deep[i + 1].x, deep[i + 1].y, deep[i + 1].z);
        rlVertex3f(out[i].x, out[i].y, out[i].z);
        rlVertex3f(deep[i + 1].x, deep[i + 1].y, deep[i + 1].z);
        rlVertex3f(out[i + 1].x, out[i + 1].y, out[i + 1].z);
    }
    rlEnd();
    // Bloques facetados: rompen la silueta recta y dan escala al umbral. Los de arriba quedan casi al ras de la cara y
    // sin meterse en el tunel.
    DrawSphereEx((Vector3){ MOUTH_BOULDERS[0][0], g + 0.25f, MOUTH_BOULDERS[0][1] }, MOUTH_BOULDERS[0][2], 6, 5, ColorBrightness(rock, -0.12f));
    DrawSphereEx((Vector3){ MOUTH_BOULDERS[1][0], g + 0.40f, MOUTH_BOULDERS[1][1] }, MOUTH_BOULDERS[1][2], 6, 5, ColorBrightness(rock, -0.20f));
    DrawSphereEx((Vector3){ -2.80f, g + 2.75f, 1.45f }, 1.35f, 6, 5, ColorBrightness(rock, 0.04f));
    DrawSphereEx((Vector3){ 2.90f, g + 3.30f, 1.75f }, 1.55f, 6, 5, ColorBrightness(rock, -0.08f));
}

// El suelo en un punto de la boca (x, z en el plano de la cueva), sobre el pie del peñasco.
static float mouth_ground(const Terrain *t, const Cave *c, float base, float lx, float lz) {
    float wx, wz;
    cave_to_world(c, lx, lz, &wx, &wz);
    return terrain_height(t, wx, wz) - base;
}

static Vector3 ladder_point(Vector3 low, Vector3 high, float t, float side) {
    Vector3 p = Vector3Lerp(low, high, t);
    Vector3 across = Vector3Normalize((Vector3){ high.z - low.z, 0.0f, low.x - high.x });
    return Vector3Add(p, Vector3Scale(across, side));
}

// Lo que dejan los que viven en la cueva (las habitadas y la abandonada; la de fieras, nada): una escalera de mano a un
// costado de la boca, hasta un saliente de tablas sobre puntales (el del imperio, de sillares y con baranda), y en la
// de una tribu, un bastidor con un cuero al otro costado. En la abandonada, la escalera rota apoyada en la roca. Todo
// queda fuera del paso (|x| < 1,4 m) y sobre el suelo de su punto, tambien en una ladera. Es decorado: no tiene colision.
static void draw_human_entrance(const Terrain *t, const Cave *c, float base, float g) {
    CaveKind kind = c->kind;
    bool empire = kind == CAVE_EMPIRE, abandoned = kind == CAVE_ABANDONED;
    Color wood = empire ? (Color){ 112, 78, 42, 255 } : (Color){ 105, 72, 40, 255 };
    Color wood_light = kind == CAVE_TRIBE ? (Color){ 156, 108, 56, 255 } : ColorBrightness(wood, 0.18f);
    Color stone = empire ? (Color){ 142, 128, 104, 255 } : (Color){ 116, 100, 78, 255 };
    // El saliente: 1,5 x 1 m, delante de la cara de la roca (z = 0,28), a 2 m del suelo de la boca. La abandonada no
    // lo tiene: su escalera se apoya en la roca, mas afuera (sus nichos estan junto a las jambas).
    const float px = 2.85f, pz = -0.22f, ph = g + 2.0f;
    float lx = abandoned ? 3.7f : 3.15f, lz = abandoned ? -1.3f : -2.3f;
    Vector3 low = { lx, mouth_ground(t, c, base, lx, lz) - 0.05f, lz };
    Vector3 high = abandoned ? (Vector3){ 3.45f, g + 1.98f, 0.18f } : (Vector3){ px, ph - 0.02f, pz - 0.5f };
    int rungs = abandoned ? 4 : empire ? 8 : 6;
    for (int side = -1; side <= 1; side += 2) { // los largueros (en la rota, uno partido)
        Vector3 a = ladder_point(low, high, 0.0f, (float)side * 0.42f);
        Vector3 b = ladder_point(low, high, abandoned && side > 0 ? 0.62f : 1.0f, (float)side * 0.42f);
        DrawCylinderEx(a, b, 0.075f, 0.065f, 5, wood);
    }
    for (int i = 0; i < rungs; i++) {
        float k = (float)(i + 1) / (float)(rungs + 1);
        if (abandoned && k > 0.62f) break; // los peldaños de arriba se perdieron
        DrawCylinderEx(ladder_point(low, high, k, -0.42f), ladder_point(low, high, k, 0.42f), 0.06f, 0.05f, 5, wood_light);
    }
    if (abandoned) { // el pedazo de larguero, caido al pie
        float fx = 2.75f, fz = -1.75f, fy = mouth_ground(t, c, base, fx, fz) + 0.05f;
        DrawCylinderEx((Vector3){ fx, fy, fz }, (Vector3){ fx + 0.72f, fy, fz - 0.32f }, 0.06f, 0.035f, 5, wood);
        return;
    }
    Vector3 p = { px, ph - 0.08f, pz };
    DrawCube(p, 1.5f, 0.16f, 1.0f, empire ? stone : wood);
    DrawCubeWires(p, 1.5f, 0.16f, 1.0f, ColorBrightness(empire ? stone : wood, -0.3f));
    for (int s = -1; s <= 1; s += 2) { // los puntales del frente
        float x = px + 0.62f * (float)s, z = pz - 0.4f, y0 = mouth_ground(t, c, base, x, z) - 0.1f;
        if (empire) DrawCube((Vector3){ x, (y0 + ph - 0.16f) * 0.5f, z }, 0.22f, ph - 0.16f - y0, 0.22f, stone);
        else DrawCylinderEx((Vector3){ x, y0, z }, (Vector3){ x, ph - 0.16f, z }, 0.07f, 0.06f, 5, wood);
    }
    if (empire) { // la baranda
        for (int i = -1; i <= 1; i++)
            DrawCylinderEx((Vector3){ px + 0.7f * (float)i, ph, pz - 0.44f }, (Vector3){ px + 0.7f * (float)i, ph + 0.55f, pz - 0.44f }, 0.035f, 0.035f, 5, wood);
        DrawCylinderEx((Vector3){ px - 0.72f, ph + 0.55f, pz - 0.44f }, (Vector3){ px + 0.72f, ph + 0.55f, pz - 0.44f }, 0.04f, 0.04f, 5, wood);
    } else if (kind == CAVE_TRIBE) { // el bastidor con un cuero, del otro lado de la boca
        float rx = -2.75f, rz = -0.95f, ry = mouth_ground(t, c, base, rx, rz);
        for (int s = -1; s <= 1; s += 2)
            DrawCylinderEx((Vector3){ rx + 0.4f * (float)s, ry - 0.1f, rz }, (Vector3){ rx + 0.4f * (float)s, ry + 1.35f, rz }, 0.07f, 0.04f, 5, wood);
        DrawCube((Vector3){ rx, ry + 1.2f, rz }, 0.95f, 0.06f, 0.05f, wood_light);
        DrawCube((Vector3){ rx, ry + 0.88f, rz }, 0.62f, 0.55f, 0.03f, (Color){ 132, 82, 42, 255 });
    }
}

// La fachada de una civilizacion desaparecida (las abandonadas): jambas y dintel de sillares alrededor de la abertura,
// hiladas encima, dos nichos ciegos y una hornacina en la roca, y la reja vieja, caida en el suelo a un costado. Todo
// al ras de la cara de la roca (z = 0,28) y fuera del paso: no tiene colision.
static void draw_ancient_facade(const Terrain *t, const Cave *c, float base, float g, float top, Color rock) {
    Color stone = ColorBrightness(rock, 0.18f), joint = ColorBrightness(rock, -0.18f), recess = ColorBrightness(rock, -0.62f);
    Color iron = { 92, 58, 40, 255 }; // oxidada
    for (int s = -1; s <= 1; s += 2) // las jambas, del suelo al dintel
        DrawCube((Vector3){ 1.78f * (float)s, (g - 0.1f + top + 0.05f) * 0.5f, 0.11f }, 0.48f, top + 0.15f - g, 0.36f, stone);
    DrawCube((Vector3){ 0.0f, top + 0.26f, 0.11f }, 4.05f, 0.42f, 0.36f, stone);
    // Las hiladas, sobre el dintel y un poco hundidas: ninguna cara en el mismo plano que las suyas (no titilan).
    for (int i = 0; i < 5; i++)
        DrawCube((Vector3){ -1.6f + (float)i * 0.8f, top + 0.61f + (float)(i & 1) * 0.12f, 0.13f }, 0.68f, 0.28f, 0.32f, joint);
    for (int s = -1; s <= 1; s += 2) { // los nichos, del otro lado de las jambas, con su umbral y su dintel
        float x = 2.45f * (float)s;
        DrawCube((Vector3){ x, g + 1.25f, 0.255f }, 0.5f, 0.9f, 0.05f, recess);
        DrawCube((Vector3){ x, g + 0.76f, 0.2f }, 0.66f, 0.1f, 0.16f, stone);
        DrawCube((Vector3){ x, g + 1.75f, 0.2f }, 0.66f, 0.12f, 0.16f, stone);
    }
    DrawCube((Vector3){ 0.0f, top + 1.25f, 0.255f }, 0.7f, 0.62f, 0.05f, recess); // la hornacina
    DrawCube((Vector3){ 0.0f, top + 1.62f, 0.2f }, 0.86f, 0.12f, 0.16f, stone);
    float gy = mouth_ground(t, c, base, -2.7f, -1.5f) + 0.04f; // la reja, en el suelo
    for (int i = -2; i <= 2; i++)
        DrawCylinderEx((Vector3){ -2.7f + (float)i * 0.3f, gy, -0.75f }, (Vector3){ -2.7f + (float)i * 0.3f, gy, -2.25f }, 0.035f, 0.035f, 4, iron);
    for (int k = 0; k < 2; k++)
        DrawCylinderEx((Vector3){ -3.35f, gy + 0.03f, -1.05f - (float)k * 0.9f }, (Vector3){ -2.05f, gy + 0.03f, -1.05f - (float)k * 0.9f }, 0.04f, 0.04f, 4, iron);
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
    Color dark = ColorBrightness(rock, -0.25f);
    bool empire = c->kind == CAVE_EMPIRE;
    float t0 = ground - base;  // el suelo de la boca (sobre el pie)
    float top = t0 + 3.0f;     // el techo de la abertura
    rlPushMatrix();
    rlTranslatef(c->x, base, c->z);
    rlRotatef(c->yaw * RAD2DEG, 0, 1, 0); // +z del dibujo: hacia adentro (cave_to_world)
    // La boca es natural en todos los biomas; el tipo de cueva se lee por lo que hay delante.
    draw_natural_mouth(rock, t0);
    if (c->kind != CAVE_ANIMAL) draw_human_entrance(t, c, base, t0);
    if (c->kind == CAVE_ABANDONED) draw_ancient_facade(t, c, base, t0, top, rock);
    // Lo de atras, dentro de la huella (wd_keep_out_of_rock: hasta z = 6,8) y sin asomar dentro del tunel: dos lomos
    // altos, dos bajos a los costados y, entre ellos, el fondo (que los lomos tapan de costado).
    DrawSphereEx((Vector3){ -1.7f, t0 + 3.85f, 4.3f }, 2.5f, 6, 5, dark);
    DrawSphereEx((Vector3){ 1.8f, t0 + 3.7f, 4.0f }, 2.8f, 6, 5, ColorBrightness(dark, 0.08f));
    DrawSphereEx((Vector3){ -2.6f, t0 + 1.5f, 5.3f }, 1.65f, 6, 5, ColorBrightness(dark, -0.06f));
    DrawSphereEx((Vector3){ 2.6f, t0 + 1.5f, 5.3f }, 1.65f, 6, 5, ColorBrightness(dark, -0.12f));
    DrawCube((Vector3){ 0.0f, (t0 + 2.4f) * 0.5f, 5.6f }, 3.2f, t0 + 2.4f, 2.4f, ColorBrightness(dark, -0.10f));
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
            DrawCylinderEx((Vector3){ 0.0f, top + 0.7f, 0.26f }, (Vector3){ 0.0f, top + 0.7f, 0.24f }, 0.35f, 0.35f, 10, (Color){ 168, 58, 36, 255 });
        } else if (empire) { // los estandartes del imperio, colgados de la roca por encima del saliente
            for (int k = -1; k <= 1; k += 2) {
                DrawCube((Vector3){ 3.0f * (float)k, t0 + 3.0f, 0.24f }, 0.9f, 1.6f, 0.05f, (Color){ 150, 32, 30, 255 });
                DrawCube((Vector3){ 3.0f * (float)k, t0 + 3.0f, 0.21f }, 0.3f, 0.3f, 0.05f, (Color){ 220, 180, 70, 255 });
            }
        } else { // los forajidos: una estacada (la de la derecha, mas alla de la escalera)
            for (int k = 0; k < 5; k++) {
                float x = (k < 2 ? -3.8f : 3.95f) + (float)(k % 3) * 0.55f, y = mouth_ground(t, c, base, x, -1.2f) - 0.1f;
                DrawCylinderEx((Vector3){ x, y, -1.2f }, (Vector3){ x, y + 1.4f, -1.25f }, 0.07f, 0.03f, 4, (Color){ 104, 78, 52, 255 });
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

void wd_keep_out_of_rock(const Terrain *t, Vector3 *pos) {
    const World *w = t->world;
    for (int i = 0; i < w->cave_count; i++) {
        const Cave *c = &w->caves[i];
        if (dist_xz(c->x, c->z, pos->x, pos->z) > 12.0f) continue;
        float lx, lz;
        cave_from_world(c, pos->x, pos->z, &lx, &lz);
        if (lz <= 0.3f) { // delante de la huella: ahi solo asoman las dos rocas bajas del umbral, que se rodean
            bool moved = false;
            for (int b = 0; b < 2; b++) {
                float dx = lx - MOUTH_BOULDERS[b][0], dz = lz - MOUTH_BOULDERS[b][1], r = MOUTH_BOULDERS[b][2] * 0.95f;
                float d = sqrtf(dx * dx + dz * dz);
                if (d < r && d > 1e-4f) lx = MOUTH_BOULDERS[b][0] + dx / d * r, lz = MOUTH_BOULDERS[b][1] + dz / d * r, moved = true;
            }
            if (moved) {
                cave_to_world(c, lx, lz, &pos->x, &pos->z);
                pos->y = terrain_height(t, pos->x, pos->z);
            }
            continue;
        }
        float ax = fabsf(lx);
        if (ax >= 4.7f || lz >= 6.8f) continue; // fuera de la huella del peñasco (draw_cave)
        if (ax < 1.3f && lz < 4.2f) continue;                  // en el tunel
        // Al borde libre mas cercano: el frente, un costado, el fondo o (junto al tunel) su pared.
        float dfront = lz - 0.3f, dside = 4.7f - ax, dback = 6.8f - lz, dtunnel = lz < 4.2f ? ax - 1.3f : 1e9f;
        float sx = lx < 0.0f ? -1.0f : 1.0f;
        if (dfront <= dside && dfront <= dback && dfront <= dtunnel) lz = 0.3f;
        else if (dtunnel <= dside && dtunnel <= dback) lx = sx * 1.3f;
        else if (dside <= dback) lx = sx * 4.7f;
        else lz = 6.8f;
        cave_to_world(c, lx, lz, &pos->x, &pos->z);
        pos->y = terrain_height(t, pos->x, pos->z);
    }
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
