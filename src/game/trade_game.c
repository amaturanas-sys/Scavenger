#include "game/trade_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/city_game.h"
#include "game/input.h"
#include "game/inventory_game.h"
#include "raylib.h"
#include "rlgl.h"
#include "sim/anim_index.h"
#include "sim/clock.h"
#include "sim/lang.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "world/body_draw.h"

#define TALK_CARAVAN 8.0f   // m: F junto a una caravana
#define TALK_SERAI 14.0f    // al caravasar
#define TALK_VILLAGE 22.0f  // al centro de una aldea
#define TALK_CAPITAL 30.0f  // al de una capital (dentro de la muralla)
#define TALK_TRIBE 18.0f    // al fuego de una tribu
#define LOOT_CAMP 20.0f     // m del fuego de un campamento rival para saquearlo
#define LOOT_CLEAR 40.0f    // sin enemigos en pie a esta distancia
#define LOOT_REARM 600.0f   // alejarse rearma el campamento rival
#define RAID_WIN 120.0f     // m: cerca del asalto para quedarse con la carga
#define RAID_LOST 200.0f    // lejos: la caravana sigue su camino
#define RAID_CLEAR 60.0f    // ningun escolta en pie a esta distancia del asalto (ni a 40 m del jugador)
#define RAID_RELATION 10.0f // lo que baja la relacion con el reino
#define CARAVAN_REST (2.0f * GAME_SECONDS_PER_DAY) // asaltada, tarda en volver a salir
#define CARAVAN_DRAW 300.0f
#define RIDER_LIFT 1.1f
#define SELL_PAGE (DLG_OPTIONS - 3) // con «Más» y «Volver»
#define WARES_MAX 64

typedef enum { TD_ROOT = 300, TD_SELL, TD_BUY, TD_CITY } TradeTalk;

// Donde va cada caravana este cuadro (lo pone td_update).
typedef struct {
    float x, z, yaw;
    bool shown;  // en el camino (no esta descansando tras un asalto)
    bool halted; // detenida: comerciando o asaltada
} Caravan;

typedef struct {
    int kind, idx; // MarketKind (-1: un campamento rival que saquear; -2: los campos de una ciudad) y su indice
    Region region;
} Market;

typedef struct {
    char id[INV_ID_LEN];
    int count, price;
} Ware;

static CaravanRoute g_routes[CARAVANS_MAX];
static int g_nroutes;
static const World *g_routes_world;
static uint32_t g_routes_seed; // world_for_seed reusa sus huecos: el mismo puntero puede ser otro mundo
static Caravan g_car[CARAVANS_MAX];
static bool g_talk_req; // F pidio comerciar (lo atiende td_update)
static char g_hint[96];  // «F: comerciar...» al alcance

static float dist_xz(float ax, float az, float bx, float bz) { return sqrtf((ax - bx) * (ax - bx) + (az - bz) * (az - bz)); }

static float route_len(const CaravanRoute *r) { return dist_xz(r->ax, r->az, r->bx, r->bz); }

static void ensure_routes(const World *w) {
    if (g_routes_world == w && (!w || g_routes_seed == w->seed)) return;
    g_routes_world = w;
    g_routes_seed = w ? w->seed : 0;
    g_nroutes = w ? caravan_routes(w, g_routes, CARAVANS_MAX) : 0;
}

// La demora que deja a la caravana en su caravasar en el instante t (de alli sale de nuevo).
static float delay_at_serai(const CaravanRoute *r, float t) {
    float len = route_len(r);
    if (len < 1.0f) return 0.0f;
    float s = fmodf(t * CARAVAN_SPEED + r->offset, 2.0f * len);
    if (s < 0.0f) s += 2.0f * len;
    return s / CARAVAN_SPEED;
}

static const char *item_name(const GameActions *ga, const char *id) {
    const InvItem *it = inventory_find(ga->inv, id);
    return it ? T(it->name) : id;
}

// Enemigos en pie (ni muertos, ni abatidos, ni rehenes) a menos de r de (x, z).
static int standing_near(const Combat *cb, float x, float z, float r) {
    int n = 0;
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        const Enemy *e = &cb->enemies[i];
        if (!e->used || e->state == EN_DEAD || e->state == EN_DOWN || e->state == EN_HOSTAGE) continue;
        n += dist_xz(e->pos.x, e->pos.z, x, z) < r;
    }
    return n;
}

