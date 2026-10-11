#include "combat_game.h"

#include "game/death_game.h"
#include "game/input.h"
#include "game/hud_game.h"
#include "game/apparel_game.h"
#include "game/water_game.h"
#include "game/fauna_game.h"
#include "game/inventory_game.h"
#include "game/gems_game.h"
#include "game/talents_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"
#include "sim/anim_index.h"
#include "sim/body.h"
#include "sim/hazards.h"
#include "sim/melee.h"
#include "sim/stealth.h"
#include "sim/targeting.h"
#include "ui/theme.h"
#include "world/body_draw.h"
#include "sim/lang.h"

#define HERBS_ID "utileria.consumible.hierbas"
#define CAMP_RADIUS 14.0f
#define DOWN_WAKE_SECONDS 25.0f // abatido y solo: muere y vuelve a su lugar de descanso
#define DOWN_HELP_SECONDS 5.0f  // con la escolta cerca: lo levantan
#define CORPSE_SECONDS 30.0f
#define HOSTAGE_AHEAD 0.55f    // m delante del jugador
#define HOSTAGE_SPEED 0.55f    // el jugador, con un rehen, va mas lento
#define HOSTAGE_KEEP_AWAY 4.5f // m: los suyos no se acercan mas
#define HOSTAGE_JOLT 12.0f     // un golpe que pasa de esto suelta al rehen
#define DESPAWN_DIST 160.0f
#define COMPANION_DAMAGE 10.0f
#define COMPANION_COOLDOWN 1.2f
#define SHOT_LIFE 8.0f        // segundos de vuelo como maximo
#define STUCK_SECONDS 20.0f   // una flecha clavada se ve un rato
#define EYE_HEIGHT 1.45f      // de donde sale el disparo
#define ARROW_LIT_SECONDS 25.0f // lo que dura encendida una flecha antes de dispararla
#define FIRE_ARROW_BURN 7.0f  // daño de quemadura extra de una flecha encendida
#define MOUNT_REACH 0.9f      // a caballo se llega mas lejos
#define MOUNT_WEIGHT 80.0f    // kg que suma la montura contra derribos
#define GALLOP_SPEED 6.0f     // m/s: por encima, la lanza derriba y se arrolla
#define RIDER_LIFT 1.1f       // altura del jinete sobre el suelo
#define LOOT_MAX 16

static float dist2(Vector3 a, Vector3 b) { return Vector2Distance((Vector2){ a.x, a.z }, (Vector2){ b.x, b.z }); }

// Lo que piden las casillas de combate del HUD (se atiende en el proximo cb_update).
static bool g_hud_attack_press, g_hud_attack_down, g_hud_block, g_hud_parry, g_hud_charge, g_hud_grab;
static int g_back; // el enemigo con la espalda ganada + 1 (la daga sobre el; 0: ninguno)
static bool g_click_used; // ese clic eligio un objetivo: no es un golpe

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

bool cb_blocks_input(const Combat *cb) { return cb->player.down || cb->knock > 0.0f; } // abatido o derribado

float cb_speed_scale(const Combat *cb) {
    float s = health_speed_scale(&cb->player) * armor_speed_scale(&cb->armor);
    if (cb->charge_timer > 0.0f) return s * 1.5f; // la carga con escudo, a toda carrera
    if (cb->blocking || cb->aiming) s *= 0.5f;
    if (cb->stagger > 0.0f) s *= 0.6f;
    if (cb->hostage) s *= HOSTAGE_SPEED; // con un rehen a cuestas
    return cb->player.down ? 1.0f : s;
}

static float surface_y(const Terrain *t, float x, float z) {
    float ground = terrain_height(t, x, z), water = terrain_water(t, x, z);
    if (water > ground + 0.25f) return hazard_ice_walkable(terrain_ice(t, x, z)) ? water : fmaxf(ground, water - 1.25f);
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
        e->armed = true;
        e->mounted = def->mounted;
        e->shield = def->shield && rng_float(&cb->rng) < def->shield_chance;
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
    if (strcmp(what, "bandidos") && strcmp(what, "culto") && strcmp(what, "arqueros") && strcmp(what, "jinetes") &&
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
        snprintf(log, len, "%s", T("¡Fanáticos del culto! Buscan prisioneros para el sacrificio."));
    } else if (!strcmp(what, "arqueros")) {
        n = 2;
        kinds[0] = kinds[1] = ENEMY_ARCHER;
        snprintf(log, len, "%s", T("¡Arqueros! Busca cubrirte o acércate rápido."));
    } else if (!strcmp(what, "jinetes")) {
        n = 2 + rng_range(&cb->rng, 2);
        for (int i = 0; i < n; i++) kinds[i] = ENEMY_RIDER;
        snprintf(log, len, "%s", T("¡Jinetes bandidos al galope! Derríbalos del caballo."));
    } else {
        n = 2 + rng_range(&cb->rng, 2);
        for (int i = 0; i < n; i++) kinds[i] = ENEMY_BANDIT;
        if (rng_float(&cb->rng) < 0.6f) kinds[n++] = ENEMY_ARCHER; // a veces con un arquero detras
        else if (rng_float(&cb->rng) < 0.5f) kinds[n++] = ENEMY_RIDER; // o un jinete
        snprintf(log, len, "%s", T("¡Bandidos! Vienen a por tu botín."));
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
    if (!night && r < 0.12f) {
        float k = rng_float(&cb->rng);
        cb_spawn_group(cb, ga, k < 0.25f ? "culto" : k < 0.4f ? "jinetes" : "bandidos", p, t, k < 0.4f && k >= 0.25f ? 60.0f : 35.0f,
                       log, len);
    }
}

// ------------------------------------------------------------------- golpes
static void describe_hit(const Health *h, int w, float absorbed, bool broke, char *out, size_t len) {
    if (w < 0) {
        snprintf(out, len, T("la armadura para el golpe%s"), broke ? T(" y se rompe") : "");
        return;
    }
    char d[96];
    wound_describe(&h->wounds[w], h->beast, d, sizeof(d));
    lower_first(d);
    snprintf(out, len, "%s%s%s", d, absorbed > 0.5f ? T(" (la armadura amortigua)") : "", broke ? T("; se rompe la pieza") : "");
}

// En pie: ni muerto, ni abatido, ni rehen (lo que pelea, se elige y se golpea).
static bool standing(const Enemy *e) {
    return e->used && e->state != EN_DEAD && e->state != EN_DOWN && e->state != EN_HOSTAGE;
}

// El jinete cae del caballo, que queda suelto (la fauna lo suelta en el mundo).
static void unhorse(Combat *cb, Enemy *e) {
    e->mounted = false;
    e->kind = ENEMY_BANDIT;
    if (cb->loose_n < 4) cb->loose_yaw[cb->loose_n] = e->yaw, cb->loose_horse[cb->loose_n++] = e->pos;
}

// Un enemigo herido cae: muerto si lo que lo tumbo es letal (o se desangro); si no, abatido,
// en el suelo hasta que despierta a los 30-90 s (src/sim/stealth.h).
static void enemy_down_check(Combat *cb, Enemy *e) {
    if (e->state == EN_DEAD || !(e->h.down || e->h.dead)) return;
    e->windup = 0.0f, e->windup_move = 0;
    e->blocking = false;
    if (cb->hostage == (int)(e - cb->enemies) + 1) cb->hostage = 0; // el rehen cae: se suelta
    if (e->mounted) unhorse(cb, e);
    if (e->h.dead || down_is_lethal(&e->h)) {
        e->state = EN_DEAD;
        e->corpse = CORPSE_SECONDS;
    } else if (e->state != EN_DOWN) {
        e->state = EN_DOWN;
        e->timer = down_wake_seconds(&cb->rng);
    }
}

// Los de cerca se enteran: van a por el jugador un rato aunque no lo vean.
static void alert_near(Combat *cb, Vector3 at, float radius, float seconds) {
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        Enemy *o = &cb->enemies[i];
        if (!standing(o) || dist2(o->pos, at) > radius) continue;
        o->alert = fmaxf(o->alert, seconds);
        o->shown = 6.0f;
    }
}

// El abatido mas cercano a menos de reach (o NULL).
static Enemy *downed_near(Combat *cb, Vector3 pos, float reach) {
    Enemy *best = NULL;
    float bd = reach;
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        Enemy *e = &cb->enemies[i];
        if (!e->used || e->state != EN_DOWN) continue;
        float d = dist2(pos, e->pos);
        if (d < bd) bd = d, best = e;
    }
    return best;
}

// ------------------------------------------------------------------- la espalda y los rehenes

static bool g_jolt; // el jugador recibio un golpe fuerte en este cuadro (suelta al rehen)

// Los del mismo bando: el culto (fanaticos y captores) o los bandidos (a pie, arqueros, jinetes).
static bool cult(EnemyKind k) { return k == ENEMY_FANATIC || k == ENEMY_CAPTOR; }
static bool same_side(EnemyKind a, EnemyKind b) { return cult(a) == cult(b); }

static Enemy *hostage_of(Combat *cb) {
    int i = cb->hostage - 1;
    return i >= 0 && i < CB_MAX_ENEMIES && cb->enemies[i].used && cb->enemies[i].state == EN_HOSTAGE ? &cb->enemies[i] : NULL;
}

// La espalda ganada: el enemigo en pie mas cercano con el jugador en su cono trasero, a tiro, que no
// lo noto (o esta aturdido). A caballo no se le gana (src/sim/stealth.h).
static Enemy *back_open(Combat *cb, const Player *p) {
    Enemy *best = NULL;
    float bd = BACKSTAB_RANGE;
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        Enemy *e = &cb->enemies[i];
        if (!standing(e) || e->mounted) continue;
        float d = dist2(e->pos, p->pos);
        bool noticed = e->target == 0, stunned = e->stagger > 0.0f || e->knock > 0.0f;
        if (d < bd && stealth_backstab(e->pos.x, e->pos.z, e->yaw, p->pos.x, p->pos.z, noticed, stunned)) bd = d, best = e;
    }
    return best;
}

// Suelta al rehen (G, un golpe fuerte, o al caer). push: L lo empuja y cae al suelo.
static void release_hostage(Combat *cb, const Player *p, bool push, char *log, size_t len) {
    Enemy *e = hostage_of(cb);
    cb->hostage = 0;
    if (!e) return;
    e->state = EN_CHASE;
    e->target = 0;
    e->cooldown = 1.0f;
    char who[48];
    lower_name(T(enemy_def(e->kind)->name), who, sizeof(who));
    if (push) {
        e->pos.x += sinf(p->yaw) * 1.2f, e->pos.z += cosf(p->yaw) * 1.2f;
        e->knock = KNOCKDOWN_SECONDS;
        snprintf(log, len, T("Empujas al %s: cae de bruces."), who);
    } else {
        e->stagger = STAGGER_SECONDS;
        if (log) snprintf(log, len, T("Sueltas al %s."), who);
    }
}

// K con la espalda ganada (o con el rehen): lo mata en el acto. Por la espalda, solo lo oyen los que
// estan a menos de SILENT_ALERT_RANGE m; al rehen lo ven ejecutar todos los suyos que miran.
static void player_backstab(Combat *cb, Player *p, Enemy *e, char *log, size_t len) {
    bool hostage = e->state == EN_HOSTAGE;
    if (hostage) cb->hostage = 0;
    p->yaw = atan2f(e->pos.x - p->pos.x, e->pos.z - p->pos.z);
    cb->move = MOVE_BACKSTAB + 1;
    cb->move_anim = 0.9f;
    cb->attack_anim = 0.45f;
    cb->attack_cd = move_def(MOVE_BACKSTAB)->recovery;
    e->h.down = e->h.dead = true;
    e->state = EN_DEAD;
    e->corpse = CORPSE_SECONDS;
    e->windup = 0.0f, e->windup_move = 0;
    int heard = 0;
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        Enemy *o = &cb->enemies[i];
        if (!standing(o)) continue;
        float d = dist2(o->pos, e->pos);
        if (stealth_hears_kill(d) || (hostage && same_side(o->kind, e->kind) && d < 25.0f)) o->alert = 15.0f, o->shown = 6.0f, heard++;
    }
    char who[48];
    lower_name(T(enemy_def(e->kind)->name), who, sizeof(who));
    if (hostage) snprintf(log, len, T("Ejecutas al %s delante de los suyos."), who);
    else if (heard) snprintf(log, len, T("Por la espalda: el %s cae sin un grito... pero alguien lo oyó."), who);
    else snprintf(log, len, T("Por la espalda: el %s cae sin un grito."), who);
}

// G con la espalda ganada: lo agarra por el cuello y lo usa de escudo.
static void take_hostage(Combat *cb, Player *p, Enemy *e, char *log, size_t len) {
    cb->hostage = (int)(e - cb->enemies) + 1;
    if (cb->target_lock == cb->hostage) cb->target_lock = 0;
    e->state = EN_HOSTAGE;
    e->target = 0;
    e->windup = 0.0f, e->windup_move = 0, e->attack_anim = 0.0f;
    e->blocking = false, e->block_timer = 0.0f;
    cb->blocking = false, cb->aiming = false, cb->draw = 0.0f, cb->v_hold = 0.0f;
    p->yaw = atan2f(e->pos.x - p->pos.x, e->pos.z - p->pos.z);
    char who[48];
    snprintf(log, len, T("Agarras al %s por el cuello: es tu rehén. G lo suelta, K lo ejecuta, L lo empuja."),
             lower_name(T(enemy_def(e->kind)->name), who, sizeof(who)));
}

// ------------------------------------------------------------------- cuerpo a cuerpo
// Las reglas estan en src/sim/melee.h; aqui se arma quien pelea y se aplica el resultado.
static float facing_to(Vector3 pos, float yaw, Vector3 from) {
    Vector3 fwd = { sinf(yaw), 0, cosf(yaw) };
    Vector3 to = Vector3Normalize((Vector3){ from.x - pos.x, 0, from.z - pos.z });
    return Vector3DotProduct(fwd, to);
}

static float armor_kg(const Armor *a) { return (1.0f - armor_speed_scale(a)) * 200.0f; }

static bool player_has_shield(const GameActions *ga) {
    return !ga->hands.sheathed && ga->hands.left.kind == INV_HANDS_SHIELD && ga->hands.left.id[0];
}

static Fighter fighter_player(const Combat *cb, const Player *p, const GameActions *ga, Vector3 from) {
    Fighter f = { 0 };
    f.shield = player_has_shield(ga);
    f.blocking = cb->blocking || cb->charge_timer > 0.0f;
    f.down = cb->player.down || cb->knock > 0.0f;
    f.staggered = cb->stagger > 0.0f || cb->exposed > 0.0f; // expuesto tras un parry a destiempo: sin guardia
    f.attacking = cb->attack_anim > 0.0f || cb->v_hold > 0.2f;
    f.armed = hands_attack_weapon(&ga->hands, 1, NULL)[0] != '\0';
    f.facing = facing_to(p->pos, p->yaw, from);
    f.strength = 1.0f;
    f.weight = armor_kg(&cb->armor) + (ga->mounted >= 0 ? MOUNT_WEIGHT : 0.0f);
    f.health = fmaxf(0.0f, cb->player.hp / cb->player.hp_max);
    return f;
}

