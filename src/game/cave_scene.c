#include "game/cave_scene.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"
#include "sim/clock.h"
#include "sim/world.h"
#include "world/voxcave.h"

#define LIGHTS_MAX 32
#define AMBIENT 0.035f
#define CHUNKS_MAX 112        // el vestibulo, las salas y los trozos de los pasillos
#define SEG_LEN 1.1f          // m: lo que avanza cada tramo de un pasillo (la luz se calcula en cada esquina)
#define CHUNK_STATIONS 24     // tramos de pasillo por trozo
#define WALL_HALF 1.3f        // m: las paredes de un pasillo, a cada lado del eje (se camina hasta CAVE_CORRIDOR_HALF)
#define FLOOR_RINGS 8 // el suelo de una sala, del centro a la pared (fino: la luz va por vertice)
#define BUILD_VERTS 4096
#define BUILD_INDS 16384

typedef struct {
    Vector3 at;
    float radius, power, r, g, b;
} CaveLight;

static CaveLight g_lights[LIGHTS_MAX];
static int g_nlights;

typedef struct {
    Mesh mesh;   // las posiciones y los indices; en mesh.colors, los de este cuadro
    Color *base; // el color de cada vertice, sin luz
    Vector3 center;
    float radius;
} Chunk;

static Chunk g_chunks[CHUNKS_MAX];
static int g_nchunks;
static int g_loaded = -1;
static CaveDecor g_decor[CAVE_DECOR_MAX];
static int g_ndecor;
static Material g_mat;
static bool g_mat_ok;
static CsStats g_stats;

// ---------------------------------------------------------------- la luz
void cs_gather_lights(const Cave *c, uint64_t done, Vector3 player, float player_yaw, bool torch_lit, float world_time, float time) {
    g_nlights = 0;
    // La boca: de dia entra la luz de afuera; de noche, apenas la de la luna.
    float day = clock_light(world_time);
    g_lights[g_nlights++] = (CaveLight){ { 0.0f, 1.4f, 0.4f }, 6.0f + 10.0f * day, 0.12f + 0.88f * day, 0.82f, 0.9f, 1.0f };
    // Los ojos se acostumbran: alrededor del jugador siempre se ve algo.
    g_lights[g_nlights++] = (CaveLight){ { player.x, player.y + 1.3f, player.z }, 3.4f, 0.3f, 0.72f, 0.74f, 0.86f };
    if (torch_lit) { // en la mano izquierda (ga_draw_held: (0,5; 0,7 + su alto; 0,3), girado con yaw)
        float s = sinf(player_yaw), co = cosf(player_yaw), fl = 1.0f + 0.07f * sinf(time * 13.0f) * sinf(time * 7.3f);
        Vector3 hand = { player.x + 0.5f * co + 0.3f * s, player.y + 1.55f, player.z - 0.5f * s + 0.3f * co };
        g_lights[g_nlights++] = (CaveLight){ hand, 10.0f * fl, 1.3f, 1.0f, 0.72f, 0.42f };
    }
    for (int i = 0; i < c->object_count && g_nlights < LIGHTS_MAX; i++) {
        if (!cs_object_lit(c, done, i)) continue;
        const CaveObject *o = &c->objects[i];
        float fl = 1.0f + 0.1f * sinf(time * 11.0f + (float)i * 1.7f) * sinf(time * 6.1f + (float)i), x, y, z, yaw;
        cave_object_at(c, i, &x, &y, &z, &yaw);
        if (o->kind == COBJ_TORCH) // la llama: un poco hacia adentro y arriba del soporte
            g_lights[g_nlights++] = (CaveLight){ { x + sinf(yaw) * 0.22f, y + 0.32f, z + cosf(yaw) * 0.22f }, 8.0f * fl, 1.15f, 1.0f, 0.66f, 0.36f };
        else g_lights[g_nlights++] = (CaveLight){ { x, y + 0.6f, z }, 12.0f * fl, 1.35f, 1.0f, 0.62f, 0.32f };
    }
}