// Lo que hay al alcance de F: 1 un mercado, 2 un campamento rival que saquear, 3 los campos de una
// ciudad o una aldea (saquearlos, src/game/city_game.h), 0 nada.
static int market_near(const GameActions *ga, const Terrain *t, const Player *p, Market *m) {
    const World *w = t->world;
    float px = p->pos.x, pz = p->pos.z;
    for (int i = 0; i < g_nroutes; i++) {
        const Caravan *c = &g_car[i];
        if (!c->shown || ga->caravan_raid == i + 1 || dist_xz(c->x, c->z, px, pz) > TALK_CARAVAN) continue;
        *m = (Market){ MARKET_CARAVAN, i, world_region(w, c->x, c->z) };
        return 1;
    }
    for (int i = 0; i < w->tribe_count && i < 32; i++) {
        const TribeCamp *tc = &w->tribes[i];
        if (tc->attitude != TRIBE_RIVAL || dist_xz(tc->x, tc->z, px, pz) > LOOT_CAMP) continue;
        if (!((ga->raided_tribes >> i) & 1u) || ((ga->tribe_looted >> i) & 1u)) continue; // sus guerreros aun no salieron, o ya se saqueo
        *m = (Market){ -1, i, tc->region };
        return 2;
    }
    for (int i = 0; i < w->site_count && i < 64; i++) {
        const WorldSite *s = &w->sites[i];
        if (s->kind != SITE_CARAVANSERAI || dist_xz(s->x, s->z, px, pz) > TALK_SERAI) continue;
        *m = (Market){ MARKET_CARAVAN, 100 + i, s->region };
        return 1;
    }
    for (int i = 0; i < w->settlement_count; i++) {
        const Settlement *s = &w->settlements[i];
        bool capital = s->kind == SETTLE_CAPITAL;
        if (dist_xz(s->x, s->z, px, pz) > (capital ? TALK_CAPITAL : TALK_VILLAGE)) continue;
        *m = (Market){ capital ? MARKET_CAPITAL : MARKET_VILLAGE, i, s->region };
        return 1;
    }
    for (int i = 0; i < w->tribe_count && i < 32; i++) {
        const TribeCamp *tc = &w->tribes[i];
        if (tc->attitude == TRIBE_RIVAL || dist_xz(tc->x, tc->z, px, pz) > TALK_TRIBE) continue;
        *m = (Market){ MARKET_TRIBE, i, tc->region };
        return 1;
    }
    int fi = ct_field_at(ga, t, px, pz, NULL);
    if (fi >= 0) {
        *m = (Market){ -2, fi, w->settlements[fi].region };
        return 3;
    }
    return 0;
}

static bool loot_at_hand(const GameActions *ga, const Player *p) {
    for (int i = 0; i < GA_LOOT; i++)
        if (ga->loot_age[i] >= 0.0f && dist_xz(ga->loot_pos[i].x, ga->loot_pos[i].z, p->pos.x, p->pos.z) < 2.5f) return true;
    return false;
}

bool td_try_talk(GameActions *ga, const Terrain *t, const Player *p) {
    if (!t || !t->world || loot_at_hand(ga, p)) return false; // el botin a tus pies se toma primero
    ensure_routes(t->world);
    Market m;
    if (!market_near(ga, t, p, &m)) return false;
    g_talk_req = true;
    return true;
}

// La pista de F: con quien se comercia (o que se saquea) aqui.
static void update_hint(const GameActions *ga, const Terrain *t, const Player *p) {
    const World *w = t->world;
    g_hint[0] = '\0';
    Market m;
    int what = ga->dlg.open || ga->menu_open || ga->inv_open || loot_at_hand(ga, p) ? 0 : market_near(ga, t, p, &m);
    if (what == 3) snprintf(g_hint, sizeof(g_hint), T("F: saquear los campos de %s"), w->settlements[m.idx].name);
    else if (what == 2) snprintf(g_hint, sizeof(g_hint), T("F: saquear el campamento de los %s"), w->tribes[m.idx].name);
    else if (what == 1 && m.kind == MARKET_TRIBE) snprintf(g_hint, sizeof(g_hint), T("F: comerciar con los %s"), w->tribes[m.idx].name);
    else if (what == 1 && m.kind != MARKET_CARAVAN) snprintf(g_hint, sizeof(g_hint), T("F: comerciar en %s"), w->settlements[m.idx].name);
    else if (what == 1)
        snprintf(g_hint, sizeof(g_hint), "%s", m.idx >= 100 ? T("F: comerciar en el caravasar") : T("F: comerciar con la caravana (o asaltarla)"));
}

void td_draw_hud(int w, int h) {
    if (g_hint[0]) ui_text_centered(g_hint, w / 2, h - 96, 10, UI_GOLD);
}

// ------------------------------------------------------------------ saqueo
static void loot_camp(GameActions *ga, const Combat *cb, const World *w, int i, const Player *p, char *log, size_t len) {
    const TribeCamp *tc = &w->tribes[i];
    if (standing_near(cb, p->pos.x, p->pos.z, LOOT_CLEAR)) {
        snprintf(log, len, T("Aún quedan guerreros de los %s en pie: no puedes saquear."), tc->name);
        return;
    }
    int silver = 15 + rng_range(&ga->rng, 21);
    const LootItem items[] = { { "utileria.material.cuero", 2, 1.0f },        { "utileria.material.fieltro", 1, 1.0f },
                               { "utileria.consumible.carne_seca", 4, 1.0f }, { "utileria.material.pieles", 3, 1.0f },
                               { SILVER_ID, silver, 1.0f } };
    if (!ig_drop_loot(ga, p->pos, items, (int)(sizeof(items) / sizeof(items[0])))) return;
    ga->tribe_looted |= 1u << i;
    snprintf(log, len, T("Saqueas a los %s: %d de plata y más, a tus pies (F)."), tc->name, silver);
}

// Los campos de una ciudad o una aldea (src/sim/city.h): se llevan el grano de las eras (a tus pies),
// la cosecha queda arrasada y sale su guardia. Les queda rencor; si es del kanato, el kan se entera.
static void raid_fields(GameActions *ga, Combat *cb, Troop *troop, const Terrain *t, int idx, const Player *p, int day, char *log, size_t len) {
    const Settlement *s = &t->world->settlements[idx];
    City *c = &ga->cities[idx];
    int take = city_raid_fields(c, clock_season(day), day), field = 0;
    if (take < 0) {
        snprintf(log, len, T("Los campos de %s ya están arrasados: no queda nada que llevarse."), s->name);
        return;
    }
    if (ct_field_at(ga, t, p->pos.x, p->pos.z, &field) == idx) c->raided_lot = field + 1; // desde este, quemados
    int sacks = take / GRAIN_SACK_RATIONS;
    if (sacks > 0) {
        LootItem li = { GRAIN_SACK_ID, sacks, 1.0f };
        ig_drop_loot(ga, p->pos, &li, 1);
    }
    Vector3 gate = ct_gate_toward(t, idx, p->pos.x, p->pos.z);
    int guards = cb_spawn_city_guard(cb, t, idx, city_guard_party(c, s->kind == SETTLE_CAPITAL ? 6 : 3), gate, p->pos);
    if (s->region == REGION_STEPPE && troop->overlord) troop->overlord->relation = fmaxf(-100.0f, troop->overlord->relation - RAID_RELATION);
    if (guards > 0) snprintf(log, len, T("¡Campos de %s arrasados: %d sacos de grano (F)! Salen %d soldados."), s->name, sacks, guards);
    else snprintf(log, len, T("¡Campos de %s arrasados: %d sacos de grano (F)! Nadie los defiende."), s->name, sacks);
}