static const char *enemy_weapon(const Enemy *e) { return e->armed ? enemy_def(e->kind)->weapon : ""; }

static Fighter fighter_enemy(const Enemy *e, Vector3 from) {
    const EnemyDef *def = enemy_def(e->kind);
    Fighter f = { 0 };
    f.shield = e->shield;
    f.blocking = e->blocking;
    f.down = e->knock > 0.0f || e->state == EN_DEAD || e->state == EN_DOWN;
    f.staggered = e->stagger > 0.0f;
    f.attacking = e->attack_anim > 0.0f || e->windup > 0.0f;
    f.armed = e->armed;
    f.facing = facing_to(e->pos, e->yaw, from);
    // Fuerza: su daño frente al de su arma (un bandido curtido pega mas con el mismo sable).
    float wd = weapon_stats(def->weapon).damage;
    f.strength = wd > 0.0f ? def->damage / wd : 1.0f;
    f.weight = armor_kg(&e->armor) + (e->mounted ? MOUNT_WEIGHT : 0.0f);
    f.health = fmaxf(0.0f, e->h.hp / e->h.hp_max);
    return f;
}

static const char *member_weapon(const Member *m) { return m->champion >= 0 ? "arma.larga.guja" : "arma.corta.sable"; }

static Fighter fighter_member(const Npc *n, const Member *m, const Troop *troop, Vector3 from) {
    const Champion *c = troop_champion(troop, m->id);
    Fighter f = { 0 };
    f.down = m->health.down || n->knock > 0.0f;
    f.staggered = n->stagger > 0.0f;
    f.attacking = n->fight_anim > 0.0f;
    f.armed = true;
    f.facing = facing_to(n->pos, n->yaw, from);
    f.strength = (c ? c->stats.strength : 1.0f) * COMPANION_DAMAGE / weapon_stats("arma.corta.sable").damage;
    f.weight = armor_kg(&m->armor);
    f.health = fmaxf(0.0f, m->health.hp / m->health.hp_max);
    return f;
}

// Que movimiento elige un luchador de la IA contra su rival (enemigos y escolta).
static MeleeMove ai_choose(Rng *rng, const Fighter *self, const Fighter *foe, const char *weapon, bool running, float dist) {
    float r = rng_float(rng);
    if (foe->down) return r < 0.5f ? MOVE_HEAVY : MOVE_LIGHT; // en el suelo: a rematar
    if (foe->blocking && foe->shield) { // contra un escudo en guardia: romperla
        if (running && r < 0.5f) return MOVE_RUN_KICK;
        if (r < 0.3f) return MOVE_KICK;
        if (r < 0.5f && dist < 1.3f) return MOVE_GRAPPLE;
        if (r < 0.75f && weapon_can_hook(weapon)) return MOVE_HOOK;
        return MOVE_HEAVY;
    }
    if (foe->blocking) { // contra una guardia con el arma: patadas y golpes pesados que la cansan
        if (r < 0.3f) return running ? MOVE_RUN_KICK : MOVE_KICK;
        if (r < 0.55f) return MOVE_HEAVY;
        if (r < 0.65f && dist < 1.3f) return MOVE_GRAPPLE;
        return MOVE_LIGHT;
    }
    if (self->shield && r < 0.1f) return MOVE_SHIELD_BASH;
    if (r < 0.14f) return MOVE_HEAVY;
    if (r < 0.22f) return MOVE_KICK;
    if (r < 0.27f && dist < 1.3f) return MOVE_GRAPPLE;
    return MOVE_LIGHT;
}

static const char *move_verb(MeleeMove m) {
    switch (m) {
    case MOVE_KICK: return T("Patada");
    case MOVE_RUN_KICK: return T("Patada a la carrera");
    case MOVE_SHIELD_BASH: return T("Golpe de escudo");
    case MOVE_SHIELD_CHARGE: return T("Carga con escudo");
    case MOVE_GRAPPLE: return T("Agarre");
    case MOVE_HOOK: return T("Gancho");
    case MOVE_HEAVY: return T("Golpe pesado");
    default: return T("Golpe");
    }
}

static void drop_item(Props *props, const char *id, Vector3 at) {
    if (props && id && id[0]) props_add(props, id, (Vector3){ at.x + 0.6f, at.y, at.z + 0.3f }, 0.7f);
}

// Aplica el resultado a un enemigo. attacker: 0 jugador (escribe el registro), >0 integrante.
static void hit_enemy(Combat *cb, Enemy *e, MeleeMove m, const MeleeResult *r, Props *props, int attacker, char *log,
                      size_t len) {
    const EnemyDef *def = enemy_def(e->kind);
    float absorbed = 0.0f;
    bool broke = false;
    int wi = -1;
    if (r->damage > 0.5f) wi = combat_apply_hit(&e->h, &e->armor, &cb->rng, r->damage, r->wound, PART_RANDOM, false, &absorbed, &broke);
    if (r->landed) e->hit_anim = 0.3f, e->shown = 6.0f;
    if (r->staggered) e->stagger = STAGGER_SECONDS, e->blocking = false;
    if (r->knocked_down) e->knock = KNOCKDOWN_SECONDS, e->blocking = false;
    bool unhorsed = false;
    if (r->knocked_down && e->mounted) unhorse(cb, e), unhorsed = true; // derribado: cae del caballo, que queda suelto
    if (r->shield_dropped && e->shield) e->shield = false, drop_item(props, def->shield, e->pos);
    if (r->disarmed && e->armed) e->armed = false, drop_item(props, def->weapon, e->pos);
    if (e->target < 0 && attacker == 0) e->target = 0; // ahora sabe donde estas
    enemy_down_check(cb, e);
    if (attacker != 0 || !log) return;
    char who[48], d[128];
    lower_name(T(def->name), who, sizeof(who));
    if (e->state == EN_DEAD) snprintf(log, len, T("Matas al %s: deja botín (F para recogerlo)."), who);
    else if (e->state == EN_DOWN) snprintf(log, len, T("El %s cae abatido: F lo toma prisionero; H lo remata."), who);
    else if (unhorsed) snprintf(log, len, T("%s: ¡derribas al %s del caballo!"), move_verb(m), who);
    else if (r->attacker_staggered && m == MOVE_GRAPPLE) snprintf(log, len, T("El %s se zafa del agarre: ¡quedas expuesto!"), who);
    else if (!r->landed && m == MOVE_HOOK) snprintf(log, len, "%s", T("Tu arma no tiene gancho (hacha, guja o alabarda)."));
    else if (!r->landed) snprintf(log, len, "%s", T("Sin escudo no puedes hacer eso."));
    else if (r->shield_dropped) snprintf(log, len, T("%s: ¡le arrancas el escudo al %s!"), move_verb(m), who);
    else if (r->disarmed) snprintf(log, len, T("Le agarras el brazo armado y lo desarmas con una llave: ¡el %s cae!"), who);
    else if (m == MOVE_DEFLECT) snprintf(log, len, T("Desvías el golpe del %s: ¡queda abierto!"), who);
    else if (r->knocked_down) snprintf(log, len, T("%s: ¡el %s cae al suelo!"), move_verb(m), who);
    else if (m == MOVE_HEAVY && r->blocked) snprintf(log, len, T("Tu golpe pesado rompe la guardia del %s."), who);
    else if (r->blocked && r->attacker_staggered) snprintf(log, len, "%s", T("Escudo contra escudo: los dos os tambaleáis."));
    else if (r->blocked) snprintf(log, len, T("El %s para tu %s%s."), who, m == MOVE_KICK ? T("patada con el escudo: pierde la guardia") : T("golpe"),
                                  e->shield ? "" : T(" con el arma"));
    else if (m == MOVE_HOOK) snprintf(log, len, T("No logras arrancarle el escudo al %s."), who);
    else if (wi >= 0) describe_hit(&e->h, wi, absorbed, broke, d, sizeof(d)), snprintf(log, len, "%s: %s.", move_verb(m), d);
    else if (r->staggered) snprintf(log, len, T("%s: el %s se tambalea."), move_verb(m), who);
}

static void hit_player(Combat *cb, GameActions *ga, MeleeMove m, const MeleeResult *r, const char *who, char *log,
                       size_t len) {
    float absorbed = 0.0f;
    bool broke = false;
    int wi = -1;
    if (r->damage > 0.5f)
        wi = combat_apply_hit(&cb->player, &cb->armor, &cb->rng, r->damage, r->wound, PART_RANDOM, false, &absorbed, &broke);
    if (r->landed) cb->hit_anim = 0.3f;
    if (r->staggered) cb->stagger = STAGGER_SECONDS;
    if (r->knocked_down) cb->knock = KNOCKDOWN_SECONDS, cb->charge_timer = 0.0f;
    if (r->guard_cost > 0.0f) player_stamina_spend(r->guard_cost); // cubrirse cansa (con el arma, mas)
    if (r->damage >= HOSTAGE_JOLT || r->knocked_down || r->staggered) g_jolt = true; // un golpe fuerte suelta al rehen
    bool unhorsed = r->knocked_down && ga->mounted >= 0;
    if (unhorsed) ga->animals[ga->mounted].ridden = false, ga->mounted = -1; // te tira del caballo
    // Lo que se suelta queda en la mano de la tribu: la barra rapida (1 a 9) lo vuelve a empuñar.
    bool had_shield = player_has_shield(ga);
    if (r->shield_dropped && had_shield) hands_clear(&ga->hands, HAND_LEFT);
    if (r->disarmed && ga->hands.right.id[0]) hands_clear(&ga->hands, HAND_RIGHT);
    char name[48], d[128];
    lower_name(who, name, sizeof(name));
    if (r->shield_dropped) snprintf(log, len, T("¡El %s te arranca el escudo! (1 a 9 para volver a empuñar)"), name);
    else if (r->disarmed) snprintf(log, len, T("¡El %s te desarma con una llave! (1 a 9 para volver a empuñar)"), name);
    else if (unhorsed) snprintf(log, len, T("%s del %s: ¡te tira del caballo!"), move_verb(m), name);
    else if (r->knocked_down) snprintf(log, len, T("%s del %s: ¡te derriba!"), move_verb(m), name);
    else if (r->blocked && r->staggered) snprintf(log, len, T("%s del %s contra tu guardia: la pierdes."), move_verb(m), name);
    else if (r->blocked && !had_shield) snprintf(log, len, T("Paras con el arma el golpe del %s: aguanta la mitad y cansa."), name);
    else if (r->blocked) snprintf(log, len, T("Paras el golpe del %s."), name);
    else if (r->attacker_staggered) snprintf(log, len, T("Te zafas del agarre del %s."), name);
    else if (wi >= 0) describe_hit(&cb->player, wi, absorbed, broke, d, sizeof(d)), snprintf(log, len, T("Te hieren: %s."), d);
}

static void hit_member(Combat *cb, GameActions *ga, Troop *troop, Member *m, const MeleeResult *r, char *log, size_t len) {
    if (r->damage > 0.5f) combat_apply_hit(&m->health, &m->armor, &cb->rng, r->damage, r->wound, PART_RANDOM, false, NULL, NULL);
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        Npc *n = &ga->npcs[i];
        if (n->member_id != m->id) continue;
        if (r->landed) n->hurt_anim = 0.3f;
        if (r->staggered) n->stagger = STAGGER_SECONDS;
        if (r->knocked_down) n->knock = KNOCKDOWN_SECONDS;
    }
    if (m->health.down) snprintf(log, len, T("¡%s cae herido! Acércate y pulsa B para levantarlo."), m->name);
}

// El enemigo vivo mas cercano delante de pos, a menos de reach.
static Enemy *enemy_in_front(Combat *cb, Vector3 pos, float yaw, float reach, float *dist) {
    Enemy *best = NULL;
    float bd = reach;
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        Enemy *e = &cb->enemies[i];
        if (!standing(e)) continue;
        float d = dist2(pos, e->pos);
        if (d < bd && (d < 0.8f || facing_to(pos, yaw, e->pos) > 0.3f)) bd = d, best = e;
    }
    if (dist) *dist = bd;
    return best;
}

// ------------------------------------------------------------------- objetivo
static Enemy *locked(Combat *cb) {
    int i = cb->target_lock - 1;
    return i >= 0 && i < CB_MAX_ENEMIES && standing(&cb->enemies[i]) ? &cb->enemies[i] : NULL;
}

const Enemy *cb_target(const Combat *cb) { return locked((Combat *)cb); }

static int target_cands(const Combat *cb, TargetCand *c) {
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        const Enemy *e = &cb->enemies[i];
        c[i] = (TargetCand){ e->pos.x, e->pos.z, standing(e) };
    }
    return CB_MAX_ENEMIES;
}

// Tab: el siguiente (dir 1) o el anterior (-1). Se pierde a TARGET_RANGE o si muere.
void cb_cycle_target(Combat *cb, const Player *p, int dir, char *log, size_t len) {
    TargetCand c[CB_MAX_ENEMIES];
    int n = target_cands(cb, c);
    int t = target_cycle(c, n, p->pos.x, p->pos.z, p->yaw, cb->target_lock - 1, dir);
    cb->target_lock = t + 1;
    char who[48];
    if (t >= 0) snprintf(log, len, T("Objetivo: %s."), lower_name(T(enemy_def(cb->enemies[t].kind)->name), who, sizeof(who)));
}

static void keep_target(Combat *cb, const Player *p) {
    TargetCand c[CB_MAX_ENEMIES];
    int n = target_cands(cb, c);
    if (cb->target_lock && !target_keep(c, n, cb->target_lock - 1, p->pos.x, p->pos.z)) cb->target_lock = 0;
}

bool cb_pick_target(Combat *cb, Camera3D cam, Vector2 pointer, int w, int h, char *log, size_t len) {
    int best = -1;
    float bd = 18.0f; // px de la pantalla virtual
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        const Enemy *e = &cb->enemies[i];
        if (!standing(e)) continue;
        Vector3 torso = { e->pos.x, e->pos.y + (e->mounted ? 2.1f : 1.1f), e->pos.z };
        Vector3 to = Vector3Subtract(torso, cam.position);
        if (Vector3DotProduct(to, Vector3Subtract(cam.target, cam.position)) <= 0.0f || Vector3Length(to) > TARGET_RANGE + 12.0f) continue;
        Vector2 s = GetWorldToScreenEx(torso, cam, w, h);
        float d = Vector2Distance(s, pointer);
        if (d < bd) bd = d, best = i;
    }
    if (best < 0 || best == cb->target_lock - 1) return false; // sobre el elegido: ese clic es un golpe
    cb->target_lock = best + 1;
    g_click_used = true;
    char who[48];
    snprintf(log, len, T("Objetivo: %s."), lower_name(T(enemy_def(cb->enemies[best].kind)->name), who, sizeof(who)));
    return true;
}

