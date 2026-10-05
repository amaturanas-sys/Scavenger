#include "game/disasters_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/fauna_game.h"
#include "raymath.h"
#include "sim/clock.h"
#include "sim/hazards.h"
#include "sim/lang.h"

#define WILDFIRE_MIN 60.0f
#define WILDFIRE_MAX 140.0f
#define BOLT_RANGE 120.0f
#define BOLT_ATTRACT 25.0f  // m: lo alto atrae al rayo
#define BOLT_HURT 4.0f      // m: hiere a quien este cerca
#define CAMP_TRAMPLED 12.0f // m: el suelo pisado del campamento no tiene pasto
#define FIRE_SOURCE 4.0f    // m: para encender una flecha

static float dist_xz(float ax, float az, float bx, float bz) { return sqrtf((ax - bx) * (ax - bx) + (az - bz) * (az - bz)); }

static bool is_structure(const Prop *pr) {
    const char *id = pr->item->id;
    return !strncmp(id, "estructura.", 11) && strncmp(id, "estructura.ruina.", 17) != 0;
}

static bool is_fire_prop(const Prop *pr) {
    return !strcmp(pr->item->id, "estructura.campamento.fogata") || !strcmp(pr->item->id, "estructura.campamento.hoguera");
}

void dz_init(Disasters *dz, unsigned seed) {
    memset(dz, 0, sizeof(*dz));
    fire_init(&dz->fire, seed);
    rng_seed(&dz->rng, seed ^ 0xD15Au);
}

// ------------------------------------------------------------------ combustible
typedef struct {
    const Disasters *dz;
    const Camp *camp;
    const Props *props;
    const Terrain *t;
    const Climate *c;
} FuelCtx;

static FuelKind fuel_at(void *ud, float x, float z, int *ref) {
    const FuelCtx *f = ud;
    *ref = -1;
    for (int i = 0; i < f->camp->tree_count; i++)
        if (f->dz->tree_burn[i] < 1.0f && dist_xz(f->camp->trees[i].x, f->camp->trees[i].z, x, z) < 1.8f) {
            *ref = i;
            return FUEL_TREE;
        }
    for (int i = 0; i < f->props->count; i++) {
        const Prop *pr = &f->props->items[i];
        if (!is_structure(pr)) continue;
        float r = fmaxf(pr->item->w, pr->item->l) * 0.5f + 0.8f;
        if (dist_xz(pr->pos.x, pr->pos.z, x, z) < r) {
            *ref = i;
            return FUEL_STRUCTURE;
        }
    }
    // Pasto: no en el agua, la nieve, el desierto pelado ni el suelo pisado del campamento.
    float h = terrain_height(f->t, x, z);
    if (terrain_water(f->t, x, z) > h - 0.1f || f->c->snow_cover > 0.3f || h > f->t->look.snowline - 20.0f) return FUEL_NONE;
    if (biome_desert(f->t->seed, x, z) > 0.6f || sqrtf(x * x + z * z) < CAMP_TRAMPLED) return FUEL_NONE;
    return FUEL_GRASS;
}

static int structure_at(const Props *props, float x, float z, int hint) {
    if (hint >= 0 && hint < props->count && is_structure(&props->items[hint]) &&
        dist_xz(props->items[hint].pos.x, props->items[hint].pos.z, x, z) < 6.0f)
        return hint;
    int best = -1;
    float bd = 6.0f;
    for (int i = 0; i < props->count; i++) {
        const Prop *pr = &props->items[i];
        float d = dist_xz(pr->pos.x, pr->pos.z, x, z);
        if (is_structure(pr) && d < bd) bd = d, best = i;
    }
    return best;
}

// La estructura se pierde: quedan escombros (lluvia) o cenizas (fuego).
static void ruin(Props *props, int i, bool fire, char *log, size_t len) {
    Prop pr = props->items[i];
    props_remove(props, i);
    props_add(props, fire ? "estructura.ruina.cenizas" : "estructura.ruina.escombros", pr.pos, pr.yaw);
    snprintf(log, len, fire ? T("El fuego reduce a cenizas: %s.") : T("La lluvia torrencial derrumba: %s (sin mantenimiento)."),
             T(pr.item->name));
}

