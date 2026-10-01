#include "game/actions_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"
#include "ui/theme.h"

// Equipo de prueba del jugador (Fase 0): todo lo necesario para probar las acciones.
static const char *KIT[] = {
    "arma.corta.sable",          "arma.corta.daga",         "escudo.mano.mimbre",    "escudo.mano.cuero",
    "arma.larga.guja",           "arma.larga.lanza",        "arma.distancia.arco_compuesto",
    "utileria.objeto.antorcha",  "arma.distancia.lazo",     "utileria.herramienta.gancho_trepa",
    "utileria.herramienta.pala", "accesorio.arreo.silla_montar",
};
// Empunaduras que recorre "cambiar empunadura" (X): derecha, izquierda.
static const char *PRESETS[][2] = {
    { "arma.corta.sable", "escudo.mano.mimbre" },        // arma y escudo
    { "arma.corta.sable", "arma.corta.daga" },           // una en cada mano
    { "arma.larga.guja", NULL },                         // a dos manos
    { "arma.distancia.arco_compuesto", NULL },           // arco (dos manos)
    { "arma.larga.lanza", "escudo.mano.cuero" },         // lanza y escudo
    { NULL, NULL },                                      // desarmado
};
#define PRESET_COUNT ((int)(sizeof(PRESETS) / sizeof(PRESETS[0])))
#define REACH 2.2f        // m: alcance para tomar objetos
#define TARGET_RANGE 14.0f // m: alcance de la trepa y el lazo
#define HELP_RANGE 8.0f   // m: el jugador ayuda en una obra si esta cerca

static bool has_item(const char *id) {
    for (size_t i = 0; i < sizeof(KIT) / sizeof(KIT[0]); i++)
        if (!strcmp(KIT[i], id)) return true;
    return false;
}

static const char *item_name(const GameActions *ga, const char *id) {
    const InvItem *it = inventory_find(ga->inv, id);
    return it ? it->name : id;
}

static void apply_preset(GameActions *ga) {
    hands_clear(&ga->hands, HAND_RIGHT);
    hands_clear(&ga->hands, HAND_LEFT);
    ga->torch_lit = false;
    const char *r = PRESETS[ga->preset][0], *l = PRESETS[ga->preset][1];
    if (r) hands_equip(&ga->hands, inventory_find(ga->inv, r), HAND_RIGHT);
    if (l) hands_equip(&ga->hands, inventory_find(ga->inv, l), HAND_LEFT);
}

static Vector3 forward_of(float yaw) { return (Vector3){ sinf(yaw), 0.0f, cosf(yaw) }; }

static Vector3 ground_ahead(const Terrain *t, const Player *p, float dist) {
    Vector3 f = forward_of(p->yaw);
    float x = p->pos.x + f.x * dist, z = p->pos.z + f.z * dist;
    return (Vector3){ x, terrain_height(t, x, z), z };
}

void ga_init(GameActions *ga, const Inventory *inv, Props *props, const Terrain *t) {
    memset(ga, 0, sizeof(*ga));
    ga->inv = inv;
    ga->doing = -1;
    hands_init(&ga->hands);
    apply_preset(ga);
    // Objetos sueltos junto a la entrada del campamento, para probar tomar y lanzar,
    // y un tramo de empalizada para la trepa.
    static const struct {
        const char *id;
        float x, z, yaw;
    } START[] = {
        { "utileria.objeto.lena", 2.0f, 17.0f, 0.3f },   { "utileria.objeto.lena", 2.8f, 17.6f, 1.2f },
        { "utileria.objeto.odre", -2.2f, 17.4f, 0.0f },  { "utileria.objeto.cofre", -3.5f, 15.5f, 0.6f },
        { "utileria.objeto.caldero", 1.4f, 3.0f, 0.0f }, { "estructura.campamento.empalizada", 9.0f, 14.0f, 1.57f },
    };
    for (size_t i = 0; i < sizeof(START) / sizeof(START[0]); i++)
        props_add(props, START[i].id, (Vector3){ START[i].x, terrain_height(t, START[i].x, START[i].z), START[i].z },
                  START[i].yaw);
}

