#include "game/cave_game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/cave_props.h"
#include "game/cave_scene.h"
#include "game/dialog.h"
#include "game/input.h"
#include "game/inventory_game.h"
#include "raymath.h"
#include "rlgl.h"
#include "sim/caves.h"
#include "sim/clock.h"
#include "sim/health.h"
#include "sim/lang.h"
#include "sim/rng.h"
#include "sim/world.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "world/hearth.h"

// GameActions guarda una casilla por cueva del mundo (su indice es el de la guarida, src/sim/world.c) y un bit por
// objeto: si cambian CAVES_MAX o CAVE_OBJECTS_MAX, esto no compila.
typedef char cave_slots_match[(GA_CAVES == CAVES_MAX && CAVE_OBJECTS_MAX <= 64) ? 1 : -1];

#define EXPLORE_HALF 2.4f   // m: de costado, lo que alcanza la boca para explorar (F)
#define EXPLORE_FRONT -3.2f // m: desde cuan afuera de la boca (en el plano de la cueva, +z adentro)
#define EXPLORE_BACK 4.3f   // m: hasta el fondo del tunel oscuro de la boca (src/game/world_game.c)
#define GUARD_NEAR 38.0f  // m: a esta distancia de una cueva habitada salen sus guardias (una vez por dia; se la ve a 45)
#define GUARD_BLOCK 18.0f // m: con guardias en pie a esta distancia de la boca no se entra
#define REACH 1.9f        // m: lo que alcanza F
#define CAVE_SPEED 0.85f  // el suelo de la cueva, desparejo: un poco mas lento que afuera
#define TALK_PRISONER 400 // GameActions.talk_mode del dialogo con un prisionero (de 400 a 499: las cuevas)

static int g_cave = -1;     // la cueva en la que esta el jugador (indice de World.caves), o -1
static Player g_in;         // el jugador adentro: pos en coordenadas de la cueva
static int g_obj = -1;      // lo que alcanza F (un objeto de la cueva), o -1
static char g_hint[160];    // lo que hace F (o por que no se puede)
static int g_talk = -1;     // la celda del prisionero con quien se habla
static uint32_t g_seen;     // las salas que ya viste en esta visita (el plano)
static bool g_steep_told;   // ya se dijo que la bajada no se sube (hasta pisar otra cosa)
static float g_block_told;  // s de juego del ultimo «los guardias te cierran el paso»
static Rectangle g_hint_rect; // el aviso de F, que tambien se toca
static int g_mouth = -1;      // afuera: la cueva en cuya boca esta el jugador (F: explorar), o -1
static int g_loading = -1;    // la cueva que se esta cargando (explorar), o -1
static bool g_loading_drawn;  // ya se mostro «Explorando...»: en el cuadro que sigue se arma la escena
static int g_loading_frames;  // los cuadros desde que se pulso F (si no se llega a mostrar, se arma igual)
static bool g_claimed;        // en este cuadro, F fue para explorar (src/game/actions_game.c no la usa)

// ---------------------------------------------------------------- utiles
static const Cave *cave_at(const Terrain *t, int i) { return i >= 0 && i < t->world->cave_count ? &t->world->caves[i] : NULL; }

static bool is_done(const GameActions *ga, int cave, int obj) {
    return cave >= 0 && cave < GA_CAVES && obj >= 0 && obj < CAVE_OBJECTS_MAX && ((ga->cave_done[cave] >> obj) & 1u);
}

static void set_done(GameActions *ga, int cave, int obj) {
    if (cave >= 0 && cave < GA_CAVES && obj >= 0 && obj < CAVE_OBJECTS_MAX) ga->cave_done[cave] |= (uint64_t)1 << obj;
}

static const char *name_of(const GameActions *ga, const char *id) {
    const InvItem *it = inventory_find(ga->inv, id);
    return it ? T(it->name) : id;
}

// La primera letra en minuscula (los nombres van en medio de una frase); solo ASCII, como en world_game.c.
static void lower_first(char *s) {
    if (s[0] >= 'A' && s[0] <= 'Z') s[0] = (char)(s[0] - 'A' + 'a');
}

static bool lit_kind(int k) { return k == COBJ_TORCH || k == COBJ_FIRE; }

// Lo hecho en la cueva en la que esta (un bit por objeto: tomado, encendido, liberado).
static uint64_t done_bits(const GameActions *ga) { return g_cave >= 0 && g_cave < GA_CAVES ? ga->cave_done[g_cave] : 0u; }

// Donde esta un objeto (lo de la pared, sobre la roca; src/sim/caves.c, cave_object_at).
static Vector3 obj_pos(const Cave *c, int i) {
    float x, y, z;
    cave_object_at(c, i, &x, &y, &z, NULL);
    return (Vector3){ x, y, z };
}

// ---------------------------------------------------------------- moverse adentro
static bool object_blocks(const GameActions *ga, const Cave *c, float x, float z) {
    for (int i = 0; i < c->object_count; i++) {
        const CaveObject *o = &c->objects[i];
        float r = 0.0f;
        switch (o->kind) {
        case COBJ_COLUMN: r = 0.55f; break;
        case COBJ_STALAGMITE: r = 0.3f * o->size; break;
        case COBJ_CELL: r = 0.85f; break;
        case COBJ_CHEST: r = 0.5f; break;
        case COBJ_FIRE: r = 0.6f; break;
        case COBJ_AMPHORA: r = is_done(ga, g_cave, i) ? 0.0f : 0.3f; break;
        case COBJ_ARMS: r = 0.45f; break;
        default: break;
        }
        if (r <= 0.0f) continue;
        Vector3 at = obj_pos(c, i);
        if ((x - at.x) * (x - at.x) + (z - at.z) * (z - at.z) < r * r) return true;
    }
    int nd = 0;
    const CaveDecor *d = cs_decor(&nd); // las rocas grandes del decorado (src/sim/caves.c, cave_decor)
    return cave_decor_blocked(d, nd, x, z);
}

