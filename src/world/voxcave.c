#include "world/voxcave.h"

#include <math.h>
#include <string.h>

#include "sim/voxel.h"
#include "sim/voxmat.h"
#include "world/voxstruct.h"

// Lo que mide cada modelo (m), el alto y el ancho de su rejilla. vc_draw lo escala parejo al size pedido: su alto, o su
// ancho en el derrumbe (lo que se mide de el es lo que ocupa).
static const float KIND_HEIGHT[VC_KINDS] = { 2.0f, 1.6f, 1.08f, 2.64f, 2.28f, 6.0f, 0.96f, 1.4f, 0.9f, 3.38f };
static const float KIND_WIDTH[VC_KINDS] = { 0.9f, 0.9f, 1.44f, 1.68f, 1.68f, 1.8f, 2.16f, 1.6f, 1.6f, 1.17f };
static Color g_rock, g_chalk, g_moss;
static bool g_rock_set, g_chalk_set, g_moss_set;

static struct {
    bool tried, ok;
    Model model;
    int tris;
} g_vc[VC_KINDS][REGION_COUNT][VC_VARIANTS];

Color vc_rock_color(Region r) {
    switch (r) {
    case REGION_DESERT: return (Color){ 168, 124, 88, 255 };
    case REGION_HIGHLAND: return (Color){ 126, 128, 134, 255 };
    case REGION_FJORD: return (Color){ 104, 110, 116, 255 };
    case REGION_FOREST: return (Color){ 112, 104, 92, 255 };
    default: return (Color){ 132, 118, 100, 255 };
    }
}

static void mix_into(uint8_t out[4], Color a, Color b, float t) {
    out[0] = (uint8_t)((float)a.r + ((float)b.r - (float)a.r) * t);
    out[1] = (uint8_t)((float)a.g + ((float)b.g - (float)a.g) * t);
    out[2] = (uint8_t)((float)a.b + ((float)b.b - (float)a.b) * t);
    out[3] = 255;
}

void vc_set_rock(Color rock) { g_rock = rock, g_rock_set = true; }
void vc_set_growth(Color chalk, Color moss) { g_chalk = chalk, g_chalk_set = chalk.a > 0, g_moss = moss, g_moss_set = moss.a > 0; }

// La paleta de adentro: la roca de la cueva (y su tono oscuro, humedo) y la calcita de las formaciones (clara).
static void cave_palette(Region r, uint8_t pal[M_COUNT][4]) {
    voxs_palette(r, pal);
    Color rock = g_rock_set ? g_rock : vc_rock_color(r);
    mix_into(pal[M_STONE], rock, rock, 0.0f);
    mix_into(pal[M_STONE_DARK], rock, (Color){ 20, 18, 16, 255 }, 0.3f);
    Color chalk = g_chalk_set ? g_chalk : (Color){ 236, 232, 222, 255 }, chalk_d = g_chalk_set ? ColorBrightness(g_chalk, -0.1f) : (Color){ 210, 204, 192, 255 };
    mix_into(pal[M_CHALK], rock, chalk, 0.55f);
    mix_into(pal[M_CHALK_D], rock, chalk_d, 0.3f);
    if (g_moss_set) mix_into(pal[M_MOSS], g_moss, g_moss, 0.0f);
}

// El musgo (las cuevas abandonadas, vc_set_growth): en manchas sobre la cara de arriba de cada columna de la rejilla.
static void moss_tops(VoxGrid *g, uint32_t seed) {
    if (!g_moss_set) return;
    for (int z = 0; z < g->nz; z++)
        for (int x = 0; x < g->nx; x++)
            for (int y = g->ny - 1; y >= 0; y--) {
                if (!vox_get(g, x, y, z)) continue;
                if (vox_noise3((float)x * 0.45f, 0.0f, (float)z * 0.45f, seed + 77u) > 0.38f) vox_set(g, x, y, z, M_MOSS);
                break;
            }
}

// La calcita, con vetas verticales mas oscuras (por donde corre el agua); la roca de donde nace, abajo (o arriba).
static void calcite(VoxGrid *g, uint32_t seed, int rock_from, int rock_to) {
    for (int y = 0; y < g->ny; y++)
        for (int z = 0; z < g->nz; z++)
            for (int x = 0; x < g->nx; x++) {
                if (!vox_get(g, x, y, z)) continue;
                uint8_t m = vox_noise3((float)x * 0.9f, (float)y * 0.15f, (float)z * 0.9f, seed) > 0.6f ? M_CHALK_D : M_CHALK;
                if (y >= rock_from && y <= rock_to) m = M_STONE;
                vox_set(g, x, y, z, m);
            }
}

