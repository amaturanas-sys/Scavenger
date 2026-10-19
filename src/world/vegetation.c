#include "world/vegetation.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "platform.h"
#include "raymath.h"
#include "rlgl.h"
#include "sim/flora.h"

#define VEG_GRID 42            // celdas por lado de la ventana (84 m)
#define VEG_SLOTS (VEG_GRID * VEG_GRID)
#define VEG_RADIUS 39.0f       // hasta donde se dibuja
#define VEG_RADIUS_TOUCH 18.0f // en Android
#define VEG_FADE 5.0f          // los ultimos metros crecen de a poco: no aparecen de golpe
#define VEG_BUDGET 48          // celdas que se siembran por cuadro (en Android, la mitad)
#define VEG_BURNT 48           // focos y tierra calcinada cerca que se miran por cuadro

static const Vector2 WIND_DIR = { 0.86f, 0.5f }; // el mismo viento que la lluvia (weather.c)

typedef struct {
    int gx, gz; // la celda que tiene (INT_MIN: ninguna)
    int n;
    FloraTuft tuft[FLORA_PER_CELL];
} VegSlot;

static VegSlot g_slots[VEG_SLOTS];
static signed char g_order[VEG_SLOTS][2]; // las celdas de la ventana, de la mas cercana a la mas lejana
static const World *g_world;
static uint32_t g_seed;
static int g_cx, g_cz; // la celda del centro
static int g_drawn;

static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