// A quien va el golpe: el elegido si esta a tiro (y el jugador se vuelve hacia el), o el mas
// cercano delante.
static Enemy *melee_target(Combat *cb, Player *p, float reach, float *dist) {
    Enemy *e = locked(cb);
    if (e && dist2(p->pos, e->pos) < reach) {
        p->yaw = atan2f(e->pos.x - p->pos.x, e->pos.z - p->pos.z);
        if (dist) *dist = dist2(p->pos, e->pos);
        return e;
    }
    return enemy_in_front(cb, p->pos, p->yaw, reach, dist);
}

// El jugador hace un movimiento: contra el enemigo de delante, o un animal, un nido o un pez.
static void player_move(Combat *cb, Player *p, GameActions *ga, const Terrain *t, Props *props, MeleeMove m, char *log,
                        size_t len) {
    int step = 1;
    if (m == MOVE_LIGHT) {
        step = cb->combo_timer > 0.0f ? cb->combo % 3 + 1 : 1;
        cb->combo = step;
    }
    bool off = false;
    const char *weapon = hands_attack_weapon(&ga->hands, step, &off);
    WeaponStats w = weapon_stats(weapon);
    const MoveDef *md = move_def(m);
    float reach = md->reach > 0.0f ? md->reach : w.reach;
    bool riding = ga->mounted >= 0;
    float momentum = riding ? mounted_momentum(cb->pl_speed) : 1.0f; // la inercia del galope
    if (riding) reach += MOUNT_REACH;
    cb->attack_cd = m == MOVE_LIGHT ? melee_combo_cooldown(weapon, w.cooldown, step) : m == MOVE_HEAVY ? w.cooldown * 1.6f
                                                                                                     : md->recovery;
    cb->combo_timer = m == MOVE_LIGHT ? cb->attack_cd + COMBO_WINDOW : 0.0f;
    cb->move = (int)m + 1;
    if (!riding) player_stamina_spend(m == MOVE_LIGHT ? 0.04f : 0.09f); // cada golpe cansa; los fuertes, mas
    cb->move_anim = 0.45f;
    if (m == MOVE_LIGHT || m == MOVE_HEAVY) cb->attack_anim = 0.4f;
    ga->pl_spear = w.spear;
    float ed;
    Enemy *e = melee_target(cb, p, reach + 0.6f, &ed);
    // Un animal mas cerca que el enemigo: el golpe (o la patada) es para el.
    float ad;
    int an = m == MOVE_GRAPPLE || m == MOVE_HOOK || m == MOVE_SHIELD_BASH ? -1 : fg_melee_target(ga, t, p->pos, p->yaw, reach, &ad);
    if (an >= 0 && (!e || ad < ed)) {
        float base = m == MOVE_LIGHT || m == MOVE_HEAVY ? w.damage * (m == MOVE_HEAVY ? 1.8f : melee_combo_scale(step)) : md->damage;
        float dmg = combat_damage(base, (off ? 0.8f : 1.0f) * momentum * (1.0f + ig_stat(ga, STAT_MELEE)) * (1.0f - 0.35f * wg_clumsy(ga)), health_attack_scale(&cb->player),
                                  false, &cb->rng);
        fg_hurt(ga, an, dmg, m == MOVE_LIGHT || m == MOVE_HEAVY ? w.wound : WOUND_BRUISE, PART_RANDOM, 0, log, len);
        return;
    }
    if (!e) {
        if (m != MOVE_LIGHT && m != MOVE_HEAVY) return;
        // Nada a quien golpear: un nido que se enfada o un pez en el agua.
        float rock_dmg = (m == MOVE_HEAVY ? 1.8f : 1.0f) * w.damage * (w.wound == WOUND_BRUISE ? 1.6f : 1.0f); // la maza parte mejor
        if (gems_hit_rock(ga, props, p, rock_dmg, w.reach, log, len)) return;
        if (fg_poke_nest(ga, p->pos, w.reach)) snprintf(log, len, "%s", T("¡Golpeaste el nido! Ahí vienen..."));
        else fg_fish(ga, t, p->pos, p->yaw, w.reach, w.spear, log, len);
        return;
    }
    Fighter me = fighter_player(cb, p, ga, e->pos), foe = fighter_enemy(e, p->pos);
    me.strength *= health_attack_scale(&cb->player) * (off ? 0.8f : 1.0f);
    if (m == MOVE_GRAPPLE) me.strength *= 1.0f + ig_stat(ga, STAT_GRAPPLE_POWER); // amuletos del tigre y del oso
    me.strength *= momentum * (1.0f + ig_stat(ga, STAT_MELEE)) * (1.0f - 0.35f * wg_clumsy(ga)); // tatuajes, joyas, el aullido; borracho, menos
    MeleeResult r = melee_resolve(m, &me, &foe, weapon, step, &cb->rng);
    // La lanza al galope: el que la recibe (sin pararla) suele caer.
    if (riding && w.spear && cb->pl_speed > GALLOP_SPEED && r.landed && !r.blocked && rng_float(&cb->rng) < 0.6f)
        r.knocked_down = true;
    if (r.attacker_staggered) cb->stagger = STAGGER_SECONDS;
    hit_enemy(cb, e, m, &r, props, 0, log, len);
}

// Quien recibe el golpe de un enemigo: el jugador (target 0) o un integrante de la escolta.
// false si ya no esta (o ya no pelea).
static bool enemy_foe(Combat *cb, Enemy *e, Player *p, GameActions *ga, Troop *troop, Fighter *foe, Member **m, Npc **n,
                      Vector3 *at) {
    *m = NULL, *n = NULL;
    if (e->target == 0) {
        *foe = fighter_player(cb, p, ga, e->pos);
        *at = p->pos;
        return !cb->player.down;
    }
    *m = troop_find(troop, e->target);
    if (!*m || (*m)->status != STATUS_ACTIVE) return false;
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
        if (ga->npcs[i].member_id == (*m)->id) *n = &ga->npcs[i];
    if (!*n) return false;
    *foe = fighter_member(*n, *m, troop, e->pos);
    *at = (*n)->pos;
    return true;
}

// Un enemigo anuncia su golpe: levanta el arma un momento antes de darlo (MELEE_WINDUP). En los
// ultimos PARRY_WINDOW s, el jugador puede pararlo con un parry (K).
static void enemy_windup(Combat *cb, Enemy *e, Player *p, GameActions *ga, Troop *troop, float dist) {
    Fighter me = fighter_enemy(e, p->pos), foe;
    Member *m;
    Npc *n;
    Vector3 at;
    if (!enemy_foe(cb, e, p, ga, troop, &foe, &m, &n, &at)) return;
    if (m) me.facing = facing_to(e->pos, e->yaw, at);
    const char *weapon = enemy_weapon(e);
    MeleeMove mv = ai_choose(&cb->rng, &me, &foe, weapon, e->speed > 3.5f, dist);
    if (e->mounted && mv != MOVE_LIGHT && mv != MOVE_HEAVY) mv = MOVE_HEAVY; // a caballo: solo el arma
    if ((mv == MOVE_SHIELD_BASH && !e->shield) || (mv == MOVE_HOOK && !weapon_can_hook(weapon))) mv = MOVE_HEAVY;
    e->windup_move = (int)mv + 1;
    // El pesado se anuncia mas; las armas cortas, menos.
    e->windup = MELEE_WINDUP * (mv == MOVE_HEAVY ? 1.4f : weapon_is_short(weapon) ? 0.85f : 1.0f);
}

// El golpe anunciado cae sobre quien sea ahora su objetivo, si sigue a tiro.
static void enemy_strike(Combat *cb, Enemy *e, Player *p, GameActions *ga, Troop *troop, Props *props, char *log, size_t len) {
    const EnemyDef *def = enemy_def(e->kind);
    MeleeMove mv = (MeleeMove)(e->windup_move - 1);
    e->windup_move = 0;
    Fighter me = fighter_enemy(e, p->pos), foe;
    Member *m;
    Npc *n;
    Vector3 at;
    if (!enemy_foe(cb, e, p, ga, troop, &foe, &m, &n, &at)) return;
    if (m) me.facing = facing_to(e->pos, e->yaw, at);
    if (dist2(e->pos, at) > def->reach + 0.5f) { // se aparto a tiempo: el golpe da al aire
        e->cooldown = def->cooldown * 0.6f;
        e->attack_anim = 0.45f;
        return;
    }
    const char *weapon = enemy_weapon(e);
    if (mv == MOVE_LIGHT) e->combo = e->combo % 3 + 1;
    me.strength *= health_attack_scale(&e->h) * (e->mounted ? mounted_momentum(e->speed) : 1.0f);
    MeleeResult r = melee_resolve(mv, &me, &foe, weapon, mv == MOVE_LIGHT ? e->combo : 1, &cb->rng);
    // Montado: a veces el golpe se lo lleva el caballo.
    if (!m && ga->mounted >= 0 && r.damage > 0.5f && !r.blocked && rng_float(&cb->rng) < 0.35f) {
        fg_hurt(ga, ga->mounted, r.damage, r.wound, PART_RANDOM, -1, NULL, 0);
        char who[48];
        snprintf(log, len, T("El golpe del %s alcanza a tu montura."), lower_name(T(def->name), who, sizeof(who)));
        e->cooldown = def->cooldown;
        e->attack_anim = 0.45f;
        return;
    }
    WeaponStats w = weapon_stats(weapon);
    e->cooldown = mv == MOVE_LIGHT ? fmaxf(def->cooldown * 0.8f, melee_combo_cooldown(weapon, def->cooldown, e->combo))
                  : mv == MOVE_HEAVY ? def->cooldown * 1.6f
                                     : move_def(mv)->recovery + 0.4f;
    (void)w;
    e->attack_anim = 0.45f;
    e->move = (int)mv + 1;
    e->move_anim = 0.45f;
    if (r.attacker_staggered) e->stagger = STAGGER_SECONDS;
    if (m) hit_member(cb, ga, troop, m, &r, log, len);
    else hit_player(cb, ga, mv, &r, T(def->name), log, len);
    (void)props;
}

// El enemigo cuyo golpe esta por caer sobre el jugador (el que menos le falta), o NULL.
static Enemy *incoming(Combat *cb, const Player *p) {
    Enemy *best = NULL;
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        Enemy *e = &cb->enemies[i];
        if (!standing(e) || e->target != 0 || e->windup <= 0.0f) continue;
        if (dist2(e->pos, p->pos) > enemy_def(e->kind)->reach + 0.6f) continue;
        if (!best || e->windup < best->windup) best = e;
    }
    return best;
}

bool cb_parry_cue(const Combat *cb) {
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        const Enemy *e = &cb->enemies[i];
        if (standing(e) && e->target == 0 && melee_parry_in_window(e->windup)) return true;
    }
    return false;
}

// K: el parry contra el golpe que viene. En la ventana justa le gana al rival segun el arma
// (src/sim/melee.h: engancha el escudo, hace la llave o desvia); a destiempo, expuesto.
static void player_parry(Combat *cb, Player *p, GameActions *ga, Props *props, char *log, size_t len) {
    Enemy *e = incoming(cb, p);
    cb->parry_cd = 0.8f;
    cb->move = MOVE_PARRY + 1;
    cb->move_anim = 0.45f;
    if (!e || !melee_parry_in_window(e->windup)) {
        cb->exposed = PARRY_EXPOSED;
        snprintf(log, len, "%s", e ? T("Parry a destiempo: ¡quedas expuesto!") : T("Parry al aire: nadie te está golpeando. Quedas expuesto."));
        return;
    }
    p->yaw = atan2f(e->pos.x - p->pos.x, e->pos.z - p->pos.z);
    ParryKind k = melee_parry_kind(hands_attack_weapon(&ga->hands, 1, NULL));
    Fighter me = fighter_player(cb, p, ga, e->pos), foe = fighter_enemy(e, p->pos);
    me.strength *= health_attack_scale(&cb->player) * (1.0f + ig_stat(ga, STAT_GRAPPLE_POWER));
    MeleeResult r = melee_parry(k, &me, &foe, &cb->rng);
    // El golpe del rival no llega a caer.
    e->windup = 0.0f, e->windup_move = 0;
    e->cooldown = enemy_def(e->kind)->cooldown;
    MeleeMove mv = k == PARRY_HOOK ? MOVE_HOOK : k == PARRY_GRAPPLE ? MOVE_GRAPPLE : MOVE_DEFLECT;
    cb->move = (int)mv + 1;
    cb->attack_cd = 0.0f; // la contra: el rival queda abierto
    if (r.attacker_staggered) cb->stagger = STAGGER_SECONDS;
    hit_enemy(cb, e, mv, &r, props, 0, log, len);
    if (r.landed && e->state != EN_DEAD && !r.disarmed && !r.shield_dropped && mv != MOVE_DEFLECT) {
        char who[48];
        snprintf(log, len, T("¡Parry! El %s queda abierto."), lower_name(T(enemy_def(e->kind)->name), who, sizeof(who)));
    }
}

// Cuanto pasa de un ataque la guardia del jugador (1: todo). Con escudo, de frente, casi nada; con
// el arma, la mitad (y cansa); contra flechas el arma no sirve.
static float player_guard(Combat *cb, const Player *p, const GameActions *ga, Vector3 from, bool projectile) {
    if (!cb->blocking) return 1.0f;
    float facing = facing_to(p->pos, p->yaw, from);
    if (player_has_shield(ga)) return rng_float(&cb->rng) < combat_block_chance(true, facing) ? (projectile ? 0.1f : 0.15f) : 1.0f;
    if (projectile || facing <= 0.35f) return 1.0f;
    player_stamina_spend(0.1f);
    return 0.5f;
}

