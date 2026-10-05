#include "sim/animals.h"
#include "sim/lang.h"
#include "sim/water.h"

#include <math.h>
#include <string.h>

static bool water_near(const FaunaCtx *c, float x, float z);

// clang-format off
static const SpeciesDef SPECIES[SPECIES_COUNT] = {
    //                 nombre               modelo                                clase            vel   paso  talla  vida  daño alc.  cd    herida         sentido lazo  montable ride  social flier defiende caza carne piel leche grupo   habitat                         rareza
    [SPECIES_HORSE]    = { N_("Caballo estepario"), "animal.montura.caballo_estepario", CLASS_MOUNT,     7.0f, 1.3f, 2.2f, 120.0f,  8.0f, 1.6f, 1.4f, WOUND_BRUISE, 30.0f, 0.45f, true,  2.4f, true,  false, false, 0.0f,  6, 2, 0, 3, 6, HAB_STEPPE,                      1.0f, false, 0.0f, 0.0f },
    [SPECIES_MULE]     = { N_("Mula"),              "animal.montura.mula",              CLASS_MOUNT,     5.5f, 1.2f, 2.0f, 100.0f,  7.0f, 1.5f, 1.4f, WOUND_BRUISE, 25.0f, 0.60f, true,  1.8f, true,  false, false, 0.0f,  5, 1, 0, 2, 4, HAB_STEPPE | HAB_DESERT,         0.5f, false, 0.0f, 0.0f },
    [SPECIES_DONKEY]   = { N_("Burro"),             "animal.montura.burro",             CLASS_MOUNT,     5.0f, 1.1f, 1.6f,  80.0f,  6.0f, 1.4f, 1.4f, WOUND_BRUISE, 25.0f, 0.70f, true,  1.5f, true,  false, false, 0.0f,  4, 1, 0, 2, 4, HAB_DESERT | HAB_STEPPE,         0.6f, false, 0.0f, 0.0f },
    [SPECIES_OX]       = { N_("Buey"),              "animal.ganado.buey",               CLASS_MOUNT,     4.0f, 1.0f, 2.5f, 160.0f, 14.0f, 1.8f, 1.6f, WOUND_CUT,    25.0f, 0.50f, true,  1.3f, true,  false, true,  0.0f, 10, 3, 0, 2, 5, HAB_STEPPE | HAB_COLD | HAB_COAST,           0.6f, false, 0.0f, 0.0f },
    [SPECIES_CAMEL]    = { N_("Camello bactriano"), "animal.montura.camello_bactriano", CLASS_MOUNT,     4.5f, 1.0f, 3.0f, 140.0f,  8.0f, 1.6f, 1.5f, WOUND_BITE,   28.0f, 0.55f, true,  1.7f, true,  false, false, 0.0f,  8, 2, 0, 2, 4, HAB_DESERT,                      1.0f, false, 0.0f, 0.0f },
    [SPECIES_ELEPHANT] = { N_("Elefante"),          "animal.montura.elefante_guerra",   CLASS_MOUNT,     4.0f, 1.0f, 5.0f, 400.0f, 30.0f, 2.6f, 2.0f, WOUND_BRUISE, 30.0f, 0.35f, true,  1.6f, true,  false, true,  0.0f, 25, 4, 0, 2, 4, HAB_STEPPE,                      0.15f, false, 0.0f, 0.0f },
    [SPECIES_WOLF]     = { N_("Lobo"),              "animal.salvaje.lobo",              CLASS_TAMEABLE,  7.5f, 1.3f, 1.4f,  45.0f, 10.0f, 1.4f, 1.0f, WOUND_BITE,   35.0f, 0.0f,  false, 1.0f, true,  false, false, 2.3f,  2, 1, 0, 3, 6, HAB_STEPPE | HAB_COLD | HAB_FOREST | HAB_COAST,           0.8f, false, 0.0f, 0.0f },
    [SPECIES_DOG]      = { N_("Perro asilvestrado"),"animal.salvaje.perro_salvaje",     CLASS_TAMEABLE,  7.0f, 1.3f, 1.1f,  38.0f,  8.0f, 1.3f, 1.0f, WOUND_BITE,   30.0f, 0.0f,  false, 1.0f, true,  false, false, 1.7f,  1, 1, 0, 3, 5, HAB_STEPPE | HAB_DESERT,         0.5f, false, 0.0f, 0.0f },
    [SPECIES_TIGER]    = { N_("Tigre"),             "animal.salvaje.tigre",             CLASS_TAMEABLE,  8.0f, 1.2f, 2.6f, 140.0f, 24.0f, 1.8f, 1.3f, WOUND_CUT,    35.0f, 0.0f,  false, 1.0f, false, false, false, 3.0f,  6, 2, 0, 1, 1, HAB_STEPPE,                      0.15f, false, 0.0f, 0.0f },
    [SPECIES_PUMA]     = { N_("Puma"),              "animal.salvaje.puma",              CLASS_TAMEABLE,  8.5f, 1.2f, 1.9f,  70.0f, 15.0f, 1.6f, 1.1f, WOUND_CUT,    32.0f, 0.0f,  false, 1.0f, false, false, false, 2.2f,  3, 1, 0, 1, 1, HAB_STEPPE | HAB_COLD | HAB_FOREST,           0.3f, false, 0.0f, 0.0f },
    [SPECIES_FALCON]   = { N_("Halcón"),            "animal.ave.halcon",                CLASS_TAMEABLE, 16.0f, 3.0f, 0.5f,  15.0f,  4.0f, 1.0f, 1.2f, WOUND_CUT,    60.0f, 0.0f,  false, 1.0f, false, true,  false, 0.6f,  0, 0, 0, 1, 1, HAB_STEPPE | HAB_DESERT | HAB_COAST,         0.4f, false, 0.0f, 0.0f },
    [SPECIES_RAVEN]    = { N_("Cuervo"),            "animal.ave.cuervo",                CLASS_TAMEABLE, 10.0f, 2.5f, 0.5f,  10.0f,  2.0f, 1.0f, 1.2f, WOUND_CUT,    40.0f, 0.0f,  false, 1.0f, true,  true,  false, 0.0f,  0, 0, 0, 2, 4, HAB_COLD | HAB_STEPPE | HAB_FOREST | HAB_COAST,           0.5f, false, 0.0f, 0.0f },
    [SPECIES_BEAR]     = { N_("Oso pardo"),         "animal.salvaje.oso",               CLASS_HOSTILE,   6.5f, 1.0f, 2.2f, 180.0f, 26.0f, 1.8f, 1.5f, WOUND_CUT,    25.0f, 0.0f,  false, 1.0f, false, false, true,  2.2f,  8, 3, 0, 1, 1, HAB_STEPPE | HAB_COLD | HAB_FOREST | HAB_COAST,           0.3f, false, 0.0f, 0.0f },
    [SPECIES_HYENA]    = { N_("Hiena"),             "animal.salvaje.hiena",             CLASS_HOSTILE,   7.0f, 1.3f, 1.3f,  50.0f, 12.0f, 1.4f, 1.1f, WOUND_BITE,   35.0f, 0.0f,  false, 1.0f, true,  false, false, 2.2f,  1, 1, 0, 3, 6, HAB_DESERT,                      0.6f, false, 0.0f, 0.0f },
    [SPECIES_COYOTE]   = { N_("Coyote"),            "animal.salvaje.coyote",            CLASS_HOSTILE,   7.5f, 1.4f, 1.1f,  30.0f,  7.0f, 1.3f, 1.0f, WOUND_BITE,   30.0f, 0.0f,  false, 1.0f, true,  false, false, 1.2f,  1, 1, 0, 2, 4, HAB_STEPPE | HAB_DESERT,         0.6f, false, 0.0f, 0.0f },
    [SPECIES_BOAR]     = { N_("Jabalí"),            "animal.salvaje.jabali",            CLASS_HOSTILE,   6.5f, 1.2f, 1.4f,  70.0f, 14.0f, 1.3f, 1.3f, WOUND_CUT,    18.0f, 0.0f,  false, 1.0f, true,  false, true,  0.0f,  4, 1, 0, 2, 5, HAB_STEPPE | HAB_FOREST,                      0.6f, false, 0.0f, 0.0f },
    [SPECIES_ANTELOPE] = { N_("Antílope saiga"),    "animal.salvaje.antilope",          CLASS_PREY,      9.0f, 1.2f, 1.6f,  50.0f,  0.0f, 1.2f, 1.5f, WOUND_BRUISE, 40.0f, 0.0f,  false, 1.0f, true,  false, false, 0.0f,  3, 1, 0, 4, 8, HAB_STEPPE,                      1.0f, false, 0.0f, 0.0f },
    [SPECIES_REINDEER] = { N_("Reno salvaje"),      "animal.salvaje.reno",              CLASS_PREY,      7.5f, 1.2f, 2.0f,  70.0f,  0.0f, 1.4f, 1.5f, WOUND_BRUISE, 35.0f, 0.0f,  false, 1.0f, true,  false, false, 0.0f,  5, 2, 0, 4, 8, HAB_COLD | HAB_COAST,                        1.0f, false, 0.0f, 0.0f },
    [SPECIES_GAZELLE]  = { N_("Gacela"),            "animal.salvaje.gacela",            CLASS_PREY,      9.5f, 1.3f, 1.2f,  35.0f,  0.0f, 1.2f, 1.5f, WOUND_BRUISE, 45.0f, 0.0f,  false, 1.0f, true,  false, false, 0.0f,  2, 1, 0, 4, 8, HAB_DESERT,                      1.0f, false, 0.0f, 0.0f },
    [SPECIES_DEER]     = { N_("Ciervo"),            "animal.salvaje.ciervo",            CLASS_PREY,      8.0f, 1.2f, 1.8f,  60.0f,  0.0f, 1.4f, 1.5f, WOUND_BRUISE, 40.0f, 0.0f,  false, 1.0f, true,  false, false, 0.0f,  4, 2, 0, 3, 5, HAB_STEPPE | HAB_FOREST,                      0.8f, false, 0.0f, 0.0f },
    [SPECIES_HARE]     = { N_("Liebre"),            "animal.salvaje.liebre",            CLASS_PREY,      9.0f, 1.5f, 0.5f,   8.0f,  0.0f, 0.6f, 1.0f, WOUND_BRUISE, 20.0f, 0.0f,  false, 1.0f, false, false, false, 0.0f,  1, 1, 0, 1, 2, HAB_STEPPE | HAB_DESERT | HAB_COLD | HAB_FOREST, 1.0f, false, 0.0f, 0.0f },
    [SPECIES_IBEX]     = { N_("Íbice"),             "animal.salvaje.ibice",             CLASS_PREY,      6.5f, 1.2f, 1.4f,  45.0f,  0.0f, 1.2f, 1.5f, WOUND_BRUISE, 35.0f, 0.0f,  false, 1.0f, true,  false, false, 0.0f,  3, 1, 0, 2, 4, HAB_COLD | HAB_COAST,                        0.8f, false, 0.0f, 0.0f },
    [SPECIES_GOAT]     = { N_("Cabra"),             "animal.ganado.cabra",              CLASS_LIVESTOCK, 5.0f, 1.0f, 1.1f,  30.0f,  3.0f, 1.0f, 1.4f, WOUND_BRUISE, 25.0f, 0.0f,  false, 1.0f, true,  false, false, 0.0f,  2, 1, 1, 3, 6, HAB_STEPPE,                      0.0f, false, 0.0f, 0.0f },
    [SPECIES_CALF]     = { N_("Becerro"),           "animal.ganado.becerro",            CLASS_LIVESTOCK, 4.5f, 1.0f, 1.4f,  45.0f,  0.0f, 1.0f, 1.4f, WOUND_BRUISE, 25.0f, 0.0f,  false, 1.0f, true,  false, false, 0.0f,  4, 1, 0, 2, 4, HAB_STEPPE,                      0.0f, false, 0.0f, 0.0f },
    [SPECIES_CROCODILE]= { N_("Cocodrilo"),         "animal.acuatico.cocodrilo",        CLASS_HOSTILE,   6.0f, 0.8f, 3.5f, 160.0f, 28.0f, 1.8f, 1.8f, WOUND_BITE,   12.0f, 0.0f,  false, 1.0f, false, false, true,  2.4f, 10, 3, 0, 1, 1, HAB_WATER,                       0.5f, true,  0.0f,  9.0f },
    [SPECIES_TURTLE]   = { N_("Tortuga marina"),    "animal.acuatico.tortuga_marina",        CLASS_PREY,      1.6f, 0.4f, 1.0f,  40.0f,  0.0f, 0.8f, 1.5f, WOUND_BITE,   12.0f, 0.0f,  false, 1.0f, false, false, false, 0.0f,  3, 1, 0, 1, 2, HAB_WATER,                       0.8f, true,  0.0f,  0.0f },
    [SPECIES_SNAKE]    = { N_("Víbora"),            "animal.salvaje.vibora",            CLASS_HOSTILE,   3.0f, 0.5f, 1.0f,   6.0f,  3.0f, 0.9f, 1.5f, WOUND_BITE,    3.5f, 0.0f,  false, 1.0f, false, false, true,  0.0f,  0, 0, 0, 1, 1, HAB_STEPPE | HAB_DESERT,         0.7f, false, 22.0f, 4.0f },
    [SPECIES_SCORPION] = { N_("Escorpión"),         "animal.salvaje.escorpion",         CLASS_HOSTILE,   1.5f, 0.3f, 0.2f,   3.0f,  1.0f, 0.5f, 1.3f, WOUND_CUT,     2.5f, 0.0f,  false, 1.0f, false, false, true,  0.0f,  0, 0, 0, 1, 1, HAB_DESERT,                      0.8f, false, 15.0f, 3.0f },
    [SPECIES_SPIDER]   = { N_("Araña"),             "animal.salvaje.arana",             CLASS_HOSTILE,   1.2f, 0.3f, 0.15f,  2.0f,  0.5f, 0.5f, 1.5f, WOUND_BITE,    2.0f, 0.0f,  false, 1.0f, false, false, true,  0.0f,  0, 0, 0, 1, 1, HAB_STEPPE | HAB_DESERT | HAB_COLD | HAB_FOREST, 0.5f, false, 8.0f,  2.5f },
};
// clang-format on

