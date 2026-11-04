#include "game/cave_game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/dialog.h"
#include "game/input.h"
#include "game/inventory_game.h"
#include "raymath.h"
#include "rlgl.h"
#include "sim/caves.h"
#include "sim/clock.h"
#include "sim/health.h"
#include "sim/lang.h"
#include "sim/rng.h"
#include "sim/world.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "world/body_draw.h"
#include "world/hearth.h"

// GameActions guarda una casilla por cueva del mundo (su indice es el de la guarida, src/sim/world.c) y un bit por
// objeto: si cambian CAVES_MAX o CAVE_OBJECTS_MAX, esto no compila.
typedef char cave_slots_match[(GA_CAVES == CAVES_MAX && CAVE_OBJECTS_MAX <= 64) ? 1 : -1];

#define MOUTH_HALF 1.5f   // m: medio ancho de la boca (por donde se entra)
#define GUARD_NEAR 38.0f  // m: a esta distancia de una cueva habitada salen sus guardias (una vez por dia; se la ve a 45)
#define GUARD_BLOCK 18.0f // m: con guardias en pie a esta distancia de la boca no se entra
#define REACH 1.9f        // m: lo que alcanza F
#define CAVE_SPEED 0.85f  // el suelo de la cueva, desparejo: un poco mas lento que afuera
#define TALK_PRISONER 400 // GameActions.talk_mode del dialogo con un prisionero (de 400 a 499: las cuevas)
#define LIGHTS_MAX 32
#define ROOM_SEGS 40
#define ROOM_RINGS 7
#define FLOOR_RINGS 4
#define WALL_HALF 1.3f // m: las paredes de un pasillo, a cada lado del eje (se camina hasta CAVE_CORRIDOR_HALF)
#define SEG_LEN 1.3f   // m: los tramos de un pasillo (la luz se calcula en cada esquina)
#define STATIONS_MAX 96

static int g_cave = -1;     // la cueva en la que esta el jugador (indice de World.caves), o -1
static Player g_in;         // el jugador adentro: pos en coordenadas de la cueva
static int g_obj = -1;      // lo que alcanza F (un objeto de la cueva), o -1
static char g_hint[160];    // lo que hace F (o por que no se puede)
static int g_talk = -1;     // la celda del prisionero con quien se habla
static uint32_t g_seen;     // las salas que ya viste en esta visita (el plano)
static bool g_steep_told;   // ya se dijo que la bajada no se sube (hasta pisar otra cosa)
static float g_block_told;  // s de juego del ultimo «los guardias te cierran el paso»
static Rectangle g_hint_rect; // el aviso de F, que tambien se toca

// ---------------------------------------------------------------- utiles
static const Cave *cave_at(const Terrain *t, int i) { return i >= 0 && i < t->world->cave_count ? &t->world->caves[i] : NULL; }

static bool is_done(const GameActions *ga, int cave, int obj) {
    return cave >= 0 && cave < GA_CAVES && obj >= 0 && obj < CAVE_OBJECTS_MAX && ((ga->cave_done[cave] >> obj) & 1u);
}

static void set_done(GameActions *ga, int cave, int obj) {
    if (cave >= 0 && cave < GA_CAVES && obj >= 0 && obj < CAVE_OBJECTS_MAX) ga->cave_done[cave] |= (uint64_t)1 << obj;
}

static const char *name_of(const GameActions *ga, const char *id) {
    const InvItem *it = inventory_find(ga->inv, id);
    return it ? T(it->name) : id;
}

// La primera letra en minuscula (los nombres van en medio de una frase); solo ASCII, como en world_game.c.
static void lower_first(char *s) {
    if (s[0] >= 'A' && s[0] <= 'Z') s[0] = (char)(s[0] - 'A' + 'a');
}

static float hash01(int a, int b) {
    uint32_t h = (uint32_t)a * 0x9E3779B1u ^ (uint32_t)b * 0x85EBCA77u;
    h ^= h >> 15, h *= 0x2C1B3C6Du, h ^= h >> 12;
    return (float)(h & 0xFFFFu) / 65535.0f;
}

static bool lit_kind(int k) { return k == COBJ_TORCH || k == COBJ_FIRE; }
// Las que van contra la pared: se dibujan sobre la roca (la planta las deja un poco adentro).
static bool wall_kind(int k) {
    return k == COBJ_TORCH || k == COBJ_VEIN || k == COBJ_WEB || k == COBJ_PAINTING || k == COBJ_ARMS || k == COBJ_CELL;
}

static bool obj_lit(const GameActions *ga, const Cave *c, int i) {
    const CaveObject *o = &c->objects[i];
    return lit_kind(o->kind) && (o->lit || is_done(ga, g_cave, i));
}

// ---------------------------------------------------------------- la roca
// El perfil de una sala, de abajo arriba: la pared recta hasta 2,8 m (el alto de un pasillo) y la boveda.
static const float RING_H[ROOM_RINGS] = { -0.25f, 1.4f, 2.8f, 0.62f, 0.82f, 0.95f, 1.0f }; // las tres primeras en m; el resto, del alto
static const float RING_F[ROOM_RINGS] = { 1.0f, 1.0f, 1.0f, 0.93f, 0.7f, 0.36f, 0.0f };    // del radio de la pared
static const float FLOOR_F[FLOOR_RINGS] = { 0.0f, 0.45f, 0.8f, 1.0f };

static float room_height(int room) { return (room == 0 ? 2.0f : 1.0f) * CAVE_ROOM_HEIGHT; }
static float ring_h(int j, float height) { return j < 3 ? RING_H[j] : RING_H[j] * height; }

// El radio de la pared en el angulo a: siempre mas alla de donde se camina (radius - 0,15).
static float wall_r(const CaveRoom *r, int room, float a) {
    return r->radius + 0.36f + 0.22f * sinf(3.0f * a + (float)room * 1.7f) + 0.12f * sinf(7.0f * a + (float)room * 0.9f);
}

// La altura de la boveda (sobre el suelo) a frac del radio de la pared, en una sala de ese alto.
static float dome_h(float height, float frac) {
    if (frac >= 1.0f) return ring_h(2, height);
    for (int j = 2; j < ROOM_RINGS - 1; j++) {
        if (frac > RING_F[j] || frac < RING_F[j + 1]) continue;
        float u = (RING_F[j] - frac) / (RING_F[j] - RING_F[j + 1]);
        return ring_h(j, height) + (ring_h(j + 1, height) - ring_h(j, height)) * u;
    }
    return height;
}

static float dome_y(int room, float frac) { return dome_h(room_height(room), frac); }

// El techo de verdad sobre (x, z): en una sala y en el vestibulo, la boveda tal como se dibuja (mas baja junto a la
// pared que en el centro); en un pasillo, el suyo.
static float rock_above(const Cave *c, float x, float z, int where, float floor_y) {
    static const CaveRoom VEST = { 0.0f, CAVE_VEST_Z, 0.0f, CAVE_VEST_R };
    const CaveRoom *r = where >= 0 && where < c->room_count ? &c->rooms[where] : where == CAVE_VESTIBULE ? &VEST : NULL;
    if (!r) return cave_ceiling(c, where, floor_y);
    float a = atan2f(x - r->x, z - r->z), d = sqrtf((x - r->x) * (x - r->x) + (z - r->z) * (z - r->z));
    float height = where == CAVE_VESTIBULE ? CAVE_VEST_HEIGHT : room_height(where);
    return r->floor_y + dome_h(height, d / wall_r(r, where == CAVE_VESTIBULE ? -1 : where, a));
}

// Donde se dibuja un objeto: lo de la pared, sobre la roca (inset: cuanto sobresale); yaw: mirando al centro.
static Vector3 obj_at(const Cave *c, int i, float *yaw) {
    const CaveObject *o = &c->objects[i];
    const CaveRoom *r = &c->rooms[o->room];
    float a = atan2f(o->x - r->x, o->z - r->z);
    if (yaw) *yaw = a + PI;
    if (!wall_kind(o->kind)) return (Vector3){ o->x, o->y, o->z };
    float inset = o->kind == COBJ_CELL ? 0.05f : o->kind == COBJ_ARMS ? 0.4f : o->kind == COBJ_WEB ? 0.25f : 0.08f;
    float d = wall_r(r, o->room, a) - inset;
    return (Vector3){ r->x + sinf(a) * d, o->y, r->z + cosf(a) * d };
}

static Color rock_color(const Cave *c) {
    switch (c->region) {
    case REGION_DESERT: return (Color){ 168, 124, 88, 255 };
    case REGION_HIGHLAND: return (Color){ 126, 128, 134, 255 };
    case REGION_FJORD: return (Color){ 104, 110, 116, 255 };
    case REGION_FOREST: return (Color){ 112, 104, 92, 255 };
    default: return (Color){ 132, 118, 100, 255 };
    }
}

// ---------------------------------------------------------------- la luz
// Adentro no llega el sol: cada vertice suma la luz de las llamas cercanas (y la de la boca), con su caida.
typedef struct {
    Vector3 at;
    float radius, power, r, g, b;
} CaveLight;

static CaveLight g_lights[LIGHTS_MAX];
static int g_nlights;
#define AMBIENT 0.035f

static void add_light(Vector3 at, float radius, float power, float r, float g, float b) {
    if (g_nlights < LIGHTS_MAX) g_lights[g_nlights++] = (CaveLight){ at, radius, power, r, g, b };
}