static void start_raid(GameActions *ga, Combat *cb, Troop *troop, const Terrain *t, int i, char *log, size_t len) {
    const Caravan *c = &g_car[i];
    ga->caravan_raid = i + 1;
    ga->caravan_raid_at = (Vector3){ c->x, terrain_height(t, c->x, c->z), c->z };
    char tmp[160];
    cb_spawn_party_at(cb, "caravana", ga->caravan_raid_at, t, tmp, sizeof(tmp));
    if (troop->overlord) {
        troop->overlord->relation = fmaxf(-100.0f, troop->overlord->relation - RAID_RELATION);
        snprintf(log, len, T("¡Asaltas la caravana! El %s se enterará (−%d)."), T(troop->overlord->name), (int)RAID_RELATION);
    } else {
        snprintf(log, len, "%s", tmp);
    }
    dlg_close(&ga->dlg);
}

// El asalto en curso: vencida la escolta, la carga queda en el suelo; si te alejas, la caravana sigue.
static void tick_raid(GameActions *ga, const Combat *cb, const Player *p, float now, char *log, size_t len) {
    if (ga->caravan_raid <= 0) return;
    int i = ga->caravan_raid - 1;
    if (i >= g_nroutes) {
        ga->caravan_raid = 0;
        return;
    }
    Vector3 at = ga->caravan_raid_at;
    float d = dist_xz(at.x, at.z, p->pos.x, p->pos.z);
    if (d > RAID_LOST) {
        ga->caravan_raid = 0;
        snprintf(log, len, "%s", T("Te alejas: la caravana sigue su camino."));
        return;
    }
    if (d > RAID_WIN || standing_near(cb, at.x, at.z, RAID_CLEAR) || standing_near(cb, p->pos.x, p->pos.z, 40.0f)) return;
    int silver = 30 + rng_range(&ga->rng, 31);
    const LootItem items[] = { { "utileria.material.seda", 3, 1.0f }, { "utileria.consumible.especias", 2, 1.0f },
                               { "utileria.consumible.vino", 2, 1.0f }, { SILVER_ID, silver, 1.0f },
                               { "utileria.material.oro", 1, 1.0f },  { "utileria.gema.turquesa", 1, 1.0f } };
    ig_drop_loot(ga, at, items, (int)(sizeof(items) / sizeof(items[0])));
    ga->caravan_raid = 0;
    ga->caravan_rest[i] = now + CARAVAN_REST;
    ga->caravan_delay[i] = delay_at_serai(&g_routes[i], ga->caravan_rest[i]); // vuelve a salir de su caravasar
    snprintf(log, len, T("Cae la escolta: la carga queda en el suelo (%d de plata y más). F: tomarla."), silver);
}

// ------------------------------------------------------------------ comercio
static void set_mode(GameActions *ga, int mode) {
    ga->talk_mode = mode;
    ga->talk_a = 0; // pagina
    ga->dlg.cursor = 0;
}

// La ciudad o aldea con la que se comercia (src/sim/city.h), o NULL: su granero y su armeria mueven
// los precios, y su tesoro paga lo que le vendes (y cobra lo que le compras).
static City *market_city(GameActions *ga) {
    bool town = ga->market_kind == MARKET_VILLAGE || ga->market_kind == MARKET_CAPITAL;
    return town && ga->market_idx >= 0 && ga->market_idx < SETTLEMENTS_MAX ? &ga->cities[ga->market_idx] : NULL;
}

static int city_price(const GameActions *ga, const char *id, int base) {
    const City *c = market_city((GameActions *)ga);
    if (!c || base <= 0) return base;
    int v = (int)lroundf((float)base * city_price_factor(c, id));
    return v < 1 ? 1 : v;
}

static int sell_price(const GameActions *ga, const char *id) { return city_price(ga, id, trade_sell_price(id, (Region)ga->market_region)); }
static int buy_price(const GameActions *ga, const char *id) { return city_price(ga, id, trade_buy_price(id, (Region)ga->market_region)); }

// ¿Comercian contigo? Una ciudad con mucho rencor, no; el kanato, con la relacion por el suelo, tampoco.
static bool market_trades(GameActions *ga, const Troop *troop) {
    const City *c = market_city(ga);
    float rel = troop && troop->overlord ? troop->overlord->relation : 0.0f;
    return !c || city_trades(c, ga->market_region == REGION_STEPPE, rel);
}