#define WANDER_RADIUS 18.0f
#define FOLLOW_GAP 4.0f
#define FOLLOW_RANGE 30.0f   // m: los domados siguen al jugador si esta a menos
#define HERD_RANGE 10.0f     // m: el ganado sigue al jugador que camina cerca
#define HERD_LOSE 18.0f      // m: si el pastor se aleja, el rebaño se queda
#define GROUP_SPREAD 9.0f    // m: un animal social vuelve con su grupo si se aleja
#define FEAR_SECONDS 60.0f   // s: una presa herida por personas les teme
#define ALARM_RADIUS 25.0f   // m: hasta donde se contagia la alarma en un grupo
#define CRITICAL 0.2f        // vida: depredadores y hostiles huyen por debajo
#define WEAK 0.4f            // vida: un depredador domable se puede atar
#define BOUND_SECONDS 60.0f  // s: atado sin comer, se suelta
#define EAT_SECONDS 25.0f    // s: un cazador come su presa
#define RIVAL_SECONDS 6.0f   // s: dura una pelea de rivales
#define RIVAL_RANGE 20.0f    // m: territorio de un solitario
#define STALK_POUNCE 7.0f    // m: el acecho termina en un salto
#define HUNGER_RATE (1.0f / 420.0f)

const SpeciesDef *species_def(Species s) { return s >= 0 && s < SPECIES_COUNT ? &SPECIES[s] : NULL; }

bool species_is_prey(Species s) {
    AnimalClass c = SPECIES[s].cls;
    return c == CLASS_MOUNT || c == CLASS_PREY || c == CLASS_LIVESTOCK;
}

