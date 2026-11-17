#include "game/cave_props.h"

#include <math.h>
#include <string.h>

#include "game/cave_look.h"
#include "game/cave_scene.h"
#include "raymath.h"
#include "rlgl.h"
#include "sim/world.h"
#include "world/body_draw.h"
#include "world/hearth.h"
#include "world/voxcave.h"

static float hash01(int a, int b) {
    uint32_t h = (uint32_t)a * 0x9E3779B1u ^ (uint32_t)b * 0x85EBCA77u;
    h ^= h >> 15, h *= 0x2C1B3C6Du, h ^= h >> 12;
    return (float)(h & 0xFFFFu) / 65535.0f;
}

static bool is_done(uint64_t done, int i) { return i >= 0 && i < 64 && ((done >> i) & 1u); }
static int variant_of(int i) { return (int)(hash01(i, 11) * 4.0f) & 3; }
// Lo que mide una estalactita y una estalagmita (m): con las bovedas de la v0.14 (6 a 18 m), mas largas que antes.
static float stalactite_len(const CaveObject *o) { return 1.2f + 1.4f * o->size; }
static float stalagmite_len(const CaveObject *o) { return 0.8f + 0.8f * o->size; }

void cp_preload(const Cave *c) {
    if (c->kind == CAVE_EMPIRE) return; // en las salas labradas no hay formaciones: lamparas y pedestales (draw_object)
    for (int i = 0; i < c->object_count; i++) {
        int k = c->objects[i].kind;
        if (k == COBJ_STALACTITE) vc_preload(VC_STALACTITE, (Region)c->region, variant_of(i));
        else if (k == COBJ_STALAGMITE) vc_preload(VC_STALAGMITE, (Region)c->region, variant_of(i));
    }
}

// Lo que cave_props arma por su cuenta (ademas de los modelos de voxeles, que libera cs_unload con vc_unload). Por ahora
// nada: aqui se liberan las texturas de las pinturas (docs/PLAN_CUEVAS_GRANDES.md, C9).
void cp_unload(void) {}
void cp_forget_gpu(void) {}

static void quad(Vector3 a, Color ca, Vector3 b, Color cb, Vector3 c, Color cc, Vector3 d, Color cd) {
    rlColor4ub(ca.r, ca.g, ca.b, 255), rlVertex3f(a.x, a.y, a.z);
    rlColor4ub(cb.r, cb.g, cb.b, 255), rlVertex3f(b.x, b.y, b.z);
    rlColor4ub(cc.r, cc.g, cc.b, 255), rlVertex3f(c.x, c.y, c.z);
    rlColor4ub(ca.r, ca.g, ca.b, 255), rlVertex3f(a.x, a.y, a.z);
    rlColor4ub(cc.r, cc.g, cc.b, 255), rlVertex3f(c.x, c.y, c.z);
    rlColor4ub(cd.r, cd.g, cd.b, 255), rlVertex3f(d.x, d.y, d.z);
}

static Color mixc(Color a, Color b, float t) {
    return (Color){ (unsigned char)((float)a.r + ((float)b.r - (float)a.r) * t), (unsigned char)((float)a.g + ((float)b.g - (float)a.g) * t),
                    (unsigned char)((float)a.b + ((float)b.b - (float)a.b) * t), 255 };
}

// Las pinturas grandes (la tribu y los forajidos, como en las referencias) se doblan con la pared: un plano recto se
// meteria en la roca a los costados. Mientras on, on_wall pone (u, v) sobre la pared de la sala, agrandado scale veces.
typedef struct {
    bool on;
    const Cave *c;
    int room;
    float a0, r0, inset, scale;
} Bend;
static Bend g_bend;

// Sobre la pared de un objeto: (u, v) a lo largo de la pared (ux, uz) y hacia arriba, off hacia la sala (fx, fz).
static Vector3 on_wall(Vector3 at, float ux, float uz, float fx, float fz, float u, float v, float off) {
    if (g_bend.on) {
        const CaveRoom *r = &g_bend.c->rooms[g_bend.room];
        float a = g_bend.a0 - u * g_bend.scale / g_bend.r0, d = cave_wall_radius(g_bend.c, g_bend.room, a) - g_bend.inset - off;
        return (Vector3){ r->x + sinf(a) * d, at.y + v * g_bend.scale, r->z + cosf(a) * d };
    }
    return (Vector3){ at.x + ux * u + fx * off, at.y + v, at.z + uz * u + fz * off };
}

// Lo mas grande (hasta want veces) que entra una pintura (de 1,9 m de punta a punta, con la cornamenta del ultimo
// ciervo) sin pasar por la boca de un pasillo.
static float painting_scale(const Cave *c, int room, float a0, float r0, float want) {
    for (float s = want; s > 1.0f; s -= 0.3f) {
        bool ok = true;
        for (int k = -4; k <= 4 && ok; k++) ok = !cave_door_at(c, room, a0 + (float)k / 4.0f * 0.95f * s / r0);
        if (ok) return s;
    }
    return 1.0f;
}

