#include "game/city_game.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "game/camp_game.h"
#include "game/inventory_game.h"
#include "raymath.h"
#include "rlgl.h"
#include "sim/clock.h"
#include "sim/lang.h"
#include "sim/trade.h"
#include "ui/theme.h"
#include "world/body_draw.h"
#include "world/hearth.h"
#include "world/voxstruct.h"

#define CITY_DRAW 280.0f  // m: los asentamientos se dibujan hasta aca
#define FIELD_DRAW 200.0f // los campos y los corrales
#define FOLK_DRAW 120.0f  // la gente
#define SIEGE_CAMP_R 220.0f     // m: un campamento de la tribu asi de cerca de una ciudad en guerra la cerca
#define EXPEDITION_NEAR 150.0f  // m: con el jugador asi de cerca del campamento, la expedicion llega a la vista
#define SALLY_NEAR 45.0f        // m de la muralla (o del centro de la aldea): sale la guarnicion
#define SALLY_COOLDOWN 60.0f    // s entre una salida y otra
#define GATE_BURN_SECONDS 20.0f // lo que tarda en quemarse una puerta
#define CITY_FIRES 8            // techos ardiendo a la vez (flechas encendidas que caen adentro)

static float dist_xz(float ax, float az, float bx, float bz) { return sqrtf((ax - bx) * (ax - bx) + (az - bz) * (az - bz)); }

// ------------------------------------------------------------------ la economia
// Las ciudades que todavia no se armaron (partida nueva, o de antes de la v0.9.0).
static void ensure(GameActions *ga, const World *w) {
    for (int i = 0; i < w->settlement_count && i < SETTLEMENTS_MAX; i++)
        if (ga->cities[i].people <= 0.0f)
            city_start(&ga->cities[i], w->settlements[i].kind, w->settlements[i].region, w->seed ^ (uint32_t)(i + 1) * 7919u);
}

// ------------------------------------------------------------------ el trazado
static CityLot g_lots[SETTLEMENTS_MAX][CITY_LOTS_MAX];
static int g_nlots[SETTLEMENTS_MAX], g_key[SETTLEMENTS_MAX]; // g_key: de que gente es el trazado + 1 (0: sin hacer)
static const World *g_world;
static uint32_t g_seed; // world_for_seed reusa sus huecos: el mismo puntero puede ser otro mundo

const CityLot *ct_lots(const GameActions *ga, const Terrain *t, int idx, int *n) {
    const World *w = t ? t->world : NULL;
    *n = 0;
    if (!w || idx < 0 || idx >= w->settlement_count || idx >= SETTLEMENTS_MAX) return g_lots[0];
    if (w != g_world || w->seed != g_seed) memset(g_key, 0, sizeof(g_key)), g_world = w, g_seed = w->seed;
    const City *c = &ga->cities[idx];
    int key = 1 + (int)(c->people / 25.0f) * 8 + (c->mercs + 11) / 12; // se rehace cuando cambia la gente
    if (g_key[idx] != key) g_nlots[idx] = city_layout(w, idx, c, g_lots[idx], CITY_LOTS_MAX), g_key[idx] = key;
    *n = g_nlots[idx];
    return g_lots[idx];
}

int ct_field_at(const GameActions *ga, const Terrain *t, float x, float z, int *field) {
    const World *w = t ? t->world : NULL;
    for (int i = 0; w && i < w->settlement_count && i < SETTLEMENTS_MAX; i++) {
        if (dist_xz(w->settlements[i].x, w->settlements[i].z, x, z) > CITY_FIELDS_R1 + 12.0f) continue;
        int n;
        const CityLot *lots = ct_lots(ga, t, i, &n);
        int k = city_lot_at(lots, n, LOT_FIELD, x, z, 1.0f);
        if (k < 0) continue;
        if (field) {
            *field = 0;
            for (int j = 0; j < k; j++) *field += lots[j].kind == LOT_FIELD;
        }
        return i;
    }
    return -1;
}

// La muralla deja sus cuatro puertas a 15, 105, 195 y 285 grados (ver draw_walls).
Vector3 ct_gate_toward(const Terrain *t, int idx, float x, float z) {
    const Settlement *s = &t->world->settlements[idx];
    Vector3 out = { s->x, terrain_height(t, s->x, s->z), s->z };
    if (s->kind != SETTLE_CAPITAL) return out;
    float best = 1e30f;
    for (int k = 0; k < 4; k++) {
        float a = (15.0f + 90.0f * (float)k) * DEG2RAD, gx = s->x + cosf(a) * (CITY_WALL_R + 3.0f), gz = s->z + sinf(a) * (CITY_WALL_R + 3.0f);
        float d = dist_xz(gx, gz, x, z);
        if (d < best) best = d, out = (Vector3){ gx, terrain_height(t, gx, gz), gz };
    }
    return out;
}

// ------------------------------------------------------------------ la guerra
// La capital del kanato (la de la estepa), o -1.
static int khanate_idx(const World *w) {
    for (int i = 0; w && i < w->settlement_count && i < SETTLEMENTS_MAX; i++)
        if (w->settlements[i].kind == SETTLE_CAPITAL && w->settlements[i].region == REGION_STEPPE) return i;
    return -1;
}

static bool khanate_fallen(const GameActions *ga, const World *w) {
    int k = khanate_idx(w);
    return k >= 0 && ga->cities[k].taken;
}

// ¿Cuenta la relacion con el reino al que se rinde tributo? En la estepa, mientras el kanato no caiga.
static bool overlord_of(const GameActions *ga, const World *w, const Troop *troop, int idx) {
    return troop && troop->overlord && w->settlements[idx].region == REGION_STEPPE && !khanate_fallen(ga, w);
}

static bool at_war(const GameActions *ga, const World *w, const Troop *troop, int idx) {
    if (!w || idx < 0 || idx >= w->settlement_count || idx >= SETTLEMENTS_MAX) return false;
    bool ov = overlord_of(ga, w, troop, idx);
    return city_at_war(&ga->cities[idx], ov, ov ? troop->overlord->relation : 0.0f);
}

bool ct_at_war(const GameActions *ga, const Terrain *t, const Troop *troop, int idx) { return t && at_war(ga, t->world, troop, idx); }

bool ct_khanate_fallen(const GameActions *ga, const Terrain *t) { return t && khanate_fallen(ga, t->world); }

// Un punto de la muralla de la capital s: en el angulo ang, a out m de ella (afuera, positivo).
static Vector3 wall_point(const Terrain *t, const Settlement *s, float ang, float out) {
    float r = city_wall_dist(ang) + out, x = s->x + cosf(ang) * r, z = s->z + sinf(ang) * r;
    return (Vector3){ x, terrain_height(t, x, z), z };
}

static float seg_mid(int seg) { return ((float)seg + 0.5f) * 2.0f * PI / (float)CITY_WALL_SEGMENTS; }

// ------------------------------------------------------------------ el asedio
static float g_sally_cd[SETTLEMENTS_MAX];             // s hasta que la guarnicion pueda salir de nuevo
static float g_gate_fire[SETTLEMENTS_MAX][CITY_GATES]; // s que le quedan ardiendo a cada puerta (0: no arde)
typedef struct {
    Vector3 at;
    float t; // s que le quedan
} CityFire;
static CityFire g_fires[CITY_FIRES]; // techos que arden (flechas encendidas que cayeron adentro)
static Vector3 g_last;               // donde estaba el jugador el cuadro anterior (la muralla corta el paso)
static bool g_last_ok;
static char g_hint[96];

Vector3 ct_route(const GameActions *ga, const Terrain *t, Vector3 from, Vector3 to) {
    const World *w = t ? t->world : NULL;
    for (int i = 0; w && i < w->settlement_count && i < SETTLEMENTS_MAX; i++) {
        const Settlement *s = &w->settlements[i];
        if (s->kind != SETTLE_CAPITAL || dist_xz(from.x, from.z, s->x, s->z) > CITY_WALL_R + 40.0f) continue;
        float fx = from.x - s->x, fz = from.z - s->z;
        if (!city_wall_blocks(NULL, false, fx, fz, to.x - s->x, to.z - s->z, NULL)) continue; // las suyas, abiertas
        bool inside = sqrtf(fx * fx + fz * fz) < city_wall_dist(atan2f(fz, fx));
        float best = 1e30f;
        Vector3 out = to;
        for (int g = 0; g < CITY_GATES; g++) {
            float a = city_gate_angle(g);
            Vector3 o = wall_point(t, s, a, 4.0f), n = wall_point(t, s, a, -4.0f), nearp = inside ? n : o, farp = inside ? o : n;
            float cost = dist_xz(from.x, from.z, nearp.x, nearp.z) + dist_xz(farp.x, farp.z, to.x, to.z);
            if (cost < best) best = cost, out = dist_xz(from.x, from.z, nearp.x, nearp.z) < 2.5f ? farp : nearp;
        }
        (void)ga;
        return out;
    }
    return to;
}