static Color shade(Color base, float x, float y, float z) {
    float r = AMBIENT, g = AMBIENT, b = AMBIENT * 1.25f;
    for (int i = 0; i < g_nlights; i++) {
        const CaveLight *l = &g_lights[i];
        float dx = x - l->at.x, dy = y - l->at.y, dz = z - l->at.z, d2 = dx * dx + dy * dy + dz * dz;
        if (d2 >= l->radius * l->radius) continue;
        float f = 1.0f - sqrtf(d2) / l->radius;
        f = f * f * l->power;
        r += f * l->r, g += f * l->g, b += f * l->b;
    }
    return (Color){ (unsigned char)fminf(255.0f, (float)base.r * fminf(r, 1.5f)), (unsigned char)fminf(255.0f, (float)base.g * fminf(g, 1.5f)),
                    (unsigned char)fminf(255.0f, (float)base.b * fminf(b, 1.5f)), base.a };
}

static Color shade_v(Color base, Vector3 p) { return shade(base, p.x, p.y, p.z); }

// La mano izquierda del jugador, donde arde la antorcha (ga_draw_held: (0,5; 0,7 + su alto; 0,3), girado con yaw).
static Vector3 hand_flame(const Player *q) {
    float s = sinf(q->yaw), co = cosf(q->yaw);
    return (Vector3){ q->pos.x + 0.5f * co + 0.3f * s, q->pos.y + 1.55f, q->pos.z - 0.5f * s + 0.3f * co };
}

// La llama de una antorcha de la pared: un poco hacia adentro y arriba del soporte.
static Vector3 torch_flame(const Cave *c, int i) {
    float yaw = 0.0f;
    Vector3 at = obj_at(c, i, &yaw);
    return (Vector3){ at.x + sinf(yaw) * 0.22f, at.y + 0.32f, at.z + cosf(yaw) * 0.22f };
}

static void gather_lights(const Cave *c, const GameActions *ga, float world_time, float time) {
    g_nlights = 0;
    // La boca: de dia entra la luz de afuera; de noche, apenas la de la luna.
    float day = clock_light(world_time);
    add_light((Vector3){ 0.0f, 1.4f, 0.4f }, 6.0f + 10.0f * day, 0.12f + 0.88f * day, 0.82f, 0.9f, 1.0f);
    // Los ojos se acostumbran: alrededor del jugador siempre se ve algo.
    add_light((Vector3){ g_in.pos.x, g_in.pos.y + 1.3f, g_in.pos.z }, 3.4f, 0.3f, 0.72f, 0.74f, 0.86f);
    if (ga->torch_lit) {
        float fl = 1.0f + 0.07f * sinf(time * 13.0f) * sinf(time * 7.3f);
        add_light(hand_flame(&g_in), 10.0f * fl, 1.3f, 1.0f, 0.72f, 0.42f);
    }
    for (int i = 0; i < c->object_count; i++) {
        if (!obj_lit(ga, c, i)) continue;
        const CaveObject *o = &c->objects[i];
        float fl = 1.0f + 0.1f * sinf(time * 11.0f + (float)i * 1.7f) * sinf(time * 6.1f + (float)i);
        if (o->kind == COBJ_TORCH) add_light(torch_flame(c, i), 8.0f * fl, 1.15f, 1.0f, 0.66f, 0.36f);
        else add_light((Vector3){ o->x, o->y + 0.6f, o->z }, 12.0f * fl, 1.35f, 1.0f, 0.62f, 0.32f);
    }
}

// La luz en un punto (blanco = de dia, afuera): para teñir los modelos.
static Color light_at(Vector3 p) { return shade(WHITE, p.x, p.y, p.z); }

// ---------------------------------------------------------------- dibujar la roca
static void tri(Vector3 a, Color ca, Vector3 b, Color cb, Vector3 c, Color cc) {
    rlColor4ub(ca.r, ca.g, ca.b, 255), rlVertex3f(a.x, a.y, a.z);
    rlColor4ub(cb.r, cb.g, cb.b, 255), rlVertex3f(b.x, b.y, b.z);
    rlColor4ub(cc.r, cc.g, cc.b, 255), rlVertex3f(c.x, c.y, c.z);
}

static void quad(Vector3 a, Color ca, Vector3 b, Color cb, Vector3 c, Color cc, Vector3 d, Color cd) {
    tri(a, ca, b, cb, c, cc);
    tri(a, ca, c, cc, d, cd);
}

static Color grain(Color base, int a, int b) {
    float k = 0.86f + 0.28f * hash01(a, b);
    return (Color){ (unsigned char)fminf(255.0f, (float)base.r * k), (unsigned char)fminf(255.0f, (float)base.g * k),
                    (unsigned char)fminf(255.0f, (float)base.b * k), 255 };
}

static Color material_tone(Color base, float factor, float blue) {
    return (Color){ (unsigned char)fminf(255.0f, (float)base.r * factor),
                    (unsigned char)fminf(255.0f, (float)base.g * factor),
                    (unsigned char)fminf(255.0f, (float)base.b * factor + blue), 255 };
}

static Color rock_surface(Color base, int room, int ring, int segment) {
    float band = 0.94f + 0.07f * sinf((float)ring * 2.4f + (float)room * 0.8f);
    float facets = 0.94f + 0.12f * hash01(room * 31 + ring, segment);
    float damp = ring < 2 ? 0.9f : 1.0f;
    return material_tone(base, band * facets * damp, ring < 2 ? 3.0f : 0.0f);
}

static Color floor_surface(Color base, int room, int ring, int segment) {
    float edge = 1.0f - 0.08f * (float)ring;
    float gravel = 0.9f + 0.16f * hash01(room * 47 + ring, segment + 19);
    return material_tone(base, edge * gravel, ring == 0 ? 2.0f : 0.0f);
}

// Una sala (o el vestibulo: room = -1, con la boca abierta hacia -z).
static void draw_room(const Cave *c, int room, Color rock, Color ground) {
    static const CaveRoom VEST = { 0.0f, CAVE_VEST_Z, 0.0f, CAVE_VEST_R };
    const CaveRoom *r = room >= 0 ? &c->rooms[room] : &VEST;
    float height = room >= 0 ? room_height(room) : CAVE_VEST_HEIGHT, step = 2.0f * PI / (float)ROOM_SEGS;
    static Vector3 wp[ROOM_RINGS][ROOM_SEGS + 1], fp[FLOOR_RINGS][ROOM_SEGS + 1];
    static Color wc[ROOM_RINGS][ROOM_SEGS + 1], fc[FLOOR_RINGS][ROOM_SEGS + 1];
    bool door[ROOM_SEGS];
    for (int k = 0; k <= ROOM_SEGS; k++) {
        float a = (float)(k % ROOM_SEGS) * step, wr = wall_r(r, room, a), s = sinf(a), co = cosf(a);
        for (int j = 0; j < ROOM_RINGS; j++) {
            float rad = wr * RING_F[j];
            wp[j][k] = (Vector3){ r->x + s * rad, r->floor_y + ring_h(j, height), r->z + co * rad };
            wc[j][k] = shade_v(rock_surface(grain(rock, room * 1000 + j, k % ROOM_SEGS), room, j, k % ROOM_SEGS), wp[j][k]);
        }
        for (int j = 0; j < FLOOR_RINGS; j++) {
            float rad = wr * FLOOR_F[j];
            fp[j][k] = (Vector3){ r->x + s * rad, r->floor_y, r->z + co * rad };
            fc[j][k] = shade_v(floor_surface(grain(ground, room * 1000 + 500 + j, k % ROOM_SEGS), room, j, k % ROOM_SEGS), fp[j][k]);
        }
    }
    // Las puertas: donde del otro lado de la pared hay un pasillo, la pared se abre hasta el alto del pasillo.
    for (int k = 0; k < ROOM_SEGS; k++) {
        float am = ((float)k + 0.5f) * step, d = r->radius + 0.6f;
        int where = 0;
        door[k] = cave_floor(c, r->x + sinf(am) * d, r->z + cosf(am) * d, NULL, &where) && where >= CAVE_IN_CORRIDOR;
        if (room < 0 && r->z + cosf(am) * r->radius < 0.6f) door[k] = true; // la boca: la luz de afuera
    }
    rlBegin(RL_TRIANGLES);
    for (int k = 0; k < ROOM_SEGS; k++) {
        for (int j = 0; j < ROOM_RINGS - 1; j++) {
            if (door[k] && j < 2) continue;
            quad(wp[j][k], wc[j][k], wp[j][k + 1], wc[j][k + 1], wp[j + 1][k + 1], wc[j + 1][k + 1], wp[j + 1][k], wc[j + 1][k]);
        }
        for (int j = 0; j < FLOOR_RINGS - 1; j++)
            quad(fp[j][k], fc[j][k], fp[j][k + 1], fc[j][k + 1], fp[j + 1][k + 1], fc[j + 1][k + 1], fp[j + 1][k], fc[j + 1][k]);
    }
    rlEnd();
}

