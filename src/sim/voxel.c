#include "voxel.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
static float smoothstep(float a, float b, float v) {
    float t = clamp01((v - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

bool vox_init(VoxGrid *g, int nx, int ny, int nz, float size) {
    memset(g, 0, sizeof(*g));
    if (nx <= 0 || ny <= 0 || nz <= 0) return false;
    g->m = calloc((size_t)nx * ny * nz, 1);
    if (!g->m) return false;
    g->nx = nx, g->ny = ny, g->nz = nz, g->size = size;
    return true;
}

void vox_free(VoxGrid *g) {
    free(g->m);
    free(g->light);
    memset(g, 0, sizeof(*g));
}

static int idx(const VoxGrid *g, int x, int y, int z) { return x + g->nx * (z + g->nz * y); }

uint8_t vox_get(const VoxGrid *g, int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= g->nx || y >= g->ny || z >= g->nz) return 0;
    return g->m[idx(g, x, y, z)];
}

void vox_set(VoxGrid *g, int x, int y, int z, uint8_t v) {
    if (x < 0 || y < 0 || z < 0 || x >= g->nx || y >= g->ny || z >= g->nz) return;
    g->m[idx(g, x, y, z)] = v;
}

int vox_count(const VoxGrid *g) {
    int n = 0, total = g->nx * g->ny * g->nz;
    for (int i = 0; i < total; i++) n += g->m[i] != 0;
    return n;
}

void vox_box(VoxGrid *g, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t mat) {
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) vox_set(g, x, y, z, mat);
}

void vox_carve(VoxGrid *g, int x0, int y0, int z0, int x1, int y1, int z1) { vox_box(g, x0, y0, z0, x1, y1, z1, 0); }

void vox_cylinder(VoxGrid *g, float cx, float cz, int y0, int y1, float r0, float r1, uint8_t mat) {
    for (int y = y0; y <= y1; y++) {
        float t = y1 > y0 ? (float)(y - y0) / (float)(y1 - y0) : 0.0f, r = r0 + (r1 - r0) * t;
        for (int z = (int)floorf(cz - r - 1); z <= (int)ceilf(cz + r + 1); z++)
            for (int x = (int)floorf(cx - r - 1); x <= (int)ceilf(cx + r + 1); x++) {
                float dx = x + 0.5f - cx, dz = z + 0.5f - cz;
                if (dx * dx + dz * dz <= r * r) vox_set(g, x, y, z, mat);
            }
    }
}

void vox_ellipsoid(VoxGrid *g, float cx, float cy, float cz, float rx, float ry, float rz, uint8_t mat, bool half) {
    for (int y = (int)floorf(cy - ry); y <= (int)ceilf(cy + ry); y++) {
        if (half && y + 0.5f < cy) continue;
        for (int z = (int)floorf(cz - rz); z <= (int)ceilf(cz + rz); z++)
            for (int x = (int)floorf(cx - rx); x <= (int)ceilf(cx + rx); x++) {
                float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry, dz = (z + 0.5f - cz) / rz;
                if (dx * dx + dy * dy + dz * dz <= 1.0f) vox_set(g, x, y, z, mat);
            }
    }
}

bool vox_refine(VoxGrid *g) {
    VoxGrid r;
    if (!vox_init(&r, g->nx * 2, g->ny * 2, g->nz * 2, g->size * 0.5f)) return false;
    for (int y = 0; y < r.ny; y++)
        for (int z = 0; z < r.nz; z++)
            for (int x = 0; x < r.nx; x++) r.m[idx(&r, x, y, z)] = g->m[idx(g, x / 2, y / 2, z / 2)];
    // Redondeo: una pasada que decide con la rejilla original refinada (no en cascada).
    uint8_t *out = malloc((size_t)r.nx * r.ny * r.nz);
    if (out) {
        memcpy(out, r.m, (size_t)r.nx * r.ny * r.nz);
        for (int y = 1; y < r.ny; y++) // la fila de abajo (cimiento) no se toca
            for (int z = 0; z < r.nz; z++)
                for (int x = 0; x < r.nx; x++) {
                    int full = 0;
                    uint8_t near = 0;
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dz = -1; dz <= 1; dz++)
                            for (int dx = -1; dx <= 1; dx++) {
                                if (!dx && !dy && !dz) continue;
                                uint8_t v = vox_get(&r, x + dx, y + dy, z + dz);
                                if (v) full++, near = v;
                            }
                    int i = idx(&r, x, y, z);
                    if (r.m[i] && full <= 9) out[i] = 0;          // esquina saliente: se gasta
                    else if (!r.m[i] && full >= 19) out[i] = near; // rincon: se llena
                }
        memcpy(r.m, out, (size_t)r.nx * r.ny * r.nz);
        free(out);
    }
    r.jitter = g->jitter;
    vox_free(g);
    *g = r;
    return true;
}

