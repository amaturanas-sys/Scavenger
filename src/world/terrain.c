#include "terrain.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../sim/hazards.h"
#include "../sim/noise.h"
#include "platform.h"
#include "raymath.h"

// Por triangulo: altura, pendiente, luz, mancha, y pesos de desierto, bosque, altiplano y
// costa, distancia sobre el agua (para la orilla) y que es (suelo, copa o tronco).
enum { A_H, A_SLOPE, A_LIGHT, A_PATCH, A_DESERT, A_FOREST, A_HIGH, A_FJORD, A_SHORE, A_EDGE, A_KIND, TRI_ATTRS };
enum { KIND_GROUND, KIND_CANOPY, KIND_TRUNK };
enum { W_LAKE, W_RIVER, W_SEA, W_GLACIAL }; // las mallas de agua de cada chunk

// Texturas del suelo (assets/terrain/texturas.png, tools/assets/texturas_terreno.py): un atlas
// low-res de 4x4 celdas; cada celda cubre 8 m. El orden es fijo.
typedef enum {
    TT_TUSSOCK, TT_TALLGRASS, TT_FOREST, TT_TUNDRA, TT_MOSS, TT_SAND, TT_STRATA, TT_GRAVEL,
    TT_SNOW, TT_ROCK, TT_MUD, TT_ICE, TT_R12, TT_R13, TT_R14, TT_WHITE,
} TerrainTex;
#define TEX_COLS 4
#define TEX_SPAN 8.0f   // m de suelo por celda del atlas
// El detalle oscurece en promedio: el color del suelo se aclara para compensar, segun lo clara
// que sea cada celda (la nieve casi nada: si no, se confunde con el cielo).
static float g_tex_boost[16] = { 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.0f };
static Texture2D g_atlas;
static bool g_atlas_tried;
static float g_perma_snow; // altura de la nieve de verano (las cumbres siempre blancas)

float terrain_height(const Terrain *t, float x, float z) { return world_height(t->world, x, z); }
float terrain_plain_height(const Terrain *t) { return t->world->camp_height; }
Region terrain_region(const Terrain *t, float x, float z) { return world_region(t->world, x, z); }

float terrain_water(const Terrain *t, float x, float z) { return world_water(t->world, x, z, t->look.flood, NULL); }

float terrain_ice(const Terrain *t, float x, float z) {
    WaterKind k;
    world_water(t->world, x, z, t->look.flood, &k);
    return k == WATER_SEA ? 0.0f : t->look.ice;
}

bool terrain_deep_water(const Terrain *t, float x, float z, float depth) {
    WaterKind k;
    float w = world_water(t->world, x, z, t->look.flood, &k);
    return w > terrain_height(t, x, z) + depth && (k == WATER_SEA || !hazard_ice_walkable(t->look.ice));
}

static Color mixc(Color a, Color b, float k) {
    k = Clamp(k, 0.0f, 1.0f);
    return (Color){ (unsigned char)Lerp(a.r, b.r, k), (unsigned char)Lerp(a.g, b.g, k), (unsigned char)Lerp(a.b, b.b, k),
                    255 };
}

// Paleta segun la region y la estacion: pasto de estepa (verde, seco, ocre o dormido),
// sotobosque de coniferas, tundra del altiplano, musgo de la costa y arena del desierto;
// tierra y roca en las pendientes; barro con lluvia; orillas; nieve, hielo y glaciares.
// h: altura relativa al llano; patch en [0, 1]: manchas (nieve a medio cubrir).
static Color ground_color(const TerrainLook *L, const float *a, float h) {
    float h_abs = a[A_H], slope = a[A_SLOPE], patch = a[A_PATCH], desert = a[A_DESERT];
    const Color green = { 112, 150, 70, 255 }, dry = { 188, 168, 106, 255 }, ochre = { 172, 124, 68, 255 };
    const Color dormant = { 140, 128, 98, 255 }, dirt = { 140, 110, 76, 255 }, rock = { 120, 116, 108, 255 };
    const Color snow = { 226, 232, 240, 255 }, glacier = { 196, 218, 236, 255 }, mud = { 112, 96, 72, 255 };
    const Color lakebed = { 158, 140, 108, 255 }, needles = { 72, 96, 54, 255 }, tundra = { 132, 136, 104, 255 };
    const Color moss = { 96, 124, 92, 255 }, shingle = { 128, 128, 124, 255 };
    Color grass = mixc(dry, green, L->greenness);
    grass = mixc(grass, ochre, L->autumn * 0.85f);
    grass = mixc(dormant, grass, 0.35f + 0.65f * Clamp(L->greenness + L->autumn, 0.0f, 1.0f));
    if (h > 6.0f) grass = mixc(grass, dry, 0.45f); // lomas mas secas
    grass = mixc(grass, mixc(needles, ochre, L->autumn * 0.3f), a[A_FOREST]); // el suelo del bosque
    grass = mixc(grass, tundra, a[A_HIGH]);                                    // la tundra del altiplano
    grass = mixc(grass, moss, a[A_FJORD]);                                     // el musgo de la costa
    Color c = grass;
    bool bare = false;
    float steep = 0.45f + 0.15f * a[A_HIGH];
    if (slope > steep) c = dirt, bare = true;
    if (slope > 0.75f || (slope > 0.35f && a[A_HIGH] + a[A_FJORD] > 0.5f && h > 30.0f)) c = rock, bare = true;
    // Orillas: el lecho que el agua deja al bajar, y el barro (o los guijarros del mar) junto al agua.
    float above_water = a[A_SHORE];
    if (above_water > 0.0f && above_water < TERRAIN_LAKE_FLOOD)
        c = above_water < 0.4f ? (a[A_FJORD] > 0.5f ? shingle : mud) : mixc(c, lakebed, 0.7f);
    if (!bare) c = mixc(c, mud, L->wetness * 0.35f * (1.0f - desert)); // suelo mojado (la arena no se embarra)
    // Desierto: arena dorada con vetas; las mesetas y el muro, roca rojiza en estratos.
    if (desert > 0.0f) {
        const Color sand = { 214, 188, 134, 255 }, sand_dark = { 190, 160, 108, 255 }, redrock = { 178, 104, 70, 255 };
        Color dc = slope > 0.5f ? mixc(redrock, sand_dark, 0.5f + 0.5f * sinf(h_abs * 0.6f)) : mixc(sand, sand_dark, patch * 0.8f);
        c = mixc(c, dc, Clamp(desert * 1.4f, 0.0f, 1.0f));
    }
    // El muro de hielo del altiplano: hielo azulado, mas en lo empinado.
    if (a[A_HIGH] > 0.5f && a[A_EDGE] < 150.0f) c = mixc(c, (Color){ 176, 206, 232, 255 }, 0.5f + 0.5f * Clamp(slope * 2.0f, 0.0f, 1.0f));
    // Nieve: cubre el llano segun la estacion (en manchas a medio cubrir); menos en lo empinado y en la arena.
    float cover = L->snow_cover * (slope > 0.75f ? 0.55f : 1.0f) * (1.0f - 0.6f * desert);
    if (patch < cover * 1.15f - 0.05f) c = snow;
    // Glaciares y nieve permanente en las cumbres; una franja de manchas por debajo.
    float above = h_abs - L->snowline;
    if (above > 0.0f) c = slope < 0.7f ? glacier : mixc(rock, snow, 0.5f);
    else if (above > -6.0f && patch < (above + 6.0f) / 6.0f) c = snow;
    return c;
}

