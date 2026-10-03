#include "combat_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"
#include "sim/anim_index.h"
#include "sim/hazards.h"
#include "ui/theme.h"

#define HERBS_ID "utileria.consumible.hierbas"
#define CAMP_RADIUS 14.0f
#define DOWN_WAKE_SECONDS 25.0f // abatido y solo: despierta en el campamento
#define DOWN_HELP_SECONDS 5.0f  // con la escolta cerca: lo levantan
#define CORPSE_SECONDS 30.0f
#define DESPAWN_DIST 160.0f
#define COMPANION_DAMAGE 10.0f
#define COMPANION_COOLDOWN 1.2f

static float dist2(Vector3 a, Vector3 b) { return Vector2Distance((Vector2){ a.x, a.z }, (Vector2){ b.x, b.z }); }

void cb_init(Combat *cb, unsigned seed) {
    memset(cb, 0, sizeof(*cb));
    cb->seed = seed;
    rng_seed(&cb->rng, seed ^ 0xC0B7u);
    health_init(&cb->player, 100.0f);
}

bool cb_blocks_input(const Combat *cb) { return cb->player.down; }

float cb_speed_scale(const Combat *cb) {
    float s = health_speed_scale(&cb->player);
    if (cb->blocking) s *= 0.5f;
    return cb->player.down ? 1.0f : s;
}

static float surface_y(const Terrain *t, float x, float z) {
    float ground = terrain_height(t, x, z);
    if (t->look.water_level > ground + 0.25f)
        return hazard_ice_walkable(t->look.ice) ? t->look.water_level : fmaxf(ground, t->look.water_level - 1.25f);
    return ground;
}

// ------------------------------------------------------------------- aparicion
static Enemy *spawn_enemy(Combat *cb, EnemyKind kind, Vector3 pos, const Terrain *t) {
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        Enemy *e = &cb->enemies[i];
        if (e->used) continue;
        memset(e, 0, sizeof(*e));
        e->used = true;
        e->kind = kind;
        e->pos = (Vector3){ pos.x, surface_y(t, pos.x, pos.z), pos.z };
        e->home = e->wander_to = e->pos;
        e->target = -1;
        health_init(&e->h, enemy_def(kind)->hp);
        return e;
    }
    return NULL;
}

void cb_spawn_group(Combat *cb, const char *what, const Player *p, const Terrain *t, float dist, char *log, size_t len) {
    float ang = rng_float(&cb->rng) * 2.0f * PI;
    if (dist < 25.0f) ang = p->yaw + (rng_float(&cb->rng) - 0.5f) * 0.8f; // pruebas: delante del jugador
    Vector3 c = { p->pos.x + sinf(ang) * dist, 0, p->pos.z + cosf(ang) * dist };
    EnemyKind kinds[4];
    int n = 0;
    if (!strcmp(what, "lobos")) {
        n = 2 + rng_range(&cb->rng, 3);
        for (int i = 0; i < n; i++) kinds[i] = ENEMY_WOLF;
        snprintf(log, len, "Una manada de lobos te rodea.");
    } else if (!strcmp(what, "culto")) {
        n = 3;
        kinds[0] = kinds[1] = ENEMY_FANATIC;
        kinds[2] = ENEMY_CAPTOR;
        snprintf(log, len, "¡Fanáticos del culto! Buscan prisioneros para el sacrificio.");
    } else {
        n = 2 + rng_range(&cb->rng, 2);
        for (int i = 0; i < n; i++) kinds[i] = ENEMY_BANDIT;
        snprintf(log, len, "¡Bandidos! Vienen a por tu botín.");
    }
    for (int i = 0; i < n; i++) {
        float a = (float)i * 2.1f;
        Enemy *e = spawn_enemy(cb, kinds[i], (Vector3){ c.x + cosf(a) * 2.5f, 0, c.z + sinf(a) * 2.5f }, t);
        if (e) e->yaw = ang + PI;
    }
}

// Lejos del campamento aparecen bandidos o el culto de dia, y lobos de noche y en invierno.
static void natural_spawns(Combat *cb, const Player *p, const Terrain *t, Vector3 camp_fire, bool night, bool winter,
                           float dt, char *log, size_t len) {
    cb->spawn_timer += dt;
    if (cb->spawn_timer < 60.0f) return;
    cb->spawn_timer = 0.0f;
    if (dist2(p->pos, camp_fire) < 90.0f) return;
    int alive = 0;
    for (int i = 0; i < CB_MAX_ENEMIES; i++) alive += cb->enemies[i].used && cb->enemies[i].state != EN_DEAD;
    if (alive > 6) return;
    float r = rng_float(&cb->rng);
    if (night || winter) {
        if (r < 0.22f) cb_spawn_group(cb, "lobos", p, t, 35.0f, log, len);
    } else if (r < 0.12f) {
        cb_spawn_group(cb, rng_float(&cb->rng) < 0.3f ? "culto" : "bandidos", p, t, 35.0f, log, len);
    }
}