void cb_beast_strike(Combat *cb, Player *p, GameActions *ga, Troop *troop, int kind, int id, Vector3 from, float dmg,
                     WoundKind wound, float venom, const char *who, char *log, size_t len) {
    char name[48];
    lower_name(who, name, sizeof(name));
    if (kind == 0) { // el jugador: el escudo y la armadura cuentan
        if (cb->player.down) return;
        float guard = player_guard(cb, p, ga, from, false);
        bool blocked = guard < 1.0f;
        dmg *= guard;
        float absorbed = 0.0f;
        bool broke = false;
        int wi = combat_apply_hit(&cb->player, &cb->armor, &cb->rng, dmg, wound, PART_RANDOM, false, &absorbed, &broke);
        // El veneno entra si la mordedura paso la armadura.
        if (!blocked && wi >= 0) health_poison(&cb->player, venom);
        cb->hit_anim = 0.3f;
        char d[128];
        if (blocked && player_has_shield(ga)) snprintf(log, len, T("Paras el ataque del %s con el escudo."), name);
        else if (blocked) snprintf(log, len, T("Paras con el arma el ataque del %s: aguanta la mitad."), name);
        else if (wi >= 0 && venom > 0.0f) snprintf(log, len, T("¡Te muerde un %s: veneno! Véndate con hierbas (B)."), name);
        else describe_hit(&cb->player, wi, absorbed, broke, d, sizeof(d)), snprintf(log, len, T("Te ataca un %s: %s."), name, d);
        return;
    }
    if (kind == 1) {
        Member *m = troop_find(troop, id);
        if (!m || m->status != STATUS_ACTIVE || m->health.down) return;
        if (combat_apply_hit(&m->health, &m->armor, &cb->rng, dmg, wound, PART_RANDOM, false, NULL, NULL) >= 0)
            health_poison(&m->health, venom);
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
            if (ga->npcs[i].member_id == m->id) ga->npcs[i].hurt_anim = 0.3f;
        if (m->health.down) snprintf(log, len, T("¡Un %s derriba a %s! Acércate y pulsa B."), name, m->name);
        return;
    }
    if (id < 0 || id >= CB_MAX_ENEMIES || !cb->enemies[id].used || cb->enemies[id].state == EN_DEAD) return;
    Enemy *e = &cb->enemies[id];
    combat_apply_hit(&e->h, &e->armor, &cb->rng, dmg, wound, PART_RANDOM, false, NULL, NULL);
    e->hit_anim = 0.3f;
    e->shown = 6.0f;
    EnemyState before = e->state;
    enemy_down_check(cb, e);
    if (e->state != before && (e->state == EN_DEAD || e->state == EN_DOWN) && dist2(e->pos, p->pos) < 40.0f) {
        char en[48];
        snprintf(log, len, e->state == EN_DEAD ? T("Un %s mata a un %s.") : T("Un %s tumba a un %s."), name,
                 lower_name(T(enemy_def(e->kind)->name), en, sizeof(en)));
    }
}

// ------------------------------------------------------------------- disparos
static Shot *new_shot(Combat *cb) {
    for (int i = 0; i < CB_MAX_SHOTS; i++)
        if (!cb->shots[i].p.alive) return &cb->shots[i];
    return NULL;
}

static void fire(Combat *cb, const RangedDef *rd, Vector3 from, float yaw, float pitch, float charge, int owner, bool burning,
                 float steady) {
    Shot *s = new_shot(cb);
    if (!s) return;
    float spread = rd->spread * (1.5f - 0.5f * charge) * steady; // poco tenso, menos preciso
    yaw += (rng_float(&cb->rng) - 0.5f) * 2.0f * spread;
    pitch += (rng_float(&cb->rng) - 0.5f) * 2.0f * spread;
    memset(s, 0, sizeof(*s));
    projectile_launch(&s->p, rd->projectile, to_v3(from), yaw, pitch, ranged_muzzle_speed(rd, charge));
    s->owner = owner;
    s->burning = burning;
}

