#include "hazards_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "sim/clock.h"
#include "ui/theme.h"
#include "sim/lang.h"

#define CLOTHING 12.0f       // abrigo de pieles (grados de sensacion termica)
#define FAINT_SECONDS 25.0f  // en hipotermia, hasta desmayarse
#define HOLE_RADIUS 1.4f
#define RESCUE_RANGE 3.0f
#define ESCORT_SPEED 5.5f
#define FIRE_WOOD_ID "utileria.objeto.lena"

static const int QTE_KEYMAP[QTE_KEYS] = { KEY_J, KEY_K, KEY_L, KEY_U, KEY_I, KEY_O };
static const char *QTE_LABELS[QTE_KEYS] = { "J", "K", "L", "U", "I", "O" };

void hz_init(Hazards *hz, unsigned seed) {
    memset(hz, 0, sizeof(*hz));
    hz->seed = seed;
    rng_seed(&hz->rng, seed ^ 0x4A2Du);
    hz->warmth.heat = WARMTH_MAX;
    hz->mud = 1.0f;
    hz->used_day = -1;
    hz->hole_day = -1;
}

bool hz_blocks_input(const Hazards *hz) { return hz->trap != TRAP_NONE || hz->qte.state == QTE_RUNNING; }

float hz_speed_scale(const Hazards *hz) {
    float s = hz->mud * warmth_speed_scale(&hz->warmth);
    if (hz->swimming) s *= 0.4f;
    else if (hz->wading) s *= 0.55f;
    return s;
}

static float dist2(Vector3 a, Vector3 b) { return Vector2Distance((Vector2){ a.x, a.z }, (Vector2){ b.x, b.z }); }

// ------------------------------------------------------------------- terreno
// Que socavon hay bajo (x, z) hoy, si hay. out recibe el socavon.
static SinkKind sinkhole_at(const Hazards *hz, const Terrain *t, int day, float x, float z, Sinkhole *out) {
    int cx = (int)floorf(x / SINK_CELL), cz = (int)floorf(z / SINK_CELL);
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            Sinkhole s;
            if (!hazard_sinkhole_cell(hz->seed, day, cx + dx, cz + dz, &s)) continue;
            // El bioma decide si existe: arena movediza en el desierto, socavon en la nieve del glaciar.
            SinkKind kind = SINK_NONE;
            if (biome_desert(hz->seed, s.x, s.z) > 0.6f) kind = SINK_QUICKSAND;
            else if (terrain_height(t, s.x, s.z) > t->look.snowline - 2.0f) kind = SINK_SNOW;
            if (kind == SINK_NONE) continue;
            if (Vector2Distance((Vector2){ x, z }, (Vector2){ s.x, s.z }) > s.radius + 2.5f) continue;
            *out = s;
            return kind;
        }
    return SINK_NONE;
}

static bool in_hole(const Hazards *hz, Vector3 p) {
    for (int i = 0; i < hz->hole_count; i++)
        if (dist2(hz->holes[i], p) < HOLE_RADIUS) return true;
    return false;
}

static void add_hole(Hazards *hz, Vector3 p) {
    if (hz->hole_count < HZ_MAX_HOLES) hz->holes[hz->hole_count++] = p;
}

static Npc *npc_for(GameActions *ga, const Troop *troop, int member_id) {
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
        if (ga->npcs[i].member_id == member_id) return &ga->npcs[i];
    return NULL;
}

static void dismount(GameActions *ga) {
    if (ga->mounted < 0) return;
    ga->animals[ga->mounted].ridden = false;
    ga->mounted = -1;
}

// Pierde lo que lleva y el arma de la mano derecha (se hunden).
static void lose_gear(GameActions *ga, char *what, size_t len) {
    what[0] = '\0';
    if (ga->hands.carried[0]) {
        snprintf(what, len, "%s", T("lo que cargabas"));
        ga->hands.carried[0] = '\0';
    }
    if (ga->hands.right.id[0] && !ga->hands.sheathed) {
        snprintf(what, len, "%s", what[0] ? T("lo que cargabas y tu arma") : T("tu arma"));
        memset(&ga->hands.right, 0, sizeof(ga->hands.right));
    }
}