static bool step_ok(const GameActions *ga, const Cave *c, float x, float z, float nx, float nz) {
    return cave_step_ok(c, x, z, nx, nz) && !object_blocks(ga, c, nx, nz);
}

// Como player_update, pero en la cueva: el suelo de cave_floor, la roca y la bajada sin vuelta (cave_step_ok).
static void move_inside(const GameActions *ga, const Cave *c, Player *q, PlayerInput in, float cam_yaw, float dt, char *log, size_t len) {
    if (in.toggle_sneak) q->sneaking = !q->sneaking;
    if (in.toggle_crouch) q->crouching = !q->crouching;
    float mx = in.move_x, mz = in.move_z, l = sqrtf(mx * mx + mz * mz);
    if (l > 1.0f) mx /= l, mz /= l;
    q->moving = l > 0.05f;
    if (player_winded()) in.run = false;
    if (in.run && q->moving) q->sneaking = false, q->crouching = false;
    q->stance = q->sneaking ? STANCE_SNEAK : (in.run && q->moving ? STANCE_RUN : STANCE_WALK);
    float speed = q->stance == STANCE_SNEAK ? 1.8f : q->stance == STANCE_RUN ? 8.5f : 4.0f;
    if (q->crouching) speed = q->sneaking ? 1.2f : 2.2f;
    speed *= (q->speed_scale > 0.0f ? q->speed_scale : 1.0f) * CAVE_SPEED;
    if (q->moving) {
        float fx = sinf(cam_yaw), fz = cosf(cam_yaw), vx = fx * mz - fz * mx, vz = fz * mz + fx * mx;
        float nx = q->pos.x + vx * speed * dt, nz = q->pos.z + vz * speed * dt;
        if (step_ok(ga, c, q->pos.x, q->pos.z, nx, nz)) q->pos.x = nx, q->pos.z = nz;
        else if (step_ok(ga, c, q->pos.x, q->pos.z, nx, q->pos.z)) q->pos.x = nx; // se desliza junto a la roca
        else if (step_ok(ga, c, q->pos.x, q->pos.z, q->pos.x, nz)) q->pos.z = nz;
        else {
            int where = 0;
            if (cave_floor(c, q->pos.x, q->pos.z, NULL, &where) && where == CAVE_IN_CORRIDOR && !g_steep_told) {
                g_steep_told = true;
                snprintf(log, len, "%s", T("La bajada es demasiado empinada para subirla: busca otra salida por las recámaras."));
            }
        }
        float target = atan2f(vx, vz), diff = atan2f(sinf(target - q->yaw), cosf(target - q->yaw));
        q->yaw += diff * fminf(1.0f, 10.0f * dt);
    }
    player_stamina_tick(q->stance == STANCE_RUN, q->moving, dt);
    float y = q->pos.y;
    int where = 0;
    if (cave_floor(c, q->pos.x, q->pos.z, &y, &where)) {
        q->pos.y = y;
        if (where != CAVE_IN_CORRIDOR) g_steep_told = false;
        if (where >= 0 && where < CAVE_IN_CORRIDOR) g_seen |= 1u << where;
    }
    q->grounded = true, q->vy = 0.0f;
    q->noise = !q->moving ? 0.0f : q->stance == STANCE_RUN ? 1.0f : (q->stance == STANCE_SNEAK ? 0.15f : 0.5f) * (q->crouching ? 0.6f : 1.0f);
}

// ---------------------------------------------------------------- F
static bool interactive(const GameActions *ga, const Cave *c, int i) {
    const CaveObject *o = &c->objects[i];
    if (o->kind == COBJ_PAINTING) return true;
    if (is_done(ga, g_cave, i)) return false;
    int n = 0;
    if (cave_object_loot(c, i, &n)) return true;
    if (o->kind == COBJ_CELL) return o->prisoner != 0;
    return lit_kind(o->kind) && !o->lit;
}

static int nearest_object(const GameActions *ga, const Cave *c) {
    int best = -1;
    float bd = REACH * REACH;
    for (int i = 0; i < c->object_count; i++) {
        if (!interactive(ga, c, i)) continue;
        Vector3 at = obj_pos(c, i);
        float dx = at.x - g_in.pos.x, dz = at.z - g_in.pos.z, d = dx * dx + dz * dz;
        if (fabsf(at.y - g_in.pos.y) > 2.5f) continue;
        if (c->objects[i].kind == COBJ_CELL) d -= 0.5f; // la celda es ancha: se habla desde la reja
        if (d < bd) bd = d, best = i;
    }
    return best;
}

