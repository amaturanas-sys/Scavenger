#include "game/actions_game.h"

#include "game/fauna_game.h"
#include "game/inventory_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"
#include "sim/anim_index.h"
#include "sim/body.h"
#include "sim/melee.h"
#include "sim/clock.h"
#include "world/body_draw.h"
#include "ui/theme.h"

// Equipo de prueba del jugador (Fase 0): herramientas y armas para probar las acciones.
// Lo forjado en los hornos se suma a traves del acopio.
static const char *KIT[] = {
    "arma.corta.sable",          "arma.corta.daga",         "escudo.mano.mimbre",    "escudo.mano.cuero",
    "arma.larga.guja",           "arma.larga.lanza",        "arma.distancia.arco_compuesto",
    "utileria.objeto.antorcha",  "arma.distancia.lazo",     "utileria.herramienta.gancho_trepa",
    "utileria.herramienta.pala", "accesorio.arreo.silla_montar", "arma.distancia.ballesta", "arma.distancia.honda",
};
// Empunaduras que recorre "cambiar empunadura" (X): derecha, izquierda. El sable
// se sustituye por el mejor que haya en el acopio (acero > bronce > comun).
static const char *PRESETS[][2] = {
    { "arma.corta.sable", "escudo.mano.mimbre" },        // arma y escudo
    { "arma.corta.sable", "arma.corta.daga" },           // una en cada mano
    { "arma.larga.guja", NULL },                         // a dos manos
    { "arma.distancia.arco_compuesto", NULL },           // arco (dos manos)
    { "arma.distancia.ballesta", NULL },                 // ballesta
    { "arma.distancia.honda", NULL },                    // honda
    { "arma.larga.lanza", "escudo.mano.cuero" },         // lanza y escudo
    { NULL, NULL },                                      // desarmado
};
#define PRESET_COUNT ((int)(sizeof(PRESETS) / sizeof(PRESETS[0])))
#define REACH 2.2f          // m: alcance para tomar objetos
#define TARGET_RANGE 14.0f  // m: alcance de la trepa y el lazo
#define SADDLE_RANGE 4.0f   // m: para ensillar o montar
#define HELP_RANGE 8.0f     // m: el jugador ayuda en una obra si esta cerca
#define AT_SITE 2.6f        // m: un NPC ya esta en la obra
#define NPC_SPEED 2.6f      // m/s
#define CAMP_RADIUS 45.0f   // m: lo construido dentro de este radio cuenta para el campamento
#define MENU_ROWS 21        // filas visibles del menu (se desplaza)

static const int CAMP_YURTS = 5; // las yurtas del campamento inicial (world/camp.c)

// ---------------------------------------------------------------- utilidades
static const char *item_name(const GameActions *ga, const char *id) {
    const InvItem *it = inventory_find(ga->inv, id);
    return it ? it->name : id;
}

static bool has_item(const GameActions *ga, const char *id) {
    for (size_t i = 0; i < sizeof(KIT) / sizeof(KIT[0]); i++)
        if (!strcmp(KIT[i], id)) return true;
    return stock_count(&ga->stock, id) > 0;
}

static const char *best_sable(const GameActions *ga) {
    if (stock_count(&ga->stock, "arma.corta.sable_acero") > 0) return "arma.corta.sable_acero";
    if (stock_count(&ga->stock, "arma.corta.sable_bronce") > 0) return "arma.corta.sable_bronce";
    return "arma.corta.sable";
}

static void apply_preset(GameActions *ga) {
    hands_clear(&ga->hands, HAND_RIGHT);
    hands_clear(&ga->hands, HAND_LEFT);
    ga->torch_lit = false;
    const char *r = PRESETS[ga->preset][0], *l = PRESETS[ga->preset][1];
    if (r && !strcmp(r, "arma.corta.sable")) r = best_sable(ga);
    if (r) hands_equip(&ga->hands, inventory_find(ga->inv, r), HAND_RIGHT);
    if (l) hands_equip(&ga->hands, inventory_find(ga->inv, l), HAND_LEFT);
}

static Vector3 forward_of(float yaw) { return (Vector3){ sinf(yaw), 0.0f, cosf(yaw) }; }

static Vector3 ground_at(const Terrain *t, float x, float z) { return (Vector3){ x, terrain_height(t, x, z), z }; }

static Vector3 ground_ahead(const Terrain *t, const Player *p, float dist) {
    Vector3 f = forward_of(p->yaw);
    return ground_at(t, p->pos.x + f.x * dist, p->pos.z + f.z * dist);
}

static float dist2d(Vector3 a, Vector3 b) { return Vector2Distance((Vector2){ a.x, a.z }, (Vector2){ b.x, b.z }); }

// Esta el punto delante del jugador (dentro de ~60 grados) y a menos de range?
static bool in_front(const Player *p, float x, float z, float range) {
    Vector3 f = forward_of(p->yaw);
    float dx = x - p->pos.x, dz = z - p->pos.z, d = sqrtf(dx * dx + dz * dz);
    return d < range && (dx * f.x + dz * f.z) > d * 0.5f;
}

// Los recursos tomados van directo al acopio de la tribu.
static bool is_resource(const char *id) {
    return !strncmp(id, "utileria.material.", 18) || !strncmp(id, "utileria.consumible.", 20) ||
           !strcmp(id, "utileria.objeto.lena");
}

// Lo que al tomarlo va a la mochila (o donde quepa): materiales, comida, armas, armaduras,
// municion, amuletos y las matas de hierbas. Lo demas (un odre, un cofre) se lleva en brazos.
static bool goes_to_bags(const InvItem *it) {
    if (!strcmp(it->id, "mapa.vegetacion.mata_hierbas")) return true;
    if (!item_storable(it)) return false;
    return strncmp(it->id, "utileria.objeto.", 16) != 0 || !strcmp(it->id, "utileria.objeto.lena");
}

static int count_props(const Props *props, const char *id) {
    int n = 0;
    for (int i = 0; i < props->count; i++)
        if (!strcmp(props->items[i].item->id, id)) n++;
    return n;
}

// ---------------------------------------------------------------- inicio
static void scatter(Props *props, const Terrain *t, Rng *rng, const char *id, int n, float rmin, float rmax) {
    for (int i = 0; i < n; i++) {
        float a = rng_float(rng) * 6.2831853f, r = rmin + rng_float(rng) * (rmax - rmin);
        float x = cosf(a) * r, z = sinf(a) * r;
        props_add(props, id, ground_at(t, x, z), rng_float(rng) * 6.28f);
    }
}