static void draw_corridor(const Cave *c, int k, Color rock, Color ground) {
    float ax, az, ay, bx, bz, by;
    cave_corridor_ends(c, k, &ax, &az, &ay, &bx, &bz, &by);
    float dx = bx - ax, dz = bz - az, len = sqrtf(dx * dx + dz * dz);
    if (len < 0.05f) return;
    float ux = dx / len, uz = dz / len, nx = -uz, nz = ux; // a lo largo y de costado
    int n = (int)ceilf(len / SEG_LEN);
    if (n < 2) n = 2;
    if (n > STATIONS_MAX - 1) n = STATIONS_MAX - 1;
    static Vector3 P[STATIONS_MAX][2][3];
    static Color C[STATIONS_MAX][2][3];
    static Vector3 F[STATIONS_MAX][2];
    static Color FC[STATIONS_MAX][2];
    for (int i = 0; i <= n; i++) {
        float t = (float)i / (float)n, cx = ax + dx * t, cz = az + dz * t, y = ay + (by - ay) * t;
        for (int side = 0; side < 2; side++) {
            float sg = side ? -1.0f : 1.0f, jit = 0.14f * sinf((float)i * 2.3f + (float)k * 1.1f + (float)side * 0.7f);
            float w = WALL_HALF + jit;
            for (int h = 0; h < 3; h++) {
                float lift = (float)h * CAVE_CORRIDOR_HEIGHT * 0.5f, in = h == 2 ? 0.25f : 0.0f; // arriba, la boveda se cierra un poco
                P[i][side][h] = (Vector3){ cx + nx * sg * (w - in), y + lift - (h == 0 ? 0.2f : 0.0f), cz + nz * sg * (w - in) };
                C[i][side][h] = shade_v(rock_surface(grain(rock, k * 7000 + i, side * 3 + h), k, h, i), P[i][side][h]);
            }
            F[i][side] = (Vector3){ cx + nx * sg * w, y, cz + nz * sg * w };
            FC[i][side] = shade_v(floor_surface(grain(ground, k * 7000 + 900 + i, side), k, side, i), F[i][side]);
        }
    }
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < n; i++) {
        quad(F[i][0], FC[i][0], F[i + 1][0], FC[i + 1][0], F[i + 1][1], FC[i + 1][1], F[i][1], FC[i][1]); // el suelo
        quad(P[i][0][2], C[i][0][2], P[i + 1][0][2], C[i + 1][0][2], P[i + 1][1][2], C[i + 1][1][2], P[i][1][2], C[i][1][2]); // el techo
        for (int side = 0; side < 2; side++) {
            // Una pared con algo del otro lado (otro pasillo junto a la boca, una sala) no se dibuja: se abre.
            float t = ((float)i + 0.5f) / (float)n, sg = side ? -1.0f : 1.0f, d = WALL_HALF + 0.45f;
            float px = ax + dx * t + nx * sg * d, pz = az + dz * t + nz * sg * d;
            int where = 0;
            if (cave_floor(c, px, pz, NULL, &where) && where != CAVE_IN_CORRIDOR + k) continue;
            for (int h = 0; h < 2; h++)
                quad(P[i][side][h], C[i][side][h], P[i + 1][side][h], C[i + 1][side][h], P[i + 1][side][h + 1], C[i + 1][side][h + 1],
                     P[i][side][h + 1], C[i][side][h + 1]);
        }
    }
    rlEnd();
}

// La boca vista desde adentro: la luz de afuera (de dia, clara; de noche, azul oscuro).
static void draw_mouth(float world_time) {
    float day = clock_light(world_time);
    Color sky = { (unsigned char)(24.0f + 196.0f * day), (unsigned char)(30.0f + 192.0f * day), (unsigned char)(48.0f + 186.0f * day), 255 };
    Color low = { (unsigned char)(16.0f + 120.0f * day), (unsigned char)(20.0f + 130.0f * day), (unsigned char)(26.0f + 110.0f * day), 255 };
    rlBegin(RL_TRIANGLES);
    quad((Vector3){ -2.4f, -0.4f, -0.95f }, low, (Vector3){ 2.4f, -0.4f, -0.95f }, low, (Vector3){ 2.4f, 3.0f, -0.95f }, sky,
         (Vector3){ -2.4f, 3.0f, -0.95f }, sky);
    rlEnd();
}

// ---------------------------------------------------------------- dibujar lo de adentro
static void bars(Vector3 at, float yaw, float radius, float height, int n, int skip_from, int skip_to, Color col) {
    // Barrotes en semicirculo, de la pared (atras) hacia adentro: de -90 a +90 grados de yaw.
    Vector3 prev = { 0 };
    for (int i = 0; i < n; i++) {
        float a = yaw - PI * 0.5f + PI * (float)i / (float)(n - 1);
        Vector3 b = { at.x + sinf(a) * radius, at.y, at.z + cosf(a) * radius };
        if (i < skip_from || i > skip_to) DrawCylinderEx(b, (Vector3){ b.x, b.y + height, b.z }, 0.03f, 0.03f, 4, col);
        if (i > 0) DrawCylinderEx((Vector3){ prev.x, prev.y + height, prev.z }, (Vector3){ b.x, b.y + height, b.z }, 0.03f, 0.03f, 4, col);
        prev = b;
    }
}

static void draw_prisoner(Vector3 at, float yaw, PrisonerKind k, float time) {
    static const Color CLOTH[PRISONER_KINDS] = { { 128, 96, 62, 255 }, { 86, 104, 60, 255 }, { 70, 66, 64, 255 }, { 50, 44, 46, 255 } };
    BodyPose pose;
    BodyPoseParams bp = { .crouch = 1.0f, .scale = 1.0f, .held = 0.35f + 0.1f * sinf(time * 0.7f) }; // de rodillas, las manos juntas (las cadenas)
    body_pose(&pose, &bp);
    BodyColors bc = { shade_v((Color){ 196, 156, 118, 255 }, at), shade_v(CLOTH[k], at), shade_v((Color){ 70, 56, 44, 255 }, at) };
    body_draw(&pose, at, yaw, bc, NULL);
}

