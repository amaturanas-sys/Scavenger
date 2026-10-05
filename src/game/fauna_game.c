#include "game/fauna_game.h"

#include "game/inventory_game.h"

#define FODDER_ID "utileria.consumible.forraje"
#define WATER_ID "utileria.consumible.agua"
#define BOILED_ID "utileria.consumible.agua_hervida"
#include "game/talents_game.h"
#include "game/world_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"
#include "sim/anim_index.h"
#include "sim/apparel.h"
#include "sim/clock.h"
#include "sim/hazards.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "sim/lang.h"

#define POP_TARGET 16        // animales salvajes vivos alrededor del jugador
#define POP_RADIUS 150.0f
#define SPAWN_MIN 70.0f
#define SPAWN_MAX 110.0f
#define DESPAWN_DIST 190.0f
#define CAMP_CLEAR 50.0f     // no aparecen salvajes tan cerca del campamento
#define INTERACT_RANGE 3.0f
#define CORPSE_KEEP 300.0f   // s que se queda un cadaver entero
#define MAX_HUMANS (1 + TROOP_MAX + CB_MAX_ENEMIES)

// Personas de la ultima actualizacion: quienes son (para aplicar los ataques).
typedef struct {
    int kind; // 0 jugador, 1 integrante, 2 enemigo
    int id;   // id del integrante o indice del enemigo
} HumanRef;
static HumanRef g_refs[MAX_HUMANS];
static int g_ref_count;
static char g_hint[96];

static float adist(const Animal *a, Vector3 p) { return Vector2Distance((Vector2){ a->x, a->z }, (Vector2){ p.x, p.z }); }
static bool alive(const Animal *a) { return a->used && a->state != ANIMAL_DEAD; }
static float dist_xz(float ax, float az, float bx, float bz) { return sqrtf((ax - bx) * (ax - bx) + (az - bz) * (az - bz)); }

static const char *lower(const char *name, char *buf, size_t len) {
    snprintf(buf, len, "%s", name);
    if (buf[0] >= 'A' && buf[0] <= 'Z') buf[0] = (char)(buf[0] - 'A' + 'a');
    return buf;
}

static bool deep_water(const Terrain *t, float x, float z) {
    return terrain_deep_water(t, x, z, 0.6f);
}

// Profundidad del agua (para la fauna y los peces); helado, se camina por encima.
static const char *water_at_hand(const GameActions *ga, const Player *p);

static float water_depth_cb(void *ud, float x, float z) {
    const Terrain *t = ud;
    if (hazard_ice_walkable(terrain_ice(t, x, z))) return 0.0f;
    return terrain_water(t, x, z) - terrain_height(t, x, z);
}

static int free_slot(GameActions *ga) {
    for (int i = 0; i < GA_MAX_ANIMALS; i++) {
        if (ga->animals[i].used) continue;
        if (i >= ga->animal_count) ga->animal_count = i + 1;
        return i;
    }
    return -1;
}

// ------------------------------------------------------------------ aparicion
int fg_spawn_group(GameActions *ga, Species s, float x, float z) {
    const SpeciesDef *d = species_def(s);
    int n = d->group_min + rng_range(&ga->rng, d->group_max - d->group_min + 1);
    int group = d->social ? ga->next_group++ : -1;
    int made = 0;
    for (int k = 0; k < n; k++) {
        int i = free_slot(ga);
        if (i < 0) break;
        float ang = rng_float(&ga->rng) * 2.0f * PI, r = k == 0 ? 0.0f : 1.5f + rng_float(&ga->rng) * 3.5f;
        Animal *a = &ga->animals[i];
        animal_init(a, s, x + cosf(ang) * r, z + sinf(ang) * r);
        a->home_x = x, a->home_z = z;
        a->group = group;
        a->yaw = rng_float(&ga->rng) * 2.0f * PI;
        a->hunger = rng_float(&ga->rng) * 0.6f;
        a->rival_timer = -rng_float(&ga->rng) * 40.0f;
        made++;
    }
    return made;
}

int fg_spawn_one(GameActions *ga, Species s, float x, float z, float yaw) {
    int i = free_slot(ga);
    if (i < 0) return -1;
    Animal *a = &ga->animals[i];
    animal_init(a, s, x, z);
    a->home_x = x, a->home_z = z;
    a->group = -1;
    a->yaw = yaw;
    return i;
}

void fg_init(GameActions *ga, const Terrain *t) {
    ga->next_group = 1;
    // Ganado del campamento y manadas en los alrededores.
    static const struct {
        Species sp;
        float x, z;
    } START[] = {
        { SPECIES_GOAT, -20.0f, 12.0f },     { SPECIES_CALF, -24.0f, 6.0f },     { SPECIES_HORSE, 55.0f, 25.0f },
        { SPECIES_DEER, -55.0f, 40.0f },     { SPECIES_ANTELOPE, 75.0f, 70.0f }, { SPECIES_HARE, 28.0f, 34.0f },
        { SPECIES_DONKEY, -45.0f, -50.0f }, { SPECIES_WOLF, 70.0f, -85.0f },    { SPECIES_BOAR, -85.0f, -25.0f },
    };
    for (size_t i = 0; i < sizeof(START) / sizeof(START[0]); i++) {
        if (deep_water(t, START[i].x, START[i].z) && !species_def(START[i].sp)->aquatic) continue;
        fg_spawn_group(ga, START[i].sp, START[i].x, START[i].z);
    }
}

static int habitat_at(const Terrain *t, float x, float z) {
    if (deep_water(t, x, z)) return HAB_WATER;
    if (terrain_height(t, x, z) > t->look.snowline - 30.0f) return HAB_COLD; // cumbres nevadas
    return region_habitat(terrain_region(t, x, z));
}

static int pick_species(GameActions *ga, int hab, bool night) {
    float w[SPECIES_COUNT], total = 0.0f;
    for (int s = 0; s < SPECIES_COUNT; s++) {
        const SpeciesDef *d = species_def((Species)s);
        w[s] = (d->habitat & hab) && d->cls != CLASS_LIVESTOCK ? d->rarity : 0.0f;
        if (night && (d->hunt_max > 0.0f || d->cls == CLASS_HOSTILE)) w[s] *= 2.5f; // la noche es de los cazadores
        total += w[s];
    }
    if (total <= 0.0f) return -1;
    float r = rng_float(&ga->rng) * total;
    for (int s = 0; s < SPECIES_COUNT; s++) {
        if (r < w[s]) return s;
        r -= w[s];
    }
    return SPECIES_HARE;
}

