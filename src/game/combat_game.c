#include "combat_game.h"

#include "game/fauna_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"
#include "sim/anim_index.h"
#include "sim/body.h"
#include "sim/hazards.h"
#include "ui/theme.h"
#include "world/body_draw.h"

#define HERBS_ID "utileria.consumible.hierbas"
#define CAMP_RADIUS 14.0f
#define DOWN_WAKE_SECONDS 25.0f // abatido y solo: despierta en el campamento
#define DOWN_HELP_SECONDS 5.0f  // con la escolta cerca: lo levantan
#define CORPSE_SECONDS 30.0f
#define DESPAWN_DIST 160.0f
#define COMPANION_DAMAGE 10.0f
#define COMPANION_COOLDOWN 1.2f
#define SHOT_LIFE 8.0f        // segundos de vuelo como maximo
#define STUCK_SECONDS 20.0f   // una flecha clavada se ve un rato
#define EYE_HEIGHT 1.45f      // de donde sale el disparo

static float dist2(Vector3 a, Vector3 b) { return Vector2Distance((Vector2){ a.x, a.z }, (Vector2){ b.x, b.z }); }
static V3 to_v3(Vector3 v) { return (V3){ v.x, v.y, v.z }; }
static Vector3 to_vec(V3 v) { return (Vector3){ v.x, v.y, v.z }; }

void cb_init(Combat *cb, unsigned seed) {
    memset(cb, 0, sizeof(*cb));
    cb->seed = seed;
    rng_seed(&cb->rng, seed ^ 0xC0B7u);
    health_init(&cb->player, 100.0f);
    // Equipo inicial: armadura laminar de cuero de la estepa.
    armor_equip(&cb->armor, "armadura.casco.laminar_cuero");
    armor_equip(&cb->armor, "armadura.torso.laminar_cuero");
    armor_equip(&cb->armor, "armadura.brazales.laminar_cuero");
    armor_equip(&cb->armor, "armadura.botas.fieltro");
}

bool cb_blocks_input(const Combat *cb) { return cb->player.down; }

float cb_speed_scale(const Combat *cb) {
    float s = health_speed_scale(&cb->player) * armor_speed_scale(&cb->armor);
    if (cb->blocking || cb->aiming) s *= 0.5f;
    return cb->player.down ? 1.0f : s;
}

static float surface_y(const Terrain *t, float x, float z) {
    float ground = terrain_height(t, x, z);
    if (t->look.water_level > ground + 0.25f)
        return hazard_ice_walkable(t->look.ice) ? t->look.water_level : fmaxf(ground, t->look.water_level - 1.25f);
    return ground;
}

static void lower_first(char *s) {
    if (s[0] >= 'A' && s[0] <= 'Z') s[0] = (char)(s[0] - 'A' + 'a');
}

static const char *lower_name(const char *name, char *buf, size_t len) {
    snprintf(buf, len, "%s", name);
    lower_first(buf);
    return buf;
}

// ------------------------------------------------------------------- aparicion
static Enemy *spawn_enemy(Combat *cb, EnemyKind kind, Vector3 pos, const Terrain *t) {
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        Enemy *e = &cb->enemies[i];
        if (e->used) continue;
        memset(e, 0, sizeof(*e));
        const EnemyDef *def = enemy_def(kind);
        e->used = true;
        e->kind = kind;
        e->pos = (Vector3){ pos.x, surface_y(t, pos.x, pos.z), pos.z };
        e->home = e->wander_to = e->pos;
        e->target = -1;
        if (def->beast) health_init_beast(&e->h, def->hp);
        else health_init(&e->h, def->hp);
        // Su armadura: la primera pieza siempre, las demas a veces.
        for (int k = 0; k < 4; k++)
            if (def->armor[k] && (k == 0 || rng_float(&cb->rng) < 0.6f)) armor_equip(&e->armor, def->armor[k]);
        return e;
    }
    return NULL;
}

void cb_spawn_group(Combat *cb, GameActions *ga, const char *what, const Player *p, const Terrain *t, float dist, char *log,
                    size_t len) {
    // Fieras: las pone la fauna (src/game/fauna_game.c).
    if (strcmp(what, "bandidos") && strcmp(what, "culto") && strcmp(what, "arqueros") &&
        fg_spawn_named(ga, what, p, dist, log, len))
        return;
    float ang = rng_float(&cb->rng) * 2.0f * PI;
    if (dist < 25.0f) ang = p->yaw + (rng_float(&cb->rng) - 0.5f) * 0.8f; // pruebas: delante del jugador
    Vector3 c = { p->pos.x + sinf(ang) * dist, 0, p->pos.z + cosf(ang) * dist };
    EnemyKind kinds[5];
    int n = 0;
    if (!strcmp(what, "culto")) {
        n = 3;
        kinds[0] = kinds[1] = ENEMY_FANATIC;
        kinds[2] = ENEMY_CAPTOR;
        snprintf(log, len, "¡Fanáticos del culto! Buscan prisioneros para el sacrificio.");
    } else if (!strcmp(what, "arqueros")) {
        n = 2;
        kinds[0] = kinds[1] = ENEMY_ARCHER;
        snprintf(log, len, "¡Arqueros! Busca cubrirte o acércate rápido.");
    } else {
        n = 2 + rng_range(&cb->rng, 2);
        for (int i = 0; i < n; i++) kinds[i] = ENEMY_BANDIT;
        if (rng_float(&cb->rng) < 0.6f) kinds[n++] = ENEMY_ARCHER; // a veces con un arquero detras
        snprintf(log, len, "¡Bandidos! Vienen a por tu botín.");
    }
    for (int i = 0; i < n; i++) {
        float a = (float)i * 2.1f;
        float back = kinds[i] == ENEMY_ARCHER ? 8.0f : 0.0f; // los arqueros se quedan atras
        Vector3 at = { c.x + cosf(a) * 2.5f + sinf(ang) * back, 0, c.z + sinf(a) * 2.5f + cosf(ang) * back };
        Enemy *e = spawn_enemy(cb, kinds[i], at, t);
        if (e) e->yaw = ang + PI;
    }
}

// Lejos del campamento aparecen bandidos o el culto de dia (las fieras, src/game/fauna_game.c).
static void natural_spawns(Combat *cb, GameActions *ga, const Player *p, const Terrain *t, Vector3 camp_fire, bool night,
                           float dt, char *log, size_t len) {
    cb->spawn_timer += dt;
    if (cb->spawn_timer < 60.0f) return;
    cb->spawn_timer = 0.0f;
    if (dist2(p->pos, camp_fire) < 90.0f) return;
    int alive = 0;
    for (int i = 0; i < CB_MAX_ENEMIES; i++) alive += cb->enemies[i].used && cb->enemies[i].state != EN_DEAD;
    if (alive > 6) return;
    float r = rng_float(&cb->rng);
    if (!night && r < 0.12f) cb_spawn_group(cb, ga, rng_float(&cb->rng) < 0.3f ? "culto" : "bandidos", p, t, 35.0f, log, len);
}