// ------------------------------------------------------------------- trampas
static void start_self_trap(Hazards *hz, TrapKind kind, Player *p, GameActions *ga, const Troop *troop, char *log,
                            size_t len) {
    hz->trap = kind;
    hz->trap_pos = p->pos;
    hz->sink_depth = 0.0f;
    hz->qte_rescue = false;
    dismount(ga);
    // Un companero de la escolta cerca te tiende la mano: el desafio es mas facil.
    hz->helper = 0;
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        const Npc *n = &ga->npcs[i];
        if (n->escort && troop->members[i].status == STATUS_ACTIVE && n->member_id != hz->victim &&
            dist2(n->pos, p->pos) < 8.0f) {
            hz->helper = n->member_id;
            break;
        }
    }
    int keys = kind == TRAP_ICE ? 7 : kind == TRAP_SNOW ? 6 : 8;
    float per = kind == TRAP_SNOW ? 1.3f : 1.15f;
    int miss = 2;
    if (hz->helper) keys -= 2, per += 0.25f, miss++;
    qte_start(&hz->qte, &hz->rng, keys, per, miss);
    const Member *h = hz->helper ? troop_find((Troop *)troop, hz->helper) : NULL;
    const char *what = kind == TRAP_ICE ? T("¡El hielo se rompió bajo tus pies!")
                       : kind == TRAP_SNOW ? T("¡Caíste en un socavón de nieve!")
                                           : T("¡Arena movediza! Te hundes.");
    if (h) snprintf(log, len, T("%s %s te tiende la lanza."), what, h->name);
    else snprintf(log, len, "%s", what);
}

// Sale del agujero hacia atras (de donde venia) o fuera del socavon.
static void climb_out(Hazards *hz, Player *p, const Terrain *t, float away) {
    float back = p->yaw + PI;
    p->pos.x = hz->trap_pos.x + sinf(back) * away;
    p->pos.z = hz->trap_pos.z + cosf(back) * away;
    float ground = terrain_height(t, p->pos.x, p->pos.z);
    p->pos.y = fmaxf(ground, t->look.water_level);
    p->vy = 0.0f;
    p->grounded = true;
}

static void hurt(Hazards *hz, float dmg, WoundKind kind, int part) {
    if (hz->body) health_hit(hz->body, &hz->rng, dmg, kind, part);
}

static void resolve_self(Hazards *hz, bool won, Player *p, GameActions *ga, const Terrain *t, int day, char *log,
                         size_t len) {
    char lost[48];
    // Las caidas dejan marcas: golpes contra el hielo, una pierna torcida en el socavon.
    if (hz->trap == TRAP_ICE && !won) hurt(hz, 12.0f, WOUND_BRUISE, PART_RANDOM);
    if (hz->trap == TRAP_SNOW) hurt(hz, won ? 6.0f : 34.0f, WOUND_BRUISE, rng_range(&hz->rng, 2) ? PART_SHIN_L : PART_SHIN_R);
    switch (hz->trap) {
    case TRAP_ICE:
        hz->warmth.wet = 1.0f;
        if (won) {
            hz->warmth.heat = fmaxf(0.0f, hz->warmth.heat - 20.0f);
            snprintf(log, len, "%s", T("Saliste del agua helada. Estás empapado: busca un fuego."));
        } else {
            hz->warmth.heat = fminf(hz->warmth.heat, 5.0f);
            lose_gear(ga, lost, sizeof(lost));
            snprintf(log, len, T("Saliste a duras penas, helado%s%s."), lost[0] ? T("; se hundió ") : "", lost);
        }
        climb_out(hz, p, t, 2.6f);
        break;
    case TRAP_SNOW:
        hz->warmth.heat = fmaxf(0.0f, hz->warmth.heat - (won ? 8.0f : 30.0f));
        snprintf(log, len, "%s", won ? T("Trepaste fuera del socavón.") : T("Tardaste en salir del socavón: el frío te caló."));
        climb_out(hz, p, t, 4.5f);
        break;
    case TRAP_QUICKSAND:
        if (won) {
            snprintf(log, len, "%s", T("Te arrastraste fuera de la arena movediza."));
        } else {
            lose_gear(ga, lost, sizeof(lost));
            snprintf(log, len, T("Escapaste de la arena%s%s."), lost[0] ? T(", pero se tragó ") : "", lost);
        }
        climb_out(hz, p, t, 4.5f);
        break;
    default: break;
    }
    if (hz->trap == TRAP_SNOW || hz->trap == TRAP_QUICKSAND) { // este socavon ya no atrapa hoy
        hz->used_x = hz->trap_pos.x;
        hz->used_z = hz->trap_pos.z;
        hz->used_day = day;
    }
    hz->trap = TRAP_NONE;
    hz->helper = 0;
}

