#include "game/world_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"
#include "sim/ancient.h"
#include "sim/buildkit.h"
#include "sim/lang.h"
#include "world/body_draw.h"
#include "world/hearth.h"
#include "world/shapes.h"
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
            snprintf(log, len, T("Una cueva (%s): marcada como peligro. Junto a la boca, F la explora."), T(cave_kind_name(kind)));
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

// Las piezas de lo de cada boca, con sombra por cara como los voxeles de la roca (src/world/shapes.c).
static void wbox(Vector3 c, float w, float h, float d, float yaw, Color col) { shape_box(c, w, h, d, yaw, col, SHAPE_VOXEL); }
static void wcyl(Vector3 a, Vector3 b, float ra, float rb, int sides, Color col, int caps) {
    shape_solid(a, b, ra, rb, sides, col, SHAPE_VOXEL, 0u, 1.0f, caps);
}

// El azar de cada boca (siempre el mismo para la misma cueva).
static float mouth_rand(const Cave *c, int k) {
    uint32_t h = (uint32_t)(int)(c->x * 8.0f) * 73856093u ^ (uint32_t)(int)(c->z * 8.0f) * 19349663u ^ (uint32_t)k * 83492791u;
    h ^= h >> 13, h *= 0x5BD1E995u, h ^= h >> 15;
    return (float)(h & 0xFFFFu) / 65535.0f;
}

// El suelo en un punto de la boca (x, z en el plano de la cueva), sobre el pie del peñasco.
static float mouth_ground(const Terrain *t, const Cave *c, float base, float lx, float lz) {
    float wx, wz;
    cave_to_world(c, lx, lz, &wx, &wz);
    return terrain_height(t, wx, wz) - base;
}

// Una antorcha en su palo de h m, en (x, z) del plano de la cueva, con el cuenco de hierro y la llama de los fogones
// (hearth_add_flame va en el mundo: base es la altura del plano de la cueva).
static void mouth_torch(const Terrain *t, const Cave *c, float base, float x, float z, float h, int k) {
    float g = mouth_ground(t, c, base, x, z), wx, wz;
    wcyl((Vector3){ x, g - 0.1f, z }, (Vector3){ x, g + h, z }, 0.06f, 0.05f, 5, (Color){ 96, 68, 44, 255 }, 1);
    wcyl((Vector3){ x, g + h - 0.04f, z }, (Vector3){ x, g + h + 0.16f, z }, 0.06f, 0.12f, 6, (Color){ 58, 54, 50, 255 }, 1);
    cave_to_world(c, x, z, &wx, &wz);
    hearth_add_flame((Vector3){ wx, base + g + h + 0.12f, wz }, 0.42f, 1200u + (uint32_t)k + (uint32_t)(int)(fabsf(c->x) * 3.0f));
}

// Una losa o una piedra chata, sobre el suelo de su punto (un poco hundida: en una ladera no queda en el aire).
static void ground_slab(const Terrain *t, const Cave *c, float base, float x, float z, float w, float d, float h, float yaw, Color col) {
    float y = mouth_ground(t, c, base, x, z);
    wbox((Vector3){ x, y + h * 0.5f - 0.05f, z }, w, h, d, yaw, col);
}

// Un camino de tierra pisada de la boca hacia afuera (los forajidos), siguiendo el suelo, angosto y de bordes desparejos.
static void dirt_path(const Terrain *t, const Cave *c, float base, Color col) {
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < 9; i++) {
        float z0 = -1.0f - 0.7f * (float)i, z1 = z0 - 0.7f, ph = mouth_rand(c, 40) * 6.0f;
        float w0 = 0.8f + 0.22f * sinf((float)i * 1.7f + ph), w1 = 0.8f + 0.22f * sinf((float)(i + 1) * 1.7f + ph);
        float s0 = 0.18f * sinf((float)i * 0.9f + ph), s1 = 0.18f * sinf((float)(i + 1) * 0.9f + ph); // serpentea un poco
        Vector3 a = { s0 - w0, mouth_ground(t, c, base, s0 - w0, z0) + 0.025f, z0 }, b = { s0 + w0, mouth_ground(t, c, base, s0 + w0, z0) + 0.025f, z0 };
        Vector3 d = { s1 - w1, mouth_ground(t, c, base, s1 - w1, z1) + 0.025f, z1 }, e = { s1 + w1, mouth_ground(t, c, base, s1 + w1, z1) + 0.025f, z1 };
        Color k = ColorBrightness(col, (float)(i & 1) * 0.04f);
        rlColor4ub(k.r, k.g, k.b, 255), rlVertex3f(a.x, a.y, a.z);
        rlColor4ub(k.r, k.g, k.b, 255), rlVertex3f(d.x, d.y, d.z);
        rlColor4ub(k.r, k.g, k.b, 255), rlVertex3f(e.x, e.y, e.z);
        rlColor4ub(k.r, k.g, k.b, 255), rlVertex3f(a.x, a.y, a.z);
        rlColor4ub(k.r, k.g, k.b, 255), rlVertex3f(e.x, e.y, e.z);
        rlColor4ub(k.r, k.g, k.b, 255), rlVertex3f(b.x, b.y, b.z);
    }
    rlEnd();
}

static Vector3 ladder_point(Vector3 low, Vector3 high, float t, float side) {
    Vector3 p = Vector3Lerp(low, high, t);
    Vector3 across = Vector3Normalize((Vector3){ high.z - low.z, 0.0f, low.x - high.x });
    return Vector3Add(p, Vector3Scale(across, side));
}