int species_find(const char *name) {
    if (!name || !name[0]) return -1;
    for (int s = 0; s < SPECIES_COUNT; s++) {
        const char *dot = strrchr(SPECIES[s].model, '.');
        if ((dot && !strcmp(dot + 1, name)) || !strcmp(SPECIES[s].model, name)) return s;
    }
    return -1;
}

void animal_init(Animal *a, Species s, float x, float z) {
    memset(a, 0, sizeof(*a));
    const SpeciesDef *d = &SPECIES[s];
    a->used = true;
    a->species = s;
    a->state = d->cls == CLASS_LIVESTOCK ? ANIMAL_TAMED : ANIMAL_WILD;
    a->mode = MODE_GRAZE;
    a->x = a->home_x = a->tx = x;
    a->z = a->home_z = a->tz = z;
    a->group = -1;
    a->tkind = TGT_NONE;
    a->target = -1;
    a->alt = d->flier ? 8.0f : 0.0f;
    a->hunger = 0.3f;
    a->stamina = 1.0f;
    health_init_beast(&a->h, d->hp);
}

// ------------------------------------------------------------------ utilidades
static float dist_xz(float ax, float az, float bx, float bz) {
    float dx = ax - bx, dz = az - bz;
    return sqrtf(dx * dx + dz * dz);
}

static float adist(const Animal *a, const Animal *b) { return dist_xz(a->x, a->z, b->x, b->z); }

static bool alive(const Animal *a) { return a->used && a->state != ANIMAL_DEAD; }

static void move_towards(Animal *a, float tx, float tz, float speed, float dt) {
    float dx = tx - a->x, dz = tz - a->z, d = sqrtf(dx * dx + dz * dz);
    if (d < 0.05f || speed <= 0.0f) return;
    float step = fminf(speed * dt, d);
    a->x += dx / d * step;
    a->z += dz / d * step;
    a->yaw = atan2f(dx, dz);
}

static void move_away(Animal *a, float fx, float fz, float speed, float dt) {
    float dx = a->x - fx, dz = a->z - fz, d = sqrtf(dx * dx + dz * dz);
    if (d < 0.01f) dx = 1.0f, dz = 0.0f, d = 1.0f;
    move_towards(a, a->x + dx / d * 5.0f, a->z + dz / d * 5.0f, speed, dt);
}

static void push(FaunaEvents *ev, FaunaEventKind k, int animal, int other, float dmg, WoundKind w) {
    if (!ev || ev->n >= FAUNA_EVENTS_MAX) return;
    ev->ev[ev->n++] = (FaunaEvent){ k, animal, other, dmg, w, 0.0f };
}

static void clear_target(Animal *a) {
    a->tkind = TGT_NONE;
    a->target = -1;
}

static void die(Animal *a, bool by_human) {
    a->state = ANIMAL_DEAD;
    a->mode = MODE_GRAZE;
    a->corpse = 0.0f;
    a->speed = 0.0f;
    a->fleeing = false;
    a->killed_by_human = by_human;
    clear_target(a);
}

// A la carrera: cansado, corre menos (hasta la mitad).
static float water_at(const FaunaCtx *c, float x, float z) {
    return c && c->water_depth ? c->water_depth(c->water_ud, x, z) : 0.0f;
}

// En el agua: a salvo de los que no nadan.
static bool safe_in_water(const Animal *prey, const Animal *hunter, const FaunaCtx *c) {
    return SPECIES[prey->species].aquatic && !SPECIES[hunter->species].aquatic && water_at(c, prey->x, prey->z) > 0.5f;
}

static float run_speed(const Animal *a) {
    return SPECIES[a->species].speed * health_speed_scale(&a->h) * (0.5f + 0.5f * a->stamina);
}

// Cuanto cansa correr: las presas se agotan antes que una manada; un solitario solo aguanta un salto corto.
static float tire_rate(const SpeciesDef *d) {
    if (d->hunt_max <= 0.0f && d->cls != CLASS_HOSTILE) return 1.0f / 14.0f;
    return d->social ? 1.0f / 40.0f : 1.0f / 9.0f;
}

// Los de tierra (ni aves ni nadadores) no entran en agua honda.
static bool walks_on_land(const SpeciesDef *d) { return !d->aquatic && !d->flier; }

// Tras moverse: si el paso lo mete en agua honda (y no venia de ella), prueba a rodearla
// girando a uno y otro lado; si no puede, se queda en la orilla y elige otro destino.
static void keep_dry(Animal *a, const FaunaCtx *c, float x0, float z0) {
    if (!c || !c->water_depth || !walks_on_land(&SPECIES[a->species])) return;
    float depth = water_at(c, a->x, a->z);
    if (depth <= ANIMAL_WADE_MAX) return;
    if (depth <= water_at(c, x0, z0)) return; // ya estaba en el agua: que salga
    float dx = a->x - x0, dz = a->z - z0;
    static const float TURN[8] = { 0.5f, -0.5f, 1.0f, -1.0f, 1.5f, -1.5f, 2.0f, -2.0f };
    for (int k = 0; k < 8; k++) {
        float cs = cosf(TURN[k]), sn = sinf(TURN[k]);
        float nx = x0 + dx * cs - dz * sn, nz = z0 + dx * sn + dz * cs;
        if (water_at(c, nx, nz) <= ANIMAL_WADE_MAX) {
            a->x = nx, a->z = nz;
            a->yaw = atan2f(nx - x0, nz - z0);
            return;
        }
    }
    a->x = x0, a->z = z0;
    if (a->mode == MODE_GRAZE) a->timer = 0.0f; // otro destino
    if (water_at(c, a->tx, a->tz) > ANIMAL_WADE_MAX) a->tx = x0, a->tz = z0;
}

// Con sed, los de tierra buscan la orilla mas cercana, van hasta ella y beben desde
// tierra firme (mirando al agua). Devuelve true mientras se ocupa de eso.
static bool seek_drink(Animal *a, const FaunaCtx *c, float dt) {
    const SpeciesDef *d = &SPECIES[a->species];
    if (!c || !c->water_depth || !walks_on_land(d) || a->thirst < 0.8f) {
        if (a->mode == MODE_DRINK) a->mode = MODE_GRAZE;
        return false;
    }
    if (a->mode != MODE_DRINK) {
        bool found = false;
        for (float r = 6.0f; r <= 120.0f && !found; r += 6.0f)
            for (int k = 0; k < 16 && !found; k++) {
                float ang = (float)k / 16.0f * 6.2831853f + r * 0.13f, ux = cosf(ang), uz = sinf(ang);
                if (water_at(c, a->x + ux * r, a->z + uz * r) <= 0.05f) continue;
                for (float back = 2.0f; back < r; back += 1.5f) { // retrocede hasta tierra firme
                    float tx = a->x + ux * (r - back), tz = a->z + uz * (r - back);
                    if (water_at(c, tx, tz) <= 0.0f && water_near(c, tx, tz)) {
                        a->tx = tx, a->tz = tz, found = true;
                        break;
                    }
                }
            }
        if (!found) { // no hay agua cerca: aguanta (bebera de un charco)
            a->thirst = 0.3f;
            return false;
        }
        a->mode = MODE_DRINK;
    }
    if (dist_xz(a->x, a->z, a->tx, a->tz) > 1.2f) move_towards(a, a->tx, a->tz, d->walk, dt);
    if (water_near(c, a->x, a->z)) { // en la orilla: bebe
        a->thirst = fmaxf(0.0f, a->thirst - dt * 0.25f);
        if (a->thirst <= 0.05f) a->mode = MODE_GRAZE, a->timer = 0.0f;
    }
    return true;
}

