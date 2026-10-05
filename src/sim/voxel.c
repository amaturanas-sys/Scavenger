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

int vox_exposed_faces(const VoxGrid *g) {
    int n = 0;
    for (int y = 0; y < g->ny; y++)
        for (int z = 0; z < g->nz; z++)
            for (int x = 0; x < g->nx; x++) {
                if (!g->m[idx(g, x, y, z)]) continue;
                for (int f = 0; f < 6; f++) n += !vox_get(g, x + FACE_N[f][0], y + FACE_N[f][1], z + FACE_N[f][2]);
            }
    return n;
}

// Oclusion ambiental de una esquina: cuantas de las 3 celdas vecinas del lado de afuera estan llenas.
static float corner_ao(const VoxGrid *g, int x, int y, int z, int f, const int *corner) {
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
    int occ = side1 && side2 ? 3 : side1 + side2 + diag;
    return 1.0f - 0.16f * (float)occ;
}

bool vox_mesh(const VoxGrid *g, const uint8_t (*palette)[4], VoxMesh *out) {
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
                    if (vox_get(g, x + FACE_N[f][0], y + FACE_N[f][1], z + FACE_N[f][2])) continue;
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
