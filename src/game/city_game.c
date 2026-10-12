#include "game/city_game.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "game/camp_game.h"
#include "raymath.h"
#include "rlgl.h"
#include "sim/clock.h"
#include "sim/lang.h"
#include "world/body_draw.h"
#include "world/hearth.h"
#include "world/voxstruct.h"

#define CITY_DRAW 280.0f  // m: los asentamientos se dibujan hasta aca
#define FIELD_DRAW 200.0f // los campos y los corrales
#define FOLK_DRAW 120.0f  // la gente

static float dist_xz(float ax, float az, float bx, float bz) { return sqrtf((ax - bx) * (ax - bx) + (az - bz) * (az - bz)); }

// ------------------------------------------------------------------ la economia
// Las ciudades que todavia no se armaron (partida nueva, o de antes de la v0.9.0).
static void ensure(GameActions *ga, const World *w) {
    for (int i = 0; i < w->settlement_count && i < SETTLEMENTS_MAX; i++)
        if (ga->cities[i].people <= 0.0f)
            city_start(&ga->cities[i], w->settlements[i].kind, w->settlements[i].region, w->seed ^ (uint32_t)(i + 1) * 7919u);
}

void ct_new_day(GameActions *ga, const Terrain *t, Troop *troop, int day, char *log, size_t len) {
    const World *w = t ? t->world : NULL;
    if (!w) return;
    ensure(ga, w);
    Season s = clock_season(day);
    for (int i = 0; i < w->settlement_count && i < SETTLEMENTS_MAX; i++)
        city_day(&ga->cities[i], w->settlements[i].kind, w->settlements[i].region, s, city_season_luck(w->seed, i, day));
    // El tributo: al empezar una estacion, si la anterior paso sin pagarlo, el kan se enoja. La
    // primera vez que se revisa (partida nueva o vieja) solo se anota.
    int si = season_index(day);
    if (si > 0 && ga->tribute_checked != si + 1) {
        bool first = ga->tribute_checked == 0;
        ga->tribute_checked = si + 1;
        if (!first && ga->tribute_season != si && troop && troop->overlord) {
            troop->overlord->relation = fmaxf(-100.0f, troop->overlord->relation - TRIBUTE_MISSED);
            snprintf(log, len, T("El %s no recibió tu tributo la estación pasada: la relación baja (−%d). Se paga en su capital."),
                     T(troop->overlord->name), (int)TRIBUTE_MISSED);
            cg_news(ga, 0, log); // y lo cuenta el guardian del campamento inicial (otro aviso del dia puede taparlo)
        }
    }
}

void ct_update(GameActions *ga, Combat *cb, const Terrain *t) {
    if (!t || !t->world) return;
    ensure(ga, t->world);
    for (int i = 0; i < CB_MAX_ENEMIES; i++) { // la guardia que cae: la ciudad pierde un soldado
        Enemy *e = &cb->enemies[i];
        if (!e->used || e->city <= 0 || e->state != EN_DEAD) continue;
        if (e->city <= SETTLEMENTS_MAX) city_lose_soldiers(&ga->cities[e->city - 1], 1);
        e->city = 0;
    }
}

// ------------------------------------------------------------------ el trazado
static CityLot g_lots[SETTLEMENTS_MAX][CITY_LOTS_MAX];
static int g_nlots[SETTLEMENTS_MAX], g_key[SETTLEMENTS_MAX]; // g_key: de que gente es el trazado + 1 (0: sin hacer)
static const World *g_world;
static uint32_t g_seed; // world_for_seed reusa sus huecos: el mismo puntero puede ser otro mundo