// La escalera de mano de los forajidos y la tribu, a un costado de la boca, hasta un saliente de tablas sobre puntales;
// en la abandonada, la escalera rota apoyada en la roca. Todo queda fuera del paso (|x| < 1,4 m) y sobre el suelo de su
// punto, tambien en una ladera. Es decorado: no tiene colision.
static void draw_ladder(const Terrain *t, const Cave *c, float base, float g) {
    bool abandoned = c->kind == CAVE_ABANDONED;
    Color wood = { 105, 72, 40, 255 }, wood_light = c->kind == CAVE_TRIBE ? (Color){ 156, 108, 56, 255 } : ColorBrightness(wood, 0.18f);
    // El saliente: 1,5 x 1,12 m, de la cara de la roca (z = VOXS_MOUTH_FACE) hacia afuera, a 2 m del suelo de la boca.
    const float px = 2.85f, pz = -0.16f, ph = g + 2.0f, pd = 1.12f;
    float lx = abandoned ? 3.5f : 3.15f, lz = abandoned ? -1.2f : -2.3f;
    Vector3 low = { lx, mouth_ground(t, c, base, lx, lz) - 0.05f, lz };
    Vector3 high = abandoned ? (Vector3){ 3.25f, g + 1.98f, VOXS_MOUTH_FACE - 0.1f } : (Vector3){ px, ph - 0.02f, pz - pd * 0.5f };
    int rungs = abandoned ? 4 : 6;
    for (int side = -1; side <= 1; side += 2) { // los largueros (en la rota, uno partido)
        Vector3 a = ladder_point(low, high, 0.0f, (float)side * 0.42f);
        Vector3 b = ladder_point(low, high, abandoned && side > 0 ? 0.62f : 1.0f, (float)side * 0.42f);
        wcyl(a, b, 0.075f, 0.065f, 5, wood, 1);
    }
    for (int i = 0; i < rungs; i++) {
        float k = (float)(i + 1) / (float)(rungs + 1);
        if (abandoned && k > 0.62f) break; // los peldaños de arriba se perdieron
        wcyl(ladder_point(low, high, k, -0.42f), ladder_point(low, high, k, 0.42f), 0.06f, 0.05f, 5, wood_light, 0);
    }
    if (abandoned) { // el pedazo de larguero, caido al pie
        float fx = 2.75f, fz = -1.75f, fy = mouth_ground(t, c, base, fx, fz) + 0.05f;
        wcyl((Vector3){ fx, fy, fz }, (Vector3){ fx + 0.72f, fy, fz - 0.32f }, 0.06f, 0.035f, 5, wood, 3);
        return;
    }
    wbox((Vector3){ px, ph - 0.08f, pz }, 1.5f, 0.16f, pd, 0.0f, wood);
    for (int s = -1; s <= 1; s += 2) { // los puntales del frente
        float x = px + 0.62f * (float)s, z = pz - pd * 0.5f + 0.1f, y0 = mouth_ground(t, c, base, x, z) - 0.1f;
        wcyl((Vector3){ x, y0, z }, (Vector3){ x, ph - 0.16f, z }, 0.07f, 0.06f, 5, wood, 0);
    }
    if (c->kind == CAVE_TRIBE) { // el bastidor con un cuero, del otro lado de la boca
        float rx = -2.75f, rz = -0.95f, ry = mouth_ground(t, c, base, rx, rz);
        for (int s = -1; s <= 1; s += 2) wcyl((Vector3){ rx + 0.4f * (float)s, ry - 0.1f, rz }, (Vector3){ rx + 0.4f * (float)s, ry + 1.35f, rz }, 0.07f, 0.04f, 5, wood, 1);
        wbox((Vector3){ rx, ry + 1.2f, rz }, 0.95f, 0.06f, 0.06f, 0.0f, wood_light);
        wbox((Vector3){ rx, ry + 0.88f, rz }, 0.62f, 0.55f, 0.03f, 0.0f, (Color){ 132, 82, 42, 255 });
    }
}

// Un arco de dovelas delante de la abertura (el portico del imperio; en la abandonada, con alguna caida): de centro
// (0, cy) y radio r, al ras de la cara.
static void voussoirs(float cy, float r, float z, Color stone, int missing) {
    enum { N = 9 };
    for (int k = 0; k < N; k++) {
        if (k == missing) continue;
        float a0 = PI * (float)k / (float)N, a1 = PI * (float)(k + 1) / (float)N, gap = 0.035f;
        Vector3 p0 = { cosf(a0 + gap) * r, cy + sinf(a0 + gap) * r, z }, p1 = { cosf(a1 - gap) * r, cy + sinf(a1 - gap) * r, z };
        wcyl(p0, p1, 0.2f, 0.2f, 4, ColorBrightness(stone, (k & 1) ? -0.05f : 0.03f), 3);
    }
}