void ga_init(GameActions *ga, const Inventory *inv, Props *props, const Terrain *t, unsigned seed) {
    memset(ga, 0, sizeof(*ga));
    ga->inv = inv;
    rng_seed(&ga->rng, seed);
    ga->doing = ga->target = ga->crafting = ga->mounted = -1;
    hands_init(&ga->hands);
    stock_init(&ga->stock);
    stock_seed_camp(&ga->stock);
    apply_preset(ga);

    // Objetos sueltos para tomar y lanzar, un tramo de empalizada para la trepa,
    // recursos para recolectar y hierba alta para esconderse.
    static const struct {
        const char *id;
        float x, z, yaw;
    } START[] = {
        { "utileria.objeto.odre", -2.2f, 17.4f, 0.0f },  { "utileria.objeto.cofre", -3.5f, 15.5f, 0.6f },
        { "utileria.objeto.caldero", 1.4f, 3.0f, 0.0f }, { "estructura.campamento.empalizada", 9.0f, 14.0f, 1.57f },
    };
    for (size_t i = 0; i < sizeof(START) / sizeof(START[0]); i++)
        props_add(props, START[i].id, ground_at(t, START[i].x, START[i].z), START[i].yaw);
    scatter(props, t, &ga->rng, "utileria.objeto.lena", 6, 14.0f, 30.0f);
    scatter(props, t, &ga->rng, "utileria.material.piedra", 4, 20.0f, 40.0f);
    scatter(props, t, &ga->rng, "utileria.material.barro", 3, 20.0f, 40.0f);
    scatter(props, t, &ga->rng, "mapa.vegetacion.hierba_alta", 28, 18.0f, 45.0f);
    // Lo que se recolecta para fabricar: matas de hierbas curativas y pedernal.
    scatter(props, t, &ga->rng, "mapa.vegetacion.mata_hierbas", 12, 16.0f, 70.0f);
    scatter(props, t, &ga->rng, "utileria.material.pedernal", 8, 20.0f, 70.0f);

    // Animales: el ganado del campamento y manadas en los alrededores.
    fg_init(ga, t);
    // Lo que se lleva encima, la carreta y la armeria (src/game/inventory_game.c).
    ig_init(ga, props, t);
}

bool ga_menu_open(const GameActions *ga) { return ga->menu_open; }
bool ga_blocks_input(const GameActions *ga) { return ga->menu_open || ga->climbing || ga->inv_open || ga->equip_open; }

float ga_speed_scale(const GameActions *ga) {
    // Montado, la montura (y el amuleto del caballo); a pie, cuanto pesa lo que llevas.
    if (ga->mounted >= 0)
        return species_def(ga->animals[ga->mounted].species)->ride_speed * (1.0f + 0.5f * ig_stat(ga, STAT_RIDING));
    return ig_speed_scale(ga);
}

// ---------------------------------------------------------------- objetivos
static int wall_ahead(const Props *props, const Player *p) {
    for (int i = 0; i < props->count; i++) {
        const char *id = props->items[i].item->id;
        if (!strstr(id, "empalizada") && !strstr(id, "muro") && !strstr(id, "muralla")) continue;
        if (in_front(p, props->items[i].pos.x, props->items[i].pos.z, TARGET_RANGE)) return i;
    }
    return -1;
}