static void hint_for(const GameActions *ga, const Cave *c, int i) {
    g_hint[0] = '\0';
    if (i < 0) return;
    const CaveObject *o = &c->objects[i];
    int n = 0;
    const char *id = cave_object_loot(c, i, &n);
    char what[64] = "";
    if (id) {
        snprintf(what, sizeof(what), "%s", name_of(ga, id));
        lower_first(what);
    }
    switch (o->kind) {
    case COBJ_CHEST: snprintf(g_hint, sizeof(g_hint), "%s", T("F: abrir el cofre")); break;
    case COBJ_AMPHORA: snprintf(g_hint, sizeof(g_hint), T("F: tomar el ánfora (%s)"), what); break;
    case COBJ_ARMS: snprintf(g_hint, sizeof(g_hint), T("F: tomar del depósito de armas (%s)"), what); break;
    case COBJ_VEIN: snprintf(g_hint, sizeof(g_hint), T("F: arrancar la veta (%s)"), what); break;
    case COBJ_NEST: snprintf(g_hint, sizeof(g_hint), T("F: deshacer el nido (%s)"), what); break;
    case COBJ_BONES: snprintf(g_hint, sizeof(g_hint), "%s", T("F: juntar los huesos")); break;
    case COBJ_LOOT: snprintf(g_hint, sizeof(g_hint), T("F: tomar el botín de la fiera (%s)"), what); break;
    case COBJ_WEAPON: snprintf(g_hint, sizeof(g_hint), T("F: tomar el arma abandonada (%s)"), what); break;
    case COBJ_CELL: {
        char who[32];
        snprintf(who, sizeof(who), "%s", T(cave_prisoner_name((PrisonerKind)((o->prisoner - 1) % PRISONER_KINDS))));
        snprintf(g_hint, sizeof(g_hint), T("F: hablar con el prisionero (un %s)"), who);
        break;
    }
    case COBJ_TORCH:
        snprintf(g_hint, sizeof(g_hint), "%s", ga->torch_lit ? T("F: encender la antorcha de la pared") : T("Una antorcha apagada: enciende la tuya para prenderla"));
        break;
    case COBJ_FIRE:
        snprintf(g_hint, sizeof(g_hint), "%s", ga->torch_lit ? T("F: encender la fogata") : T("Una fogata apagada: hace falta tu antorcha encendida"));
        break;
    case COBJ_PAINTING: snprintf(g_hint, sizeof(g_hint), "%s", T("F: mirar las pinturas")); break;
    default: break;
    }
}

static const char *RESCUE_NAMES[] = { "Altan", "Bayar", "Chinua", "Dash", "Enkh", "Ganbaatar", "Jargal", "Khongor",
                                      "Mönkhbat", "Nergui", "Odval", "Saran", "Temür", "Tsetseg", "Uyanga", "Zolboo" };

static const char *prisoner_name(int cave, int obj) {
    return RESCUE_NAMES[(unsigned)(cave * 7 + obj * 3) % (sizeof(RESCUE_NAMES) / sizeof(RESCUE_NAMES[0]))];
}

static void open_prisoner(GameActions *ga, const Cave *c, int obj) {
    static const char *SAY[PRISONER_KINDS] = {
        N_("Me agarraron con mis ovejas. Sácame de aquí y te seguiré a donde vayas."),
        N_("Conozco cada sendero de esta tierra. Rompe estas cadenas y cazaré para tu tribu."),
        N_("Me tienen forjando sus armas. Libérame y forjaré las tuyas."),
        N_("No soy como ellos... ya no. Suéltame: no te vas a arrepentir."),
    };
    const CaveObject *o = &c->objects[obj];
    PrisonerKind k = (PrisonerKind)((o->prisoner - 1) % PRISONER_KINDS);
    int pct = cave_betray_percent(c, obj);
    char text[400], who[64], kind[32];
    snprintf(kind, sizeof(kind), "%s", T(cave_prisoner_name(k)));
    snprintf(text, sizeof(text), "%s %s", T(SAY[k]),
             pct < 20 ? T("Se lo ve sincero.") : pct < 40 ? T("No sabes si fiarte.") : T("Tiene mirada de lobo: puede traicionarte."));
    snprintf(who, sizeof(who), T("%s, un %s encadenado"), prisoner_name(g_cave, obj), kind);
    ga->talk_mode = TALK_PRISONER;
    g_talk = obj;
    dlg_begin(&ga->dlg, who, ICON_PERSONA, text);
    ga->dlg.cursor = 0;
    dlg_option(&ga->dlg, ICON_SOLTAR, T("Romper las cadenas"), T("Se une a la tribu, o te traiciona y huye"), true, 1);
    dlg_option(&ga->dlg, ICON_SALIR, T("Dejarlo"), T("Sigue encadenado: puedes volver"), true, 0);
}

// La traicion: te golpea, te roba algo de la mochila (o de los bolsillos) y huye.
static void steal(GameActions *ga, char *what, size_t len) {
    what[0] = '\0';
    Bag *bags[2] = { ga->pack_where == PACKW_WORN ? &ga->backpack : NULL, &ga->pockets };
    int start = (int)(rng_float(&ga->rng) * 97.0f);
    for (int b = 0; b < 2; b++) {
        Bag *g = bags[b];
        if (!g || g->n <= 0) continue;
        int slot = start % g->n, n = g->s[slot].count < 3 ? g->s[slot].count : 3;
        char name[64];
        snprintf(name, sizeof(name), "%s", name_of(ga, g->s[slot].id));
        lower_first(name);
        snprintf(what, len, "%s (%d)", name, n);
        bag_remove_slot(g, slot, n);
        return;
    }
}