static void populate(GameActions *ga, const Player *p, const Terrain *t, bool night, float dt) {
    ga->fauna_timer += dt;
    if (ga->fauna_timer < 8.0f) return;
    ga->fauna_timer = 0.0f;
    int wild = 0;
    for (int i = 0; i < ga->animal_count; i++) {
        Animal *a = &ga->animals[i];
        if (!a->used) continue;
        float d = adist(a, p->pos);
        // Lejos: los salvajes se van (los de la tribu, nunca).
        if (d > DESPAWN_DIST && (a->state == ANIMAL_WILD || a->state == ANIMAL_DEAD) && i != ga->mounted) {
            a->used = false;
            continue;
        }
        if (a->state == ANIMAL_WILD && d < POP_RADIUS) wild++;
    }
    while (ga->animal_count > 0 && !ga->animals[ga->animal_count - 1].used) ga->animal_count--;
    if (wild >= POP_TARGET) return;
    for (int tries = 0; tries < 6; tries++) {
        float ang = rng_float(&ga->rng) * 2.0f * PI, r = SPAWN_MIN + rng_float(&ga->rng) * (SPAWN_MAX - SPAWN_MIN);
        float x = p->pos.x + cosf(ang) * r, z = p->pos.z + sinf(ang) * r;
        if (sqrtf(x * x + z * z) < CAMP_CLEAR) continue;
        // Cerca de una guarida, a menudo aparece su dueño (junto a ella).
        int den = wd_den_near(t, p->pos.x, p->pos.z, SPAWN_MAX + 60.0f);
        if (den >= 0 && rng_float(&ga->rng) < 0.5f) {
            const Den *d = &t->world->dens[den];
            float da = rng_float(&ga->rng) * 2.0f * PI;
            fg_spawn_group(ga, (Species)d->species, d->x + cosf(da) * 8.0f, d->z + sinf(da) * 8.0f);
            return;
        }
        int s = pick_species(ga, habitat_at(t, x, z), night);
        if (s >= 0) fg_spawn_group(ga, (Species)s, x, z);
        return;
    }
}

bool fg_spawn_named(GameActions *ga, const char *what, const Player *p, float dist, char *log, size_t len) {
    int s = !strcmp(what, "lobos") ? SPECIES_WOLF : species_find(what);
    if (s < 0) {
        // Tambien en plural ("tigres", "jabalies").
        char buf[48];
        snprintf(buf, sizeof(buf), "%s", what);
        size_t l = strlen(buf);
        if (l > 3 && !strcmp(buf + l - 2, "es")) buf[l - 2] = '\0', s = species_find(buf);
        if (s < 0 && l > 2 && buf[l - 1] == 's') buf[l - 1] = '\0', s = species_find(buf);
        if (s < 0) return false;
    }
    float ang = p->yaw + (rng_float(&ga->rng) - 0.5f) * 0.6f;
    fg_spawn_group(ga, (Species)s, p->pos.x + sinf(ang) * dist, p->pos.z + cosf(ang) * dist);
    snprintf(log, len, T("Aparecen cerca: %s."), T(species_def((Species)s)->name));
    return true;
}

// ------------------------------------------------------------------ combate
static float animal_ground(const Animal *a, const Terrain *t) { return terrain_height(t, a->x, a->z); }

int fg_melee_target(const GameActions *ga, const Terrain *t, Vector3 pos, float yaw, float reach, float *dist) {
    Vector3 fwd = { sinf(yaw), 0, cosf(yaw) };
    int best = -1;
    float best_d = 1e9f;
    for (int i = 0; i < ga->animal_count; i++) {
        const Animal *a = &ga->animals[i];
        if (!alive(a) || a->ridden || (species_def(a->species)->flier && a->alt > 2.5f)) continue;
        if (fabsf(animal_ground(a, t) - pos.y) > 3.0f) continue;
        float d = adist(a, pos) - species_def(a->species)->size * 0.3f;
        Vector3 to = Vector3Normalize((Vector3){ a->x - pos.x, 0, a->z - pos.z });
        if (d < reach + 0.6f && d < best_d && (d < 0.8f || Vector3DotProduct(fwd, to) > 0.3f)) best_d = d, best = i;
    }
    if (dist) *dist = best_d;
    return best;
}

int fg_raycast(const GameActions *ga, const Terrain *t, V3 a, V3 d, float *tt, int *part) {
    int best = -1;
    float best_t = 2.0f;
    Vector3 mid = { a.x + d.x * 0.5f, a.y, a.z + d.z * 0.5f };
    float reach = sqrtf(d.x * d.x + d.z * d.z) * 0.5f + 3.5f;
    for (int i = 0; i < ga->animal_count; i++) {
        const Animal *an = &ga->animals[i];
        if (!alive(an) || an->ridden || adist(an, mid) > reach) continue;
        BodyPose b;
        animal_body(an, &b);
        V3 pos = { an->x, animal_ground(an, t), an->z };
        float tr;
        int hp = body_raycast(&b, body_to_local(a, pos, an->yaw), body_dir_to_local(d, an->yaw), 1.0f, &tr);
        if (hp >= 0 && tr < best_t) best_t = tr, best = i, *part = hp;
    }
    if (tt) *tt = best_t;
    return best;
}

static int human_index_of(int attacker) {
    for (int h = 0; h < g_ref_count; h++) {
        if (attacker == 0 && g_refs[h].kind == 0) return h;
        if (attacker > 0 && g_refs[h].kind == 1 && g_refs[h].id == attacker) return h;
    }
    return -1;
}

void fg_hurt(GameActions *ga, int idx, float dmg, WoundKind w, int part, int attacker, char *log, size_t len) {
    Animal *a = &ga->animals[idx];
    if (!alive(a)) return;
    const SpeciesDef *d = species_def(a->species);
    bool was_strong = !a->weakened;
    int wi = animal_hurt(ga->animals, ga->animal_count, idx, &ga->rng, dmg, w, part, attacker >= 0, human_index_of(attacker));
    if (attacker != 0 || !log) return;
    char who[48], dsc[96];
    lower(T(d->name), who, sizeof(who));
    if (a->state == ANIMAL_DEAD) {
        snprintf(log, len, T("Abatiste: %s. K para despiezarlo."), who);
        tg_xp(ga, XP_HUNT, log, len);
    } else if (d->cls == CLASS_TAMEABLE && a->weakened && was_strong && a->state == ANIMAL_WILD) {
        snprintf(log, len, T("El %s está débil: ¡lánzale el lazo (menú) y dale carne!"), who);
    } else if (wi >= 0) {
        wound_describe(&a->h.wounds[wi], true, dsc, sizeof(dsc));
        if (dsc[0] >= 'A' && dsc[0] <= 'Z') dsc[0] = (char)(dsc[0] - 'A' + 'a');
        snprintf(log, len, T("Hieres al %s: %s."), who, dsc);
    }
}

