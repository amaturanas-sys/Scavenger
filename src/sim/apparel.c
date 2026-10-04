#include "apparel.h"

#include <math.h>
#include <string.h>

#include "animals.h"
#include "lang.h"

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

//                                                     capa        abrigo sombra lluvia peso   escarm. color
static const GarmentDef GARMENTS[] = {
    // Cabeza.
    { "vestimenta.cabeza.gorro_piel", WEAR_HEAD, 4.0f, 0.10f, 0.10f, 0.000f, 0.00f, 128, 96, 64 },
    { "vestimenta.cabeza.gorro_punta", WEAR_HEAD, 2.5f, 0.20f, 0.10f, 0.000f, 0.00f, 150, 60, 50 },
    { "vestimenta.cabeza.sombrero", WEAR_HEAD, 0.5f, 0.50f, 0.20f, 0.000f, 0.00f, 196, 170, 120 },
    { "vestimenta.cabeza.gorro_lobo", WEAR_HEAD, 3.5f, 0.15f, 0.10f, 0.000f, 0.10f, 120, 116, 108 },
    // Cuello y cara.
    { "vestimenta.cuello.bufanda", WEAR_FACE, 1.5f, 0.15f, 0.00f, 0.000f, 0.00f, 170, 150, 110 },
    { "vestimenta.cuello.panuelo_desierto", WEAR_FACE, 0.0f, 0.30f, 0.00f, 0.000f, 0.00f, 226, 220, 200 },
    { "vestimenta.cuello.bufanda_piel", WEAR_FACE, 3.0f, 0.05f, 0.05f, 0.000f, 0.03f, 176, 120, 70 },
    // Cuerpo.
    { "vestimenta.torso.deel", WEAR_BODY, 4.0f, 0.10f, 0.10f, 0.000f, 0.00f, 60, 90, 140 },
    { "vestimenta.torso.deel_invierno", WEAR_BODY, 9.0f, 0.10f, 0.20f, 0.020f, 0.00f, 90, 60, 50 },
    { "vestimenta.torso.tunica_campesina", WEAR_BODY, 2.0f, 0.10f, 0.05f, 0.000f, 0.00f, 150, 130, 100 },
    { "vestimenta.torso.tunica_seda", WEAR_BODY, 0.0f, 0.35f, 0.00f, 0.000f, 0.00f, 236, 232, 222 },
    { "vestimenta.torso.tunica_lana", WEAR_BODY, 5.0f, 0.10f, 0.10f, 0.005f, 0.00f, 130, 110, 80 },
    { "vestimenta.torso.harapos", WEAR_BODY, 0.5f, 0.05f, 0.00f, 0.000f, 0.00f, 110, 100, 86 },
    // Capas y abrigos (las pieles de depredador espantan).
    { "vestimenta.espalda.capa", WEAR_CLOAK, 4.0f, 0.15f, 0.35f, 0.010f, 0.00f, 110, 80, 60 },
    { "vestimenta.espalda.capa_piel", WEAR_CLOAK, 7.0f, 0.15f, 0.30f, 0.015f, 0.15f, 128, 124, 116 },
    { "vestimenta.espalda.abrigo_oso", WEAR_CLOAK, 13.0f, 0.15f, 0.40f, 0.050f, 0.30f, 92, 64, 42 },
    { "vestimenta.espalda.abrigo_tigre", WEAR_CLOAK, 10.0f, 0.15f, 0.30f, 0.030f, 0.35f, 214, 130, 50 },
    { "vestimenta.espalda.capa_puma", WEAR_CLOAK, 7.0f, 0.15f, 0.25f, 0.015f, 0.20f, 184, 140, 90 },
    { "vestimenta.espalda.capa_hiena", WEAR_CLOAK, 6.0f, 0.15f, 0.25f, 0.015f, 0.12f, 160, 140, 100 },
    { "vestimenta.espalda.abrigo_reno", WEAR_CLOAK, 11.0f, 0.15f, 0.30f, 0.030f, 0.00f, 150, 120, 90 },
    { "vestimenta.espalda.abrigo_cabra", WEAR_CLOAK, 9.0f, 0.15f, 0.20f, 0.020f, 0.00f, 220, 210, 190 },
    { "vestimenta.espalda.manto_blanco", WEAR_CLOAK, 0.5f, 0.40f, 0.10f, 0.000f, 0.00f, 240, 236, 226 },
    // Pies.
    { "vestimenta.pies.botas_fieltro", WEAR_FEET, 3.0f, 0.00f, 0.10f, 0.000f, 0.00f, 120, 100, 80 },
    { "vestimenta.pies.botas_piel", WEAR_FEET, 4.5f, 0.00f, 0.40f, 0.005f, 0.00f, 110, 84, 60 },
    { "vestimenta.pies.sandalias", WEAR_FEET, -0.5f, 0.05f, 0.00f, 0.000f, 0.00f, 150, 110, 70 },
};
#define GARMENT_N ((int)(sizeof(GARMENTS) / sizeof(GARMENTS[0])))