// Lo que llevas y te compran aqui (sin la plata), lo mas caro primero.
static int sell_list(GameActions *ga, const Props *props, const Player *p, Ware *out, int max) {
    const char *ids[WARES_MAX];
    int counts[WARES_MAX];
    int n = ig_carried_list(ga, props, p, ids, counts, WARES_MAX), k = 0;
    for (int i = 0; i < n && k < max; i++) {
        int price = strcmp(ids[i], SILVER_ID) ? sell_price(ga, ids[i]) : 0;
        if (price <= 0) continue;
        snprintf(out[k].id, sizeof(out[k].id), "%s", ids[i]);
        out[k].count = counts[i], out[k].price = price;
        k++;
    }
    for (int i = 1; i < k; i++) // insercion (estable): el orden no salta al vender uno
        for (int j = i; j > 0 && (out[j].price > out[j - 1].price || (out[j].price == out[j - 1].price && strcmp(out[j].id, out[j - 1].id) < 0)); j--) {
            Ware tmp = out[j];
            out[j] = out[j - 1], out[j - 1] = tmp;
        }
    return k;
}

// Lo que vende el mercado y existe en el inventario.
static int buy_list(const GameActions *ga, const char **out, int max) {
    const char *all[32];
    int n = market_goods((MarketKind)ga->market_kind, (Region)ga->market_region, all, 32), k = 0;
    for (int i = 0; i < n && k < max; i++)
        if (inventory_find(ga->inv, all[i]) && buy_price(ga, all[i]) > 0) out[k++] = all[i];
    return k;
}

// La plata que cobras: a la bolsa, y lo que no cabe, a tus pies.
static void pay(GameActions *ga, const Props *props, const Player *p, const char *id, int n) {
    int kept = ig_store(ga, props, p, id, n, 1.0f);
    if (kept < n) {
        LootItem li = { id, n - kept, 1.0f };
        ig_drop_loot(ga, p->pos, &li, 1);
    }
}

static const char *price_note(const GameActions *ga, const char *id) {
    const City *c = market_city((GameActions *)ga);
    float f = price_factor(id, (Region)ga->market_region) * (c ? city_price_factor(c, id) : 1.0f);
    return f < 0.99f ? T(" · aquí abunda") : f > 1.01f ? T(" · aquí escasea") : "";
}

static void market_speaker(const GameActions *ga, const World *w, char *out, size_t n, IconId *icon) {
    int i = ga->market_idx;
    *icon = ICON_BALANZA;
    switch (ga->market_kind) {
    case MARKET_TRIBE:
        *icon = ICON_TRIBU;
        snprintf(out, n, T("Los %s"), i < w->tribe_count ? w->tribes[i].name : "?");
        break;
    case MARKET_VILLAGE:
    case MARKET_CAPITAL:
        *icon = ICON_PERSONA;
        snprintf(out, n, "%s", i < w->settlement_count ? w->settlements[i].name : "?");
        break;
    default:
        *icon = ICON_CAMELLO;
        snprintf(out, n, "%s", i >= 100 ? T("El caravasar") : T("Una caravana"));
        break;
    }
}

// ¿Es la capital del kanato de la estepa? Alli se paga el tributo.
static bool overlord_capital(const GameActions *ga) { return ga->market_kind == MARKET_CAPITAL && ga->market_region == REGION_STEPPE; }

