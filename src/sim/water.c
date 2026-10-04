#include "water.h"

#include <math.h>
#include <string.h>

#include "lang.h"

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

static const DrinkDef DRINKS[DRINK_COUNT] = {
    [DRINK_RAW] = { "utileria.consumible.agua", 35.0f, 0.0f, true },
    [DRINK_BOILED] = { "utileria.consumible.agua_hervida", 35.0f, 0.0f, false },
    [DRINK_WATERED_WINE] = { "utileria.consumible.agua_vino", 30.0f, 0.06f, false },
    [DRINK_MILK] = { "utileria.consumible.leche", 20.0f, 0.0f, false },
    [DRINK_AIRAG] = { "utileria.consumible.airag", 22.0f, 0.12f, false },
    [DRINK_BEER] = { "utileria.consumible.cerveza", 25.0f, 0.25f, false },
    [DRINK_WINE] = { "utileria.consumible.vino", 15.0f, 0.40f, false },
};

const DrinkDef *drink_def(DrinkKind k) { return k >= 0 && k < DRINK_COUNT ? &DRINKS[k] : NULL; }

int drink_find(const char *inv_id) {
    if (!inv_id) return -1;
    for (int k = 0; k < DRINK_COUNT; k++)
        if (!strcmp(DRINKS[k].id, inv_id)) return k;
    return -1;
}

float water_spirit_chance(float temp_mean, bool still) { return 0.15f + 0.25f * clamp01(temp_mean / 25.0f) + (still ? 0.1f : 0.0f); }

void hydration_init(Hydration *h) {
    memset(h, 0, sizeof(*h));
    h->water = THIRST_MAX;
}

#define THIRST_RATE (THIRST_MAX / 1500.0f) // sin calor, de lleno a seco en 25 minutos
#define SOBER_RATE (1.0f / 400.0f)         // el alcohol se pasa
#define CURE_RATE (1.0f / 600.0f)          // los espiritus se van solos en unos 10 minutos

void hydration_update(Hydration *h, float thirst_scale, bool resting, float dt) {
    bool sick = hydration_sick(h);
    h->water -= THIRST_RATE * fmaxf(0.5f, thirst_scale) * (sick ? 1.0f + 0.8f * h->curse : 1.0f) * dt;
    if (h->water < 0.0f) h->water = 0.0f;
    h->drunk = fmaxf(0.0f, h->drunk - SOBER_RATE * (resting ? 1.5f : 1.0f) * dt);
    if (h->incubate > 0.0f) {
        h->incubate -= dt;
        if (h->incubate < 0.0f) h->incubate = 0.0f;
    } else if (h->curse > 0.0f) {
        h->curse = fmaxf(0.0f, h->curse - CURE_RATE * (resting ? 2.0f : 1.0f) * dt);
    }
}

bool hydration_drink(Hydration *h, DrinkKind k, float spirit_chance, Rng *rng) {
    const DrinkDef *d = drink_def(k);
    if (!d) return false;
    h->water = fminf(THIRST_MAX, h->water + d->water);
    h->drunk = fminf(2.0f, h->drunk + d->alcohol);
    if (!d->raw || !rng || rng_float(rng) >= spirit_chance) return false;
    // Espiritus malditos: se incuban de 1 a 2 minutos; cada trago malo los agrava.
    if (h->curse <= 0.0f) h->incubate = 60.0f + 60.0f * rng_float(rng); // si ya estaba, sigue su curso
    h->curse = fminf(1.0f, h->curse + 0.35f + 0.25f * rng_float(rng));
    return true;
}

void hydration_herbs(Hydration *h) {
    if (h->incubate > 0.0f) h->curse *= 0.2f;
    else h->curse *= 0.5f;
    if (h->curse < 0.05f) h->curse = 0.0f, h->incubate = 0.0f;
}

bool hydration_sick(const Hydration *h) { return h->curse > 0.05f && h->incubate <= 0.0f; }