static void draw_object(const GameActions *ga, const Cave *c, int i, Color rock, float time) {
    const CaveObject *o = &c->objects[i];
    bool done = is_done(ga, g_cave, i);
    float yaw = 0.0f;
    Vector3 at = obj_at(c, i, &yaw);
    float s = o->size, fx = sinf(yaw), fz = cosf(yaw);
    switch (o->kind) {
    case COBJ_STALACTITE: {
        const CaveRoom *r = &c->rooms[o->room];
        float frac = Vector2Distance((Vector2){ o->x, o->z }, (Vector2){ r->x, r->z }) / wall_r(r, o->room, yaw - PI);
        float top = r->floor_y + dome_y(o->room, frac) + 0.15f, len = 0.6f + 0.9f * s;
        Vector3 a = { o->x, top, o->z }, b = { o->x, top - len, o->z };
        DrawCylinderEx(a, b, 0.22f * s, 0.02f, 6, shade_v(rock, b));
        break;
    }
    case COBJ_STALAGMITE:
        DrawCylinderEx(at, (Vector3){ at.x, at.y + 0.9f * s, at.z }, 0.3f * s, 0.04f, 6, shade_v(rock, (Vector3){ at.x, at.y + 0.4f, at.z }));
        break;
    case COBJ_WEB: {
        Vector3 ctr = { at.x, at.y, at.z };
        Color w = shade_v((Color){ 210, 210, 216, 255 }, ctr);
        float ux = fz, uz = -fx; // a lo largo de la pared
        for (int k = 0; k < 8; k++) {
            float a = (float)k * PI / 4.0f, ex = cosf(a) * 0.6f, ey = sinf(a) * 0.6f;
            DrawLine3D(ctr, (Vector3){ ctr.x + ux * ex, ctr.y + ey, ctr.z + uz * ex }, w);
        }
        for (int ring = 1; ring <= 2; ring++)
            for (int k = 0; k < 8; k++) {
                float a0 = (float)k * PI / 4.0f, a1 = a0 + PI / 4.0f, rr = 0.22f * (float)ring;
                DrawLine3D((Vector3){ ctr.x + ux * cosf(a0) * rr, ctr.y + sinf(a0) * rr, ctr.z + uz * cosf(a0) * rr },
                           (Vector3){ ctr.x + ux * cosf(a1) * rr, ctr.y + sinf(a1) * rr, ctr.z + uz * cosf(a1) * rr }, w);
            }
        break;
    }
    case COBJ_INSECTS:
        for (int k = 0; k < 7; k++) {
            float a = time * (1.6f + 0.2f * (float)k) + (float)k * 0.9f;
            Vector3 b = { at.x + cosf(a) * 0.35f, at.y + 0.15f * sinf(a * 1.7f), at.z + sinf(a) * 0.35f };
            DrawCube(b, 0.035f, 0.035f, 0.035f, shade_v((Color){ 40, 36, 30, 255 }, b));
        }
        break;
    case COBJ_VEIN: {
        if (done) break;
        int n = 0;
        const char *id = cave_object_loot(c, i, &n);
        Color ore = id && strstr(id, "cobre") ? (Color){ 70, 150, 128, 255 } : (Color){ 150, 84, 58, 255 };
        for (int k = 0; k < 5; k++) {
            float ox = (hash01(i, k) - 0.5f) * 0.7f, oy = (hash01(k, i) - 0.5f) * 0.5f, sz = 0.1f + 0.08f * hash01(i + k, 3);
            Vector3 b = { at.x + fz * ox, at.y + oy, at.z - fx * ox };
            DrawCube(b, sz, sz, sz, shade_v(ore, b));
        }
        break;
    }
    case COBJ_BLOOD: DrawCylinder(at, 0.45f * s, 0.45f * s, 0.01f, 10, shade_v((Color){ 80, 16, 14, 255 }, at)); break;
    case COBJ_PUDDLE:
        DrawCylinder(at, 0.75f, 0.75f, 0.01f, 12, shade_v((Color){ 34, 52, 70, 255 }, at));
        DrawCylinder((Vector3){ at.x + 0.15f, at.y + 0.005f, at.z + 0.1f }, 0.2f, 0.2f, 0.01f, 8, shade_v((Color){ 90, 118, 140, 255 }, at));
        break;
    case COBJ_BONES:
        if (done) break;
        for (int k = 0; k < 3; k++) {
            float a = hash01(i, k) * 2.0f * PI, l = 0.2f + 0.15f * hash01(k, i);
            Vector3 b = { at.x + cosf(a) * 0.25f, at.y + 0.03f, at.z + sinf(a) * 0.25f };
            DrawCylinderEx((Vector3){ b.x - cosf(a + 1.0f) * l, b.y, b.z - sinf(a + 1.0f) * l }, (Vector3){ b.x + cosf(a + 1.0f) * l, b.y, b.z + sinf(a + 1.0f) * l },
                           0.03f, 0.03f, 4, shade_v((Color){ 222, 214, 192, 255 }, b));
        }
        DrawSphere((Vector3){ at.x, at.y + 0.09f, at.z }, 0.1f, shade_v((Color){ 230, 222, 200, 255 }, at));
        break;
    case COBJ_NEST:
        if (done) break;
        DrawCylinder(at, 0.6f, 0.5f, 0.16f, 10, shade_v((Color){ 92, 70, 44, 255 }, at));
        DrawCylinder((Vector3){ at.x, at.y + 0.1f, at.z }, 0.38f, 0.38f, 0.07f, 10, shade_v((Color){ 60, 44, 28, 255 }, at));
        break;
    case COBJ_EGGS: {
        int n = (int)o->size;
        for (int k = 0; k < n && k < 6; k++) {
            float a = (float)k * 2.0f * PI / (float)n;
            Vector3 b = { at.x + cosf(a) * 0.14f, at.y, at.z + sinf(a) * 0.14f };
            DrawSphereEx(b, 0.075f, 6, 6, shade_v((Color){ 224, 210, 170, 255 }, b));
        }
        break;
    }
    case COBJ_PREY: {
        float a = hash01(i, 7) * 2.0f * PI, ca = cosf(a), sa = sinf(a);
        Color hide = shade_v((Color){ 108, 80, 56, 255 }, at), rib = shade_v((Color){ 214, 204, 180, 255 }, at);
        DrawCylinderEx((Vector3){ at.x - ca * 0.5f, at.y + 0.18f, at.z - sa * 0.5f }, (Vector3){ at.x + ca * 0.45f, at.y + 0.16f, at.z + sa * 0.45f }, 0.2f,
                       0.16f, 6, hide);
        for (int k = 0; k < 4; k++) {
            float u = -0.25f + 0.17f * (float)k;
            Vector3 b = { at.x + ca * u, at.y + 0.3f, at.z + sa * u };
            DrawLine3D(b, (Vector3){ b.x - sa * 0.25f, at.y + 0.05f, b.z + ca * 0.25f }, rib);
        }
        DrawSphere((Vector3){ at.x + ca * 0.62f, at.y + 0.14f, at.z + sa * 0.62f }, 0.12f, hide);
        DrawCylinder((Vector3){ at.x + 0.1f, at.y, at.z - 0.2f }, 0.35f, 0.35f, 0.01f, 8, shade_v((Color){ 70, 14, 12, 255 }, at));
        break;
    }
    case COBJ_WEAPON: {
        if (done) break;
        float a = hash01(i, 5) * 2.0f * PI, ca = cosf(a), sa = sinf(a);
        Vector3 b = { at.x, at.y + 0.03f, at.z };
        DrawCylinderEx((Vector3){ b.x - ca * 0.15f, b.y, b.z - sa * 0.15f }, (Vector3){ b.x + ca * 0.45f, b.y, b.z + sa * 0.45f }, 0.025f, 0.01f, 4,
                       shade_v((Color){ 176, 176, 182, 255 }, b));
        DrawCylinderEx((Vector3){ b.x - ca * 0.15f, b.y, b.z - sa * 0.15f }, (Vector3){ b.x - ca * 0.4f, b.y, b.z - sa * 0.4f }, 0.03f, 0.03f, 4,
                       shade_v((Color){ 96, 66, 40, 255 }, b));
        break;
    }
    case COBJ_LOOT: {
        if (done) break;
        static const Color GEM[4] = { { 64, 180, 170, 255 }, { 180, 56, 40, 255 }, { 50, 70, 160, 255 }, { 220, 170, 60, 255 } };
        DrawCube((Vector3){ at.x, at.y + 0.01f, at.z }, 0.7f, 0.02f, 0.55f, shade_v((Color){ 104, 34, 30, 255 }, at));
        for (int k = 0; k < 6; k++) {
            Vector3 b = { at.x + (hash01(i, k) - 0.5f) * 0.5f, at.y + 0.06f, at.z + (hash01(k, i) - 0.5f) * 0.4f };
            DrawCube(b, 0.08f, 0.08f, 0.08f, shade_v(GEM[k % 4], b));
        }
        break;
    }
    case COBJ_TORCH: {
        Vector3 base = { at.x, at.y - 0.35f, at.z }, top = { at.x + fx * 0.2f, at.y + 0.22f, at.z + fz * 0.2f };
        DrawCylinderEx(base, top, 0.035f, 0.045f, 5, shade_v((Color){ 92, 66, 42, 255 }, top));
        if (obj_lit(ga, c, i)) {
            float fl = 0.85f + 0.15f * sinf(time * 17.0f + (float)i);
            DrawSphere((Vector3){ top.x, top.y + 0.1f, top.z }, 0.12f * fl, (Color){ 250, 150, 50, 255 });
            DrawSphere((Vector3){ top.x, top.y + 0.13f, top.z }, 0.06f * fl, (Color){ 255, 236, 170, 255 });
        }
        break;
    }
    case COBJ_COLUMN: {
        const CaveRoom *r = &c->rooms[o->room];
        float frac = Vector2Distance((Vector2){ o->x, o->z }, (Vector2){ r->x, r->z }) / r->radius;
        float h = dome_y(o->room, frac) + 0.2f;
        for (float y = 0.0f; y < h; y += 0.5f) { // hiladas de ladrillo
            Vector3 b = { at.x, at.y + y, at.z };
            Color brick = ((int)(y * 2.0f) & 1) ? (Color){ 138, 86, 60, 255 } : (Color){ 122, 76, 54, 255 };
            DrawCylinder(b, 0.45f, 0.45f, fminf(0.5f, h - y), 8, shade_v(brick, (Vector3){ b.x, b.y + 0.25f, b.z }));
        }
        break;
    }
    case COBJ_CHEST: {
        Color wood = shade_v((Color){ 112, 74, 42, 255 }, at), iron = shade_v((Color){ 60, 58, 56, 255 }, at);
        rlPushMatrix();
        rlTranslatef(at.x, at.y, at.z);
        rlRotatef(hash01(i, 2) * 360.0f, 0, 1, 0);
        DrawCube((Vector3){ 0, 0.25f, 0 }, 0.9f, 0.5f, 0.55f, wood);
        DrawCube((Vector3){ -0.3f, 0.26f, 0 }, 0.05f, 0.52f, 0.57f, iron);
        DrawCube((Vector3){ 0.3f, 0.26f, 0 }, 0.05f, 0.52f, 0.57f, iron);
        if (done) DrawCube((Vector3){ 0, 0.68f, -0.3f }, 0.92f, 0.4f, 0.05f, wood); // abierto y vacio
        else DrawCube((Vector3){ 0, 0.54f, 0 }, 0.92f, 0.08f, 0.57f, wood);
        rlPopMatrix();
        break;
    }
    case COBJ_AMPHORA: {
        if (done) break;
        Color clay = shade_v((Color){ 178, 98, 62, 255 }, at);
        DrawCylinderEx(at, (Vector3){ at.x, at.y + 0.15f, at.z }, 0.06f, 0.2f, 8, clay);
        DrawSphereEx((Vector3){ at.x, at.y + 0.38f, at.z }, 0.26f, 8, 8, clay);
        DrawCylinderEx((Vector3){ at.x, at.y + 0.58f, at.z }, (Vector3){ at.x, at.y + 0.8f, at.z }, 0.08f, 0.1f, 8, clay);
        break;
    }
    case COBJ_ARMS: {
        Color wood = shade_v((Color){ 100, 72, 46, 255 }, at), steel = shade_v((Color){ 170, 170, 176, 255 }, at);
        float ux = fz, uz = -fx;
        Vector3 l = { at.x - ux * 0.6f, at.y, at.z - uz * 0.6f }, r = { at.x + ux * 0.6f, at.y, at.z + uz * 0.6f };
        DrawCylinderEx(l, (Vector3){ l.x, l.y + 1.3f, l.z }, 0.05f, 0.05f, 4, wood);
        DrawCylinderEx(r, (Vector3){ r.x, r.y + 1.3f, r.z }, 0.05f, 0.05f, 4, wood);
        DrawCylinderEx((Vector3){ l.x, l.y + 1.1f, l.z }, (Vector3){ r.x, r.y + 1.1f, r.z }, 0.04f, 0.04f, 4, wood);
        if (!done)
            for (int k = 0; k < 3; k++) { // lanzas apoyadas
                float u = -0.35f + 0.35f * (float)k;
                Vector3 b = { at.x + ux * u + fx * 0.25f, at.y, at.z + uz * u + fz * 0.25f }, tp = { at.x + ux * u - fx * 0.1f, at.y + 1.9f, at.z + uz * u - fz * 0.1f };
                DrawCylinderEx(b, tp, 0.025f, 0.025f, 4, wood);
                DrawCylinderEx(tp, (Vector3){ tp.x - fx * 0.03f, tp.y + 0.25f, tp.z - fz * 0.03f }, 0.04f, 0.0f, 4, steel);
            }
        break;
    }
    case COBJ_CELL: {
        bool freed = done || !o->prisoner;
        Color iron = shade_v((Color){ 74, 70, 66, 255 }, (Vector3){ at.x + fx * 0.6f, at.y + 1.0f, at.z + fz * 0.6f });
        bars(at, yaw, 0.85f, 2.2f, 9, freed && o->prisoner ? 3 : 99, freed && o->prisoner ? 5 : -1, iron); // liberado: la puerta abierta
        if (o->prisoner && !done) {
            Vector3 p = { at.x + fx * 0.4f, at.y, at.z + fz * 0.4f };
            draw_prisoner(p, yaw, (PrisonerKind)((o->prisoner - 1) % PRISONER_KINDS), time);
            DrawLine3D((Vector3){ at.x, at.y + 1.0f, at.z }, (Vector3){ p.x + fx * 0.3f, p.y + 0.75f, p.z + fz * 0.3f }, iron); // la cadena
        }
        break;
    }
    case COBJ_PAINTING: {
        float ux = fz, uz = -fx;
        Color ochre = shade_v((Color){ 190, 150, 104, 255 }, at), red = shade_v((Color){ 150, 50, 34, 255 }, at);
        Vector3 a0 = { at.x - ux * 0.7f, at.y - 0.4f, at.z - uz * 0.7f }, a1 = { at.x + ux * 0.7f, at.y - 0.4f, at.z + uz * 0.7f };
        rlBegin(RL_TRIANGLES);
        quad(a0, ochre, a1, ochre, (Vector3){ a1.x, a1.y + 0.8f, a1.z }, ochre, (Vector3){ a0.x, a0.y + 0.8f, a0.z }, ochre);
        rlEnd();
        for (int k = 0; k < 5; k++) { // figuras: jinetes, ciervos, manos
            float u = -0.5f + 0.25f * (float)k, v = 0.15f + 0.4f * hash01(i, k);
            Vector3 b = { at.x + ux * u + fx * 0.02f, at.y - 0.4f + v, at.z + uz * u + fz * 0.02f };
            DrawCube(b, 0.1f + 0.06f * hash01(k, i), 0.06f + 0.06f * hash01(k + 3, i), 0.02f, red);
        }
        break;
    }
    case COBJ_FIRE: hearth_draw_base(at, 0.9f, 300u + (uint32_t)i, obj_lit(ga, c, i)); break;
    default: break;
    }
}