bool ct_shot_wall(const GameActions *ga, const Terrain *t, Vector3 from, Vector3 to, bool burning, Vector3 *at) {
    const World *w = t ? t->world : NULL;
    const Troop *troop = ga->troop_ref;
    for (int i = 0; w && i < w->settlement_count && i < SETTLEMENTS_MAX; i++) {
        const Settlement *s = &w->settlements[i];
        if (s->kind != SETTLE_CAPITAL || dist_xz(to.x, to.z, s->x, s->z) > CITY_WALL_R + 20.0f) continue;
        float x0 = from.x - s->x, z0 = from.z - s->z, x1 = to.x - s->x, z1 = to.z - s->z;
        int gate = -1;
        bool war = at_war(ga, w, troop, i);
        if (!city_wall_blocks(&ga->cities[i], war, x0, z0, x1, z1, &gate)) continue;
        float s0 = sqrtf(x0 * x0 + z0 * z0) - city_wall_dist(atan2f(z0, x0)), s1 = sqrtf(x1 * x1 + z1 * z1) - city_wall_dist(atan2f(z1, x1));
        float f = s0 / (s0 - s1);
        Vector3 hit = Vector3Lerp(from, to, f);
        if (hit.y > terrain_height(t, hit.x, hit.z) + CITY_WALL_HEIGHT) continue; // por encima de la muralla
        *at = hit;
        // Encendida en una puerta cerrada: la puerta arde.
        float a = gate >= 0 ? city_gate_angle(gate) : 0.0f, side = -(hit.x - s->x) * sinf(a) + (hit.z - s->z) * cosf(a);
        if (burning && war && gate >= 0 && fabsf(side) <= CITY_GATE_HALF && g_gate_fire[i][gate] <= 0.0f && !ga->raining)
            g_gate_fire[i][gate] = GATE_BURN_SECONDS, g_sally_cd[i] = 0.0f;
        return true;
    }
    return false;
}

bool ct_wall_ahead(const GameActions *ga, const Terrain *t, const Player *p, Vector3 *top, Vector3 *over, Vector3 *to) {
    const World *w = t ? t->world : NULL;
    for (int i = 0; w && i < w->settlement_count && i < SETTLEMENTS_MAX; i++) {
        const Settlement *s = &w->settlements[i];
        if (s->kind != SETTLE_CAPITAL || dist_xz(p->pos.x, p->pos.z, s->x, s->z) > CITY_WALL_R + 10.0f) continue;
        float rx = p->pos.x - s->x, rz = p->pos.z - s->z, r = sqrtf(rx * rx + rz * rz), ang = atan2f(rz, rx), wd = city_wall_dist(ang);
        if (r < 1.0f || fabsf(r - wd) > 3.5f) continue;
        float ux = rx / r, uz = rz / r, side = r > wd ? 1.0f : -1.0f; // side: de que lado esta (afuera, +)
        if (-(sinf(p->yaw) * ux + cosf(p->yaw) * uz) * side < 0.5f) continue; // tiene que mirar a la muralla
        // ¿Hay muralla (o una puerta cerrada) delante? Por una puerta abierta se pasa caminando.
        bool war = at_war(ga, w, ga->troop_ref, i);
        if (!city_wall_blocks(&ga->cities[i], war, ux * (wd + side), uz * (wd + side), ux * (wd - side), uz * (wd - side), NULL)) continue;
        Vector3 near_p = wall_point(t, s, ang, 0.6f * side), far_p = wall_point(t, s, ang, -0.6f * side), land = wall_point(t, s, ang, -1.8f * side);
        float h = fmaxf(near_p.y, far_p.y) + CITY_WALL_HEIGHT;
        *top = (Vector3){ near_p.x, h, near_p.z };
        *over = (Vector3){ far_p.x, h, far_p.z };
        *to = land;
        return true;
    }
    return false;
}

// Junto a la muralla de una capital: subir por una escala apoyada (de cualquier lado), apoyar la tuya
// en un tramo de muro (en guerra, desde afuera) o prender una puerta cerrada con la antorcha.
typedef enum { SPOT_NONE, SPOT_LADDER, SPOT_PLANT, SPOT_TORCH } SiegeSpot;

static SiegeSpot siege_spot(GameActions *ga, const Props *props, const Terrain *t, const Player *p, int *city, int *which) {
    const World *w = t ? t->world : NULL;
    for (int i = 0; w && i < w->settlement_count && i < SETTLEMENTS_MAX; i++) {
        const Settlement *s = &w->settlements[i];
        if (s->kind != SETTLE_CAPITAL || dist_xz(p->pos.x, p->pos.z, s->x, s->z) > CITY_WALL_R + 10.0f) continue;
        const City *c = &ga->cities[i];
        *city = i;
        for (int k = 0; k < CITY_WALL_SEGMENTS; k++) { // una escala apoyada: por afuera o por adentro
            if (!((c->ladders >> k) & 1)) continue;
            Vector3 o = wall_point(t, s, seg_mid(k), 1.4f), n = wall_point(t, s, seg_mid(k), -1.6f);
            if (dist_xz(p->pos.x, p->pos.z, o.x, o.z) < 2.2f || dist_xz(p->pos.x, p->pos.z, n.x, n.z) < 2.2f) return *which = k, SPOT_LADDER;
        }
        if (!at_war(ga, w, ga->troop_ref, i)) continue;
        float rx = p->pos.x - s->x, rz = p->pos.z - s->z, r = sqrtf(rx * rx + rz * rz), ang = atan2f(rz, rx), wd = city_wall_dist(ang);
        if (ga->torch_lit)
            for (int g = 0; g < CITY_GATES; g++) {
                if (city_gate_open(c, g, true) || g_gate_fire[i][g] > 0.0f) continue;
                Vector3 o = wall_point(t, s, city_gate_angle(g), 1.0f), n = wall_point(t, s, city_gate_angle(g), -1.0f);
                if (dist_xz(p->pos.x, p->pos.z, o.x, o.z) < 3.5f || dist_xz(p->pos.x, p->pos.z, n.x, n.z) < 3.5f) return *which = g, SPOT_TORCH;
            }
        int seg = city_wall_segment(ang);
        if (r > wd && r - wd < 3.0f && city_segment_gate(seg) < 0 && !((c->ladders >> seg) & 1) && ig_carried_count(ga, props, p, LADDER_ID) > 0)
            return *which = seg, SPOT_PLANT;
    }
    return SPOT_NONE;
}

bool ct_try_interact(GameActions *ga, const Props *props, const Terrain *t, Player *p, char *log, size_t len) {
    int i = -1, k = -1;
    SiegeSpot spot = siege_spot(ga, props, t, p, &i, &k);
    if (spot == SPOT_NONE) return false;
    const Settlement *s = &t->world->settlements[i];
    City *c = &ga->cities[i];
    if (spot == SPOT_LADDER) {
        if (ga->mounted >= 0) {
            snprintf(log, len, "%s", T("Desmonta antes de subir por la escala (R)."));
            return true;
        }
        float a = seg_mid(k), rx = p->pos.x - s->x, rz = p->pos.z - s->z, side = sqrtf(rx * rx + rz * rz) > city_wall_dist(a) ? 1.0f : -1.0f;
        Vector3 near_p = wall_point(t, s, a, 0.6f * side), far_p = wall_point(t, s, a, -0.6f * side), land = wall_point(t, s, a, -1.8f * side);
        float h = fmaxf(near_p.y, far_p.y) + CITY_WALL_HEIGHT;
        p->yaw = atan2f(far_p.x - p->pos.x, far_p.z - p->pos.z);
        ga_climb(ga, p, (Vector3){ near_p.x, h, near_p.z }, (Vector3){ far_p.x, h, far_p.z }, land);
        snprintf(log, len, side > 0.0f ? T("Subes por la escala a la muralla de %s.") : T("Pasas la muralla de %s y bajas por la escala."), s->name);
    } else if (spot == SPOT_PLANT) {
        if (ig_take_carried(ga, props, p, LADDER_ID, 1) <= 0) return false;
        c->ladders |= 1 << k;
        snprintf(log, len, T("Apoyas la escala en la muralla de %s: F para subir."), s->name);
    } else {
        g_gate_fire[i][k] = GATE_BURN_SECONDS;
        g_sally_cd[i] = 0.0f; // la guarnicion sale a apagarla
        snprintf(log, len, T("Prendes fuego a la puerta de %s con la antorcha: arde."), s->name);
    }
    return true;
}

const char *ct_hint(void) { return g_hint; }

void ct_draw_hud(int w, int h) {
    if (g_hint[0]) ui_text_centered(g_hint, w / 2, h - 96, 10, UI_GOLD);
}

bool ct_workshop_at(const GameActions *ga, const Terrain *t, float x, float z) {
    const World *w = t ? t->world : NULL;
    for (int i = 0; w && i < w->settlement_count && i < SETTLEMENTS_MAX; i++)
        if (w->settlements[i].kind == SETTLE_CAPITAL && ga->cities[i].taken && dist_xz(x, z, w->settlements[i].x, w->settlements[i].z) < CITY_WALL_R)
            return true;
    return false;
}