bool cs_object_lit(const Cave *c, uint64_t done, int obj) {
    const CaveObject *o = &c->objects[obj];
    return (o->kind == COBJ_TORCH || o->kind == COBJ_FIRE) && (o->lit || ((done >> obj) & 1u));
}

// Cada vertice suma la luz de las llamas cercanas (y la de la boca), con su caida. idx: las luces que alcanzan (NULL: todas).
static Color shade_list(Color base, float x, float y, float z, const int *idx, int n) {
    float r = AMBIENT, g = AMBIENT, b = AMBIENT * 1.25f;
    for (int k = 0; k < n; k++) {
        const CaveLight *l = &g_lights[idx ? idx[k] : k];
        float dx = x - l->at.x, dy = y - l->at.y, dz = z - l->at.z, d2 = dx * dx + dy * dy + dz * dz;
        if (d2 >= l->radius * l->radius) continue;
        float f = 1.0f - sqrtf(d2) / l->radius;
        f = f * f * l->power;
        r += f * l->r, g += f * l->g, b += f * l->b;
    }
    return (Color){ (unsigned char)fminf(255.0f, (float)base.r * fminf(r, 1.5f)), (unsigned char)fminf(255.0f, (float)base.g * fminf(g, 1.5f)),
                    (unsigned char)fminf(255.0f, (float)base.b * fminf(b, 1.5f)), base.a };
}

Color cs_shade(Color base, Vector3 p) { return shade_list(base, p.x, p.y, p.z, NULL, g_nlights); }

// ---------------------------------------------------------------- armar la roca
static Vector3 b_pos[BUILD_VERTS];
static Color b_col[BUILD_VERTS];
static unsigned short b_ind[BUILD_INDS];
static int b_nv, b_ni;
static bool b_full;

static int bv(Vector3 p, Color c) {
    if (b_nv >= BUILD_VERTS) return b_full = true, -1;
    b_pos[b_nv] = p, b_col[b_nv] = c;
    return b_nv++;
}

static void bquad(int a, int b, int c, int d) {
    if (a < 0 || b < 0 || c < 0 || d < 0 || b_ni + 6 > BUILD_INDS) return;
    unsigned short *o = &b_ind[b_ni];
    o[0] = (unsigned short)a, o[1] = (unsigned short)b, o[2] = (unsigned short)c, o[3] = (unsigned short)a, o[4] = (unsigned short)c,
    o[5] = (unsigned short)d;
    b_ni += 6;
}

// Lo armado pasa a un trozo: su malla (en la GPU, con los colores que cambian cada cuadro) y su esfera.
static bool b_finish(void) {
    if (b_full || b_nv < 3 || b_ni < 3) return !b_full;
    if (g_nchunks >= CHUNKS_MAX) return false;
    Chunk *ch = &g_chunks[g_nchunks];
    memset(ch, 0, sizeof(*ch));
    Mesh *m = &ch->mesh;
    m->vertexCount = b_nv, m->triangleCount = b_ni / 3;
    m->vertices = MemAlloc((unsigned int)b_nv * 3 * sizeof(float));
    m->colors = MemAlloc((unsigned int)b_nv * 4);
    m->indices = MemAlloc((unsigned int)b_ni * sizeof(unsigned short));
    ch->base = MemAlloc((unsigned int)b_nv * sizeof(Color));
    if (!m->vertices || !m->colors || !m->indices || !ch->base) {
        MemFree(m->vertices), MemFree(m->colors), MemFree(m->indices), MemFree(ch->base);
        return false;
    }
    Vector3 lo = b_pos[0], hi = b_pos[0];
    for (int i = 0; i < b_nv; i++) {
        m->vertices[i * 3] = b_pos[i].x, m->vertices[i * 3 + 1] = b_pos[i].y, m->vertices[i * 3 + 2] = b_pos[i].z;
        memcpy(&m->colors[i * 4], &b_col[i], 4);
        ch->base[i] = b_col[i];
        lo = Vector3Min(lo, b_pos[i]), hi = Vector3Max(hi, b_pos[i]);
    }
    memcpy(m->indices, b_ind, (size_t)b_ni * sizeof(unsigned short));
    ch->center = Vector3Scale(Vector3Add(lo, hi), 0.5f);
    ch->radius = Vector3Distance(lo, hi) * 0.5f;
    UploadMesh(m, true);
    g_nchunks++;
    g_stats.vertices += b_nv, g_stats.triangles += b_ni / 3;
    return true;
}

