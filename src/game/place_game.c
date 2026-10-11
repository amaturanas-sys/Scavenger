#include "place_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/input.h"
#include "raylib.h"
#include "sim/lang.h"
#include "sim/placement.h"
#include "ui/icons.h"
#include "ui/theme.h"

#define VIEW_R 48.0f     // m: hasta donde se mueve la huella (el campamento tiene 45)
#define CAM_HEIGHT 76.0f // la camara, sobre el centro: a la vista entran unos 50 m de radio
#define CAM_BACK 33.0f   // y hacia el sur: el norte queda arriba
#define MOVE_SPEED 16.0f // m/s: la huella con el teclado
#define CONFIRM_R 2.5f   // m: tocar la huella (o muy cerca) la coloca
#define CLEARANCE 0.2f   // m de margen alrededor de una obra
#define MAX_OBS 192
#define MAX_SEG 24 // tramos de un muro de una vez

typedef struct {
    Footprint f;
    char name[64];
    bool wall; // cerca o muro: los muros se pueden juntar y cruzar
} Obstacle;

typedef enum { BTN_ROT_L, BTN_ROT_R, BTN_GRID, BTN_OK, BTN_CANCEL, BTN_COUNT } PlaceButton;
static const IconId BTN_ICON[BTN_COUNT] = { ICON_GIRAR_IZQ, ICON_GIRAR_DER, ICON_REJILLA, ICON_OK, ICON_FALTA };

static struct {
    bool active;
    bool fresh; // el primer cuadro: el clic o el Enter que eligio la obra no cuentan aca
    BuildId b;
    int camp;               // -1: fuera de todo campamento
    float cx, cy, cz;       // el centro de la vista y su altura
    float radius;           // el del campamento (0: sin limite)
    float x, z, yaw;        // la huella, sin alinear
    float px, pz;           // y alineada (si hay rejilla): donde se ve
    bool two_points, has_a; // cerca o muro: ya se marco el primer extremo
    float ax, az;
    bool grid;              // la casilla de la rejilla (Mayus tambien alinea)
    float l, w;             // medidas de la obra: largo (X local) y ancho (Z local)
    Footprint fp[MAX_SEG];  // lo que se colocaria
    int nfp, need;
    bool ok;
    char reason[128];
} P;

static const Vector3 *g_yurts, *g_trees;
static int g_nyurts, g_ntrees;
static Vector3 g_fire;
static bool g_fire_used;
static Obstacle g_obs[MAX_OBS];
static int g_nobs;

bool pg_active(void) { return P.active; }
void pg_cancel(void) { P.active = false; }

void pg_set_world(const Vector3 *yurts, int nyurts, Vector3 fire, bool fire_used, const Vector3 *trees, int ntrees) {
    g_yurts = yurts, g_nyurts = yurts ? nyurts : 0;
    g_fire = fire, g_fire_used = fire_used;
    g_trees = trees, g_ntrees = trees ? ntrees : 0;
}

static bool is_wall(BuildId b) { return b == BUILD_PALISADE || b == BUILD_STONE_WALL; }
static bool wall_item(const char *id) { return strstr(id, "empalizada") || strstr(id, "muro_piedra"); }

// La primera letra en minuscula (los nombres del inventario empiezan en mayuscula).
static const char *lower_first(const char *s) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "%s", s);
    if (buf[0] >= 'A' && buf[0] <= 'Z') buf[0] = (char)(buf[0] - 'A' + 'a');
    return buf;
}

static const InvItem *build_item(const GameActions *ga, BuildId b) {
    const BuildDef *d = build_def(b);
    return d ? inventory_find(ga->inv, d->produces) : NULL;
}