// ------------------------------------------------------------------ ruido
static uint32_t hash4(int x, int y, int z, uint32_t s) {
    uint32_t h = (uint32_t)x * 0x8DA6B343u ^ (uint32_t)y * 0xD8163841u ^ (uint32_t)z * 0xCB1AB31Fu ^ s * 0x165667B1u;
    h ^= h >> 13;
    h *= 0x5BD1E995u;
    return h ^ (h >> 15);
}

static float lattice(int x, int y, int z, uint32_t s) { return (float)(hash4(x, y, z, s) & 0xFFFF) / 65535.0f; }

float vox_noise3(float x, float y, float z, uint32_t seed) {
    int x0 = (int)floorf(x), y0 = (int)floorf(y), z0 = (int)floorf(z);
    float tx = x - x0, ty = y - y0, tz = z - z0;
    tx = tx * tx * (3 - 2 * tx), ty = ty * ty * (3 - 2 * ty), tz = tz * tz * (3 - 2 * tz);
    float c[2][2][2];
    for (int k = 0; k < 2; k++)
        for (int j = 0; j < 2; j++)
            for (int i = 0; i < 2; i++) c[k][j][i] = lattice(x0 + i, y0 + j, z0 + k, seed);
    float a = (c[0][0][0] * (1 - tx) + c[0][0][1] * tx) * (1 - ty) + (c[0][1][0] * (1 - tx) + c[0][1][1] * tx) * ty;
    float b = (c[1][0][0] * (1 - tx) + c[1][0][1] * tx) * (1 - ty) + (c[1][1][0] * (1 - tx) + c[1][1][1] * tx) * ty;
    return a * (1 - tz) + b * tz;
}

static float fbm3(float x, float y, float z, uint32_t seed) {
    return 0.55f * vox_noise3(x, y, z, seed) + 0.3f * vox_noise3(x * 2.1f, y * 2.1f, z * 2.1f, seed + 1u) +
           0.15f * vox_noise3(x * 4.3f, y * 4.3f, z * 4.3f, seed + 2u);
}

void vox_erode(VoxGrid *g, uint32_t seed, float amount) {
    for (int y = 0; y < g->ny; y++) {
        float h = powf((y + 0.5f) / (float)g->ny, 0.7f);
        for (int z = 0; z < g->nz; z++)
            for (int x = 0; x < g->nx; x++) {
                if (!g->m[idx(g, x, y, z)]) continue;
                if (fbm3(x * 0.45f, y * 0.45f, z * 0.45f, seed) < amount * h) g->m[idx(g, x, y, z)] = 0;
            }
    }
}