static void b_begin(void) { b_nv = 0, b_ni = 0, b_full = false; }

static float hash01(int a, int b) {
    uint32_t h = (uint32_t)a * 0x9E3779B1u ^ (uint32_t)b * 0x85EBCA77u;
    h ^= h >> 15, h *= 0x2C1B3C6Du, h ^= h >> 12;
    return (float)(h & 0xFFFFu) / 65535.0f;
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

// Una sala (o el vestibulo: CAVE_VESTIBULE, con la boca abierta hacia -z): la pared y la boveda del perfil de la sala
// (cave_room_profile) y el suelo. Donde del otro lado de la pared hay un pasillo, la pared se abre hasta su techo.
static bool build_room(const Cave *c, int room, Color rock, Color ground) {
    bool vest = room == CAVE_VESTIBULE;
    float cx = vest ? 0.0f : c->rooms[room].x, cz = vest ? CAVE_VEST_Z : c->rooms[room].z, fy = vest ? 0.0f : c->rooms[room].floor_y;
    float radius = vest ? CAVE_VEST_R : c->rooms[room].radius;
    int segs = (int)(2.0f * PI * radius / 1.0f);
    segs = segs < 40 ? 40 : segs > 96 ? 96 : segs;
    static const float FLOOR_F[FLOOR_RINGS] = { 0.0f, 0.14f, 0.28f, 0.42f, 0.56f, 0.7f, 0.85f, 1.0f };
    float step = 2.0f * PI / (float)segs;
    int seed = vest ? 999 : room;
    b_begin();
    int wall0 = b_nv;
    for (int k = 0; k <= segs; k++) { // la pared y la boveda: segs + 1 columnas de CAVE_PROFILE_PTS
        float a = (float)(k % segs) * step, wr = cave_wall_radius(c, room, a), s = sinf(a), co = cosf(a);
        for (int j = 0; j < CAVE_PROFILE_PTS; j++) {
            float f, h;
            cave_room_profile(c, room, j, &f, &h);
            bv((Vector3){ cx + s * wr * f, fy + h, cz + co * wr * f },
               rock_surface(grain(rock, seed * 1000 + j, k % segs), room, j, k % segs));
        }
    }
    // El suelo: anillos del centro a la pared, cada uno con los tramos que le tocan por su largo (cerca del centro, pocos),
    // cosidos entre si: triangulos parejos, sin astillas que rayen la luz (va por vertice).
    int ring0[FLOOR_RINGS], ringn[FLOOR_RINGS];
    for (int j = 0; j < FLOOR_RINGS; j++) {
        int n = j == 0 ? 1 : j == FLOOR_RINGS - 1 ? segs : (int)((float)segs * FLOOR_F[j] + 0.5f);
        if (j > 0 && n < 8) n = 8;
        ring0[j] = b_nv, ringn[j] = n;
        for (int k = 0; k <= (j == 0 ? 0 : n); k++) { // el ultimo repite el primero (la costura)
            float a = (float)(k % (n > 0 ? n : 1)) * 2.0f * PI / (float)n, wr = cave_wall_radius(c, room, a);
            bv((Vector3){ cx + sinf(a) * wr * FLOOR_F[j], fy, cz + cosf(a) * wr * FLOOR_F[j] },
               floor_surface(grain(ground, seed * 1000 + 500 + j, k % (n > 0 ? n : 1)), room, j, k % (n > 0 ? n : 1)));
        }
    }
    for (int j = 1; j < FLOOR_RINGS; j++) {
        int i = 0, k = 0, na = j == 1 ? 1 : ringn[j - 1], nb = ringn[j];
        while (i < na || k < nb) { // se avanza por el anillo que llega antes a su proximo angulo
            int a0 = ring0[j - 1] + (j == 1 ? 0 : i), b0 = ring0[j] + k;
            bool step_a = k >= nb || (j > 1 && i < na && (float)(i + 1) / (float)na < (float)(k + 1) / (float)nb);
            if (step_a) {
                if (b_ni + 3 <= BUILD_INDS) b_ind[b_ni++] = (unsigned short)a0, b_ind[b_ni++] = (unsigned short)(a0 + 1), b_ind[b_ni++] = (unsigned short)b0;
                i++;
            } else {
                if (b_ni + 3 <= BUILD_INDS) b_ind[b_ni++] = (unsigned short)a0, b_ind[b_ni++] = (unsigned short)(b0 + 1), b_ind[b_ni++] = (unsigned short)b0;
                k++;
                if (j == 1 && k >= nb) i = na;
            }
        }
    }
    for (int k = 0; k < segs; k++) {
        float am = ((float)k + 0.5f) * step;
        bool door = cave_door_at(c, room, am) || (vest && cz + cosf(am) * radius < 0.6f); // el vestibulo: la boca, la luz de afuera
        for (int j = 0; j < CAVE_PROFILE_PTS - 1; j++) {
            if (door && j == 0) continue;
            int a0 = wall0 + k * CAVE_PROFILE_PTS + j, a1 = wall0 + (k + 1) * CAVE_PROFILE_PTS + j;
            bquad(a0, a1, a1 + 1, a0 + 1);
        }
    }
    return b_finish();
}

// Si (x, z) esta adentro de la pared de alguna sala o del vestibulo (alli manda su boveda, no el techo del pasillo).
static bool under_room(const Cave *c, float x, float z) {
    for (int r = -1; r < c->room_count; r++) {
        bool vest = r < 0;
        float cx = vest ? 0.0f : c->rooms[r].x, cz = vest ? CAVE_VEST_Z : c->rooms[r].z, d = sqrtf((x - cx) * (x - cx) + (z - cz) * (z - cz));
        if (d < cave_wall_radius(c, vest ? CAVE_VESTIBULE : r, atan2f(x - cx, z - cz)) - 0.1f) return true;
    }
    return false;
}

// Un pasillo, a lo largo de su eje (cave_corridor_at), en trozos de CHUNK_STATIONS tramos: el suelo (un poco sobre el de
// las salas, que no titile donde se juntan), las dos paredes y el techo. Una pared con algo del otro lado (otro pasillo
// junto a una sala, una sala) no se dibuja: se abre; adentro de una sala, el techo es su boveda.
static bool build_corridor(const Cave *c, int k, Color rock, Color ground) {
    const CaveCorridor *cr = &c->corridors[k];
    int n = (int)ceilf(cr->length / SEG_LEN);
    if (n < 2) n = 2;
    for (int i0 = 0; i0 < n; i0 += CHUNK_STATIONS) {
        int i1 = i0 + CHUNK_STATIONS < n ? i0 + CHUNK_STATIONS : n;
        b_begin();
        for (int i = i0; i <= i1; i++) {
            float t = (float)i / (float)n, x, z, y, dx, dz;
            cave_corridor_at(c, k, t, &x, &z, &y, &dx, &dz);
            float nx = -dz, nz = dx;
            for (int side = 0; side < 2; side++) {
                float sg = side ? -1.0f : 1.0f, jit = 0.14f * sinf((float)i * 2.3f + (float)k * 1.1f + (float)side * 0.7f), w = WALL_HALF + jit;
                for (int h = 0; h < 3; h++) {
                    float lift = (float)h * CAVE_CORRIDOR_HEIGHT * 0.5f, in = h == 2 ? 0.25f : 0.0f; // arriba, la boveda se cierra un poco
                    bv((Vector3){ x + nx * sg * (w - in), y + lift - (h == 0 ? 0.2f : 0.0f), z + nz * sg * (w - in) },
                       rock_surface(grain(rock, k * 7000 + i, side * 3 + h), k, h, i));
                }
                bv((Vector3){ x + nx * sg * w, y + 0.015f, z + nz * sg * w },
                   floor_surface(grain(ground, k * 7000 + 900 + i, side), k, side, i));
            }
        }
        for (int i = i0; i < i1; i++) {
            int a = (i - i0) * 8, b = a + 8; // por estacion: lado 0 (3 de pared y el suelo), lado 1 (igual)
            float t = ((float)i + 0.5f) / (float)n, x, z, y, dx, dz;
            cave_corridor_at(c, k, t, &x, &z, &y, &dx, &dz);
            bquad(a + 3, b + 3, b + 7, a + 7);                         // el suelo
            if (!under_room(c, x, z)) bquad(a + 2, b + 2, b + 6, a + 6); // el techo
            for (int side = 0; side < 2; side++) {
                float sg = side ? -1.0f : 1.0f, d = WALL_HALF + 0.45f;
                int where = 0, o = side * 4;
                if (cave_floor(c, x - dz * sg * d, z + dx * sg * d, NULL, &where) && where != CAVE_IN_CORRIDOR + k) continue;
                for (int h = 0; h < 2; h++) bquad(a + o + h, b + o + h, b + o + h + 1, a + o + h + 1);
            }
        }
        if (!b_finish()) return false;
    }
    return true;
}

// ---------------------------------------------------------------- cargar y liberar
bool cs_load(const Cave *c, int cave) {
    cs_unload();
    double t0 = GetTime();
    if (!g_mat_ok) g_mat = LoadMaterialDefault(), g_mat_ok = true;
    memset(&g_stats, 0, sizeof(g_stats));
    Color rock = vc_rock_color((Region)c->region);
    Color ground = c->kind >= CAVE_BANDITS ? (Color){ 112, 94, 72, 255 } : ColorBrightness(rock, -0.15f);
    bool ok = build_room(c, CAVE_VESTIBULE, rock, ground);
    for (int r = 0; r < c->room_count && ok; r++) ok = build_room(c, r, rock, ground);
    for (int k = 0; k < c->corridor_count && ok; k++) ok = build_corridor(c, k, rock, ground);
    g_ndecor = ok ? cave_decor(c, g_decor, CAVE_DECOR_MAX) : 0;
    for (int i = 0; i < g_ndecor && ok; i++)
        if (g_decor[i].kind == CDEC_ROCK) vc_preload(VC_ROCK, (Region)c->region, g_decor[i].variant);
    if (!ok) {
        TraceLog(LOG_WARNING, "CUEVA: no cupo la escena de la cueva %d", cave);
        cs_unload();
        return false;
    }
    g_loaded = cave;
    g_stats.chunks = g_nchunks, g_stats.decor = g_ndecor;
    g_stats.load_ms = (float)((GetTime() - t0) * 1000.0);
    return true;
}

void cs_unload(void) {
    for (int i = 0; i < g_nchunks; i++) {
        UnloadMesh(g_chunks[i].mesh);
        MemFree(g_chunks[i].base);
    }
    memset(g_chunks, 0, sizeof(g_chunks));
    g_nchunks = 0, g_ndecor = 0, g_loaded = -1;
    vc_unload();
}

bool cs_loaded(void) { return g_loaded >= 0; }
int cs_cave(void) { return g_loaded; }

void cs_forget_gpu(void) {
    for (int i = 0; i < g_nchunks; i++) { // lo de la CPU se libera; lo de la GPU ya no existe
        MemFree(g_chunks[i].mesh.vertices), MemFree(g_chunks[i].mesh.colors), MemFree(g_chunks[i].mesh.indices), MemFree(g_chunks[i].base);
        MemFree(g_chunks[i].mesh.vboId);
    }
    memset(g_chunks, 0, sizeof(g_chunks));
    g_nchunks = 0, g_ndecor = 0, g_loaded = -1, g_mat_ok = false;
    vc_forget_gpu();
}

const CaveDecor *cs_decor(int *n) {
    if (n) *n = g_ndecor;
    return g_decor;
}

CsStats cs_stats(void) {
    CsStats s = g_stats;
    s.models = vc_loaded(&s.model_tris); // los del decorado y los de los objetos (src/game/cave_props.c)
    return s;
}

// ---------------------------------------------------------------- dibujar
// La boca vista desde adentro: la luz de afuera (de dia, clara; de noche, azul oscuro).
static void draw_mouth(float world_time) {
    float day = clock_light(world_time);
    Color sky = { (unsigned char)(24.0f + 196.0f * day), (unsigned char)(30.0f + 192.0f * day), (unsigned char)(48.0f + 186.0f * day), 255 };
    Color low = { (unsigned char)(16.0f + 120.0f * day), (unsigned char)(20.0f + 130.0f * day), (unsigned char)(26.0f + 110.0f * day), 255 };
    rlBegin(RL_TRIANGLES);
    Vector3 a = { -2.4f, -0.4f, -0.95f }, b = { 2.4f, -0.4f, -0.95f }, cc = { 2.4f, 3.0f, -0.95f }, d = { -2.4f, 3.0f, -0.95f };
    rlColor4ub(low.r, low.g, low.b, 255), rlVertex3f(a.x, a.y, a.z);
    rlColor4ub(low.r, low.g, low.b, 255), rlVertex3f(b.x, b.y, b.z);
    rlColor4ub(sky.r, sky.g, sky.b, 255), rlVertex3f(cc.x, cc.y, cc.z);
    rlColor4ub(low.r, low.g, low.b, 255), rlVertex3f(a.x, a.y, a.z);
    rlColor4ub(sky.r, sky.g, sky.b, 255), rlVertex3f(cc.x, cc.y, cc.z);
    rlColor4ub(sky.r, sky.g, sky.b, 255), rlVertex3f(d.x, d.y, d.z);
    rlEnd();
}

void cs_draw(const Cave *c, Vector3 eye, float world_time) {
    if (g_loaded < 0) return;
    g_stats.drawn_chunks = 0;
    // Se ve desde adentro: sin descartar las caras de atras. El lote de raylib se vacia antes de cambiar el estado.
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    int near[LIGHTS_MAX];
    for (int i = 0; i < g_nchunks; i++) {
        Chunk *ch = &g_chunks[i];
        if (Vector3Distance(eye, ch->center) - ch->radius > CS_VIEW) continue;
        int nl = 0; // las luces que alcanzan este trozo
        for (int l = 0; l < g_nlights; l++)
            if (Vector3Distance(g_lights[l].at, ch->center) < g_lights[l].radius + ch->radius) near[nl++] = l;
        Mesh *m = &ch->mesh;
        for (int v = 0; v < m->vertexCount; v++) {
            Color col = shade_list(ch->base[v], m->vertices[v * 3], m->vertices[v * 3 + 1], m->vertices[v * 3 + 2], near, nl);
            memcpy(&m->colors[v * 4], &col, 4);
        }
        UpdateMeshBuffer(*m, RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, m->colors, m->vertexCount * 4, 0);
        DrawMesh(*m, g_mat, MatrixIdentity());
        g_stats.drawn_chunks++;
    }
    draw_mouth(world_time);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    // El decorado: modelos de voxeles teñidos con la luz de donde estan.
    for (int i = 0; i < g_ndecor; i++) {
        const CaveDecor *d = &g_decor[i];
        Vector3 at = { d->x, d->y, d->z };
        if (Vector3Distance(eye, at) > CS_VIEW) continue;
        Color tint = cs_shade(WHITE, (Vector3){ d->x, d->y + d->size * 0.5f, d->z });
        if (d->kind == CDEC_ROCK) vc_draw(VC_ROCK, (Region)c->region, d->variant, at, d->yaw, d->size, tint);
    }
}