static int animal_ahead(const GameActions *ga, const Player *p, AnimalState state, float range, bool need_front) {
    int best = -1;
    float best_d = range;
    for (int i = 0; i < ga->animal_count; i++) {
        const Animal *a = &ga->animals[i];
        if (!a->used || a->state != state || a->ridden) continue;
        float d = dist2d(p->pos, (Vector3){ a->x, 0, a->z });
        if (d < best_d && (!need_front || in_front(p, a->x, a->z, range))) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

// ---------------------------------------------------------------- acciones del jugador
static void start_action(GameActions *ga, ActionId a, const Props *props, const Player *p, char *log, size_t len) {
    const ActionDef *d = action_def(a);
    if (ga->doing >= 0 || ga->climbing) return;
    if (d->requires && !has_item(ga, d->requires)) {
        snprintf(log, len, "Te falta: %s.", item_name(ga, d->requires));
        return;
    }
    const Ingredient *miss = stock_first_missing(&ga->stock, d->mats);
    if (miss) {
        snprintf(log, len, "Falta en el acopio: %s (%d de %d).", item_name(ga, miss->id),
                 stock_count(&ga->stock, miss->id), miss->count);
        return;
    }
    ga->target = -1;
    switch (a) {
    case ACTION_TAKE: {
        if (ig_take_loot(ga, props, p, log, len)) return; // una bolsa de botin cerca: al momento
        int near = props_nearest(props, p->pos, REACH, true);
        ga->take_all = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (near >= 0 && goes_to_bags(props->items[near].item)) break; // a la mochila: no hace falta mano libre
        if (ga->hands.carried[0]) {
            snprintf(log, len, "Ya llevas algo: T para lanzarlo.");
            return;
        }
        if (!hands_can_take(&ga->hands)) {
            snprintf(log, len, "Necesitas una mano libre (H para enfundar).");
            return;
        }
        if (near < 0) {
            snprintf(log, len, "No hay nada que tomar al alcance.");
            return;
        }
        break;
    }
    case ACTION_THROW:
        if (!ga->hands.carried[0]) {
            snprintf(log, len, "No llevas nada para lanzar (F para tomar).");
            return;
        }
        break;
    case ACTION_SHEATHE:
        if (hands_grip(&ga->hands) == GRIP_EMPTY) {
            snprintf(log, len, "No tienes armas que enfundar.");
            return;
        }
        break;
    case ACTION_THROW_GRAPPLE:
        if (ga->mounted >= 0) {
            snprintf(log, len, "Desmonta antes de trepar (R).");
            return;
        }
        if ((ga->target = wall_ahead(props, p)) < 0) {
            snprintf(log, len, "No hay un muro a tiro de trepa delante.");
            return;
        }
        break;
    case ACTION_THROW_LASSO:
        if ((ga->target = animal_ahead(ga, p, ANIMAL_WILD, TARGET_RANGE, true)) < 0) {
            snprintf(log, len, "No hay un animal salvaje a tiro de lazo delante.");
            return;
        }
        break;
    case ACTION_SADDLE:
        if ((ga->target = animal_ahead(ga, p, ANIMAL_TAMED, SADDLE_RANGE, false)) < 0) {
            snprintf(log, len, "No hay un animal domado cerca para ensillar.");
            return;
        }
        if (!species_def(ga->animals[ga->target].species)->rideable) {
            snprintf(log, len, "Un %s no se puede montar.", species_def(ga->animals[ga->target].species)->name);
            return;
        }
        break;
    default: break;
    }
    stock_take_all(&ga->stock, d->mats);
    ga->doing = a;
    ga->timer = 0.0f;
}

static void start_climb(GameActions *ga, const Props *props, const Terrain *t, const Player *p, int wall) {
    const Prop *w = &props->items[wall];
    // Del jugador al muro, por encima y al otro lado.
    float dx = w->pos.x - p->pos.x, dz = w->pos.z - p->pos.z, d = sqrtf(dx * dx + dz * dz);
    if (d < 0.01f) d = 0.01f;
    dx /= d;
    dz /= d;
    float top = w->pos.y + w->item->h + 0.1f;
    ga->climbing = true;
    ga->climb_t = 0.0f;
    ga->climb_from = p->pos;
    ga->climb_top = (Vector3){ w->pos.x - dx * 0.5f, top, w->pos.z - dz * 0.5f };
    ga->climb_over = (Vector3){ w->pos.x + dx * 0.5f, top, w->pos.z + dz * 0.5f };
    ga->climb_to = ground_at(t, w->pos.x + dx * 1.5f, w->pos.z + dz * 1.5f);
}

static void finish_action(GameActions *ga, Props *props, const Terrain *t, const Player *p, char *log, size_t len) {
    ActionId a = (ActionId)ga->doing;
    const ActionDef *d = action_def(a);
    ga->doing = -1;
    switch (a) {
    case ACTION_TAKE: {
        int i = props_nearest(props, p->pos, REACH, true);
        if (i < 0) break;
        const InvItem *it = props->items[i].item;
        if (!strcmp(it->id, "mapa.vegetacion.mata_hierbas")) { // recolectar hierbas curativas
            int n = 1 + rng_range(&ga->rng, 2), kept = ig_store(ga, props, p, "utileria.consumible.hierbas", n, 1.0f);
            snprintf(log, len, kept ? "Recoges %d hierbas curativas." : "No te caben más hierbas.", kept);
            if (kept) props_remove(props, i);
        } else if (goes_to_bags(it)) {
            // A la mochila (o donde quepa); con Mayús, todo lo que haya alrededor.
            int taken = 0, left = 0;
            const char *last = it->name;
            for (int k = props->count - 1; k >= 0; k--) {
                const Prop *pr = &props->items[k];
                if (k != i && (!ga->take_all || pr->flying || !goes_to_bags(pr->item) ||
                               Vector3Distance(pr->pos, p->pos) > REACH + 1.5f ||
                               !strcmp(pr->item->id, "mapa.vegetacion.mata_hierbas")))
                    continue;
                if (ig_store(ga, props, p, pr->item->id, 1, pr->condition) == 1) {
                    last = pr->item->name;
                    taken++;
                    props_remove(props, k);
                    if (k < i) i--;
                } else {
                    left++;
                }
            }
            if (taken > 1) snprintf(log, len, "Tomas %d cosas%s.", taken, left ? TextFormat(" (%d no caben)", left) : "");
            else if (taken) snprintf(log, len, "Tomaste: %s.", last);
            else if (is_resource(it->id) && !ga->hands.carried[0] && hands_take(&ga->hands, it->id)) { // demasiado grande: en brazos
                snprintf(log, len, "No cabe en la mochila: llevas %s en brazos (T para soltarlo).", it->name);
                props_remove(props, i);
            } else snprintf(log, len, "No te cabe: %s.", it->name);
        } else if (hands_take(&ga->hands, it->id)) {
            snprintf(log, len, "Tomaste: %s (T para lanzarlo).", it->name);
            props_remove(props, i);
        }
        break;
    }
    case ACTION_THROW: {
        char id[INV_ID_LEN];
        if (!hands_throw(&ga->hands, id, sizeof(id))) break;
        Vector3 f = forward_of(p->yaw);
        int i = props_add(props, id, (Vector3){ p->pos.x + f.x * 0.6f, p->pos.y + 1.5f, p->pos.z + f.z * 0.6f }, p->yaw);
        if (i >= 0) {
            props->items[i].flying = true;
            props->items[i].vel = (Vector3){ f.x * 9.0f, 4.0f, f.z * 9.0f };
        }
        snprintf(log, len, "Lanzaste: %s.", item_name(ga, id));
        break;
    }
    case ACTION_CHANGE_GRIP:
        ga->preset = (ga->preset + 1) % PRESET_COUNT;
        apply_preset(ga);
        snprintf(log, len, "Empuñadura: %s.", grip_name(hands_grip(&ga->hands)));
        break;
    case ACTION_SHEATHE:
        hands_toggle_sheathe(&ga->hands);
        if (ga->hands.sheathed) ga->torch_lit = false;
        snprintf(log, len, ga->hands.sheathed ? "Enfundaste las armas: manos libres." : "Desenfundaste.");
        break;
    case ACTION_LIGHT_TORCH:
        hands_equip(&ga->hands, inventory_find(ga->inv, d->requires), HAND_LEFT);
        ga->torch_lit = true;
        snprintf(log, len, "Encendiste la antorcha (mano izquierda).");
        break;
    case ACTION_THROW_GRAPPLE:
        if (ga->target >= 0 && ga->target < props->count) {
            start_climb(ga, props, t, p, ga->target);
            snprintf(log, len, "La trepa se engancha: ¡a escalar!");
        }
        break;
    case ACTION_THROW_LASSO: {
        Animal *an = &ga->animals[ga->target];
        if (!an->used) break;
        const SpeciesDef *sd = species_def(an->species);
        char who[48];
        snprintf(who, sizeof(who), "%s", sd->name);
        if (who[0] >= 'A' && who[0] <= 'Z') who[0] = (char)(who[0] - 'A' + 'a');
        switch (animal_lasso(an, &ga->rng, ga->mounted >= 0 ? 0.1f : 0.0f, 0.0f, 0.0f)) {
        case TAME_OK:
            if (an->state == ANIMAL_BOUND)
                snprintf(log, len, "¡Atrapaste al %s con el lazo! Dale carne (K) antes de que se suelte.", who);
            else
                snprintf(log, len, "¡Domaste: %s! Ahora sigue a la tribu%s.", who, sd->rideable ? " (silla para montarlo)" : "");
            break;
        case TAME_TOO_STRONG: snprintf(log, len, "El %s está demasiado entero: debilítalo peleando antes del lazo.", who); break;
        case TAME_NEVER: snprintf(log, len, "Un animal así no se doma (%s): solo se caza.", who); break;
        case TAME_ALREADY: snprintf(log, len, "Ya es de la tribu."); break;
        default: snprintf(log, len, "El %s se zafó del lazo.", who); break;
        }
        break;
    }
    case ACTION_SADDLE:
        if (animal_saddle(&ga->animals[ga->target]))
            snprintf(log, len, "Montura instalada: R para montar.");
        break;
    default:
        if (d->produces) { // instalar fogata, tienda, cavar trinchera
            props_add(props, d->produces, ground_ahead(t, p, 2.5f), p->yaw);
            snprintf(log, len, "Hecho: %s.", item_name(ga, d->produces));
        }
        break;
    }
}

static void start_build(GameActions *ga, BuildId b, const Terrain *t, const Player *p, char *log, size_t len) {
    const BuildDef *d = build_def(b);
    if (ga->project_count >= GA_MAX_PROJECTS) {
        snprintf(log, len, "Demasiadas obras a la vez.");
        return;
    }
    const Ingredient *miss = stock_first_missing(&ga->stock, d->mats);
    if (miss) {
        snprintf(log, len, "Falta en el acopio: %s (%d de %d).", item_name(ga, miss->id),
                 stock_count(&ga->stock, miss->id), miss->count);
        return;
    }
    stock_take_all(&ga->stock, d->mats);
    Vector3 at = ground_ahead(t, p, 6.0f);
    ga->projects[ga->project_count++] = (BuildProject){ b, at.x, at.z, 0.0f, false };
    snprintf(log, len, "Obra iniciada: %s. La cuadrilla va en camino.", d->name);
}

static void start_craft(GameActions *ga, CraftId c, const Props *props, const Player *p, const Troop *troop, char *log,
                        size_t len) {
    const CraftDef *d = craft_def(c);
    if (ga->crafting >= 0) {
        snprintf(log, len, "Ya estás fabricando algo: %s.", craft_def((CraftId)ga->crafting)->name);
        return;
    }
    if (d->building && count_props(props, d->building) == 0) {
        snprintf(log, len, "Hace falta: %s.", item_name(ga, d->building));
        return;
    }
    float secs = craft_seconds(d, troop);
    if (secs < 0.0f) {
        snprintf(log, len, "Hace falta un %s en la tribu.", role_name(d->role));
        return;
    }
    // Los ingredientes: lo que llevas encima, lo que tienes cerca o el acopio (en el campamento).
    const Ingredient *miss = ig_first_missing(ga, props, p, d->mats);
    if (miss) {
        snprintf(log, len, "Falta: %s (%d de %d).", item_name(ga, miss->id), ig_count(ga, props, p, miss->id), miss->count);
        return;
    }
    ig_use_all(ga, props, p, d->mats);
    ga->crafting = c;
    ga->craft_timer = 0.0f;
    ga->craft_total = secs;
    snprintf(log, len, craft_by_hand(d) ? "Fabricando: %s." : "Forjando: %s.", d->name);
}

// ---------------------------------------------------------------- NPCs
// Mantiene un NPC por integrante activo de la tropa (los nuevos aparecen junto al fuego).
static void sync_npcs(GameActions *ga, const Troop *troop, const Terrain *t) {
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        Npc *n = &ga->npcs[i];
        if (n->member_id == troop->members[i].id) continue;
        memset(n, 0, sizeof(*n));
        n->member_id = troop->members[i].id;
        float a = (float)i * 2.39996f, r = 5.5f + (float)(i % 3) * 1.5f; // espiral dorada alrededor del fuego
        n->home = ground_at(t, cosf(a) * r, sinf(a) * r);
        n->pos = n->home;
        n->project = n->job = -1;
    }
}

static bool npc_walk(Npc *n, const Terrain *t, Vector3 to, float dt) {
    float d = dist2d(n->pos, to);
    n->moving = d >= 0.3f;
    if (d < 0.3f) return true;
    float step = fminf(NPC_SPEED * dt, d);
    float dx = (to.x - n->pos.x) / d, dz = (to.z - n->pos.z) / d;
    n->pos.x += dx * step;
    n->pos.z += dz * step;
    n->pos.y = terrain_height(t, n->pos.x, n->pos.z);
    n->yaw = atan2f(dx, dz);
    return false;
}

static Npc *npc_of(GameActions *ga, const Troop *troop, int member_id) {
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
        if (ga->npcs[i].member_id == member_id) return &ga->npcs[i];
    return NULL;
}

// Un NPC libre hace una accion individual (las mismas definiciones que el jugador).
static bool npc_assign_job(GameActions *ga, const Troop *troop, const Terrain *t, ActionId a, Vector3 where) {
    const ActionDef *d = action_def(a);
    if (!(d->actors & ACTOR_NPC) || !stock_has_all(&ga->stock, d->mats)) return false;
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        Npc *n = &ga->npcs[i];
        if (troop->members[i].status != STATUS_ACTIVE || n->project >= 0 || n->job >= 0 || n->escort) continue;
        stock_take_all(&ga->stock, d->mats);
        n->job = a;
        n->job_pos = ground_at(t, where.x, where.z);
        n->job_timer = 0.0f;
        return true;
    }
    return false;
}