static Color tree_color(const TerrainLook *L, const float *a) {
    if (a[A_KIND] == KIND_TRUNK) return (Color){ 92, 66, 44, 255 };
    Color c = mixc((Color){ 52, 86, 50, 255 }, (Color){ 74, 92, 58, 255 }, a[A_PATCH]);
    // Los alerces (la mitad del bosque) se doran en otoño, como en las laderas del Altai.
    if (a[A_PATCH] > 0.5f) c = mixc(c, (Color){ 196, 150, 60, 255 }, L->autumn * 0.9f);
    float cover = L->snow_cover * 0.8f;
    if (a[A_H] > L->snowline - 4.0f) cover = fmaxf(cover, 0.7f);
    return mixc(c, (Color){ 222, 228, 236, 255 }, cover * (0.4f + 0.6f * a[A_PATCH])); // nieve en las ramas
}

static Color shade(Color c, float light) {
    return (Color){ (unsigned char)fminf(255.0f, c.r * light), (unsigned char)fminf(255.0f, c.g * light), (unsigned char)fminf(255.0f, c.b * light), 255 };
}

// Que textura lleva un triangulo de suelo (fija: la estacion la tiñe con el color).
static TerrainTex ground_tex(const float *a, float slope) {
    float hi = a[A_HIGH], fo = a[A_FOREST], fj = a[A_FJORD], de = a[A_DESERT];
    if (hi > 0.5f && a[A_EDGE] < 150.0f) return TT_ICE;
    if (a[A_H] > g_perma_snow && slope < 0.7f) return TT_SNOW;
    if (a[A_SHORE] > 0.0f && a[A_SHORE] < 0.5f) return hi + fj > 0.5f ? TT_GRAVEL : TT_MUD;
    if (de > 0.5f) return slope > 0.45f ? TT_STRATA : TT_SAND;
    if (slope > 0.6f) return TT_ROCK;
    if (hi > 0.5f) return TT_TUNDRA;
    if (fo > 0.5f) return TT_FOREST;
    if (fj > 0.5f) return TT_MOSS;
    return a[A_PATCH] > 0.55f ? TT_TALLGRASS : TT_TUSSOCK;
}

// Pinta los colores del chunk segun el aspecto actual (sin tocar la geometria).
static void paint_chunk(const Terrain *t, Chunk *ch, bool upload) {
    Mesh *mesh = &ch->model.meshes[0];
    const int tris = mesh->triangleCount;
    for (int i = 0; i < tris; i++) {
        const float *a = &ch->tri[i * TRI_ATTRS];
        Color base = a[A_KIND] == KIND_GROUND ? ground_color(&t->look, a, a[A_H] - t->plain) : tree_color(&t->look, a);
        Color col = shade(base, a[A_LIGHT] * (a[A_KIND] == KIND_GROUND ? g_tex_boost[ground_tex(a, a[A_SLOPE])] : 1.0f));
        for (int k = 0; k < 3; k++) {
            unsigned char *dst = &mesh->colors[(i * 3 + k) * 4];
            dst[0] = col.r;
            dst[1] = col.g;
            dst[2] = col.b;
            dst[3] = 255;
        }
    }
    if (upload) UpdateMeshBuffer(*mesh, 3, mesh->colors, mesh->vertexCount * 4, 0);
}