// ------------------------------------------------------------------- golpes
static void describe_hit(const Health *h, int w, float absorbed, bool broke, char *out, size_t len) {
    if (w < 0) {
        snprintf(out, len, "la armadura para el golpe%s", broke ? " y se rompe" : "");
        return;
    }
    char d[96];
    wound_describe(&h->wounds[w], h->beast, d, sizeof(d));
    lower_first(d);
    snprintf(out, len, "%s%s%s", d, absorbed > 0.5f ? " (la armadura amortigua)" : "", broke ? "; se rompe la pieza" : "");
}

static void enemy_down_check(Enemy *e) {
    if (e->h.down || e->h.dead) {
        e->state = EN_DEAD;
        e->corpse = CORPSE_SECONDS;
    }
}

static void player_attack(Combat *cb, Player *p, GameActions *ga, const Terrain *t, char *log, size_t len) {
    const char *weapon = ga->hands.sheathed ? "" : ga->hands.right.id;
    WeaponStats w = weapon_stats(weapon);
    cb->attack_cd = w.cooldown;
    cb->attack_anim = 0.4f;
    cb->combo = cb->combo % 3 + 1;
    ga->pl_spear = w.spear;
    // El enemigo mas cercano delante, al alcance del arma.
    Vector3 fwd = { sinf(p->yaw), 0, cosf(p->yaw) };
    Enemy *best = NULL;
    float best_d = w.reach + 0.6f;
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        Enemy *e = &cb->enemies[i];
        if (!e->used || e->state == EN_DEAD) continue;
        float d = dist2(p->pos, e->pos);
        Vector3 to = Vector3Normalize((Vector3){ e->pos.x - p->pos.x, 0, e->pos.z - p->pos.z });
        if (d < best_d && (d < 0.8f || Vector3DotProduct(fwd, to) > 0.3f)) best_d = d, best = e;
    }
    // Un animal mas cerca que el enemigo: el golpe es para el.
    float ad;
    int an = fg_melee_target(ga, t, p->pos, p->yaw, w.reach, &ad);
    if (an >= 0 && ad < best_d) {
        float dmg = combat_damage(w.damage, 1.0f, health_attack_scale(&cb->player), false, &cb->rng);
        fg_hurt(ga, an, dmg, w.wound, PART_RANDOM, 0, log, len);
        return;
    }
    if (!best) return;
    const EnemyDef *def = enemy_def(best->kind);
    float dmg = combat_damage(w.damage, 1.0f, health_attack_scale(&cb->player), false, &cb->rng);
    float absorbed = 0.0f;
    bool broke = false;
    int wi = combat_apply_hit(&best->h, &best->armor, &cb->rng, dmg, w.wound, PART_RANDOM, false, &absorbed, &broke);
    best->hit_anim = 0.3f;
    best->shown = 6.0f;
    if (best->target < 0) best->target = 0; // ahora sabe donde estas
    enemy_down_check(best);
    char d[128], who[48];
    if (best->state == EN_DEAD) snprintf(log, len, "Abatiste al %s.", lower_name(def->name, who, sizeof(who)));
    else describe_hit(&best->h, wi, absorbed, broke, d, sizeof(d)), snprintf(log, len, "Golpeas: %s.", d);
}

static bool player_blocks(Combat *cb, const Player *p, const GameActions *ga, Vector3 from) {
    Vector3 fwd = { sinf(p->yaw), 0, cosf(p->yaw) };
    Vector3 to = Vector3Normalize((Vector3){ from.x - p->pos.x, 0, from.z - p->pos.z });
    bool shield = ga->hands.left.kind == INV_HANDS_SHIELD && ga->hands.left.id[0] && !ga->hands.sheathed;
    float facing = Vector3DotProduct(fwd, to);
    float chance = cb->blocking ? (shield ? combat_block_chance(true, facing) : (facing > 0.35f ? 0.25f : 0.0f)) : 0.0f;
    return rng_float(&cb->rng) < chance;
}

// Un enemigo golpea al jugador (target 0) o a un integrante.
static void enemy_strike(Combat *cb, Enemy *e, Player *p, GameActions *ga, Troop *troop, char *log, size_t len) {
    const EnemyDef *def = enemy_def(e->kind);
    e->cooldown = def->cooldown;
    e->attack_anim = 0.45f;
    float arms = health_attack_scale(&e->h);
    float absorbed = 0.0f;
    bool broke = false;
    if (e->target == 0) {
        bool blocked = player_blocks(cb, p, ga, e->pos);
        float dmg = combat_damage(def->damage, 1.0f, arms, blocked, &cb->rng);
        int wi = combat_apply_hit(&cb->player, &cb->armor, &cb->rng, dmg, def->wound, PART_RANDOM, false, &absorbed, &broke);
        cb->hit_anim = 0.3f;
        char d[128], who[48];
        if (blocked) snprintf(log, len, "Paras el golpe del %s.", lower_name(def->name, who, sizeof(who)));
        else describe_hit(&cb->player, wi, absorbed, broke, d, sizeof(d)), snprintf(log, len, "Te hieren: %s.", d);
        return;
    }
    Member *m = troop_find(troop, e->target);
    if (!m || m->status != STATUS_ACTIVE) return;
    float dmg = combat_damage(def->damage, 1.0f, arms, false, &cb->rng);
    combat_apply_hit(&m->health, &m->armor, &cb->rng, dmg, def->wound, PART_RANDOM, false, NULL, NULL);
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
        if (ga->npcs[i].member_id == m->id) ga->npcs[i].hurt_anim = 0.3f;
    if (m->health.down) snprintf(log, len, "¡%s cae herido! Acércate y pulsa B para levantarlo.", m->name);
}

void cb_beast_strike(Combat *cb, Player *p, GameActions *ga, Troop *troop, int kind, int id, Vector3 from, float dmg,
                     WoundKind wound, const char *who, char *log, size_t len) {
    char name[48];
    lower_name(who, name, sizeof(name));
    if (kind == 0) { // el jugador: el escudo y la armadura cuentan
        if (cb->player.down) return;
        bool blocked = player_blocks(cb, p, ga, from);
        if (blocked) dmg *= 0.15f;
        float absorbed = 0.0f;
        bool broke = false;
        int wi = combat_apply_hit(&cb->player, &cb->armor, &cb->rng, dmg, wound, PART_RANDOM, false, &absorbed, &broke);
        cb->hit_anim = 0.3f;
        char d[128];
        if (blocked) snprintf(log, len, "Paras el ataque del %s con el escudo.", name);
        else describe_hit(&cb->player, wi, absorbed, broke, d, sizeof(d)), snprintf(log, len, "Te ataca un %s: %s.", name, d);
        return;
    }
    if (kind == 1) {
        Member *m = troop_find(troop, id);
        if (!m || m->status != STATUS_ACTIVE || m->health.down) return;
        combat_apply_hit(&m->health, &m->armor, &cb->rng, dmg, wound, PART_RANDOM, false, NULL, NULL);
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
            if (ga->npcs[i].member_id == m->id) ga->npcs[i].hurt_anim = 0.3f;
        if (m->health.down) snprintf(log, len, "¡Un %s derriba a %s! Acércate y pulsa B.", name, m->name);
        return;
    }
    if (id < 0 || id >= CB_MAX_ENEMIES || !cb->enemies[id].used || cb->enemies[id].state == EN_DEAD) return;
    Enemy *e = &cb->enemies[id];
    combat_apply_hit(&e->h, &e->armor, &cb->rng, dmg, wound, PART_RANDOM, false, NULL, NULL);
    e->hit_anim = 0.3f;
    e->shown = 6.0f;
    enemy_down_check(e);
    if (e->state == EN_DEAD && dist2(e->pos, p->pos) < 40.0f) {
        char en[48];
        snprintf(log, len, "Un %s abate a un %s.", name, lower_name(enemy_def(e->kind)->name, en, sizeof(en)));
    }
}