static void build_dialog(GameActions *ga, const Props *props, const Player *p, const Terrain *t, const Troop *troop, int day) {
    const World *w = t->world;
    Dialog *d = &ga->dlg;
    Region r = (Region)ga->market_region;
    char speaker[48];
    IconId icon;
    market_speaker(ga, w, speaker, sizeof(speaker), &icon);
    int silver = ig_carried_count(ga, props, p, SILVER_ID);
    Ware wares[WARES_MAX];
    int nw = sell_list(ga, props, p, wares, WARES_MAX);
    const City *city = market_city(ga);
    bool trades = market_trades(ga, troop);
    switch (ga->talk_mode) {
    case TD_ROOT: {
        int worth = 0;
        for (int i = 0; i < nw; i++) worth += wares[i].price * wares[i].count;
        const char *what = ga->market_kind == MARKET_CARAVAN && ga->market_idx < 100
                               ? T("Seda, especias y vino de tierras lejanas; la escolta no les quita el ojo a los camellos.")
                               : "";
        if (city && !trades)
            what = city->grudge >= CITY_GRUDGE_NO_TRADE ? T("No comercian con quien les arrasó los campos.")
                                                        : T("No comercian con quien no paga el tributo al kan.");
        else if (city)
            what = TextFormat(T("Su tesoro tiene %d de plata para pagarte."), (int)city->silver);
        dlg_begin(d, speaker, icon,
                  TextFormat(T("Llevas %d de plata; lo que cargas vale %d aquí.\nEn las tierras del %s, lo que abunda vale menos y lo que escasea, más. %s"),
                             silver, worth, world_kingdom_name(r), what));
        dlg_option(d, ICON_MONEDAS, T("Vender"), T("Lo que llevas encima, en las alforjas, la carreta o las mochilas de la escolta: te pagan en plata."),
                   nw > 0 && trades, 1);
        dlg_option(d, ICON_ALFORJAS, T("Comprar"), T("Lo suyo, a pagar en plata."), trades, 2);
        if (ga->market_kind == MARKET_CARAVAN && ga->market_idx < 100)
            dlg_option(d, ICON_SABLE, T("Asaltar la caravana"), T("Su escolta pelea; vencida, la carga es tuya. El reino se enterará."), true, 3);
        if (city) dlg_option(d, ICON_ESTRUCTURA, T("La ciudad"), T("Su gente, el granero, el tesoro y la guarnición."), true, 4);
        if (overlord_capital(ga) && troop && troop->overlord) {
            int due = tribute_due(troop_count_with_status(troop, STATUS_ACTIVE));
            bool paid = ga->tribute_season == season_index(day) + 1;
            dlg_option(d, ICON_MONEDAS, TextFormat(T("Pagar el tributo (%d de plata)"), due),
                       paid ? T("Ya pagaste el de esta estación.")
                            : TextFormat(T("La relación con el kan sube (+%d). Si una estación pasa sin tributo, baja."), (int)TRIBUTE_RELATION),
                       !paid && silver >= due, 5);
        }
        dlg_option(d, ICON_TITULO, T("Adiós"), "", true, 0);
        break;
    }
    case TD_CITY: {
        char report[640];
        ct_report(ga, t, troop, ga->market_idx, day, report, sizeof(report));
        dlg_begin(d, speaker, icon, report);
        dlg_option(d, ICON_TITULO, T("Volver"), "", true, 0);
        break;
    }
    case TD_SELL: {
        int pages = (nw + SELL_PAGE - 1) / SELL_PAGE;
        if (ga->talk_a >= pages) ga->talk_a = 0;
        dlg_begin(d, speaker, icon, TextFormat(T("Llevas %d de plata. Enter o clic: vendes uno; con Mayús, todos."), silver));
        for (int i = ga->talk_a * SELL_PAGE; i < nw && i < (ga->talk_a + 1) * SELL_PAGE; i++) {
            DlgOption *o = dlg_option(d, icon_for_item(wares[i].id), item_name(ga, wares[i].id),
                                      TextFormat(T("%d de plata cada uno · llevas %d%s"), wares[i].price, wares[i].count, price_note(ga, wares[i].id)),
                                      true, 100 + i);
            if (o && wares[i].count > 1) snprintf(o->badge, sizeof(o->badge), "%.7s", TextFormat("%d", wares[i].count));
        }
        if (pages > 1) dlg_option(d, ICON_CONTINUAR, TextFormat(T("Más (%d/%d)"), ga->talk_a + 1, pages), "", true, 1);
        dlg_option(d, ICON_TITULO, T("Volver"), "", true, 0);
        break;
    }
    case TD_BUY: {
        const char *goods[DLG_OPTIONS - 1];
        int ng = buy_list(ga, goods, DLG_OPTIONS - 1);
        dlg_begin(d, speaker, icon, TextFormat(T("Llevas %d de plata. Enter o clic: compras uno; con Mayús, cinco."), silver));
        for (int i = 0; i < ng; i++) {
            int price = buy_price(ga, goods[i]);
            DlgOption *o = dlg_option(d, icon_for_item(goods[i]), item_name(ga, goods[i]), TextFormat(T("%d de plata%s"), price, price_note(ga, goods[i])),
                                      silver >= price, 100 + i);
            if (o) snprintf(o->badge, sizeof(o->badge), "%.7s", TextFormat("%d", price)); // el precio, a la vista
        }
        dlg_option(d, ICON_TITULO, T("Volver"), "", true, 0);
        break;
    }
    default: break;
    }
}

static void sell(GameActions *ga, const Props *props, const Player *p, const Ware *w, bool all, char *log, size_t len) {
    int want = all ? w->count : 1;
    City *c = market_city(ga); // una ciudad paga con su tesoro: si no le alcanza, compra menos
    if (c && (float)(want * w->price) > c->silver) want = (int)(c->silver / (float)w->price);
    if (want <= 0) {
        snprintf(log, len, "%s", T("El tesoro de la ciudad no tiene plata para pagarte."));
        return;
    }
    int n = ig_take_carried(ga, props, p, w->id, want);
    if (n <= 0) return;
    if (c) {
        c->silver -= (float)(n * w->price);
        if (!strcmp(w->id, GRAIN_SACK_ID)) c->grain += (float)(n * GRAIN_SACK_RATIONS); // el grano, a su granero
    }
    pay(ga, props, p, SILVER_ID, n * w->price);
    snprintf(log, len, T("Vendes %d × %s por %d de plata."), n, item_name(ga, w->id), n * w->price);
}

static void buy(GameActions *ga, const Props *props, const Player *p, const char *id, bool five, char *log, size_t len) {
    int price = buy_price(ga, id), have = ig_carried_count(ga, props, p, SILVER_ID);
    int n = five ? 5 : 1;
    if (price > 0 && n * price > have) n = have / price;
    if (n <= 0 || price <= 0) {
        snprintf(log, len, T("No te alcanza: cuesta %d de plata y llevas %d."), price, have);
        return;
    }
    ig_take_carried(ga, props, p, SILVER_ID, n * price);
    City *c = market_city(ga);
    if (c) c->silver += (float)(n * price); // lo que pagas, a su tesoro
    pay(ga, props, p, id, n);
    snprintf(log, len, T("Compras %d × %s por %d de plata."), n, item_name(ga, id), n * price);
}

// El tributo de esta estacion: la plata, al tesoro del kan; la relacion sube.
static void pay_tribute(GameActions *ga, const Props *props, const Player *p, Troop *troop, int day, char *log, size_t len) {
    if (!troop->overlord || ga->tribute_season == season_index(day) + 1) return;
    int due = tribute_due(troop_count_with_status(troop, STATUS_ACTIVE)), have = ig_carried_count(ga, props, p, SILVER_ID);
    if (have < due) {
        snprintf(log, len, T("No te alcanza: el tributo es de %d de plata y llevas %d."), due, have);
        return;
    }
    ig_take_carried(ga, props, p, SILVER_ID, due);
    City *c = market_city(ga);
    if (c) c->silver += (float)due;
    ga->tribute_season = season_index(day) + 1;
    troop->overlord->relation = fminf(100.0f, troop->overlord->relation + TRIBUTE_RELATION);
    snprintf(log, len, T("Pagas el tributo al %s: %d de plata. La relación sube (+%d)."), T(troop->overlord->name), due, (int)TRIBUTE_RELATION);
}

