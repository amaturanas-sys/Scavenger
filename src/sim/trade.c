#include "sim/trade.h"

#include <math.h>
#include <string.h>

#define R(x) (1u << (x))
#define STEPPE R(REGION_STEPPE)
#define FOREST R(REGION_FOREST)
#define HIGH R(REGION_HIGHLAND)
#define FJORD R(REGION_FJORD)
#define DESERT R(REGION_DESERT)
#define NOT_DESERT (STEPPE | FOREST | HIGH | FJORD)

// Valor en plata y donde abunda (mas barato) o escasea (mas caro). Una clave que termina en
// punto vale para todo lo que empieza asi; la primera que coincide manda.
typedef struct {
    const char *key;
    float value;
    unsigned cheap, dear;
} PriceRule;

static const PriceRule RULES[] = {
    { SILVER_ID, 1.0f, 0, 0 }, // la moneda no cambia de precio
    { "utileria.material.oro", 10.0f, 0, 0 },
    // Comida y bebida: la estepa es del ganado; el desierto, de los oasis y las caravanas.
    { "utileria.consumible.carne_seca", 2.0f, STEPPE, DESERT | FJORD },
    { "utileria.consumible.carne_fresca", 1.0f, STEPPE, DESERT },
    { "utileria.consumible.leche", 1.0f, STEPPE, DESERT | FJORD },
    { "utileria.consumible.queso_seco", 3.0f, STEPPE, DESERT | FJORD },
    { "utileria.consumible.airag", 4.0f, STEPPE, DESERT | FOREST },
    { "utileria.consumible.pan", 2.0f, FOREST | DESERT, STEPPE },
    { "utileria.consumible.vino", 6.0f, DESERT, FJORD | HIGH },
    { "utileria.consumible.cerveza", 3.0f, FOREST | FJORD, DESERT },
    { "utileria.consumible.miel", 4.0f, FOREST, DESERT },
    { "utileria.consumible.hierbas", 2.0f, FOREST | STEPPE, DESERT },
    { "utileria.consumible.unguento", 6.0f, 0, 0 },
    { "utileria.consumible.forraje", 1.0f, STEPPE, DESERT | HIGH },
    { "utileria.consumible.especias", 12.0f, DESERT, NOT_DESERT },
    { "utileria.consumible.agua", 0.0f, 0, 0 }, // el agua no se vende
    { "utileria.consumible.agua_hervida", 0.0f, 0, 0 },
    { "utileria.consumible.agua_vino", 0.0f, 0, 0 },
    // Madera y barro.
    { "utileria.material.troncos", 1.0f, FOREST, DESERT | STEPPE },
    { "utileria.objeto.lena", 1.0f, FOREST, DESERT },
    { "utileria.material.madera_labrada", 3.0f, FOREST, DESERT | STEPPE },
    { "utileria.material.carbon", 2.0f, FOREST, DESERT },
    { "utileria.material.piedra", 1.0f, HIGH, 0 },
    { "utileria.material.barro", 1.0f, 0, 0 },
    { "utileria.material.adobe", 2.0f, DESERT, FOREST | FJORD },
    // Del ganado y de la caza.
    { "utileria.material.pieles", 4.0f, FOREST | FJORD, DESERT },
    { "utileria.material.cuero", 10.0f, STEPPE, DESERT },
    { "utileria.material.fieltro", 10.0f, STEPPE, DESERT | FOREST },
    { "utileria.piel.cabra", 4.0f, STEPPE, DESERT },
    { "utileria.piel.", 10.0f, FOREST | FJORD | HIGH, DESERT | STEPPE },
    { "utileria.material.grasa", 2.0f, STEPPE, 0 },
    { "utileria.material.hueso", 1.0f, 0, 0 },
    { "utileria.material.tendones", 2.0f, 0, 0 },
    { "utileria.material.plumas", 1.0f, 0, 0 },
    { "utileria.material.pedernal", 1.0f, 0, 0 },
    { "utileria.material.puntas_hueso", 0.5f, 0, 0 },
    { "utileria.material.piezas_arco", 6.0f, 0, 0 },
    { "utileria.material.cuerda", 3.0f, 0, 0 },
    // Metales: del altiplano.
    { "utileria.material.cobre", 4.0f, HIGH, STEPPE | DESERT },
    { "utileria.material.estano", 5.0f, HIGH, STEPPE | DESERT },
    { "utileria.material.hierro", 6.0f, HIGH, STEPPE | DESERT },
    { "utileria.material.bronce", 14.0f, HIGH, STEPPE | DESERT },
    { "utileria.material.seda", 15.0f, DESERT, FOREST | FJORD | HIGH },
    // Objetos.
    { "utileria.objeto.vela", 1.0f, 0, 0 },
    { "utileria.objeto.antorcha", 2.0f, FOREST, DESERT },
    { "utileria.objeto.peine", 5.0f, 0, 0 },
    { "utileria.objeto.aljaba", 14.0f, STEPPE, 0 },
    { "utileria.objeto.odre", 4.0f, 0, 0 },
    { "utileria.objeto.caldero", 20.0f, 0, 0 },
    { "utileria.objeto.cofre", 25.0f, 0, 0 },
    { "utileria.objeto.saco_grano", 8.0f, FOREST | DESERT, STEPPE },
    { "utileria.objeto.", 5.0f, 0, 0 },
    { "utileria.gema.", 25.0f, 0, 0 },
    { "utileria.mochila.", 10.0f, 0, 0 },
    { "utileria.herramienta.", 8.0f, 0, 0 },
    // Arreos y armas.
    { "accesorio.arreo.silla_montar", 40.0f, STEPPE, 0 },
    { "accesorio.arreo.brida", 15.0f, STEPPE, 0 },
    { "accesorio.arreo.estribos", 15.0f, STEPPE, 0 },
    { "accesorio.arreo.alforjas", 24.0f, STEPPE, 0 },
    { "accesorio.amuleto.", 30.0f, 0, 0 },
    { "armadura.casco.laminar_cuero", 26.0f, STEPPE, 0 },
    { "armadura.brazales.laminar_cuero", 14.0f, STEPPE, 0 },
    { "armadura.grebas.laminar_cuero", 16.0f, STEPPE, 0 },
    { "armadura.torso.laminar_cuero", 28.0f, STEPPE, 0 },
    { "arma.especial.", 120.0f, 0, 0 },
    { "arma.distancia.", 35.0f, 0, 0 },
    { "arma.larga.", 30.0f, 0, 0 },
    { "arma.corta.", 25.0f, 0, 0 },
    { "arma.", 20.0f, 0, 0 },
    { "escudo.", 20.0f, 0, 0 },
    { "armadura.", 22.0f, 0, 0 },
    { "vestimenta.", 10.0f, 0, 0 },
    { "proyectil.", 1.0f, 0, 0 },
};