// ------------------------------------------------------------------ construccion
typedef struct {
    float *v, *n, *attr, *uv;
    int count, cap; // triangulos
    float local[6]; // coordenadas (u, v) dentro de la celda del atlas, de los tres vertices
} TriBuf;

static void push_tri(TriBuf *b, Vector3 p0, Vector3 p1, Vector3 p2, const float *attrs) {
    if (b->count >= b->cap) return;
    Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(p1, p0), Vector3Subtract(p2, p0)));
    Vector3 tri[3] = { p0, p1, p2 };
    for (int k = 0; k < 3; k++) {
        int v = b->count * 3 + k;
        b->v[v * 3 + 0] = tri[k].x, b->v[v * 3 + 1] = tri[k].y, b->v[v * 3 + 2] = tri[k].z;
        b->n[v * 3 + 0] = n.x, b->n[v * 3 + 1] = n.y, b->n[v * 3 + 2] = n.z;
    }
    float *at = &b->attr[b->count * TRI_ATTRS];
    memcpy(at, attrs, sizeof(float) * TRI_ATTRS);
    const Vector3 sun = Vector3Normalize((Vector3){ -0.45f, 1.0f, -0.3f });
    at[A_SLOPE] = 1.0f - n.y;
    at[A_LIGHT] = 0.55f + 0.45f * fmaxf(0.0f, Vector3DotProduct(n, sun)); // iluminacion horneada: cero costo en GPU
    // La celda del atlas (los arboles, la blanca) y un margen para no tomar la vecina.
    TerrainTex tt = at[A_KIND] == KIND_GROUND ? ground_tex(at, at[A_SLOPE]) : TT_WHITE;
    const float cw = 1.0f / TEX_COLS, eps = 0.5f / (32.0f * TEX_COLS);
    float u0 = (float)(tt % TEX_COLS) * cw, v0 = (float)(tt / TEX_COLS) * cw;
    // En lo empinado (paredes, muros) la textura va de pie: v sigue la altura, asi los
    // estratos quedan horizontales.
    bool wall = at[A_KIND] == KIND_GROUND && at[A_SLOPE] > 0.45f;
    float ybase = floorf(fminf(fminf(p0.y, p1.y), p2.y) / TEX_SPAN) * TEX_SPAN;
    float yspan = fmaxf(TEX_SPAN, fmaxf(fmaxf(p0.y, p1.y), p2.y) - ybase); // los muros altos: la celda entera en el triangulo
    for (int k = 0; k < 3; k++) {
        int v = b->count * 3 + k;
        float lu = at[A_KIND] == KIND_GROUND ? b->local[k * 2] : 0.5f, lv = at[A_KIND] == KIND_GROUND ? b->local[k * 2 + 1] : 0.5f;
        if (wall) lv = Clamp((tri[k].y - ybase) / yspan, 0.0f, 1.0f);
        b->uv[v * 2 + 0] = u0 + eps + lu * (cw - 2.0f * eps);
        b->uv[v * 2 + 1] = v0 + eps + lv * (cw - 2.0f * eps);
    }
    b->count++;
}

static uint32_t hash2(int x, int z, uint32_t seed) {
    uint32_t h = (uint32_t)x * 0x8DA6B343u ^ (uint32_t)z * 0xD8163841u ^ seed * 0xCB1AB31Fu;
    h ^= h >> 13;
    h *= 0x5BD1E995u;
    return h ^ (h >> 15);
}

// Un pino: tronco y dos copas conicas.
static void push_tree(TriBuf *b, Vector3 base, float height, float radius, float patch) {
    float at[TRI_ATTRS] = { 0 };
    at[A_H] = base.y + height * 0.6f;
    at[A_PATCH] = patch;
    at[A_KIND] = KIND_TRUNK;
    const int sides = 5;
    float tr = radius * 0.18f, th = height * 0.3f;
    for (int i = 0; i < sides; i++) {
        float a0 = (float)i / sides * 2.0f * PI, a1 = (float)(i + 1) / sides * 2.0f * PI;
        Vector3 b0 = { base.x + cosf(a0) * tr, base.y - 0.3f, base.z + sinf(a0) * tr }, b1 = { base.x + cosf(a1) * tr, base.y - 0.3f, base.z + sinf(a1) * tr };
        Vector3 t0 = { b0.x, base.y + th, b0.z }, t1 = { b1.x, base.y + th, b1.z };
        push_tri(b, b0, t0, b1, at);
        push_tri(b, b1, t0, t1, at);
    }
    at[A_KIND] = KIND_CANOPY;
    for (int tier = 0; tier < 2; tier++) {
        float y0 = base.y + th + tier * height * 0.3f, y1 = y0 + height * (tier ? 0.45f : 0.55f), r = radius * (tier ? 0.7f : 1.0f);
        Vector3 tip = { base.x, y1, base.z };
        for (int i = 0; i < sides + 1; i++) {
            float a0 = (float)i / (sides + 1) * 2.0f * PI, a1 = (float)(i + 1) / (sides + 1) * 2.0f * PI;
            Vector3 p0 = { base.x + cosf(a0) * r, y0, base.z + sinf(a0) * r }, p1 = { base.x + cosf(a1) * r, y0, base.z + sinf(a1) * r };
            push_tri(b, p0, tip, p1, at);
        }
    }
}

