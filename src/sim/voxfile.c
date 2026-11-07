#include "sim/voxfile.h"

#include <string.h>

static int32_t rd32(const uint8_t *p) { return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24); }

static void wr32(uint8_t *p, int32_t v) {
    uint32_t u = (uint32_t)v;
    p[0] = (uint8_t)u, p[1] = (uint8_t)(u >> 8), p[2] = (uint8_t)(u >> 16), p[3] = (uint8_t)(u >> 24);
}

// Un trozo: id (4), largo de lo suyo, largo de sus hijos. false si no cabe en [pos, end).
static bool chunk_at(const uint8_t *d, size_t pos, size_t end, int32_t *n, int32_t *m) {
    if (pos + 12 > end) return false;
    *n = rd32(d + pos + 4), *m = rd32(d + pos + 8);
    return *n >= 0 && *m >= 0 && (size_t)*n <= end - pos - 12 && (size_t)*m <= end - pos - 12 - (size_t)*n;
}

bool voxfile_read(const uint8_t *data, size_t len, float size, VoxGrid *g, uint8_t palette[256][4]) {
    memset(g, 0, sizeof(*g));
    if (!data || len < 8 + 12 || memcmp(data, "VOX ", 4)) return false;
    size_t pos = 8, end = len;
    int32_t n, m;
    if (!memcmp(data + pos, "MAIN", 4)) { // los demas trozos son hijos del MAIN
        if (!chunk_at(data, pos, end, &n, &m)) return false;
        pos += 12 + (size_t)n;
        end = pos + (size_t)m;
    }
    int32_t sx = 0, sy = 0, sz = 0;
    const uint8_t *xyzi = NULL, *rgba = NULL;
    int32_t count = 0;
    while (pos + 12 <= end) {
        if (!chunk_at(data, pos, end, &n, &m)) return false;
        const uint8_t *id = data + pos, *body = data + pos + 12;
        if (!memcmp(id, "SIZE", 4) && !sx) {
            if (n < 12) return false;
            sx = rd32(body), sy = rd32(body + 4), sz = rd32(body + 8);
            if (sx < 1 || sy < 1 || sz < 1 || sx > VOXFILE_MAX || sy > VOXFILE_MAX || sz > VOXFILE_MAX) return false;
        } else if (!memcmp(id, "XYZI", 4) && sx && !xyzi) {
            if (n < 4) return false;
            count = rd32(body);
            if (count < 0 || (size_t)count > ((size_t)n - 4) / 4) return false;
            xyzi = body + 4;
        } else if (!memcmp(id, "RGBA", 4) && !rgba) {
            if (n < 1024) return false;
            rgba = body;
        }
        pos += 12 + (size_t)n + (size_t)m;
    }
    if (!sx || !xyzi) return false;
    // Rejilla del juego: x hacia el frente (-y del .vox), y arriba (z), z hacia la izquierda (-x).
    if (!vox_init(g, sy, sz, sx, size)) return false;
    for (int32_t i = 0; i < count; i++) {
        const uint8_t *v = xyzi + 4 * (size_t)i;
        if (!v[3] || v[0] >= sx || v[1] >= sy || v[2] >= sz) continue;
        vox_set(g, sy - 1 - v[1], v[2], sx - 1 - v[0], v[3]);
    }
    memset(palette[0], 0, 4);
    for (int i = 1; i < 256; i++) {
        if (rgba) {
            memcpy(palette[i], rgba + 4 * (i - 1), 4);
        } else { // sin paleta: grises
            uint8_t gray = (uint8_t)(40 + i * 190 / 255);
            palette[i][0] = palette[i][1] = palette[i][2] = gray, palette[i][3] = 255;
        }
    }
    return true;
}

size_t voxfile_write(const VoxGrid *g, const uint8_t palette[256][4], uint8_t *out, size_t cap) {
    if (g->nx < 1 || g->ny < 1 || g->nz < 1 || g->nx > VOXFILE_MAX || g->ny > VOXFILE_MAX || g->nz > VOXFILE_MAX) return 0;
    size_t cells = (size_t)vox_count(g);
    size_t sz_size = 12 + 12, sz_xyzi = 12 + 4 + 4 * cells, sz_rgba = 12 + 1024;
    size_t total = 8 + 12 + sz_size + sz_xyzi + sz_rgba;
    if (!out || cap < total) return 0;
    memcpy(out, "VOX ", 4);
    wr32(out + 4, 150);
    memcpy(out + 8, "MAIN", 4);
    wr32(out + 12, 0);
    wr32(out + 16, (int32_t)(sz_size + sz_xyzi + sz_rgba));
    uint8_t *p = out + 20;
    memcpy(p, "SIZE", 4), wr32(p + 4, 12), wr32(p + 8, 0);
    wr32(p + 12, g->nz), wr32(p + 16, g->nx), wr32(p + 20, g->ny); // el .vox: x, y (fondo), z (alto)
    p += sz_size;
    memcpy(p, "XYZI", 4), wr32(p + 4, (int32_t)(4 + 4 * cells)), wr32(p + 8, 0), wr32(p + 12, (int32_t)cells);
    uint8_t *v = p + 16;
    for (int y = 0; y < g->ny; y++)
        for (int z = 0; z < g->nz; z++)
            for (int x = 0; x < g->nx; x++) {
                uint8_t c = vox_get(g, x, y, z);
                if (!c) continue;
                v[0] = (uint8_t)(g->nz - 1 - z), v[1] = (uint8_t)(g->nx - 1 - x), v[2] = (uint8_t)y, v[3] = c;
                v += 4;
            }
    p += sz_xyzi;
    memcpy(p, "RGBA", 4), wr32(p + 4, 1024), wr32(p + 8, 0);
    for (int i = 0; i < 256; i++) memcpy(p + 12 + 4 * i, palette[i < 255 ? i + 1 : 0], 4);
    return total;
}