const CityLot *ct_lots(const GameActions *ga, const Terrain *t, int idx, int *n) {
    const World *w = t ? t->world : NULL;
    *n = 0;
    if (!w || idx < 0 || idx >= w->settlement_count || idx >= SETTLEMENTS_MAX) return g_lots[0];
    if (w != g_world || w->seed != g_seed) memset(g_key, 0, sizeof(g_key)), g_world = w, g_seed = w->seed;
    const City *c = &ga->cities[idx];
    int key = 1 + (int)(c->people / 25.0f) * 8 + (c->mercs + 11) / 12; // se rehace cuando cambia la gente
    if (g_key[idx] != key) g_nlots[idx] = city_layout(w, idx, c, g_lots[idx], CITY_LOTS_MAX), g_key[idx] = key;
    *n = g_nlots[idx];
    return g_lots[idx];
}

int ct_field_at(const GameActions *ga, const Terrain *t, float x, float z, int *field) {
    const World *w = t ? t->world : NULL;
    for (int i = 0; w && i < w->settlement_count && i < SETTLEMENTS_MAX; i++) {
        if (dist_xz(w->settlements[i].x, w->settlements[i].z, x, z) > CITY_FIELDS_R1 + 12.0f) continue;
        int n;
        const CityLot *lots = ct_lots(ga, t, i, &n);
        int k = city_lot_at(lots, n, LOT_FIELD, x, z, 1.0f);
        if (k < 0) continue;
        if (field) {
            *field = 0;
            for (int j = 0; j < k; j++) *field += lots[j].kind == LOT_FIELD;
        }
        return i;
    }
    return -1;
}

// La muralla deja sus cuatro puertas a 15, 105, 195 y 285 grados (ver draw_walls).
Vector3 ct_gate_toward(const Terrain *t, int idx, float x, float z) {
    const Settlement *s = &t->world->settlements[idx];
    Vector3 out = { s->x, terrain_height(t, s->x, s->z), s->z };
    if (s->kind != SETTLE_CAPITAL) return out;
    float best = 1e30f;
    for (int k = 0; k < 4; k++) {
        float a = (15.0f + 90.0f * (float)k) * DEG2RAD, gx = s->x + cosf(a) * (CITY_WALL_R + 3.0f), gz = s->z + sinf(a) * (CITY_WALL_R + 3.0f);
        float d = dist_xz(gx, gz, x, z);
        if (d < best) best = d, out = (Vector3){ gx, terrain_height(t, gx, gz), gz };
    }
    return out;
}

static void add(char *out, size_t len, const char *fmt, ...) {
    size_t n = strlen(out);
    if (n + 1 >= len) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out + n, len - n, fmt, ap);
    va_end(ap);
}

void ct_report(const GameActions *ga, const Terrain *t, const Troop *troop, int idx, int day, char *out, size_t len) {
    out[0] = '\0';
    const World *w = t ? t->world : NULL;
    if (!w || idx < 0 || idx >= w->settlement_count || idx >= SETTLEMENTS_MAX) return;
    const Settlement *s = &w->settlements[idx];
    const City *c = &ga->cities[idx];
    Season season = clock_season(day);
    float luck = city_season_luck(w->seed, idx, day);
    const char *harvest = season == SEASON_WINTER ? T("En invierno los campos no dan.")
                          : c->fields < 0.5f  ? T("Los campos están arrasados.")
                          : luck > 1.1f       ? T("La cosecha de esta estación viene buena.")
                          : luck < 0.9f       ? T("La cosecha de esta estación viene mala.")
                                              : T("La cosecha viene como siempre.");
    add(out, len, T("%s: %d almas. El granero da para %d días. %s"), s->name, (int)c->people, c->people > 0.0f ? (int)(c->grain / c->people) : 0,
        harvest);
    add(out, len, "\n");
    add(out, len, T("Tesoro: %d de plata (los escribas cobran %d al día). Armería: %d juegos de armas."), (int)c->silver,
        (int)(city_taxes(c, s->kind) + 0.5f), (int)c->arms);
    add(out, len, "\n");
    if (c->mercs > 0) add(out, len, T("Guarnición: %d soldados y %d mercenarios."), c->soldiers, c->mercs);
    else add(out, len, T("Guarnición: %d soldados."), c->soldiers);
    if (c->grudge >= CITY_GRUDGE_NO_TRADE) add(out, len, " %s", T("No comercian contigo: les saqueaste los campos."));
    else if (c->grudge > 15.0f) add(out, len, " %s", T("Te guardan rencor por los saqueos."));
    if (s->region == REGION_STEPPE && troop && troop->overlord) {
        add(out, len, "\n");
        add(out, len, T("Relación con el %s: %d."), T(troop->overlord->name), (int)troop->overlord->relation);
        if (ga->tribute_season == season_index(day) + 1) add(out, len, " %s", T("El tributo de esta estación, pagado."));
        else add(out, len, " %s", T("El tributo de esta estación, sin pagar (en su capital)."));
    }
}