// Densidad de arboles: bosque cerrado; algunos en la costa y en las faldas del altiplano;
// pocos en la estepa (mas junto al agua); ninguno en el desierto.
static float tree_density(const float *k, float near_water) {
    return k[REGION_FOREST] * 0.62f + k[REGION_FJORD] * 0.12f + k[REGION_HIGHLAND] * 0.05f + k[REGION_STEPPE] * (0.01f + 0.08f * near_water);
}

static Model make_model(TriBuf *b) {
    Mesh mesh = { 0 };
    mesh.triangleCount = b->count;
    mesh.vertexCount = b->count * 3;
    mesh.vertices = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.normals = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.colors = MemAlloc(mesh.vertexCount * 4 * sizeof(unsigned char));
    mesh.texcoords = MemAlloc(mesh.vertexCount * 2 * sizeof(float));
    memcpy(mesh.vertices, b->v, mesh.vertexCount * 3 * sizeof(float));
    memcpy(mesh.normals, b->n, mesh.vertexCount * 3 * sizeof(float));
    memcpy(mesh.texcoords, b->uv, mesh.vertexCount * 2 * sizeof(float));
    Model m = LoadModelFromMesh(mesh);
    if (g_atlas.id) m.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = g_atlas;
    return m;
}

// Agua de un chunk: un cuadro plano por celda donde el agua (con la crecida maxima) queda
// sobre alguna esquina del suelo. Tres mallas: lagos, rios y canal, y mar; al dibujarlas,
// los lagos suben con la crecida, los rios con la mitad, el mar nada.
static void build_water(const Terrain *t, Chunk *ch, int cx, int cz) {
    enum { CELLS = CHUNK_CELLS / 2, MAXT = CELLS * CELLS * 2 };
    const float cell = CHUNK_SIZE / CELLS, ox = cx * CHUNK_SIZE, oz = cz * CHUNK_SIZE;
    static float verts[4][MAXT * 9];
    int count[4] = { 0, 0, 0, 0 };
    for (int iz = 0; iz < CELLS; iz++)
        for (int ix = 0; ix < CELLS; ix++) {
            float x0 = ox + ix * cell, z0 = oz + iz * cell, x1 = x0 + cell, z1 = z0 + cell;
            WaterKind k;
            float lv = world_water(t->world, x0 + cell * 0.5f, z0 + cell * 0.5f, 0.0f, &k);
            if (k == WATER_NONE) continue;
            float lowest = fminf(fminf(terrain_height(t, x0, z0), terrain_height(t, x1, z0)), fminf(terrain_height(t, x0, z1), terrain_height(t, x1, z1)));
            if (lv + (k == WATER_SEA ? 0.0f : TERRAIN_LAKE_FLOOD) < lowest) continue;
            int m = k == WATER_SEA ? W_SEA : k == WATER_LAKE ? W_LAKE : W_RIVER;
            if (m != W_SEA && world_region_weight(t->world, REGION_HIGHLAND, x0 + cell * 0.5f, z0 + cell * 0.5f) > 0.5f) m = W_GLACIAL; // deshielo turquesa
            float q[6][3] = { { x0, lv, z0 }, { x0, lv, z1 }, { x1, lv, z0 }, { x1, lv, z0 }, { x0, lv, z1 }, { x1, lv, z1 } };
            memcpy(&verts[m][count[m] * 9], q, sizeof(q));
            count[m] += 2;
        }
    for (int m = 0; m < 4; m++) {
        ch->has_water[m] = count[m] > 0;
        if (!count[m]) continue;
        Mesh mesh = { 0 };
        mesh.triangleCount = count[m];
        mesh.vertexCount = count[m] * 3;
        mesh.vertices = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
        mesh.normals = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
        memcpy(mesh.vertices, verts[m], mesh.vertexCount * 3 * sizeof(float));
        for (int i = 0; i < mesh.vertexCount; i++) mesh.normals[i * 3 + 1] = 1.0f;
        UploadMesh(&mesh, false);
        ch->water[m] = LoadModelFromMesh(mesh);
    }
}

