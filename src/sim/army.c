#include "sim/army.h"

#include <math.h>
#include <string.h>

#include "sim/trade.h"

#define PI_F 3.14159265f

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

// ---------------------------------------------------------------- el ejercito
int army_units(const ArmyComp *a) {
    int n = 0;
    for (int k = 0; k < ARMY_UNIT_KINDS; k++) n += a->n[k];
    return n;
}

int army_men(const ArmyComp *a) { return army_units(a) + a->n[ARMY_CHARIOT]; }

static float unit_cost(ArmyUnit u) {
    return u == ARMY_CHARIOT ? ARMY_CHARIOT_COST : u == ARMY_HEAVY ? ARMY_HEAVY_COST : ARMY_MAN_COST;
}

float army_cost(const ArmyComp *a) {
    float c = 0.0f;
    for (int k = 0; k < ARMY_UNIT_KINDS; k++) c += (float)a->n[k] * unit_cost((ArmyUnit)k);
    return c;
}

int army_compose(const City *c, SettleKind k, int max_units, bool campaign, ArmyComp *out) {
    memset(out, 0, sizeof(*out));
    if (!c || c->people <= 0.0f || c->taken || max_units <= 0) return 0;
    bool cap = k == SETTLE_CAPITAL;
    int men = (c->soldiers + c->mercs) * 2 / 3; // un tercio queda en casa, de guardia
    float budget = campaign ? c->silver * ARMY_BUDGET : 1e30f;
    // De a uno, en este orden: el frente de paveses, los soldados, los tiradores y los carros.
    static const ArmyUnit ORDER[] = { ARMY_HEAVY, ARMY_HEAVY,    ARMY_SOLDIER, ARMY_CROSSBOW, ARMY_HEAVY,
                                      ARMY_ARCHER, ARMY_CHARIOT, ARMY_SOLDIER, ARMY_HEAVY,    ARMY_CROSSBOW,
                                      ARMY_ARCHER, ARMY_CHARIOT };
    const int N = (int)(sizeof(ORDER) / sizeof(ORDER[0]));
    int units = 0, misses = 0;
    for (int i = 0; units < max_units && misses < N; i++) {
        ArmyUnit u = ORDER[i % N];
        if (!cap && u == ARMY_HEAVY) u = ARMY_SOLDIER; // la aldea: soldados y tiradores
        if (u == ARMY_CHARIOT && (!cap || !campaign)) u = ARMY_ARCHER;
        int need = u == ARMY_CHARIOT ? 2 : 1;
        float cost = campaign ? unit_cost(u) : 0.0f;
        if (men < need || budget < cost) {
            misses++;
            continue;
        }
        misses = 0;
        out->n[u]++;
        men -= need;
        budget -= cost;
        units++;
    }
    return units;
}

void formation_slot(int i, int n, int cols, float spacing, float cx, float cz, float yaw, float *x, float *z) {
    if (cols < 1) cols = 1;
    if (n < 1) n = 1;
    int row = i / cols, col = i % cols, in_row = n - row * cols;
    if (in_row > cols) in_row = cols;
    if (in_row < 1) in_row = 1;
    float across = ((float)col - (float)(in_row - 1) * 0.5f) * spacing, back = (float)row * spacing;
    float fx = sinf(yaw), fz = cosf(yaw), rx = cosf(yaw), rz = -sinf(yaw); // el frente y la derecha
    *x = cx + rx * across - fx * back;
    *z = cz + rz * across - fz * back;
}

float chariot_speed(float base, float slope, float wade) {
    float k = 1.0f;
    slope = fabsf(slope);
    if (slope > CHARIOT_FLAT) k = fmaxf(0.2f, 1.0f - (slope - CHARIOT_FLAT) * 3.5f); // en la cuesta y lo roto, salta y vuelca
    if (wade > 0.1f) k *= fmaxf(0.08f, 0.45f - 0.35f * wade);                       // las ruedas se hunden en el agua
    return base * k;
}

// ---------------------------------------------------------------- las expediciones de castigo
bool punitive_due(bool overlord, float relation, float grudge, int day, int last) {
    bool angry = (overlord && relation <= PUNITIVE_RELATION) || grudge >= PUNITIVE_GRUDGE;
    return angry && (last <= 0 || day - last >= PUNITIVE_DAYS);
}

float punitive_share(int men, int defenders) {
    if (men <= 0) return 0.0f;
    if (defenders < 0) defenders = 0;
    return clampf((float)men / ((float)men + 2.0f * (float)defenders + 2.0f), 0.1f, 0.6f);
}

static bool plunderable(const char *id) {
    return !strncmp(id, "utileria.consumible.", 20) || !strcmp(id, GRAIN_SACK_ID) || !strcmp(id, SILVER_ID);
}

int stock_plunder(Stockpile *s, float share) {
    if (!s || share <= 0.0f) return 0;
    share = fminf(share, 1.0f);
    int taken = 0;
    for (int i = 0; i < s->n; i++) {
        StockEntry *e = &s->e[i];
        if (e->count <= 0 || !plunderable(e->id)) continue;
        int n = (int)((float)e->count * share);
        if (n < 1 && e->count >= 2) n = 1;
        e->count -= n;
        taken += n;
    }
    return taken;
}

