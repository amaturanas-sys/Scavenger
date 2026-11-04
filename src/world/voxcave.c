#include "world/voxcave.h"

#include <math.h>
#include <string.h>

#include "sim/voxel.h"
#include "sim/voxmat.h"
#include "world/voxstruct.h"

// El alto de cada modelo (m): el de su rejilla. vc_draw lo escala al size pedido.
static const float KIND_HEIGHT[VC_KINDS] = { 2.0f, 1.6f, 1.08f };

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

// La paleta de adentro: la roca de la region (y su tono oscuro, humedo) y la calcita de las formaciones (clara).
static void cave_palette(Region r, uint8_t pal[M_COUNT][4]) {
    voxs_palette(r, pal);
    Color rock = vc_rock_color(r);
    mix_into(pal[M_STONE], rock, rock, 0.0f);
    mix_into(pal[M_STONE_DARK], rock, (Color){ 20, 18, 16, 255 }, 0.3f);
    mix_into(pal[M_CHALK], rock, (Color){ 236, 232, 222, 255 }, 0.55f);
    mix_into(pal[M_CHALK_D], rock, (Color){ 210, 204, 192, 255 }, 0.3f);
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
    float s = size / KIND_HEIGHT[k];
    Vector3 p = { at.x, k == VC_STALACTITE ? at.y - size : at.y, at.z };
    DrawModelEx(g_vc[k][r][variant].model, p, (Vector3){ 0, 1, 0 }, yaw * RAD2DEG, (Vector3){ s, s, s }, tint);
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

void vc_unload(void) {
    for (int k = 0; k < VC_KINDS; k++)
        for (int r = 0; r < REGION_COUNT; r++)
            for (int v = 0; v < VC_VARIANTS; v++)
                if (g_vc[k][r][v].ok) UnloadModel(g_vc[k][r][v].model);
    memset(g_vc, 0, sizeof(g_vc));
}

void vc_forget_gpu(void) { memset(g_vc, 0, sizeof(g_vc)); }
