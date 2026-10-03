#include "terrain.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../sim/noise.h"
#include "raymath.h"

// Campamento inicial en el origen: el terreno se aplana a su alrededor.
#define CAMP_FLAT_INNER 18.0f
#define CAMP_FLAT_OUTER 42.0f

static float smoothstepf(float a, float b, float x) {
    float t = Clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float terrain_height(const Terrain *t, float x, float z) {
    float hills = fbm2d(x * 0.006f, z * 0.006f, t->seed, 5) * 16.0f;  // colinas amplias
    float bumps = fbm2d(x * 0.04f, z * 0.04f, t->seed + 17u, 3) * 1.2f; // ondulacion fina
    float h = hills + bumps;
    float d = sqrtf(x * x + z * z);
    // Cordilleras lejos del campamento: crestas que guardan nieve y glaciares.
    float region = smoothstepf(0.05f, 0.45f, fbm2d(x * 0.0016f, z * 0.0016f, t->seed + 101u, 3));
    if (region > 0.0f) {
        float ridge = 1.0f - fabsf(fbm2d(x * 0.005f, z * 0.005f, t->seed + 202u, 4));
        h += region * ridge * ridge * 58.0f * smoothstepf(150.0f, 320.0f, d);
    }
    float k = smoothstepf(CAMP_FLAT_INNER, CAMP_FLAT_OUTER, d);
    float base = fbm2d(0.0f, 0.0f, t->seed, 5) * 16.0f; // altura del llano del campamento
    return base + (h - base) * k;
}

float terrain_plain_height(const Terrain *t) { return fbm2d(0.0f, 0.0f, t->seed, 5) * 16.0f; }

static Color mixc(Color a, Color b, float k) {
    k = Clamp(k, 0.0f, 1.0f);
    return (Color){ (unsigned char)Lerp(a.r, b.r, k), (unsigned char)Lerp(a.g, b.g, k), (unsigned char)Lerp(a.b, b.b, k),
                    255 };
}

// Paleta de estepa segun la estacion: pasto verde, seco, ocre o dormido; tierra
// y roca en las pendientes; barro con lluvia; nieve, hielo y glaciares.
// h: altura relativa al llano; patch en [0, 1]: manchas (nieve a medio cubrir).
static Color ground_color(const TerrainLook *L, float lake_base, float h, float h_abs, float slope, float patch) {
    const Color green = { 112, 150, 70, 255 }, dry = { 188, 168, 106, 255 }, ochre = { 172, 124, 68, 255 };
    const Color dormant = { 140, 128, 98, 255 }, dirt = { 140, 110, 76, 255 }, rock = { 120, 116, 108, 255 };
    const Color snow = { 226, 232, 240, 255 }, glacier = { 196, 218, 236, 255 }, mud = { 112, 96, 72, 255 };
    const Color lakebed = { 158, 140, 108, 255 };
    Color grass = mixc(dry, green, L->greenness);
    grass = mixc(grass, ochre, L->autumn * 0.85f);
    grass = mixc(dormant, grass, 0.35f + 0.65f * Clamp(L->greenness + L->autumn, 0.0f, 1.0f));
    if (h > 6.0f) grass = mixc(grass, dry, 0.45f); // lomas mas secas
    Color c = grass;
    bool bare = false;
    if (slope > 0.45f || h > 18.0f) c = dirt, bare = true;
    if (slope > 0.75f || (h > 24.0f && slope > 0.35f)) c = rock, bare = true;
    // Orillas: el lecho que el agua dejo al bajar, y el barro junto al agua.
    float above_water = h_abs - L->water_level;
    if (h_abs < lake_base + TERRAIN_LAKE_FLOOD && above_water > 0.0f) c = above_water < 0.4f ? mud : mixc(c, lakebed, 0.7f);
    if (!bare) c = mixc(c, mud, L->wetness * 0.35f); // suelo mojado
    // Nieve: cubre el llano segun la estacion (en manchas a medio cubrir); menos en lo empinado.
    float cover = L->snow_cover * (slope > 0.75f ? 0.55f : 1.0f);
    if (patch < cover * 1.15f - 0.05f) c = snow;
    // Glaciares y nieve permanente en las cumbres; una franja de manchas por debajo.
    float above = h_abs - L->snowline;
    if (above > 0.0f) c = slope < 0.7f ? glacier : mixc(rock, snow, 0.5f);
    else if (above > -6.0f && patch < (above + 6.0f) / 6.0f) c = snow;
    return c;
}

static Color shade(Color c, float light) {
    return (Color){ (unsigned char)(c.r * light), (unsigned char)(c.g * light), (unsigned char)(c.b * light), 255 };
}

// Pinta los colores del chunk segun el aspecto actual (sin tocar la geometria).
static void paint_chunk(const Terrain *t, Chunk *ch, bool upload) {
    Mesh *mesh = &ch->model.meshes[0];
    const int tris = mesh->triangleCount;
    for (int i = 0; i < tris; i++) {
        const float *a = &ch->tri[i * 4];
        Color col = shade(ground_color(&t->look, t->lake_base, a[0] - t->plain, a[0], a[1], a[3]), a[2]);
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

static Chunk build_chunk(const Terrain *t, int cx, int cz) {
    const int tris = CHUNK_CELLS * CHUNK_CELLS * 2;
    Mesh mesh = { 0 };
    mesh.triangleCount = tris;
    mesh.vertexCount = tris * 3;
    mesh.vertices = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.normals = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.colors = MemAlloc(mesh.vertexCount * 4 * sizeof(unsigned char));
    float *attr = MemAlloc(tris * 4 * sizeof(float));

    const float cell = CHUNK_SIZE / CHUNK_CELLS;
    const float ox = cx * CHUNK_SIZE, oz = cz * CHUNK_SIZE;
    const Vector3 sun = Vector3Normalize((Vector3){ -0.45f, 1.0f, -0.3f });
    int v = 0;

    for (int iz = 0; iz < CHUNK_CELLS; iz++) {
        for (int ix = 0; ix < CHUNK_CELLS; ix++) {
            float x0 = ox + ix * cell, x1 = x0 + cell;
            float z0 = oz + iz * cell, z1 = z0 + cell;
            Vector3 p00 = { x0, terrain_height(t, x0, z0), z0 };
            Vector3 p10 = { x1, terrain_height(t, x1, z0), z0 };
            Vector3 p01 = { x0, terrain_height(t, x0, z1), z1 };
            Vector3 p11 = { x1, terrain_height(t, x1, z1), z1 };
            Vector3 quads[2][3] = { { p00, p01, p10 }, { p10, p01, p11 } };
            for (int q = 0; q < 2; q++) {
                Vector3 a = quads[q][0], b = quads[q][1], c = quads[q][2];
                Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
                float slope = 1.0f - n.y;
                float hmid = (a.y + b.y + c.y) / 3.0f;
                // Iluminacion horneada: ambiente + difusa. Cero costo en GPU.
                float light = 0.55f + 0.45f * fmaxf(0.0f, Vector3DotProduct(n, sun));
                float cx_ = (a.x + b.x + c.x) / 3.0f, cz_ = (a.z + b.z + c.z) / 3.0f;
                int ti = v / 3;
                attr[ti * 4 + 0] = hmid;
                attr[ti * 4 + 1] = slope;
                attr[ti * 4 + 2] = light;
                attr[ti * 4 + 3] = 0.5f + 0.5f * fbm2d(cx_ * 0.08f, cz_ * 0.08f, t->seed + 303u, 2); // manchas
                Vector3 tri[3] = { a, b, c };
                for (int k = 0; k < 3; k++, v++) {
                    mesh.vertices[v * 3 + 0] = tri[k].x;
                    mesh.vertices[v * 3 + 1] = tri[k].y;
                    mesh.vertices[v * 3 + 2] = tri[k].z;
                    mesh.normals[v * 3 + 0] = n.x;
                    mesh.normals[v * 3 + 1] = n.y;
                    mesh.normals[v * 3 + 2] = n.z;
                }
            }
        }
    }
    Chunk ch = { .loaded = true, .cx = cx, .cz = cz, .model = LoadModelFromMesh(mesh), .tri = attr };
    paint_chunk(t, &ch, false);
    // Los colores cambian con las estaciones: buffer dinamico.
    UploadMesh(&ch.model.meshes[0], true);
    return ch;
}

static void free_chunk(Chunk *c) {
    UnloadModel(c->model);
    MemFree(c->tri);
    c->tri = NULL;
    c->loaded = false;
}

static int cmp_float(const void *a, const void *b) {
    float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

void terrain_init(Terrain *t, unsigned seed) {
    memset(t, 0, sizeof(*t));
    t->seed = seed;
    t->plain = terrain_plain_height(t);
    // Nivel base de los lagos: el percentil TERRAIN_LAKE_SHARE de las alturas en 1.2 km a la redonda.
    enum { N = 41 };
    static float samples[N * N];
    int k = 0;
    for (int iz = 0; iz < N; iz++)
        for (int ix = 0; ix < N; ix++)
            samples[k++] = terrain_height(t, -600.0f + ix * 30.0f, -600.0f + iz * 30.0f);
    qsort(samples, N * N, sizeof(float), cmp_float);
    t->lake_base = fminf(samples[(int)(TERRAIN_LAKE_SHARE * N * N)], t->plain - TERRAIN_LAKE_DEPTH - TERRAIN_LAKE_FLOOD);
    // Aspecto inicial: verano seco, sin nieve.
    t->look = (TerrainLook){ .greenness = 0.4f, .snowline = t->plain + 40.0f,
                             .water_level = t->lake_base };
}

static bool look_changed(const TerrainLook *a, const TerrainLook *b) {
    return fabsf(a->greenness - b->greenness) > 0.02f || fabsf(a->autumn - b->autumn) > 0.02f ||
           fabsf(a->snow_cover - b->snow_cover) > 0.02f || fabsf(a->wetness - b->wetness) > 0.03f ||
           fabsf(a->snowline - b->snowline) > 0.25f || fabsf(a->water_level - b->water_level) > 0.05f;
}

void terrain_set_look(Terrain *t, const TerrainLook *look) {
    bool repaint = look_changed(&t->look, look);
    float ice = look->ice;
    if (!repaint) {
        t->look.ice = ice; // el hielo solo cambia el agua, no el suelo
        return;
    }
    t->look = *look;
    for (int i = 0; i < CHUNK_SLOTS; i++)
        if (t->slots[i].loaded) paint_chunk(t, &t->slots[i], true);
}

void terrain_draw_water(const Terrain *t, float time) {
    if (!t->has_center) return;
    const float span = (2 * CHUNK_RADIUS + 1) * CHUNK_SIZE;
    Vector3 c = { (t->center_cx + 0.5f) * CHUNK_SIZE, t->look.water_level, (t->center_cz + 0.5f) * CHUNK_SIZE };
    const Color water = { 52, 96, 128, 200 }, ice = { 198, 220, 236, 240 };
    Color col = mixc(water, ice, t->look.ice);
    col.a = (unsigned char)Lerp(water.a, ice.a, Clamp(t->look.ice, 0.0f, 1.0f));
    // Oleaje leve: el brillo del agua respira (el hielo no).
    float ripple = (1.0f - t->look.ice) * 0.06f * sinf(time * 1.3f);
    col = ColorBrightness(col, ripple);
    DrawPlane(c, (Vector2){ span, span }, col);
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
}

void terrain_draw(const Terrain *t) {
    for (int i = 0; i < CHUNK_SLOTS; i++)
        if (t->slots[i].loaded) DrawModel(t->slots[i].model, (Vector3){ 0 }, 1.0f, WHITE);
}

void terrain_unload(Terrain *t) {
    for (int i = 0; i < CHUNK_SLOTS; i++) {
        if (t->slots[i].loaded) free_chunk(&t->slots[i]);
    }
    t->has_center = false;
}
