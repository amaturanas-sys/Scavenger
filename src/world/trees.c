#include "world/trees.h"

#include <math.h>
#include <string.h>

#include "platform.h"
#include "raymath.h"
#include "sim/voxfile.h"
#include "world/voxstruct.h"

#define TREE_LODS 3
static const char *const IDS[TREE_KINDS] = { "mapa.vegetacion.pino", "mapa.vegetacion.sauce", "mapa.vegetacion.arce" };
// Los detalles: el .vox tal cual; celdas del doble (llena si lo estan 3 de sus 8); del cuadruple (16 de 64).
static const int LOD_F[TREE_LODS] = { 1, 2, 4 }, LOD_NEED[TREE_LODS] = { 1, 3, 16 };

typedef struct {
    bool tried, ok;
    Model model;
} TreeMesh;

typedef struct {
    bool tried, ok;
    VoxGrid grid; // el .vox leido: las estaciones y los detalles se arman de aca
    uint8_t pal[256][4];
    float height, radius; // m, a escala 1: el alto del inventario y la mitad de lo ancho
    float crown;          // donde empieza la copa (la hoja mas baja), por el alto
    TreeMesh mesh[TREES_SEASONS][TREE_LODS];
} Species;

static Species g_sp[TREE_KINDS];
static const Inventory *g_inv;
static Vector3 g_eye, g_fwd, g_target; // el ojo y a quien mira (el jugador)
static float g_cone = -1.0f, g_near0 = 34.0f, g_near1 = 72.0f, g_far = 1e9f;
static int g_drawn, g_drawn_lod[TREE_LODS];

// Las hojas: lo verde de la paleta (el tronco y las ramas son pardos o grises).
static bool leafy(const uint8_t *c) { return c[1] > c[0] + 6 && c[1] >= c[2]; }

void trees_init(const Inventory *inv) { g_inv = inv; }

static bool load_species(TreeKind k) {
    Species *s = &g_sp[k];
    if (s->tried) return s->ok;
    s->tried = true;
    const InvItem *it = g_inv ? inventory_find(g_inv, IDS[k]) : NULL;
    if (!it || !it->voxel) return false;
    char path[160];
    inventory_path(it, path, sizeof(path));
    const char *full = platform_asset_path(path);
    if (!platform_asset_exists(full)) return false;
    int len = 0;
    unsigned char *data = LoadFileData(full, &len);
    if (!data) return false;
    bool ok = len > 0 && voxfile_read(data, (size_t)len, 0.1f, &s->grid, s->pal);
    UnloadFileData(data);
    platform_note_asset(full, ok);
    if (!ok) return false;
    vox_paint_hidden(&s->grid); // lo de adentro, del color de su piel: se ve al bajar el detalle y en los pelados
    s->height = it->h > 0.0f ? it->h : (float)s->grid.ny * 0.1f;
    s->grid.size = s->height / (float)s->grid.ny;
    s->grid.on_ground = s->grid.merge = true;
    s->radius = 0.5f * (float)(s->grid.nx > s->grid.nz ? s->grid.nx : s->grid.nz) * s->grid.size;
    s->crown = 1.0f;
    for (int y = 0, done = 0; y < s->grid.ny && !done; y++)
        for (int z = 0; z < s->grid.nz && !done; z++)
            for (int x = 0; x < s->grid.nx && !done; x++)
                if (leafy(s->pal[vox_get(&s->grid, x, y, z)]) && vox_get(&s->grid, x, y, z)) s->crown = (float)y / (float)s->grid.ny, done = 1;
    s->ok = true;
    return true;
}


static float brightness(const uint8_t *c) { return ((float)c[0] + (float)c[1] + (float)c[2]) / 765.0f; }

// Un color de la rampa (oscuro, medio, claro) segun el brillo de la hoja que reemplaza (de 0,1 a 0,5).
static void ramp(uint8_t *out, const uint8_t dark[3], const uint8_t mid[3], const uint8_t light[3], float b) {
    float t = fminf(fmaxf((b - 0.12f) / 0.36f, 0.0f), 1.0f);
    const uint8_t *a = t < 0.5f ? dark : mid, *z = t < 0.5f ? mid : light;
    float u = t < 0.5f ? t * 2.0f : (t - 0.5f) * 2.0f;
    for (int i = 0; i < 3; i++) out[i] = (uint8_t)((float)a[i] + ((float)z[i] - (float)a[i]) * u + 0.5f);
}