static void update_npcs(GameActions *ga, Props *props, const Terrain *t, const Troop *troop, float dt, char *log,
                        size_t len) {
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        Npc *n = &ga->npcs[i];
        const Member *m = &troop->members[i];
        if (m->status != STATUS_ACTIVE || n->escort) continue;
        if (n->project >= 0 && n->project < ga->project_count) {
            const BuildProject *bp = &ga->projects[n->project];
            float ang = (float)i * 1.3f; // cada uno en su lado de la obra
            npc_walk(n, t, (Vector3){ bp->x + cosf(ang) * 1.6f, 0, bp->z + sinf(ang) * 1.6f }, dt);
        } else if (n->job >= 0) {
            if (npc_walk(n, t, n->job_pos, dt)) {
                n->job_timer += dt;
                const ActionDef *d = action_def((ActionId)n->job);
                if (n->job_timer >= d->seconds) {
                    if (d->produces) props_add(props, d->produces, n->job_pos, n->yaw);
                    snprintf(log, len, "%s terminó: %s.", m->name, d->name);
                    n->job = -1;
                }
            }
        } else {
            npc_walk(n, t, n->home, dt);
        }
    }
}

// ---------------------------------------------------------------- actualizacion
void ga_update(GameActions *ga, Props *props, const Terrain *t, Player *p, Troop *troop, float dt, char *log,
               size_t log_len) {
    sync_npcs(ga, troop, t);
    ga->swap_anim = fmaxf(0.0f, ga->swap_anim - dt);
    bool other_menu = ga->inv_open || ga->equip_open; // inventario o equipo abiertos
    if (IsKeyPressed(KEY_TAB) && !other_menu) ga->menu_open = !ga->menu_open;
    if (ga->menu_open) {
        // Pestañas: acciones, obras, fabricar, reparar.
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) ga->menu_tab = (ga->menu_tab + 1) % 4;
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) ga->menu_tab = (ga->menu_tab + 3) % 4;
        RepairItem rep[24];
        int nrep = ga->menu_tab == 3 ? ig_repair_list(ga, props, p, rep, 24) : 0;
        int total = ga->menu_tab == 0 ? ACTION_COUNT : ga->menu_tab == 1 ? BUILD_COUNT : ga->menu_tab == 2 ? CRAFT_COUNT : nrep;
        int *cur = &ga->tab_cursor[ga->menu_tab];
        if (total > 0) {
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) *cur = (*cur + 1) % total;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) *cur = (*cur + total - 1) % total;
            if (*cur >= total) *cur = total - 1;
        }
        if (IsKeyPressed(KEY_ENTER) && total > 0) {
            if (ga->menu_tab != 3) ga->menu_open = false;
            switch (ga->menu_tab) {
            case 0: start_action(ga, (ActionId)*cur, props, p, log, log_len); break;
            case 1: start_build(ga, (BuildId)*cur, t, p, log, log_len); break;
            case 2: start_craft(ga, (CraftId)*cur, props, p, troop, log, log_len); break;
            default: ig_repair(ga, props, p, troop, &rep[*cur], log, log_len); break;
            }
        }
    } else if (ga->doing < 0 && !ga->climbing && !other_menu) {
        if (IsKeyPressed(KEY_X) && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))) { // cambiar de mano
            snprintf(log, log_len, hands_swap(&ga->hands) ? "Pasas el arma a la otra mano: %s."
                                                         : "No se puede: el escudo va en el brazo izquierdo y las armas a dos manos, en las dos (%s).",
                     grip_name(hands_grip(&ga->hands)));
            ga->swap_anim = 0.5f;
        } else if (IsKeyPressed(KEY_X)) start_action(ga, ACTION_CHANGE_GRIP, props, p, log, log_len);
        if (IsKeyPressed(KEY_H)) start_action(ga, ACTION_SHEATHE, props, p, log, log_len);
        if (IsKeyPressed(KEY_F)) start_action(ga, ACTION_TAKE, props, p, log, log_len);
        if (IsKeyPressed(KEY_T)) start_action(ga, ACTION_THROW, props, p, log, log_len);
        if (IsKeyPressed(KEY_R)) { // montar / desmontar
            if (ga->mounted >= 0) {
                ga->animals[ga->mounted].ridden = false;
                ga->mounted = -1;
                snprintf(log, log_len, "Desmontaste.");
            } else {
                int a = -1;
                for (int i = 0; i < ga->animal_count; i++)
                    if (animal_can_ride(&ga->animals[i]) && !ga->animals[i].h.down &&
                        dist2d(p->pos, (Vector3){ ga->animals[i].x, 0, ga->animals[i].z }) < SADDLE_RANGE)
                        a = i;
                if (a >= 0) {
                    ga->mounted = a;
                    ga->animals[a].ridden = true;
                    snprintf(log, log_len, "Montaste el %s.", species_def(ga->animals[a].species)->name);
                } else {
                    snprintf(log, log_len, "No hay una montura ensillada cerca (lazo + silla).");
                }
            }
        }
    }

    if (ga->doing >= 0) {
        ga->timer += dt;
        if (ga->timer >= action_def((ActionId)ga->doing)->seconds) finish_action(ga, props, t, p, log, log_len);
    }

    // Objetos en vuelo.
    for (int i = 0; i < props->count; i++) {
        Prop *pr = &props->items[i];
        if (!pr->flying) continue;
        pr->vel.y -= 18.0f * dt;
        pr->pos = Vector3Add(pr->pos, Vector3Scale(pr->vel, dt));
        float ground = terrain_height(t, pr->pos.x, pr->pos.z);
        if (pr->pos.y <= ground) {
            pr->pos.y = ground;
            pr->flying = false;
        }
    }

    // Obras: cada una elige su cuadrilla (sin repetir gente) y avanza con los que ya llegaron.
    int busy[TROOP_MAX], n_busy = 0;
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        ga->npcs[i].project = -1;
        if (ga->npcs[i].escort) busy[n_busy++] = ga->npcs[i].member_id; // la escolta no trabaja en obras
    }
    for (int i = 0; i < ga->project_count; i++) {
        BuildProject *bp = &ga->projects[i];
        const BuildDef *d = build_def(bp->def);
        Vector3 site = { bp->x, 0, bp->z };
        bool player_near = dist2d(p->pos, site) < HELP_RANGE;
        CrewPlan plan = build_plan_excluding(d, troop, player_near, busy, n_busy);
        bool present[TROOP_MAX + 1] = { false };
        ga->crew_present[i] = 0;
        ga->crew_size[i] = plan.check == BUILD_READY ? plan.workers : 0;
        for (int k = 0; plan.check == BUILD_READY && k < plan.workers; k++) {
            if (plan.ids[k] < 0) {
                present[k] = player_near;
            } else {
                Npc *n = npc_of(ga, troop, plan.ids[k]);
                if (!n || n->job >= 0) continue;
                n->project = i;
                busy[n_busy++] = plan.ids[k];
                present[k] = dist2d(n->pos, site) < AT_SITE;
            }
            ga->crew_present[i] += present[k];
        }
        if (build_advance(bp, build_rate_present(d, &plan, present), dt)) {
            props_add(props, d->produces, ground_at(t, bp->x, bp->z), 0.0f);
            snprintf(log, log_len, "Obra terminada: %s.", d->name);
            ga->projects[i] = ga->projects[--ga->project_count];
            ga->crew_present[i] = ga->crew_present[ga->project_count];
            ga->crew_size[i] = ga->crew_size[ga->project_count];
            i--;
        }
    }
    update_npcs(ga, props, t, troop, dt, log, log_len);

    // Forja.
    if (ga->crafting >= 0) {
        ga->craft_timer += dt;
        if (ga->craft_timer >= ga->craft_total) {
            const CraftDef *c = craft_def((CraftId)ga->crafting);
            int n = c->amount > 0 ? c->amount : 1;
            // A lo que llevas (o al acopio, si no cabe).
            int kept = ig_store(ga, props, p, c->produces, n, 1.0f);
            if (kept < n) stock_add(&ga->stock, c->produces, n - kept);
            snprintf(log, log_len, "%s: %s%s.", craft_by_hand(c) ? "Hecho" : "Forjado", c->name,
                     kept < n ? " (lo que no cabe, al acopio)" : "");
            ga->crafting = -1;
        }
    }
}