static bool build_kind(VoxCaveKind k, int v, VoxGrid *g) {
    uint32_t seed = 5100u + (uint32_t)k * 97u + (uint32_t)v * 7u, rs = seed;
    switch (k) {
    case VC_STALACTITE: { // colgada: lo ancho arriba (donde nace de la boveda), la punta abajo, y una o dos gotas al costado
        if (!vox_init(g, 9, 20, 9, 0.1f)) return false;
        float top = 3.0f + voxs_rand(&rs) * 0.9f;
        vox_cylinder(g, 4.5f, 4.5f, 0, 19, 0.5f, top, M_CHALK);
        int drips = 1 + (int)(voxs_rand(&rs) * 2.0f);
        for (int i = 0; i < drips; i++) {
            float a = voxs_rand(&rs) * 6.283f, d = 1.6f + voxs_rand(&rs);
            vox_cylinder(g, 4.5f + sinf(a) * d, 4.5f + cosf(a) * d, 9 + (int)(voxs_rand(&rs) * 5.0f), 19, 0.4f, 1.5f, M_CHALK);
        }
        for (int y = 8; y < 18; y += 3) vox_ellipsoid(g, 4.5f + voxs_rand(&rs) - 0.5f, (float)y, 4.5f, 0.9f + top * (float)y / 22.0f, 0.8f, 1.0f + top * (float)y / 24.0f, M_CHALK, false);
        voxs_erode_skin(g, seed, 0.6f, 0.16f);
        calcite(g, seed, 18, 19);
        return true;
    }
    case VC_STALAGMITE: { // del suelo: el cono con sus anillos de colada y el monticulo del pie
        if (!vox_init(g, 9, 16, 9, 0.1f)) return false;
        float base = 3.2f + voxs_rand(&rs) * 0.8f;
        vox_cylinder(g, 4.5f, 4.5f, 0, 15, base, 0.5f, M_CHALK);
        for (int y = 2; y < 13; y += 3) {
            float r = base * (1.0f - (float)y / 16.0f) + 0.6f;
            vox_ellipsoid(g, 4.5f, (float)y, 4.5f, r, 0.7f, r, M_CHALK, false);
        }
        vox_ellipsoid(g, 4.5f, 0.0f, 4.5f, 4.4f, 1.6f, 4.4f, M_CHALK, true);
        voxs_erode_skin(g, seed, 0.6f, 0.16f);
        calcite(g, seed, 0, 0);
        return true;
    }
    case VC_ROCK: { // un canto caido: dos lobulos de roca, la base plana, gastado
        if (!vox_init(g, 12, 9, 12, 0.12f)) return false;
        vox_ellipsoid(g, 6.0f, 0.0f, 6.0f, 5.0f + voxs_rand(&rs) * 0.6f, 7.5f + voxs_rand(&rs), 4.4f + voxs_rand(&rs) * 0.8f, M_STONE, true);
        vox_ellipsoid(g, 3.5f + voxs_rand(&rs) * 5.0f, 0.0f, 4.0f + voxs_rand(&rs) * 4.0f, 3.0f, 4.5f + voxs_rand(&rs) * 1.5f, 3.0f, M_STONE, true);
        voxs_erode_skin(g, seed, 0.45f, 0.26f);
        for (int y = 0; y < g->ny; y++)
            for (int z = 0; z < g->nz; z++)
                for (int x = 0; x < g->nx; x++)
                    if (vox_get(g, x, y, z) && vox_noise3((float)x * 0.7f, (float)y * 0.7f, (float)z * 0.7f, seed) > 0.62f) vox_set(g, x, y, z, M_STONE_DARK);
        moss_tops(g, seed);
        return true;
    }
    case VC_STALACTITES: { // la placa de la boveda y de ella, las estalactitas: una larga y otras mas cortas alrededor
        if (!vox_init(g, 14, 22, 14, 0.12f)) return false;
        vox_ellipsoid(g, 7.0f, 21.5f, 7.0f, 6.2f, 2.0f, 5.4f, M_STONE, false);
        int n = 4 + (int)(voxs_rand(&rs) * 2.0f);
        for (int i = 0; i < n; i++) {
            float a = voxs_rand(&rs) * 6.283f, d = i == 0 ? 0.0f : 1.9f + voxs_rand(&rs) * 3.0f;
            int len = i == 0 ? 18 + (int)(voxs_rand(&rs) * 3.0f) : 5 + (int)(voxs_rand(&rs) * 11.0f);
            vox_cylinder(g, 7.0f + sinf(a) * d, 7.0f + cosf(a) * d, 21 - len, 21, 0.4f, 0.9f + (float)len * 0.07f + voxs_rand(&rs) * 0.4f, M_CHALK);
        }
        voxs_erode_skin(g, seed, 0.6f, 0.12f);
        calcite(g, seed, 20, 21);
        return true;
    }
    case VC_STALAGMITES: { // la colada del pie y los conos, con sus anillos
        if (!vox_init(g, 14, 19, 14, 0.12f)) return false;
        vox_ellipsoid(g, 7.0f, 0.0f, 7.0f, 6.0f, 1.8f, 5.5f, M_CHALK, true);
        int n = 3 + (int)(voxs_rand(&rs) * 3.0f);
        for (int i = 0; i < n; i++) {
            float a = voxs_rand(&rs) * 6.283f, d = i == 0 ? 0.0f : 2.0f + voxs_rand(&rs) * 2.7f, cx = 7.0f + sinf(a) * d, cz = 7.0f + cosf(a) * d;
            int h = i == 0 ? 16 + (int)(voxs_rand(&rs) * 3.0f) : 4 + (int)(voxs_rand(&rs) * 9.0f);
            float base = 1.0f + (float)h * 0.08f + voxs_rand(&rs) * 0.4f;
            vox_cylinder(g, cx, cz, 0, h, base, 0.4f, M_CHALK);
            for (int y = 2; y < h - 2; y += 3) {
                float r = base * (1.0f - (float)y / (float)h) + 0.5f;
                vox_ellipsoid(g, cx, (float)y, cz, r, 0.6f, r, M_CHALK, false);
            }
        }
        voxs_erode_skin(g, seed, 0.6f, 0.12f);
        calcite(g, seed, 0, 0);
        return true;
    }
    case VC_PILLAR: { // una estalactita y una estalagmita que se juntaron: anchas en los extremos, angosta mas arriba del medio
        if (!vox_init(g, 12, 40, 12, 0.15f)) return false;
        float ph = voxs_rand(&rs) * 6.283f, waist = 0.5f + voxs_rand(&rs) * 0.12f;
        for (int y = 0; y < 40; y++) {
            float t = (float)y / 39.0f, r = 2.3f + 2.7f * powf(fabsf(t - waist) * 1.9f, 1.6f) + 0.45f * sinf(t * 23.0f + ph);
            r = fminf(fmaxf(r, 1.8f), 5.8f);
            vox_ellipsoid(g, 6.0f + 0.4f * sinf(t * 5.0f + ph), (float)y, 6.0f + 0.4f * cosf(t * 4.0f + ph), r, 0.6f, r * 0.92f, M_CHALK, false);
        }
        voxs_erode_skin(g, seed, 0.7f, 0.12f);
        calcite(g, seed, 39, 39);
        return true;
    }
    case VC_RUBBLE: { // la grava y los bloques caidos: sillares claros (lo que fue labrado) entre la roca
        if (!vox_init(g, 18, 8, 18, 0.12f)) return false;
        vox_ellipsoid(g, 9.0f, 0.0f, 9.0f, 8.0f, 2.0f, 7.5f, M_STONE_DARK, true);
        int n = 9 + (int)(voxs_rand(&rs) * 5.0f);
        for (int i = 0; i < n; i++) {
            float a = voxs_rand(&rs) * 6.283f, d = voxs_rand(&rs) * 6.2f;
            int sx = 2 + (int)(voxs_rand(&rs) * 2.5f), sy = 1 + (int)(voxs_rand(&rs) * 2.0f), sz = 2 + (int)(voxs_rand(&rs) * 2.5f);
            int x0 = 9 + (int)(sinf(a) * d) - sx / 2, z0 = 9 + (int)(cosf(a) * d) - sz / 2;
            int y0 = (int)((1.0f - d / 6.6f) * 4.0f * voxs_rand(&rs));
            uint8_t m = (v & 1) || voxs_rand(&rs) < 0.35f ? M_CHALK_D : (i & 1) ? M_STONE : M_STONE_DARK;
            vox_box(g, x0, y0, z0, x0 + sx - 1, y0 + sy - 1, z0 + sz - 1, m);
        }
        voxs_erode_skin(g, seed, 0.5f, 0.14f);
        moss_tops(g, seed);
        return true;
    }
    case VC_CRATES: { // dos cajas en el suelo (o una y un tonel) y una arriba; con las tablas y los cantos oscuros
        if (!vox_init(g, 16, 14, 12, 0.1f)) return false;
        static const int CR[3][6] = { { 0, 0, 0, 7, 6, 6 }, { 8, 0, 1, 6, 5, 6 }, { 2, 6, 1, 5, 5, 5 } };
        for (int i = 0; i < 3; i++) {
            if ((i == 1 && v == 1) || (i == 2 && v == 3)) continue;
            int x0 = CR[i][0], y0 = CR[i][1], z0 = CR[i][2], w = CR[i][3], h = CR[i][4], d = CR[i][5];
            vox_box(g, x0, y0, z0, x0 + w - 1, y0 + h - 1, z0 + d - 1, M_WOOD);
            for (int y = y0; y < y0 + h; y++)
                for (int z = z0; z < z0 + d; z++)
                    for (int x = x0; x < x0 + w; x++) {
                        int ex = x == x0 || x == x0 + w - 1, ey = y == y0 || y == y0 + h - 1, ez = z == z0 || z == z0 + d - 1;
                        if (ex + ey + ez >= 2 || (y == y0 + h / 2 && (ex || ez))) vox_set(g, x, y, z, M_WOOD_DARK);
                    }
        }
        if (v == 1) { // el tonel, con sus aros
            vox_cylinder(g, 11.5f, 4.5f, 0, 7, 2.6f, 2.6f, M_WOOD_DARK);
            vox_cylinder(g, 11.5f, 4.5f, 1, 1, 2.75f, 2.75f, M_SLAG);
            vox_cylinder(g, 11.5f, 4.5f, 6, 6, 2.75f, 2.75f, M_SLAG);
        }
        if (v == 3) vox_erode(g, seed, 0.25f); // rotas (las de las abandonadas)
        return true;
    }
    case VC_SACKS: { // sacos de arpillera parados y uno acostado encima, atados
        if (!vox_init(g, 16, 9, 12, 0.1f)) return false;
        int n = 3 + (int)(voxs_rand(&rs) * 2.0f);
        uint8_t cloth = v == 2 ? M_FELT_DARK : M_THATCH;
        for (int i = 0; i < n; i++) {
            float cx = 3.0f + (float)(i % 3) * 4.6f + voxs_rand(&rs), cz = 3.5f + (float)(i / 3) * 4.5f + voxs_rand(&rs);
            vox_ellipsoid(g, cx, 0.0f, cz, 2.5f, 6.2f + voxs_rand(&rs), 2.3f, cloth, true);
            vox_cylinder(g, cx, cz, 6, 7, 0.9f, 0.7f, M_REED);
        }
        if (v != 3) vox_ellipsoid(g, 8.0f, 6.5f, 6.0f, 4.6f, 2.0f, 2.3f, cloth, false);
        return true;
    }
    case VC_STELE: { // la tortuga (el caparazon, la cabeza, las patas) y la losa con su remate y su inscripcion al frente (+z)
        if (!vox_init(g, 9, 26, 9, 0.13f)) return false;
        vox_ellipsoid(g, 4.5f, 0.0f, 4.2f, 4.3f, 3.2f, 4.2f, M_STONE_DARK, true);
        vox_ellipsoid(g, 4.5f, 1.5f, 8.0f, 1.2f, 1.0f, 1.0f, M_STONE_DARK, false);
        for (int k = 0; k < 4; k++) vox_box(g, k & 1 ? 7 : 1, 0, k & 2 ? 6 : 1, k & 1 ? 7 : 1, 0, k & 2 ? 6 : 1, M_STONE_DARK);
        vox_box(g, 1, 3, 4, 7, 22, 5, M_STONE);
        vox_ellipsoid(g, 4.5f, 22.0f, 4.5f, 3.4f, 3.2f, 1.2f, M_STONE, false);
        vox_carve(g, 2, 7, 5, 6, 20, 5); // el panel, rehundido, con los renglones de caracteres (el fondo, oscuro)
        for (int y = 8; y < 20; y += 2) vox_box(g, 2, y, 4, 6, y, 4, M_STONE_DARK);
        return true;
    }
    default: return false;
    }
}

