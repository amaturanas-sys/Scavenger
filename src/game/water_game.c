#include "game/water_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/gems_game.h"
#include "game/inventory_game.h"
#include "sim/lang.h"
#include "ui/icons.h"
#include "ui/theme.h"

#define ODRE_ID "utileria.objeto.odre"
#define RAW_ID "utileria.consumible.agua"
#define BOILED_ID "utileria.consumible.agua_hervida"
#define PER_ODRE 3
#define DRY_FAINT 20.0f // s con la sed a cero, hasta desmayarse

int wg_water_room(GameActions *ga, const Props *props, const Player *p) {
    int odres = ig_count(ga, props, p, ODRE_ID);
    int room = odres * PER_ODRE - ig_count(ga, props, p, RAW_ID) - ig_count(ga, props, p, BOILED_ID);
    return room > 0 ? room : 0;
}

bool wg_water_near(const Terrain *t, float x, float z) {
    if (!t) return false;
    for (float r = 0.0f; r <= 60.0f; r += 15.0f)
        for (int k = 0; k < (r > 0.0f ? 8 : 1); k++) {
            float a = (float)k * 0.785398f, px = x + cosf(a) * r, pz = z + sinf(a) * r;
            if (terrain_water(t, px, pz) > terrain_height(t, px, pz) + 0.1f) return true;
        }
    return false;
}

// Lo mas seguro primero; la cruda al final (con riesgo).
static const DrinkKind ORDER[] = { DRINK_BOILED, DRINK_WATERED_WINE, DRINK_AIRAG, DRINK_BEER, DRINK_MILK, DRINK_WINE, DRINK_RAW };

static void drink(GameActions *ga, const Props *props, Player *p, const Hazards *hz, const Climate *c, char *log, size_t len) {
    if (ga->hydro.water >= THIRST_MAX - 2.0f) {
        snprintf(log, len, "%s", T("No tienes sed."));
        return;
    }
    float chance = water_spirit_chance(c->temp_mean, true);
    for (size_t k = 0; k < sizeof(ORDER) / sizeof(ORDER[0]); k++) {
        const DrinkDef *d = drink_def(ORDER[k]);
        if (ig_count(ga, props, p, d->id) <= 0) continue;
        ig_use(ga, props, p, d->id, 1);
        DrunkLevel before = drunk_level(&ga->hydro);
        hydration_drink(&ga->hydro, ORDER[k], chance, &ga->rng);
        const InvItem *it = inventory_find(ga->inv, d->id);
        const char *name = it ? T(it->name) : d->id;
        if (drunk_level(&ga->hydro) > before && drunk_level(&ga->hydro) >= DRUNK_DRUNK)
            snprintf(log, len, T("Bebes: %s. Estás %s: torpe con las armas y te cansas antes."), name, drunk_name(drunk_level(&ga->hydro)));
        else if (d->raw)
            snprintf(log, len, T("Bebes: %s. El agua cruda puede traer espíritus malditos: mejor hervida."), name);
        else
            snprintf(log, len, T("Bebes: %s."), name);
        return;
    }
    // Sin nada que beber: del rio o del lago, si estas en la orilla.
    bool at_water = (ga->terrain && gems_water_ahead(ga->terrain, p)) || hz->wading || hz->swimming;
    if (at_water) {
        hydration_drink(&ga->hydro, DRINK_RAW, chance, &ga->rng);
        snprintf(log, len, "%s", T("Bebes del agua que corre por el suelo. Puede traer espíritus malditos..."));
        return;
    }
    snprintf(log, len, "%s", T("No tienes nada que beber: llena el odre en la orilla (Tab) o bebe del río (N, mirando al agua)."));
}

static void herbs(GameActions *ga, const Props *props, const Player *p, char *log, size_t len) {
    if (ga->hydro.curse <= 0.0f) {
        snprintf(log, len, "%s", T("No tienes los espíritus del agua: guarda las hierbas."));
        return;
    }
    const char *id = ig_count(ga, props, p, "utileria.consumible.hierbas") > 0 ? "utileria.consumible.hierbas"
                     : ig_count(ga, props, p, "utileria.consumible.unguento") > 0 ? "utileria.consumible.unguento"
                                                                                   : NULL;
    if (!id) {
        snprintf(log, len, "%s", T("No tienes hierbas para echar a los espíritus. Descansa y bebe agua hervida."));
        return;
    }
    ig_use(ga, props, p, id, 1);
    hydration_herbs(&ga->hydro);
    snprintf(log, len, "%s", ga->hydro.curse > 0.0f ? T("Las hierbas alivian la fiebre.") : T("Las hierbas echan a los espíritus malditos."));
}

