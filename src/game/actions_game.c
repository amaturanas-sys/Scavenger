#include "game/actions_game.h"

#include "game/combat_game.h"
#include "game/input.h"
#include "game/fauna_game.h"
#include "game/inventory_game.h"
#include "game/camp_game.h"
#include "game/gems_game.h"
#include "game/water_game.h"
#include "game/talents_game.h"

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
#include "world/hearth.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "sim/lang.h"

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
    return it ? T(it->name) : id;
}

static bool has_item(const GameActions *ga, const char *id) {
    for (size_t i = 0; i < sizeof(KIT) / sizeof(KIT[0]); i++)
        if (!strcmp(KIT[i], id)) return true;
    return stock_count(ga_stock_c(ga), id) > 0;
}

static const char *best_sable(const GameActions *ga) {
    if (stock_count(ga_stock_c(ga), "arma.corta.sable_acero") > 0) return "arma.corta.sable_acero";
    if (stock_count(ga_stock_c(ga), "arma.corta.sable_bronce") > 0) return "arma.corta.sable_bronce";
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

// ---------------------------------------------------------------- barra rapida
// La barra del HUD (src/game/hud_game.c) pide empuñaduras, objetos y acciones; se atienden en
// ga_update, cuando el jugador esta libre.
static int g_quick_grip = -1;          // empuñadura pedida (indice de PRESETS)
static char g_quick_wield[INV_ID_LEN]; // arma o escudo pedido por id
static int g_quick_action = -1;        // accion pedida
static bool g_quick_menu;              // abrir el menu de acciones
static bool g_quick_swap;              // pasar el arma a la otra mano (Mayus + numero)
static int g_target_req;               // Tab en combate: 1 el enemigo siguiente, -1 el anterior
static bool g_combat_near;             // hay enemigos a tiro de objetivo (lo pone src/game/combat_game.c)

static bool preset_available(const GameActions *ga, int k) {
    for (int h = 0; h < 2; h++)
        if (PRESETS[k][h] && !has_item(ga, PRESETS[k][h])) return false;
    return true;
}

int ga_grip_count(void) { return PRESET_COUNT; }

GaGrip ga_grip(const GameActions *ga, int k) {
    GaGrip g = { 0 };
    if (k < 0 || k >= PRESET_COUNT) return g;
    g.right = PRESETS[k][0], g.left = PRESETS[k][1];
    if (g.right && !strcmp(g.right, "arma.corta.sable")) g.right = best_sable(ga);
    g.available = preset_available(ga, k);
    g.active = ga->preset == k && !ga->hands.sheathed && !ga->torch_lit;
    return g;
}

bool ga_has_item(const GameActions *ga, const char *id) { return id && has_item(ga, id); }

void ga_quick_request_grip(int k) { g_quick_grip = k; }
void ga_quick_request_wield(const char *id) { snprintf(g_quick_wield, sizeof(g_quick_wield), "%s", id ? id : ""); }
void ga_quick_request_action(ActionId a) { g_quick_action = (int)a; }
void ga_quick_request_menu(void) { g_quick_menu = true; }
void ga_quick_request_swap(void) { g_quick_swap = true; }
void ga_set_combat_near(bool near) { g_combat_near = near; }
int ga_take_target_request(void) {
    int r = g_target_req;
    g_target_req = 0;
    return r;
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
    camps_init(ga->camps, CAMPS_MAX);
    camp_found(ga->camps, CAMPS_MAX, 0.0f, 0.0f, 1); // el campamento con el que empieza la tribu
    snprintf(ga->camps[0].name, CAMP_NAME_LEN, "%s", T("Campamento de las estelas"));
    stock_seed_camp(&ga->camps[0].stock);
    // Ropa de repuesto en el acopio: la gente se cambia segun el tiempo (src/game/apparel_game.c).
    static const struct { const char *id; int n; } WARDROBE[] = {
        { "vestimenta.espalda.capa", 3 }, { "vestimenta.torso.deel_invierno", 2 }, { "vestimenta.espalda.abrigo_cabra", 2 },
        { "vestimenta.cabeza.gorro_piel", 3 }, { "vestimenta.cabeza.sombrero", 2 }, { "vestimenta.torso.tunica_seda", 1 },
        { "vestimenta.cuello.panuelo_desierto", 2 }, { "vestimenta.pies.sandalias", 2 }, { "vestimenta.pies.botas_piel", 1 },
    };
    for (size_t i = 0; i < sizeof(WARDROBE) / sizeof(WARDROBE[0]); i++) stock_add(&ga->camps[0].stock, WARDROBE[i].id, WARDROBE[i].n);
    // Lo que lleva puesto el jugador al empezar.
    hydration_init(&ga->hydro);
    outfit_clear(&ga->outfit);
    static const char *const START_WEAR[] = { "vestimenta.torso.deel", "vestimenta.cabeza.gorro_piel", "vestimenta.pies.botas_fieltro",
                                              "vestimenta.espalda.capa" };
    for (size_t i = 0; i < sizeof(START_WEAR) / sizeof(START_WEAR[0]); i++) outfit_wear(&ga->outfit, garment_find(START_WEAR[i]));
    ga->here = 0;
    ga->burn_camp = ga->burn_next = -1;
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
    gems_scatter(ga, props, t); // rocas que romper y corales en los lagos

    // Animales: el ganado del campamento y manadas en los alrededores.
    fg_init(ga, t);
    // Lo que se lleva encima, la carreta y la armeria (src/game/inventory_game.c).
    ig_init(ga, props, t);
}

bool ga_menu_open(const GameActions *ga) { return ga->menu_open; }

Stockpile *ga_stock_at(GameActions *ga, float x, float z) {
    static Stockpile none; // sin campamentos: un acopio vacio (lo que se deje ahi se pierde)
    int k = camp_at(ga->camps, CAMPS_MAX, x, z);
    if (k < 0) k = camp_nearest(ga->camps, CAMPS_MAX, x, z, NULL);
    if (k < 0) {
        stock_init(&none);
        return &none;
    }
    return &ga->camps[k].stock;
}

Stockpile *ga_stock(GameActions *ga) {
    if (ga->here >= 0 && ga->camps[ga->here].used) return &ga->camps[ga->here].stock;
    return ga_stock_at(ga, ga->player_pos.x, ga->player_pos.z);
}

const Stockpile *ga_stock_c(const GameActions *ga) { return ga_stock((GameActions *)ga); }
bool ga_blocks_input(const GameActions *ga) { return ga->menu_open || ga->climbing || ga->inv_open || ga->equip_open || ga->dlg.open; }

float ga_speed_scale(const GameActions *ga) {
    // Montado, la montura (y el amuleto del caballo); a pie, cuanto pesa lo que llevas.
    if (ga->mounted >= 0)
        return species_def(ga->animals[ga->mounted].species)->ride_speed * (1.0f + 0.5f * ig_stat(ga, STAT_RIDING));
    return ig_speed_scale(ga) * (1.0f + ig_stat(ga, STAT_SPEED)) * wg_speed_scale(ga); // a pie: la carga, tatuajes y joyas, la sed
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
        snprintf(log, len, T("Te falta: %s."), item_name(ga, d->requires));
        return;
    }
    const Ingredient *miss = stock_first_missing(ga_stock(ga), d->mats);
    if (miss) {
        snprintf(log, len, T("Falta en el acopio: %s (%d de %d)."), item_name(ga, miss->id),
                 stock_count(ga_stock_c(ga), miss->id), miss->count);
        return;
    }
    ga->target = -1;
    switch (a) {
    case ACTION_TAKE: {
        if (ig_take_loot(ga, props, p, log, len)) return; // una bolsa de botin cerca: al momento
        if (gems_coral(ga, (Props *)props, p, log, len)) return; // un arrecife de coral: al momento
        if (props_nearest(props, p->pos, REACH, true) < 0 && fg_cut_grass(ga, (Props *)props, p, log, len)) return; // forraje
        int near = props_nearest(props, p->pos, REACH, true);
        ga->take_all = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (near >= 0 && goes_to_bags(props->items[near].item)) break; // a la mochila: no hace falta mano libre
        if (ga->hands.carried[0]) {
            snprintf(log, len, "%s", T("Ya llevas algo: T para lanzarlo."));
            return;
        }
        if (!hands_can_take(&ga->hands)) {
            snprintf(log, len, "%s", T("Necesitas una mano libre (enfunda con la tecla de su casilla, 1 a 9)."));
            return;
        }
        if (near < 0) {
            snprintf(log, len, "%s", T("No hay nada que tomar al alcance."));
            return;
        }
        break;
    }
    case ACTION_THROW:
        if (!ga->hands.carried[0]) {
            snprintf(log, len, "%s", T("No llevas nada para lanzar (F para tomar)."));
            return;
        }
        break;
    case ACTION_SHEATHE:
        if (hands_grip(&ga->hands) == GRIP_EMPTY) {
            snprintf(log, len, "%s", T("No tienes armas que enfundar."));
            return;
        }
        break;
    case ACTION_THROW_GRAPPLE:
        if (ga->mounted >= 0) {
            snprintf(log, len, "%s", T("Desmonta antes de trepar (R)."));
            return;
        }
        if ((ga->target = wall_ahead(props, p)) < 0) {
            snprintf(log, len, "%s", T("No hay un muro a tiro de trepa delante."));
            return;
        }
        break;
    case ACTION_THROW_LASSO:
        if ((ga->target = animal_ahead(ga, p, ANIMAL_WILD, TARGET_RANGE, true)) < 0) {
            snprintf(log, len, "%s", T("No hay un animal salvaje a tiro de lazo delante."));
            return;
        }
        break;
    case ACTION_PAN:
        if (!ga->terrain || !gems_water_ahead(ga->terrain, p)) {
            snprintf(log, len, "%s", T("Para cribar hay que estar en la orilla, mirando al agua."));
            return;
        }
        break;
    case ACTION_FILL_WATER:
        if (!ga->terrain || (!gems_water_ahead(ga->terrain, p) && !wg_at_well(ga->terrain, p->pos.x, p->pos.z))) {
            snprintf(log, len, "%s", T("Para llenar el odre hay que estar en la orilla, mirando al agua, o junto a un pozo."));
            return;
        }
        if (wg_water_room(ga, props, p) <= 0) {
            snprintf(log, len, "%s", T("Tus odres ya están llenos."));
            return;
        }
        break;
    case ACTION_BOIL:
        if (!ga->fire_near || ga->fires_out) {
            snprintf(log, len, "%s", T("Para hervir el agua hace falta un fuego encendido cerca."));
            return;
        }
        if (ig_count(ga, props, p, "utileria.consumible.agua") <= 0) {
            snprintf(log, len, "%s", T("No tienes agua cruda que hervir (llena el odre en la orilla)."));
            return;
        }
        break;
    case ACTION_SADDLE:
        if ((ga->target = animal_ahead(ga, p, ANIMAL_TAMED, SADDLE_RANGE, false)) < 0) {
            snprintf(log, len, "%s", T("No hay un animal domado cerca para ensillar."));
            return;
        }
        if (!species_def(ga->animals[ga->target].species)->rideable) {
            snprintf(log, len, T("Un %s no se puede montar."), T(species_def(ga->animals[ga->target].species)->name));
            return;
        }
        break;
    default: break;
    }
    stock_take_all(ga_stock(ga), d->mats);
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
            snprintf(log, len, kept ? T("Recoges %d hierbas curativas.") : T("No te caben más hierbas."), kept);
            if (kept) props_remove(props, i);
        } else if (goes_to_bags(it)) {
            // A la mochila (o donde quepa); con Mayús, todo lo que haya alrededor.
            int taken = 0, left = 0;
            const char *last = T(it->name);
            for (int k = props->count - 1; k >= 0; k--) {
                const Prop *pr = &props->items[k];
                if (k != i && (!ga->take_all || pr->flying || !goes_to_bags(pr->item) ||
                               Vector3Distance(pr->pos, p->pos) > REACH + 1.5f ||
                               !strcmp(pr->item->id, "mapa.vegetacion.mata_hierbas")))
                    continue;
                if (ig_store(ga, props, p, pr->item->id, 1, pr->condition) == 1) {
                    last = T(pr->item->name);
                    taken++;
                    props_remove(props, k);
                    if (k < i) i--;
                } else {
                    left++;
                }
            }
            if (taken > 1) snprintf(log, len, T("Tomas %d cosas%s."), taken, left ? TextFormat(T(" (%d no caben)"), left) : "");
            else if (taken) snprintf(log, len, T("Tomaste: %s."), last);
            else if (is_resource(it->id) && !ga->hands.carried[0] && hands_take(&ga->hands, it->id)) { // demasiado grande: en brazos
                snprintf(log, len, T("No cabe en la mochila: llevas %s en brazos (T para soltarlo)."), T(it->name));
                props_remove(props, i);
            } else snprintf(log, len, T("No te cabe: %s."), T(it->name));
        } else if (hands_take(&ga->hands, it->id)) {
            snprintf(log, len, T("Tomaste: %s (T para lanzarlo)."), T(it->name));
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
        snprintf(log, len, T("Lanzaste: %s."), item_name(ga, id));
        break;
    }
    case ACTION_CHANGE_GRIP:
        ga->preset = (ga->preset + 1) % PRESET_COUNT;
        apply_preset(ga);
        snprintf(log, len, T("Empuñadura: %s."), grip_name(hands_grip(&ga->hands)));
        break;
    case ACTION_SHEATHE:
        hands_toggle_sheathe(&ga->hands);
        if (ga->hands.sheathed) ga->torch_lit = false;
        snprintf(log, len, "%s", ga->hands.sheathed ? T("Enfundaste las armas: manos libres.") : T("Desenfundaste."));
        break;
    case ACTION_LIGHT_TORCH:
        hands_equip(&ga->hands, inventory_find(ga->inv, d->requires), HAND_LEFT);
        ga->torch_lit = true;
        snprintf(log, len, "%s", T("Encendiste la antorcha (mano izquierda)."));
        break;
    case ACTION_THROW_GRAPPLE:
        if (ga->target >= 0 && ga->target < props->count) {
            start_climb(ga, props, t, p, ga->target);
            snprintf(log, len, "%s", T("La trepa se engancha: ¡a escalar!"));
        }
        break;
    case ACTION_THROW_LASSO: {
        Animal *an = &ga->animals[ga->target];
        if (!an->used) break;
        const SpeciesDef *sd = species_def(an->species);
        char who[48];
        snprintf(who, sizeof(who), "%s", T(sd->name));
        if (who[0] >= 'A' && who[0] <= 'Z') who[0] = (char)(who[0] - 'A' + 'a');
        switch (animal_lasso(an, &ga->rng, ga->mounted >= 0 ? 0.1f : 0.0f, 0.0f, 0.0f)) {
        case TAME_OK:
            if (an->state == ANIMAL_BOUND)
                snprintf(log, len, T("¡Atrapaste al %s con el lazo! Dale carne (F) antes de que se suelte."), who);
            else {
                snprintf(log, len, T("¡Domaste: %s! Ahora sigue a la tribu%s."), who, sd->rideable ? T(" (silla para montarlo)") : "");
                tg_xp(ga, XP_TAME, log, len);
            }
            break;
        case TAME_TOO_STRONG: snprintf(log, len, T("El %s está demasiado entero: debilítalo peleando antes del lazo."), who); break;
        case TAME_NEVER: snprintf(log, len, T("Un animal así no se doma (%s): solo se caza."), who); break;
        case TAME_ALREADY: snprintf(log, len, "%s", T("Ya es de la tribu.")); break;
        default: snprintf(log, len, T("El %s se zafó del lazo."), who); break;
        }
        break;
    }
    case ACTION_SADDLE:
        if (animal_saddle(&ga->animals[ga->target]))
            snprintf(log, len, "%s", T("Montura instalada: R para montar."));
        break;
    case ACTION_PAN: gems_pan(ga, props, p, log, len); break;
    case ACTION_FILL_WATER: {
        if (ga->terrain && wg_at_well(ga->terrain, p->pos.x, p->pos.z)) { // el pozo: agua limpia, como hervida
            int n = ig_store(ga, props, p, "utileria.consumible.agua_hervida", wg_water_room(ga, props, p), 1.0f);
            snprintf(log, len, T("Sacas agua del pozo: +%d agua limpia (no hace falta hervirla)."), n);
            break;
        }
        int n = ig_store(ga, props, p, "utileria.consumible.agua", wg_water_room(ga, props, p), 1.0f);
        snprintf(log, len, T("Llenas el odre: +%d agua cruda. Hiérvela junto a un fuego antes de beberla (o bebe con riesgo)."), n);
        break;
    }
    case ACTION_BOIL: {
        int n = ig_count(ga, props, p, "utileria.consumible.agua");
        if (n > 4) n = 4; // una olla
        ig_use(ga, props, p, "utileria.consumible.agua", n);
        int kept = ig_store(ga, props, p, "utileria.consumible.agua_hervida", n, 1.0f);
        snprintf(log, len, T("Hierves el agua: +%d agua hervida, sin espíritus malditos."), kept);
        break;
    }
    default:
        if (d->produces) { // instalar fogata, tienda, cavar trinchera
            Vector3 at = ground_ahead(t, p, 2.5f);
            props_add(props, d->produces, at, p->yaw);
            snprintf(log, len, T("Hecho: %s."), item_name(ga, d->produces));
            if (a == ACTION_PLACE_TENT) cg_basic_structure(ga, at.x, at.z); // lejos de todo: un campamento nuevo
        }
        if (a == ACTION_DIG_TRENCH) gems_dig(ga, props, p, log, len); // a veces sale algo de la tierra
        break;
    }
}