void ga_after_player(GameActions *ga, Props *props, const Terrain *t, Player *p) {
    // Trepar: subir por la cuerda, pasar por encima del muro y bajar del otro lado.
    if (ga->climbing) {
        const float up = 1.6f, over = 0.6f, down = 0.4f;
        ga->climb_t += GetFrameTime();
        float c = ga->climb_t;
        if (c < up) p->pos = Vector3Lerp(ga->climb_from, ga->climb_top, c / up);
        else if (c < up + over) p->pos = Vector3Lerp(ga->climb_top, ga->climb_over, (c - up) / over);
        else if (c < up + over + down) p->pos = Vector3Lerp(ga->climb_over, ga->climb_to, (c - up - over) / down);
        else {
            p->pos = ga->climb_to;
            ga->climbing = false;
        }
        p->vy = 0.0f;
        p->grounded = !ga->climbing;
    }
    // Montado: el animal va debajo del jugador.
    if (ga->mounted >= 0) {
        Animal *a = &ga->animals[ga->mounted];
        a->x = p->pos.x;
        a->z = p->pos.z;
        a->yaw = p->yaw;
    }
    // Esconderse: acechando dentro de la hierba alta no se hace ruido.
    ga->hidden = false;
    if (p->sneaking && ga->mounted < 0) {
        for (int i = 0; i < props->count; i++)
            if (!strcmp(props->items[i].item->id, "mapa.vegetacion.hierba_alta") &&
                dist2d(p->pos, props->items[i].pos) < 1.3f) {
                ga->hidden = true;
                p->noise = 0.0f;
                break;
            }
    }
    (void)t;
}

// ---------------------------------------------------------------- dia nuevo
void ga_new_day(GameActions *ga, Props *props, const Terrain *t, Troop *troop, MemoryMap *mem, float now, int day,
                char *log, size_t log_len) {
    // Comida y recoleccion.
    UpkeepReport up = economy_daily_upkeep(&ga->stock, troop);
    // Efectos de lo construido en el campamento.
    const char *ids[PROPS_MAX + 8];
    int n = 0;
    for (int i = 0; i < CAMP_YURTS; i++) ids[n++] = "estructura.vivienda.yurta_comun";
    for (int i = 0; i < props->count; i++)
        if (dist2d(props->items[i].pos, (Vector3){ 0, 0, 0 }) < CAMP_RADIUS) ids[n++] = props->items[i].item->id;
    CampEffects fx = camp_effects(ids, n);
    troop_adjust_morale(troop, fx.morale_per_day);
    troop->rebellion_scale = fx.rebellion_scale;
    if (fx.reveal_radius > 0.0f) // la torre deja ver los alrededores
        for (int i = 0; i < props->count; i++)
            if (!strcmp(props->items[i].item->id, "estructura.campamento.atalaya"))
                memmap_reveal(mem, props->items[i].pos.x, props->items[i].pos.z, fx.reveal_radius, 30.0f, now);
    // Trabajos de los NPCs: una fogata si no hay ninguna, una tienda si faltan techos.
    int active = troop_count_with_status(troop, STATUS_ACTIVE);
    float a = (float)day * 1.7f;
    if (count_props(props, "estructura.campamento.fogata") == 0)
        npc_assign_job(ga, troop, t, ACTION_PLACE_FIRE, (Vector3){ cosf(a) * 12.0f, 0, sinf(a) * 12.0f });
    if (fx.shelters < active)
        npc_assign_job(ga, troop, t, ACTION_PLACE_TENT, (Vector3){ cosf(a + 1.0f) * 16.0f, 0, sinf(a + 1.0f) * 16.0f });
    // Carisma de los amuletos: la tribu confia mas en quien los lleva.
    float charisma = ig_stat(ga, STAT_CHARISMA);
    if (charisma > 0.0f) troop_adjust_morale(troop, charisma * 10.0f);
    snprintf(log, log_len, "Día %d: comieron %d%s, recolectaron %d. Ánimo del campamento %+.0f.", day, up.eaten,
             up.hungry ? TextFormat(" (%d sin ración)", up.hungry) : "", up.gathered, fx.morale_per_day);
}

// ---------------------------------------------------------------- dibujo
static void draw_held(const InvItem *it, Vector3 local, bool shield) {
    if (!it) return;
    Vector3 size = shield ? (Vector3){ it->w, it->h, fmaxf(it->l, 0.06f) }
                          : (Vector3){ fmaxf(it->w, 0.04f), it->h, fmaxf(it->l, 0.04f) };
    Vector3 c = { local.x, local.y + size.y * 0.5f, local.z };
    DrawCubeV(c, size, (Color){ 170, 170, 175, 255 });
    DrawCubeWiresV(c, size, (Color){ 60, 55, 50, 255 });
}