// ---------------------------------------------------------------- moverse adentro
static bool object_blocks(const GameActions *ga, const Cave *c, float x, float z) {
    for (int i = 0; i < c->object_count; i++) {
        const CaveObject *o = &c->objects[i];
        float r = 0.0f;
        switch (o->kind) {
        case COBJ_COLUMN: r = 0.55f; break;
        case COBJ_STALAGMITE: r = 0.3f * o->size; break;
        case COBJ_CELL: r = 0.85f; break;
        case COBJ_CHEST: r = 0.5f; break;
        case COBJ_FIRE: r = 0.6f; break;
        case COBJ_AMPHORA: r = is_done(ga, g_cave, i) ? 0.0f : 0.3f; break;
        case COBJ_ARMS: r = 0.45f; break;
        default: break;
        }
        if (r <= 0.0f) continue;
        Vector3 at = obj_at(c, i, NULL);
        if ((x - at.x) * (x - at.x) + (z - at.z) * (z - at.z) < r * r) return true;
    }
    return false;
}

static bool step_ok(const GameActions *ga, const Cave *c, float x, float z, float nx, float nz) {
    return cave_step_ok(c, x, z, nx, nz) && !object_blocks(ga, c, nx, nz);
}

// Como player_update, pero en la cueva: el suelo de cave_floor, la roca y la bajada sin vuelta (cave_step_ok).
static void move_inside(const GameActions *ga, const Cave *c, Player *q, PlayerInput in, float cam_yaw, float dt, char *log, size_t len) {
    if (in.toggle_sneak) q->sneaking = !q->sneaking;
    if (in.toggle_crouch) q->crouching = !q->crouching;
    float mx = in.move_x, mz = in.move_z, l = sqrtf(mx * mx + mz * mz);
    if (l > 1.0f) mx /= l, mz /= l;
    q->moving = l > 0.05f;
    if (player_winded()) in.run = false;
    if (in.run && q->moving) q->sneaking = false, q->crouching = false;
    q->stance = q->sneaking ? STANCE_SNEAK : (in.run && q->moving ? STANCE_RUN : STANCE_WALK);
    float speed = q->stance == STANCE_SNEAK ? 1.8f : q->stance == STANCE_RUN ? 8.5f : 4.0f;
    if (q->crouching) speed = q->sneaking ? 1.2f : 2.2f;
    speed *= (q->speed_scale > 0.0f ? q->speed_scale : 1.0f) * CAVE_SPEED;
    if (q->moving) {
        float fx = sinf(cam_yaw), fz = cosf(cam_yaw), vx = fx * mz - fz * mx, vz = fz * mz + fx * mx;
        float nx = q->pos.x + vx * speed * dt, nz = q->pos.z + vz * speed * dt;
        if (step_ok(ga, c, q->pos.x, q->pos.z, nx, nz)) q->pos.x = nx, q->pos.z = nz;
        else if (step_ok(ga, c, q->pos.x, q->pos.z, nx, q->pos.z)) q->pos.x = nx; // se desliza junto a la roca
        else if (step_ok(ga, c, q->pos.x, q->pos.z, q->pos.x, nz)) q->pos.z = nz;
        else {
            int where = 0;
            if (cave_floor(c, q->pos.x, q->pos.z, NULL, &where) && where == CAVE_IN_CORRIDOR && !g_steep_told) {
                g_steep_told = true;
                snprintf(log, len, "%s", T("La bajada es demasiado empinada para subirla: busca otra salida por las recámaras."));
            }
        }
        float target = atan2f(vx, vz), diff = atan2f(sinf(target - q->yaw), cosf(target - q->yaw));
        q->yaw += diff * fminf(1.0f, 10.0f * dt);
    }
    player_stamina_tick(q->stance == STANCE_RUN, q->moving, dt);
    float y = q->pos.y;
    int where = 0;
    if (cave_floor(c, q->pos.x, q->pos.z, &y, &where)) {
        q->pos.y = y;
        if (where != CAVE_IN_CORRIDOR) g_steep_told = false;
        if (where >= 0 && where < CAVE_IN_CORRIDOR) g_seen |= 1u << where;
    }
    q->grounded = true, q->vy = 0.0f;
    q->noise = !q->moving ? 0.0f : q->stance == STANCE_RUN ? 1.0f : (q->stance == STANCE_SNEAK ? 0.15f : 0.5f) * (q->crouching ? 0.6f : 1.0f);
}

// ---------------------------------------------------------------- F
static bool interactive(const GameActions *ga, const Cave *c, int i) {
    const CaveObject *o = &c->objects[i];
    if (o->kind == COBJ_PAINTING) return true;
    if (is_done(ga, g_cave, i)) return false;
    int n = 0;
    if (cave_object_loot(c, i, &n)) return true;
    if (o->kind == COBJ_CELL) return o->prisoner != 0;
    return lit_kind(o->kind) && !o->lit;
}

static int nearest_object(const GameActions *ga, const Cave *c) {
    int best = -1;
    float bd = REACH * REACH;
    for (int i = 0; i < c->object_count; i++) {
        if (!interactive(ga, c, i)) continue;
        Vector3 at = obj_at(c, i, NULL);
        float dx = at.x - g_in.pos.x, dz = at.z - g_in.pos.z, d = dx * dx + dz * dz;
        if (fabsf(at.y - g_in.pos.y) > 2.5f) continue;
        if (c->objects[i].kind == COBJ_CELL) d -= 0.5f; // la celda es ancha: se habla desde la reja
        if (d < bd) bd = d, best = i;
    }
    return best;
}