static void start_build(GameActions *ga, BuildId b, const Terrain *t, const Player *p, char *log, size_t len) {
    const BuildDef *d = build_def(b);
    if (ga->project_count >= GA_MAX_PROJECTS) {
        snprintf(log, len, "%s", T("Demasiadas obras a la vez."));
        return;
    }
    const Ingredient *miss = stock_first_missing(ga_stock(ga), d->mats);
    if (miss) {
        snprintf(log, len, T("Falta en el acopio: %s (%d de %d)."), item_name(ga, miss->id),
                 stock_count(ga_stock_c(ga), miss->id), miss->count);
        return;
    }
    stock_take_all(ga_stock(ga), d->mats);
    Vector3 at = ground_ahead(t, p, 6.0f);
    ga->projects[ga->project_count++] = (BuildProject){ b, at.x, at.z, 0.0f, false };
    snprintf(log, len, T("Obra iniciada: %s. La cuadrilla va en camino."), T(d->name));
}

bool ga_order_build(GameActions *ga, BuildId b, int camp, const Props *props, char *log, size_t len) {
    const BuildDef *d = build_def(b);
    if (camp < 0 || camp >= CAMPS_MAX || !ga->camps[camp].used) return false;
    if (ga->project_count >= GA_MAX_PROJECTS) {
        snprintf(log, len, "%s", T("Demasiadas obras a la vez."));
        return false;
    }
    Stockpile *st = &ga->camps[camp].stock;
    const Ingredient *miss = stock_first_missing(st, d->mats);
    if (miss) {
        snprintf(log, len, T("Falta en el acopio: %s (%d de %d)."), item_name(ga, miss->id), stock_count(st, miss->id), miss->count);
        return false;
    }
    // Un sitio libre en el campamento: en anillos alrededor del fuego.
    const CampSite *c = &ga->camps[camp];
    float bx = c->x + 14.0f, bz = c->z;
    for (int tries = 0; tries < 48; tries++) {
        float ang = (float)tries * 2.39996f, r = 12.0f + (float)(tries / 8) * 4.0f;
        float x = c->x + cosf(ang) * r, z = c->z + sinf(ang) * r;
        bool free = true;
        for (int i = 0; i < props->count && free; i++)
            if (dist2d(props->items[i].pos, (Vector3){ x, 0, z }) < 5.0f) free = false;
        for (int i = 0; i < ga->project_count && free; i++)
            if (dist2d((Vector3){ ga->projects[i].x, 0, ga->projects[i].z }, (Vector3){ x, 0, z }) < 6.0f) free = false;
        if (free) {
            bx = x, bz = z;
            break;
        }
    }
    stock_take_all(st, d->mats);
    ga->projects[ga->project_count++] = (BuildProject){ b, bx, bz, 0.0f, false };
    snprintf(log, len, T("Obra iniciada: %s. La cuadrilla va en camino."), T(d->name));
    return true;
}