void pg_begin(GameActions *ga, BuildId b, int camp, float x, float z) {
    memset(&P, 0, sizeof(P));
    const InvItem *it = build_item(ga, b);
    P.active = build_def(b) != NULL;
    P.fresh = true;
    P.b = b;
    P.l = it && it->l > 0.0f ? it->l : 3.0f;
    P.w = it && it->w > 0.0f ? it->w : 3.0f;
    if (camp >= 0 && camp < CAMPS_MAX && ga->camps[camp].used) {
        P.camp = camp;
        P.cx = ga->camps[camp].x, P.cz = ga->camps[camp].z;
        P.radius = CAMP_RADIUS_M;
        P.x = P.cx + 14.0f, P.z = P.cz + 8.0f; // empieza a un lado del fuego
    } else {
        P.camp = -1;
        P.cx = x, P.cz = z;
        P.x = x + 6.0f, P.z = z;
    }
    P.px = P.x, P.pz = P.z;
    P.two_points = is_wall(b);
    ga->menu_open = false;
}

void pg_camera(Camera3D *cam) {
    if (!P.active) return;
    cam->target = (Vector3){ P.cx, P.cy, P.cz };
    cam->position = (Vector3){ P.cx, P.cy + CAM_HEIGHT, P.cz + CAM_BACK };
    cam->up = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam->fovy = 60.0f;
    cam->projection = CAMERA_PERSPECTIVE;
}

// ---------------------------------------------------------------- lo que no se puede pisar
static void add_obs(Footprint f, const char *name, bool wall) {
    if (g_nobs >= MAX_OBS) return;
    g_obs[g_nobs].f = f;
    g_obs[g_nobs].wall = wall;
    snprintf(g_obs[g_nobs].name, sizeof(g_obs[0].name), "%s", name);
    g_nobs++;
}

static bool in_view(float x, float z) { return (x - P.cx) * (x - P.cx) + (z - P.cz) * (z - P.cz) < (VIEW_R + 20.0f) * (VIEW_R + 20.0f); }

static void gather(const GameActions *ga, const Props *props) {
    g_nobs = 0;
    for (int i = 0; i < g_nyurts; i++)
        if (in_view(g_yurts[i].x, g_yurts[i].z)) add_obs(footprint_make(g_yurts[i].x, g_yurts[i].z, 0.0f, 4.8f, 4.8f, 0.0f), T("una yurta"), false);
    if (g_fire_used && in_view(g_fire.x, g_fire.z)) add_obs(footprint_make(g_fire.x, g_fire.z, 0.0f, 2.6f, 2.6f, 0.0f), T("la fogata"), false);
    for (int i = 0; i < g_ntrees; i++)
        if (in_view(g_trees[i].x, g_trees[i].z)) add_obs(footprint_make(g_trees[i].x, g_trees[i].z, 0.0f, 1.2f, 1.2f, 0.0f), T("un árbol"), false);
    // Las estructuras, los totems y los carros (no las cenizas ni lo suelto en el suelo).
    for (int i = 0; props && i < props->count; i++) {
        const Prop *pr = &props->items[i];
        const char *id = pr->item ? pr->item->id : "";
        bool solid = (!strncmp(id, "estructura.", 11) && strncmp(id, "estructura.ruina.", 17) != 0) || !strncmp(id, "totem.", 6) ||
                     !strncmp(id, "vehiculo.", 9);
        if (!solid || pr->flying || !in_view(pr->pos.x, pr->pos.z)) continue;
        add_obs(footprint_make(pr->pos.x, pr->pos.z, pr->yaw, pr->item->l, pr->item->w, 0.0f), lower_first(T(pr->item->name)), wall_item(id));
    }
    for (int i = 0; i < ga->project_count && i < GA_MAX_PROJECTS; i++) {
        const BuildProject *bp = &ga->projects[i];
        const InvItem *it = build_item(ga, bp->def);
        char name[64];
        snprintf(name, sizeof(name), "%s", lower_first(T(build_def(bp->def)->name)));
        add_obs(footprint_make(bp->x, bp->z, bp->yaw, it ? it->l : 3.0f, it ? it->w : 3.0f, 0.0f), TextFormat(T("otra obra (%s)"), name),
                is_wall(bp->def));
    }
}