int fg_threat_near(const GameActions *ga, Vector3 pos, float range, float *dist) {
    int best = -1;
    float bd = range;
    for (int i = 0; i < ga->animal_count; i++) {
        const Animal *a = &ga->animals[i];
        if (!alive(a) || a->state != ANIMAL_WILD || (species_def(a->species)->flier && a->alt > 2.5f)) continue;
        AnimalClass c = species_def(a->species)->cls;
        if (c != CLASS_HOSTILE && c != CLASS_TAMEABLE && a->tkind != TGT_HUMAN) continue;
        float d = adist(a, pos);
        if (d < bd) bd = d, best = i;
    }
    if (dist) *dist = bd;
    return best;
}

// ------------------------------------------------------------------ enjambres
#define SWARM_CELL 40.0f
#define SWARM_RANGE 3       // celdas alrededor del jugador
#define SWARM_FAR 160.0f
#define HIVE_RANGE 2.5f

static uint32_t cell_hash(unsigned seed, int cx, int cz) {
    uint32_t h = seed * 0x9E3779B1u ^ (uint32_t)cx * 0x85EBCA77u ^ (uint32_t)cz * 0xC2B2AE3Du;
    h ^= h >> 15, h *= 0x2C1B3C6Du, h ^= h >> 12;
    return h;
}

static Swarm *new_swarm(GameActions *ga) {
    for (int i = 0; i < GA_MAX_SWARMS; i++)
        if (!ga->swarms[i].used) return &ga->swarms[i];
    return NULL;
}

static bool cell_taken(const GameActions *ga, int cx, int cz) {
    for (int i = 0; i < GA_MAX_SWARMS; i++)
        if (ga->swarms[i].used && ga->swarms[i].kind != SWARM_FLIES && ga->swarms[i].cell_x == cx && ga->swarms[i].cell_z == cz)
            return true;
    return false;
}

// Cada celda del mundo tiene (o no) su enjambre, siempre el mismo: peces y mosquitos en
// el agua, colmenas y avisperos en tierra (no en el desierto ni en la nieve).
static void seed_cell(GameActions *ga, const Terrain *t, int cx, int cz) {
    if (cell_taken(ga, cx, cz)) return;
    uint32_t h = cell_hash(t->seed, cx, cz);
    float r = (float)(h & 0xFFFF) / 65536.0f;
    float x = ((float)cx + 0.2f + 0.6f * (float)((h >> 16) & 0xFF) / 255.0f) * SWARM_CELL;
    float z = ((float)cz + 0.2f + 0.6f * (float)((h >> 24) & 0xFF) / 255.0f) * SWARM_CELL;
    if (sqrtf(x * x + z * z) < 30.0f) return; // no en el campamento
    SwarmKind k;
    float y;
    if (deep_water(t, x, z) && water_depth_cb((void *)t, x, z) > 1.0f) {
        if (r < 0.55f) k = SWARM_FISH;
        else if (r < 0.8f) k = SWARM_MOSQUITOES;
        else return;
        y = terrain_water(t, x, z);
        if (k == SWARM_MOSQUITOES) y += 0.2f;
    } else {
        if (deep_water(t, x, z) || terrain_region(t, x, z) == REGION_DESERT || terrain_height(t, x, z) > t->look.snowline - 30.0f) return;
        if (r < 0.12f) k = SWARM_BEES;
        else if (r < 0.2f) k = SWARM_WASPS;
        else return;
        y = terrain_height(t, x, z);
    }
    Swarm *s = new_swarm(ga);
    if (!s) return;
    swarm_init(s, k, x, y, z, &ga->rng);
    s->cell_x = cx, s->cell_z = cz;
}

static void flies_on_corpses(GameActions *ga, const Terrain *t) {
    for (int i = 0; i < ga->animal_count; i++) {
        const Animal *a = &ga->animals[i];
        if (!a->used || a->state != ANIMAL_DEAD || a->butchered || a->corpse < 20.0f) continue;
        bool has = false;
        for (int k = 0; k < GA_MAX_SWARMS && !has; k++) {
            const Swarm *s = &ga->swarms[k];
            has = s->used && s->kind == SWARM_FLIES && fabsf(s->hx - a->x) < 1.0f && fabsf(s->hz - a->z) < 1.0f;
        }
        if (has) continue;
        Swarm *s = new_swarm(ga);
        if (!s) return;
        swarm_init(s, SWARM_FLIES, a->x, terrain_height(t, a->x, a->z), a->z, &ga->rng);
        s->cell_x = i; // el cadaver del que viven
    }
}

static void update_swarms(GameActions *ga, Combat *cb, const Player *p, const Terrain *t, float now, float temp, float dt,
                          char *log, size_t len) {
    ga->swarm_timer += dt;
    ga->sting_timer = fmaxf(0.0f, ga->sting_timer - dt);
    if (ga->swarm_timer > 3.0f) {
        ga->swarm_timer = 0.0f;
        for (int i = 0; i < GA_MAX_SWARMS; i++) { // lejos, o sin su cadaver: fuera
            Swarm *s = &ga->swarms[i];
            if (!s->used) continue;
            float d = dist_xz(s->hx, s->hz, p->pos.x, p->pos.z);
            bool corpse_gone = s->kind == SWARM_FLIES && (s->cell_x >= ga->animal_count || !ga->animals[s->cell_x].used ||
                                                          ga->animals[s->cell_x].state != ANIMAL_DEAD);
            if (d > SWARM_FAR || corpse_gone || (s->kind == SWARM_FISH && swarm_alive(s) == 0)) s->used = false;
        }
        int pcx = (int)floorf(p->pos.x / SWARM_CELL), pcz = (int)floorf(p->pos.z / SWARM_CELL);
        for (int dz = -SWARM_RANGE; dz <= SWARM_RANGE; dz++)
            for (int dx = -SWARM_RANGE; dx <= SWARM_RANGE; dx++) seed_cell(ga, t, pcx + dx, pcz + dz);
        flies_on_corpses(ga, t);
    }
    DayPhase ph = clock_phase(now);
    SwarmCtx c = { p->pos.x, p->pos.y, p->pos.z, cb->player.down, ga->torch_lit && !ga->hands.sheathed,
                   terrain_deep_water(t, p->pos.x, p->pos.z, 1.3f), temp,
                   ph == PHASE_NIGHT, ph == PHASE_DUSK || ph == PHASE_DAWN, water_depth_cb, (void *)t };
    for (int i = 0; i < GA_MAX_SWARMS; i++) {
        Swarm *s = &ga->swarms[i];
        if (!s->used) continue;
        if (s->kind == SWARM_FISH && hazard_ice_walkable(terrain_ice(t, s->cx, s->cz))) continue; // bajo el hielo
        if (s->kind == SWARM_FISH) s->hy = terrain_water(t, s->cx, s->cz);                         // el nivel cambia con la estacion
        SwarmHit hit = swarm_update(s, &c, &ga->rng, dt);
        if (!hit.stings) continue;
        cb->player.hp -= hit.damage;
        health_poison(&cb->player, hit.venom);
        if (ga->sting_timer <= 0.0f) {
            char who[48];
            const char *tip = s->kind == SWARM_MOSQUITOES ? T(" (el fuego los espanta)")
                                                          : T(" ¡Corre, métete al agua o usa humo!");
            snprintf(log, len, T("Te pican: %s.%s"), lower(T(swarm_def(s->kind)->name), who, sizeof(who)), tip);
            ga->sting_timer = 4.0f;
        }
    }
}

