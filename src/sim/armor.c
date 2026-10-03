#include "armor.h"

#include <stdio.h>
#include <string.h>

const MaterialDef *material_def(ArmorMaterial m) {
    static const MaterialDef defs[MAT_COUNT] = {
        [MAT_FELT] = { "fieltro", 0.25f, 0.30f, 0.20f, 60.0f, 0.6f, 0.01f },
        [MAT_LEATHER] = { "cuero laminar", 0.40f, 0.25f, 0.30f, 90.0f, 0.8f, 0.02f },
        [MAT_BRONZE] = { "bronce", 0.55f, 0.30f, 0.45f, 140.0f, 1.2f, 0.05f },
        [MAT_IRON] = { "hierro", 0.65f, 0.30f, 0.50f, 180.0f, 1.4f, 0.06f },
        [MAT_STEEL] = { "acero", 0.75f, 0.35f, 0.60f, 240.0f, 1.7f, 0.06f },
        [MAT_GOLD] = { "oro", 0.45f, 0.20f, 0.35f, 80.0f, 0.7f, 0.07f },
    };
    return &defs[(unsigned)m < MAT_COUNT ? m : 0];
}

#define Z(p) (1u << (p))

static bool parse(const char *id, ArmorSlot *slot, ArmorPiece *pc) {
    if (!id || strncmp(id, "armadura.", 9) != 0) return false;
    const char *type = id + 9;
    struct { const char *prefix; ArmorSlot slot; unsigned zones; float coverage; } types[] = {
        { "casco.", SLOT_HELMET, Z(PART_HEAD), 0.9f },
        { "cuello.", SLOT_NECK, Z(PART_NECK), 0.85f },
        { "torso.", SLOT_TORSO, Z(PART_THORAX) | Z(PART_ABDOMEN), 0.95f },
        { "hombreras.", SLOT_SHOULDERS, Z(PART_UPPER_ARM_L) | Z(PART_UPPER_ARM_R), 0.8f },
        { "brazales.", SLOT_BRACERS, Z(PART_FOREARM_L) | Z(PART_FOREARM_R), 0.75f },
        { "guantes.", SLOT_GLOVES, Z(PART_FOREARM_L) | Z(PART_FOREARM_R), 0.35f },
        { "faldar.", SLOT_SKIRT, Z(PART_PELVIS) | Z(PART_THIGH_L) | Z(PART_THIGH_R), 0.7f },
        { "grebas.", SLOT_GREAVES, Z(PART_SHIN_L) | Z(PART_SHIN_R), 0.8f },
        { "botas.", SLOT_BOOTS, Z(PART_SHIN_L) | Z(PART_SHIN_R), 0.4f },
    };
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        size_t n = strlen(types[i].prefix);
        if (strncmp(type, types[i].prefix, n) != 0) continue;
        const char *mat = type + n;
        ArmorMaterial m = MAT_LEATHER;
        if (strstr(mat, "fieltro")) m = MAT_FELT;
        else if (strstr(mat, "cuero")) m = MAT_LEATHER;
        else if (strstr(mat, "bronce") || strstr(mat, "culto")) m = MAT_BRONZE;
        else if (strstr(mat, "acero")) m = MAT_STEEL;
        else if (strstr(mat, "hierro") || strstr(mat, "malla")) m = MAT_IRON;
        else if (strstr(mat, "oro")) m = MAT_GOLD;
        *slot = types[i].slot;
        memset(pc, 0, sizeof(*pc));
        snprintf(pc->id, sizeof(pc->id), "%s", id);
        pc->material = m;
        pc->zones = types[i].zones;
        pc->coverage = types[i].coverage;
        pc->mail = strstr(mat, "malla") != NULL;
        pc->durability = pc->durability_max = material_def(m)->durability;
        return true;
    }
    return false;
}

bool armor_equip(Armor *a, const char *inv_id) {
    ArmorSlot s;
    ArmorPiece pc;
    if (!parse(inv_id, &s, &pc)) return false;
    a->slot[s] = pc;
    return true;
}