// ---------------------------------------------------------------- si el sitio vale
static float ground_h(void *ud, float x, float z) { return terrain_height((const Terrain *)ud, x, z); }
static float ground_water(void *ud, float x, float z) {
    const Terrain *t = ud;
    return terrain_water(t, x, z) - terrain_height(t, x, z); // el hielo tampoco: es agua
}

static Stockpile *stock_for(GameActions *ga) { return P.camp >= 0 ? &ga->camps[P.camp].stock : ga_stock(ga); }

static void validate(GameActions *ga, const Terrain *t) {
    const BuildDef *d = build_def(P.b);
    if (P.two_points && P.has_a) {
        P.need = place_wall(P.ax, P.az, P.px, P.pz, P.w, P.l, P.fp, MAX_SEG);
        P.nfp = P.need < MAX_SEG ? P.need : MAX_SEG;
    } else {
        P.fp[0] = footprint_make(P.px, P.pz, P.yaw, P.l, P.w, 0.0f);
        P.nfp = P.need = 1;
    }
    P.ok = false;
    // Los muros se pueden juntar y cruzar con otros muros.
    static Footprint obs[MAX_OBS];
    static int idx[MAX_OBS];
    int n = 0;
    for (int k = 0; k < g_nobs; k++)
        if (!(P.two_points && g_obs[k].wall)) obs[n] = g_obs[k].f, idx[n++] = k;
    PlaceGround g = { ground_h, ground_water, (void *)t };
    for (int i = 0; i < P.nfp; i++) {
        Footprint f = P.fp[i];
        f.hl += CLEARANCE, f.hw += CLEARANCE;
        PlaceCheck c = place_check(&f, P.cx, P.cz, P.radius, t ? &g : NULL, obs, n);
        switch (c.res) {
        case PLACE_OK: continue;
        case PLACE_OUTSIDE: snprintf(P.reason, sizeof(P.reason), "%s", T("Fuera del campamento")); return;
        case PLACE_WATER: snprintf(P.reason, sizeof(P.reason), "%s", T("En el agua")); return;
        case PLACE_STEEP: snprintf(P.reason, sizeof(P.reason), "%s", T("Demasiado empinado")); return;
        default: snprintf(P.reason, sizeof(P.reason), T("Encima de %s"), c.hit >= 0 ? g_obs[idx[c.hit]].name : "?"); return;
        }
    }
    int free = GA_MAX_PROJECTS - ga->project_count;
    if (P.need > free || P.need > MAX_SEG) {
        if (P.need > 1)
            snprintf(P.reason, sizeof(P.reason), T("Demasiado largo: %d tramos (caben %d)"), P.need, free < MAX_SEG ? free : MAX_SEG);
        else
            snprintf(P.reason, sizeof(P.reason), "%s", T("Demasiadas obras a la vez."));
        return;
    }
    const Stockpile *st = stock_for(ga);
    for (const Ingredient *m = d->mats; m->id; m++)
        if (stock_count(st, m->id) < m->count * P.nfp) {
            const InvItem *it = inventory_find(ga->inv, m->id);
            snprintf(P.reason, sizeof(P.reason), T("Falta en el acopio: %s (%d de %d)."), it ? T(it->name) : m->id, stock_count(st, m->id),
                     m->count * P.nfp);
            return;
        }
    P.ok = true;
    if (P.two_points && !P.has_a) snprintf(P.reason, sizeof(P.reason), "%s", T("Marca el primer extremo: Enter o clic"));
    else if (P.nfp > 1) snprintf(P.reason, sizeof(P.reason), T("Aquí vale (%d tramos): Enter o clic para colocar"), P.nfp);
    else snprintf(P.reason, sizeof(P.reason), "%s", T("Aquí vale: Enter o clic para colocar"));
}