// Elige un destino al azar en su territorio; los que pastan a veces se quedan quietos.
static void wander(Animal *a, Rng *rng, float radius, float dt) {
    const SpeciesDef *d = &SPECIES[a->species];
    a->timer -= dt;
    if (a->timer <= 0.0f) {
        if (!d->flier && rng_float(rng) < 0.4f) {
            a->tx = a->x; // pastar en el sitio
            a->tz = a->z;
        } else {
            float ang = rng_float(rng) * 6.2831853f, r = rng_float(rng) * radius;
            a->tx = a->home_x + cosf(ang) * r;
            a->tz = a->home_z + sinf(ang) * r;
        }
        a->timer = 4.0f + rng_float(rng) * 6.0f;
    }
    move_towards(a, a->tx, a->tz, d->walk, dt);
}

static int nearest_human(const FaunaCtx *c, float x, float z, float range, bool sneak_aware, float *out_d) {
    int best = -1;
    float bd = range;
    for (int h = 0; h < c->human_count; h++) {
        const FaunaHuman *hu = &c->humans[h];
        if (hu->down) continue;
        float d = dist_xz(x, z, hu->x, hu->z);
        float lim = sneak_aware && hu->sneaking ? range * 0.45f : range;
        if (d < lim && d < bd) bd = d, best = h;
    }
    if (out_d) *out_d = bd;
    return best;
}

// ------------------------------------------------------------------ daño
int animal_hurt(Animal *a, int n, int idx, Rng *rng, float damage, WoundKind kind, int part, bool by_human,
                int attacker_human) {
    if (idx < 0 || idx >= n || !alive(&a[idx])) return -1;
    Animal *t = &a[idx];
    const SpeciesDef *d = &SPECIES[t->species];
    int w = health_hit(&t->h, rng, damage, kind, part);
    t->hit_anim = 0.3f;
    if (t->h.down || t->h.dead) {
        if (t->ridden) t->ridden = false;
        die(t, by_human);
        return w;
    }
    t->weakened = t->h.hp < WEAK * t->h.hp_max;
    if (!by_human || t->state != ANIMAL_WILD) return w;
    if (species_is_prey(t->species) && !d->defends) {
        // Presa herida por una persona: le teme, y todo su grupo con ella.
        t->fear_humans = FEAR_SECONDS;
        t->alert = 8.0f;
        for (int i = 0; i < n; i++)
            if (i != idx && alive(&a[i]) && a[i].state == ANIMAL_WILD && t->group >= 0 && a[i].group == t->group &&
                adist(&a[i], t) < ALARM_RADIUS)
                a[i].fear_humans = FEAR_SECONDS, a[i].alert = 8.0f;
    } else if (attacker_human >= 0) {
        // Cazadores, hostiles y los que se defienden: se vuelven contra quien los hirio.
        t->tkind = TGT_HUMAN;
        t->target = attacker_human;
        t->mode = MODE_CHASE;
    }
    return w;
}

// ------------------------------------------------------------------ presas
// Busca la amenaza que nota una presa (un cazador, o personas si les teme).
static bool prey_threat(const Animal *a, int i, const Animal *all, int n, const FaunaCtx *c, float *fx, float *fz) {
    const SpeciesDef *d = &SPECIES[a->species];
    float best = 1e9f;
    bool found = false;
    float notice_scale = c->night ? 0.75f : 1.0f;
    for (int k = 0; k < n; k++) {
        const Animal *p = &all[k];
        if (k == i || !alive(p) || p->state != ANIMAL_WILD) continue;
        const SpeciesDef *pd = &SPECIES[p->species];
        if (pd->hunt_max < d->size || pd->hunt_max <= 0.0f || safe_in_water(a, p, c)) continue;
        // Un cazador que acecha cuesta mas notarlo; uno que corre hacia ti, menos.
        float notice = d->sense * notice_scale * (p->mode == MODE_STALK ? 0.35f : p->mode == MODE_CHASE ? 1.3f : 0.7f);
        float dd = adist(a, p);
        if (dd < notice && dd < best) best = dd, *fx = p->x, *fz = p->z, found = true;
    }
    if (a->fear_humans > 0.0f && a->state == ANIMAL_WILD) {
        float dh;
        int h = nearest_human(c, a->x, a->z, 40.0f, false, &dh);
        if (h >= 0 && dh < best) *fx = c->humans[h].x, *fz = c->humans[h].z, found = true;
    }
    return found;
}

static void alarm_group(Animal *all, int n, int i, float fx, float fz, FaunaEvents *ev) {
    Animal *a = &all[i];
    bool spread = false;
    for (int k = 0; k < n; k++) {
        Animal *o = &all[k];
        if (k == i || !alive(o) || a->group < 0 || o->group != a->group || o->alert > 0.0f) continue;
        if (adist(a, o) > ALARM_RADIUS) continue;
        o->alert = 6.0f;
        o->flee_x = fx;
        o->flee_z = fz;
        spread = true;
    }
    if (spread) push(ev, FEV_PREY_ALARM, i, -1, 0.0f, WOUND_BRUISE);
}

// Un anfibio huye hacia el agua mas honda (lejos de la amenaza si puede).
static void flee_to_water(Animal *a, const FaunaCtx *c, float dt) {
    float best = -1e9f, bx = a->x, bz = a->z;
    float ax = a->x - a->flee_x, az = a->z - a->flee_z, al = sqrtf(ax * ax + az * az);
    for (int k = 0; k < 8; k++) {
        float ang = (float)k * 0.785398f, dx = cosf(ang), dz = sinf(ang);
        float score = water_at(c, a->x + dx * 6.0f, a->z + dz * 6.0f) * 3.0f;
        if (al > 0.01f) score += (dx * ax + dz * az) / al; // mejor lejos de la amenaza
        if (score > best) best = score, bx = a->x + dx * 6.0f, bz = a->z + dz * 6.0f;
    }
    move_towards(a, bx, bz, run_speed(a), dt);
}

// Lleva un animal social de vuelta a su grupo (el centro de los suyos cercanos).
static bool keep_with_group(Animal *a, int i, const Animal *all, int n, float dt) {
    if (a->group < 0) return false;
    float cx = 0.0f, cz = 0.0f;
    int cnt = 0;
    for (int k = 0; k < n; k++) {
        const Animal *o = &all[k];
        if (k == i || !alive(o) || o->group != a->group || adist(a, o) > 60.0f) continue;
        cx += o->x, cz += o->z, cnt++;
    }
    if (!cnt) return false;
    cx /= (float)cnt, cz /= (float)cnt;
    if (dist_xz(a->x, a->z, cx, cz) < GROUP_SPREAD) return false;
    move_towards(a, cx, cz, SPECIES[a->species].walk * 1.8f, dt);
    return true;
}

// ------------------------------------------------------------------ cazadores
static bool target_valid(const Animal *a, const Animal *all, int n, const FaunaCtx *c, float range) {
    if (a->tkind == TGT_ANIMAL) {
        if (a->target < 0 || a->target >= n || !alive(&all[a->target]) || all[a->target].ridden) return false;
        return adist(a, &all[a->target]) < range * 1.6f;
    }
    if (a->tkind == TGT_HUMAN) {
        if (a->target < 0 || a->target >= c->human_count || c->humans[a->target].down) return false;
        return dist_xz(a->x, a->z, c->humans[a->target].x, c->humans[a->target].z) < range * 1.6f;
    }
    return false;
}

static void target_pos(const Animal *a, const Animal *all, const FaunaCtx *c, float *x, float *z) {
    if (a->tkind == TGT_ANIMAL) *x = all[a->target].x, *z = all[a->target].z;
    else *x = c->humans[a->target].x, *z = c->humans[a->target].z;
}

static float human_aggro(const SpeciesDef *d) { return d->flier ? 10.0f : d->sense; }

