#include "voxstruct.h"

#include <math.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"
#include "sim/voxel.h"

// Materiales (indice de la paleta).
enum {
    M_NONE, M_STONE, M_STONE_DARK, M_EARTH, M_WOOD, M_WOOD_DARK, M_FELT, M_FELT_ROOF, M_GRASS, M_ADOBE, M_TURF,
    M_WATER, M_RED, M_BLUE, M_YELLOW, M_CARVE, M_STONE_LICHEN, M_COUNT
};

static const uint8_t PAL_BASE[M_COUNT][4] = {
    [M_NONE] = { 0, 0, 0, 0 },           [M_STONE] = { 138, 134, 126, 255 },  [M_STONE_DARK] = { 96, 92, 86, 255 },
    [M_EARTH] = { 132, 116, 80, 255 },   [M_WOOD] = { 118, 86, 56, 255 },     [M_WOOD_DARK] = { 80, 60, 42, 255 },
    [M_FELT] = { 232, 226, 210, 255 },   [M_FELT_ROOF] = { 210, 200, 178, 255 }, [M_GRASS] = { 112, 128, 70, 255 },
    [M_ADOBE] = { 196, 160, 112, 255 },  [M_TURF] = { 96, 120, 74, 255 },     [M_WATER] = { 52, 92, 128, 255 },
    [M_RED] = { 168, 46, 38, 255 },      [M_BLUE] = { 52, 96, 176, 255 },     [M_YELLOW] = { 222, 186, 60, 255 },
    [M_CARVE] = { 214, 200, 164, 255 },  [M_STONE_LICHEN] = { 120, 128, 104, 255 },
};

// El desierto: piedra arenisca rojiza; la costa: piedra con liquen.
static void palette_for(Region r, uint8_t out[M_COUNT][4]) {
    memcpy(out, PAL_BASE, sizeof(PAL_BASE));
    if (r == REGION_DESERT) {
        memcpy(out[M_STONE], (uint8_t[4]){ 190, 150, 104, 255 }, 4);
        memcpy(out[M_STONE_DARK], (uint8_t[4]){ 160, 112, 76, 255 }, 4);
        memcpy(out[M_EARTH], (uint8_t[4]){ 206, 176, 124, 255 }, 4);
        memcpy(out[M_GRASS], (uint8_t[4]){ 196, 170, 118, 255 }, 4);
    } else if (r == REGION_FJORD || r == REGION_FOREST) {
        memcpy(out[M_STONE_DARK], PAL_BASE[M_STONE_LICHEN], 4);
        memcpy(out[M_GRASS], (uint8_t[4]){ 92, 122, 82, 255 }, 4);
    } else if (r == REGION_HIGHLAND) {
        memcpy(out[M_GRASS], (uint8_t[4]){ 128, 130, 100, 255 }, 4);
    }
}

// ------------------------------------------------------------------ armado de cada modelo
// Cimiento: unas capas bajo el suelo para que no quede en el aire en una ladera.
#define FOUND 3

static void stones_mixed(VoxGrid *g, uint32_t seed) { // piedras de dos tonos
    for (int i = 0; i < g->nx * g->ny * g->nz; i++)
        if (g->m[i] == M_STONE && vox_noise3((float)(i % g->nx) * 0.7f, (float)(i / (g->nx * g->nz)) * 0.7f, (float)((i / g->nx) % g->nz) * 0.7f, seed) > 0.62f)
            g->m[i] = M_STONE_DARK;
}