// Las obras nuevas: se paga el acopio y la cuadrilla va en camino.
static void place(GameActions *ga, char *log, size_t len) {
    const BuildDef *d = build_def(P.b);
    Stockpile *st = stock_for(ga);
    int n = 0;
    for (int i = 0; i < P.nfp && ga->project_count < GA_MAX_PROJECTS; i++, n++) {
        stock_take_all(st, d->mats);
        ga->projects[ga->project_count++] = (BuildProject){ P.b, P.fp[i].x, P.fp[i].z, 0.0f, false, P.fp[i].yaw };
    }
    if (n > 1) snprintf(log, len, T("Obra iniciada: %s (%d tramos). La cuadrilla va en camino."), T(d->name), n);
    else snprintf(log, len, T("Obra iniciada: %s. La cuadrilla va en camino."), T(d->name));
}

// ---------------------------------------------------------------- entrada
static Rectangle btn_rect(int i, int vw, int vh) { return (Rectangle){ (float)(vw - 8 - (BTN_COUNT - i) * 32), (float)(vh - 40), 28, 28 }; }

// Donde toca el puntero el suelo (el plano del campamento, corregido con la altura de ahi).
static bool pointer_ground(Camera3D cam, Vector2 v, int vw, int vh, const Terrain *t, float *x, float *z) {
    Ray ray = GetScreenToWorldRayEx(v, cam, vw, vh);
    if (fabsf(ray.direction.y) < 1e-4f) return false;
    float gy = P.cy;
    for (int k = 0; k < 2; k++) {
        float s = (gy - ray.position.y) / ray.direction.y;
        if (s < 0.0f) return false;
        *x = ray.position.x + ray.direction.x * s;
        *z = ray.position.z + ray.direction.z * s;
        if (t) gy = terrain_height(t, *x, *z);
    }
    return true;
}

void pg_update(GameActions *ga, const Props *props, const Terrain *t, Camera3D cam, int vw, int vh, float dt, char *log, size_t len) {
    if (!P.active) return;
    P.cy = t ? terrain_height(t, P.cx, P.cz) : 0.0f;
    gather(ga, props);
    if (P.fresh) { // recien abierta: solo se mira el sitio
        P.fresh = false;
        validate(ga, t);
        return;
    }
    // Las casillas, abajo a la derecha (con el dedo o el raton).
    int act = -1;
    bool over = false;
    for (int i = 0; i < BTN_COUNT; i++) {
        Rectangle r = btn_rect(i, vw, vh);
        over |= ui_hover(r);
        if (ui_click(r)) act = i;
    }
    bool confirm = false;
    Vector2 v = ui_pointer();
    bool press = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !over;
    float x, z;
    if ((ui_pointer_moved() || press) && !over && pointer_ground(cam, v, vw, vh, t, &x, &z)) {
        // Un toque sobre la huella (o un clic, que el puntero ya la trae) la coloca; en otro lado, la mueve.
        if (press && (x - P.px) * (x - P.px) + (z - P.pz) * (z - P.pz) <= CONFIRM_R * CONFIRM_R) confirm = true;
        else P.x = x, P.z = z;
    }
    // El teclado: WASD o las flechas, con el norte arriba.
    float mx = (float)((IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) - (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)));
    float mz = (float)((IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) - (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)));
    P.x += mx * MOVE_SPEED * dt, P.z += mz * MOVE_SPEED * dt;
    float dx = P.x - P.cx, dz = P.z - P.cz, d = sqrtf(dx * dx + dz * dz);
    if (d > VIEW_R) P.x = P.cx + dx / d * VIEW_R, P.z = P.cz + dz / d * VIEW_R; // no se va de la vista
    if (IsKeyPressed(KEY_Q) || act == BTN_ROT_L) P.yaw = place_rotate(P.yaw, 1);  // a la izquierda (vista desde arriba)
    if (IsKeyPressed(KEY_E) || act == BTN_ROT_R) P.yaw = place_rotate(P.yaw, -1); // a la derecha
    if (act == BTN_GRID) P.grid = !P.grid;
    bool grid = P.grid || (input_mods() & KM_SHIFT);
    P.px = grid ? place_snap(P.x, P.cx, PLACE_GRID) : P.x;
    P.pz = grid ? place_snap(P.z, P.cz, PLACE_GRID) : P.z;
    validate(ga, t);
    if (input_pressed(IN_BACK) || act == BTN_CANCEL) {
        if (P.has_a) {
            P.has_a = false; // vuelve a elegir el primer extremo
        } else {
            P.active = false;
            snprintf(log, len, "%s", T("Obra cancelada: no se gastó nada."));
        }
        return;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || act == BTN_OK) confirm = true;
    if (!confirm) return;
    if (!P.ok) {
        snprintf(log, len, "%s", P.reason);
        return;
    }
    if (P.two_points && !P.has_a) { // el primer extremo; el otro, con el proximo
        P.has_a = true;
        P.ax = P.px, P.az = P.pz;
        return;
    }
    place(ga, log, len);
    P.active = false;
}