// ------------------------------------------------------------------- disparos
static Shot *new_shot(Combat *cb) {
    for (int i = 0; i < CB_MAX_SHOTS; i++)
        if (!cb->shots[i].p.alive) return &cb->shots[i];
    return NULL;
}

static void fire(Combat *cb, const RangedDef *rd, Vector3 from, float yaw, float pitch, float charge, int owner) {
    Shot *s = new_shot(cb);
    if (!s) return;
    float spread = rd->spread * (1.5f - 0.5f * charge); // poco tenso, menos preciso
    yaw += (rng_float(&cb->rng) - 0.5f) * 2.0f * spread;
    pitch += (rng_float(&cb->rng) - 0.5f) * 2.0f * spread;
    memset(s, 0, sizeof(*s));
    projectile_launch(&s->p, rd->projectile, to_v3(from), yaw, pitch, ranged_muzzle_speed(rd, charge));
    s->owner = owner;
}

static Vector3 aim_origin(Vector3 pos, float yaw) {
    return (Vector3){ pos.x + sinf(yaw) * 0.35f, pos.y + EYE_HEIGHT, pos.z + cosf(yaw) * 0.35f };
}

static float aim_pitch_from_camera(float cam_pitch) { return Clamp((0.38f - cam_pitch) * 1.6f + 0.05f, -0.3f, 0.9f); }

static int ranged_anim(const RangedDef *rd) {
    if (!rd) return 0;
    if (rd->projectile == PROJ_BOLT) return 2;
    if (rd->projectile == PROJ_BALL) return 3;
    return 1;
}

// Poses para saber donde impacta un proyectil (las mismas que se dibujan).
static void pose_player(BodyPose *b, const Combat *cb, const Player *p, float time) {
    BodyPoseParams pp = { .walk_phase = time * 9.0f, .walk = p->moving ? 1.0f : 0.0f, .attack = cb->attack_anim / 0.4f,
                          .aiming = cb->aiming, .down = cb->player.down, .scale = 1.0f };
    body_pose(b, &pp);
}

static void pose_enemy(BodyPose *b, const Enemy *e, float time, int i) {
    BodyPoseParams pp = { .walk_phase = time * 9.0f + (float)i, .walk = e->speed > 0.2f ? 1.0f : 0.0f,
                          .attack = e->attack_anim / 0.45f, .aiming = enemy_def(e->kind)->ranged && e->target >= 0 && e->speed < 0.2f,
                          .down = e->state == EN_DEAD, .scale = 1.0f };
    body_pose(b, &pp);
}

static void pose_member(BodyPose *b, const Npc *n, const Member *m, const Troop *troop, float time, int i) {
    const Champion *c = m->champion >= 0 ? &troop->champions[m->champion] : NULL;
    BodyPoseParams pp = { .walk_phase = time * 9.0f + (float)i, .walk = n->moving ? 1.0f : 0.0f,
                          .attack = n->fight_anim / 0.4f, .down = m->health.down, .scale = c ? c->stats.size : 1.0f };
    body_pose(b, &pp);
}

static void shot_hits_player(Combat *cb, Shot *s, int part, Player *p, const GameActions *ga, char *log, size_t len) {
    const ProjectileDef *pd = projectile_def(s->p.kind);
    float dmg = projectile_damage(&s->p);
    if (player_blocks(cb, p, ga, to_vec(s->p.pos))) dmg *= 0.1f; // el escudo la frena
    float absorbed = 0.0f;
    bool broke = false;
    int wi = combat_apply_hit(&cb->player, &cb->armor, &cb->rng, dmg, pd->wound, part, true, &absorbed, &broke);
    cb->hit_anim = 0.3f;
    char d[128], what[32];
    describe_hit(&cb->player, wi, absorbed, broke, d, sizeof(d));
    snprintf(log, len, "¡Una %s te alcanza! %s.", lower_name(pd->name, what, sizeof(what)), d);
}

static void update_shots(Combat *cb, Player *p, GameActions *ga, Troop *troop, const Terrain *t, float time, float dt,
                         char *log, size_t len) {
    for (int si = 0; si < CB_MAX_SHOTS; si++) {
        Shot *s = &cb->shots[si];
        if (!s->p.alive) continue;
        s->life += dt;
        if (s->stuck) {
            if (s->life > STUCK_SECONDS) s->p.alive = false;
            continue;
        }
        V3 a = s->p.pos;
        projectile_step(&s->p, dt);
        V3 d = { s->p.pos.x - a.x, s->p.pos.y - a.y, s->p.pos.z - a.z };
        Vector3 mid = { (a.x + s->p.pos.x) * 0.5f, (a.y + s->p.pos.y) * 0.5f, (a.z + s->p.pos.z) * 0.5f };
        float reach = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z) * 0.5f + 2.5f;
        float best_t = 2.0f;
        int kind = -1, idx = -1, part = -1; // kind: 0 jugador, 1 integrante, 2 enemigo, 3 animal
        // Jugador.
        if (s->owner != 0 && dist2(mid, p->pos) < reach) {
            BodyPose b;
            pose_player(&b, cb, p, time);
            float tt;
            int hp = body_raycast(&b, body_to_local(a, to_v3(p->pos), p->yaw), body_dir_to_local(d, p->yaw), 1.0f, &tt);
            if (hp >= 0 && tt < best_t) best_t = tt, kind = 0, part = hp;
        }
        // Integrantes de la tribu (tambien pueden recibir flechas propias).
        for (int k = 0; k < troop->count && k < TROOP_MAX; k++) {
            const Npc *n = &ga->npcs[k];
            const Member *m = &troop->members[k];
            if (m->status != STATUS_ACTIVE || n->member_id != m->id || s->owner == m->id || dist2(mid, n->pos) > reach) continue;
            BodyPose b;
            pose_member(&b, n, m, troop, time, k);
            float tt;
            int hp = body_raycast(&b, body_to_local(a, to_v3(n->pos), n->yaw), body_dir_to_local(d, n->yaw), 1.0f, &tt);
            if (hp >= 0 && tt < best_t) best_t = tt, kind = 1, idx = k, part = hp;
        }
        // Enemigos (personas y fieras).
        for (int i = 0; i < CB_MAX_ENEMIES; i++) {
            const Enemy *e = &cb->enemies[i];
            if (!e->used || e->state == EN_DEAD || s->owner == -(1 + i) || dist2(mid, e->pos) > reach) continue;
            float tt;
            BodyPose b;
            pose_enemy(&b, e, time, i);
            int hp = body_raycast(&b, body_to_local(a, to_v3(e->pos), e->yaw), body_dir_to_local(d, e->yaw), 1.0f, &tt);
            if (hp >= 0 && tt < best_t) best_t = tt, kind = 2, idx = i, part = hp;
        }
        // Animales.
        {
            float tt;
            int ap = -1, ai = fg_raycast(ga, t, a, d, &tt, &ap);
            if (ai >= 0 && tt < best_t) best_t = tt, kind = 3, idx = ai, part = ap;
        }
        if (kind >= 0) {
            // Retrocede hasta el punto del impacto para el daño (la energia de ese instante).
            s->p.alive = false;
            const ProjectileDef *pd = projectile_def(s->p.kind);
            if (kind == 0) {
                shot_hits_player(cb, s, part, p, ga, log, len);
            } else if (kind == 1) {
                Member *m = &troop->members[idx];
                combat_apply_hit(&m->health, &m->armor, &cb->rng, projectile_damage(&s->p), pd->wound, part, true, NULL, NULL);
                ga->npcs[idx].hurt_anim = 0.3f;
                char what[32];
                snprintf(log, len, "Una %s alcanza a %s en %s.", lower_name(pd->name, what, sizeof(what)), m->name,
                         part_name((BodyPart)part, false));
            } else if (kind == 3) {
                int owner = s->owner > 0 ? s->owner : s->owner == 0 ? 0 : -1;
                fg_hurt(ga, idx, projectile_damage(&s->p), pd->wound, part, owner, s->owner == 0 ? log : NULL, len);
            } else {
                Enemy *e = &cb->enemies[idx];
                float absorbed = 0.0f;
                bool broke = false;
                int wi = combat_apply_hit(&e->h, &e->armor, &cb->rng, projectile_damage(&s->p), pd->wound, part, true,
                                          &absorbed, &broke);
                e->hit_anim = 0.3f;
                e->shown = 6.0f;
                if (s->owner == 0 && e->target < 0) e->target = 0;
                enemy_down_check(e);
                if (s->owner == 0) {
                    char dsc[128], who[48];
                    lower_name(enemy_def(e->kind)->name, who, sizeof(who));
                    if (e->state == EN_DEAD) snprintf(log, len, "¡Diana! Abatiste al %s (%s).", who, part_name((BodyPart)part, e->h.beast));
                    else describe_hit(&e->h, wi, absorbed, broke, dsc, sizeof(dsc)), snprintf(log, len, "Le das al %s: %s.", who, dsc);
                }
            }
            continue;
        }
        // Suelo: se clava (o se hunde en el agua).
        float ground = surface_y(t, s->p.pos.x, s->p.pos.z);
        if (s->p.pos.y <= ground) {
            float l = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
            s->dir = l > 1e-4f ? (V3){ d.x / l, d.y / l, d.z / l } : (V3){ 0, -1, 0 };
            s->p.pos.y = ground;
            s->stuck = true;
            s->life = 0.0f;
            if (t->look.water_level > terrain_height(t, s->p.pos.x, s->p.pos.z) + 0.25f && !hazard_ice_walkable(t->look.ice))
                s->p.alive = false; // al agua
        } else if (s->life > SHOT_LIFE) {
            s->p.alive = false;
        }
    }
}