static void start_rescue(Hazards *hz, const Troop *troop, char *log, size_t len) {
    hz->qte_rescue = true;
    int keys = hz->victim_trap == TRAP_QUICKSAND ? 9 : 8;
    qte_start(&hz->qte, &hz->rng, keys, 1.1f, 2);
    const Member *m = troop_find((Troop *)troop, hz->victim);
    snprintf(log, len, T("Tiras de %s: ¡no lo sueltes!"), m ? m->name : T("tu compañero"));
}

static void resolve_rescue(Hazards *hz, bool won, GameActions *ga, Troop *troop, const Terrain *t, const Player *p,
                           char *log, size_t len) {
    hz->qte_rescue = false;
    Member *m = troop_find(troop, hz->victim);
    if (!won) {
        hz->victim_timer = fmaxf(1.0f, hz->victim_timer - 10.0f);
        snprintf(log, len, T("Se te escapa %s de las manos. ¡Inténtalo otra vez (F)!"), m ? m->name : "");
        return;
    }
    Npc *n = npc_for(ga, troop, hz->victim);
    if (n) { // lo deja junto al jugador, en suelo firme
        n->pos = (Vector3){ p->pos.x + 1.2f, 0.0f, p->pos.z };
        n->pos.y = fmaxf(terrain_height(t, n->pos.x, n->pos.z), t->look.water_level);
    }
    troop_adjust_morale(troop, 3.0f);
    snprintf(log, len, T("¡Salvaste a %s! La tribu lo celebra (moral +3)."), m ? m->name : T("tu compañero"));
    hz->victim = 0;
}

static void victim_trapped(Hazards *hz, const Troop *troop, int member_id, TrapKind kind, char *log, size_t len) {
    if (hz->victim) return;
    hz->victim = member_id;
    hz->victim_trap = kind;
    hz->victim_timer = kind == TRAP_ICE ? 45.0f : kind == TRAP_QUICKSAND ? 40.0f : 60.0f;
    const Member *m = troop_find((Troop *)troop, member_id);
    const char *where = kind == TRAP_ICE ? T("cayó al agua helada") : kind == TRAP_SNOW ? T("cayó en un socavón") : T("se hunde en la arena");
    snprintf(log, len, T("¡%s %s! Acércate y pulsa F."), m ? m->name : T("Un compañero"), where);
}

// -------------------------------------------------------------------- escolta
static void toggle_escort(Hazards *hz, GameActions *ga, const Troop *troop, const Player *p, float reach, char *log,
                          size_t len) {
    if (hz->escort_on) {
        for (int i = 0; i < TROOP_MAX; i++)
            if (ga->npcs[i].member_id != hz->victim) ga->npcs[i].escort = false;
        hz->escort_on = false;
        snprintf(log, len, "%s", T("La escolta vuelve al campamento."));
        return;
    }
    char names[64] = "";
    int picked = 0;
    for (int k = 0; k < HZ_ESCORT; k++) {
        int best = -1;
        float best_d = reach;
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
            const Npc *n = &ga->npcs[i];
            if (troop->members[i].status != STATUS_ACTIVE || n->escort || n->project >= 0 || n->job >= 0) continue;
            float d = dist2(n->pos, p->pos);
            if (d < best_d) best_d = d, best = i;
        }
        if (best < 0) break;
        ga->npcs[best].escort = true;
        size_t l = strlen(names);
        snprintf(names + l, sizeof(names) - l, "%s%s", picked ? T(" y ") : "", troop->members[best].name);
        picked++;
    }
    hz->escort_on = picked > 0;
    if (picked) snprintf(log, len, T("Te acompañan %s (Y: volver)."), names);
    else snprintf(log, len, "%s", T("No hay nadie libre cerca para acompañarte."));
}

static float surface_y(const Terrain *t, float x, float z) {
    float ground = terrain_height(t, x, z);
    if (t->look.water_level > ground + 0.25f)
        return hazard_ice_walkable(t->look.ice) ? t->look.water_level : fmaxf(ground, t->look.water_level - 1.25f);
    return ground;
}

