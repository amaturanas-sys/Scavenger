#include "clouds.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "raymath.h"
#include "sim/voxel.h"
#include "world/sky.h"

#define CELL 230.0f // m: una nube posible por celda (en el marco que arrastra el viento)
#define PUFFS_MAX 6

static const Vector2 WIND_DIR = { 0.86f, 0.5f }; // el mismo viento que la lluvia (weather.c)

static Model g_puff;
static bool g_ready;
// El atlas de nubes pixeladas (assets/sky/nubes.png, tools/assets/nubes.py): 8 celdas de 64x32
// en 2 columnas. Filas: cumulos, estratos, cubierto, tormenta.
static Texture2D g_atlas;
static bool g_atlas_tried;
// Nubes de voxeles (al modo de Nubis, src/sim/voxel.h): plantillas por tipo, con la luz del sol
// horneada en el color; se vuelven a iluminar cuando el sol se mueve. Lejos, el cartel del atlas.
#define VT_PER_KIND 3
#define VT_COUNT (VCLOUD_KINDS * VT_PER_KIND)
#define VOX_NEAR 950.0f // m: hasta aqui, voxeles; mas lejos, carteles
static const int VT_DIM[VCLOUD_KINDS][3] = { { 20, 8, 20 }, { 26, 5, 18 }, { 22, 12, 22 }, { 18, 7, 18 } };
static const float VT_SIZE = 8.0f; // m por voxel
static struct {
    VoxGrid g;
    VoxMesh m;
    Model model;
    bool ok;
} g_vt[VT_COUNT];
static bool g_vt_ready;
static Vector3 g_lit_sun;
static Vector2 g_drift; // cuanto arrastro el viento la capa
static float g_last_time = -1.0f;

static uint32_t hash3(int x, int z, uint32_t k) {
    uint32_t h = (uint32_t)x * 0x8DA6B343u ^ (uint32_t)z * 0xD8163841u ^ k * 0xCB1AB31Fu;
    h ^= h >> 13;
    h *= 0x5BD1E995u;
    return h ^ (h >> 15);
}

static float unit(uint32_t h, int shift) { return (float)((h >> shift) & 0xFF) / 255.0f; }

// Un bollo: esfera low-poly con el tope claro y la panza gris (sombra horneada).
static void ensure_puff(void) {
    if (g_ready) return;
    Mesh s = GenMeshSphere(1.0f, 4, 7);
    Mesh m = { 0 };
    m.vertexCount = s.vertexCount;
    m.triangleCount = s.triangleCount;
    m.vertices = MemAlloc(m.vertexCount * 3 * sizeof(float));
    memcpy(m.vertices, s.vertices, m.vertexCount * 3 * sizeof(float));
    if (s.indices) {
        m.indices = MemAlloc(m.triangleCount * 3 * sizeof(unsigned short));
        memcpy(m.indices, s.indices, m.triangleCount * 3 * sizeof(unsigned short));
    }
    m.colors = MemAlloc(m.vertexCount * 4);
    for (int i = 0; i < m.vertexCount; i++) {
        float y = m.vertices[i * 3 + 1], x = m.vertices[i * 3 + 0];
        float v = 0.62f + 0.3f * Clamp(y * 0.5f + 0.5f, 0.0f, 1.0f) + 0.08f * x; // luz desde arriba y el este
        unsigned char c = (unsigned char)(255.0f * Clamp(v, 0.0f, 1.0f));
        m.colors[i * 4 + 0] = c, m.colors[i * 4 + 1] = c, m.colors[i * 4 + 2] = (unsigned char)fminf(255.0f, c * 1.03f + 4.0f);
        m.colors[i * 4 + 3] = 255;
    }
    UnloadMesh(s);
    UploadMesh(&m, false);
    g_puff = LoadModelFromMesh(m);
    g_ready = true;
}

static void vt_upload(int i) {
    Mesh *mesh = &g_vt[i].model.meshes[0];
    memcpy(mesh->colors, g_vt[i].m.col, (size_t)g_vt[i].m.tris * 12);
    UpdateMeshBuffer(*mesh, 3, mesh->colors, g_vt[i].m.tris * 12, 0);
}