// ------------------------------------------------------------------ nubes (Nubis)
void vox_cloud_shape(VoxGrid *g, VoxCloudKind kind, uint32_t seed) {
    memset(g->m, 0, (size_t)g->nx * g->ny * g->nz);
    // Perfil dimensional: gradiente de abajo (sube rapido: base plana), de arriba (baja hacia la
    // cima, mas alto en el centro: cumulo de coliflor) y del borde (de afuera hacia adentro).
    float base_soft = kind == VCLOUD_STRATUS ? 0.25f : 0.1f;
    float top_center = kind == VCLOUD_STRATUS ? 0.55f : kind == VCLOUD_STORM ? 1.0f : kind == VCLOUD_CAP ? 0.75f : 0.95f;
    float top_edge = kind == VCLOUD_STRATUS ? 0.4f : 0.3f;
    float freq = kind == VCLOUD_STRATUS ? 0.16f : 0.22f;
    float erosion = kind == VCLOUD_STORM ? 0.85f : 1.0f;
    for (int y = 0; y < g->ny; y++) {
        float v = (y + 0.5f) / (float)g->ny;
        for (int z = 0; z < g->nz; z++)
            for (int x = 0; x < g->nx; x++) {
                float ex = ((x + 0.5f) / g->nx - 0.5f) * 2.0f, ez = ((z + 0.5f) / g->nz - 0.5f) * 2.0f;
                float r = sqrtf(ex * ex + ez * ez);
                if (kind == VCLOUD_CAP) r = fabsf(r - 0.55f) * 1.8f; // gorro: un anillo alrededor de la cima
                float edge = 1.0f - smoothstep(0.55f, 1.0f, r);
                float bottom = smoothstep(0.0f, base_soft, v);
                float top_h = top_edge + (top_center - top_edge) * (1.0f - smoothstep(0.0f, 0.9f, r));
                float top = 1.0f - smoothstep(top_h - 0.25f, top_h, v);
                float profile = bottom * top * edge;
                float n = fbm3(x * freq, y * freq * 1.4f, z * freq, seed);
                // Billowy (cumulos) o deshilachada (estratos).
                if (kind == VCLOUD_STRATUS) n = 0.6f * n + 0.4f * vox_noise3(x * 0.5f, y * 0.9f, z * 0.08f, seed + 9u);
                float d = clamp01(n - (1.0f - profile) * erosion);
                if (d > 0.04f) g->m[idx(g, x, y, z)] = (uint8_t)(1 + d * 254.0f);
            }
    }
}

void vox_cloud_light(VoxGrid *g, float sx, float sy, float sz) {
    int total = g->nx * g->ny * g->nz;
    if (!g->light) g->light = calloc((size_t)total, 1);
    if (!g->light) return;
    if (sy < 0.15f) sy = 0.15f; // el sol bajo (o la luna): luz rasante
    float l = sqrtf(sx * sx + sy * sy + sz * sz);
    sx /= l, sy /= l, sz /= l;
    const float sigma = 0.11f; // extincion por celda a densidad plena
    for (int y = 0; y < g->ny; y++)
        for (int z = 0; z < g->nz; z++)
            for (int x = 0; x < g->nx; x++) {
                int i = idx(g, x, y, z);
                if (!g->m[i]) continue;
                // Profundidad optica hacia el sol (marcha por la rejilla).
                float od = 0.0f, px = x + 0.5f, py = y + 0.5f, pz = z + 0.5f;
                for (int s = 0; s < 48; s++) {
                    px += sx, py += sy, pz += sz;
                    int qx = (int)floorf(px), qy = (int)floorf(py), qz = (int)floorf(pz);
                    if (qx < 0 || qy < 0 || qz < 0 || qx >= g->nx || qy >= g->ny || qz >= g->nz) break;
                    od += g->m[idx(g, qx, qy, qz)] / 255.0f;
                }
                float trans = expf(-sigma * od * 6.0f);
                // Polvo (Beer-Powder): el borde de cara al sol, apenas mas oscuro que el interior iluminado.
                float dens = g->m[i] / 255.0f, powder = 1.0f - 0.35f * expf(-dens * 6.0f);
                // Ambiente: entra por arriba y por los bordes.
                float hv = (y + 0.5f) / g->ny, ambient = 0.42f + 0.3f * sqrtf(hv);
                float e = ambient + 0.62f * trans * powder;
                g->light[i] = (uint8_t)(255.0f * clamp01(e / 1.2f));
            }
}