static void wall_rect(Vector3 at, float ux, float uz, float fx, float fz, float u0, float v0, float u1, float v1, float off, Color col) {
    rlBegin(RL_TRIANGLES);
    quad(on_wall(at, ux, uz, fx, fz, u0, v0, off), col, on_wall(at, ux, uz, fx, fz, u1, v0, off), col, on_wall(at, ux, uz, fx, fz, u1, v1, off), col,
         on_wall(at, ux, uz, fx, fz, u0, v1, off), col);
    rlEnd();
}

// Un trazo de ancho w de (u0, v0) a (u1, v1).
static void wall_seg(Vector3 at, float ux, float uz, float fx, float fz, float u0, float v0, float u1, float v1, float w, Color col) {
    float du = u1 - u0, dv = v1 - v0, l = sqrtf(du * du + dv * dv) + 1e-5f, pu = -dv / l * w * 0.5f, pv = du / l * w * 0.5f;
    rlBegin(RL_TRIANGLES);
    quad(on_wall(at, ux, uz, fx, fz, u0 + pu, v0 + pv, 0.04f), col, on_wall(at, ux, uz, fx, fz, u1 + pu, v1 + pv, 0.04f), col,
         on_wall(at, ux, uz, fx, fz, u1 - pu, v1 - pv, 0.04f), col, on_wall(at, ux, uz, fx, fz, u0 - pu, v0 - pv, 0.04f), col);
    rlEnd();
}

static void wall_disc(Vector3 at, float ux, float uz, float fx, float fz, float u, float v, float r, float off, Color col) {
    rlBegin(RL_TRIANGLES);
    for (int k = 0; k < 10; k++) {
        float a0 = (float)k * 0.6283f, a1 = a0 + 0.6283f;
        Vector3 c0 = on_wall(at, ux, uz, fx, fz, u, v, off), p0 = on_wall(at, ux, uz, fx, fz, u + cosf(a0) * r, v + sinf(a0) * r, off),
                p1 = on_wall(at, ux, uz, fx, fz, u + cosf(a1) * r, v + sinf(a1) * r, off);
        rlColor4ub(col.r, col.g, col.b, 255), rlVertex3f(c0.x, c0.y, c0.z);
        rlColor4ub(col.r, col.g, col.b, 255), rlVertex3f(p0.x, p0.y, p0.z);
        rlColor4ub(col.r, col.g, col.b, 255), rlVertex3f(p1.x, p1.y, p1.z);
    }
    rlEnd();
}

static void bars(Vector3 at, float yaw, float radius, float height, int n, int skip_from, int skip_to, Color col) {
    // Barrotes en semicirculo, de la pared (atras) hacia adentro: de -90 a +90 grados de yaw.
    Vector3 prev = { 0 };
    for (int i = 0; i < n; i++) {
        float a = yaw - PI * 0.5f + PI * (float)i / (float)(n - 1);
        Vector3 b = { at.x + sinf(a) * radius, at.y, at.z + cosf(a) * radius };
        if (i < skip_from || i > skip_to) DrawCylinderEx(b, (Vector3){ b.x, b.y + height, b.z }, 0.03f, 0.03f, 4, col);
        if (i > 0) DrawCylinderEx((Vector3){ prev.x, prev.y + height, prev.z }, (Vector3){ b.x, b.y + height, b.z }, 0.03f, 0.03f, 4, col);
        prev = b;
    }
}

static void draw_prisoner(Vector3 at, float yaw, PrisonerKind k, float time) {
    static const Color CLOTH[PRISONER_KINDS] = { { 128, 96, 62, 255 }, { 86, 104, 60, 255 }, { 70, 66, 64, 255 }, { 50, 44, 46, 255 } };
    BodyPose pose;
    BodyPoseParams bp = { .crouch = 1.0f, .scale = 1.0f, .held = 0.35f + 0.1f * sinf(time * 0.7f) }; // de rodillas, las manos juntas (las cadenas)
    body_pose(&pose, &bp);
    BodyColors bc = { cs_shade((Color){ 196, 156, 118, 255 }, at), cs_shade(CLOTH[k], at), cs_shade((Color){ 70, 56, 44, 255 }, at) };
    body_draw(&pose, at, yaw, bc, NULL);
}