static void player_ranged(Combat *cb, Player *p, GameActions *ga, const RangedDef *rd, bool can_act, float cam_yaw,
                          float cam_pitch, float dt, char *log, size_t len) {
    const char *ammo = projectile_def(rd->projectile)->ammo;
    bool held = IsKeyDown(KEY_V) || IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    bool released = IsKeyReleased(KEY_V) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    bool pressed = IsKeyPressed(KEY_V) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    cb->aim_pitch = aim_pitch_from_camera(cam_pitch);
    if (!can_act) {
        cb->aiming = false;
        cb->draw = 0.0f;
        return;
    }
    if (pressed && stock_count(&ga->stock, ammo) <= 0) {
        char what[32];
        snprintf(log, len, "No te quedan %ss en el acopio.", lower_name(projectile_def(rd->projectile)->name, what, sizeof(what)));
        return;
    }
    cb->aiming = held && cb->reload <= 0.0f && stock_count(&ga->stock, ammo) > 0;
    if (cb->aiming) {
        p->yaw = cam_yaw; // apunta hacia donde mira la camara
        cb->draw += dt;
    }
    // Arco y honda: se tensa y se suelta; ballesta y mosquete: disparan al pulsar.
    bool shoot = rd->draw_time > 0.0f ? (released && cb->draw > 0.05f) : (cb->aiming && pressed);
    if (shoot && cb->reload <= 0.0f && stock_count(&ga->stock, ammo) > 0) {
        float charge = rd->draw_time > 0.0f ? fminf(1.0f, cb->draw / rd->draw_time) : 1.0f;
        stock_take(&ga->stock, ammo, 1);
        fire(cb, rd, aim_origin(p->pos, p->yaw), p->yaw, cb->aim_pitch, charge, 0);
        cb->reload = rd->reload;
        cb->draw = 0.0f;
        cb->aiming = false;
    }
    if (!held) cb->draw = 0.0f;
}