// ------------------------------------------------------------------ malla
static const int FACE_N[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
// Las 4 esquinas de cada cara (en unidades de celda, desde la esquina menor), en orden antihorario visto desde fuera.
static const int FACE_V[6][4][3] = {
    { { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 }, { 1, 0, 1 } }, // +x
    { { 0, 0, 1 }, { 0, 1, 1 }, { 0, 1, 0 }, { 0, 0, 0 } }, // -x
    { { 0, 1, 0 }, { 0, 1, 1 }, { 1, 1, 1 }, { 1, 1, 0 } }, // +y
    { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } }, // -y
    { { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 }, { 0, 0, 1 } }, // +z
    { { 0, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 }, { 1, 0, 0 } }, // -z
};
static const float FACE_SHADE[6] = { 0.82f, 0.74f, 1.0f, 0.55f, 0.88f, 0.68f };

// Una cara que se arma: da al aire y no es la de abajo de la base, tapada por el suelo (on_ground; el borde de la
// base, si: una ladera lo deja a la vista).
static bool face_shown(const VoxGrid *g, int x, int y, int z, int f) {
    if (vox_get(g, x + FACE_N[f][0], y + FACE_N[f][1], z + FACE_N[f][2])) return false;
    if (f != 3 || y != 0 || !g->on_ground) return true;
    return !(vox_get(g, x - 1, 0, z) && vox_get(g, x + 1, 0, z) && vox_get(g, x, 0, z - 1) && vox_get(g, x, 0, z + 1));
}

int vox_exposed_faces(const VoxGrid *g) {
    int n = 0;
    for (int y = 0; y < g->ny; y++)
        for (int z = 0; z < g->nz; z++)
            for (int x = 0; x < g->nx; x++) {
                if (!g->m[idx(g, x, y, z)]) continue;
                for (int f = 0; f < 6; f++) n += face_shown(g, x, y, z, f);
            }
    return n;
}

// Oclusion ambiental de una esquina: cuantas de las 3 celdas vecinas del lado de afuera estan llenas (0..3).
static int corner_occ(const VoxGrid *g, int x, int y, int z, int f, const int *corner) {
    int n[3] = { FACE_N[f][0], FACE_N[f][1], FACE_N[f][2] };
    int ox = x + n[0], oy = y + n[1], oz = z + n[2]; // la celda de afuera
    int s[3] = { 0, 0, 0 }, t[3] = { 0, 0, 0 }, k = 0;
    // Las dos direcciones del plano de la cara, hacia la esquina.
    for (int a = 0; a < 3; a++) {
        if (n[a] != 0) continue;
        int d = corner[a] ? 1 : -1;
        if (k == 0) s[0] = s[1] = s[2] = 0, s[a] = d;
        else t[0] = t[1] = t[2] = 0, t[a] = d;
        k++;
    }
    int side1 = vox_get(g, ox + s[0], oy + s[1], oz + s[2]) != 0, side2 = vox_get(g, ox + t[0], oy + t[1], oz + t[2]) != 0;
    int diag = vox_get(g, ox + s[0] + t[0], oy + s[1] + t[1], oz + s[2] + t[2]) != 0;
    return side1 && side2 ? 3 : side1 + side2 + diag;
}

static float occ_shade(int occ) { return 1.0f - 0.16f * (float)occ; }

static float corner_ao(const VoxGrid *g, int x, int y, int z, int f, const int *corner) {
    return occ_shade(corner_occ(g, x, y, z, f, corner));
}