int garment_count(void) { return GARMENT_N; }
const GarmentDef *garment(int i) { return i >= 0 && i < GARMENT_N ? &GARMENTS[i] : NULL; }

int garment_find(const char *inv_id) {
    if (!inv_id) return -1;
    for (int i = 0; i < GARMENT_N; i++)
        if (!strcmp(GARMENTS[i].id, inv_id)) return i;
    return -1;
}

const char *wear_slot_name(WearSlot s) {
    static const char *names[WEAR_COUNT] = { N_("cabeza"), N_("cuello y cara"), N_("cuerpo"), N_("capa o abrigo"), N_("pies") };
    return (unsigned)s < WEAR_COUNT ? T(names[s]) : "?";
}

void outfit_clear(Outfit *o) {
    for (int s = 0; s < WEAR_COUNT; s++) o->g[s] = -1;
}

int outfit_wear(Outfit *o, int idx) {
    const GarmentDef *g = garment(idx);
    if (!g) return -1;
    int old = o->g[g->slot];
    o->g[g->slot] = (signed char)idx;
    return old;
}

float outfit_warmth(const Outfit *o) {
    float w = 0.0f;
    for (int s = 0; s < WEAR_COUNT; s++)
        if (garment(o->g[s])) w += garment(o->g[s])->warmth;
    return w;
}

static float combine(const Outfit *o, int field) {
    float pass = 1.0f;
    for (int s = 0; s < WEAR_COUNT; s++) {
        const GarmentDef *g = garment(o->g[s]);
        if (g) pass *= 1.0f - (field ? g->rain : g->shade);
    }
    return 1.0f - pass;
}

float outfit_shade(const Outfit *o) { return combine(o, 0); }
float outfit_rain(const Outfit *o) { return combine(o, 1); }

float outfit_dread(const Outfit *o) {
    float d = 0.0f;
    for (int s = 0; s < WEAR_COUNT; s++)
        if (garment(o->g[s])) d += garment(o->g[s])->dread;
    return fminf(0.5f, d);
}

float outfit_speed_scale(const Outfit *o) {
    float w = 0.0f;
    for (int s = 0; s < WEAR_COUNT; s++)
        if (garment(o->g[s])) w += garment(o->g[s])->weight;
    return 1.0f - fminf(0.2f, w);
}

const char *species_pelt(int species) {
    switch (species) {
    case SPECIES_WOLF: return "utileria.piel.lobo";
    case SPECIES_BEAR: return "utileria.piel.oso";
    case SPECIES_TIGER: return "utileria.piel.tigre";
    case SPECIES_PUMA: return "utileria.piel.puma";
    case SPECIES_HYENA: return "utileria.piel.hiena";
    case SPECIES_COYOTE: return "utileria.piel.coyote";
    case SPECIES_REINDEER: return "utileria.piel.reno";
    case SPECIES_GOAT: return "utileria.piel.cabra";
    default: return NULL;
    }
}