static float smooth(float a, float b, float x) {
    float t = clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

static Color mixc(Color a, Color b, float k) {
    k = clamp01(k);
    return (Color){ (unsigned char)Lerp(a.r, b.r, k), (unsigned char)Lerp(a.g, b.g, k), (unsigned char)Lerp(a.b, b.b, k), 255 };
}

static Color shade(Color c, float k) {
    return (Color){ (unsigned char)fminf(255.0f, c.r * k), (unsigned char)fminf(255.0f, c.g * k), (unsigned char)fminf(255.0f, c.b * k), 255 };
}

static int by_dist(const void *a, const void *b) {
    const signed char *p = a, *q = b;
    return (p[0] * p[0] + p[1] * p[1]) - (q[0] * q[0] + q[1] * q[1]);
}

static int slot_of(int gx, int gz) { return ((gx % VEG_GRID + VEG_GRID) % VEG_GRID) + VEG_GRID * ((gz % VEG_GRID + VEG_GRID) % VEG_GRID); }

void vegetation_update(const Terrain *t, Vector3 pos) {
    if (g_world != t->world || g_seed != t->seed) { // otro mundo (otra partida): de cero
        if (!g_world) {
            for (int j = 0, i = 0; j < VEG_GRID; j++)
                for (int k = 0; k < VEG_GRID; k++, i++) g_order[i][0] = (signed char)(k - VEG_GRID / 2), g_order[i][1] = (signed char)(j - VEG_GRID / 2);
            qsort(g_order, VEG_SLOTS, sizeof(g_order[0]), by_dist);
        }
        for (int i = 0; i < VEG_SLOTS; i++) g_slots[i].gx = g_slots[i].gz = INT_MIN, g_slots[i].n = 0;
        g_world = t->world, g_seed = t->seed;
    }
    g_cx = (int)floorf(pos.x / FLORA_CELL), g_cz = (int)floorf(pos.z / FLORA_CELL);
    int budget = platform_touch_ui() ? VEG_BUDGET / 2 : VEG_BUDGET;
    for (int i = 0; i < VEG_SLOTS && budget > 0; i++) {
        int gx = g_cx + g_order[i][0], gz = g_cz + g_order[i][1];
        VegSlot *s = &g_slots[slot_of(gx, gz)];
        if (s->gx == gx && s->gz == gz) continue;
        s->gx = gx, s->gz = gz;
        s->n = flora_cell(t->world, t->seed, gx, gz, s->tuft);
        budget--;
    }
}

void vegetation_stats(int *planted, int *drawn) {
    int n = 0;
    for (int i = 0; i < VEG_SLOTS; i++)
        if (g_slots[i].gx != INT_MIN) n += g_slots[i].n;
    if (planted) *planted = g_world ? n : 0;
    if (drawn) *drawn = g_drawn;
}

// ---------------------------------------------------------------- el dibujo
// Los colores de la estacion (los de cada mata salen de aca, con su region y su azar).
typedef struct {
    float year, plume, snow;
    bool cattail; // los juncos con su espiga parda: del verano al invierno
    Color stipa_tip, fern, reed;
} VegLook;

// El pasto: el color del suelo de su region y su estacion (src/world/terrain.c: ground_color).
static Color grass_color(const Climate *c, const FloraTuft *t) {
    const Color green = { 112, 150, 70, 255 }, dry = { 188, 168, 106, 255 }, ochre = { 172, 124, 68, 255 };
    const Color dormant = { 140, 128, 98, 255 }, needles = { 72, 96, 54, 255 }, tundra = { 132, 136, 104, 255 };
    const Color moss = { 96, 124, 92, 255 }, desert = { 170, 150, 100, 255 };
    Color g = mixc(dry, green, c->greenness);
    g = mixc(g, ochre, c->autumn * 0.85f);
    g = mixc(dormant, g, 0.35f + 0.65f * clamp01(c->greenness + c->autumn));
    g = mixc(g, mixc(needles, ochre, c->autumn * 0.3f), t->forest / 255.0f);
    g = mixc(g, tundra, t->high / 255.0f);
    g = mixc(g, moss, t->fjord / 255.0f);
    return mixc(g, desert, t->desert / 255.0f * 0.6f);
}

static void tri(Vector3 a, Color ca, Vector3 b, Color cb, Vector3 c, Color cc) {
    rlColor4ub(ca.r, ca.g, ca.b, 255);
    rlVertex3f(a.x, a.y, a.z);
    rlColor4ub(cb.r, cb.g, cb.b, 255);
    rlVertex3f(b.x, b.y, b.z);
    rlColor4ub(cc.r, cc.g, cc.b, 255);
    rlVertex3f(c.x, c.y, c.z);
}

// Una hoja de alto h desde b: se abre hacia (ox, oz) (open: m por m de alto) y la dobla (bx, bz) (el
// viento y quien pasa); el largo se conserva: doblada, baja. w: medio ancho de la base, de costado a
// (sx, sz). mid: un quiebre a esa altura (0: una sola pieza), que se dobla menos; wmid: el ancho ahi
// (por el de la base: la pluma de la stipa es mas ancha que su tallo).
static void blade(Vector3 b, float h, float ox, float oz, float open, float bx, float bz, float w, float sx, float sz, Color cb,
                  Color ct, float mid, float wmid, Color cm) {
    float dx = ox * open + bx, dz = oz * open + bz, f2 = dx * dx + dz * dz;
    if (f2 > 0.8f) {
        float k = sqrtf(0.8f / f2);
        dx *= k, dz *= k, f2 = 0.8f;
    }
    Vector3 tip = { b.x + dx * h, b.y + h * sqrtf(1.0f - f2), b.z + dz * h };
    Vector3 l = { b.x - sx * w, b.y, b.z - sz * w }, r = { b.x + sx * w, b.y, b.z + sz * w };
    if (mid <= 0.0f) {
        tri(l, cb, r, cb, tip, ct);
        return;
    }
    // El quiebre: la mitad de abajo casi derecha, la de arriba se dobla.
    float k = mid * mid, wm = w * wmid;
    Vector3 m = { b.x + dx * h * k, b.y + h * mid * sqrtf(1.0f - f2 * k), b.z + dz * h * k };
    Vector3 ml = { m.x - sx * wm, m.y, m.z - sz * wm }, mr = { m.x + sx * wm, m.y, m.z + sz * wm };
    tri(l, cb, r, cb, mr, cm);
    tri(l, cb, mr, cm, ml, cm);
    tri(ml, cm, mr, cm, tip, ct);
}

// Una flor: un rombo doble (de dos lados que se cruzan) en (c), de radio r.
static void blossom(Vector3 c, float r, Color col) {
    Color dark = shade(col, 0.7f);
    Vector3 top = { c.x, c.y + 1.5f * r, c.z }, bot = { c.x, c.y - 0.7f * r, c.z };
    tri((Vector3){ c.x - r, c.y, c.z }, col, (Vector3){ c.x + r, c.y, c.z }, col, top, col);
    tri((Vector3){ c.x - r, c.y, c.z }, dark, bot, dark, (Vector3){ c.x + r, c.y, c.z }, dark);
    tri((Vector3){ c.x, c.y, c.z - r }, col, (Vector3){ c.x, c.y, c.z + r }, col, top, col);
    tri((Vector3){ c.x, c.y, c.z - r }, dark, bot, dark, (Vector3){ c.x, c.y, c.z + r }, dark);
}

// Una mata. h: su alto; lod: 3 de cerca, 2 a media distancia, 1 lejos; (bx, bz): cuanto la doblan
// el viento y quien pasa (m por m de alto); (fx, fz): de costado a la camara (las de una sola hoja
// la miran de frente).
static void draw_tuft(const FloraTuft *t, const Climate *c, const VegLook *L, float h, int lod, float bx, float bz, float fx, float fz) {
    Vector3 b = { t->x, t->y - 0.03f, t->z };
    float a0 = t->rnd * 6.2832f, v = 0.88f + 0.24f * t->rnd, s = t->size;
    Color snow = { 232, 236, 242, 255 };
    switch ((FloraKind)t->kind) {
    case FLORA_GRASS: {
        // La base, a la sombra; las puntas, al sol (en primavera y verano, de un verde mas claro).
        Color g = grass_color(c, t), cb = shade(g, 0.7f * v);
        Color ct = mixc(mixc(shade(g, 1.18f * v), (Color){ 200, 208, 120, 255 }, 0.22f * c->greenness), snow, L->snow * 0.5f);
        int n = lod == 3 ? 4 : lod;
        for (int j = 0; j < n; j++) {
            float a = a0 + (float)j * 6.2832f / (float)n, ox = cosf(a), oz = sinf(a);
            float hj = h * (0.72f + 0.28f * fmodf(t->rnd * 7.3f + (float)j * 0.37f, 1.0f));
            Vector3 bj = { b.x + ox * 0.05f, b.y, b.z + oz * 0.05f };
            if (n == 1) blade(bj, hj, ox, oz, 0.22f, bx, bz, 0.1f * s, fx, fz, cb, ct, 0.0f, 1.0f, cb);
            else blade(bj, hj, ox, oz, 0.3f, bx, bz, 0.065f * s, -oz, ox, cb, ct, 0.0f, 1.0f, cb);
        }
        break;
    }
    case FLORA_STIPA: { // tallos finos que el viento dobla mas; arriba, la pluma
        Color g = mixc(grass_color(c, t), (Color){ 196, 184, 140, 255 }, 0.35f);
        Color cb = shade(g, 0.6f * v), cm = shade(g, 0.95f * v), ct = mixc(mixc(cm, L->stipa_tip, L->plume), snow, L->snow * 0.6f);
        int n = lod == 3 ? 3 : lod;
        for (int j = 0; j < n; j++) {
            float a = a0 + (float)j * 2.094f + 0.4f, ox = cosf(a), oz = sinf(a);
            float hj = h * (0.8f + 0.2f * fmodf(t->rnd * 5.1f + (float)j * 0.61f, 1.0f));
            if (n == 1) blade(b, hj, ox, oz, 0.18f, bx * 1.3f, bz * 1.3f, 0.06f * s, fx, fz, cb, ct, 0.0f, 1.0f, cm);
            else blade(b, hj, ox, oz, 0.2f, bx * 1.3f, bz * 1.3f, 0.03f * s, -oz, ox, cb, ct, lod == 3 ? 0.6f : 0.0f, 1.8f, cm);
        }
        break;
    }
    case FLORA_FLOWER: { // un tallo, dos hojas y la flor (tulipanes y lirios; en el altiplano, azules y blancas)
        static const Color STEPPE[4] = { { 204, 58, 52, 255 }, { 232, 196, 64, 255 }, { 136, 96, 190, 255 }, { 214, 86, 120, 255 } };
        static const Color HIGH[4] = { { 70, 110, 200, 255 }, { 236, 232, 222, 255 }, { 232, 196, 64, 255 }, { 120, 100, 200, 255 } };
        int pick = (int)(t->rnd * 997.0f) % 4;
        Color petal = t->high > 128 || t->fjord > 128 ? HIGH[pick] : t->forest > 128 ? HIGH[(pick + 1) % 4] : STEPPE[pick];
        Color g = shade(grass_color(c, t), 0.9f), cb = shade(g, 0.6f);
        float f2 = bx * bx + bz * bz, k = f2 > 0.8f ? sqrtf(0.8f / f2) : 1.0f; // la flor, en la punta del tallo (como blade)
        Vector3 head = { b.x + bx * k * h, b.y + h * sqrtf(1.0f - fminf(f2, 0.8f)), b.z + bz * k * h };
        if (lod > 1) {
            blade(b, h, 0.0f, 0.0f, 0.0f, bx, bz, 0.025f * s, fx, fz, cb, g, 0.0f, 1.0f, g);
            blade(b, h * 0.45f, cosf(a0), sinf(a0), 0.5f, bx, bz, 0.04f * s, -sinf(a0), cosf(a0), cb, g, 0.0f, 1.0f, g);
            if (lod == 3) blade(b, h * 0.4f, -cosf(a0), -sinf(a0), 0.5f, bx, bz, 0.04f * s, sinf(a0), -cosf(a0), cb, g, 0.0f, 1.0f, g);
        }
        blossom(head, (0.055f + 0.02f * t->rnd) * s * (lod == 1 ? 1.3f : 1.0f), shade(petal, v));
        break;
    }
    case FLORA_FERN: { // frondas anchas que se abren hacia afuera
        Color cb = shade(L->fern, 0.55f * v), ct = mixc(shade(L->fern, 1.05f * v), snow, L->snow * 0.5f);
        int n = lod == 3 ? 5 : lod == 2 ? 3 : 2;
        for (int j = 0; j < n; j++) {
            float a = a0 + (float)j * 6.2832f / (float)n, ox = cosf(a), oz = sinf(a);
            blade(b, h, ox, oz, 0.7f, bx * 0.6f, bz * 0.6f, 0.09f * s, -oz, ox, cb, ct, 0.0f, 1.0f, cb);
        }
        break;
    }
    case FLORA_REED: { // tallos altos casi derechos; del verano al invierno, la espiga parda
        Color cb = shade(L->reed, 0.6f * v), cm = shade(L->reed, 0.9f * v), ct = mixc(shade(L->reed, 1.05f * v), snow, L->snow * 0.6f);
        int n = lod == 3 ? 4 : lod == 2 ? 3 : 1;
        for (int j = 0; j < n; j++) {
            float a = a0 + (float)j * 1.5708f, ox = cosf(a), oz = sinf(a);
            float hj = h * (0.75f + 0.25f * fmodf(t->rnd * 3.7f + (float)j * 0.29f, 1.0f));
            Vector3 bj = { b.x + ox * 0.12f, b.y, b.z + oz * 0.12f };
            float w = (n == 1 ? 0.05f : 0.03f) * s, sx = n == 1 ? fx : -oz, sz = n == 1 ? fz : ox;
            blade(bj, hj, ox, oz, 0.07f, bx * 0.7f, bz * 0.7f, w, sx, sz, cb, ct, lod == 3 ? 0.55f : 0.0f, 0.8f, cm);
            if (L->cattail && (j & 1) == 0) { // la espiga, cerca de la punta
                float f = 0.8f, dx = (ox * 0.07f + bx * 0.7f) * hj * f * f, dz = (oz * 0.07f + bz * 0.7f) * hj * f * f;
                Vector3 e = { bj.x + dx, bj.y + hj * f, bj.z + dz };
                Color brown = { 92, 60, 36, 255 };
                float r = 0.045f * s, len = 0.14f * hj;
                tri((Vector3){ e.x - sx * r, e.y, e.z - sz * r }, brown, (Vector3){ e.x + sx * r, e.y, e.z + sz * r }, brown,
                    (Vector3){ e.x, e.y + len * 0.5f, e.z }, brown);
                tri((Vector3){ e.x - sx * r, e.y, e.z - sz * r }, brown, (Vector3){ e.x, e.y - len * 0.5f, e.z }, brown,
                    (Vector3){ e.x + sx * r, e.y, e.z + sz * r }, brown);
            }
        }
        break;
    }
    default: break;
    }
}

typedef struct {
    float x, z, r2, grow; // grow: cuanto volvio a crecer (lo calcinado reverdece al desvanecerse)
} Burnt;

void vegetation_draw(const Terrain *t, Camera3D cam, const Climate *c, float time, const FireField *fire, const VegClearing *clear,
                     int nclear, const Vector3 *walkers, int nwalkers) {
    g_drawn = 0;
    if (!g_world || g_world != t->world) return;
    float radius = platform_touch_ui() ? VEG_RADIUS_TOUCH : VEG_RADIUS;
    float ccx = ((float)g_cx + 0.5f) * FLORA_CELL, ccz = ((float)g_cz + 0.5f) * FLORA_CELL;
    // La estacion.
    float hk[FLORA_KINDS];
    for (int k = 0; k < FLORA_KINDS; k++) hk[k] = flora_height((FloraKind)k, c->year, c->greenness, c->autumn);
    VegLook L = { .year = c->year, .plume = flora_plume(c->year), .snow = clamp01(c->snow_cover), .stipa_tip = { 214, 210, 188, 255 } };
    L.cattail = c->year > 0.3f && c->year < 0.95f;
    float dormant = 1.0f - clamp01(c->greenness + c->autumn);
    L.fern = mixc(mixc((Color){ 156, 92, 44, 255 }, (Color){ 70, 116, 56, 255 }, c->greenness * 1.4f), (Color){ 112, 88, 64, 255 }, dormant);
    L.reed = mixc((Color){ 184, 162, 108, 255 }, (Color){ 88, 124, 62, 255 }, c->greenness * (1.0f - c->autumn));
    float lean = flora_lean(c->wind);
    // Los focos y la tierra calcinada cerca.
    Burnt burnt[VEG_BURNT];
    int nb = 0;
    if (fire) {
        float reach = (radius + 12.0f) * (radius + 12.0f);
        for (int i = 0; i < FIRE_MAX && nb < VEG_BURNT; i++) {
            const FireCell *f = &fire->cells[i];
            float dx = f->x - ccx, dz = f->z - ccz;
            if (f->used && dx * dx + dz * dz < reach) burnt[nb++] = (Burnt){ f->x, f->z, 2.2f * 2.2f, 0.0f };
        }
        for (int i = 0; i < SCORCH_MAX && nb < VEG_BURNT; i++) {
            const Scorch *s = &fire->scorch[i];
            float dx = s->x - ccx, dz = s->z - ccz;
            if (s->r > 0.0f && s->age < SCORCH_DAYS_SECONDS && dx * dx + dz * dz < reach)
                burnt[nb++] = (Burnt){ s->x, s->z, s->r * s->r, smooth(0.6f, 1.0f, s->age / SCORCH_DAYS_SECONDS) };
        }
    }
    // Lo que ve la camara: un cono algo mas ancho que la diagonal de la pantalla.
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    float hy = tanf(cam.fovy * 0.5f * DEG2RAD), hx = hy * 16.0f / 9.0f;
    float cone = cosf(fminf(atanf(sqrtf(hx * hx + hy * hy)) + 0.2f, 1.5f));
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling(); // hojas de una sola cara: se ven de los dos lados
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < VEG_SLOTS; i++) {
        const VegSlot *sl = &g_slots[i];
        if (!sl->n || sl->gx == INT_MIN || abs(sl->gx - g_cx) > VEG_GRID / 2 || abs(sl->gz - g_cz) > VEG_GRID / 2) continue;
        for (int q = 0; q < sl->n; q++) {
            const FloraTuft *tf = &sl->tuft[q];
            float dx = tf->x - ccx, dz = tf->z - ccz, d = sqrtf(dx * dx + dz * dz);
            if (d > radius) continue;
            float h = hk[tf->kind] * tf->size * smooth(radius, radius - VEG_FADE, d);
            if (h < 0.04f) continue;
            Vector3 to = { tf->x - cam.position.x, tf->y + h * 0.5f - cam.position.y, tf->z - cam.position.z };
            float cd = Vector3Length(to);
            if (cd > 2.5f && Vector3DotProduct(to, fwd) < cone * cd) continue;
            // La nieve: la del suelo de su celda (src/world/terrain.c); la stipa y los juncos secos asoman.
            float cover = L.snow * (1.0f - 0.6f * tf->desert / 255.0f), above = tf->y - t->look.snowline;
            if (above > 0.0f) continue; // el glaciar
            bool snowy = tf->patch < cover * 1.15f - 0.05f || (above > -6.0f && tf->patch < (above + 6.0f) / 6.0f);
            if (snowy) {
                if (tf->kind != FLORA_STIPA && tf->kind != FLORA_REED) continue;
                h -= 0.25f;
                if (h < 0.1f) continue;
            }
            bool gone = false;
            for (int k = 0; k < nclear && !gone; k++) {
                float ex = tf->x - clear[k].x, ez = tf->z - clear[k].z;
                gone = ex * ex + ez * ez < clear[k].r * clear[k].r;
            }
            for (int k = 0; k < nb && !gone; k++) {
                float ex = tf->x - burnt[k].x, ez = tf->z - burnt[k].z;
                if (ex * ex + ez * ez < burnt[k].r2) {
                    h *= burnt[k].grow;
                    gone = h < 0.04f;
                }
            }
            if (gone) continue;
            // El viento: olas que cruzan el pasto a favor; un poco de costado; y quien pasa la aparta.
            float phase = -(tf->x * WIND_DIR.x + tf->z * WIND_DIR.y) * 0.45f + tf->rnd * 1.5f;
            float s = lean + flora_sway(time, c->wind, phase), side = 0.35f * flora_sway(time * 1.3f, c->wind, phase + 1.7f);
            float bx = WIND_DIR.x * s - WIND_DIR.y * side, bz = WIND_DIR.y * s + WIND_DIR.x * side;
            for (int k = 0; k < nwalkers; k++) {
                float ex = tf->x - walkers[k].x, ez = tf->z - walkers[k].z, e = sqrtf(ex * ex + ez * ez);
                float push = flora_trample(e);
                if (push <= 0.0f) continue;
                bx += ex / fmaxf(e, 0.05f) * push * 0.9f, bz += ez / fmaxf(e, 0.05f) * push * 0.9f;
                h *= 1.0f - 0.35f * push;
            }
            float hd = sqrtf(to.x * to.x + to.z * to.z), fx = hd > 0.01f ? -to.z / hd : 1.0f, fz = hd > 0.01f ? to.x / hd : 0.0f;
            int lod = cd < 10.0f ? 3 : cd < 18.0f ? 2 : 1;
            draw_tuft(tf, c, &L, h, lod, bx, bz, fx, fz);
            g_drawn++;
        }
    }
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}
