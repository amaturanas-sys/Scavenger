#include "body_draw.h"

#include "raymath.h"

static Vector3 world(V3 l, Vector3 pos, float yaw) {
    V3 w = body_to_world(l, (V3){ pos.x, pos.y, pos.z }, yaw);
    return (Vector3){ w.x, w.y, w.z };
}

Color armor_color(const ArmorPiece *pc) {
    static const Color mats[MAT_COUNT] = {
        [MAT_FELT] = { 150, 128, 98, 255 },  [MAT_LEATHER] = { 112, 76, 46, 255 }, [MAT_BRONZE] = { 192, 140, 70, 255 },
        [MAT_IRON] = { 118, 120, 126, 255 }, [MAT_STEEL] = { 172, 178, 188, 255 }, [MAT_GOLD] = { 222, 182, 64, 255 },
    };
    if (pc->durability <= 0.0f) return (Color){ 80, 76, 72, 255 };
    Color c = mats[pc->material];
    float k = 0.6f + 0.4f * pc->durability / pc->durability_max;
    return (Color){ (unsigned char)(c.r * k), (unsigned char)(c.g * k), (unsigned char)(c.b * k), 255 };
}

void body_draw(const BodyPose *b, Vector3 pos, float yaw, BodyColors c, const Armor *armor) {
    for (int i = 0; i < PART_COUNT; i++) {
        const BodySeg *s = &b->seg[i];
        Vector3 a = world(s->a, pos, yaw), e = world(s->b, pos, yaw);
        Color col = i == PART_HEAD || i == PART_NECK || i == PART_FOREARM_L || i == PART_FOREARM_R ? c.skin
                    : i >= PART_PELVIS && i != PART_UPPER_ARM_L && i != PART_UPPER_ARM_R ? c.legs
                                                                                          : c.cloth;
        if (i == PART_HEAD) DrawSphere(Vector3Lerp(a, e, 0.5f), s->radius * 1.05f, col);
        else DrawCapsule(a, e, s->radius, 6, 3, col);
        if (!armor) continue;
        // La pieza que cubre esta zona, un poco mas gruesa que el cuerpo.
        for (int k = 0; k < SLOT_COUNT; k++) {
            const ArmorPiece *pc = &armor->slot[k];
            if (!pc->id[0] || !(pc->zones & (1u << i))) continue;
            float grow = 1.25f + 0.1f * (float)(k == SLOT_GLOVES || k == SLOT_BOOTS);
            if (k == SLOT_GLOVES || k == SLOT_BOOTS) { // solo el extremo del miembro
                Vector3 m = Vector3Lerp(a, e, 0.65f);
                DrawCapsule(m, e, s->radius * grow, 6, 3, armor_color(pc));
            } else if (i == PART_HEAD) {
                DrawSphere(Vector3Lerp(a, e, 0.7f), s->radius * 1.2f, armor_color(pc));
            } else {
                DrawCapsule(a, e, s->radius * grow, 6, 3, armor_color(pc));
            }
            break;
        }
    }
}