// ------------------------------------------------------------------------------ caras juntas (merge)
// Los ejes de cada cara: el de su normal y los dos de su plano (a y b), en el orden en que se barren. Tambien
// los barre asi tools/modelos/modelos.py (tris_vox), que cuenta los mismos triangulos.
static const int FACE_AX[6][3] = { { 0, 1, 2 }, { 0, 1, 2 }, { 1, 0, 2 }, { 1, 0, 2 }, { 2, 0, 1 }, { 2, 0, 1 } };

typedef struct {
    int16_t c[3];   // la celda de la esquina menor
    int16_t ea, eb; // celdas a lo largo de los ejes a y b del plano
    uint8_t f, m;   // la cara y el material
    uint8_t occ[4]; // la oclusion de cada esquina (pareja en un rectangulo de varias caras)
} VoxRect;

// La esquina q del rectangulo en la red de vertices (celdas enteras).
static void rect_corner(const VoxRect *r, int q, int L[3]) {
    const int *ax = FACE_AX[r->f], *cv = FACE_V[r->f][q];
    L[ax[0]] = r->c[ax[0]] + cv[ax[0]];
    L[ax[1]] = r->c[ax[1]] + (cv[ax[1]] ? r->ea : 0);
    L[ax[2]] = r->c[ax[2]] + (cv[ax[2]] ? r->eb : 0);
}

static size_t lattice_idx(const VoxGrid *g, const int L[3]) {
    return (size_t)L[0] + (size_t)(g->nx + 1) * ((size_t)L[2] + (size_t)(g->nz + 1) * (size_t)L[1]);
}

// El borde del rectangulo, de la esquina 0 a la 3 y de vuelta (antihorario visto desde afuera): las esquinas y
// los vertices de otros rectangulos que caen en sus lados. at[q]: donde queda la esquina q; side[v]: el lado del
// vertice v (la esquina donde empieza). Devuelve cuantos son.
static int rect_ring(const VoxGrid *g, const VoxRect *r, const uint8_t *corner, int (*ring)[3], int *side, int cap, int at[4]) {
    int n = 0, P[4][3];
    for (int q = 0; q < 4; q++) rect_corner(r, q, P[q]);
    for (int q = 0; q < 4; q++) {
        const int *A = P[q], *B = P[(q + 1) & 3];
        if (ring && n < cap) memcpy(ring[n], A, sizeof(int[3])), side[n] = q;
        if (at) at[q] = n;
        n++;
        int ax = A[0] != B[0] ? 0 : A[1] != B[1] ? 1 : 2, step = B[ax] > A[ax] ? 1 : -1;
        for (int k = A[ax] + step; k != B[ax]; k += step) {
            int L[3] = { A[0], A[1], A[2] };
            L[ax] = k;
            if (!corner[lattice_idx(g, L)]) continue;
            if (ring && n < cap) memcpy(ring[n], L, sizeof(int[3])), side[n] = q;
            n++;
        }
    }
    return n;
}

// Como se puede juntar una cara, por la oclusion de sus esquinas: pareja (en las dos direcciones del plano), o
// pareja a lo largo de b o de a (la sombra de un rincon: cambia de un lado al otro y no a lo largo); si no, sola.
enum { JOIN_NONE = -1, JOIN_BOTH = 0, JOIN_B = 1, JOIN_A = 2 };
static int join_kind(int f, const uint8_t o[4]) {
    if (o[0] == o[1] && o[0] == o[2] && o[0] == o[3]) return JOIN_BOTH;
    const int aa = FACE_AX[f][1], ab = FACE_AX[f][2];
    bool along_b = true, along_a = true; // las esquinas del mismo lado de a (o de b) tienen la misma oclusion
    for (int p = 0; p < 4; p++)
        for (int q = p + 1; q < 4; q++) {
            if (FACE_V[f][p][aa] == FACE_V[f][q][aa] && o[p] != o[q]) along_b = false;
            if (FACE_V[f][p][ab] == FACE_V[f][q][ab] && o[p] != o[q]) along_a = false;
        }
    return along_b ? JOIN_B : along_a ? JOIN_A : JOIN_NONE;
}