// ------------------------------------------------------------------ dibujo
static Color folk_color(Region rg, int k) {
    static const Color C[REGION_COUNT][2] = {
        { { 60, 90, 140, 255 }, { 150, 60, 50, 255 } },  { { 70, 96, 58, 255 }, { 120, 90, 60, 255 } },
        { { 130, 110, 80, 255 }, { 90, 80, 110, 255 } }, { { 90, 110, 130, 255 }, { 140, 130, 110, 255 } },
        { { 230, 226, 210, 255 }, { 180, 120, 70, 255 } },
    };
    return C[rg][k & 1];
}

static const Color SOLDIER_RED = { 150, 40, 36, 255 };

static void person(Vector3 pos, float yaw, Color cloth, float walk_phase, float walk, float attack) {
    BodyPose pose;
    BodyPoseParams bp = { .walk_phase = walk_phase, .walk = walk, .attack = attack, .scale = 1.0f };
    body_pose(&pose, &bp);
    BodyColors bc = { { 200, 160, 120, 255 }, cloth, { 84, 64, 46, 255 } };
    body_draw(&pose, pos, yaw, bc, NULL);
}

// Un guardia con su lanza en alto (yaw: hacia donde mira, como body_draw).
static void guard(Vector3 pos, float yaw) {
    person(pos, yaw, SOLDIER_RED, 0.0f, 0.0f, 0.0f);
    Vector3 r = { cosf(yaw), 0.0f, -sinf(yaw) }; // su derecha
    Vector3 a = { pos.x + r.x * 0.38f, pos.y + 0.15f, pos.z + r.z * 0.38f }, b = { a.x, a.y + 2.6f, a.z };
    DrawCylinderEx(a, b, 0.03f, 0.03f, 4, (Color){ 104, 78, 52, 255 });
    DrawCylinderEx(b, (Vector3){ b.x, b.y + 0.3f, b.z }, 0.06f, 0.0f, 4, (Color){ 170, 170, 176, 255 });
}

// Un punto del lote en el mundo: along, a lo largo de su giro; across, de lado.
static Vector3 lot_point(const Terrain *t, const CityLot *l, float along, float across) {
    float ca = cosf(l->yaw), sa = sinf(l->yaw), x = l->x + ca * along - sa * across, z = l->z + sa * along + ca * across;
    return (Vector3){ x, terrain_height(t, x, z), z };
}