static void choose(GameActions *ga, Combat *cb, Troop *troop, const Props *props, const Terrain *t, const Player *p, int v, int day, char *log,
                   size_t len) {
    bool shift = (input_mods() & KM_SHIFT) != 0;
    switch (ga->talk_mode) {
    case TD_ROOT:
        if (v == 0) dlg_close(&ga->dlg);
        else if (v == 1) set_mode(ga, TD_SELL);
        else if (v == 2) set_mode(ga, TD_BUY);
        else if (v == 3 && ga->market_kind == MARKET_CARAVAN && ga->market_idx < g_nroutes) start_raid(ga, cb, troop, t, ga->market_idx, log, len);
        else if (v == 4) set_mode(ga, TD_CITY);
        else if (v == 5) pay_tribute(ga, props, p, troop, day, log, len);
        break;
    case TD_CITY: set_mode(ga, TD_ROOT); break;
    case TD_SELL: {
        if (v == 0) {
            set_mode(ga, TD_ROOT);
            break;
        }
        if (v == 1) {
            ga->talk_a++, ga->dlg.cursor = 0;
            break;
        }
        Ware wares[WARES_MAX];
        int nw = sell_list(ga, props, p, wares, WARES_MAX);
        if (v >= 100 && v - 100 < nw) sell(ga, props, p, &wares[v - 100], shift, log, len);
        break;
    }
    case TD_BUY: {
        if (v == 0) {
            set_mode(ga, TD_ROOT);
            break;
        }
        const char *goods[DLG_OPTIONS - 1];
        int ng = buy_list(ga, goods, DLG_OPTIONS - 1);
        if (v >= 100 && v - 100 < ng) buy(ga, props, p, goods[v - 100], shift, log, len);
        break;
    }
    default: break;
    }
}

static void open_market(GameActions *ga, Combat *cb, Troop *troop, const Terrain *t, const Player *p, int day, char *log, size_t len) {
    Market m;
    int what = market_near(ga, t, p, &m);
    if (what == 2) loot_camp(ga, cb, t->world, m.idx, p, log, len);
    if (what == 3) raid_fields(ga, cb, troop, t, m.idx, p, day, log, len);
    if (what != 1) return;
    ga->market_kind = m.kind;
    ga->market_idx = m.idx;
    ga->market_region = (int)m.region;
    set_mode(ga, TD_ROOT);
    ga->dlg.open = true;
}

void td_update(GameActions *ga, Combat *cb, Troop *troop, const Props *props, const Terrain *t, const Player *p, float now, float dt,
               char *log, size_t len) {
    if (!t || !t->world) return;
    const World *w = t->world;
    ensure_routes(w);
    bool trading = ga->dlg.open && ga->talk_mode >= TD_ROOT && ga->talk_mode < TD_ROOT + 100;
    // Las caravanas: se detienen mientras se comercia con ellas o las asaltan.
    for (int i = 0; i < CARAVANS_MAX; i++) {
        Caravan *c = &g_car[i];
        c->shown = i < g_nroutes && now >= ga->caravan_rest[i];
        c->halted = ga->caravan_raid == i + 1 || (trading && ga->market_kind == MARKET_CARAVAN && ga->market_idx == i);
        if (!c->shown) continue;
        if (c->halted) {
            float period = 2.0f * route_len(&g_routes[i]) / CARAVAN_SPEED;
            ga->caravan_delay[i] += dt;
            if (period > 1.0f && ga->caravan_delay[i] > period) ga->caravan_delay[i] -= period; // va y vuelve: lo mismo
        }
        caravan_at(&g_routes[i], now - ga->caravan_delay[i], &c->x, &c->z, &c->yaw);
    }
    tick_raid(ga, cb, p, now, log, len);
    // Los campamentos rivales saqueados se rearman al alejarse.
    for (int i = 0; i < w->tribe_count && i < 32; i++)
        if (((ga->tribe_looted >> i) & 1u) && dist_xz(w->tribes[i].x, w->tribes[i].z, p->pos.x, p->pos.z) > LOOT_REARM)
            ga->tribe_looted &= ~(1u << i);
    if (g_talk_req) {
        g_talk_req = false;
        if (!ga->dlg.open) open_market(ga, cb, troop, t, p, clock_day(now), log, len);
    }
    update_hint(ga, t, p);
    if (!ga->dlg.open || ga->talk_mode < TD_ROOT || ga->talk_mode >= TD_ROOT + 100) return;
    build_dialog(ga, props, p, t, troop, clock_day(now));
    int v = dlg_update(&ga->dlg);
    if (v == DLG_BACK) {
        if (ga->talk_mode == TD_ROOT) dlg_close(&ga->dlg);
        else set_mode(ga, TD_ROOT);
    } else if (v >= 0) {
        choose(ga, cb, troop, props, t, p, v, clock_day(now), log, len);
    }
    if (ga->dlg.open) build_dialog(ga, props, p, t, troop, clock_day(now));
}

// ------------------------------------------------------------------ dibujo
static float ground_at(const Terrain *t, float x, float z) {
    float y = terrain_height(t, x, z), wl = terrain_water(t, x, z);
    return wl != TERRAIN_NO_WATER && wl - 0.8f > y ? wl - 0.8f : y; // vadean: el agua al vientre
}

// Un bulto atado al lomo (en el espacio del animal: x al costado, z adelante).
static void bundle(Vector3 at, float yaw, Vector3 local, Vector3 size, Color c) {
    rlPushMatrix();
    rlTranslatef(at.x, at.y, at.z);
    rlRotatef(yaw * RAD2DEG, 0, 1, 0);
    DrawCube(local, size.x, size.y, size.z, c);
    DrawCubeWires(local, size.x, size.y, size.z, (Color){ 70, 52, 36, 255 });
    rlPopMatrix();
}