static Chunk build_chunk(const Terrain *t, int cx, int cz) {
    enum { GROUND = CHUNK_CELLS * CHUNK_CELLS * 2, TREE_SLOTS = 12, TREES_MAX = TREE_SLOTS * TREE_SLOTS, TREE_TRIS = 22 };
    static float vbuf[(GROUND + TREES_MAX * TREE_TRIS) * 9], nbuf[(GROUND + TREES_MAX * TREE_TRIS) * 9];
    static float abuf[(GROUND + TREES_MAX * TREE_TRIS) * TRI_ATTRS], uvbuf[(GROUND + TREES_MAX * TREE_TRIS) * 6];
    TriBuf b = { vbuf, nbuf, abuf, uvbuf, 0, GROUND + TREES_MAX * TREE_TRIS, { 0 } };
    g_perma_snow = t->plain + 40.0f;
    const float cell = CHUNK_SIZE / CHUNK_CELLS;
    const float ox = cx * CHUNK_SIZE, oz = cz * CHUNK_SIZE;
    // El suelo: dos triangulos por celda, con los pesos de region de su centro.
    static float hgrid[CHUNK_CELLS + 1][CHUNK_CELLS + 1];
    for (int iz = 0; iz <= CHUNK_CELLS; iz++)
        for (int ix = 0; ix <= CHUNK_CELLS; ix++) hgrid[iz][ix] = terrain_height(t, ox + ix * cell, oz + iz * cell);
    for (int iz = 0; iz < CHUNK_CELLS; iz++) {
        for (int ix = 0; ix < CHUNK_CELLS; ix++) {
            float x0 = ox + ix * cell, x1 = x0 + cell, z0 = oz + iz * cell, z1 = z0 + cell;
            Vector3 p00 = { x0, hgrid[iz][ix], z0 }, p10 = { x1, hgrid[iz][ix + 1], z0 };
            Vector3 p01 = { x0, hgrid[iz + 1][ix], z1 }, p11 = { x1, hgrid[iz + 1][ix + 1], z1 };
            float mx = x0 + cell * 0.5f, mz = z0 + cell * 0.5f, k[REGION_COUNT];
            world_region_weights(t->world, mx, mz, k);
            float at[TRI_ATTRS] = { 0 };
            at[A_PATCH] = 0.5f + 0.5f * fbm2d(mx * 0.08f, mz * 0.08f, t->seed + 303u, 2); // manchas
            at[A_DESERT] = k[REGION_DESERT], at[A_FOREST] = k[REGION_FOREST], at[A_HIGH] = k[REGION_HIGHLAND], at[A_FJORD] = k[REGION_FJORD];
            float hm = (p00.y + p10.y + p01.y + p11.y) * 0.25f, wl = world_water(t->world, mx, mz, 0.0f, NULL);
            at[A_SHORE] = wl > -1e8f ? hm - wl : 99.0f;
            at[A_EDGE] = world_edge_distance(t->world, mx, mz);
            at[A_KIND] = KIND_GROUND;
            // La celda del atlas se repite cada TEX_SPAN m: cuatro celdas del suelo por textura.
            const int per = (int)(TEX_SPAN / cell);
            float u0 = (float)(((cx * CHUNK_CELLS + ix) % per + per) % per) / per, v0 = (float)(((cz * CHUNK_CELLS + iz) % per + per) % per) / per;
            float du = 1.0f / per;
            at[A_H] = (p00.y + p01.y + p10.y) / 3.0f;
            float l1[6] = { u0, v0, u0, v0 + du, u0 + du, v0 };
            memcpy(b.local, l1, sizeof(l1));
            push_tri(&b, p00, p01, p10, at);
            at[A_H] = (p10.y + p01.y + p11.y) / 3.0f;
            float l2[6] = { u0 + du, v0, u0, v0 + du, u0 + du, v0 + du };
            memcpy(b.local, l2, sizeof(l2));
            push_tri(&b, p10, p01, p11, at);
        }
    }
    // Arboles (deterministas por posicion): segun la region, nunca en el agua, el campamento ni lo empinado.
    const float slot = CHUNK_SIZE / TREE_SLOTS;
    for (int iz = 0; iz < TREE_SLOTS; iz++)
        for (int ix = 0; ix < TREE_SLOTS; ix++) {
            int gx = cx * TREE_SLOTS + ix, gz = cz * TREE_SLOTS + iz;
            uint32_t h = hash2(gx, gz, t->seed);
            float r0 = (float)(h & 0xFFFF) / 65535.0f, r1 = (float)((h >> 16) & 0xFF) / 255.0f, r2 = (float)(h >> 24) / 255.0f;
            float x = ox + (ix + 0.15f + 0.7f * r1) * slot, z = oz + (iz + 0.15f + 0.7f * r2) * slot;
            if (x * x + z * z < 120.0f * 120.0f || world_edge_distance(t->world, x, z) < 110.0f) continue;
            float k[REGION_COUNT];
            world_region_weights(t->world, x, z, k);
            float y = terrain_height(t, x, z), wl = world_water(t->world, x, z, 0.0f, NULL);
            float near_water = wl > -1e8f && y - wl < 6.0f ? 1.0f : 0.0f;
            if (r0 > tree_density(k, near_water)) continue;
            if (wl > y - 0.8f || y > t->look.snowline + 8.0f) continue; // en el agua o en el glaciar, no
            float sx = terrain_height(t, x + 2.0f, z) - y, sz = terrain_height(t, x, z + 2.0f) - y;
            if (sx * sx + sz * sz > 2.2f) continue;
            float dist;
            int si = world_nearest_settlement(t->world, x, z, &dist);
            if (si >= 0 && dist < (t->world->settlements[si].kind == SETTLE_CAPITAL ? 80.0f : 45.0f)) continue; // claros de los pueblos
            float height = 5.0f + 5.0f * r1 * (0.6f + 0.4f * k[REGION_FOREST]);
            push_tree(&b, (Vector3){ x, y, z }, height, 1.4f + 0.8f * r2, r2);
        }
    Chunk ch = { .loaded = true, .cx = cx, .cz = cz };
    ch.model = make_model(&b);
    ch.tri = MemAlloc(b.count * TRI_ATTRS * sizeof(float));
    memcpy(ch.tri, abuf, b.count * TRI_ATTRS * sizeof(float));
    paint_chunk(t, &ch, false);
    // Los colores cambian con las estaciones: buffer dinamico.
    UploadMesh(&ch.model.meshes[0], true);
    build_water(t, &ch, cx, cz);
    return ch;
}