// ------------------------------------------------------------------- sol
float sun_strength(float sun_height, float clouds, float desert) {
    return clamp01(sun_height) * (1.0f - 0.75f * clamp01(clouds)) * (0.8f + 0.2f * clamp01(desert));
}

float local_temperature(float temperature, float sun_height, float desert) {
    float d = clamp01(desert);
    return temperature + d * (sun_height > 0.0f ? 11.0f : 7.0f) * sun_height;
}

float apparel_feels_like(float temperature, float wind, float wet, float sun, const Outfit *o, float extra, float fire) {
    float shade = o ? outfit_shade(o) : 0.0f;
    float warm = o ? outfit_warmth(o) : 0.0f;
    // El abrigo corta el viento (hasta la mitad con mucha ropa).
    float windcut = 1.0f - 0.5f * clamp01(warm / 16.0f);
    return temperature + warm + extra + fire - wind * 10.0f * windcut - wet * 10.0f + clamp01(sun) * (1.0f - shade) * 9.0f;
}

float apparel_wet_scale(const Outfit *o) { return o ? 1.0f - 0.8f * outfit_rain(o) : 1.0f; }

// ----------------------------------------------------------------- calor
void heat_update(HeatStress *h, float feels, float wet, bool shade_here, float dt) {
    if (feels > HEAT_COMFORT) h->load += (feels - HEAT_COMFORT) * 0.012f * (1.0f - 0.5f * clamp01(wet)) * dt;
    else h->load -= (0.25f + (HEAT_COMFORT - feels) * 0.02f) * dt;
    if (shade_here) h->load -= 0.3f * dt;
    if (wet > 0.3f) h->load -= 0.2f * wet * dt; // mojarse refresca
    if (h->load < 0.0f) h->load = 0.0f;
    if (h->load > HEAT_MAX) h->load = HEAT_MAX;
}

HeatLevel heat_level(const HeatStress *h) {
    if (h->load >= HEAT_MAX) return HEAT_STROKE;
    if (h->load > 75.0f) return HEAT_EXHAUSTED;
    if (h->load > 50.0f) return HEAT_HOT;
    if (h->load > 25.0f) return HEAT_WARM;
    return HEAT_FINE;
}

const char *heat_name(HeatLevel l) {
    static const char *names[] = { N_("a gusto"), N_("acalorado"), N_("sofocado"), N_("agotado por el calor"), N_("golpe de calor") };
    return (unsigned)l <= HEAT_STROKE ? T(names[l]) : "?";
}

float heat_speed_scale(const HeatStress *h) { return h->load <= 50.0f ? 1.0f : 1.0f - 0.35f * (h->load - 50.0f) / 50.0f; }
float heat_thirst_scale(const HeatStress *h) { return 1.0f + 1.5f * clamp01(h->load / HEAT_MAX); }

// ------------------------------------------------------------------ NPCs
#define COMFORT_TARGET 17.0f // sensacion termica a la que se esta a gusto

float comfort_need(float temperature, float wind, float sun) { return COMFORT_TARGET - (temperature - wind * 10.0f + clamp01(sun) * 9.0f); }

int garment_pick(WearSlot s, float need, float sun, const int *cands, int n) {
    float best = -fabsf(need) - 0.5f; // sin nada en esa capa (un poco peor: algo siempre ayuda si encaja)
    int pick = -1;
    for (int i = 0; i < n; i++) {
        const GarmentDef *g = garment(cands[i]);
        if (!g || g->slot != s) continue;
        float score = -fabsf(need - g->warmth) + clamp01(sun) * g->shade * 9.0f - g->weight * 20.0f;
        if (score > best) best = score, pick = i;
    }
    return pick;
}

float npc_feels_like(float temperature, float wind, float sun, const Outfit *o) { return apparel_feels_like(temperature, wind, 0.0f, sun, o, 0.0f, 0.0f); }