// Un campo: los lomos de tierra a lo largo y lo sembrado encima, segun la estacion (en otoño, el
// trigo dorado y despues el rastrojo con sus gavillas). Arrasado: tierra negra.
static void draw_field(const Terrain *t, const CityLot *l, Season s, int dos, bool burnt) {
    int rows = (int)(l->w / 1.8f);
    if (rows < 3) rows = 3;
    float rw = l->w / (float)rows, ch = 0.0f;
    Color soil = burnt ? (Color){ 38, 32, 30, 255 } : s == SEASON_WINTER ? (Color){ 96, 82, 70, 255 } : (Color){ 112, 84, 58, 255 }, crop = soil;
    if (!burnt) {
        if (s == SEASON_SPRING) crop = (Color){ 122, 162, 78, 255 }, ch = 0.1f + 0.03f * (float)dos;
        else if (s == SEASON_SUMMER) crop = (Color){ 94, 140, 56, 255 }, ch = 0.4f + 0.04f * (float)dos;
        else if (s == SEASON_AUTUMN && dos <= 4) crop = (Color){ 214, 178, 82, 255 }, ch = 0.75f;
        else if (s == SEASON_AUTUMN) crop = (Color){ 198, 178, 122, 255 }, ch = 0.12f; // el rastrojo
    }
    rlPushMatrix();
    rlTranslatef(l->x, 0.0f, l->z);
    rlRotatef(-l->yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
    for (int r = 0; r < rows; r++) {
        float across = -l->w * 0.5f + ((float)r + 0.5f) * rw;
        for (int seg = 0; seg < 2; seg++) {
            float along = ((float)seg - 0.5f) * l->l * 0.5f, y = lot_point(t, l, along, across).y;
            DrawCube((Vector3){ along, y + 0.05f, across }, l->l * 0.5f, 0.3f, rw * 0.7f, soil);
            if (ch > 0.0f) DrawCube((Vector3){ along, y + 0.2f + ch * 0.5f, across }, l->l * 0.5f - 0.3f, ch, rw * 0.45f, crop);
        }
    }
    rlPopMatrix();
    if (!burnt && s == SEASON_AUTUMN && dos > 4) // las gavillas
        for (int k = 0; k < 4; k++) {
            Vector3 g = lot_point(t, l, (float)(k - 1.5f) * l->l * 0.22f, (k % 2 ? 1.0f : -1.0f) * l->w * 0.2f);
            DrawCylinder(g, 0.35f, 0.2f, 1.0f, 6, (Color){ 214, 182, 96, 255 });
        }
}

// Un corral: postes y dos travesaños que siguen el terreno, y un par de caballos.
static void draw_corral(const Terrain *t, const CityLot *l, float time, int idx) {
    static const Color WOOD = { 104, 78, 52, 255 };
    float hl = l->l * 0.5f, hw = l->w * 0.5f;
    Vector3 c[4] = { lot_point(t, l, -hl, -hw), lot_point(t, l, hl, -hw), lot_point(t, l, hl, hw), lot_point(t, l, -hl, hw) };
    for (int side = 0; side < 4; side++) {
        Vector3 a = c[side], b = c[(side + 1) % 4];
        int posts = (int)(Vector3Distance(a, b) / 2.5f) + 1;
        for (int k = 0; k <= posts; k++) {
            if (side == 1 && k == posts / 2) continue; // la tranquera
            Vector3 p = Vector3Lerp(a, b, (float)k / (float)posts);
            p.y = terrain_height(t, p.x, p.z);
            DrawCylinder(p, 0.08f, 0.08f, 1.3f, 4, WOOD);
        }
        for (int rail = 0; rail < 2; rail++) {
            float h = 0.55f + 0.5f * (float)rail;
            DrawCylinderEx((Vector3){ a.x, a.y + h, a.z }, (Vector3){ b.x, b.y + h, b.z }, 0.05f, 0.05f, 4, WOOD);
        }
    }
    for (int k = 0; k < 2; k++) {
        Vector3 h = lot_point(t, l, ((float)k - 0.5f) * hl, ((float)k - 0.5f) * hw * 0.6f);
        cb_draw_horse(h, l->yaw + (float)k * 2.0f + (float)idx, 0.0f, time + (float)k, false);
    }
}

// La muralla de la capital: doce torres, y entre ellas muro de piedra o empalizada; cada tercer
// tramo queda abierto (las puertas: 15, 105, 195 y 285 grados).
static void draw_walls(const Terrain *t, const Settlement *s) {
    bool wood = s->region == REGION_STEPPE || s->region == REGION_FOREST;
    for (int i = 0; i < 12; i++) {
        float a = (float)i / 12.0f * 2.0f * PI, x = s->x + cosf(a) * CITY_WALL_R, z = s->z + sinf(a) * CITY_WALL_R;
        voxs_draw_building(wood ? VB_TOWER_WOOD : VB_TOWER_STONE, (Vector3){ x, terrain_height(t, x, z), z }, a, 1.0f, WHITE);
        if (i % 3 == 0) continue;
        float a2 = (float)(i + 1) / 12.0f * 2.0f * PI, x2 = s->x + cosf(a2) * CITY_WALL_R, z2 = s->z + sinf(a2) * CITY_WALL_R;
        float len = dist_xz(x, z, x2, z2), ang = -atan2f(z2 - z, x2 - x);
        for (int k = 0; (float)k * 8.0f < len; k++) {
            float f = ((float)k * 8.0f + 4.0f) / len, px = x + (x2 - x) * f, pz = z + (z2 - z) * f;
            voxs_draw_building(wood ? VB_PALISADE : VB_WALL_STONE, (Vector3){ px, terrain_height(t, px, pz), pz }, ang, 1.0f, WHITE);
        }
    }
}

static bool g_fires_out;

static void draw_city(const GameActions *ga, const Terrain *t, int idx, Vector3 eye, float time, int day, bool night) {
    const World *w = t->world;
    const Settlement *s = &w->settlements[idx];
    const City *c = &ga->cities[idx];
    bool cap = s->kind == SETTLE_CAPITAL;
    Season season = clock_season(day);
    int dos = clock_day_of_season(day), n, fields = 0, workers = 0;
    const CityLot *lots = ct_lots(ga, t, idx, &n);
    for (int i = 0; i < n; i++) fields += lots[i].kind == LOT_FIELD;
    // Los arrasados: tantos como falta sembrar, desde el ultimo que se saqueo.
    int burnt = (int)((1.0f - c->fields) * (float)fields + 0.5f), first = c->raided_lot > 0 ? c->raided_lot - 1 : 0, nfield = 0;
    bool first_house = true;
    Vector3 plaza = { s->x, 0.0f, s->z };
    for (int i = 0; i < n; i++) {
        const CityLot *l = &lots[i];
        float d = dist_xz(l->x, l->z, eye.x, eye.z);
        Vector3 at = { l->x, terrain_height(t, l->x, l->z), l->z };
        switch (l->kind) {
        case LOT_PALACE: {
            voxs_draw_city(VB_PALACE, s->region, at, -l->yaw, 1.0f, WHITE);
            plaza = lot_point(t, l, 12.0f, 0.0f); // la plaza, delante del palacio
            if (d < FOLK_DRAW) { // la corte: la guardia del porche y un escriba que va y viene
                for (int k = -1; k <= 1; k += 2) {
                    Vector3 g = lot_point(t, l, 5.6f, (float)k * 2.4f);
                    g.y = at.y + 1.0f;
                    guard(g, -l->yaw + PI * 0.5f);
                }
                Vector3 sc = lot_point(t, l, 4.2f, sinf(time * 0.25f) * 4.0f);
                sc.y = at.y + 1.0f;
                person(sc, -l->yaw + (cosf(time * 0.25f) > 0.0f ? 0.0f : PI), (Color){ 70, 60, 110, 255 }, time * 4.0f, 0.5f, 0.0f);
            }
            break;
        }
        case LOT_HOUSE:
            voxs_draw_building(voxs_region_house(s->region), at, -l->yaw, !cap && first_house ? 1.25f : 1.0f, WHITE);
            first_house = false;
            break;
        case LOT_FORGE: voxs_draw_city(VB_FORGE, s->region, at, -l->yaw, 1.0f, WHITE); break;
        case LOT_BARRACKS: voxs_draw_city(VB_BARRACKS, s->region, at, -l->yaw, 1.0f, WHITE); break;
        case LOT_CARPENTRY: voxs_draw_city(VB_CARPENTRY, s->region, at, -l->yaw, 1.0f, WHITE); break;
        case LOT_MERC_TENT: voxs_draw_building(VB_YURT, at, -l->yaw, 0.6f, (Color){ 150, 128, 104, 255 }); break;
        case LOT_FIELD: {
            bool scorched = fields > 0 && ((nfield++ - first) % fields + fields) % fields < burnt;
            if (d < FIELD_DRAW) draw_field(t, l, season, dos, scorched);
            // Los labradores, de dia y fuera del invierno: carpen a lo largo de los surcos.
            if (!night && !scorched && season != SEASON_WINTER && d < FOLK_DRAW && workers < 6) {
                float ph = time * 0.18f + (float)i, along = sinf(ph) * l->l * 0.35f;
                Vector3 wp = lot_point(t, l, along, (float)(workers % 3 - 1) * l->w * 0.2f);
                person(wp, -l->yaw + (cosf(ph) > 0.0f ? PI * 0.5f : -PI * 0.5f), folk_color(s->region, workers), time * 3.0f, 0.3f,
                       0.5f + 0.5f * sinf(time * 3.0f + (float)i));
                workers++;
            }
            break;
        }
        case LOT_CORRAL:
            if (d < FIELD_DRAW) draw_corral(t, l, time, idx);
            break;
        default: break;
        }
    }
    float ground = terrain_height(t, plaza.x, plaza.z);
    plaza.y = ground;
    if (cap) {
        draw_walls(t, s);
        // El estandarte del reino, en la plaza.
        Vector3 pole = { plaza.x + 3.0f, ground, plaza.z };
        DrawCylinder(pole, 0.12f, 0.12f, 9.0f, 5, (Color){ 90, 70, 50, 255 });
        DrawCube((Vector3){ pole.x + 1.0f, ground + 8.0f + sinf(time * 2.0f) * 0.06f, pole.z }, 2.0f, 1.2f, 0.08f, folk_color(s->region, idx + 1));
        // La guardia de las puertas: mas soldados, mas guardias (de a dos por puerta).
        if (dist_xz(s->x, s->z, eye.x, eye.z) < FOLK_DRAW + CITY_WALL_R) {
            int guards = c->soldiers / 20;
            guards = guards < 1 ? (c->soldiers > 0) : guards > 8 ? 8 : guards;
            for (int k = 0; k < guards; k++) {
                float a = (15.0f + 90.0f * (float)(k % 4)) * DEG2RAD + (k / 4 ? 0.1f : -0.1f), r = CITY_WALL_R + 2.0f;
                Vector3 g = { s->x + cosf(a) * r, 0.0f, s->z + sinf(a) * r };
                g.y = terrain_height(t, g.x, g.z);
                guard(g, PI * 0.5f - a); // mira hacia afuera
            }
        }
    }
    // La fogata de la plaza y la gente que anda alrededor.
    hearth_draw_base(plaza, cap ? HEARTH_SCALE_BONFIRE : 1.3f, 100u + (uint32_t)idx, !g_fires_out);
    if (dist_xz(plaza.x, plaza.z, eye.x, eye.z) < FOLK_DRAW)
        for (int k = 0; k < 4; k++) {
            float a = time * 0.05f + (float)k * 1.6f + (float)idx, r = 5.0f + 1.5f * (float)(k % 2);
            Vector3 pos = { plaza.x + cosf(a) * r, 0.0f, plaza.z + sinf(a) * r };
            pos.y = terrain_height(t, pos.x, pos.z);
            person(pos, a + PI, folk_color(s->region, k), time * 6.0f + (float)k, 0.0f, 0.0f);
        }
}

void ct_draw_world(const GameActions *ga, const Terrain *t, const Player *p, float time, float world_time) {
    const World *w = t ? t->world : NULL;
    if (!w) return;
    g_fires_out = ga->fires_out; // la lluvia apaga tambien los fuegos de los demas
    int day = clock_day(world_time);
    bool night = clock_is_night(world_time);
    for (int i = 0; i < w->settlement_count && i < SETTLEMENTS_MAX; i++)
        if (dist_xz(w->settlements[i].x, w->settlements[i].z, p->pos.x, p->pos.z) < CITY_DRAW) draw_city(ga, t, i, p->pos, time, day, night);
}
