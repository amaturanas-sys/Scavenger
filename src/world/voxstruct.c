#include "voxstruct.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"
#include "sim/voxel.h"
#include "sim/voxmat.h"


static const uint8_t PAL_BASE[M_COUNT][4] = {
    [M_NONE] = { 0, 0, 0, 0 },           [M_STONE] = { 138, 134, 126, 255 },  [M_STONE_DARK] = { 96, 92, 86, 255 },
    [M_EARTH] = { 132, 116, 80, 255 },   [M_WOOD] = { 118, 86, 56, 255 },     [M_WOOD_DARK] = { 80, 60, 42, 255 },
    [M_FELT] = { 232, 226, 210, 255 },   [M_FELT_ROOF] = { 210, 200, 178, 255 }, [M_GRASS] = { 112, 128, 70, 255 },
    [M_ADOBE] = { 196, 160, 112, 255 },  [M_TURF] = { 96, 120, 74, 255 },     [M_WATER] = { 52, 92, 128, 255 },
    [M_RED] = { 168, 46, 38, 255 },      [M_BLUE] = { 52, 96, 176, 255 },     [M_YELLOW] = { 222, 186, 60, 255 },
    [M_CARVE] = { 214, 200, 164, 255 },  [M_STONE_LICHEN] = { 120, 128, 104, 255 },
    // De las fotos: basalto negro de la costa, granito claro de los cantos de rio, areniscas en
    // bandas y creta blanca del desierto, hielo del glaciar con grietas azules.
    [M_BASALT] = { 44, 43, 46, 255 },    [M_BASALT_L] = { 70, 68, 72, 255 },   [M_GRANITE] = { 190, 186, 174, 255 },
    [M_GRANITE_D] = { 152, 148, 138, 255 }, [M_SAND1] = { 206, 164, 106, 255 }, [M_SAND2] = { 228, 200, 150, 255 },
    [M_SAND3] = { 178, 128, 82, 255 },   [M_CHALK] = { 238, 232, 216, 255 },   [M_CHALK_D] = { 206, 192, 164, 255 },
    [M_ICE] = { 214, 236, 244, 255 },    [M_ICE_B] = { 150, 204, 230, 255 },   [M_ICE_D] = { 92, 158, 206, 255 },
    [M_MOSS] = { 92, 120, 70, 255 },
    // El kit de edificios: paja, zarzo encalado, caña, el barro del horno, escoria, cuero, fieltro gris
    // y lo oscuro de adentro.
    [M_THATCH] = { 188, 158, 92, 255 },  [M_DAUB] = { 214, 200, 170, 255 },   [M_REED] = { 164, 146, 96, 255 },
    [M_CLAY] = { 160, 88, 60, 255 },     [M_SLAG] = { 52, 47, 46, 255 },      [M_HIDE] = { 148, 106, 70, 255 },
    [M_FELT_DARK] = { 128, 112, 96, 255 }, [M_SHADOW] = { 30, 26, 24, 255 },
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

// ------------------------------------------------------------------ formaciones del paisaje
static float hrand(uint32_t *s) { // azar determinista de cada variante
    *s = *s * 1664525u + 1013904223u;
    return (float)(*s >> 8) / 16777216.0f;
}

// Erosion lateral: quita lo de la superficie donde el ruido es bajo (las formas se vuelven irregulares).
static void erode_skin(VoxGrid *g, uint32_t seed, float freq, float cut) {
    for (int y = 0; y < g->ny; y++)
        for (int z = 0; z < g->nz; z++)
            for (int x = 0; x < g->nx; x++) {
                if (!vox_get(g, x, y, z)) continue;
                bool skin = !vox_get(g, x + 1, y, z) || !vox_get(g, x - 1, y, z) || !vox_get(g, x, y, z + 1) || !vox_get(g, x, y, z - 1) || !vox_get(g, x, y + 1, z);
                if (skin && vox_noise3(x * freq, y * freq, z * freq, seed) < cut) vox_set(g, x, y, z, 0);
            }
}

// Estratos: el material de cada capa segun la altura (bandas de 1 a 3 celdas).
static uint8_t stratum(int y, uint32_t seed) {
    static const uint8_t BANDS[6] = { M_SAND1, M_SAND2, M_SAND1, M_SAND3, M_SAND2, M_SAND1 };
    return BANDS[((unsigned)(y / 2) + seed) % 6];
}

static bool build_feature(VoxFeature f, int v, VoxGrid *g) {
    uint32_t seed = 7000u + (uint32_t)f * 131u + (uint32_t)v * 17u, rs = seed;
    switch (f) {
    case VF_BASALT: { // columnas de basalto de alturas distintas, apretadas
        if (!vox_init(g, 14, FOUND + 10, 14, 0.5f)) return false;
        for (int i = 0; i < 9; i++) {
            float cx = 3 + hrand(&rs) * 8, cz = 3 + hrand(&rs) * 8, r = 1.2f + hrand(&rs) * 1.6f;
            int top = FOUND + 2 + (int)(hrand(&rs) * 7.0f);
            vox_cylinder(g, cx, cz, 0, top, r, r * 0.9f, (i % 3) ? M_BASALT : M_BASALT_L);
        }
        erode_skin(g, seed, 0.5f, 0.25f);
        return true;
    }
    case VF_SEA_STACK: { // farallon: pilar erosionado con hierba arriba
        if (!vox_init(g, 12, FOUND + 22, 12, 1.0f)) return false;
        float r = 3.6f + hrand(&rs) * 1.2f;
        int top = FOUND + 12 + (int)(hrand(&rs) * 8.0f);
        vox_cylinder(g, 6, 6, 0, top, r + 1.0f, r * 0.7f, M_BASALT);
        for (int y = 0; y <= top; y += 3) vox_cylinder(g, 6, 6, y, y, r * 0.8f, r * 0.8f, M_BASALT_L);
        erode_skin(g, seed, 0.35f, 0.32f);
        for (int z = 0; z < g->nz; z++) // la hierba de la cima
            for (int x = 0; x < g->nx; x++)
                for (int y = g->ny - 1; y >= top - 1; y--)
                    if (vox_get(g, x, y, z)) {
                        vox_set(g, x, y, z, M_MOSS);
                        break;
                    }
        return true;
    }
    case VF_BOULDER: { // canto rodado: elipsoide liso, gris claro con motas
        if (!vox_init(g, 12, FOUND + 9, 12, 0.25f)) return false;
        float rx = 3.5f + hrand(&rs) * 2.0f, ry = 2.5f + hrand(&rs) * 2.0f, rz = 3.0f + hrand(&rs) * 2.0f;
        vox_ellipsoid(g, 6, FOUND, 6, rx, ry, rz, M_GRANITE, false);
        for (int i = 0; i < g->nx * g->ny * g->nz; i++)
            if (g->m[i] == M_GRANITE && vox_noise3((float)(i % 12) * 0.9f, (float)(i / 144) * 0.9f, (float)((i / 12) % 12) * 0.9f, seed) > 0.66f) g->m[i] = M_GRANITE_D;
        return true;
    }
    case VF_HOODOO: { // torre de estratos: capas blandas angostas, duras anchas, sombrero de roca dura
        if (!vox_init(g, 16, FOUND + 30, 16, 0.5f)) return false;
        int top = FOUND + 18 + (int)(hrand(&rs) * 8.0f);
        for (int y = 0; y <= top; y++) {
            float r = 3.0f + 1.4f * sinf(y * 0.55f + hrand(&rs) * 0.3f) + (y < FOUND + 4 ? 2.0f : 0.0f);
            vox_cylinder(g, 8, 8, y, y, r, r, stratum(y, seed));
        }
        vox_ellipsoid(g, 8, top + 1, 8, 6.5f, 2.2f, 5.5f, M_SAND3, false); // el sombrero
        erode_skin(g, seed, 0.6f, 0.22f);
        return true;
    }
    case VF_MESA: { // farallon estratificado: alero sobre una capa blanda y una cueva
        if (!vox_init(g, 30, FOUND + 18, 22, 1.0f)) return false;
        int top = FOUND + 12 + (int)(hrand(&rs) * 5.0f);
        for (int y = 0; y <= top; y++) vox_box(g, 2, y, 2, 27, y, 19, stratum(y, seed));
        vox_carve(g, 0, FOUND + 1, 0, 29, FOUND + 3, 4);             // el alero: la capa blanda se va
        vox_carve(g, 10 + (int)(hrand(&rs) * 8), FOUND, 2, 15 + (int)(hrand(&rs) * 6), FOUND + 4, 9); // la cueva
        erode_skin(g, seed, 0.25f, 0.3f);
        return true;
    }
    case VF_CHALK: { // creta blanca: un bloque redondeado por el viento, con base mas angosta
        if (!vox_init(g, 16, FOUND + 18, 14, 0.5f)) return false;
        // Pilar angosto abajo que se ensancha arriba (el viento con arena come la base).
        vox_cylinder(g, 8, 7, 0, FOUND + 5, 3.2f, 4.2f, M_CHALK_D);
        vox_cylinder(g, 8, 7, FOUND + 6, FOUND + 12, 4.6f, 6.4f, M_CHALK);
        vox_ellipsoid(g, 8, FOUND + 12, 7, 6.4f, 3.5f, 5.6f, M_CHALK, true);
        for (int y = FOUND + 4; y < g->ny; y += 4) // vetas
            for (int z = 0; z < g->nz; z++)
                for (int x = 0; x < g->nx; x++)
                    if (vox_get(g, x, y, z) == M_CHALK) vox_set(g, x, y, z, M_CHALK_D);
        erode_skin(g, seed, 0.45f, 0.3f);
        return true;
    }
    case VF_SERAC: { // bloques de hielo inclinados, de alturas distintas, con grietas azules
        if (!vox_init(g, 16, FOUND + 16, 16, 0.75f)) return false;
        for (int i = 0; i < 7; i++) {
            int x0 = (int)(hrand(&rs) * 11), z0 = (int)(hrand(&rs) * 11), w = 3 + (int)(hrand(&rs) * 3), h = FOUND + 6 + (int)(hrand(&rs) * 9);
            vox_box(g, x0, 0, z0, x0 + w, h, z0 + w - 1, M_ICE);
        }
        for (int y = 0; y < g->ny; y++)
            for (int z = 0; z < g->nz; z++)
                for (int x = 0; x < g->nx; x++) {
                    if (!vox_get(g, x, y, z)) continue;
                    float n = vox_noise3(x * 0.6f, y * 0.12f, z * 0.6f, seed); // grietas verticales
                    if (n < 0.28f) vox_set(g, x, y, z, M_ICE_D);
                    else if (n < 0.42f) vox_set(g, x, y, z, M_ICE_B);
                }
        erode_skin(g, seed, 0.4f, 0.25f);
        return true;
    }
    case VF_ICEBERG: { // tempano: arriba plano, abajo hundido (se dibuja a ras del agua)
        if (!vox_init(g, 14, 9, 10, 0.75f)) return false;
        vox_ellipsoid(g, 7, 3, 5, 6.0f, 6.0f, 4.0f, M_ICE, false);
        vox_carve(g, 0, 5 + (int)(hrand(&rs) * 2), 0, 13, 8, 9);
        for (int i = 0; i < g->nx * g->ny * g->nz; i++)
            if (g->m[i] && (i / (g->nx * g->nz)) < 3) g->m[i] = M_ICE_B;
        erode_skin(g, seed, 0.5f, 0.2f);
        return true;
    }
    case VF_FLOE: { // tempano del mar: una placa chata de hielo, de borde quebrado, que asoma un palmo
        if (!vox_init(g, 18, 4, 14, 0.5f)) return false;
        float rx = 6.0f + hrand(&rs) * 2.5f, rz = 4.5f + hrand(&rs) * 2.0f;
        for (int z = 0; z < g->nz; z++)
            for (int x = 0; x < g->nx; x++) {
                float dx = ((float)x - 8.5f) / rx, dz = ((float)z - 6.5f) / rz, a = atan2f(dz, dx);
                float edge = 1.0f + 0.44f * (vox_noise3(cosf(a) * 2.0f + 2.0f, sinf(a) * 2.0f + 2.0f, 0.5f, seed) - 0.5f); // borde quebrado
                if (dx * dx + dz * dz > edge * edge) continue;
                int top = 1 + (vox_noise3((float)x * 0.4f, 0.5f, (float)z * 0.4f, seed + 1u) > 0.62f); // unos bloques encima
                for (int y = 0; y <= top; y++) vox_set(g, x, y, z, y == 0 ? M_ICE_B : M_ICE); // abajo, el azul del agua
            }
        erode_skin(g, seed, 0.3f, 0.15f);
        return true;
    }
    default: return false;
    }
}

// ------------------------------------------------------------------ la roca de la boca de una cueva
// En celdas de 0,4 m. La malla tiene el origen en el centro de la base de la rejilla: x de -4,8 a 4,8; z de -1,2 a 6,8
// (se dibuja corrida MOUTH_MID hacia adentro); y desde el cimiento (MOUTH_GROUND capas bajo el suelo de la boca) hasta
// 7,2 m sobre el.
#define MOUTH_S 0.4f
#define MOUTH_NX 24
#define MOUTH_NY 25
#define MOUTH_NZ 20
#define MOUTH_GROUND 7
#define MOUTH_Z0 (-1.2f)
#define MOUTH_MID 2.8f // el centro de la rejilla, en el plano de la cueva
const float VOXS_MOUTH_BOULDERS[2][3] = { { -3.15f, 0.55f, 1.35f }, { 3.55f, 0.95f, 1.25f } }; // fuera de escaleras y puntales

static float mouth_u(float x) { return x / MOUTH_S + MOUTH_NX * 0.5f; } // de metros a celdas
static float mouth_v(float y) { return y / MOUTH_S + MOUTH_GROUND; }
static float mouth_w(float z) { return (z - MOUTH_Z0) / MOUTH_S; }

static void mouth_ellipsoid(VoxGrid *g, float x, float y, float z, float rx, float ry, float rz) {
    vox_ellipsoid(g, mouth_u(x), mouth_v(y), mouth_w(z), rx / MOUTH_S, ry / MOUTH_S, rz / MOUTH_S, M_STONE, false);
}

// Llena (o vacia, con M_NONE) las celdas cuyo centro cae en la caja (en metros).
static void mouth_box(VoxGrid *g, float x0, float y0, float z0, float x1, float y1, float z1, uint8_t mat) {
    vox_box(g, (int)ceilf(mouth_u(x0) - 0.5f), (int)ceilf(mouth_v(y0) - 0.5f), (int)ceilf(mouth_w(z0) - 0.5f), (int)floorf(mouth_u(x1) - 0.5f),
            (int)floorf(mouth_v(y1) - 0.5f), (int)floorf(mouth_w(z1) - 0.5f), mat);
}

static bool build_cave_mouth(Region r, int v, bool facade, VoxGrid *g) {
    uint32_t seed = 9100u + (uint32_t)r * 37u + (uint32_t)v * 11u + (facade ? 5u : 0u), rs = seed;
    if (!vox_init(g, MOUTH_NX, MOUTH_NY, MOUTH_NZ, MOUTH_S)) return false;
    // La cara: un risco casi vertical (z = 0,4), de 7,2 m de ancho y 4,8 de alto, con el lomo redondeado encima; los
    // hombros y unos bultos le rompen la silueta, siempre dentro de la huella.
    mouth_box(g, -3.6f, -2.8f, 0.4f, 3.6f, 4.8f, 6.4f, M_STONE);
    mouth_ellipsoid(g, 0.0f, 4.8f, 3.6f, 3.8f, 1.8f + hrand(&rs) * 0.6f, 3.0f);
    mouth_ellipsoid(g, -3.5f, 1.6f + hrand(&rs), 3.3f, 1.2f, 2.6f, 2.7f);
    mouth_ellipsoid(g, 3.5f, 1.4f + hrand(&rs), 3.5f, 1.2f, 2.4f, 2.6f);
    mouth_ellipsoid(g, -1.8f + hrand(&rs), 5.6f, 2.8f, 1.6f, 1.1f, 1.6f);
    mouth_ellipsoid(g, 1.6f, 5.9f + hrand(&rs) * 0.4f, 3.6f, 1.8f, 1.2f, 1.8f);
    erode_skin(g, seed, 0.45f, 0.24f);
    if (facade) mouth_box(g, -3.0f, 0.0f, 0.4f, 3.0f, 5.0f, 1.2f, M_STONE); // la cara lisa donde se apoya la fachada
    for (int b = 0; b < 2; b++) { // las dos rocas bajas del umbral
        const float *k = VOXS_MOUTH_BOULDERS[b];
        mouth_ellipsoid(g, k[0], 0.25f + 0.15f * (float)b, k[1], k[2], k[2] * 0.85f, k[2]);
    }
    // El cimiento: lo de cada columna baja hasta el fondo de la rejilla (en una ladera, nada queda en el aire).
    for (int z = 0; z < g->nz; z++)
        for (int x = 0; x < g->nx; x++) {
            int low = 0;
            while (low < g->ny && !vox_get(g, x, low, z)) low++;
            if (low < g->ny) vox_box(g, x, 0, z, x, low, z, M_STONE);
        }
    // El tunel, mas ancho y mas alto que el de world_game.c (sus paredes oscuras lo tapan y no comparten plano), y el
    // arco del frente: 3,2 m de ancho hasta los 2 m, que se cierra hasta los 3,2 m (un poco corrido, si es natural; mas
    // derecho, si lo labraron).
    mouth_box(g, -2.0f, -0.4f, 1.2f, 2.0f, 3.6f, 4.8f, M_NONE);
    mouth_box(g, -1.6f, -0.4f, MOUTH_Z0, 1.6f, 2.0f, 1.2f, M_NONE);
    static const float HW[4] = { 1.6f, 1.2f, 0.8f, 0.4f };
    float shift = facade ? 0.0f : (float)((int)(hrand(&rs) * 3.0f) - 1) * MOUTH_S;
    for (int b = 1; b < 4; b++) {
        float hw = facade ? (b < 3 ? 1.6f : 1.2f) : HW[b], dx = b >= 2 ? shift : 0.0f;
        mouth_box(g, -hw + dx, 1.6f + 0.4f * (float)b, MOUTH_Z0, hw + dx, 2.0f + 0.4f * (float)b, 1.2f, M_NONE);
    }
    // La piedra de la region: arenisca en estratos (desierto), granito (altiplano) o piedra con su liquen, y pasto en lo
    // alto donde lo hay.
    for (int y = 0; y < g->ny; y++)
        for (int z = 0; z < g->nz; z++)
            for (int x = 0; x < g->nx; x++) {
                if (!vox_get(g, x, y, z)) continue;
                if (r == REGION_DESERT) vox_set(g, x, y, z, stratum(y, seed));
                else if (r == REGION_HIGHLAND) vox_set(g, x, y, z, vox_noise3(x * 0.7f, y * 0.7f, z * 0.7f, seed) > 0.6f ? M_GRANITE_D : M_GRANITE);
            }
    if (r == REGION_DESERT || r == REGION_HIGHLAND) return true;
    stones_mixed(g, seed);
    for (int z = 0; z < g->nz; z++)
        for (int x = 0; x < g->nx; x++)
            for (int y = g->ny - 1; y >= MOUTH_GROUND + 8; y--)
                if (vox_get(g, x, y, z)) {
                    vox_set(g, x, y, z, M_GRASS);
                    break;
                }
    return true;
}

// ------------------------------------------------------------------ lo que ocupa cada modelo
// Un bit por celda de ~0,5 m, armado una vez junto con el modelo (antes de refinarlo): lo que tapa la camara
// (voxs_solid_at). La rejilla, centrada en x y z como la malla, desde el fondo del cimiento.
typedef struct {
    uint8_t *bits;
    int nx, ny, nz;
    float cell, ox, oz; // m: el lado de la celda y la esquina de la rejilla
} VoxOcc;

static void occ_build(VoxOcc *o, const VoxGrid *g) {
    if (o->bits || !g->m || g->size <= 0.0f) return;
    int f = (int)lroundf(0.5f / g->size);
    if (f < 1) f = 1;
    o->nx = (g->nx + f - 1) / f, o->ny = (g->ny + f - 1) / f, o->nz = (g->nz + f - 1) / f;
    o->cell = g->size * (float)f;
    o->ox = -(float)g->nx * g->size * 0.5f, o->oz = -(float)g->nz * g->size * 0.5f;
    size_t n = (size_t)o->nx * (size_t)o->ny * (size_t)o->nz;
    o->bits = calloc((n + 7) / 8, 1);
    if (!o->bits) return;
    for (int y = 0; y < g->ny; y++)
        for (int z = 0; z < g->nz; z++)
            for (int x = 0; x < g->nx; x++) {
                if (!g->m[(size_t)x + (size_t)g->nx * ((size_t)z + (size_t)g->nz * (size_t)y)]) continue;
                size_t i = (size_t)(x / f) + (size_t)o->nx * ((size_t)(z / f) + (size_t)o->nz * (size_t)(y / f));
                o->bits[i >> 3] |= (uint8_t)(1u << (i & 7));
            }
}

static void occ_free(VoxOcc *o) {
    free(o->bits);
    memset(o, 0, sizeof(*o));
}

// ------------------------------------------------------------------ cache de modelos
#define SITE_SLOTS (SITE_KINDS * REGION_COUNT)
static struct {
    bool tried, ok;
    Model model;
    float found_h; // m de cimiento (se dibuja hundido)
    VoxOcc occ;
} g_sites[SITE_SLOTS], g_feats[VF_KINDS * VF_VARIANTS];
// El kit: de lejos, el modelo sin refinar; de cerca, el refinado (se arma la primera vez que hace falta).
static struct {
    bool tried, ok, fine_tried, fine_ok;
    Model model, fine;
    BkInfo info;
    VoxOcc occ;
} g_kit[BK_KINDS * REGION_COUNT * BK_VARIANTS];
// La roca de las bocas de cueva: de lejos, sin refinar; de cerca, refinada (como el kit).
static struct {
    bool tried, ok, fine_tried, fine_ok;
    Model model, fine;
} g_mouths[REGION_COUNT * VF_VARIANTS * 2];
static Vector3 g_lod_eye;
static float g_lod_near = 1e9f; // sin voxs_set_lod, todo de cerca

// Los modelos dibujados cerca del ojo en el cuadro anterior (voxs_set_lod empieza la lista de cada cuadro).
#define NEAR_MAX 48
static struct {
    const VoxOcc *occ;
    Vector3 base;          // el fondo de la rejilla en el mundo (donde se dibuja, menos el cimiento)
    float c, s, inv, r;    // su giro, 1/escala y el radio de su planta (m)
} g_near[NEAR_MAX];
static int g_near_n;

static void remember(const VoxOcc *o, float found_h, Vector3 at, float yaw, float scale) {
    if (!o->bits || g_near_n >= NEAR_MAX || scale <= 0.0f) return;
    float hx = (float)o->nx * o->cell * 0.5f, hz = (float)o->nz * o->cell * 0.5f, r = sqrtf(hx * hx + hz * hz) * scale;
    float dx = at.x - g_lod_eye.x, dz = at.z - g_lod_eye.z;
    if (dx * dx + dz * dz > (r + 24.0f) * (r + 24.0f)) return; // lejos del brazo de la camara
    g_near[g_near_n].occ = o;
    g_near[g_near_n].base = (Vector3){ at.x, at.y - found_h * scale, at.z };
    g_near[g_near_n].c = cosf(yaw), g_near[g_near_n].s = sinf(yaw), g_near[g_near_n].inv = 1.0f / scale, g_near[g_near_n].r = r;
    g_near_n++;
}

// ¿q cae en una celda llena del modelo i de la lista? (al reves del giro de DrawModelEx)
static bool near_hit(int i, Vector3 q) {
    const VoxOcc *o = g_near[i].occ;
    float dx = q.x - g_near[i].base.x, dz = q.z - g_near[i].base.z;
    if (dx * dx + dz * dz > g_near[i].r * g_near[i].r) return false;
    float c = g_near[i].c, s = g_near[i].s, k = g_near[i].inv;
    float lx = (dx * c - dz * s) * k, lz = (dx * s + dz * c) * k, ly = (q.y - g_near[i].base.y) * k;
    int x = (int)floorf((lx - o->ox) / o->cell), y = (int)floorf(ly / o->cell), z = (int)floorf((lz - o->oz) / o->cell);
    if (x < 0 || y < 0 || z < 0 || x >= o->nx || y >= o->ny || z >= o->nz) return false;
    size_t b = (size_t)x + (size_t)o->nx * ((size_t)z + (size_t)o->nz * (size_t)y);
    return (o->bits[b >> 3] >> (b & 7)) & 1u;
}

bool voxs_solid_at(Vector3 q, Vector3 who) {
    for (int i = 0; i < g_near_n; i++)
        if (near_hit(i, q) && !near_hit(i, who)) return true;
    return false;
}

int voxs_near_count(void) { return g_near_n; }

void voxs_palette(Region r, uint8_t out[M_COUNT][4]) { palette_for(r, out); }
void voxs_erode_skin(VoxGrid *g, uint32_t seed, float freq, float cut) { erode_skin(g, seed, freq, cut); }
float voxs_rand(uint32_t *s) { return hrand(s); }

static bool to_model(VoxGrid *g, Region r, Model *out, bool refine) {
    // Estructuras y casas: el doble de resolucion (cada celda en 2x2x2, con las esquinas
    // redondeadas) y tono por celda. Las formaciones del paisaje (muchas a la vez) no se refinan.
    uint8_t pal[M_COUNT][4];
    palette_for(r, pal);
    return voxs_grid_model(g, (const uint8_t(*)[4])pal, out, refine, 22);
}

bool voxs_grid_model(VoxGrid *g, const uint8_t (*pal)[4], Model *out, bool refine, uint8_t jitter) {
    g->jitter = jitter;
    if (refine) vox_refine(g);
    VoxMesh vm;
    bool ok = vox_mesh(g, pal, &vm) && vm.tris > 0;
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

int voxs_site_variant(Vector3 at) { return (int)(fabsf(at.x) * 0.37f + fabsf(at.z) * 0.61f) % BK_VARIANTS; }

void voxs_draw_site(SiteKind kind, Region region, Vector3 at, float yaw, Color tint) {
    if ((unsigned)kind >= SITE_KINDS || (unsigned)region >= REGION_COUNT) return;
    if (kind == SITE_RUINED_FORT || kind == SITE_BURIED_CITY || kind == SITE_ANCIENT_TOWN) { // las ruinas, del kit: una de sus variantes, segun donde esten
        BkType t = kind == SITE_RUINED_FORT ? BK_RUIN_FORT : kind == SITE_BURIED_CITY ? BK_RUIN_CITY : BK_RUIN_TOWN;
        voxs_draw_kit(t, bk_available(t, region) ? region : REGION_DESERT, voxs_site_variant(at), at, yaw, 1.0f, tint);
        return;
    }
    int i = (int)kind * REGION_COUNT + (int)region;
    if (!g_sites[i].tried) {
        g_sites[i].tried = true;
        VoxGrid g;
        if (build_site2(kind, region, &g, 1000u + (uint32_t)i * 17u)) {
            g_sites[i].found_h = FOUND * g.size;
            occ_build(&g_sites[i].occ, &g);
            g_sites[i].ok = to_model(&g, region, &g_sites[i].model, true);
            vox_free(&g);
        }
    }
    if (!g_sites[i].ok) return;
    draw_model(g_sites[i].model, g_sites[i].found_h, at, yaw, 1.0f, tint);
    remember(&g_sites[i].occ, g_sites[i].found_h, at, yaw, 1.0f);
}

static int kit_slot(BkType t, Region region, int variant, bool fine) {
    if (!bk_available(t, region)) return -1;
    int v = variant & (BK_VARIANTS - 1), i = ((int)t * REGION_COUNT + (int)region) * BK_VARIANTS + v;
    if (fine ? !g_kit[i].fine_tried : !g_kit[i].tried) {
        if (fine) g_kit[i].fine_tried = true;
        else g_kit[i].tried = true;
        VoxGrid g;
        BkInfo info;
        if (bk_build(t, region, bk_variant_seed(t, region, v), v < 2, &g, &info)) {
            g_kit[i].info = info;
            occ_build(&g_kit[i].occ, &g);
            if (fine) g_kit[i].fine_ok = to_model(&g, region, &g_kit[i].fine, true);
            else g_kit[i].ok = to_model(&g, region, &g_kit[i].model, false);
            vox_free(&g);
        }
    }
    return (fine ? g_kit[i].fine_ok : g_kit[i].ok) ? i : -1;
}

void voxs_set_lod(Vector3 eye, float near_m) {
    g_lod_eye = eye;
    g_lod_near = near_m;
    g_near_n = 0; // un cuadro nuevo: la lista de lo dibujado cerca empieza de cero
}

void voxs_draw_kit(BkType t, Region region, int variant, Vector3 at, float yaw, float scale, Color tint) {
    float dx = at.x - g_lod_eye.x, dz = at.z - g_lod_eye.z;
    bool fine = dx * dx + dz * dz < g_lod_near * g_lod_near;
    int i = kit_slot(t, region, variant, fine);
    if (i < 0 && fine) i = kit_slot(t, region, variant, false), fine = false;
    if (i < 0) return;
    draw_model(fine ? g_kit[i].fine : g_kit[i].model, g_kit[i].info.found, at, yaw, scale, tint);
    remember(&g_kit[i].occ, g_kit[i].info.found, at, yaw, scale);
}

bool voxs_kit_info(BkType t, Region region, int variant, BkInfo *out) {
    int i = kit_slot(t, region, variant, false);
    if (i < 0) return false;
    if (out) *out = g_kit[i].info;
    return true;
}

bool voxs_draw_item(const InvItem *item, Vector3 at, float yaw) {
    BkType t;
    Region r;
    int v;
    if (!item || !bk_for_item(item->id, &t, &r, &v)) return false;
    v += (int)(fabsf(at.x) * 0.7f + fabsf(at.z) * 1.3f) & 1; // de las dos de su tamaño, la que toca aca
    float s = 1.0f;
    BkInfo in;
    if (t == BK_HOUSE && voxs_kit_info(t, r, v, &in) && in.l > 0.0f && in.w > 0.0f) s = fminf(item->l / in.l, item->w / in.w);
    voxs_draw_kit(t, r, v, at, yaw, s, WHITE);
    return true;
}

void voxs_draw_feature(VoxFeature f, int variant, Vector3 at, float yaw, float scale, Color tint) {
    if ((unsigned)f >= VF_KINDS) return;
    int i = (int)f * VF_VARIANTS + (variant & (VF_VARIANTS - 1));
    if (!g_feats[i].tried) {
        g_feats[i].tried = true;
        VoxGrid g;
        if (build_feature(f, variant & (VF_VARIANTS - 1), &g)) {
            occ_build(&g_feats[i].occ, &g);
            g_feats[i].ok = to_model(&g, REGION_STEPPE, &g_feats[i].model, false);
            g_feats[i].found_h = (f == VF_ICEBERG ? 3 : f == VF_FLOE ? 1 : FOUND) * g.size;
            vox_free(&g);
        }
    }
    if (!g_feats[i].ok) return;
    draw_model(g_feats[i].model, g_feats[i].found_h, at, yaw, scale, tint);
    remember(&g_feats[i].occ, g_feats[i].found_h, at, yaw, scale);
}

static int mouth_slot(Region r, int v, bool facade, bool fine) {
    int i = ((int)r * VF_VARIANTS + (v & (VF_VARIANTS - 1))) * 2 + (facade ? 1 : 0);
    bool *tried = fine ? &g_mouths[i].fine_tried : &g_mouths[i].tried;
    if (!*tried) {
        *tried = true;
        VoxGrid g;
        if (build_cave_mouth(r, v & (VF_VARIANTS - 1), facade, &g)) {
            if (fine) g_mouths[i].fine_ok = to_model(&g, r, &g_mouths[i].fine, true);
            else g_mouths[i].ok = to_model(&g, r, &g_mouths[i].model, false);
            vox_free(&g);
        }
    }
    return (fine ? g_mouths[i].fine_ok : g_mouths[i].ok) ? i : -1;
}

void voxs_draw_cave_mouth(Region region, int variant, bool facade, Vector3 at, float yaw) {
    if ((unsigned)region >= REGION_COUNT) region = REGION_STEPPE;
    Vector3 p = { at.x + sinf(yaw) * MOUTH_MID, at.y, at.z + cosf(yaw) * MOUTH_MID }; // el centro de la rejilla
    float dx = p.x - g_lod_eye.x, dz = p.z - g_lod_eye.z;
    bool fine = dx * dx + dz * dz < g_lod_near * g_lod_near;
    int i = mouth_slot(region, variant, facade, fine);
    if (i < 0 && fine) i = mouth_slot(region, variant, facade, false), fine = false;
    if (i >= 0) draw_model(fine ? g_mouths[i].fine : g_mouths[i].model, MOUTH_GROUND * MOUTH_S, p, yaw, 1.0f, WHITE);
}

// La ocupacion de todos los modelos (memoria nuestra, no de la GPU) y la lista de los cercanos.
static void forget_occupancy(void) {
    for (int i = 0; i < SITE_SLOTS; i++) occ_free(&g_sites[i].occ);
    for (int i = 0; i < VF_KINDS * VF_VARIANTS; i++) occ_free(&g_feats[i].occ);
    for (int i = 0; i < BK_KINDS * REGION_COUNT * BK_VARIANTS; i++) occ_free(&g_kit[i].occ);
    g_near_n = 0;
}

void voxs_forget_gpu(void) {
    forget_occupancy(); // los modelos se vuelven a armar, y con ellos su ocupacion
    memset(g_mouths, 0, sizeof(g_mouths));
    memset(g_kit, 0, sizeof(g_kit));
    memset(g_feats, 0, sizeof(g_feats));
    memset(g_sites, 0, sizeof(g_sites));
}

void voxs_unload(void) {
    forget_occupancy();
    for (int i = 0; i < SITE_SLOTS; i++)
        if (g_sites[i].ok) UnloadModel(g_sites[i].model);
    for (int i = 0; i < VF_KINDS * VF_VARIANTS; i++)
        if (g_feats[i].ok) UnloadModel(g_feats[i].model);
    for (int i = 0; i < BK_KINDS * REGION_COUNT * BK_VARIANTS; i++) {
        if (g_kit[i].ok) UnloadModel(g_kit[i].model);
        if (g_kit[i].fine_ok) UnloadModel(g_kit[i].fine);
    }
    for (int i = 0; i < REGION_COUNT * VF_VARIANTS * 2; i++) {
        if (g_mouths[i].ok) UnloadModel(g_mouths[i].model);
        if (g_mouths[i].fine_ok) UnloadModel(g_mouths[i].fine);
    }
    memset(g_mouths, 0, sizeof(g_mouths));
    memset(g_kit, 0, sizeof(g_kit));
    memset(g_feats, 0, sizeof(g_feats));
    memset(g_sites, 0, sizeof(g_sites));
}