// --------------------------------------------------------------- enemigos
static void update_enemies(Combat *cb, Player *p, GameActions *ga, Troop *troop, const Terrain *t, bool night, float dt,
                           char *log, size_t len) {
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        Enemy *e = &cb->enemies[i];
        if (!e->used) continue;
        const EnemyDef *def = enemy_def(e->kind);
        e->attack_anim = fmaxf(0.0f, e->attack_anim - dt);
        e->hit_anim = fmaxf(0.0f, e->hit_anim - dt);
        e->shown = fmaxf(0.0f, e->shown - dt);
        e->reload = fmaxf(0.0f, e->reload - dt);
        if (e->state == EN_DEAD) {
            e->corpse -= dt;
            if (e->corpse <= 0.0f) e->used = false;
            continue;
        }
        health_update(&e->h, &cb->rng, dt, false, 0.0f);
        enemy_down_check(e); // se desangro
        if (e->state == EN_DEAD) continue;
        if (dist2(e->pos, p->pos) > DESPAWN_DIST) {
            e->used = false;
            continue;
        }
        e->cooldown = fmaxf(0.0f, e->cooldown - dt);
        e->timer -= dt;
        // Objetivo: el mas cercano que vea (al acechar cuesta mas verte; los lobos ven de noche).
        float sight = def->sight * (p->sneaking ? 0.45f : 1.0f) * (night && !def->beast ? 0.7f : 1.0f);
        Vector3 tpos = { 0 };
        float best = 1e9f;
        int target = -1;
        if (!cb->player.down) {
            float d = dist2(e->pos, p->pos);
            if (d < sight || (e->target == 0 && d < sight * 1.5f)) best = d, target = 0, tpos = p->pos;
        }
        for (int k = 0; k < troop->count && k < TROOP_MAX; k++) {
            const Npc *n = &ga->npcs[k];
            const Member *m = &troop->members[k];
            if (m->status != STATUS_ACTIVE || m->health.down || n->member_id != m->id || !n->escort) continue;
            float d = dist2(e->pos, n->pos);
            if (d < def->sight && d < best) best = d, target = m->id, tpos = n->pos;
        }
        e->target = target;
        float speed = 0.0f;
        Vector3 goal = e->pos;
        const RangedDef *rd = ranged_def(def->ranged);
        if (target >= 0 && def->flee_at > 0.0f && e->h.hp < def->flee_at * e->h.hp_max) e->state = EN_FLEE, e->timer = 8.0f;
        if (e->state == EN_FLEE) {
            if (e->timer <= 0.0f) e->state = EN_WANDER;
            Vector3 from = target >= 0 ? tpos : p->pos;
            Vector3 away = Vector3Normalize((Vector3){ e->pos.x - from.x, 0, e->pos.z - from.z });
            goal = (Vector3){ e->pos.x + away.x * 5.0f, 0, e->pos.z + away.z * 5.0f };
            speed = def->speed;
        } else if (target >= 0) {
            e->state = EN_CHASE;
            e->yaw = atan2f(tpos.x - e->pos.x, tpos.z - e->pos.z);
            if (rd && best > def->reach) {
                // Arquero: guarda distancia y dispara con la curva calculada (y algo de error).
                Vector3 dir = Vector3Normalize((Vector3){ tpos.x - e->pos.x, 0, tpos.z - e->pos.z });
                if (best > 28.0f) goal = tpos, speed = def->speed;
                else if (best < 10.0f) goal = (Vector3){ e->pos.x - dir.x * 4.0f, 0, e->pos.z - dir.z * 4.0f }, speed = def->speed;
                if (e->reload <= 0.0f && best < 45.0f) {
                    Vector3 from = aim_origin(e->pos, e->yaw);
                    Vector3 aim = { tpos.x, tpos.y + 1.2f, tpos.z };
                    float pitch;
                    if (ballistic_solve(rd->projectile, to_v3(from), to_v3(aim), ranged_muzzle_speed(rd, 1.0f), &pitch)) {
                        float err = 0.015f + best * 0.0008f; // a mayor distancia, peor punteria
                        fire(cb, rd, from, e->yaw + (rng_float(&cb->rng) - 0.5f) * err * 2.0f, pitch + (rng_float(&cb->rng) - 0.5f) * err,
                             1.0f, -(1 + i));
                        e->reload = rd->reload + 1.4f + rng_float(&cb->rng);
                        e->attack_anim = 0.3f;
                    }
                }
            } else {
                if (best > def->reach * 0.9f) goal = tpos, speed = def->speed;
                if (best <= def->reach && e->cooldown <= 0.0f) enemy_strike(cb, e, p, ga, troop, log, len);
            }
        } else {
            e->state = EN_WANDER;
            if (e->timer <= 0.0f) {
                float a = rng_float(&cb->rng) * 2.0f * PI, r = rng_float(&cb->rng) * 12.0f;
                e->wander_to = (Vector3){ e->home.x + cosf(a) * r, 0, e->home.z + sinf(a) * r };
                e->timer = 4.0f + rng_float(&cb->rng) * 5.0f;
            }
            goal = e->wander_to;
            speed = def->speed * 0.3f;
        }
        speed *= health_speed_scale(&e->h) * armor_speed_scale(&e->armor);
        float d = dist2(e->pos, goal);
        e->speed = 0.0f;
        if (speed > 0.0f && d > 0.2f) {
            float step = fminf(speed * dt, d);
            e->pos.x += (goal.x - e->pos.x) / d * step;
            e->pos.z += (goal.z - e->pos.z) / d * step;
            if (e->state != EN_CHASE) e->yaw = atan2f(goal.x - e->pos.x, goal.z - e->pos.z);
            e->speed = speed;
        }
        e->pos.y = surface_y(t, e->pos.x, e->pos.z);
    }
}

// ------------------------------------------------------------------ escolta
static void outfit(Member *m, const Troop *troop) {
    // Equipo de la tribu: fieltro y cuero; los grandes guerreros, hierro.
    if (m->champion >= 0) {
        armor_equip(&m->armor, "armadura.torso.escamas_hierro");
        armor_equip(&m->armor, "armadura.casco.escamas_hierro");
        armor_equip(&m->armor, "armadura.grebas.escamas_hierro");
    } else {
        armor_equip(&m->armor, "armadura.torso.fieltro");
        if (m->id % 2) armor_equip(&m->armor, "armadura.casco.laminar_cuero");
        if (m->id % 3 == 0) armor_equip(&m->armor, "armadura.brazales.laminar_cuero");
    }
    (void)troop;
    m->outfitted = true;
}

static void update_companions(Combat *cb, GameActions *ga, Troop *troop, const Player *p, const Terrain *t, float dt,
                              char *log, size_t len) {
    float healer = troop_healer_skill(troop);
    for (int k = 0; k < troop->count && k < TROOP_MAX; k++) {
        Npc *n = &ga->npcs[k];
        Member *m = &troop->members[k];
        n->fight_anim = fmaxf(0.0f, n->fight_anim - dt);
        n->hurt_anim = fmaxf(0.0f, n->hurt_anim - dt);
        cb->comp_cd[k] = fmaxf(0.0f, cb->comp_cd[k] - dt);
        n->fighting = false;
        if (m->status != STATUS_ACTIVE) continue;
        if (!m->outfitted) outfit(m, troop);
        // Toda la tribu sangra y sana; en el campamento descansan.
        health_update(&m->health, &cb->rng, dt, !n->escort, healer);
        if (m->health.dead) {
            snprintf(log, len, "%s murió de sus heridas. La tribu está de luto (moral -6).", m->name);
            troop_mourn(troop, m->id, 6.0f);
            n->escort = false;
            continue;
        }
        if (!n->escort || m->health.down || n->member_id != m->id) continue;
        // La escolta pelea: va al enemigo mas cercano (a ella o al jugador).
        Enemy *best = NULL;
        float best_d = 14.0f;
        for (int i = 0; i < CB_MAX_ENEMIES; i++) {
            Enemy *e = &cb->enemies[i];
            if (!e->used || e->state == EN_DEAD) continue;
            float d = fminf(dist2(n->pos, e->pos), dist2(p->pos, e->pos));
            if (d < best_d) best_d = d, best = e;
        }
        // Tambien las fieras que amenazan.
        float ad;
        int an = fg_threat_near(ga, n->pos, best_d, &ad);
        if (an >= 0 && (!best || ad < dist2(n->pos, best->pos))) {
            const Animal *a = &ga->animals[an];
            Vector3 apos = { a->x, n->pos.y, a->z };
            float d = dist2(n->pos, apos);
            n->fighting = true;
            n->yaw = atan2f(apos.x - n->pos.x, apos.z - n->pos.z);
            n->moving = d > 1.6f;
            if (n->moving) {
                float speed = 5.0f * health_speed_scale(&m->health) * armor_speed_scale(&m->armor);
                float step = fminf(speed * dt, d - 1.5f);
                n->pos.x += (apos.x - n->pos.x) / d * step;
                n->pos.z += (apos.z - n->pos.z) / d * step;
                n->pos.y = surface_y(t, n->pos.x, n->pos.z);
            } else if (cb->comp_cd[k] <= 0.0f) {
                const Champion *c = troop_champion(troop, m->id);
                float dmg = combat_damage(COMPANION_DAMAGE, c ? c->stats.strength : 1.0f, health_attack_scale(&m->health),
                                          false, &cb->rng);
                fg_hurt(ga, an, dmg, WOUND_CUT, PART_RANDOM, m->id, NULL, 0);
                cb->comp_cd[k] = COMPANION_COOLDOWN;
                n->fight_anim = 0.4f;
            }
            continue;
        }
        if (!best) continue;
        n->fighting = true;
        float d = dist2(n->pos, best->pos);
        n->yaw = atan2f(best->pos.x - n->pos.x, best->pos.z - n->pos.z);
        float speed = 5.0f * health_speed_scale(&m->health) * armor_speed_scale(&m->armor);
        n->moving = d > 1.5f;
        if (n->moving) {
            float step = fminf(speed * dt, d - 1.4f);
            n->pos.x += (best->pos.x - n->pos.x) / d * step;
            n->pos.z += (best->pos.z - n->pos.z) / d * step;
            n->pos.y = surface_y(t, n->pos.x, n->pos.z);
        } else if (cb->comp_cd[k] <= 0.0f) {
            const Champion *c = troop_champion(troop, m->id);
            float strength = c ? c->stats.strength : 1.0f;
            float dmg = combat_damage(COMPANION_DAMAGE, strength, health_attack_scale(&m->health), false, &cb->rng);
            combat_apply_hit(&best->h, &best->armor, &cb->rng, dmg, WOUND_CUT, PART_RANDOM, false, NULL, NULL);
            best->hit_anim = 0.3f;
            best->shown = 6.0f;
            cb->comp_cd[k] = COMPANION_COOLDOWN;
            n->fight_anim = 0.4f;
            enemy_down_check(best);
            if (best->state == EN_DEAD) {
                char who[48];
                snprintf(log, len, "%s abatió a un %s.", m->name, lower_name(enemy_def(best->kind)->name, who, sizeof(who)));
            }
        }
    }
}