static void update_escort(Hazards *hz, GameActions *ga, Troop *troop, const Terrain *t, const Player *p, int day,
                          float dt, bool roll, char *log, size_t len) {
    int k = 0;
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        Npc *n = &ga->npcs[i];
        if (!n->escort) continue;
        if (troop->members[i].status != STATUS_ACTIVE) {
            n->escort = false;
            continue;
        }
        int slot = k++;
        if (troop->members[i].health.down || n->fighting) continue; // abatido o peleando: no sigue al jugador
        if (n->member_id == hz->victim) { // atrapado: no se mueve, hundido
            float ground = terrain_height(t, n->pos.x, n->pos.z);
            n->pos.y = (hz->victim_trap == TRAP_ICE ? t->look.water_level : ground) - 1.0f;
            n->moving = false;
            continue;
        }
        // Detras del jugador, uno a cada lado.
        float side = slot == 0 ? 1.3f : -1.3f;
        Vector3 to = { p->pos.x - sinf(p->yaw) * 2.2f + cosf(p->yaw) * side, 0.0f,
                       p->pos.z - cosf(p->yaw) * 2.2f - sinf(p->yaw) * side };
        float d = dist2(n->pos, to);
        n->moving = d > 0.6f;
        float speed = 0.0f;
        if (n->moving) {
            speed = fminf(ESCORT_SPEED, d * 2.0f);
            float step = fminf(speed * dt, d);
            n->pos.x += (to.x - n->pos.x) / d * step;
            n->pos.z += (to.z - n->pos.z) / d * step;
            n->yaw = atan2f(to.x - n->pos.x, to.z - n->pos.z);
        }
        n->pos.y = surface_y(t, n->pos.x, n->pos.z);
        if (!roll || hz->victim) continue;
        // La escolta tambien puede romper el hielo o pisar un socavon.
        bool over_water = t->look.water_level > terrain_height(t, n->pos.x, n->pos.z) + 0.25f;
        if (over_water && hazard_ice_walkable(t->look.ice)) {
            if (in_hole(hz, n->pos) || rng_float(&hz->rng) < hazard_ice_break_chance(t->look.ice, speed, 1.0f) * 0.5f) {
                add_hole(hz, n->pos);
                victim_trapped(hz, troop, n->member_id, TRAP_ICE, log, len);
            }
            continue;
        }
        Sinkhole s;
        SinkKind kind = sinkhole_at(hz, t, day, n->pos.x, n->pos.z, &s);
        if (kind != SINK_NONE && dist2(n->pos, (Vector3){ s.x, 0, s.z }) < s.radius &&
            !(hz->used_day == day && fabsf(hz->used_x - s.x) < 0.1f))
            victim_trapped(hz, troop, n->member_id, kind == SINK_SNOW ? TRAP_SNOW : TRAP_QUICKSAND, log, len);
    }
    if (k == 0) hz->escort_on = false;
}

// ----------------------------------------------------------------------- frio
static float fire_warmth(const Props *props, Vector3 camp_fire, Vector3 p) {
    float best = 0.0f;
    struct { const char *id; float reach; } fires[] = {
        { "estructura.campamento.fogata", 6.0f },       { "estructura.campamento.hoguera", 10.0f },
        { "estructura.campamento.horno_cocina", 3.0f },  { "estructura.campamento.horno_bronce", 4.0f },
        { "estructura.campamento.horno_acero", 4.0f },
    };
    float d = dist2(camp_fire, p);
    if (d < 6.0f) best = 30.0f * (1.0f - d / 6.0f);
    for (int i = 0; i < props->count; i++)
        for (size_t f = 0; f < sizeof(fires) / sizeof(fires[0]); f++) {
            if (strcmp(props->items[i].item->id, fires[f].id) != 0) continue;
            float df = dist2(props->items[i].pos, p);
            if (df < fires[f].reach) best = fmaxf(best, 30.0f * (1.0f - df / fires[f].reach));
        }
    return best;
}

