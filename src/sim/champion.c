#include "sim/champion.h"

#include <stdio.h>
#include <string.h>

#include "sim/lang.h"
#include "sim/troop.h"

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

// Tablas de historia (UTF-8). Redaccion sin genero gramatical: valen para cualquiera.
static const char *ORIGINS[] = {
    N_("en una yurta de pastores de la estepa central"),
    N_("entre los herreros de un pueblo de montaña"),
    N_("en una familia campesina de la ciudad amurallada"),
    N_("en un clan de pescadores de la costa de fiordos"),
    N_("en una caravana que cruzaba el desierto"),
    N_("entre los cazadores del bosque de coníferas"),
    N_("en un clan aliado de la antigua confederación"),
    N_("en un campamento de esclavos del imperio"),
};
static const char *EXILES[] = {
    N_("Su clan fue arrasado en la masacre de la estepa"),
    N_("Huyó tras negarse a servir al culto"),
    N_("Fue desterrado por matar a un recaudador de tributos"),
    N_("Escapó de una columna de prisioneros destinados al sacrificio"),
    N_("La sequía y las malas cosechas se llevaron a su familia"),
    N_("Su propio clan le dio la espalda tras una derrota"),
    N_("Desertó del ejército imperial"),
    N_("Una deuda de sangre le obligó a huir"),
};
static const char *LIVELIHOODS[] = {
    N_("vive de cazar en soledad"),
    N_("vende su espada a quien pague"),
    N_("protege caravanas en las rutas del desierto"),
    N_("asalta los convoyes del imperio"),
    N_("cura a los heridos de los caminos a cambio de comida"),
    N_("vaga de campamento en campamento"),
    N_("doma caballos salvajes en las praderas"),
    N_("sobrevive con lo que encuentra entre las ruinas"),
};
// La aspiracion fija el rasgo de tropa.
static const struct {
    const char *text;
    unsigned trait;
} ASPIRATIONS[] = {
    { N_("vengar a los suyos"), TRAIT_BLOODTHIRSTY },
    { N_("ver arder el altar del culto"), TRAIT_DEVOUT },
    { N_("reunir un clan propio"), TRAIT_AMBITIOUS },
    { N_("encontrar un lugar donde envejecer en paz"), TRAIT_MERCIFUL },
    { N_("que su nombre viva en los cantos de la estepa"), TRAIT_AMBITIOUS },
    { N_("liberar a un ser querido prisionero"), TRAIT_LOYALIST },
    { N_("pagar una deuda de honor"), TRAIT_LOYALIST },
    { N_("recuperar las tierras de su familia"), TRAIT_AMBITIOUS },
};
static const char *NAMES[] = {
    "Arslan", "Toregene", "Chilaun", "Mukhali", "Sorghaghtani", "Bo'orchu", "Khutulun", "Tolui",
    "Alaqai", "Jelme",    "Qasar",   "Oghul",   "Bayan",        "Checheyigen", "Kokochu", "Ebegei",
    "Taichar", "Unegen",  "Mongke",  "Altani",  "Darmala",      "Sechen",   "Yesui",    "Temur",
};
static const struct {
    const char *name, *epithet;
} WEAPONS[] = {
    { N_("guja de hoja ancha"), N_("Hoja Ancha") },
    { N_("sable de acero damasquinado"), N_("Acero Rizado") },
    { N_("arco compuesto de cuerno"), N_("Arco de Cuerno") },
    { N_("maza de bronce con cabeza de tigre"), N_("Cabeza de Tigre") },
    { N_("lazo con plomadas"), N_("Lazo de Plomo") },
    { N_("hacha ceremonial"), N_("Hacha de los Muertos") },
    { N_("lanza de caballería de doble punta"), N_("Doble Punta") },
    { N_("ballesta de repetición"), N_("Lluvia de Virotes") },
};
static const char *GIFT_NAMES[GIFT_COUNT] = {
    N_("gran talla"), N_("rapidez"), N_("fuerza"), N_("aguante"), N_("arma especial"), N_("sanación"), N_("puntería"), N_("monta"),
};
static const char *GIFT_EPITHETS[GIFT_COUNT] = {
    N_("Sombra Alta"), N_("Pies de Viento"), N_("Mano de Hierro"), N_("Piel de Roca"),
    NULL, N_("Manos que Curan"), N_("Ojo de Halcón"), N_("Crin Negra"),
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
    return snprintf(buf, len, T("Nació %s. %s. Hoy %s, y sueña con %s."), T(ORIGINS[c->origin]), T(EXILES[c->exile]),
                    T(LIVELIHOODS[c->livelihood]), T(ASPIRATIONS[c->aspiration].text));
}

const char *gift_name(ChampionGift g) {
    for (int i = 0; i < GIFT_COUNT; i++)
        if (g == (ChampionGift)(1u << i)) return T(GIFT_NAMES[i]);
    return "?";
}

const char *champion_weapon_name(int weapon) {
    return weapon >= 0 && weapon < COUNT(WEAPONS) ? T(WEAPONS[weapon].name) : "";
}

int champion_gift_count(const Champion *c) {
    int n = 0;
    for (unsigned g = c->gifts; g; g &= g - 1) n++;
    return n;
}