// La fachada de una civilizacion desaparecida (las abandonadas): jambas y dintel de sillares, el arco con una dovela
// caida, hiladas encima, dos nichos ciegos, una hornacina, musgo en lo que mira arriba, la reja caida y las losas rotas
// del frente. Al ras de la cara de la roca (lisa en las abandonadas) y fuera del paso: no tiene colision.
static void draw_ancient_facade(const Terrain *t, const Cave *c, float base, float g, float top, Color rock, bool near) {
    Color stone = ColorBrightness(rock, 0.18f), joint = ColorBrightness(rock, -0.18f), recess = ColorBrightness(rock, -0.62f);
    Color moss = { 96, 132, 66, 255 }, iron = { 92, 58, 40, 255 }; // la reja, oxidada
    for (int s = -1; s <= 1; s += 2) { // las jambas, del suelo al dintel, con musgo al pie
        wbox((Vector3){ 1.78f * (float)s, (g - 0.1f + top + 0.05f) * 0.5f, 0.22f }, 0.48f, top + 0.15f - g, 0.36f, 0.0f, stone);
        wbox((Vector3){ 1.78f * (float)s, g + 0.12f, 0.15f }, 0.56f, 0.26f, 0.44f, 0.0f, moss);
    }
    wbox((Vector3){ 0.0f, top + 0.26f, 0.22f }, 4.05f, 0.42f, 0.36f, 0.0f, stone);
    wbox((Vector3){ 0.4f, top + 0.5f, 0.2f }, 2.6f, 0.06f, 0.4f, 0.0f, moss);
    voussoirs(g + 1.6f, 1.58f, 0.3f, stone, 6);
    // Las hiladas, sobre el dintel y un poco hundidas: ninguna cara en el mismo plano que las suyas (no titilan).
    for (int i = 0; i < 5; i++)
        wbox((Vector3){ -1.6f + (float)i * 0.8f, top + 0.61f + (float)(i & 1) * 0.12f, 0.24f }, 0.68f, 0.28f, 0.32f, 0.0f, joint);
    for (int s = -1; s <= 1; s += 2) { // los nichos, del otro lado de las jambas, con su umbral y su dintel
        float x = 2.45f * (float)s;
        wbox((Vector3){ x, g + 1.25f, 0.375f }, 0.5f, 0.9f, 0.05f, 0.0f, recess);
        wbox((Vector3){ x, g + 0.76f, 0.32f }, 0.66f, 0.1f, 0.16f, 0.0f, stone);
        wbox((Vector3){ x, g + 1.75f, 0.32f }, 0.66f, 0.12f, 0.16f, 0.0f, stone);
    }
    wbox((Vector3){ 0.0f, top + 1.25f, 0.375f }, 0.7f, 0.62f, 0.05f, 0.0f, recess); // la hornacina
    wbox((Vector3){ 0.0f, top + 1.62f, 0.32f }, 0.86f, 0.12f, 0.16f, 0.0f, stone);
    float gy = mouth_ground(t, c, base, -2.7f, -1.65f) + 0.04f; // la reja, en el suelo (delante de la roca baja)
    for (int i = -2; i <= 2; i++) wcyl((Vector3){ -2.7f + (float)i * 0.3f, gy, -0.9f }, (Vector3){ -2.7f + (float)i * 0.3f, gy, -2.4f }, 0.035f, 0.035f, 4, iron, 0);
    for (int k = 0; k < 2; k++)
        wcyl((Vector3){ -3.35f, gy + 0.03f, -1.2f - (float)k * 0.9f }, (Vector3){ -2.05f, gy + 0.03f, -1.2f - (float)k * 0.9f }, 0.04f, 0.04f, 4, iron, 0);
    // Los sillares caidos, con su musgo.
    static const float FALLEN[3][5] = { { -0.9f, -0.6f, 0.7f, 0.5f, 0.6f }, { 1.1f, -1.3f, 0.5f, 0.36f, 0.5f }, { 2.2f, -2.6f, 0.62f, 0.42f, 0.52f } };
    for (int k = 0; k < 3; k++) {
        const float *f = FALLEN[k];
        float y = mouth_ground(t, c, base, f[0], f[1]), yaw = mouth_rand(c, 50 + k) * 1.2f;
        wbox((Vector3){ f[0], y + f[3] * 0.5f - 0.04f, f[1] }, f[2], f[3], f[4], yaw, k & 1 ? rock : joint);
        wbox((Vector3){ f[0], y + f[3] - 0.01f, f[1] }, f[2] * 0.8f, 0.05f, f[4] * 0.8f, yaw, moss);
    }
    if (!near) return;
    for (int i = 0; i < 4; i++) // las losas del frente, rotas: alguna falta, otras se movieron
        for (int j = -1; j <= 1; j++) {
            if (mouth_rand(c, 60 + i * 3 + j) < 0.3f) continue;
            float x = (float)j * 0.95f + (mouth_rand(c, 70 + i) - 0.5f) * 0.1f, z = -0.7f - 0.95f * (float)i;
            ground_slab(t, c, base, x, z, 0.86f, 0.86f, 0.1f, (mouth_rand(c, 80 + i * 3 + j) - 0.5f) * 0.3f, mouth_rand(c, 90 + i + j) < 0.3f ? moss : joint);
        }
}