ThirstLevel thirst_level(const Hydration *h) {
    if (h->water <= 0.0f) return THIRST_DRY;
    if (h->water < 15.0f) return THIRST_PARCHED;
    if (h->water < 40.0f) return THIRST_THIRSTY;
    return THIRST_FINE;
}

DrunkLevel drunk_level(const Hydration *h) {
    if (h->drunk > 1.4f) return DRUNK_WASTED;
    if (h->drunk > 0.8f) return DRUNK_DRUNK;
    if (h->drunk > 0.3f) return DRUNK_MERRY;
    return DRUNK_SOBER;
}

const char *thirst_name(ThirstLevel l) {
    static const char *names[] = { N_("sin sed"), N_("con sed"), N_("reseco"), N_("deshidratado") };
    return (unsigned)l <= THIRST_DRY ? T(names[l]) : "?";
}

const char *drunk_name(DrunkLevel l) {
    static const char *names[] = { N_("sobrio"), N_("alegre"), N_("borracho"), N_("ebrio perdido") };
    return (unsigned)l <= DRUNK_WASTED ? T(names[l]) : "?";
}

float hydration_speed_scale(const Hydration *h) {
    float s = 1.0f;
    ThirstLevel t = thirst_level(h);
    if (t == THIRST_THIRSTY) s *= 0.92f;
    else if (t >= THIRST_PARCHED) s *= 0.75f;
    if (hydration_sick(h)) s *= 1.0f - 0.25f * h->curse;
    if (h->drunk > 0.8f) s *= 0.9f;
    return s;
}

float hydration_stamina_scale(const Hydration *h) {
    float s = (1.0f - 0.5f * clamp01(h->drunk)) * (thirst_level(h) >= THIRST_THIRSTY ? 0.7f : 1.0f);
    if (hydration_sick(h)) s *= 1.0f - 0.4f * h->curse;
    return s;
}

float hydration_clumsy(const Hydration *h) { return clamp01((h->drunk - 0.3f) / 1.2f); }

// ------------------------------------------------------------------ la tribu
static void suffer(Member *m, float hp_loss, float morale_loss) {
    m->health.hp = fmaxf(m->health.hp_max * 0.3f, m->health.hp - hp_loss);
    m->morale = fmaxf(0.0f, m->morale - morale_loss);
}

WaterReport water_daily(Stockpile *s, Troop *t, bool water_near, float spirit_chance, Rng *rng) {
    // Lo seguro del acopio, en este orden; lo crudo solo si no queda otra.
    static const DrinkKind SAFE[] = { DRINK_BOILED, DRINK_WATERED_WINE, DRINK_AIRAG, DRINK_BEER, DRINK_MILK, DRINK_WINE };
    WaterReport r = { 0, 0, 0, 0, 0 };
    int wood_left = 0; // agua que todavia hierve la leña ya echada al fuego
    for (int i = 0; i < t->count; i++) {
        Member *m = &t->members[i];
        if (m->status != STATUS_ACTIVE) continue;
        bool drank = false;
        for (size_t k = 0; k < sizeof(SAFE) / sizeof(SAFE[0]) && !drank; k++)
            drank = stock_take(s, drink_def(SAFE[k])->id, 1);
        if (drank) {
            r.safe++;
            continue;
        }
        bool raw_stock = stock_take(s, drink_def(DRINK_RAW)->id, 1);
        if (!raw_stock && !water_near) {
            r.dry++;
            suffer(m, 15.0f, 12.0f);
            continue;
        }
        // Del rio (o del odre crudo): hervida si hay leña.
        if (wood_left <= 0 && stock_take(s, "utileria.objeto.lena", 1)) wood_left = WATER_WOOD_PER;
        if (wood_left > 0) {
            wood_left--;
            r.boiled++;
            continue;
        }
        r.raw++;
        if (rng_float(rng) < spirit_chance) {
            r.sick++;
            suffer(m, 20.0f, 6.0f);
        }
    }
    return r;
}