bool vc_preload(VoxCaveKind k, Region r, int variant) {
    if ((unsigned)k >= VC_KINDS) return false;
    if ((unsigned)r >= REGION_COUNT) r = REGION_STEPPE;
    variant &= VC_VARIANTS - 1;
    if (g_vc[k][r][variant].tried) return g_vc[k][r][variant].ok;
    g_vc[k][r][variant].tried = true;
    VoxGrid g;
    if (!build_kind(k, variant, &g)) return false;
    uint8_t pal[M_COUNT][4];
    cave_palette(r, pal);
    Model m;
    bool ok = voxs_grid_model(&g, (const uint8_t(*)[4])pal, &m, false, 18);
    vox_free(&g);
    if (!ok) return false;
    g_vc[k][r][variant].ok = true;
    g_vc[k][r][variant].model = m;
    g_vc[k][r][variant].tris = m.meshCount > 0 ? m.meshes[0].triangleCount : 0;
    return true;
}

void vc_draw(VoxCaveKind k, Region r, int variant, Vector3 at, float yaw, float size, Color tint) {
    if ((unsigned)k >= VC_KINDS) return;
    if ((unsigned)r >= REGION_COUNT) r = REGION_STEPPE;
    variant &= VC_VARIANTS - 1;
    if (!g_vc[k][r][variant].ok) return;
    float s = size / (k == VC_RUBBLE ? KIND_WIDTH[k] : KIND_HEIGHT[k]);
    Vector3 p = { at.x, k == VC_STALACTITE || k == VC_STALACTITES ? at.y - KIND_HEIGHT[k] * s : at.y, at.z };
    DrawModelEx(g_vc[k][r][variant].model, p, (Vector3){ 0, 1, 0 }, yaw * RAD2DEG, (Vector3){ s, s, s }, tint);
}