static Color role_color(Role r) {
    switch (r) {
    case ROLE_HUNTER: return (Color){ 110, 140, 70, 255 };
    case ROLE_COOK: return (Color){ 200, 150, 80, 255 };
    case ROLE_SMITH: return (Color){ 90, 90, 100, 255 };
    case ROLE_HEALER: return (Color){ 230, 230, 220, 255 };
    case ROLE_SCOUT: return (Color){ 70, 110, 150, 255 };
    case ROLE_LIEUTENANT: return (Color){ 160, 50, 40, 255 };
    case ROLE_BUILDER: return (Color){ 150, 110, 60, 255 };
    default: return (Color){ 140, 120, 100, 255 };
    }
}

void ga_draw_world(GameActions *ga, Props *props, const Terrain *t, const Troop *troop, const Player *p, float time) {
    props_draw(props);
    for (int i = 0; i < ga->project_count; i++) {
        const BuildProject *bp = &ga->projects[i];
        const InvItem *it = inventory_find(ga->inv, build_def(bp->def)->produces);
        if (it) props_draw_item(props, it, ground_at(t, bp->x, bp->z), 0.0f, bp->progress);
    }

    // Integrantes de la tribu: capsulas del color de su funcion; los grandes guerreros, a su talla.
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        const Member *m = &troop->members[i];
        const Npc *n = &ga->npcs[i];
        if (m->status != STATUS_ACTIVE || n->member_id != m->id) continue;
        float s = m->champion >= 0 ? troop->champions[m->champion].stats.size : 1.0f;
        bool working = n->project >= 0 && n->project < ga->project_count &&
                       dist2d(n->pos, (Vector3){ ga->projects[n->project].x, 0, ga->projects[n->project].z }) < AT_SITE;
        float bob = working || (n->job >= 0 && n->job_timer > 0.0f) ? fabsf(sinf(time * 6.0f + (float)i)) * 0.15f : 0.0f;
        const InvItem *body = inventory_find(ga->inv, s >= 1.2f ? "personaje.base.cuerpo_gigante" : "personaje.base.cuerpo_comun");
        if (body && props_has_model(props, body)) { // modelo importado: con su animacion (assets/animaciones.tsv)
            HumanoidState hs = { .moving = n->moving, .grounded = true, .doing = -1, .building = -1 };
            hs.down = m->health.down;
            hs.hit = n->hurt_anim > 0.0f;
            hs.attacking = n->fight_anim > 0.0f ? 1 + n->combo % 3 : 0;
            hs.move = n->move_anim > 0.0f ? n->move : 0;
            hs.knocked = n->knock > 0.0f;
            hs.limping = health_speed_scale(&m->health) < 0.85f;
            if (working) hs.building = ga->projects[n->project].def;
            if (n->job >= 0 && !n->moving) hs.doing = n->job;
            props_draw_item_anim(props, body, n->pos, n->yaw - PI / 2.0f, anim_humanoid(&hs), time + (float)i);
            continue;
        }
        // Sin modelo: cuerpo articulado (src/sim/body.h), del color de su funcion y con su armadura.
        Vector3 base = { n->pos.x, n->pos.y + bob, n->pos.z };
        bool kicking = n->move_anim > 0.0f && (n->move == MOVE_KICK + 1 || n->move == MOVE_RUN_KICK + 1);
        BodyPoseParams bp = { .walk_phase = time * 9.0f + (float)i, .walk = n->moving ? 1.0f : 0.0f,
                              .attack = kicking ? 0.0f : n->fight_anim / 0.4f, .down = m->health.down || n->knock > 0.0f,
                              .scale = s, .kick = kicking ? n->move_anim / 0.45f : 0.0f,
                              .grab = n->move_anim > 0.0f && n->move == MOVE_GRAPPLE + 1 ? n->move_anim / 0.45f : 0.0f };
        if (working || (n->job >= 0 && n->job_timer > 0.0f)) bp.attack = 0.5f + 0.5f * sinf(time * 6.0f + (float)i);
        BodyPose pose;
        body_pose(&pose, &bp);
        BodyColors bc = { { 200, 160, 120, 255 }, role_color(m->role), { 84, 64, 46, 255 } };
        if (n->hurt_anim > 0.0f) bc.cloth = (Color){ 236, 226, 214, 255 };
        body_draw(&pose, base, n->yaw, bc, &m->armor);
        if (m->health.down) continue;
        if (m->champion >= 0) DrawSphere((Vector3){ base.x, base.y + 1.85f * s, base.z }, 0.08f, UI_GOLD);
    }

    // Lo que el jugador tiene en las manos.
    rlPushMatrix();
    rlTranslatef(p->pos.x, p->pos.y + p->draw_lift, p->pos.z);
    rlRotatef(p->yaw * RAD2DEG, 0, 1, 0);
    const Hands *h = &ga->hands;
    const InvItem *r = h->right.id[0] ? inventory_find(ga->inv, h->right.id) : NULL;
    const InvItem *l = h->left.id[0] ? inventory_find(ga->inv, h->left.id) : NULL;
    if (h->sheathed) {
        draw_held(r, (Vector3){ -0.32f, 0.35f, -0.1f }, false);
        draw_held(l, (Vector3){ 0.0f, 0.6f, -0.3f }, l && l->hands == INV_HANDS_SHIELD);
    } else if (h->right.kind == INV_HANDS_TWO) {
        draw_held(r, (Vector3){ 0.0f, 0.5f, 0.45f }, false);
    } else {
        draw_held(r, (Vector3){ -0.45f, 0.75f, 0.2f }, false);
        draw_held(l, (Vector3){ 0.5f, 0.7f, 0.3f }, l && l->hands == INV_HANDS_SHIELD);
        if (ga->torch_lit && l) {
            float flick = 0.8f + 0.2f * sinf(time * 17.0f);
            DrawSphere((Vector3){ 0.5f, 0.7f + l->h + 0.08f, 0.3f }, 0.11f * flick, (Color){ 250, 160, 50, 255 });
        }
    }
    if (h->carried[0]) {
        const InvItem *c = inventory_find(ga->inv, h->carried);
        if (c) {
            Vector3 size = { fmaxf(c->l, 0.1f), fmaxf(c->h, 0.1f), fmaxf(c->w, 0.1f) };
            DrawCubeV((Vector3){ 0, 1.15f, 0.45f }, size, (Color){ 190, 150, 100, 255 });
            DrawCubeWiresV((Vector3){ 0, 1.15f, 0.45f }, size, (Color){ 80, 60, 40, 255 });
        }
    }
    rlPopMatrix();
    // La cuerda de la trepa mientras se escala.
    if (ga->climbing) DrawLine3D(ga->climb_top, (Vector3){ p->pos.x, p->pos.y + 1.2f, p->pos.z }, (Color){ 140, 110, 70, 255 });
}