static void free_prisoner(GameActions *ga, Combat *cb, Troop *troop, const Cave *c, int obj, char *log, size_t len) {
    static const Role ROLES[PRISONER_KINDS] = { ROLE_HERDER, ROLE_HUNTER, ROLE_SMITH, ROLE_SOLDIER };
    static const unsigned TRAITS[PRISONER_KINDS] = { TRAIT_MERCIFUL, 0u, TRAIT_DEVOUT, TRAIT_AMBITIOUS };
    const CaveObject *o = &c->objects[obj];
    PrisonerKind k = (PrisonerKind)((o->prisoner - 1) % PRISONER_KINDS);
    const char *name = prisoner_name(g_cave, obj);
    char kind[32];
    snprintf(kind, sizeof(kind), "%s", T(cave_prisoner_name(k)));
    set_done(ga, g_cave, obj);
    if (cave_prisoner_joins(c, obj, rng_float(&ga->rng))) {
        int id = troop_recruit(troop, name, TRAITS[k]);
        Member *m = troop_find(troop, id);
        if (!m) {
            snprintf(log, len, T("%s te da las gracias, pero la tribu está llena: se va por su cuenta."), name);
            return;
        }
        troop_assign_role(troop, id, ROLES[k]);
        m->camp = -1; // va con el jugador
        m->pack = PACK_SMALL;
        bag_init_pack(&m->bag, PACK_SMALL);
        m->loyalty = fminf(100.0f, m->loyalty + 20.0f); // le debe la vida
        snprintf(log, len, T("%s, el %s, se une a la tribu: te espera en la boca de la cueva."), name, kind);
        return;
    }
    health_hit(&cb->player, &ga->rng, 10.0f, WOUND_BRUISE, PART_FOREARM_L);
    char what[96];
    steal(ga, what, sizeof(what));
    if (what[0]) snprintf(log, len, T("¡%s (%s) te traiciona: te golpea, te roba %s y huye!"), name, kind, what);
    else snprintf(log, len, T("¡%s (%s) te traiciona: te golpea y huye a la oscuridad!"), name, kind);
}

static void painting_text(const Cave *c, char *log, size_t len) {
    const char *t = c->kind == CAVE_EMPIRE   ? T("Un relieve del imperio: soldados en fila ante un rey, y una lista de lo que guardan aquí.")
                    : c->kind == CAVE_TRIBE  ? T("Pinturas de la tribu: jinetes, ciervos y el sol; cuentan de dónde vinieron.")
                    : c->kind == CAVE_BANDITS ? T("Marcas de los forajidos: rayas por cada caravana asaltada.")
                                              : T("Pinturas muy viejas: manos en ocre y animales que ya no se ven en la estepa.");
    snprintf(log, len, "%s", t);
}

static void interact(GameActions *ga, Troop *troop, const Props *props, const Player *outside, const Cave *c, int i, char *log, size_t len) {
    (void)troop;
    const CaveObject *o = &c->objects[i];
    int n = 0;
    const char *id = cave_object_loot(c, i, &n);
    if (id) {
        int got = ig_store(ga, props, outside, id, n, 1.0f);
        char what[64];
        snprintf(what, sizeof(what), "%s", name_of(ga, id));
        if (got <= 0) {
            snprintf(log, len, T("No te cabe: %s."), what);
            return;
        }
        set_done(ga, g_cave, i);
        lower_first(what);
        if (o->kind == COBJ_CHEST) snprintf(log, len, got < n ? T("Abres el cofre: %d × %s (el resto no te cabe).") : T("Abres el cofre: %d × %s."), got, what);
        else snprintf(log, len, got < n ? T("Tomas %d × %s (el resto no te cabe).") : T("Tomas %d × %s."), got, what);
        return;
    }
    switch (o->kind) {
    case COBJ_CELL: open_prisoner(ga, c, i); break;
    case COBJ_TORCH:
    case COBJ_FIRE:
        if (!ga->torch_lit) {
            snprintf(log, len, "%s", T("Hace falta tu antorcha encendida (la barra rápida)."));
            break;
        }
        set_done(ga, g_cave, i);
        snprintf(log, len, "%s", o->kind == COBJ_TORCH ? T("Enciendes la antorcha de la pared.") : T("Enciendes la fogata: la recámara se ilumina."));
        break;
    case COBJ_PAINTING: painting_text(c, log, len); break;
    default: break;
    }
}

// ---------------------------------------------------------------- entrar y salir
// Entrar: primero se arma la escena de la cueva (src/game/cave_scene.c, aparte del mundo); si no cupo, no se entra.
static bool enter(GameActions *ga, Player *p, const Terrain *t, int idx, float *cam_yaw, char *log, size_t len) {
    const Cave *c = cave_at(t, idx);
    if (!c) return false;
    if (!cs_load(c, idx)) {
        snprintf(log, len, "%s", T("No hay memoria para explorar esta cueva."));
        return false;
    }
    cp_preload(c);
    CsStats st = cs_stats();
    TraceLog(LOG_INFO, "CUEVA %d: %d trozos de roca (%d vertices, %d triangulos), %d del decorado, %d modelos de voxeles (%d triangulos), %.0f ms", idx,
             st.chunks, st.vertices, st.triangles, st.decor, st.models, st.model_tris, (double)st.load_ms);
    g_cave = idx;
    g_in = *p;
    float x = 0.0f, z = 0.0f, yaw = 0.0f, y = 0.0f;
    cave_entry(c, &x, &z, &yaw);
    cave_floor(c, x, z, &y, NULL);
    (void)yaw;
    g_in.pos = (Vector3){ x, y, z };
    // Sin girar de golpe: el jugador y la camara siguen mirando adonde miraban (en la cueva, el angulo se cuenta
    // desde su eje: el del mundo menos el de la boca).
    g_in.yaw = p->yaw - c->yaw, g_in.vy = 0.0f, g_in.grounded = true, g_in.covered = g_in.peeking = false, g_in.draw_lift = 0.0f;
    *cam_yaw -= c->yaw;
    // Para el mundo, el jugador queda en la boca, del lado de afuera.
    cave_to_world(c, 0.0f, -0.8f, &p->pos.x, &p->pos.z);
    p->pos.y = terrain_height(t, p->pos.x, p->pos.z);
    p->yaw = c->yaw, p->covered = p->peeking = false, p->moving = false;
    g_seen = 0, g_obj = -1, g_talk = -1, g_steep_told = false, g_hint[0] = '\0';
    ga_set_in_cave(true);
    char kind[48];
    snprintf(kind, sizeof(kind), "%s", T(cave_kind_name(c->kind)));
    const char *how = c->kind == CAVE_ANIMAL      ? T("Huele a fiera.")
                      : c->kind == CAVE_ABANDONED ? T("Hace mucho que nadie vive aquí.")
                                                  : T("Hay antorchas al fondo: alguien vive aquí.");
    if (ga->torch_lit) snprintf(log, len, T("Cueva: %s. %s"), kind, how);
    else snprintf(log, len, T("Cueva: %s. Está oscuro: enciende la antorcha."), kind);
    return true;
}