bool dz_ignite_at(Disasters *dz, const Camp *camp, const Props *props, const Terrain *t, const Climate *c, float x, float z) {
    if (c->rain > 0.3f) return false;
    FuelCtx f = { dz, camp, props, t, c };
    int ref;
    FuelKind k = fuel_at(&f, x, z, &ref);
    return fire_ignite(&dz->fire, x, z, k, ref);
}

// ------------------------------------------------------------------ rayos
static void lightning(Disasters *dz, const Climate *c, Camp *camp, GameActions *ga, Props *props, Combat *cb, Player *p,
                      const Terrain *t, float dryness, char *log, size_t len) {
    float a = rng_float(&dz->rng) * 2.0f * PI, r = rng_float(&dz->rng) * BOLT_RANGE;
    float x = p->pos.x + cosf(a) * r, z = p->pos.z + sinf(a) * r;
    // Lo alto atrae al rayo: el arbol o la estructura mas cercana a donde iba a caer.
    FuelKind kind = FUEL_GRASS;
    int ref = -1;
    float bd = BOLT_ATTRACT;
    for (int i = 0; i < camp->tree_count; i++) {
        float d = dist_xz(camp->trees[i].x, camp->trees[i].z, x, z);
        if (dz->tree_burn[i] < 1.0f && d < bd) bd = d, kind = FUEL_TREE, ref = i;
    }
    for (int i = 0; i < props->count; i++) {
        float d = dist_xz(props->items[i].pos.x, props->items[i].pos.z, x, z);
        if (is_structure(&props->items[i]) && props->items[i].item->h > 1.5f && d < bd) bd = d, kind = FUEL_STRUCTURE, ref = i;
    }
    if (kind == FUEL_TREE) x = camp->trees[ref].x, z = camp->trees[ref].z;
    if (kind == FUEL_STRUCTURE) x = props->items[ref].pos.x, z = props->items[ref].pos.z;
    float y = terrain_height(t, x, z);
    dz->bolt_to = (Vector3){ x, y, z };
    dz->bolt_time = 0.35f;
    dz->bolt_seed = rng_float(&dz->rng) * 100.0f;
    // Lo que toca: un arbol queda chamuscado; una estructura, dañada; y quiza prende.
    if (kind == FUEL_TREE) dz->tree_burn[ref] = fmaxf(dz->tree_burn[ref], 0.5f);
    if (kind == FUEL_STRUCTURE) {
        Prop *pr = &props->items[ref];
        pr->condition -= 0.35f;
        if (pr->condition <= 0.0f) {
            ruin(props, ref, true, log, len);
            kind = FUEL_GRASS, ref = -1;
        } else if (dist_xz(x, z, p->pos.x, p->pos.z) < 80.0f) {
            snprintf(log, len, T("¡Un rayo cae sobre: %s!"), T(pr->item->name));
        }
    }
    bool water = terrain_water(t, x, z) > y;
    if (!water && rng_float(&dz->rng) < fire_lightning_ignite_chance(dryness, c->rain))
        fire_ignite_hot(&dz->fire, x, z, kind, ref, 1.0f);
    // Quien este cerca.
    float dp = dist_xz(x, z, p->pos.x, p->pos.z);
    if (dp < BOLT_HURT && !cb->player.down) {
        health_hit(&cb->player, &cb->rng, 40.0f * (1.0f - dp / BOLT_HURT) + 10.0f, WOUND_BURN, PART_RANDOM);
        cb->hit_anim = 0.4f;
        snprintf(log, len, "%s", T("¡Te alcanza un rayo! Aléjate de lo alto durante la tormenta."));
    }
    for (int i = 0; i < ga->animal_count; i++) {
        const Animal *an = &ga->animals[i];
        if (an->used && an->state != ANIMAL_DEAD && dist_xz(an->x, an->z, x, z) < BOLT_HURT)
            fg_hurt(ga, i, 60.0f, WOUND_BURN, PART_RANDOM, -1, NULL, 0);
    }
}