static bool build_mesh(const Species *s, TreeKind k, TreeSeason season, int lod, Model *out) {
    VoxGrid g;
    if (!vox_init(&g, s->grid.nx, s->grid.ny, s->grid.nz, s->grid.size)) return false;
    memcpy(g.m, s->grid.m, (size_t)g.nx * (size_t)g.ny * (size_t)g.nz);
    g.on_ground = g.merge = true;
    static uint8_t pal[256][4]; // solo al armar la malla (nada por cuadro)
    memcpy(pal, s->pal, sizeof(pal));
    bool leaf[256] = { false };
    for (int i = 1; i < 256; i++) leaf[i] = leafy(pal[i]);
    size_t n = (size_t)g.nx * (size_t)g.ny * (size_t)g.nz;
    if (k != TREE_PINE && (season == TREES_BARE || season == TREES_SNOW)) { // pelado: el tronco y las ramas
        for (size_t i = 0; i < n; i++)
            if (leaf[g.m[i]]) g.m[i] = 0;
    } else if (k != TREE_PINE && season == TREES_AUTUMN) { // el arce rojo y naranja; el sauce amarillo
        static const uint8_t MAPLE[3][3] = { { 138, 34, 22 }, { 196, 70, 26 }, { 236, 144, 42 } };
        static const uint8_t WILLOW[3][3] = { { 146, 128, 38 }, { 194, 172, 58 }, { 230, 208, 98 } };
        const uint8_t(*r)[3] = k == TREE_MAPLE ? MAPLE : WILLOW;
        for (int i = 1; i < 256; i++)
            if (leaf[i]) ramp(pal[i], r[0], r[1], r[2], brightness(pal[i]));
    } else if (k == TREE_PINE && season == TREES_SNOW) { // la nieve sobre cada piso de ramas
        bool used[256] = { false };
        for (size_t i = 0; i < n; i++) used[g.m[i]] = true;
        int snow = 255;
        while (snow > 1 && used[snow]) snow--;
        pal[snow][0] = 232, pal[snow][1] = 236, pal[snow][2] = 242, pal[snow][3] = 255;
        for (int y = g.ny - 1; y >= 0; y--) // de arriba abajo: la de abajo de una celda nevada ya no tiene aire encima
            for (int z = 0; z < g.nz; z++)
                for (int x = 0; x < g.nx; x++) {
                    uint8_t v = vox_get(&g, x, y, z);
                    if (v && leaf[v] && !vox_get(&g, x, y + 1, z)) vox_set(&g, x, y, z, (uint8_t)snow);
                }
    }
    if (LOD_F[lod] > 1) {
        VoxGrid d;
        bool ok = vox_downsample(&g, LOD_F[lod], LOD_NEED[lod], &d);
        vox_free(&g);
        if (!ok) return false;
        g = d;
    }
    bool ok = vox_count(&g) > 0 && voxs_grid_model(&g, (const uint8_t(*)[4])pal, out, false, 0);
    vox_free(&g);
    return ok;
}

static Model *mesh_for(TreeKind k, TreeSeason season, int lod) {
    Species *s = &g_sp[k];
    // El pino no cambia en otoño ni al caer las hojas; el arce y el sauce, con nieve, estan pelados.
    if (k == TREE_PINE && (season == TREES_AUTUMN || season == TREES_BARE)) season = TREES_SUMMER;
    if (k != TREE_PINE && season == TREES_SNOW) season = TREES_BARE;
    TreeMesh *m = &s->mesh[season][lod];
    if (!m->tried) {
        m->tried = true;
        m->ok = build_mesh(s, k, season, lod, &m->model);
    }
    return m->ok ? &m->model : NULL;
}

void trees_begin(Camera3D cam) {
    g_eye = cam.position, g_target = cam.target;
    g_fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    float hy = tanf(cam.fovy * 0.5f * DEG2RAD), hx = hy * 16.0f / 9.0f;
    g_cone = cosf(fminf(atanf(sqrtf(hx * hx + hy * hy)) + 0.15f, 1.5f));
    bool touch = platform_touch_ui(); // en Android, el detalle de cerca solo al lado
    g_near0 = touch ? 16.0f : 34.0f, g_near1 = touch ? 40.0f : 72.0f, g_far = touch ? 85.0f : 1e9f; // y no tantos de lejos
    g_drawn = 0;
    memset(g_drawn_lod, 0, sizeof(g_drawn_lod));
}