// Elige objetivo: personas cerca (si es hostil o salvaje) o una presa si tiene hambre.
static void pick_target(Animal *a, int i, Animal *all, int n, const FaunaCtx *c) {
    const SpeciesDef *d = &SPECIES[a->species];
    float best = 1e9f;
    if (a->state == ANIMAL_WILD) {
        float dh;
        int h = nearest_human(c, a->x, a->z, human_aggro(d), true, &dh);
        if (h >= 0 && (d->leash <= 0.0f || dist_xz(c->humans[h].x, c->humans[h].z, a->home_x, a->home_z) <= d->leash))
            best = dh, a->tkind = TGT_HUMAN, a->target = h;
    }
    if (d->hunt_max > 0.0f && (a->hunger > 0.5f || a->state != ANIMAL_WILD)) {
        for (int k = 0; k < n; k++) {
            const Animal *p = &all[k];
            if (k == i || !alive(p) || p->ridden || !species_is_prey(p->species)) continue;
            if (SPECIES[p->species].size > d->hunt_max || safe_in_water(p, a, c)) continue;
            if (d->leash > 0.0f && dist_xz(p->x, p->z, a->home_x, a->home_z) > d->leash) continue;
            if (a->state != ANIMAL_WILD && (p->state != ANIMAL_WILD || p->species != SPECIES_HARE)) continue;
            float dd = adist(a, p);
            if (dd < d->sense && dd < best) best = dd, a->tkind = TGT_ANIMAL, a->target = k;
        }
    }
    // Manada: si un compañero ya tiene presa, la comparten.
    if (a->tkind == TGT_NONE && d->social && a->group >= 0) {
        for (int k = 0; k < n; k++) {
            const Animal *o = &all[k];
            if (k == i || !alive(o) || o->group != a->group || o->tkind == TGT_NONE || adist(a, o) > 30.0f) continue;
            if (o->tkind == TGT_ANIMAL && (o->target < 0 || !alive(&all[o->target]))) continue;
            a->tkind = o->tkind;
            a->target = o->target;
            break;
        }
    }
}

static void strike(Animal *a, int i, Animal *all, int n, Rng *rng, FaunaEvents *ev, float scale, WoundKind w) {
    const SpeciesDef *d = &SPECIES[a->species];
    a->cooldown = d->cooldown;
    a->attack_anim = 0.4f;
    float dmg = fmaxf(1.0f, d->damage * scale) * (0.8f + 0.4f * rng_float(rng)) * health_attack_scale(&a->h);
    float venom = scale >= 1.0f ? d->venom : 0.0f;
    if (a->tkind == TGT_HUMAN) {
        push(ev, FEV_BITE_HUMAN, i, a->target, dmg, w);
        if (ev && ev->n > 0) ev->ev[ev->n - 1].venom = venom;
        return;
    }
    int t = a->target;
    health_poison(&all[t].h, venom);
    animal_hurt(all, n, t, rng, dmg, w, PART_RANDOM, false, -1);
    all[t].alert = fmaxf(all[t].alert, 4.0f);
    all[t].flee_x = a->x, all[t].flee_z = a->z;
    if (all[t].state == ANIMAL_DEAD) {
        push(ev, FEV_KILL, i, t, 0.0f, w);
        a->mode = MODE_EAT;
        a->timer = EAT_SECONDS;
        a->hunger = 0.0f;
        // La manada come con el.
        for (int k = 0; k < n; k++)
            if (k != i && alive(&all[k]) && a->group >= 0 && all[k].group == a->group && all[k].tkind == TGT_ANIMAL &&
                all[k].target == t)
                all[k].mode = MODE_EAT, all[k].timer = EAT_SECONDS, all[k].hunger = 0.0f;
    }
}

static void hunt(Animal *a, int i, Animal *all, int n, const FaunaCtx *c, Rng *rng, float dt, FaunaEvents *ev) {
    const SpeciesDef *d = &SPECIES[a->species];
    float tx, tz;
    target_pos(a, all, c, &tx, &tz);
    float dist = dist_xz(a->x, a->z, tx, tz);
    float reach = d->reach + (a->tkind == TGT_ANIMAL ? SPECIES[all[a->target].species].size * 0.3f : 0.3f);
    bool unaware = a->tkind == TGT_ANIMAL && all[a->target].alert <= 0.0f;
    if (!d->social && unaware && dist > STALK_POUNCE) {
        // Acecho: despacio, sin que lo noten, hasta saltar de cerca.
        a->mode = MODE_STALK;
        move_towards(a, tx, tz, run_speed(a) * 0.28f, dt);
        return;
    }
    a->mode = MODE_CHASE;
    if (dist > reach * 0.9f) {
        float gx = tx, gz = tz;
        if (d->social && dist > 4.0f) { // la manada rodea: cada uno por su lado
            float ang = (float)(i % 5) * 1.25f;
            gx += cosf(ang) * 2.5f, gz += sinf(ang) * 2.5f;
        }
        move_towards(a, gx, gz, run_speed(a), dt);
        if (dist > 0.01f) a->yaw = atan2f(tx - a->x, tz - a->z);
    }
    if (dist <= reach && a->cooldown <= 0.0f && (!d->flier || a->alt < 2.5f)) strike(a, i, all, n, rng, ev, 1.0f, d->wound);
}

// ------------------------------------------------------------------ rivales
static void start_rivalry(Animal *a, int i, Animal *all, int n) {
    const SpeciesDef *d = &SPECIES[a->species];
    if (d->social || d->flier || a->state != ANIMAL_WILD || a->rival_timer > -20.0f) return; // descansa entre peleas
    for (int k = 0; k < n; k++) {
        Animal *o = &all[k];
        if (k == i || !alive(o) || o->species != a->species || o->state != ANIMAL_WILD || o->mode != MODE_GRAZE) continue;
        if (o->rival_timer > -20.0f || adist(a, o) > RIVAL_RANGE) continue;
        a->mode = o->mode = MODE_RIVAL;
        a->tkind = o->tkind = TGT_ANIMAL;
        a->target = k;
        o->target = i;
        a->rival_timer = o->rival_timer = RIVAL_SECONDS;
        return;
    }
}

static void rivalry(Animal *a, int i, Animal *all, int n, Rng *rng, float dt, FaunaEvents *ev) {
    Animal *o = a->target >= 0 && a->target < n ? &all[a->target] : NULL;
    if (!o || !alive(o) || o->mode != MODE_RIVAL) {
        a->mode = MODE_GRAZE;
        clear_target(a);
        return;
    }
    if (a->rival_timer <= 0.0f) {
        if (i > a->target) return; // decide el de menor indice
        // Pierde el que quedo peor: se va lejos del ganador.
        Animal *loser = a->h.hp / a->h.hp_max < o->h.hp / o->h.hp_max ? a : o, *winner = loser == a ? o : a;
        float dx = loser->x - winner->x, dz = loser->z - winner->z, l = sqrtf(dx * dx + dz * dz);
        if (l < 0.01f) dx = 1.0f, dz = 0.0f, l = 1.0f;
        loser->home_x = winner->x + dx / l * 50.0f;
        loser->home_z = winner->z + dz / l * 50.0f;
        loser->alert = 10.0f;
        loser->flee_x = winner->x, loser->flee_z = winner->z;
        a->mode = o->mode = MODE_GRAZE;
        clear_target(a), clear_target(o);
        push(ev, FEV_RIVAL_WON, (int)(winner - all), (int)(loser - all), 0.0f, WOUND_BRUISE);
        return;
    }
    const SpeciesDef *d = &SPECIES[a->species];
    float dist = adist(a, o);
    if (dist > d->reach) move_towards(a, o->x, o->z, run_speed(a) * 0.6f, dt);
    else a->yaw = atan2f(o->x - a->x, o->z - a->z);
    // Golpes de advertencia: hieren poco.
    if (dist <= d->reach + 0.2f && a->cooldown <= 0.0f) strike(a, i, all, n, rng, ev, 0.25f, WOUND_BRUISE);
}