static void draw_camel(const GameActions *ga, Props *props, Vector3 at, float yaw, bool moving, float time, int k) {
    const SpeciesDef *d = species_def(SPECIES_CAMEL);
    const InvItem *it = inventory_find(ga->inv, d->model);
    float s = d->size / 1.3f, bob = 0.0f; // las medidas del cuerpo de capsulas (src/sim/animals.c: animal_body)
    if (it && props_has_model(props, it)) {
        // El modelo mira a +X (Kiln); el animal avanza segun yaw (0 = +Z).
        props_draw_item_anim(props, it, at, yaw - PI / 2.0f, anim_quadruped_fight(moving ? CARAVAN_SPEED : 0.0f, false, false, false),
                             time + 0.4f * (float)k);
    } else { // sin modelo: el cuerpo de capsulas de la fauna, con las dos jorobas del bactriano
        Animal a;
        animal_init(&a, SPECIES_CAMEL, at.x, at.z);
        a.yaw = yaw;
        BodyPose b;
        animal_body(&a, &b);
        Color c = { 142, 104, 66, 255 };
        bob = moving ? 0.04f * sinf(time * 5.0f + (float)k) : 0.0f;
        V3 o = { at.x, at.y + bob, at.z };
        for (int sg = 0; sg < PART_COUNT; sg++) {
            const BodySeg *g = &b.seg[sg];
            if (g->radius <= 0.0f) continue;
            V3 wa = body_to_world(g->a, o, yaw), wb = body_to_world(g->b, o, yaw);
            DrawCapsule((Vector3){ wa.x, wa.y, wa.z }, (Vector3){ wb.x, wb.y, wb.z }, g->radius, 5, 2, sg == PART_HEAD ? ColorBrightness(c, -0.15f) : c);
        }
        for (int h = 0; h < 2; h++) {
            V3 hp = body_to_world((V3){ 0.0f, 0.76f * s, h ? -0.18f * s : 0.24f * s }, o, yaw);
            DrawSphere((Vector3){ hp.x, hp.y, hp.z }, 0.13f * s, ColorBrightness(c, -0.08f));
        }
    }
    // La carga: un fardo a cada costado y uno entre las jorobas, de tela y cuero.
    static const Color CLOTH[3] = { { 150, 44, 40, 255 }, { 52, 84, 140, 255 }, { 196, 150, 60, 255 } };
    Vector3 o = { at.x, at.y + bob, at.z };
    float flank = 0.2f * s + 0.16f, mid = 0.55f * s;
    bundle(o, yaw, (Vector3){ flank, mid, 0.03f * s }, (Vector3){ 0.3f, 0.55f, 0.75f }, CLOTH[k % 3]);
    bundle(o, yaw, (Vector3){ -flank, mid, 0.03f * s }, (Vector3){ 0.3f, 0.55f, 0.75f }, CLOTH[(k + 1) % 3]);
    bundle(o, yaw, (Vector3){ 0.0f, 0.8f * s, 0.03f * s }, (Vector3){ 0.5f, 0.3f, 0.45f }, (Color){ 112, 78, 48, 255 });
}

static void draw_person(Vector3 at, float yaw, bool walking, float crouch, float time, Color cloth) {
    BodyPose b;
    BodyPoseParams pp = { .walk_phase = time * 6.0f, .walk = walking ? 1.0f : 0.0f, .scale = 1.0f, .crouch = crouch };
    body_pose(&b, &pp);
    body_draw(&b, at, yaw, (BodyColors){ { 196, 150, 112, 255 }, cloth, { 70, 56, 44, 255 } }, NULL);
}

void td_draw_world(const GameActions *ga, Props *props, const Terrain *t, const Player *p, float time) {
    if (!t || !t->world) return;
    for (int i = 0; i < g_nroutes; i++) {
        const Caravan *c = &g_car[i];
        if (!c->shown || dist_xz(c->x, c->z, p->pos.x, p->pos.z) > CARAVAN_DRAW) continue;
        bool raided = ga->caravan_raid == i + 1, moving = !c->halted;
        float fx = sinf(c->yaw), fz = cosf(c->yaw), tt = time + (float)i * 1.7f;
        // Los camellos en fila, uno tras otro.
        for (int k = 0; k < 3; k++) {
            float x = c->x - fx * 4.5f * (float)k, z = c->z - fz * 4.5f * (float)k;
            draw_camel(ga, props, (Vector3){ x, ground_at(t, x, z), z }, c->yaw, moving, tt, k);
        }
        // El mercader, a pie junto al primero (asaltada, se agacha).
        float mx = c->x + fz * 1.3f, mz = c->z - fx * 1.3f;
        draw_person((Vector3){ mx, ground_at(t, mx, mz), mz }, c->yaw, moving, raided ? 1.0f : 0.0f, tt, (Color){ 128, 36, 60, 255 });
        if (raided) continue; // la escolta esta peleando (src/game/combat_game.c)
        // La escolta: un jinete delante y otro detras.
        for (int k = 0; k < 2; k++) {
            float off = k ? -13.5f : 6.0f, x = c->x + fx * off, z = c->z + fz * off;
            Vector3 at = { x, ground_at(t, x, z), z };
            cb_draw_horse(at, c->yaw, moving ? CARAVAN_SPEED : 0.0f, tt + (float)k, false);
            at.y += RIDER_LIFT;
            draw_person(at, c->yaw, false, 0.0f, tt, (Color){ 40, 52, 92, 255 });
        }
    }
}