static void start_craft(GameActions *ga, CraftId c, const Props *props, const Player *p, const Troop *troop, char *log,
                        size_t len) {
    const CraftDef *d = craft_def(c);
    if (ga->crafting >= 0) {
        snprintf(log, len, T("Ya estás fabricando algo: %s."), T(craft_def((CraftId)ga->crafting)->name));
        return;
    }
    if (d->building && count_props(props, d->building) == 0) {
        snprintf(log, len, T("Hace falta: %s."), item_name(ga, d->building));
        return;
    }
    float secs = craft_seconds(d, troop);
    if (secs < 0.0f) {
        snprintf(log, len, T("Hace falta un %s en la tribu."), T(role_name(d->role)));
        return;
    }
    // Los ingredientes: lo que llevas encima, lo que tienes cerca o el acopio (en el campamento).
    const Ingredient *miss = ig_first_missing(ga, props, p, d->mats);
    if (miss) {
        snprintf(log, len, T("Falta: %s (%d de %d)."), item_name(ga, miss->id), ig_count(ga, props, p, miss->id), miss->count);
        return;
    }
    ig_use_all(ga, props, p, d->mats);
    ga->crafting = c;
    ga->craft_timer = 0.0f;
    ga->craft_total = secs / (1.0f + fmaxf(0.0f, ig_stat(ga, STAT_CRAFT))); // las manos del clan
    snprintf(log, len, craft_by_hand(d) ? T("Fabricando: %s.") : T("Forjando: %s."), T(d->name));
}