bool fg_fish(GameActions *ga, const Terrain *t, Vector3 pos, float yaw, float reach, bool spear, char *log, size_t len) {
    float ax = pos.x + sinf(yaw) * reach * 0.8f, az = pos.z + cosf(yaw) * reach * 0.8f;
    if (hazard_ice_walkable(terrain_ice(t, ax, az))) return false;
    Vector3 at = { ax, terrain_water(t, ax, az) - 0.3f, az };
    for (int i = 0; i < GA_MAX_SWARMS; i++) {
        Swarm *s = &ga->swarms[i];
        if (!s->used || s->kind != SWARM_FISH || dist_xz(s->cx, s->cz, at.x, at.z) > 4.0f) continue;
        if (rng_float(&ga->rng) > (spear ? 0.7f : 0.3f)) {
            snprintf(log, len, "%s", T("Los peces se escapan entre tus piernas."));
            return true;
        }
        if (swarm_catch(s, at.x, at.y, at.z, spear ? 1.4f : 1.0f, 1)) {
            bool kept = ig_store(ga, NULL, &(Player){ .pos = pos }, FRESH_MEAT_ID, 1, 1.0f) == 1;
            snprintf(log, len, "%s", kept ? T("¡Pescaste un pez! (+1 carne fresca)") : T("¡Pescaste un pez! Pero no te cabe: lo sueltas."));
            return true;
        }
    }
    return false;
}

bool fg_shot_water(GameActions *ga, const Terrain *t, Vector3 at, char *log, size_t len) {
    for (int i = 0; i < GA_MAX_SWARMS; i++) {
        Swarm *s = &ga->swarms[i];
        if (!s->used || s->kind != SWARM_FISH || dist_xz(s->cx, s->cz, at.x, at.z) > 4.0f) continue;
        if (swarm_catch(s, at.x, terrain_water(t, at.x, at.z) - 0.3f, at.z, 1.2f, 1)) {
            ig_store(ga, NULL, &(Player){ .pos = at }, FRESH_MEAT_ID, 1, 1.0f);
            snprintf(log, len, "%s", T("¡La flecha atraviesa un pez! (+1 carne fresca)"));
            return true;
        }
    }
    return false;
}

bool fg_poke_nest(GameActions *ga, Vector3 pos, float reach) {
    for (int i = 0; i < GA_MAX_SWARMS; i++) {
        Swarm *s = &ga->swarms[i];
        if (!s->used || !swarm_def(s->kind)->nest || dist_xz(s->hx, s->hz, pos.x, pos.z) > reach + 0.5f) continue;
        swarm_provoke(s);
        return true;
    }
    return false;
}

static Swarm *hive_near(GameActions *ga, const Player *p) {
    for (int i = 0; i < GA_MAX_SWARMS; i++) {
        Swarm *s = &ga->swarms[i];
        if (s->used && s->kind == SWARM_BEES && dist_xz(s->hx, s->hz, p->pos.x, p->pos.z) < HIVE_RANGE) return s;
    }
    return NULL;
}

// ------------------------------------------------------------------ interaccion (K)
static int nearest_interactable(const GameActions *ga, const Player *p, int *what) {
    // what: 1 atado (dar de comer), 2 cadaver (despiezar), 3 ganado (ordeñar / sacrificar)
    int best = -1;
    float bd = INTERACT_RANGE;
    for (int i = 0; i < ga->animal_count; i++) {
        const Animal *a = &ga->animals[i];
        if (!a->used || a->ridden || (species_def(a->species)->flier && a->alt > 2.0f && a->state != ANIMAL_DEAD)) continue;
        int w = a->state == ANIMAL_BOUND                                                              ? 1
                : a->state == ANIMAL_DEAD && !a->butchered                                             ? 2
                : animal_domestic(a) && a->thirst > 0.5f && water_at_hand(ga, p)                ? 5
                : animal_domestic(a) && a->hunger > 0.3f                                       ? 4
                : a->state == ANIMAL_TAMED && species_def(a->species)->cls == CLASS_LIVESTOCK ? 3
                                                                                                       : 0;
        if (!w) continue;
        float d = adist(a, p->pos) - species_def(a->species)->size * 0.3f;
        if (d < bd) bd = d, best = i, *what = w;
    }
    return best;
}

// Agua para dar de beber a un animal: cruda o hervida (a ellos no les hacen nada los espiritus).
static const char *water_at_hand(const GameActions *ga, const Player *p) {
    if (ig_count(ga, NULL, p, WATER_ID) > 0) return WATER_ID;
    if (ig_count(ga, NULL, p, BOILED_ID) > 0) return BOILED_ID;
    return NULL;
}

static void butcher(GameActions *ga, const Player *p, Animal *a, char *log, size_t len, const char *how) {
    int meat = 0, hide = 0;
    if (!animal_butcher(a, &meat, &hide)) return;
    // A lo que lleves encima (o a lo que tengas cerca); lo que no cabe se queda.
    // Los depredadores, el reno y la cabra dan su piel (ropa: src/sim/apparel.h); el resto, pieles curtidas.
    const char *pelt = hide > 0 ? species_pelt(a->species) : NULL;
    int kept = ig_store(ga, NULL, p, FRESH_MEAT_ID, meat, 1.0f) + ig_store(ga, NULL, p, HIDE_ID, pelt ? hide - 1 : hide, 1.0f);
    if (pelt) kept += ig_store(ga, NULL, p, pelt, 1, 1.0f);
    // Para fabricar: plumas de las aves; huesos y tendones de los grandes.
    const SpeciesDef *sd = species_def(a->species);
    if (sd->flier) ig_store(ga, NULL, p, "utileria.material.plumas", 3, 1.0f);
    else if (sd->size >= 1.0f) {
        int k = sd->size >= 2.0f ? 2 : 1;
        ig_store(ga, NULL, p, "utileria.material.hueso", k, 1.0f);
        ig_store(ga, NULL, p, "utileria.material.tendones", k, 1.0f);
    }
    char who[48];
    snprintf(log, len, T("%s %s: +%d carne fresca, +%d pieles%s"), how, lower(T(species_def(a->species)->name), who, sizeof(who)),
             meat, hide, kept < meat + hide ? TextFormat(T(" (no te cabe todo: %d se quedan)"), meat + hide - kept) : ".");
    if (pelt) {
        size_t used = strlen(log);
        if (used + 2 < len) snprintf(log + used, len - used, " %s", TextFormat(T("Una es %s."), lower(inventory_find(ga->inv, pelt) ? T(inventory_find(ga->inv, pelt)->name) : pelt, who, sizeof(who))));
    }
}