void armor_unequip(Armor *a, ArmorSlot s) {
    if ((unsigned)s < SLOT_COUNT) memset(&a->slot[s], 0, sizeof(a->slot[s]));
}

static float piece_prot(const ArmorPiece *pc, WoundKind kind, bool projectile) {
    const MaterialDef *m = material_def(pc->material);
    float prot = projectile ? m->vs_pierce : (kind == WOUND_BRUISE || kind == WOUND_FRACTURE) ? m->vs_blunt : m->vs_cut;
    if (pc->mail) prot *= projectile ? 0.7f : (kind == WOUND_CUT ? 1.1f : 1.0f);
    // Gastada protege menos; rota, nada.
    if (pc->durability <= 0.0f) return 0.0f;
    float cond = pc->durability / pc->durability_max;
    return prot * (0.35f + 0.65f * cond);
}

float armor_absorb(Armor *a, BodyPart part, WoundKind *kind, bool projectile, float damage, Rng *rng, bool *broke) {
    if (broke) *broke = false;
    if (kind && *kind == WOUND_FROSTBITE) return damage; // el frio no se para con placas
    if (kind && *kind == WOUND_BURN) return damage * 0.85f; // el fuego pasa casi todo
    // La pieza que mejor cubre esa zona (si el impacto le da: cobertura).
    ArmorPiece *best = NULL;
    float best_prot = 0.0f;
    for (int s = 0; s < SLOT_COUNT; s++) {
        ArmorPiece *pc = &a->slot[s];
        if (!pc->id[0] || !(pc->zones & (1u << part)) || pc->durability <= 0.0f) continue;
        float prot = piece_prot(pc, kind ? *kind : WOUND_CUT, projectile);
        if (prot > best_prot) best_prot = prot, best = pc;
    }
    if (!best || rng_float(rng) >= best->coverage) return damage; // por un hueco
    float absorbed = damage * best_prot;
    // Desgaste: lo que absorbe, segun la dureza del material, y algo por cada golpe.
    best->durability -= absorbed * 2.0f / material_def(best->material)->hardness + damage * 0.1f;
    if (best->durability <= 0.0f) {
        best->durability = 0.0f;
        if (broke) *broke = true;
    }
    // Un filo que casi no pasa llega como golpe.
    if (kind && best_prot > 0.5f && (*kind == WOUND_CUT || *kind == WOUND_BITE)) *kind = WOUND_BRUISE;
    return damage - absorbed;
}

float armor_protection(const Armor *a, BodyPart part) {
    float best = 0.0f;
    for (int s = 0; s < SLOT_COUNT; s++) {
        const ArmorPiece *pc = &a->slot[s];
        if (!pc->id[0] || !(pc->zones & (1u << part))) continue;
        float p = piece_prot(pc, WOUND_CUT, false) * pc->coverage;
        if (p > best) best = p;
    }
    return best;
}

float armor_speed_scale(const Armor *a) {
    float w = 0.0f;
    for (int s = 0; s < SLOT_COUNT; s++)
        if (a->slot[s].id[0]) w += material_def(a->slot[s].material)->weight;
    return w > 0.4f ? 0.6f : 1.0f - w;
}

void armor_repair(Armor *a, float fraction) {
    for (int s = 0; s < SLOT_COUNT; s++) {
        ArmorPiece *pc = &a->slot[s];
        if (!pc->id[0]) continue;
        pc->durability += pc->durability_max * fraction;
        if (pc->durability > pc->durability_max) pc->durability = pc->durability_max;
    }
}

const char *slot_name(ArmorSlot s) {
    static const char *names[SLOT_COUNT] = { "casco",   "gorjal", "coraza", "hombreras", "brazales",
                                             "guanteletes", "faldar", "grebas", "botas" };
    return (unsigned)s < SLOT_COUNT ? names[s] : "?";
}