// ---------------------------------------------------------------- NPCs
// Mantiene un NPC por integrante activo de la tropa (los nuevos aparecen junto al fuego).
static void sync_npcs(GameActions *ga, const Troop *troop, const Terrain *t) {
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        Npc *n = &ga->npcs[i];
        const Member *m = &troop->members[i];
        bool fresh = n->member_id != m->id;
        if (fresh) {
            memset(n, 0, sizeof(*n));
            n->member_id = m->id;
            n->project = n->job = -1;
        }
        // La casa: en espiral dorada alrededor del fuego de su campamento.
        int camp = m->camp >= 0 && m->camp < CAMPS_MAX && ga->camps[m->camp].used ? m->camp : -1;
        if (fresh || n->home_camp != camp + 1) {
            const CampSite *c = camp >= 0 ? &ga->camps[camp] : NULL;
            float a = (float)i * 2.39996f, r = 5.5f + (float)(i % 3) * 1.5f;
            float cx = c ? c->x : n->pos.x, cz = c ? c->z : n->pos.z;
            n->home = ground_at(t, cx + cosf(a) * r, cz + sinf(a) * r);
            if (fresh) n->pos = n->home;
            n->home_camp = camp + 1;
        }
    }
}

static bool npc_walk(Npc *n, const Terrain *t, Vector3 to, float dt) {
    float d = dist2d(n->pos, to);
    n->moving = d >= 0.3f;
    if (d < 0.3f) return true;
    float step = fminf(NPC_SPEED * dt, d);
    float dx = (to.x - n->pos.x) / d, dz = (to.z - n->pos.z) / d;
    float x0 = n->pos.x, z0 = n->pos.z;
    n->pos.x += dx * step;
    n->pos.z += dz * step;
    terrain_dry_step(t, x0, z0, &n->pos.x, &n->pos.z); // rodea lagos y rios hondos
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
    if (!(d->actors & ACTOR_NPC) || !stock_has_all(ga_stock(ga), d->mats)) return false;
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        Npc *n = &ga->npcs[i];
        if (troop->members[i].status != STATUS_ACTIVE || n->project >= 0 || n->job >= 0 || n->escort) continue;
        stock_take_all(ga_stock(ga), d->mats);
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
        if (m->status != STATUS_ACTIVE || n->escort || m->journey > 0) continue; // de viaje: lo mueve src/game/travel_game.c
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
                    snprintf(log, len, T("%s terminó: %s."), m->name, T(d->name));
                    n->job = -1;
                }
            }
        } else {
            npc_walk(n, t, n->home, dt);
        }
    }
}

// ---------------------------------------------------------------- menu Tab: disposicion
// Lo comparten el dibujo y el raton. Pantalla virtual de 640x360.
#define TAB_W 520
#define TAB_H 312
#define TAB_X ((640 - TAB_W) / 2)
#define TAB_Y 14
#define TAB_PAD (UI_PANEL_INSET + 3)
#define GRID_COLS 6
#define GRID_ROWS 6
#define TILE 36
#define TILE_GAP 4

static const IconId TAB_ICONS[4] = { ICON_ACCIONES, ICON_OBRAS, ICON_FABRICAR, ICON_REPARAR };
static const IconId ACTION_ICONS[ACTION_COUNT] = { ICON_TOMAR,   ICON_LANZAR,   ICON_EMPUNAR, ICON_ENFUNDAR,
                                                   ICON_FOGATA,  ICON_TIENDA,   ICON_TRINCHERA, ICON_TREPA,
                                                   ICON_LAZO,    ICON_ANTORCHA, ICON_ENSILLAR, ICON_GEMA,
                                                   ICON_AGUA,    ICON_HERVIR };
static const IconId BUILD_ICONS[BUILD_COUNT] = { ICON_REFUGIO, ICON_OBRAS, ICON_MURO_PIEDRA, ICON_HOGUERA, ICON_TOTEM,
                                                 ICON_HORNO,   ICON_FUNDICION, ICON_FUNDICION, ICON_TORRE, ICON_CORRAL };

static Rectangle tab_rect(int t) {
    return (Rectangle){ (float)(TAB_X + TAB_PAD + t * 32), (float)(TAB_Y + TAB_PAD), 28, 28 };
}

static int grid_first_row(int cur) {
    int row = cur / GRID_COLS;
    return row >= GRID_ROWS ? row - GRID_ROWS + 1 : 0; // el elegido siempre a la vista
}

static Rectangle grid_rect(int i, int first_row) {
    int c = i % GRID_COLS, r = i / GRID_COLS - first_row;
    return (Rectangle){ (float)(TAB_X + TAB_PAD + c * (TILE + TILE_GAP)), (float)(TAB_Y + TAB_PAD + 38 + r * (TILE + TILE_GAP)),
                        TILE, TILE };
}

// ---------------------------------------------------------------- actualizacion
// Un horno o una fundicion de la tribu a mano (para fabricar).
static bool oven_near(const Props *props, const Player *p) {
    for (int i = 0; i < props->count; i++)
        if (strstr(props->items[i].item->id, "estructura.campamento.horno") && dist2d(p->pos, props->items[i].pos) < 3.0f) return true;
    return false;
}

// F: interactuar con lo que haya. Hablar con la gente (el guardian, el druida, el orfebre); un
// enemigo abatido (lo toma prisionero, src/game/combat_game.c); los animales y las colmenas
// (src/game/fauna_game.c; Mayus+F, quieto junto al ganado, lo sacrifica); tomar objetos y botin
// (Mayus: todo lo de alrededor); el pozo (sacar agua) y el horno (fabricar).
static void interact(GameActions *ga, Troop *troop, Props *props, const Terrain *t, Player *p, char *log, size_t len) {
    if (cg_try_talk(ga, troop, p) || tg_try_talk(ga, troop, p, log, len)) return;
    int near = props_nearest(props, p->pos, REACH, true);
    float prop_d = near >= 0 ? dist2d(p->pos, props->items[near].pos) : 1e9f;
    float animal_d = ga->mounted < 0 ? fg_interact_dist(ga, p) : 1e9f;
    float downed_d = ga->mounted < 0 ? cb_capture_dist() : 1e9f;
    if (downed_d <= CB_DOWNED_REACH && downed_d <= prop_d && downed_d <= animal_d) { // un abatido: prisionero
        cb_request_capture();
        return;
    }
    if (animal_d < 1e8f && animal_d <= prop_d) { // el animal esta mas cerca que lo que hay para tomar
        fg_interact(ga, p, (input_mods() & KM_SHIFT) && !p->moving, log, len);
        return;
    }
    if (near < 0 && t && wg_at_well(t, p->pos.x, p->pos.z)) {
        start_action(ga, ACTION_FILL_WATER, props, p, log, len);
        return;
    }
    if (near < 0 && oven_near(props, p)) {
        ga->menu_open = true;
        ga->menu_tab = 2;
        snprintf(log, len, "%s", T("El horno: elige qué fabricar."));
        return;
    }
    start_action(ga, ACTION_TAKE, props, p, log, len);
}

