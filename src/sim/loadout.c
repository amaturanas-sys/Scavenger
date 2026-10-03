#include "loadout.h"

#include <stdio.h>
#include <string.h>

void loadout_init(Loadout *l) { memset(l, 0, sizeof(*l)); }

bool loadout_equip_amulet(Loadout *l, int slot, const Charm *amulet) {
    if (slot < 0 || slot >= AMULET_SLOTS || !amulet) return false;
    l->amulets[slot] = *amulet;
    l->amulet_equipped[slot] = true;
    return true;
}

bool loadout_unequip_amulet(Loadout *l, int slot) {
    if (slot < 0 || slot >= AMULET_SLOTS || !l->amulet_equipped[slot]) return false;
    l->amulet_equipped[slot] = false;
    return true;
}

bool loadout_apply_tattoo(Loadout *l, const Charm *tattoo) {
    if (!tattoo || l->tattoo_count >= TATTOO_MAX) return false;
    for (int i = 0; i < l->tattoo_count; i++)
        if (strcmp(l->tattoos[i].id, tattoo->id) == 0) return false;
    l->tattoos[l->tattoo_count++] = *tattoo;
    return true;
}

// Recorre todos los charms vigentes (amuletos equipados + tatuajes).
typedef void (*CharmVisitor)(const Charm *c, void *ctx);

static void visit_charms(const Loadout *l, CharmVisitor fn, void *ctx) {
    for (int i = 0; i < AMULET_SLOTS; i++)
        if (l->amulet_equipped[i]) fn(&l->amulets[i], ctx);
    for (int i = 0; i < l->tattoo_count; i++) fn(&l->tattoos[i], ctx);
}

typedef struct { Stat stat; float sum; } StatCtx;

static void sum_stat(const Charm *c, void *vctx) {
    StatCtx *ctx = vctx;
    for (int i = 0; i < c->buff_count; i++)
        if (c->buffs[i].kind == BUFF_PASSIVE) ctx->sum += c->buffs[i].mods[ctx->stat];
}

float loadout_stat(const Loadout *l, Stat stat) {
    if (stat < 0 || stat >= STAT_COUNT) return 0.0f;
    StatCtx ctx = { stat, 0.0f };
    visit_charms(l, sum_stat, &ctx);
    return ctx.sum;
}

typedef struct { const Buff **out; int max; int n; } ActiveCtx;

static void collect_active(const Charm *c, void *vctx) {
    ActiveCtx *ctx = vctx;
    for (int i = 0; i < c->buff_count && ctx->n < ctx->max; i++)
        if (c->buffs[i].kind == BUFF_ACTIVE) ctx->out[ctx->n++] = &c->buffs[i];
}

int loadout_active_buffs(const Loadout *l, const Buff **out, int max) {
    ActiveCtx ctx = { out, max, 0 };
    visit_charms(l, collect_active, &ctx);
    return ctx.n;
}

typedef struct { const char *id; bool found; } FindCtx;

static void find_charm(const Charm *c, void *vctx) {
    FindCtx *ctx = vctx;
    if (strcmp(c->id, ctx->id) == 0) ctx->found = true;
}

bool skill_unlocked(const Loadout *l, const SkillNode *node) {
    if (!node || node->requires_charm[0] == '\0') return true; // nodo base
    FindCtx ctx = { node->requires_charm, false };
    visit_charms(l, find_charm, &ctx);
    return ctx.found;
}

Charm charm_make(const char *id) {
    Charm c;
    memset(&c, 0, sizeof(c));
    snprintf(c.id, CHARM_ID_LEN, "%s", id ? id : "");
    return c;
}

bool charm_add_buff(Charm *c, const char *buff_id, BuffKind kind, Stat stat, float value) {
    if (c->buff_count >= CHARM_MAX_BUFFS || stat < 0 || stat >= STAT_COUNT) return false;
    Buff *b = &c->buffs[c->buff_count++];
    memset(b, 0, sizeof(*b));
    snprintf(b->id, CHARM_ID_LEN, "%s", buff_id ? buff_id : "");
    b->kind = kind;
    b->mods[stat] = value;
    return true;
}

const char *stat_name(Stat s) {
    static const char *names[STAT_COUNT] = { "aguante", "agarre", "sigilo", "puntería", "carisma", "monta" };
    return (unsigned)s < STAT_COUNT ? names[s] : "?";
}

bool amulet_charm(const char *inv_id, Charm *out) {
    static const struct {
        const char *id;
        Stat a;
        float va;
        Stat b;
        float vb;
    } AMULETS[] = {
        { "accesorio.amuleto.lobo", STAT_STEALTH, 0.30f, STAT_GRAPPLE_POWER, 0.10f },
        { "accesorio.amuleto.ciervo", STAT_STAMINA_REGEN, 0.30f, STAT_RIDING, 0.10f },
        { "accesorio.amuleto.aguila_ibice", STAT_ARCHERY, 0.35f, STAT_STEALTH, 0.05f },
        { "accesorio.amuleto.tigre_dragon", STAT_GRAPPLE_POWER, 0.35f, STAT_CHARISMA, 0.05f },
        { "accesorio.amuleto.oso", STAT_STAMINA_REGEN, 0.20f, STAT_GRAPPLE_POWER, 0.20f },
        { "accesorio.amuleto.caballo", STAT_RIDING, 0.35f, STAT_CHARISMA, 0.10f },
    };
    for (size_t i = 0; i < sizeof(AMULETS) / sizeof(AMULETS[0]); i++) {
        if (strcmp(inv_id, AMULETS[i].id) != 0) continue;
        const char *short_id = strrchr(inv_id, '.') + 1;
        *out = charm_make(short_id);
        charm_add_buff(out, "a", BUFF_PASSIVE, AMULETS[i].a, AMULETS[i].va);
        charm_add_buff(out, "b", BUFF_PASSIVE, AMULETS[i].b, AMULETS[i].vb);
        return true;
    }
    return false;
}

int charm_describe(const Charm *c, char *out, int len) {
    int n = 0;
    out[0] = '\0';
    for (int b = 0; b < c->buff_count; b++)
        for (int s = 0; s < STAT_COUNT; s++) {
            float v = c->buffs[b].mods[s];
            if (v == 0.0f || n >= len) continue;
            n += snprintf(out + n, (size_t)(len - n), "%s%s %+d %%", n ? ", " : "", stat_name((Stat)s), (int)(v * 100.0f + (v > 0 ? 0.5f : -0.5f)));
        }
    return n;
}