static void leave(GameActions *ga, Player *p, const Terrain *t, float *cam_yaw, char *log, size_t len) {
    const Cave *c = cave_at(t, g_cave);
    if (c) {
        cave_to_world(c, 0.0f, -1.6f, &p->pos.x, &p->pos.z);
        p->pos.y = terrain_height(t, p->pos.x, p->pos.z);
        p->yaw = g_in.yaw + c->yaw; // como al entrar: sin girar de golpe
        *cam_yaw += c->yaw;
    }
    p->sneaking = g_in.sneaking, p->crouching = g_in.crouching, p->stance = g_in.stance;
    if (ga->dlg.open && ga->talk_mode == TALK_PRISONER) dlg_close(&ga->dlg);
    g_cave = -1, g_talk = -1, g_obj = -1, g_hint[0] = '\0';
    ga_set_in_cave(false);
    cp_unload(), cs_unload(); // afuera, la cueva no ocupa nada
    snprintf(log, len, "%s", T("Sales de la cueva."));
}

// Afuera: los guardias de las cuevas habitadas cercanas y la boca. En el umbral o en el tunel de la boca, F explora: la
// escena se arma aparte en el cuadro que sigue (con «Explorando...» en pantalla) y se entra.
static void outside(GameActions *ga, Combat *cb, const Props *props, Player *p, const Terrain *t, float world_time, bool input_ok, char *log,
                    size_t len) {
    const World *w = t->world;
    int day = clock_day(world_time);
    for (int i = 0; i < w->cave_count && i < GA_CAVES; i++) {
        const Cave *c = &w->caves[i];
        float dx = c->x - p->pos.x, dz = c->z - p->pos.z, d2 = dx * dx + dz * dz;
        if (c->kind < CAVE_BANDITS || d2 > GUARD_NEAR * GUARD_NEAR || ga->cave_guards_day[i] == day) continue;
        ga->cave_guards_day[i] = day; // una vez por dia
        float gx = 0.0f, gz = 0.0f;
        cave_to_world(c, 0.0f, -4.0f, &gx, &gz);
        cb_spawn_party_at(cb, c->kind == CAVE_EMPIRE ? "guardia_imperial" : c->kind == CAVE_TRIBE ? "guerreros_cueva" : "guardia_cueva",
                          (Vector3){ gx, terrain_height(t, gx, gz), gz }, t, log, len);
    }
    g_mouth = -1;
    g_hint[0] = '\0';
    if (ga->climbing || ga->dlg.open || cb->player.down) return;
    for (int i = 0; i < w->cave_count && i < GA_CAVES && g_mouth < 0; i++) {
        float lx = 0.0f, lz = 0.0f;
        cave_from_world(&w->caves[i], p->pos.x, p->pos.z, &lx, &lz);
        if (lz >= EXPLORE_FRONT && lz <= EXPLORE_BACK && fabsf(lx) <= EXPLORE_HALF) g_mouth = i;
    }
    if (g_mouth < 0) return;
    const Cave *c = &w->caves[g_mouth];
    Vector3 mouth = { c->x, 0.0f, c->z };
    bool guarded = cb_hostiles_near(cb, mouth, GUARD_BLOCK) > 0;
    snprintf(g_hint, sizeof(g_hint), "%s", ga->mounted >= 0 ? T("A caballo no se entra en la cueva: desmonta primero.")
                                           : guarded       ? T("Hay enemigos en pie junto a la boca: no te dejan entrar.")
                                                           : T("F: explorar la cueva"));
    bool press = input_ok && ga->doing < 0 && (input_action_pressed(KA_INTERACT) || ui_click(g_hint_rect));
    if (!press || props_nearest(props, p->pos, REACH, true) >= 0) return; // con algo a mano, F lo toma (actions_game.c)
    g_claimed = true;
    if (ga->mounted >= 0 || guarded) {
        if (world_time - g_block_told > 2.0f) g_block_told = world_time, snprintf(log, len, "%s", g_hint);
        return;
    }
    g_loading = g_mouth, g_loading_drawn = false, g_loading_frames = 0;
}

// ---------------------------------------------------------------- lo publico
bool cv_inside(void) { return g_cave >= 0; }
int cv_cave(void) { return g_cave; }

bool cv_loading(void) { return g_loading >= 0; }
bool cv_claimed_interact(void) { return g_claimed; }

void cv_reset(void) {
    g_cave = -1, g_talk = -1, g_obj = -1, g_hint[0] = '\0';
    g_mouth = -1, g_loading = -1, g_loading_drawn = false, g_claimed = false;
    g_block_told = -100.0f;
    ga_set_in_cave(false);
    props_set_tint(WHITE);
    cp_unload(), cs_unload();
}