// ---------------------------------------------------------------- principal
void hz_update(Hazards *hz, const Climate *c, const Terrain *t, Player *p, GameActions *ga, const Props *props,
               Troop *troop, Vector3 camp_fire, float world_time, float dt, char *log, size_t log_len) {
    int day = clock_day(world_time);
    if (hz->hole_day != day) { // el hielo se cierra durante la noche
        hz->hole_count = 0;
        hz->hole_day = day;
    }
    hz->ice_roll += dt;
    bool roll = hz->ice_roll >= 0.5f; // tiradas de riesgo dos veces por segundo
    if (roll) hz->ice_roll = 0.0f;

    if (IsKeyPressed(KEY_Y) && hz->trap == TRAP_NONE) toggle_escort(hz, ga, troop, p, 60.0f, log, log_len);
    update_escort(hz, ga, troop, t, p, day, dt, roll, log, log_len);

    // Minijuego en curso.
    if (hz->qte.state == QTE_RUNNING) {
        for (int k = 0; k < QTE_KEYS; k++)
            if (IsKeyPressed(QTE_KEYMAP[k])) qte_press(&hz->qte, k);
        qte_update(&hz->qte, dt);
        if (hz->qte.state != QTE_RUNNING) {
            bool won = hz->qte.state == QTE_WON;
            if (hz->qte_rescue) resolve_rescue(hz, won, ga, troop, t, p, log, log_len);
            else resolve_self(hz, won, p, ga, t, day, log, log_len);
        }
    }

    // Companero atrapado: el tiempo corre.
    if (hz->victim) {
        Npc *vn = npc_for(ga, troop, hz->victim);
        if (!hz->qte_rescue || hz->qte.state != QTE_RUNNING) hz->victim_timer -= dt;
        if (hz->victim_timer <= 0.0f || !vn) {
            Member *m = troop_find(troop, hz->victim);
            const char *how = hz->victim_trap == TRAP_ICE ? T("se ahogó bajo el hielo")
                              : hz->victim_trap == TRAP_SNOW ? T("murió de frío en el socavón")
                                                             : T("desapareció en la arena");
            if (m) snprintf(log, log_len, T("%s %s. La tribu está de luto (moral -6)."), m->name, how);
            troop_mourn(troop, hz->victim, 6.0f);
            if (vn) vn->escort = false;
            hz->victim = 0;
            if (hz->qte_rescue) hz->qte.state = QTE_IDLE, hz->qte_rescue = false;
        } else if (hz->trap == TRAP_NONE && hz->qte.state != QTE_RUNNING && dist2(vn->pos, p->pos) < RESCUE_RANGE &&
                   IsKeyPressed(KEY_F)) {
            start_rescue(hz, troop, log, log_len);
        }
    }

    // Atrapado: hundido hasta que se resuelva el minijuego.
    if (hz->trap != TRAP_NONE) {
        if (hz->trap == TRAP_QUICKSAND) hz->sink_depth = fminf(1.3f, hz->sink_depth + dt * 0.06f);
        p->pos = hz->trap_pos;
        p->vy = 0.0f;
        p->draw_lift = hz->trap == TRAP_ICE ? -1.0f : hz->trap == TRAP_SNOW ? -1.4f : -0.5f - hz->sink_depth;
        if (hz->trap == TRAP_ICE) hz->warmth.wet = 1.0f;
    }

    // Terreno bajo los pies: hielo, vadeo, nado, barro.
    float ground = terrain_height(t, p->pos.x, p->pos.z);
    float water = t->look.water_level;
    bool over_water = water > ground + 0.25f;
    hz->on_ice = hz->wading = hz->swimming = false;
    if (hz->trap == TRAP_NONE && over_water && !ga->climbing) {
        if (hazard_ice_walkable(t->look.ice)) {
            hz->on_ice = true;
            if (p->pos.y < water) {
                p->pos.y = water;
                p->vy = 0.0f;
                p->grounded = true;
            }
            float speed = p->moving ? (p->stance == STANCE_RUN ? 7.5f : 4.0f) * p->speed_scale : 0.0f;
            float load = ga->mounted >= 0 ? 2.5f : 1.0f;
            if (p->grounded &&
                (in_hole(hz, p->pos) || (roll && rng_float(&hz->rng) < hazard_ice_break_chance(t->look.ice, speed, load) * 0.5f))) {
                add_hole(hz, p->pos);
                start_self_trap(hz, TRAP_ICE, p, ga, troop, log, log_len);
            }
        } else if (water - ground < 1.2f) {
            hz->wading = true;
            hz->warmth.wet = fminf(1.0f, hz->warmth.wet + dt / 3.0f);
        } else {
            hz->swimming = true;
            hz->warmth.wet = 1.0f;
            float float_y = water - 1.25f;
            if (p->pos.y < float_y) {
                p->pos.y = float_y;
                p->vy = 0.0f;
                p->grounded = true;
            }
        }
    }
    float desert = biome_desert(hz->seed, p->pos.x, p->pos.z);
    hz->mud = desert > 0.5f || over_water ? 1.0f : hazard_mud_scale(c->wetness, c->snow_cover);

    // Socavones ocultos: una pista sutil al acercarse; pisarlo atrapa.
    Sinkhole s;
    SinkKind kind = hz->trap == TRAP_NONE ? sinkhole_at(hz, t, day, p->pos.x, p->pos.z, &s) : SINK_NONE;
    bool used = kind != SINK_NONE && hz->used_day == day && fabsf(hz->used_x - s.x) < 0.1f && fabsf(hz->used_z - s.z) < 0.1f;
    hz->danger = kind != SINK_NONE && !used;
    if (hz->danger) {
        hz->danger_pos = (Vector3){ s.x, terrain_height(t, s.x, s.z), s.z };
        hz->danger_radius = s.radius;
        hz->danger_kind = kind;
        if (dist2(p->pos, hz->danger_pos) < s.radius && p->grounded && !ga->climbing) {
            hz->trap_pos = p->pos;
            start_self_trap(hz, kind == SINK_SNOW ? TRAP_SNOW : TRAP_QUICKSAND, p, ga, troop, log, log_len);
            hz->trap_pos = hz->danger_pos; // al centro del socavon
            hz->used_x = s.x, hz->used_z = s.z, hz->used_day = day;
        }
    }

    // Frio: sensacion termica y calor corporal.
    hz->fire = ga->fires_out ? 0.0f : fire_warmth(props, camp_fire, p->pos); // apagados por la lluvia, no calientan
    float shelter = dist2(p->pos, camp_fire) < 14.0f ? 6.0f : 0.0f; // las yurtas cortan el viento
    float torch = ga->torch_lit && !ga->hands.sheathed ? 4.0f : 0.0f;
    float wind = c->wind * (shelter > 0.0f ? 0.5f : 1.0f);
    hz->feels = hazard_feels_like(c->temperature, wind, hz->warmth.wet, CLOTHING + shelter + torch, hz->fire);
    warmth_update(&hz->warmth, hz->feels, c->rain, hz->fire > 5.0f, dt);
    if (ga->warmth_boost > 0.0f) { // el calor del ambar
        hz->warmth.heat = fminf(100.0f, hz->warmth.heat + ga->warmth_boost);
        ga->warmth_boost = 0.0f;
    }
    if (hz->trap == TRAP_ICE) hz->warmth.heat = fmaxf(0.0f, hz->warmth.heat - 1.5f * dt); // el agua helada quema

    // Hipotermia: congela manos y pies; tras unos segundos, desmayo y la tribu lo lleva al fuego.
    if (warmth_level(&hz->warmth) == COLD_HYPOTHERMIA && hz->trap == TRAP_NONE) {
        hz->frost_timer += dt;
        if (hz->frost_timer > 10.0f) {
            hz->frost_timer = 0.0f;
            hurt(hz, 5.0f, WOUND_FROSTBITE, health_random_limb(&hz->rng));
        }
        hz->faint_timer += dt;
        if (hz->faint_timer > FAINT_SECONDS) {
            dismount(ga);
            p->pos = (Vector3){ camp_fire.x + 2.5f, terrain_height(t, camp_fire.x + 2.5f, camp_fire.z), camp_fire.z };
            p->vy = 0.0f;
            hz->warmth.heat = 35.0f;
            hz->warmth.wet = 0.0f;
            hz->faint_timer = 0.0f;
            troop_adjust_morale(troop, -3.0f);
            snprintf(log, log_len, "%s", T("Te desmayaste de frío; la tribu te llevó junto al fuego (moral -3)."));
        }
    } else {
        hz->faint_timer = 0.0f;
    }
}

