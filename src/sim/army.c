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

int city_gate_closed_side(int gate) { return gate % 2 ? -1 : 1; }

// Un punto en las coordenadas de la puerta (X hacia afuera desde la muralla, Z de costado), relativo
// al centro de la ciudad.
static void gate_point(int gate, float X, float Z, float *x, float *z) {
    float a = city_gate_angle(gate), d = city_wall_dist(a) + X;
    *x = cosf(a) * d - sinf(a) * Z, *z = sinf(a) * d + cosf(a) * Z;
}

void city_gate_path(int gate, float out[CITY_GATE_PATH][2]) {
    float s = (float)city_gate_closed_side(gate), far = CITY_BARBICAN_OUT + 2.5f, lane = CITY_BARBICAN_OUT * 0.55f;
    gate_point(gate, far, s * (CITY_BARBICAN_HALF + 2.5f), &out[0][0], &out[0][1]);  // afuera, la esquina cerrada
    gate_point(gate, far, -s * (CITY_BARBICAN_HALF + 2.5f), &out[1][0], &out[1][1]); // afuera, la esquina de la boca
    gate_point(gate, lane, -s * (CITY_BARBICAN_HALF + 3.0f), &out[2][0], &out[2][1]); // la boca
    gate_point(gate, lane, 0.0f, &out[3][0], &out[3][1]);                             // el patio, delante de la puerta
    gate_point(gate, -4.0f, 0.0f, &out[4][0], &out[4][1]);                            // adentro
}

// La distancia (al cuadrado) del punto (px, pz) al paso (x0, z0)-(x1, z1).
static float seg_dist2(float x0, float z0, float x1, float z1, float px, float pz) {
    float vx = x1 - x0, vz = z1 - z0, l2 = vx * vx + vz * vz, t = l2 > 0.0f ? ((px - x0) * vx + (pz - z0) * vz) / l2 : 0.0f;
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    float dx = x0 + vx * t - px, dz = z0 + vz * t - pz;
    return dx * dx + dz * dz;
}

// Donde cruza el paso (x0, z0)-(x1, z1) al segmento (ax, az)-(bx, bz): t en [0, 1] del paso, o -1.
static float seg_cross(float x0, float z0, float x1, float z1, float ax, float az, float bx, float bz) {
    float rx = x1 - x0, rz = z1 - z0, sx = bx - ax, sz = bz - az, den = rx * sz - rz * sx;
    if (fabsf(den) < 1e-9f) return -1.0f;
    float qx = ax - x0, qz = az - z0, t = (qx * sz - qz * sx) / den, u = (qx * rz - qz * rx) / den;
    return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f ? t : -1.0f;
}

bool city_wall_hit(const City *c, bool war, float x0, float z0, float x1, float z1, WallHit *hit) {
    WallHit best = { 2.0f, 0.0f, 0.0f, -1 };
    int passed = -1, seg = -1;
    if (city_wall_crossed(x0, z0, x1, z1, &seg)) { // la muralla, con sus puertas
        float s0 = sqrtf(x0 * x0 + z0 * z0) - city_wall_dist(atan2f(z0, x0)), s1 = sqrtf(x1 * x1 + z1 * z1) - city_wall_dist(atan2f(z1, x1));
        float f = s0 / (s0 - s1), x = x0 + (x1 - x0) * f, z = z0 + (z1 - z0) * f, ac = atan2f(z, x);
        int g = city_segment_gate(seg);
        bool door = g >= 0 && fabsf(-x * sinf(city_gate_angle(g)) + z * cosf(city_gate_angle(g))) <= CITY_GATE_HALF;
        if (g < 0 || !door || (c && !city_gate_open(c, g, war))) best = (WallHit){ f, -sinf(ac), cosf(ac), g };
        else passed = g;
    }
    for (int g = 0; g < CITY_GATES; g++) { // los antemuros: el muro de adelante y el costado cerrado
        float px, pz, s = (float)city_gate_closed_side(g), w[2][4];
        gate_point(g, CITY_BARBICAN_OUT * 0.5f, 0.0f, &px, &pz);
        float r = CITY_BARBICAN_HALF + CITY_BARBICAN_OUT; // lejos de esta puerta, nada
        if (seg_dist2(x0, z0, x1, z1, px, pz) > r * r) continue;
        gate_point(g, CITY_BARBICAN_OUT, -CITY_BARBICAN_HALF, &w[0][0], &w[0][1]);
        gate_point(g, CITY_BARBICAN_OUT, CITY_BARBICAN_HALF, &w[0][2], &w[0][3]);
        gate_point(g, 0.0f, s * CITY_BARBICAN_HALF, &w[1][0], &w[1][1]);
        gate_point(g, CITY_BARBICAN_OUT, s * CITY_BARBICAN_HALF, &w[1][2], &w[1][3]);
        for (int k = 0; k < 2; k++) {
            float t = seg_cross(x0, z0, x1, z1, w[k][0], w[k][1], w[k][2], w[k][3]);
            if (t < 0.0f || t >= best.t) continue;
            float dx = w[k][2] - w[k][0], dz = w[k][3] - w[k][1], l = sqrtf(dx * dx + dz * dz);
            best = (WallHit){ t, dx / l, dz / l, -1 };
        }
    }
    bool blocked = best.t <= 1.0f;
    if (hit) *hit = blocked ? best : (WallHit){ 2.0f, 0.0f, 0.0f, passed };
    return blocked;
}