// El portico del imperio (las referencias): las jambas de sillares, el arco de dovelas, el entablamento con su cornisa y,
// encima, el balcon con sus columnas y la baranda; la plataforma y los escalones, la plaza de losas y los estandartes en
// sus mastiles a los lados. Sobre la cara lisa de la roca y fuera del paso: no tiene colision.
static void draw_imperial_portal(const Terrain *t, const Cave *c, float base, float g, float top, bool near) {
    Color stone = { 160, 156, 148, 255 }, dark = { 118, 114, 108, 255 }, flag = { 132, 128, 122, 255 };
    Color red = { 156, 34, 30, 255 }, gold = { 214, 172, 64, 255 }, wood = { 92, 66, 44, 255 };
    for (int s = -1; s <= 1; s += 2) { // las jambas, con su basa
        wbox((Vector3){ 1.85f * (float)s, (g + top + 0.4f) * 0.5f, 0.2f }, 0.56f, top + 0.4f - g, 0.5f, 0.0f, stone);
        wbox((Vector3){ 1.85f * (float)s, g + 0.15f, 0.12f }, 0.74f, 0.3f, 0.66f, 0.0f, dark);
    }
    voussoirs(g + 1.6f, 1.58f, 0.28f, stone, -1);
    wbox((Vector3){ 0.0f, top + 0.62f, 0.16f }, 4.6f, 0.5f, 0.6f, 0.0f, stone); // el entablamento
    wbox((Vector3){ 0.0f, top + 0.92f, 0.04f }, 5.0f, 0.14f, 0.86f, 0.0f, dark); // la cornisa
    // El balcon: el piso volado, cuatro columnas y la baranda de balaustres entre ellas.
    float by = top + 0.99f, fz = -0.3f;
    wbox((Vector3){ 0.0f, by + 0.08f, -0.05f }, 3.8f, 0.16f, 1.1f, 0.0f, stone);
    for (int k = 0; k < 4; k++) {
        float x = -1.65f + 1.1f * (float)k;
        wcyl((Vector3){ x, by + 0.16f, fz }, (Vector3){ x, by + 1.25f, fz }, 0.11f, 0.1f, 8, stone, 0);
        wbox((Vector3){ x, by + 1.3f, fz }, 0.3f, 0.1f, 0.3f, 0.0f, dark);
    }
    wbox((Vector3){ 0.0f, by + 1.4f, -0.05f }, 3.8f, 0.12f, 1.1f, 0.0f, dark); // el techo del balcon
    for (int k = 0; k < 15; k++) {
        float x = -1.65f + 3.3f * (float)k / 14.0f;
        wcyl((Vector3){ x, by + 0.16f, fz - 0.02f }, (Vector3){ x, by + 0.58f, fz - 0.02f }, 0.045f, 0.035f, 5, stone, 0);
    }
    wbox((Vector3){ 0.0f, by + 0.62f, fz - 0.02f }, 3.4f, 0.08f, 0.14f, 0.0f, dark);
    // La plataforma del umbral, al ras del suelo de la boca, y dos escalones que bajan (se ven si el suelo baja).
    wbox((Vector3){ 0.0f, g - 0.2f, -0.35f }, 4.4f, 0.44f, 1.5f, 0.0f, dark);
    for (int k = 1; k <= 2; k++) wbox((Vector3){ 0.0f, g - 0.2f - 0.15f * (float)k, -1.1f - 0.4f * (float)(k - 1) }, 4.4f - 0.3f * (float)k, 0.44f, 0.4f, 0.0f, dark);
    for (int s = -1; s <= 1; s += 2) { // los estandartes, en sus mastiles con travesaño
        float x = 3.3f * (float)s, z = -1.5f, y = mouth_ground(t, c, base, x, z);
        wcyl((Vector3){ x, y - 0.1f, z }, (Vector3){ x, y + 4.9f, z }, 0.07f, 0.06f, 6, wood, 1);
        wcyl((Vector3){ x - 0.55f, y + 4.5f, z }, (Vector3){ x + 0.55f, y + 4.5f, z }, 0.04f, 0.04f, 5, wood, 3);
        wbox((Vector3){ x, y + 3.35f, z - 0.03f }, 0.92f, 2.2f, 0.04f, 0.0f, red);
        wbox((Vector3){ x, y + 3.35f, z - 0.06f }, 0.92f, 0.08f, 0.02f, 0.0f, gold);
        wbox((Vector3){ x, y + 3.6f, z - 0.06f }, 0.34f, 0.42f, 0.02f, 0.0f, gold); // el emblema
        wbox((Vector3){ x, y + 4.38f, z - 0.06f }, 0.92f, 0.08f, 0.02f, 0.0f, gold);
        wcyl((Vector3){ x, y + 4.98f, z }, (Vector3){ x, y + 5.2f, z }, 0.08f, 0.0f, 6, gold, 0); // la punta
    }
    if (!near) return;
    for (int i = 0; i < 5; i++) // la plaza de losas, delante de los escalones
        for (int j = -3; j <= 3; j++) {
            float x = (float)j * 0.95f + ((i & 1) ? 0.47f : 0.0f), z = -1.9f - 0.95f * (float)i;
            if (fabsf(x) > 3.1f) continue;
            ground_slab(t, c, base, x, z, 0.88f, 0.88f, 0.1f, 0.0f, ColorBrightness(flag, (mouth_rand(c, 100 + i * 7 + j) - 0.5f) * 0.14f));
        }
}

// Lo de afuera de una guarida de fieras: craneos con cuernos, huesos y piedras sueltas.
static void draw_skull(Vector3 p, float yaw, float s) {
    Color bone = { 226, 218, 196, 255 }, horn = { 150, 128, 98, 255 }, dark = { 70, 62, 54, 255 };
    float fx = sinf(yaw), fz = cosf(yaw), sx = fz, sz = -fx;
    wbox((Vector3){ p.x, p.y + 0.11f * s, p.z }, 0.26f * s, 0.2f * s, 0.3f * s, yaw, bone); // el craneo y el hocico
    wbox((Vector3){ p.x + fx * 0.24f * s, p.y + 0.07f * s, p.z + fz * 0.24f * s }, 0.16f * s, 0.12f * s, 0.24f * s, yaw, bone);
    for (int k = -1; k <= 1; k += 2) { // las cuencas y los cuernos
        float kk = (float)k;
        wbox((Vector3){ p.x + fx * 0.15f * s + sx * 0.08f * kk * s, p.y + 0.15f * s, p.z + fz * 0.15f * s + sz * 0.08f * kk * s }, 0.07f * s, 0.06f * s, 0.03f * s, yaw, dark);
        Vector3 h0 = { p.x + sx * 0.12f * kk * s, p.y + 0.17f * s, p.z + sz * 0.12f * kk * s };
        Vector3 h1 = { h0.x + sx * 0.22f * kk * s, h0.y + 0.05f * s, h0.z + sz * 0.22f * kk * s };
        Vector3 h2 = { h1.x + sx * 0.1f * kk * s - fx * 0.06f * s, h1.y + 0.2f * s, h1.z + sz * 0.1f * kk * s - fz * 0.06f * s };
        wcyl(h0, h1, 0.05f * s, 0.035f * s, 5, horn, 0);
        wcyl(h1, h2, 0.035f * s, 0.004f * s, 5, horn, 0);
    }
}