static void free_chunk(Chunk *c) {
    UnloadModel(c->model);
    for (int m = 0; m < 4; m++)
        if (c->has_water[m]) UnloadModel(c->water[m]);
    MemFree(c->tri);
    c->tri = NULL;
    c->loaded = false;
    memset(c->has_water, 0, sizeof(c->has_water));
}

// ------------------------------------------------------------------ el horizonte lejano
// Una malla gruesa (una celda por chunk) que cubre FAR_CELLS chunks por lado alrededor del
// jugador, con un hueco donde estan los chunks cargados: asi las montañas, el muro, el hielo
// y el mar se ven a lo lejos. Sin arboles ni textura; el bosque se tiñe de copas oscuras y lo
// lejano se pierde en la bruma del cielo. Los datos del mundo de cada vertice se guardan y,
// al moverse el jugador, solo se piden los de las filas nuevas.
#define FAR_CELLS 100
#define FAR_VERTS (FAR_CELLS + 1)
#define FAR_HAZE_NEAR 350.0f
typedef struct {
    float h, water, k[REGION_COUNT], edge;
    unsigned char wkind;
} FarVert;
static struct {
    bool ready, valid;
    int gx0, gz0; // chunk de la esquina
    FarVert v[FAR_VERTS * FAR_VERTS], tmp[FAR_VERTS * FAR_VERTS];
    Model model;
    Color haze;
    TerrainLook look;
} g_far;

static void far_sample(const Terrain *t, FarVert *f, int gx, int gz) {
    float x = gx * CHUNK_SIZE, z = gz * CHUNK_SIZE;
    WaterKind wk;
    f->h = world_height(t->world, x, z);
    f->water = world_water(t->world, x, z, 0.0f, &wk);
    f->wkind = (unsigned char)wk;
    world_region_weights(t->world, x, z, f->k);
    f->edge = world_edge_distance(t->world, x, z);
}

// Mueve la ventana de datos a la nueva esquina: copia lo que ya estaba y pide lo nuevo.
static void far_shift(const Terrain *t, int gx0, int gz0) {
    for (int j = 0; j < FAR_VERTS; j++)
        for (int i = 0; i < FAR_VERTS; i++) {
            int oi = gx0 + i - g_far.gx0, oj = gz0 + j - g_far.gz0;
            FarVert *dst = &g_far.tmp[j * FAR_VERTS + i];
            if (g_far.valid && oi >= 0 && oj >= 0 && oi < FAR_VERTS && oj < FAR_VERTS) *dst = g_far.v[oj * FAR_VERTS + oi];
            else far_sample(t, dst, gx0 + i, gz0 + j);
        }
    memcpy(g_far.v, g_far.tmp, sizeof(g_far.v));
    g_far.gx0 = gx0, g_far.gz0 = gz0, g_far.valid = true;
}

