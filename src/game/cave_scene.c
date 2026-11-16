#include "game/cave_scene.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/cave_look.h"
#include "raymath.h"
#include "rlgl.h"
#include "sim/clock.h"
#include "sim/world.h"
#include "world/hearth.h"
#include "world/voxcave.h"

#define LIGHTS_MAX 64
#define CHUNKS_MAX 320        // el vestibulo, las salas y los trozos de los pasillos, cada uno en pared, boveda y suelo
#define SEG_LEN 1.1f          // m: lo que avanza cada tramo de un pasillo (la luz se calcula en cada esquina)
#define CHUNK_STATIONS 24     // tramos de pasillo por trozo
#define WALL_HALF 1.3f        // m: las paredes de un pasillo, a cada lado del eje (se camina hasta CAVE_CORRIDOR_HALF)
#define FLOOR_RINGS 8         // el suelo de una sala, del centro a la pared (fino: la luz va por vertice)
#define WALL_ROWS 5           // la pared de una sala: el pie, media altura, el techo de los pasillos, otra vez y la boveda
#define BUILD_VERTS 4096
#define BUILD_INDS 16384

typedef struct {
    Vector3 at;
    float radius, power, r, g, b;
} CaveLight;

static CaveLight g_lights[LIGHTS_MAX];
static int g_nlights;
static Vector3 g_eye; // de donde se mide la bruma (el jugador)

typedef struct {
    Mesh mesh;       // las posiciones, la textura y los indices; en mesh.colors, los de este cuadro
    Color *base;     // el color de cada vertice, sin luz
    Vector3 center;
    float radius;
    uint8_t surface; // ClSurface: su textura
} Chunk;

static Chunk g_chunks[CHUNKS_MAX];
static int g_nchunks;
static int g_loaded = -1;
static CaveDecor g_decor[CAVE_DECOR_MAX];
static int g_ndecor;
static Material g_mat;
static bool g_mat_ok;
static CsStats g_stats;

static float hash01(int a, int b) {
    uint32_t h = (uint32_t)a * 0x9E3779B1u ^ (uint32_t)b * 0x85EBCA77u;
    h ^= h >> 15, h *= 0x2C1B3C6Du, h ^= h >> 12;
    return (float)(h & 0xFFFFu) / 65535.0f;
}

static Color mixc(Color a, Color b, float t) {
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    return (Color){ (unsigned char)((float)a.r + ((float)b.r - (float)a.r) * t), (unsigned char)((float)a.g + ((float)b.g - (float)a.g) * t),
                    (unsigned char)((float)a.b + ((float)b.b - (float)a.b) * t), a.a };
}

// ---------------------------------------------------------------- la luz
// La altura de la lampara colgada de una «estalactita» del imperio (en las salas labradas, en vez de la piedra cuelga
// una lampara de bronce: src/game/cave_props.c la dibuja).
float cs_lamp_height(const Cave *c, int obj) {
    const CaveObject *o = &c->objects[obj];
    return c->rooms[o->room].floor_y + (o->room == 0 ? 6.5f : 4.6f) + 0.4f * o->size;
}

void cs_gather_lights(const Cave *c, uint64_t done, Vector3 player, float player_yaw, bool torch_lit, float world_time, float time) {
    g_nlights = 0;
    g_eye = player;
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
    bool carved = cave_room_sides(c, 0) > 0;
    for (int i = 0; i < c->object_count && g_nlights < LIGHTS_MAX; i++) {
        const CaveObject *o = &c->objects[i];
        float fl = 1.0f + 0.1f * sinf(time * 11.0f + (float)i * 1.7f) * sinf(time * 6.1f + (float)i), x, y, z, yaw;
        if (carved && o->kind == COBJ_STALACTITE) { // las lamparas colgadas del imperio
            g_lights[g_nlights++] = (CaveLight){ { o->x, cs_lamp_height(c, i) - 0.1f, o->z }, 6.0f * fl, 0.9f, 1.0f, 0.7f, 0.4f };
            continue;
        }
        if (!cs_object_lit(c, done, i)) continue;
        cave_object_at(c, i, &x, &y, &z, &yaw);
        if (o->kind == COBJ_TORCH) // la llama: un poco hacia adentro y arriba del soporte
            g_lights[g_nlights++] = (CaveLight){ { x + sinf(yaw) * 0.22f, y + 0.32f, z + cosf(yaw) * 0.22f }, 8.0f * fl, 1.15f, 1.0f, 0.66f, 0.36f };
        else g_lights[g_nlights++] = (CaveLight){ { x, y + 0.6f, z }, 12.0f * fl, 1.35f, 1.0f, 0.62f, 0.32f };
    }
    // Lo que alumbra del decorado: braseros, lamparas y la luz del agujero de la boveda.
    for (int i = 0; i < g_ndecor && g_nlights < LIGHTS_MAX; i++) {
        const CaveDecor *d = &g_decor[i];
        float fl = 1.0f + 0.09f * sinf(time * 9.0f + (float)i * 2.3f) * sinf(time * 5.3f + (float)i);
        if (d->kind == CDEC_BRAZIER)
            g_lights[g_nlights++] = (CaveLight){ { d->x, d->y + d->size + 0.35f, d->z }, 9.0f * fl, 1.2f, 1.0f, 0.6f, 0.3f };
        else if (d->kind == CDEC_LAMP)
            g_lights[g_nlights++] = (CaveLight){ { d->x, d->y + 0.45f, d->z }, 4.6f * fl, 0.95f, 1.0f, 0.7f, 0.38f };
        else if (d->kind == CDEC_MOSS && c->kind == CAVE_ANIMAL) // los hongos que brillan
            g_lights[g_nlights++] = (CaveLight){ { d->x, d->y + 0.5f, d->z }, 3.8f, 0.55f, 0.3f, 0.9f, 0.8f };
        else if (d->kind == CDEC_SHAFT) // de dia, el sol que entra; de noche, la luna
            g_lights[g_nlights++] = (CaveLight){ { d->x, d->y + 4.0f, d->z }, 13.0f, 0.15f + 1.05f * day, 0.55f + 0.45f * day, 0.65f + 0.3f * day, 0.95f - 0.1f * day };
    }
}

bool cs_object_lit(const Cave *c, uint64_t done, int obj) {
    const CaveObject *o = &c->objects[obj];
    return (o->kind == COBJ_TORCH || o->kind == COBJ_FIRE) && (o->lit || ((done >> obj) & 1u));
}

