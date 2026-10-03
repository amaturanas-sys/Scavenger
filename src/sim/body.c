#include "body.h"

#include <math.h>

static V3 v3(float x, float y, float z) { return (V3){ x, y, z }; }
static V3 add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static V3 sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static V3 mul(V3 a, float k) { return v3(a.x * k, a.y * k, a.z * k); }
static float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// Un hueso que cuelga de `from` con largo `len`, girado `angle` hacia adelante (0 = hacia abajo).
static V3 limb(V3 from, float len, float angle) { return add(from, v3(0.0f, -cosf(angle) * len, sinf(angle) * len)); }

void body_pose(BodyPose *out, const BodyPoseParams *p) {
    BodySeg *s = out->seg;
    float sw = p->walk * sinf(p->walk_phase);
    // Tronco.
    s[PART_HEAD] = (BodySeg){ v3(0, 1.61f, 0.01f), v3(0, 1.67f, 0.01f), 0.10f };
    s[PART_NECK] = (BodySeg){ v3(0, 1.45f, 0), v3(0, 1.54f, 0), 0.06f };
    s[PART_THORAX] = (BodySeg){ v3(0, 1.18f, 0), v3(0, 1.30f, 0), 0.16f };
    s[PART_ABDOMEN] = (BodySeg){ v3(0, 1.00f, 0), v3(0, 1.10f, 0), 0.14f };
    s[PART_PELVIS] = (BodySeg){ v3(0, 0.86f, 0), v3(0, 0.94f, 0), 0.16f };
    // Brazos: se balancean al andar (al reves que las piernas).
    float arm_l = -0.4f * sw, arm_r = 0.4f * sw, fore_l = 0.25f, fore_r = 0.25f;
    if (p->attack > 0.0f) { // golpe: el brazo derecho sube y baja hacia adelante
        arm_r = arm_r + (1.9f - arm_r) * p->attack;
        fore_r = 0.1f;
    }
    if (p->aiming) { // arco: el izquierdo extendido al frente, el derecho tensa la cuerda
        arm_l = 1.5f, fore_l = 0.0f;
        arm_r = 1.5f, fore_r = 2.7f;
    }
    if (p->guard > 0.0f) { // escudo: el brazo izquierdo al frente, el antebrazo cruzado
        arm_l = arm_l + (1.2f - arm_l) * p->guard;
        fore_l = 0.35f * p->guard;
    }
    if (p->grab > 0.0f) { // agarre: los dos brazos al frente, a la altura del pecho
        arm_l = arm_l + (1.45f - arm_l) * p->grab, arm_r = arm_r + (1.45f - arm_r) * p->grab;
        fore_l = fore_r = 0.2f;
    }
    V3 sh_l = v3(0.22f, 1.40f, 0), sh_r = v3(-0.22f, 1.40f, 0);
    V3 el_l = limb(sh_l, 0.30f, arm_l), el_r = limb(sh_r, 0.30f, arm_r);
    s[PART_UPPER_ARM_L] = (BodySeg){ sh_l, el_l, 0.055f };
    s[PART_UPPER_ARM_R] = (BodySeg){ sh_r, el_r, 0.055f };
    s[PART_FOREARM_L] = (BodySeg){ el_l, limb(el_l, 0.28f, arm_l + fore_l), 0.05f };
    s[PART_FOREARM_R] = (BodySeg){ el_r, limb(el_r, 0.28f, arm_r + fore_r), 0.05f };
    // Piernas: el muslo avanza y la rodilla se dobla cuando la pierna va atras.
    float leg_l = 0.45f * sw, leg_r = -0.45f * sw;
    if (p->kick > 0.0f) leg_r = leg_r + (1.5f - leg_r) * p->kick, leg_l *= 1.0f - p->kick; // patada al frente
    float knee_l = -fmaxf(0.0f, -leg_l) * 1.2f, knee_r = -fmaxf(0.0f, -leg_r) * 1.2f;
    if (p->kick > 0.0f) knee_r = -0.2f * p->kick; // la pierna estirada al golpear
    V3 hip_l = v3(0.10f, 0.88f, 0), hip_r = v3(-0.10f, 0.88f, 0);
    V3 kn_l = limb(hip_l, 0.43f, leg_l), kn_r = limb(hip_r, 0.43f, leg_r);
    s[PART_THIGH_L] = (BodySeg){ hip_l, kn_l, 0.075f };
    s[PART_THIGH_R] = (BodySeg){ hip_r, kn_r, 0.075f };
    s[PART_SHIN_L] = (BodySeg){ kn_l, limb(kn_l, 0.43f, leg_l + knee_l), 0.06f };
    s[PART_SHIN_R] = (BodySeg){ kn_r, limb(kn_r, 0.43f, leg_r + knee_r), 0.06f };

    float k = p->scale > 0.0f ? p->scale : 1.0f;
    for (int i = 0; i < PART_COUNT; i++) {
        BodySeg *g = &s[i];
        if (p->down) { // tendido boca arriba: la cabeza hacia atras, los pies adelante
            g->a = v3(g->a.x, 0.15f + g->a.z, 0.9f - g->a.y);
            g->b = v3(g->b.x, 0.15f + g->b.z, 0.9f - g->b.y);
        }
        g->a = mul(g->a, k);
        g->b = mul(g->b, k);
        g->radius *= k;
    }
}