// ----------------------------------------------------------------- vendajes
static Npc *npc_near(GameActions *ga, Troop *troop, const Player *p, float range, bool want_down, Member **out) {
    for (int k = 0; k < troop->count && k < TROOP_MAX; k++) {
        Npc *n = &ga->npcs[k];
        Member *m = &troop->members[k];
        if (m->status != STATUS_ACTIVE || n->member_id != m->id || dist2(n->pos, p->pos) > range) continue;
        if (want_down ? m->health.down : health_untreated(&m->health) > 0) {
            *out = m;
            return n;
        }
    }
    return NULL;
}

static void bandage(Combat *cb, GameActions *ga, Troop *troop, const Player *p, char *log, size_t len) {
    bool herbs = stock_count(&ga->stock, HERBS_ID) > 0;
    Member *m = NULL;
    // 1) Un companero abatido cerca: levantarlo (y vendarlo si hay hierbas).
    if (npc_near(ga, troop, p, 3.0f, true, &m)) {
        if (herbs) {
            stock_take(&ga->stock, HERBS_ID, 1);
            health_treat(&m->health);
        }
        health_revive(&m->health);
        snprintf(log, len, herbs ? "Levantas y vendas a %s." : "Levantas a %s, pero sin hierbas sigue sangrando.", m->name);
        return;
    }
    if (!herbs) {
        snprintf(log, len, "No quedan hierbas curativas en el acopio.");
        return;
    }
    // 2) Tus propias heridas.
    if (health_untreated(&cb->player) > 0) {
        stock_take(&ga->stock, HERBS_ID, 1);
        int n = health_treat(&cb->player);
        snprintf(log, len, "Te vendas %d herida%s con hierbas curativas.", n, n == 1 ? "" : "s");
        return;
    }
    // 3) Un companero herido cerca.
    if (npc_near(ga, troop, p, 3.0f, false, &m)) {
        stock_take(&ga->stock, HERBS_ID, 1);
        health_treat(&m->health);
        snprintf(log, len, "Vendas las heridas de %s.", m->name);
        return;
    }
    snprintf(log, len, "No hay heridas que vendar.");
}

static const Member *role_member(const Troop *troop, Role role) {
    for (int i = 0; i < troop->count; i++)
        if (troop->members[i].status == STATUS_ACTIVE && troop->members[i].role == role && !troop->members[i].health.down)
            return &troop->members[i];
    return NULL;
}

// ------------------------------------------------------------------ jugador
static void update_player(Combat *cb, Player *p, GameActions *ga, Troop *troop, const Terrain *t, Vector3 camp_fire,
                          float dt, char *log, size_t len) {
    bool at_camp = dist2(p->pos, camp_fire) < CAMP_RADIUS;
    float healer = at_camp ? troop_healer_skill(troop) : 0.0f;
    health_update(&cb->player, &cb->rng, dt, !p->moving, healer);
    // El jugador no muere: desangrado queda abatido hasta que lo socorran.
    if (cb->player.dead) {
        cb->player.dead = false;
        cb->player.down = true;
        cb->player.blood = fmaxf(cb->player.blood, 0.06f);
        cb->player.hp = fmaxf(cb->player.hp, -0.4f * cb->player.hp_max);
    }
    // El curandero del campamento atiende al jugador.
    cb->healer_timer -= dt;
    const Member *hm = role_member(troop, ROLE_HEALER);
    if (at_camp && hm && cb->healer_timer <= 0.0f && health_untreated(&cb->player) > 0) {
        health_treat(&cb->player);
        cb->healer_timer = 8.0f;
        snprintf(log, len, "%s te venda las heridas.", hm->name);
    }
    if (!cb->player.down) {
        cb->down_timer = 0.0f;
        return;
    }
    // Abatido: la escolta te levanta; si estas solo, despiertas en el campamento.
    cb->down_timer += dt;
    const Member *helper = NULL;
    for (int k = 0; k < troop->count && k < TROOP_MAX; k++) {
        const Npc *n = &ga->npcs[k];
        const Member *m = &troop->members[k];
        if (n->escort && m->status == STATUS_ACTIVE && !m->health.down && dist2(n->pos, p->pos) < 20.0f) helper = m;
    }
    if (helper && cb->down_timer > DOWN_HELP_SECONDS) {
        health_treat(&cb->player);
        health_revive(&cb->player);
        snprintf(log, len, "%s te levanta y te venda. ¡Sigue en pie!", helper->name);
    } else if (!helper && cb->down_timer > DOWN_WAKE_SECONDS) {
        if (ga->mounted >= 0) ga->animals[ga->mounted].ridden = false, ga->mounted = -1;
        p->pos = (Vector3){ camp_fire.x - 2.5f, terrain_height(t, camp_fire.x - 2.5f, camp_fire.z), camp_fire.z };
        p->vy = 0.0f;
        health_treat(&cb->player);
        health_revive(&cb->player);
        ga->hands.carried[0] = '\0';
        troop_adjust_morale(troop, -5.0f);
        snprintf(log, len, "Despiertas en el campamento: te encontraron malherido (moral -5).");
    }
}