bool ga_menu_open(const GameActions *ga) { return ga->menu_open; }

// Un muro al frente, a tiro de trepa.
static bool wall_ahead(const Props *props, const Player *p) {
    Vector3 f = forward_of(p->yaw);
    for (int i = 0; i < props->count; i++) {
        const char *id = props->items[i].item->id;
        if (!strstr(id, "empalizada") && !strstr(id, "muro") && !strstr(id, "muralla")) continue;
        Vector3 d = Vector3Subtract(props->items[i].pos, p->pos);
        float dist = sqrtf(d.x * d.x + d.z * d.z);
        if (dist < TARGET_RANGE && (d.x * f.x + d.z * f.z) > dist * 0.5f) return true; // dentro de ~60 grados
    }
    return false;
}

static bool any_prop_in_category(const Props *props, const Player *p, const char *prefix) {
    for (int i = 0; i < props->count; i++)
        if (!strncmp(props->items[i].item->id, prefix, strlen(prefix)) &&
            Vector3Distance(props->items[i].pos, p->pos) < TARGET_RANGE)
            return true;
    return false;
}

// Comprueba requisitos y arranca una accion con duracion.
static void start_action(GameActions *ga, ActionId a, const Props *props, const Player *p, char *log, size_t len) {
    const ActionDef *d = action_def(a);
    if (ga->doing >= 0) return;
    if (d->requires && !has_item(d->requires)) {
        snprintf(log, len, "Te falta: %s.", item_name(ga, d->requires));
        return;
    }
    switch (a) {
    case ACTION_TAKE:
        if (ga->hands.carried[0]) {
            snprintf(log, len, "Ya llevas algo: T para lanzarlo.");
            return;
        }
        if (!hands_can_take(&ga->hands)) {
            snprintf(log, len, "Necesitas una mano libre (H para enfundar).");
            return;
        }
        if (props_nearest(props, p->pos, REACH, true) < 0) {
            snprintf(log, len, "No hay nada que tomar al alcance.");
            return;
        }
        break;
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
        if (!wall_ahead(props, p)) {
            snprintf(log, len, "No hay un muro a tiro de trepa delante.");
            return;
        }
        break;
    case ACTION_THROW_LASSO:
        if (!any_prop_in_category(props, p, "animal.salvaje.")) {
            snprintf(log, len, "No hay animales salvajes cerca.");
            return;
        }
        break;
    case ACTION_SADDLE:
        if (!any_prop_in_category(props, p, "animal.montura.")) {
            snprintf(log, len, "No hay una montura cerca para ensillar.");
            return;
        }
        break;
    default: break;
    }
    ga->doing = a;
    ga->timer = 0.0f;
}