static void draw_den_front(const Terrain *t, const Cave *c, float base, bool near) {
    Color bone = { 222, 214, 192, 255 }, rock = { 118, 114, 108, 255 };
    static const float SKULL[2][3] = { { 0.9f, -1.1f, -0.6f }, { -1.7f, -2.3f, 0.5f } };
    for (int k = 0; k < 2; k++) draw_skull((Vector3){ SKULL[k][0], mouth_ground(t, c, base, SKULL[k][0], SKULL[k][1]), SKULL[k][1] }, SKULL[k][2], 1.3f);
    if (!near) return;
    for (int k = 0; k < 6; k++) { // los huesos largos, con sus nudos
        float x = -2.2f + 4.4f * mouth_rand(c, 10 + k), z = -0.8f - 2.8f * mouth_rand(c, 20 + k), a = mouth_rand(c, 30 + k) * PI;
        float y = mouth_ground(t, c, base, x, z) + 0.04f, l = 0.22f + 0.14f * mouth_rand(c, 35 + k);
        Vector3 p0 = { x - cosf(a) * l, y, z - sinf(a) * l }, p1 = { x + cosf(a) * l, y, z + sinf(a) * l };
        wcyl(p0, p1, 0.035f, 0.035f, 5, bone, 0);
        wbox(p0, 0.09f, 0.08f, 0.09f, a, bone);
        wbox(p1, 0.09f, 0.08f, 0.09f, a, bone);
    }
    for (int k = 0; k < 7; k++) { // las piedras sueltas
        float x = (mouth_rand(c, 120 + k) - 0.5f) * 8.0f, z = -0.9f - 4.5f * mouth_rand(c, 130 + k), sz = 0.25f + 0.35f * mouth_rand(c, 140 + k);
        if (fabsf(x) < 1.0f && z > -2.5f) continue; // el paso, libre
        ground_slab(t, c, base, x, z, sz * 1.3f, sz, sz * 0.8f, mouth_rand(c, 150 + k) * PI, ColorBrightness(rock, (mouth_rand(c, 160 + k) - 0.5f) * 0.3f));
    }
}

// Lo de afuera de los forajidos: estacadas de palos aguzados a los dos lados, cajas, un tonel y el camino de tierra.
static void draw_bandit_front(const Terrain *t, const Cave *c, float base, bool near) {
    Color stake = { 110, 82, 54, 255 }, tip = { 150, 120, 84, 255 }, wood = { 118, 84, 50, 255 }, band = { 64, 52, 44, 255 };
    for (int s = -1; s <= 1; s += 2) { // las estacadas: de la roca hacia afuera, con su travesaño atado
        float x0 = s < 0 ? -2.3f : 4.0f, x1 = s < 0 ? -5.4f : 5.9f;
        Vector3 prev = { 0 };
        for (int k = 0; k < 8; k++) {
            float u = (float)k / 7.0f, x = x0 + (x1 - x0) * u, z = -0.9f + 0.7f * u * u, h = 1.55f + 0.35f * mouth_rand(c, 200 + k + s * 20);
            float y = mouth_ground(t, c, base, x, z) - 0.15f;
            Vector3 b = { x, y, z }, top = { x - (float)s * 0.04f, y + h, z - 0.1f };
            wcyl(b, top, 0.085f, 0.075f, 5, stake, 0);
            wcyl(top, (Vector3){ top.x, top.y + 0.28f, top.z - 0.03f }, 0.075f, 0.0f, 5, tip, 0);
            Vector3 rail = { x, y + 0.95f, z - 0.09f };
            if (k > 0) wcyl(prev, rail, 0.035f, 0.035f, 4, band, 0);
            prev = rail;
        }
    }
    wbox((Vector3){ -2.0f, mouth_ground(t, c, base, -2.0f, -1.7f) + 0.3f, -1.7f }, 0.62f, 0.6f, 0.62f, 0.3f, wood); // las cajas
    wbox((Vector3){ -2.05f, mouth_ground(t, c, base, -2.0f, -1.7f) + 0.82f, -1.7f }, 0.46f, 0.44f, 0.46f, 0.6f, ColorBrightness(wood, 0.08f));
    float bx = 1.95f, bz = -1.5f, by = mouth_ground(t, c, base, bx, bz) - 0.03f; // el tonel, con sus aros
    wcyl((Vector3){ bx, by, bz }, (Vector3){ bx, by + 0.42f, bz }, 0.28f, 0.33f, 10, wood, 0);
    wcyl((Vector3){ bx, by + 0.42f, bz }, (Vector3){ bx, by + 0.84f, bz }, 0.33f, 0.28f, 10, wood, 1);
    for (int k = 0; k < 2; k++) wcyl((Vector3){ bx, by + 0.18f + 0.46f * (float)k, bz }, (Vector3){ bx, by + 0.24f + 0.46f * (float)k, bz }, 0.32f, 0.32f, 10, band, 0);
    if (near) dirt_path(t, c, base, (Color){ 132, 114, 92, 255 });
}