// ---------------------------------------------------------------- el rodeo por las puertas
// Los caminos (city_route) van por un grafo chico: el camino de cada puerta (city_gate_path), adentro
// de puerta a puerta y afuera una vuelta frente a las torres. Con holgura, para que el que camina no
// roce las esquinas: la muralla se cruza solo por el medio de una puerta (a ROUTE_GAP de su centro) y
// los antemuros se alargan ROUTE_PAD en sus puntas.
#define ROUTE_GAP (CITY_GATE_HALF - 1.0f)
#define ROUTE_PAD 0.6f
#define ROUTE_RING (CITY_WALL_R + CITY_BARBICAN_OUT + 3.0f)
#define ROUTE_GATE_NODES (CITY_GATES * CITY_GATE_PATH)
#define ROUTE_NODES (ROUTE_GATE_NODES + CITY_WALL_SEGMENTS)
#define ROUTE_FAR 1e30f

typedef struct {
    float vx[CITY_WALL_SEGMENTS + 1], vz[CITY_WALL_SEGMENTS + 1]; // las torres (la ultima, otra vez la primera)
    float bar[CITY_GATES][2][4];          // los antemuros, alargados: el muro de adelante y el costado cerrado
    float box[CITY_GATES][4];             // y la caja que los contiene: x0, z0, x1, z1
    float ax[CITY_GATES], az[CITY_GATES]; // a lo largo de cada puerta: (-sin a, cos a)
    float node[ROUTE_NODES][2];           // los caminos de las puertas y, despues, la vuelta de afuera
    float in2;                            // el circulo de adentro de la muralla (al cuadrado)
} RouteMap;