static bool build_site(SiteKind k, VoxGrid *g, uint32_t seed) {
    switch (k) {
    case SITE_KURGAN: // tumulo de tierra con hierba y anillo de piedras
        if (!vox_init(g, 48, FOUND + 9, 48, 0.5f)) return false;
        vox_box(g, 4, 0, 4, 43, FOUND - 1, 43, M_EARTH);
        vox_ellipsoid(g, 24, FOUND, 24, 18, 8, 18, M_EARTH, true);
        for (int i = 0; i < g->nx * g->ny * g->nz; i++) // la piel de hierba
            if (g->m[i] == M_EARTH && (i / (g->nx * g->nz)) >= FOUND) g->m[i] = M_GRASS;
        vox_ellipsoid(g, 24, FOUND, 24, 17, 7, 17, M_EARTH, true);
        for (int i = 0; i < 14; i++) {
            float a = (float)i / 14.0f * 2.0f * PI;
            int x = 24 + (int)(cosf(a) * 21.0f), z = 24 + (int)(sinf(a) * 21.0f);
            vox_box(g, x, FOUND - 1, z, x + 1, FOUND + 1, z + 1, i % 3 ? M_STONE : M_STONE_DARK);
        }
        return true;
    case SITE_BALBALS: // hilera de estelas con rostro grabado
        if (!vox_init(g, 52, FOUND + 9, 6, 0.25f)) return false;
        for (int i = 0; i < 6; i++) {
            int x = 2 + i * 8, h = FOUND + 5 + (i * 5) % 4;
            vox_box(g, x, 0, 2, x + 2, h, 3, M_STONE);
            vox_set(g, x + 1, h - 1, 4, M_CARVE), vox_set(g, x, h - 2, 4, M_CARVE), vox_set(g, x + 2, h - 2, 4, M_CARVE);
            vox_set(g, x + 1, h - 3, 4, M_CARVE);
        }
        stones_mixed(g, seed);
        return true;
    case SITE_DEER_STONE: // monolito con bandas y ciervos grabados
        if (!vox_init(g, 6, FOUND + 16, 4, 0.25f)) return false;
        vox_box(g, 1, 0, 1, 4, FOUND + 14, 2, M_STONE_DARK);
        for (int y = FOUND + 3; y < FOUND + 14; y += 4) vox_box(g, 1, y, 3, 4, y, 3, M_CARVE);
        vox_set(g, 2, FOUND + 9, 3, M_CARVE), vox_set(g, 3, FOUND + 10, 3, M_CARVE), vox_set(g, 2, FOUND + 5, 3, M_CARVE);
        return true;
    case SITE_RUINED_FORT: // murallas derruidas y torre rota
        if (!vox_init(g, 48, FOUND + 16, 48, 0.5f)) return false;
        vox_box(g, 2, 0, 2, 45, FOUND + 10, 3, M_STONE);
        vox_box(g, 2, 0, 44, 45, FOUND + 10, 45, M_STONE);
        vox_box(g, 2, 0, 2, 3, FOUND + 10, 45, M_STONE);
        vox_box(g, 44, 0, 2, 45, FOUND + 10, 45, M_STONE);
        vox_cylinder(g, 42, 42, 0, FOUND + 15, 5, 4.5f, M_STONE);
        vox_carve(g, 20, FOUND, 0, 27, FOUND + 6, 5); // la puerta
        stones_mixed(g, seed);
        vox_erode(g, seed, 0.85f);
        vox_box(g, 2, 0, 2, 45, FOUND - 1, 3, M_STONE_DARK); // los cimientos quedan
        return true;
    case SITE_BURIED_CITY: // cupulas y muros asomando de la arena
        if (!vox_init(g, 48, FOUND + 10, 40, 0.5f)) return false;
        for (int i = 0; i < 5; i++) {
            int x = 4 + (i % 3) * 15, z = 4 + (i / 3) * 18;
            vox_box(g, x, 0, z, x + 10, FOUND + 3, z + 10, M_STONE);
            if (i % 2 == 0) vox_ellipsoid(g, x + 5.5f, FOUND + 3, z + 5.5f, 5, 5, 5, M_ADOBE, true);
            vox_carve(g, x + 4, FOUND, z, x + 6, FOUND + 2, z + 1);
        }
        vox_erode(g, seed, 0.55f);
        vox_box(g, 0, 0, 0, 47, FOUND, 39, M_EARTH); // la arena las cubre
        return true;
    case SITE_PETROGLYPHS: // peñas con grabados claros
        if (!vox_init(g, 40, FOUND + 12, 20, 0.25f)) return false;
        for (int i = 0; i < 3; i++) {
            float cx = 7 + i * 13, cz = 10, r = 6 + (i % 2) * 2;
            vox_ellipsoid(g, cx, FOUND, cz, r, r * 1.2f, r * 0.8f, M_STONE_DARK, false);
            for (int k = 0; k < 7; k++) vox_set(g, (int)cx - 3 + k, FOUND + 3 + (k * 3) % 4, (int)(cz - r * 0.8f), M_CARVE);
        }
        return true;
    case SITE_CARAVANSERAI: // patio amurallado, portada alta, fuente
        if (!vox_init(g, 50, FOUND + 14, 50, 0.5f)) return false;
        vox_box(g, 1, 0, 1, 48, FOUND + 7, 2, M_STONE);
        vox_box(g, 1, 0, 47, 48, FOUND + 7, 48, M_STONE);
        vox_box(g, 1, 0, 1, 2, FOUND + 7, 48, M_STONE);
        vox_box(g, 47, 0, 1, 48, FOUND + 7, 48, M_STONE);
        vox_box(g, 46, 0, 18, 49, FOUND + 12, 31, M_ADOBE); // portada
        vox_carve(g, 46, FOUND, 21, 49, FOUND + 7, 28);
        for (int i = 0; i < 4; i++) vox_cylinder(g, i % 2 ? 46.5f : 2.5f, i / 2 ? 46.5f : 2.5f, 0, FOUND + 10, 2.5f, 2.0f, M_STONE_DARK);
        vox_box(g, 21, 0, 21, 28, FOUND, 28, M_STONE);
        vox_box(g, 22, FOUND, 22, 27, FOUND, 27, M_WATER);
        stones_mixed(g, seed);
        for (int x = 1; x < 49; x += 2) vox_set(g, x, FOUND + 8, 1, M_STONE_DARK), vox_set(g, x, FOUND + 8, 48, M_STONE_DARK); // almenas
        return true;
    default: return false;
    }
}