static bool mesh_merged(const VoxGrid *g, const uint8_t (*palette)[4], VoxMesh *out) {
    int faces = vox_exposed_faces(g);
    memset(out, 0, sizeof(*out));
    if (!faces) return true;
    const int dim[3] = { g->nx, g->ny, g->nz };
    int slice = g->nx * g->ny;
    if (g->ny * g->nz > slice) slice = g->ny * g->nz;
    if (g->nx * g->nz > slice) slice = g->nx * g->nz;
    int ringcap = 2 * (g->nx + g->ny + g->nz) + 8; // el borde mas largo posible de un rectangulo
    VoxRect *rects = malloc((size_t)faces * sizeof(*rects));
    int32_t *key = malloc((size_t)slice * sizeof(*key));
    uint8_t *corner = calloc((size_t)(g->nx + 1) * (g->ny + 1) * (g->nz + 1), 1);
    int(*ring)[3] = malloc((size_t)ringcap * sizeof(*ring));
    int *side = malloc((size_t)ringcap * sizeof(*side));
    bool ok = rects && key && corner && ring && side;
    int nr = 0;
    // 1. Los rectangulos de cada plano. La clave de una cara: su material, la oclusion de sus 4 esquinas y como se
    //    puede juntar. Las de oclusion pareja se juntan a lo largo de b y despues de a; las de sombra de rincon,
    //    solo a lo largo del rincon (el color pasa de un lado al otro igual que en las caras sueltas); las demas
    //    quedan solas.
    for (int f = 0; ok && f < 6; f++) {
        const int an = FACE_AX[f][0], aa = FACE_AX[f][1], ab = FACE_AX[f][2], na = dim[aa], nb = dim[ab];
        for (int s = 0; s < dim[an]; s++) {
            for (int i = 0; i < na; i++)
                for (int j = 0; j < nb; j++) {
                    int c[3];
                    c[an] = s, c[aa] = i, c[ab] = j;
                    int32_t k = -1;
                    uint8_t m = vox_get(g, c[0], c[1], c[2]);
                    if (m && face_shown(g, c[0], c[1], c[2], f)) {
                        uint8_t o[4];
                        for (int q = 0; q < 4; q++) o[q] = (uint8_t)corner_occ(g, c[0], c[1], c[2], f, FACE_V[f][q]);
                        int kind = join_kind(f, o);
                        if (kind != JOIN_NONE)
                            k = (int32_t)m | (int32_t)(o[0] | o[1] << 2 | o[2] << 4 | o[3] << 6) << 8 | (int32_t)kind << 16;
                        else rects[nr++] = (VoxRect){ { (int16_t)c[0], (int16_t)c[1], (int16_t)c[2] }, 1, 1, (uint8_t)f, m, { o[0], o[1], o[2], o[3] } };
                    }
                    key[i * nb + j] = k;
                }
            for (int i = 0; i < na; i++)
                for (int j = 0; j < nb; j++) {
                    int32_t k = key[i * nb + j];
                    if (k < 0) continue;
                    int kind = k >> 16, eb = 1, ea = 1;
                    if (kind != JOIN_A)
                        while (j + eb < nb && key[i * nb + j + eb] == k) eb++;
                    if (kind != JOIN_B)
                        for (; i + ea < na; ea++) {
                            bool row = true;
                            for (int t = 0; t < eb && row; t++) row = key[(i + ea) * nb + j + t] == k;
                            if (!row) break;
                        }
                    for (int u = 0; u < ea; u++)
                        for (int t = 0; t < eb; t++) key[(i + u) * nb + j + t] = -1;
                    int c[3];
                    c[an] = s, c[aa] = i, c[ab] = j;
                    int o = (k >> 8) & 0xFF;
                    rects[nr++] = (VoxRect){ { (int16_t)c[0], (int16_t)c[1], (int16_t)c[2] }, (int16_t)ea, (int16_t)eb, (uint8_t)f, (uint8_t)(k & 0xFF),
                                             { (uint8_t)(o & 3), (uint8_t)(o >> 2 & 3), (uint8_t)(o >> 4 & 3), (uint8_t)(o >> 6 & 3) } };
                }
        }
    }
    // 2. La red de vertices: las esquinas de todos los rectangulos.
    for (int r = 0; ok && r < nr; r++)
        for (int q = 0; q < 4; q++) {
            int L[3];
            rect_corner(&rects[r], q, L);
            corner[lattice_idx(g, L)] = 1;
        }
    // 3. Cada rectangulo, con todos los vertices de su borde (asi comparte cada arista con su vecino y no hay
    //    juntas en T): n vertices, n - 2 triangulos, nunca mas que sus caras sueltas (2 por cara).
    int tris = 0;
    for (int r = 0; ok && r < nr; r++) tris += rect_ring(g, &rects[r], corner, NULL, NULL, 0, NULL) - 2;
    if (ok) {
        out->cap = tris;
        out->pos = malloc((size_t)tris * 9 * sizeof(float));
        out->nrm = malloc((size_t)tris * 9 * sizeof(float));
        out->col = malloc((size_t)tris * 12);
        ok = out->pos && out->nrm && out->col;
    }
    const float s = g->size, ox = -g->nx * s * 0.5f, oz = -g->nz * s * 0.5f;
    for (int r = 0; ok && r < nr; r++) {
        const VoxRect *R = &rects[r];
        const uint8_t *base = palette ? palette[R->m] : (const uint8_t[4]){ 200, 200, 200, 255 };
        uint8_t col[4][4];
        for (int q = 0; q < 4; q++) {
            float c = FACE_SHADE[R->f] * occ_shade(R->occ[q]);
            for (int ch = 0; ch < 3; ch++) col[q][ch] = (uint8_t)fminf(255.0f, base[ch] * c);
            col[q][3] = base[3];
        }
        int at[4], n = rect_ring(g, R, corner, ring, side, ringcap, at);
        // Dos mitades por la diagonal de la esquina 0 a la 2; cada una, en zigzag alrededor de su esquina del medio
        // (la 1 y la 3) entre sus dos lados: ningun triangulo queda con los tres vertices en un mismo lado. Sin
        // vertices de mas, son los dos de siempre: (0, 1, 2) y (2, 3, 0), con la oclusion de cada esquina.
        for (int half = 0; half < 2; half++) {
            int mid = at[1 + 2 * half], ua = at[2 * half], wb = half ? n : at[2]; // la esquina del medio y los extremos
            int lu = mid - ua + 1, lw = wb - mid + 1, i = 1, j = 1;
            int idx3[3] = { mid - 1, mid, mid + 1 }; // la primera oreja, en la esquina del medio
            for (bool first = true, turn_u = true;; turn_u = !turn_u) {
                if (!first) {
                    if (i >= lu - 1 && j >= lw - 1) break;
                    bool adv_u = i < lu - 1 && (j >= lw - 1 || turn_u);
                    if (adv_u) idx3[0] = mid - i - 1, idx3[1] = mid - i, idx3[2] = mid + j, i++;
                    else idx3[0] = mid - i, idx3[1] = mid + j, idx3[2] = mid + j + 1, j++;
                }
                first = false;
                int tri = out->tris++;
                for (int k = 0; k < 3; k++) {
                    int v = idx3[k] % n, o = tri * 3 + k;
                    const int *L = ring[v];
                    out->pos[o * 3 + 0] = ox + (float)L[0] * s, out->pos[o * 3 + 1] = (float)L[1] * s, out->pos[o * 3 + 2] = oz + (float)L[2] * s;
                    for (int a = 0; a < 3; a++) out->nrm[o * 3 + a] = (float)FACE_N[R->f][a];
                    memcpy(&out->col[o * 4], col[side[v]], 4); // un vertice de mas: el color de su lado (parejo)
                }
            }
        }
    }
    free(rects);
    free(key);
    free(corner);
    free(ring);
    free(side);
    if (!ok) vox_mesh_free(out);
    return ok;
}