static void far_fill(const Terrain *t) {
    Mesh *m = &g_far.model.meshes[0];
    const Vector3 sun = Vector3Normalize((Vector3){ -0.45f, 1.0f, -0.3f });
    const Color sea = { 36, 72, 104, 255 }, lake = { 52, 96, 128, 255 }, glacial = { 84, 168, 178, 255 };
    const Color ice = { 198, 220, 236, 255 };
    float ccx = (t->center_cx + 0.5f) * CHUNK_SIZE, ccz = (t->center_cz + 0.5f) * CHUNK_SIZE;
    g_perma_snow = t->plain + 40.0f;
    int tri = 0;
    for (int j = 0; j < FAR_CELLS; j++)
        for (int i = 0; i < FAR_CELLS; i++) {
            int gx = g_far.gx0 + i, gz = g_far.gz0 + j;
            bool hole = abs(gx - t->center_cx) <= CHUNK_RADIUS && abs(gz - t->center_cz) <= CHUNK_RADIUS;
            const FarVert *q[4] = { &g_far.v[j * FAR_VERTS + i], &g_far.v[j * FAR_VERTS + i + 1], &g_far.v[(j + 1) * FAR_VERTS + i],
                                    &g_far.v[(j + 1) * FAR_VERTS + i + 1] };
            Vector3 p[4];
            for (int c = 0; c < 4; c++)
                p[c] = (Vector3){ (gx + (c & 1)) * CHUNK_SIZE, fmaxf(q[c]->h, q[c]->water), (gz + (c >> 1)) * CHUNK_SIZE };
            const int idx[2][3] = { { 0, 2, 1 }, { 1, 2, 3 } };
            for (int s = 0; s < 2; s++, tri++) {
                float *v = &m->vertices[tri * 9];
                unsigned char *col = &m->colors[tri * 12];
                if (hole) { // triangulo degenerado: el hueco de los chunks cargados
                    memset(v, 0, 9 * sizeof(float));
                    continue;
                }
                const FarVert *a = q[idx[s][0]], *b = q[idx[s][1]], *c = q[idx[s][2]];
                Vector3 p0 = p[idx[s][0]], p1 = p[idx[s][1]], p2 = p[idx[s][2]];
                Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(p1, p0), Vector3Subtract(p2, p0)));
                float at[TRI_ATTRS] = { 0 };
                at[A_H] = (a->h + b->h + c->h) / 3.0f;
                at[A_SLOPE] = 1.0f - n.y;
                at[A_PATCH] = 0.5f;
                at[A_DESERT] = (a->k[REGION_DESERT] + b->k[REGION_DESERT] + c->k[REGION_DESERT]) / 3.0f;
                at[A_FOREST] = (a->k[REGION_FOREST] + b->k[REGION_FOREST] + c->k[REGION_FOREST]) / 3.0f;
                at[A_HIGH] = (a->k[REGION_HIGHLAND] + b->k[REGION_HIGHLAND] + c->k[REGION_HIGHLAND]) / 3.0f;
                at[A_FJORD] = (a->k[REGION_FJORD] + b->k[REGION_FJORD] + c->k[REGION_FJORD]) / 3.0f;
                at[A_SHORE] = 99.0f;
                at[A_EDGE] = (a->edge + b->edge + c->edge) / 3.0f;
                float water = (a->water + b->water + c->water) / 3.0f;
                Color base;
                if (water > at[A_H]) { // agua: el mar, los lagos y, en el altiplano, el deshielo turquesa
                    const FarVert *w = a->water > -1e8f ? a : b->water > -1e8f ? b : c;
                    base = w->wkind == WATER_SEA ? sea : at[A_HIGH] > 0.5f ? glacial : lake;
                    if (w->wkind != WATER_SEA) base = mixc(base, ice, Clamp(t->look.ice, 0.0f, 1.0f));
                } else {
                    base = ground_color(&t->look, at, at[A_H] - t->plain);
                    float trees = Clamp(tree_density(a->k, 0.0f) / 0.62f, 0.0f, 1.0f) * (at[A_H] < t->look.snowline ? 0.85f : 0.0f);
                    base = mixc(base, tree_color(&t->look, (float[TRI_ATTRS]){ [A_PATCH] = 0.6f, [A_KIND] = KIND_CANOPY, [A_H] = at[A_H] }), trees);
                    base = shade(base, 0.55f + 0.45f * fmaxf(0.0f, Vector3DotProduct(n, sun)));
                }
                // La bruma: lo lejano toma el color del cielo.
                float mx = (p0.x + p1.x + p2.x) / 3.0f - ccx, mz = (p0.z + p1.z + p2.z) / 3.0f - ccz;
                float d = sqrtf(mx * mx + mz * mz), hz = Clamp((d - FAR_HAZE_NEAR) / (FAR_CELLS * 0.5f * CHUNK_SIZE - FAR_HAZE_NEAR), 0.0f, 1.0f);
                Color fin = mixc(base, g_far.haze, 0.8f * hz * (2.0f - hz));
                Vector3 pts[3] = { p0, p1, p2 };
                for (int k = 0; k < 3; k++) {
                    v[k * 3 + 0] = pts[k].x, v[k * 3 + 1] = pts[k].y, v[k * 3 + 2] = pts[k].z;
                    col[k * 4 + 0] = fin.r, col[k * 4 + 1] = fin.g, col[k * 4 + 2] = fin.b, col[k * 4 + 3] = 255;
                }
            }
        }
    UpdateMeshBuffer(*m, 0, m->vertices, m->vertexCount * 3 * sizeof(float), 0);
    UpdateMeshBuffer(*m, 3, m->colors, m->vertexCount * 4, 0);
    g_far.look = t->look;
}

static void far_update(const Terrain *t) {
    if (!g_far.ready) {
        Mesh mesh = { 0 };
        mesh.triangleCount = FAR_CELLS * FAR_CELLS * 2;
        mesh.vertexCount = mesh.triangleCount * 3;
        mesh.vertices = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
        mesh.colors = MemAlloc(mesh.vertexCount * 4);
        UploadMesh(&mesh, true);
        g_far.model = LoadModelFromMesh(mesh);
        g_far.ready = true;
        if (!g_far.haze.a) g_far.haze = (Color){ 168, 196, 214, 255 };
    }
    far_shift(t, t->center_cx - FAR_CELLS / 2, t->center_cz - FAR_CELLS / 2);
    far_fill(t);
}

void terrain_set_haze(Terrain *t, Color haze) {
    Color o = g_far.haze;
    if (abs(o.r - haze.r) + abs(o.g - haze.g) + abs(o.b - haze.b) < 9) return;
    g_far.haze = haze;
    if (g_far.ready && t->has_center) far_fill(t);
}

void terrain_init(Terrain *t, unsigned seed) {
    if (!g_atlas_tried) { // una vez: el atlas sirve para cualquier mundo
        g_atlas_tried = true;
        Image img = LoadImage(platform_asset_path("assets/terrain/texturas.png"));
        if (img.data) {
            // Cuanto aclarar cada celda: lo que la lleve a un gris medio de 205.
            Color *px = LoadImageColors(img);
            int cell = img.width / TEX_COLS;
            for (int i = 0; i < 16 && cell > 0; i++) {
                float sum = 0.0f;
                for (int y = 0; y < cell; y++)
                    for (int x = 0; x < cell; x++) {
                        Color c = px[((i / TEX_COLS) * cell + y) * img.width + (i % TEX_COLS) * cell + x];
                        sum += (c.r + c.g + c.b) / 3.0f;
                    }
                g_tex_boost[i] = Clamp(205.0f / fmaxf(sum / (cell * cell), 1.0f), 1.0f, 1.6f);
            }
            UnloadImageColors(px);
            g_atlas = LoadTextureFromImage(img);
            SetTextureFilter(g_atlas, TEXTURE_FILTER_POINT); // pixeles nitidos (low-res)
            UnloadImage(img);
        }
    }
    memset(t, 0, sizeof(*t));
    g_far.valid = false; // otro mundo: el horizonte se vuelve a pedir
    t->seed = seed;
    t->world = world_for_seed(seed);
    t->plain = terrain_plain_height(t);
    // Aspecto inicial: verano seco, sin nieve.
    t->look = (TerrainLook){ .greenness = 0.4f, .snowline = t->plain + 40.0f };
}

