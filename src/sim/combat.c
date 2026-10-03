#include "combat.h"

#include <string.h>

WeaponStats weapon_stats(const char *id) {
    WeaponStats unarmed = { 6.0f, 1.1f, 0.7f, WOUND_BRUISE, false };
    if (!id || !id[0] || strncmp(id, "arma.", 5) != 0) return unarmed;
    WeaponStats w = { 16.0f, 1.5f, 0.85f, WOUND_CUT, false };
    if (strstr(id, "cuchillo") || strstr(id, "daga")) w = (WeaponStats){ 12.0f, 1.3f, 0.6f, WOUND_CUT, false };
    else if (strstr(id, "sable")) w = (WeaponStats){ 18.0f, 1.6f, 0.8f, WOUND_CUT, false };
    else if (strstr(id, "hacha")) w = (WeaponStats){ 20.0f, 1.5f, 0.95f, WOUND_CUT, false };
    else if (strstr(id, "maza")) w = (WeaponStats){ 20.0f, 1.5f, 1.0f, WOUND_BRUISE, false };
    else if (strstr(id, "espada")) w = (WeaponStats){ 24.0f, 1.9f, 1.0f, WOUND_CUT, false };
    else if (strstr(id, "lanza") || strstr(id, "pica") || strstr(id, "guja") || strstr(id, "alabarda"))
        w = (WeaponStats){ 22.0f, 2.6f, 1.1f, WOUND_CUT, true };
    else if (!strncmp(id, "arma.distancia.", 15)) return unarmed; // cuerpo a cuerpo, un arco no corta
    // El metal mejor corta mas: bronce y acero; las armas especiales, mas aun.
    if (strstr(id, "bronce")) w.damage *= 1.1f;
    if (strstr(id, "acero") || strstr(id, "damasquinado")) w.damage *= 1.3f;
    if (!strncmp(id, "arma.especial.", 14)) w.damage *= 1.2f;
    return w;
}

const EnemyDef *enemy_def(EnemyKind k) {
    static const EnemyDef defs[ENEMY_COUNT] = {
        [ENEMY_BANDIT] = { "Bandido", "personaje.npc.bandido", false, 70.0f, 12.0f, 1.6f, 1.2f, 4.2f, 22.0f, WOUND_CUT, 0.2f },
        [ENEMY_FANATIC] = { "Fanático del culto", "personaje.npc.fanatico", false, 80.0f, 15.0f, 1.7f, 1.1f, 4.5f, 26.0f,
                            WOUND_CUT, 0.0f },
        [ENEMY_CAPTOR] = { "Captor del culto", "personaje.npc.captor_culto", false, 90.0f, 10.0f, 1.6f, 1.3f, 4.0f, 24.0f,
                           WOUND_BRUISE, 0.15f },
        [ENEMY_WOLF] = { "Lobo", "animal.salvaje.lobo", true, 45.0f, 10.0f, 1.4f, 1.0f, 7.0f, 30.0f, WOUND_BITE, 0.35f },
    };
    return &defs[(unsigned)k < ENEMY_COUNT ? k : 0];
}

float combat_block_chance(bool shield, float facing) {
    if (!shield || facing < 0.35f) return 0.0f; // el escudo cubre el frente (unos 140 grados)
    return 0.55f;
}

float combat_damage(float base, float strength, float attack_scale, bool blocked, Rng *rng) {
    float d = base * strength * attack_scale * (0.8f + 0.4f * rng_float(rng));
    return blocked ? d * 0.15f : d;
}
