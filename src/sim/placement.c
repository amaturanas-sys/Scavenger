#include "sim/placement.h"

#include <math.h>

#define TWO_PI 6.2831853f

Footprint footprint_make(float x, float z, float yaw, float l, float w, float margin) {
    Footprint f = { x, z, yaw, fmaxf(0.05f, l * 0.5f + margin), fmaxf(0.05f, w * 0.5f + margin) };
    return f;
}

// Un punto local (X: largo, Z: ancho) en el mundo, con el giro de raylib alrededor de +Y.
static void to_world(const Footprint *f, float lx, float lz, float *x, float *z) {
    float c = cosf(f->yaw), s = sinf(f->yaw);
    *x = f->x + lx * c + lz * s;
    *z = f->z - lx * s + lz * c;
}

void footprint_corners(const Footprint *f, float out[4][2]) {
    static const float SX[4] = { -1.0f, 1.0f, 1.0f, -1.0f }, SZ[4] = { -1.0f, -1.0f, 1.0f, 1.0f };
    for (int i = 0; i < 4; i++) to_world(f, SX[i] * f->hl, SZ[i] * f->hw, &out[i][0], &out[i][1]);
}

// Proyeccion de las esquinas sobre el eje (ax, az).
static void project(float c[4][2], float ax, float az, float *lo, float *hi) {
    *lo = *hi = c[0][0] * ax + c[0][1] * az;
    for (int i = 1; i < 4; i++) {
        float p = c[i][0] * ax + c[i][1] * az;
        *lo = fminf(*lo, p), *hi = fmaxf(*hi, p);
    }
}

bool footprints_overlap(const Footprint *a, const Footprint *b) {
    float ca[4][2], cb[4][2];
    footprint_corners(a, ca);
    footprint_corners(b, cb);
    // Los ejes de los dos rectangulos: si en alguno las sombras no se tocan, no se pisan.
    const float yaws[2] = { a->yaw, b->yaw };
    for (int k = 0; k < 2; k++) {
        float c = cosf(yaws[k]), s = sinf(yaws[k]);
        const float axes[2][2] = { { c, -s }, { s, c } };
        for (int e = 0; e < 2; e++) {
            float alo, ahi, blo, bhi;
            project(ca, axes[e][0], axes[e][1], &alo, &ahi);
            project(cb, axes[e][0], axes[e][1], &blo, &bhi);
            if (ahi <= blo + 1e-3f || bhi <= alo + 1e-3f) return false; // tocarse no es pisarse
        }
    }
    return true;
}

PlaceCheck place_check(const Footprint *f, float cx, float cz, float radius, const PlaceGround *g, const Footprint *obs, int nobs) {
    PlaceCheck r = { PLACE_OK, -1 };
    float c[4][2];
    footprint_corners(f, c);
    for (int i = 0; radius > 0.0f && i < 4; i++)
        if ((c[i][0] - cx) * (c[i][0] - cx) + (c[i][1] - cz) * (c[i][1] - cz) > radius * radius) {
            r.res = PLACE_OUTSIDE;
            return r;
        }
    // El suelo, en una rejilla de 3x3 sobre la huella (esquinas, bordes y centro).
    float hmin = 1e30f, hmax = -1e30f;
    for (int i = -1; g && i <= 1; i++)
        for (int j = -1; j <= 1; j++) {
            float x, z;
            to_world(f, (float)i * f->hl, (float)j * f->hw, &x, &z);
            if (g->water && g->water(g->ud, x, z) > PLACE_WET) {
                r.res = PLACE_WATER;
                return r;
            }
            if (g->height) {
                float h = g->height(g->ud, x, z);
                hmin = fminf(hmin, h), hmax = fmaxf(hmax, h);
            }
        }
    float span = fmaxf(2.0f * fmaxf(f->hl, f->hw), 2.0f);
    if (g && g->height && hmax - hmin > PLACE_FLAT && (hmax - hmin) / span > PLACE_MAX_SLOPE) {
        r.res = PLACE_STEEP;
        return r;
    }
    for (int i = 0; i < nobs; i++)
        if (footprints_overlap(f, &obs[i])) {
            r.res = PLACE_OVERLAP;
            r.hit = i;
            return r;
        }
    return r;
}

float place_rotate(float yaw, int steps) {
    float y = roundf(yaw / PLACE_ROT_STEP + (float)steps) * PLACE_ROT_STEP; // siempre en un paso justo
    y = fmodf(y, TWO_PI);
    if (y < 0.0f) y += TWO_PI;
    return y > TWO_PI - 1e-4f ? 0.0f : y;
}

float place_snap(float v, float origin, float grid) { return grid > 0.0f ? origin + roundf((v - origin) / grid) * grid : v; }

int place_wall(float ax, float az, float bx, float bz, float seg_w, float seg_l, Footprint *out, int max) {
    float dx = bx - ax, dz = bz - az, len = sqrtf(dx * dx + dz * dz);
    if (seg_w <= 0.0f) seg_w = 1.0f;
    int n = len < 1e-3f ? 1 : (int)ceilf(len / seg_w - 1e-4f);
    if (n < 1) n = 1;
    float yaw = len < 1e-3f ? 0.0f : atan2f(dx, dz); // el ancho del tramo (Z local) va a lo largo
    float step = n > 0 ? len / (float)n : 0.0f;       // <= seg_w: sin huecos
    for (int i = 0; i < n && i < max; i++) {
        float t = len < 1e-3f ? 0.0f : step * ((float)i + 0.5f) / len;
        out[i] = footprint_make(ax + dx * t, az + dz * t, yaw, seg_l, seg_w, 0.0f);
    }
    return n;
}