void hz_new_day(Hazards *hz, const Climate *c, GameActions *ga, const Props *props, Troop *troop, char *log,
                size_t log_len) {
    (void)hz;
    if (c->temp_mean >= 0.0f) return;
    // Cada fuego del campamento gasta lena en las noches heladas (el doble con frio extremo).
    int fires = 1;
    for (int i = 0; i < props->count; i++) {
        const char *id = props->items[i].item->id;
        if (!strcmp(id, "estructura.campamento.fogata")) fires += 1;
        else if (!strcmp(id, "estructura.campamento.hoguera")) fires += 2;
    }
    int need = fires * (c->temp_mean < -10.0f ? 2 : 1);
    int have = stock_count(&ga->stock, FIRE_WOOD_ID);
    if (have >= need) {
        stock_take(&ga->stock, FIRE_WOOD_ID, need);
        return;
    }
    if (have > 0) stock_take(&ga->stock, FIRE_WOOD_ID, have);
    troop_adjust_morale(troop, -4.0f);
    snprintf(log, log_len, T("Noche helada: faltó leña (%d de %d). La tribu pasó frío (moral -4)."), have, need);
}

// ------------------------------------------------------------------ dibujo
void hz_draw_world(const Hazards *hz, const Terrain *t, const GameActions *ga, const Troop *troop, float time) {
    float water = t->look.water_level;
    // Agujeros en el hielo: agua negra con esquirlas alrededor.
    for (int i = 0; i < hz->hole_count; i++) {
        Vector3 h = { hz->holes[i].x, water + 0.02f, hz->holes[i].z };
        DrawCylinder(h, HOLE_RADIUS, HOLE_RADIUS, 0.03f, 12, (Color){ 18, 34, 48, 255 });
        for (int k = 0; k < 8; k++) {
            float a = k * PI / 4.0f + i;
            DrawCube((Vector3){ h.x + cosf(a) * (HOLE_RADIUS + 0.2f), h.y + 0.04f, h.z + sinf(a) * (HOLE_RADIUS + 0.2f) },
                     0.35f, 0.08f, 0.2f, (Color){ 214, 232, 244, 255 });
        }
    }
    // Pista de un socavon oculto: apenas un cerco de grietas (nieve) o de arena humeda.
    if (hz->danger && hz->trap == TRAP_NONE) {
        Color col = hz->danger_kind == SINK_SNOW ? (Color){ 150, 170, 190, 70 } : (Color){ 150, 120, 80, 70 };
        Vector3 c = { hz->danger_pos.x, hz->danger_pos.y + 0.05f, hz->danger_pos.z };
        DrawCircle3D(c, hz->danger_radius * (0.9f + 0.05f * sinf(time * 2.0f)), (Vector3){ 1, 0, 0 }, 90.0f, col);
        DrawCircle3D(c, hz->danger_radius * 0.55f, (Vector3){ 1, 0, 0 }, 90.0f, col);
    }
    // Trampa del jugador: el borde se agita.
    if (hz->trap != TRAP_NONE) {
        Vector3 c = hz->trap_pos;
        c.y = (hz->trap == TRAP_ICE ? water : terrain_height(t, c.x, c.z)) + 0.05f;
        Color col = hz->trap == TRAP_ICE ? (Color){ 230, 240, 250, 255 } : hz->trap == TRAP_SNOW ? (Color){ 240, 244, 248, 255 }
                                                                                                : (Color){ 170, 140, 96, 255 };
        DrawCircle3D(c, 1.0f + 0.15f * sinf(time * 6.0f), (Vector3){ 1, 0, 0 }, 90.0f, col);
        if (hz->trap != TRAP_ICE) DrawCylinder((Vector3){ c.x, c.y - 0.04f, c.z }, 1.6f, 1.6f, 0.03f, 12,
                                                hz->trap == TRAP_SNOW ? (Color){ 120, 140, 160, 255 } : (Color){ 150, 118, 76, 255 });
    }
    // Companero atrapado: ondas a su alrededor.
    if (hz->victim) {
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
            const Npc *n = &ga->npcs[i];
            if (n->member_id != hz->victim) continue;
            Vector3 c = { n->pos.x, n->pos.y + 1.0f, n->pos.z };
            if (hz->victim_trap == TRAP_ICE)
                DrawCylinder((Vector3){ c.x, water + 0.02f, c.z }, HOLE_RADIUS, HOLE_RADIUS, 0.03f, 12, (Color){ 18, 34, 48, 255 });
            for (int k = 0; k < 2; k++)
                DrawCircle3D(c, 0.8f + fmodf(time * 0.8f + k * 0.5f, 1.0f), (Vector3){ 1, 0, 0 }, 90.0f,
                             (Color){ 220, 235, 250, 200 });
        }
    }
}