static const PriceRule *rule_of(const char *id) {
    if (!id) return NULL;
    size_t n = strlen(id);
    for (size_t i = 0; i < sizeof(RULES) / sizeof(RULES[0]); i++) {
        const char *k = RULES[i].key;
        size_t kl = strlen(k);
        bool prefix = kl > 0 && k[kl - 1] == '.';
        if (prefix ? !strncmp(id, k, kl) : (kl == n && !strcmp(id, k))) return &RULES[i];
    }
    return NULL;
}

float item_value(const char *id) {
    if (!id) return 0.0f;
    // Las joyas: segun su metal.
    if (!strncmp(id, "accesorio.", 10) && strncmp(id, "accesorio.amuleto.", 18) && strncmp(id, "accesorio.arreo.", 16)) {
        size_t n = strlen(id);
        if (n > 4 && !strcmp(id + n - 4, ".oro")) return 90.0f;
        if (n > 6 && !strcmp(id + n - 6, ".plata")) return 35.0f;
        if (n > 7 && !strcmp(id + n - 7, ".bronce")) return 12.0f;
    }
    const PriceRule *r = rule_of(id);
    return r ? r->value : 0.0f;
}

float stock_value(const Stockpile *s) {
    float v = 0.0f;
    for (int i = 0; i < s->n; i++) v += item_value(s->e[i].id) * (float)s->e[i].count;
    return v;
}

float price_factor(const char *id, Region r) {
    const PriceRule *p = rule_of(id);
    if (!p || r < 0 || r >= REGION_COUNT) return 1.0f;
    if (p->cheap & R(r)) return PRICE_CHEAP;
    if (p->dear & R(r)) return PRICE_DEAR;
    return 1.0f;
}

static int price(const char *id, Region r, float k) {
    float v = item_value(id);
    if (v <= 0.0f) return 0;
    int p = (int)floorf(v * price_factor(id, r) * k + 0.5f);
    return p < 1 ? 1 : p;
}

int trade_sell_price(const char *id, Region r) { return id && !strcmp(id, SILVER_ID) ? 1 : price(id, r, TRADE_SELL_CUT); }
int trade_buy_price(const char *id, Region r) { return id && !strcmp(id, SILVER_ID) ? 1 : price(id, r, TRADE_BUY_MARKUP); }