// ------------------------------------------------------------------- golpes
static void describe_last(const Health *h, int w, char *out, size_t len) {
    if (w < 0) {
        out[0] = '\0';
        return;
    }
    char d[96];
    wound_describe(&h->wounds[w], d, sizeof(d));
    snprintf(out, len, "%s", d);
}

static void lower_first(char *s) {
    if (s[0] >= 'A' && s[0] <= 'Z') s[0] = (char)(s[0] - 'A' + 'a');
}

static void player_attack(Combat *cb, Player *p, GameActions *ga, char *log, size_t len) {
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
    if (!best) return;
    const EnemyDef *def = enemy_def(best->kind);
    float dmg = combat_damage(w.damage, 1.0f, health_attack_scale(&cb->player), false, &cb->rng);
    int wi = health_hit(&best->h, &cb->rng, dmg, w.wound, PART_RANDOM);
    best->hit_anim = 0.3f;
    best->shown = 6.0f;
    if (best->target < 0) best->target = 0; // ahora sabe donde estas
    char d[96];
    describe_last(&best->h, wi, d, sizeof(d));
    lower_first(d);
    if (best->h.down || best->h.dead) {
        best->state = EN_DEAD;
        best->corpse = CORPSE_SECONDS;
        char who[48];
        snprintf(who, sizeof(who), "%s", def->name);
        lower_first(who);
        snprintf(log, len, "Abatiste al %s.", who);
    } else {
        snprintf(log, len, "Golpeas: %s.", d);
    }
}