static void draw_object(const Cave *c, uint64_t done_bits, int i, float time) {
    const CaveObject *o = &c->objects[i];
    bool done = is_done(done_bits, i);
    float yaw = 0.0f, ax, ay, az;
    cave_object_at(c, i, &ax, &ay, &az, &yaw);
    Vector3 at = { ax, ay, az };
    float s = o->size, fx = sinf(yaw), fz = cosf(yaw);
    bool carved = c->kind == CAVE_EMPIRE;
    switch (o->kind) {
    case COBJ_STALACTITE: { // de voxeles (src/world/voxcave.c), colgada de la boveda; en el imperio, una lampara de bronce
        if (carved) {
            float ly = cs_lamp_height(c, i);
            Vector3 ring = { at.x, ly + 0.55f, at.z };
            Color bronze = cs_shade((Color){ 150, 112, 64, 255 }, (Vector3){ at.x, ly, at.z }), chain = cs_shade((Color){ 70, 62, 52, 255 }, ring);
            DrawLine3D((Vector3){ at.x, at.y, at.z }, ring, chain);
            for (int k = 0; k < 3; k++) {
                float a = (float)k * 2.094f;
                DrawLine3D(ring, (Vector3){ at.x + sinf(a) * 0.26f, ly + 0.08f, at.z + cosf(a) * 0.26f }, chain);
            }
            DrawCylinderEx((Vector3){ at.x, ly - 0.12f, at.z }, (Vector3){ at.x, ly + 0.08f, at.z }, 0.1f, 0.28f, 8, bronze);
            float fl = 0.85f + 0.15f * sinf(time * 13.0f + (float)i);
            DrawSphereEx((Vector3){ at.x, ly + 0.14f, at.z }, 0.08f * fl, 5, 6, (Color){ 255, 170, 70, 255 });
            DrawSphereEx((Vector3){ at.x, ly + 0.17f, at.z }, 0.04f * fl, 4, 5, (Color){ 255, 236, 180, 255 });
            break;
        }
        float len = stalactite_len(o);
        vc_draw(VC_STALACTITE, (Region)c->region, variant_of(i), (Vector3){ at.x, at.y + 0.1f, at.z }, hash01(i, 3) * 2.0f * PI, len,
                cs_shade(WHITE, (Vector3){ at.x, at.y - len * 0.6f, at.z }));
        break;
    }
    case COBJ_STALAGMITE: {
        if (carved) { // un pedestal de piedra con su cuenco de ofrendas
            Color st = cs_shade(ColorBrightness((Color){ 160, 140, 118, 255 }, -0.1f), (Vector3){ at.x, at.y + 0.5f, at.z });
            DrawCube((Vector3){ at.x, at.y + 0.08f, at.z }, 0.8f, 0.16f, 0.8f, st);
            DrawCube((Vector3){ at.x, at.y + 0.55f, at.z }, 0.56f, 0.8f, 0.56f, st);
            DrawCube((Vector3){ at.x, at.y + 1.0f, at.z }, 0.74f, 0.12f, 0.74f, st);
            DrawCylinderEx((Vector3){ at.x, at.y + 1.06f, at.z }, (Vector3){ at.x, at.y + 1.2f, at.z }, 0.12f, 0.24f, 8, cs_shade((Color){ 150, 112, 64, 255 }, at));
            break;
        }
        float len = stalagmite_len(o);
        vc_draw(VC_STALAGMITE, (Region)c->region, variant_of(i), (Vector3){ at.x, at.y - 0.05f, at.z }, hash01(i, 5) * 2.0f * PI, len,
                cs_shade(WHITE, (Vector3){ at.x, at.y + len * 0.4f, at.z }));
        break;
    }
    case COBJ_WEB: {
        Vector3 ctr = { at.x, at.y, at.z };
        Color w = cs_shade((Color){ 210, 210, 216, 255 }, ctr);
        float ux = fz, uz = -fx; // a lo largo de la pared
        for (int k = 0; k < 8; k++) {
            float a = (float)k * PI / 4.0f, ex = cosf(a) * 0.6f, ey = sinf(a) * 0.6f;
            DrawLine3D(ctr, (Vector3){ ctr.x + ux * ex, ctr.y + ey, ctr.z + uz * ex }, w);
        }
        for (int ring = 1; ring <= 2; ring++)
            for (int k = 0; k < 8; k++) {
                float a0 = (float)k * PI / 4.0f, a1 = a0 + PI / 4.0f, rr = 0.22f * (float)ring;
                DrawLine3D((Vector3){ ctr.x + ux * cosf(a0) * rr, ctr.y + sinf(a0) * rr, ctr.z + uz * cosf(a0) * rr },
                           (Vector3){ ctr.x + ux * cosf(a1) * rr, ctr.y + sinf(a1) * rr, ctr.z + uz * cosf(a1) * rr }, w);
            }
        break;
    }
    case COBJ_INSECTS:
        for (int k = 0; k < 7; k++) {
            float a = time * (1.6f + 0.2f * (float)k) + (float)k * 0.9f;
            Vector3 b = { at.x + cosf(a) * 0.35f, at.y + 0.15f * sinf(a * 1.7f), at.z + sinf(a) * 0.35f };
            DrawCube(b, 0.035f, 0.035f, 0.035f, cs_shade((Color){ 40, 36, 30, 255 }, b));
        }
        break;
    case COBJ_VEIN: {
        if (done) break;
        int n = 0;
        const char *id = cave_object_loot(c, i, &n);
        Color ore = id && strstr(id, "cobre") ? (Color){ 70, 150, 128, 255 } : (Color){ 150, 84, 58, 255 };
        for (int k = 0; k < 5; k++) {
            float ox = (hash01(i, k) - 0.5f) * 0.7f, oy = (hash01(k, i) - 0.5f) * 0.5f, sz = 0.1f + 0.08f * hash01(i + k, 3);
            Vector3 b = { at.x + fz * ox, at.y + oy, at.z - fx * ox };
            DrawCube(b, sz, sz, sz, cs_shade(ore, b));
        }
        break;
    }
    case COBJ_BLOOD: DrawCylinder(at, 0.45f * s, 0.45f * s, 0.01f, 10, cs_shade((Color){ 80, 16, 14, 255 }, at)); break;
    case COBJ_PUDDLE: // en las naturales, agua; en las habitadas, una mancha mojada del suelo
        if (c->kind <= CAVE_ABANDONED) {
            cs_blob((Vector3){ at.x, at.y + 0.01f, at.z }, 0.8f, i * 3, (Color){ 22, 38, 52, 255 }, (Color){ 48, 70, 86, 255 }, true);
            cs_blob((Vector3){ at.x + 0.15f, at.y + 0.016f, at.z + 0.1f }, 0.22f, i * 3 + 1, (Color){ 96, 124, 146, 255 }, (Color){ 40, 62, 80, 255 }, true);
        } else {
            Color wet = ColorBrightness(cl_look()->floor, -0.45f);
            cs_blob((Vector3){ at.x, at.y + 0.01f, at.z }, 0.75f, i * 3, wet, ColorBrightness(cl_look()->floor, -0.25f), true);
        }
        break;
    case COBJ_BONES:
        if (done) break;
        for (int k = 0; k < 3; k++) {
            float a = hash01(i, k) * 2.0f * PI, l = 0.2f + 0.15f * hash01(k, i);
            Vector3 b = { at.x + cosf(a) * 0.25f, at.y + 0.03f, at.z + sinf(a) * 0.25f };
            DrawCylinderEx((Vector3){ b.x - cosf(a + 1.0f) * l, b.y, b.z - sinf(a + 1.0f) * l }, (Vector3){ b.x + cosf(a + 1.0f) * l, b.y, b.z + sinf(a + 1.0f) * l },
                           0.03f, 0.03f, 4, cs_shade((Color){ 222, 214, 192, 255 }, b));
        }
        DrawSphere((Vector3){ at.x, at.y + 0.09f, at.z }, 0.1f, cs_shade((Color){ 230, 222, 200, 255 }, at));
        break;
    case COBJ_NEST:
        if (done) break;
        DrawCylinder(at, 0.6f, 0.5f, 0.16f, 10, cs_shade((Color){ 92, 70, 44, 255 }, at));
        DrawCylinder((Vector3){ at.x, at.y + 0.1f, at.z }, 0.38f, 0.38f, 0.07f, 10, cs_shade((Color){ 60, 44, 28, 255 }, at));
        break;
    case COBJ_EGGS: {
        int n = (int)o->size;
        for (int k = 0; k < n && k < 6; k++) {
            float a = (float)k * 2.0f * PI / (float)n;
            Vector3 b = { at.x + cosf(a) * 0.14f, at.y, at.z + sinf(a) * 0.14f };
            DrawSphereEx(b, 0.075f, 6, 6, cs_shade((Color){ 224, 210, 170, 255 }, b));
        }
        break;
    }
    case COBJ_PREY: {
        float a = hash01(i, 7) * 2.0f * PI, ca = cosf(a), sa = sinf(a);
        Color hide = cs_shade((Color){ 108, 80, 56, 255 }, at), rib = cs_shade((Color){ 214, 204, 180, 255 }, at);
        DrawCylinderEx((Vector3){ at.x - ca * 0.5f, at.y + 0.18f, at.z - sa * 0.5f }, (Vector3){ at.x + ca * 0.45f, at.y + 0.16f, at.z + sa * 0.45f }, 0.2f,
                       0.16f, 6, hide);
        for (int k = 0; k < 4; k++) {
            float u = -0.25f + 0.17f * (float)k;
            Vector3 b = { at.x + ca * u, at.y + 0.3f, at.z + sa * u };
            DrawLine3D(b, (Vector3){ b.x - sa * 0.25f, at.y + 0.05f, b.z + ca * 0.25f }, rib);
        }
        DrawSphere((Vector3){ at.x + ca * 0.62f, at.y + 0.14f, at.z + sa * 0.62f }, 0.12f, hide);
        DrawCylinder((Vector3){ at.x + 0.1f, at.y, at.z - 0.2f }, 0.35f, 0.35f, 0.01f, 8, cs_shade((Color){ 70, 14, 12, 255 }, at));
        break;
    }
    case COBJ_WEAPON: {
        if (done) break;
        float a = hash01(i, 5) * 2.0f * PI, ca = cosf(a), sa = sinf(a);
        Vector3 b = { at.x, at.y + 0.03f, at.z };
        DrawCylinderEx((Vector3){ b.x - ca * 0.15f, b.y, b.z - sa * 0.15f }, (Vector3){ b.x + ca * 0.45f, b.y, b.z + sa * 0.45f }, 0.025f, 0.01f, 4,
                       cs_shade((Color){ 176, 176, 182, 255 }, b));
        DrawCylinderEx((Vector3){ b.x - ca * 0.15f, b.y, b.z - sa * 0.15f }, (Vector3){ b.x - ca * 0.4f, b.y, b.z - sa * 0.4f }, 0.03f, 0.03f, 4,
                       cs_shade((Color){ 96, 66, 40, 255 }, b));
        break;
    }
    case COBJ_LOOT: {
        if (done) break;
        static const Color GEM[4] = { { 64, 180, 170, 255 }, { 180, 56, 40, 255 }, { 50, 70, 160, 255 }, { 220, 170, 60, 255 } };
        DrawCube((Vector3){ at.x, at.y + 0.01f, at.z }, 0.7f, 0.02f, 0.55f, cs_shade((Color){ 104, 34, 30, 255 }, at));
        for (int k = 0; k < 6; k++) {
            Vector3 b = { at.x + (hash01(i, k) - 0.5f) * 0.5f, at.y + 0.06f, at.z + (hash01(k, i) - 0.5f) * 0.4f };
            DrawCube(b, 0.08f, 0.08f, 0.08f, cs_shade(GEM[k % 4], b));
        }
        break;
    }
    case COBJ_TORCH: {
        Vector3 base = { at.x, at.y - 0.35f, at.z }, top = { at.x + fx * 0.2f, at.y + 0.22f, at.z + fz * 0.2f };
        DrawCylinderEx(base, top, 0.035f, 0.045f, 5, cs_shade((Color){ 92, 66, 42, 255 }, top));
        if (cs_object_lit(c, done_bits, i)) hearth_add_flame((Vector3){ top.x, top.y + 0.02f, top.z }, 0.34f, 950u + (uint32_t)i);
        break;
    }
    case COBJ_COLUMN: { // del suelo a la boveda (la de verdad, mas baja junto a la pared), segun quien la puso
        const CaveRoom *r = &c->rooms[o->room];
        float h = cave_rock_above(c, at.x, at.z, o->room, r->floor_y) - r->floor_y + 0.2f;
        Vector3 up = { at.x, at.y + h, at.z };
        if (carved) { // el imperio: plinto, basa, fuste en tambores de sillares (los de la pared), capitel y abaco
            Color st = ColorBrightness(cl_look()->wall, -0.03f), dk = ColorBrightness(cl_look()->wall, -0.18f);
            cs_box((Vector3){ at.x, at.y + 0.15f, at.z }, 1.1f, 0.3f, 1.1f, 0.0f, dk);
            cs_solid((Vector3){ at.x, at.y + 0.3f, at.z }, (Vector3){ at.x, at.y + 0.46f, at.z }, 0.56f, 0.5f, 12, st, true, 1);
            for (float y = 0.46f; y < h - 0.6f; y += 1.0f) {
                float top = fminf(1.0f, h - 0.6f - y);
                Vector3 b = { at.x, at.y + y, at.z }, e = { at.x, at.y + y + top - 0.03f, at.z };
                cs_solid(b, e, 0.42f - 0.02f * y / h, 0.42f - 0.02f * (y + top) / h, 12, ((int)y & 1) ? st : ColorBrightness(st, -0.06f), true, 0);
                cs_solid(e, (Vector3){ e.x, e.y + 0.03f, e.z }, 0.39f, 0.39f, 12, dk, false, 0); // la junta
            }
            Vector3 cap = { at.x, at.y + h - 0.6f, at.z };
            cs_solid(cap, (Vector3){ cap.x, cap.y + 0.34f, cap.z }, 0.4f, 0.6f, 12, st, true, 0);
            cs_box((Vector3){ at.x, at.y + h - 0.16f, at.z }, 1.2f, 0.22f, 1.2f, 0.0f, dk);
        } else if (c->kind == CAVE_ABANDONED) { // rota: el fuste hasta donde quedo, con musgo arriba, y un tambor caido al lado
            float hb = 1.2f + 2.2f * hash01(i, 21), a = hash01(i, 22) * 2.0f * PI;
            Color st = (Color){ 150, 166, 150, 255 }, moss = (Color){ 88, 150, 72, 255 };
            Vector3 tp = { at.x, at.y + 0.3f + hb, at.z };
            cs_box((Vector3){ at.x, at.y + 0.15f, at.z }, 1.0f, 0.3f, 1.0f, a, ColorBrightness(st, -0.2f));
            cs_solid((Vector3){ at.x, at.y + 0.3f, at.z }, tp, 0.42f, 0.42f, 10, st, true, 0);
            cs_solid(tp, (Vector3){ tp.x + 0.1f, tp.y + 0.25f, tp.z }, 0.34f, 0.14f, 7, st, true, 1);
            cs_solid((Vector3){ tp.x, tp.y - 0.04f, tp.z }, (Vector3){ tp.x, tp.y + 0.08f, tp.z }, 0.46f, 0.3f, 9, moss, false, 1);
            Vector3 dr = { at.x + sinf(a) * 1.2f, at.y + 0.4f, at.z + cosf(a) * 1.2f };
            cs_solid((Vector3){ dr.x - cosf(a) * 0.45f, dr.y, dr.z + sinf(a) * 0.45f }, (Vector3){ dr.x + cosf(a) * 0.45f, dr.y, dr.z - sinf(a) * 0.45f }, 0.4f,
                     0.4f, 10, st, true, 3);
        } else if (c->kind == CAVE_TRIBE) { // un poste tallado con franjas pintadas
            static const Color BAND[3] = { { 150, 48, 34, 255 }, { 196, 140, 64, 255 }, { 40, 34, 30, 255 } };
            cs_solid(at, up, 0.22f, 0.2f, 7, (Color){ 112, 82, 54, 255 }, false, 0);
            for (int k = 0; k < 4; k++) {
                Vector3 b = { at.x, at.y + 0.9f + 0.55f * (float)k, at.z };
                cs_solid(b, (Vector3){ b.x, b.y + 0.16f, b.z }, 0.235f, 0.235f, 7, BAND[k % 3], false, 0);
            }
        } else { // los forajidos: un puntal de madera con su sombrero contra la roca
            Color wood = (Color){ 96, 70, 46, 255 };
            cs_solid(at, up, 0.2f, 0.17f, 6, wood, false, 0);
            cs_box((Vector3){ at.x, at.y + h - 0.25f, at.z }, 0.3f, 0.2f, 1.7f, hash01(i, 23) * PI, wood);
        }
        break;
    }
    case COBJ_CHEST: {
        Color wood = cs_shade((Color){ 112, 74, 42, 255 }, at), iron = cs_shade((Color){ 60, 58, 56, 255 }, at);
        rlPushMatrix();
        rlTranslatef(at.x, at.y, at.z);
        rlRotatef(hash01(i, 2) * 360.0f, 0, 1, 0);
        DrawCube((Vector3){ 0, 0.25f, 0 }, 0.9f, 0.5f, 0.55f, wood);
        DrawCube((Vector3){ -0.3f, 0.26f, 0 }, 0.05f, 0.52f, 0.57f, iron);
        DrawCube((Vector3){ 0.3f, 0.26f, 0 }, 0.05f, 0.52f, 0.57f, iron);
        if (done) DrawCube((Vector3){ 0, 0.68f, -0.3f }, 0.92f, 0.4f, 0.05f, wood); // abierto y vacio
        else DrawCube((Vector3){ 0, 0.54f, 0 }, 0.92f, 0.08f, 0.57f, wood);
        rlPopMatrix();
        break;
    }
    case COBJ_AMPHORA: {
        if (done) break;
        Color clay = cs_shade((Color){ 178, 98, 62, 255 }, at);
        DrawCylinderEx(at, (Vector3){ at.x, at.y + 0.15f, at.z }, 0.06f, 0.2f, 8, clay);
        DrawSphereEx((Vector3){ at.x, at.y + 0.38f, at.z }, 0.26f, 8, 8, clay);
        DrawCylinderEx((Vector3){ at.x, at.y + 0.58f, at.z }, (Vector3){ at.x, at.y + 0.8f, at.z }, 0.08f, 0.1f, 8, clay);
        break;
    }
    case COBJ_ARMS: {
        Color wood = cs_shade((Color){ 100, 72, 46, 255 }, at), steel = cs_shade((Color){ 170, 170, 176, 255 }, at);
        float ux = fz, uz = -fx;
        Vector3 l = { at.x - ux * 0.6f, at.y, at.z - uz * 0.6f }, r = { at.x + ux * 0.6f, at.y, at.z + uz * 0.6f };
        DrawCylinderEx(l, (Vector3){ l.x, l.y + 1.3f, l.z }, 0.05f, 0.05f, 4, wood);
        DrawCylinderEx(r, (Vector3){ r.x, r.y + 1.3f, r.z }, 0.05f, 0.05f, 4, wood);
        DrawCylinderEx((Vector3){ l.x, l.y + 1.1f, l.z }, (Vector3){ r.x, r.y + 1.1f, r.z }, 0.04f, 0.04f, 4, wood);
        if (!done)
            for (int k = 0; k < 3; k++) { // lanzas apoyadas
                float u = -0.35f + 0.35f * (float)k;
                Vector3 b = { at.x + ux * u + fx * 0.25f, at.y, at.z + uz * u + fz * 0.25f }, tp = { at.x + ux * u - fx * 0.1f, at.y + 1.9f, at.z + uz * u - fz * 0.1f };
                DrawCylinderEx(b, tp, 0.025f, 0.025f, 4, wood);
                DrawCylinderEx(tp, (Vector3){ tp.x - fx * 0.03f, tp.y + 0.25f, tp.z - fz * 0.03f }, 0.04f, 0.0f, 4, steel);
            }
        break;
    }
    case COBJ_CELL: {
        bool freed = done || !o->prisoner;
        Color iron = cs_shade((Color){ 74, 70, 66, 255 }, (Vector3){ at.x + fx * 0.6f, at.y + 1.0f, at.z + fz * 0.6f });
        bars(at, yaw, 0.85f, 2.2f, 9, freed && o->prisoner ? 3 : 99, freed && o->prisoner ? 5 : -1, iron); // liberado: la puerta abierta
        if (o->prisoner && !done) {
            Vector3 p = { at.x + fx * 0.4f, at.y, at.z + fz * 0.4f };
            draw_prisoner(p, yaw, (PrisonerKind)((o->prisoner - 1) % PRISONER_KINDS), time);
            DrawLine3D((Vector3){ at.x, at.y + 1.0f, at.z }, (Vector3){ p.x + fx * 0.3f, p.y + 0.75f, p.z + fz * 0.3f }, iron); // la cadena
        }
        break;
    }
    case COBJ_PAINTING: { // sobre la roca, segun quien pinto (docs/PLAN_CUEVAS_GRANDES.md, C9)
        float ux = fz, uz = -fx;
        Color rock = cs_shade(cl_look()->wall, at);
        if (c->kind == CAVE_TRIBE || c->kind == CAVE_BANDITS) { // el friso grande de la tribu, los palotes grandes de los forajidos
            const CaveRoom *rm = &c->rooms[o->room];
            float a0 = atan2f(at.x - rm->x, at.z - rm->z), r0 = fmaxf(hypotf(at.x - rm->x, at.z - rm->z), 1.0f);
            g_bend = (Bend){ true, c, o->room, a0, r0, 0.08f + 0.1f, painting_scale(c, o->room, a0, r0, c->kind == CAVE_TRIBE ? 2.2f : 1.6f) };
        }
        if (carved) { // el imperio: una tabla labrada con su marco y renglones de caracteres
            Color slab = cs_shade((Color){ 196, 182, 156, 255 }, at), glyph = cs_shade((Color){ 74, 64, 54, 255 }, at);
            wall_rect(at, ux, uz, fx, fz, -0.75f, -0.55f, 0.75f, 0.6f, 0.03f, cs_shade((Color){ 150, 136, 114, 255 }, at));
            wall_rect(at, ux, uz, fx, fz, -0.66f, -0.46f, 0.66f, 0.51f, 0.05f, slab);
            for (int row = 0; row < 6; row++)
                for (int k = 0; k < 9; k++) {
                    if (hash01(i * 13 + row, k) < 0.18f) continue;
                    float u = -0.56f + 0.14f * (float)k, v = 0.38f - 0.15f * (float)row;
                    wall_rect(at, ux, uz, fx, fz, u, v, u + 0.08f, v + 0.09f, 0.06f, glyph);
                }
        } else if (c->kind == CAVE_BANDITS) { // carbon: palotes contados de a cinco y un monigote
            Color coal = cs_shade((Color){ 30, 26, 24, 255 }, at);
            int groups = 2 + (int)(hash01(i, 31) * 3.0f);
            for (int g = 0; g < groups; g++) {
                float u0 = -0.7f + 0.32f * (float)g, v0 = -0.1f + 0.1f * hash01(i, g);
                for (int k = 0; k < 4; k++) wall_seg(at, ux, uz, fx, fz, u0 + 0.05f * (float)k, v0, u0 + 0.05f * (float)k + 0.01f, v0 + 0.3f, 0.018f, coal);
                wall_seg(at, ux, uz, fx, fz, u0 - 0.03f, v0 + 0.04f, u0 + 0.2f, v0 + 0.26f, 0.016f, coal);
            }
            float u = 0.45f; // el monigote, con su arco
            wall_seg(at, ux, uz, fx, fz, u, 0.0f, u, 0.32f, 0.02f, coal);
            wall_seg(at, ux, uz, fx, fz, u, 0.0f, u - 0.1f, -0.2f, 0.02f, coal);
            wall_seg(at, ux, uz, fx, fz, u, 0.0f, u + 0.1f, -0.2f, 0.02f, coal);
            wall_seg(at, ux, uz, fx, fz, u - 0.14f, 0.22f, u + 0.14f, 0.22f, 0.02f, coal);
            wall_rect(at, ux, uz, fx, fz, u - 0.06f, 0.33f, u + 0.06f, 0.45f, 0.03f, coal);
        } else { // la tribu (y lo que quedo en las abandonadas, desvaido): manos en negativo y cazadores con sus ciervos
            float faded = c->kind == CAVE_ABANDONED ? 0.6f : 0.0f;
            static const Color PIGMENT[3] = { { 168, 52, 30, 255 }, { 196, 120, 52, 255 }, { 40, 30, 28, 255 } };
            int hands = 3 + (int)(hash01(i, 41) * 4.0f);
            for (int k = 0; k < hands; k++) { // en la tribu, la mano pintada en rojo; en las abandonadas, el soplido y la mano del color de la roca
                float u = -0.65f + 1.3f * hash01(i, 42 + k), v = 0.05f + 0.45f * hash01(42 + k, i);
                Color pig = cs_shade(mixc(PIGMENT[k % 3], cl_look()->wall, faded), at), hand = rock;
                if (faded > 0.0f) wall_disc(at, ux, uz, fx, fz, u, v, 0.15f, 0.03f + 0.002f * (float)k, pig);
                else hand = cs_shade(k % 3 == 1 ? (Color){ 150, 44, 28, 255 } : (Color){ 132, 36, 24, 255 }, at);
                float off = 0.035f + 0.002f * (float)k, h = faded > 0.0f ? 1.0f : 0.6f; // las del friso grande, del tamano de una mano
                wall_rect(at, ux, uz, fx, fz, u - 0.045f * h, v - 0.08f * h, u + 0.045f * h, v + 0.02f * h, off, hand);
                for (int f = 0; f < 4; f++)
                    wall_seg(at, ux, uz, fx, fz, u + (-0.04f + 0.027f * (float)f) * h, v + 0.02f * h, u + (-0.05f + 0.035f * (float)f) * h, v + 0.1f * h, 0.016f * h, hand);
                wall_seg(at, ux, uz, fx, fz, u + 0.045f * h, v - 0.03f * h, u + 0.09f * h, v + 0.03f * h, 0.016f * h, hand);
            }
            Color red = cs_shade(mixc((Color){ 140, 40, 26, 255 }, cl_look()->wall, faded), at);
            for (int k = 0; k < 5; k++) { // el friso: cazadores (palotes con arco) y ciervos con su cornamenta
                float u = -0.62f + 0.31f * (float)k, v = -0.45f;
                if (k == 1 || k == 2 || k == 4) {
                    wall_seg(at, ux, uz, fx, fz, u + 0.22f, v + 0.33f, u + 0.29f, v + 0.42f, 0.01f, red); // la cornamenta, de dos puntas
                    wall_seg(at, ux, uz, fx, fz, u + 0.17f, v + 0.34f, u + 0.12f, v + 0.42f, 0.01f, red);
                    wall_rect(at, ux, uz, fx, fz, u - 0.14f, v + 0.1f, u + 0.14f, v + 0.18f, 0.03f, red);
                    for (int l = 0; l < 4; l++) wall_seg(at, ux, uz, fx, fz, u - 0.12f + 0.08f * (float)l, v + 0.1f, u - 0.13f + 0.08f * (float)l, v - 0.02f, 0.014f, red);
                    wall_seg(at, ux, uz, fx, fz, u + 0.14f, v + 0.18f, u + 0.2f, v + 0.26f, 0.016f, red);
                    wall_seg(at, ux, uz, fx, fz, u + 0.2f, v + 0.26f, u + 0.16f, v + 0.36f, 0.012f, red);
                    wall_seg(at, ux, uz, fx, fz, u + 0.2f, v + 0.26f, u + 0.27f, v + 0.35f, 0.012f, red);
                } else {
                    wall_seg(at, ux, uz, fx, fz, u, v, u + 0.02f, v + 0.24f, 0.016f, red);
                    wall_seg(at, ux, uz, fx, fz, u, v, u - 0.08f, v - 0.12f, 0.014f, red);
                    wall_seg(at, ux, uz, fx, fz, u, v, u + 0.08f, v - 0.12f, 0.014f, red);
                    wall_seg(at, ux, uz, fx, fz, u + 0.01f, v + 0.16f, u + 0.14f, v + 0.2f, 0.012f, red);
                    wall_seg(at, ux, uz, fx, fz, u + 0.12f, v + 0.06f, u + 0.14f, v + 0.32f, 0.01f, red);
                    wall_disc(at, ux, uz, fx, fz, u + 0.02f, v + 0.28f, 0.035f, 0.03f, red);
                }
            }
        }
        (void)rock;
        g_bend.on = false;
        break;
    }
    case COBJ_FIRE:
        if (c->kind == CAVE_TRIBE) // el fogon de la tribu (las referencias): un anillo alto de piedras alrededor
            for (int k = 0; k < 14; k++) {
                float a = (float)k * 2.0f * PI / 14.0f + hash01(i, 60), g = 0.88f + 0.24f * hash01(i, 61 + k);
                Color st = { (unsigned char)(168.0f * g), (unsigned char)(150.0f * g), (unsigned char)(128.0f * g), 255 };
                cs_box((Vector3){ at.x + sinf(a) * 1.05f, at.y + 0.17f, at.z + cosf(a) * 1.05f }, 0.5f, 0.34f, 0.32f, a, st);
            }
        hearth_draw_base(at, 0.9f, 300u + (uint32_t)i, cs_object_lit(c, done_bits, i));
        break;
    default: break;
    }
}

void cp_draw(const Cave *c, uint64_t done, Vector3 eye, float time) {
    for (int i = 0; i < c->object_count; i++) {
        const CaveObject *o = &c->objects[i];
        float dx = o->x - eye.x, dz = o->z - eye.z;
        if (dx * dx + dz * dz > CS_VIEW * CS_VIEW) continue;
        draw_object(c, done, i, time);
    }
}