// Las plantillas: se arman una vez; se vuelven a iluminar si el sol se movio (unos 8 grados).
static void ensure_templates(Vector3 sun) {
    if (!g_vt_ready) {
        g_vt_ready = true;
        for (int i = 0; i < VT_COUNT; i++) {
            int k = i / VT_PER_KIND;
            if (!vox_init(&g_vt[i].g, VT_DIM[k][0], VT_DIM[k][1], VT_DIM[k][2], VT_SIZE)) continue;
            vox_cloud_shape(&g_vt[i].g, (VoxCloudKind)k, 101u + (uint32_t)i * 37u);
            vox_cloud_light(&g_vt[i].g, sun.x, sun.y, sun.z);
            if (!vox_mesh(&g_vt[i].g, NULL, &g_vt[i].m) || !g_vt[i].m.tris) continue;
            Mesh mesh = { 0 };
            mesh.triangleCount = g_vt[i].m.tris;
            mesh.vertexCount = g_vt[i].m.tris * 3;
            mesh.vertices = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
            mesh.normals = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
            mesh.colors = MemAlloc(mesh.vertexCount * 4);
            memcpy(mesh.vertices, g_vt[i].m.pos, mesh.vertexCount * 3 * sizeof(float));
            memcpy(mesh.normals, g_vt[i].m.nrm, mesh.vertexCount * 3 * sizeof(float));
            memcpy(mesh.colors, g_vt[i].m.col, (size_t)mesh.vertexCount * 4);
            UploadMesh(&mesh, true); // los colores cambian con el sol
            g_vt[i].model = LoadModelFromMesh(mesh);
            g_vt[i].ok = true;
        }
        g_lit_sun = sun;
        return;
    }
    if (Vector3DotProduct(sun, g_lit_sun) > 0.99f) return;
    g_lit_sun = sun;
    for (int i = 0; i < VT_COUNT; i++) {
        if (!g_vt[i].ok) continue;
        vox_cloud_light(&g_vt[i].g, sun.x, sun.y, sun.z);
        vox_mesh_free(&g_vt[i].m);
        if (vox_mesh(&g_vt[i].g, NULL, &g_vt[i].m)) vt_upload(i);
    }
}

void clouds_forget_gpu(void) {
    g_ready = g_vt_ready = false;
    for (int i = 0; i < VT_COUNT; i++) g_vt[i].ok = false; // las rejillas se rehacen (fugan poco, una vez)
    g_atlas = (Texture2D){ 0 };
    g_atlas_tried = false;
}

void clouds_unload(void) {
    if (g_ready) UnloadModel(g_puff);
    g_ready = false;
    for (int i = 0; i < VT_COUNT; i++) {
        if (g_vt[i].ok) UnloadModel(g_vt[i].model);
        vox_mesh_free(&g_vt[i].m);
        vox_free(&g_vt[i].g);
        g_vt[i].ok = false;
    }
    g_vt_ready = false;
    if (g_atlas.id) UnloadTexture(g_atlas);
    g_atlas = (Texture2D){ 0 };
    g_atlas_tried = false;
}

static void ensure_atlas(void) {
    if (g_atlas_tried) return;
    g_atlas_tried = true;
    const char *path = platform_asset_path("assets/sky/nubes.png");
    if (platform_asset_exists(path)) g_atlas = LoadTexture(path);
    if (g_atlas.id) SetTextureFilter(g_atlas, TEXTURE_FILTER_POINT); // pixeles nitidos
    platform_note_asset("assets/sky/nubes.png", g_atlas.id != 0);
}

// Que celda del atlas usa una nube: con poca cobertura, cumulos; con mas, estratos y cielo
// cubierto; con tormenta, nubarrones.
static int cloud_tile(float cover, uint32_t h) {
    float r = unit(h, 4);
    int row = cover < 0.35f ? (r < 0.8f ? 0 : 1) : cover < 0.75f ? (r < 0.4f ? 0 : r < 0.7f ? 1 : 2) : (r < 0.6f ? 3 : 2);
    return row * 2 + (int)((h >> 13) & 1u);
}

// El tipo de nube de voxeles de cada fila del atlas: cumulos, estratos, cubierto, tormenta.
static VoxCloudKind tile_kind(int tile) {
    static const VoxCloudKind K[4] = { VCLOUD_CUMULUS, VCLOUD_STRATUS, VCLOUD_STRATUS, VCLOUD_STORM };
    return K[(tile / 2) & 3];
}

typedef struct {
    Vector3 p;
    float w, d2;
    int tile;
    Color c;
} Sprite;

static int by_distance(const void *a, const void *b) {
    float da = ((const Sprite *)a)->d2, db = ((const Sprite *)b)->d2;
    return da < db ? 1 : da > db ? -1 : 0; // de lejos a cerca
}

float clouds_base(const Terrain *t) { return t->plain + CLOUD_ABOVE_PLAIN; }

// El viento arrastra la capa: se integra la velocidad (asi un cambio de viento no la hace saltar).
static void advance(float time, float wind) {
    float dt = time - g_last_time;
    if (g_last_time < 0.0f || dt < 0.0f || dt > 30.0f) { // primera vez o salto (cargar partida)
        g_drift = Vector2Scale(WIND_DIR, 6.0f * time);
    } else {
        g_drift = Vector2Add(g_drift, Vector2Scale(WIND_DIR, (3.0f + 14.0f * wind) * dt));
    }
    g_last_time = time;
}