static void interact(GameActions *ga, const Player *p, bool shift, char *log, size_t len) {
    Swarm *hive = hive_near(ga, p);
    if (hive) { // la miel: con humo, las abejas se calman
        if (ga->torch_lit && !ga->hands.sheathed) {
            swarm_smoke(hive);
            if (hive->honey_taken) {
                snprintf(log, len, "%s", T("Esta colmena ya no tiene miel hoy."));
            } else {
                hive->honey_taken = true;
                ig_store(ga, NULL, p, "utileria.consumible.miel", 2, 1.0f);
                snprintf(log, len, "%s", T("El humo calma a las abejas: tomas miel (+2)."));
            }
        } else {
            swarm_provoke(hive);
            snprintf(log, len, "%s", T("¡Las abejas defienden la colmena! Hace falta humo (antorcha encendida)."));
        }
        return;
    }
    int what = 0, i = nearest_interactable(ga, p, &what);
    if (i < 0) {
        snprintf(log, len, "%s", T("No hay ningún animal con el que hacer algo aquí."));
        return;
    }
    Animal *a = &ga->animals[i];
    char who[48];
    lower(T(species_def(a->species)->name), who, sizeof(who));
    if (what == 1) {
        const char *food = ig_count(ga, NULL, p, FRESH_MEAT_ID) > 0 ? FRESH_MEAT_ID
                           : ig_count(ga, NULL, p, FOOD_ID) > 0    ? FOOD_ID
                                                                   : NULL;
        if (!food) {
            snprintf(log, len, T("Necesitas llevar carne para darle de comer al %s."), who);
            return;
        }
        ig_use(ga, NULL, p, food, 1);
        animal_feed(a, 0.0f, 0.0f);
        snprintf(log, len, T("Le das carne al %s: ¡ahora es de la tribu y te defenderá!"), who);
        tg_xp(ga, XP_TAME, log, len);
    } else if (what == 5) { // un animal de la tribu con sed: un trago del odre
        const char *w = water_at_hand(ga, p);
        if (w && animal_give_water(a)) {
            ig_use(ga, NULL, p, w, 1);
            snprintf(log, len, T("Das de beber al %s."), who);
        }
    } else if (what == 4) { // un animal de la tribu con hambre: carne o forraje
        bool meat = species_eats_meat(a->species);
        const char *food = meat ? (ig_count(ga, NULL, p, FRESH_MEAT_ID) > 0 ? FRESH_MEAT_ID : ig_count(ga, NULL, p, FOOD_ID) > 0 ? FOOD_ID : NULL)
                                : (ig_count(ga, NULL, p, FODDER_ID) > 0 ? FODDER_ID : NULL);
        if (!food) {
            snprintf(log, len, meat ? T("El %s tiene hambre: necesitas carne, o déjalo cazar.") : T("El %s tiene hambre: necesitas forraje (corta hierba alta con F), o llévalo a pastar."), who);
            return;
        }
        ig_use(ga, NULL, p, food, 1);
        animal_feed_tamed(a, meat);
        snprintf(log, len, T("Das de comer al %s."), who);
    } else if (what == 2) {
        butcher(ga, p, a, log, len, T("Despiezas"));
    } else if (shift) {
        if (animal_slaughter(a)) butcher(ga, p, a, log, len, T("Sacrificas"));
    } else {
        int milk = animal_milk(a);
        if (milk > 0) {
            ig_store(ga, NULL, p, MILK_ID, milk, 1.0f);
            snprintf(log, len, T("Ordeñas la %s: +%d leche."), who, milk);
        } else {
            snprintf(log, len, "%s", species_def(a->species)->milk > 0 ? T("Ya ordeñaste hoy a este animal.")
                                                                 : T("Este animal no da leche (Mayús+K: sacrificar)."));
        }
    }
}

static void update_hint(GameActions *ga, const Player *p) {
    int what = 0, i = nearest_interactable(ga, p, &what);
    g_hint[0] = '\0';
    if (hive_near(ga, p)) {
        snprintf(g_hint, sizeof(g_hint), "%s", ga->torch_lit ? T("K: tomar miel (el humo calma a las abejas)")
                                                       : T("Colmena: enciende la antorcha (humo) antes de tomar la miel"));
        return;
    }
    if (i < 0) return;
    char who[48];
    lower(T(species_def(ga->animals[i].species)->name), who, sizeof(who));
    if (what == 1) snprintf(g_hint, sizeof(g_hint), T("K: dar de comer al %s atado (%.0f s)"), who, ga->animals[i].bound_timer);
    else if (what == 2) snprintf(g_hint, sizeof(g_hint), T("K: despiezar (%s)"), who);
    else if (what == 5) snprintf(g_hint, sizeof(g_hint), T("K: dar de beber al %s (tiene sed)"), who);
    else if (what == 4)
        snprintf(g_hint, sizeof(g_hint), species_eats_meat(ga->animals[i].species) ? T("K: dar carne al %s (tiene hambre)") : T("K: dar forraje al %s (tiene hambre)"), who);
    else if (species_def(ga->animals[i].species)->milk > 0 && !ga->animals[i].milked)
        snprintf(g_hint, sizeof(g_hint), T("K: ordeñar · Mayús+K: sacrificar (%s)"), who);
    else snprintf(g_hint, sizeof(g_hint), T("Mayús+K: sacrificar (%s)"), who);
}