static void finish_action(GameActions *ga, Props *props, const Terrain *t, const Player *p, char *log, size_t len) {
    ActionId a = (ActionId)ga->doing;
    const ActionDef *d = action_def(a);
    ga->doing = -1;
    switch (a) {
    case ACTION_TAKE: {
        int i = props_nearest(props, p->pos, REACH, true);
        if (i >= 0 && hands_take(&ga->hands, props->items[i].item->id)) {
            snprintf(log, len, "Tomaste: %s.", props->items[i].item->name);
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
        snprintf(log, len, "La trepa se engancha en el muro. (Trepar: Fase 1)");
        break;
    case ACTION_THROW_LASSO: snprintf(log, len, "Lanzaste el lazo."); break;
    case ACTION_SADDLE: snprintf(log, len, "Montura instalada."); break;
    default:
        if (d->produces) { // instalar fogata, tienda, cavar trinchera
            props_add(props, d->produces, ground_ahead(t, p, 2.5f), p->yaw);
            snprintf(log, len, "Hecho: %s.", item_name(ga, d->produces));
        }
        break;
    }
}

static void start_build(GameActions *ga, BuildId b, const Terrain *t, const Player *p, char *log, size_t len) {
    if (ga->project_count >= GA_MAX_PROJECTS) {
        snprintf(log, len, "Demasiadas obras a la vez.");
        return;
    }
    Vector3 at = ground_ahead(t, p, 6.0f);
    ga->projects[ga->project_count++] = (BuildProject){ b, at.x, at.z, 0.0f, false };
    snprintf(log, len, "Obra iniciada: %s.", build_def(b)->name);
}

void ga_update(GameActions *ga, Props *props, const Terrain *t, const Player *p, const Troop *troop, float dt,
               char *log, size_t log_len) {
    // Menu de acciones.
    if (IsKeyPressed(KEY_TAB)) ga->menu_open = !ga->menu_open;
    if (ga->menu_open) {
        const int total = ACTION_COUNT + BUILD_COUNT;
        if (IsKeyPressed(KEY_DOWN)) ga->cursor = (ga->cursor + 1) % total;
        if (IsKeyPressed(KEY_UP)) ga->cursor = (ga->cursor + total - 1) % total;
        if (IsKeyPressed(KEY_ENTER)) {
            ga->menu_open = false;
            if (ga->cursor < ACTION_COUNT) start_action(ga, (ActionId)ga->cursor, props, p, log, log_len);
            else start_build(ga, (BuildId)(ga->cursor - ACTION_COUNT), t, p, log, log_len);
        }
    } else if (ga->doing < 0) {
        // Atajos.
        if (IsKeyPressed(KEY_X)) start_action(ga, ACTION_CHANGE_GRIP, props, p, log, log_len);
        if (IsKeyPressed(KEY_H)) start_action(ga, ACTION_SHEATHE, props, p, log, log_len);
        if (IsKeyPressed(KEY_F)) start_action(ga, ACTION_TAKE, props, p, log, log_len);
        if (IsKeyPressed(KEY_T)) start_action(ga, ACTION_THROW, props, p, log, log_len);
    }

    // Accion en curso.
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

    // Obras en grupo: la tribu trabaja; el jugador ayuda si esta cerca.
    for (int i = 0; i < ga->project_count; i++) {
        BuildProject *bp = &ga->projects[i];
        bool near = Vector2Distance((Vector2){ p->pos.x, p->pos.z }, (Vector2){ bp->x, bp->z }) < HELP_RANGE;
        CrewPlan plan = build_plan(build_def(bp->def), troop, near);
        if (build_advance(bp, plan.rate, dt)) {
            const BuildDef *d = build_def(bp->def);
            props_add(props, d->produces, (Vector3){ bp->x, terrain_height(t, bp->x, bp->z), bp->z }, 0.0f);
            snprintf(log, log_len, "Obra terminada: %s.", d->name);
            ga->projects[i--] = ga->projects[--ga->project_count];
        }
    }
}

// Objeto empunado: caja con las medidas del inventario, en coordenadas locales del jugador
// (+Z adelante, +X a la izquierda).
static void draw_held(const InvItem *it, Vector3 local, bool shield) {
    if (!it) return;
    Vector3 size = shield ? (Vector3){ it->w, it->h, fmaxf(it->l, 0.06f) }
                          : (Vector3){ fmaxf(it->w, 0.04f), it->h, fmaxf(it->l, 0.04f) };
    Vector3 c = { local.x, local.y + size.y * 0.5f, local.z };
    DrawCubeV(c, size, (Color){ 170, 170, 175, 255 });
    DrawCubeWiresV(c, size, (Color){ 60, 55, 50, 255 });
}

void ga_draw_world(GameActions *ga, Props *props, const Terrain *t, const Player *p, float time) {
    props_draw(props);
    for (int i = 0; i < ga->project_count; i++) {
        const BuildProject *bp = &ga->projects[i];
        const InvItem *it = inventory_find(ga->inv, build_def(bp->def)->produces);
        if (it) props_draw_item(props, it, (Vector3){ bp->x, terrain_height(t, bp->x, bp->z), bp->z }, 0.0f, bp->progress);
    }

    // Lo que el jugador tiene en las manos.
    rlPushMatrix();
    rlTranslatef(p->pos.x, p->pos.y, p->pos.z);
    rlRotatef(p->yaw * RAD2DEG, 0, 1, 0);
    const Hands *h = &ga->hands;
    const InvItem *r = h->right.id[0] ? inventory_find(ga->inv, h->right.id) : NULL;
    const InvItem *l = h->left.id[0] ? inventory_find(ga->inv, h->left.id) : NULL;
    if (h->sheathed) { // a la cintura y a la espalda
        draw_held(r, (Vector3){ -0.32f, 0.35f, -0.1f }, false);
        draw_held(l, (Vector3){ 0.0f, 0.6f, -0.3f }, l && l->hands == INV_HANDS_SHIELD);
    } else if (h->right.kind == INV_HANDS_TWO) {
        draw_held(r, (Vector3){ 0.0f, 0.5f, 0.45f }, false);
    } else {
        draw_held(r, (Vector3){ -0.45f, 0.75f, 0.2f }, false);
        draw_held(l, (Vector3){ 0.5f, 0.7f, 0.3f }, l && l->hands == INV_HANDS_SHIELD);
        if (ga->torch_lit && l) { // llama de la antorcha
            float flick = 0.8f + 0.2f * sinf(time * 17.0f);
            DrawSphere((Vector3){ 0.5f, 0.7f + l->h + 0.08f, 0.3f }, 0.11f * flick, (Color){ 250, 160, 50, 255 });
        }
    }
    if (h->carried[0]) { // objeto tomado: sobre los brazos
        const InvItem *c = inventory_find(ga->inv, h->carried);
        if (c) {
            Vector3 size = { fmaxf(c->l, 0.1f), fmaxf(c->h, 0.1f), fmaxf(c->w, 0.1f) };
            DrawCubeV((Vector3){ 0, 1.15f, 0.45f }, size, (Color){ 190, 150, 100, 255 });
            DrawCubeWiresV((Vector3){ 0, 1.15f, 0.45f }, size, (Color){ 80, 60, 40, 255 });
        }
    }
    rlPopMatrix();
}

const char *ga_hands_text(const GameActions *ga) {
    static char buf[96];
    const Hands *h = &ga->hands;
    snprintf(buf, sizeof(buf), "Empuñe: %s%s%s%s", grip_name(hands_grip(h)), h->sheathed ? " (enfundado)" : "",
             ga->torch_lit ? ", antorcha" : "", h->carried[0] ? ", lleva algo" : "");
    return buf;
}

static void draw_menu(const GameActions *ga, const Troop *troop, int width) {
    const int w = 460, h = 312, x0 = (width - w) / 2, y0 = 22, pad = UI_PANEL_INSET + 3;
    const int list_x = x0 + pad, list_w = 200, desc_x = list_x + list_w + 10, desc_w = w - 2 * pad - list_w - 10;
    ui_panel((Rectangle){ (float)x0, (float)y0, (float)w, (float)h }, UI_METAL_GOLD);
    ui_text("Acciones", list_x, y0 + pad, 10, UI_GOLD_LIGHT);
    ui_text("Flechas: elegir · Enter: hacer · Tab: cerrar", desc_x, y0 + pad, 10, UI_BONE_DIM);
    int y = y0 + pad + 14;
    for (int i = 0; i < ACTION_COUNT + BUILD_COUNT; i++) {
        if (i == 0 || i == ACTION_COUNT) {
            ui_text(i == 0 ? "UNA PERSONA" : "CONSTRUCCIONES EN GRUPO", list_x, y, 10, UI_TURQUOISE);
            y += 12;
        }
        const char *name = i < ACTION_COUNT ? action_def((ActionId)i)->name : build_def((BuildId)(i - ACTION_COUNT))->name;
        if (i == ga->cursor) DrawRectangle(list_x - 2, y - 1, list_w, 11, (Color){ 26, 110, 116, 200 });
        ui_text(name, list_x + 4, y, 10, i == ga->cursor ? UI_BONE : UI_BONE_DIM);
        y += 11;
    }
    // Detalle del elemento elegido.
    int dy = y0 + pad + 26;
    if (ga->cursor < ACTION_COUNT) {
        const ActionDef *d = action_def((ActionId)ga->cursor);
        ui_text(d->name, desc_x, dy, 10, UI_GOLD_LIGHT);
        dy += 14;
        dy += ui_text_wrapped(d->desc, desc_x, dy, desc_w, 10, UI_BONE) + 6;
        ui_text(TextFormat("Duración: %.1f s", d->seconds), desc_x, dy, 10, UI_BONE_DIM);
        dy += 12;
        if (d->requires) {
            const InvItem *it = inventory_find(ga->inv, d->requires);
            dy += ui_text_wrapped(TextFormat("Requiere: %s", it ? it->name : d->requires), desc_x, dy, desc_w, 10,
                                  has_item(d->requires) ? UI_BONE_DIM : UI_CARNELIAN);
        }
        if (d->target) ui_text(TextFormat("Sobre: %s", d->target), desc_x, dy, 10, UI_BONE_DIM);
        ui_text("También la pueden hacer los NPCs.", desc_x, y0 + h - pad - 10, 10, UI_BONE_DIM);
    } else {
        const BuildDef *d = build_def((BuildId)(ga->cursor - ACTION_COUNT));
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
        dy += ui_text_wrapped(TextFormat("Materiales: %s", d->materials), desc_x, dy, desc_w, 10, UI_BONE_DIM) + 6;
        ui_divider(desc_x, dy, desc_w, UI_METAL_GOLD);
        dy += 6;
        if (plan.check == BUILD_READY) {
            ui_text(TextFormat("Tu tribu: %d trabajarían", plan.workers), desc_x, dy, 10, UI_TURQUOISE);
            dy += 12;
            ui_text(TextFormat("Tiempo estimado: %.0f s de juego", d->work / plan.rate), desc_x, dy, 10, UI_TURQUOISE);
        } else {
            dy += ui_text_wrapped(TextFormat("No se puede: %s", build_check_text(d, plan.check)), desc_x, dy, desc_w,
                                  10, UI_CARNELIAN);
        }
        ui_text("Se levanta 6 m delante de ti.", desc_x, y0 + h - pad - 10, 10, UI_BONE_DIM);
    }
}

void ga_draw_hud(const GameActions *ga, const Troop *troop, int width, int height) {
    // Accion en curso.
    if (ga->doing >= 0) {
        const ActionDef *d = action_def((ActionId)ga->doing);
        int w = 160, x = (width - w) / 2, y = height - 64;
        ui_text_centered(TextFormat("%s...", d->name), width / 2, y - 12, 10, UI_BONE);
        ui_bar(x, y, w, ga->timer / d->seconds, UI_TURQUOISE, UI_METAL_GOLD);
    }
    // Obras en curso.
    if (ga->project_count > 0) {
        int x = width - 236, y = 128, w = 230, h = 20 + 12 * ga->project_count;
        ui_panel((Rectangle){ (float)x, (float)y, (float)w, (float)h + UI_PANEL_INSET }, UI_METAL_SILVER);
        for (int i = 0; i < ga->project_count; i++) {
            const BuildProject *bp = &ga->projects[i];
            const BuildDef *d = build_def(bp->def);
            CrewPlan plan = build_plan(d, troop, false);
            Color c = plan.check == BUILD_READY ? UI_BONE : UI_CARNELIAN;
            ui_text(TextFormat("%s %d%%", d->name, (int)(bp->progress * 100.0f)), x + UI_PANEL_INSET, y + UI_PANEL_INSET + 12 * i, 10, c);
        }
    }
    if (ga->menu_open) draw_menu(ga, troop, width);
}