// Lo de afuera de la tribu: el sol pintado sobre el arco, la tela colgada de una vara, vasijas y el camino de piedras.
static void draw_tribe_front(const Terrain *t, const Cave *c, float base, float g, float top, bool near) {
    Color red = { 176, 52, 34, 255 }, cloth = { 196, 92, 46, 255 }, fringe = { 150, 60, 34, 255 }, clay = { 168, 96, 60, 255 }, wood = { 120, 84, 50, 255 };
    Vector3 sun = { 0.0f, top + 1.05f, 0.36f }; // el sol: el disco y los rayos, en la roca sobre el arco
    rlBegin(RL_TRIANGLES);
    for (int k = 0; k < 12; k++) {
        float a0 = (float)k * PI / 6.0f, a1 = a0 + PI / 6.0f;
        rlColor4ub(red.r, red.g, red.b, 255), rlVertex3f(sun.x, sun.y, sun.z);
        rlColor4ub(red.r, red.g, red.b, 255), rlVertex3f(sun.x + cosf(a1) * 0.42f, sun.y + sinf(a1) * 0.42f, sun.z);
        rlColor4ub(red.r, red.g, red.b, 255), rlVertex3f(sun.x + cosf(a0) * 0.42f, sun.y + sinf(a0) * 0.42f, sun.z);
        float am = (a0 + a1) * 0.5f; // un rayo
        rlColor4ub(red.r, red.g, red.b, 255), rlVertex3f(sun.x + cosf(a0 + 0.1f) * 0.5f, sun.y + sinf(a0 + 0.1f) * 0.5f, sun.z);
        rlColor4ub(red.r, red.g, red.b, 255), rlVertex3f(sun.x + cosf(am) * 0.82f, sun.y + sinf(am) * 0.82f, sun.z);
        rlColor4ub(red.r, red.g, red.b, 255), rlVertex3f(sun.x + cosf(a1 - 0.1f) * 0.5f, sun.y + sinf(a1 - 0.1f) * 0.5f, sun.z);
    }
    rlEnd();
    // La tela: colgada de una vara de lado a lado del arco, corrida hacia un costado, con pliegues y el fleco abajo.
    wcyl((Vector3){ -1.75f, top - 0.1f, 0.2f }, (Vector3){ 1.75f, top - 0.1f, 0.2f }, 0.05f, 0.05f, 5, wood, 3);
    rlBegin(RL_TRIANGLES);
    for (int k = 0; k < 6; k++) {
        float x0 = -1.5f + 0.32f * (float)k, x1 = x0 + 0.32f, z0 = 0.18f + ((k & 1) ? 0.07f : 0.0f), z1 = 0.18f + ((k & 1) ? 0.0f : 0.07f);
        float y0 = g + 0.3f + 0.1f * (float)k, y1 = top - 0.12f; // corrida: mas corta hacia el medio, donde se recoge
        Color k0 = ColorBrightness(cloth, (k & 1) ? -0.12f : 0.0f);
        rlColor4ub(k0.r, k0.g, k0.b, 255), rlVertex3f(x0, y0, z0);
        rlColor4ub(k0.r, k0.g, k0.b, 255), rlVertex3f(x1, y0 + 0.1f, z1);
        rlColor4ub(k0.r, k0.g, k0.b, 255), rlVertex3f(x1, y1, z1);
        rlColor4ub(k0.r, k0.g, k0.b, 255), rlVertex3f(x0, y0, z0);
        rlColor4ub(k0.r, k0.g, k0.b, 255), rlVertex3f(x1, y1, z1);
        rlColor4ub(k0.r, k0.g, k0.b, 255), rlVertex3f(x0, y1, z0);
        rlColor4ub(fringe.r, fringe.g, fringe.b, 255), rlVertex3f(x0, y0 - 0.12f, z0 - 0.01f); // el fleco
        rlColor4ub(fringe.r, fringe.g, fringe.b, 255), rlVertex3f(x1, y0 - 0.02f, z1 - 0.01f);
        rlColor4ub(fringe.r, fringe.g, fringe.b, 255), rlVertex3f(x0, y0 + 0.12f, z0 - 0.01f);
    }
    rlEnd();
    static const float POTS[3][3] = { { 1.95f, -1.0f, 1.0f }, { 2.1f, -1.6f, 0.75f }, { -1.85f, -1.65f, 0.9f } };
    for (int k = 0; k < 3; k++) { // las vasijas: la panza, el cuello y una franja pintada
        float x = POTS[k][0], z = POTS[k][1], s = POTS[k][2], y = mouth_ground(t, c, base, x, z) - 0.03f;
        wcyl((Vector3){ x, y, z }, (Vector3){ x, y + 0.28f * s, z }, 0.16f * s, 0.27f * s, 8, clay, 0);
        wcyl((Vector3){ x, y + 0.28f * s, z }, (Vector3){ x, y + 0.5f * s, z }, 0.27f * s, 0.12f * s, 8, clay, 0);
        wcyl((Vector3){ x, y + 0.5f * s, z }, (Vector3){ x, y + 0.62f * s, z }, 0.1f * s, 0.12f * s, 8, clay, 1);
        wcyl((Vector3){ x, y + 0.26f * s, z }, (Vector3){ x, y + 0.32f * s, z }, 0.275f * s, 0.275f * s, 8, red, 0);
    }
    if (!near) return;
    for (int k = 0; k < 9; k++) { // el camino de piedras chatas
        float z = -0.5f - 0.62f * (float)k, x = ((k & 1) ? 0.35f : -0.3f) + (mouth_rand(c, 300 + k) - 0.5f) * 0.3f;
        ground_slab(t, c, base, x, z, 0.62f + 0.2f * mouth_rand(c, 310 + k), 0.5f, 0.08f, mouth_rand(c, 320 + k) * PI,
                    ColorBrightness((Color){ 168, 146, 118, 255 }, (mouth_rand(c, 330 + k) - 0.5f) * 0.2f));
    }
}