// ------------------------------------------------------------------ actualizacion
static void update_wild(Animal *a, int i, Animal *all, int n, const FaunaCtx *c, Rng *rng, float dt, FaunaEvents *ev) {
    const SpeciesDef *d = &SPECIES[a->species];
    bool critical = a->h.hp < CRITICAL * a->h.hp_max;
    a->fleeing = false;

    // Presas (y monturas salvajes): huyen de lo que notan, salvo las que se defienden.
    bool defending = d->defends && a->tkind == TGT_HUMAN && target_valid(a, all, n, c, d->sense);
    if (species_is_prey(a->species) && !defending) {
        float fx, fz;
        if (prey_threat(a, i, all, n, c, &fx, &fz)) {
            if (a->alert <= 0.0f) alarm_group(all, n, i, fx, fz, ev);
            a->alert = fmaxf(a->alert, 5.0f);
            a->flee_x = fx, a->flee_z = fz;
        }
        if (a->alert > 0.0f) {
            a->mode = MODE_FLEE;
            a->fleeing = true;
            if (d->aquatic && c->water_depth) flee_to_water(a, c, dt);
            else move_away(a, a->flee_x, a->flee_z, run_speed(a), dt);
            return;
        }
        if (seek_drink(a, c, dt)) return;
        a->mode = MODE_GRAZE;
        clear_target(a);
        if (!keep_with_group(a, i, all, n, dt)) wander(a, rng, WANDER_RADIUS, dt);
        start_rivalry(a, i, all, n);
        return;
    }

    // Cazadores, hostiles y defensores.
    if (critical && d->cls != CLASS_MOUNT && d->cls != CLASS_LIVESTOCK) {
        // Vida critica: ahora si, huye.
        if (a->tkind != TGT_NONE && target_valid(a, all, n, c, 1e6f)) target_pos(a, all, c, &a->flee_x, &a->flee_z);
        a->mode = MODE_FLEE;
        a->fleeing = true;
        move_away(a, a->flee_x, a->flee_z, run_speed(a), dt);
        return;
    }
    if (a->alert > 0.0f && a->mode != MODE_CHASE) { // perdio una pelea de rivales
        a->fleeing = true;
        move_away(a, a->flee_x, a->flee_z, run_speed(a), dt);
        return;
    }
    if (a->mode == MODE_RIVAL) {
        rivalry(a, i, all, n, rng, dt, ev);
        return;
    }
    if (a->mode == MODE_EAT) {
        a->timer -= dt;
        // Si alguien se acerca mientras come, lo defiende.
        float dh;
        int h = nearest_human(c, a->x, a->z, human_aggro(d) * 0.5f, true, &dh);
        if (h >= 0) {
            a->tkind = TGT_HUMAN, a->target = h, a->mode = MODE_CHASE;
        } else {
            if (a->tkind == TGT_ANIMAL && a->target >= 0 && a->target < n && all[a->target].used)
                all[a->target].eaten = fminf(1.0f, all[a->target].eaten + dt / 60.0f);
            if (a->timer <= 0.0f) a->mode = MODE_GRAZE, clear_target(a);
            return;
        }
    }
    if (!target_valid(a, all, n, c, d->sense)) clear_target(a);
    if (a->tkind != TGT_NONE && d->leash > 0.0f) { // no se aleja de su guarida
        float tx, tz;
        target_pos(a, all, c, &tx, &tz);
        if (dist_xz(tx, tz, a->home_x, a->home_z) > d->leash * 1.3f) clear_target(a), a->timer = 0.0f;
    }
    if (a->tkind == TGT_NONE) pick_target(a, i, all, n, c);
    if (a->tkind != TGT_NONE) {
        hunt(a, i, all, n, c, rng, dt, ev);
        return;
    }
    if (seek_drink(a, c, dt)) return;
    a->mode = MODE_GRAZE;
    float radius = d->leash > 0.0f ? d->leash * 0.5f : d->flier ? 30.0f : WANDER_RADIUS;
    if (!keep_with_group(a, i, all, n, dt)) wander(a, rng, radius, dt);
    start_rivalry(a, i, all, n);
}

static void update_tamed(Animal *a, int i, Animal *all, int n, const FaunaCtx *c, Rng *rng, float dt, FaunaEvents *ev) {
    const SpeciesDef *d = &SPECIES[a->species];
    a->fleeing = false;
    // Las presas domadas (monturas, ganado) tambien huyen de los cazadores.
    if (species_is_prey(a->species) && !d->defends) {
        float fx, fz;
        if (prey_threat(a, i, all, n, c, &fx, &fz)) a->alert = fmaxf(a->alert, 4.0f), a->flee_x = fx, a->flee_z = fz;
        if (a->alert > 0.0f) {
            a->mode = MODE_FLEE;
            a->fleeing = true;
            move_away(a, a->flee_x, a->flee_z, run_speed(a), dt);
            return;
        }
    }
    float dp = dist_xz(a->x, a->z, c->px, c->pz);
    // Cazadores domados: defienden al jugador de las fieras y cazan liebres a su lado.
    if (d->hunt_max > 0.0f) {
        if (!target_valid(a, all, n, c, 14.0f) || (a->tkind == TGT_HUMAN && !c->humans[a->target].enemy)) clear_target(a);
        if (a->tkind == TGT_NONE && dp < 25.0f) {
            float best = 14.0f;
            for (int h = 0; h < c->human_count; h++) { // enemigos de la tribu cerca del jugador
                const FaunaHuman *hu = &c->humans[h];
                float dd = dist_xz(hu->x, hu->z, c->px, c->pz);
                if (hu->enemy && !hu->down && dd < best) best = dd, a->tkind = TGT_HUMAN, a->target = h;
            }
            for (int k = 0; k < n; k++) {
                const Animal *o = &all[k];
                if (k == i || !alive(o) || o->state != ANIMAL_WILD) continue;
                const SpeciesDef *od = &SPECIES[o->species];
                bool threat = od->cls == CLASS_HOSTILE || od->cls == CLASS_TAMEABLE;
                // Con hambre cazan cualquier presa de su talla; si no, solo liebres para la tribu.
                bool game = od->size <= d->hunt_max && species_is_prey(o->species) && (o->species == SPECIES_HARE || a->hunger > 0.6f);
                if (!threat && !game) continue;
                float dd = dist_xz(o->x, o->z, game && a->hunger > 0.6f ? a->x : c->px, game && a->hunger > 0.6f ? a->z : c->pz);
                if (dd < best) best = dd, a->tkind = TGT_ANIMAL, a->target = k;
            }
        }
        if (a->tkind != TGT_NONE && d->damage > 0.0f) {
            float hunger0 = a->hunger; // el golpe que mata la deja en 0: miramos antes
            hunt(a, i, all, n, c, rng, dt, ev);
            if (a->mode == MODE_EAT && hunger0 <= 0.6f) a->mode = MODE_FOLLOW, clear_target(a); // sin hambre: la presa es para la tribu
            return;
        }
    }
    // Cuervo domado: vuela en circulos por delante del jugador (explora).
    if (a->species == SPECIES_RAVEN) {
        float ang = a->clock * 0.35f;
        move_towards(a, c->px + cosf(ang) * 22.0f, c->pz + sinf(ang) * 22.0f, d->speed, dt);
        a->mode = MODE_FOLLOW;
        return;
    }
    // Ganado: pastoreo. Sigue al jugador que camina cerca; si se aleja, se queda donde lo dejo.
    if (d->cls == CLASS_LIVESTOCK) {
        if (a->mode == MODE_FOLLOW && (dp > HERD_LOSE || !c->player_moving)) {
            a->mode = MODE_GRAZE;
            a->home_x = a->tx = a->x;
            a->home_z = a->tz = a->z;
        }
        if (dp < HERD_RANGE && c->player_moving) a->mode = MODE_FOLLOW;
        if (a->mode == MODE_FOLLOW) {
            float gap = 3.0f + (float)(i % 4);
            if (dp > gap) {
                float ang = (float)i * 2.4f; // cada uno a un lado: rebaño suelto
                move_towards(a, c->px + cosf(ang) * gap * 0.6f, c->pz + sinf(ang) * gap * 0.6f, d->walk * 2.8f, dt);
            }
            return;
        }
        if (!keep_with_group(a, i, all, n, dt)) wander(a, rng, 10.0f, dt);
        return;
    }
    // Domados: siguen al jugador si esta cerca; si no, pastan en el campamento.
    if (dp > FOLLOW_GAP && dp < FOLLOW_RANGE) {
        a->mode = MODE_FOLLOW;
        float dx = a->x - c->px, dz = a->z - c->pz;
        move_towards(a, c->px + dx / dp * FOLLOW_GAP, c->pz + dz / dp * FOLLOW_GAP, d->speed * 0.6f, dt);
        return;
    }
    a->mode = MODE_GRAZE;
    if (dp >= FOLLOW_RANGE) wander(a, rng, WANDER_RADIUS, dt);
}