static void hint_for(const GameActions *ga, const Cave *c, int i) {
    g_hint[0] = '\0';
    if (i < 0) return;
    const CaveObject *o = &c->objects[i];
    int n = 0;
    const char *id = cave_object_loot(c, i, &n);
    char what[64] = "";
    if (id) {
        snprintf(what, sizeof(what), "%s", name_of(ga, id));
        lower_first(what);
    }
    switch (o->kind) {
    case COBJ_CHEST: snprintf(g_hint, sizeof(g_hint), "%s", T("F: abrir el cofre")); break;
    case COBJ_AMPHORA: snprintf(g_hint, sizeof(g_hint), T("F: tomar el ánfora (%s)"), what); break;
    case COBJ_ARMS: snprintf(g_hint, sizeof(g_hint), T("F: tomar del depósito de armas (%s)"), what); break;
    case COBJ_VEIN: snprintf(g_hint, sizeof(g_hint), T("F: arrancar la veta (%s)"), what); break;
    case COBJ_NEST: snprintf(g_hint, sizeof(g_hint), T("F: deshacer el nido (%s)"), what); break;
    case COBJ_BONES: snprintf(g_hint, sizeof(g_hint), "%s", T("F: juntar los huesos")); break;
    case COBJ_LOOT: snprintf(g_hint, sizeof(g_hint), T("F: tomar el botín de la fiera (%s)"), what); break;
    case COBJ_WEAPON: snprintf(g_hint, sizeof(g_hint), T("F: tomar el arma abandonada (%s)"), what); break;
    case COBJ_CELL: {
        char who[32];
        snprintf(who, sizeof(who), "%s", T(cave_prisoner_name((PrisonerKind)((o->prisoner - 1) % PRISONER_KINDS))));
        snprintf(g_hint, sizeof(g_hint), T("F: hablar con el prisionero (un %s)"), who);
        break;
    }
    case COBJ_TORCH:
        snprintf(g_hint, sizeof(g_hint), "%s", ga->torch_lit ? T("F: encender la antorcha de la pared") : T("Una antorcha apagada: enciende la tuya para prenderla"));
        break;
    case COBJ_FIRE:
        snprintf(g_hint, sizeof(g_hint), "%s", ga->torch_lit ? T("F: encender la fogata") : T("Una fogata apagada: hace falta tu antorcha encendida"));
        break;
    case COBJ_PAINTING: snprintf(g_hint, sizeof(g_hint), "%s", T("F: mirar las pinturas")); break;
    default: break;
    }
}

static const char *RESCUE_NAMES[] = { "Altan", "Bayar", "Chinua", "Dash", "Enkh", "Ganbaatar", "Jargal", "Khongor",
                                      "Mönkhbat", "Nergui", "Odval", "Saran", "Temür", "Tsetseg", "Uyanga", "Zolboo" };

static const char *prisoner_name(int cave, int obj) {
    return RESCUE_NAMES[(unsigned)(cave * 7 + obj * 3) % (sizeof(RESCUE_NAMES) / sizeof(RESCUE_NAMES[0]))];
}

static void open_prisoner(GameActions *ga, const Cave *c, int obj) {
    static const char *SAY[PRISONER_KINDS] = {
        N_("Me agarraron con mis ovejas. Sácame de aquí y te seguiré a donde vayas."),
        N_("Conozco cada sendero de esta tierra. Rompe estas cadenas y cazaré para tu tribu."),
        N_("Me tienen forjando sus armas. Libérame y forjaré las tuyas."),
        N_("No soy como ellos... ya no. Suéltame: no te vas a arrepentir."),
    };
    const CaveObject *o = &c->objects[obj];
    PrisonerKind k = (PrisonerKind)((o->prisoner - 1) % PRISONER_KINDS);
    int pct = cave_betray_percent(c, obj);
    char text[400], who[64], kind[32];
    snprintf(kind, sizeof(kind), "%s", T(cave_prisoner_name(k)));
    snprintf(text, sizeof(text), "%s %s", T(SAY[k]),
             pct < 20 ? T("Se lo ve sincero.") : pct < 40 ? T("No sabes si fiarte.") : T("Tiene mirada de lobo: puede traicionarte."));
    snprintf(who, sizeof(who), T("%s, un %s encadenado"), prisoner_name(g_cave, obj), kind);
    ga->talk_mode = TALK_PRISONER;
    g_talk = obj;
    dlg_begin(&ga->dlg, who, ICON_PERSONA, text);
    ga->dlg.cursor = 0;
    dlg_option(&ga->dlg, ICON_SOLTAR, T("Romper las cadenas"), T("Se une a la tribu, o te traiciona y huye"), true, 1);
    dlg_option(&ga->dlg, ICON_SALIR, T("Dejarlo"), T("Sigue encadenado: puedes volver"), true, 0);
}

// La traicion: te golpea, te roba algo de la mochila (o de los bolsillos) y huye.
static void steal(GameActions *ga, char *what, size_t len) {
    what[0] = '\0';
    Bag *bags[2] = { ga->pack_where == PACKW_WORN ? &ga->backpack : NULL, &ga->pockets };
    int start = (int)(rng_float(&ga->rng) * 97.0f);
    for (int b = 0; b < 2; b++) {
        Bag *g = bags[b];
        if (!g || g->n <= 0) continue;
        int slot = start % g->n, n = g->s[slot].count < 3 ? g->s[slot].count : 3;
        char name[64];
        snprintf(name, sizeof(name), "%s", name_of(ga, g->s[slot].id));
        lower_first(name);
        snprintf(what, len, "%s (%d)", name, n);
        bag_remove_slot(g, slot, n);
        return;
    }
}

static void free_prisoner(GameActions *ga, Combat *cb, Troop *troop, const Cave *c, int obj, char *log, size_t len) {
    static const Role ROLES[PRISONER_KINDS] = { ROLE_HERDER, ROLE_HUNTER, ROLE_SMITH, ROLE_SOLDIER };
    static const unsigned TRAITS[PRISONER_KINDS] = { TRAIT_MERCIFUL, 0u, TRAIT_DEVOUT, TRAIT_AMBITIOUS };
    const CaveObject *o = &c->objects[obj];
    PrisonerKind k = (PrisonerKind)((o->prisoner - 1) % PRISONER_KINDS);
    const char *name = prisoner_name(g_cave, obj);
    char kind[32];
    snprintf(kind, sizeof(kind), "%s", T(cave_prisoner_name(k)));
    set_done(ga, g_cave, obj);
    if (cave_prisoner_joins(c, obj, rng_float(&ga->rng))) {
        int id = troop_recruit(troop, name, TRAITS[k]);
        Member *m = troop_find(troop, id);
        if (!m) {
            snprintf(log, len, T("%s te da las gracias, pero la tribu está llena: se va por su cuenta."), name);
            return;
        }
        troop_assign_role(troop, id, ROLES[k]);
        m->camp = -1; // va con el jugador
        m->pack = PACK_SMALL;
        bag_init_pack(&m->bag, PACK_SMALL);
        m->loyalty = fminf(100.0f, m->loyalty + 20.0f); // le debe la vida
        snprintf(log, len, T("%s, el %s, se une a la tribu: te espera en la boca de la cueva."), name, kind);
        return;
    }
    health_hit(&cb->player, &ga->rng, 10.0f, WOUND_BRUISE, PART_FOREARM_L);
    char what[96];
    steal(ga, what, sizeof(what));
    if (what[0]) snprintf(log, len, T("¡%s (%s) te traiciona: te golpea, te roba %s y huye!"), name, kind, what);
    else snprintf(log, len, T("¡%s (%s) te traiciona: te golpea y huye a la oscuridad!"), name, kind);
}

static void painting_text(const Cave *c, char *log, size_t len) {
    const char *t = c->kind == CAVE_EMPIRE   ? T("Un relieve del imperio: soldados en fila ante un rey, y una lista de lo que guardan aquí.")
                    : c->kind == CAVE_TRIBE  ? T("Pinturas de la tribu: jinetes, ciervos y el sol; cuentan de dónde vinieron.")
                    : c->kind == CAVE_BANDITS ? T("Marcas de los forajidos: rayas por cada caravana asaltada.")
                                              : T("Pinturas muy viejas: manos en ocre y animales que ya no se ven en la estepa.");
    snprintf(log, len, "%s", t);
}

static void interact(GameActions *ga, Troop *troop, const Props *props, const Player *outside, const Cave *c, int i, char *log, size_t len) {
    (void)troop;
    const CaveObject *o = &c->objects[i];
    int n = 0;
    const char *id = cave_object_loot(c, i, &n);
    if (id) {
        int got = ig_store(ga, props, outside, id, n, 1.0f);
        char what[64];
        snprintf(what, sizeof(what), "%s", name_of(ga, id));
        if (got <= 0) {
            snprintf(log, len, T("No te cabe: %s."), what);
            return;
        }
        set_done(ga, g_cave, i);
        lower_first(what);
        if (o->kind == COBJ_CHEST) snprintf(log, len, got < n ? T("Abres el cofre: %d × %s (el resto no te cabe).") : T("Abres el cofre: %d × %s."), got, what);
        else snprintf(log, len, got < n ? T("Tomas %d × %s (el resto no te cabe).") : T("Tomas %d × %s."), got, what);
        return;
    }
    switch (o->kind) {
    case COBJ_CELL: open_prisoner(ga, c, i); break;
    case COBJ_TORCH:
    case COBJ_FIRE:
        if (!ga->torch_lit) {
            snprintf(log, len, "%s", T("Hace falta tu antorcha encendida (la barra rápida)."));
            break;
        }
        set_done(ga, g_cave, i);
        snprintf(log, len, "%s", o->kind == COBJ_TORCH ? T("Enciendes la antorcha de la pared.") : T("Enciendes la fogata: la recámara se ilumina."));
        break;
    case COBJ_PAINTING: painting_text(c, log, len); break;
    default: break;
    }
}