static Color heat_color(const Warmth *w) {
    switch (warmth_level(w)) {
    case COLD_WARM: return UI_GOLD;
    case COLD_COOL: return UI_TURQUOISE;
    case COLD_COLD: return UI_LAPIS;
    default: return UI_CARNELIAN;
    }
}

static void draw_qte(const Hazards *hz, const Troop *troop, int width, int height) {
    const Qte *q = &hz->qte;
    const int w = 330, h = 104, x0 = (width - w) / 2, y0 = height / 2 - 12; // bajo el panel del HUD
    ui_panel((Rectangle){ (float)x0, (float)y0, (float)w, (float)h }, UI_METAL_GOLD);
    char title[96];
    if (hz->qte_rescue) {
        const Member *m = troop_find((Troop *)troop, hz->victim);
        snprintf(title, sizeof(title), T("¡Saca a %s!"), m ? m->name : T("tu compañero"));
    } else {
        snprintf(title, sizeof(title), "%s", hz->trap == TRAP_ICE ? T("¡El hielo se rompió!")
                                             : hz->trap == TRAP_SNOW ? T("¡Socavón de nieve!")
                                                                     : T("¡Arena movediza!"));
    }
    ui_text_centered(title, width / 2, y0 + 12, 20, UI_GOLD_LIGHT);
    // Las teclas de la serie: hechas en oro, la actual en turquesa, el resto apagadas.
    const int box = 22, gap = 5, total = q->len * box + (q->len - 1) * gap;
    int x = width / 2 - total / 2, y = y0 + 40;
    for (int i = 0; i < q->len; i++, x += box + gap) {
        Color fill = i < q->pos ? UI_GOLD : i == q->pos ? UI_TURQUOISE : UI_LEATHER;
        Color txt = i < q->pos ? UI_LEATHER : i == q->pos ? UI_LEATHER : UI_BONE_DIM;
        DrawRectangle(x, y, box, box, fill);
        DrawRectangleLines(x, y, box, box, i == q->pos ? UI_BONE : UI_GOLD);
        ui_text_centered(QTE_LABELS[q->keys[i]], x + box / 2, y + 6, 10, txt);
    }
    // Tiempo de la tecla actual y errores.
    float frac = q->per_key > 0.0f ? q->time_left / q->per_key : 0.0f;
    ui_bar(x0 + 20, y0 + 72, w - 40, frac, frac < 0.35f ? UI_CARNELIAN : UI_TURQUOISE, UI_METAL_GOLD);
    char foot[96];
    snprintf(foot, sizeof(foot), T("Pulsa las teclas en orden  ·  errores %d/%d"), q->mistakes, q->max_mistakes);
    ui_text_centered(foot, width / 2, y0 + 84, 10, UI_BONE);
}

