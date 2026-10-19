#include "sim/city.h"

#include <math.h>
#include <string.h>

#include "sim/rng.h"

#define PI_F 3.14159265f

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int city_land(SettleKind k) { return k == SETTLE_CAPITAL ? 4000 : 600; }
float city_scribes(SettleKind k) { return k == SETTLE_CAPITAL ? 0.8f : 0.45f; }

float city_fertility(Region r) {
    switch (r) {
    case REGION_FOREST: return 1.0f;
    case REGION_HIGHLAND: return 0.8f;
    case REGION_FJORD: return 0.85f;  // y lo que da el mar
    case REGION_DESERT: return 0.75f; // los oasis riegan poco
    default: return 0.9f;
    }
}

float city_season_yield(Season s) {
    static const float Y[SEASON_COUNT] = { 0.5f, 0.9f, 2.0f, 0.0f }; // primavera, verano, la cosecha, invierno
    return (unsigned)s < SEASON_COUNT ? Y[s] : 0.0f;
}

int season_index(int day) { return day < 1 ? 0 : (day - 1) / DAYS_PER_SEASON; }

float city_season_luck(uint32_t seed, int idx, int day) {
    Rng r;
    rng_seed(&r, seed * 2654435761u ^ (uint32_t)(idx + 1) * 40503u ^ (uint32_t)(season_index(day) + 1) * 2246822519u);
    rng_next(&r);
    return 0.8f + 0.4f * rng_float(&r);
}

float city_taxes(const City *c, SettleKind k) {
    return c->people * CITY_TAX * city_scribes(k) * (1.0f - 0.6f * c->unrest) * (0.6f + 0.4f * c->fields);
}

int city_soldier_target(const City *c, SettleKind k) { return c->taken ? 0 : (int)(c->people / (k == SETTLE_CAPITAL ? 25.0f : 40.0f)); }
float city_reserve(const City *c) { return c->people * 1.5f; }

void city_start(City *c, SettleKind k, Region r, uint32_t seed) {
    Rng rng;
    rng_seed(&rng, seed ^ 0xC17Fu);
    rng_next(&rng);
    memset(c, 0, sizeof(*c));
    bool cap = k == SETTLE_CAPITAL;
    float base = cap ? 2500.0f + (float)rng_range(&rng, 800) : 300.0f + (float)rng_range(&rng, 150);
    c->people = floorf(base * (0.75f + 0.3f * city_fertility(r))); // las tierras buenas, mas pobladas
    c->grain = c->people * 60.0f;
    c->silver = c->people * (cap ? 1.2f : 0.3f);
    c->arms = c->people / (cap ? 15.0f : 60.0f);
    c->fields = 1.0f;
    c->unrest = 0.05f;
    c->soldiers = city_soldier_target(c, k) * 9 / 10;
}