int ct_gate_near(const GameActions *ga, const Terrain *t, const Troop *troop, float x, float z, float r, int *city) {
    const World *w = t ? t->world : NULL;
    for (int i = 0; w && i < w->settlement_count && i < SETTLEMENTS_MAX; i++) {
        const Settlement *s = &w->settlements[i];
        if (s->kind != SETTLE_CAPITAL || dist_xz(x, z, s->x, s->z) > CITY_WALL_R + r + 5.0f || !at_war(ga, w, troop, i)) continue;
        for (int g = 0; g < CITY_GATES; g++) {
            Vector3 o = wall_point(t, s, city_gate_angle(g), 0.0f);
            if (!city_gate_open(&ga->cities[i], g, true) && dist_xz(x, z, o.x, o.z) < r) return *city = i, g;
        }
    }
    return -1;
}

static void clear_expedition(GameActions *ga);

void ct_demand_surrender(GameActions *ga, Combat *cb, const Terrain *t, Troop *troop, int idx, int day, char *log, size_t len) {
    const World *w = t ? t->world : NULL;
    if (!w || idx < 0 || idx >= w->settlement_count || idx >= SETTLEMENTS_MAX) return;
    const Settlement *s = &w->settlements[idx];
    City *c = &ga->cities[idx];
    if (!at_war(ga, w, troop, idx)) {
        snprintf(log, len, T("%s no está en guerra contigo."), s->name);
        return;
    }
    if (!city_surrenders(c, s->kind)) {
        snprintf(log, len, T("%s no se rinde: le quedan %d soldados (se rinde con %d), grano para %d días y el descontento es del %d %%."), s->name,
                 c->soldiers + c->mercs, city_garrison_floor(c, s->kind), c->people > 0.0f ? (int)(c->grain / c->people) : 0, (int)(c->unrest * 100.0f));
        return;
    }
    bool khanate = idx == khanate_idx(w), capital = s->kind == SETTLE_CAPITAL;
    city_take(c, day);
    c->gates_burnt = 0, c->ladders = 0;
    for (int g = 0; g < CITY_GATES; g++) g_gate_fire[idx][g] = 0.0f;
    for (int i = 0; i < CB_MAX_ENEMIES; i++) // los suyos dejan las armas y se van (tambien su expedicion de castigo)
        if (cb->enemies[i].used && cb->enemies[i].city == idx + 1 && cb->enemies[i].state != EN_DEAD) cb->enemies[i].used = false;
    if (ga->expedition_city == idx + 1) clear_expedition(ga);
    if (khanate) snprintf(log, len, T("¡%s se rinde! El kanato cae: ya no le debes tributo. Su fundición es tu taller y te pagará tributo cada estación."), s->name);
    else if (capital) snprintf(log, len, T("¡%s se rinde y abre sus puertas! Su fundición es tu taller y te pagará tributo cada estación."), s->name);
    else snprintf(log, len, T("¡%s se rinde! Te pagará tributo cada estación."), s->name);
}

// ------------------------------------------------------------------ las expediciones de castigo
// El campamento contra el que marcha: el que tiene cerca el jugador; si no, el mas cercano a la ciudad.
static int target_camp(const GameActions *ga, const Settlement *s) {
    int best = -1;
    float bd = 1e30f;
    for (int k = 0; k < CAMPS_MAX; k++) {
        const CampSite *c = &ga->camps[k];
        if (!c->used || c->migrating) continue;
        if (dist_xz(c->x, c->z, ga->player_pos.x, ga->player_pos.z) < EXPEDITION_NEAR) return k;
        float d = dist_xz(c->x, c->z, s->x, s->z);
        if (d < bd) bd = d, best = k;
    }
    return best;
}

// Los que defienden: los guardias y soldados de la tribu que estan (no de viaje).
static int camp_defenders(const GameActions *ga, const Troop *troop) {
    int n = 0;
    for (int i = 0; troop && i < troop->count; i++) {
        const Member *m = &troop->members[i];
        n += m->status == STATUS_ACTIVE && (m->role == ROLE_GUARD || m->role == ROLE_SOLDIER) && !ga_away(ga, m);
    }
    return n;
}

// La expedicion saquea el campamento: se lleva parte de la comida, el grano y la plata del acopio
// (menos con defensores, que se cobran a algunos) y el enojo se calma.
static void sack(GameActions *ga, const World *w, Troop *troop, int city, int camp, int men, char *log, size_t len) {
    CampSite *cs = &ga->camps[camp];
    City *c = &ga->cities[city];
    const Settlement *s = &w->settlements[city];
    int defenders = camp_defenders(ga, troop);
    int taken = stock_plunder(&cs->stock, punitive_share(men, defenders));
    city_lose_soldiers(c, defenders / 3 < men ? defenders / 3 : men);
    c->grudge = fmaxf(0.0f, c->grudge - 40.0f);
    if (overlord_of(ga, w, troop, city)) troop->overlord->relation = fminf(100.0f, troop->overlord->relation + PUNITIVE_APPEASE);
    char msg[240];
    snprintf(msg, sizeof(msg), T("La expedición de castigo de %s saqueó %s: se llevó %d de comida, grano y plata del acopio."), s->name, cs->name, taken);
    snprintf(log, len, "%s", msg);
    cg_news(ga, camp, msg);
}

static void clear_expedition(GameActions *ga) {
    ga->expedition_city = ga->expedition_camp = ga->expedition_lost = 0;
    ga->expedition_out = ga->expedition_sacked = false;
    memset(&ga->expedition_army, 0, sizeof(ga->expedition_army));
}

// Al empezar el dia: de la capital del kanato (con la relacion por el suelo) o de una ciudad con
// mucho rencor sale una expedicion, si su tesoro la paga. Con el jugador cerca del campamento llega a
// la vista (ct_update la saca al mundo); si no, lo saquea y lo cuenta el guardian.
static void punitive(GameActions *ga, const World *w, Troop *troop, int day, char *log, size_t len) {
    if (ga->expedition_city) return; // ya hay una en marcha
    for (int i = 0; i < w->settlement_count && i < SETTLEMENTS_MAX; i++) {
        const Settlement *s = &w->settlements[i];
        City *c = &ga->cities[i];
        bool ov = overlord_of(ga, w, troop, i) && s->kind == SETTLE_CAPITAL;
        if (c->people <= 0.0f || c->taken || !punitive_due(ov, ov ? troop->overlord->relation : 0.0f, c->grudge, day, c->expedition)) continue;
        c->expedition = day; // aunque no le alcance: lo vuelve a intentar la estacion que viene
        ArmyComp a;
        int camp = target_camp(ga, s);
        if (camp < 0 || army_compose(c, s->kind, PUNITIVE_MAX_UNITS, true, &a) <= 0) continue;
        c->silver = fmaxf(0.0f, c->silver - army_cost(&a));
        const CampSite *cs = &ga->camps[camp];
        if (dist_xz(cs->x, cs->z, ga->player_pos.x, ga->player_pos.z) < EXPEDITION_NEAR) {
            clear_expedition(ga);
            ga->expedition_city = i + 1, ga->expedition_camp = camp + 1, ga->expedition_army = a;
        } else {
            sack(ga, w, troop, i, camp, army_units(&a), log, len);
        }
        return;
    }
}

