#include "game/cave_props.h"

#include <math.h>
#include <string.h>

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
    for (int i = 0; i < c->object_count; i++) {
        int k = c->objects[i].kind;
        if (k == COBJ_STALACTITE) vc_preload(VC_STALACTITE, (Region)c->region, variant_of(i));
        else if (k == COBJ_STALAGMITE) vc_preload(VC_STALAGMITE, (Region)c->region, variant_of(i));
    }
}

static void quad(Vector3 a, Color ca, Vector3 b, Color cb, Vector3 c, Color cc, Vector3 d, Color cd) {
    rlColor4ub(ca.r, ca.g, ca.b, 255), rlVertex3f(a.x, a.y, a.z);
    rlColor4ub(cb.r, cb.g, cb.b, 255), rlVertex3f(b.x, b.y, b.z);
    rlColor4ub(cc.r, cc.g, cc.b, 255), rlVertex3f(c.x, c.y, c.z);
    rlColor4ub(ca.r, ca.g, ca.b, 255), rlVertex3f(a.x, a.y, a.z);
    rlColor4ub(cc.r, cc.g, cc.b, 255), rlVertex3f(c.x, c.y, c.z);
    rlColor4ub(cd.r, cd.g, cd.b, 255), rlVertex3f(d.x, d.y, d.z);
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
    switch (o->kind) {
    case COBJ_STALACTITE: { // de voxeles (src/world/voxcave.c), colgada de la boveda
        float len = stalactite_len(o);
        vc_draw(VC_STALACTITE, (Region)c->region, variant_of(i), (Vector3){ at.x, at.y + 0.1f, at.z }, hash01(i, 3) * 2.0f * PI, len,
                cs_shade(WHITE, (Vector3){ at.x, at.y - len * 0.6f, at.z }));
        break;
    }
    case COBJ_STALAGMITE: {
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
    case COBJ_PUDDLE:
        DrawCylinder(at, 0.75f, 0.75f, 0.01f, 12, cs_shade((Color){ 34, 52, 70, 255 }, at));
        DrawCylinder((Vector3){ at.x + 0.15f, at.y + 0.005f, at.z + 0.1f }, 0.2f, 0.2f, 0.01f, 8, cs_shade((Color){ 90, 118, 140, 255 }, at));
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
        if (cs_object_lit(c, done_bits, i)) {
            float fl = 0.85f + 0.15f * sinf(time * 17.0f + (float)i);
            DrawSphere((Vector3){ top.x, top.y + 0.1f, top.z }, 0.12f * fl, (Color){ 250, 150, 50, 255 });
            DrawSphere((Vector3){ top.x, top.y + 0.13f, top.z }, 0.06f * fl, (Color){ 255, 236, 170, 255 });
        }
        break;
    }
    case COBJ_COLUMN: { // hiladas de ladrillo, del suelo a la boveda (la de verdad, mas baja junto a la pared)
        const CaveRoom *r = &c->rooms[o->room];
        float h = cave_rock_above(c, at.x, at.z, o->room, r->floor_y) - r->floor_y + 0.2f;
        for (float y = 0.0f; y < h; y += 0.5f) {
            Vector3 b = { at.x, at.y + y, at.z };
            Color brick = ((int)(y * 2.0f) & 1) ? (Color){ 138, 86, 60, 255 } : (Color){ 122, 76, 54, 255 };
            DrawCylinder(b, 0.45f, 0.45f, fminf(0.5f, h - y), 8, cs_shade(brick, (Vector3){ b.x, b.y + 0.25f, b.z }));
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
    case COBJ_PAINTING: {
        float ux = fz, uz = -fx;
        Color ochre = cs_shade((Color){ 190, 150, 104, 255 }, at), red = cs_shade((Color){ 150, 50, 34, 255 }, at);
        Vector3 a0 = { at.x - ux * 0.7f, at.y - 0.4f, at.z - uz * 0.7f }, a1 = { at.x + ux * 0.7f, at.y - 0.4f, at.z + uz * 0.7f };
        rlBegin(RL_TRIANGLES);
        quad(a0, ochre, a1, ochre, (Vector3){ a1.x, a1.y + 0.8f, a1.z }, ochre, (Vector3){ a0.x, a0.y + 0.8f, a0.z }, ochre);
        rlEnd();
        for (int k = 0; k < 5; k++) { // figuras: jinetes, ciervos, manos
            float u = -0.5f + 0.25f * (float)k, v = 0.15f + 0.4f * hash01(i, k);
            Vector3 b = { at.x + ux * u + fx * 0.02f, at.y - 0.4f + v, at.z + uz * u + fz * 0.02f };
            DrawCube(b, 0.1f + 0.06f * hash01(k, i), 0.06f + 0.06f * hash01(k + 3, i), 0.02f, red);
        }
        break;
    }
    case COBJ_FIRE: hearth_draw_base(at, 0.9f, 300u + (uint32_t)i, cs_object_lit(c, done_bits, i)); break;
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