void cv_update(GameActions *ga, Combat *cb, Troop *troop, const Props *props, Player *p, const Terrain *t, PlayerInput in, float *cam_yaw,
               float world_time, bool input_ok, float dt, char *log, size_t len) {
    g_claimed = false;
    if (g_loading >= 0) { // explorar: la escena se arma despues de que se vio «Explorando...» (cv_draw_loading)
        if (!g_loading_drawn && ++g_loading_frames < 3) return;
        int idx = g_loading;
        g_loading = -1, g_mouth = -1;
        enter(ga, p, t, idx, cam_yaw, log, len);
        return;
    }
    if (g_cave < 0) {
        outside(ga, cb, props, p, t, world_time, input_ok, log, len);
        return;
    }
    const Cave *c = cave_at(t, g_cave);
    if (!c) {
        cv_reset();
        return;
    }
    // El dialogo con un prisionero.
    if (ga->dlg.open && ga->talk_mode == TALK_PRISONER) {
        int v = dlg_update(&ga->dlg);
        if (v == DLG_BACK || v == 0) dlg_close(&ga->dlg), g_talk = -1;
        else if (v == 1 && g_talk >= 0) {
            dlg_close(&ga->dlg);
            free_prisoner(ga, cb, troop, c, g_talk, log, len);
            g_talk = -1;
        }
        return;
    }
    // Las heridas siguen (sangran y sanan como afuera, sin curandero): abatido o desangrado, aparece en la boca y
    // lo atiende el combate de afuera (cb_update).
    health_update(&cb->player, &cb->rng, dt, !g_in.moving, ig_stat(ga, STAT_STAMINA_REGEN));
    if (cb->player.down || cb->player.dead) {
        leave(ga, p, t, cam_yaw, log, len);
        return;
    }
    g_in.speed_scale = p->speed_scale;
    move_inside(ga, c, &g_in, in, *cam_yaw, dt, log, len);
    p->sneaking = g_in.sneaking, p->crouching = g_in.crouching, p->stance = g_in.stance; // el HUD
    if (cave_at_exit(c, g_in.pos.x, g_in.pos.z)) {
        leave(ga, p, t, cam_yaw, log, len);
        return;
    }
    g_obj = nearest_object(ga, c);
    hint_for(ga, c, g_obj);
    bool tap = g_hint[0] && ui_click(g_hint_rect);
    if (g_obj >= 0 && input_ok && ga->doing < 0 && !ga->dlg.open && (input_action_pressed(KA_INTERACT) || tap))
        interact(ga, troop, props, p, c, g_obj, log, len);
}

void cv_camera(const Terrain *t, Camera3D *cam, float yaw, float pitch, float dist) {
    const Cave *c = cave_at(t, g_cave);
    if (!c) return;
    Vector3 target = { g_in.pos.x, g_in.pos.y + player_eye_height(&g_in), g_in.pos.z };
    Vector3 back = { -sinf(yaw) * cosf(pitch), sinf(pitch), -cosf(yaw) * cosf(pitch) };
    float want = fminf(dist, 5.5f), ok = 0.5f;
    // Hacia atras hasta la roca: la camara no sale de la cueva (ni por el techo ni por el suelo).
    for (float d = 0.5f; d <= want; d += 0.15f) {
        Vector3 q = Vector3Add(target, Vector3Scale(back, d));
        float fy = 0.0f;
        int where = 0;
        if (!cave_floor(c, q.x, q.z, &fy, &where) || q.z < -0.5f) break; // ni detras de la boca (su luz, en z = -0,95)
        if (q.y > cave_rock_above(c, q.x, q.z, where, fy) - 0.4f || q.y < fy + 0.3f) break;
        ok = d;
    }
    cam->position = Vector3Add(target, Vector3Scale(back, ok));
    cam->target = target;
}

void cv_draw(GameActions *ga, const Combat *cb, Props *props, const Terrain *t, float world_time, float time) {
    const Cave *c = cave_at(t, g_cave);
    if (!c || cs_cave() != g_cave) return;
    uint64_t done = done_bits(ga);
    cs_gather_lights(c, done, g_in.pos, g_in.yaw, ga->torch_lit, world_time, time);
    hearth_frame_begin();
    cs_draw(c, g_in.pos, world_time); // la roca, la boca y el decorado (src/game/cave_scene.c)
    // Lo de adentro y el jugador, sin descartar las caras de atras (como antes). El lote de raylib se dibuja despues, al
    // llenarse o al cerrar el modo 3D, asi que se vacia antes de cambiar el estado (si no, el cambio llega tarde).
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    cp_draw(c, done, g_in.pos, time); // src/game/cave_props.c
    // El jugador, con la luz de donde esta (la llama de su antorcha, no).
    Color here = cs_shade(WHITE, (Vector3){ g_in.pos.x, g_in.pos.y + 1.0f, g_in.pos.z });
    props_set_tint(here);
    if (!ga_draw_player(ga, props, &g_in, time)) cb_draw_player_body(cb, ga, &g_in, time, here);
    props_set_tint(WHITE);
    ga_draw_held(ga, &g_in, time, here);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    hearth_draw_flames(time, 0.0f);
}