// ------------------------------------------------------------------ actualizacion
void fg_update(GameActions *ga, Combat *cb, Player *p, Troop *troop, const Terrain *t, MemoryMap *mem, float now,
               float temp, bool input_ok, float dt, char *log, size_t len) {
    bool night = clock_is_night(now);
    // Las personas que ven los animales.
    FaunaHuman hu[MAX_HUMANS];
    int n = 0;
    hu[n] = (FaunaHuman){ p->pos.x, p->pos.z, cb->player.down, p->sneaking || ga->hidden, false };
    g_refs[n++] = (HumanRef){ 0, 0 };
    for (int k = 0; k < troop->count && k < TROOP_MAX; k++) {
        const Member *m = &troop->members[k];
        const Npc *np = &ga->npcs[k];
        if (m->status != STATUS_ACTIVE || np->member_id != m->id || m->journey > 0) continue; // de viaje: fuera del mundo
        hu[n] = (FaunaHuman){ np->pos.x, np->pos.z, m->health.down, false, false };
        g_refs[n++] = (HumanRef){ 1, m->id };
    }
    for (int e = 0; e < CB_MAX_ENEMIES; e++) {
        const Enemy *en = &cb->enemies[e];
        if (!en->used || en->state == EN_DEAD) continue;
        hu[n] = (FaunaHuman){ en->pos.x, en->pos.z, false, false, true };
        g_refs[n++] = (HumanRef){ 2, e };
    }
    g_ref_count = n;
    FaunaCtx ctx = { hu, n, p->pos.x, p->pos.z, p->moving && ga->mounted < 0, 0.0f, 0.0f, night, water_depth_cb, (void *)t, ga->grass };

    float ox[GA_MAX_ANIMALS], oz[GA_MAX_ANIMALS];
    for (int i = 0; i < ga->animal_count; i++) ox[i] = ga->animals[i].x, oz[i] = ga->animals[i].z;
    FaunaEvents ev = { .n = 0 };
    fauna_update(ga->animals, ga->animal_count, &ctx, &ga->rng, dt, &ev);
    // Al agua no entran (salvo las aves): se quedan en la orilla.
    for (int i = 0; i < ga->animal_count; i++) {
        Animal *a = &ga->animals[i];
        const SpeciesDef *sd = species_def(a->species);
        if (!a->used || a->ridden || sd->flier || sd->aquatic || !deep_water(t, a->x, a->z)) continue;
        a->x = ox[i], a->z = oz[i];
        a->tx = a->home_x, a->tz = a->home_z;
        a->timer = 0.0f;
    }

    for (int e = 0; e < ev.n; e++) {
        const FaunaEvent *f = &ev.ev[e];
        Animal *a = &ga->animals[f->animal];
        char who[48], other[48];
        lower(T(species_def(a->species)->name), who, sizeof(who));
        switch (f->kind) {
        case FEV_BITE_HUMAN: {
            if (f->other < 0 || f->other >= g_ref_count) break;
            const HumanRef *r = &g_refs[f->other];
            Vector3 from = { a->x, terrain_height(t, a->x, a->z), a->z };
            cb_beast_strike(cb, p, ga, troop, r->kind, r->id, from, f->damage, f->wound, f->venom,
                            T(species_def(a->species)->name), log, len);
            break;
        }
        case FEV_KILL: {
            Animal *v = &ga->animals[f->other];
            lower(T(species_def(v->species)->name), other, sizeof(other));
            if (a->state == ANIMAL_TAMED || a->state == ANIMAL_SADDLED) {
                char msg[96];
                snprintf(msg, sizeof(msg), T("Tu %s caza: %s."), who, other);
                butcher(ga, p, v, log, len, msg);
            } else if (adist(a, p->pos) < 45.0f) {
                snprintf(log, len, T("Cerca de ti, un cazador (%s) abate a su presa (%s)."), who, other);
            }
            break;
        }
        case FEV_RIVAL_WON:
            if (adist(a, p->pos) < 45.0f) snprintf(log, len, T("Rivales (%s) pelean por el territorio: el perdedor se retira."), who);
            break;
        case FEV_BREAK_FREE:
            if (adist(a, p->pos) < 60.0f) snprintf(log, len, T("Se soltó del lazo sin comer: %s. ¡Cuidado!"), who);
            break;
        case FEV_HUNGRY:
            snprintf(log, len, species_eats_meat(a->species) ? T("El %s de la tribu tiene hambre: dale carne (K) o déjalo cazar.")
                                                           : T("El %s de la tribu tiene hambre: llévalo a pastar o dale forraje (K)."),
                     who);
            break;
        case FEV_STARVED: snprintf(log, len, T("El %s se fue: pasó demasiada hambre y vuelve a ser salvaje."), who); break;
        case FEV_THIRSTY: snprintf(log, len, T("El %s de la tribu tiene sed: llévalo al agua o dale de beber (K, con agua)."), who); break;
        case FEV_PARCHED: snprintf(log, len, T("El %s se fue: pasó demasiada sed y vuelve a ser salvaje."), who); break;
        default: break;
        }
    }

    // Los pastores alimentan a los animales de la tribu que esten en su campamento (con el acopio).
    static float herd_t = 0.0f;
    if ((herd_t += dt) > 15.0f) {
        herd_t = 0.0f;
        for (int k = 0; k < CAMPS_MAX; k++) {
            CampSite *c = &ga->camps[k];
            if (!c->used) continue;
            int herders = 0;
            for (int m = 0; m < troop->count; m++)
                herders += troop->members[m].status == STATUS_ACTIVE && troop->members[m].camp == k && troop->members[m].role == ROLE_HERDER;
            for (int i = 0; herders > 0 && i < ga->animal_count; i++) {
                Animal *a = &ga->animals[i];
                if (!animal_domestic(a) || a->hunger < 0.5f || adist(a, (Vector3){ c->x, 0, c->z }) > CAMP_RADIUS_M) continue;
                bool meat = species_eats_meat(a->species);
                const char *food = meat ? (stock_count(&c->stock, FRESH_MEAT_ID) > 0 ? FRESH_MEAT_ID : FOOD_ID) : FODDER_ID;
                if (stock_take(&c->stock, food, 1)) animal_feed_tamed(a, meat);
            }
            for (int i = 0; herders > 0 && i < ga->animal_count; i++) { // y les dan de beber
                Animal *a = &ga->animals[i];
                if (!animal_domestic(a) || a->thirst < 0.5f || adist(a, (Vector3){ c->x, 0, c->z }) > CAMP_RADIUS_M) continue;
                if (stock_take(&c->stock, WATER_ID, 1) || stock_take(&c->stock, BOILED_ID, 1)) animal_give_water(a);
            }
        }
    }
    // Cada dia los pastores esquilan las cabras de su campamento: lana para fieltro y abrigos.
    static int shear_day = -1;
    if (clock_day(now) != shear_day) {
        bool first = shear_day < 0;
        shear_day = clock_day(now);
        for (int k = 0; k < CAMPS_MAX && !first; k++) {
            CampSite *c = &ga->camps[k];
            bool herder = false;
            for (int m = 0; c->used && m < troop->count; m++)
                herder |= troop->members[m].status == STATUS_ACTIVE && troop->members[m].camp == k && troop->members[m].role == ROLE_HERDER;
            for (int i = 0; herder && i < ga->animal_count; i++) {
                const Animal *a = &ga->animals[i];
                if (alive(a) && a->species == SPECIES_GOAT && a->state == ANIMAL_TAMED && adist(a, (Vector3){ c->x, 0, c->z }) < CAMP_RADIUS_M)
                    stock_add(&c->stock, "utileria.piel.cabra", 1);
            }
        }
    }

    // La montura cayo.
    if (ga->mounted >= 0 && !alive(&ga->animals[ga->mounted])) {
        ga->mounted = -1;
        snprintf(log, len, "%s", T("¡Tu montura cayó! Sigues a pie."));
    }
    // El cuervo domado explora: revela el mapa por donde vuela.
    ga->reveal_timer += dt;
    if (ga->reveal_timer > 2.0f) {
        ga->reveal_timer = 0.0f;
        for (int i = 0; i < ga->animal_count; i++) {
            const Animal *a = &ga->animals[i];
            if (alive(a) && a->species == SPECIES_RAVEN && a->state == ANIMAL_TAMED) memmap_reveal(mem, a->x, a->z, 26.0f, 40.0f, now);
        }
    }
    // Cadaveres: despiezados se retiran enseguida; enteros, al rato.
    for (int i = 0; i < ga->animal_count; i++) {
        Animal *a = &ga->animals[i];
        if (a->used && a->state == ANIMAL_DEAD && ((a->butchered && a->corpse > 4.0f) || a->corpse > CORPSE_KEEP)) a->used = false;
    }
    populate(ga, p, t, night, dt);
    update_swarms(ga, cb, p, t, now, temp, dt, log, len);

    if (input_ok && ga->mounted < 0 && IsKeyPressed(KEY_K))
        interact(ga, p, IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT), log, len);
    update_hint(ga, p);
}