static bool build_site2(SiteKind k, Region r, VoxGrid *g, uint32_t seed) {
    switch (k) {
    case SITE_WATCHTOWER:
        if (r == REGION_FOREST || r == REGION_STEPPE) { // de madera: postes, plataforma, techo
            if (!vox_init(g, 16, FOUND + 40, 16, 0.25f)) return false;
            for (int i = 0; i < 4; i++) vox_box(g, i % 2 ? 12 : 2, 0, i / 2 ? 12 : 2, i % 2 ? 13 : 3, FOUND + 30, i / 2 ? 13 : 3, M_WOOD_DARK);
            vox_box(g, 0, FOUND + 30, 0, 15, FOUND + 31, 15, M_WOOD);
            vox_box(g, 0, FOUND + 32, 0, 15, FOUND + 33, 0, M_WOOD), vox_box(g, 0, FOUND + 32, 15, 15, FOUND + 33, 15, M_WOOD);
            for (int y = 0; y < 6; y++) vox_box(g, y, FOUND + 34 + y, y, 15 - y, FOUND + 34 + y, 15 - y, M_WOOD_DARK);
            for (int y = FOUND + 4; y < FOUND + 30; y += 6) vox_box(g, 2, y, 7, 13, y, 8, M_WOOD); // travesaños
        } else { // de piedra, con almenas
            if (!vox_init(g, 12, FOUND + 22, 12, 0.5f)) return false;
            vox_cylinder(g, 6, 6, 0, FOUND + 18, 5, 4.5f, M_STONE);
            vox_cylinder(g, 6, 6, FOUND + 19, FOUND + 19, 5.5f, 5.5f, M_STONE_DARK);
            for (int i = 0; i < 8; i++) {
                float a = (float)i / 8.0f * 2.0f * PI;
                vox_set(g, 6 + (int)(cosf(a) * 5.0f), FOUND + 20, 6 + (int)(sinf(a) * 5.0f), M_STONE_DARK);
            }
            vox_carve(g, 5, FOUND, 0, 6, FOUND + 3, 3);
            stones_mixed(g, seed);
        }
        return true;
    case SITE_OVOO: // cono de piedras, palo y cintas de colores
        if (!vox_init(g, 18, FOUND + 22, 18, 0.25f)) return false;
        vox_cylinder(g, 9, 9, 0, FOUND + 7, 8, 1.5f, M_STONE);
        stones_mixed(g, seed);
        vox_box(g, 9, FOUND + 7, 9, 9, FOUND + 18, 9, M_WOOD_DARK);
        for (int i = 0; i < 3; i++) vox_box(g, 10, FOUND + 17 - i * 2, 9, 13, FOUND + 17 - i * 2, 9, i == 0 ? M_BLUE : i == 1 ? M_FELT : M_YELLOW);
        return true;
    case SITE_WELL: // brocal, agua, postes y travesaño
        if (!vox_init(g, 12, FOUND + 10, 12, 0.25f)) return false;
        vox_cylinder(g, 6, 6, 0, FOUND + 3, 5, 5, M_STONE);
        vox_cylinder(g, 6, 6, FOUND - 1, FOUND + 3, 3.5f, 3.5f, M_NONE);
        vox_cylinder(g, 6, 6, FOUND + 1, FOUND + 1, 3.5f, 3.5f, M_WATER);
        vox_box(g, 1, FOUND + 3, 6, 1, FOUND + 9, 6, M_WOOD_DARK), vox_box(g, 10, FOUND + 3, 6, 10, FOUND + 9, 6, M_WOOD_DARK);
        vox_box(g, 1, FOUND + 9, 6, 10, FOUND + 9, 6, M_WOOD);
        stones_mixed(g, seed);
        return true;
    case SITE_HARBOR: // muelle de tablas y una barca
        if (!vox_init(g, 24, FOUND + 4, 40, 0.5f)) return false;
        vox_box(g, 9, FOUND + 1, 0, 14, FOUND + 1, 33, M_WOOD);
        for (int z = 2; z < 34; z += 6) vox_box(g, 9, 0, z, 9, FOUND + 1, z, M_WOOD_DARK), vox_box(g, 14, 0, z, 14, FOUND + 1, z, M_WOOD_DARK);
        vox_box(g, 17, FOUND, 18, 21, FOUND + 1, 34, M_WOOD_DARK); // la barca
        vox_carve(g, 18, FOUND + 1, 19, 20, FOUND + 1, 33);
        vox_box(g, 19, FOUND + 1, 26, 19, FOUND + 3, 26, M_WOOD);
        return true;
    default: return build_site(k, g, seed);
    }
}