// Interseccion de un rayo (rd normalizada) con una capsula; distancia o -1.
static float capsule_hit(V3 ro, V3 rd, V3 pa, V3 pb, float r) {
    V3 ba = sub(pb, pa), oa = sub(ro, pa);
    float baba = dot(ba, ba), bard = dot(ba, rd), baoa = dot(ba, oa), rdoa = dot(rd, oa), oaoa = dot(oa, oa);
    float a = baba - bard * bard, b = baba * rdoa - baoa * bard, c = baba * oaoa - baoa * baoa - r * r * baba;
    float h = b * b - a * c;
    if (a > 1e-8f && h >= 0.0f) {
        float t = (-b - sqrtf(h)) / a;
        float y = baoa + t * bard;
        if (y > 0.0f && y < baba) return t; // el cuerpo del cilindro
    }
    // Las tapas esfericas.
    float best = -1.0f;
    V3 caps[2] = { pa, pb };
    for (int i = 0; i < 2; i++) {
        V3 oc = sub(ro, caps[i]);
        float bb = dot(rd, oc), cc = dot(oc, oc) - r * r, hh = bb * bb - cc;
        if (hh < 0.0f) continue;
        float t = -bb - sqrtf(hh);
        if (t >= 0.0f && (best < 0.0f || t < best)) best = t;
    }
    return best;
}

int body_raycast(const BodyPose *b, V3 origin, V3 dir, float max_t, float *t_out) {
    float len = sqrtf(dot(dir, dir));
    if (len < 1e-6f) return -1;
    V3 rd = mul(dir, 1.0f / len);
    int part = -1;
    float best = max_t * len;
    for (int i = 0; i < PART_COUNT; i++) {
        float t = capsule_hit(origin, rd, b->seg[i].a, b->seg[i].b, b->seg[i].radius);
        if (t >= 0.0f && t <= best) best = t, part = i;
    }
    if (part >= 0 && t_out) *t_out = best / len;
    return part;
}

V3 body_to_local(V3 w, V3 pos, float yaw) {
    V3 d = sub(w, pos);
    float c = cosf(yaw), s = sinf(yaw);
    return v3(d.x * c - d.z * s, d.y, d.x * s + d.z * c);
}

V3 body_dir_to_local(V3 d, float yaw) {
    float c = cosf(yaw), s = sinf(yaw);
    return v3(d.x * c - d.z * s, d.y, d.x * s + d.z * c);
}

V3 body_to_world(V3 l, V3 pos, float yaw) {
    float c = cosf(yaw), s = sinf(yaw);
    return v3(pos.x + l.x * c + l.z * s, pos.y + l.y, pos.z - l.x * s + l.z * c);
}