// Cada vertice suma la luz de las llamas cercanas (y la de la boca) sobre la de fondo de su tipo, con su caida; a lo
// lejos, la bruma. idx: las luces que alcanzan (NULL: todas).
static Color shade_list(Color base, float x, float y, float z, const int *idx, int n) {
    const ClLook *lk = cl_look();
    float r = lk->ambient[0], g = lk->ambient[1], b = lk->ambient[2];
    for (int k = 0; k < n; k++) {
        const CaveLight *l = &g_lights[idx ? idx[k] : k];
        float dx = x - l->at.x, dy = y - l->at.y, dz = z - l->at.z, d2 = dx * dx + dy * dy + dz * dz;
        if (d2 >= l->radius * l->radius) continue;
        float f = 1.0f - sqrtf(d2) / l->radius;
        f = f * f * l->power;
        r += f * l->r, g += f * l->g, b += f * l->b;
    }
    Color lit = { (unsigned char)fminf(255.0f, (float)base.r * fminf(r, 1.5f)), (unsigned char)fminf(255.0f, (float)base.g * fminf(g, 1.5f)),
                  (unsigned char)fminf(255.0f, (float)base.b * fminf(b, 1.5f)), base.a };
    float ex = x - g_eye.x, ey = y - g_eye.y, ez = z - g_eye.z, d = sqrtf(ex * ex + ey * ey + ez * ez);
    float fog = (d - lk->fog_near) / fmaxf(lk->fog_far - lk->fog_near, 1.0f);
    return fog > 0.0f ? mixc(lit, lk->fog, fminf(fog, 1.0f) * lk->fog_max) : lit;
}

Color cs_shade(Color base, Vector3 p) { return shade_list(base, p.x, p.y, p.z, NULL, g_nlights); }
Color cs_fog(void) { return cl_look()->fog; }

// ---------------------------------------------------------------- armar la roca
// Tres armados a la vez (pared, boveda y suelo): cada uno termina en su trozo, con su textura.
typedef struct {
    Vector3 pos[BUILD_VERTS];
    Vector2 uv[BUILD_VERTS];
    Color col[BUILD_VERTS];
    unsigned short ind[BUILD_INDS];
    int nv, ni;
    bool full;
} Builder;

static Builder g_b[CL_SURFACES];

static int bv(Builder *b, Vector3 p, Vector2 uv, Color c) {
    if (b->nv >= BUILD_VERTS) return b->full = true, -1;
    b->pos[b->nv] = p, b->uv[b->nv] = uv, b->col[b->nv] = c;
    return b->nv++;
}

static void bquad(Builder *b, int v0, int v1, int v2, int v3) {
    if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0 || b->ni + 6 > BUILD_INDS) return;
    unsigned short *o = &b->ind[b->ni];
    o[0] = (unsigned short)v0, o[1] = (unsigned short)v1, o[2] = (unsigned short)v2, o[3] = (unsigned short)v0, o[4] = (unsigned short)v2,
    o[5] = (unsigned short)v3;
    b->ni += 6;
}

static void btri(Builder *b, int v0, int v1, int v2) {
    if (v0 < 0 || v1 < 0 || v2 < 0 || b->ni + 3 > BUILD_INDS) return;
    b->ind[b->ni++] = (unsigned short)v0, b->ind[b->ni++] = (unsigned short)v1, b->ind[b->ni++] = (unsigned short)v2;
}

static void b_begin(void) {
    for (int s = 0; s < CL_SURFACES; s++) g_b[s].nv = 0, g_b[s].ni = 0, g_b[s].full = false;
}

// Lo armado pasa a un trozo: su malla (en la GPU, con los colores que cambian cada cuadro) y su esfera.
static bool b_finish_one(int surface) {
    Builder *b = &g_b[surface];
    if (b->full) return false;
    if (b->nv < 3 || b->ni < 3) return true;
    if (g_nchunks >= CHUNKS_MAX) return false;
    Chunk *ch = &g_chunks[g_nchunks];
    memset(ch, 0, sizeof(*ch));
    Mesh *m = &ch->mesh;
    m->vertexCount = b->nv, m->triangleCount = b->ni / 3;
    m->vertices = MemAlloc((unsigned int)b->nv * 3 * sizeof(float));
    m->texcoords = MemAlloc((unsigned int)b->nv * 2 * sizeof(float));
    m->colors = MemAlloc((unsigned int)b->nv * 4);
    m->indices = MemAlloc((unsigned int)b->ni * sizeof(unsigned short));
    ch->base = MemAlloc((unsigned int)b->nv * sizeof(Color));
    if (!m->vertices || !m->texcoords || !m->colors || !m->indices || !ch->base) {
        MemFree(m->vertices), MemFree(m->texcoords), MemFree(m->colors), MemFree(m->indices), MemFree(ch->base);
        return false;
    }
    Vector3 lo = b->pos[0], hi = b->pos[0];
    for (int i = 0; i < b->nv; i++) {
        m->vertices[i * 3] = b->pos[i].x, m->vertices[i * 3 + 1] = b->pos[i].y, m->vertices[i * 3 + 2] = b->pos[i].z;
        m->texcoords[i * 2] = b->uv[i].x, m->texcoords[i * 2 + 1] = b->uv[i].y;
        memcpy(&m->colors[i * 4], &b->col[i], 4);
        ch->base[i] = b->col[i];
        lo = Vector3Min(lo, b->pos[i]), hi = Vector3Max(hi, b->pos[i]);
    }
    memcpy(m->indices, b->ind, (size_t)b->ni * sizeof(unsigned short));
    ch->center = Vector3Scale(Vector3Add(lo, hi), 0.5f);
    ch->radius = Vector3Distance(lo, hi) * 0.5f;
    ch->surface = (uint8_t)surface;
    UploadMesh(m, true);
    g_nchunks++;
    g_stats.vertices += b->nv, g_stats.triangles += b->ni / 3;
    return true;
}

static bool b_finish(void) {
    bool ok = true;
    for (int s = 0; s < CL_SURFACES; s++) ok = b_finish_one(s) && ok;
    return ok;
}

static Color grain(Color base, int a, int b) {
    float k = 0.88f + 0.24f * hash01(a, b);
    return (Color){ (unsigned char)fminf(255.0f, (float)base.r * k), (unsigned char)fminf(255.0f, (float)base.g * k),
                    (unsigned char)fminf(255.0f, (float)base.b * k), 255 };
}

// La pared a la altura h: abajo, el musgo, lo mojado o la mugre (hasta moss_h, mas en unos tramos que en otros; en las
// labradas, el zocalo parejo).
static Color wall_tone(const ClLook *lk, int seed, int k, int row, float h) {
    Color c = grain(lk->wall, seed * 1000 + row, k);
    if (lk->moss_h > 0.0f && h < lk->moss_h) {
        float patchy = lk->jag > 0.0f ? 0.4f + 0.6f * hash01(seed * 77 + k / 3, 5) : 1.0f;
        c = mixc(c, lk->moss, (1.0f - fmaxf(h, 0.0f) / lk->moss_h) * patchy * 0.85f);
    }
    return c;
}

// La boveda: t de la pared (0) a la clave (1); el hollin de las antorchas se junta arriba.
static Color vault_tone(const ClLook *lk, int seed, int k, int j, float t) {
    Color c = grain(lk->vault, seed * 1000 + 300 + j, k);
    if (lk->soot > 0.0f) c = mixc(c, (Color){ 22, 18, 14, 255 }, lk->soot * powf(t, 1.4f) * (0.7f + 0.3f * hash01(seed + k, j)));
    return c;
}