// La expedicion en marcha: sale al mundo a 80 m del campamento; si llega a el, lo saquea y se vuelve;
// si cae, se cuenta; si el jugador se aleja (y desaparece de la vista), llega igual.
static void expedition_tick(GameActions *ga, Combat *cb, Troop *troop, const Terrain *t, const Player *p, char *log, size_t len) {
    if (!ga->expedition_city) return;
    const World *w = t->world;
    int ci = ga->expedition_city - 1, k = ga->expedition_camp - 1;
    if (ci >= w->settlement_count || ci >= SETTLEMENTS_MAX || k < 0 || k >= CAMPS_MAX || !ga->camps[k].used || ga->camps[k].migrating) {
        clear_expedition(ga); // el campamento ya no esta: se vuelven
        return;
    }
    const Settlement *s = &w->settlements[ci];
    const CampSite *cs = &ga->camps[k];
    Vector3 campv = { cs->x, terrain_height(t, cs->x, cs->z), cs->z };
    int units = army_units(&ga->expedition_army);
    if (!ga->expedition_out) {
        float dx = s->x - cs->x, dz = s->z - cs->z, d = sqrtf(dx * dx + dz * dz) + 1e-3f;
        Vector3 from = { cs->x + dx / d * 80.0f, 0.0f, cs->z + dz / d * 80.0f };
        if (dist_xz(from.x, from.z, p->pos.x, p->pos.z) > 140.0f) { // del lado de su ciudad queda lejos: de costado al jugador
            float px = p->pos.x - cs->x, pz = p->pos.z - cs->z, pd = sqrtf(px * px + pz * pz) + 1e-3f;
            from = (Vector3){ cs->x + (px * 0.5f - pz * 0.87f) / pd * 80.0f, 0.0f, cs->z + (pz * 0.5f + px * 0.87f) / pd * 80.0f };
        }
        from.y = terrain_height(t, from.x, from.z);
        ga->expedition_out = true;
        if (cb_spawn_army(cb, t, ci, &ga->expedition_army, from, campv, true) <= 0) { // sin lugar en el mundo: llega igual
            sack(ga, w, troop, ci, k, units, log, len);
            clear_expedition(ga);
            return;
        }
        const ArmyComp *a = &ga->expedition_army;
        snprintf(log, len, T("¡Una expedición de castigo de %s marcha sobre %s: %d infantes, %d tiradores y %d carros de guerra!"), s->name, cs->name,
                 a->n[ARMY_HEAVY] + a->n[ARMY_SOLDIER], a->n[ARMY_ARCHER] + a->n[ARMY_CROSSBOW], a->n[ARMY_CHARIOT]);
        return;
    }
    int alive = cb_punitive_alive(cb);
    if (alive > 0) {
        if (!ga->expedition_sacked && cb_punitive_near(cb, campv, 14.0f)) { // llego: saquea y se vuelve
            sack(ga, w, troop, ci, k, alive, log, len);
            ga->expedition_sacked = true;
            cb_punitive_withdraw(cb, ct_gate_toward(t, ci, cs->x, cs->z));
        }
        return;
    }
    if (!ga->expedition_sacked) {
        if (ga->expedition_lost * 2 >= units) {
            char msg[240];
            snprintf(msg, sizeof(msg), T("¡Cae la expedición de castigo de %s! %d de los suyos quedaron en el campo."), s->name, ga->expedition_lost);
            snprintf(log, len, "%s", msg);
            cg_news(ga, k, msg);
        } else {
            sack(ga, w, troop, ci, k, units - ga->expedition_lost, log, len); // te alejaste: llegaron igual
        }
    }
    clear_expedition(ga);
}

// ------------------------------------------------------------------ dia a dia
// ¿Esta cercada? En guerra, con un campamento de la tribu cerca.
static bool besieged(const GameActions *ga, const World *w, const Troop *troop, int i) {
    if (!at_war(ga, w, troop, i)) return false;
    const Settlement *s = &w->settlements[i];
    for (int k = 0; k < CAMPS_MAX; k++) {
        const CampSite *c = &ga->camps[k];
        if (c->used && !c->migrating && dist_xz(c->x, c->z, s->x, s->z) < SIEGE_CAMP_R) return true;
    }
    return false;
}

// El acopio adonde va el tributo de las ciudades tomadas: el del campamento inicial (o el primero que
// haya).
static int home_camp(const GameActions *ga) {
    if (ga->camps[0].used && !ga->camps[0].migrating) return 0;
    for (int k = 1; k < CAMPS_MAX; k++)
        if (ga->camps[k].used && !ga->camps[k].migrating) return k;
    return -1;
}

void ct_new_day(GameActions *ga, const Terrain *t, Troop *troop, int day, char *log, size_t len) {
    const World *w = t ? t->world : NULL;
    if (!w) return;
    ensure(ga, w);
    Season s = clock_season(day);
    for (int i = 0; i < w->settlement_count && i < SETTLEMENTS_MAX; i++) {
        const Settlement *st = &w->settlements[i];
        City *c = &ga->cities[i];
        bool sieged = besieged(ga, w, troop, i); // cercada: los campos no se trabajan (sin cosecha)
        city_day(c, st->kind, st->region, s, sieged ? 0.0f : city_season_luck(w->seed, i, day));
        if (!at_war(ga, w, troop, i)) c->gates_burnt = 0, c->ladders = 0; // en paz rehacen sus puertas y quitan las escalas
        if (!sieged) continue;
        city_siege_day(c, st->kind);
        if (c->besieged != day - 1) snprintf(log, len, T("%s está cercada: no entra grano y la gente del campo se refugia adentro."), st->name);
        c->besieged = day;
    }
    // El tributo: al empezar una estacion, si la anterior paso sin pagarlo, el kan se enoja. La
    // primera vez que se revisa (partida nueva o vieja) solo se anota. Caido el kanato, nada.
    int si = season_index(day);
    if (si > 0 && ga->tribute_checked != si + 1) {
        bool first = ga->tribute_checked == 0;
        ga->tribute_checked = si + 1;
        if (!first && ga->tribute_season != si && troop && troop->overlord && !khanate_fallen(ga, w)) {
            troop->overlord->relation = fmaxf(-100.0f, troop->overlord->relation - TRIBUTE_MISSED);
            snprintf(log, len, T("El %s no recibió tu tributo la estación pasada: la relación baja (-%d). Se paga en su capital."),
                     T(troop->overlord->name), (int)TRIBUTE_MISSED);
            cg_news(ga, 0, log); // y lo cuenta el guardian del campamento inicial (otro aviso del dia puede taparlo)
        }
    }
    // Las ciudades tomadas pagan su tributo al empezar cada estacion, al acopio del campamento inicial.
    if (si > 0 && ga->vassal_season != si + 1) {
        ga->vassal_season = si + 1;
        int total = 0, camp = home_camp(ga);
        for (int i = 0; i < w->settlement_count && i < SETTLEMENTS_MAX && camp >= 0; i++)
            if (ga->cities[i].taken) total += city_vassal_tribute(&ga->cities[i], w->settlements[i].kind);
        if (total > 0 && stock_add(&ga->camps[camp].stock, SILVER_ID, total)) {
            char msg[240];
            snprintf(msg, sizeof(msg), T("Las ciudades tomadas te pagan su tributo: %d de plata, al acopio de %s."), total, ga->camps[camp].name);
            snprintf(log, len, "%s", msg);
            cg_news(ga, camp, msg);
        }
    }
    punitive(ga, w, troop, day, log, len);
}

// La muralla corta el paso del jugador: si el paso de este cuadro la cruza (donde esta cerrada), se
// queda de este lado (desliza a lo largo si puede). Trepando, no; un salto grande es un viaje.
static void wall_blocks_player(const GameActions *ga, const World *w, const Troop *troop, Player *p) {
    if (!g_last_ok || ga->climbing) return;
    float step = dist_xz(g_last.x, g_last.z, p->pos.x, p->pos.z);
    if (step < 1e-4f || step > 3.0f) return;
    for (int i = 0; i < w->settlement_count && i < SETTLEMENTS_MAX; i++) {
        const Settlement *s = &w->settlements[i];
        if (s->kind != SETTLE_CAPITAL || dist_xz(p->pos.x, p->pos.z, s->x, s->z) > CITY_WALL_R + 8.0f) continue;
        bool war = at_war(ga, w, troop, i);
        float x0 = g_last.x - s->x, z0 = g_last.z - s->z, x1 = p->pos.x - s->x, z1 = p->pos.z - s->z;
        if (!city_wall_blocks(&ga->cities[i], war, x0, z0, x1, z1, NULL)) continue;
        float r0 = sqrtf(x0 * x0 + z0 * z0) + 1e-4f, ux = x0 / r0, uz = z0 / r0, dx = x1 - x0, dz = z1 - z0, along = dx * ux + dz * uz;
        float sx = x0 + dx - ux * along, sz = z0 + dz - uz * along; // solo de costado, a lo largo de la muralla
        if (!city_wall_blocks(&ga->cities[i], war, x0, z0, sx, sz, NULL)) p->pos.x = s->x + sx, p->pos.z = s->z + sz;
        else p->pos.x = g_last.x, p->pos.z = g_last.z;
        return;
    }
}

static void add_fire(Vector3 at) {
    int k = 0;
    for (int i = 1; i < CITY_FIRES; i++)
        if (g_fires[i].t < g_fires[k].t) k = i;
    g_fires[k] = (CityFire){ at, 25.0f };
}