void cv_draw_plan(const Terrain *t, int cx, int cy, int r) {
    const Cave *c = cave_at(t, g_cave);
    if (!c) return;
    // La medida de la cueva: de la boca al fondo y de lado a lado.
    float minx = -2.0f, maxx = 2.0f, minz = -2.0f, maxz = 2.0f;
    for (int i = 0; i < c->room_count; i++) {
        const CaveRoom *rm = &c->rooms[i];
        minx = fminf(minx, rm->x - rm->radius), maxx = fmaxf(maxx, rm->x + rm->radius);
        minz = fminf(minz, rm->z - rm->radius), maxz = fmaxf(maxz, rm->z + rm->radius);
    }
    for (int i = 0; i < c->corridor_count; i++) // las vueltas de los pasillos, tambien
        for (int j = 0; j < CAVE_PATH_PTS; j++) {
            const CaveCorridor *cr = &c->corridors[i];
            minx = fminf(minx, cr->px[j] - 1.5f), maxx = fmaxf(maxx, cr->px[j] + 1.5f);
            minz = fminf(minz, cr->pz[j] - 1.5f), maxz = fmaxf(maxz, cr->pz[j] + 1.5f);
        }
    float span = fmaxf(maxx - minx, maxz - minz), k = (float)(2 * r - 12) / fmaxf(span, 1.0f);
    float ox = (minx + maxx) * 0.5f, oz = (minz + maxz) * 0.5f;
    DrawCircle(cx, cy, (float)r, (Color){ 14, 12, 12, 235 });
    DrawCircleLines(cx, cy, (float)r, UI_GOLD_DARK);
#define PLAN(px, pz) (Vector2){ (float)cx + ((px) - ox) * k, (float)cy - ((pz) - oz) * k }
    for (int i = 0; i < c->corridor_count; i++) {
        const CaveCorridor *cr = &c->corridors[i];
        bool seen = cr->from == CAVE_MOUTH || (cr->from >= 0 && ((g_seen >> cr->from) & 1u)) || (cr->to >= 0 && ((g_seen >> cr->to) & 1u));
        Color col = seen ? (cr->one_way ? (Color){ 150, 92, 60, 255 } : (Color){ 120, 108, 92, 255 }) : (Color){ 40, 36, 32, 255 };
        for (int j = 0; j < CAVE_PATH_PTS - 1; j++)
            DrawLineEx(PLAN(cr->px[j], cr->pz[j]), PLAN(cr->px[j + 1], cr->pz[j + 1]), fmaxf(1.0f, CAVE_CORRIDOR_HALF * 2.0f * k), col);
    }
    for (int i = 0; i < c->room_count; i++) {
        const CaveRoom *rm = &c->rooms[i];
        bool seen = (g_seen >> i) & 1u;
        Vector2 p = PLAN(rm->x, rm->z);
        DrawCircleV(p, rm->radius * k, seen ? (Color){ 128, 114, 96, 255 } : (Color){ 40, 36, 32, 255 });
    }
    DrawCircleV(PLAN(0.0f, CAVE_VEST_Z), CAVE_VEST_R * k, (Color){ 128, 114, 96, 255 }); // el vestibulo
    Vector2 mouth = PLAN(0.0f, 0.0f), me = PLAN(g_in.pos.x, g_in.pos.z);
    DrawRectangle((int)mouth.x - 3, (int)mouth.y - 1, 6, 3, UI_GOLD_LIGHT);
    DrawCircleV(me, 2.5f, UI_TURQUOISE);
    DrawLineV(me, (Vector2){ me.x + sinf(g_in.yaw) * 6.0f, me.y - cosf(g_in.yaw) * 6.0f }, UI_TURQUOISE);
#undef PLAN
}

void cv_draw_hud(const GameActions *ga, int w, int h) {
    if ((g_cave < 0 && g_mouth < 0) || g_loading >= 0 || ga->dlg.open || !g_hint[0]) {
        g_hint_rect = (Rectangle){ 0 };
        return;
    }
    int tw = MeasureText(g_hint, 10);
    g_hint_rect = (Rectangle){ (float)(w / 2 - tw / 2 - 6), (float)(h - 99), (float)(tw + 12), 16 };
    ui_text_centered(g_hint, w / 2, h - 96, 10, UI_GOLD);
}

void cv_draw_loading(const Terrain *t, int w, int h) {
    const Cave *c = cave_at(t, g_loading);
    DrawRectangle(0, 0, w, h, BLACK);
    char kind[64];
    snprintf(kind, sizeof(kind), "%s", c ? T(cave_kind_name(c->kind)) : "");
    ui_text_centered(T("Explorando la cueva..."), w / 2, h / 2 - 10, 20, UI_GOLD);
    if (kind[0]) ui_text_centered(kind, w / 2, h / 2 + 16, 10, UI_BONE_DIM);
    g_loading_drawn = true;
}

bool cv_test_explore(Player *p, const Terrain *t) {
    const World *w = t->world;
    int best = -1;
    float bd = 30.0f * 30.0f;
    for (int i = 0; i < w->cave_count && i < GA_CAVES; i++) {
        float dx = w->caves[i].x - p->pos.x, dz = w->caves[i].z - p->pos.z;
        if (dx * dx + dz * dz < bd) bd = dx * dx + dz * dz, best = i;
    }
    if (best < 0) return false;
    cave_to_world(&w->caves[best], 0.0f, -1.5f, &p->pos.x, &p->pos.z); // en el umbral, de cara a la boca
    p->pos.y = terrain_height(t, p->pos.x, p->pos.z);
    p->yaw = w->caves[best].yaw;
    g_loading = best, g_loading_drawn = false, g_loading_frames = 0;
    return true;
}