static Color floor_tone(const ClLook *lk, int seed, int ring, int k, float x, float z) {
    Color c = grain(lk->floor, seed * 1000 + 500 + ring, k);
    float edge = 1.0f - 0.07f * (float)ring / (float)FLOOR_RINGS;
    c = (Color){ (unsigned char)((float)c.r * edge), (unsigned char)((float)c.g * edge), (unsigned char)((float)c.b * edge), 255 };
    if (lk->jag > 0.7f) { // en las naturales, manchas mojadas o de musgo
        float n = 0.5f + 0.5f * sinf(x * 0.37f + z * 0.21f) * sinf(z * 0.43f - x * 0.17f);
        c = mixc(c, lk->moss, (n > 0.62f ? (n - 0.62f) * 1.6f : 0.0f));
    }
    return c;
}

// Lo desparejo de la roca en una columna de la pared (a) y una fila: de -1 a 1, siguiendo el angulo (sin costura).
static float rock_noise(int seed, int row, float a, int k) {
    return 0.6f * sinf(a * 5.0f + (float)row * 1.3f + (float)seed * 0.7f) * sinf(a * 3.0f + (float)seed * 1.9f) +
           0.4f * (hash01(seed * 131 + row, k) * 2.0f - 1.0f);
}

// Una sala (o el vestibulo: CAVE_VESTIBULE, con la boca abierta hacia -z): la pared (WALL_ROWS filas: el perfil hasta
// donde nace la boveda, con filas de mas para que la roca natural no sea un tabique), la boveda del perfil
// (cave_room_profile) y el suelo, cada uno en su trozo con su textura. En las naturales, la roca entra y sale (jag): abajo
// solo hacia afuera (lo que cuelga de la pared queda sobre ella), arriba tambien hacia adentro. Donde del otro lado de la
// pared hay un pasillo, la pared se abre hasta su techo.
static bool build_room(const Cave *c, int room) {
    const ClLook *lk = cl_look();
    bool vest = room == CAVE_VESTIBULE;
    float cx = vest ? 0.0f : c->rooms[room].x, cz = vest ? CAVE_VEST_Z : c->rooms[room].z, fy = vest ? 0.0f : c->rooms[room].floor_y;
    float radius = vest ? CAVE_VEST_R : c->rooms[room].radius, jag = lk->jag;
    int segs = (int)(2.0f * PI * radius / 1.1f); // una columna cada 1,1 m de pared (la roca, en menos de 20000 triangulos)
    segs = segs < 40 ? 40 : segs > 96 ? 96 : segs;
    static const float FLOOR_F[FLOOR_RINGS] = { 0.0f, 0.14f, 0.28f, 0.42f, 0.56f, 0.7f, 0.85f, 1.0f };
    float step = 2.0f * PI / (float)segs, pf[CAVE_PROFILE_PTS], ph[CAVE_PROFILE_PTS], pv[CAVE_PROFILE_PTS];
    int seed = vest ? 999 : room;
    for (int j = 0; j < CAVE_PROFILE_PTS; j++) cave_room_profile(c, room, j, &pf[j], &ph[j]);
    float mean_r = radius + 0.3f + 0.05f * radius;
    pv[0] = ph[0], pv[1] = ph[1], pv[2] = ph[2]; // de la textura de la boveda: lo andado por el perfil desde el suelo
    for (int j = 3; j < CAVE_PROFILE_PTS; j++) pv[j] = pv[j - 1] + hypotf((pf[j - 1] - pf[j]) * mean_r, ph[j] - ph[j - 1]);
    const float wh[WALL_ROWS] = { ph[0], (ph[0] + ph[1]) * 0.5f, ph[1], (ph[1] + ph[2]) * 0.5f, ph[2] };
    // Las vueltas de la textura alrededor de la sala: enteras, sin costura.
    float rep_w = fmaxf(1.0f, roundf(2.0f * PI * mean_r / lk->tile[CL_WALL])), rep_v = fmaxf(1.0f, roundf(2.0f * PI * mean_r / lk->tile[CL_VAULT]));
    b_begin();
    Builder *bw = &g_b[CL_WALL], *bvt = &g_b[CL_VAULT], *bf = &g_b[CL_FLOOR];
    for (int k = 0; k <= segs; k++) {
        int kk = k % segs;
        float a = (float)kk * step, wr = cave_wall_radius(c, room, a), s = sinf(a), co = cosf(a), u = (float)k / (float)segs;
        // La pared: el pie justo en la pared (ahi termina el suelo), abajo solo hacia afuera, arriba hacia los dos lados.
        float out[WALL_ROWS] = { 0.0f, 0.22f * (0.5f + 0.5f * rock_noise(seed, 1, a, kk)), 0.08f * rock_noise(seed, 2, a, kk),
                                 0.35f * rock_noise(seed, 3, a, kk), -0.3f * (0.5f + 0.5f * rock_noise(seed, 4, a, kk)) };
        for (int i = 0; i < WALL_ROWS; i++) {
            float rr = wr + out[i] * jag;
            bv(bw, (Vector3){ cx + s * rr, fy + wh[i], cz + co * rr }, (Vector2){ u * rep_w, -wh[i] / lk->tile[CL_WALL] },
               wall_tone(lk, seed, kk, i, wh[i]));
        }
        // La boveda: desde la misma fila de arriba de la pared hasta la clave, con bultos en las naturales.
        for (int j = 2; j < CAVE_PROFILE_PTS; j++) {
            float rr = j == 2 ? wr + out[WALL_ROWS - 1] * jag : wr * pf[j] * (1.0f - 0.07f * jag * (0.5f + 0.5f * rock_noise(seed, 10 + j, a, kk)));
            float hh = ph[j] + (j > 2 && j < CAVE_PROFILE_PTS - 1 ? 0.35f * jag * rock_noise(seed, 20 + j, a, kk) : 0.0f);
            bv(bvt, (Vector3){ cx + s * rr, fy + hh, cz + co * rr }, (Vector2){ u * rep_v, -pv[j] / lk->tile[CL_VAULT] },
               vault_tone(lk, seed, kk, j, (float)(j - 2) / (float)(CAVE_PROFILE_PTS - 3)));
        }
    }
    // El suelo: anillos del centro a la pared, cada uno con los tramos que le tocan por su largo (cerca del centro, pocos),
    // cosidos entre si: triangulos parejos, sin astillas que rayen la luz (va por vertice).
    int ring0[FLOOR_RINGS], ringn[FLOOR_RINGS];
    for (int j = 0; j < FLOOR_RINGS; j++) {
        int n = j == 0 ? 1 : j == FLOOR_RINGS - 1 ? segs : (int)((float)segs * FLOOR_F[j] + 0.5f);
        if (j > 0 && n < 8) n = 8;
        ring0[j] = bf->nv, ringn[j] = n;
        for (int k = 0; k <= (j == 0 ? 0 : n); k++) { // el ultimo repite el primero (la costura)
            float a = (float)(k % (n > 0 ? n : 1)) * 2.0f * PI / (float)n, wr = cave_wall_radius(c, room, a);
            float x = cx + sinf(a) * wr * FLOOR_F[j], z = cz + cosf(a) * wr * FLOOR_F[j];
            bv(bf, (Vector3){ x, fy, z }, (Vector2){ x / lk->tile[CL_FLOOR], z / lk->tile[CL_FLOOR] }, floor_tone(lk, seed, j, k % (n > 0 ? n : 1), x, z));
        }
    }
    for (int j = 1; j < FLOOR_RINGS; j++) {
        int i = 0, k = 0, na = j == 1 ? 1 : ringn[j - 1], nb = ringn[j];
        while (i < na || k < nb) { // se avanza por el anillo que llega antes a su proximo angulo
            int a0 = ring0[j - 1] + (j == 1 ? 0 : i), b0 = ring0[j] + k;
            bool step_a = k >= nb || (j > 1 && i < na && (float)(i + 1) / (float)na < (float)(k + 1) / (float)nb);
            if (step_a) {
                btri(bf, a0, a0 + 1, b0);
                i++;
            } else {
                btri(bf, a0, b0 + 1, b0);
                k++;
                if (j == 1 && k >= nb) i = na;
            }
        }
    }
    for (int k = 0; k < segs; k++) {
        float am = ((float)k + 0.5f) * step;
        bool door = cave_door_at(c, room, am) || (vest && cz + cosf(am) * radius < 0.6f); // el vestibulo: la boca, la luz de afuera
        for (int i = 0; i < WALL_ROWS - 1; i++) {
            if (door && i < 2) continue; // hasta el techo del pasillo
            bquad(bw, k * WALL_ROWS + i, (k + 1) * WALL_ROWS + i, (k + 1) * WALL_ROWS + i + 1, k * WALL_ROWS + i + 1);
        }
        for (int j = 0; j < CAVE_PROFILE_PTS - 3; j++) {
            int n = CAVE_PROFILE_PTS - 2;
            bquad(bvt, k * n + j, (k + 1) * n + j, (k + 1) * n + j + 1, k * n + j + 1);
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
// las salas, que no titile donde se juntan), las dos paredes y el techo, con su clave en el medio (en las labradas, una
// boveda de cañon; en las naturales, despareja). Una pared con algo del otro lado (otro pasillo junto a una sala, una
// sala) no se dibuja: se abre; adentro de una sala, el techo es su boveda.
static bool build_corridor(const Cave *c, int k) {
    const ClLook *lk = cl_look();
    const CaveCorridor *cr = &c->corridors[k];
    float jag = lk->jag, tw = lk->tile[CL_WALL], tv = lk->tile[CL_VAULT], tf = lk->tile[CL_FLOOR];
    int n = (int)ceilf(cr->length / SEG_LEN);
    if (n < 2) n = 2;
    for (int i0 = 0; i0 < n; i0 += CHUNK_STATIONS) {
        int i1 = i0 + CHUNK_STATIONS < n ? i0 + CHUNK_STATIONS : n;
        b_begin();
        Builder *bw = &g_b[CL_WALL], *bvt = &g_b[CL_VAULT], *bf = &g_b[CL_FLOOR];
        for (int i = i0; i <= i1; i++) {
            float t = (float)i / (float)n, x, z, y, dx, dz, along = t * cr->length;
            cave_corridor_at(c, k, t, &x, &z, &y, &dx, &dz);
            float nx = -dz, nz = dx, top[2] = { 0.0f, 0.0f };
            for (int side = 0; side < 2; side++) {
                float sg = side ? -1.0f : 1.0f, w = WALL_HALF + jag * 0.14f * sinf((float)i * 2.3f + (float)k * 1.1f + (float)side * 0.7f);
                for (int h = 0; h < 3; h++) {
                    float lift = (float)h * CAVE_CORRIDOR_HEIGHT * 0.5f;
                    // natural: a media altura, la roca se abre un poco; arriba, la boveda se cierra
                    float in = h == 2 ? 0.25f * jag : h == 1 ? -0.18f * jag * (0.5f + 0.5f * rock_noise(k * 7 + side, h, (float)i * 0.6f, i)) : 0.0f;
                    float py = y + lift - (h == 0 ? 0.2f : 0.0f);
                    bv(bw, (Vector3){ x + nx * sg * (w - in), py, z + nz * sg * (w - in) }, (Vector2){ along / tw, -lift / tw },
                       wall_tone(lk, k * 7 + side, i, h, lift));
                    if (h == 2) top[side] = w - in;
                }
            }
            // El techo: el borde de cada pared y la clave en el medio.
            float key = CAVE_CORRIDOR_HEIGHT + (jag > 0.0f ? 0.18f + 0.22f * jag * (0.5f + 0.5f * rock_noise(k, 9, (float)i * 0.5f, i)) : 0.45f);
            for (int p = 0; p < 3; p++) {
                float lat = p == 0 ? top[0] : p == 2 ? -top[1] : 0.0f, hy = p == 1 ? key : CAVE_CORRIDOR_HEIGHT;
                bv(bvt, (Vector3){ x + nx * lat, y + hy, z + nz * lat }, (Vector2){ along / tv, lat / tv },
                   vault_tone(lk, k * 7 + 3, i, p, p == 1 ? 0.6f : 0.2f));
            }
            for (int side = 0; side < 2; side++) {
                float sg = side ? -1.0f : 1.0f;
                bv(bf, (Vector3){ x + nx * sg * WALL_HALF, y + 0.015f, z + nz * sg * WALL_HALF }, (Vector2){ along / tf, sg * WALL_HALF / tf },
                   floor_tone(lk, k * 7 + 5, side * 3, i, x, z));
            }
        }
        for (int i = i0; i < i1; i++) {
            int a = (i - i0) * 6, b = a + 6, av = (i - i0) * 3, bvv = av + 3, af = (i - i0) * 2, bff = af + 2;
            float t = ((float)i + 0.5f) / (float)n, x, z, y, dx, dz;
            cave_corridor_at(c, k, t, &x, &z, &y, &dx, &dz);
            bquad(bf, af, bff, bff + 1, af + 1); // el suelo
            if (!under_room(c, x, z)) bquad(bvt, av, bvv, bvv + 1, av + 1), bquad(bvt, av + 1, bvv + 1, bvv + 2, av + 2); // el techo
            for (int side = 0; side < 2; side++) {
                float sg = side ? -1.0f : 1.0f, d = WALL_HALF + 0.45f;
                int where = 0, o = side * 3;
                if (cave_floor(c, x - dz * sg * d, z + dx * sg * d, NULL, &where) && where != CAVE_IN_CORRIDOR + k) continue;
                for (int h = 0; h < 2; h++) bquad(bw, a + o + h, b + o + h, b + o + h + 1, a + o + h + 1);
            }
        }
        if (!b_finish()) return false;
    }
    return true;
}

// ---------------------------------------------------------------- cargar y liberar
static VoxCaveKind decor_model(int kind) {
    switch (kind) {
    case CDEC_ROCK: return VC_ROCK;
    case CDEC_STALACTITES: return VC_STALACTITES;
    case CDEC_STALAGMITES: return VC_STALAGMITES;
    case CDEC_PILLAR: return VC_PILLAR;
    case CDEC_RUBBLE: return VC_RUBBLE;
    case CDEC_CRATES: return VC_CRATES;
    case CDEC_SACKS: return VC_SACKS;
    case CDEC_STELE: return VC_STELE;
    default: return VC_KINDS; // lo demas se dibuja con figuras simples
    }
}

bool cs_load(const Cave *c, int cave) {
    cs_unload();
    double t0 = GetTime();
    if (!g_mat_ok) g_mat = LoadMaterialDefault(), g_mat_ok = true;
    memset(&g_stats, 0, sizeof(g_stats));
    if (!cl_load(c)) TraceLog(LOG_WARNING, "CUEVA: sin las texturas de la cueva %d (se ve sin ellas)", cave);
    const ClLook *lk = cl_look();
    // Los modelos, con la roca de las paredes (un poco mas oscura: a las paredes las oscurece su textura).
    vc_set_rock(ColorBrightness(lk->wall, -0.12f));
    bool ok = build_room(c, CAVE_VESTIBULE);
    for (int r = 0; r < c->room_count && ok; r++) ok = build_room(c, r);
    for (int k = 0; k < c->corridor_count && ok; k++) ok = build_corridor(c, k);
    g_ndecor = ok ? cave_decor(c, g_decor, CAVE_DECOR_MAX) : 0;
    for (int i = 0; i < g_ndecor && ok; i++) {
        VoxCaveKind m = decor_model(g_decor[i].kind);
        if (m != VC_KINDS) vc_preload(m, (Region)c->region, g_decor[i].variant);
    }
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
    cl_unload();
}

bool cs_loaded(void) { return g_loaded >= 0; }
int cs_cave(void) { return g_loaded; }

void cs_forget_gpu(void) {
    for (int i = 0; i < g_nchunks; i++) { // lo de la CPU se libera; lo de la GPU ya no existe
        Mesh *m = &g_chunks[i].mesh;
        MemFree(m->vertices), MemFree(m->texcoords), MemFree(m->colors), MemFree(m->indices), MemFree(g_chunks[i].base);
        MemFree(m->vboId);
    }
    memset(g_chunks, 0, sizeof(g_chunks));
    g_nchunks = 0, g_ndecor = 0, g_loaded = -1, g_mat_ok = false;
    vc_forget_gpu();
    cl_forget_gpu();
}

const CaveDecor *cs_decor(int *n) {
    if (n) *n = g_ndecor;
    return g_decor;
}

CsStats cs_stats(void) {
    CsStats s = g_stats;
    s.models = vc_loaded(&s.model_tris); // los del decorado y los de los objetos (src/game/cave_props.c)
    s.model_max = vc_largest();
    return s;
}

// ---------------------------------------------------------------- dibujar el decorado
static void vtx(Vector3 p, Color c) {
    rlColor4ub(c.r, c.g, c.b, c.a);
    rlVertex3f(p.x, p.y, p.z);
}

// Una mancha chata y despareja en el suelo (charcos, musgo, pieles): un abanico con el borde corrido por el ruido, con la
// luz en el centro y en cada punto del borde.
void cs_blob(Vector3 at, float radius, int seed, Color in, Color edge, bool lit) {
    enum { N = 14 };
    Vector3 rim[N];
    Color rc[N];
    for (int i = 0; i < N; i++) {
        float a = (float)i * 2.0f * PI / (float)N, r = radius * (0.78f + 0.3f * hash01(seed, i));
        rim[i] = (Vector3){ at.x + sinf(a) * r, at.y, at.z + cosf(a) * r };
        rc[i] = lit ? cs_shade(edge, rim[i]) : edge;
    }
    Color ci = lit ? cs_shade(in, at) : in;
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < N; i++) {
        vtx(at, ci);
        vtx(rim[(i + 1) % N], rc[(i + 1) % N]);
        vtx(rim[i], rc[i]);
    }
    rlEnd();
}

// Un rectangulo chato en el suelo (alfombras, jergones vistos de arriba), girado yaw, a la altura y.
static void flat_rect(Vector3 at, float yaw, float len, float wid, float y, Color col) {
    float fx = sinf(yaw), fz = cosf(yaw), sx = fz, sz = -fx, hl = len * 0.5f, hw = wid * 0.5f;
    Vector3 p[4] = { { at.x - fx * hl - sx * hw, at.y + y, at.z - fz * hl - sz * hw }, { at.x + fx * hl - sx * hw, at.y + y, at.z + fz * hl - sz * hw },
                     { at.x + fx * hl + sx * hw, at.y + y, at.z + fz * hl + sz * hw }, { at.x - fx * hl + sx * hw, at.y + y, at.z - fz * hl + sz * hw } };
    Color c[4];
    for (int i = 0; i < 4; i++) c[i] = cs_shade(col, p[i]);
    rlBegin(RL_TRIANGLES);
    vtx(p[0], c[0]), vtx(p[2], c[2]), vtx(p[1], c[1]);
    vtx(p[0], c[0]), vtx(p[3], c[3]), vtx(p[2], c[2]);
    rlEnd();
}

// Una caja girada yaw con el pie en at: (fwd, up, side) desde at en los ejes de la caja.
static void box_at(Vector3 at, float yaw, float fwd, float up, float side, float len, float h, float wid, Color col) {
    float fx = sinf(yaw), fz = cosf(yaw);
    Vector3 p = { at.x + fx * fwd + fz * side, at.y + up, at.z + fz * fwd - fx * side };
    Color c = cs_shade(col, p);
    rlPushMatrix();
    rlTranslatef(p.x, p.y, p.z);
    rlRotatef(yaw * RAD2DEG, 0, 1, 0);
    DrawCube((Vector3){ 0, 0, 0 }, wid, h, len, c);
    rlPopMatrix();
}

// Una caja contra la pared de una sala, con la textura de la pared (las pilastras): en el angulo a, su centro a rc del
// centro de la sala, dpt de fondo, w de ancho y de y0 a y1 sobre el suelo (fy). Las hiladas siguen las de la pared.
static void tex_box(float cx, float cz, float fy, float a, float rc, float dpt, float w, float y0, float y1, Color col) {
    const ClLook *lk = cl_look();
    float rx = sinf(a), rz = cosf(a), tx = rz, tz = -rx, t = lk->tile[CL_WALL], ri = rc - dpt * 0.5f, ro = rc + dpt * 0.5f, hw = w * 0.5f;
    Vector3 p[8]; // 0..3 abajo, 4..7 arriba: (adentro, -t), (adentro, +t), (afuera, +t), (afuera, -t)
    for (int k = 0; k < 8; k++) {
        float rr = (k & 3) < 2 ? ri : ro, ss = ((k & 3) == 1 || (k & 3) == 2) ? hw : -hw;
        p[k] = (Vector3){ cx + rx * rr + tx * ss, fy + (k < 4 ? y0 : y1), cz + rz * rr + tz * ss };
    }
    // Las caras que se ven: la de adelante, los dos costados y la de arriba (con su luz en el medio).
    static const int F[4][4] = { { 0, 1, 5, 4 }, { 1, 2, 6, 5 }, { 3, 0, 4, 7 }, { 4, 5, 6, 7 } };
    rlSetTexture(cl_texture(CL_WALL).id);
    rlBegin(RL_QUADS);
    for (int f = 0; f < 4; f++) {
        Vector3 m = Vector3Scale(Vector3Add(Vector3Add(p[F[f][0]], p[F[f][1]]), Vector3Add(p[F[f][2]], p[F[f][3]])), 0.25f);
        Color cc = cs_shade(col, m);
        float uw = (f == 0 ? w : dpt) / t;
        for (int k = 0; k < 4; k++) {
            const Vector3 q = p[F[f][k]];
            float u = (k == 1 || k == 2) ? uw : 0.0f, v = f == 3 ? ((k >= 2) ? dpt / t : 0.0f) : -(q.y - fy) / t;
            rlColor4ub(cc.r, cc.g, cc.b, 255);
            rlTexCoord2f(u, v);
            rlVertex3f(q.x, q.y, q.z);
        }
    }
    rlEnd();
    rlSetTexture(0);
}

static void draw_decor(const Cave *c, const CaveDecor *d, int i, float world_time, float time) {
    Vector3 at = { d->x, d->y, d->z };
    int kind = c->kind;
    Region rg = (Region)c->region;
    switch (d->kind) {
    case CDEC_ROCK:
    case CDEC_STALAGMITES:
    case CDEC_RUBBLE:
    case CDEC_CRATES:
    case CDEC_SACKS:
    case CDEC_STELE: {
        Color tint = cs_shade(WHITE, (Vector3){ d->x, d->y + fminf(d->size, 1.6f) * 0.5f, d->z });
        vc_draw(decor_model(d->kind), rg, d->variant, at, d->yaw, d->size, tint);
        break;
    }
    case CDEC_STALACTITES: { // de la boveda, tal como esta sobre ese punto del suelo
        float top = cave_rock_above(c, d->x, d->z, d->room, d->y) + 0.3f;
        vc_draw(VC_STALACTITES, rg, d->variant, (Vector3){ d->x, top, d->z }, d->yaw, d->size, cs_shade(WHITE, (Vector3){ d->x, top - d->size * 0.6f, d->z }));
        break;
    }
    case CDEC_PILLAR: { // del suelo a la boveda (mas baja junto a la pared): se estira a lo alto
        float h = cave_rock_above(c, d->x, d->z, d->room, d->y) - d->y + 0.5f;
        vc_draw_scaled(VC_PILLAR, rg, d->variant, (Vector3){ d->x, d->y - 0.1f, d->z }, d->yaw, d->size, h, cs_shade(WHITE, (Vector3){ d->x, d->y + 1.6f, d->z }));
        break;
    }
    case CDEC_POOL: { // el borde mojado, el agua honda en el medio y un brillo
        bool green = kind == CAVE_ABANDONED;
        float r = d->size * 0.5f;
        cs_blob((Vector3){ at.x, at.y + 0.018f, at.z }, r * 1.12f, i * 7 + 1, (Color){ 46, 48, 50, 255 }, (Color){ 58, 58, 58, 255 }, true);
        cs_blob((Vector3){ at.x, at.y + 0.03f, at.z }, r, i * 7 + 2, green ? (Color){ 16, 34, 26, 255 } : (Color){ 12, 28, 40, 255 },
             green ? (Color){ 54, 80, 56, 255 } : (Color){ 46, 78, 96, 255 }, true);
        cs_blob((Vector3){ at.x + r * 0.25f, at.y + 0.034f, at.z - r * 0.2f }, r * 0.35f, i * 7 + 3, (Color){ 110, 140, 156, 255 },
             green ? (Color){ 26, 52, 40, 255 } : (Color){ 22, 46, 62, 255 }, true);
        break;
    }
    case CDEC_BONES: { // la osamenta de la guarida: huesos largos, costillas y un craneo o dos
        Color bone = (Color){ 214, 204, 180, 255 };
        float spread = d->size * 0.45f;
        for (int k = 0; k < 8; k++) {
            float a = hash01(i, k) * 2.0f * PI, r = spread * hash01(k, i + 3), l = 0.15f + 0.22f * hash01(i + k, 9), b = hash01(k, i) * 2.0f * PI;
            Vector3 p = { at.x + sinf(a) * r, at.y + 0.03f, at.z + cosf(a) * r };
            Color cb = cs_shade(bone, p);
            DrawCylinderEx((Vector3){ p.x - sinf(b) * l, p.y, p.z - cosf(b) * l }, (Vector3){ p.x + sinf(b) * l, p.y, p.z + cosf(b) * l }, 0.028f, 0.022f, 4, cb);
        }
        for (int k = 0; k < 1 + (d->variant & 1); k++) {
            Vector3 p = { at.x + (hash01(i, 20 + k) - 0.5f) * spread, at.y + 0.09f, at.z + (hash01(20 + k, i) - 0.5f) * spread };
            DrawSphereEx(p, 0.1f, 5, 6, cs_shade((Color){ 226, 218, 196, 255 }, p));
            DrawSphereEx((Vector3){ p.x + 0.05f, p.y - 0.02f, p.z + 0.07f }, 0.05f, 4, 5, cs_shade((Color){ 200, 190, 166, 255 }, p));
        }
        break;
    }
    case CDEC_MOSS:
        if (kind == CAVE_ANIMAL) { // hongos que brillan (se ven sin luz): la mancha oscura y los sombreritos
            cs_blob((Vector3){ at.x, at.y + 0.02f, at.z }, d->size * 0.5f, i * 5, (Color){ 34, 42, 36, 255 }, (Color){ 40, 46, 42, 255 }, true);
            float pulse = 0.85f + 0.15f * sinf(time * 1.3f + (float)i);
            for (int k = 0; k < 9; k++) {
                float a = hash01(i, k + 40) * 2.0f * PI, r = d->size * 0.42f * hash01(k + 40, i);
                Vector3 p = { at.x + sinf(a) * r, at.y + 0.04f + 0.03f * hash01(i, k), at.z + cosf(a) * r };
                DrawSphereEx(p, 0.035f + 0.02f * hash01(k, i + 1), 4, 5,
                             (Color){ (unsigned char)(70 * pulse), (unsigned char)(220 * pulse), (unsigned char)(196 * pulse), 255 });
            }
        } else { // musgo: dos manchas verdes
            cs_blob((Vector3){ at.x, at.y + 0.02f, at.z }, d->size * 0.5f, i * 5, (Color){ 64, 92, 46, 255 }, (Color){ 58, 78, 48, 255 }, true);
            cs_blob((Vector3){ at.x + d->size * 0.12f, at.y + 0.026f, at.z - d->size * 0.1f }, d->size * 0.28f, i * 5 + 1, (Color){ 84, 116, 58, 255 },
                 (Color){ 70, 98, 52, 255 }, true);
        }
        break;
    case CDEC_BEDROLL: { // el colchon, la manta encima y la cabecera (enrollada)
        bool felt = kind == CAVE_TRIBE;
        static const Color TRIBE_BLANKET[4] = { { 146, 50, 38, 255 }, { 48, 66, 112, 255 }, { 178, 128, 60, 255 }, { 120, 44, 52, 255 } };
        static const Color BANDIT_BLANKET[4] = { { 56, 52, 50, 255 }, { 80, 46, 34, 255 }, { 60, 64, 58, 255 }, { 48, 46, 58, 255 } };
        Color mat = felt ? (Color){ 186, 172, 148, 255 } : (Color){ 132, 112, 74, 255 }, blanket = felt ? TRIBE_BLANKET[d->variant] : BANDIT_BLANKET[d->variant];
        box_at(at, d->yaw, 0.0f, 0.08f, 0.0f, d->size, 0.16f, 0.8f, mat);
        box_at(at, d->yaw, -d->size * 0.12f, 0.19f, 0.0f, d->size * 0.7f, 0.07f, 0.88f, blanket);
        box_at(at, d->yaw, -d->size * 0.12f, 0.12f, 0.43f, d->size * 0.7f, 0.12f, 0.04f, blanket); // el borde que cae a un costado
        float fx = sinf(d->yaw), fz = cosf(d->yaw), hd = d->size * 0.5f - 0.16f;
        Vector3 hp = { at.x + fx * hd, at.y + 0.17f, at.z + fz * hd };
        DrawCylinderEx((Vector3){ hp.x - fz * 0.34f, hp.y, hp.z + fx * 0.34f }, (Vector3){ hp.x + fz * 0.34f, hp.y, hp.z - fx * 0.34f }, 0.1f, 0.1f, 6,
                       cs_shade(felt ? (Color){ 150, 110, 70, 255 } : (Color){ 112, 86, 62, 255 }, hp));
        break;
    }
    case CDEC_RUG:
        if (kind == CAVE_TRIBE) { // fieltro con su guarda y un rombo al medio
            static const Color BASE[4] = { { 150, 48, 36, 255 }, { 44, 62, 110, 255 }, { 182, 132, 62, 255 }, { 206, 192, 166, 255 } };
            static const Color TRIM[4] = { { 214, 168, 72, 255 }, { 196, 72, 48, 255 }, { 120, 40, 34, 255 }, { 52, 70, 120, 255 } };
            Color b = BASE[d->variant], t = TRIM[d->variant];
            float l = d->size, w = d->size * 0.62f;
            flat_rect(at, d->yaw, l, w, 0.012f, b);
            flat_rect(at, d->yaw, l * 0.86f, w * 0.78f, 0.016f, t);
            flat_rect(at, d->yaw, l * 0.8f, w * 0.7f, 0.02f, b);
            flat_rect(at, d->yaw + PI * 0.25f, w * 0.42f, w * 0.42f, 0.024f, t);
        } else { // una piel tendida
            cs_blob((Vector3){ at.x, at.y + 0.015f, at.z }, d->size * 0.45f, i * 3, (Color){ 128, 92, 60, 255 }, (Color){ 104, 74, 50, 255 }, true);
            cs_blob((Vector3){ at.x - d->size * 0.1f, at.y + 0.02f, at.z + d->size * 0.08f }, d->size * 0.16f, i * 3 + 1, (Color){ 90, 64, 42, 255 },
                 (Color){ 100, 72, 48, 255 }, true);
        }
        break;
    case CDEC_RACK: { // dos postes, el travesaño y las pieles colgadas
        float fx = sinf(d->yaw), fz = cosf(d->yaw), h = d->size;
        Color wood = cs_shade((Color){ 104, 76, 50, 255 }, (Vector3){ at.x, at.y + h * 0.5f, at.z });
        Vector3 l = { at.x - fx * 0.95f, at.y, at.z - fz * 0.95f }, r = { at.x + fx * 0.95f, at.y, at.z + fz * 0.95f };
        DrawCylinderEx(l, (Vector3){ l.x, l.y + h, l.z }, 0.05f, 0.045f, 5, wood);
        DrawCylinderEx(r, (Vector3){ r.x, r.y + h, r.z }, 0.05f, 0.045f, 5, wood);
        DrawCylinderEx((Vector3){ l.x, l.y + h - 0.1f, l.z }, (Vector3){ r.x, r.y + h - 0.1f, r.z }, 0.04f, 0.04f, 5, wood);
        for (int k = 0; k < 2; k++) {
            float off = k ? 0.42f : -0.42f, len = 0.9f + 0.3f * hash01(i, k);
            box_at(at, d->yaw, off, h - 0.12f - len * 0.5f, 0.0f, 0.7f, len, 0.03f, k ? (Color){ 150, 110, 72, 255 } : (Color){ 128, 92, 60, 255 });
        }
        break;
    }
    case CDEC_LAMP: { // la piedra, el cuenco de barro y la llama (que se ve sin luz)
        DrawCylinder(at, 0.26f, 0.3f, 0.14f, 7, cs_shade((Color){ 112, 104, 96, 255 }, at));
        Vector3 b = { at.x, at.y + 0.14f, at.z };
        DrawCylinderEx(b, (Vector3){ b.x, b.y + 0.07f, b.z }, 0.08f, 0.12f, 7, cs_shade((Color){ 164, 92, 60, 255 }, b));
        float fl = 0.85f + 0.15f * sinf(time * 15.0f + (float)i * 1.3f);
        DrawSphereEx((Vector3){ b.x + 0.06f, b.y + 0.12f, b.z }, 0.045f * fl, 4, 5, (Color){ 255, 170, 70, 255 });
        DrawSphereEx((Vector3){ b.x + 0.06f, b.y + 0.14f, b.z }, 0.022f * fl, 4, 5, (Color){ 255, 236, 180, 255 });
        break;
    }
    case CDEC_POTS: { // vasijas: las del imperio, tinajas altas con su tapa; las demas, ollas con una franja
        int n = kind == CAVE_EMPIRE ? 2 + (d->variant & 1) : 2 + d->variant % 3;
        for (int k = 0; k < n; k++) {
            float a = (float)k * 2.0f * PI / (float)n + d->yaw, r = n > 1 ? d->size * 0.28f : 0.0f;
            Vector3 p = { at.x + sinf(a) * r, at.y, at.z + cosf(a) * r };
            float h = kind == CAVE_EMPIRE ? 0.8f + 0.2f * hash01(i, k) : 0.35f + 0.25f * hash01(i, k), w = kind == CAVE_EMPIRE ? 0.26f : 0.16f + 0.08f * hash01(k, i);
            Color clay = cs_shade(kind == CAVE_EMPIRE ? (Color){ 118, 98, 84, 255 } : (Color){ 140, 82, 58, 255 }, (Vector3){ p.x, p.y + h * 0.5f, p.z });
            DrawCylinderEx(p, (Vector3){ p.x, p.y + h * 0.18f, p.z }, w * 0.5f, w, 7, clay);
            DrawSphereEx((Vector3){ p.x, p.y + h * 0.48f, p.z }, w * 1.08f, 6, 7, clay);
            DrawCylinderEx((Vector3){ p.x, p.y + h * 0.78f, p.z }, (Vector3){ p.x, p.y + h, p.z }, w * 0.5f, w * 0.55f, 7, clay);
            if (kind == CAVE_EMPIRE) DrawCylinder((Vector3){ p.x, p.y + h, p.z }, w * 0.62f, w * 0.62f, 0.04f, 7, cs_shade((Color){ 120, 96, 70, 255 }, p));
            else DrawCylinder((Vector3){ p.x, p.y + h * 0.5f, p.z }, w * 1.1f, w * 1.1f, 0.05f, 7, cs_shade((Color){ 52, 34, 28, 255 }, p));
        }
        break;
    }
    case CDEC_BRAZIER: { // el tripode, el cuenco de bronce y el fuego (las llamas, con las de los fogones)
        float h = d->size;
        Color bronze = cs_shade((Color){ 104, 78, 46, 255 }, (Vector3){ at.x, at.y + h * 0.4f, at.z });
        for (int k = 0; k < 3; k++) {
            float a = (float)k * 2.094f + d->yaw;
            DrawCylinderEx((Vector3){ at.x + sinf(a) * 0.34f, at.y, at.z + cosf(a) * 0.34f }, (Vector3){ at.x + sinf(a) * 0.16f, at.y + h - 0.18f, at.z + cosf(a) * 0.16f },
                           0.03f, 0.03f, 4, bronze);
        }
        DrawCylinderEx((Vector3){ at.x, at.y + h - 0.22f, at.z }, (Vector3){ at.x, at.y + h, at.z }, 0.22f, 0.4f, 9, bronze);
        DrawCylinder((Vector3){ at.x, at.y + h - 0.02f, at.z }, 0.34f, 0.34f, 0.02f, 9, (Color){ 196, 74, 30, 255 }); // las brasas
        hearth_draw_base((Vector3){ at.x, at.y + h - 0.02f, at.z }, 0.5f, 700u + (uint32_t)i, true);
        break;
    }
    case CDEC_PILASTER: { // contra la esquina, de sillares como la pared: el plinto, el fuste y el capitel
        const CaveRoom *r = &c->rooms[d->room];
        float a = d->yaw, wr = cave_wall_radius(c, d->room, a), h = d->size;
        Color stone = ColorBrightness(cl_look()->wall, 0.05f), dark = ColorBrightness(cl_look()->wall, -0.12f);
        tex_box(r->x, r->z, d->y, a, wr - 0.25f, 0.5f, 1.0f, 0.0f, 0.6f, dark);
        int segs = h > 8.0f ? 4 : 2;
        for (int k = 0; k < segs; k++)
            tex_box(r->x, r->z, d->y, a, wr - 0.2f, 0.4f, 0.72f, 0.6f + (h - 1.2f) * (float)k / (float)segs, 0.6f + (h - 1.2f) * (float)(k + 1) / (float)segs, stone);
        tex_box(r->x, r->z, d->y, a, wr - 0.27f, 0.54f, 1.06f, h - 0.6f, h, dark);
        break;
    }
    default: break; // la luz del agujero va aparte (draw_shafts: transparente, al final)
    }
    (void)world_time;
}

// La luz del agujero de la boveda: el agujero (el cielo), el haz con su polvo y el charco de luz en el suelo. Se suma a lo
// que ya esta dibujado, sin escribir la profundidad.
static void draw_shafts(const Cave *c, float world_time, float time) {
    float day = clock_light(world_time);
    Color sky = { (unsigned char)(30.0f + 200.0f * day), (unsigned char)(38.0f + 196.0f * day), (unsigned char)(60.0f + 176.0f * day), 255 };
    for (int i = 0; i < g_ndecor; i++) {
        const CaveDecor *d = &g_decor[i];
        if (d->kind != CDEC_SHAFT || Vector3Distance(g_eye, (Vector3){ d->x, d->y, d->z }) > CS_VIEW) continue;
        float top = cave_rock_above(c, d->x, d->z, d->room, d->y), r = d->size * 0.5f;
        DrawCylinder((Vector3){ d->x, top - 0.08f, d->z }, r * 0.5f, r * 0.5f, 0.06f, 10, sky); // el agujero
        rlDrawRenderBatchActive();
        rlDisableDepthMask();
        BeginBlendMode(BLEND_ADDITIVE);
        float al = 6.0f + 26.0f * day; // el haz: tres planos cruzados, mas claros arriba
        rlBegin(RL_TRIANGLES);
        for (int k = 0; k < 3; k++) {
            float a = (float)k * PI / 3.0f + 0.3f, sx = sinf(a), sz = cosf(a), rt = r * 0.45f, rb = r * 0.9f;
            Color ct = { 255, 242, 216, (unsigned char)al }, cb = { 255, 236, 200, (unsigned char)(al * 0.35f) };
            Vector3 t0 = { d->x - sx * rt, top - 0.1f, d->z - sz * rt }, t1 = { d->x + sx * rt, top - 0.1f, d->z + sz * rt };
            Vector3 b0 = { d->x - sx * rb, d->y + 0.05f, d->z - sz * rb }, b1 = { d->x + sx * rb, d->y + 0.05f, d->z + sz * rb };
            vtx(t0, ct), vtx(b0, cb), vtx(b1, cb);
            vtx(t0, ct), vtx(b1, cb), vtx(t1, ct);
        }
        rlEnd();
        cs_blob((Vector3){ d->x, d->y + 0.05f, d->z }, r * 1.05f, i * 11, (Color){ 255, 240, 210, (unsigned char)(al * 1.6f) }, (Color){ 255, 240, 210, 0 }, false);
        for (int k = 0; k < 14; k++) { // el polvo que baja despacio por el haz
            float u = fmodf(hash01(i, k) + time * (0.012f + 0.01f * hash01(k, i)), 1.0f), a = hash01(i, k + 30) * 2.0f * PI + time * 0.2f;
            float rr = r * 0.7f * hash01(k + 30, i) * (0.5f + u * 0.5f);
            Vector3 p = { d->x + sinf(a) * rr, top - (top - d->y) * u, d->z + cosf(a) * rr };
            DrawCube(p, 0.03f, 0.03f, 0.03f, (Color){ 255, 236, 200, (unsigned char)fminf(255.0f, al * 3.0f) });
        }
        EndBlendMode();
        rlDrawRenderBatchActive();
        rlEnableDepthMask();
    }
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
    float time = (float)GetTime();
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
        g_mat.maps[MATERIAL_MAP_DIFFUSE].texture = cl_texture((ClSurface)ch->surface);
        DrawMesh(*m, g_mat, MatrixIdentity());
        g_stats.drawn_chunks++;
    }
    draw_mouth(world_time);
    // El decorado: modelos de voxeles teñidos con la luz de donde estan, y figuras simples con la luz en cada punto.
    for (int i = 0; i < g_ndecor; i++) {
        const CaveDecor *d = &g_decor[i];
        if (Vector3Distance(eye, (Vector3){ d->x, d->y, d->z }) > CS_VIEW) continue;
        if (decor_model(d->kind) != VC_KINDS) {
            rlDrawRenderBatchActive();
            rlEnableBackfaceCulling();
        }
        draw_decor(c, d, i, world_time, time);
        if (decor_model(d->kind) != VC_KINDS) {
            rlDrawRenderBatchActive();
            rlDisableBackfaceCulling();
        }
    }
    rlDrawRenderBatchActive();
    draw_shafts(c, world_time, time);
    rlEnableBackfaceCulling();
}