// L: encender la flecha en un fuego cercano.
static void light_arrow(Combat *cb, const GameActions *ga, const RangedDef *rd, char *log, size_t len) {
    if (rd->projectile != PROJ_ARROW && rd->projectile != PROJ_BOLT) {
        snprintf(log, len, "%s", T("Solo las flechas y los virotes se encienden."));
    } else if (cb->arrow_lit) {
        snprintf(log, len, "%s", T("La flecha ya arde: ¡dispara!"));
    } else if (ga->raining) {
        snprintf(log, len, "%s", T("Con esta lluvia la flecha no prende."));
    } else if (!ga->fire_near) {
        snprintf(log, len, "%s", T("Acércate a un fuego (fogata, hoguera o algo que arda) para encender la flecha."));
    } else {
        cb->arrow_lit = true;
        cb->arrow_lit_timer = ARROW_LIT_SECONDS;
        snprintf(log, len, "%s", T("Flecha encendida: dispara antes de que se apague."));
    }
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
// La pose de un movimiento cuerpo a cuerpo (patada, agarre, escudo) sobre los parametros.
static void pose_move(BodyPoseParams *pp, int move, float anim, bool guard) {
    float k = anim > 0.0f ? fminf(1.0f, anim / 0.45f) : 0.0f;
    if (move == MOVE_KICK + 1 || move == MOVE_RUN_KICK + 1) pp->kick = k, pp->attack = 0.0f;
    if (move == MOVE_GRAPPLE + 1) pp->grab = k, pp->attack = 0.0f;
    if (move == MOVE_SHIELD_BASH + 1 || move == MOVE_SHIELD_CHARGE + 1) pp->guard = 1.0f, pp->attack = 0.0f;
    if (move == MOVE_PARRY + 1 || move == MOVE_DEFLECT + 1 || move == MOVE_HOOK + 1) pp->parry = k, pp->attack = 0.0f;
    if (move == MOVE_BACKSTAB + 1) pp->hold = k, pp->attack = k; // tapa la boca y apuñala
    if (move == MOVE_FINISH + 1) pp->crouch = fmaxf(pp->crouch, 0.6f * k), pp->attack = k; // rodilla en tierra
    if (guard) pp->guard = fmaxf(pp->guard, 0.8f);
}

static void pose_player(BodyPose *b, const Combat *cb, const Player *p, float time) {
    BodyPoseParams pp = { .walk_phase = time * 9.0f, .walk = p->moving ? 1.0f : 0.0f, .attack = cb->attack_anim / 0.4f,
                          .aiming = cb->aiming, .down = cb->player.down || cb->knock > 0.0f, .scale = 1.0f,
                          .crouch = p->crouching ? 1.0f : p->sneaking ? 0.3f : 0.0f, .hold = cb->hostage ? 1.0f : 0.0f };
    pose_move(&pp, cb->move, cb->move_anim, cb->blocking || cb->charge_timer > 0.0f);
    body_pose(b, &pp);
}

static void pose_enemy(BodyPose *b, const Enemy *e, float time, int i) {
    // Anunciando el golpe, el brazo sube; al darlo, baja.
    float attack = e->windup > 0.0f ? fmaxf(0.0f, 1.0f - e->windup / MELEE_WINDUP) : e->attack_anim / 0.45f;
    BodyPoseParams pp = { .walk_phase = time * 9.0f + (float)i, .walk = e->speed > 0.2f && !e->mounted ? 1.0f : 0.0f,
                          .attack = attack, .aiming = enemy_def(e->kind)->ranged && e->target >= 0 && e->speed < 0.2f,
                          .down = e->state == EN_DEAD || e->state == EN_DOWN || e->knock > 0.0f, .scale = 1.0f,
                          .held = e->state == EN_HOSTAGE ? 1.0f : 0.0f };
    pose_move(&pp, e->move, e->move_anim, e->blocking);
    body_pose(b, &pp);
}

// Un escudo en el antebrazo izquierdo (dibujado sin modelo).
static void draw_shield_on(const BodyPose *b, Vector3 pos, float yaw, Color c) {
    V3 w = body_to_world(b->seg[PART_FOREARM_L].b, to_v3(pos), yaw);
    V3 e = body_to_world(b->seg[PART_FOREARM_L].a, to_v3(pos), yaw);
    Vector3 mid = Vector3Lerp(to_vec(e), to_vec(w), 0.6f);
    rlPushMatrix();
    rlTranslatef(mid.x, mid.y, mid.z);
    rlRotatef(yaw * RAD2DEG, 0, 1, 0);
    DrawCube((Vector3){ 0.08f, 0, 0.1f }, 0.06f, 0.55f, 0.5f, c);
    DrawCubeWires((Vector3){ 0.08f, 0, 0.1f }, 0.06f, 0.55f, 0.5f, (Color){ 60, 50, 40, 255 });
    rlPopMatrix();
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
    dmg *= player_guard(cb, p, ga, to_vec(s->p.pos), true); // el escudo la frena
    float absorbed = 0.0f;
    bool broke = false;
    int wi = combat_apply_hit(&cb->player, &cb->armor, &cb->rng, dmg, pd->wound, part, true, &absorbed, &broke);
    cb->hit_anim = 0.3f;
    if (dmg >= HOSTAGE_JOLT) g_jolt = true; // un flechazo fuerte suelta al rehen
    char d[128], what[32];
    describe_hit(&cb->player, wi, absorbed, broke, d, sizeof(d));
    snprintf(log, len, T("¡Una %s te alcanza! %s."), lower_name(T(pd->name), what, sizeof(what)), d);
}

static void update_shots(Combat *cb, Player *p, GameActions *ga, Troop *troop, const Terrain *t, float time, float dt,
                         char *log, size_t len) {
    for (int si = 0; si < CB_MAX_SHOTS; si++) {
        Shot *s = &cb->shots[si];
        if (!s->p.alive) continue;
        s->life += dt;
        if (s->stuck) {
            if (s->life > STUCK_SECONDS) s->p.alive = false;
            if (s->life > 6.0f) s->burning = false; // se consume
            continue;
        }
        if (s->burning && ga->raining && rng_float(&cb->rng) < dt * 3.0f) s->burning = false; // la lluvia la apaga
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
            if (s->burning) { // la flecha encendida tambien quema
                if (kind == 0) health_hit(&cb->player, &cb->rng, FIRE_ARROW_BURN, WOUND_BURN, part);
                else if (kind == 1) health_hit(&troop->members[idx].health, &cb->rng, FIRE_ARROW_BURN, WOUND_BURN, part);
                else if (kind == 2) health_hit(&cb->enemies[idx].h, &cb->rng, FIRE_ARROW_BURN, WOUND_BURN, part);
                else fg_hurt(ga, idx, FIRE_ARROW_BURN, WOUND_BURN, part, -1, NULL, 0);
            }
            if (kind == 0) {
                shot_hits_player(cb, s, part, p, ga, log, len);
            } else if (kind == 1) {
                Member *m = &troop->members[idx];
                combat_apply_hit(&m->health, &m->armor, &cb->rng, projectile_damage(&s->p), pd->wound, part, true, NULL, NULL);
                ga->npcs[idx].hurt_anim = 0.3f;
                char what[32];
                snprintf(log, len, T("Una %s alcanza a %s en %s."), lower_name(T(pd->name), what, sizeof(what)), m->name,
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
                bool was_down = e->state == EN_DOWN;
                enemy_down_check(cb, e);
                if (s->owner == 0) {
                    char dsc[128], who[48];
                    lower_name(T(enemy_def(e->kind)->name), who, sizeof(who));
                    if (was_down && e->state == EN_DEAD) { // rematar de un flechazo tambien es rematar
                        troop_finish_downed(troop);
                        snprintf(log, len, T("Rematas al %s de un flechazo. Deja botín (F)."), who);
                    } else if (e->state == EN_DEAD) snprintf(log, len, T("¡Diana! Matas al %s (%s)."), who, part_name((BodyPart)part, e->h.beast));
                    else if (e->state == EN_DOWN && !was_down) snprintf(log, len, T("¡Diana! El %s cae abatido (%s)."), who, part_name((BodyPart)part, e->h.beast));
                    else describe_hit(&e->h, wi, absorbed, broke, dsc, sizeof(dsc)), snprintf(log, len, T("Le das al %s: %s."), who, dsc);
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
            if (s->burning && ga->ignite_n < 8) ga->ignite_at[ga->ignite_n++] = to_vec(s->p.pos); // prende donde cae
            if (terrain_deep_water(t, s->p.pos.x, s->p.pos.z, 0.25f)) {
                s->p.alive = false; // al agua (quiza atraviese un pez)
                if (s->owner == 0) fg_shot_water(ga, t, to_vec(s->p.pos), log, len);
            }
        } else if (s->life > SHOT_LIFE) {
            s->p.alive = false;
        }
    }
}

// Con objetivo, la mira sola: hacia el y con la caida calculada para la tension de ahora.
static bool aim_at_target(Combat *cb, Player *p, const RangedDef *rd, float charge) {
    const Enemy *e = locked(cb);
    if (!e) return false;
    p->yaw = atan2f(e->pos.x - p->pos.x, e->pos.z - p->pos.z);
    Vector3 to = { e->pos.x, e->pos.y + (e->mounted ? 2.0f : 1.2f), e->pos.z };
    float pitch;
    if (!ballistic_solve(rd->projectile, to_v3(aim_origin(p->pos, p->yaw)), to_v3(to), ranged_muzzle_speed(rd, charge), &pitch))
        pitch = 0.75f; // fuera de alcance: lo mas lejos que llega
    cb->aim_pitch = pitch;
    return true;
}

// Arco, honda, ballesta o mosquete: mantener H (o clic) tensa y soltar dispara. Con objetivo la
// mira es sola; sin el, apunta donde mira la camara.
static void player_ranged(Combat *cb, Player *p, GameActions *ga, const Props *props, const RangedDef *rd, bool can_act,
                          float cam_yaw, float cam_pitch, float dt, char *log, size_t len) {
    const char *ammo = projectile_def(rd->projectile)->ammo;
    bool click = !hud_pointer_over() && !g_click_used;
    bool held = input_action_down(KA_ATTACK) || (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && click) || g_hud_attack_down;
    bool released = input_action_released(KA_ATTACK) || (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && click) ||
                    (cb->aiming && !held);
    bool pressed = input_action_pressed(KA_ATTACK) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && click) || g_hud_attack_press;
    cb->aim_pitch = aim_pitch_from_camera(cam_pitch);
    if (!can_act) {
        cb->aiming = false;
        cb->draw = 0.0f;
        return;
    }
    int ammo_left = ig_count(ga, props, p, ammo); // lo que llevas encima (o tienes a mano)
    cb->ammo_shown = ammo_left;
    if (pressed && ammo_left <= 0) {
        char what[32];
        snprintf(log, len, T("No te quedan %ss a mano (I: inventario)."), lower_name(T(projectile_def(rd->projectile)->name), what, sizeof(what)));
        return;
    }
    bool was_aiming = cb->aiming;
    cb->aiming = held && cb->reload <= 0.0f && ammo_left > 0;
    float tension = rd->draw_time > 0.0f ? fminf(1.0f, cb->draw / rd->draw_time) : 1.0f;
    if (cb->aiming) {
        if (!aim_at_target(cb, p, rd, tension)) p->yaw = cam_yaw; // sin objetivo: hacia donde mira la camara
        cb->draw += dt;
    }
    // Arco y honda: se tensa y se suelta; ballesta y mosquete: disparan al pulsar.
    bool shoot = rd->draw_time > 0.0f ? (released && was_aiming && cb->draw > 0.05f) : (cb->aiming && pressed);
    if (shoot && cb->reload <= 0.0f && ammo_left > 0) {
        float charge = rd->draw_time > 0.0f ? fminf(1.0f, cb->draw / rd->draw_time) : 1.0f;
        aim_at_target(cb, p, rd, charge);
        ig_use(ga, props, p, ammo, 1);
        // Punteria (amuleto del aguila): menos dispersion.
        float steady = (1.0f - 0.6f * fminf(1.0f, ig_stat(ga, STAT_ARCHERY))) * (1.0f + 1.5f * wg_clumsy(ga)); // borracho, torpe
        // A caballo, la carrera dispersa el tiro; la monta (amuletos) lo corrige.
        if (ga->mounted >= 0)
            steady *= 1.0f + fminf(cb->pl_speed, 12.0f) / 6.0f * (1.0f - 0.6f * fminf(1.0f, ig_stat(ga, STAT_RIDING)));
        fire(cb, rd, aim_origin(p->pos, p->yaw), p->yaw, cb->aim_pitch, charge, 0, cb->arrow_lit, steady);
        cb->arrow_lit = false;
        cb->reload = rd->reload;
        cb->draw = 0.0f;
        cb->aiming = false;
    }
    if (!held) cb->draw = 0.0f;
}

// --------------------------------------------------------------- enemigos
// Al caer, el enemigo deja una bolsa con lo suyo: arma, escudo, flechas, plata, comida
// (src/sim/combat.c) y a veces piezas de su armadura, con el estado en que quedaron.
static void drop_loot(Combat *cb, GameActions *ga, Enemy *e, const Player *p, char *log, size_t len) {
    e->loot_dropped = true;
    if (dist2(e->pos, p->pos) < 40.0f) tg_xp(ga, XP_KILL, log, len); // pelear da experiencia (a ti y a tu escolta)
    LootItem items[LOOT_MAX];
    int n = enemy_loot(e->kind, e->armed, e->shield, &cb->rng, items, LOOT_MAX);
    for (int s = 0; s < SLOT_COUNT && n < LOOT_MAX; s++) {
        const ArmorPiece *a = &e->armor.slot[s];
        if (!a->id[0] || a->durability <= 0.0f || rng_float(&cb->rng) >= 0.5f) continue;
        items[n++] = (LootItem){ a->id, 1, a->durability_max > 0.0f ? a->durability / a->durability_max : 1.0f };
    }
    if (n <= 0 || !ig_drop_loot(ga, e->pos, items, n)) return;
    if (!log[0] && dist2(e->pos, p->pos) < 30.0f) {
        char who[48];
        snprintf(log, len, T("El %s deja botín (F para recogerlo)."), lower_name(T(enemy_def(e->kind)->name), who, sizeof(who)));
    }
}

static void update_enemies(Combat *cb, Player *p, GameActions *ga, Troop *troop, Props *props, const Terrain *t, bool night,
                           float dt, char *log, size_t len) {
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        Enemy *e = &cb->enemies[i];
        if (!e->used) continue;
        const EnemyDef *def = enemy_def(e->kind);
        e->attack_anim = fmaxf(0.0f, e->attack_anim - dt);
        e->hit_anim = fmaxf(0.0f, e->hit_anim - dt);
        e->shown = fmaxf(0.0f, e->shown - dt);
        e->reload = fmaxf(0.0f, e->reload - dt);
        e->stagger = fmaxf(0.0f, e->stagger - dt);
        e->knock = fmaxf(0.0f, e->knock - dt);
        e->move_anim = fmaxf(0.0f, e->move_anim - dt);
        e->block_timer = fmaxf(0.0f, e->block_timer - dt);
        if (e->state == EN_DEAD && !e->loot_dropped) drop_loot(cb, ga, e, p, log, len);
        if (e->state == EN_DEAD) {
            e->corpse -= dt;
            if (e->corpse <= 0.0f) e->used = false;
            continue;
        }
        health_update(&e->h, &cb->rng, dt, false, 0.0f);
        enemy_down_check(cb, e); // se desangro
        if (e->state == EN_DEAD) continue;
        if (dist2(e->pos, p->pos) > DESPAWN_DIST) {
            e->used = false;
            continue;
        }
        e->alert = fmaxf(0.0f, e->alert - dt);
        if (e->state == EN_DOWN) { // abatido: en el suelo hasta despertar; despierta herido, huye y avisa a los suyos
            e->speed = 0.0f;
            if (down_tick(&e->timer, dt)) {
                health_revive(&e->h);
                e->state = EN_FLEE;
                e->timer = 45.0f;
                e->knock = 0.0f;
                alert_near(cb, e->pos, 30.0f, 25.0f);
                char who[48];
                if (dist2(e->pos, p->pos) < 30.0f && !log[0])
                    snprintf(log, len, T("El %s despierta y huye a avisar a los suyos."), lower_name(T(def->name), who, sizeof(who)));
            }
            continue;
        }
        if (e->state == EN_HOSTAGE) { // rehen: delante del jugador, sujeto por el cuello
            e->pos.x = p->pos.x + sinf(p->yaw) * HOSTAGE_AHEAD;
            e->pos.z = p->pos.z + cosf(p->yaw) * HOSTAGE_AHEAD;
            e->pos.y = surface_y(t, e->pos.x, e->pos.z);
            e->yaw = p->yaw;
            e->speed = p->moving ? 1.0f : 0.0f;
            continue;
        }
        e->cooldown = fmaxf(0.0f, e->cooldown - dt);
        e->timer -= dt;
        if (e->knock > 0.0f) { // en el suelo: ni se mueve ni pelea hasta levantarse
            e->speed = 0.0f;
            e->blocking = false;
            e->windup = 0.0f, e->windup_move = 0;
            continue;
        }
        if (e->windup > 0.0f) { // el golpe anunciado cae al terminar, si nada lo interrumpe
            if (e->stagger > 0.0f) e->windup = 0.0f, e->windup_move = 0;
            else if ((e->windup -= dt) <= 0.0f) e->windup = 0.0f, enemy_strike(cb, e, p, ga, troop, props, log, len);
        }
        // Objetivo: el mas cercano que vea (al acechar cuesta mas verte; los lobos ven de noche).
        float sight = def->sight * player_visibility(p, ga->hidden) * (night && !def->beast ? 0.7f : 1.0f) *
                      (1.0f - 0.5f * ig_stat(ga, STAT_STEALTH)); // el amuleto del lobo: cuesta mas verte
        // De donde vienes: de frente te ve; por detras solo te oye (src/sim/stealth.h).
        sight *= stealth_sight_scale(stealth_facing(e->pos.x, e->pos.z, e->yaw, p->pos.x, p->pos.z), p->noise);
        Vector3 tpos = { 0 };
        float best = 1e9f;
        int target = -1;
        if (!cb->player.down) {
            float d = dist2(e->pos, p->pos);
            if (d < sight || (e->target == 0 && d < sight * 1.5f) || (e->alert > 0.0f && d < 60.0f)) best = d, target = 0, tpos = p->pos;
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
        // Escarmiento: las pieles de depredador espantan. Al ver de cerca a quien las lleva puede
        // echarse atras un rato, y herido huye antes (los fanaticos del culto casi no se inmutan).
        float dread = target >= 0 ? ag_dread(ga, troop, target) * (e->kind == ENEMY_FANATIC ? 0.3f : 1.0f) : 0.0f;
        if (target < 0 || best > 12.0f) e->awed = e->awed && target >= 0;
        else if (!e->awed) {
            e->awed = true;
            if (dread > 0.0f && rng_float(&cb->rng) < dread * 0.8f) {
                e->state = EN_FLEE, e->timer = 4.0f + 6.0f * dread;
                char who[48];
                if (!log[0])
                    snprintf(log, len, T("El %s se espanta ante las pieles de fiera y retrocede."), lower_name(T(def->name), who, sizeof(who)));
            }
        }
        if (target >= 0 && def->flee_at > 0.0f && e->h.hp < (def->flee_at + 0.6f * dread) * e->h.hp_max) e->state = EN_FLEE, e->timer = 8.0f;
        // El jugador tiene de rehen a uno de los suyos: no se le acercan, no pegan, y el arquero no tira
        // si el rehen le tapa el blanco (y si tira, falla mas).
        const Enemy *hostage = hostage_of(cb);
        bool hold_back = hostage && target == 0 && same_side(e->kind, hostage->kind);
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
                bool covered = hold_back && hostage_blocks_shot(e->pos.x, e->pos.z, p->pos.x, p->pos.z, hostage->pos.x, hostage->pos.z);
                if (e->reload <= 0.0f && best < 45.0f && !covered) {
                    Vector3 from = aim_origin(e->pos, e->yaw);
                    Vector3 aim = { tpos.x, tpos.y + 1.2f, tpos.z };
                    float pitch;
                    if (ballistic_solve(rd->projectile, to_v3(from), to_v3(aim), ranged_muzzle_speed(rd, 1.0f), &pitch)) {
                        float err = (0.015f + best * 0.0008f) * (hold_back ? 2.5f : 1.0f); // a mayor distancia, peor punteria; nervioso, peor
                        fire(cb, rd, from, e->yaw + (rng_float(&cb->rng) - 0.5f) * err * 2.0f, pitch + (rng_float(&cb->rng) - 0.5f) * err,
                             1.0f, -(1 + i), false, 1.0f);
                        e->reload = rd->reload + 1.4f + rng_float(&cb->rng);
                        e->attack_anim = 0.3f;
                    }
                }
            } else if (hold_back) { // retroceden hasta HOSTAGE_KEEP_AWAY, de cara al jugador
                e->blocking = false;
                if (best < HOSTAGE_KEEP_AWAY) {
                    Vector3 away = Vector3Normalize((Vector3){ e->pos.x - tpos.x, 0, e->pos.z - tpos.z });
                    goal = (Vector3){ e->pos.x + away.x * 2.0f, 0, e->pos.z + away.z * 2.0f }, speed = def->speed * 0.6f;
                }
            } else {
                // Anunciando el golpe, se planta; el jinete no, que pega con la inercia del galope.
                if (best > def->reach * 0.9f && (e->windup <= 0.0f || e->mounted))
                    goal = tpos, speed = def->speed * (best > 4.0f && best < 9.0f ? 1.25f : 1.0f);
                // Con escudo, se cubre cuando el jugador va a golpear de frente.
                bool threat = target == 0 && (cb->attack_anim > 0.0f || cb->v_hold > 0.15f || cb->charge_timer > 0.0f);
                if (e->shield && threat && best < 3.0f && e->stagger <= 0.0f && e->block_timer <= 0.0f &&
                    rng_float(&cb->rng) < dt * 4.0f)
                    e->block_timer = 0.8f;
                e->blocking = e->shield && e->block_timer > 0.0f && e->stagger <= 0.0f && e->windup <= 0.0f;
                if (e->windup <= 0.0f && best <= def->reach && e->cooldown <= 0.0f && e->stagger <= 0.0f && !e->blocking)
                    enemy_windup(cb, e, p, ga, troop, best);
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
            float step = fminf(speed * dt, d), x0 = e->pos.x, z0 = e->pos.z;
            e->pos.x += (goal.x - e->pos.x) / d * step;
            e->pos.z += (goal.z - e->pos.z) / d * step;
            terrain_dry_step(t, x0, z0, &e->pos.x, &e->pos.z);
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

static void update_companions(Combat *cb, GameActions *ga, Troop *troop, Props *props, const Player *p, const Terrain *t,
                              float dt, char *log, size_t len) {
    float healer = troop_healer_skill(troop);
    for (int k = 0; k < troop->count && k < TROOP_MAX; k++) {
        Npc *n = &ga->npcs[k];
        Member *m = &troop->members[k];
        n->fight_anim = fmaxf(0.0f, n->fight_anim - dt);
        n->hurt_anim = fmaxf(0.0f, n->hurt_anim - dt);
        n->stagger = fmaxf(0.0f, n->stagger - dt);
        n->knock = fmaxf(0.0f, n->knock - dt);
        n->move_anim = fmaxf(0.0f, n->move_anim - dt);
        cb->comp_cd[k] = fmaxf(0.0f, cb->comp_cd[k] - dt);
        n->fighting = false;
        if (m->status != STATUS_ACTIVE) continue;
        if (!m->outfitted) outfit(m, troop);
        // Toda la tribu sangra y sana; en el campamento descansan.
        health_update(&m->health, &cb->rng, dt, !n->escort, healer);
        if (m->health.dead) {
            snprintf(log, len, T("%s murió de sus heridas. La tribu está de luto (moral -6)."), m->name);
            troop_mourn(troop, m->id, 6.0f);
            n->escort = false;
            continue;
        }
        if (!n->escort || m->health.down || n->member_id != m->id || n->knock > 0.0f) continue;
        // La escolta pelea: va al enemigo mas cercano (a ella o al jugador).
        Enemy *best = NULL;
        float best_d = 14.0f;
        for (int i = 0; i < CB_MAX_ENEMIES; i++) {
            Enemy *e = &cb->enemies[i];
            if (!standing(e)) continue;
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
                float step = fminf(speed * dt, d - 1.5f), x0 = n->pos.x, z0 = n->pos.z;
                n->pos.x += (apos.x - n->pos.x) / d * step;
                n->pos.z += (apos.z - n->pos.z) / d * step;
                terrain_dry_step(t, x0, z0, &n->pos.x, &n->pos.z);
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
            float step = fminf(speed * dt, d - 1.4f), x0 = n->pos.x, z0 = n->pos.z;
            n->pos.x += (best->pos.x - n->pos.x) / d * step;
            n->pos.z += (best->pos.z - n->pos.z) / d * step;
            terrain_dry_step(t, x0, z0, &n->pos.x, &n->pos.z);
            n->pos.y = surface_y(t, n->pos.x, n->pos.z);
        } else if (cb->comp_cd[k] <= 0.0f && n->stagger <= 0.0f) {
            // Las mismas reglas que el jugador: combos, patadas, agarres y ganchos segun lo que haga el rival.
            const char *weapon = member_weapon(m);
            Fighter me = fighter_member(n, m, troop, best->pos), foe = fighter_enemy(best, n->pos);
            me.strength *= health_attack_scale(&m->health);
            if (ga->ab_timer[ABIL_WAR_CRY] > 0.0f && dist2(n->pos, p->pos) < 20.0f) me.strength *= 1.0f + 0.3f * ga->ab_pot[ABIL_WAR_CRY];
            MeleeMove mv = ai_choose(&cb->rng, &me, &foe, weapon, false, d);
            if (mv == MOVE_SHIELD_BASH || (mv == MOVE_HOOK && !weapon_can_hook(weapon))) mv = MOVE_HEAVY;
            if (mv == MOVE_LIGHT) n->combo = n->combo % 3 + 1;
            MeleeResult r = melee_resolve(mv, &me, &foe, weapon, mv == MOVE_LIGHT ? n->combo : 1, &cb->rng);
            if (r.attacker_staggered) n->stagger = STAGGER_SECONDS;
            hit_enemy(cb, best, mv, &r, props, m->id, NULL, 0);
            cb->comp_cd[k] = mv == MOVE_LIGHT ? melee_combo_cooldown(weapon, COMPANION_COOLDOWN, n->combo) : move_def(mv)->recovery + 0.5f;
            n->fight_anim = mv == MOVE_LIGHT || mv == MOVE_HEAVY ? 0.4f : 0.0f;
            n->move = (int)mv + 1;
            n->move_anim = 0.45f;
            char who[48];
            lower_name(T(enemy_def(best->kind)->name), who, sizeof(who));
            if (best->state == EN_DEAD) snprintf(log, len, T("%s mató a un %s."), m->name, who);
            else if (best->state == EN_DOWN && dist2(n->pos, p->pos) < 25.0f) snprintf(log, len, T("%s tumbó a un %s: queda abatido."), m->name, who);
            else if (r.knocked_down && dist2(n->pos, p->pos) < 25.0f) snprintf(log, len, T("%s: %s derriba al %s."), move_verb(mv), m->name, who);
            else if (r.shield_dropped && dist2(n->pos, p->pos) < 25.0f) snprintf(log, len, T("%s le arranca el escudo al %s."), m->name, who);
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

static void bandage(Combat *cb, GameActions *ga, Troop *troop, const Props *props, const Player *p, char *log, size_t len) {
    bool herbs = ig_count(ga, props, p, HERBS_ID) > 0; // las hierbas que llevas (o el acopio, en el campamento)
    Member *m = NULL;
    // 1) Un companero abatido cerca: levantarlo (y vendarlo si hay hierbas).
    if (npc_near(ga, troop, p, 3.0f, true, &m)) {
        if (herbs) {
            ig_use(ga, props, p, HERBS_ID, 1);
            health_treat(&m->health);
        }
        health_revive(&m->health);
        snprintf(log, len, herbs ? T("Levantas y vendas a %s.") : T("Levantas a %s, pero sin hierbas sigue sangrando."), m->name);
        return;
    }
    if (!herbs && ig_count(ga, props, p, "utileria.consumible.unguento") <= 0) {
        snprintf(log, len, "%s", T("No llevas hierbas curativas (I: inventario)."));
        return;
    }
    // 2) Tus propias heridas: con ungüento (si llevas), mejor que con hierbas.
    if (health_untreated(&cb->player) > 0 && ig_count(ga, props, p, "utileria.consumible.unguento") > 0) {
        ig_use(ga, props, p, "utileria.consumible.unguento", 1);
        health_treat(&cb->player);
        cb->player.venom = 0.0f;
        cb->player.hp = fminf(cb->player.hp_max, cb->player.hp + cb->player.hp_max * 0.2f);
        snprintf(log, len, "%s", T("Te untas el ungüento: cierra las heridas y corta el veneno."));
        return;
    }
    if (health_untreated(&cb->player) > 0 && herbs) {
        ig_use(ga, props, p, HERBS_ID, 1);
        int n = health_treat(&cb->player);
        snprintf(log, len, T("Te vendas %d herida%s con hierbas curativas."), n, n == 1 ? "" : "s");
        return;
    }
    // 3) Un companero herido cerca.
    if (herbs && npc_near(ga, troop, p, 3.0f, false, &m)) {
        ig_use(ga, props, p, HERBS_ID, 1);
        health_treat(&m->health);
        snprintf(log, len, T("Vendas las heridas de %s."), m->name);
        return;
    }
    snprintf(log, len, "%s", T("No hay heridas que vendar."));
}

static const Member *role_member(const Troop *troop, Role role) {
    for (int i = 0; i < troop->count; i++)
        if (troop->members[i].status == STATUS_ACTIVE && troop->members[i].role == role && !troop->members[i].health.down)
            return &troop->members[i];
    return NULL;
}

// ------------------------------------------------------------------ jugador
// La muerte: en el suelo quedan una calavera y unos huesos, y el jugador vuelve a su ultimo
// lugar de descanso (src/game/death_game.c), curado, sin lo que llevaba en las manos.
static void player_dies(Combat *cb, Player *p, GameActions *ga, Troop *troop, const Terrain *t, Vector3 camp_fire,
                        const char *why, char *log, size_t len) {
    dg_leave_remains(p->pos, p->yaw);
    if (ga->mounted >= 0) ga->animals[ga->mounted].ridden = false, ga->mounted = -1;
    p->pos = dg_rest_point(t, camp_fire);
    p->vy = 0.0f;
    cb->player.dead = false; // health_revive no levanta a un muerto
    health_treat(&cb->player);
    health_revive(&cb->player);
    cb->player.hp = fmaxf(cb->player.hp, 0.6f * cb->player.hp_max);
    cb->player.blood = fmaxf(cb->player.blood, 0.6f);
    cb->down_timer = 0.0f;
    cb->knock = 0.0f;
    ga->hands.carried[0] = '\0';
    troop_adjust_morale(troop, -8.0f);
    snprintf(log, len, T("%s Mueres. Vuelves en %s; tus huesos quedan donde caíste (moral -8)."), why, dg_rest_name());
}

static void update_player(Combat *cb, Player *p, GameActions *ga, Troop *troop, const Terrain *t, Vector3 camp_fire,
                          float dt, char *log, size_t len) {
    bool at_camp = dist2(p->pos, camp_fire) < CAMP_RADIUS;
    float healer = at_camp ? troop_healer_skill(troop) : 0.0f;
    // Aguante (amuletos del ciervo y del oso): se sana como si alguien atendiera.
    // Vida maxima: 100 mas lo que den tatuajes y joyas (se conserva la proporcion al cambiar).
    float want = 100.0f * (1.0f + fmaxf(-0.5f, ig_stat(ga, STAT_HEALTH)));
    if (fabsf(cb->player.hp_max - want) > 0.05f) {
        cb->player.hp = cb->player.hp * want / cb->player.hp_max;
        cb->player.hp_max = want;
    }
    health_update(&cb->player, &cb->rng, dt, !p->moving, healer + ig_stat(ga, STAT_STAMINA_REGEN));
    // Desangrado: muere (ver player_dies).
    if (cb->player.dead) {
        player_dies(cb, p, ga, troop, t, camp_fire, T("Te desangras."), log, len);
        return;
    }
    // El curandero del campamento atiende al jugador.
    cb->healer_timer -= dt;
    const Member *hm = role_member(troop, ROLE_HEALER);
    if (at_camp && hm && cb->healer_timer <= 0.0f && health_untreated(&cb->player) > 0) {
        health_treat(&cb->player);
        cb->healer_timer = 8.0f;
        snprintf(log, len, T("%s te venda las heridas."), hm->name);
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
        snprintf(log, len, T("%s te levanta y te venda. ¡Sigue en pie!"), helper->name);
    } else if (!helper && cb->down_timer > DOWN_WAKE_SECONDS) {
        player_dies(cb, p, ga, troop, t, camp_fire, T("Nadie llega a socorrerte."), log, len);
    }
}

// Los cuatro botones con un arma de mano (o sin armas): H ataque (suelta pronto, golpe y combo;
// mantenida, golpe pesado), J bloqueo (lo lee cb_update), K parry, L carga con escudo o patada
// (a la carrera, con inercia). Cubriendose con escudo, H es golpe de escudo.
// H o K junto a un abatido: rematarlo. A la tribu le pesa segun sus rasgos (src/sim/troop.h).
static void player_finish(Combat *cb, Player *p, Troop *troop, Enemy *e, char *log, size_t len) {
    p->yaw = atan2f(e->pos.x - p->pos.x, e->pos.z - p->pos.z);
    cb->move = MOVE_FINISH + 1;
    cb->move_anim = 0.9f;
    cb->attack_anim = 0.45f;
    cb->attack_cd = move_def(MOVE_FINISH)->recovery;
    e->h.down = e->h.dead = true;
    e->state = EN_DEAD;
    e->corpse = CORPSE_SECONDS;
    troop_finish_downed(troop);
    char who[48];
    snprintf(log, len, T("Rematas al %s: deja botín (F). La tribu lo ve."), lower_name(T(enemy_def(e->kind)->name), who, sizeof(who)));
}

// F junto a un abatido: lo ata y lo toma prisionero; lo que llevaba queda en el suelo, como botin.
static bool g_capture_req;
static float g_capture_d = 1e9f; // al abatido mas cercano, en el ultimo cuadro
float cb_capture_dist(void) { return g_capture_d; }
void cb_request_capture(void) { g_capture_req = true; }

static void capture(Combat *cb, GameActions *ga, Troop *troop, Player *p, char *log, size_t len) {
    Enemy *e = downed_near(cb, p->pos, CB_DOWNED_REACH);
    if (!e) return;
    const EnemyDef *def = enemy_def(e->kind);
    char name[NAME_LEN], who[48];
    lower_name(T(def->name), who, sizeof(who));
    snprintf(name, sizeof(name), "%s %d", T(def->name), troop->next_id);
    if (troop_take_prisoner(troop, name, 0) < 0) {
        snprintf(log, len, "%s", T("La tribu no tiene sitio para más gente."));
        return;
    }
    p->yaw = atan2f(e->pos.x - p->pos.x, e->pos.z - p->pos.z);
    cb->move = MOVE_GRAPPLE + 1;
    cb->move_anim = 0.6f;
    drop_loot(cb, ga, e, p, log, len);
    e->used = false;
    snprintf(log, len, T("Atas al %s y lo tomas prisionero; lo que llevaba queda en el suelo (F)."), who);
}

// Lo que pide la ventana del parry: un golpe enemigo a punto de caer sobre el jugador.
static bool parry_due(Combat *cb, const Player *p) {
    const Enemy *e = incoming(cb, p);
    return e && melee_parry_in_window(e->windup);
}

static void player_melee_input(Combat *cb, Player *p, GameActions *ga, Troop *troop, const Terrain *t, Props *props,
                               bool can_fight, float dt, char *log, size_t len) {
    bool shield = player_has_shield(ga);
    bool running = p->stance == STANCE_RUN && p->moving;
    bool click = !hud_pointer_over() && !g_click_used; // un clic en el HUD, o el que eligio objetivo, no es un golpe
    bool h_press = input_action_pressed(KA_ATTACK) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && click) || g_hud_attack_press;
    bool h_down = input_action_down(KA_ATTACK) || (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && click) || g_hud_attack_down;
    bool k_press = input_action_pressed(KA_PARRY) || g_hud_parry;
    bool l_press = input_action_pressed(KA_CHARGE) || g_hud_charge;
    if (!can_fight) {
        cb->v_hold = 0.0f;
        return;
    }
    bool ready = cb->attack_cd <= 0.0f;
    bool riding = ga->mounted >= 0;
    // A caballo solo se golpea con el arma (H); el parry, las cargas y las patadas, a pie.
    if (riding && ready && (k_press || l_press || (cb->blocking && h_press))) {
        snprintf(log, len, "%s", T("A caballo no: solo golpe (H) y golpe pesado (mantener H)."));
        return;
    }
    // Con un rehen: K lo ejecuta, L lo empuja; H golpea con la mano libre (no con un arma a dos manos).
    Enemy *hostage = hostage_of(cb);
    if (hostage) {
        if (ready && k_press) {
            player_backstab(cb, p, hostage, log, len);
            return;
        }
        if (ready && l_press) {
            release_hostage(cb, p, true, log, len);
            cb->move = MOVE_KICK + 1, cb->move_anim = 0.45f, cb->attack_cd = 0.6f;
            return;
        }
        if (h_press && ga->hands.right.kind == INV_HANDS_TWO) {
            snprintf(log, len, "%s", T("Con un rehén solo tienes una mano libre."));
            return;
        }
        if (k_press || l_press) return;
    } else if (ready && !riding && k_press && cb->charge_timer <= 0.0f && !parry_due(cb, p)) {
        // K con la espalda ganada: ejecucion silenciosa.
        Enemy *b = back_open(cb, p);
        if (b) {
            player_backstab(cb, p, b, log, len);
            return;
        }
    }
    // H o K junto a un abatido, sin nadie en pie a tiro (ni un golpe que parar): rematarlo.
    if (ready && !riding && (h_press || k_press) && cb->charge_timer <= 0.0f) {
        Enemy *dn = downed_near(cb, p->pos, CB_DOWNED_REACH);
        float reach = weapon_stats(hands_attack_weapon(&ga->hands, 1, NULL)).reach;
        bool foe_up = h_press && enemy_in_front(cb, p->pos, p->yaw, reach + 0.3f, NULL);
        if (dn && !foe_up && !(k_press && parry_due(cb, p))) {
            player_finish(cb, p, troop, dn, log, len);
            return;
        }
    }
    if (k_press && cb->parry_cd <= 0.0f && cb->charge_timer <= 0.0f) {
        player_parry(cb, p, ga, props, log, len);
        return;
    }
    // L: con escudo, carga que empuja y arrolla al primero que encuentra delante; sin escudo, patada.
    if (ready && l_press && cb->charge_timer <= 0.0f) {
        if (shield) {
            cb->charge_timer = 0.7f;
            cb->charge_hit = false;
            cb->move = MOVE_SHIELD_CHARGE + 1;
            cb->move_anim = 0.7f;
            cb->attack_cd = move_def(MOVE_SHIELD_CHARGE)->recovery;
            player_stamina_spend(0.12f);
        } else {
            player_move(cb, p, ga, t, props, running ? MOVE_RUN_KICK : MOVE_KICK, log, len);
        }
        return;
    }
    if (cb->charge_timer > 0.0f) {
        cb->charge_timer -= dt;
        // La carga lleva al jugador hacia adelante aunque no camine.
        float x0 = p->pos.x, z0 = p->pos.z, step = 5.5f * dt;
        p->pos.x += sinf(p->yaw) * step, p->pos.z += cosf(p->yaw) * step;
        terrain_dry_step(t, x0, z0, &p->pos.x, &p->pos.z);
        float d;
        Enemy *e = cb->charge_hit ? NULL : enemy_in_front(cb, p->pos, p->yaw, 1.6f, &d);
        if (e) {
            Fighter me = fighter_player(cb, p, ga, e->pos), foe = fighter_enemy(e, p->pos);
            MeleeResult r = melee_resolve(MOVE_SHIELD_CHARGE, &me, &foe, "", 1, &cb->rng);
            hit_enemy(cb, e, MOVE_SHIELD_CHARGE, &r, props, 0, log, len);
            cb->charge_hit = true;
        }
        return;
    }
    if (!ready) return;
    if (cb->blocking && shield && h_press) { // cubierto con el escudo, el ataque es un golpe de escudo
        player_move(cb, p, ga, t, props, MOVE_SHIELD_BASH, log, len);
        return;
    }
    if (cb->blocking) {
        cb->v_hold = 0.0f;
        return;
    }
    // H: suelta pronto, golpe (combo); mantenida, golpe pesado.
    if (h_press) cb->v_hold = 0.001f;
    if (cb->v_hold > 0.0f && h_down) cb->v_hold += dt;
    if (cb->v_hold > 0.0f && !h_down) {
        player_move(cb, p, ga, t, props, cb->v_hold >= 0.45f ? MOVE_HEAVY : MOVE_LIGHT, log, len);
        cb->v_hold = 0.0f;
    }
}

// Al galope, el caballo arrolla al que este delante (salvo otro jinete, que aguanta mejor).
static void trample(Combat *cb, Player *p, GameActions *ga, Props *props, char *log, size_t len) {
    float d;
    Enemy *e = enemy_in_front(cb, p->pos, p->yaw, 1.8f, &d);
    if (!e || e->knock > 0.0f) return;
    float k = mounted_momentum(cb->pl_speed);
    MeleeResult r = { .landed = true, .damage = 10.0f * k, .wound = WOUND_BRUISE };
    r.knocked_down = !e->mounted || rng_float(&cb->rng) < 0.3f;
    r.staggered = !r.knocked_down;
    char who[48];
    lower_name(T(enemy_def(e->kind)->name), who, sizeof(who));
    hit_enemy(cb, e, MOVE_RUN_KICK, &r, props, 0, NULL, 0);
    if (e->state == EN_DEAD) snprintf(log, len, T("Arrollas al %s con el caballo: queda en el suelo. Deja botín (F)."), who);
    else if (e->state == EN_DOWN) snprintf(log, len, T("Arrollas al %s con el caballo: queda abatido."), who);
    else snprintf(log, len, T("¡Arrollas al %s con el caballo!"), who);
    if (e->target < 0) e->target = 0;
    cb->trample_cd = 1.2f;
    (void)ga;
}

static char g_hint[96]; // lo que se puede hacer ahi (junto a un abatido, con la espalda ganada...)
static bool g_bandage_req; // vendar pedido desde el HUD
void cb_request_bandage(void) { g_bandage_req = true; }
static bool g_light_req; // encender la flecha, pedido desde la columna del HUD
void cb_request_light_arrow(void) { g_light_req = true; }

void cb_update(Combat *cb, Player *p, GameActions *ga, Troop *troop, Props *props, const Terrain *t, Vector3 camp_fire,
               bool input_ok, bool night, bool winter, float cam_yaw, float cam_pitch, float dt, char *log, size_t log_len) {
    float time = (float)GetTime();
    cb->attack_cd = fmaxf(0.0f, cb->attack_cd - dt);
    cb->attack_anim = fmaxf(0.0f, cb->attack_anim - dt);
    cb->hit_anim = fmaxf(0.0f, cb->hit_anim - dt);
    cb->reload = fmaxf(0.0f, cb->reload - dt);
    cb->combo_timer = fmaxf(0.0f, cb->combo_timer - dt);
    cb->move_anim = fmaxf(0.0f, cb->move_anim - dt);
    cb->stagger = fmaxf(0.0f, cb->stagger - dt);
    cb->parry_cd = fmaxf(0.0f, cb->parry_cd - dt);
    cb->exposed = fmaxf(0.0f, cb->exposed - dt);
    if (cb->knock > 0.0f && (cb->knock -= dt) <= 0.0f && !cb->player.down) snprintf(log, log_len, "%s", T("Te levantas."));
    cb->knock = fmaxf(0.0f, cb->knock);
    // Velocidad real (para la inercia a caballo); un salto grande es un viaje, no una carrera.
    float moved = dist2(p->pos, cb->last_pos);
    cb->pl_speed = dt > 0.0f && moved < 5.0f ? Lerp(cb->pl_speed, moved / dt, fminf(1.0f, dt * 8.0f)) : 0.0f;
    cb->last_pos = p->pos;
    cb->trample_cd = fmaxf(0.0f, cb->trample_cd - dt);
    if (ga->mounted >= 0 && cb->pl_speed > GALLOP_SPEED && cb->trample_cd <= 0.0f && !cb->player.down) trample(cb, p, ga, props, log, log_len);
    for (int i = 0; i < cb->loose_n; i++) { // los caballos de los jinetes derribados quedan sueltos
        Vector3 h = cb->loose_horse[i];
        fg_spawn_one(ga, SPECIES_HORSE, h.x + sinf(cb->loose_yaw[i]) * 2.0f, h.z + cosf(cb->loose_yaw[i]) * 2.0f, cb->loose_yaw[i]);
    }
    cb->loose_n = 0;
    bool can_act = input_ok && !cb->player.down && !ga->climbing && cb->knock <= 0.0f;
    const RangedDef *rd = ga->hands.sheathed ? NULL : ranged_def(ga->hands.right.id);
    // Abatidos: F junto a uno lo toma prisionero (src/game/actions_game.c lo pide); el aviso de teclas.
    {
        Enemy *dn = downed_near(cb, p->pos, 1e9f);
        g_capture_d = dn ? dist2(dn->pos, p->pos) : 1e9f;
        if (g_capture_req && input_ok) capture(cb, ga, troop, p, log, log_len);
        g_capture_req = false;
        g_hint[0] = '\0';
        if (g_capture_d <= CB_DOWNED_REACH + 0.8f && ga->mounted < 0)
            snprintf(g_hint, sizeof(g_hint), "%s", T("Abatido: F lo toma prisionero · H o K lo rematan"));
    }
    // El objetivo se pierde a TARGET_RANGE o si muere; Tab (en combate) pasa al siguiente.
    keep_target(cb, p);
    int req = ga_take_target_request();
    if (req && input_ok) cb_cycle_target(cb, p, req, log, log_len);
    // J (o el clic derecho sin arrastrar): cubrirse. Con escudo, de frente; sin escudo, con el arma
    // de mano, que cansa (agotado no se puede). A distancia, J baja el arma y solo cubre el escudo.
    bool shield = player_has_shield(ga);
    bool melee_weapon = !rd && hands_attack_weapon(&ga->hands, 1, NULL)[0] != '\0';
    bool guard_key = input_action_down(KA_BLOCK) || input_right_hold() || g_hud_block;
    cb->blocking = can_act && guard_key && !ga->hands.sheathed && cb->stagger <= 0.0f && cb->exposed <= 0.0f && !cb->hostage &&
                   (shield || (melee_weapon && !player_winded()));
    // G: agarrar por la espalda (rehen) o soltarlo. Un golpe fuerte, caer o montar lo sueltan.
    Enemy *back = !cb->hostage && ga->mounted < 0 ? back_open(cb, p) : NULL;
    g_back = back ? (int)(back - cb->enemies) + 1 : 0;
    bool grab = (input_action_pressed(KA_GRAB) || g_hud_grab) && input_ok;
    if (cb->hostage && (g_jolt || cb->knock > 0.0f || cb->player.down || ga->mounted >= 0 || !hostage_of(cb))) {
        bool had = hostage_of(cb) != NULL;
        release_hostage(cb, p, false, NULL, 0);
        if (had) snprintf(log, log_len, "%s", ga->mounted >= 0 ? T("Sueltas al rehén para montar.") : T("¡Con el golpe se te escapa el rehén!"));
    } else if (grab && cb->hostage) {
        release_hostage(cb, p, false, log, log_len);
    } else if (grab && back && can_act && !rd) {
        take_hostage(cb, p, back, log, log_len);
    } else if (grab) {
        snprintf(log, log_len, "%s", rd ? T("Con un arma a distancia en las manos no puedes agarrar a nadie.")
                                        : T("G agarra por la espalda (sin que te vean, o a uno aturdido). La mochila va con Mayús+G."));
    }
    g_jolt = false;
    if (cb->hostage) snprintf(g_hint, sizeof(g_hint), "%s", T("Rehén: G lo suelta · K lo ejecuta · L lo empuja"));
    else if (back) snprintf(g_hint, sizeof(g_hint), "%s", T("Por la espalda: K lo ejecuta en silencio · G lo toma de rehén"));
    if (rd && can_act && (input_action_pressed(KA_LIGHT_ARROW) || g_light_req)) light_arrow(cb, ga, rd, log, log_len);
    g_light_req = false;
    if (cb->arrow_lit) {
        cb->arrow_lit_timer -= dt;
        if (ga->raining || cb->arrow_lit_timer <= 0.0f || !rd) {
            cb->arrow_lit = false;
            snprintf(log, log_len, "%s", ga->raining ? T("La lluvia apaga la flecha.") : T("La flecha encendida se apagó."));
        }
    }
    if (rd) {
        if (guard_key && can_act) cb->aiming = false, cb->draw = 0.0f; // J baja el arma
        else player_ranged(cb, p, ga, props, rd, can_act && cb->exposed <= 0.0f && !cb->hostage, cam_yaw, cam_pitch, dt, log, log_len);
        // L con el arma a distancia (sin Mayus, que enciende la flecha): patada.
        bool kick = (input_action_pressed(KA_CHARGE) && !(input_mods() & KM_SHIFT)) || g_hud_charge;
        if (kick && can_act && cb->attack_cd <= 0.0f && cb->stagger <= 0.0f && ga->mounted < 0)
            player_move(cb, p, ga, t, props, MOVE_KICK, log, log_len);
    } else {
        cb->aiming = false;
        player_melee_input(cb, p, ga, troop, t, props, can_act && cb->stagger <= 0.0f && cb->exposed <= 0.0f, dt, log, log_len);
    }
    if (input_ok && (input_action_pressed(KA_BANDAGE) || g_bandage_req)) bandage(cb, ga, troop, props, p, log, log_len);
    g_bandage_req = false;
    if (input_ok && input_debug() && IsKeyPressed(KEY_NINE)) { // prueba (con Ctrl+D): enemigos delante
        bool sh = IsKeyDown(KEY_LEFT_SHIFT), ct = IsKeyDown(KEY_LEFT_CONTROL);
        const char *what = sh && ct ? "jinetes" : sh ? "lobos" : ct ? "culto"
                           : IsKeyDown(KEY_LEFT_ALT)                                         ? "arqueros"
                                                                                             : "bandidos";
        cb_spawn_group(cb, ga, what, p, t, 14.0f, log, log_len);
    }
    natural_spawns(cb, ga, p, t, camp_fire, night, dt, log, log_len);
    (void)winter;
    update_enemies(cb, p, ga, troop, props, t, night, dt, log, log_len);
    update_shots(cb, p, ga, troop, t, time, dt, log, log_len);
    update_companions(cb, ga, troop, props, p, t, dt, log, log_len);
    if (!cb->player.down) dg_track_rest(ga, props, p->pos, camp_fire, dt);
    update_player(cb, p, ga, troop, t, camp_fire, dt, log, log_len);
    // Animacion del jugador.
    ga->pl_down = cb->player.down;
    ga->pl_hit = cb->hit_anim > 0.0f;
    ga->pl_attacking = cb->attack_anim > 0.0f ? cb->combo : 0;
    ga->pl_blocking = cb->blocking || cb->charge_timer > 0.0f;
    ga->pl_knocked = cb->knock > 0.0f;
    ga->pl_move = cb->move_anim > 0.0f ? cb->move : 0;
    ga->pl_limping = health_speed_scale(&cb->player) < 0.85f;
    ga->pl_ranged = cb->aiming ? ranged_anim(rd) : 0;
    ga->pl_holding = cb->hostage != 0;
    // En combate (algun enemigo vivo a tiro de objetivo), Tab elige objetivo en vez de abrir el menu.
    bool near = false;
    for (int i = 0; i < CB_MAX_ENEMIES && !near; i++)
        near = standing(&cb->enemies[i]) && dist2(cb->enemies[i].pos, p->pos) < TARGET_RANGE;
    ga_set_combat_near(near);
    g_hud_attack_press = g_hud_attack_down = g_hud_block = g_hud_parry = g_hud_charge = g_hud_grab = false;
    g_click_used = false;
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
    else if (k == ENEMY_RIDER) c.cloth = (Color){ 52, 66, 92, 255 };
    if (hit) c.cloth = (Color){ 236, 226, 214, 255 };
    return c;
}

// El caballo del jinete enemigo: cuerpo, cuello, cabeza y patas que trotan.
void cb_draw_horse(Vector3 pos, float yaw, float speed, float time, bool hit) {
    Color hide = hit ? (Color){ 220, 200, 180, 255 } : (Color){ 96, 64, 40, 255 };
    Color dark = { 48, 32, 22, 255 };
    Vector3 f = { sinf(yaw), 0, cosf(yaw) }, r = { cosf(yaw), 0, -sinf(yaw) };
    float y = pos.y + 1.05f;
    Vector3 back = { pos.x - f.x * 0.75f, y, pos.z - f.z * 0.75f }, front = { pos.x + f.x * 0.7f, y + 0.05f, pos.z + f.z * 0.7f };
    DrawCapsule(back, front, 0.32f, 6, 4, hide);
    Vector3 neck = { pos.x + f.x * 1.15f, y + 0.55f, pos.z + f.z * 1.15f };
    DrawCylinderEx(front, neck, 0.2f, 0.14f, 5, hide);
    DrawCapsule(neck, (Vector3){ neck.x + f.x * 0.45f, neck.y - 0.2f, neck.z + f.z * 0.45f }, 0.12f, 5, 3, hide);
    DrawCylinderEx((Vector3){ back.x - f.x * 0.2f, y + 0.1f, back.z - f.z * 0.2f }, (Vector3){ back.x - f.x * 0.5f, y - 0.5f, back.z - f.z * 0.5f },
                   0.06f, 0.03f, 4, dark); // cola
    float gait = speed > 0.2f ? time * (4.0f + speed) : 0.0f;
    for (int k = 0; k < 4; k++) {
        float side = k % 2 ? 0.2f : -0.2f, along = k < 2 ? 0.6f : -0.6f;
        float swing = sinf(gait + (float)k * 1.7f) * (speed > 0.2f ? 0.35f : 0.0f);
        Vector3 hip = { pos.x + f.x * along + r.x * side, y - 0.1f, pos.z + f.z * along + r.z * side };
        Vector3 hoof = { hip.x + f.x * swing, pos.y + 0.05f, hip.z + f.z * swing };
        DrawCylinderEx(hip, hoof, 0.08f, 0.05f, 4, hide);
    }
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
    if (s->burning) { // la punta arde
        Vector3 f = Vector3Lerp(tail, tip, 0.85f);
        float k = 0.07f + 0.02f * sinf((float)GetTime() * 25.0f + s->life);
        DrawSphere(f, k * 1.5f, (Color){ 240, 120, 30, 220 });
        DrawSphere(f, k, (Color){ 255, 220, 90, 255 });
    }
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
        bool dead = e->state == EN_DEAD, downed = e->state == EN_DOWN;
        Vector3 at = e->pos;
        if (e->mounted && !dead) { // a caballo: el jinete va encima
            cb_draw_horse(e->pos, e->yaw, e->speed, time + (float)i, e->hit_anim > 0.0f);
            at.y += RIDER_LIFT;
        }
        if (props_has_model(props, it)) {
            HumanoidState hs = { .moving = e->speed > 0.2f, .running = e->speed > 3.5f, .grounded = true, .doing = -1,
                                 .mounted = e->mounted && !dead,
                                 .building = -1, .dead = dead, .down = downed, .hit = e->hit_anim > 0.0f,
                                 .attacking = (e->attack_anim > 0.0f || e->windup > 0.0f) && !def->ranged ? 1 + i % 3 : 0,
                                 .ranged = def->ranged && e->target >= 0 && e->speed < 0.2f ? 1 : 0,
                                 .limping = health_speed_scale(&e->h) < 0.85f,
                                 .grip = e->shield ? GRIP_WEAPON_SHIELD : e->armed ? GRIP_ONE_HANDED : GRIP_EMPTY,
                                 .blocking = e->blocking, .move = e->move_anim > 0.0f ? e->move : 0, .knocked = e->knock > 0.0f,
                                 .held = e->state == EN_HOSTAGE };
            const char *clip = anim_humanoid(&hs);
            // Muerto: el clip se queda en su ultimo tramo en vez de repetirse.
            float tt = dead ? fminf(CORPSE_SECONDS - e->corpse, 1.2f) : time + (float)i * 0.31f;
            props_draw_item_anim(props, it, at, e->yaw - PI / 2.0f, clip, tt);
            continue;
        }
        // Sin modelo: cuerpo articulado con su armadura (y su escudo).
        BodyPose b;
        pose_enemy(&b, e, time, i);
        body_draw(&b, at, e->yaw, enemy_colors(e->kind, e->hit_anim > 0.0f), &e->armor);
        if (e->shield && !dead) draw_shield_on(&b, at, e->yaw, (Color){ 168, 134, 84, 255 });
    }
    // El objetivo: un anillo dorado a sus pies, que late.
    const Enemy *te = cb_target(cb);
    if (te) {
        float pulse = 0.88f + 0.12f * sinf(time * 6.0f), r = (te->mounted ? 1.3f : 0.75f) * pulse;
        Vector3 c = { te->pos.x, te->pos.y + 0.06f, te->pos.z };
        for (int k = 0; k < 3; k++) DrawCircle3D(c, r - 0.04f * (float)k, (Vector3){ 1.0f, 0.0f, 0.0f }, 90.0f, (Color){ 232, 196, 92, 255 });
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
        body_draw_outfit(&b, pos, p->yaw, &ga->outfit);
        if (player_has_shield(ga)) draw_shield_on(&b, pos, p->yaw, (Color){ 150, 112, 70, 255 });
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

// Una barra de vida sobre alguien (la del objetivo, mas ancha y con marco dorado). false si no se ve.
static bool bar_at(Camera3D cam, Vector3 pos, float h, float frac, Color fill, int w, int hgt, bool target) {
    Vector3 to = Vector3Subtract(pos, cam.position);
    if (Vector3DotProduct(to, Vector3Subtract(cam.target, cam.position)) <= 0.0f || Vector3Length(to) > (target ? TARGET_RANGE + 8.0f : 30.0f))
        return false;
    Vector2 s = GetWorldToScreenEx((Vector3){ pos.x, pos.y + h, pos.z }, cam, w, hgt);
    int half = target ? 20 : 14;
    if (target) DrawRectangleLines((int)s.x - half - 1, (int)s.y - 3, 2 * half + 2, 6, UI_GOLD_LIGHT);
    DrawRectangle((int)s.x - half, (int)s.y - 2, 2 * half, 4, UI_LEATHER_CRACK);
    DrawRectangle((int)s.x - half + 1, (int)s.y - 1, (int)((2 * half - 2) * Clamp(frac, 0.0f, 1.0f)), 2, fill);
    return true;
}

void cb_draw_overlay(const Combat *cb, const GameActions *ga, const Troop *troop, Camera3D cam, int w, int h) {
    const Enemy *te = cb_target(cb);
    float blink = sinf((float)GetTime() * 30.0f) > 0.0f ? 1.0f : 0.55f;
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        const Enemy *e = &cb->enemies[i];
        bool target = e == te;
        if (!e->used || e->state == EN_DEAD) continue;
        if (e->state == EN_DOWN) { // abatido: una barra gris con lo que le falta para despertar
            bar_at(cam, e->pos, 0.7f, e->timer / DOWN_WAKE_MAX, UI_BONE_DIM, w, h, false);
            continue;
        }
        if (!target && e->shown <= 0.0f && e->target < 0) continue;
        float top = e->mounted ? 3.1f : 2.0f;
        if (!bar_at(cam, e->pos, top, e->h.hp / e->h.hp_max, UI_CARNELIAN, w, h, target)) continue;
        // Su golpe esta en la ventana del parry: un rombo que destella sobre la barra (K).
        if (e->target == 0 && melee_parry_in_window(e->windup)) {
            Vector2 s = GetWorldToScreenEx((Vector3){ e->pos.x, e->pos.y + top + 0.35f, e->pos.z }, cam, w, h);
            Color c = Fade(UI_GOLD_LIGHT, blink);
            DrawTriangle((Vector2){ s.x, s.y - 5 }, (Vector2){ s.x - 4, s.y }, (Vector2){ s.x + 4, s.y }, c);
            DrawTriangle((Vector2){ s.x - 4, s.y }, (Vector2){ s.x, s.y + 5 }, (Vector2){ s.x + 4, s.y }, c);
        }
    }
    // La espalda ganada: una daga sobre su cabeza (K lo ejecuta, G lo toma de rehen).
    if (g_back > 0 && g_back <= CB_MAX_ENEMIES && standing(&cb->enemies[g_back - 1])) {
        const Enemy *e = &cb->enemies[g_back - 1];
        Vector2 s = GetWorldToScreenEx((Vector3){ e->pos.x, e->pos.y + 2.25f, e->pos.z }, cam, w, h);
        ui_icon(ICON_DAGA, s.x - 8.0f, s.y - 8.0f, 16.0f, Fade(UI_GOLD_LIGHT, 0.75f + 0.25f * blink));
    }
    for (int k = 0; k < troop->count && k < TROOP_MAX; k++) {
        const Member *m = &troop->members[k];
        const Npc *n = &ga->npcs[k];
        if (m->status != STATUS_ACTIVE || n->member_id != m->id) continue;
        if (m->health.hp >= m->health.hp_max * 0.98f && !m->health.down) continue;
        bar_at(cam, n->pos, m->health.down ? 0.7f : 2.0f, m->health.hp / m->health.hp_max,
               health_bleeding(&m->health) ? UI_CARNELIAN : UI_TURQUOISE, w, h, false);
    }
}

void cb_draw_hud(const Combat *cb, const GameActions *ga, const Troop *troop, int right_x, int y, int w, int h) {
    (void)troop;
    const Health *ph = &cb->player;
    const char *txt = TextFormat("%s%s", health_state_name(ph), health_bleeding(ph) ? T(" · sangra") : "");
    Color col = health_bleeding(ph) || ph->down ? UI_CARNELIAN : ph->wound_count ? UI_GOLD : UI_BONE;
    hud_vital(health_bleeding(ph) ? ICON_SANGRE : ICON_VIDA, txt, col, fmaxf(0.0f, ph->hp) / ph->hp_max, UI_CARNELIAN, right_x, y);
    DrawRectangle(right_x - 80, y + 18, (int)(80 * ph->blood), 2, UI_LAPIS); // sangre

    // Arma a distancia: municion, tension y recarga, abajo al centro.
    const RangedDef *rd = ga->hands.sheathed ? NULL : ranged_def(ga->hands.right.id);
    if (rd) {
        const InvItem *wi = inventory_find(ga->inv, rd->weapon);
        const ProjectileDef *pd = projectile_def(rd->projectile);
        const char *fire = cb->arrow_lit                   ? TextFormat(T(" · ENCENDIDA (%.0f s)"), cb->arrow_lit_timer)
                           : ga->fire_near && !ga->raining && rd->projectile <= PROJ_BOLT ? T(" · Mayús+L: encender")
                                                                                           : "";
        const char *line = TextFormat("%s · %ss: %d%s%s", wi ? T(wi->name) : T("Arma"), T(pd->name), cb->ammo_shown,
                                      cb->reload > 0.0f ? T(" · recargando") : "", fire);
        int lw = MeasureText(line, 10);
        ui_text(line, w / 2 - lw / 2, h - 82, 10, UI_BONE); // sobre la barra rapida, que no la tape
        if (cb->aiming && rd->draw_time > 0.0f)
            ui_bar(w / 2 - 50, h - 70, 100, fminf(1.0f, cb->draw / rd->draw_time), UI_GOLD, UI_METAL_GOLD);
    }

    if (g_hint[0] && !ph->down) ui_text_centered(g_hint, w / 2, h - 96, 10, UI_TURQUOISE);

    if (ph->down) {
        DrawRectangle(0, 0, w, h, (Color){ 60, 0, 0, 90 });
        ui_text_centered(T("Estás abatido"), w / 2, h / 2 - 30, 20, UI_CARNELIAN);
        ui_text_centered(T("Si tu escolta está cerca, te levantará; si no, la tribu te buscará."), w / 2, h / 2 - 6, 10, UI_BONE);
    }
}

// Los cuatro botones de combate, abajo a la derecha (donde antes iban las habilidades): lo
// mismo que H J K L con el dedo o el raton. Cada casilla dice si se puede usar y cambia con lo
// que se empuña (arco: disparar y bajar el arma; escudo: cubrirse y cargar); el parry destella
// en la ventana justa.
void cb_draw_buttons(const Combat *cb, const GameActions *ga, int w, int h) {
    const int tile = 26, gap = 4, n = 4;
    const int x0 = w - 8 - n * tile - (n - 1) * gap, y = h - 52;
    const RangedDef *rd = ga->hands.sheathed ? NULL : ranged_def(ga->hands.right.id);
    const char *weapon = hands_attack_weapon(&ga->hands, 1, NULL);
    bool shield = player_has_shield(ga), riding = ga->mounted >= 0;
    bool free = !cb->player.down && cb->knock <= 0.0f && !ga->climbing && cb->stagger <= 0.0f && cb->exposed <= 0.0f;
    bool ready = free && cb->attack_cd <= 0.0f, cue = !rd && !riding && cb_parry_cue(cb);
    // La fase 2: con un rehen, K lo ejecuta y L lo empuja; con la espalda ganada, K ejecuta en
    // silencio; junto a un abatido, H y K lo rematan. G (agarrar o soltar) aparece a la izquierda.
    bool holding = cb->hostage != 0, back = g_back > 0, downed = g_capture_d <= CB_DOWNED_REACH && !riding;
    if (holding || back) {
        Rectangle g = { (float)(x0 - tile - gap - 6), (float)y, (float)tile, (float)tile };
        hud_plate((int)g.x - 3, y - 3, tile + 6, tile + 6);
        bool hv = hud_hover(g);
        ui_tile(g, ICON_GUANTES, holding, free && !rd);
        ui_tile_badge(g, keymap_text(KA_GRAB), UI_GOLD_LIGHT);
        if (hv)
            ui_legend(holding ? T("Soltar al rehén") : T("Agarrar: rehén"),
                      holding ? T("G: lo sueltas.") : T("G: lo agarras por el cuello y te cubres con él; los suyos no se acercan ni disparan."));
        g_hud_grab |= ui_click(g);
    }
    hud_plate(x0 - 3, y - 3, n * tile + (n - 1) * gap + 6, tile + 6);
    for (int i = 0; i < n; i++) {
        Rectangle r = { (float)(x0 + i * (tile + gap)), (float)y, (float)tile, (float)tile };
        IconId icon;
        bool on = false, ok = ready;
        const char *key, *title, *detail;
        if (i == 0) { // H: ataque
            key = keymap_text(KA_ATTACK);
            icon = rd ? icon_for_item(rd->weapon) : weapon[0] ? icon_for_item(weapon) : ICON_GOLPE;
            on = cb->attack_anim > 0.0f || cb->aiming;
            title = downed && !rd ? T("Rematar") : rd ? T("Disparar") : T("Atacar");
            detail = downed && !rd ? T("H junto a un abatido: lo rematas (a la tribu no le es indiferente).")
                     : rd ? T("H o clic: tensa y suelta. Apunta al objetivo (o al más cercano delante).")
                          : T("H o clic: golpe (encadena hasta tres). Mantener: golpe pesado.");
        } else if (i == 1) { // J: bloqueo
            key = keymap_text(KA_BLOCK);
            icon = shield ? ICON_ESCUDO : ICON_COBERTURA;
            on = cb->blocking;
            ok = free && !ga->hands.sheathed && (shield || rd || (weapon[0] && !player_winded()));
            title = rd ? T("Bajar el arma") : shield ? T("Cubrirse") : T("Parar con el arma");
            detail = rd       ? T("J: baja el arma (si llevas escudo, te cubre).")
                     : shield ? T("J o clic derecho, mantener: el escudo cubre el frente. Con H, golpe de escudo.")
                              : T("J o clic derecho, mantener: paras con el arma; aguanta la mitad y cansa.");
        } else if (i == 2 && (holding || back || (downed && !cue))) { // K: ejecutar o rematar
            key = keymap_text(KA_PARRY);
            icon = ICON_DAGA;
            on = holding || back;
            title = holding ? T("Ejecutar al rehén") : back ? T("Por la espalda") : T("Rematar");
            detail = holding ? T("K: lo ejecutas delante de los suyos.")
                     : back  ? T("K: lo matas en silencio; solo lo oyen los que están a menos de 6 m.")
                             : T("K junto a un abatido: lo rematas (a la tribu no le es indiferente).");
        } else if (i == 2) { // K: parry
            key = keymap_text(KA_PARRY);
            icon = ICON_PARADA;
            on = cue;
            ok = free && !rd && !riding && cb->parry_cd <= 0.0f;
            title = T("Parry");
            ParryKind k = melee_parry_kind(weapon);
            detail = rd       ? T("Con un arma a distancia no hay parry.")
                     : riding ? T("A caballo no hay parry.")
                     : k == PARRY_HOOK
                         ? T("K justo antes del golpe enemigo: enganchas su escudo y se lo arrancas. A destiempo, quedas expuesto.")
                     : k == PARRY_GRAPPLE
                         ? T("K justo antes del golpe enemigo: llave y derribo, y lo desarmas. A destiempo, quedas expuesto.")
                         : T("K justo antes del golpe enemigo: desvías y queda abierto un segundo. A destiempo, quedas expuesto.");
        } else if (holding) { // L: empujar al rehen
            key = keymap_text(KA_CHARGE);
            icon = ICON_PATADA;
            title = T("Empujar al rehén");
            detail = T("L: lo empujas y cae de bruces.");
        } else { // L: carga o patada
            key = keymap_text(KA_CHARGE);
            bool charge = shield && !rd;
            icon = charge ? ICON_CARGA : ICON_PATADA;
            on = cb->charge_timer > 0.0f;
            ok = ready && !riding;
            title = charge ? T("Carga con escudo") : T("Patada");
            detail = riding   ? T("A caballo no: solo golpe y golpe pesado.")
                     : charge ? T("L: carga con el escudo; empuja y tumba al primero que encuentra.")
                              : T("L: patada que empuja y desequilibra. Corriendo, patada a la carrera.");
        }
        bool hover = hud_hover(r);
        ui_tile(r, icon, on, ok);
        if (i == 2 && cue && fmodf((float)GetTime(), 0.24f) < 0.12f) // la ventana justa: destella
            DrawRectangleLinesEx((Rectangle){ r.x - 2, r.y - 2, r.width + 4, r.height + 4 }, 2.0f, UI_GOLD_LIGHT);
        ui_tile_badge(r, key, ok ? UI_GOLD_LIGHT : UI_BONE_DIM);
        if (hover) ui_legend(title, detail);
        bool press = ui_click(r), down = hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        if (i == 0) g_hud_attack_press |= press, g_hud_attack_down |= down;
        else if (i == 1) g_hud_block |= down;
        else if (i == 2) g_hud_parry |= press;
        else g_hud_charge |= press;
    }
}