// ------------------------------------------------------------------ actualizacion
void dz_update(Disasters *dz, const Climate *c, Camp *camp, GameActions *ga, Props *props, Troop *troop, Combat *cb,
               Player *p, const Terrain *t, float now, float dt, char *log, size_t len) {
    (void)troop;
    float dryness = fire_dryness(c->greenness, c->wetness, c->snow_cover, c->temperature);
    FuelCtx fctx = { dz, camp, props, t, c };
    FireEnv env = { fuel_at, &fctx, dryness, c->rain, 0.86f * c->wind * 6.0f, 0.5f * c->wind * 6.0f };
    FireEvent ev[24];
    int n = fire_update(&dz->fire, &env, dt, ev, 24);
    for (int i = 0; i < n; i++) {
        const FireEvent *e = &ev[i];
        if (e->fuel == FUEL_TREE && e->ref >= 0 && e->ref < camp->tree_count) {
            if (e->kind == FIRE_EV_BURNED_OUT) dz->tree_burn[e->ref] = 1.0f;
            else if (e->kind == FIRE_EV_EXTINGUISHED) dz->tree_burn[e->ref] = fmaxf(dz->tree_burn[e->ref], 0.5f);
        } else if (e->fuel == FUEL_STRUCTURE) {
            int s = structure_at(props, e->x, e->z, e->ref);
            if (s < 0) continue;
            if (e->kind == FIRE_EV_BURNED_OUT) ruin(props, s, true, log, len);
            else if (e->kind == FIRE_EV_EXTINGUISHED) props->items[s].condition -= 0.3f;
        }
        if (e->kind == FIRE_EV_SPREAD && !dz->camp_warned && dist_xz(e->x, e->z, 0.0f, 0.0f) < 45.0f) {
            snprintf(log, len, "%s", T("¡El fuego llega al campamento!"));
            dz->camp_warned = true;
        }
    }
    if (fire_count(&dz->fire) == 0) dz->camp_warned = false;
    // Incendio forestal en verano.
    bool summer = clock_season(clock_day(now)) == SEASON_SUMMER;
    if (fire_wildfire_roll(summer, dryness, c->rain, dt, &dz->rng)) {
        for (int k = 0; k < 6; k++) {
            float a = rng_float(&dz->rng) * 2.0f * PI, r = WILDFIRE_MIN + rng_float(&dz->rng) * (WILDFIRE_MAX - WILDFIRE_MIN);
            float x = p->pos.x + cosf(a) * r, z = p->pos.z + sinf(a) * r;
            int ref;
            FuelKind kd = fuel_at(&fctx, x, z, &ref);
            if (kd == FUEL_NONE || !fire_ignite_hot(&dz->fire, x, z, kd, ref, 0.8f)) continue;
            snprintf(log, len, "%s", T("Humo en el horizonte: ¡un incendio en la estepa seca!"));
            break;
        }
    }
    // Rayos.
    dz->bolt_time = fmaxf(0.0f, dz->bolt_time - dt);
    if (fire_lightning_roll(c->storm, dt, &dz->rng)) lightning(dz, c, camp, ga, props, cb, p, t, dryness, log, len);
    // Lluvia torrencial: se derrumban las estructuras descuidadas.
    for (int i = 0; i < props->count; i++) {
        Prop *pr = &props->items[i];
        if (!is_structure(pr)) continue;
        if (rng_float(&dz->rng) < structure_collapse_chance(pr->condition, c->rain, dt)) ruin(props, i--, false, log, len);
    }
    // La lluvia apaga todo fuego: antorcha, fogatas; la tribu las vuelve a encender al escampar.
    ga->raining = c->rain > 0.3f;
    if (ga->raining) {
        if (ga->torch_lit) {
            ga->torch_lit = false;
            snprintf(log, len, "%s", T("La lluvia apaga tu antorcha."));
        }
        if (!ga->fires_out) snprintf(log, len, "%s", T("La lluvia apaga las fogatas del campamento."));
        ga->fires_out = true;
        dz->relight_timer = 60.0f;
    } else if (ga->fires_out && c->rain < 0.1f) {
        dz->relight_timer -= dt;
        if (dz->relight_timer <= 0.0f) {
            ga->fires_out = false;
            snprintf(log, len, "%s", T("Escampó: la tribu vuelve a encender las fogatas."));
        }
    }
    // Hay fuego cerca (para encender una flecha)?
    ga->fire_near = fire_heat_at(&dz->fire, p->pos.x, p->pos.z, FIRE_SOURCE + 2.0f) > 0.2f;
    if (!ga->fires_out) {
        ga->fire_near |= dist_xz(camp->fire.x, camp->fire.z, p->pos.x, p->pos.z) < FIRE_SOURCE;
        for (int i = 0; i < props->count && !ga->fire_near; i++)
            ga->fire_near = is_fire_prop(&props->items[i]) &&
                            dist_xz(props->items[i].pos.x, props->items[i].pos.z, p->pos.x, p->pos.z) < FIRE_SOURCE;
    }
    // Flechas encendidas que cayeron: prenden lo que haya.
    for (int i = 0; i < ga->ignite_n; i++) {
        Vector3 at = ga->ignite_at[i];
        if (dz_ignite_at(dz, camp, props, t, c, at.x, at.z) && dist_xz(at.x, at.z, p->pos.x, p->pos.z) < 60.0f)
            snprintf(log, len, "%s", T("La flecha encendida prende fuego."));
    }
    ga->ignite_n = 0;
    // Pisar el fuego quema.
    dz->burn_tick -= dt;
    float heat = fire_heat_at(&dz->fire, p->pos.x, p->pos.z, 1.8f);
    if (heat > 0.3f && dz->burn_tick <= 0.0f && !cb->player.down) {
        health_hit(&cb->player, &cb->rng, 5.0f * fminf(heat, 2.0f), WOUND_BURN, health_random_limb(&cb->rng));
        cb->hit_anim = 0.3f;
        dz->burn_tick = 0.7f;
        dz->warn_timer -= 0.7f;
        if (dz->warn_timer <= 0.0f) snprintf(log, len, "%s", T("¡Te quemas! Sal del fuego.")), dz->warn_timer = 4.0f;
    }
    // Los animales en el fuego tambien.
    for (int i = 0; i < ga->animal_count; i++) {
        const Animal *an = &ga->animals[i];
        if (!an->used || an->state == ANIMAL_DEAD || species_def(an->species)->flier) continue;
        float h = fire_heat_at(&dz->fire, an->x, an->z, 1.5f);
        if (h > 0.4f && rng_float(&dz->rng) < dt * 1.5f) fg_hurt(ga, i, 4.0f * h, WOUND_BURN, PART_RANDOM, -1, NULL, 0);
    }
}