void cb_update(Combat *cb, Player *p, GameActions *ga, Troop *troop, const Terrain *t, Vector3 camp_fire, bool input_ok,
               bool night, bool winter, float cam_yaw, float cam_pitch, float dt, char *log, size_t log_len) {
    float time = (float)GetTime();
    cb->attack_cd = fmaxf(0.0f, cb->attack_cd - dt);
    cb->attack_anim = fmaxf(0.0f, cb->attack_anim - dt);
    cb->hit_anim = fmaxf(0.0f, cb->hit_anim - dt);
    cb->reload = fmaxf(0.0f, cb->reload - dt);
    if (IsKeyPressed(KEY_P)) cb->show_panel = !cb->show_panel;
    bool can_act = input_ok && !cb->player.down && !ga->climbing;
    const RangedDef *rd = ga->hands.sheathed ? NULL : ranged_def(ga->hands.right.id);
    cb->blocking = can_act && IsKeyDown(KEY_Z) && !ga->hands.sheathed && !rd;
    if (rd) player_ranged(cb, p, ga, rd, can_act, cam_yaw, cam_pitch, dt, log, log_len);
    else {
        cb->aiming = false;
        if (can_act && cb->attack_cd <= 0.0f && (IsKeyPressed(KEY_V) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))
            player_attack(cb, p, ga, t, log, log_len);
    }
    if (input_ok && IsKeyPressed(KEY_B)) bandage(cb, ga, troop, p, log, log_len);
    if (input_ok && IsKeyPressed(KEY_NINE)) { // prueba: enemigos delante
        const char *what = IsKeyDown(KEY_LEFT_SHIFT) ? "lobos" : IsKeyDown(KEY_LEFT_CONTROL) ? "culto"
                           : IsKeyDown(KEY_LEFT_ALT)                                         ? "arqueros"
                                                                                             : "bandidos";
        cb_spawn_group(cb, ga, what, p, t, 14.0f, log, log_len);
    }
    natural_spawns(cb, ga, p, t, camp_fire, night, dt, log, log_len);
    (void)winter;
    update_enemies(cb, p, ga, troop, t, night, dt, log, log_len);
    update_shots(cb, p, ga, troop, t, time, dt, log, log_len);
    update_companions(cb, ga, troop, p, t, dt, log, log_len);
    update_player(cb, p, ga, troop, t, camp_fire, dt, log, log_len);
    // Animacion del jugador.
    ga->pl_down = cb->player.down;
    ga->pl_hit = cb->hit_anim > 0.0f;
    ga->pl_attacking = cb->attack_anim > 0.0f ? cb->combo : 0;
    ga->pl_blocking = cb->blocking;
    ga->pl_limping = health_speed_scale(&cb->player) < 0.85f;
    ga->pl_ranged = cb->aiming ? ranged_anim(rd) : 0;
}

void cb_new_day(Combat *cb, Troop *troop) {
    health_daily(&cb->player, troop_healer_skill(troop));
    // Las armaduras se remiendan en el campamento; con herrero, mucho mas.
    float fix = role_member(troop, ROLE_SMITH) ? 0.4f : 0.1f;
    armor_repair(&cb->armor, fix);
    for (int i = 0; i < troop->count; i++) armor_repair(&troop->members[i].armor, fix);
}

// ------------------------------------------------------------------- dibujo
static BodyColors enemy_colors(EnemyKind k, bool hit) {
    BodyColors c = { { 196, 156, 118, 255 }, { 120, 90, 60, 255 }, { 80, 64, 48, 255 } };
    if (k == ENEMY_FANATIC) c.cloth = (Color){ 96, 22, 32, 255 };
    else if (k == ENEMY_CAPTOR) c.cloth = (Color){ 60, 18, 30, 255 };
    else if (k == ENEMY_ARCHER) c.cloth = (Color){ 104, 98, 66, 255 };
    if (hit) c.cloth = (Color){ 236, 226, 214, 255 };
    return c;
}

static void draw_shot(const Shot *s) {
    V3 d = s->stuck ? s->dir : s->p.vel;
    float l = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
    if (l < 1e-4f) return;
    Vector3 tip = to_vec(s->p.pos);
    float len = s->p.kind == PROJ_STONE || s->p.kind == PROJ_BALL ? 0.0f : (s->p.kind == PROJ_BOLT ? 0.4f : 0.75f);
    if (len <= 0.0f) {
        DrawSphere(tip, s->p.kind == PROJ_STONE ? 0.04f : 0.02f, (Color){ 120, 116, 108, 255 });
        return;
    }
    // Clavada, se hunde un poco en el suelo.
    if (s->stuck) tip = (Vector3){ tip.x + d.x / l * 0.2f, tip.y + d.y / l * 0.2f, tip.z + d.z / l * 0.2f };
    Vector3 tail = { tip.x - d.x / l * len, tip.y - d.y / l * len, tip.z - d.z / l * len };
    DrawCylinderEx(tail, tip, 0.012f, 0.012f, 4, (Color){ 140, 104, 64, 255 });
    DrawCylinderEx(tail, Vector3Lerp(tail, tip, 0.15f), 0.03f, 0.012f, 4, (Color){ 220, 214, 200, 255 }); // plumas
}

void cb_draw_world(const Combat *cb, Props *props, const GameActions *ga, const Terrain *t, const Player *p,
                   bool player_has_model, float time) {
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        const Enemy *e = &cb->enemies[i];
        if (!e->used) continue;
        const EnemyDef *def = enemy_def(e->kind);
        const InvItem *it = inventory_find(ga->inv, def->model);
        if (!it) continue;
        bool dead = e->state == EN_DEAD;
        if (props_has_model(props, it)) {
            HumanoidState hs = { .moving = e->speed > 0.2f, .running = e->speed > 3.5f, .grounded = true, .doing = -1,
                                 .building = -1, .dead = dead, .hit = e->hit_anim > 0.0f,
                                 .attacking = e->attack_anim > 0.0f && !def->ranged ? 1 + i % 3 : 0,
                                 .ranged = def->ranged && e->target >= 0 && e->speed < 0.2f ? 1 : 0,
                                 .limping = health_speed_scale(&e->h) < 0.85f, .grip = GRIP_ONE_HANDED };
            const char *clip = anim_humanoid(&hs);
            // Muerto: el clip se queda en su ultimo tramo en vez de repetirse.
            float tt = dead ? fminf(CORPSE_SECONDS - e->corpse, 1.2f) : time + (float)i * 0.31f;
            props_draw_item_anim(props, it, e->pos, e->yaw - PI / 2.0f, clip, tt);
            continue;
        }
        // Sin modelo: cuerpo articulado con su armadura.
        BodyPose b;
        pose_enemy(&b, e, time, i);
        body_draw(&b, e->pos, e->yaw, enemy_colors(e->kind, e->hit_anim > 0.0f), &e->armor);
    }
    for (int i = 0; i < CB_MAX_SHOTS; i++)
        if (cb->shots[i].p.alive) draw_shot(&cb->shots[i]);
    // El jugador sin modelo importado: cuerpo articulado con su armadura.
    if (!player_has_model) {
        BodyPose b;
        pose_player(&b, cb, p, time);
        BodyColors c = { { 204, 164, 124, 255 }, { 150, 40, 40, 255 }, { 88, 66, 46, 255 } };
        if (cb->hit_anim > 0.0f) c.cloth = (Color){ 240, 230, 220, 255 };
        Vector3 pos = { p->pos.x, p->pos.y + p->draw_lift, p->pos.z };
        body_draw(&b, pos, p->yaw, c, &cb->armor);
    }
    // La mira: la curva que hara el proyectil con la tension actual.
    const RangedDef *rd = ga->hands.sheathed ? NULL : ranged_def(ga->hands.right.id);
    if (cb->aiming && rd) {
        float charge = rd->draw_time > 0.0f ? fminf(1.0f, cb->draw / rd->draw_time) : 1.0f;
        V3 pts[90];
        int n = ballistic_trace(rd->projectile, to_v3(aim_origin(p->pos, p->yaw)), p->yaw, cb->aim_pitch,
                                ranged_muzzle_speed(rd, charge), 0.04f, p->pos.y - 40.0f, pts, 90);
        for (int i = 1; i < n; i++) {
            if (pts[i].y < terrain_height(t, pts[i].x, pts[i].z)) {
                DrawSphere(to_vec(pts[i]), 0.25f, (Color){ 214, 72, 52, 255 }); // donde cae
                break;
            }
            float k = 0.06f + 0.004f * (float)i; // lejos, mas grande para que se vea
            DrawCube(to_vec(pts[i]), k, k, k, (Color){ 232, 196, 92, 255 });
        }
    }
}