int ga_lights(const GameActions *ga, const Props *props, const Player *p, Vector3 *pos, float *radius, int max) {
    static const struct { const char *id; float radius, height; } emitters[] = {
        { "estructura.campamento.fogata", 9.0f, 0.4f },      { "estructura.campamento.hoguera", 16.0f, 0.8f },
        { "estructura.campamento.horno_cocina", 4.0f, 0.6f }, { "estructura.campamento.horno_bronce", 7.0f, 1.0f },
        { "estructura.campamento.horno_acero", 8.0f, 1.0f },
    };
    int n = 0;
    for (int i = 0; i < props->count && n < max && !ga->fires_out; i++) { // la lluvia apaga los fuegos
        const Prop *pr = &props->items[i];
        for (size_t e = 0; e < sizeof(emitters) / sizeof(emitters[0]); e++) {
            if (strcmp(pr->item->id, emitters[e].id) != 0) continue;
            pos[n] = (Vector3){ pr->pos.x, pr->pos.y + emitters[e].height, pr->pos.z };
            radius[n++] = emitters[e].radius;
            break;
        }
    }
    if (ga->torch_lit && !ga->hands.sheathed && n < max) {
        pos[n] = (Vector3){ p->pos.x, p->pos.y + p->draw_lift + 1.6f, p->pos.z };
        radius[n++] = 6.0f;
    }
    return n;
}

bool ga_draw_player(GameActions *ga, Props *props, const Player *p, float time) {
    const InvItem *body = inventory_find(ga->inv, "personaje.narrativo.protagonista");
    if (!body || !props_has_model(props, body)) return false;
    HumanoidState hs = {
        .moving = p->moving, .running = p->stance == STANCE_RUN, .sneaking = p->sneaking, .grounded = p->grounded,
        .hidden = ga->hidden, .climbing = ga->climbing, .climb_top = ga->climbing && ga->climb_t > 1.6f,
        .mounted = ga->mounted >= 0, .carrying = ga->hands.carried[0] != '\0', .sheathed = ga->hands.sheathed,
        .grip = hands_grip(&ga->hands), .doing = ga->doing, .building = -1,
        .down = ga->pl_down, .hit = ga->pl_hit, .attacking = ga->pl_attacking, .spear = ga->pl_spear,
        .blocking = ga->pl_blocking, .limping = ga->pl_limping, .ranged = ga->pl_ranged, .move = ga->pl_move,
        .knocked = ga->pl_knocked, .swapping = ga->swap_anim > 0.0f,
    };
    if (hs.mounted) hs.mount_speed = p->moving ? (p->stance == STANCE_RUN ? 8.5f : 4.0f) * ga_speed_scale(ga) : 0.0f;
    Vector3 pos = { p->pos.x, p->pos.y + p->draw_lift, p->pos.z };
    props_draw_item_anim(props, body, pos, p->yaw - PI / 2.0f, anim_humanoid(&hs), time);
    return true;
}

const char *ga_hands_text(const GameActions *ga) {
    static char buf[128];
    const Hands *h = &ga->hands;
    snprintf(buf, sizeof(buf), "Empuñe: %s%s%s%s%s%s", grip_name(hands_grip(h)), h->sheathed ? " (enfundado)" : "",
             ga->torch_lit ? ", antorcha" : "", h->carried[0] ? ", lleva algo" : "",
             ga->mounted >= 0 ? " · montado" : "", ga->hidden ? " · OCULTO" : "");
    return buf;
}

// Lista de materiales con lo que hay a mano (rojo si falta). Devuelve la altura usada.
static int draw_materials(const GameActions *ga, const Props *props, const Player *p, const Ingredient *mats, bool at_hand,
                          int x, int y, int w) {
    if (!mats[0].id) return 0;
    int dy = 0;
    ui_text(at_hand ? "Ingredientes (a mano):" : "Materiales (acopio):", x, y, 10, UI_BONE_DIM);
    dy += 12;
    for (const Ingredient *m = mats; m->id; m++) {
        int have = at_hand ? ig_count((GameActions *)ga, props, p, m->id) : stock_count(&ga->stock, m->id);
        dy += ui_text_wrapped(TextFormat("  %s: %d / %d", item_name(ga, m->id), have, m->count), x, y + dy, w, 10,
                              have >= m->count ? UI_BONE : UI_CARNELIAN);
    }
    return dy;
}

static const char *TAB_NAMES[4] = { "Acciones", "Obras", "Fabricar", "Reparar" };