// ---------------------------------------------------------------- dibujo
static float hy(const Terrain *t, float x, float z) { return t ? terrain_height(t, x, z) : 0.0f; }

static void draw_outline(const Terrain *t, const Footprint *f, Color c) {
    float k[4][2];
    footprint_corners(f, k);
    for (int i = 0; i < 4; i++) {
        int j = (i + 1) % 4;
        DrawLine3D((Vector3){ k[i][0], hy(t, k[i][0], k[i][1]) + 0.1f, k[i][1] }, (Vector3){ k[j][0], hy(t, k[j][0], k[j][1]) + 0.1f, k[j][1] }, c);
    }
}

void pg_draw_world(const GameActions *ga, Props *props, const Terrain *t, float time) {
    (void)time;
    if (!P.active) return;
    // El borde del campamento: estacas cada 4 grados.
    for (int i = 0; P.radius > 0.0f && i < 90; i++) {
        float a = (float)i * 0.0698132f, x = P.cx + cosf(a) * P.radius, z = P.cz + sinf(a) * P.radius;
        DrawCube((Vector3){ x, hy(t, x, z) + 0.5f, z }, 0.7f, 1.0f, 0.7f, (Color){ 235, 200, 110, 255 });
    }
    // Lo que no se puede pisar dentro del campamento, con su contorno.
    for (int i = 0; i < g_nobs; i++) {
        const Footprint *f = &g_obs[i].f;
        float dx = f->x - P.cx, dz = f->z - P.cz;
        if (dx * dx + dz * dz < (VIEW_R + 6.0f) * (VIEW_R + 6.0f)) draw_outline(t, f, (Color){ 250, 245, 230, 150 });
    }
    // La rejilla, alrededor de la huella.
    if (P.grid || (input_mods() & KM_SHIFT)) {
        float gx = place_snap(P.px, P.cx, PLACE_GRID), gz = place_snap(P.pz, P.cz, PLACE_GRID);
        for (int k = -6; k <= 6; k++)
            for (int s = -6; s < 6; s++) {
                float a = gx + (float)k * PLACE_GRID, b0 = gz + (float)s * PLACE_GRID, b1 = b0 + PLACE_GRID;
                Color c = (Color){ 240, 230, 200, 90 };
                DrawLine3D((Vector3){ a, hy(t, a, b0) + 0.06f, b0 }, (Vector3){ a, hy(t, a, b1) + 0.06f, b1 }, c);
                float c0 = gx + (float)s * PLACE_GRID, c1 = c0 + PLACE_GRID, e = gz + (float)k * PLACE_GRID;
                DrawLine3D((Vector3){ c0, hy(t, c0, e) + 0.06f, e }, (Vector3){ c1, hy(t, c1, e) + 0.06f, e }, c);
            }
    }
    // El fantasma: verde si vale, rojo si no.
    const InvItem *it = build_item(ga, P.b);
    Color tint = P.ok ? (Color){ 90, 225, 110, 140 } : (Color){ 235, 70, 60, 140 };
    for (int i = 0; i < P.nfp; i++) {
        const Footprint *f = &P.fp[i];
        if (it) props_draw_ghost(props, it, (Vector3){ f->x, hy(t, f->x, f->z), f->z }, f->yaw, tint);
        Footprint g = *f;
        draw_outline(t, &g, ColorAlpha(tint, 1.0f));
        g.hl += 0.25f, g.hw += 0.25f; // doble, para que se vea desde arriba
        draw_outline(t, &g, ColorAlpha(tint, 1.0f));
    }
    if (P.has_a) DrawCylinder((Vector3){ P.ax, hy(t, P.ax, P.az), P.az }, 0.35f, 0.35f, 3.0f, 8, (Color){ 235, 200, 110, 255 }); // el primer extremo
}