// La nube de una celda (en el marco del viento), o false si no hay. puffs: x, y, z, radio.
static int cell_cloud(int cx, int cz, uint32_t seed, float base, float cover, float out[PUFFS_MAX][4]) {
    uint32_t h = hash3(cx, cz, seed);
    float chance = 0.06f + 0.85f * cover;
    if (unit(h, 0) > chance) return 0;
    int n = 2 + (int)(unit(h, 8) * (PUFFS_MAX - 2.0f)); // a lo sumo PUFFS_MAX
    float size = (0.55f + 0.45f * unit(h, 16)) * (0.7f + 0.5f * cover);
    float ox = (cx + 0.2f + 0.6f * unit(h, 24)) * CELL, oz = (cz + 0.2f + 0.6f * unit(hash3(cz, cx, seed), 0)) * CELL;
    for (int i = 0; i < n; i++) {
        uint32_t k = hash3(cx * 7 + i, cz * 13 - i, seed ^ 0x9E37u);
        float a = unit(k, 0) * 2.0f * PI, d = unit(k, 8) * 55.0f * size;
        out[i][0] = ox + cosf(a) * d;
        out[i][2] = oz + sinf(a) * d;
        out[i][3] = (26.0f + 26.0f * unit(k, 16)) * size;
        out[i][1] = base + out[i][3] * 0.2f + 6.0f * unit(k, 24);
    }
    return n;
}

static Color mixc(Color a, Color b, float k) {
    k = Clamp(k, 0.0f, 1.0f);
    return (Color){ (unsigned char)Lerp(a.r, b.r, k), (unsigned char)Lerp(a.g, b.g, k), (unsigned char)Lerp(a.b, b.b, k), 255 };
}

static void draw_puff(Vector3 p, float r, Color c) {
    DrawModelEx(g_puff, p, (Vector3){ 0, 1, 0 }, 0.0f, (Vector3){ r, r * 0.42f, r }, c);
}

void clouds_draw(const Terrain *t, Camera3D cam, float time, float cover, float wind, Color haze) {
    ensure_puff();
    ensure_atlas();
    advance(time, wind);
    float base = clouds_base(t);
    Color white = mixc((Color){ 246, 247, 250, 255 }, (Color){ 158, 162, 170, 255 }, (cover - 0.4f) / 0.6f);
    // La camara en el marco del viento.
    float lx = cam.position.x - g_drift.x, lz = cam.position.z - g_drift.y;
    int r = (int)(CLOUD_VIEW / CELL) + 1, c0x = (int)floorf(lx / CELL), c0z = (int)floorf(lz / CELL);
    ensure_templates(sky_sun_dir(time));
    float puffs[PUFFS_MAX][4];
    static Sprite spr[1024];
    int ns = 0;
    for (int dz = -r; dz <= r; dz++)
        for (int dx = -r; dx <= r; dx++) {
            int cx = c0x + dx, cz = c0z + dz, n = cell_cloud(cx, cz, t->seed, base, cover, puffs);
            if (!n) continue;
            uint32_t h = hash3(cx, cz, t->seed ^ 0xC10Du);
            int tile = cloud_tile(cover, h), vt = (int)tile_kind(tile) * VT_PER_KIND + (int)(h % VT_PER_KIND);
            // El centro de la nube: el primer bollo (en el suelo de la capa).
            Vector3 c = { puffs[0][0] + g_drift.x, base, puffs[0][2] + g_drift.y };
            float d = Vector2Distance((Vector2){ c.x, c.z }, (Vector2){ cam.position.x, cam.position.z });
            if (d > CLOUD_VIEW) continue;
            Color col = mixc(white, haze, (d - 500.0f) / (CLOUD_VIEW - 500.0f));
            float scale = 0.6f + 0.55f * unit(h, 20) * (0.6f + 0.6f * cover);
            if (g_vt[vt].ok && d < VOX_NEAR) {
                DrawModelEx(g_vt[vt].model, c, (Vector3){ 0, 1, 0 }, 0.0f, (Vector3){ scale, scale, scale }, col);
                continue;
            }
            for (int i = 0; i < n; i++) { // lejos: los carteles del atlas (o los bollos, sin atlas)
                Vector3 p = { puffs[i][0] + g_drift.x, puffs[i][1], puffs[i][2] + g_drift.y };
                if (!g_atlas.id) draw_puff(p, puffs[i][3], col);
                else if (ns < 1024) spr[ns++] = (Sprite){ p, puffs[i][3] * 2.6f, Vector3DistanceSqr(p, cam.position), tile, col };
            }
        }
    // Los gorros de las cumbres altas: una nube de voxeles en anillo alrededor de la cima.
    const World *w = t->world;
    for (int i = 0; i < w->peak_count; i++) {
        const Peak *pk = &w->peaks[i];
        float top = world_height(w, pk->x, pk->z);
        if (top < base - 12.0f) continue;
        float d = Vector2Distance((Vector2){ pk->x, pk->z }, (Vector2){ cam.position.x, cam.position.z });
        if (d > CLOUD_VIEW * 1.4f) continue;
        Color col = mixc(white, haze, (d - 700.0f) / (CLOUD_VIEW * 1.4f - 700.0f));
        int vt = VCLOUD_CAP * VT_PER_KIND + i % VT_PER_KIND;
        Vector3 c = { pk->x + WIND_DIR.x * 20.0f + 6.0f * sinf(time * 0.01f + i), fminf(top - 30.0f, base - 4.0f), pk->z + WIND_DIR.y * 20.0f };
        if (g_vt[vt].ok) DrawModelEx(g_vt[vt].model, c, (Vector3){ 0, 1, 0 }, 0.0f, (Vector3){ 1.0f + 0.3f * cover, 1.0f, 1.0f + 0.3f * cover }, col);
    }
    if (!ns) return;
    // Las nubes del atlas: carteles de cara a la camara, de lejos a cerca (la transparencia).
    qsort(spr, (size_t)ns, sizeof(Sprite), by_distance);
    for (int i = 0; i < ns; i++) {
        Rectangle src = { (float)(spr[i].tile % 2) * 64.0f, (float)(spr[i].tile / 2) * 32.0f, 64.0f, 32.0f };
        DrawBillboardRec(cam, g_atlas, src, spr[i].p, (Vector2){ spr[i].w, spr[i].w * 0.5f }, spr[i].c);
    }
}

