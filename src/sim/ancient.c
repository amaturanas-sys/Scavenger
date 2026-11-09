// Los vestigios de los asentamientos antiguos (ver ancient.h): la planta de un pueblo de otros tiempos.
#include "sim/ancient.h"

#include <math.h>
#include <string.h>

#define T_OUT_PALACE 1.0f // m: el muro de afuera del palacio
#define T_OUT_HOUSE 0.75f // el de las casas
#define T_IN 0.5f         // el de entre cuartos
#define STREET 2.5f       // m: lo minimo entre dos edificios (la calle)
#define ROOM_MIN 2.2f     // m: el lado mas corto de un cuarto
#define DOOR 1.0f         // m: el vano de una puerta de adentro
#define BLOCKED_MAX 48

typedef struct {
    uint32_t s;
} Az;

static uint32_t mezcla(uint32_t h) {
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

static float az(Az *a) {
    a->s = a->s * 1664525u + 1013904223u;
    return (float)(mezcla(a->s) >> 8) / 16777216.0f;
}

static float az_entre(Az *a, float lo, float hi) { return lo + (hi - lo) * az(a); }

static bool dentro(AncRect r, float x, float z) { return x >= r.x0 && x < r.x1 && z >= r.z0 && z < r.z1; }

// ¿Se tocan (o se pisan) a menos de gap?
static bool se_tocan(AncRect a, AncRect b, float gap) {
    return a.x0 - gap < b.x1 && b.x0 - gap < a.x1 && a.z0 - gap < b.z1 && b.z0 - gap < a.z1;
}

static bool a_lo_largo_x(AncRect r) { return r.x1 - r.x0 >= r.z1 - r.z0; }

static void muro(AncientPlan *p, float x0, float z0, float x1, float z1, float h, bool outer) {
    if (p->wall_count < ANC_WALLS_MAX && x1 > x0 && z1 > z0) p->walls[p->wall_count++] = (AncWall){ { x0, z0, x1, z1 }, h, outer };
}

// Los cuatro muros de afuera de un edificio, siempre en el mismo orden: el de -Z, el de +Z, el de -X y el
// de +X (los dos ultimos, entre los primeros: las esquinas no se pisan).
static void exteriores(AncientPlan *p, Az *a, AncRect b, float t) {
    float h = az_entre(a, 0.7f, 1.3f);
    muro(p, b.x0, b.z0, b.x1, b.z0 + t, h, true);
    muro(p, b.x0, b.z1 - t, b.x1, b.z1, h, true);
    muro(p, b.x0, b.z0 + t, b.x0 + t, b.z1 - t, h, true);
    muro(p, b.x1 - t, b.z0 + t, b.x1, b.z1 - t, h, true);
}

// Los cuartos: cada parte mas grande que max_lado se corta al traves de su lado largo, a una distancia al
// azar (ningun cuarto de menos de ROOM_MIN), hasta que todos midan menos. Las puertas van despues
// (puertas): asi ningun vano queda contra un muro que llega despues.
static void cuartos(AncientPlan *p, Az *a, AncRect r, float max_lado, int hondo) {
    float w = r.x1 - r.x0, d = r.z1 - r.z0;
    bool cut_x = w >= d;
    float largo = cut_x ? w : d;
    if ((w <= max_lado && d <= max_lado) || largo < 2.0f * ROOM_MIN + T_IN || hondo > 8 || p->wall_count >= ANC_WALLS_MAX - 2) {
        p->rooms++;
        return;
    }
    float at = (cut_x ? r.x0 : r.z0) + az_entre(a, ROOM_MIN, largo - ROOM_MIN - T_IN), h = az_entre(a, 0.35f, 0.9f);
    if (cut_x) {
        muro(p, at, r.z0, at + T_IN, r.z1, h, false);
        cuartos(p, a, (AncRect){ r.x0, r.z0, at, r.z1 }, max_lado, hondo + 1);
        cuartos(p, a, (AncRect){ at + T_IN, r.z0, r.x1, r.z1 }, max_lado, hondo + 1);
    } else {
        muro(p, r.x0, at, r.x1, at + T_IN, h, false);
        cuartos(p, a, (AncRect){ r.x0, r.z0, r.x1, at }, max_lado, hondo + 1);
        cuartos(p, a, (AncRect){ r.x0, at + T_IN, r.x1, r.z1 }, max_lado, hondo + 1);
    }
}

// Un vano de `ancho` m en el muro m, a lo largo de su lado largo: donde no tope ningun otro muro del mismo
// edificio (los de desde en adelante; con 0.2 m de aire) ni a menos de 0.3 m de sus puntas. Al azar entre
// los lugares que quedan. false si no cabe.
static bool vano(AncientPlan *p, Az *a, int m, int desde, float ancho) {
    AncRect W = p->walls[m].r;
    bool ax = a_lo_largo_x(W);
    float lo = (ax ? W.x0 : W.z0) + 0.3f, hi = (ax ? W.x1 : W.z1) - 0.3f;
    float b0[BLOCKED_MAX], b1[BLOCKED_MAX];
    int nb = 0;
    for (int i = desde; i < p->wall_count && nb < BLOCKED_MAX; i++) {
        AncRect V = p->walls[i].r;
        if (i == m || !se_tocan(V, W, 0.01f)) continue;
        b0[nb] = (ax ? V.x0 : V.z0) - 0.2f, b1[nb] = (ax ? V.x1 : V.z1) + 0.2f, nb++;
    }
    for (int i = 1; i < nb; i++) // por donde empiezan (pocos: insercion)
        for (int j = i; j > 0 && b0[j] < b0[j - 1]; j--) {
            float t0 = b0[j], t1 = b1[j];
            b0[j] = b0[j - 1], b1[j] = b1[j - 1], b0[j - 1] = t0, b1[j - 1] = t1;
        }
    float libre[BLOCKED_MAX + 1][2], total = 0.0f, x = lo;
    int nl = 0;
    for (int i = 0; i <= nb; i++) {
        float fin = i < nb ? b0[i] : hi;
        if (fin > hi) fin = hi;
        if (fin - x >= ancho) libre[nl][0] = x, libre[nl][1] = fin - ancho, total += fin - ancho - x, nl++;
        if (i < nb && b1[i] > x) x = b1[i];
    }
    if (nl == 0 || p->door_count >= ANC_DOORS_MAX) return false;
    float t = az(a) * total, s = libre[nl - 1][1];
    for (int i = 0; i < nl; i++) {
        float l = libre[i][1] - libre[i][0];
        if (t <= l) {
            s = libre[i][0] + t;
            break;
        }
        t -= l;
    }
    p->doors[p->door_count++] = ax ? (AncRect){ s, W.z0 - 0.01f, s + ancho, W.z1 + 0.01f } : (AncRect){ W.x0 - 0.01f, s, W.x1 + 0.01f, s + ancho };
    return true;
}

// Una puerta en cada muro de adentro del edificio (los de desde + 4 en adelante: los primeros cuatro son los
// de afuera) y la entrada en el de afuera que diga `lado` (0: -Z, 1: +Z, 2: -X, 3: +X).
static void puertas(AncientPlan *p, Az *a, int desde, int lado, float entrada) {
    for (int i = desde + 4, n = p->wall_count; i < n; i++)
        if (!vano(p, a, i, desde, DOOR)) vano(p, a, i, desde, 0.8f);
    if (!vano(p, a, desde + lado, desde, entrada)) vano(p, a, desde + lado, desde, DOOR);
}

static AncRect achicar(AncRect r, float t) { return (AncRect){ r.x0 + t, r.z0 + t, r.x1 - t, r.z1 - t }; }

// El palacio: los cuartos en hilera alrededor del patio (las dos fajas de los lados largos, de punta a
// punta; las de los otros dos, entre ellas), como el palacio norte de Ras Ibn Hani.
static void palacio(AncientPlan *p, Az *a, AncRect b) {
    int desde = p->wall_count;
    exteriores(p, a, b, T_OUT_PALACE);
    AncRect in = achicar(b, T_OUT_PALACE);
    // Las fajas de cuartos, de 3 a 4.5 m de hondo, sin dejar el patio de menos de 4 m.
    float mx = fminf(4.5f, (in.x1 - in.x0 - 2.0f * T_IN - 4.0f) * 0.5f), mz = fminf(4.5f, (in.z1 - in.z0 - 2.0f * T_IN - 4.0f) * 0.5f);
    float zs = in.z0 + az_entre(a, fminf(3.0f, mz), mz), ze = in.z1 - az_entre(a, fminf(3.0f, mz), mz) - T_IN;
    float xs = in.x0 + az_entre(a, fminf(3.0f, mx), mx), xe = in.x1 - az_entre(a, fminf(3.0f, mx), mx) - T_IN;
    if (xe - xs - T_IN < 3.99f || ze - zs - T_IN < 3.99f || mx < 2.2f || mz < 2.2f) { // muy chico para un patio
        cuartos(p, a, in, 4.5f, 0);
    } else {
        muro(p, in.x0, zs, in.x1, zs + T_IN, az_entre(a, 0.35f, 0.9f), false);
        muro(p, in.x0, ze, in.x1, ze + T_IN, az_entre(a, 0.35f, 0.9f), false);
        muro(p, xs, zs + T_IN, xs + T_IN, ze, az_entre(a, 0.35f, 0.9f), false);
        muro(p, xe, zs + T_IN, xe + T_IN, ze, az_entre(a, 0.35f, 0.9f), false);
        p->court = (AncRect){ xs + T_IN, zs + T_IN, xe, ze };
        cuartos(p, a, (AncRect){ in.x0, in.z0, in.x1, zs }, 4.5f, 0);
        cuartos(p, a, (AncRect){ in.x0, ze + T_IN, in.x1, in.z1 }, 4.5f, 0);
        cuartos(p, a, (AncRect){ in.x0, zs + T_IN, xs, ze }, 4.5f, 0);
        cuartos(p, a, (AncRect){ xe + T_IN, zs + T_IN, in.x1, ze }, 4.5f, 0);
    }
    puertas(p, a, desde, (int)(az(a) * 4.0f) & 3, 2.0f);
}

// Una casa: de dos a cinco cuartos, con la entrada del lado del medio del pueblo (la calle).
static void casa(AncientPlan *p, Az *a, AncRect b) {
    int desde = p->wall_count;
    exteriores(p, a, b, T_OUT_HOUSE);
    cuartos(p, a, achicar(b, T_OUT_HOUSE), az_entre(a, 3.5f, 5.0f), 0);
    float cx = (b.x0 + b.x1) * 0.5f, cz = (b.z0 + b.z1) * 0.5f;
    int lado = fabsf(cx) > fabsf(cz) ? (cx > 0.0f ? 2 : 3) : (cz > 0.0f ? 0 : 1);
    puertas(p, a, desde, lado, 1.2f);
}

// Los tramos de pie (el muro en ruinas del inventario): en muros de afuera largos, lejos de las puertas y
// entre si.
static void de_pie(AncientPlan *p, Az *a) {
    int quiero = 2 + (int)(az(a) * 3.0f);
    for (int k = 0; k < 80 && p->standing_count < quiero && p->standing_count < ANC_STANDING_MAX && p->wall_count > 0; k++) {
        const AncWall *w = &p->walls[(int)(az(a) * (float)p->wall_count) % p->wall_count];
        bool ax = a_lo_largo_x(w->r);
        float largo = ax ? w->r.x1 - w->r.x0 : w->r.z1 - w->r.z0, L = ANC_STANDING_LEN;
        if (!w->outer || largo < L + 1.0f) continue;
        float c = (ax ? w->r.x0 : w->r.z0) + az_entre(a, L * 0.5f + 0.4f, largo - L * 0.5f - 0.4f);
        AncRect r = ax ? (AncRect){ c - L * 0.5f, w->r.z0, c + L * 0.5f, w->r.z1 } : (AncRect){ w->r.x0, c - L * 0.5f, w->r.x1, c + L * 0.5f };
        bool ok = true;
        for (int i = 0; i < p->door_count && ok; i++) ok = !se_tocan(r, p->doors[i], 0.3f);
        for (int i = 0; i < p->standing_count && ok; i++) ok = !se_tocan(r, p->standing[i].r, 1.0f);
        if (!ok) continue;
        AncStanding *s = &p->standing[p->standing_count++];
        s->r = r;
        s->x = (r.x0 + r.x1) * 0.5f, s->z = (r.z0 + r.z1) * 0.5f;
        // El largo del muro en ruinas va en su Z: a lo largo de X, girado un cuarto; de que lado mira, al azar.
        s->yaw = (ax ? 1.5707963f : 0.0f) + (az(a) < 0.5f ? 3.1415927f : 0.0f);
    }
}

void ancient_plan(AncientPlan *p, uint32_t seed) {
    memset(p, 0, sizeof(*p));
    p->seed = mezcla(seed ^ 0xA9C1E47u);
    Az a = { p->seed | 1u };
    // El palacio, cerca del medio.
    float pw = az_entre(&a, 15.0f, 21.0f), pd = az_entre(&a, 13.0f, 18.0f);
    float pcx = az_entre(&a, -3.0f, 3.0f), pcz = az_entre(&a, -3.0f, 3.0f), m = ANC_HALF - 0.5f;
    AncRect pal = { pcx - pw * 0.5f, pcz - pd * 0.5f, pcx + pw * 0.5f, pcz + pd * 0.5f };
    if (pal.x0 < -m) pal.x1 += -m - pal.x0, pal.x0 = -m;
    if (pal.x1 > m) pal.x0 -= pal.x1 - m, pal.x1 = m;
    if (pal.z0 < -m) pal.z1 += -m - pal.z0, pal.z0 = -m;
    if (pal.z1 > m) pal.z0 -= pal.z1 - m, pal.z1 = m;
    p->buildings[p->building_count++] = pal;
    palacio(p, &a, pal);
    // Las casas, alrededor, cada una con su calle.
    int casas = 3 + (int)(az(&a) * 4.0f);
    for (int k = 0; k < 120 && p->building_count < ANC_BUILDINGS_MAX && p->building_count - 1 < casas; k++) {
        float w = az_entre(&a, 6.0f, 10.5f), d = az_entre(&a, 5.0f, 8.5f);
        if (az(&a) < 0.5f) {
            float t = w;
            w = d, d = t;
        }
        float cx = az_entre(&a, -m + w * 0.5f, m - w * 0.5f), cz = az_entre(&a, -m + d * 0.5f, m - d * 0.5f);
        AncRect b = { cx - w * 0.5f, cz - d * 0.5f, cx + w * 0.5f, cz + d * 0.5f };
        bool libre = true;
        for (int i = 0; i < p->building_count && libre; i++) libre = !se_tocan(b, p->buildings[i], STREET);
        if (!libre) continue;
        p->buildings[p->building_count++] = b;
        casa(p, &a, b);
    }
    de_pie(p, &a);
}

bool ancient_wall_at(const AncientPlan *p, float x, float z) {
    for (int i = 0; i < p->door_count; i++)
        if (dentro(p->doors[i], x, z)) return false;
    for (int i = 0; i < p->wall_count; i++)
        if (dentro(p->walls[i].r, x, z)) return true;
    return false;
}

// Ruido de valor (0 a 1) en una rejilla de 1 m, suave entre los nudos.
static float nudo(int i, int j, uint32_t s) { return (float)(mezcla((uint32_t)i * 73856093u ^ (uint32_t)j * 19349663u ^ s) >> 8) / 16777216.0f; }

static float ruido(float x, float z, uint32_t s) {
    int i = (int)floorf(x), j = (int)floorf(z);
    float fx = x - (float)i, fz = z - (float)j;
    fx = fx * fx * (3.0f - 2.0f * fx), fz = fz * fz * (3.0f - 2.0f * fz);
    float a = nudo(i, j, s), b = nudo(i + 1, j, s), c = nudo(i, j + 1, s), d = nudo(i + 1, j + 1, s);
    return (a + (b - a) * fx) * (1.0f - fz) + (c + (d - c) * fx) * fz;
}

float ancient_height(const AncientPlan *p, float x, float z) {
    for (int i = 0; i < p->standing_count; i++)
        if (dentro(p->standing[i].r, x, z)) return 0.0f;
    for (int i = 0; i < p->door_count; i++)
        if (dentro(p->doors[i], x, z)) return 0.0f;
    for (int i = 0; i < p->wall_count; i++) {
        const AncWall *w = &p->walls[i];
        if (!dentro(w->r, x, z)) continue;
        float h = w->h * (0.25f + 1.1f * ruido(x * 0.45f, z * 0.45f, p->seed));
        if (h < 0.22f) return 0.0f; // una brecha: el muro se fue hasta el suelo
        return h < ANC_HEIGHT_MAX ? h : ANC_HEIGHT_MAX;
    }
    return 0.0f;
}