bool vox_mesh(const VoxGrid *g, const uint8_t (*palette)[4], VoxMesh *out) {
    if (g->merge && !g->jitter && !g->light) return mesh_merged(g, palette, out);
    int faces = vox_exposed_faces(g);
    memset(out, 0, sizeof(*out));
    out->cap = faces * 2;
    if (!out->cap) return true;
    out->pos = malloc((size_t)out->cap * 9 * sizeof(float));
    out->nrm = malloc((size_t)out->cap * 9 * sizeof(float));
    out->col = malloc((size_t)out->cap * 12);
    if (!out->pos || !out->nrm || !out->col) {
        vox_mesh_free(out);
        return false;
    }
    const float s = g->size, ox = -g->nx * s * 0.5f, oz = -g->nz * s * 0.5f;
    for (int y = 0; y < g->ny; y++)
        for (int z = 0; z < g->nz; z++)
            for (int x = 0; x < g->nx; x++) {
                int i = idx(g, x, y, z);
                uint8_t m = g->m[i];
                if (!m) continue;
                uint8_t base[4];
                if (g->light) {
                    uint8_t l = g->light[i];
                    base[0] = l, base[1] = l, base[2] = (uint8_t)(l + (255 - l) / 12), base[3] = 255;
                } else {
                    memcpy(base, palette ? palette[m] : (const uint8_t[4]){ 200, 200, 200, 255 }, 4);
                    if (g->jitter) { // tono de cada celda: piedra a piedra, tabla a tabla
                        uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)z * 83492791u;
                        h ^= h >> 13, h *= 0x5BD1E995u, h ^= h >> 15;
                        float k = 1.0f + ((float)(h & 0xFF) / 255.0f - 0.5f) * (float)g->jitter / 50.0f;
                        for (int ch = 0; ch < 3; ch++) base[ch] = (uint8_t)fminf(255.0f, base[ch] * k);
                    }
                }
                for (int f = 0; f < 6; f++) {
                    if (!face_shown(g, x, y, z, f)) continue;
                    float p[4][3], c[4];
                    for (int k = 0; k < 4; k++) {
                        const int *cv = FACE_V[f][k];
                        p[k][0] = ox + (x + cv[0]) * s, p[k][1] = (y + cv[1]) * s, p[k][2] = oz + (z + cv[2]) * s;
                        c[k] = FACE_SHADE[f] * corner_ao(g, x, y, z, f, cv);
                    }
                    if (g->light) // las nubes: la cara sombreada mucho menos (la luz ya viene horneada)
                        for (int k = 0; k < 4; k++) c[k] = 0.75f + 0.25f * c[k];
                    static const int TRI[2][3] = { { 0, 1, 2 }, { 0, 2, 3 } };
                    for (int t = 0; t < 2; t++) {
                        int tri = out->tris++;
                        for (int k = 0; k < 3; k++) {
                            int v = TRI[t][k], o = tri * 3 + k;
                            memcpy(&out->pos[o * 3], p[v], sizeof(p[v]));
                            out->nrm[o * 3 + 0] = (float)FACE_N[f][0], out->nrm[o * 3 + 1] = (float)FACE_N[f][1], out->nrm[o * 3 + 2] = (float)FACE_N[f][2];
                            for (int ch = 0; ch < 3; ch++) out->col[o * 4 + ch] = (uint8_t)fminf(255.0f, base[ch] * c[v]);
                            out->col[o * 4 + 3] = base[3];
                        }
                    }
                }
            }
    return true;
}

void vox_mesh_free(VoxMesh *m) {
    free(m->pos);
    free(m->nrm);
    free(m->col);
    memset(m, 0, sizeof(*m));
}