void pg_draw_hud(GameActions *ga, int vw, int vh) {
    if (!P.active) return;
    const BuildDef *d = build_def(P.b);
    // Arriba: que se ubica, lo que cuesta y como.
    DrawRectangle(0, 0, vw, 46, (Color){ 12, 9, 6, 170 });
    ui_text(TextFormat(T("Ubicar: %s"), T(d->name)), 10, 6, 10, UI_GOLD_LIGHT);
    char cost[160];
    int n = 0;
    cost[0] = '\0';
    const Stockpile *st = stock_for(ga);
    for (const Ingredient *m = d->mats; m->id && n < (int)sizeof(cost); m++) {
        const InvItem *it = inventory_find(ga->inv, m->id);
        n += snprintf(cost + n, sizeof(cost) - (size_t)n, "%s%s %d/%d", n ? ", " : "", it ? T(it->name) : m->id, stock_count(st, m->id),
                      m->count * (P.nfp > 0 ? P.nfp : 1));
    }
    ui_text(TextFormat(T("Lleva: %s"), cost), 10, 19, 10, UI_BONE);
    ui_text(P.two_points ? (P.has_a ? T("Marca el otro extremo · Esc: volver al primero")
                                    : T("Cerca o muro: marca un extremo y después el otro · Mayús: rejilla · Esc: cancelar"))
                         : T("Puntero o WASD: mover · Q/E: girar · Mayús: rejilla · Enter o clic: colocar · Esc: cancelar"),
            10, 32, 10, UI_BONE_DIM);
    // Abajo: si el sitio vale, o por que no.
    int tw = MeasureText(P.reason, 10);
    DrawRectangle(vw / 2 - tw / 2 - 6, vh - 62, tw + 12, 15, (Color){ 12, 9, 6, 190 });
    ui_text_centered(P.reason, vw / 2, vh - 59, 10, P.ok ? (Color){ 120, 235, 130, 255 } : (Color){ 245, 100, 85, 255 });
    // Las casillas (para el dedo; con teclado tambien sirven).
    static const char *LABEL[BTN_COUNT] = { N_("Girar a la izquierda"), N_("Girar a la derecha"), N_("Rejilla"), N_("Colocar"), N_("Cancelar") };
    static const char *KEYS[BTN_COUNT] = { "Q", "E", N_("Mayús"), N_("Enter o clic sobre la obra"), "Esc" };
    for (int i = 0; i < BTN_COUNT; i++) {
        Rectangle r = btn_rect(i, vw, vh);
        bool hover = ui_tile(r, BTN_ICON[i], i == BTN_GRID && P.grid, i != BTN_OK || P.ok);
        if (hover) ui_legend(T(LABEL[i]), T(KEYS[i]));
    }
}