void hz_draw_hud(const Hazards *hz, const Troop *troop, int right_x, int y, int width, int height) {
    // Calor corporal y terreno, alineados a la derecha bajo el minimapa.
    const char *ground = hz->on_ice ? T(" · hielo") : hz->swimming ? T(" · nadando") : hz->wading ? T(" · vadeando")
                         : hz->mud < 0.9f ? T(" · barro") : "";
    const char *txt = TextFormat("%s%s%s", cold_name(warmth_level(&hz->warmth)), hz->warmth.wet > 0.3f ? T(" · mojado") : "",
                                 ground);
    ui_text(txt, right_x - MeasureText(txt, 10), y, 10, heat_color(&hz->warmth));
    ui_bar(right_x - 92, y + 12, 92, hz->warmth.heat / WARMTH_MAX, heat_color(&hz->warmth), UI_METAL_SILVER);

    if (hz->victim && !(hz->qte_rescue && hz->qte.state == QTE_RUNNING)) {
        const Member *m = troop_find((Troop *)troop, hz->victim);
        const char *msg = TextFormat(T("¡%s está atrapado! Acércate y pulsa F (%d s)"), m ? m->name : T("Un compañero"),
                                     (int)ceilf(hz->victim_timer));
        int w = MeasureText(msg, 10) + 24;
        ui_strip((Rectangle){ (float)(width - w) / 2, 168, (float)w, 20 }, UI_METAL_GOLD);
        ui_text_centered(msg, width / 2, 174, 10, UI_CARNELIAN);
    }
    if (hz->qte.state == QTE_RUNNING) draw_qte(hz, troop, width, height);
}

// ------------------------------------------------------------------- prueba
void hz_force(Hazards *hz, const char *what, Player *p, GameActions *ga, Troop *troop, const Terrain *t) {
    char log[128];
    (void)t;
    if (!strcmp(what, "hielo")) start_self_trap(hz, TRAP_ICE, p, ga, troop, log, sizeof(log));
    else if (!strcmp(what, "nieve")) start_self_trap(hz, TRAP_SNOW, p, ga, troop, log, sizeof(log));
    else if (!strcmp(what, "arena")) start_self_trap(hz, TRAP_QUICKSAND, p, ga, troop, log, sizeof(log));
    else if (!strcmp(what, "rescate")) {
        toggle_escort(hz, ga, troop, p, 1e9f, log, sizeof(log));
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
            Npc *n = &ga->npcs[i];
            if (!n->escort) continue;
            n->pos = (Vector3){ p->pos.x + 1.5f, p->pos.y, p->pos.z - 1.0f };
            victim_trapped(hz, troop, n->member_id, TRAP_ICE, log, sizeof(log));
            start_rescue(hz, troop, log, sizeof(log));
            break;
        }
    }
}