static void draw_menu(const GameActions *ga, const Props *props, const Troop *troop, const Player *p, int width) {
    const int w = 500, h = 316, x0 = (width - w) / 2, y0 = 20, pad = UI_PANEL_INSET + 3;
    const int list_x = x0 + pad, list_w = 210, desc_x = list_x + list_w + 10, desc_w = w - 2 * pad - list_w - 10;
    ui_panel((Rectangle){ (float)x0, (float)y0, (float)w, (float)h }, UI_METAL_GOLD);
    // Pestañas.
    int tx = list_x;
    for (int t = 0; t < 4; t++) {
        int tw = MeasureText(TAB_NAMES[t], 10) + 16;
        bool sel = t == ga->menu_tab;
        DrawRectangle(tx, y0 + pad - 2, tw, 14, sel ? Fade(UI_TURQ_DARK, 0.95f) : Fade(UI_LEATHER_CRACK, 0.8f));
        DrawRectangleLines(tx, y0 + pad - 2, tw, 14, sel ? UI_GOLD : UI_GOLD_DARK);
        ui_text(TAB_NAMES[t], tx + 8, y0 + pad, 10, sel ? UI_GOLD_LIGHT : UI_BONE_DIM);
        tx += tw + 4;
    }
    ui_text("Izq/Der pestaña · Arriba/Abajo · Enter · Tab cerrar", x0 + w - pad - MeasureText("Izq/Der pestaña · Arriba/Abajo · Enter · Tab cerrar", 10),
            y0 + h - pad - 10, 10, UI_BONE_DIM);
    ui_divider(list_x, y0 + pad + 16, w - 2 * pad, UI_METAL_GOLD);
    RepairItem rep[24];
    int nrep = ga->menu_tab == 3 ? ig_repair_list((GameActions *)ga, props, p, rep, 24) : 0;
    int total = ga->menu_tab == 0 ? ACTION_COUNT : ga->menu_tab == 1 ? BUILD_COUNT : ga->menu_tab == 2 ? CRAFT_COUNT : nrep;
    int cur = ga->tab_cursor[ga->menu_tab];
    if (cur >= total) cur = total ? total - 1 : 0;
    // Lista con desplazamiento: el cursor siempre visible.
    int first = cur - MENU_ROWS / 2;
    if (first > total - MENU_ROWS) first = total - MENU_ROWS;
    if (first < 0) first = 0;
    int y = y0 + pad + 24;
    if (!total) ui_text(ga->menu_tab == 3 ? "Nada gastado a mano." : "(vacío)", list_x + 4, y, 10, UI_BONE_DIM);
    for (int i = first; i < total && i < first + MENU_ROWS; i++) {
        const char *name;
        Color col = UI_BONE_DIM;
        if (ga->menu_tab == 0) name = action_def((ActionId)i)->name;
        else if (ga->menu_tab == 1) name = build_def((BuildId)i)->name;
        else if (ga->menu_tab == 2) {
            const CraftDef *d = craft_def((CraftId)i);
            name = d->name;
            if (craft_by_hand(d)) col = UI_TURQ_LIGHT; // a mano, en cualquier sitio
        } else {
            name = TextFormat("%s %d%%", item_name(ga, rep[i].id), (int)(rep[i].cond * 100));
        }
        if (i == cur) DrawRectangle(list_x - 2, y - 1, list_w, 11, (Color){ 26, 110, 116, 200 });
        BeginScissorMode(list_x, y - 1, list_w - 4, 12);
        ui_text(name, list_x + 4, y, 10, i == cur ? UI_BONE : col);
        EndScissorMode();
        y += 11;
    }
    if (!total) return;
    int dy = y0 + pad + 24;
    switch (ga->menu_tab) {
    case 0: {
        const ActionDef *d = action_def((ActionId)cur);
        ui_text(d->name, desc_x, dy, 10, UI_GOLD_LIGHT);
        dy += 14;
        dy += ui_text_wrapped(d->desc, desc_x, dy, desc_w, 10, UI_BONE) + 6;
        ui_text(TextFormat("Duración: %.1f s", d->seconds), desc_x, dy, 10, UI_BONE_DIM);
        dy += 12;
        if (d->requires)
            dy += ui_text_wrapped(TextFormat("Requiere: %s", item_name(ga, d->requires)), desc_x, dy, desc_w, 10,
                                  has_item(ga, d->requires) ? UI_BONE_DIM : UI_CARNELIAN);
        if (d->target) {
            ui_text(TextFormat("Sobre: %s", d->target), desc_x, dy, 10, UI_BONE_DIM);
            dy += 12;
        }
        draw_materials(ga, props, p, d->mats, false, desc_x, dy + 4, desc_w);
        break;
    }
    case 1: {
        const BuildDef *d = build_def((BuildId)cur);
        CrewPlan plan = build_plan(d, troop, true);
        ui_text(d->name, desc_x, dy, 10, UI_GOLD_LIGHT);
        dy += 14;
        ui_text(TextFormat("Cuadrilla: %d a %d personas", d->min_workers, d->max_workers), desc_x, dy, 10, UI_BONE);
        dy += 12;
        if (d->required_role != ROLE_NONE) {
            ui_text(TextFormat("Requiere: %s", role_name(d->required_role)), desc_x, dy, 10, UI_BONE);
            dy += 12;
        }
        if (d->skilled_role != ROLE_NONE) {
            ui_text(TextFormat("Rinde el doble: %s", role_name(d->skilled_role)), desc_x, dy, 10, UI_BONE_DIM);
            dy += 12;
        }
        dy += draw_materials(ga, props, p, d->mats, false, desc_x, dy, desc_w) + 6;
        ui_divider(desc_x, dy, desc_w, UI_METAL_GOLD);
        dy += 6;
        if (plan.check == BUILD_READY) {
            ui_text(TextFormat("Tu tribu: %d trabajarían", plan.workers), desc_x, dy, 10, UI_TURQUOISE);
            dy += 12;
            ui_text(TextFormat("Tiempo estimado: %.0f s de juego", d->work / plan.rate), desc_x, dy, 10, UI_TURQUOISE);
        } else {
            ui_text_wrapped(TextFormat("No se puede: %s", build_check_text(d, plan.check)), desc_x, dy, desc_w, 10, UI_CARNELIAN);
        }
        ui_text("Se levanta 6 m delante de ti.", desc_x, y0 + h - pad - 24, 10, UI_BONE_DIM);
        break;
    }
    case 2: {
        const CraftDef *d = craft_def((CraftId)cur);
        float secs = craft_seconds(d, troop);
        ui_text(d->name, desc_x, dy, 10, UI_GOLD_LIGHT);
        dy += 14;
        if (d->building) {
            bool has_oven = count_props(props, d->building) > 0;
            dy += ui_text_wrapped(TextFormat("En: %s", item_name(ga, d->building)), desc_x, dy, desc_w, 10, has_oven ? UI_BONE : UI_CARNELIAN);
            ui_text(TextFormat("Artesano: %s", role_name(d->role)), desc_x, dy, 10, secs >= 0.0f ? UI_BONE : UI_CARNELIAN);
            dy += 12;
        } else {
            ui_text("A mano, en cualquier sitio.", desc_x, dy, 10, UI_TURQ_LIGHT);
            dy += 12;
        }
        const InvItem *out = inventory_find(ga->inv, d->produces);
        dy += ui_text_wrapped(TextFormat("Da: %d x %s", d->amount > 0 ? d->amount : 1, out ? out->name : d->produces), desc_x, dy,
                              desc_w, 10, UI_BONE);
        dy += draw_materials(ga, props, p, d->mats, true, desc_x, dy + 4, desc_w) + 10;
        if (secs >= 0.0f) ui_text(TextFormat("Tiempo: %.0f s de juego", secs), desc_x, dy, 10, UI_TURQUOISE);
        ui_text("Lo hecho va a la mochila (o al acopio).", desc_x, y0 + h - pad - 24, 10, UI_BONE_DIM);
        break;
    }
    default: {
        const RepairItem *r = &rep[cur];
        const RepairDef *d = repair_def(r->material);
        ui_text(item_name(ga, r->id), desc_x, dy, 10, UI_GOLD_LIGHT);
        dy += 14;
        ui_text(TextFormat("%s · estado %d %% -> %d %%", r->worn ? "Puesta" : "Guardada", (int)(r->cond * 100),
                           (int)(fminf(1.0f, r->cond + d->restore) * 100)),
                desc_x, dy, 10, UI_BONE);
        dy += 12;
        if (d->building) {
            ui_text(TextFormat("En: %s", item_name(ga, d->building)), desc_x, dy, 10, UI_BONE_DIM);
            dy += 12;
        }
        if (d->role != ROLE_NONE) {
            ui_text(TextFormat("Con: %s", role_name(d->role)), desc_x, dy, 10, UI_BONE_DIM);
            dy += 12;
        }
        dy += draw_materials(ga, props, p, d->mats, true, desc_x, dy + 4, desc_w) + 10;
        char why[128];
        bool ok = ig_can_repair((GameActions *)ga, props, p, troop, r, why, sizeof(why));
        ui_text_wrapped(ok ? "Enter: reparar." : why, desc_x, dy, desc_w, 10, ok ? UI_TURQUOISE : UI_CARNELIAN);
        break;
    }
    }
}

void ga_draw_hud(const GameActions *ga, const Props *props, const Troop *troop, const Player *p, int width, int height) {
    if (ga->doing >= 0) {
        const ActionDef *d = action_def((ActionId)ga->doing);
        int w = 160, x = (width - w) / 2, y = height - 64;
        ui_text_centered(TextFormat("%s...", d->name), width / 2, y - 12, 10, UI_BONE);
        ui_bar(x, y, w, ga->timer / d->seconds, UI_TURQUOISE, UI_METAL_GOLD);
    }
    int lines = ga->project_count + (ga->crafting >= 0);
    if (lines > 0 && !ga->stock_open) {
        int x = width - 236, y = 128, w = 230, h = 20 + 12 * lines;
        ui_panel((Rectangle){ (float)x, (float)y, (float)w, (float)h + UI_PANEL_INSET }, UI_METAL_SILVER);
        int ty = y + UI_PANEL_INSET;
        for (int i = 0; i < ga->project_count; i++, ty += 12) {
            const BuildProject *bp = &ga->projects[i];
            const BuildDef *d = build_def(bp->def);
            bool stalled = ga->crew_size[i] == 0;
            const char *crew = stalled ? "sin cuadrilla" : TextFormat("%d/%d", ga->crew_present[i], ga->crew_size[i]);
            ui_text(TextFormat("%s %d%% · %s", d->name, (int)(bp->progress * 100.0f), crew), x + UI_PANEL_INSET, ty,
                    10, stalled ? UI_CARNELIAN : UI_BONE);
        }
        if (ga->crafting >= 0)
            ui_text(TextFormat("Haciendo: %s %d%%", craft_def((CraftId)ga->crafting)->name,
                               (int)(100.0f * ga->craft_timer / ga->craft_total)),
                    x + UI_PANEL_INSET, ty, 10, UI_GOLD);
    }
    if (ga->menu_open) draw_menu(ga, props, troop, p, width);
}