static bool look_changed(const TerrainLook *a, const TerrainLook *b) {
    return fabsf(a->greenness - b->greenness) > 0.02f || fabsf(a->autumn - b->autumn) > 0.02f ||
           fabsf(a->snow_cover - b->snow_cover) > 0.02f || fabsf(a->wetness - b->wetness) > 0.03f ||
           fabsf(a->snowline - b->snowline) > 0.25f;
}

void terrain_set_look(Terrain *t, const TerrainLook *look) {
    bool repaint = look_changed(&t->look, look);
    if (!repaint) { // la crecida y el hielo solo cambian el agua, no el suelo
        t->look.ice = look->ice;
        t->look.flood = look->flood;
        return;
    }
    t->look = *look;
    for (int i = 0; i < CHUNK_SLOTS; i++)
        if (t->slots[i].loaded) paint_chunk(t, &t->slots[i], true);
    if (g_far.ready && t->has_center) far_fill(t);
}

void terrain_draw_water(const Terrain *t, float time) {
    if (!t->has_center) return;
    const Color water = { 52, 96, 128, 200 }, river = { 62, 112, 140, 200 }, sea = { 36, 72, 104, 220 }, ice = { 198, 220, 236, 240 };
    float ripple = 0.06f * sinf(time * 1.3f); // oleaje leve: el brillo del agua respira (el hielo no)
    const Color glacial = { 84, 168, 178, 210 };
    for (int m = 0; m < 4; m++) {
        Color col = m == W_SEA ? sea : m == W_RIVER ? river : m == W_GLACIAL ? glacial : water;
        float frozen = m == W_SEA ? 0.0f : Clamp(t->look.ice, 0.0f, 1.0f);
        Color c = mixc(col, ice, frozen);
        c.a = (unsigned char)Lerp(col.a, ice.a, frozen);
        c = ColorBrightness(c, ripple * (1.0f - frozen));
        float lift = m == W_SEA ? 0.0f : m == W_LAKE ? t->look.flood : 0.5f * t->look.flood;
        for (int i = 0; i < CHUNK_SLOTS; i++) {
            const Chunk *ch = &t->slots[i];
            if (ch->loaded && ch->has_water[m]) DrawModel(ch->water[m], (Vector3){ 0, lift, 0 }, 1.0f, c);
        }
    }
}

static bool in_range(int cx, int cz, int ccx, int ccz) {
    return abs(cx - ccx) <= CHUNK_RADIUS && abs(cz - ccz) <= CHUNK_RADIUS;
}

void terrain_update(Terrain *t, Vector3 pos) {
    int ccx = (int)floorf(pos.x / CHUNK_SIZE);
    int ccz = (int)floorf(pos.z / CHUNK_SIZE);
    if (t->has_center && ccx == t->center_cx && ccz == t->center_cz) return;
    t->center_cx = ccx;
    t->center_cz = ccz;
    t->has_center = true;

    // 1) Libera los chunks que quedaron fuera del radio.
    for (int i = 0; i < CHUNK_SLOTS; i++) {
        Chunk *c = &t->slots[i];
        if (c->loaded && !in_range(c->cx, c->cz, ccx, ccz)) free_chunk(c);
    }
    // 2) Genera los que faltan en los huecos libres.
    for (int dz = -CHUNK_RADIUS; dz <= CHUNK_RADIUS; dz++) {
        for (int dx = -CHUNK_RADIUS; dx <= CHUNK_RADIUS; dx++) {
            int cx = ccx + dx, cz = ccz + dz;
            bool present = false;
            for (int i = 0; i < CHUNK_SLOTS && !present; i++)
                present = t->slots[i].loaded && t->slots[i].cx == cx && t->slots[i].cz == cz;
            if (present) continue;
            for (int i = 0; i < CHUNK_SLOTS; i++) {
                if (!t->slots[i].loaded) {
                    t->slots[i] = build_chunk(t, cx, cz);
                    break;
                }
            }
        }
    }
    far_update(t);
}

void terrain_draw(const Terrain *t) {
    if (g_far.ready && t->has_center) DrawModel(g_far.model, (Vector3){ 0, -0.4f, 0 }, 1.0f, WHITE); // bajo los chunks: sin costuras a la vista
    for (int i = 0; i < CHUNK_SLOTS; i++)
        if (t->slots[i].loaded) DrawModel(t->slots[i].model, (Vector3){ 0 }, 1.0f, WHITE);
}

void terrain_unload(Terrain *t) {
    for (int i = 0; i < CHUNK_SLOTS; i++) {
        if (t->slots[i].loaded) free_chunk(&t->slots[i]);
    }
    if (g_far.ready) UnloadModel(g_far.model);
    g_far.ready = g_far.valid = false;
    t->has_center = false;
}