// ---------------------------------------------------------------- entrar y salir
static void enter(GameActions *ga, Player *p, const Terrain *t, int idx, float *cam_yaw, char *log, size_t len) {
    const Cave *c = cave_at(t, idx);
    if (!c) return;
    g_cave = idx;
    g_in = *p;
    float x = 0.0f, z = 0.0f, yaw = 0.0f, y = 0.0f;
    cave_entry(c, &x, &z, &yaw);
    cave_floor(c, x, z, &y, NULL);
    (void)yaw;
    g_in.pos = (Vector3){ x, y, z };
    // Sin girar de golpe: el jugador y la camara siguen mirando adonde miraban (en la cueva, el angulo se cuenta
    // desde su eje: el del mundo menos el de la boca).
    g_in.yaw = p->yaw - c->yaw, g_in.vy = 0.0f, g_in.grounded = true, g_in.covered = g_in.peeking = false, g_in.draw_lift = 0.0f;
    *cam_yaw -= c->yaw;
    // Para el mundo, el jugador queda en la boca, del lado de afuera.
    cave_to_world(c, 0.0f, -0.8f, &p->pos.x, &p->pos.z);
    p->pos.y = terrain_height(t, p->pos.x, p->pos.z);
    p->yaw = c->yaw, p->covered = p->peeking = false, p->moving = false;
    g_seen = 0, g_obj = -1, g_talk = -1, g_steep_told = false, g_hint[0] = '\0';
    ga_set_in_cave(true);
    char kind[48];
    snprintf(kind, sizeof(kind), "%s", T(cave_kind_name(c->kind)));
    const char *how = c->kind == CAVE_ANIMAL      ? T("Huele a fiera.")
                      : c->kind == CAVE_ABANDONED ? T("Hace mucho que nadie vive aquí.")
                                                  : T("Hay antorchas al fondo: alguien vive aquí.");
    if (ga->torch_lit) snprintf(log, len, T("Cueva: %s. %s"), kind, how);
    else snprintf(log, len, T("Cueva: %s. Está oscuro: enciende la antorcha."), kind);
}

static void leave(GameActions *ga, Player *p, const Terrain *t, float *cam_yaw, char *log, size_t len) {
    const Cave *c = cave_at(t, g_cave);
    if (c) {
        cave_to_world(c, 0.0f, -1.6f, &p->pos.x, &p->pos.z);
        p->pos.y = terrain_height(t, p->pos.x, p->pos.z);
        p->yaw = g_in.yaw + c->yaw; // como al entrar: sin girar de golpe
        *cam_yaw += c->yaw;
    }
    p->sneaking = g_in.sneaking, p->crouching = g_in.crouching, p->stance = g_in.stance;
    if (ga->dlg.open && ga->talk_mode == TALK_PRISONER) dlg_close(&ga->dlg);
    g_cave = -1, g_talk = -1, g_obj = -1, g_hint[0] = '\0';
    ga_set_in_cave(false);
    snprintf(log, len, "%s", T("Sales de la cueva."));
}

// Afuera: los guardias de las cuevas habitadas cercanas y la boca, que se cruza caminando hacia adentro.
static void outside(GameActions *ga, Combat *cb, Player *p, const Terrain *t, PlayerInput in, float *cam_yaw, float world_time, char *log,
                    size_t len) {
    const World *w = t->world;
    int day = clock_day(world_time);
    for (int i = 0; i < w->cave_count && i < GA_CAVES; i++) {
        const Cave *c = &w->caves[i];
        float dx = c->x - p->pos.x, dz = c->z - p->pos.z, d2 = dx * dx + dz * dz;
        if (c->kind < CAVE_BANDITS || d2 > GUARD_NEAR * GUARD_NEAR || ga->cave_guards_day[i] == day) continue;
        ga->cave_guards_day[i] = day; // una vez por dia
        float gx = 0.0f, gz = 0.0f;
        cave_to_world(c, 0.0f, -4.0f, &gx, &gz);
        cb_spawn_party_at(cb, c->kind == CAVE_EMPIRE ? "guardia_imperial" : c->kind == CAVE_TRIBE ? "guerreros_cueva" : "guardia_cueva",
                          (Vector3){ gx, terrain_height(t, gx, gz), gz }, t, log, len);
    }
    bool moving_in = fabsf(in.move_x) + fabsf(in.move_z) > 0.1f;
    if (!moving_in || ga->climbing || ga->dlg.open || cb->player.down) return;
    for (int i = 0; i < w->cave_count; i++) {
        const Cave *c = &w->caves[i];
        float lx = 0.0f, lz = 0.0f;
        cave_from_world(c, p->pos.x, p->pos.z, &lx, &lz);
        if (lz < 0.0f || lz > 1.2f || fabsf(lx) > MOUTH_HALF) continue;
        if (cosf(p->yaw - c->yaw) < 0.3f) continue; // de espaldas a la boca no se entra
        if (ga->mounted >= 0) {
            snprintf(log, len, "%s", T("A caballo no se entra en la cueva: desmonta primero."));
            return;
        }
        Vector3 mouth = { c->x, 0.0f, c->z };
        if (cb_hostiles_near(cb, mouth, GUARD_BLOCK) > 0) {
            if (world_time - g_block_told > 6.0f) {
                g_block_told = world_time;
                snprintf(log, len, "%s", T("Hay enemigos en pie junto a la boca: no te dejan entrar."));
            }
            return;
        }
        enter(ga, p, t, i, cam_yaw, log, len);
        return;
    }
}

// ---------------------------------------------------------------- lo publico
bool cv_inside(void) { return g_cave >= 0; }
int cv_cave(void) { return g_cave; }

void cv_reset(void) {
    g_cave = -1, g_talk = -1, g_obj = -1, g_hint[0] = '\0';
    g_block_told = -100.0f;
    ga_set_in_cave(false);
    props_set_tint(WHITE);
}

void cv_update(GameActions *ga, Combat *cb, Troop *troop, const Props *props, Player *p, const Terrain *t, PlayerInput in, float *cam_yaw,
               float world_time, bool input_ok, float dt, char *log, size_t len) {
    if (g_cave < 0) {
        outside(ga, cb, p, t, in, cam_yaw, world_time, log, len);
        return;
    }
    const Cave *c = cave_at(t, g_cave);
    if (!c) {
        cv_reset();
        return;
    }
    // El dialogo con un prisionero.
    if (ga->dlg.open && ga->talk_mode == TALK_PRISONER) {
        int v = dlg_update(&ga->dlg);
        if (v == DLG_BACK || v == 0) dlg_close(&ga->dlg), g_talk = -1;
        else if (v == 1 && g_talk >= 0) {
            dlg_close(&ga->dlg);
            free_prisoner(ga, cb, troop, c, g_talk, log, len);
            g_talk = -1;
        }
        return;
    }
    // Las heridas siguen (sangran y sanan como afuera, sin curandero): abatido o desangrado, aparece en la boca y
    // lo atiende el combate de afuera (cb_update).
    health_update(&cb->player, &cb->rng, dt, !g_in.moving, ig_stat(ga, STAT_STAMINA_REGEN));
    if (cb->player.down || cb->player.dead) {
        leave(ga, p, t, cam_yaw, log, len);
        return;
    }
    g_in.speed_scale = p->speed_scale;
    move_inside(ga, c, &g_in, in, *cam_yaw, dt, log, len);
    p->sneaking = g_in.sneaking, p->crouching = g_in.crouching, p->stance = g_in.stance; // el HUD
    if (cave_at_exit(c, g_in.pos.x, g_in.pos.z)) {
        leave(ga, p, t, cam_yaw, log, len);
        return;
    }
    g_obj = nearest_object(ga, c);
    hint_for(ga, c, g_obj);
    bool tap = g_hint[0] && ui_click(g_hint_rect);
    if (g_obj >= 0 && input_ok && ga->doing < 0 && !ga->dlg.open && (input_action_pressed(KA_INTERACT) || tap))
        interact(ga, troop, props, p, c, g_obj, log, len);
}

void cv_camera(const Terrain *t, Camera3D *cam, float yaw, float pitch, float dist) {
    const Cave *c = cave_at(t, g_cave);
    if (!c) return;
    Vector3 target = { g_in.pos.x, g_in.pos.y + player_eye_height(&g_in), g_in.pos.z };
    Vector3 back = { -sinf(yaw) * cosf(pitch), sinf(pitch), -cosf(yaw) * cosf(pitch) };
    float want = fminf(dist, 5.5f), ok = 0.5f;
    // Hacia atras hasta la roca: la camara no sale de la cueva (ni por el techo ni por el suelo).
    for (float d = 0.5f; d <= want; d += 0.15f) {
        Vector3 q = Vector3Add(target, Vector3Scale(back, d));
        float fy = 0.0f;
        int where = 0;
        if (!cave_floor(c, q.x, q.z, &fy, &where) || q.z < -0.5f) break; // ni detras de la boca (su luz, en z = -0,95)
        if (q.y > rock_above(c, q.x, q.z, where, fy) - 0.4f || q.y < fy + 0.3f) break;
        ok = d;
    }
    cam->position = Vector3Add(target, Vector3Scale(back, ok));
    cam->target = target;
}

