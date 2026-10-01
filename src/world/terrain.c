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
    float k = smoothstepf(CAMP_FLAT_INNER, CAMP_FLAT_OUTER, d);
    float base = fbm2d(0.0f, 0.0f, t->seed, 5) * 16.0f; // altura del llano del campamento
    return base + (h - base) * k;
}

// Paleta de estepa: pasto seco, tierra y roca segun altura y pendiente.
static Color ground_color(float h, float slope) {
    Color grass = { 168, 160, 88, 255 };
    Color dry = { 190, 170, 110, 255 };
    Color dirt = { 140, 110, 76, 255 };
    Color rock = { 120, 116, 108, 255 };
    Color c = h > 6.0f ? dry : grass;
    if (slope > 0.45f) c = dirt;
    if (slope > 0.75f) c = rock;
    return c;
}

static Color shade(Color c, float light) {
    return (Color){ (unsigned char)(c.r * light), (unsigned char)(c.g * light), (unsigned char)(c.b * light), 255 };
}

static Model build_chunk(const Terrain *t, int cx, int cz) {
    const int tris = CHUNK_CELLS * CHUNK_CELLS * 2;
    Mesh mesh = { 0 };
    mesh.triangleCount = tris;
    mesh.vertexCount = tris * 3;
    mesh.vertices = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.normals = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.colors = MemAlloc(mesh.vertexCount * 4 * sizeof(unsigned char));

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
                Color col = shade(ground_color(hmid, slope), light);
                Vector3 tri[3] = { a, b, c };
                for (int k = 0; k < 3; k++, v++) {
                    mesh.vertices[v * 3 + 0] = tri[k].x;
                    mesh.vertices[v * 3 + 1] = tri[k].y;
                    mesh.vertices[v * 3 + 2] = tri[k].z;
                    mesh.normals[v * 3 + 0] = n.x;
                    mesh.normals[v * 3 + 1] = n.y;
                    mesh.normals[v * 3 + 2] = n.z;
                    mesh.colors[v * 4 + 0] = col.r;
                    mesh.colors[v * 4 + 1] = col.g;
                    mesh.colors[v * 4 + 2] = col.b;
                    mesh.colors[v * 4 + 3] = 255;
                }
            }
        }
    }
    UploadMesh(&mesh, false);
    return LoadModelFromMesh(mesh);
}

void terrain_init(Terrain *t, unsigned seed) {
    memset(t, 0, sizeof(*t));
    t->seed = seed;
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
        if (c->loaded && !in_range(c->cx, c->cz, ccx, ccz)) {
            UnloadModel(c->model);
            c->loaded = false;
        }
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
                    t->slots[i] = (Chunk){ .loaded = true, .cx = cx, .cz = cz, .model = build_chunk(t, cx, cz) };
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
        if (t->slots[i].loaded) UnloadModel(t->slots[i].model);
        t->slots[i].loaded = false;
    }
    t->has_center = false;
}