void city_day(City *c, SettleKind k, Region r, Season s, float luck) {
    if (c->people <= 0.0f) return;
    bool cap = k == SETTLE_CAPITAL;
    float land = (float)city_land(k);
    // La cosecha: los labradores que caben en su tierra, segun la estacion, la region y la suerte.
    float farmers = fminf(c->people, land) * CITY_FARMERS;
    float harvest = farmers * CITY_FARM_RATE * city_season_yield(s) * city_fertility(r) * luck * c->fields;
    c->grain += harvest - c->people; // cada alma come una racion
    bool hungry = c->grain < 0.0f;
    c->grain = clampf(c->grain, 0.0f, c->people * (float)CITY_GRANARY_DAYS);
    // La gente: con hambre, se muere o se va; bien comida y tranquila, crece (menos, pasada su tierra).
    if (hungry) {
        c->people = fmaxf(20.0f, c->people * 0.985f);
        c->unrest = clampf(c->unrest + 0.06f, 0.0f, 1.0f);
    } else {
        if (c->grain >= c->people * 30.0f && c->unrest < 0.5f) c->people += c->people * 0.004f * fmaxf(0.0f, 1.0f - c->people / (land * 1.6f));
        if (c->grain >= c->people * 7.0f) c->unrest = clampf(c->unrest - 0.015f, 0.0f, 1.0f);
    }
    // Los impuestos: lo que cobran los escribas.
    c->silver += city_taxes(c, k);
    // Los talleres: armas y armaduras para la armeria, mientras el tesoro no baje de su reserva.
    float floor_silver = city_reserve(c) * 0.3f, room = c->people / (cap ? 8.0f : 30.0f) - c->arms;
    float make = fminf(c->people / (cap ? 300.0f : 2000.0f), room);
    if (make > 0.0f && c->silver > floor_silver) {
        make = fminf(make, (c->silver - floor_silver) / CITY_ARMS_COST);
        c->arms += make, c->silver -= make * CITY_ARMS_COST;
    }
    // La paga: primero los soldados; si no alcanza, los mercenarios se van y los soldados desertan.
    float pay = (float)c->soldiers * CITY_SOLDIER_PAY;
    if (c->silver >= pay) {
        c->silver -= pay;
    } else {
        c->silver = 0.0f;
        c->mercs = 0;
        c->soldiers -= (int32_t)ceilf((float)c->soldiers * 0.05f);
        c->unrest = clampf(c->unrest + 0.03f, 0.0f, 1.0f);
    }
    float mpay = (float)c->mercs * CITY_MERC_PAY;
    if (c->silver >= mpay) {
        c->silver -= mpay;
    } else { // los que no cobran, se van
        c->mercs = (int32_t)(c->silver / CITY_MERC_PAY);
        c->silver -= (float)c->mercs * CITY_MERC_PAY;
    }
    // Reclutar: soldados, si hay armas y paga para veinte dias; mercenarios, con lo que sobra de la reserva.
    int want = city_soldier_target(c, k);
    if (c->soldiers < want && c->silver >= (float)(c->soldiers + 1) * CITY_SOLDIER_PAY * 20.0f) {
        int add = clampi(want / 30, 1, want - c->soldiers);
        if (add > (int)c->arms) add = (int)c->arms;
        c->soldiers += add, c->arms -= (float)add;
    } else if (c->soldiers > want) {
        c->soldiers = want; // menos gente: se licencia a los que sobran
    }
    float spare = c->silver - city_reserve(c);
    if (spare > 0.0f && !c->taken) { // tomada, no contrata: paga tributo a la tribu
        int target = (int)(spare / (CITY_MERC_PAY * 30.0f));
        if (target > c->mercs) c->mercs += clampi(target - c->mercs, 0, 4);
    } else if (c->mercs > 0) {
        c->mercs -= c->mercs < 2 ? c->mercs : 2; // bajo la reserva (o tomada): no se renueva el contrato
    }
    // El rencor se olvida despacio; los campos se resiembran (en invierno, no).
    static const float SOW[SEASON_COUNT] = { 0.06f, 0.03f, 0.01f, 0.0f };
    c->grudge = fmaxf(0.0f, c->grudge - 0.6f);
    c->fields = fminf(1.0f, c->fields + SOW[(unsigned)s < SEASON_COUNT ? s : SEASON_WINTER]);
}

int city_raid_fields(City *c, Season s, int day) {
    if (c->people <= 0.0f || (c->raided > 0 && day - c->raided < CITY_RAID_DAYS)) return -1;
    static const float TAKE[SEASON_COUNT] = { 50.0f, 80.0f, 120.0f, 30.0f }; // en otoño, la cosecha en las eras
    int take = (int)fminf(c->grain, TAKE[(unsigned)s < SEASON_COUNT ? s : SEASON_WINTER]);
    c->grain = fmaxf(0.0f, c->grain - (float)take - c->grain * 0.02f); // y lo que se quema
    c->fields = fmaxf(0.0f, c->fields - 0.35f);
    c->unrest = clampf(c->unrest + 0.15f, 0.0f, 1.0f);
    c->grudge = fminf(100.0f, c->grudge + 30.0f);
    c->raided = day;
    return take;
}

void city_lose_soldiers(City *c, int n) {
    for (; n > 0; n--) {
        if (c->soldiers > 0) c->soldiers--;
        else if (c->mercs > 0) c->mercs--;
        c->people = fmaxf(20.0f, c->people - 1.0f);
        c->grudge = fminf(100.0f, c->grudge + 6.0f);
    }
}

int city_guard_party(const City *c, int max) {
    int all = c->soldiers + c->mercs;
    return clampi(clampi(all / 15, 2, max), 0, all);
}

static bool is_food(const char *id) {
    static const char *const FOOD[] = { "utileria.consumible.carne_fresca", "utileria.consumible.carne_seca", "utileria.consumible.leche",
                                        "utileria.consumible.pan",          "utileria.consumible.queso_seco", "utileria.consumible.miel",
                                        "utileria.consumible.forraje",      GRAIN_SACK_ID };
    for (size_t i = 0; i < sizeof(FOOD) / sizeof(FOOD[0]); i++)
        if (!strcmp(id, FOOD[i])) return true;
    return false;
}

