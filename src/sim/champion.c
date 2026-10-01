#include "sim/champion.h"

#include <stdio.h>
#include <string.h>

#include "sim/troop.h"

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

// Tablas de historia (UTF-8). Redaccion sin genero gramatical: valen para cualquiera.
static const char *ORIGINS[] = {
    "en una yurta de pastores de la estepa central",
    "entre los herreros de un pueblo de montaña",
    "en una familia campesina de la ciudad amurallada",
    "en un clan de pescadores de la costa de fiordos",
    "en una caravana que cruzaba el desierto",
    "entre los cazadores del bosque de coníferas",
    "en un clan aliado de la antigua confederación",
    "en un campamento de esclavos del imperio",
};
static const char *EXILES[] = {
    "Su clan fue arrasado en la masacre de la estepa",
    "Huyó tras negarse a servir al culto",
    "Fue desterrado por matar a un recaudador de tributos",
    "Escapó de una columna de prisioneros destinados al sacrificio",
    "La sequía y las malas cosechas se llevaron a su familia",
    "Su propio clan le dio la espalda tras una derrota",
    "Desertó del ejército imperial",
    "Una deuda de sangre le obligó a huir",
};
static const char *LIVELIHOODS[] = {
    "vive de cazar en soledad",
    "vende su espada a quien pague",
    "protege caravanas en las rutas del desierto",
    "asalta los convoyes del imperio",
    "cura a los heridos de los caminos a cambio de comida",
    "vaga de campamento en campamento",
    "doma caballos salvajes en las praderas",
    "sobrevive con lo que encuentra entre las ruinas",
};
// La aspiracion fija el rasgo de tropa.
static const struct {
    const char *text;
    unsigned trait;
} ASPIRATIONS[] = {
    { "vengar a los suyos", TRAIT_BLOODTHIRSTY },
    { "ver arder el altar del culto", TRAIT_DEVOUT },
    { "reunir un clan propio", TRAIT_AMBITIOUS },
    { "encontrar un lugar donde envejecer en paz", TRAIT_MERCIFUL },
    { "que su nombre viva en los cantos de la estepa", TRAIT_AMBITIOUS },
    { "liberar a un ser querido prisionero", TRAIT_LOYALIST },
    { "pagar una deuda de honor", TRAIT_LOYALIST },
    { "recuperar las tierras de su familia", TRAIT_AMBITIOUS },
};
static const char *NAMES[] = {
    "Arslan", "Toregene", "Chilaun", "Mukhali", "Sorghaghtani", "Bo'orchu", "Khutulun", "Tolui",
    "Alaqai", "Jelme",    "Qasar",   "Oghul",   "Bayan",        "Checheyigen", "Kokochu", "Ebegei",
    "Taichar", "Unegen",  "Mongke",  "Altani",  "Darmala",      "Sechen",   "Yesui",    "Temur",
};
static const struct {
    const char *name, *epithet;
} WEAPONS[] = {
    { "guja de hoja ancha", "Hoja Ancha" },
    { "sable de acero damasquinado", "Acero Rizado" },
    { "arco compuesto de cuerno", "Arco de Cuerno" },
    { "maza de bronce con cabeza de tigre", "Cabeza de Tigre" },
    { "lazo con plomadas", "Lazo de Plomo" },
    { "hacha ceremonial", "Hacha de los Muertos" },
    { "lanza de caballería de doble punta", "Doble Punta" },
    { "ballesta de repetición", "Lluvia de Virotes" },
};
static const char *GIFT_NAMES[GIFT_COUNT] = {
    "gran talla", "rapidez", "fuerza", "aguante", "arma especial", "sanación", "puntería", "monta",
};
static const char *GIFT_EPITHETS[GIFT_COUNT] = {
    "Sombra Alta", "Pies de Viento", "Mano de Hierro", "Piel de Roca",
    NULL, "Manos que Curan", "Ojo de Halcón", "Crin Negra",
};

bool champion_appears(Rng *rng, float chance) { return rng_float(rng) < chance; }

static float roll(Rng *rng, float lo, float hi) { return lo + (hi - lo) * rng_float(rng); }

void champion_generate(Champion *c, Rng *rng) {
    memset(c, 0, sizeof(*c));
    snprintf(c->name, sizeof(c->name), "%s", NAMES[rng_range(rng, COUNT(NAMES))]);
    c->origin = rng_range(rng, COUNT(ORIGINS));
    c->exile = rng_range(rng, COUNT(EXILES));
    c->livelihood = rng_range(rng, COUNT(LIVELIHOODS));
    c->aspiration = rng_range(rng, COUNT(ASPIRATIONS));
    c->traits = ASPIRATIONS[c->aspiration].trait;
    c->weapon = -1;
    c->stats = (CombatStats){ 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };

    // 1 a 3 dones distintos; el primero da el epiteto.
    int n = 1 + (rng_float(rng) < 0.5f) + (rng_float(rng) < 0.2f);
    int first = -1;
    while (champion_gift_count(c) < n) {
        int g = rng_range(rng, GIFT_COUNT);
        if (c->gifts & (1u << g)) continue;
        c->gifts |= 1u << g;
        if (first < 0) first = g;
    }

    CombatStats *s = &c->stats;
    if (c->gifts & GIFT_TALL) {
        s->size = roll(rng, 1.2f, 1.45f);
        s->strength *= 1.15f;
        s->speed *= 0.95f; // la talla pesa
    }
    if (c->gifts & GIFT_SWIFT) s->speed *= roll(rng, 1.2f, 1.4f);
    if (c->gifts & GIFT_STRONG) s->strength *= roll(rng, 1.3f, 1.6f);
    if (c->gifts & GIFT_ENDURING) s->endurance = roll(rng, 1.3f, 1.7f);
    if (c->gifts & GIFT_MARKSMAN) s->aim = roll(rng, 1.3f, 1.6f);
    if (c->gifts & GIFT_RIDER) s->riding = roll(rng, 1.3f, 1.6f);
    if (c->gifts & GIFT_HEALER) s->healing = roll(rng, 0.5f, 1.0f);
    if (c->gifts & GIFT_WEAPON) c->weapon = rng_range(rng, COUNT(WEAPONS));

    const char *ep = first == 4 ? WEAPONS[c->weapon].epithet : GIFT_EPITHETS[first];
    snprintf(c->epithet, sizeof(c->epithet), "%s", ep);
}

int champion_story(const Champion *c, char *buf, size_t len) {
    return snprintf(buf, len, "Nació %s. %s. Hoy %s, y sueña con %s.", ORIGINS[c->origin], EXILES[c->exile],
                    LIVELIHOODS[c->livelihood], ASPIRATIONS[c->aspiration].text);
}

const char *gift_name(ChampionGift g) {
    for (int i = 0; i < GIFT_COUNT; i++)
        if (g == (ChampionGift)(1u << i)) return GIFT_NAMES[i];
    return "?";
}

const char *champion_weapon_name(int weapon) {
    return weapon >= 0 && weapon < COUNT(WEAPONS) ? WEAPONS[weapon].name : "";
}

int champion_gift_count(const Champion *c) {
    int n = 0;
    for (unsigned g = c->gifts; g; g &= g - 1) n++;
    return n;
}