void ct_update(GameActions *ga, Combat *cb, Troop *troop, const Props *props, const Terrain *t, Player *p, int day, float dt, char *log,
               size_t len) {
    if (!t || !t->world) return;
    const World *w = t->world;
    ensure(ga, w);
    for (int i = 0; i < CB_MAX_ENEMIES; i++) { // la guardia y el ejercito que caen: la ciudad pierde un soldado
        Enemy *e = &cb->enemies[i];
        if (!e->used || e->state != EN_DEAD) continue;
        if (e->city > 0 && e->city <= SETTLEMENTS_MAX) city_lose_soldiers(&ga->cities[e->city - 1], 1);
        if (e->punitive) ga->expedition_lost++; // y la expedicion de castigo, un hombre
        e->city = 0, e->punitive = false;
    }
    expedition_tick(ga, cb, troop, t, p, log, len);
    wall_blocks_player(ga, w, troop, p);
    g_last = p->pos, g_last_ok = true;
    // Las puertas que arden: al consumirse quedan abiertas; la lluvia las apaga.
    for (int i = 0; i < w->settlement_count && i < SETTLEMENTS_MAX; i++)
        for (int g = 0; g < CITY_GATES; g++) {
            if (g_gate_fire[i][g] <= 0.0f) continue;
            if (ga->raining) {
                g_gate_fire[i][g] = 0.0f;
                snprintf(log, len, T("La lluvia apaga el fuego de la puerta de %s."), w->settlements[i].name);
            } else if ((g_gate_fire[i][g] -= dt) <= 0.0f) {
                g_gate_fire[i][g] = 0.0f;
                ga->cities[i].gates_burnt |= 1 << g;
                snprintf(log, len, T("La puerta de %s se consume: queda abierta."), w->settlements[i].name);
            }
        }
    // Las flechas encendidas que caen adentro de una ciudad en guerra: arde un techo y parte del granero.
    for (int k = 0; k < ga->ignite_n; k++) {
        Vector3 at = ga->ignite_at[k];
        for (int i = 0; i < w->settlement_count && i < SETTLEMENTS_MAX; i++) {
            const Settlement *s = &w->settlements[i];
            float d = dist_xz(at.x, at.z, s->x, s->z);
            bool inside = s->kind == SETTLE_CAPITAL ? d < city_wall_dist(atan2f(at.z - s->z, at.x - s->x)) - 1.0f : d < 20.0f;
            if (!inside || !at_war(ga, w, troop, i)) continue;
            int burnt = city_burn_grain(&ga->cities[i]);
            add_fire(at);
            if (burnt > 0) snprintf(log, len, T("Arde un techo en %s: se queman %d raciones de su granero."), s->name, burnt);
            break;
        }
    }
    for (int i = 0; i < CITY_FIRES; i++) g_fires[i].t = fmaxf(0.0f, g_fires[i].t - dt);
    // La guarnicion sale a pelear si te acercas (y no hay nadie suyo afuera): de la puerta mas
    // cercana, o del cuartel si ya estas adentro.
    for (int i = 0; i < w->settlement_count && i < SETTLEMENTS_MAX; i++) {
        g_sally_cd[i] = fmaxf(0.0f, g_sally_cd[i] - dt);
        const Settlement *s = &w->settlements[i];
        bool cap = s->kind == SETTLE_CAPITAL;
        float d = dist_xz(p->pos.x, p->pos.z, s->x, s->z), wd = cap ? city_wall_dist(atan2f(p->pos.z - s->z, p->pos.x - s->x)) : 0.0f;
        if (d > wd + SALLY_NEAR || g_sally_cd[i] > 0.0f || !at_war(ga, w, troop, i) || cb_city_standing(cb, i) > 0) continue;
        bool inside = cap && d < wd;
        ArmyComp a;
        if (army_compose(&ga->cities[i], s->kind, inside ? 8 : 6, false, &a) <= 0) continue;
        Vector3 from = ct_gate_toward(t, i, p->pos.x, p->pos.z);
        if (inside) {
            int n;
            const CityLot *lots = ct_lots(ga, t, i, &n);
            for (int l = 0; l < n; l++)
                if (lots[l].kind == LOT_BARRACKS) from = (Vector3){ lots[l].x, terrain_height(t, lots[l].x, lots[l].z), lots[l].z };
        }
        int made = cb_spawn_army(cb, t, i, &a, from, p->pos, false);
        g_sally_cd[i] = SALLY_COOLDOWN;
        if (made > 0) snprintf(log, len, T("¡Sale la guarnición de %s: %d hombres!"), s->name, made);
    }
    // Lo que hace F aqui (la escala, la puerta).
    int ci = -1, k = -1;
    g_hint[0] = '\0';
    if (!ga->dlg.open && !ga->menu_open && !ga->inv_open && !ga->climbing) {
        SiegeSpot spot = siege_spot(ga, props, t, p, &ci, &k);
        if (spot == SPOT_LADDER) snprintf(g_hint, sizeof(g_hint), "%s", T("F: subir por la escala"));
        else if (spot == SPOT_PLANT) snprintf(g_hint, sizeof(g_hint), "%s", T("F: apoyar la escala en la muralla"));
        else if (spot == SPOT_TORCH) snprintf(g_hint, sizeof(g_hint), "%s", T("F: prender la puerta con la antorcha"));
    }
    (void)day;
}

static void add(char *out, size_t len, const char *fmt, ...) {
    size_t n = strlen(out);
    if (n + 1 >= len) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out + n, len - n, fmt, ap);
    va_end(ap);
}

void ct_report(const GameActions *ga, const Terrain *t, const Troop *troop, int idx, int day, char *out, size_t len) {
    out[0] = '\0';
    const World *w = t ? t->world : NULL;
    if (!w || idx < 0 || idx >= w->settlement_count || idx >= SETTLEMENTS_MAX) return;
    const Settlement *s = &w->settlements[idx];
    const City *c = &ga->cities[idx];
    Season season = clock_season(day);
    float luck = city_season_luck(w->seed, idx, day);
    const char *harvest = season == SEASON_WINTER ? T("En invierno los campos no dan.")
                          : c->fields < 0.5f  ? T("Los campos están arrasados.")
                          : luck > 1.1f       ? T("La cosecha de esta estación viene buena.")
                          : luck < 0.9f       ? T("La cosecha de esta estación viene mala.")
                                              : T("La cosecha viene como siempre.");
    add(out, len, T("%s: %d almas. El granero da para %d días. %s"), s->name, (int)c->people, c->people > 0.0f ? (int)(c->grain / c->people) : 0,
        harvest);
    add(out, len, "\n");
    add(out, len, T("Tesoro: %d de plata (los escribas cobran %d al día). Armería: %d juegos de armas."), (int)c->silver,
        (int)(city_taxes(c, s->kind) + 0.5f), (int)c->arms);
    add(out, len, "\n");
    if (c->mercs > 0) add(out, len, T("Guarnición: %d soldados y %d mercenarios."), c->soldiers, c->mercs);
    else add(out, len, T("Guarnición: %d soldados."), c->soldiers);
    if (c->grudge >= CITY_GRUDGE_NO_TRADE) add(out, len, " %s", T("No comercian contigo: les saqueaste los campos."));
    else if (c->grudge > 15.0f) add(out, len, " %s", T("Te guardan rencor por los saqueos."));
    if (c->taken) {
        add(out, len, "\n");
        add(out, len, s->kind == SETTLE_CAPITAL ? T("Es de la tribu: te paga tributo cada estación y su fundición es tu taller.")
                                                : T("Es de la tribu: te paga tributo cada estación."));
    } else if (at_war(ga, w, troop, idx)) {
        add(out, len, "\n");
        add(out, len, T("En guerra con la tribu%s. Se rinde con %d soldados o menos, sin grano o en revuelta (descontento: %d %%)."),
            c->besieged >= day - 1 && c->besieged > 0 ? T(" y cercada") : "", city_garrison_floor(c, s->kind), (int)(c->unrest * 100.0f));
    }
    if (overlord_of(ga, w, troop, idx)) {
        add(out, len, "\n");
        add(out, len, T("Relación con el %s: %d."), T(troop->overlord->name), (int)troop->overlord->relation);
        if (ga->tribute_season == season_index(day) + 1) add(out, len, " %s", T("El tributo de esta estación, pagado."));
        else add(out, len, " %s", T("El tributo de esta estación, sin pagar (en su capital)."));
    }
}

// ------------------------------------------------------------------ dibujo
static Color folk_color(Region rg, int k) {
    static const Color C[REGION_COUNT][2] = {
        { { 60, 90, 140, 255 }, { 150, 60, 50, 255 } },  { { 70, 96, 58, 255 }, { 120, 90, 60, 255 } },
        { { 130, 110, 80, 255 }, { 90, 80, 110, 255 } }, { { 90, 110, 130, 255 }, { 140, 130, 110, 255 } },
        { { 230, 226, 210, 255 }, { 180, 120, 70, 255 } },
    };
    return C[rg][k & 1];
}

static const Color SOLDIER_RED = { 150, 40, 36, 255 };

static void person(Vector3 pos, float yaw, Color cloth, float walk_phase, float walk, float attack) {
    BodyPose pose;
    BodyPoseParams bp = { .walk_phase = walk_phase, .walk = walk, .attack = attack, .scale = 1.0f };
    body_pose(&pose, &bp);
    BodyColors bc = { { 200, 160, 120, 255 }, cloth, { 84, 64, 46, 255 } };
    body_draw(&pose, pos, yaw, bc, NULL);
}