static void update_bird_alt(Animal *a, float dt) {
    const SpeciesDef *d = &SPECIES[a->species];
    if (!d->flier) return;
    float want;
    if (a->state == ANIMAL_DEAD) want = 0.0f;
    else if (a->state == ANIMAL_BOUND) want = 0.3f;
    else if (a->mode == MODE_CHASE || a->mode == MODE_EAT) want = a->mode == MODE_EAT ? 0.2f : 1.2f; // en picado
    else if (a->state != ANIMAL_WILD) want = a->species == SPECIES_RAVEN ? 12.0f : 4.0f;
    else want = 9.0f + 3.0f * sinf(a->clock * 0.5f);
    float rate = a->mode == MODE_CHASE ? 9.0f : 4.0f;
    a->alt += fmaxf(-rate * dt, fminf(rate * dt, want - a->alt));
}

void fauna_update(Animal *all, int n, const FaunaCtx *c, Rng *rng, float dt, FaunaEvents *ev) {
    for (int i = 0; i < n; i++) {
        Animal *a = &all[i];
        if (!a->used) continue;
        a->clock += dt;
        a->cooldown = fmaxf(0.0f, a->cooldown - dt);
        a->attack_anim = fmaxf(0.0f, a->attack_anim - dt);
        a->hit_anim = fmaxf(0.0f, a->hit_anim - dt);
        a->alert = fmaxf(0.0f, a->alert - dt);
        a->fear_humans = fmaxf(0.0f, a->fear_humans - dt);
        a->rival_timer -= dt;
        if (a->rival_timer < -1000.0f) a->rival_timer = -1000.0f;
        if (a->state == ANIMAL_DEAD) {
            a->corpse += dt;
            update_bird_alt(a, dt);
            continue;
        }
        health_update(&a->h, rng, dt, a->mode == MODE_GRAZE, 0.0f);
        if (a->h.down || a->h.dead) {
            a->ridden = false;
            die(a, false);
            continue;
        }
        a->weakened = a->h.hp < WEAK * a->h.hp_max;
        float hunger0 = a->hunger;
        a->hunger += dt * HUNGER_RATE;
        const SpeciesDef *d = &SPECIES[a->species];
        if (animal_domestic(a)) {
            // Los herbivoros de la tribu pastan solos si hay pasto (quietos o al paso).
            if (d->hunt_max <= 0.0f && c->grass && !a->ridden && a->mode != MODE_FLEE && a->speed < d->walk * 1.5f)
                a->hunger = fmaxf(0.0f, a->hunger - dt * HUNGER_RATE * 4.0f);
            if (hunger0 < 1.0f && a->hunger >= 1.0f) push(ev, FEV_HUNGRY, i, -1, 0.0f, WOUND_BRUISE);
            if (a->hunger > 1.0f) a->h.hp = fmaxf(a->h.hp_max * 0.15f, a->h.hp - dt * 0.12f); // pasa hambre: adelgaza
            // Sed: junto al agua beben solos; con mucha sed adelgazan y al final se van.
            float thirst0 = a->thirst;
            a->thirst += dt * THIRST_ANIMAL_RATE;
            if (water_near(c, a->x, a->z)) a->thirst = fmaxf(0.0f, a->thirst - dt * THIRST_ANIMAL_RATE * 30.0f);
            if (thirst0 < 1.0f && a->thirst >= 1.0f) push(ev, FEV_THIRSTY, i, -1, 0.0f, WOUND_BRUISE);
            if (a->thirst > 1.0f) a->h.hp = fmaxf(a->h.hp_max * 0.15f, a->h.hp - dt * 0.12f);
            bool parched = a->thirst > 2.0f;
            if ((a->hunger > 2.0f || parched) && !a->ridden) { // abandona a la tribu
                a->state = ANIMAL_WILD;
                a->mode = MODE_GRAZE;
                a->group = -1;
                a->home_x = a->tx = a->x;
                a->home_z = a->tz = a->z;
                a->hunger = 0.6f;
                a->thirst = 0.0f;
                clear_target(a);
                push(ev, parched ? FEV_PARCHED : FEV_STARVED, i, -1, 0.0f, WOUND_BRUISE);
            }
        }
        else if (walks_on_land(d) && c->water_depth) { // los salvajes de tierra tambien tienen sed
            a->thirst = fminf(1.5f, a->thirst + dt * THIRST_ANIMAL_RATE);
            if (a->mode != MODE_DRINK && water_near(c, a->x, a->z)) a->thirst = fmaxf(0.0f, a->thirst - dt * THIRST_ANIMAL_RATE * 30.0f);
        }
        if (a->ridden) continue; // lo mueve el jinete (y fija su velocidad)
        float x0 = a->x, z0 = a->z;
        switch (a->state) {
        case ANIMAL_BOUND:
            a->mode = MODE_GRAZE;
            a->bound_timer -= dt;
            a->yaw += sinf(a->clock * 7.0f) * dt * 2.0f; // forcejea
            if (a->bound_timer <= 0.0f) {
                a->state = ANIMAL_WILD;
                push(ev, FEV_BREAK_FREE, i, -1, 0.0f, WOUND_BRUISE);
            }
            break;
        case ANIMAL_TAMED:
        case ANIMAL_SADDLED: update_tamed(a, i, all, n, c, rng, dt, ev); break;
        default: update_wild(a, i, all, n, c, rng, dt, ev); break;
        }
        if (a->state != ANIMAL_BOUND) keep_dry(a, c, x0, z0);
        a->speed = dt > 0.0f ? dist_xz(a->x, a->z, x0, z0) / dt : 0.0f;
        if (a->speed > d->walk * 2.5f) a->stamina = fmaxf(0.0f, a->stamina - tire_rate(d) * dt);
        else a->stamina = fminf(1.0f, a->stamina + dt / 20.0f);
        update_bird_alt(a, dt);
    }
}

void animal_update(Animal *a, float dt, float px, float pz, Rng *rng) {
    FaunaHuman h = { px, pz, false, false, false };
    FaunaCtx c = { &h, 1, px, pz, false, 0.0f, 0.0f, false, NULL, NULL, false };
    fauna_update(a, 1, &c, rng, dt, NULL);
}

// ------------------------------------------------------------------ tribu
TameResult animal_lasso(Animal *a, Rng *rng, float bonus, float camp_x, float camp_z) {
    const SpeciesDef *d = &SPECIES[a->species];
    if (!alive(a)) return TAME_NEVER;
    if (a->state != ANIMAL_WILD) return TAME_ALREADY;
    if (d->cls == CLASS_HOSTILE || d->cls == CLASS_PREY) return TAME_NEVER;
    if (d->cls == CLASS_TAMEABLE) {
        // Depredador: solo se deja atar si esta debilitado.
        if (a->h.hp >= WEAK * a->h.hp_max) return TAME_TOO_STRONG;
        if (rng_float(rng) >= 0.75f + bonus) return TAME_FAILED;
        a->state = ANIMAL_BOUND;
        a->bound_timer = BOUND_SECONDS;
        health_treat(&a->h); // atado y quieto deja de sangrar
        a->fleeing = false;
        clear_target(a);
        return TAME_OK;
    }
    if (rng_float(rng) >= d->tame_chance + bonus) {
        a->alert = 6.0f; // se asusta y huye un rato
        return TAME_FAILED;
    }
    a->state = ANIMAL_TAMED;
    a->fleeing = false;
    a->alert = a->fear_humans = 0.0f;
    a->home_x = a->tx = camp_x; // su territorio pasa a ser el campamento
    a->home_z = a->tz = camp_z;
    a->group = -1;
    return TAME_OK;
}