// ---------------------------------------------------------------- mercados
static const char *const KIND_GOODS[MARKET_KINDS][8] = {
    [MARKET_TRIBE] = { "utileria.consumible.carne_seca", "utileria.consumible.queso_seco", "utileria.consumible.airag", "utileria.material.cuero",
                       "utileria.material.fieltro", "utileria.consumible.forraje", "utileria.material.pieles", NULL },
    [MARKET_VILLAGE] = { "utileria.consumible.pan", "utileria.consumible.cerveza", "utileria.material.cuerda", "utileria.material.carbon",
                         "utileria.objeto.vela", "utileria.objeto.antorcha", "proyectil.flecha.comun", NULL },
    [MARKET_CAPITAL] = { "arma.corta.sable_acero", "escudo.mano.lamina", "armadura.casco.bronce", "utileria.material.bronce",
                         "utileria.consumible.vino", "accesorio.anillo.plata", "accesorio.arreo.silla_montar", NULL },
    [MARKET_CARAVAN] = { "utileria.material.seda", "utileria.consumible.vino", "utileria.consumible.especias", "utileria.gema.turquesa",
                         "utileria.gema.lapislazuli", "utileria.material.oro", NULL },
};

static const char *const REGION_GOODS[REGION_COUNT][5] = {
    [REGION_STEPPE] = { "utileria.consumible.queso_seco", "utileria.material.fieltro", "utileria.consumible.forraje", "utileria.piel.cabra", NULL },
    [REGION_FOREST] = { "utileria.material.troncos", "utileria.material.madera_labrada", "utileria.consumible.miel", "utileria.piel.lobo", NULL },
    [REGION_HIGHLAND] = { "utileria.material.cobre", "utileria.material.estano", "utileria.material.hierro", "utileria.gema.lapislazuli", NULL },
    [REGION_FJORD] = { "utileria.piel.reno", "utileria.gema.ambar", "utileria.material.pieles", "utileria.consumible.cerveza", NULL },
    [REGION_DESERT] = { "utileria.consumible.especias", "utileria.material.seda", "utileria.gema.turquesa", "utileria.material.adobe", NULL },
};

static int add_good(const char **out, int n, int max, const char *id) {
    if (n >= max) return n;
    for (int i = 0; i < n; i++)
        if (!strcmp(out[i], id)) return n;
    out[n] = id;
    return n + 1;
}

int market_goods(MarketKind k, Region r, const char **out, int max) {
    int n = 0;
    if (k >= 0 && k < MARKET_KINDS)
        for (int i = 0; i < 8 && KIND_GOODS[k][i]; i++) n = add_good(out, n, max, KIND_GOODS[k][i]);
    if (r >= 0 && r < REGION_COUNT)
        for (int i = 0; i < 5 && REGION_GOODS[r][i]; i++) n = add_good(out, n, max, REGION_GOODS[r][i]);
    return n;
}

// ---------------------------------------------------------------- caravanas
int caravan_routes(const World *w, CaravanRoute *out, int max) {
    int n = 0;
    for (int i = 0; i < w->site_count && n < max; i++) {
        const WorldSite *s = &w->sites[i];
        if (s->kind != SITE_CARAVANSERAI) continue;
        int best = -1;
        float bd = 1e30f;
        for (int k = 0; k < w->settlement_count; k++) {
            const Settlement *c = &w->settlements[k];
            float d = (c->x - s->x) * (c->x - s->x) + (c->z - s->z) * (c->z - s->z);
            if (c->kind == SETTLE_CAPITAL && d < bd) bd = d, best = k;
        }
        if (best < 0) continue;
        out[n] = (CaravanRoute){ s->x, s->z, w->settlements[best].x, w->settlements[best].z, (float)n * 977.0f };
        n++;
    }
    return n;
}

void caravan_at(const CaravanRoute *r, float t, float *x, float *z, float *yaw) {
    float dx = r->bx - r->ax, dz = r->bz - r->az, len = sqrtf(dx * dx + dz * dz);
    if (len < 1.0f) {
        *x = r->ax, *z = r->az, *yaw = 0.0f;
        return;
    }
    float s = fmodf(t * CARAVAN_SPEED + r->offset, 2.0f * len);
    if (s < 0.0f) s += 2.0f * len;
    bool back = s > len;
    float u = back ? 2.0f * len - s : s; // metros desde el caravasar
    *x = r->ax + dx * u / len;
    *z = r->az + dz * u / len;
    *yaw = back ? atan2f(-dx, -dz) : atan2f(dx, dz);
}