void dz_new_day(Disasters *dz, Props *props, GameActions *ga, const Troop *troop, bool rained, char *log, size_t len) {
    (void)dz;
    int structures = 0;
    for (int i = 0; i < props->count; i++) {
        Prop *pr = &props->items[i];
        if (!is_structure(pr)) continue;
        pr->condition = fmaxf(0.0f, pr->condition - structure_daily_wear(rained));
        structures++;
    }
    if (!structures) return;
    // Reparaciones: cada constructor arregla dos estructuras; el resto de la tribu, una entre todos.
    int builders = 0, active = 0;
    for (int i = 0; i < troop->count; i++) {
        if (troop->members[i].status != STATUS_ACTIVE) continue;
        active++;
        builders += troop->members[i].role == ROLE_BUILDER;
    }
    int repairs = builders * 2 + (active > 0);
    int fixed = 0;
    for (int k = 0; k < repairs; k++) {
        int worst = -1;
        for (int i = 0; i < props->count; i++)
            if (is_structure(&props->items[i]) && props->items[i].condition < 0.8f &&
                (worst < 0 || props->items[i].condition < props->items[worst].condition))
                worst = i;
        if (worst < 0) break;
        // Los troncos salen del acopio del campamento de esa estructura.
        Stockpile *st = ga_stock_at(ga, props->items[worst].pos.x, props->items[worst].pos.z);
        if (!stock_take(st, "utileria.material.troncos", 1)) break;
        props->items[worst].condition = fminf(1.0f, props->items[worst].condition + 0.5f);
        fixed++;
    }
    int neglected = 0;
    for (int i = 0; i < props->count; i++)
        neglected += is_structure(&props->items[i]) && props->items[i].condition < STRUCTURE_NEGLECTED;
    if (neglected)
        snprintf(log, len, T("%d estructura%s sin mantenimiento: la lluvia torrencial puede derrumbarla%s (faltan troncos o constructores)."),
                 neglected, neglected == 1 ? "" : "s", neglected == 1 ? "" : "s");
    else if (fixed)
        snprintf(log, len, T("La tribu repara %d estructura%s (troncos del acopio)."), fixed, fixed == 1 ? "" : "s");
}