float city_price_factor(const City *c, const char *id) {
    if (!c || c->people <= 0.0f || !id) return 1.0f;
    float f = 1.0f;
    if (is_food(id)) { // el granero: con hambre, la comida vale oro; con el granero lleno, poco
        float days = c->grain / c->people;
        f = days < 10.0f ? 1.6f : days < 25.0f ? 1.25f : days > 90.0f ? 0.8f : 1.0f;
    } else if (!strncmp(id, "arma.", 5) || !strncmp(id, "armadura.", 9) || !strncmp(id, "escudo.", 7)) {
        float need = fmaxf(1.0f, (float)c->soldiers); // la armeria llena abarata las armas
        f = c->arms > need * 1.5f ? 0.85f : c->arms < need * 0.3f ? 1.2f : 1.0f;
    }
    if (c->unrest > 0.6f) f *= 1.1f;
    return f;
}

bool city_trades(const City *c, bool overlord, float relation) {
    if (c && c->taken) return true; // tomada: es de la tribu
    if (c && c->grudge >= CITY_GRUDGE_NO_TRADE) return false;
    return !overlord || relation > OVERLORD_NO_TRADE;
}

int tribute_due(int members) { return 4 + 4 * (members > 0 ? members : 0); }

// ---------------------------------------------------------------- el trazado
// En seco, fuera del lecho de los rios y en llano: el centro y las cuatro esquinas.
static bool lot_ok(const World *w, float x, float z, float yaw, float wd, float ln) {
    float ca = cosf(yaw), sa = sinf(yaw), lo = 1e9f, hi = -1e9f;
    static const float P[5][2] = { { 0, 0 }, { 0.5f, 0.5f }, { 0.5f, -0.5f }, { -0.5f, 0.5f }, { -0.5f, -0.5f } };
    for (int i = 0; i < 5; i++) {
        float a = P[i][0] * ln, b = P[i][1] * wd, px = x + ca * a - sa * b, pz = z + sa * a + ca * b;
        float h = world_height(w, px, pz);
        if (world_water(w, px, pz, 0.0f, NULL) > h - 0.3f || world_river_bank(w, px, pz, NULL) > 0.4f || world_edge_distance(w, px, pz) < 60.0f)
            return false;
        lo = fminf(lo, h), hi = fmaxf(hi, h);
    }
    return hi - lo < 0.24f * fmaxf(wd, ln); // una cuesta suave (en el altiplano, en terrazas)
}

static void add_lot(CityLot *out, int *n, int max, float x, float z, float yaw, float wd, float ln, LotKind k) {
    if (*n < max) out[(*n)++] = (CityLot){ x, z, yaw, wd, ln, k };
}