// Un enemigo golpea al jugador (target 0) o a un integrante.
static void enemy_strike(Combat *cb, Enemy *e, Player *p, GameActions *ga, Troop *troop, char *log, size_t len) {
    const EnemyDef *def = enemy_def(e->kind);
    e->cooldown = def->cooldown;
    e->attack_anim = 0.45f;
    float strength = 1.0f, arms = health_attack_scale(&e->h);
    if (e->target == 0) {
        // Escudo (o el arma) si te cubres de frente.
        Vector3 fwd = { sinf(p->yaw), 0, cosf(p->yaw) };
        Vector3 to = Vector3Normalize((Vector3){ e->pos.x - p->pos.x, 0, e->pos.z - p->pos.z });
        bool shield = ga->hands.left.kind == INV_HANDS_SHIELD && ga->hands.left.id[0] && !ga->hands.sheathed;
        float chance = cb->blocking ? (shield ? combat_block_chance(true, Vector3DotProduct(fwd, to))
                                              : (Vector3DotProduct(fwd, to) > 0.35f ? 0.25f : 0.0f))
                                    : 0.0f;
        bool blocked = rng_float(&cb->rng) < chance;
        float dmg = combat_damage(def->damage, strength, arms, blocked, &cb->rng);
        int wi = health_hit(&cb->player, &cb->rng, dmg, def->wound, PART_RANDOM);
        cb->hit_anim = 0.3f;
        if (blocked) {
            snprintf(log, len, "Paras el golpe del %s.", def->name);
        } else {
            char d[96];
            describe_last(&cb->player, wi, d, sizeof(d));
            lower_first(d);
            snprintf(log, len, "Te hieren: %s.", d);
        }
        return;
    }
    Member *m = troop_find(troop, e->target);
    if (!m || m->status != STATUS_ACTIVE) return;
    float dmg = combat_damage(def->damage, strength, arms, false, &cb->rng);
    health_hit(&m->health, &cb->rng, dmg, def->wound, PART_RANDOM);
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
        if (ga->npcs[i].member_id == m->id) ga->npcs[i].hurt_anim = 0.3f;
    if (m->health.down) snprintf(log, len, "¡%s cae herido! Acércate y pulsa B para levantarlo.", m->name);
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
        if (e->state == EN_DEAD) {
            e->corpse -= dt;
            if (e->corpse <= 0.0f) e->used = false;
            continue;
        }
        health_update(&e->h, &cb->rng, dt, false, 0.0f);
        if (e->h.down || e->h.dead) { // se desangro
            e->state = EN_DEAD;
            e->corpse = CORPSE_SECONDS;
            continue;
        }
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
        if (target >= 0 && def->flee_at > 0.0f && e->h.hp < def->flee_at * e->h.hp_max) e->state = EN_FLEE, e->timer = 8.0f;
        if (e->state == EN_FLEE) {
            if (e->timer <= 0.0f) e->state = EN_WANDER;
            Vector3 from = target >= 0 ? tpos : p->pos;
            Vector3 away = Vector3Normalize((Vector3){ e->pos.x - from.x, 0, e->pos.z - from.z });
            goal = (Vector3){ e->pos.x + away.x * 5.0f, 0, e->pos.z + away.z * 5.0f };
            speed = def->speed;
        } else if (target >= 0) {
            e->state = EN_CHASE;
            if (best > def->reach * 0.9f) goal = tpos, speed = def->speed;
            if (best <= def->reach && e->cooldown <= 0.0f) enemy_strike(cb, e, p, ga, troop, log, len);
            e->yaw = atan2f(tpos.x - e->pos.x, tpos.z - e->pos.z);
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
        speed *= health_speed_scale(&e->h);
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
        if (!best) continue;
        n->fighting = true;
        float d = dist2(n->pos, best->pos);
        n->yaw = atan2f(best->pos.x - n->pos.x, best->pos.z - n->pos.z);
        float speed = 5.0f * health_speed_scale(&m->health);
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
            health_hit(&best->h, &cb->rng, dmg, WOUND_CUT, PART_RANDOM);
            best->hit_anim = 0.3f;
            best->shown = 6.0f;
            cb->comp_cd[k] = COMPANION_COOLDOWN;
            n->fight_anim = 0.4f;
            if (best->h.down || best->h.dead) {
                best->state = EN_DEAD;
                best->corpse = CORPSE_SECONDS;
                char who[48];
                snprintf(who, sizeof(who), "%s", enemy_def(best->kind)->name);
                lower_first(who);
                snprintf(log, len, "%s abatió a un %s.", m->name, who);
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

static const Member *healer_member(const Troop *troop) {
    for (int i = 0; i < troop->count; i++)
        if (troop->members[i].status == STATUS_ACTIVE && troop->members[i].role == ROLE_HEALER && !troop->members[i].health.down)
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
    const Member *hm = healer_member(troop);
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
               bool night, bool winter, float dt, char *log, size_t log_len) {
    cb->attack_cd = fmaxf(0.0f, cb->attack_cd - dt);
    cb->attack_anim = fmaxf(0.0f, cb->attack_anim - dt);
    cb->hit_anim = fmaxf(0.0f, cb->hit_anim - dt);
    if (IsKeyPressed(KEY_P)) cb->show_panel = !cb->show_panel;
    bool can_act = input_ok && !cb->player.down && !ga->climbing;
    cb->blocking = can_act && IsKeyDown(KEY_Z) && !ga->hands.sheathed;
    if (can_act && cb->attack_cd <= 0.0f && (IsKeyPressed(KEY_V) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))
        player_attack(cb, p, ga, log, log_len);
    if (input_ok && IsKeyPressed(KEY_B)) bandage(cb, ga, troop, p, log, log_len);
    if (input_ok && IsKeyPressed(KEY_NINE)) { // prueba: enemigos delante
        const char *what = IsKeyDown(KEY_LEFT_SHIFT) ? "lobos" : IsKeyDown(KEY_LEFT_CONTROL) ? "culto" : "bandidos";
        cb_spawn_group(cb, what, p, t, 14.0f, log, log_len);
    }
    natural_spawns(cb, p, t, camp_fire, night, winter, dt, log, log_len);
    update_enemies(cb, p, ga, troop, t, night, dt, log, log_len);
    update_companions(cb, ga, troop, p, t, dt, log, log_len);
    update_player(cb, p, ga, troop, t, camp_fire, dt, log, log_len);
    // Animacion del jugador.
    ga->pl_down = cb->player.down;
    ga->pl_hit = cb->hit_anim > 0.0f;
    ga->pl_attacking = cb->attack_anim > 0.0f ? cb->combo : 0;
    ga->pl_blocking = cb->blocking;
    ga->pl_limping = health_speed_scale(&cb->player) < 0.85f;
}

void cb_new_day(Combat *cb, const Troop *troop) { health_daily(&cb->player, troop_healer_skill(troop)); }

// ------------------------------------------------------------------- dibujo
void cb_draw_world(const Combat *cb, Props *props, const GameActions *ga, const Player *p, bool player_has_model,
                   float time) {
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        const Enemy *e = &cb->enemies[i];
        if (!e->used) continue;
        const EnemyDef *def = enemy_def(e->kind);
        const InvItem *it = inventory_find(ga->inv, def->model);
        if (!it) continue;
        bool dead = e->state == EN_DEAD;
        if (props_has_model(props, it)) {
            const char *clip;
            if (def->beast) {
                clip = anim_quadruped_fight(e->speed, e->attack_anim > 0.0f, e->hit_anim > 0.0f, dead);
            } else {
                HumanoidState hs = { .moving = e->speed > 0.2f, .running = e->speed > 3.5f, .grounded = true, .doing = -1,
                                     .building = -1, .dead = dead, .hit = e->hit_anim > 0.0f,
                                     .attacking = e->attack_anim > 0.0f ? 1 + i % 3 : 0,
                                     .limping = health_speed_scale(&e->h) < 0.85f, .grip = GRIP_ONE_HANDED };
                clip = anim_humanoid(&hs);
            }
            // Muerto: el clip se queda en su ultimo tramo en vez de repetirse.
            float t = dead ? fminf(CORPSE_SECONDS - e->corpse, 1.2f) : time + (float)i * 0.31f;
            props_draw_item_anim(props, it, e->pos, e->yaw - PI / 2.0f, clip, t);
            continue;
        }
        // Marcador: de pie, o tendido si cayo.
        Color c = def->beast ? (Color){ 120, 110, 100, 255 } : e->kind == ENEMY_BANDIT ? (Color){ 120, 70, 50, 255 }
                                                                                    : (Color){ 90, 30, 40, 255 };
        if (e->hit_anim > 0.0f) c = (Color){ 230, 220, 210, 255 };
        rlPushMatrix();
        rlTranslatef(e->pos.x, e->pos.y, e->pos.z);
        rlRotatef(e->yaw * RAD2DEG, 0, 1, 0);
        if (dead) DrawCube((Vector3){ 0, 0.15f, 0 }, 0.6f, 0.3f, def->beast ? 1.1f : 1.7f, ColorBrightness(c, -0.3f));
        else if (def->beast) DrawCube((Vector3){ 0, 0.45f, 0 }, 0.35f, 0.6f, 1.1f, c);
        else DrawCube((Vector3){ 0, 0.85f, 0 }, 0.5f, 1.7f, 0.35f, c);
        if (!dead && e->attack_anim > 0.0f) // el golpe: un destello hacia adelante
            DrawCube((Vector3){ 0, def->beast ? 0.5f : 1.1f, def->reach * 0.6f }, 0.08f, 0.08f, def->reach * 0.7f,
                     (Color){ 240, 230, 200, 255 });
        rlPopMatrix();
    }
    // El jugador abatido (sin modelo importado): tendido en el suelo.
    if (cb->player.down && !player_has_model)
        DrawCapsule((Vector3){ p->pos.x - 0.6f, p->pos.y + 0.25f, p->pos.z }, (Vector3){ p->pos.x + 0.6f, p->pos.y + 0.25f, p->pos.z },
                    0.26f, 6, 4, (Color){ 150, 50, 45, 255 });
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
        bar_at(cam, e->pos, enemy_def(e->kind)->beast ? 1.1f : 2.0f, e->h.hp / e->h.hp_max, UI_CARNELIAN, w, h);
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

    if (cb->show_panel) { // heridas propias y de la tribu
        const int pw = 340, x0 = 8, y0 = 162;
        char lines[14][112];
        int n = 0;
        snprintf(lines[n++], sizeof(lines[0]), "Tú: %s (vida %d/%d, sangre %d%%)", health_state_name(ph),
                 (int)fmaxf(0.0f, ph->hp), (int)ph->hp_max, (int)(ph->blood * 100));
        for (int i = 0; i < ph->wound_count && n < 6; i++) {
            char d[96];
            wound_describe(&ph->wounds[i], d, sizeof(d));
            snprintf(lines[n++], sizeof(lines[0]), "  %s", d);
        }
        for (int k = 0; k < troop->count && n < 13; k++) {
            const Member *m = &troop->members[k];
            if (m->status != STATUS_ACTIVE || (m->health.wound_count == 0 && !m->health.down)) continue;
            int worst = health_worst(&m->health);
            char d[96] = "";
            if (worst >= 0) wound_describe(&m->health.wounds[worst], d, sizeof(d));
            snprintf(lines[n++], sizeof(lines[0]), "%s: %s%s%s", m->name, health_state_name(&m->health), d[0] ? " · " : "", d);
        }
        int ph_h = 2 * UI_PANEL_INSET + 30 + 11 * n;
        ui_panel((Rectangle){ (float)x0, (float)y0, (float)pw, (float)ph_h }, UI_METAL_SILVER);
        int x = x0 + UI_PANEL_INSET + 2, yy = y0 + UI_PANEL_INSET + 2;
        ui_text(TextFormat("Salud (P)  ·  B vendar  ·  hierbas: %d", stock_count(&ga->stock, HERBS_ID)), x, yy, 10,
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