// ------------------------------------------------------------------ dibujo
void dz_draw_world(const Disasters *dz, const Terrain *t, float time) {
    // Tierra calcinada.
    for (int i = 0; i < SCORCH_MAX; i++) {
        const Scorch *s = &dz->fire.scorch[i];
        if (s->r <= 0.0f || s->age > SCORCH_DAYS_SECONDS) continue;
        float y = terrain_height(t, s->x, s->z) + 0.04f;
        unsigned char a = (unsigned char)(200.0f * (1.0f - s->age / SCORCH_DAYS_SECONDS));
        DrawCylinder((Vector3){ s->x, y, s->z }, s->r, s->r, 0.02f, 7, (Color){ 30, 26, 22, a });
    }
    // Llamas y humo.
    for (int i = 0; i < FIRE_MAX; i++) {
        const FireCell *c = &dz->fire.cells[i];
        if (!c->used) continue;
        float y = terrain_height(t, c->x, c->z);
        float tall = c->kind == FUEL_TREE ? 3.5f : c->kind == FUEL_STRUCTURE ? 2.5f : 1.0f;
        float flick = 0.8f + 0.2f * sinf(time * 11.0f + (float)i * 1.7f);
        float h = tall * c->heat * flick, r = (c->kind == FUEL_GRASS ? 0.9f : 1.3f) * (0.5f + 0.5f * c->heat);
        DrawCylinder((Vector3){ c->x, y, c->z }, 0.0f, r, h, 5, (Color){ 236, 120, 34, 230 });
        DrawCylinder((Vector3){ c->x, y, c->z }, 0.0f, r * 0.55f, h * 0.7f, 5, (Color){ 255, 214, 90, 240 });
        float sm = fmodf(time * 0.8f + (float)i * 0.37f, 1.0f); // humo que sube
        DrawSphere((Vector3){ c->x + sm * 1.5f, y + tall + sm * 6.0f, c->z + sm * 0.8f }, 0.6f + sm * 1.4f,
                   (Color){ 70, 66, 62, (unsigned char)(150.0f * (1.0f - sm)) });
    }
    // El rayo: una linea quebrada desde el cielo.
    if (dz->bolt_time > 0.0f) {
        Vector3 a = { dz->bolt_to.x, dz->bolt_to.y + 60.0f, dz->bolt_to.z };
        for (int k = 0; k < 8; k++) {
            float f = (float)(k + 1) / 8.0f;
            Vector3 b = Vector3Lerp((Vector3){ dz->bolt_to.x, dz->bolt_to.y + 60.0f, dz->bolt_to.z }, dz->bolt_to, f);
            if (k < 7) b.x += sinf(dz->bolt_seed + (float)k * 2.3f) * 2.5f, b.z += cosf(dz->bolt_seed + (float)k * 1.7f) * 2.5f;
            DrawCylinderEx(a, b, 0.12f, 0.12f, 4, (Color){ 236, 240, 255, 255 });
            a = b;
        }
    }
}