// El pie del peñasco: lo mas bajo del terreno alrededor de la boca, 0,4 m mas abajo (en una ladera no queda en el
// aire). ground: el suelo en la boca.
static float cave_foot(const Terrain *t, const Cave *c, float *ground) {
    float base = *ground = terrain_height(t, c->x, c->z);
    for (int k = 0; k < 6; k++) {
        float wx, wz;
        cave_to_world(c, (k % 3 - 1) * 3.0f, (float)(k / 3) * 4.0f, &wx, &wz);
        base = fminf(base, terrain_height(t, wx, wz));
    }
    return base - 0.4f;
}

float wd_cave_drop(const Terrain *t, const Cave *c) {
    float ground, base = cave_foot(t, c, &ground);
    return ground - base;
}

// near: lo chico (huesos, losas, caminos), solo de cerca.
static void draw_cave(const Terrain *t, const Cave *c, float time, bool near) {
    float ground, base = cave_foot(t, c, &ground);
    Color rock = c->region == REGION_DESERT ? (Color){ 170, 120, 84, 255 } : c->region == REGION_HIGHLAND ? (Color){ 124, 126, 132, 255 } : (Color){ 112, 108, 100, 255 };
    float t0 = ground - base;  // el suelo de la boca (sobre el pie)
    float top = t0 + 3.0f;     // el techo de la abertura
    // La roca: un monticulo de peñascos de voxeles de la piedra de la region, con su arco y su tunel
    // (src/world/voxstruct.c); la abandonada y el imperio, con la cara lisa para su fachada; la tribu, de piedra ocre. El
    // tipo de cueva se lee por lo que hay delante.
    VoxMouthStyle style = c->kind == CAVE_ABANDONED || c->kind == CAVE_EMPIRE ? VOXS_MOUTH_FACADE : c->kind == CAVE_TRIBE ? VOXS_MOUTH_OCHRE : VOXS_MOUTH_NATURAL;
    voxs_draw_cave_mouth(c->region, (int)(c - t->world->caves), style, (Vector3){ c->x, ground, c->z }, c->yaw);
    rlPushMatrix();
    rlTranslatef(c->x, base, c->z);
    rlRotatef(c->yaw * RAD2DEG, 0, 1, 0); // +z del dibujo: hacia adentro (cave_to_world)
    if (c->kind == CAVE_BANDITS || c->kind == CAVE_TRIBE || c->kind == CAVE_ABANDONED) draw_ladder(t, c, base, t0);
    if (c->kind == CAVE_ABANDONED) draw_ancient_facade(t, c, base, t0, top, rock, near);
    // El tunel: se oscurece hacia adentro (a los 4 m ya no se ve nada). Sin descartar caras (vaciando el lote antes
    // y despues: raylib dibuja al final).
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    // Las paredes, a 1,42 m y el techo a 3,02: fuera de los planos de los voxeles (multiplos de 0,2 m), que no titilan.
    float g0 = t0 + 0.02f, tc = top + 0.02f, tz = 0.45f;
    Color front = ColorBrightness(rock, -0.55f), back = { 6, 6, 7, 255 };
    gradient_quad((Vector3){ -1.42f, g0, tz }, (Vector3){ 1.42f, g0, tz }, (Vector3){ 1.42f, g0, 4.3f }, (Vector3){ -1.42f, g0, 4.3f }, front, back);
    gradient_quad((Vector3){ -1.42f, tc, tz }, (Vector3){ 1.42f, tc, tz }, (Vector3){ 1.42f, tc, 4.3f }, (Vector3){ -1.42f, tc, 4.3f }, front, back);
    gradient_quad((Vector3){ -1.42f, g0, tz }, (Vector3){ -1.42f, tc, tz }, (Vector3){ -1.42f, tc, 4.3f }, (Vector3){ -1.42f, g0, 4.3f }, front, back);
    gradient_quad((Vector3){ 1.42f, g0, tz }, (Vector3){ 1.42f, tc, tz }, (Vector3){ 1.42f, tc, 4.3f }, (Vector3){ 1.42f, g0, 4.3f }, front, back);
    DrawCube((Vector3){ 0.0f, tc * 0.5f, 4.35f }, 2.84f, tc, 0.1f, back);
    // Lo de cada tipo, como en las referencias de Perchance (docs/PLAN_TACTICO_Y_CUEVAS.md, B1). Las tablas de la tribu y
    // el sol son de una sola cara: sin descartar caras todavia.
    switch (c->kind) {
    case CAVE_ANIMAL: draw_den_front(t, c, base, near); break;
    case CAVE_BANDITS: draw_bandit_front(t, c, base, near); break;
    case CAVE_TRIBE: draw_tribe_front(t, c, base, t0, top, near); break;
    case CAVE_EMPIRE: draw_imperial_portal(t, c, base, t0, top, near); break;
    default: break;
    }
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    if (c->kind == CAVE_BANDITS || c->kind == CAVE_TRIBE) { // las antorchas a los lados de la boca
        mouth_torch(t, c, base, -1.7f, -0.45f, 1.75f, 0);
        mouth_torch(t, c, base, 1.7f, -0.45f, 1.75f, 1);
    } else if (c->kind == CAVE_EMPIRE) { // en mastiles mas altos, junto a la plataforma
        mouth_torch(t, c, base, -2.45f, -1.25f, 2.3f, 0);
        mouth_torch(t, c, base, 2.45f, -1.25f, 2.3f, 1);
    }
    (void)time;
    rlPopMatrix();
}