bool animal_try_tame(Animal *a, Rng *rng, float bonus, float camp_x, float camp_z) {
    return animal_lasso(a, rng, bonus, camp_x, camp_z) == TAME_OK;
}

bool animal_domestic(const Animal *a) { return a->used && (a->state == ANIMAL_TAMED || a->state == ANIMAL_SADDLED); }

bool species_eats_meat(Species s) { return s >= 0 && s < SPECIES_COUNT && SPECIES[s].hunt_max > 0.0f; }

bool animal_feed_tamed(Animal *a, bool meat) {
    if (!animal_domestic(a) || a->hunger < 0.2f || meat != species_eats_meat(a->species)) return false;
    a->hunger = 0.0f;
    return true;
}

bool animal_give_water(Animal *a) {
    if (!animal_domestic(a) || a->thirst < 0.2f) return false;
    a->thirst = 0.0f;
    return true;
}

// Agua a unos pasos (beben solos).
static bool water_near(const FaunaCtx *c, float x, float z) {
    if (!c->water_depth) return false;
    static const float off[5][2] = { { 0, 0 }, { 3, 0 }, { -3, 0 }, { 0, 3 }, { 0, -3 } };
    for (int k = 0; k < 5; k++)
        if (c->water_depth(c->water_ud, x + off[k][0], z + off[k][1]) > 0.0f) return true;
    return false;
}

bool animal_feed(Animal *a, float camp_x, float camp_z) {
    if (a->state != ANIMAL_BOUND) return false;
    a->state = ANIMAL_TAMED;
    a->mode = MODE_FOLLOW;
    a->hunger = 0.0f;
    a->group = -1;
    a->alert = a->fear_humans = 0.0f;
    a->home_x = a->tx = camp_x;
    a->home_z = a->tz = camp_z;
    clear_target(a);
    return true;
}

bool animal_saddle(Animal *a) {
    if (a->state != ANIMAL_TAMED || !SPECIES[a->species].rideable) return false;
    a->state = ANIMAL_SADDLED;
    return true;
}

bool animal_can_ride(const Animal *a) { return a->used && a->state == ANIMAL_SADDLED; }

int animal_milk(Animal *a) {
    const SpeciesDef *d = &SPECIES[a->species];
    if (!alive(a) || a->state == ANIMAL_WILD || a->state == ANIMAL_BOUND || d->milk <= 0 || a->milked) return 0;
    a->milked = true;
    return d->milk;
}

bool animal_slaughter(Animal *a) {
    if (!alive(a) || a->ridden || SPECIES[a->species].cls != CLASS_LIVESTOCK || a->state != ANIMAL_TAMED) return false;
    die(a, true);
    return true;
}

bool animal_butcher(Animal *a, int *meat, int *hide) {
    const SpeciesDef *d = &SPECIES[a->species];
    if (!a->used || a->state != ANIMAL_DEAD || a->butchered) return false;
    a->butchered = true;
    if (meat) *meat = (int)floorf((float)d->meat * (1.0f - a->eaten) + 0.5f);
    if (hide) *hide = a->eaten < 0.5f ? d->hide : 0;
    return true;
}

void animal_new_day(Animal *a) { a->milked = false; }

// ------------------------------------------------------------------ cuerpo
void animal_body(const Animal *a, BodyPose *b) {
    const SpeciesDef *d = &SPECIES[a->species];
    memset(b, 0, sizeof(*b));
    float s = d->size / 1.3f; // las medidas de abajo, para una fiera de 1.3 m
    float y0 = d->flier ? a->alt : 0.0f;
    if (d->flier) { // un ave: cuerpo y cabeza
        b->seg[PART_THORAX] = (BodySeg){ { 0, y0 + 0.1f, -0.15f }, { 0, y0 + 0.1f, 0.1f }, 0.09f };
        b->seg[PART_HEAD] = (BodySeg){ { 0, y0 + 0.15f, 0.15f }, { 0, y0 + 0.15f, 0.2f }, 0.05f };
        b->seg[PART_UPPER_ARM_L] = (BodySeg){ { 0.05f, y0 + 0.12f, 0 }, { 0.45f, y0 + 0.12f, 0 }, 0.04f };
        b->seg[PART_UPPER_ARM_R] = (BodySeg){ { -0.05f, y0 + 0.12f, 0 }, { -0.45f, y0 + 0.12f, 0 }, 0.04f };
        return;
    }
    if (a->species == SPECIES_SNAKE) { // larga, a ras de suelo, en zigzag
        for (int k = 0; k < 4; k++) {
            float z0 = 0.5f - 0.25f * (float)k, z1 = z0 - 0.25f, x0 = (k % 2 ? 0.05f : -0.05f);
            b->seg[PART_THORAX + k] = (BodySeg){ { x0, 0.04f, z0 }, { -x0, 0.04f, z1 }, 0.035f };
        }
        b->seg[PART_HEAD] = (BodySeg){ { 0, 0.05f, 0.5f }, { 0, 0.05f, 0.56f }, 0.045f };
        return;
    }
    if (a->state == ANIMAL_DEAD) { // tendido de lado
        b->seg[PART_THORAX] = (BodySeg){ { 0, 0.2f * s, -0.4f * s }, { 0, 0.2f * s, 0.4f * s }, 0.2f * s };
        b->seg[PART_HEAD] = (BodySeg){ { 0, 0.12f * s, 0.6f * s }, { 0, 0.12f * s, 0.75f * s }, 0.12f * s };
        return;
    }
    b->seg[PART_HEAD] = (BodySeg){ { 0, 0.65f * s, 0.55f * s }, { 0, 0.65f * s, 0.7f * s }, 0.13f * s };
    b->seg[PART_NECK] = (BodySeg){ { 0, 0.58f * s, 0.42f * s }, { 0, 0.63f * s, 0.52f * s }, 0.09f * s };
    b->seg[PART_THORAX] = (BodySeg){ { 0, 0.55f * s, 0.1f * s }, { 0, 0.55f * s, 0.4f * s }, 0.2f * s };
    b->seg[PART_ABDOMEN] = (BodySeg){ { 0, 0.55f * s, -0.3f * s }, { 0, 0.55f * s, 0.1f * s }, 0.18f * s };
    b->seg[PART_PELVIS] = (BodySeg){ { 0, 0.55f * s, -0.5f * s }, { 0, 0.55f * s, -0.32f * s }, 0.17f * s };
    b->seg[PART_FOREARM_L] = (BodySeg){ { 0.12f * s, 0.4f * s, 0.35f * s }, { 0.12f * s, 0.02f, 0.35f * s }, 0.05f * s };
    b->seg[PART_FOREARM_R] = (BodySeg){ { -0.12f * s, 0.4f * s, 0.35f * s }, { -0.12f * s, 0.02f, 0.35f * s }, 0.05f * s };
    b->seg[PART_SHIN_L] = (BodySeg){ { 0.12f * s, 0.4f * s, -0.35f * s }, { 0.12f * s, 0.02f, -0.35f * s }, 0.05f * s };
    b->seg[PART_SHIN_R] = (BodySeg){ { -0.12f * s, 0.4f * s, -0.35f * s }, { -0.12f * s, 0.02f, -0.35f * s }, 0.05f * s };
    // Reptiles y bichos: cuerpo bajo, pegado al suelo (y ancho).
    bool low = a->species == SPECIES_CROCODILE || a->species == SPECIES_TURTLE || a->species == SPECIES_SCORPION ||
               a->species == SPECIES_SPIDER;
    if (!low) return;
    float k = a->species == SPECIES_TURTLE ? 0.45f : 0.3f;
    for (int i = 0; i < PART_COUNT; i++) {
        BodySeg *g = &b->seg[i];
        if (g->radius <= 0.0f) continue;
        g->a.y *= k, g->b.y *= k;
        g->a.x *= 1.8f, g->b.x *= 1.8f; // patas abiertas
        g->radius *= i == PART_THORAX || i == PART_ABDOMEN ? (a->species == SPECIES_TURTLE ? 1.6f : 0.8f) : 0.7f;
    }
}