static void route_map(RouteMap *m) {
    float step = 2.0f * PI_F / (float)CITY_WALL_SEGMENTS, in = CITY_WALL_R * cosf(step * 0.5f);
    m->in2 = in * in;
    for (int k = 0; k <= CITY_WALL_SEGMENTS; k++) {
        float a = (float)k * step;
        m->vx[k] = CITY_WALL_R * cosf(a), m->vz[k] = CITY_WALL_R * sinf(a);
        if (k < CITY_WALL_SEGMENTS) m->node[ROUTE_GATE_NODES + k][0] = ROUTE_RING * cosf(a), m->node[ROUTE_GATE_NODES + k][1] = ROUTE_RING * sinf(a);
    }
    for (int g = 0; g < CITY_GATES; g++) {
        float s = (float)city_gate_closed_side(g), a = city_gate_angle(g);
        m->ax[g] = -sinf(a), m->az[g] = cosf(a);
        gate_point(g, CITY_BARBICAN_OUT, -CITY_BARBICAN_HALF - ROUTE_PAD, &m->bar[g][0][0], &m->bar[g][0][1]);
        gate_point(g, CITY_BARBICAN_OUT, CITY_BARBICAN_HALF + ROUTE_PAD, &m->bar[g][0][2], &m->bar[g][0][3]);
        gate_point(g, 0.0f, s * CITY_BARBICAN_HALF, &m->bar[g][1][0], &m->bar[g][1][1]);
        gate_point(g, CITY_BARBICAN_OUT + ROUTE_PAD, s * CITY_BARBICAN_HALF, &m->bar[g][1][2], &m->bar[g][1][3]);
        city_gate_path(g, &m->node[g * CITY_GATE_PATH]);
        float *b = m->box[g], *f = m->bar[g][0], *c = m->bar[g][1];
        b[0] = fminf(fminf(f[0], f[2]), fminf(c[0], c[2])), b[1] = fminf(fminf(f[1], f[3]), fminf(c[1], c[3]));
        b[2] = fmaxf(fmaxf(f[0], f[2]), fmaxf(c[0], c[2])), b[3] = fmaxf(fmaxf(f[1], f[3]), fmaxf(c[1], c[3]));
    }
}

// ¿Se va derecho de (x0, z0) a (x1, z1), con holgura? Exacto: tramo por tramo de la muralla, de torre
// a torre (un paso con los dos extremos afuera tambien puede atravesar la ciudad).
static bool route_clear(const RouteMap *m, float x0, float z0, float x1, float z1) {
    float lx = fminf(x0, x1), lz = fminf(z0, z1), hx = fmaxf(x0, x1), hz = fmaxf(z0, z1);
    for (int g = 0; g < CITY_GATES; g++) {
        const float *b = m->box[g];
        if (hx < b[0] || hz < b[1] || lx > b[2] || lz > b[3]) continue; // lejos de este antemuro
        for (int k = 0; k < 2; k++) {
            const float *w = m->bar[g][k];
            if (seg_cross(x0, z0, x1, z1, w[0], w[1], w[2], w[3]) >= 0.0f) return false;
        }
    }
    if (seg_dist2(x0, z0, x1, z1, 0.0f, 0.0f) > CITY_WALL_R * CITY_WALL_R) return true; // ni se acerca a la muralla
    if (x0 * x0 + z0 * z0 < m->in2 && x1 * x1 + z1 * z1 < m->in2) return true;        // bien adentro
    for (int k = 0; k < CITY_WALL_SEGMENTS; k++) {
        float t = seg_cross(x0, z0, x1, z1, m->vx[k], m->vz[k], m->vx[k + 1], m->vz[k + 1]);
        if (t < 0.0f) continue;
        int g = city_segment_gate(k);
        float x = x0 + (x1 - x0) * t, z = z0 + (z1 - z0) * t;
        if (g < 0 || fabsf(x * m->ax[g] + z * m->az[g]) > ROUTE_GAP) return false;
    }
    return true;
}

// Los vecinos del nodo i en el grafo (a lo sumo 4): en cada puerta, su camino en orden; adentro, de
// puerta a puerta; afuera, de torre en torre y, de cada antemuro, sus dos esquinas a las dos torres de
// su tramo (la esquina cerrada, a la de su costado: Z crece con el angulo, hacia la torre 3g + 1).
static int route_links(int i, int out[4]) {
    int n = 0;
    if (i >= ROUTE_GATE_NODES) {
        int t = i - ROUTE_GATE_NODES, g = t / 3;
        out[n++] = ROUTE_GATE_NODES + (t + 1) % CITY_WALL_SEGMENTS;
        out[n++] = ROUTE_GATE_NODES + (t + CITY_WALL_SEGMENTS - 1) % CITY_WALL_SEGMENTS;
        if (t % 3 != 2) out[n++] = g * CITY_GATE_PATH + ((t % 3 == 1) == (city_gate_closed_side(g) > 0) ? 0 : 1);
        return n;
    }
    int g = i / CITY_GATE_PATH, k = i % CITY_GATE_PATH;
    if (k > 0) out[n++] = i - 1;
    if (k < CITY_GATE_PATH - 1) out[n++] = i + 1;
    for (int h = 0; k == CITY_GATE_PATH - 1 && h < CITY_GATES; h++)
        if (h != g) out[n++] = h * CITY_GATE_PATH + k;
    if (k <= 1) out[n++] = ROUTE_GATE_NODES + 3 * g + ((k == 0) == (city_gate_closed_side(g) > 0) ? 1 : 0);
    return n;
}