// Adentro de la roca de una boca (voxs_draw_cave_mouth): toda la huella, hasta lo alto de la roca (7,4 m), menos el arco
// (3,2 m de ancho hasta los 2 m, que se cierra hasta los 3,2) y el tunel oscuro detras.
static bool in_outcrop(const Cave *c, float ground, Vector3 q) {
    float lx, lz, h = q.y - ground;
    cave_from_world(c, q.x, q.z, &lx, &lz);
    if (fabsf(lx) > 5.0f || lz < 0.2f || lz > 7.0f || h > 7.4f) return false;
    if (lz < 1.2f) { // el labio del arco
        float half = h < 2.0f ? 1.5f : h < 2.4f ? 1.1f : h < 2.8f ? 0.7f : h < 3.2f ? 0.3f : 0.0f;
        return !(fabsf(lx) < half);
    }
    return !(fabsf(lx) < 1.3f && lz < 4.2f && h < 2.9f);
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
                const float *k = VOXS_MOUTH_BOULDERS[b];
                float dx = lx - k[0], dz = lz - k[1], r = k[2] * 0.95f;
                float d = sqrtf(dx * dx + dz * dz);
                if (d < r && d > 1e-4f) lx = k[0] + dx / d * r, lz = k[1] + dz / d * r, moved = true;
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
        // Del jugador hacia la camara: si la roca se cruza (la camara detras del risco, o adentro), la camara queda antes;
        // por el arco y el tunel se ve.
        Vector3 from = target, to = *cam;
        for (int k = 1; k <= 32; k++)
            if (in_outcrop(c, ground, Vector3Lerp(from, to, (float)k / 32.0f))) {
                *cam = Vector3Lerp(from, to, (float)(k - 1) / 32.0f);
                break;
            }
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

// Una estructura: el modelo de voxeles de su clase, al estilo de su region (src/world/voxstruct.c). En los
// vestigios de un asentamiento antiguo, ademas, los tramos de pie de su planta (src/sim/ancient.h): el muro en
// ruinas del inventario, a la altura del medio del sitio y girado como el.
static struct { // los tramos de pie de cada sitio, sacados de su planta la primera vez que se dibuja
    bool done;
    uint32_t seed;
    int n;
    AncStanding st[ANC_STANDING_MAX];
} g_standing[SITES_MAX];

static void draw_site(Props *props, const Terrain *t, const WorldSite *st, int idx) {
    Vector3 at = { st->x, terrain_height(t, st->x, st->z), st->z };
    voxs_draw_site(st->kind, st->region, at, st->yaw, WHITE);
    if (st->kind != SITE_ANCIENT_TOWN || !props || idx < 0 || idx >= SITES_MAX) return;
    const InvItem *wall = inventory_find(props->inv, "estructura.ruina.muro");
    uint32_t seed = bk_variant_seed(BK_RUIN_TOWN, st->region, voxs_site_variant(at));
    if (!wall) return;
    if (!g_standing[idx].done || g_standing[idx].seed != seed) { // (otro mundo: la misma ranura, otra planta)
        AncientPlan plan;
        ancient_plan(&plan, seed);
        g_standing[idx].done = true, g_standing[idx].seed = seed, g_standing[idx].n = plan.standing_count;
        memcpy(g_standing[idx].st, plan.standing, sizeof(plan.standing));
    }
    float c = cosf(st->yaw), s = sinf(st->yaw);
    for (int i = 0; i < g_standing[idx].n; i++) {
        const AncStanding *sp = &g_standing[idx].st[i];
        Vector3 pos = { at.x + sp->x * c + sp->z * s, at.y, at.z - sp->x * s + sp->z * c }; // como gira DrawModelEx
        props_draw_item(props, wall, pos, st->yaw + sp->yaw, 1.0f);
    }
}

void wd_draw_world(const GameActions *ga, Props *props, const Terrain *t, const Player *p, float time) {
    g_fires_out = ga->fires_out; // la lluvia apaga tambien los fuegos de los demas
    const World *w = t->world;
    for (int i = 0; i < w->cave_count; i++) {
        float d = dist_xz(w->caves[i].x, w->caves[i].z, p->pos.x, p->pos.z);
        if (d < DEN_DRAW) draw_cave(t, &w->caves[i], time, d < 70.0f);
    }
    for (int i = 0; i < w->tribe_count; i++)
        if (dist_xz(w->tribes[i].x, w->tribes[i].z, p->pos.x, p->pos.z) < TRIBE_DRAW) draw_tribe(t, &w->tribes[i], i, time);
    for (int i = 0; i < w->site_count; i++)
        if (dist_xz(w->sites[i].x, w->sites[i].z, p->pos.x, p->pos.z) < SITE_DRAW) draw_site(props, t, &w->sites[i], i);
}
