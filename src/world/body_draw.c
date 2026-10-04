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

static Color garment_color(const GarmentDef *g) { return (Color){ g->r, g->g, g->b, 255 }; }

void body_draw_outfit(const BodyPose *b, Vector3 pos, float yaw, const Outfit *o) {
    if (!o) return;
    const BodySeg *head = &b->seg[PART_HEAD], *thorax = &b->seg[PART_THORAX], *pelvis = &b->seg[PART_PELVIS];
    const GarmentDef *g;
    if ((g = garment(o->g[WEAR_BODY]))) { // tunica o deel: el tronco y los brazos, algo mas gruesos
        static const int parts[] = { PART_THORAX, PART_ABDOMEN, PART_PELVIS }; // los brazos siguen del color de su funcion
        for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); i++) {
            const BodySeg *s = &b->seg[parts[i]];
            DrawCapsule(world(s->a, pos, yaw), world(s->b, pos, yaw), s->radius * 1.12f, 6, 3, garment_color(g));
        }
    }
    if ((g = garment(o->g[WEAR_CLOAK]))) { // capa o abrigo: por la espalda, de los hombros a las rodillas
        float back = thorax->radius * 0.9f;
        V3 top = { thorax->b.x, thorax->b.y, thorax->b.z - back };
        const BodySeg *thigh = &b->seg[PART_THIGH_L];
        V3 low = { pelvis->a.x, (thigh->a.y + thigh->b.y) * 0.5f, pelvis->a.z - back };
        float r = thorax->radius * (g->warmth > 8.0f ? 1.05f : 0.85f);
        DrawCapsule(world(top, pos, yaw), world(low, pos, yaw), r, 6, 3, garment_color(g));
        DrawSphere(world((V3){ thorax->b.x, thorax->b.y, thorax->b.z - back * 0.5f }, pos, yaw), r * 1.1f, garment_color(g));
    }
    Vector3 hc = world((V3){ (head->a.x + head->b.x) * 0.5f, (head->a.y + head->b.y) * 0.5f, (head->a.z + head->b.z) * 0.5f }, pos, yaw);
    float hr = head->radius;
    if ((g = garment(o->g[WEAR_FACE]))) // pañuelo o bufanda: el cuello y la boca
        DrawCapsule(world(b->seg[PART_NECK].a, pos, yaw), world(b->seg[PART_NECK].b, pos, yaw), b->seg[PART_NECK].radius * 1.5f, 6, 3,
                    garment_color(g));
    if ((g = garment(o->g[WEAR_HEAD]))) {
        Vector3 top = { hc.x, hc.y + hr * 0.55f, hc.z };
        if (g->shade >= 0.4f) { // sombrero de ala ancha
            DrawCylinder((Vector3){ top.x, top.y - hr * 0.1f, top.z }, hr * 2.0f, hr * 2.0f, hr * 0.12f, 10, garment_color(g));
            DrawCylinder(top, hr * 0.8f, hr * 0.9f, hr * 0.7f, 8, garment_color(g));
        } else if (g->warmth < 3.0f) { // gorro de punta
            DrawCylinder((Vector3){ top.x, top.y - hr * 0.2f, top.z }, 0.0f, hr * 1.05f, hr * 1.4f, 8, garment_color(g));
        } else { // de piel (con orejas si es de lobo)
            DrawSphere(top, hr * 0.95f, garment_color(g));
            if (g->dread > 0.0f) {
                for (int side = -1; side <= 1; side += 2) {
                    Vector3 ear = world((V3){ (head->a.x + head->b.x) * 0.5f + 0.06f * (float)side, (head->a.y + head->b.y) * 0.5f + hr * 1.3f,
                                              (head->a.z + head->b.z) * 0.5f }, pos, yaw);
                    DrawCylinder(ear, 0.0f, hr * 0.35f, hr * 0.5f, 4, garment_color(g));
                }
            }
        }
    }
    if ((g = garment(o->g[WEAR_FEET]))) { // botas o sandalias: el final de la pierna
        static const int legs[] = { PART_SHIN_L, PART_SHIN_R };
        for (int i = 0; i < 2; i++) {
            const BodySeg *s = &b->seg[legs[i]];
            Vector3 a = world(s->a, pos, yaw), e = world(s->b, pos, yaw);
            DrawCapsule(Vector3Lerp(a, e, g->warmth > 3.5f ? 0.45f : 0.8f), e, s->radius * 1.3f, 6, 3, garment_color(g));
        }
    }
}