// Sin el .vox (un asset que falta): el cono del pino o la bola de un frondoso, del alto que tendria.
static void fallback(TreeKind kind, Vector3 at, float h, Color tint) {
    Color wood = ColorTint((Color){ 96, 70, 46, 255 }, tint), leaf = ColorTint((Color){ 74, 104, 60, 255 }, tint);
    DrawCylinder(at, 0.18f * h / 6.0f, 0.25f * h / 6.0f, h * 0.35f, 5, wood);
    if (kind == TREE_PINE) DrawCylinder((Vector3){ at.x, at.y + h * 0.3f, at.z }, 0.0f, h * 0.32f, h * 0.7f, 6, leaf);
    else DrawSphereEx((Vector3){ at.x, at.y + h * 0.62f, at.z }, h * 0.36f, 4, 6, leaf);
}

void trees_draw(TreeKind kind, TreeSeason season, Vector3 at, float scale, float yaw, Color tint) {
    if ((unsigned)kind >= TREE_KINDS) return;
    if (!load_species(kind)) {
        fallback(kind, at, (kind == TREE_PINE ? 10.0f : 9.0f) * scale, tint);
        return;
    }
    const Species *s = &g_sp[kind];
    float h = s->height * scale, r = fmaxf(s->radius * scale, h * 0.5f);
    Vector3 to = { at.x - g_eye.x, at.y + h * 0.5f - g_eye.y, at.z - g_eye.z };
    float d = Vector3Length(to);
    if (g_cone > -1.0f && d > r && Vector3DotProduct(to, g_fwd) < g_cone * d - r) return; // detras o al costado de la camara
    if (d > g_far) return;
    // Una copa que atraviesa lo que va de la camara al jugador (la camara lejana, en el bosque): ese arbol no se dibuja,
    // que no lo tape (el tronco, si la camara va baja, si).
    float dx = g_target.x - g_eye.x, dz = g_target.z - g_eye.z, dd = dx * dx + dz * dz;
    float u = dd > 1e-6f ? ((at.x - g_eye.x) * dx + (at.z - g_eye.z) * dz) / dd : 0.0f;
    u = u < 0.0f ? 0.0f : u > 1.0f ? 1.0f : u;
    float qx = g_eye.x + dx * u - at.x, qz = g_eye.z + dz * u - at.z, qy = g_eye.y + (g_target.y - g_eye.y) * u;
    float rc = s->radius * scale * 0.85f + 0.3f;
    if (qx * qx + qz * qz < rc * rc && qy > at.y + s->crown * h - 0.3f && qy < at.y + h + 0.3f) return;
    float near = d - r * 0.5f;
    if ((unsigned)season >= TREES_SEASONS) season = TREES_SUMMER;
    Model *m = NULL;
    int lod = near < g_near0 ? 0 : near < g_near1 ? 1 : 2;
    while (lod >= 0 && !(m = mesh_for(kind, season, lod))) lod--; // pelado, de lejos puede no quedar nada: el de mas detalle
    if (!m) return;
    DrawModelEx(*m, at, (Vector3){ 0, 1, 0 }, yaw * RAD2DEG, (Vector3){ scale, scale, scale }, tint);
    g_drawn++;
    g_drawn_lod[lod]++;
}

float trees_height(TreeKind kind) { return (unsigned)kind < TREE_KINDS && load_species(kind) ? g_sp[kind].height : 0.0f; }

int trees_drawn(int per_lod[3]) {
    if (per_lod) memcpy(per_lod, g_drawn_lod, sizeof(g_drawn_lod));
    return g_drawn;
}

static void forget(bool unload) {
    for (int k = 0; k < TREE_KINDS; k++)
        for (int se = 0; se < TREES_SEASONS; se++)
            for (int l = 0; l < TREE_LODS; l++) {
                TreeMesh *m = &g_sp[k].mesh[se][l];
                if (unload && m->ok) UnloadModel(m->model);
                m->tried = m->ok = false;
            }
}

void trees_unload(void) {
    forget(true);
    for (int k = 0; k < TREE_KINDS; k++) {
        if (g_sp[k].ok) vox_free(&g_sp[k].grid);
        g_sp[k].tried = g_sp[k].ok = false;
    }
}

void trees_forget_gpu(void) { forget(false); } // el contexto de GL ya no esta: se arman de nuevo al pedirlas