bool cv_test_enter(GameActions *ga, Player *p, const Terrain *t, const char *spec, float *cam_yaw) {
    static const char *KINDS[CAVE_KINDS] = { "animal", "abandonada", "forajidos", "tribu", "imperio" };
    static const struct { const char *name; CaveObjectKind kind; } SPOTS[] = {
        { "celda", COBJ_CELL },   { "dialogo", COBJ_CELL },  { "nido", COBJ_NEST },     { "huevos", COBJ_EGGS },  { "presa", COBJ_PREY },
        { "botin", COBJ_LOOT },   { "arma", COBJ_WEAPON },   { "cofre", COBJ_CHEST },   { "anfora", COBJ_AMPHORA }, { "deposito", COBJ_ARMS },
        { "pintura", COBJ_PAINTING }, { "fogata", COBJ_FIRE }, { "antorcha", COBJ_TORCH }, { "veta", COBJ_VEIN }, { "columna", COBJ_COLUMN },
        { "estalactita", COBJ_STALACTITE }, { "estalagmita", COBJ_STALAGMITE }, { "huesos", COBJ_BONES },
    };
    // Ante algo del decorado (src/sim/caves.h, cave_decor): las tareas de docs/PLAN_CUEVAS_GRANDES.md agregan aqui el suyo.
    static const struct { const char *name; CaveDecorKind kind; } DECOR_SPOTS[] = { { "roca", CDEC_ROCK } };
    char kind[24];
    snprintf(kind, sizeof(kind), "%s", spec);
    char *colon = strchr(kind, ':');
    const char *where = colon ? colon + 1 : "";
    if (colon) *colon = '\0';
    int want = -1; // el objeto pedido: la cueva mas cercana que lo tenga
    for (size_t k = 0; k < sizeof(SPOTS) / sizeof(SPOTS[0]); k++)
        if (!strcmp(where, SPOTS[k].name)) want = (int)SPOTS[k].kind;
    const World *w = t->world;
    int best = -1;
    float bd = 1e30f;
    for (int i = 0; i < w->cave_count && i < GA_CAVES; i++) {
        const Cave *c = &w->caves[i];
        bool has = want < 0;
        for (int o = 0; o < c->object_count && !has; o++)
            has = c->objects[o].kind == want && (want != COBJ_CELL || c->objects[o].prisoner);
        if (strcmp(KINDS[c->kind], kind) != 0 || !has) continue;
        float d = c->x * c->x + c->z * c->z;
        if (d < bd) bd = d, best = i;
    }
    if (best < 0) return false;
    const Cave *c = &w->caves[best];
    char tmp[256];
    if (!enter(ga, p, t, best, cam_yaw, tmp, sizeof(tmp))) return false;
    g_in.yaw = 0.0f, *cam_yaw = 0.0f; // como quien entra caminando: mirando hacia adentro
    if (!strcmp(where, "boca")) { // junto a la boca, de cara a la salida
        g_in.pos.z = 1.6f;
        g_in.yaw = PI, *cam_yaw = PI;
        return true;
    }
    if (!strncmp(where, "pasillo", 7)) { // a mitad de un pasillo (pasillo1: el 1; de serie, el 1), mirando a lo largo
        int k = where[7] ? atoi(where + 7) : 1;
        if (k < 0 || k >= c->corridor_count) return true;
        float x, z, y, dx, dz;
        cave_corridor_at(c, k, 0.5f, &x, &z, &y, &dx, &dz);
        g_in.pos = (Vector3){ x, y, z };
        g_in.yaw = atan2f(dx, dz), *cam_yaw = g_in.yaw;
        return true;
    }
    if (where[0] >= '0' && where[0] <= '9') { // al borde de la sala, mirando su centro
        int room = atoi(where);
        if (room < 0 || room >= c->room_count) return true;
        const CaveRoom *rm = &c->rooms[room];
        float a = atan2f(-rm->x, -rm->z), d = rm->radius * 0.7f;
        g_in.pos = (Vector3){ rm->x + sinf(a) * d, rm->floor_y, rm->z + cosf(a) * d };
        g_in.yaw = a + PI;
        *cam_yaw = g_in.yaw;
        g_seen |= 1u << room;
        return true;
    }
    for (size_t k = 0; k < sizeof(DECOR_SPOTS) / sizeof(DECOR_SPOTS[0]); k++) { // ante lo primero de ese decorado, mirandolo
        if (strcmp(where, DECOR_SPOTS[k].name) != 0) continue;
        int nd = 0;
        const CaveDecor *d = cs_decor(&nd);
        for (int i = 0; i < nd; i++) {
            if (d[i].kind != DECOR_SPOTS[k].kind) continue;
            const CaveRoom *rm = &c->rooms[d[i].room];
            float a = atan2f(rm->x - d[i].x, rm->z - d[i].z), dd = d[i].radius + 1.6f; // del lado del centro de la sala
            g_in.pos = (Vector3){ d[i].x + sinf(a) * dd, rm->floor_y, d[i].z + cosf(a) * dd };
            g_in.yaw = a + PI;
            *cam_yaw = g_in.yaw + 0.6f;
            g_seen |= 1u << d[i].room;
            return true;
        }
        return true;
    }
    for (size_t k = 0; k < sizeof(SPOTS) / sizeof(SPOTS[0]); k++) { // ante el primer objeto de esa clase, mirandolo de costado
        if (strcmp(where, SPOTS[k].name) != 0) continue;
        for (int i = 0; i < c->object_count; i++) {
            const CaveObject *o = &c->objects[i];
            if (o->kind != SPOTS[k].kind || (o->kind == COBJ_CELL && !o->prisoner)) continue;
            const CaveRoom *rm = &c->rooms[o->room];
            Vector3 at = obj_pos(c, i);
            float a = atan2f(rm->x - at.x, rm->z - at.z), d = o->kind == COBJ_CELL ? 1.5f : 1.6f; // hacia el centro de la sala
            g_in.pos = (Vector3){ at.x + sinf(a) * d, rm->floor_y, at.z + cosf(a) * d };
            g_in.yaw = a + PI;
            *cam_yaw = g_in.yaw + 0.6f;
            g_seen |= 1u << o->room;
            if (!strcmp(where, "dialogo")) open_prisoner(ga, c, i);
            return true;
        }
    }
    return true;
}

void cv_test_press(GameActions *ga, Combat *cb, Troop *troop, const Props *props, Player *p, const Terrain *t, char *log, size_t len) {
    const Cave *c = cave_at(t, g_cave);
    if (!c) return;
    if (ga->dlg.open && ga->talk_mode == TALK_PRISONER && g_talk >= 0) { // como elegir «Romper las cadenas»
        dlg_close(&ga->dlg);
        free_prisoner(ga, cb, troop, c, g_talk, log, len);
        g_talk = -1;
        return;
    }
    g_obj = nearest_object(ga, c);
    if (g_obj >= 0) interact(ga, troop, props, p, c, g_obj, log, len);
}