// ------------------------------------------------------------------ pruebas
void td_test(GameActions *ga, Combat *cb, Troop *troop, const Props *props, const Terrain *t, Player *p, const char *what, int day, char *log,
             size_t len) {
    if (!t || !t->world) return;
    const World *w = t->world;
    ensure_routes(w);
    if (!strcmp(what, "campos")) { // el primer campo de la ciudad mas cercana, y a saquearlo
        int si = world_nearest_settlement(w, p->pos.x, p->pos.z, NULL), n = 0;
        const CityLot *lots = si >= 0 ? ct_lots(ga, t, si, &n) : NULL;
        for (int i = 0; lots && i < n; i++) {
            if (lots[i].kind != LOT_FIELD) continue;
            p->pos = (Vector3){ lots[i].x, terrain_height(t, lots[i].x, lots[i].z), lots[i].z };
            p->yaw = atan2f(w->settlements[si].x - lots[i].x, w->settlements[si].z - lots[i].z);
            raid_fields(ga, cb, troop, t, si, p, day, log, len);
            break;
        }
        return;
    }
    float bx = p->pos.x, bz = p->pos.z, bd = 1e18f, gap = 6.0f;
    bool caravan = !strcmp(what, "caravana") || !strcmp(what, "asaltar"), loot = !strcmp(what, "saquear");
    int ci = 0; // la de la ruta mas larga: va por campo abierto
    for (int i = 1; i < g_nroutes; i++)
        if (route_len(&g_routes[i]) > route_len(&g_routes[ci])) ci = i;
    if (caravan && g_nroutes > 0 && g_car[ci].shown) { // asaltar: delante de ella; si no, a su costado, mirandola pasar
        const Caravan *c = &g_car[ci];
        bool raid = !strcmp(what, "asaltar");
        float fx = sinf(c->yaw), fz = cosf(c->yaw);
        bx = raid ? c->x + fx * gap : c->x - fx * 3.0f + fz * 14.0f, bz = raid ? c->z + fz * gap : c->z - fz * 3.0f - fx * 14.0f;
        p->pos = (Vector3){ bx, terrain_height(t, bx, bz), bz };
        p->yaw = raid ? c->yaw + PI : atan2f(c->x - fx * 3.0f - bx, c->z - fz * 3.0f - bz);
        if (raid) { // como desde el dialogo de la caravana
            ga->market_kind = MARKET_CARAVAN, ga->market_idx = ci, ga->market_region = (int)world_region(w, c->x, c->z);
            start_raid(ga, cb, troop, t, ci, log, len);
        }
        return;
    }
    for (int i = 0; i < w->tribe_count && i < 32; i++) { // un campamento de tribu (rival, para saquearlo)
        const TribeCamp *tc = &w->tribes[i];
        if ((tc->attitude == TRIBE_RIVAL) != loot) continue;
        float d = dist_xz(tc->x, tc->z, p->pos.x, p->pos.z);
        if (d < bd) bd = d, bx = tc->x, bz = tc->z, gap = 8.0f;
        if (loot) ga->raided_tribes |= 1u << i, ga->seen_tribes |= 1u << i; // sus guerreros ya salieron
    }
    for (int i = 0; !loot && i < w->settlement_count; i++) {
        float d = dist_xz(w->settlements[i].x, w->settlements[i].z, p->pos.x, p->pos.z);
        if (d < bd) bd = d, bx = w->settlements[i].x, bz = w->settlements[i].z, gap = w->settlements[i].kind == SETTLE_CAPITAL ? 20.0f : 9.0f; // la capital: delante del palacio
    }
    for (int i = 0; !loot && i < w->site_count && i < 64; i++) {
        float d = dist_xz(w->sites[i].x, w->sites[i].z, p->pos.x, p->pos.z);
        if (w->sites[i].kind == SITE_CARAVANSERAI && d < bd) bd = d, bx = w->sites[i].x, bz = w->sites[i].z, gap = 10.0f;
    }
    float dx = p->pos.x - bx, dz = p->pos.z - bz, l = sqrtf(dx * dx + dz * dz);
    if (l < 1e-3f) dx = 1.0f, dz = 0.0f, l = 1.0f;
    float x = bx + dx / l * gap, z = bz + dz / l * gap;
    p->pos = (Vector3){ x, terrain_height(t, x, z), z };
    p->yaw = atan2f(bx - x, bz - z);
    if (loot) {
        g_talk_req = true;
        return;
    }
    if (!strcmp(what, "cerca")) return; // solo al alcance de F (la pista)
    // Algo que vender, y plata para comprar.
    static const struct {
        const char *id;
        int n;
    } GOODS[] = { { "utileria.material.cuero", 6 }, { "utileria.material.fieltro", 3 }, { "utileria.material.puntas_hueso", 24 },
                  { "utileria.objeto.peine", 2 },    { "utileria.objeto.vela", 9 },     { SILVER_ID, 40 } };
    for (size_t i = 0; i < sizeof(GOODS) / sizeof(GOODS[0]); i++) ig_store(ga, props, p, GOODS[i].id, GOODS[i].n, 1.0f);
    Market m;
    if (market_near(ga, t, p, &m) != 1) return;
    ga->market_kind = m.kind, ga->market_idx = m.idx, ga->market_region = (int)m.region;
    set_mode(ga, !strcmp(what, "vender") ? TD_SELL : !strcmp(what, "comprar") ? TD_BUY : !strcmp(what, "ciudad") ? TD_CITY : TD_ROOT);
    ga->dlg.open = true;
}