static bool build_building(VoxBuilding b, VoxGrid *g) {
    switch (b) {
    case VB_YURT: // fieltro blanco, techo en cono, puerta de madera pintada
        if (!vox_init(g, 20, FOUND + 12, 20, 0.25f)) return false;
        vox_cylinder(g, 10, 10, 0, FOUND + 6, 9, 9, M_FELT);
        vox_cylinder(g, 10, 10, FOUND + 7, FOUND + 11, 9.5f, 1.5f, M_FELT_ROOF);
        vox_box(g, 9, FOUND + 11, 9, 10, FOUND + 11, 10, M_WOOD_DARK); // la corona
        vox_box(g, 19, FOUND, 8, 19, FOUND + 4, 11, M_RED);             // la puerta (+x)
        for (int y = FOUND + 2; y <= FOUND + 6; y += 4) // las sogas
            for (int i = 0; i < 36; i++) {
                float a = (float)i / 36.0f * 2.0f * PI;
                vox_set(g, 10 + (int)floorf(cosf(a) * 9.0f), y, 10 + (int)floorf(sinf(a) * 9.0f), M_WOOD_DARK);
            }
        return true;
    case VB_LOG_CABIN: // troncos alternados, techo a dos aguas escalonado
        if (!vox_init(g, 18, FOUND + 16, 22, 0.25f)) return false;
        for (int y = 0; y <= FOUND + 8; y++) {
            uint8_t m = (y % 2) ? M_WOOD : M_WOOD_DARK;
            vox_box(g, 1, y, 1, 16, y, 1, m), vox_box(g, 1, y, 20, 16, y, 20, m);
            vox_box(g, 1, y, 1, 1, y, 20, m), vox_box(g, 16, y, 1, 16, y, 20, m);
        }
        for (int s = 0; s < 8; s++) vox_box(g, s, FOUND + 9 + s, 0, 17 - s, FOUND + 9 + s, 21, M_WOOD_DARK);
        vox_carve(g, 16, FOUND, 9, 16, FOUND + 5, 12);
        return true;
    case VB_STONE_HOUSE: // muros de piedra, techo plano de tablas
        if (!vox_init(g, 18, FOUND + 11, 18, 0.25f)) return false;
        vox_box(g, 1, 0, 1, 16, FOUND + 8, 16, M_STONE);
        vox_carve(g, 3, FOUND, 3, 14, FOUND + 8, 14);
        vox_box(g, 0, FOUND + 9, 0, 17, FOUND + 9, 17, M_WOOD_DARK);
        vox_box(g, 2, FOUND + 10, 2, 15, FOUND + 10, 15, M_STONE_DARK); // piedras sobre el techo
        vox_carve(g, 16, FOUND, 7, 16, FOUND + 5, 10);
        stones_mixed(g, 5u);
        return true;
    case VB_LONGHOUSE: // casa larga con techo de turba
        if (!vox_init(g, 22, FOUND + 14, 48, 0.25f)) return false;
        vox_box(g, 2, 0, 1, 19, FOUND + 6, 46, M_WOOD_DARK);
        vox_carve(g, 3, FOUND, 2, 18, FOUND + 6, 45);
        for (int s = 0; s < 10; s++) vox_box(g, s, FOUND + 7 + s, 0, 21 - s, FOUND + 7 + s, 47, s < 9 ? M_TURF : M_WOOD_DARK);
        vox_carve(g, 19, FOUND, 22, 19, FOUND + 5, 25);
        return true;
    case VB_ADOBE: // cubo de adobe y cupula
        if (!vox_init(g, 18, FOUND + 16, 18, 0.25f)) return false;
        vox_box(g, 1, 0, 1, 16, FOUND + 9, 16, M_ADOBE);
        vox_ellipsoid(g, 9, FOUND + 10, 9, 6, 6, 6, M_ADOBE, true);
        vox_carve(g, 16, FOUND, 7, 16, FOUND + 5, 10);
        vox_box(g, 4, FOUND + 6, 16, 5, FOUND + 7, 16, M_WOOD_DARK); // ventanuco
        return true;
    case VB_TOWER_STONE:
        if (!vox_init(g, 8, FOUND + 16, 8, 0.5f)) return false;
        vox_cylinder(g, 4, 4, 0, FOUND + 13, 3.7f, 3.4f, M_STONE);
        for (int i = 0; i < 6; i++) {
            float a = (float)i / 6.0f * 2.0f * PI;
            vox_set(g, 4 + (int)floorf(cosf(a) * 3.4f), FOUND + 14, 4 + (int)floorf(sinf(a) * 3.4f), M_STONE_DARK);
        }
        stones_mixed(g, 9u);
        return true;
    case VB_TOWER_WOOD:
        if (!vox_init(g, 8, FOUND + 16, 8, 0.5f)) return false;
        vox_box(g, 1, 0, 1, 6, FOUND + 12, 6, M_WOOD_DARK);
        vox_box(g, 0, FOUND + 13, 0, 7, FOUND + 13, 7, M_WOOD);
        for (int s = 0; s < 3; s++) vox_box(g, s, FOUND + 14 + s, s, 7 - s, FOUND + 14 + s, 7 - s, M_WOOD_DARK);
        return true;
    default: return false;
    }
}