// Densidad de una plantilla en un punto del mundo (centro de la base c, escala sx/sy/sz).
static float template_density(int vt, Vector3 c, Vector3 scale, Vector3 pos) {
    if (vt < 0 || vt >= VT_COUNT || !g_vt[vt].ok) return 0.0f;
    const VoxGrid *g = &g_vt[vt].g;
    float lx = (pos.x - c.x) / scale.x / g->size + g->nx * 0.5f, ly = (pos.y - c.y) / scale.y / g->size, lz = (pos.z - c.z) / scale.z / g->size + g->nz * 0.5f;
    return vox_get(g, (int)floorf(lx), (int)floorf(ly), (int)floorf(lz)) / 255.0f;
}

float clouds_mist(const Terrain *t, Vector3 pos, float time, float cover) {
    float base = clouds_base(t);
    if (pos.y < base - 60.0f || pos.y > base + 120.0f) return 0.0f;
    float mist = 0.0f, puffs[PUFFS_MAX][4];
    float lx = pos.x - g_drift.x, lz = pos.z - g_drift.y;
    int c0x = (int)floorf(lx / CELL), c0z = (int)floorf(lz / CELL);
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            int cx = c0x + dx, cz = c0z + dz;
            if (!cell_cloud(cx, cz, t->seed, base, cover, puffs)) continue;
            uint32_t h = hash3(cx, cz, t->seed ^ 0xC10Du);
            int vt = (int)tile_kind(cloud_tile(cover, h)) * VT_PER_KIND + (int)(h % VT_PER_KIND);
            float scale = 0.6f + 0.55f * unit(h, 20) * (0.6f + 0.6f * cover);
            Vector3 c = { puffs[0][0] + g_drift.x, base, puffs[0][2] + g_drift.y };
            mist = fmaxf(mist, fminf(1.0f, template_density(vt, c, (Vector3){ scale, scale, scale }, pos) * 2.5f));
        }
    const World *w = t->world;
    for (int i = 0; i < w->peak_count; i++) {
        const Peak *pk = &w->peaks[i];
        float top = world_height(w, pk->x, pk->z);
        if (top < base - 12.0f) continue;
        Vector3 c = { pk->x + WIND_DIR.x * 20.0f + 6.0f * sinf(time * 0.01f + i), fminf(top - 30.0f, base - 4.0f), pk->z + WIND_DIR.y * 20.0f };
        int vt = VCLOUD_CAP * VT_PER_KIND + i % VT_PER_KIND;
        mist = fmaxf(mist, fminf(1.0f, template_density(vt, c, (Vector3){ 1.0f + 0.3f * cover, 1.0f, 1.0f + 0.3f * cover }, pos) * 2.5f));
    }
    return mist;
}

void clouds_draw_mist(float mist, int w, int h) {
    if (mist <= 0.01f) return;
    DrawRectangle(0, 0, w, h, (Color){ 226, 230, 236, (unsigned char)(235.0f * mist) });
}
