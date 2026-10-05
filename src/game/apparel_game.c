#include "game/apparel_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "sim/apparel.h"
#include "sim/clock.h"
#include "sim/hazards.h"
#include "sim/lang.h"
#include "ui/icons.h"
#include "ui/theme.h"

#define DRESS_EVERY 4.0f   // s entre repasos de la ropa de la gente
#define COLD_FEEL 2.0f     // sensacion termica bajo la que un NPC pasa frio
#define HOT_FEEL 33.0f     // y sobre la que pasa calor
#define SAY_RANGE 25.0f    // se avisa en el registro si se cambia cerca del jugador

static const WearSlot ORDER[WEAR_COUNT] = { WEAR_CLOAK, WEAR_BODY, WEAR_HEAD, WEAR_FEET, WEAR_FACE };

// La gente que vive en un campamento y esta en el: puede usar la ropa del acopio.
static Stockpile *home_stock(GameActions *ga, const Member *m, Vector3 pos) {
    if (m->camp < 0 || m->camp >= CAMPS_MAX || !ga->camps[m->camp].used) return NULL;
    const CampSite *c = &ga->camps[m->camp];
    float dx = pos.x - c->x, dz = pos.z - c->z;
    return dx * dx + dz * dz < CAMP_RADIUS_M * CAMP_RADIUS_M ? &ga->camps[m->camp].stock : NULL;
}

// Repasa la ropa de un integrante. Devuelve la prenda nueva mas visible que se puso (o -1).
static int dress(GameActions *ga, Member *m, Stockpile *stock, float temp, float wind, float sun) {
    float need = comfort_need(temp, wind, sun);
    int shown = -1;
    for (int k = 0; k < WEAR_COUNT; k++) {
        WearSlot s = ORDER[k];
        int cands[24], from[24], n = 0; // from: 0 puesta, 1 mochila, 2 acopio
        if (m->outfit.g[s] >= 0) cands[n] = m->outfit.g[s], from[n++] = 0;
        for (int i = 0; i < m->bag.n && n < 24; i++) {
            int g = garment_find(m->bag.s[i].id);
            if (g >= 0 && garment(g)->slot == s) cands[n] = g, from[n++] = 1;
        }
        for (int i = 0; stock && i < stock->n && n < 24; i++) {
            int g = garment_find(stock->e[i].id);
            if (g >= 0 && garment(g)->slot == s && stock->e[i].count > 0) cands[n] = g, from[n++] = 2;
        }
        int pick = garment_pick(s, need, sun, cands, n);
        int want = pick >= 0 ? cands[pick] : -1;
        int worn = m->outfit.g[s];
        if (want != worn && !(pick >= 0 && from[pick] == 0)) {
            // Lo que se quita va a su mochila (o al acopio); si no cabe en ningun lado, se lo deja puesto.
            bool off = worn < 0 || bag_add(&m->bag, ga->inv, garment(worn)->id, 1, 1.0f) == 1 ||
                       (stock && stock_add(stock, garment(worn)->id, 1));
            if (off) {
                m->outfit.g[s] = -1;
                if (want >= 0) {
                    bool got = from[pick] == 1 ? bag_take(&m->bag, garment(want)->id, 1, NULL) == 1 : stock_take(stock, garment(want)->id, 1);
                    if (got) {
                        m->outfit.g[s] = (signed char)want;
                        if (shown < 0 || s == WEAR_CLOAK) shown = want;
                    }
                }
            }
        }
        if (m->outfit.g[s] >= 0) need -= garment(m->outfit.g[s])->warmth;
    }
    return shown;
}

void ag_update(GameActions *ga, Troop *troop, const Climate *c, unsigned seed, float world_time, float dt, char *log, size_t len) {
    if ((ga->dress_t += dt) < DRESS_EVERY) return;
    float step = ga->dress_t;
    ga->dress_t = 0.0f;
    float sun_h = clock_sun_height(world_time);
    bool said = false;
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        Member *m = &troop->members[i];
        Npc *n = &ga->npcs[i];
        if (m->status != STATUS_ACTIVE || n->member_id != m->id || m->journey > 0) continue;
        float desert = biome_desert(seed, n->pos.x, n->pos.z);
        float temp = local_temperature(c->temperature, sun_h, desert) - (ga->terrain ? fmaxf(0.0f, n->pos.y - ga->terrain->plain) * 0.07f : 0.0f);
        float sun = sun_strength(sun_h, c->clouds, desert);
        Stockpile *stock = home_stock(ga, m, n->pos);
        int put = m->health.down ? -1 : dress(ga, m, stock, temp, c->wind, sun);
        // En su campamento, las yurtas y el fuego abrigan (y dan sombra).
        float feels = stock ? apparel_feels_like(temp, c->wind * 0.5f, 0.0f, 0.0f, &m->outfit, 6.0f, 0.0f) : npc_feels_like(temp, c->wind, sun, &m->outfit);
        n->comfort = (signed char)(feels < COLD_FEEL ? -1 : feels > HOT_FEEL ? 1 : 0);
        // Sin ropa adecuada se pasa mal: baja la moral (poco a poco).
        if (n->comfort) m->morale = fmaxf(0.0f, m->morale - 0.25f * step / 60.0f * fminf(3.0f, 1.0f + fabsf(feels - (n->comfort < 0 ? COLD_FEEL : HOT_FEEL)) / 6.0f));
        if (put >= 0 && !said && Vector3Distance(n->pos, (Vector3){ ga->player_pos.x, n->pos.y, ga->player_pos.z }) < SAY_RANGE) {
            const InvItem *it = inventory_find(ga->inv, garment(put)->id);
            snprintf(log, len, T("%s se pone: %s."), m->name, it ? T(it->name) : garment(put)->id);
            said = true;
        }
    }
}

float ag_dread(const GameActions *ga, const Troop *troop, int member_id) {
    if (member_id == 0) return outfit_dread(&ga->outfit);
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
        if (troop->members[i].id == member_id) return outfit_dread(&troop->members[i].outfit);
    return 0.0f;
}

void ag_draw_overlay(const GameActions *ga, const Troop *troop, Camera3D cam, int w, int h) {
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        const Member *m = &troop->members[i];
        const Npc *n = &ga->npcs[i];
        if (m->status != STATUS_ACTIVE || n->member_id != m->id || m->journey > 0 || !n->comfort || m->health.down) continue;
        Vector3 pos = { n->pos.x, n->pos.y + 2.25f, n->pos.z };
        Vector3 to = Vector3Subtract(pos, cam.position);
        if (Vector3DotProduct(to, Vector3Subtract(cam.target, cam.position)) <= 0.0f || Vector3Length(to) > 30.0f) continue;
        Vector2 s = GetWorldToScreenEx(pos, cam, w, h);
        ui_icon(n->comfort < 0 ? ICON_FRIO : ICON_SOL, s.x - 8, s.y - 8, 16, n->comfort < 0 ? UI_TURQ_LIGHT : UI_GOLD_LIGHT);
    }
}