// ------------------------------------------------------------------ cache de modelos
#define SITE_SLOTS (SITE_KINDS * REGION_COUNT)
static struct {
    bool tried, ok;
    Model model;
    float found_h; // m de cimiento (se dibuja hundido)
} g_sites[SITE_SLOTS], g_builds[VB_KINDS];

static bool to_model(VoxGrid *g, Region r, Model *out) {
    uint8_t pal[M_COUNT][4];
    palette_for(r, pal);
    VoxMesh vm;
    bool ok = vox_mesh(g, (const uint8_t(*)[4])pal, &vm) && vm.tris > 0;
    if (ok) {
        Mesh mesh = { 0 };
        mesh.triangleCount = vm.tris;
        mesh.vertexCount = vm.tris * 3;
        mesh.vertices = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
        mesh.normals = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
        mesh.colors = MemAlloc(mesh.vertexCount * 4);
        memcpy(mesh.vertices, vm.pos, mesh.vertexCount * 3 * sizeof(float));
        memcpy(mesh.normals, vm.nrm, mesh.vertexCount * 3 * sizeof(float));
        memcpy(mesh.colors, vm.col, (size_t)mesh.vertexCount * 4);
        UploadMesh(&mesh, false);
        *out = LoadModelFromMesh(mesh);
    }
    vox_mesh_free(&vm);
    return ok;
}