// ---------------------------------------------------------------- el asedio
static float wrap_angle(float a) {
    a = fmodf(a, 2.0f * PI_F);
    return a < 0.0f ? a + 2.0f * PI_F : a;
}

int city_wall_segment(float ang) {
    int s = (int)(wrap_angle(ang) / (2.0f * PI_F) * (float)CITY_WALL_SEGMENTS);
    return s < 0 ? 0 : s >= CITY_WALL_SEGMENTS ? CITY_WALL_SEGMENTS - 1 : s;
}

int city_segment_gate(int seg) { return seg >= 0 && seg < CITY_WALL_SEGMENTS && seg % 3 == 0 ? seg / 3 : -1; }

float city_gate_angle(int gate) { return (15.0f + 90.0f * (float)gate) * PI_F / 180.0f; }

float city_wall_dist(float ang) {
    float step = 2.0f * PI_F / (float)CITY_WALL_SEGMENTS, mid = ((float)city_wall_segment(ang) + 0.5f) * step;
    return CITY_WALL_R * cosf(step * 0.5f) / cosf(wrap_angle(ang) - mid); // el tramo recto entre dos torres
}

bool city_wall_crossed(float x0, float z0, float x1, float z1, int *seg) {
    float a0 = atan2f(z0, x0), a1 = atan2f(z1, x1);
    float s0 = sqrtf(x0 * x0 + z0 * z0) - city_wall_dist(a0), s1 = sqrtf(x1 * x1 + z1 * z1) - city_wall_dist(a1);
    if ((s0 < 0.0f) == (s1 < 0.0f)) return false;
    // Por donde cruza: el punto del paso que cae sobre la muralla (aprox.: interpolando la distancia).
    float f = s0 / (s0 - s1), x = x0 + (x1 - x0) * f, z = z0 + (z1 - z0) * f;
    if (seg) *seg = city_wall_segment(atan2f(z, x));
    return true;
}

bool city_wall_blocks(const City *c, bool war, float x0, float z0, float x1, float z1, int *gate) {
    int seg = -1;
    if (gate) *gate = -1;
    if (!city_wall_crossed(x0, z0, x1, z1, &seg)) return false;
    int g = city_segment_gate(seg);
    if (g < 0) return true; // el muro
    if (gate) *gate = g;
    // Por donde cruza, de costado respecto del centro de la puerta.
    float s0 = sqrtf(x0 * x0 + z0 * z0) - city_wall_dist(atan2f(z0, x0)), s1 = sqrtf(x1 * x1 + z1 * z1) - city_wall_dist(atan2f(z1, x1));
    float f = s0 / (s0 - s1), x = x0 + (x1 - x0) * f, z = z0 + (z1 - z0) * f, a = city_gate_angle(g);
    if (fabsf(-x * sinf(a) + z * cosf(a)) > CITY_GATE_HALF) return true; // los lienzos a los lados de la puerta
    return c ? !city_gate_open(c, g, war) : false;
}

bool city_at_war(const City *c, bool overlord, float relation) {
    if (!c || c->people <= 0.0f || c->taken) return false;
    return !city_trades(c, overlord, relation);
}

bool city_gate_open(const City *c, int gate, bool war) {
    if (gate < 0 || gate >= CITY_GATES) return false;
    return !war || ((c->gates_burnt >> gate) & 1);
}

void city_siege_day(City *c, SettleKind k) {
    if (c->people <= 0.0f) return;
    c->grain = fmaxf(0.0f, c->grain - c->people * CITY_SIEGE_EAT);
    c->silver = fmaxf(0.0f, c->silver - city_taxes(c, k) * 0.5f); // no entra el comercio: la mitad de los impuestos
    c->unrest = clampf(c->unrest + 0.02f, 0.0f, 1.0f);
}

int city_burn_grain(City *c) {
    if (c->people <= 0.0f || c->grain <= 0.0f) return 0;
    float burnt = fminf(c->grain, fmaxf(c->grain * CITY_FIRE_GRAIN, 40.0f));
    c->grain -= burnt;
    c->unrest = clampf(c->unrest + 0.03f, 0.0f, 1.0f);
    return (int)burnt;
}

int city_garrison_floor(const City *c, SettleKind k) {
    int target = (int)(c->people / (k == SETTLE_CAPITAL ? 25.0f : 40.0f)); // la de una ciudad libre
    return target / 5 > 2 ? target / 5 : 2;
}

bool city_surrenders(const City *c, SettleKind k) {
    if (!c || c->people <= 0.0f || c->taken) return false;
    if (c->soldiers + c->mercs <= city_garrison_floor(c, k)) return true;
    if (c->grain < c->people * CITY_STARVING) return true;
    return c->unrest >= CITY_REVOLT;
}

void city_take(City *c, int day) {
    c->taken = day > 0 ? day : 1;
    c->soldiers = 0;
    c->mercs = 0;
    c->grudge = 0.0f;
    c->unrest = clampf(c->unrest + 0.1f, 0.0f, 1.0f);
}

int city_vassal_tribute(City *c, SettleKind k) {
    if (!c->taken || c->people <= 0.0f) return 0;
    float want = city_taxes(c, k) * (float)DAYS_PER_SEASON * 0.5f;
    int pay = (int)fminf(want, c->silver);
    if (pay < 0) pay = 0;
    c->silver -= (float)pay;
    return pay;
}

int peace_price(int members) { return 3 * tribute_due(members); }