// Un guardia con su lanza en alto (yaw: hacia donde mira, como body_draw).
static void guard(Vector3 pos, float yaw) {
    person(pos, yaw, SOLDIER_RED, 0.0f, 0.0f, 0.0f);
    Vector3 r = { cosf(yaw), 0.0f, -sinf(yaw) }; // su derecha
    Vector3 a = { pos.x + r.x * 0.38f, pos.y + 0.15f, pos.z + r.z * 0.38f }, b = { a.x, a.y + 2.6f, a.z };
    DrawCylinderEx(a, b, 0.03f, 0.03f, 4, (Color){ 104, 78, 52, 255 });
    DrawCylinderEx(b, (Vector3){ b.x, b.y + 0.3f, b.z }, 0.06f, 0.0f, 4, (Color){ 170, 170, 176, 255 });
}

// Un punto del lote en el mundo: along, a lo largo de su giro; across, de lado.
static Vector3 lot_point(const Terrain *t, const CityLot *l, float along, float across) {
    float ca = cosf(l->yaw), sa = sinf(l->yaw), x = l->x + ca * along - sa * across, z = l->z + sa * along + ca * across;
    return (Vector3){ x, terrain_height(t, x, z), z };
}

// Un campo: los lomos de tierra a lo largo y lo sembrado encima, segun la estacion (en otoño, el
// trigo dorado y despues el rastrojo con sus gavillas). Arrasado: tierra negra.
static void draw_field(const Terrain *t, const CityLot *l, Season s, int dos, bool burnt) {
    int rows = (int)(l->w / 1.8f);
    if (rows < 3) rows = 3;
    float rw = l->w / (float)rows, ch = 0.0f;
    Color soil = burnt ? (Color){ 38, 32, 30, 255 } : s == SEASON_WINTER ? (Color){ 96, 82, 70, 255 } : (Color){ 112, 84, 58, 255 }, crop = soil;
    if (!burnt) {
        if (s == SEASON_SPRING) crop = (Color){ 122, 162, 78, 255 }, ch = 0.1f + 0.03f * (float)dos;
        else if (s == SEASON_SUMMER) crop = (Color){ 94, 140, 56, 255 }, ch = 0.4f + 0.04f * (float)dos;
        else if (s == SEASON_AUTUMN && dos <= 4) crop = (Color){ 214, 178, 82, 255 }, ch = 0.75f;
        else if (s == SEASON_AUTUMN) crop = (Color){ 198, 178, 122, 255 }, ch = 0.12f; // el rastrojo
    }
    rlPushMatrix();
    rlTranslatef(l->x, 0.0f, l->z);
    rlRotatef(-l->yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
    for (int r = 0; r < rows; r++) {
        float across = -l->w * 0.5f + ((float)r + 0.5f) * rw;
        for (int seg = 0; seg < 2; seg++) {
            float along = ((float)seg - 0.5f) * l->l * 0.5f, y = lot_point(t, l, along, across).y;
            DrawCube((Vector3){ along, y + 0.05f, across }, l->l * 0.5f, 0.3f, rw * 0.7f, soil);
            if (ch > 0.0f) DrawCube((Vector3){ along, y + 0.2f + ch * 0.5f, across }, l->l * 0.5f - 0.3f, ch, rw * 0.45f, crop);
        }
    }
    rlPopMatrix();
    if (!burnt && s == SEASON_AUTUMN && dos > 4) // las gavillas
        for (int k = 0; k < 4; k++) {
            Vector3 g = lot_point(t, l, (float)(k - 1.5f) * l->l * 0.22f, (k % 2 ? 1.0f : -1.0f) * l->w * 0.2f);
            DrawCylinder(g, 0.35f, 0.2f, 1.0f, 6, (Color){ 214, 182, 96, 255 });
        }
}

// Un corral: postes y dos travesaños que siguen el terreno, y un par de caballos.
static void draw_corral(const Terrain *t, const CityLot *l, float time, int idx) {
    static const Color WOOD = { 104, 78, 52, 255 };
    float hl = l->l * 0.5f, hw = l->w * 0.5f;
    Vector3 c[4] = { lot_point(t, l, -hl, -hw), lot_point(t, l, hl, -hw), lot_point(t, l, hl, hw), lot_point(t, l, -hl, hw) };
    for (int side = 0; side < 4; side++) {
        Vector3 a = c[side], b = c[(side + 1) % 4];
        int posts = (int)(Vector3Distance(a, b) / 2.5f) + 1;
        for (int k = 0; k <= posts; k++) {
            if (side == 1 && k == posts / 2) continue; // la tranquera
            Vector3 p = Vector3Lerp(a, b, (float)k / (float)posts);
            p.y = terrain_height(t, p.x, p.z);
            DrawCylinder(p, 0.08f, 0.08f, 1.3f, 4, WOOD);
        }
        for (int rail = 0; rail < 2; rail++) {
            float h = 0.55f + 0.5f * (float)rail;
            DrawCylinderEx((Vector3){ a.x, a.y + h, a.z }, (Vector3){ b.x, b.y + h, b.z }, 0.05f, 0.05f, 4, WOOD);
        }
    }
    for (int k = 0; k < 2; k++) {
        Vector3 h = lot_point(t, l, ((float)k - 0.5f) * hl, ((float)k - 0.5f) * hw * 0.6f);
        cb_draw_horse(h, l->yaw + (float)k * 2.0f + (float)idx, 0.0f, time + (float)k, false);
    }
}

// Una llama: conos rojo y amarillo que tiemblan, y una bocanada de humo que sube.
static void flames(Vector3 at, float h, float r, float time, int seed) {
    float flick = 0.8f + 0.2f * sinf(time * 11.0f + (float)seed * 1.7f);
    DrawCylinder(at, 0.0f, r, h * flick, 5, (Color){ 236, 120, 34, 230 });
    DrawCylinder(at, 0.0f, r * 0.55f, h * 0.7f * flick, 5, (Color){ 255, 214, 90, 240 });
    float sm = fmodf(time * 0.8f + (float)seed * 0.37f, 1.0f);
    DrawSphere((Vector3){ at.x + sm * 1.5f, at.y + h + sm * 5.0f, at.z + sm * 0.8f }, 0.5f + sm * 1.2f,
               (Color){ 70, 66, 62, (unsigned char)(150.0f * (1.0f - sm)) });
}

// Una puerta de la muralla, entre las torres (x, z) y (x2, z2): dos lienzos de muro a los lados y en
// medio las dos hojas: abiertas hacia adentro en paz; cerradas en guerra; ardiendo; o quemadas (los
// tocones negros).
static void draw_gate(const Terrain *t, float x, float z, float x2, float z2, VoxBuilding piece, int state, float time, int seed) {
    float len = dist_xz(x, z, x2, z2), ang = -atan2f(z2 - z, x2 - x), dx = (x2 - x) / len, dz = (z2 - z) / len;
    float mx = (x + x2) * 0.5f, mz = (z + z2) * 0.5f, side = CITY_GATE_HALF + 4.0f;
    for (int k = -1; k <= 1; k += 2) {
        float px = mx + dx * side * (float)k, pz = mz + dz * side * (float)k;
        voxs_draw_building(piece, (Vector3){ px, terrain_height(t, px, pz), pz }, ang, 1.0f, WHITE);
    }
    float y = terrain_height(t, mx, mz);
    static const Color DOOR = { 96, 64, 40, 255 }, IRON = { 52, 48, 46, 255 }, CHAR = { 34, 30, 28, 255 };
    rlPushMatrix();
    rlTranslatef(mx, y, mz);
    rlRotatef(ang * RAD2DEG, 0.0f, 1.0f, 0.0f); // x: a lo largo de la muralla; z: hacia adentro
    for (int k = -1; k <= 1; k += 2) {
        float hx = (float)k * (CITY_GATE_HALF - 1.5f);
        if (state == 0) { // abierta: cada hoja, de su gozne hacia adentro
            DrawCube((Vector3){ (float)k * (CITY_GATE_HALF - 0.15f), 2.1f, 1.5f }, 0.25f, 4.2f, 2.9f, DOOR);
        } else if (state == 3) { // quemada
            DrawCube((Vector3){ hx, 0.4f, 0.0f }, 2.9f, 0.8f, 0.22f, CHAR);
            DrawCube((Vector3){ hx * 0.6f, 0.08f, 1.2f }, 1.8f, 0.16f, 0.3f, CHAR);
        } else { // cerrada (ardiendo, o no)
            DrawCube((Vector3){ hx, 2.1f, 0.0f }, 2.95f, 4.2f, 0.25f, state == 2 ? (Color){ 70, 44, 30, 255 } : DOOR);
            for (int b = 0; b < 2; b++) DrawCube((Vector3){ hx, 1.0f + 2.2f * (float)b, 0.0f }, 2.95f, 0.15f, 0.3f, IRON);
        }
    }
    rlPopMatrix();
    if (state == 2) // arde
        for (int k = -1; k <= 1; k++) flames((Vector3){ mx + dx * 1.6f * (float)k, y + 0.2f, mz + dz * 1.6f * (float)k }, 3.6f, 1.2f, time, seed + k);
}

// La muralla de la capital: doce torres y entre ellas muro de piedra o empalizada; cada tercer tramo
// es una puerta (15, 105, 195 y 285 grados: ver draw_gate). Y las escalas que la tribu le apoyo.
static void draw_walls(const GameActions *ga, const Terrain *t, const Settlement *s, int idx, bool war, float time) {
    bool wood = s->region == REGION_STEPPE || s->region == REGION_FOREST;
    const City *c = &ga->cities[idx];
    for (int i = 0; i < 12; i++) {
        float a = (float)i / 12.0f * 2.0f * PI, x = s->x + cosf(a) * CITY_WALL_R, z = s->z + sinf(a) * CITY_WALL_R;
        voxs_draw_building(wood ? VB_TOWER_WOOD : VB_TOWER_STONE, (Vector3){ x, terrain_height(t, x, z), z }, a, 1.0f, WHITE);
        float a2 = (float)(i + 1) / 12.0f * 2.0f * PI, x2 = s->x + cosf(a2) * CITY_WALL_R, z2 = s->z + sinf(a2) * CITY_WALL_R;
        if (i % 3 == 0) {
            int g = i / 3, state = !war ? 0 : (c->gates_burnt >> g) & 1 ? 3 : g_gate_fire[idx][g] > 0.0f ? 2 : 1;
            draw_gate(t, x, z, x2, z2, wood ? VB_PALISADE : VB_WALL_STONE, state, time, idx * 4 + g);
            continue;
        }
        float len = dist_xz(x, z, x2, z2), ang = -atan2f(z2 - z, x2 - x);
        for (int k = 0; (float)k * 8.0f < len; k++) {
            float f = ((float)k * 8.0f + 4.0f) / len, px = x + (x2 - x) * f, pz = z + (z2 - z) * f;
            voxs_draw_building(wood ? VB_PALISADE : VB_WALL_STONE, (Vector3){ px, terrain_height(t, px, pz), pz }, ang, 1.0f, WHITE);
        }
    }
    static const Color RAIL = { 120, 90, 58, 255 };
    for (int k = 0; k < CITY_WALL_SEGMENTS; k++) { // las escalas, apoyadas por afuera
        if (!((c->ladders >> k) & 1)) continue;
        float a = seg_mid(k), tx = -sinf(a), tz = cosf(a);
        Vector3 base = wall_point(t, s, a, 1.6f), top = wall_point(t, s, a, 0.35f);
        top.y = fmaxf(top.y, base.y) + CITY_WALL_HEIGHT + 0.4f;
        for (int r = -1; r <= 1; r += 2) {
            Vector3 b0 = { base.x + tx * 0.28f * (float)r, base.y, base.z + tz * 0.28f * (float)r }, t0 = { top.x + tx * 0.28f * (float)r, top.y, top.z + tz * 0.28f * (float)r };
            DrawCylinderEx(b0, t0, 0.05f, 0.05f, 4, RAIL);
        }
        for (float f = 0.08f; f < 1.0f; f += 0.085f) {
            Vector3 m = Vector3Lerp(base, top, f);
            DrawCylinderEx((Vector3){ m.x - tx * 0.28f, m.y, m.z - tz * 0.28f }, (Vector3){ m.x + tx * 0.28f, m.y, m.z + tz * 0.28f }, 0.03f, 0.03f, 4, RAIL);
        }
    }
}

static bool g_fires_out;

// Que hay en el lote de la casa h (desde el centro) del asentamiento idx: casi siempre una casa del
// kit de la edad del hierro (src/sim/buildkit.h), y entre ellas los graneros, el templo del fuego
// (en el desierto y el altiplano) y las carretas-casa de la estepa. Las aldeas, las variantes chicas;
// la capital, las grandes en el anillo de adentro y de todas en el de afuera.
static BkType house_lot(Region rg, bool cap, int idx, int h, int *variant) {
    uint32_t r = (uint32_t)(idx + 1) * 2654435761u ^ (uint32_t)(h + 1) * 0x9E3779B1u;
    r ^= r >> 15;
    *variant = cap && h < 10 ? 2 + (int)(r & 1u) : cap ? (int)(r & 3u) : (int)(r & 1u);
    if (h == 2 && bk_available(BK_TEMPLE, rg)) return BK_TEMPLE;
    if (h % 5 == 4) return BK_GRANARY;
    if (h % 4 == 1 && bk_available(BK_WAGON, rg)) return BK_WAGON;
    return BK_HOUSE;
}

static void draw_city(const GameActions *ga, const Terrain *t, int idx, Vector3 eye, float time, int day, bool night, bool war) {
    const World *w = t->world;
    const Settlement *s = &w->settlements[idx];
    const City *c = &ga->cities[idx];
    bool cap = s->kind == SETTLE_CAPITAL;
    Season season = clock_season(day);
    int dos = clock_day_of_season(day), n, fields = 0, workers = 0;
    const CityLot *lots = ct_lots(ga, t, idx, &n);
    for (int i = 0; i < n; i++) fields += lots[i].kind == LOT_FIELD;
    // Los arrasados: tantos como falta sembrar, desde el ultimo que se saqueo.
    int burnt = (int)((1.0f - c->fields) * (float)fields + 0.5f), first = c->raided_lot > 0 ? c->raided_lot - 1 : 0, nfield = 0;
    int house = 0;
    Vector3 plaza = { s->x, 0.0f, s->z };
    for (int i = 0; i < n; i++) {
        const CityLot *l = &lots[i];
        float d = dist_xz(l->x, l->z, eye.x, eye.z);
        Vector3 at = { l->x, terrain_height(t, l->x, l->z), l->z };
        switch (l->kind) {
        case LOT_PALACE: {
            int v = idx & (BK_VARIANTS - 1);
            BkInfo pi;
            voxs_draw_kit(BK_PALACE, s->region, v, at, -l->yaw, 1.0f, WHITE);
            if (!voxs_kit_info(BK_PALACE, s->region, v, &pi)) pi = (BkInfo){ .front = 7.0f, .door_along = 5.0f, .door_up = 1.0f };
            plaza = lot_point(t, l, pi.front + 7.0f, 0.0f); // la plaza, delante del palacio
            if (d < FOLK_DRAW) { // la corte: la guardia a los lados de la puerta y un escriba que va y viene al pie
                for (int k = -1; k <= 1; k += 2) {
                    Vector3 g = lot_point(t, l, pi.door_along + 1.25f, pi.door_across + (float)k * 1.1f);
                    g.y = at.y + pi.door_up;
                    guard(g, -l->yaw + PI * 0.5f);
                }
                Vector3 sc = lot_point(t, l, pi.front + 1.2f, sinf(time * 0.25f) * 4.0f);
                person(sc, -l->yaw + (cosf(time * 0.25f) > 0.0f ? 0.0f : PI), (Color){ 70, 60, 110, 255 }, time * 4.0f, 0.5f, 0.0f);
            }
            break;
        }
        case LOT_HOUSE: { // la del jefe de la aldea, la primera, un poco mas grande
            int v;
            BkType bt = house_lot(s->region, cap, idx, house, &v);
            voxs_draw_kit(bt, s->region, v, at, -l->yaw, !cap && house == 0 ? 1.15f : 1.0f, WHITE);
            house++;
            break;
        }
        case LOT_FORGE: voxs_draw_kit(BK_FORGE, s->region, idx & (BK_VARIANTS - 1), at, -l->yaw, 1.0f, WHITE); break;
        case LOT_BARRACKS: voxs_draw_kit(BK_BARRACKS, s->region, idx & (BK_VARIANTS - 1), at, -l->yaw, 1.0f, WHITE); break;
        case LOT_CARPENTRY: voxs_draw_kit(BK_CARPENTRY, s->region, idx & (BK_VARIANTS - 1), at, -l->yaw, 1.0f, WHITE); break;
        case LOT_MERC_TENT: voxs_draw_kit(BK_HOUSE, REGION_STEPPE, 1, at, -l->yaw, 0.55f, (Color){ 150, 128, 104, 255 }); break;
        case LOT_FIELD: {
            bool scorched = fields > 0 && ((nfield++ - first) % fields + fields) % fields < burnt;
            if (d < FIELD_DRAW) draw_field(t, l, season, dos, scorched);
            // Los labradores, de dia y fuera del invierno: carpen a lo largo de los surcos.
            if (!night && !scorched && season != SEASON_WINTER && d < FOLK_DRAW && workers < 6) {
                float ph = time * 0.18f + (float)i, along = sinf(ph) * l->l * 0.35f;
                Vector3 wp = lot_point(t, l, along, (float)(workers % 3 - 1) * l->w * 0.2f);
                person(wp, -l->yaw + (cosf(ph) > 0.0f ? PI * 0.5f : -PI * 0.5f), folk_color(s->region, workers), time * 3.0f, 0.3f,
                       0.5f + 0.5f * sinf(time * 3.0f + (float)i));
                workers++;
            }
            break;
        }
        case LOT_CORRAL:
            if (d < FIELD_DRAW) draw_corral(t, l, time, idx);
            break;
        default: break;
        }
    }
    float ground = terrain_height(t, plaza.x, plaza.z);
    plaza.y = ground;
    if (cap) {
        draw_walls(ga, t, s, idx, war, time);
        // El estandarte del reino, en la plaza (tomada, el de la tribu: rojo y oro).
        Vector3 pole = { plaza.x + 3.0f, ground, plaza.z };
        DrawCylinder(pole, 0.12f, 0.12f, 9.0f, 5, (Color){ 90, 70, 50, 255 });
        Vector3 flag = { pole.x + 1.0f, ground + 8.0f + sinf(time * 2.0f) * 0.06f, pole.z };
        DrawCube(flag, 2.0f, 1.2f, 0.08f, c->taken ? (Color){ 168, 34, 30, 255 } : folk_color(s->region, idx + 1));
        if (c->taken) DrawCube((Vector3){ flag.x, flag.y, flag.z + 0.05f }, 2.0f, 0.3f, 0.02f, (Color){ 214, 176, 70, 255 });
        // La guardia de las puertas: mas soldados, mas guardias (de a dos por puerta).
        if (dist_xz(s->x, s->z, eye.x, eye.z) < FOLK_DRAW + CITY_WALL_R) {
            int guards = c->soldiers / 20;
            guards = guards < 1 ? (c->soldiers > 0) : guards > 8 ? 8 : guards;
            for (int k = 0; k < guards; k++) {
                float a = (15.0f + 90.0f * (float)(k % 4)) * DEG2RAD + (k / 4 ? 0.1f : -0.1f), r = CITY_WALL_R + 2.0f;
                Vector3 g = { s->x + cosf(a) * r, 0.0f, s->z + sinf(a) * r };
                g.y = terrain_height(t, g.x, g.z);
                guard(g, PI * 0.5f - a); // mira hacia afuera
            }
        }
    }
    // La fogata de la plaza y la gente que anda alrededor.
    hearth_draw_base(plaza, cap ? HEARTH_SCALE_BONFIRE : 1.3f, 100u + (uint32_t)idx, !g_fires_out);
    if (dist_xz(plaza.x, plaza.z, eye.x, eye.z) < FOLK_DRAW)
        for (int k = 0; k < 4; k++) {
            float a = time * 0.05f + (float)k * 1.6f + (float)idx, r = 5.0f + 1.5f * (float)(k % 2);
            Vector3 pos = { plaza.x + cosf(a) * r, 0.0f, plaza.z + sinf(a) * r };
            pos.y = terrain_height(t, pos.x, pos.z);
            person(pos, a + PI, folk_color(s->region, k), time * 6.0f + (float)k, 0.0f, 0.0f);
        }
}

void ct_draw_world(const GameActions *ga, const Terrain *t, const Player *p, float time, float world_time) {
    const World *w = t ? t->world : NULL;
    if (!w) return;
    g_fires_out = ga->fires_out; // la lluvia apaga tambien los fuegos de los demas
    int day = clock_day(world_time);
    bool night = clock_is_night(world_time);
    for (int i = 0; i < w->settlement_count && i < SETTLEMENTS_MAX; i++)
        if (dist_xz(w->settlements[i].x, w->settlements[i].z, p->pos.x, p->pos.z) < CITY_DRAW)
            draw_city(ga, t, i, p->pos, time, day, night, at_war(ga, w, ga->troop_ref, i));
    for (int i = 0; i < CITY_FIRES; i++) // los techos que arden
        if (g_fires[i].t > 0.0f && dist_xz(g_fires[i].at.x, g_fires[i].at.z, p->pos.x, p->pos.z) < CITY_DRAW)
            flames(g_fires[i].at, 3.0f * fminf(1.0f, g_fires[i].t / 5.0f), 1.4f, time, 30 + i);
}

// ------------------------------------------------------------------ pruebas
// --asedio: la capital del kanato en guerra (la relacion por el suelo) y el jugador donde se ve:
// «puerta» (afuera, mirando una puerta cerrada), «trepa» (al pie de un tramo de muro: la trepa),
// «escala» (una escala apoyada y el jugador al pie), «fuego» (la puerta ardiendo), «quemada» (la
// puerta quemada y el jugador pasando), «salida» (a 30 m de la muralla: sale la guarnicion),
// «rendicion» y «tomada» (sin guarnicion: se rinde; el jugador en la plaza). «partida»: la expedicion,
// una puerta quemada, una escala y una aldea tomada (la partida de referencia). «expedicion»: la
// expedicion de castigo contra el campamento cercano; «ejercito»: un ejercito delante, en formacion;
// «carro»: un carro de guerra cerca.
void ct_test(GameActions *ga, Combat *cb, Troop *troop, const Props *props, const Terrain *t, Player *p, const char *what, int day, char *log,
             size_t len) {
    const World *w = t ? t->world : NULL;
    int ki = khanate_idx(w);
    if (ki < 0) return;
    ensure(ga, w);
    const Settlement *s = &w->settlements[ki];
    City *c = &ga->cities[ki];
    if (troop && troop->overlord) troop->overlord->relation = -50.0f; // en guerra, y le toca expedicion
    else c->grudge = 85.0f;
    if (!strcmp(what, "expedicion") || !strcmp(what, "partida")) {
        if (!strcmp(what, "partida")) { // la partida de referencia (tests/partidas): ademas, el asedio a medias y una aldea tomada
            c->gates_burnt |= 1 << 2, c->ladders |= 1 << 4, c->besieged = day > 1 ? day - 1 : 1;
            for (int i = 0; i < w->settlement_count && i < SETTLEMENTS_MAX; i++)
                if (w->settlements[i].kind == SETTLE_VILLAGE) {
                    city_take(&ga->cities[i], day > 0 ? day : 1);
                    break;
                }
        }
        c->expedition = 0;
        c->silver = fmaxf(c->silver, 2000.0f);
        punitive(ga, w, troop, day > 0 ? day : 1, log, len);
        return;
    }
    if (!strcmp(what, "ejercito") || !strcmp(what, "carro")) { // delante del jugador, marchando hacia el (o un carro solo, cerca)
        bool one = !strcmp(what, "carro");
        ArmyComp a = { { one ? 0 : 4, one ? 0 : 2, one ? 0 : 2, one ? 0 : 2, one ? 1 : 2 } };
        float d = one ? 9.0f : 34.0f;
        Vector3 from = { p->pos.x + sinf(p->yaw) * d, 0.0f, p->pos.z + cosf(p->yaw) * d };
        cb_spawn_army(cb, t, ki, &a, from, p->pos, false);
        snprintf(log, len, "%s", one ? T("Un carro de guerra del reino.") : T("Un ejército del reino en formación, con sus carros."));
        return;
    }
    if (!strcmp(what, "rendicion") || !strcmp(what, "tomada")) {
        c->soldiers = city_garrison_floor(c, s->kind), c->mercs = 0;
        ct_demand_surrender(ga, cb, t, troop, ki, day, log, len);
        int n;
        const CityLot *lots = ct_lots(ga, t, ki, &n);
        if (n > 0) { // en la plaza, mirando al palacio
            float x = s->x + cosf(lots[0].yaw) * 24.0f, z = s->z + sinf(lots[0].yaw) * 24.0f;
            p->pos = (Vector3){ x, terrain_height(t, x, z), z };
            p->yaw = atan2f(s->x - x, s->z - z);
        }
        return;
    }
    if (!strcmp(what, "trepa") || !strcmp(what, "escala")) { // al pie de un tramo de muro, mirandolo
        int seg = 1;
        float a = seg_mid(seg);
        bool ladder = !strcmp(what, "escala");
        if (ladder) c->ladders |= 1 << seg;
        p->pos = wall_point(t, s, a, ladder ? 4.5f : 1.8f);
        p->yaw = atan2f(s->x - p->pos.x, s->z - p->pos.z);
        snprintf(log, len, "%s", ladder ? T("Una escala apoyada en la muralla: F para subir.") : T("Al pie de la muralla: la trepa (Tab) se engancha en las almenas."));
        return;
    }
    if (!strcmp(what, "trepar")) { // (despues de «trepa») la trepa ya enganchada: a escalar
        Vector3 top, over, to;
        if (ct_wall_ahead(ga, t, p, &top, &over, &to)) ga_climb(ga, p, top, over, to);
        return;
    }
    int g = 0;
    if (!strcmp(what, "fuego")) g_gate_fire[ki][g] = GATE_BURN_SECONDS * 6.0f; // que siga ardiendo para la foto
    if (!strcmp(what, "quemada")) c->gates_burnt |= 1 << g;
    bool sally = !strcmp(what, "salida");
    p->pos = wall_point(t, s, city_gate_angle(g), sally ? 30.0f : 12.0f);
    p->yaw = atan2f(s->x - p->pos.x, s->z - p->pos.z);
    g_last_ok = false; // un salto: no es un paso
    if (!sally) g_sally_cd[ki] = 120.0f; // que no salga nadie a tapar la puerta
    (void)props;
}