void ga_update(GameActions *ga, Props *props, const Terrain *t, Player *p, Troop *troop, float dt, char *log,
               size_t log_len) {
    ga->terrain = t;
    ga->troop_ref = troop;
    ga->here = camp_at(ga->camps, CAMPS_MAX, p->pos.x, p->pos.z);
    ga->player_pos = p->pos; // fuera de todo campamento: el acopio del mas cercano
    sync_npcs(ga, troop, t);
    ga->swap_anim = fmaxf(0.0f, ga->swap_anim - dt);
    bool other_menu = ga->inv_open || ga->equip_open || ga->dlg.open; // inventario, equipo o un dialogo
    // Tab: en combate elige el objetivo (con Mayus, el anterior); si no, el menu de acciones.
    bool tab = input_action_pressed(KA_MENU);
    if (tab && g_combat_near && !ga->menu_open && !other_menu) g_target_req = input_mods() & KM_SHIFT ? -1 : 1;
    else if ((tab || g_quick_menu) && !other_menu) ga->menu_open = !ga->menu_open;
    g_quick_menu = false;
    // Lo que pide la barra rapida del HUD (teclas 1..9 o toques).
    if (ga->doing < 0 && !ga->climbing && !other_menu && !ga->menu_open) {
        if (g_quick_grip >= 0 && g_quick_grip < PRESET_COUNT) {
            int k = g_quick_grip;
            GaGrip g = ga_grip(ga, k);
            if (!g.available) snprintf(log, log_len, T("No tienes %s."), item_name(ga, g.right ? g.right : g.left));
            else if (g.active) start_action(ga, ACTION_SHEATHE, props, p, log, log_len); // otra vez: enfunda
            else {
                if (ga->hands.sheathed) hands_toggle_sheathe(&ga->hands);
                ga->preset = (k + PRESET_COUNT - 1) % PRESET_COUNT; // CHANGE_GRIP pasa a la siguiente: la pedida
                start_action(ga, ACTION_CHANGE_GRIP, props, p, log, log_len);
            }
        }
        if (g_quick_wield[0]) { // un arma o un escudo suelto: a su mano
            const InvItem *it = inventory_find(ga->inv, g_quick_wield);
            if (!it || it->hands == INV_HANDS_NONE) snprintf(log, log_len, T("%s no se empuña."), item_name(ga, g_quick_wield));
            else {
                if (ga->hands.sheathed) hands_toggle_sheathe(&ga->hands);
                if (it->hands != INV_HANDS_SHIELD) ga->torch_lit = false;
                hands_equip(&ga->hands, it, it->hands == INV_HANDS_SHIELD ? HAND_LEFT : HAND_RIGHT);
                ga->swap_anim = 0.5f;
                snprintf(log, log_len, T("Empuñas: %s."), item_name(ga, g_quick_wield));
            }
        }
        if (g_quick_action >= 0) start_action(ga, (ActionId)g_quick_action, props, p, log, log_len);
        if (g_quick_swap) { // Mayus + numero: el arma empuñada, a la otra mano
            snprintf(log, log_len, hands_swap(&ga->hands) ? T("Pasas el arma a la otra mano: %s.")
                                                         : T("No se puede: el escudo va en el brazo izquierdo y las armas a dos manos, en las dos (%s)."),
                     grip_name(hands_grip(&ga->hands)));
            ga->swap_anim = 0.5f;
        }
    }
    g_quick_grip = g_quick_action = -1;
    g_quick_wield[0] = '\0';
    g_quick_swap = false;
    if (ga->menu_open) {
        // Pestañas (acciones, obras, fabricar, reparar): Q/E, Re Pág/Av Pág o un clic en su icono.
        if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_PAGE_DOWN)) ga->menu_tab = (ga->menu_tab + 1) % 4;
        if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_PAGE_UP)) ga->menu_tab = (ga->menu_tab + 3) % 4;
        for (int tb = 0; tb < 4; tb++)
            if (ui_click(tab_rect(tb))) ga->menu_tab = tb;
        RepairItem rep[24];
        int nrep = ga->menu_tab == 3 ? ig_repair_list(ga, props, p, rep, 24) : 0;
        int total = ga->menu_tab == 0 ? ACTION_COUNT : ga->menu_tab == 1 ? BUILD_COUNT : ga->menu_tab == 2 ? CRAFT_COUNT : nrep;
        int *cur = &ga->tab_cursor[ga->menu_tab];
        bool activate = IsKeyPressed(KEY_ENTER);
        if (total > 0) { // la cuadricula: flechas (o WASD) y el raton
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) *cur = (*cur + 1) % total;
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) *cur = (*cur + total - 1) % total;
            if ((IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) && *cur + GRID_COLS < total) *cur += GRID_COLS;
            if ((IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) && *cur >= GRID_COLS) *cur -= GRID_COLS;
            if (*cur >= total) *cur = total - 1;
            int first = grid_first_row(*cur);
            for (int i = first * GRID_COLS; i < total && i < (first + GRID_ROWS) * GRID_COLS; i++) {
                Rectangle r = grid_rect(i, first);
                if (ui_pointer_moved() && ui_hover(r)) *cur = i;
                if (ui_click(r)) { // un clic elige; otro sobre el elegido, lo hace
                    if (*cur == i) activate = true;
                    *cur = i;
                }
            }
        }
        if (activate && total > 0) {
            if (ga->menu_tab != 3) ga->menu_open = false;
            switch (ga->menu_tab) {
            case 0: start_action(ga, (ActionId)*cur, props, p, log, log_len); break;
            case 1: start_build(ga, (BuildId)*cur, t, p, log, log_len); break;
            case 2: start_craft(ga, (CraftId)*cur, props, p, troop, log, log_len); break;
            default: ig_repair(ga, props, p, troop, &rep[*cur], log, log_len); break;
            }
        }
    } else if (ga->doing < 0 && !ga->climbing && !other_menu) {
        if (input_action_pressed(KA_INTERACT)) interact(ga, troop, props, t, p, log, log_len);
        if (input_action_pressed(KA_THROW)) start_action(ga, ACTION_THROW, props, p, log, log_len);
        if (input_action_pressed(KA_MOUNT)) { // montar / desmontar
            if (ga->mounted >= 0) {
                ga->animals[ga->mounted].ridden = false;
                ga->mounted = -1;
                snprintf(log, log_len, "%s", T("Desmontaste."));
            } else {
                int a = -1;
                for (int i = 0; i < ga->animal_count; i++)
                    if (animal_can_ride(&ga->animals[i]) && !ga->animals[i].h.down &&
                        dist2d(p->pos, (Vector3){ ga->animals[i].x, 0, ga->animals[i].z }) < SADDLE_RANGE)
                        a = i;
                if (a >= 0) {
                    ga->mounted = a;
                    ga->animals[a].ridden = true;
                    snprintf(log, log_len, T("Montaste el %s."), T(species_def(ga->animals[a].species)->name));
                } else {
                    snprintf(log, log_len, "%s", T("No hay una montura ensillada cerca (lazo + silla)."));
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
        // Trabajan los del campamento de la obra (los que estan ocupados en tareas, no); fuera de todo
        // campamento (fundando uno), la escolta que este cerca.
        int site_camp = camp_at(ga->camps, CAMPS_MAX, bp->x, bp->z);
        int excl[TROOP_MAX * 2], n_excl = 0;
        for (int k = 0; k < n_busy; k++)
            if (site_camp >= 0) excl[n_excl++] = busy[k];
        for (int k = 0; k < troop->count && k < TROOP_MAX; k++) {
            const Member *m = &troop->members[k];
            const Npc *nk = &ga->npcs[k];
            bool ok = site_camp >= 0 ? m->camp == site_camp && !nk->escort && !camp_member_busy(&ga->camps[site_camp], m->id)
                                     : nk->escort && dist2d(nk->pos, site) < 40.0f;
            if (!ok) excl[n_excl++] = m->id;
        }
        for (int k = 0; site_camp < 0 && k < n_busy; k++) { // ya en otra obra
            bool esc = false;
            for (int j = 0; j < troop->count && j < TROOP_MAX; j++)
                if (ga->npcs[j].member_id == busy[k] && ga->npcs[j].escort) esc = true;
            if (!esc) excl[n_excl++] = busy[k];
        }
        CrewPlan plan = build_plan_excluding(d, troop, player_near, excl, n_excl);
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
                present[k] = dist2d(n->pos, site) < (n->escort ? 12.0f : AT_SITE); // la escolta trabaja a tu lado
            }
            ga->crew_present[i] += present[k];
        }
        if (build_advance(bp, build_rate_present(d, &plan, present), dt)) {
            props_add(props, d->produces, ground_at(t, bp->x, bp->z), 0.0f);
            snprintf(log, log_len, T("Obra terminada: %s."), T(d->name));
            if (bp->def == BUILD_SHELTER) cg_basic_structure(ga, bp->x, bp->z);
            tg_xp(ga, XP_BUILD, log, log_len);
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
            if (kept < n) stock_add(ga_stock(ga), c->produces, n - kept);
            snprintf(log, log_len, "%s: %s%s.", craft_by_hand(c) ? T("Hecho") : T("Forjado"), T(c->name),
                     kept < n ? T(" (lo que no cabe, al acopio)") : "");
            tg_xp(ga, XP_CRAFT, log, log_len);
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
    // Esconderse: agachado junto a la hierba alta, una roca o una mata (o en sigilo dentro de la
    // hierba alta) no se hace ruido y cuesta mucho verlo (player_visibility).
    ga->hidden = false;
    if ((p->sneaking || p->crouching) && ga->mounted < 0) {
        for (int i = 0; i < props->count; i++) {
            const char *id = props->items[i].item->id;
            bool grass = !strcmp(id, "mapa.vegetacion.hierba_alta");
            bool cover = grass || (p->crouching && (!strncmp(id, "mapa.roca.", 10) || !strncmp(id, "mapa.vegetacion.mata", 20)));
            if (cover && dist2d(p->pos, props->items[i].pos) < (grass ? 1.3f : 1.6f)) {
                ga->hidden = true;
                p->noise = 0.0f;
                break;
            }
        }
    }
    (void)t;
}

// ---------------------------------------------------------------- dia nuevo
void ga_new_day(GameActions *ga, Props *props, const Terrain *t, Troop *troop, MemoryMap *mem, float now, int day, float temp_mean,
                char *log, size_t log_len) {
    // Comida y recoleccion: cada campamento con su gente y su acopio; la escolta (y quien no
    // tiene casa) come de lo del campamento mas cercano al jugador.
    UpkeepReport up = { 0, 0, 0 };
    WaterReport wr = { 0, 0, 0, 0, 0 };
    int near_camp = camp_nearest(ga->camps, CAMPS_MAX, ga->player_pos.x, ga->player_pos.z, NULL);
    static Troop sub; // grande: fuera de la pila
    for (int k = 0; k < CAMPS_MAX; k++) {
        if (!ga->camps[k].used) continue;
        int idx[TROOP_MAX];
        sub = *troop;
        sub.count = 0;
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
            const Member *m = &troop->members[i];
            int home = ga->npcs[i].escort || m->camp < 0 || !ga->camps[m->camp].used ? near_camp : m->camp;
            if (home != k) continue;
            idx[sub.count] = i;
            sub.members[sub.count++] = *m;
        }
        if (!sub.count) continue;
        UpkeepReport r = economy_daily_upkeep(&ga->camps[k].stock, &sub);
        // El agua: del acopio, o del rio cercano (hervida si hay leña; cruda, con los espiritus).
        CampSite *cs = &ga->camps[k];
        WaterReport w = water_daily(&cs->stock, &sub, wg_water_near(t, cs->x, cs->z), water_spirit_chance(temp_mean, false), &ga->rng);
        for (int j = 0; j < sub.count; j++) { // el hambre, la sed y la fiebre bajan el animo (y la vida)
            troop->members[idx[j]].morale = sub.members[j].morale;
            troop->members[idx[j]].health.hp = sub.members[j].health.hp;
        }
        up.eaten += r.eaten, up.hungry += r.hungry, up.gathered += r.gathered;
        wr.safe += w.safe, wr.boiled += w.boiled, wr.raw += w.raw, wr.sick += w.sick, wr.dry += w.dry;
    }
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
    snprintf(log, log_len, T("Día %d: comieron %d%s, recolectaron %d. Ánimo del campamento %+.0f."), day, up.eaten,
             up.hungry ? TextFormat(T(" (%d sin ración)"), up.hungry) : "", up.gathered, fx.morale_per_day);
    size_t used = strlen(log);
    if (wr.sick || wr.dry) // el agua: solo si algo salio mal
        snprintf(log + used, log_len - used, " %s",
                 wr.dry ? TextFormat(T("%d pasaron sed: no hay agua cerca ni en el acopio."), wr.dry)
                        : TextFormat(T("%d bebieron agua cruda y les cayeron los espíritus malditos (hace falta leña para hervirla)."), wr.sick));
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
    for (int i = 0; i < props->count; i++) { // fogatas y hogueras: piedras, leños, llamas y humo
        const Prop *pr = &props->items[i];
        float scale;
        if (props_is_fire(pr->item, &scale) && !props_has_model(props, pr->item))
            hearth_draw_base(pr->pos, scale, (uint32_t)(int)(pr->pos.x * 13.0f + pr->pos.z * 7.0f), !ga->fires_out);
    }
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
        if (m->journey > 0 && n->leave_t >= 14.0f) continue; // ya se perdio en el horizonte
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
        body_draw_outfit(&pose, base, n->yaw, &m->outfit);
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
        .moving = p->moving, .running = p->stance == STANCE_RUN, .sneaking = p->sneaking, .crouching = p->crouching, .grounded = p->grounded,
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
    snprintf(buf, sizeof(buf), T("Empuñe: %s%s%s%s%s%s"), grip_name(hands_grip(h)), h->sheathed ? T(" (enfundado)") : "",
             ga->torch_lit ? T(", antorcha") : "", h->carried[0] ? T(", lleva algo") : "",
             ga->mounted >= 0 ? T(" · montado") : "", ga->hidden ? T(" · OCULTO") : "");
    return buf;
}

// Ingredientes como iconos con lo que hay a mano / lo que hace falta (rojo si falta).
// Devuelve la altura usada; el nombre de cada uno sale en la leyenda al pasar por encima.
static int draw_materials(const GameActions *ga, const Props *props, const Player *p, const Ingredient *mats, bool at_hand,
                          int x, int y, int w) {
    if (!mats[0].id) return 0;
    int cx = x, cy = y;
    for (const Ingredient *m = mats; m->id; m++) {
        int have = at_hand ? ig_count((GameActions *)ga, props, p, m->id) : stock_count(ga_stock_c(ga), m->id);
        const char *txt = TextFormat("%d/%d", have, m->count);
        int cw = 20 + MeasureText(txt, 10) + 8;
        if (cx + cw > x + w) cx = x, cy += 20;
        Rectangle r = { (float)cx, (float)cy, 18, 18 };
        bool ok = have >= m->count;
        DrawRectangleRec(r, ok ? UI_LEATHER : (Color){ 70, 24, 18, 236 });
        DrawRectangleLinesEx(r, 1, ok ? UI_GOLD_DARK : UI_CARNELIAN);
        ui_icon(icon_for_item(m->id), r.x + 1, r.y + 1, 16, ok ? UI_GOLD : UI_CARNELIAN);
        ui_text(txt, cx + 21, cy + 4, 10, ok ? UI_BONE : UI_CARNELIAN);
        if (ui_hover((Rectangle){ r.x, r.y, (float)cw, 18 }))
            ui_legend(item_name(ga, m->id), TextFormat(T("tienes %d %s, hacen falta %d"), have, at_hand ? T("a mano") : T("en el acopio"), m->count));
        cx += cw;
    }
    return cy - y + 22;
}

// Un dato con su icono (tiempo, cuadrilla, artesano...). Devuelve el ancho usado.
static int chip(IconId icon, const char *text, int x, int y, Color c) {
    ui_icon(icon, (float)x, (float)y, 16, c);
    ui_text(text, x + 19, y + 4, 10, c);
    return 19 + MeasureText(text, 10) + 10;
}

static const char *TAB_NAMES[4] = { N_("Acciones"), N_("Obras"), N_("Fabricar"), N_("Reparar") };

static void draw_menu(const GameActions *ga, const Props *props, const Troop *troop, const Player *p, int width) {
    (void)width;
    const int x0 = TAB_X, y0 = TAB_Y, w = TAB_W, h = TAB_H, pad = TAB_PAD;
    const int grid_w = GRID_COLS * (TILE + TILE_GAP), desc_x = x0 + pad + grid_w + 10, desc_w = w - 2 * pad - grid_w - 10;
    ui_panel((Rectangle){ (float)x0, (float)y0, (float)w, (float)h }, UI_METAL_GOLD);
    // Pestañas: solo iconos; su nombre en la leyenda y en grande el de la abierta.
    for (int t = 0; t < 4; t++) {
        Rectangle r = tab_rect(t);
        if (ui_tile(r, TAB_ICONS[t], t == ga->menu_tab, true)) ui_legend(T(TAB_NAMES[t]), T("Q / E cambia de pestaña"));
    }
    ui_text(T(TAB_NAMES[ga->menu_tab]), x0 + pad + 4 * 32 + 6, y0 + pad + 6, 20, UI_GOLD_LIGHT);
    ui_divider(x0 + pad, y0 + pad + 32, w - 2 * pad, UI_METAL_GOLD);
    RepairItem rep[24];
    int nrep = ga->menu_tab == 3 ? ig_repair_list((GameActions *)ga, props, p, rep, 24) : 0;
    int total = ga->menu_tab == 0 ? ACTION_COUNT : ga->menu_tab == 1 ? BUILD_COUNT : ga->menu_tab == 2 ? CRAFT_COUNT : nrep;
    int cur = ga->tab_cursor[ga->menu_tab];
    if (cur >= total) cur = total ? total - 1 : 0;
    if (!total) {
        ui_icon(ICON_OK, (float)(x0 + pad + 8), (float)(y0 + pad + 46), 32, UI_BONE_DIM);
        ui_legend_default(ga->menu_tab == 3 ? T("Nada que reparar") : T("Vacío"), ga->menu_tab == 3 ? T("Ninguna pieza gastada a mano") : "");
        return;
    }
    // La cuadricula de botones.
    int first = grid_first_row(cur);
    for (int i = first * GRID_COLS; i < total && i < (first + GRID_ROWS) * GRID_COLS; i++) {
        Rectangle r = grid_rect(i, first);
        IconId icon;
        bool enabled = true;
        if (ga->menu_tab == 0) icon = ACTION_ICONS[i];
        else if (ga->menu_tab == 1) icon = BUILD_ICONS[i], enabled = build_plan(build_def((BuildId)i), troop, true).check == BUILD_READY;
        else if (ga->menu_tab == 2) {
            const CraftDef *d = craft_def((CraftId)i);
            icon = icon_for_item(d->produces);
            enabled = !ig_first_missing((GameActions *)ga, props, p, d->mats) && craft_seconds(d, troop) >= 0.0f;
        } else {
            icon = icon_for_item(rep[i].id);
            enabled = ig_can_repair((GameActions *)ga, props, p, troop, &rep[i], NULL, 0);
        }
        ui_tile(r, icon, i == cur, enabled);
        if (ga->menu_tab == 2) {
            const CraftDef *d = craft_def((CraftId)i);
            if (d->amount > 1) ui_tile_badge(r, TextFormat("x%d", d->amount), UI_BONE);
            if (craft_by_hand(d)) DrawCircle((int)r.x + 5, (int)r.y + 5, 2, UI_TURQUOISE); // a mano, en cualquier sitio
        } else if (ga->menu_tab == 3) {
            ui_tile_bar(r, rep[i].cond, rep[i].cond < 0.3f ? UI_CARNELIAN : UI_GOLD);
        }
    }
    if (total > (first + GRID_ROWS) * GRID_COLS || first > 0) // hay mas filas
        ui_text(TextFormat("%d/%d", cur + 1, total), x0 + pad, y0 + h - pad - 10, 10, UI_BONE_DIM);
    // Detalle del elegido: icono grande, nombre e iconos de lo que pide.
    Rectangle big = { (float)desc_x, (float)(y0 + pad + 38), 68, 68 };
    int tx = desc_x + 76, dy = y0 + pad + 40;
    switch (ga->menu_tab) {
    case 0: {
        const ActionDef *d = action_def((ActionId)cur);
        ui_tile(big, ACTION_ICONS[cur], true, true);
        ui_text_wrapped(T(d->name), tx, dy, desc_w - 76, 10, UI_GOLD_LIGHT);
        chip(ICON_TIEMPO, TextFormat("%.1f s", d->seconds), tx, dy + 26, UI_BONE_DIM);
        int my = (int)(big.y + big.height) + 8;
        if (d->requires) {
            bool has = has_item(ga, d->requires);
            Rectangle rq = { (float)desc_x, (float)my, 18, 18 };
            ui_icon(icon_for_item(d->requires), rq.x + 1, rq.y + 1, 16, has ? UI_GOLD : UI_CARNELIAN);
            ui_icon(has ? ICON_OK : ICON_FALTA, rq.x + 20, rq.y + 1, 16, has ? UI_TURQUOISE : UI_CARNELIAN);
            if (ui_hover((Rectangle){ rq.x, rq.y, 40, 18 })) ui_legend(TextFormat(T("Requiere: %s"), item_name(ga, d->requires)), has ? T("lo tienes") : T("no lo tienes a mano"));
            my += 22;
        }
        draw_materials(ga, props, p, d->mats, false, desc_x, my, desc_w);
        ui_legend_default(T(d->name), T(d->desc));
        break;
    }
    case 1: {
        const BuildDef *d = build_def((BuildId)cur);
        CrewPlan plan = build_plan(d, troop, true);
        bool ok = plan.check == BUILD_READY;
        ui_tile(big, BUILD_ICONS[cur], true, ok);
        ui_text_wrapped(T(d->name), tx, dy, desc_w - 76, 10, UI_GOLD_LIGHT);
        chip(ICON_TRIBU, TextFormat("%d-%d", d->min_workers, d->max_workers), tx, dy + 26, UI_BONE_DIM);
        if (ok) chip(ICON_TIEMPO, TextFormat("%.0f s", d->work / plan.rate), tx, dy + 44, UI_TURQUOISE);
        int my = (int)(big.y + big.height) + 8;
        if (d->required_role != ROLE_NONE) chip(ICON_PERSONA, T(role_name(d->required_role)), desc_x, my, ok ? UI_BONE : UI_CARNELIAN), my += 20;
        my += draw_materials(ga, props, p, d->mats, false, desc_x, my, desc_w);
        ui_icon(ok ? ICON_OK : ICON_FALTA, (float)desc_x, (float)my + 4, 16, ok ? UI_TURQUOISE : UI_CARNELIAN);
        if (!ok) ui_text_wrapped(build_check_text(d, plan.check), desc_x + 20, my + 6, desc_w - 20, 10, UI_CARNELIAN);
        ui_legend_default(T(d->name), ok ? TextFormat(T("Cuadrilla de %d · se levanta 6 m delante de ti"), plan.workers)
                              : build_check_text(d, plan.check));
        break;
    }
    case 2: {
        const CraftDef *d = craft_def((CraftId)cur);
        float secs = craft_seconds(d, troop);
        const Ingredient *miss = ig_first_missing((GameActions *)ga, props, p, d->mats);
        ui_tile(big, icon_for_item(d->produces), true, !miss && secs >= 0.0f);
        if (d->amount > 1) ui_tile_badge(big, TextFormat("x%d", d->amount), UI_BONE);
        ui_text_wrapped(T(d->name), tx, dy, desc_w - 76, 10, UI_GOLD_LIGHT);
        if (secs >= 0.0f) chip(ICON_TIEMPO, TextFormat("%.0f s", secs), tx, dy + 26, UI_BONE_DIM);
        int my = (int)(big.y + big.height) + 8;
        if (d->building) { // horno y artesano
            bool oven = count_props(props, d->building) > 0;
            int cw = chip(icon_for_item(d->building), oven ? "" : T("falta"), desc_x, my, oven ? UI_GOLD : UI_CARNELIAN);
            chip(ICON_PERSONA, T(role_name(d->role)), desc_x + cw, my, secs >= 0.0f ? UI_BONE : UI_CARNELIAN);
            if (ui_hover((Rectangle){ (float)desc_x, (float)my, (float)desc_w, 18 })) ui_legend(item_name(ga, d->building), TextFormat(T("y un %s en la tribu"), T(role_name(d->role))));
            my += 22;
        } else {
            chip(ICON_ACCIONES, T("a mano"), desc_x, my, UI_TURQ_LIGHT);
            my += 22;
        }
        draw_materials(ga, props, p, d->mats, true, desc_x, my, desc_w);
        const InvItem *out = inventory_find(ga->inv, d->produces);
        ui_legend_default(T(d->name), TextFormat(T("da %d x %s · va a la mochila (o al acopio)"), d->amount > 0 ? d->amount : 1, out ? T(out->name) : d->produces));
        break;
    }
    default: {
        const RepairItem *r = &rep[cur];
        const RepairDef *d = repair_def(r->material);
        char why[128];
        bool ok = ig_can_repair((GameActions *)ga, props, p, troop, r, why, sizeof(why));
        ui_tile(big, icon_for_item(r->id), true, ok);
        ui_tile_bar(big, r->cond, UI_GOLD);
        ui_text_wrapped(item_name(ga, r->id), tx, dy, desc_w - 76, 10, UI_GOLD_LIGHT);
        chip(ICON_COBERTURA, TextFormat("%d%% > %d%%", (int)(r->cond * 100), (int)(fminf(1.0f, r->cond + d->restore) * 100)), tx, dy + 26,
             UI_BONE);
        int my = (int)(big.y + big.height) + 8;
        if (d->building || d->role != ROLE_NONE) {
            int cw = d->building ? chip(icon_for_item(d->building), "", desc_x, my, UI_GOLD) : 0;
            if (d->role != ROLE_NONE) chip(ICON_PERSONA, T(role_name(d->role)), desc_x + cw, my, UI_BONE);
            my += 22;
        }
        my += draw_materials(ga, props, p, d->mats, true, desc_x, my, desc_w);
        ui_icon(ok ? ICON_OK : ICON_FALTA, (float)desc_x, (float)my + 4, 16, ok ? UI_TURQUOISE : UI_CARNELIAN);
        if (!ok) ui_text_wrapped(why, desc_x + 20, my + 6, desc_w - 20, 10, UI_CARNELIAN);
        ui_legend_default(item_name(ga, r->id), ok ? TextFormat(T("%s · Enter o clic otra vez: reparar"), r->worn ? T("puesta") : T("guardada")) : why);
        break;
    }
    }
}

void ga_draw_hud(const GameActions *ga, const Props *props, const Troop *troop, const Player *p, int width, int height) {
    if (ga->doing >= 0) {
        const ActionDef *d = action_def((ActionId)ga->doing);
        int w = 160, x = (width - w) / 2, y = height - 64;
        ui_text_centered(TextFormat("%s...", T(d->name)), width / 2, y - 12, 10, UI_BONE);
        ui_bar(x, y, w, ga->timer / d->seconds, UI_TURQUOISE, UI_METAL_GOLD);
    }
    int lines = ga->project_count + (ga->crafting >= 0);
    if (lines > 0 && !ga->stock_open) {
        // Bajo la placa de las constantes (src/game/hud_game.c).
        int x = width - 236, y = 234, w = 230, h = 20 + 12 * lines;
        ui_panel((Rectangle){ (float)x, (float)y, (float)w, (float)h + UI_PANEL_INSET }, UI_METAL_SILVER);
        int ty = y + UI_PANEL_INSET;
        for (int i = 0; i < ga->project_count; i++, ty += 12) {
            const BuildProject *bp = &ga->projects[i];
            const BuildDef *d = build_def(bp->def);
            bool stalled = ga->crew_size[i] == 0;
            const char *crew = stalled ? T("sin cuadrilla") : TextFormat("%d/%d", ga->crew_present[i], ga->crew_size[i]);
            ui_text(TextFormat("%s %d%% · %s", T(d->name), (int)(bp->progress * 100.0f), crew), x + UI_PANEL_INSET, ty,
                    10, stalled ? UI_CARNELIAN : UI_BONE);
        }
        if (ga->crafting >= 0)
            ui_text(TextFormat(T("Haciendo: %s %d%%"), T(craft_def((CraftId)ga->crafting)->name),
                               (int)(100.0f * ga->craft_timer / ga->craft_total)),
                    x + UI_PANEL_INSET, ty, 10, UI_GOLD);
    }
    if (ga->menu_open) draw_menu(ga, props, troop, p, width);
}