static void bar_at(Camera3D cam, Vector3 pos, float h, float frac, Color fill, int w, int hgt) {
    Vector3 to = Vector3Subtract(pos, cam.position);
    if (Vector3DotProduct(to, Vector3Subtract(cam.target, cam.position)) <= 0.0f || Vector3Length(to) > 30.0f) return;
    Vector2 s = GetWorldToScreenEx((Vector3){ pos.x, pos.y + h, pos.z }, cam, w, hgt);
    DrawRectangle((int)s.x - 14, (int)s.y - 2, 28, 4, UI_LEATHER_CRACK);
    DrawRectangle((int)s.x - 13, (int)s.y - 1, (int)(26 * Clamp(frac, 0.0f, 1.0f)), 2, fill);
}

void cb_draw_overlay(const Combat *cb, const GameActions *ga, const Troop *troop, Camera3D cam, int w, int h) {
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        const Enemy *e = &cb->enemies[i];
        if (!e->used || e->state == EN_DEAD || (e->shown <= 0.0f && e->target < 0)) continue;
        bar_at(cam, e->pos, 2.0f, e->h.hp / e->h.hp_max, UI_CARNELIAN, w, h);
    }
    for (int k = 0; k < troop->count && k < TROOP_MAX; k++) {
        const Member *m = &troop->members[k];
        const Npc *n = &ga->npcs[k];
        if (m->status != STATUS_ACTIVE || n->member_id != m->id) continue;
        if (m->health.hp >= m->health.hp_max * 0.98f && !m->health.down) continue;
        bar_at(cam, n->pos, m->health.down ? 0.7f : 2.0f, m->health.hp / m->health.hp_max,
               health_bleeding(&m->health) ? UI_CARNELIAN : UI_TURQUOISE, w, h);
    }
}

void cb_draw_hud(const Combat *cb, const GameActions *ga, const Troop *troop, int right_x, int y, int w, int h) {
    const Health *ph = &cb->player;
    const char *txt = TextFormat("%s%s", health_state_name(ph), health_bleeding(ph) ? " · sangra" : "");
    Color col = health_bleeding(ph) || ph->down ? UI_CARNELIAN : ph->wound_count ? UI_GOLD : UI_BONE;
    ui_text(txt, right_x - MeasureText(txt, 10), y, 10, col);
    ui_bar(right_x - 92, y + 12, 92, fmaxf(0.0f, ph->hp) / ph->hp_max, UI_CARNELIAN, UI_METAL_SILVER);
    DrawRectangle(right_x - 92, y + 19, (int)(92 * ph->blood), 2, UI_LAPIS); // sangre

    // Arma a distancia: municion, tension y recarga, abajo al centro.
    const RangedDef *rd = ga->hands.sheathed ? NULL : ranged_def(ga->hands.right.id);
    if (rd) {
        const InvItem *wi = inventory_find(ga->inv, rd->weapon);
        const ProjectileDef *pd = projectile_def(rd->projectile);
        const char *line = TextFormat("%s · %ss: %d%s", wi ? wi->name : "Arma", pd->name, stock_count(&ga->stock, pd->ammo),
                                      cb->reload > 0.0f ? " · recargando" : "");
        int lw = MeasureText(line, 10);
        ui_text(line, w / 2 - lw / 2, h - 58, 10, UI_BONE);
        if (cb->aiming && rd->draw_time > 0.0f)
            ui_bar(w / 2 - 50, h - 47, 100, fminf(1.0f, cb->draw / rd->draw_time), UI_GOLD, UI_METAL_GOLD);
    }

    if (cb->show_panel) { // heridas y armadura propias, heridas de la tribu
        const int pw = 340, x0 = 8, y0 = 162;
        char lines[18][112];
        int n = 0;
        snprintf(lines[n++], sizeof(lines[0]), "Tú: %s (vida %d/%d, sangre %d%%)", health_state_name(ph),
                 (int)fmaxf(0.0f, ph->hp), (int)ph->hp_max, (int)(ph->blood * 100));
        for (int i = 0; i < ph->wound_count && n < 6; i++) {
            char d[96];
            wound_describe(&ph->wounds[i], false, d, sizeof(d));
            snprintf(lines[n++], sizeof(lines[0]), "  %s", d);
        }
        for (int s = 0; s < SLOT_COUNT && n < 11; s++) {
            const ArmorPiece *pc = &cb->armor.slot[s];
            if (!pc->id[0]) continue;
            const InvItem *it = inventory_find(ga->inv, pc->id);
            snprintf(lines[n++], sizeof(lines[0]), "  [%s] %s %d%%%s", slot_name((ArmorSlot)s), it ? it->name : pc->id,
                     (int)(100.0f * pc->durability / pc->durability_max), pc->durability <= 0.0f ? " (rota)" : "");
        }
        for (int k = 0; k < troop->count && n < 17; k++) {
            const Member *m = &troop->members[k];
            if (m->status != STATUS_ACTIVE || (m->health.wound_count == 0 && !m->health.down)) continue;
            int worst = health_worst(&m->health);
            char d[96] = "";
            if (worst >= 0) wound_describe(&m->health.wounds[worst], false, d, sizeof(d));
            snprintf(lines[n++], sizeof(lines[0]), "%s: %s%s%s", m->name, health_state_name(&m->health), d[0] ? " · " : "", d);
        }
        int ph_h = 2 * UI_PANEL_INSET + 30 + 11 * n;
        ui_panel((Rectangle){ (float)x0, (float)y0, (float)pw, (float)ph_h }, UI_METAL_SILVER);
        int x = x0 + UI_PANEL_INSET + 2, yy = y0 + UI_PANEL_INSET + 2;
        ui_text(TextFormat("Salud y armadura (P)  ·  B vendar  ·  hierbas: %d", stock_count(&ga->stock, HERBS_ID)), x, yy, 10,
                UI_GOLD_LIGHT);
        yy += 14;
        for (int i = 0; i < n; i++, yy += 11) ui_text(lines[i], x, yy, 10, i == 0 ? UI_BONE : UI_BONE_DIM);
    }
    if (ph->down) {
        DrawRectangle(0, 0, w, h, (Color){ 60, 0, 0, 90 });
        ui_text_centered("Estás abatido", w / 2, h / 2 - 30, 20, UI_CARNELIAN);
        ui_text_centered("Si tu escolta está cerca, te levantará; si no, la tribu te buscará.", w / 2, h / 2 - 6, 10, UI_BONE);
    }
}