void vc_draw_scaled(VoxCaveKind k, Region r, int variant, Vector3 at, float yaw, float width, float height, Color tint) {
    if ((unsigned)k >= VC_KINDS) return;
    if ((unsigned)r >= REGION_COUNT) r = REGION_STEPPE;
    variant &= VC_VARIANTS - 1;
    if (!g_vc[k][r][variant].ok) return;
    float sw = width / KIND_WIDTH[k], sh = height / KIND_HEIGHT[k];
    DrawModelEx(g_vc[k][r][variant].model, at, (Vector3){ 0, 1, 0 }, yaw * RAD2DEG, (Vector3){ sw, sh, sw }, tint);
}

int vc_triangles(VoxCaveKind k, Region r, int variant) {
    if ((unsigned)k >= VC_KINDS || (unsigned)r >= REGION_COUNT) return 0;
    variant &= VC_VARIANTS - 1;
    return g_vc[k][r][variant].ok ? g_vc[k][r][variant].tris : 0;
}

int vc_loaded(int *tris) {
    int n = 0, t = 0;
    for (int k = 0; k < VC_KINDS; k++)
        for (int r = 0; r < REGION_COUNT; r++)
            for (int v = 0; v < VC_VARIANTS; v++)
                if (g_vc[k][r][v].ok) n++, t += g_vc[k][r][v].tris;
    if (tris) *tris = t;
    return n;
}

int vc_largest(void) {
    int big = 0;
    for (int k = 0; k < VC_KINDS; k++)
        for (int r = 0; r < REGION_COUNT; r++)
            for (int v = 0; v < VC_VARIANTS; v++)
                if (g_vc[k][r][v].ok && g_vc[k][r][v].tris > big) big = g_vc[k][r][v].tris;
    return big;
}

void vc_unload(void) {
    for (int k = 0; k < VC_KINDS; k++)
        for (int r = 0; r < REGION_COUNT; r++)
            for (int v = 0; v < VC_VARIANTS; v++)
                if (g_vc[k][r][v].ok) UnloadModel(g_vc[k][r][v].model);
    memset(g_vc, 0, sizeof(g_vc));
    g_rock_set = g_chalk_set = g_moss_set = false;
}

void vc_forget_gpu(void) { memset(g_vc, 0, sizeof(g_vc)), g_rock_set = g_chalk_set = g_moss_set = false; }