void fg_new_day(GameActions *ga) {
    for (int i = 0; i < GA_MAX_SWARMS; i++) ga->swarms[i].honey_taken = false;
    for (int i = 0; i < ga->animal_count; i++)
        if (alive(&ga->animals[i])) {
            animal_new_day(&ga->animals[i]);
            if (ga->animals[i].state != ANIMAL_WILD) health_daily(&ga->animals[i].h, 0.0f); // los de la tribu sanan
        }
}

// ------------------------------------------------------------------ dibujo
static Color animal_color(const Animal *a) {
    static const Color by_class[] = {
        [CLASS_MOUNT] = { 128, 92, 60, 255 },   [CLASS_TAMEABLE] = { 118, 110, 100, 255 },
        [CLASS_HOSTILE] = { 84, 62, 46, 255 },  [CLASS_PREY] = { 176, 140, 96, 255 },
        [CLASS_LIVESTOCK] = { 214, 204, 184, 255 },
    };
    Color c = by_class[species_def(a->species)->cls];
    if (a->species == SPECIES_TIGER) c = (Color){ 214, 128, 44, 255 };
    if (a->species == SPECIES_RAVEN) c = (Color){ 30, 30, 36, 255 };
    if (a->species == SPECIES_ELEPHANT) c = (Color){ 120, 118, 116, 255 };
    if (a->hit_anim > 0.0f) c = (Color){ 240, 230, 220, 255 };
    if (a->state == ANIMAL_DEAD) c = ColorBrightness(c, a->butchered ? -0.6f : -0.3f);
    return c;
}

static void draw_swarm(const Swarm *s, Props *props, const InvItem *nest, const Terrain *t, float time);

void fg_draw_world(const GameActions *ga, Props *props, const Terrain *t, float time) {
    for (int i = 0; i < GA_MAX_SWARMS; i++) {
        const Swarm *s = &ga->swarms[i];
        if (!s->used) continue;
        const char *nest = swarm_def(s->kind)->nest;
        draw_swarm(s, props, nest ? inventory_find(ga->inv, nest) : NULL, t, time);
    }
    for (int i = 0; i < ga->animal_count; i++) {
        const Animal *a = &ga->animals[i];
        if (!a->used) continue;
        const SpeciesDef *d = species_def(a->species);
        const InvItem *it = inventory_find(ga->inv, d->model);
        Vector3 pos = { a->x, terrain_height(t, a->x, a->z), a->z };
        // En el agua, los anfibios flotan: solo asoma el lomo (el cocodrilo, los ojos).
        float depth = water_depth_cb((void *)t, a->x, a->z);
        if (d->aquatic && depth > 0.3f && a->state != ANIMAL_DEAD)
            pos.y = terrain_water(t, a->x, a->z) - (a->species == SPECIES_CROCODILE && a->mode != MODE_CHASE ? 0.42f : 0.25f);
        float top = it ? it->h : d->size * 0.6f;
        if (it && props_has_model(props, it)) {
            // El modelo mira a +X (Kiln); el animal avanza segun yaw (0 = +Z).
            float tt = a->state == ANIMAL_DEAD ? fminf(a->corpse, 1.2f) : time + (float)i * 0.37f;
            Vector3 at = { pos.x, pos.y + a->alt, pos.z };
            props_draw_item_anim(props, it, at, a->yaw - PI / 2.0f, anim_animal(a), tt);
        } else {
            // Sin modelo: el mismo cuerpo de capsulas que reciben los impactos.
            BodyPose b;
            animal_body(a, &b);
            Color c = animal_color(a);
            for (int s = 0; s < PART_COUNT; s++) {
                const BodySeg *sg = &b.seg[s];
                if (sg->radius <= 0.0f) continue;
                V3 wa = body_to_world(sg->a, (V3){ pos.x, pos.y, pos.z }, a->yaw);
                V3 wb = body_to_world(sg->b, (V3){ pos.x, pos.y, pos.z }, a->yaw);
                Color sc = s == PART_HEAD ? ColorBrightness(c, -0.15f) : c;
                DrawCapsule((Vector3){ wa.x, wa.y, wa.z }, (Vector3){ wb.x, wb.y, wb.z }, sg->radius, 5, 2, sc);
            }
            if (a->attack_anim > 0.0f && a->state != ANIMAL_DEAD) { // el golpe: un destello hacia delante
                Vector3 f = { sinf(a->yaw), 0, cosf(a->yaw) };
                float y = pos.y + a->alt + d->size * 0.3f;
                DrawLine3D((Vector3){ pos.x, y, pos.z }, (Vector3){ pos.x + f.x * d->reach, y, pos.z + f.z * d->reach },
                           (Color){ 240, 230, 200, 255 });
            }
        }
        if (a->state == ANIMAL_DEAD) continue;
        float y = pos.y + a->alt + top;
        if (a->state != ANIMAL_WILD) DrawCube((Vector3){ pos.x, y + 0.1f, pos.z }, 0.12f, 0.12f, 0.12f, UI_TURQUOISE);
        if (a->state == ANIMAL_SADDLED)
            DrawCube((Vector3){ pos.x, pos.y + top * 0.75f, pos.z }, 0.5f, 0.15f, 0.6f, (Color){ 120, 70, 40, 255 });
        if (a->state == ANIMAL_BOUND) { // la cuerda del lazo, a una estaca
            Vector3 stake = { pos.x + 1.2f, pos.y, pos.z + 1.2f };
            DrawCylinderEx(stake, (Vector3){ stake.x, stake.y + 0.5f, stake.z }, 0.04f, 0.04f, 4, (Color){ 110, 80, 50, 255 });
            DrawLine3D((Vector3){ stake.x, stake.y + 0.45f, stake.z }, (Vector3){ pos.x, pos.y + top * 0.7f, pos.z },
                       (Color){ 200, 170, 110, 255 });
        }
        if (a->state == ANIMAL_WILD && a->weakened && d->cls == CLASS_TAMEABLE) // debil: se puede atar
            DrawCube((Vector3){ pos.x, y + 0.15f, pos.z }, 0.1f, 0.1f, 0.1f, UI_GOLD);
    }
}