static float span(float x0, float z0, float x1, float z1) { return sqrtf((x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0)); }

static bool inside_wall(float x, float z) { return sqrtf(x * x + z * z) < city_wall_dist(atan2f(z, x)); }

bool city_route(float fx, float fz, float tx, float tz, float *wx, float *wz) {
    float reach = CITY_WALL_R + CITY_BARBICAN_OUT + CITY_BARBICAN_HALF; // nada de la muralla llega mas lejos
    if (seg_dist2(fx, fz, tx, tz, 0.0f, 0.0f) > reach * reach) return false;
    if (inside_wall(fx, fz) && inside_wall(tx, tz)) return false; // adentro no hay nada en el medio
    RouteMap m;
    route_map(&m);
    if (route_clear(&m, fx, fz, tx, tz)) return false;
    // Lo que falta desde cada nodo hasta el destino (Dijkstra desde alla). Si ningun nodo lo ve (un
    // destino metido en un muro), el mas cercano.
    float left[ROUTE_NODES], near = ROUTE_FAR;
    bool done[ROUTE_NODES], seen = false;
    int nearest = 0;
    for (int i = 0; i < ROUTE_NODES; i++) {
        float d = span(m.node[i][0], m.node[i][1], tx, tz);
        left[i] = route_clear(&m, m.node[i][0], m.node[i][1], tx, tz) ? d : ROUTE_FAR, done[i] = false;
        seen |= left[i] < ROUTE_FAR;
        if (d < near) near = d, nearest = i;
    }
    if (!seen) left[nearest] = near;
    int open[ROUTE_NODES], no = 0; // los que ya tienen camino y falta abrir
    for (int i = 0; i < ROUTE_NODES; i++)
        if (left[i] < ROUTE_FAR) open[no++] = i;
    while (no > 0) {
        int b = 0;
        for (int k = 1; k < no; k++)
            if (left[open[k]] < left[open[b]]) b = k;
        int u = open[b], next[4], nn = route_links(u, next);
        open[b] = open[--no], done[u] = true;
        for (int k = 0; k < nn; k++) {
            int v = next[k];
            float d = left[u] + span(m.node[u][0], m.node[u][1], m.node[v][0], m.node[v][1]);
            if (done[v] || d >= left[v]) continue;
            if (left[v] >= ROUTE_FAR) open[no++] = v;
            left[v] = d;
        }
    }
    // El primer paso: de los nodos que se ven derecho desde aca, el del camino mas corto (sin contar
    // el que ya se piso). Se prueban de menor a mayor: el primero que se ve es ese.
    float cost[ROUTE_NODES];
    for (int i = 0; i < ROUTE_NODES; i++) {
        float d = span(fx, fz, m.node[i][0], m.node[i][1]);
        cost[i] = d < 0.25f || left[i] >= ROUTE_FAR ? ROUTE_FAR : d + left[i];
    }
    for (int it = 0; it < ROUTE_NODES; it++) {
        int u = -1;
        for (int i = 0; i < ROUTE_NODES; i++)
            if (cost[i] < ROUTE_FAR && (u < 0 || cost[i] < cost[u])) u = i;
        if (u < 0) break;
        if (route_clear(&m, fx, fz, m.node[u][0], m.node[u][1])) return *wx = m.node[u][0], *wz = m.node[u][1], true;
        cost[u] = ROUTE_FAR;
    }
    return false; // encerrado: derecho
}

bool city_wall_blocks(const City *c, bool war, float x0, float z0, float x1, float z1, int *gate) {
    WallHit h;
    bool blocked = city_wall_hit(c, war, x0, z0, x1, z1, &h);
    if (gate) *gate = h.gate;
    return blocked;
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