void wg_update(GameActions *ga, Troop *troop, Player *p, const Props *props, const Hazards *hz, const Climate *c, Vector3 camp_fire,
               const Terrain *t, bool input_ok, float dt, char *log, size_t len) {
    bool was_sick = hydration_sick(&ga->hydro);
    ThirstLevel was_thirst = thirst_level(&ga->hydro);
    hydration_update(&ga->hydro, heat_thirst_scale(&hz->heat) * (p->stance == STANCE_RUN && p->moving ? 1.3f : 1.0f), !p->moving, dt);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT), ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (input_ok && !ctrl && IsKeyPressed(KEY_N)) {
        if (shift) herbs(ga, props, p, log, len);
        else drink(ga, props, p, hz, c, log, len);
    }
    if (!was_sick && hydration_sick(&ga->hydro))
        snprintf(log, len, "%s", T("Fiebre y retortijones: espíritus malditos del agua. Toma hierbas (Mayús+N), descansa y bebe agua hervida."));
    else if (thirst_level(&ga->hydro) > was_thirst && thirst_level(&ga->hydro) >= THIRST_THIRSTY)
        snprintf(log, len, T("Estás %s: bebe (N)."), thirst_name(thirst_level(&ga->hydro)));
    // Borracho: se tambalea al andar.
    float clumsy = hydration_clumsy(&ga->hydro);
    if (clumsy > 0.2f && p->moving && ga->mounted < 0) {
        float sway = sinf((float)GetTime() * 1.9f) * clumsy * 0.9f * dt;
        p->pos.x += cosf(p->yaw) * sway;
        p->pos.z -= sinf(p->yaw) * sway;
    }
    // Sin agua: desmayo; la tribu lo lleva al campamento y le da de beber (el jugador no muere).
    if (ga->hydro.water <= 0.0f) {
        if ((ga->dry_t += dt) > DRY_FAINT) {
            ga->dry_t = 0.0f;
            p->pos = (Vector3){ camp_fire.x + 2.0f, terrain_height(t, camp_fire.x + 2.0f, camp_fire.z - 2.0f), camp_fire.z - 2.0f };
            p->vy = 0.0f;
            ga->hydro.water = 60.0f;
            troop_adjust_morale(troop, -3.0f);
            snprintf(log, len, "%s", T("Te desmayaste de sed; la tribu te llevó al campamento y te dio de beber (moral -3)."));
        }
    } else {
        ga->dry_t = 0.0f;
    }
}

float wg_speed_scale(const GameActions *ga) { return hydration_speed_scale(&ga->hydro); }
float wg_clumsy(const GameActions *ga) { return hydration_clumsy(&ga->hydro); }
float wg_stamina(const GameActions *ga) { return hydration_stamina_scale(&ga->hydro); }

void wg_draw_hud(const GameActions *ga, int right_x, int y) {
    const Hydration *h = &ga->hydro;
    ThirstLevel tl = thirst_level(h);
    DrunkLevel dl = drunk_level(h);
    const char *txt = TextFormat("%s%s%s", thirst_name(tl), dl > DRUNK_SOBER ? TextFormat(" · %s", drunk_name(dl)) : "",
                                 hydration_sick(h) ? T(" · fiebre") : "");
    Color col = tl >= THIRST_PARCHED || hydration_sick(h) ? UI_CARNELIAN : tl == THIRST_THIRSTY ? UI_GOLD_LIGHT : UI_TURQ_LIGHT;
    int tw = MeasureText(txt, 10);
    ui_text(txt, right_x - tw, y, 10, col);
    IconId icon = hydration_sick(h) ? ICON_ESPIRITUS : dl >= DRUNK_DRUNK ? ICON_JARRA : ICON_BEBER;
    ui_icon(icon, (float)(right_x - tw - 15), (float)y - 3, 16, col);
    ui_bar(right_x - 92, y + 12, 92, h->water / THIRST_MAX, UI_TURQUOISE, UI_METAL_SILVER);
}
