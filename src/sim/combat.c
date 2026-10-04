#include "combat.h"
#include "sim/lang.h"

#include <math.h>
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
        [ENEMY_BANDIT] = { N_("Bandido"), "personaje.npc.bandido", false, 70.0f, 12.0f, 1.6f, 1.2f, 4.2f, 22.0f, WOUND_CUT, 0.2f,
                           NULL, { "armadura.torso.fieltro", "armadura.casco.fieltro" }, "arma.corta.sable", "escudo.mano.mimbre", 0.5f },
        [ENEMY_FANATIC] = { N_("Fanático del culto"), "personaje.npc.fanatico", false, 80.0f, 15.0f, 1.7f, 1.1f, 4.5f, 26.0f,
                            WOUND_CUT, 0.0f, NULL, { "armadura.torso.culto", "armadura.casco.mascara_culto" }, "arma.corta.hacha_mano", NULL, 0.0f },
        [ENEMY_CAPTOR] = { N_("Captor del culto"), "personaje.npc.captor_culto", false, 90.0f, 10.0f, 1.6f, 1.3f, 4.0f, 24.0f,
                           WOUND_BRUISE, 0.15f, NULL,
                           { "armadura.torso.culto", "armadura.faldar.culto", "armadura.grebas.culto", "armadura.casco.culto" },
                           "arma.corta.maza", "escudo.mano.cuero", 0.7f },
        [ENEMY_ARCHER] = { N_("Arquero bandido"), "personaje.npc.bandido", false, 60.0f, 7.0f, 1.3f, 1.0f, 4.4f, 34.0f,
                           WOUND_CUT, 0.3f, "arma.distancia.arco_compuesto", { "armadura.casco.fieltro" }, "arma.corta.daga", NULL, 0.0f },
        [ENEMY_RIDER] = { N_("Jinete bandido"), "personaje.npc.bandido", false, 75.0f, 14.0f, 2.0f, 1.1f, 8.5f, 40.0f, WOUND_CUT, 0.25f,
                          NULL, { "armadura.torso.laminar_cuero", "armadura.casco.laminar_cuero" }, "arma.corta.sable", NULL, 0.0f,
                          true },
    };
    return &defs[(unsigned)k < ENEMY_COUNT ? k : 0];
}

float mounted_momentum(float speed) { return 1.0f + fminf(fmaxf(speed, 0.0f), 12.0f) / 15.0f; }

static void add(LootItem *out, int *n, int max, const char *id, int count, float cond) {
    if (*n < max && count > 0) out[(*n)++] = (LootItem){ id, count, cond };
}

int enemy_loot(EnemyKind k, bool armed, bool shield, Rng *rng, LootItem *out, int max) {
    const EnemyDef *d = enemy_def(k);
    int n = 0;
    float wear = 0.4f + 0.5f * rng_float(rng); // lo que llevaba, usado
    if (armed && d->weapon && rng_float(rng) < 0.7f) add(out, &n, max, d->weapon, 1, wear);
    if (shield && d->shield && rng_float(rng) < 0.6f) add(out, &n, max, d->shield, 1, 0.3f + 0.6f * rng_float(rng));
    switch (k) {
    case ENEMY_BANDIT:
        if (rng_float(rng) < 0.5f) add(out, &n, max, "utileria.consumible.carne_seca", 1 + rng_range(rng, 2), 1.0f);
        if (rng_float(rng) < 0.3f) add(out, &n, max, "utileria.consumible.hierbas", 1, 1.0f);
        if (rng_float(rng) < 0.06f) add(out, &n, max, "accesorio.amuleto.lobo", 1, 1.0f);
        if (rng_float(rng) < 0.25f) add(out, &n, max, "utileria.material.seda", 1, 1.0f); // de las caravanas que asaltan
        if (rng_float(rng) < 0.3f) add(out, &n, max, "utileria.consumible.cerveza", 1, 1.0f);
        break;
    case ENEMY_FANATIC:
        if (rng_float(rng) < 0.5f) add(out, &n, max, "utileria.consumible.hierbas", 2, 1.0f);
        if (rng_float(rng) < 0.15f) add(out, &n, max, "accesorio.amuleto.tigre_dragon", 1, 1.0f);
        if (rng_float(rng) < 0.1f) add(out, &n, max, "utileria.consumible.unguento", 1, 1.0f);
        break;
    case ENEMY_CAPTOR:
        if (rng_float(rng) < 0.7f) add(out, &n, max, "utileria.material.cuerda", 1 + rng_range(rng, 2), 1.0f);
        if (rng_float(rng) < 0.08f) add(out, &n, max, "accesorio.amuleto.oso", 1, 1.0f);
        break;
    case ENEMY_ARCHER:
        if (rng_float(rng) < 0.5f) add(out, &n, max, d->ranged, 1, wear);
        if (rng_float(rng) < 0.9f) add(out, &n, max, "proyectil.flecha.comun", 3 + rng_range(rng, 7), 1.0f);
        if (rng_float(rng) < 0.08f) add(out, &n, max, "accesorio.amuleto.aguila_ibice", 1, 1.0f);
        break;
    case ENEMY_RIDER:
        if (rng_float(rng) < 0.6f) add(out, &n, max, "utileria.consumible.carne_seca", 2, 1.0f);
        if (rng_float(rng) < 0.15f) add(out, &n, max, "accesorio.amuleto.caballo", 1, 1.0f);
        if (rng_float(rng) < 0.3f) add(out, &n, max, "utileria.material.seda", 1 + rng_range(rng, 2), 1.0f);
        if (rng_float(rng) < 0.3f) add(out, &n, max, "utileria.consumible.vino", 1, 1.0f);
        break;
    default: break;
    }
    return n;
}

int combat_apply_hit(Health *h, Armor *a, Rng *rng, float damage, WoundKind kind, int part, bool projectile,
                     float *absorbed, bool *broke) {
    int zone = part == PART_RANDOM ? health_pick_part(rng) : part;
    float left = damage;
    if (broke) *broke = false;
    if (a) left = armor_absorb(a, (BodyPart)zone, &kind, projectile, damage, rng, broke);
    if (absorbed) *absorbed = damage - left;
    if (left < 0.5f) return -1; // la armadura lo paro
    return health_hit(h, rng, left, kind, zone);
}

float combat_block_chance(bool shield, float facing) {
    if (!shield || facing < 0.35f) return 0.0f; // el escudo cubre el frente (unos 140 grados)
    return 0.55f;
}

float combat_damage(float base, float strength, float attack_scale, bool blocked, Rng *rng) {
    float d = base * strength * attack_scale * (0.8f + 0.4f * rng_float(rng));
    return blocked ? d * 0.15f : d;
}