static void draw_swarm(const Swarm *s, Props *props, const InvItem *nest, const Terrain *t, float time) {
    if (s->kind == SWARM_FISH && hazard_ice_walkable(t->look.ice)) return;
    if (nest) { // colmena o avispero, colgando de un poste
        Vector3 at = { s->hx, s->hy, s->hz };
        if (props_has_model(props, nest)) {
            props_draw_item(props, nest, at, 0.0f, 1.0f);
        } else {
            DrawCylinderEx(at, (Vector3){ at.x, at.y + 1.2f, at.z }, 0.05f, 0.05f, 4, (Color){ 96, 72, 48, 255 });
            Color c = s->kind == SWARM_BEES ? (Color){ 196, 160, 92, 255 } : (Color){ 150, 140, 120, 255 };
            DrawSphere((Vector3){ at.x, at.y + 1.35f, at.z }, 0.22f, c);
        }
    }
    if (!s->active && s->kind != SWARM_FISH) return;
    Color c;
    float sz;
    switch (s->kind) {
    case SWARM_FISH: c = (Color){ 34, 44, 52, 255 }, sz = 0.0f; break; // lomos oscuros
    case SWARM_BEES: c = s->anger > 0.0f ? (Color){ 250, 200, 40, 255 } : (Color){ 220, 180, 60, 255 }, sz = 0.07f; break;
    case SWARM_WASPS: c = (Color){ 230, 150, 30, 255 }, sz = 0.07f; break;
    case SWARM_MOSQUITOES: c = (Color){ 40, 36, 32, 255 }, sz = 0.05f; break;
    default: c = (Color){ 20, 20, 24, 255 }, sz = 0.05f; break;
    }
    for (int i = 0; i < s->n; i++) {
        const SwarmBody *b = &s->b[i];
        if (!b->alive) continue;
        if (s->kind == SWARM_FISH) { // un pez: alargado en la direccion en que nada
            float v = sqrtf(b->vx * b->vx + b->vz * b->vz);
            float dx = v > 0.01f ? b->vx / v : 1.0f, dz = v > 0.01f ? b->vz / v : 0.0f;
            float wig = sinf(time * 9.0f + (float)i) * 0.04f;
            DrawCapsule((Vector3){ b->x - dx * 0.15f + dz * wig, b->y, b->z - dz * 0.15f - dx * wig },
                        (Vector3){ b->x + dx * 0.15f, b->y, b->z + dz * 0.15f }, 0.07f, 4, 2, c);
        } else {
            DrawCube((Vector3){ b->x, b->y, b->z }, sz, sz, sz, c);
        }
    }
}

void fg_draw_overlay(const GameActions *ga, const Terrain *t, Camera3D cam, int w, int h) {
    for (int i = 0; i < ga->animal_count; i++) { // los de la tribu con hambre: un cuenco sobre la cabeza
        const Animal *a = &ga->animals[i];
        bool hungry = a->hunger >= 0.8f, thirsty = a->thirst >= 0.8f;
        if (!animal_domestic(a) || (!hungry && !thirsty) || a->ridden) continue;
        Vector3 pos = { a->x, terrain_height(t, a->x, a->z) + a->alt + species_def(a->species)->size * 0.55f + 0.9f, a->z };
        Vector3 to = Vector3Subtract(pos, cam.position);
        if (Vector3DotProduct(to, Vector3Subtract(cam.target, cam.position)) <= 0.0f || Vector3Length(to) > 35.0f) continue;
        Vector2 s = GetWorldToScreenEx(pos, cam, w, h);
        float x0 = hungry && thirsty ? s.x - 17 : s.x - 8;
        if (hungry) ui_icon(ICON_ALIMENTAR, x0, s.y - 8, 16, a->hunger > 1.0f ? UI_CARNELIAN : UI_GOLD);
        if (thirsty) ui_icon(ICON_AGUA, hungry ? x0 + 18 : x0, s.y - 8, 16, a->thirst > 1.0f ? UI_CARNELIAN : UI_TURQ_LIGHT);
    }
    for (int i = 0; i < ga->animal_count; i++) {
        const Animal *a = &ga->animals[i];
        if (!alive(a) || a->h.hp >= a->h.hp_max * 0.98f) continue;
        Vector3 pos = { a->x, terrain_height(t, a->x, a->z) + a->alt + species_def(a->species)->size * 0.55f + 0.3f, a->z };
        Vector3 to = Vector3Subtract(pos, cam.position);
        if (Vector3DotProduct(to, Vector3Subtract(cam.target, cam.position)) <= 0.0f || Vector3Length(to) > 30.0f) continue;
        Vector2 s = GetWorldToScreenEx(pos, cam, w, h);
        float frac = Clamp(a->h.hp / a->h.hp_max, 0.0f, 1.0f);
        DrawRectangle((int)s.x - 12, (int)s.y - 2, 24, 4, UI_LEATHER_CRACK);
        DrawRectangle((int)s.x - 11, (int)s.y - 1, (int)(22 * frac), 2, a->state == ANIMAL_WILD ? UI_GOLD : UI_TURQUOISE);
    }
}

void fg_draw_hud(const GameActions *ga, int w, int h) {
    (void)ga;
    if (g_hint[0]) ui_text_centered(g_hint, w / 2, h - 84, 10, UI_TURQUOISE);
}

bool fg_cut_grass(GameActions *ga, Props *props, const Player *p, char *log, size_t len) {
    int best = -1;
    float bd = 2.2f;
    for (int i = 0; i < props->count; i++) {
        if (strcmp(props->items[i].item->id, "mapa.vegetacion.hierba_alta") != 0) continue;
        float d = Vector2Distance((Vector2){ props->items[i].pos.x, props->items[i].pos.z }, (Vector2){ p->pos.x, p->pos.z });
        if (d < bd) bd = d, best = i;
    }
    if (best < 0 || ga->grass == false) return false; // con nieve no hay hierba que cortar
    int kept = ig_store(ga, props, p, FODDER_ID, 2, 1.0f);
    if (!kept) {
        snprintf(log, len, "%s", T("No te cabe más forraje."));
        return true;
    }
    props_remove(props, best);
    snprintf(log, len, T("Cortas hierba alta: +%d forraje para los animales."), kept);
    return true;
}