int city_layout(const World *w, int idx, const City *c, CityLot *out, int max) {
    if (!w || idx < 0 || idx >= w->settlement_count || !out || max <= 0) return 0;
    const Settlement *s = &w->settlements[idx];
    bool cap = s->kind == SETTLE_CAPITAL;
    int people = c && c->people > 0.0f ? (int)c->people : cap ? 2000 : 240;
    Rng rng;
    rng_seed(&rng, w->seed * 2654435761u ^ (uint32_t)(idx + 1) * 0x9E3779B1u ^ 0xC17Au);
    rng_next(&rng);
    int n = 0;
    // La capital mira a una de sus cuatro puertas (la muralla deja huecos a 15, 105, 195 y 285 grados).
    float gate = (15.0f + 90.0f * (float)rng_range(&rng, 4)) * PI_F / 180.0f, house_r[2], ring_n[2];
    if (cap) {
        add_lot(out, &n, max, s->x, s->z, gate, 12.0f, 14.0f, LOT_PALACE);
        static const LotKind Q[3] = { LOT_FORGE, LOT_BARRACKS, LOT_CARPENTRY }; // los barrios, alrededor
        for (int q = 0; q < 3; q++) {
            float a = gate + 2.0944f * ((float)q + 0.5f);
            add_lot(out, &n, max, s->x + cosf(a) * 16.0f, s->z + sinf(a) * 16.0f, a + PI_F, 6.0f, 6.0f, Q[q]);
        }
        house_r[0] = 24.0f, house_r[1] = 33.0f, ring_n[0] = 10.0f, ring_n[1] = 14.0f;
    } else {
        house_r[0] = 14.0f, house_r[1] = 17.0f, ring_n[0] = 9.0f, ring_n[1] = 0.0f;
    }
    // Las casas: crece desde el centro (el anillo de adentro primero) con la gente.
    int houses = clampi(people / (cap ? 150 : 70), cap ? 6 : 4, cap ? 24 : 9);
    for (int ring = 0, made = 0; ring < 2; ring++)
        for (int k = 0; k < (int)ring_n[ring] && made < houses; k++, made++) {
            float a = gate + ((float)k + 0.5f) * 2.0f * PI_F / ring_n[ring] + (float)ring * 0.12f;
            add_lot(out, &n, max, s->x + cosf(a) * house_r[ring], s->z + sinf(a) * house_r[ring], a + PI_F, 0.0f, 0.0f, LOT_HOUSE);
        }
    // Las tiendas de los mercenarios, junto a la puerta principal, afuera (mas alla del antemuro).
    if (cap && c) {
        static const float TENT[4] = { -0.09f, 0.09f, -0.2f, 0.2f };
        int tents = clampi((c->mercs + 11) / 12, 0, 4);
        for (int k = 0; k < tents; k++) {
            float a = gate + TENT[k], x = s->x + cosf(a) * 55.0f, z = s->z + sinf(a) * 55.0f;
            if (lot_ok(w, x, z, a, 3.0f, 3.0f)) add_lot(out, &n, max, x, z, a + PI_F, 3.0f, 3.0f, LOT_MERC_TENT);
        }
    }
    // Los campos y los corrales: huecos fijos (el mismo lugar aunque la ciudad crezca), en orden para
    // que los primeros queden repartidos. La capital, entre sus cuatro puertas; la aldea, alrededor.
    float r0 = cap ? CITY_FIELDS_R0 : VILLAGE_FIELDS_R0, r1 = cap ? CITY_FIELDS_R1 : VILLAGE_FIELDS_R1;
    float fw = cap ? 11.0f : 9.0f, fl = cap ? 16.0f : 13.0f;
    int slots = cap ? 18 : 10, fields = clampi(people / (cap ? 250 : 80), cap ? 4 : 3, cap ? 16 : 9);
    static const int ORDER_CAP[18] = { 0, 8, 4, 12, 16, 2, 10, 6, 14, 17, 1, 9, 5, 13, 3, 11, 7, 15 };
    static const int ORDER_VIL[10] = { 0, 5, 2, 7, 4, 9, 1, 6, 3, 8 };
    float jit[18];
    for (int k = 0; k < slots; k++) jit[k] = rng_float(&rng) - 0.5f; // el azar se toma siempre igual
    for (int o = 0, made = 0; o < slots && made < fields; o++) {
        int k = cap ? ORDER_CAP[o] : ORDER_VIL[o];
        float a, r;
        LotKind kind = LOT_FIELD;
        float lw = fw, ll = fl;
        if (cap) {
            int q = k / 4, j = k % 4; // cuatro por cuarto, entre dos puertas; 16 y 17: los corrales
            if (k >= 16) {
                q = (k - 16) * 2 + 1, kind = LOT_CORRAL, lw = 8.0f, ll = 10.0f;
                a = gate + (45.0f + 90.0f * (float)q) * PI_F / 180.0f, r = r0 + 4.0f;
            } else {
                static const float OFF[4] = { -27.0f, -9.0f, 9.0f, 27.0f };
                a = gate + (45.0f + 90.0f * (float)q + OFF[j] + jit[k] * 4.0f) * PI_F / 180.0f;
                r = (j == 1 || j == 2) ? r1 - ll * 0.5f - 1.0f : r0 + ll * 0.5f + 1.0f;
            }
        } else {
            a = gate + ((float)k + jit[k] * 0.3f) * 2.0f * PI_F / (float)slots;
            r = k % 2 ? r1 - ll * 0.5f : r0 + ll * 0.5f;
            if (k == 5) kind = LOT_CORRAL, lw = 7.0f, ll = 9.0f, r = r0 + 4.5f;
        }
        float x = s->x + cosf(a) * r, z = s->z + sinf(a) * r;
        if (!lot_ok(w, x, z, a, lw, ll)) continue;
        add_lot(out, &n, max, x, z, a, lw, ll, kind);
        if (kind == LOT_FIELD) made++;
    }
    return n;
}

int city_lot_at(const CityLot *lots, int n, LotKind kind, float x, float z, float margin) {
    for (int i = 0; i < n; i++) {
        const CityLot *l = &lots[i];
        if (l->kind != kind) continue;
        float dx = x - l->x, dz = z - l->z, ca = cosf(l->yaw), sa = sinf(l->yaw);
        float along = dx * ca + dz * sa, across = -dx * sa + dz * ca;
        if (fabsf(along) <= l->l * 0.5f + margin && fabsf(across) <= l->w * 0.5f + margin) return i;
    }
    return -1;
}