static void draw_model(Model m, float found_h, Vector3 at, float yaw, float scale, Color tint) {
    Vector3 p = { at.x, at.y - found_h * scale, at.z };
    DrawModelEx(m, p, (Vector3){ 0, 1, 0 }, yaw * RAD2DEG, (Vector3){ scale, scale, scale }, tint);
}

void voxs_draw_site(SiteKind kind, Region region, Vector3 at, float yaw, Color tint) {
    if ((unsigned)kind >= SITE_KINDS || (unsigned)region >= REGION_COUNT) return;
    int i = (int)kind * REGION_COUNT + (int)region;
    if (!g_sites[i].tried) {
        g_sites[i].tried = true;
        VoxGrid g;
        if (build_site2(kind, region, &g, 1000u + (uint32_t)i * 17u)) {
            g_sites[i].ok = to_model(&g, region, &g_sites[i].model);
            g_sites[i].found_h = FOUND * g.size;
            vox_free(&g);
        }
    }
    if (g_sites[i].ok) draw_model(g_sites[i].model, g_sites[i].found_h, at, yaw, 1.0f, tint);
}

void voxs_draw_building(VoxBuilding b, Vector3 at, float yaw, float scale, Color tint) {
    if ((unsigned)b >= VB_KINDS) return;
    if (!g_builds[b].tried) {
        g_builds[b].tried = true;
        VoxGrid g;
        if (build_building(b, &g)) {
            g_builds[b].ok = to_model(&g, REGION_STEPPE, &g_builds[b].model);
            g_builds[b].found_h = FOUND * g.size;
            vox_free(&g);
        }
    }
    if (g_builds[b].ok) draw_model(g_builds[b].model, g_builds[b].found_h, at, yaw, scale, tint);
}

VoxBuilding voxs_region_house(Region r) {
    switch (r) {
    case REGION_FOREST: return VB_LOG_CABIN;
    case REGION_HIGHLAND: return VB_STONE_HOUSE;
    case REGION_FJORD: return VB_LONGHOUSE;
    case REGION_DESERT: return VB_ADOBE;
    default: return VB_YURT;
    }
}

void voxs_unload(void) {
    for (int i = 0; i < SITE_SLOTS; i++)
        if (g_sites[i].ok) UnloadModel(g_sites[i].model);
    for (int i = 0; i < VB_KINDS; i++)
        if (g_builds[i].ok) UnloadModel(g_builds[i].model);
    memset(g_sites, 0, sizeof(g_sites));
    memset(g_builds, 0, sizeof(g_builds));
}