void cv_draw(GameActions *ga, const Combat *cb, Props *props, const Terrain *t, float world_time, float time) {
    const Cave *c = cave_at(t, g_cave);
    if (!c) return;
    gather_lights(c, ga, world_time, time);
    Color rock = rock_color(c);
    Color ground = c->kind >= CAVE_BANDITS ? (Color){ 112, 94, 72, 255 } : ColorBrightness(rock, -0.15f);
    hearth_frame_begin();
    // Se ve desde adentro: sin descartar las caras de atras. El lote de raylib se dibuja despues, al llenarse o al
    // cerrar el modo 3D, asi que se vacia antes de cambiar el estado (si no, el cambio llega tarde).
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    for (int r = -1; r < c->room_count; r++) draw_room(c, r, rock, ground); // el vestibulo y las salas
    for (int k = 0; k < c->corridor_count; k++) draw_corridor(c, k, rock, ground);
    draw_mouth(world_time);
    for (int i = 0; i < c->object_count; i++) draw_object(ga, c, i, rock, time);
    // El jugador, con la luz de donde esta (la llama de su antorcha, no).
    Color here = light_at((Vector3){ g_in.pos.x, g_in.pos.y + 1.0f, g_in.pos.z });
    props_set_tint(here);
    if (!ga_draw_player(ga, props, &g_in, time)) cb_draw_player_body(cb, ga, &g_in, time, here);
    props_set_tint(WHITE);
    ga_draw_held(ga, &g_in, time, here);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    hearth_draw_flames(time, 0.0f);
}

void cv_draw_plan(const Terrain *t, int cx, int cy, int r) {
    const Cave *c = cave_at(t, g_cave);
    if (!c) return;
    // La medida de la cueva: de la boca al fondo y de lado a lado.
    float minx = -2.0f, maxx = 2.0f, minz = -2.0f, maxz = 2.0f;
    for (int i = 0; i < c->room_count; i++) {
        const CaveRoom *rm = &c->rooms[i];
        minx = fminf(minx, rm->x - rm->radius), maxx = fmaxf(maxx, rm->x + rm->radius);
        minz = fminf(minz, rm->z - rm->radius), maxz = fmaxf(maxz, rm->z + rm->radius);
    }
    float span = fmaxf(maxx - minx, maxz - minz), k = (float)(2 * r - 12) / fmaxf(span, 1.0f);
    float ox = (minx + maxx) * 0.5f, oz = (minz + maxz) * 0.5f;
    DrawCircle(cx, cy, (float)r, (Color){ 14, 12, 12, 235 });
    DrawCircleLines(cx, cy, (float)r, UI_GOLD_DARK);
#define PLAN(px, pz) (Vector2){ (float)cx + ((px) - ox) * k, (float)cy - ((pz) - oz) * k }
    for (int i = 0; i < c->corridor_count; i++) {
        float ax, az, ay, bx, bz, by;
        cave_corridor_ends(c, i, &ax, &az, &ay, &bx, &bz, &by);
        bool seen = c->corridors[i].from == CAVE_MOUTH || (c->corridors[i].from >= 0 && ((g_seen >> c->corridors[i].from) & 1u)) ||
                    (c->corridors[i].to >= 0 && ((g_seen >> c->corridors[i].to) & 1u));
        DrawLineEx(PLAN(ax, az), PLAN(bx, bz), fmaxf(1.0f, CAVE_CORRIDOR_HALF * 2.0f * k),
                   seen ? (c->corridors[i].one_way ? (Color){ 150, 92, 60, 255 } : (Color){ 120, 108, 92, 255 }) : (Color){ 40, 36, 32, 255 });
    }
    for (int i = 0; i < c->room_count; i++) {
        const CaveRoom *rm = &c->rooms[i];
        bool seen = (g_seen >> i) & 1u;
        Vector2 p = PLAN(rm->x, rm->z);
        DrawCircleV(p, rm->radius * k, seen ? (Color){ 128, 114, 96, 255 } : (Color){ 40, 36, 32, 255 });
    }
    DrawCircleV(PLAN(0.0f, CAVE_VEST_Z), CAVE_VEST_R * k, (Color){ 128, 114, 96, 255 }); // el vestibulo
    Vector2 mouth = PLAN(0.0f, 0.0f), me = PLAN(g_in.pos.x, g_in.pos.z);
    DrawRectangle((int)mouth.x - 3, (int)mouth.y - 1, 6, 3, UI_GOLD_LIGHT);
    DrawCircleV(me, 2.5f, UI_TURQUOISE);
    DrawLineV(me, (Vector2){ me.x + sinf(g_in.yaw) * 6.0f, me.y - cosf(g_in.yaw) * 6.0f }, UI_TURQUOISE);
#undef PLAN
}

void cv_draw_hud(const GameActions *ga, int w, int h) {
    if (g_cave < 0 || ga->dlg.open || !g_hint[0]) {
        g_hint_rect = (Rectangle){ 0 };
        return;
    }
    int tw = MeasureText(g_hint, 10);
    g_hint_rect = (Rectangle){ (float)(w / 2 - tw / 2 - 6), (float)(h - 99), (float)(tw + 12), 16 };
    ui_text_centered(g_hint, w / 2, h - 96, 10, UI_GOLD);
}

bool cv_test_enter(GameActions *ga, Player *p, const Terrain *t, const char *spec, float *cam_yaw) {
    static const char *KINDS[CAVE_KINDS] = { "animal", "abandonada", "forajidos", "tribu", "imperio" };
    static const struct { const char *name; CaveObjectKind kind; } SPOTS[] = {
        { "celda", COBJ_CELL },   { "dialogo", COBJ_CELL },  { "nido", COBJ_NEST },     { "huevos", COBJ_EGGS },  { "presa", COBJ_PREY },
        { "botin", COBJ_LOOT },   { "arma", COBJ_WEAPON },   { "cofre", COBJ_CHEST },   { "anfora", COBJ_AMPHORA }, { "deposito", COBJ_ARMS },
        { "pintura", COBJ_PAINTING }, { "fogata", COBJ_FIRE }, { "antorcha", COBJ_TORCH }, { "veta", COBJ_VEIN }, { "columna", COBJ_COLUMN },
    };
    char kind[24];
    snprintf(kind, sizeof(kind), "%s", spec);
    char *colon = strchr(kind, ':');
    const char *where = colon ? colon + 1 : "";
    if (colon) *colon = '\0';
    int want = -1; // el objeto pedido: la cueva mas cercana que lo tenga
    for (size_t k = 0; k < sizeof(SPOTS) / sizeof(SPOTS[0]); k++)
        if (!strcmp(where, SPOTS[k].name)) want = (int)SPOTS[k].kind;
    const World *w = t->world;
    int best = -1;
    float bd = 1e30f;
    for (int i = 0; i < w->cave_count && i < GA_CAVES; i++) {
        const Cave *c = &w->caves[i];
        bool has = want < 0;
        for (int o = 0; o < c->object_count && !has; o++)
            has = c->objects[o].kind == want && (want != COBJ_CELL || c->objects[o].prisoner);
        if (strcmp(KINDS[c->kind], kind) != 0 || !has) continue;
        float d = c->x * c->x + c->z * c->z;
        if (d < bd) bd = d, best = i;
    }
    if (best < 0) return false;
    const Cave *c = &w->caves[best];
    char tmp[256];
    enter(ga, p, t, best, cam_yaw, tmp, sizeof(tmp));
    g_in.yaw = 0.0f, *cam_yaw = 0.0f; // como quien entra caminando: mirando hacia adentro
    if (!strcmp(where, "boca")) { // junto a la boca, de cara a la salida
        g_in.pos.z = 1.6f;
        g_in.yaw = PI, *cam_yaw = PI;
        return true;
    }
    if (where[0] >= '0' && where[0] <= '9') { // al borde de la sala, mirando su centro
        int room = atoi(where);
        if (room < 0 || room >= c->room_count) return true;
        const CaveRoom *rm = &c->rooms[room];
        float a = atan2f(-rm->x, -rm->z), d = rm->radius * 0.7f;
        g_in.pos = (Vector3){ rm->x + sinf(a) * d, rm->floor_y, rm->z + cosf(a) * d };
        g_in.yaw = a + PI;
        *cam_yaw = g_in.yaw;
        g_seen |= 1u << room;
        return true;
    }
    for (size_t k = 0; k < sizeof(SPOTS) / sizeof(SPOTS[0]); k++) { // ante el primer objeto de esa clase, mirandolo de costado
        if (strcmp(where, SPOTS[k].name) != 0) continue;
        for (int i = 0; i < c->object_count; i++) {
            const CaveObject *o = &c->objects[i];
            if (o->kind != SPOTS[k].kind || (o->kind == COBJ_CELL && !o->prisoner)) continue;
            const CaveRoom *rm = &c->rooms[o->room];
            Vector3 at = obj_at(c, i, NULL);
            float a = atan2f(rm->x - at.x, rm->z - at.z), d = o->kind == COBJ_CELL ? 1.5f : 1.6f; // hacia el centro de la sala
            g_in.pos = (Vector3){ at.x + sinf(a) * d, rm->floor_y, at.z + cosf(a) * d };
            g_in.yaw = a + PI;
            *cam_yaw = g_in.yaw + 0.6f;
            g_seen |= 1u << o->room;
            if (!strcmp(where, "dialogo")) open_prisoner(ga, c, i);
            return true;
        }
    }
    return true;
}

void cv_test_press(GameActions *ga, Combat *cb, Troop *troop, const Props *props, Player *p, const Terrain *t, char *log, size_t len) {
    const Cave *c = cave_at(t, g_cave);
    if (!c) return;
    if (ga->dlg.open && ga->talk_mode == TALK_PRISONER && g_talk >= 0) { // como elegir «Romper las cadenas»
        dlg_close(&ga->dlg);
        free_prisoner(ga, cb, troop, c, g_talk, log, len);
        g_talk = -1;
        return;
    }
    g_obj = nearest_object(ga, c);
    if (g_obj >= 0) interact(ga, troop, props, p, c, g_obj, log, len);
}
