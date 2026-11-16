#include "world/shapes.h"

#include <math.h>
#include <stddef.h>

#include "raymath.h"
#include "rlgl.h"

Color shape_tone(Color c, Vector3 n, ShapeTone t) {
    float k;
    if (t == SHAPE_SOFT) k = 0.85f + 0.28f * (n.x * 0.36f + n.y * 0.82f + n.z * 0.44f);
    else { // las seis caras de los voxeles, mezcladas por el cuadrado de cada eje de la normal
        float xx = n.x * n.x, yy = n.y * n.y, zz = n.z * n.z;
        k = xx * (n.x > 0.0f ? 0.82f : 0.74f) + yy * (n.y > 0.0f ? 1.0f : 0.55f) + zz * (n.z > 0.0f ? 0.88f : 0.68f);
    }
    return (Color){ (unsigned char)fminf(255.0f, (float)c.r * k), (unsigned char)fminf(255.0f, (float)c.g * k), (unsigned char)fminf(255.0f, (float)c.b * k), c.a };
}

// Un cuadrilatero (en RL_QUADS, de frente desde afuera) con el tono de su cara; uv: sus cuatro pares, o NULL.
static void tquad(Vector3 q0, Vector3 q1, Vector3 q2, Vector3 q3, Color lit, ShapeTone t, const float *uv) {
    Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(q2, q0), Vector3Subtract(q3, q1)));
    Color c = shape_tone(lit, n, t);
    const Vector3 q[4] = { q0, q1, q2, q3 };
    for (int k = 0; k < 4; k++) {
        rlColor4ub(c.r, c.g, c.b, 255);
        rlTexCoord2f(uv ? uv[k * 2] : 0.0f, uv ? uv[k * 2 + 1] : 0.0f);
        rlVertex3f(q[k].x, q[k].y, q[k].z);
    }
}

void shape_solid(Vector3 a, Vector3 b, float ra, float rb, int sides, Color lit, ShapeTone t, unsigned int tex, float tile, int caps) {
    Vector3 ax = Vector3Subtract(b, a);
    float len = Vector3Length(ax);
    if (len < 1e-4f) return;
    sides = sides < 3 ? 3 : sides > 16 ? 16 : sides;
    // El marco del eje (e2, d, e1), como (x, y, z): de pie, el angulo va de +z hacia +x.
    Vector3 d = Vector3Scale(ax, 1.0f / len), helper = fabsf(d.y) < 0.9f ? (Vector3){ 0.0f, 1.0f, 0.0f } : (Vector3){ 1.0f, 0.0f, 0.0f };
    Vector3 e1 = Vector3Normalize(Vector3CrossProduct(helper, d)), e2 = Vector3CrossProduct(d, e1);
    float step = 2.0f * PI / (float)sides, rm = (ra + rb) * 0.5f, tl = tile > 0.0f ? tile : 1.0f;
    if (tex) rlSetTexture(tex);
    rlBegin(RL_QUADS);
    for (int k = 0; k < sides; k++) {
        float t0 = (float)k * step, t1 = t0 + step;
        Vector3 o0 = Vector3Add(Vector3Scale(e2, sinf(t0)), Vector3Scale(e1, cosf(t0))), o1 = Vector3Add(Vector3Scale(e2, sinf(t1)), Vector3Scale(e1, cosf(t1)));
        Vector3 q0 = Vector3Add(a, Vector3Scale(o0, ra)), q1 = Vector3Add(a, Vector3Scale(o1, ra)), q2 = Vector3Add(b, Vector3Scale(o1, rb)),
                q3 = Vector3Add(b, Vector3Scale(o0, rb));
        float u0 = t0 * rm / tl, u1 = t1 * rm / tl, v0 = -a.y / tl, v1 = -(a.y + len) / tl, uv[8] = { u0, v0, u1, v0, u1, v1, u0, v1 };
        tquad(q0, q1, q2, q3, lit, t, uv);
        if (caps & 1) tquad(b, q3, q2, b, lit, t, NULL);
        if (caps & 2) tquad(a, q1, q0, a, lit, t, NULL);
    }
    rlEnd();
    if (tex) rlSetTexture(0);
}

void shape_box(Vector3 c, float w, float h, float d, float yaw, Color lit, ShapeTone t) {
    float fx = sinf(yaw), fz = cosf(yaw);
    Vector3 p[8]; // bit 0: +ancho (de costado), bit 1: +alto, bit 2: +largo (hacia yaw)
    for (int k = 0; k < 8; k++) {
        float lx = (k & 1) ? w * 0.5f : -w * 0.5f, ly = (k & 2) ? h * 0.5f : -h * 0.5f, lz = (k & 4) ? d * 0.5f : -d * 0.5f;
        p[k] = (Vector3){ c.x + fz * lx + fx * lz, c.y + ly, c.z - fx * lx + fz * lz };
    }
    static const int F[6][4] = { { 4, 5, 7, 6 }, { 1, 0, 2, 3 }, { 5, 1, 3, 7 }, { 0, 4, 6, 2 }, { 6, 7, 3, 2 }, { 0, 1, 5, 4 } };
    rlBegin(RL_QUADS);
    for (int f = 0; f < 6; f++) tquad(p[F[f][0]], p[F[f][1]], p[F[f][2]], p[F[f][3]], lit, t, NULL);
    rlEnd();
}

void shape_ellipsoid(Vector3 c, float rl, float rh, float rw, float yaw, Color lit, ShapeTone t) {
    enum { RINGS = 5, SLICES = 8 };
    float fx = sinf(yaw), fz = cosf(yaw);
    Vector3 q[RINGS + 1][SLICES + 1];
    for (int j = 0; j <= RINGS; j++)
        for (int k = 0; k <= SLICES; k++) {
            float p = -PI * 0.5f + PI * (float)j / (float)RINGS, th = 2.0f * PI * (float)k / (float)SLICES;
            float sx = cosf(p) * sinf(th) * rw, sy = sinf(p) * rh, sz = cosf(p) * cosf(th) * rl;
            q[j][k] = (Vector3){ c.x + fz * sx + fx * sz, c.y + sy, c.z - fx * sx + fz * sz };
        }
    rlBegin(RL_QUADS);
    for (int j = 0; j < RINGS; j++)
        for (int k = 0; k < SLICES; k++) tquad(q[j][k], q[j][k + 1], q[j + 1][k + 1], q[j + 1][k], lit, t, NULL);
    rlEnd();
}
