#include "sim/buildkit.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sim/army.h"
#include "sim/forge.h"
#include "sim/lang.h"
#include "sim/voxmat.h"

#define CELL 0.25f      // m por celda: las casas, los talleres y las carretas
#define CELL_BIG 0.5f   // los palacios (de 14 m), para no pasarse de caras

// Una caja de celdas (los extremos incluidos).
typedef struct {
    int x0, y0, z0, x1, y1, z1;
} Box;

#define VEST_MAX 8 // los vestibulos: lo que queda hueco detras de cada puerta (ver seal)

typedef struct {
    VoxGrid *g;
    uint32_t s;   // el azar de la variante
    int gy;       // la celda del suelo (debajo, el cimiento)
    Region r;
    BkInfo *info;
    bool small;   // que quepa en BK_SMALL x BK_SMALL
    Box vest[VEST_MAX];
    int nvest;
} Kit;

// La huella de un edificio rectangular en la rejilla: [x0, x1] x [z0, z1] (el frente, en x1).
typedef struct {
    int x0, z0, x1, z1;
} Rect;

static uint32_t mix(uint32_t h) {
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

static float kr(Kit *k) {
    k->s = k->s * 1664525u + 1013904223u;
    return (float)(mix(k->s) >> 8) / 16777216.0f;
}

static int ki(Kit *k, int lo, int hi) {
    int v = lo + (int)(kr(k) * (float)(hi - lo + 1));
    return v > hi ? hi : v;
}

static bool kc(Kit *k, float p) { return kr(k) < p; }

// ---------------------------------------------------------------- piezas
static void fill(Kit *k, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t m) { vox_box(k->g, x0, y0, z0, x1, y1, z1, m); }
static void hole(Kit *k, int x0, int y0, int z0, int x1, int y1, int z1) { vox_carve(k->g, x0, y0, z0, x1, y1, z1); }
static void put(Kit *k, int x, int y, int z, uint8_t m) { vox_set(k->g, x, y, z, m); }

// La rejilla: la huella de L x W celdas (de cell m) con margin celdas alrededor (aleros, vigas que
// asoman, el porche, los detalles) y above celdas sobre el suelo, mas el cimiento.
static bool grid_rect(Kit *k, int L, int W, int above, int margin, float cell, Rect *out) {
    if (!vox_init(k->g, L + 2 * margin, BK_FOUND + above, W + 2 * margin, cell)) return false;
    k->gy = BK_FOUND;
    *out = (Rect){ margin, margin, margin + L - 1, margin + W - 1 };
    return true;
}

// El cimiento bajo una huella, para que en una ladera no quede en el aire.
static void footing(Kit *k, Rect a, uint8_t m) { fill(k, a.x0, 0, a.z0, a.x1, k->gy - 1, a.z1, m); }

// Los cuatro muros de espesor t de una huella, de y0 a y1 (el adentro no se toca).
static void walls(Kit *k, Rect a, int y0, int y1, int t, uint8_t m) {
    fill(k, a.x0, y0, a.z0, a.x1, y1, a.z0 + t - 1, m);
    fill(k, a.x0, y0, a.z1 - t + 1, a.x1, y1, a.z1, m);
    fill(k, a.x0, y0, a.z0, a.x0 + t - 1, y1, a.z1, m);
    fill(k, a.x1 - t + 1, y0, a.z0, a.x1, y1, a.z1, m);
}

// Cambia un material por otro en una caja (from = 0: cualquiera que no este vacio).
static void recolor(Kit *k, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t from, uint8_t to) {
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) {
                uint8_t v = vox_get(k->g, x, y, z);
                if (v && (!from || v == from)) put(k, x, y, z, to);
            }
}

// Piedras de dos tonos (la piedra de los muros, a manchas).
static void stones(Kit *k, uint32_t seed) {
    VoxGrid *g = k->g;
    for (int y = 0; y < g->ny; y++)
        for (int z = 0; z < g->nz; z++)
            for (int x = 0; x < g->nx; x++)
                if (vox_get(g, x, y, z) == M_STONE && vox_noise3((float)x * 0.7f, (float)y * 0.7f, (float)z * 0.7f, seed) > 0.62f)
                    put(k, x, y, z, M_STONE_DARK);
}

// El vestibulo de una puerta: la caja que queda hueca, el hueco y detras (lo demas de adentro se rellena).
static void vestibule(Kit *k, int x0, int y0, int z0, int x1, int y1, int z1) {
    if (k->nvest < VEST_MAX) k->vest[k->nvest++] = (Box){ x0, y0, z0, x1, y1, z1 };
}

// Las celdas de hondo de un vestibulo (1,25 m).
static int vest_cells(const Kit *k) { return (int)(1.25f / k->g->size + 0.5f); }

// La puerta en el muro del frente: de la cara de afuera (x) hacia adentro t celdas, centrada en zc,
// de dw de ancho y dh de alto, con su marco (frame, o 0).
static void door(Kit *k, int x, int t, int zc, int dw, int dh, uint8_t frame) {
    int z0 = zc - dw / 2, z1 = z0 + dw - 1, y0 = k->gy, y1 = k->gy + dh - 1;
    hole(k, x - t + 1, y0, z0, x, y1, z1);
    vestibule(k, x - t - vest_cells(k) + 1, y0, z0, x, y1, z1);
    if (frame) {
        fill(k, x, y1 + 1, z0 - 1, x, y1 + 1, z1 + 1, frame);
        fill(k, x, y0, z0 - 1, x, y1, z0 - 1, frame);
        fill(k, x, y0, z1 + 1, x, y1, z1 + 1, frame);
    }
    k->info->door_x = x, k->info->door_y = y0, k->info->door_z0 = z0, k->info->door_z1 = z1;
}

// Techo plano: la caña, las vigas que asoman adelante y atras (cada tres celdas), el barro encima
// y el pretil. Devuelve la ultima capa.
static int flat_roof(Kit *k, Rect a, int y, int parapet, uint8_t mud) {
    fill(k, a.x0, y, a.z0, a.x1, y, a.z1, M_REED);
    for (int z = a.z0 + 1; z <= a.z1 - 1; z += 3) fill(k, a.x0 - 1, y, z, a.x1 + 1, y, z, M_WOOD_DARK);
    fill(k, a.x0, y + 1, a.z0, a.x1, y + 1, a.z1, mud);
    if (parapet > 0) walls(k, a, y + 2, y + 1 + parapet, 1, mud);
    return y + 1 + parapet;
}

// Techo a dos aguas sobre una huella, desde la capa y: la cumbrera a lo largo de z (along_z) o de
// x, a 45 grados, con alero de ov celdas a los costados. gable (o 0): el hastial de las puntas
// (zarzo, tablas), con dos celdas de techo alrededor. Devuelve la capa de la cumbrera.
static int gable_roof(Kit *k, Rect a, int y, bool along_z, int ov, uint8_t m, uint8_t gable) {
    int lo = (along_z ? a.x0 : a.z0) - ov, hi = (along_z ? a.x1 : a.z1) + ov;
    int b0 = along_z ? a.z0 : a.x0, b1 = along_z ? a.z1 : a.x1, top = y;
    for (int i = 0; lo + i <= hi - i; i++) {
        int yy = y + i, p = lo + i, q = hi - i;
        if (along_z) fill(k, p, yy, b0, q, yy, b1, m);
        else fill(k, b0, yy, p, b1, yy, q, m);
        if (gable && q - p > 4) {
            if (along_z) fill(k, p + 2, yy, b0, q - 2, yy, b0, gable), fill(k, p + 2, yy, b1, q - 2, yy, b1, gable);
            else fill(k, b0, yy, p + 2, b0, yy, q - 2, gable), fill(k, b1, yy, p + 2, b1, yy, q - 2, gable);
        }
        top = yy;
    }
    return top;
}

// Un muro redondo (yurta, casa redonda) de espesor t, de y0 a y1.
static void round_wall(Kit *k, float cx, float cz, float r, int y0, int y1, int t, uint8_t m) {
    vox_cylinder(k->g, cx, cz, y0, y1, r, r, m);
    vox_cylinder(k->g, cx, cz, y0, y1, r - (float)t, r - (float)t, M_NONE);
}

// La puerta de un muro redondo, al frente (+X): el hueco y el marco; el umbral queda en info.
static void round_door(Kit *k, float cx, float cz, float r, int dw, int dh, uint8_t frame) {
    int xo = (int)floorf(cx + r - 0.5f), zc = (int)floorf(cz);
    while (xo + 1 < k->g->nx && vox_get(k->g, xo + 1, k->gy, zc)) xo++; // la cara de afuera
    door(k, xo, 3, zc, dw, dh, frame);
}

// ---------------------------------------------------------------- detalles
static void jar(Kit *k, int x, int z) { // un cantaro de barro
    int h = ki(k, 2, 3);
    vox_cylinder(k->g, (float)x + 0.5f, (float)z + 0.5f, k->gy, k->gy + h - 1, 0.9f, 0.9f, kc(k, 0.5f) ? M_CLAY : M_RED);
    put(k, x, k->gy + h, z, M_CLAY);
}

static void woodpile(Kit *k, int x0, int z0, int x1, int z1, int h) { // leña: hileras de troncos
    for (int y = 0; y < h; y++) fill(k, x0, k->gy + y, z0, x1, k->gy + y, z1, (y & 1) ? M_WOOD : M_WOOD_DARK);
}

static void bread_oven(Kit *k, int x, int z) { // el horno de pan (tandir): una cupula de barro con la boca negra
    vox_ellipsoid(k->g, (float)x + 0.5f, (float)k->gy, (float)z + 0.5f, 2.4f, 3.2f, 2.4f, M_ADOBE, true);
    put(k, x + 2, k->gy + 1, z, M_SLAG);
    put(k, x, k->gy + 3, z, M_SLAG);
}

static void hide_rack(Kit *k, int x, int z0, int z1) { // pieles al sol
    fill(k, x, k->gy, z0, x, k->gy + 6, z0, M_WOOD_DARK);
    fill(k, x, k->gy, z1, x, k->gy + 6, z1, M_WOOD_DARK);
    fill(k, x, k->gy + 6, z0, x, k->gy + 6, z1, M_WOOD);
    if (z1 - z0 > 2) fill(k, x, k->gy + 3, z0 + 1, x, k->gy + 5, z1 - 1, M_HIDE);
}

static void ladder(Kit *k, int x, int z, int y0, int y1) { // escalera de mano, de pie contra un muro
    for (int y = y0; y <= y1; y++) {
        put(k, x, y, z, M_WOOD_DARK);
        put(k, x, y, z + 2, M_WOOD_DARK);
        if ((y - y0) % 2 == 1) put(k, x, y, z + 1, M_WOOD);
    }
}

static void spear_rack(Kit *k, int x, int z0, int z1) { // el armero: un travesaño con lanzas de pie
    fill(k, x, k->gy + 4, z0, x, k->gy + 4, z1, M_WOOD_DARK);
    for (int z = z0; z <= z1; z += 2) {
        fill(k, x, k->gy, z, x, k->gy + 7, z, M_WOOD);
        put(k, x, k->gy + 8, z, M_STONE_DARK);
    }
}

// Una rueda de carro de pie (en el plano x-y), de radio r, en z.
static void wheel(Kit *k, int cx, int cy, int z, int r) {
    for (int y = cy - r; y <= cy + r; y++)
        for (int x = cx - r; x <= cx + r; x++) {
            float d = sqrtf((float)((x - cx) * (x - cx) + (y - cy) * (y - cy)));
            if (d <= (float)r + 0.3f) put(k, x, y, z, d > (float)r - 1.0f ? M_WOOD_DARK : (d < 1.0f ? M_WOOD_DARK : M_WOOD));
        }
}

// ---------------------------------------------------------------- las casas
// La casa de techo plano: adobe sobre zocalo de piedra (desierto) o piedra abajo y adobe arriba
// (altiplano). En bloque, en L o con patio; a veces con un cuarto arriba o un porche de postes.
static bool house_flat(Kit *k, int L, int W, int plan) {
    bool desert = k->r == REGION_DESERT;
    int H = ki(k, 11, 13), t = 2, porch = !desert && plan != 2 && kc(k, 0.5f) ? 6 : 0;
    bool upper = plan == 3;
    Rect a;
    if (!grid_rect(k, L, W, H + (upper ? H + 2 : 0) + 6, 3 + porch, CELL, &a)) return false;
    uint8_t wall = desert ? M_ADOBE : M_STONE, mud = desert ? M_ADOBE : M_EARTH, trim = desert ? M_BLUE : M_WOOD_DARK;
    int y0 = k->gy, y1 = k->gy + H - 1;
    footing(k, a, M_STONE);
    if (plan == 2) { // con patio: el ala del fondo y las de los costados alrededor, el muro del frente con el porton
        int back = a.x0 + L / 2 - 1, side = W / 4;
        Rect bk = { a.x0, a.z0, back, a.z1 }, s0 = { a.x0, a.z0, a.x1, a.z0 + side }, s1 = { a.x0, a.z1 - side, a.x1, a.z1 };
        Rect wings[3] = { bk, s0, s1 };
        for (int i = 0; i < 3; i++) fill(k, wings[i].x0, y0, wings[i].z0, wings[i].x1, y1, wings[i].z1, wall);
        for (int i = 0; i < 3; i++) hole(k, wings[i].x0 + t, y0, wings[i].z0 + t, wings[i].x1 - t, y1, wings[i].z1 - t);
        fill(k, a.x1 - 1, y0, a.z0, a.x1, y1 - 3, a.z1, wall); // el muro del frente, mas bajo
        hole(k, back + 1, y0, s0.z1 + 1, a.x1 - 2, y1 + 8, s1.z0 - 1); // el patio, al cielo
        // Las puertas de las alas, al patio, con su vestibulo.
        int zc = (a.z0 + a.z1) / 2, mx = (a.x0 + a.x1) / 2, v = vest_cells(k);
        hole(k, back - t + 1, y0, zc - 2, back, y0 + 7, zc + 1);
        vestibule(k, back - t - v + 1, y0, zc - 2, back, y0 + 7, zc + 1);
        hole(k, mx, y0, s0.z1 - t + 1, mx + 3, y0 + 7, s0.z1);
        vestibule(k, mx, y0, s0.z1 - t - v + 1, mx + 3, y0 + 7, s0.z1);
        hole(k, mx, y0, s1.z0, mx + 3, y0 + 7, s1.z0 + t - 1);
        vestibule(k, mx, y0, s1.z0, mx + 3, y0 + 7, s1.z0 + t + v - 1);
        for (int i = 0; i < 3; i++) flat_roof(k, wings[i], y1 + 1, desert ? 2 : 1, mud);
        door(k, a.x1, 2, zc, 5, (y1 - 3) - y0, M_WOOD_DARK); // el porton, con su dintel en lo alto del muro
        bread_oven(k, back + 4, s0.z1 + 4);
        jar(k, back + 2, s1.z0 - 2);
        if (kc(k, 0.6f)) jar(k, back + 2, s1.z0 - 4);
        ladder(k, back + 1, s0.z1 + 8, y0, y1 + 1);
    } else {
        Rect main = a, wing = a;
        if (plan == 1) { // en L: el cuerpo del frente y un ala atras, a un costado; adelante del ala, el corral
            main.z1 = a.z0 + W * 3 / 5;
            wing.x1 = a.x0 + L / 2;
        }
        fill(k, main.x0, y0, main.z0, main.x1, y1, main.z1, wall);
        if (plan == 1) fill(k, wing.x0, y0, wing.z0, wing.x1, y1, wing.z1, wall);
        hole(k, main.x0 + t, y0, main.z0 + t, main.x1 - t, y1, main.z1 - t);
        if (plan == 1) hole(k, wing.x0 + t, y0, wing.z0 + t, wing.x1 - t, y1, wing.z1 - t);
        flat_roof(k, main, y1 + 1, desert ? 2 : 1, mud);
        if (plan == 1) flat_roof(k, wing, y1 + 1, desert ? 2 : 1, mud);
        int zc = main.z0 + (main.z1 - main.z0) / 2 + ki(k, -2, 2);
        door(k, main.x1, t, zc, 4, 8, trim);
        if (plan == 1) { // el patio de adelante del ala: el horno y los cantaros
            bread_oven(k, wing.x1 + 5, wing.z1 - 4);
            jar(k, wing.x1 + 2, main.z1 + 3);
        } else {
            jar(k, main.x1 + 2, zc + 4);
            if (kc(k, 0.5f)) jar(k, main.x1 + 2, zc - 4);
        }
        if (upper) { // el cuarto de arriba, al fondo, con su puerta a la azotea y una escalera desde afuera
            Rect up = { main.x0, main.z0, main.x0 + (L / 2), main.z1 };
            int u0 = y1 + 3, u1 = u0 + H - 4;
            walls(k, up, u0, u1, t, wall);
            hole(k, up.x1 - t + 1, u0, (up.z0 + up.z1) / 2 - 1, up.x1, u0 + 6, (up.z0 + up.z1) / 2 + 2);
            flat_roof(k, up, u1 + 1, 1, mud);
            ladder(k, main.x1 - 4, main.z1 + 1, y0, y1 + 2);
        }
        // Ventanucos altos en los costados.
        for (int i = 0; i < 2; i++) {
            int x = main.x0 + ki(k, 3, (main.x1 - main.x0) - 5);
            hole(k, x, y0 + 8, main.z0, x + 1, y0 + 9, main.z0 + t - 1);
            hole(k, x, y0 + 8, main.z1 - t + 1, x + 1, y0 + 9, main.z1);
        }
        if (porch) { // el porche: postes y un techo plano adelante
            Rect pr = { main.x1 + 1, main.z0 + 1, main.x1 + porch - 1, main.z1 - 1 };
            for (int z = pr.z0; z <= pr.z1; z += 6) fill(k, pr.x1, y0, z, pr.x1, y1, z, M_WOOD_DARK);
            fill(k, pr.x1, y0, pr.z1, pr.x1, y1, pr.z1, M_WOOD_DARK);
            fill(k, pr.x0, y1 + 1, pr.z0, pr.x1, y1 + 1, pr.z1, M_WOOD_DARK);
            fill(k, pr.x0, y1 + 2, pr.z0, pr.x1, y1 + 2, pr.z1, mud);
            hole(k, pr.x0, y0, k->info->door_z0, pr.x1 - 1, y0 + 7, k->info->door_z1); // el paso a la puerta
        }
        if (kc(k, 0.5f)) woodpile(k, main.x0 + 2, main.z0 - 2, main.x0 + 9, main.z0 - 1, 3);
    }
    if (!desert) recolor(k, a.x0 - 1, y0 + 5, a.z0 - 1, a.x1 + 1, y1 + 1, a.z1 + 1, M_STONE, M_ADOBE); // arriba, el adobe
    else recolor(k, a.x0, y0, a.z0, a.x1, y0 + 1, a.z1, M_ADOBE, M_STONE);                     // el zocalo de piedra
    stones(k, 5u + k->s % 7u);
    k->info->enclosed = true;
    return true;
}

// La casa de postes: paneles de zarzo con barro entre postes y techo de paja a dos aguas (la
// cumbrera a lo largo, la puerta en el costado largo); o redonda, con techo conico.
static bool house_post(Kit *k, int L, int W, bool round) {
    int H = ki(k, 7, 8);
    if (round) {
        float r = (float)(L < W ? L : W) * 0.5f;
        int rise = (int)(r + 2.0f);
        Rect a;
        if (!grid_rect(k, L, W, H + rise + 3, 6, CELL, &a)) return false;
        float cx = (float)(a.x0 + a.x1 + 1) * 0.5f, cz = (float)(a.z0 + a.z1 + 1) * 0.5f;
        vox_cylinder(k->g, cx, cz, 0, k->gy - 1, r, r, M_EARTH);
        round_wall(k, cx, cz, r, k->gy, k->gy + H - 1, 1, M_DAUB);
        for (int i = 0; i < 12; i++) { // los postes de la pared
            float ang = (float)i / 12.0f * 6.2831853f;
            int x = (int)floorf(cx + cosf(ang) * (r - 0.5f)), z = (int)floorf(cz + sinf(ang) * (r - 0.5f));
            fill(k, x, k->gy, z, x, k->gy + H - 1, z, M_WOOD_DARK);
        }
        vox_cylinder(k->g, cx, cz, k->gy + H - 2, k->gy + H - 2 + rise, r + 2.0f, 0.8f, M_THATCH); // el cono, con alero
        vox_cylinder(k->g, cx, cz, k->gy, k->gy + H - 3, r - 1.0f, r - 1.0f, M_NONE);
        round_door(k, cx, cz, r, 4, 7, M_WOOD_DARK);
        // El porche de la puerta: dos postes y un tejadillo de paja.
        int dx = k->info->door_x, z0 = k->info->door_z0 - 1, z1 = k->info->door_z1 + 1;
        fill(k, dx + 3, k->gy, z0, dx + 3, k->gy + 7, z0, M_WOOD_DARK);
        fill(k, dx + 3, k->gy, z1, dx + 3, k->gy + 7, z1, M_WOOD_DARK);
        fill(k, dx + 1, k->gy + 8, z0 - 1, dx + 4, k->gy + 8, z1 + 1, M_THATCH);
        fill(k, dx + 1, k->gy + 9, z0, dx + 3, k->gy + 9, z1, M_THATCH);
        if (kc(k, 0.6f)) woodpile(k, a.x0, a.z1 - 1, a.x0 + 6, a.z1, 3);
        k->info->enclosed = true;
        return true;
    }
    Rect a;
    if (!grid_rect(k, L, W, H + L / 2 + 6, 4, CELL, &a)) return false;
    footing(k, a, M_EARTH);
    walls(k, a, k->gy, k->gy + H - 1, 1, M_DAUB);
    fill(k, a.x0, k->gy, a.z0, a.x1, k->gy, a.z1, M_WOOD_DARK); // la solera
    hole(k, a.x0 + 1, k->gy, a.z0 + 1, a.x1 - 1, k->gy, a.z1 - 1);
    int step = ki(k, 6, 7);
    for (int z = a.z0; z <= a.z1; z += step) { // los postes, en los costados largos
        fill(k, a.x0, k->gy, z, a.x0, k->gy + H - 1, z, M_WOOD_DARK);
        fill(k, a.x1, k->gy, z, a.x1, k->gy + H - 1, z, M_WOOD_DARK);
    }
    fill(k, a.x0, k->gy, a.z1, a.x0, k->gy + H - 1, a.z1, M_WOOD_DARK), fill(k, a.x1, k->gy, a.z1, a.x1, k->gy + H - 1, a.z1, M_WOOD_DARK);
    gable_roof(k, a, k->gy + H - 2, true, 2, M_THATCH, M_DAUB);
    int zc = (a.z0 + a.z1) / 2 + ki(k, -3, 3);
    door(k, a.x1, 1, zc, 4, 7, M_WOOD_DARK);
    hole(k, a.x1 + 1, k->gy, k->info->door_z0, a.x1 + 2, k->gy + 6, k->info->door_z1); // el alero no tapa la puerta
    if (kc(k, 0.7f)) woodpile(k, a.x1 + 1, a.z1 - 7, a.x1 + 2, a.z1 - 1, 4);
    if (kc(k, 0.4f)) hide_rack(k, a.x1 + 3, a.z0 + 1, a.z0 + 5);
    k->info->enclosed = true;
    return true;
}

// La casa larga de turba: muros gruesos de turba sobre piedra, techo de turba con hierba a dos
// aguas (la cumbrera a lo largo), la puerta en el costado largo y el secadero de pescado.
static bool house_turf(Kit *k, int L, int W) {
    int H = 6, t = 3;
    Rect a;
    if (!grid_rect(k, L, W, H + L / 2 + 4, 4, CELL, &a)) return false;
    footing(k, a, M_STONE_DARK);
    walls(k, a, k->gy, k->gy + H - 1, t, M_TURF);
    walls(k, a, k->gy, k->gy + 1, t, M_STONE_DARK); // la base de piedra
    int top = gable_roof(k, a, k->gy + H - 1, true, 1, M_TURF, M_WOOD_DARK);
    // La hierba del techo: la capa de afuera de cada agua.
    for (int y = k->gy + H - 1; y <= top; y++)
        for (int z = a.z0 + 1; z <= a.z1 - 1; z++)
            for (int x = a.x0 - 1; x <= a.x1 + 1; x++)
                if (vox_get(k->g, x, y, z) == M_TURF && !vox_get(k->g, x, y + 1, z) && kc(k, 0.75f)) put(k, x, y, z, M_GRASS);
    int zc = (a.z0 + a.z1) / 2 + ki(k, -4, 4);
    door(k, a.x1, t, zc, 4, 7, M_WOOD_DARK);
    hole(k, a.x1 + 1, k->gy, k->info->door_z0, a.x1 + 1, k->gy + 6, k->info->door_z1); // el alero no tapa la puerta
    // El secadero de pescado, al costado.
    int sx = a.x1 + 2, z0 = a.z0 + 2, z1 = a.z0 + (W < 30 ? 6 : 9);
    if (kc(k, 0.7f)) {
        fill(k, sx, k->gy, z0, sx, k->gy + 6, z0, M_WOOD_DARK), fill(k, sx, k->gy, z1, sx, k->gy + 6, z1, M_WOOD_DARK);
        fill(k, sx, k->gy + 6, z0, sx, k->gy + 6, z1, M_WOOD);
        for (int z = z0 + 1; z < z1; z += 2) fill(k, sx, k->gy + 4, z, sx, k->gy + 5, z, M_SLAG);
    }
    if (kc(k, 0.6f)) woodpile(k, a.x0 - 2, a.z1 - 6, a.x0 - 1, a.z1 - 1, 3);
    k->info->enclosed = true;
    return true;
}

// La yurta: muro de fieltro, techo conico bajo, la corona con su humero, la puerta pintada al
// frente, las sogas y, en las ricas, franjas de colores y la corona dorada.
static void yurt_body(Kit *k, float cx, float cz, float r, int y0, int wall_h, int rise, bool rich) {
    uint8_t felt = rich || kc(k, 0.7f) ? M_FELT : M_FELT_DARK;
    round_wall(k, cx, cz, r, y0, y0 + wall_h - 1, 1, felt);
    vox_cylinder(k->g, cx, cz, y0 + wall_h, y0 + wall_h + rise, r + 0.6f, r * 0.18f + 0.6f, felt == M_FELT ? M_FELT_ROOF : M_FELT_DARK);
    vox_cylinder(k->g, cx, cz, y0, y0 + wall_h + rise - 2, r - 1.0f, r * 0.18f - 0.4f, M_NONE); // adentro, vacio
    int top = y0 + wall_h + rise;
    vox_cylinder(k->g, cx, cz, top, top, r * 0.18f + 0.9f, r * 0.18f + 0.9f, rich ? M_YELLOW : M_WOOD_DARK); // la corona
    vox_cylinder(k->g, cx, cz, top - 1, top + 1, r * 0.18f - 0.2f, r * 0.18f - 0.2f, M_NONE);           // el humero
    int bands = rich ? 2 : kc(k, 0.5f);
    uint8_t col = kc(k, 0.5f) ? M_RED : M_BLUE;
    for (int b = 0; b < bands; b++) { // franjas: cada otra celda, como un bordado
        int y = y0 + wall_h - 2 - b * 3;
        for (int i = 0; i < 64; i++) {
            float ang = (float)i / 64.0f * 6.2831853f;
            if (i % 2) put(k, (int)floorf(cx + cosf(ang) * (r - 0.5f)), y, (int)floorf(cz + sinf(ang) * (r - 0.5f)), b ? M_BLUE : col);
        }
    }
    for (int y = y0 + 2; y < y0 + wall_h; y += wall_h > 4 ? wall_h - 3 : wall_h) // las sogas
        for (int i = 0; i < 48; i++) {
            float ang = (float)i / 48.0f * 6.2831853f;
            put(k, (int)floorf(cx + cosf(ang) * (r + 0.1f)), y, (int)floorf(cz + sinf(ang) * (r + 0.1f)), M_HIDE);
        }
}

static bool house_yurt(Kit *k, int D) {
    float r = (float)D * 0.5f;
    int wall_h = ki(k, 6, 7), rise = (int)(r * 0.45f) + 1;
    Rect a;
    if (!grid_rect(k, D, D, wall_h + rise + 4, 5, CELL, &a)) return false;
    float cx = (float)(a.x0 + a.x1 + 1) * 0.5f, cz = (float)(a.z0 + a.z1 + 1) * 0.5f;
    vox_cylinder(k->g, cx, cz, 0, k->gy - 1, r, r, M_EARTH);
    bool rich = kc(k, 0.25f);
    yurt_body(k, cx, cz, r, k->gy, wall_h, rise, rich);
    static const uint8_t PAINT[3] = { M_RED, M_BLUE, M_YELLOW };
    round_door(k, cx, cz, r, 3, 6, PAINT[ki(k, 0, 2)]);
    fill(k, k->info->door_x, k->gy + 7, k->info->door_z0, k->info->door_x, k->gy + 7, k->info->door_z1, M_FELT_DARK); // el fieltro de la puerta, enrollado
    if (kc(k, 0.5f)) hide_rack(k, a.x1 + 3, a.z0, a.z0 + 4);
    if (kc(k, 0.4f)) wheel(k, a.x0 + 3, k->gy + 3, a.z1 + 2, 3); // una rueda de carro, de pie al costado
    k->info->enclosed = true;
    return true;
}

static bool house(Kit *k) {
    bool small = k->small;
    switch (k->r) {
    case REGION_DESERT:
    case REGION_HIGHLAND: {
        int plan; // 0: bloque; 1: en L; 2: con patio; 3: con un cuarto arriba
        if (small) plan = kc(k, 0.7f) ? 0 : 1;
        else if (k->r == REGION_DESERT) plan = kc(k, 0.45f) ? 2 : kc(k, 0.55f) ? 1 : 3;
        else plan = kc(k, 0.2f) ? 2 : kc(k, 0.5f) ? 1 : 3;
        int L = small ? ki(k, 18, 22) : ki(k, 24, 28), W = small ? ki(k, 18, 22) : ki(k, 30, 38);
        if (plan == 2) L = ki(k, 26, 30), W = ki(k, 34, 38);
        return house_flat(k, L, W, plan);
    }
    case REGION_FOREST: {
        bool round = kc(k, 0.3f);
        if (round) {
            int D = small ? ki(k, 18, 22) : ki(k, 24, 30);
            return house_post(k, D, D, true);
        }
        return house_post(k, small ? ki(k, 16, 20) : ki(k, 20, 24), small ? ki(k, 18, 22) : ki(k, 28, 40), false);
    }
    case REGION_FJORD: return house_turf(k, small ? ki(k, 18, 20) : ki(k, 20, 24), small ? ki(k, 20, 22) : ki(k, 36, 50));
    default: return house_yurt(k, small ? ki(k, 18, 22) : ki(k, 22, 28));
    }
}

// ---------------------------------------------------------------- el palacio
// Una yurta chica cerrada (las de las esposas, junto a la orda): la puerta pintada, sin hueco.
static void small_yurt(Kit *k, float cx, float cz, float r, int y0) {
    yurt_body(k, cx, cz, r, y0, 3, 2, false);
    int xo = (int)floorf(cx + r - 0.5f), zc = (int)floorf(cz);
    fill(k, xo, y0, zc - 1, xo, y0 + 1, zc, kc(k, 0.5f) ? M_RED : M_BLUE);
}

// Un altar del fuego de piedra, en gradas, con la llama.
static void fire_altar(Kit *k, int x, int z, int y0) {
    fill(k, x, y0, z, x + 1, y0 + 1, z + 1, M_STONE);
    put(k, x, y0 + 2, z, M_RED), put(k, x + 1, y0 + 2, z + 1, M_RED);
    put(k, x, y0 + 3, z, M_YELLOW);
}

static bool palace(Kit *k) {
    Rect a;
    int S = ki(k, 26, 28); // la terraza o la tarima: 13-14 m
    if (!grid_rect(k, S, S, 26, 4, CELL_BIG, &a)) return false;
    int y0 = k->gy, cz = (a.z0 + a.z1) / 2;
    if (k->r == REGION_STEPPE) { // la orda: la gran yurta sobre su tarima de tablas, con los estandartes
        for (int y = 0; y < y0 + 2; y++) fill(k, a.x0, y, a.z0, a.x1, y, a.z1, (y & 1) ? M_WOOD : M_WOOD_DARK);
        fill(k, a.x1 + 1, y0, cz - 2, a.x1 + 2, y0, cz + 2, M_WOOD_DARK); // el escalon
        bool wives = kc(k, 0.6f); // las yurtas de las esposas, en las esquinas de atras
        float cx = (float)(a.x0 + a.x1 + 1) * 0.5f - 1.0f, czf = (float)(a.z0 + a.z1 + 1) * 0.5f, r = (float)S * (wives ? 0.31f : 0.36f);
        yurt_body(k, cx, czf, r, y0 + 2, 5, (int)(r * 0.5f), true);
        k->gy = y0 + 2; // la puerta, sobre la tarima
        round_door(k, cx, czf, r, 3, 4, kc(k, 0.5f) ? M_YELLOW : M_RED);
        k->gy = y0;
        if (wives)
            for (int s = 0; s < 2; s++)
                if (s == 0 || kc(k, 0.6f)) small_yurt(k, (float)a.x0 + 4.0f, s ? (float)a.z1 - 3.0f : (float)a.z0 + 4.0f, 3.2f, y0 + 2);
        int tugs = kc(k, 0.5f) ? 2 : 4;
        for (int i = 0; i < tugs; i++) { // los estandartes de crin (tug), a los lados de la puerta
            int z = cz + (i % 2 ? 1 : -1) * (S / 2 - 2 - (i / 2) * 3), x = a.x1 - 1 - (i / 2) * 3;
            fill(k, x, y0 + 2, z, x, y0 + 11 - (i / 2), z, M_WOOD_DARK);
            fill(k, x, y0 + 9 - (i / 2), z, x, y0 + 10 - (i / 2), z, M_FELT_DARK);
            put(k, x, y0 + 12 - (i / 2), z, M_YELLOW);
        }
        k->info->enclosed = true;
        return true;
    }
    if (k->r == REGION_FOREST || k->r == REGION_FJORD) { // la sala larga: postes y paja, o turba
        bool turf = k->r == REGION_FJORD;
        fill(k, a.x0, 0, a.z0, a.x1, y0 + 1, a.z1, turf ? M_STONE_DARK : M_EARTH); // la base
        Rect hall = { a.x0 + 4, a.z0 + 1, a.x1 - 4, a.z1 - 1 };
        int H = turf ? ki(k, 5, 6) : ki(k, 6, 7), ov = ki(k, 1, 2);
        walls(k, hall, y0 + 2, y0 + 1 + H, turf ? 2 : 1, turf ? M_TURF : M_DAUB);
        if (!turf)
            for (int z = hall.z0; z <= hall.z1; z += 3) {
                fill(k, hall.x0, y0 + 2, z, hall.x0, y0 + 1 + H, z, M_WOOD_DARK);
                fill(k, hall.x1, y0 + 2, z, hall.x1, y0 + 1 + H, z, M_WOOD_DARK);
            }
        int top = gable_roof(k, hall, y0 + H, true, ov, turf ? M_TURF : M_THATCH, turf ? M_WOOD_DARK : M_DAUB);
        if (turf)
            for (int y = y0 + H; y <= top; y++)
                for (int z = hall.z0; z <= hall.z1; z++)
                    for (int x = hall.x0 - ov; x <= hall.x1 + ov; x++)
                        if (vox_get(k->g, x, y, z) == M_TURF && !vox_get(k->g, x, y + 1, z)) put(k, x, y, z, M_GRASS);
        for (int s = 0; s <= 1; s++) put(k, (hall.x0 + hall.x1) / 2, top + 1, s ? hall.z1 : hall.z0, M_YELLOW); // los remates de la cumbrera
        k->gy = y0 + 2;
        door(k, hall.x1, turf ? 2 : 1, cz, 3, 4, M_RED);
        k->gy = y0;
        hole(k, hall.x1 + 1, y0 + 2, k->info->door_z0, hall.x1 + ov, y0 + 5, k->info->door_z1); // el alero no tapa la puerta
        fill(k, hall.x1 + 1, y0 + 2, cz - 3, hall.x1 + 1, y0 + 6, cz - 3, M_RED), fill(k, hall.x1 + 1, y0 + 2, cz + 3, hall.x1 + 1, y0 + 6, cz + 3, M_RED);
        if (kc(k, 0.7f)) { // los escudos redondos colgados del muro del frente, de colores
            static const uint8_t SH[3] = { M_RED, M_YELLOW, M_BLUE };
            for (int z = hall.z0 + 1, i = 0; z <= hall.z1 - 1; z += 2, i++)
                if (z < cz - 4 || z > cz + 4) put(k, hall.x1 + 1, y0 + 4, z, SH[i % 3]);
        }
        fill(k, a.x1 + 1, y0, cz - 2, a.x1 + 2, y0, cz + 2, M_STONE_DARK);
        k->info->enclosed = true;
        return true;
    }
    // La sala de columnas sobre su terraza (desierto: adobe, torres en las esquinas y el azul;
    // altiplano: piedra, contrafuertes, a veces una torre atras, y el rojo). El porche de columnas
    // al frente; delante de la escalinata, a veces, dos altares del fuego.
    bool desert = k->r == REGION_DESERT;
    uint8_t wall = desert ? M_ADOBE : M_STONE, trim = desert ? M_BLUE : M_RED;
    fill(k, a.x0, 0, a.z0, a.x1, y0 + 1, a.z1, desert ? M_ADOBE : M_STONE); // la terraza, de un metro
    recolor(k, a.x0, 0, a.z0, a.x1, y0, a.z1, M_ADOBE, M_STONE);
    for (int s = 0; s < 2; s++) fill(k, a.x1 + 1 + s, y0, cz - 3, a.x1 + 1 + s, y0 + 1 - s, cz + 3, M_STONE_DARK); // la escalinata
    int porch = ki(k, 6, 7);
    Rect hall = { a.x0 + 2, a.z0 + 2, a.x1 - porch, a.z1 - 2 };
    int H = ki(k, 8, 9), f = y0 + 2, gap = ki(k, 3, 4);
    walls(k, hall, f, f + H - 1, 1, wall);
    int roof = f + H;
    Rect cover = { hall.x0, hall.z0, a.x1 - 1, hall.z1 };
    flat_roof(k, cover, roof, 1, desert ? M_ADOBE : M_EARTH);
    hole(k, hall.x1 + 1, f, hall.z0 + 1, a.x1 - 1, roof - 1, hall.z1 - 1); // el porche, abierto
    k->gy = f;
    door(k, hall.x1, 1, cz, 3, 5, trim);
    k->gy = y0;
    // Las columnas del porche, de a pares a los lados de la puerta, sobre basas de piedra y con su capitel.
    int zlo = hall.z0 + (desert ? 3 : 1), zhi = hall.z1 - (desert ? 3 : 1);
    for (int j = 0;; j++) {
        int za = cz - 2 - gap * j, zb = cz + 3 + gap * j;
        if (za < zlo && zb > zhi) break;
        for (int s = 0; s < 2; s++) {
            int z = s ? zb : za;
            if (z < zlo || z > zhi) continue;
            put(k, a.x1 - 1, f, z, M_STONE_DARK);
            fill(k, a.x1 - 1, f + 1, z, a.x1 - 1, roof - 2, z, M_WOOD);
            put(k, a.x1 - 1, roof - 1, z, trim);
        }
    }
    for (int s = -1; s <= 1; s += 2) hole(k, hall.x1, f + 3, cz + s * 5, hall.x1, f + 4, cz + s * 5); // ventanas
    if (desert) { // las torres de las esquinas, un poco mas altas, con almenas escalonadas
        int cs[4][2] = { { hall.x0, hall.z0 }, { hall.x0, hall.z1 - 2 }, { a.x1 - 3, hall.z0 }, { a.x1 - 3, hall.z1 - 2 } };
        for (int i = 0; i < 4; i++) fill(k, cs[i][0], f, cs[i][1], cs[i][0] + 2, roof + 4, cs[i][1] + 2, M_ADOBE);
        for (int x = hall.x0; x <= a.x1 - 1; x += 2) put(k, x, roof + 3, hall.z0, M_ADOBE), put(k, x, roof + 3, hall.z1, M_ADOBE);
        if (kc(k, 0.7f)) fill(k, hall.x0, roof + 1, hall.z0, a.x1 - 1, roof + 1, hall.z0, M_BLUE); // la franja azul
    } else { // los contrafuertes de los muros urarteos
        for (int x = hall.x0; x <= hall.x1; x += 3) fill(k, x, f, hall.z0 - 1, x, roof, hall.z0 - 1, M_STONE), fill(k, x, f, hall.z1 + 1, x, roof, hall.z1 + 1, M_STONE);
        for (int z = hall.z0; z <= hall.z1; z += 3) fill(k, hall.x0 - 1, f, z, hall.x0 - 1, roof, z, M_STONE);
        if (kc(k, 0.6f)) { // la torre de atras, con su pretil
            Rect tw = { hall.x0 - 1, cz - 3, hall.x0 + 4, cz + 3 };
            fill(k, tw.x0, f, tw.z0, tw.x1, roof + 5, tw.z1, M_STONE);
            walls(k, tw, roof + 6, roof + 6, 1, M_STONE);
            for (int x = tw.x0; x <= tw.x1; x += 2) put(k, x, roof + 7, tw.z0, M_STONE), put(k, x, roof + 7, tw.z1, M_STONE);
        }
        stones(k, 21u);
    }
    if (kc(k, 0.5f)) // los altares del fuego delante de la escalinata
        for (int s = -1; s <= 1; s += 2) fire_altar(k, a.x1 + 2, cz + s * 6 - (s < 0 ? 1 : 0), y0);
    k->info->enclosed = true;
    return true;
}

// ---------------------------------------------------------------- talleres
// Un cobertizo abierto: el muro del fondo, los postes y el techo de la region.
static void shed(Kit *k, Rect a, int H) {
    uint8_t back = k->r == REGION_DESERT ? M_ADOBE : k->r == REGION_HIGHLAND ? M_STONE : k->r == REGION_FJORD ? M_TURF : M_WOOD_DARK;
    footing(k, a, M_EARTH);
    if (k->r == REGION_STEPPE) back = M_HIDE;
    fill(k, a.x0, k->gy, a.z0, a.x0 + 1, k->gy + H - 1, a.z1, back);
    int step = (a.z1 - a.z0) / 2 > 0 ? (a.z1 - a.z0) / 2 : 1;
    for (int z = a.z0; z <= a.z1; z += step) fill(k, a.x1, k->gy, z, a.x1, k->gy + H - 1, z, M_WOOD_DARK);
    switch (k->r) {
    case REGION_DESERT:
    case REGION_HIGHLAND: flat_roof(k, a, k->gy + H, 0, k->r == REGION_DESERT ? M_ADOBE : M_EARTH); break;
    case REGION_FOREST: gable_roof(k, a, k->gy + H - 1, true, 1, M_THATCH, 0); break;
    case REGION_FJORD: gable_roof(k, a, k->gy + H - 1, true, 1, M_TURF, 0); break;
    default: // el toldo de cuero, en pendiente hacia el frente
        for (int x = a.x0; x <= a.x1; x++) fill(k, x, k->gy + H + (a.x1 - x) / 3, a.z0, x, k->gy + H + (a.x1 - x) / 3, a.z1, M_HIDE);
        break;
    }
    if (k->r == REGION_HIGHLAND) stones(k, 23u);
}

static bool forge(Kit *k) {
    Rect a;
    int L = ki(k, 20, 24), W = ki(k, 20, 24), H = 10;
    if (!grid_rect(k, L, W, H + 12, 5, CELL, &a)) return false;
    shed(k, a, H);
    int cz = (a.z0 + a.z1) / 2, fx = a.x0 + 5;
    // El horno de cuba: una chimenea de barro de 1,25 m con la boca arriba, la tobera y el fuelle.
    vox_cylinder(k->g, (float)fx + 0.5f, (float)cz + 0.5f, k->gy, k->gy + 5, 2.1f, 1.6f, M_CLAY);
    vox_cylinder(k->g, (float)fx + 0.5f, (float)cz + 0.5f, k->gy + 3, k->gy + 5, 0.8f, 0.8f, M_NONE);
    put(k, fx, k->gy + 2, cz, M_RED); // la brasa en la boca
    put(k, fx, k->gy + 1, cz + 2, M_CLAY), put(k, fx, k->gy + 1, cz + 3, M_CLAY); // la tobera
    fill(k, fx - 1, k->gy, cz + 4, fx + 1, k->gy + 1, cz + 5, M_HIDE);           // el fuelle
    put(k, fx, k->gy + 2, cz + 5, M_WOOD);
    vox_ellipsoid(k->g, (float)fx + 0.5f, (float)k->gy, (float)cz - 4.5f, 2.2f, 1.6f, 1.8f, M_SLAG, true); // la escoria
    vox_ellipsoid(k->g, (float)(a.x0 + 3), (float)k->gy, (float)(a.z0 + 3), 2.4f, 2.0f, 2.4f, M_BASALT, true); // el carbon
    int ax = a.x1 - 6; // el yunque
    fill(k, ax, k->gy, cz + 2, ax + 1, k->gy + 2, cz + 3, M_STONE_DARK);
    fill(k, ax, k->gy + 3, cz + 2, ax + 1, k->gy + 3, cz + 3, M_SLAG);
    if (kc(k, 0.6f)) woodpile(k, a.x0 + 3, a.z1 - 3, a.x0 + 8, a.z1 - 1, 3);
    return true;
}

static bool carpentry(Kit *k) {
    Rect a;
    int L = ki(k, 20, 24), W = ki(k, 20, 24), H = 9;
    if (!grid_rect(k, L, W, H + 12, 5, CELL, &a)) return false;
    shed(k, a, H);
    int cz = (a.z0 + a.z1) / 2;
    for (int y = 0; y < 4; y++) // los troncos apilados, de largo
        fill(k, a.x0 + 3, k->gy + y, a.z0 + 2 + (y & 1), a.x1 - 6, k->gy + y, a.z0 + 2 + (y & 1), M_WOOD),
            fill(k, a.x0 + 3, k->gy + y, a.z0 + 4 + (y & 1), a.x1 - 6, k->gy + y, a.z0 + 4 + (y & 1), M_WOOD_DARK);
    // El caballete con un tronco a medio aserrar, y las tablas.
    fill(k, a.x1 + 2, k->gy, cz - 2, a.x1 + 2, k->gy + 2, cz - 2, M_WOOD_DARK), fill(k, a.x1 + 2, k->gy, cz + 2, a.x1 + 2, k->gy + 2, cz + 2, M_WOOD_DARK);
    fill(k, a.x1 + 1, k->gy + 3, cz - 4, a.x1 + 3, k->gy + 3, cz + 4, M_WOOD);
    fill(k, a.x0 + 4, k->gy, a.z1 - 4, a.x0 + 12, k->gy, a.z1 - 2, M_CARVE);
    return true;
}

static bool barracks(Kit *k) {
    bool ok;
    switch (k->r) { // la casa de la region, larga
    case REGION_DESERT:
    case REGION_HIGHLAND: ok = house_flat(k, 20, 40, 0); break;
    case REGION_FOREST: ok = house_post(k, 20, 40, false); break;
    case REGION_FJORD: ok = house_turf(k, 20, 44); break;
    default: ok = house_yurt(k, 26); break;
    }
    if (!ok) return false;
    // El armero de lanzas, delante, a un costado de la puerta.
    int x = k->info->door_x + 3, z0 = k->info->door_z1 + 3;
    spear_rack(k, x < k->g->nx ? x : k->g->nx - 1, z0, z0 + 8 < k->g->nz ? z0 + 8 : k->g->nz - 1);
    return true;
}

// ---------------------------------------------------------------- granero, templo y carreta
static bool granary(Kit *k) {
    Rect a;
    if (k->r == REGION_DESERT) { // el deposito de adobe y los silos en cupula
        int silos = ki(k, 2, 3), sx = ki(k, 8, 10), sh = ki(k, 8, 10);
        if (!grid_rect(k, sx + 7, silos * 7 + 1, 16, 4, CELL, &a)) return false;
        footing(k, a, M_STONE);
        Rect store = { a.x0, a.z0, a.x0 + sx - 1, a.z1 };
        int zc = (a.z0 + a.z1) / 2;
        fill(k, store.x0, k->gy, store.z0, store.x1, k->gy + sh - 1, store.z1, M_ADOBE);
        flat_roof(k, store, k->gy + sh, 1, M_ADOBE);
        fill(k, store.x1, k->gy, zc - 1, store.x1, k->gy + 6, zc + 1, M_WOOD_DARK); // la puerta cerrada
        for (int i = 0; i < silos; i++) {
            float z = (float)a.z0 + 3.5f + (float)i * 7.0f, ry = (float)ki(k, 6, 8);
            vox_ellipsoid(k->g, (float)a.x1 - 2.5f, (float)k->gy, z, 2.8f, ry, 2.8f, M_ADOBE, true);
            put(k, a.x1 - 3, k->gy + (int)ry, (int)z, M_WOOD_DARK); // la tapa de arriba
        }
        if (kc(k, 0.6f)) jar(k, store.x1 + 2, zc + 3);
        return true;
    }
    // El granero sobre postes: los postes enterrados, el piso de tablas en alto, las paredes, el techo
    // de la region, la trampilla y la escalera; abajo, a veces, sacos o cantaros.
    int L = k->small ? ki(k, 7, 8) : ki(k, 8, 10), W = k->small ? ki(k, 8, 9) : ki(k, 9, 12), legs = k->small ? 4 : 6;
    int lift = ki(k, 2, 4), H = ki(k, 5, 7), ov = ki(k, 1, 2);
    if (!grid_rect(k, L, W, lift + H + L / 2 + 5, 4, CELL, &a)) return false;
    for (int i = 0; i < legs; i++) { // los postes, enterrados
        int x = i % 2 ? a.x1 : a.x0, z = legs == 4 ? (i < 2 ? a.z0 : a.z1) : (i < 2 ? a.z0 : i < 4 ? (a.z0 + a.z1) / 2 : a.z1);
        fill(k, x, 0, z, x, k->gy + lift - 1, z, M_WOOD_DARK);
    }
    int f = k->gy + lift, zc = (a.z0 + a.z1) / 2 + ki(k, -1, 1);
    fill(k, a.x0, f, a.z0, a.x1, f, a.z1, M_WOOD_DARK); // el piso
    uint8_t wall = k->r == REGION_FOREST ? M_DAUB : M_WOOD;
    walls(k, a, f + 1, f + H, 1, wall);
    if (wall == M_WOOD)
        for (int y = f + 2; y <= f + H; y += 2) walls(k, a, y, y, 1, M_WOOD_DARK); // las tablas
    uint8_t roof = k->r == REGION_FJORD ? M_TURF : k->r == REGION_HIGHLAND ? M_WOOD_DARK : M_THATCH;
    gable_roof(k, a, f + H, true, ov, roof, wall);
    hole(k, a.x1, f + 2, zc - 1, a.x1, f + 4, zc); // la trampilla
    ladder(k, a.x1 + 1, zc - 1, k->gy, f + 2);
    if (kc(k, 0.5f)) fill(k, a.x0 + 1, k->gy, a.z0 + 1, a.x0 + 2, k->gy, a.z0 + 3, M_HIDE); // sacos
    if (kc(k, 0.5f)) jar(k, a.x1 - 1, a.z1 - 1);
    return true;
}

static bool temple(Kit *k) {
    // La planta en cruz: un cuadrado en el centro (mas alto, como una torre) y cuatro brazos de 2 m
    // de ancho; la puerta en el brazo del frente; adentro, el altar del fuego. En lo alto, almenas
    // escalonadas (desierto) o un pretil; afuera, a veces, dos altares del fuego junto a la puerta.
    bool desert = k->r == REGION_DESERT;
    int C = k->small ? 14 : 2 * ki(k, 8, 9), A = k->small ? 5 : ki(k, 6, 7), B = 8;
    int H = ki(k, 11, 12), HT = ki(k, 16, 19), t = 2;
    Rect a;
    if (!grid_rect(k, C + 2 * A, C + 2 * A, HT + 8, 4, CELL, &a)) return false;
    uint8_t wall = desert ? M_ADOBE : M_STONE, trim = desert ? M_BLUE : M_RED;
    int c0x = a.x0 + A, c0z = a.z0 + A, c1x = c0x + C - 1, c1z = c0z + C - 1, mz = (a.z0 + a.z1) / 2, mx = (a.x0 + a.x1) / 2;
    Rect center = { c0x, c0z, c1x, c1z };
    Rect arms[4] = { { c1x + 1, mz - B / 2, a.x1, mz + B / 2 - 1 },  // el frente (+X)
                     { a.x0, mz - B / 2, c0x - 1, mz + B / 2 - 1 },  // el fondo
                     { mx - B / 2, a.z0, mx + B / 2 - 1, c0z - 1 },  // los costados
                     { mx - B / 2, c1z + 1, mx + B / 2 - 1, a.z1 } };
    fill(k, center.x0, 0, center.z0, center.x1, k->gy - 1, center.z1, M_STONE);
    fill(k, center.x0, k->gy, center.z0, center.x1, k->gy + HT - 1, center.z1, wall);
    for (int i = 0; i < 4; i++) {
        footing(k, arms[i], M_STONE);
        fill(k, arms[i].x0, k->gy, arms[i].z0, arms[i].x1, k->gy + H - 1, arms[i].z1, wall);
    }
    hole(k, center.x0 + t, k->gy, center.z0 + t, center.x1 - t, k->gy + HT - 1, center.z1 - t);
    // Cada brazo, hueco y abierto a la sala del centro (atraviesa su muro).
    hole(k, c1x - t + 1, k->gy, arms[0].z0 + t, a.x1 - t, k->gy + H - 1, arms[0].z1 - t);
    hole(k, a.x0 + t, k->gy, arms[1].z0 + t, c0x + t - 1, k->gy + H - 1, arms[1].z1 - t);
    hole(k, arms[2].x0 + t, k->gy, a.z0 + t, arms[2].x1 - t, k->gy + H - 1, c0z + t - 1);
    hole(k, arms[3].x0 + t, k->gy, c1z - t + 1, arms[3].x1 - t, k->gy + H - 1, a.z1 - t);
    for (int i = 0; i < 4; i++) flat_roof(k, arms[i], k->gy + H, 1, wall);
    int top = flat_roof(k, center, k->gy + HT, 2, wall);
    for (int i = 0; i < C; i += 2) // las almenas, cada otra celda del pretil (en el desierto, mas altas)
        for (int s = 0; s < 2; s++)
            for (int u = 1; u <= (desert ? 2 : 1); u++) {
                put(k, center.x0 + i, top + u, s ? center.z1 : center.z0, wall);
                put(k, s ? center.x1 : center.x0, top + u, center.z0 + i, wall);
            }
    door(k, a.x1, t, mz, 4, 9, trim);
    // El altar del fuego, en gradas, con la llama.
    int ax = (c0x + c1x) / 2, az = mz;
    fill(k, ax - 2, k->gy, az - 2, ax + 1, k->gy + 1, az + 1, M_STONE);
    fill(k, ax - 1, k->gy + 2, az - 1, ax, k->gy + 3, az, M_STONE_DARK);
    fill(k, ax - 1, k->gy + 4, az - 1, ax, k->gy + 4, az, M_RED);
    put(k, ax, k->gy + 5, az, M_YELLOW);
    for (int s = -1; s <= 1; s += 2) { // ventanucos de la torre, a los costados
        hole(k, mx + s * 5, k->gy + HT - 5, center.z0, mx + s * 5, k->gy + HT - 4, center.z0 + t - 1);
        hole(k, mx + s * 5, k->gy + HT - 5, center.z1 - t + 1, mx + s * 5, k->gy + HT - 4, center.z1);
    }
    if (kc(k, 0.6f)) // los altares del fuego de afuera, a los lados de la puerta
        for (int s = -1; s <= 1; s += 2) fire_altar(k, a.x1 + 2, mz + s * (B / 2 + 2) - (s < 0 ? 1 : 0), k->gy);
    if (!desert) {
        recolor(k, a.x0, k->gy + 6, a.z0, a.x1, top + 2, a.z1, M_STONE, M_ADOBE);
        stones(k, 31u);
    }
    k->info->enclosed = true;
    return true;
}

static bool wagon(Kit *k) {
    // El carro: la plataforma a 0,75 m, cuatro o seis ruedas grandes, la casilla de fieltro en
    // boveda (uno o tres cuartos) y la lanza al frente, con su yugo.
    int L = k->small ? ki(k, 11, 13) : ki(k, 13, 16), W = ki(k, 6, 7), bed = k->gy + 3, R = W / 2 + 1;
    bool six = kc(k, 0.4f), three = !k->small && kc(k, 0.5f);
    Rect a;
    if (!grid_rect(k, L, W, 3 + R + 4, 9, CELL, &a)) return false;
    fill(k, a.x0, bed, a.z0, a.x1, bed, a.z1, M_WOOD_DARK);
    int axles[3] = { a.x0 + 3, a.x1 - 3, (a.x0 + a.x1) / 2 };
    for (int i = 0; i < (six ? 3 : 2); i++) {
        wheel(k, axles[i], k->gy + 3, a.z0 - 1, 3);
        wheel(k, axles[i], k->gy + 3, a.z1 + 1, 3);
        fill(k, axles[i], k->gy + 3, a.z0 - 1, axles[i], k->gy + 3, a.z1 + 1, M_WOOD_DARK); // el eje
    }
    float cz = (float)(a.z0 + a.z1 + 1) * 0.5f, cy = (float)bed + 0.5f;
    for (int x = a.x0 + 1; x <= a.x1 - 1; x++) // la boveda de fieltro
        for (int y = bed + 1; y <= bed + R + 1; y++)
            for (int z = a.z0; z <= a.z1; z++) {
                float dz = (float)z + 0.5f - cz, dy = (float)y + 0.5f - cy, d = sqrtf(dz * dz + dy * dy);
                if (d <= (float)R + 0.3f && d > (float)R - 1.0f) put(k, x, y, z, M_FELT);
            }
    for (int s = 0; s <= 1; s++) { // las puntas, cerradas con fieltro gris; adelante, la entrada
        int x = s ? a.x1 - 1 : a.x0 + 1;
        for (int y = bed + 1; y <= bed + R + 1; y++)
            for (int z = a.z0; z <= a.z1; z++) {
                float dz = (float)z + 0.5f - cz, dy = (float)y + 0.5f - cy;
                if (sqrtf(dz * dz + dy * dy) <= (float)R + 0.3f) put(k, x, y, z, M_FELT_DARK);
            }
    }
    hole(k, a.x1 - 1, bed + 1, (int)cz - 1, a.x1 - 1, bed + 3, (int)cz);
    if (three) // tres cuartos: dos tabiques que se ven como franjas
        for (int s = 1; s <= 2; s++) {
            int x = a.x0 + 1 + (L - 2) * s / 3;
            for (int y = bed + 1; y <= bed + R + 1; y++)
                for (int z = a.z0; z <= a.z1; z++)
                    if (vox_get(k->g, x, y, z) == M_FELT) put(k, x, y, z, M_FELT_DARK);
        }
    fill(k, a.x1 + 1, bed, (int)cz - 1, a.x1 + 7, bed, (int)cz - 1, M_WOOD_DARK); // la lanza
    fill(k, a.x1 + 7, bed, (int)cz - 4, a.x1 + 7, bed, (int)cz + 2, M_WOOD);      // el yugo
    return true;
}

// ---------------------------------------------------------------- la muralla de la capital
// Las piezas de la muralla (src/sim/army.h): el tramo entre dos torres, la torre y la puerta con su
// antemuro (la puerta acodada), al estilo de la region. El origen del modelo esta sobre la linea de
// la muralla: X hacia afuera, Z a lo largo de ella; el adarve, a CITY_WALL_HEIGHT del suelo.
#define WALL_HALF 1.0f // m: medio espesor del muro

// La celda de un punto en m desde el origen del modelo (el centro de la base de la rejilla).
static int cell_x(const Kit *k, float x) { return (int)floorf(x / k->g->size + (float)k->g->nx * 0.5f); }
static int cell_z(const Kit *k, float z) { return (int)floorf(z / k->g->size + (float)k->g->nz * 0.5f); }
static int cell_y(const Kit *k, float y) { return k->gy + (int)floorf(y / k->g->size + 0.001f); }

// Una rejilla de celdas de cell m que va de -hx a hx (m) en X, de -hz a hz en Z y de top m de alto
// (grid_m: de 0,5 m).
static bool grid_cells(Kit *k, float hx, float hz, float top, float cell) {
    int nx = 2 * (int)ceilf(hx / cell), nz = 2 * (int)ceilf(hz / cell);
    if (!vox_init(k->g, nx, BK_FOUND + (int)ceilf(top / cell), nz, cell)) return false;
    k->gy = BK_FOUND;
    return true;
}

static bool grid_m(Kit *k, float hx, float hz, float top) { return grid_cells(k, hx, hz, top, CELL_BIG); }

// Llena la caja en m (x de x0 a x1, y de y0 a y1, z de z0 a z1; y en m sobre el suelo, negativa: el cimiento).
static void fill_m(Kit *k, float x0, float y0, float z0, float x1, float y1, float z1, uint8_t m) {
    fill(k, cell_x(k, x0), cell_y(k, y0), cell_z(k, z0), cell_x(k, x1 - 0.01f), cell_y(k, y1 - 0.01f), cell_z(k, z1 - 0.01f), m);
}

// El perfil del muro de cada region en una celda del corte: u, la distancia (m) a la linea del muro
// hacia afuera; i, la celda a lo largo (para las almenas y las vigas). Llena la columna de esa celda.
static void rampart_column(Kit *k, int x, int z, float u, int i) {
    int g = k->gy, cy = (int)(CITY_WALL_HEIGHT / CELL_BIG); // el adarve: 9 celdas (4,5 m)
    switch (k->r) {
    case REGION_DESERT: // adobe sobre zocalo de piedra, almenas escalonadas
        if (fabsf(u) <= WALL_HALF + 0.5f) fill(k, x, 0, z, x, g + 1, z, M_STONE);
        if (fabsf(u) <= WALL_HALF) fill(k, x, g + 2, z, x, g + cy - 1, z, M_ADOBE);
        if (u >= WALL_HALF - 0.5f && u <= WALL_HALF && i % 2 == 0) fill(k, x, g + cy, z, x, g + cy + (i % 4 == 0 ? 2 : 1), z, M_ADOBE);
        break;
    case REGION_HIGHLAND: // piedra, con contrafuertes y almenas
        if (fabsf(u) <= WALL_HALF) fill(k, x, 0, z, x, g + cy - 1, z, M_STONE);
        if (u > WALL_HALF && u <= WALL_HALF + 0.5f && i % 5 < 2) fill(k, x, 0, z, x, g + cy - 2, z, M_STONE);
        if (u >= WALL_HALF - 0.5f && u <= WALL_HALF && i % 2 == 0) fill(k, x, g + cy, z, x, g + cy, z, M_STONE);
        break;
    case REGION_FOREST: { // el murus gallicus: cara de piedra con las cabezas de las vigas, tierra detras y el parapeto de troncos
        int top = u >= -0.5f ? cy - 2 : u >= -1.0f ? cy - 4 : cy - 6;
        if (u >= -1.5f && u <= WALL_HALF) fill(k, x, 0, z, x, g + top - 1, z, M_EARTH);
        if (u > WALL_HALF - 0.5f && u <= WALL_HALF) {
            for (int y = g; y < g + cy - 2; y++) put(k, x, y, z, i % 3 == 0 && (y - g) % 3 == 1 ? M_WOOD_DARK : M_STONE);
            fill(k, x, g + cy - 2, z, x, g + cy - (i % 2), z, M_WOOD);
        }
        break;
    }
    case REGION_FJORD: // piedra seca abajo, turba arriba con su pasto, y un parapeto de tablas
        if (fabsf(u) <= WALL_HALF) {
            fill(k, x, 0, z, x, g + 3, z, M_STONE_DARK);
            fill(k, x, g + 4, z, x, g + cy - 2, z, M_TURF);
            put(k, x, g + cy - 1, z, M_GRASS);
        }
        if (u > WALL_HALF - 0.5f && u <= WALL_HALF && i % 3 != 2) fill(k, x, g + cy, z, x, g + cy + 1, z, M_WOOD_DARK);
        break;
    default: { // la estepa: un terraplen de tierra con la empalizada de troncos encima
        int bank = fabsf(u) < 0.5f ? 4 : fabsf(u) <= 1.0f ? 3 : fabsf(u) <= 1.5f ? 1 : 0;
        if (bank) fill(k, x, 0, z, x, g + bank - 1, z, M_EARTH);
        if (u >= 0.0f && u < 0.5f) fill(k, x, g + 4, z, x, g + cy - (i % 2), z, i % 2 ? M_WOOD : M_WOOD_DARK);
        break;
    }
    }
}

// Un lienzo de muralla a lo largo de z (along_z) o de x, de a0 a a1 (m), con su linea en c (m, en el
// otro eje) y su cara de afuera hacia out (+1 o -1 en ese otro eje).
static void rampart(Kit *k, bool along_z, float a0, float a1, float c, int out) {
    float half = WALL_HALF + 1.5f;
    int i0 = along_z ? cell_z(k, a0) : cell_x(k, a0), i1 = along_z ? cell_z(k, a1 - 0.01f) : cell_x(k, a1 - 0.01f);
    int j0 = along_z ? cell_x(k, c - half) : cell_z(k, c - half), j1 = along_z ? cell_x(k, c + half - 0.01f) : cell_z(k, c + half - 0.01f);
    for (int i = i0; i <= i1; i++)
        for (int j = j0; j <= j1; j++) {
            float mid = ((float)j + 0.5f - (float)(along_z ? k->g->nx : k->g->nz) * 0.5f) * k->g->size; // el centro de la celda, en m
            rampart_column(k, along_z ? j : i, along_z ? i : j, (mid - c) * (float)out, i);
        }
}

// Una torre de la region de lado side (m) centrada en (x, z) (m), de alto h (m).
static void wall_tower(Kit *k, float x, float z, float side, float h) {
    float a = side * 0.5f;
    switch (k->r) {
    case REGION_DESERT:
    case REGION_HIGHLAND: {
        bool desert = k->r == REGION_DESERT;
        uint8_t m = desert ? M_ADOBE : M_STONE;
        fill_m(k, x - a, -BK_FOUND * CELL_BIG, z - a, x + a, h, z + a, m);
        if (desert) fill_m(k, x - a - 0.5f, -BK_FOUND * CELL_BIG, z - a - 0.5f, x + a + 0.5f, 1.0f, z + a + 0.5f, M_STONE); // el zocalo
        fill_m(k, x - a, h - 2.0f, z - a, x + a, h - 1.5f, z + a, desert ? M_BLUE : M_STONE_DARK);                          // la franja
        for (int s = -1; s <= 1; s += 2) // las almenas, en las cuatro caras
            for (float t = -a; t < a - 0.01f; t += 1.0f) {
                fill_m(k, x + t, h, z + (float)s * (a - 0.5f), x + t + 0.5f, h + (desert ? 1.0f : 0.5f), z + (float)s * (a - 0.5f) + 0.5f, m);
                fill_m(k, x + (float)s * (a - 0.5f), h, z + t, x + (float)s * (a - 0.5f) + 0.5f, h + (desert ? 1.0f : 0.5f), z + t + 0.5f, m);
            }
        for (float y = 2.0f; y < h - 2.5f; y += 2.5f) // las saeteras de afuera
            hole(k, cell_x(k, x + a - 0.5f), cell_y(k, y), cell_z(k, z - 0.25f), cell_x(k, x + a - 0.01f), cell_y(k, y + 0.9f), cell_z(k, z + 0.24f));
        break;
    }
    case REGION_FJORD: // piedra oscura y techo de turba a dos aguas
        fill_m(k, x - a, -BK_FOUND * CELL_BIG, z - a, x + a, h - 1.5f, z + a, M_STONE_DARK);
        for (int i = 0; i < 4; i++) fill_m(k, x - a - 0.5f + (float)i * 0.5f, h - 1.5f + (float)i * 0.5f, z - a - 0.5f, x + a + 0.5f - (float)i * 0.5f, h - 1.0f + (float)i * 0.5f, z + a + 0.5f, i == 3 ? M_GRASS : M_TURF);
        break;
    default: { // de troncos: los cuatro postes, la garita de tablas arriba y el techo en punta
        bool forest = k->r == REGION_FOREST;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sz = -1; sz <= 1; sz += 2) fill_m(k, x + (float)sx * (a - 0.5f), -BK_FOUND * CELL_BIG, z + (float)sz * (a - 0.5f), x + (float)sx * (a - 0.5f) + 0.5f, h - 1.5f, z + (float)sz * (a - 0.5f) + 0.5f, M_WOOD_DARK);
        fill_m(k, x - a, h - 4.0f, z - a, x + a, h - 3.5f, z + a, M_WOOD_DARK); // el piso
        for (float y = h - 3.5f; y < h - 1.5f; y += 0.5f) { // las tablas, con saeteras
            uint8_t m = ((int)(y * 2.0f) % 2) ? M_WOOD : M_WOOD_DARK;
            fill_m(k, x - a, y, z - a, x + a, y + 0.5f, z - a + 0.5f, m), fill_m(k, x - a, y, z + a - 0.5f, x + a, y + 0.5f, z + a, m);
            fill_m(k, x - a, y, z - a, x - a + 0.5f, y + 0.5f, z + a, m), fill_m(k, x + a - 0.5f, y, z - a, x + a, y + 0.5f, z + a, m);
        }
        hole(k, cell_x(k, x + a - 0.5f), cell_y(k, h - 2.5f), cell_z(k, z - 0.5f), cell_x(k, x + a - 0.01f), cell_y(k, h - 2.01f), cell_z(k, z + 0.49f));
        for (int i = 0; i < 4; i++) // el techo en punta: paja en el bosque, cuero en la estepa
            fill_m(k, x - a - 0.5f + (float)i * 0.5f, h - 1.5f + (float)i * 0.5f, z - a - 0.5f + (float)i * 0.5f, x + a + 0.5f - (float)i * 0.5f, h - 1.0f + (float)i * 0.5f, z + a + 0.5f - (float)i * 0.5f, forest ? M_THATCH : M_HIDE);
        put(k, cell_x(k, x), cell_y(k, h + 0.5f), cell_z(k, z), M_RED); // un banderin
        break;
    }
    }
}

// El tramo entre dos torres: un tercio de la cuerda (src/game/city_game.c pone tres por tramo).
static bool wall_piece(Kit *k) {
    float len = CITY_WALL_R * sinf(3.14159265f / (float)CITY_WALL_SEGMENTS) * 2.0f / 3.0f + 0.25f;
    if (!grid_m(k, WALL_HALF + 2.5f, len * 0.5f, CITY_WALL_HEIGHT + 2.0f)) return false;
    rampart(k, true, -len * 0.5f, len * 0.5f, 0.0f, 1);
    { // una gargola, o una escalera de mano por adentro
        int z = cell_z(k, (kr(k) - 0.5f) * len * 0.6f);
        if (k->r == REGION_STEPPE || k->r == REGION_FOREST) ladder(k, cell_x(k, -WALL_HALF - 0.75f), z, k->gy, k->gy + (int)(CITY_WALL_HEIGHT / CELL_BIG) - 2);
        else put(k, cell_x(k, WALL_HALF + 0.25f), k->gy + (int)(CITY_WALL_HEIGHT / CELL_BIG) - 2, z, M_WOOD_DARK);
    }
    if (k->r == REGION_HIGHLAND) stones(k, 41u + k->s % 5u);
    return true;
}

static bool wall_tower_piece(Kit *k) {
    bool wood = k->r == REGION_STEPPE || k->r == REGION_FOREST;
    float side = wood ? 4.0f : 4.5f, h = CITY_WALL_HEIGHT + (wood ? 3.5f : 2.5f) + (float)ki(k, 0, 2) * 0.5f;
    if (!grid_m(k, side * 0.5f + 1.5f, side * 0.5f + 1.5f, h + 2.5f)) return false;
    wall_tower(k, 0.5f, 0.0f, side, h); // un poco hacia afuera
    if (k->r == REGION_HIGHLAND) stones(k, 43u + k->s % 5u);
    return true;
}

// La puerta: los dos lienzos a los lados del hueco, las torres de la puerta hacia adentro, el puente
// del adarve sobre el hueco y el antemuro adelante (el muro de adelante y el costado cerrado, con
// sus torres): variante par, cerrado hacia +Z; impar, hacia -Z (city_gate_closed_side).
static bool gate_piece(Kit *k, int variant) {
    float lh = CITY_WALL_R * sinf(3.14159265f / (float)CITY_WALL_SEGMENTS), gh = CITY_GATE_HALF, out = CITY_BARBICAN_OUT, half = CITY_BARBICAN_HALF;
    float s = (float)city_gate_closed_side(variant), cy = CITY_WALL_HEIGHT;
    bool wood = k->r == REGION_STEPPE || k->r == REGION_FOREST;
    if (!grid_m(k, out + 3.5f, lh + 0.5f, cy + 4.0f)) return false;
    rampart(k, true, -lh, -gh, 0.0f, 1), rampart(k, true, gh, lh, 0.0f, 1); // los lienzos de la muralla
    // El antemuro: el muro de adelante y el costado cerrado (su cara de afuera, lejos del patio).
    rampart(k, true, -half, half, out, 1);
    rampart(k, false, WALL_HALF, out + WALL_HALF, s * half, (int)s);
    // Las torres: a los lados del hueco, hacia adentro; en la esquina cerrada y en la punta de la boca.
    float tw = wood ? 3.0f : 3.5f;
    for (int t = -1; t <= 1; t += 2) wall_tower(k, -WALL_HALF - tw * 0.5f + 0.5f, (float)t * (gh + tw * 0.5f), tw, cy + 2.5f);
    wall_tower(k, out + 0.25f, s * (half + 0.25f), 3.0f, cy + 1.5f);
    wall_tower(k, out + 0.25f, -s * (half - 1.25f), 2.5f, cy + 1.0f);
    // El puente del adarve sobre el hueco, y el hueco libre hasta el puente.
    hole(k, cell_x(k, -WALL_HALF - 0.5f), k->gy, cell_z(k, -gh), cell_x(k, WALL_HALF + 0.49f), cell_y(k, cy - 0.51f), cell_z(k, gh - 0.01f));
    fill_m(k, -WALL_HALF, cy - 0.5f, -gh, WALL_HALF, cy, gh, wood ? M_WOOD_DARK : (k->r == REGION_DESERT ? M_ADOBE : M_STONE));
    // El patio delante de la puerta y la boca del antemuro, libres hasta el costado cerrado.
    float z_lo = s > 0.0f ? -half - 2.0f : -(half - WALL_HALF), z_hi = s > 0.0f ? half - WALL_HALF : half + 2.0f;
    hole(k, cell_x(k, WALL_HALF), k->gy, cell_z(k, z_lo), cell_x(k, out - WALL_HALF - 0.01f), cell_y(k, cy + 3.0f), cell_z(k, z_hi - 0.01f));
    if (k->r == REGION_HIGHLAND) stones(k, 47u);
    return true;
}

// ---------------------------------------------------------------- las ruinas
// Un palo de celdas de (x0, y0, z0) a (x1, y1, z1): ramas, vigas caidas, sogas.
static void pole(Kit *k, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t m) {
    int n = abs(x1 - x0);
    if (abs(y1 - y0) > n) n = abs(y1 - y0);
    if (abs(z1 - z0) > n) n = abs(z1 - z0);
    for (int i = 0; i <= n; i++) {
        float t = n ? (float)i / (float)n : 0.0f;
        put(k, x0 + (int)floorf((float)(x1 - x0) * t + 0.5f), y0 + (int)floorf((float)(y1 - y0) * t + 0.5f), z0 + (int)floorf((float)(z1 - z0) * t + 0.5f), m);
    }
}

// Lo que cae primero es lo de arriba: cada columna pierde lo que pasa de una altura que da el ruido
// (sin nada en el aire; el cimiento queda). keep: cuanto queda en promedio, de la altura de la rejilla.
static void decay(Kit *k, uint32_t seed, float keep) {
    VoxGrid *g = k->g;
    float top = (float)(g->ny - k->gy);
    for (int z = 0; z < g->nz; z++)
        for (int x = 0; x < g->nx; x++) {
            float n = vox_noise3((float)x * 0.16f, 0.5f, (float)z * 0.16f, seed) * 0.7f + vox_noise3((float)x * 0.55f, 2.5f, (float)z * 0.55f, seed + 7u) * 0.3f;
            int cut = k->gy + (int)(top * keep * (1.8f * n * n + 0.05f));
            for (int y = cut < k->gy + 1 ? k->gy + 1 : cut; y < g->ny; y++) put(k, x, y, z, M_NONE);
        }
}

// Lo que crece encima: pasto, musgo o arena sobre parte de las celdas al cielo (no en la madera).
static void overgrow(Kit *k, uint32_t seed, float p) {
    VoxGrid *g = k->g;
    uint8_t m = k->r == REGION_DESERT ? M_SAND2 : k->r == REGION_FJORD || k->r == REGION_FOREST ? M_MOSS : M_GRASS;
    for (int z = 0; z < g->nz; z++)
        for (int x = 0; x < g->nx; x++)
            for (int y = g->ny - 1; y >= k->gy; y--) {
                uint8_t v = vox_get(g, x, y, z);
                if (!v) continue;
                if (v != M_WOOD && v != M_WOOD_DARK && vox_noise3((float)x * 0.5f, (float)y * 0.5f, (float)z * 0.5f, seed) < p) put(k, x, y, z, m);
                break;
            }
}

// Lo caido: un monton de r m en (x, z) (m), de lo que estaba hecho el muro de la region.
static void heap(Kit *k, float x, float z, float r) {
    static const uint8_t FALLEN[REGION_COUNT] = { [REGION_STEPPE] = M_EARTH, [REGION_FOREST] = M_STONE, [REGION_HIGHLAND] = M_STONE,
                                                  [REGION_FJORD] = M_STONE_DARK, [REGION_DESERT] = M_ADOBE };
    float c = k->g->size;
    vox_ellipsoid(k->g, x / c + (float)k->g->nx * 0.5f, (float)k->gy, z / c + (float)k->g->nz * 0.5f, r / c, r * 0.5f / c, r / c, FALLEN[k->r], true);
}

// Los cuatro muros de t m de espesor de una caja (m), de y1 m de alto (con su cimiento).
static void ring_m(Kit *k, float x0, float z0, float x1, float z1, float y1, float t, uint8_t m) {
    float b = -(float)k->gy * k->g->size;
    fill_m(k, x0, b, z0, x1, y1, z0 + t, m), fill_m(k, x0, b, z1 - t, x1, y1, z1, m);
    fill_m(k, x0, b, z0, x0 + t, y1, z1, m), fill_m(k, x1 - t, b, z0, x1, y1, z1, m);
}

// La fortaleza derruida: el recinto cuadrado con la muralla de la region (el mismo perfil que la de
// las capitales), sus cuatro torres, la puerta al frente y los cimientos de la sala y el pozo de
// adentro; despues el tiempo (lo alto cae primero), lo caido al pie y el pasto, el musgo o la arena.
static bool ruin_fort(Kit *k) {
    float hs = 9.0f + (float)ki(k, 0, 2), tw = k->r == REGION_STEPPE || k->r == REGION_FOREST ? 3.0f : 3.5f;
    if (!grid_m(k, hs + 3.5f, hs + 3.5f, CITY_WALL_HEIGHT + 4.0f)) return false;
    rampart(k, true, -hs, hs, hs, 1), rampart(k, true, -hs, hs, -hs, -1);   // el frente y el fondo
    rampart(k, false, -hs, hs, hs, 1), rampart(k, false, -hs, hs, -hs, -1); // los costados
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2) wall_tower(k, (float)sx * hs, (float)sz * hs, tw, CITY_WALL_HEIGHT + 2.0f);
    hole(k, cell_x(k, hs - WALL_HALF - 1.5f), k->gy, cell_z(k, -2.0f), cell_x(k, hs + WALL_HALF + 1.49f), k->g->ny - 1, cell_z(k, 1.99f)); // la puerta
    float hx0 = -hs + 3.0f, hx1 = hx0 + 5.0f + kr(k) * 2.0f, hz = 3.0f + kr(k) * 2.0f; // la sala del fondo
    ring_m(k, hx0, -hz, hx1, hz, 2.0f, 0.5f, k->r == REGION_DESERT ? M_ADOBE : M_STONE);
    hole(k, cell_x(k, hx1 - 0.5f), k->gy, cell_z(k, -0.75f), cell_x(k, hx1 - 0.01f), k->g->ny - 1, cell_z(k, 0.74f));
    float wx = hs * 0.4f, wz = (kr(k) - 0.5f) * hs; // el pozo
    vox_cylinder(k->g, (float)cell_x(k, wx) + 0.5f, (float)cell_z(k, wz) + 0.5f, 0, k->gy + 1, 2.2f, 2.2f, M_STONE);
    vox_cylinder(k->g, (float)cell_x(k, wx) + 0.5f, (float)cell_z(k, wz) + 0.5f, k->gy - 1, k->gy + 1, 1.2f, 1.2f, M_NONE);
    vox_cylinder(k->g, (float)cell_x(k, wx) + 0.5f, (float)cell_z(k, wz) + 0.5f, k->gy - 1, k->gy - 1, 1.2f, 1.2f, M_WATER);
    decay(k, k->s, 0.62f);
    for (int i = 0; i < 10; i++) { // lo caido, al pie de los muros (adentro y afuera)
        float along = (kr(k) * 2.0f - 1.0f) * hs, off = hs + (kc(k, 0.5f) ? 2.5f : -2.5f) * (kr(k) * 0.5f + 0.75f);
        int side = ki(k, 0, 3);
        heap(k, side < 2 ? (side ? off : -off) : along, side < 2 ? along : (side == 2 ? off : -off), 1.0f + kr(k) * 1.2f);
    }
    if (k->r == REGION_HIGHLAND || k->r == REGION_FOREST) stones(k, 53u + k->s % 7u);
    overgrow(k, k->s + 11u, 0.42f);
    return true;
}

// La ciudad enterrada (el desierto): dos hileras de casas de adobe a los lados de la calle, unas con
// su cupula y otras con lo que queda del techo de caña, las puertas a la calle y dos columnas del
// templo; despues el tiempo y la arena en dunas, que las tapa hasta la mitad.
static bool ruin_city(Kit *k) {
    float hx = 12.0f, hz = 10.0f;
    if (!grid_m(k, hx + 1.0f, hz + 1.0f, 6.0f)) return false;
    float c = k->g->size;
    for (int i = 0; i < 6; i++) {
        float cx = -8.0f + (float)(i % 3) * 8.0f + (kr(k) - 0.5f) * 1.5f, cz = i < 3 ? -5.5f : 5.5f;
        float lx = 2.5f + kr(k), lz = 2.5f + kr(k) * 0.75f, h = 2.5f + kr(k) * 1.5f, street = i < 3 ? cz + lz : cz - lz;
        ring_m(k, cx - lx, cz - lz, cx + lx, cz + lz, h, 0.5f, M_ADOBE);
        ring_m(k, cx - lx - 0.5f, cz - lz - 0.5f, cx + lx + 0.5f, cz + lz + 0.5f, 0.5f, 1.0f, M_STONE); // el zocalo
        int sz = cell_z(k, i < 3 ? street - 0.5f : street); // la puerta, a la calle, y un ventanuco
        hole(k, cell_x(k, cx - 0.75f), cell_y(k, 0.5f), sz, cell_x(k, cx + 0.74f), cell_y(k, 2.2f), sz);
        hole(k, cell_x(k, cx + lx - 0.5f), cell_y(k, 1.5f), cell_z(k, cz - 0.25f), cell_x(k, cx + lx), cell_y(k, 1.99f), cell_z(k, cz + 0.24f));
        if (kc(k, 0.5f)) // la cupula
            vox_ellipsoid(k->g, (float)cell_x(k, cx) + 0.5f, (float)cell_y(k, h), (float)cell_z(k, cz) + 0.5f, lx / c, lx / c, lz / c, M_ADOBE, true);
        else // lo que queda del techo: vigas y caña
            for (float x = cx - lx; x < cx + lx; x += 1.0f) fill_m(k, x, h, cz - lz, x + 0.5f, h + 0.5f, cz + lz, kc(k, 0.5f) ? M_WOOD_DARK : M_REED);
    }
    for (int i = 0; i < 2; i++) { // las columnas del templo, al fondo de la calle
        float z = i ? 1.25f : -1.25f;
        vox_cylinder(k->g, (float)cell_x(k, -hx + 0.75f) + 0.5f, (float)cell_z(k, z) + 0.5f, 0, cell_y(k, 4.5f), 1.1f, 0.9f, M_CHALK);
        fill_m(k, -hx, 4.5f, z - 0.75f, -hx + 1.5f, 5.0f, z + 0.75f, M_CHALK_D);
    }
    decay(k, k->s, 0.75f);
    for (int z = 0; z < k->g->nz; z++) // la arena, en dunas
        for (int x = 0; x < k->g->nx; x++) {
            float n = vox_noise3((float)x * 0.09f, 4.5f, (float)z * 0.13f, k->s + 3u);
            int top = k->gy + (int)(n * n * 7.0f);
            for (int y = 0; y <= top; y++)
                if (!vox_get(k->g, x, y, z)) put(k, x, y, z, y == top ? M_SAND2 : M_SAND1);
        }
    return true;
}

// ---------------------------------------------------------------- las obras de la tribu
// A la medida de su id del inventario (assets/inventario.tsv), en celdas de 0,25 m, con el frente a +X.

// Un techo a dos aguas de cuero (la cumbrera de atras hacia adelante), de H celdas sobre una huella
// de W de ancho; atras, cerrado. Devuelve la capa de la cumbrera.
static int hide_roof(Kit *k, Rect a, int H, bool front_closed) {
    float cz = (float)(a.z0 + a.z1 + 1) * 0.5f, half = (float)(a.z1 - a.z0 + 1) * 0.5f;
    int ridge = k->gy;
    for (int z = a.z0; z <= a.z1; z++) {
        int top = k->gy + (int)((float)H * (1.0f - (fabsf((float)z + 0.5f - cz) - 0.5f) / half) + 0.5f);
        ridge = top > ridge ? top : ridge;
        for (int x = a.x0; x <= a.x1; x++)
            fill(k, x, top - 1 > k->gy ? top - 1 : k->gy, z, x, top, z, vox_noise3((float)x * 0.45f, (float)top * 0.45f, (float)z * 0.45f, k->s) > 0.62f ? M_FELT_DARK : M_HIDE);
        fill(k, a.x0, k->gy, z, a.x0, top, z, M_HIDE);
        if (front_closed) fill(k, a.x1, k->gy, z, a.x1, top, z, M_HIDE);
    }
    return ridge;
}

// El refugio: un armazon de ramas a dos aguas cubierto de pieles, cerrado atras y abierto adelante;
// adentro, el lecho de fieltro; delante, a veces, el fuego entre piedras y, al lado, la leña.
static bool shelter(Kit *k) {
    Rect a;
    int H = 7 + ki(k, 0, 1);
    if (!grid_rect(k, 12, 12, H + 3, 3, CELL, &a)) return false;
    footing(k, a, M_EARTH);
    int ridge = hide_roof(k, a, H, false), zc = (a.z0 + a.z1 + 1) / 2;
    for (int x = a.x0 + 1; x <= a.x1; x += 4) recolor(k, x, k->gy, a.z0, x, ridge, a.z1, 0, M_WOOD_DARK); // las varas
    recolor(k, a.x1, k->gy, a.z0, a.x1, ridge, a.z1, 0, M_WOOD_DARK);
    fill(k, a.x0 - 1, ridge + 1, zc - 1, a.x1 + 1, ridge + 1, zc, M_WOOD_DARK); // la cumbrera, que asoma
    put(k, a.x1 + 1, ridge + 2, zc - 2, M_WOOD_DARK), put(k, a.x1 + 1, ridge + 2, zc + 1, M_WOOD_DARK);
    fill(k, a.x0 + 2, k->gy, zc - 2, a.x0 + 6, k->gy, zc + 1, kc(k, 0.5f) ? M_RED : M_FELT_DARK); // el lecho
    if (kc(k, 0.5f)) { // el fuego entre piedras
        fill(k, a.x1 + 1, k->gy, zc - 1, a.x1 + 3, k->gy, zc + 1, M_STONE);
        put(k, a.x1 + 2, k->gy, zc, M_SLAG);
    }
    if (kc(k, 0.6f)) woodpile(k, a.x0 + 2, a.z1 + 1, a.x0 + 7, a.z1 + 2, 2);
    return true;
}

// Un tramo de empalizada: troncos de punta, de alturas desparejas, hundidos en la tierra y atados
// a un travesaño de un lado.
static bool palisade(Kit *k) {
    Rect a;
    if (!grid_rect(k, 2, 16, 11, 1, CELL, &a)) return false;
    for (int z = a.z0; z <= a.z1; z++) {
        int h = ki(k, 8, 10); // 2 a 2,5 m
        uint8_t m = z & 1 ? M_WOOD : M_WOOD_DARK;
        fill(k, a.x0, 0, z, a.x1, k->gy + h - 2, z, m);
        put(k, z % 3 ? a.x0 : a.x1, k->gy + h - 1, z, m); // la punta
    }
    fill(k, a.x0 - 1, k->gy + 6, a.z0, a.x0 - 1, k->gy + 6, a.z1, M_WOOD_DARK); // el travesaño, de adentro
    for (int z = a.z0 + 1; z <= a.z1; z += 4) put(k, a.x0 - 1, k->gy + 6, z, M_HIDE); // las ataduras
    return true;
}

// Un tramo de muro de piedra seca: mas ancho abajo, las piedras de amarre que asoman, la hilada de
// arriba despareja y el liquen.
static bool drystone(Kit *k) {
    Rect a;
    if (!grid_rect(k, 3, 16, 9, 1, CELL, &a)) return false;
    for (int z = a.z0; z <= a.z1; z++) {
        int h = 7 + (kc(k, 0.4f) ? 1 : 0) - (kc(k, 0.15f) ? 1 : 0); // 1,5 a 2 m
        fill(k, a.x0, 0, z, a.x1, k->gy + 3, z, M_STONE);
        fill(k, a.x1 - 1, k->gy + 4, z, a.x1, k->gy + h - 1, z, M_STONE); // la cara de afuera, a plomo
        if (kc(k, 0.35f)) put(k, a.x1 - (z & 1), k->gy + h - 1, z, M_STONE_LICHEN);
        if ((z - a.z0) % 5 == 2) put(k, a.x1 + 1, k->gy + 3, z, M_STONE_DARK), put(k, a.x0 - 1, k->gy + 3, z, M_STONE_DARK);
    }
    stones(k, k->s);
    return true;
}

// El horno de cocina: una cupula de barro sobre un anillo de piedras, la boca negra al frente con la
// brasa, el humero arriba y, a veces, la leña al lado.
static bool oven(Kit *k) {
    Rect a;
    if (!grid_rect(k, 6, 6, 7, 2, CELL, &a)) return false;
    float cx = (float)(a.x0 + a.x1 + 1) * 0.5f, cz = (float)(a.z0 + a.z1 + 1) * 0.5f;
    int zc = (int)cz;
    vox_cylinder(k->g, cx, cz, 0, k->gy, 3.0f, 3.0f, M_STONE);
    vox_ellipsoid(k->g, cx, (float)k->gy + 1.0f, cz, 2.9f, 4.4f, 2.9f, M_CLAY, true);
    vox_ellipsoid(k->g, cx, (float)k->gy + 1.0f, cz, 1.9f, 3.2f, 1.9f, M_NONE, true);
    for (int y = k->gy + 1; y < k->g->ny; y++) // el barro, a manchas
        for (int z = a.z0; z <= a.z1; z++)
            for (int x = a.x0; x <= a.x1; x++)
                if (vox_get(k->g, x, y, z) == M_CLAY && vox_noise3((float)x * 0.6f, (float)y * 0.6f, (float)z * 0.6f, k->s) > 0.6f) put(k, x, y, z, M_ADOBE);
    hole(k, a.x1 - 1, k->gy + 1, zc - 1, a.x1, k->gy + 2, zc); // la boca
    put(k, a.x1 - 2, k->gy + 1, zc - 1, M_RED), put(k, a.x1 - 2, k->gy + 1, zc, M_SLAG);
    hole(k, (int)cx - 1, k->gy + 4, zc - 1, (int)cx, k->gy + 6, zc); // el humero
    if (kc(k, 0.6f)) woodpile(k, a.x0, a.z1 + 1, a.x0 + 3, a.z1 + 1, 2);
    stones(k, k->s + 1u);
    return true;
}

// El horno de fundicion: la cuba de barro sobre su base de piedra, la boca de arriba con la brasa, la
// tobera y el fuelle de cuero a un costado, la boca de sangrado al frente, la escoria y el carbon.
// El del acero (las grandes), mas alto y forrado de piedra hasta la mitad.
static bool furnace(Kit *k) {
    bool steel = !k->small;
    int D = steel ? 10 : 8, H = steel ? 13 : 9;
    Rect a;
    if (!grid_rect(k, D, D, H + 2, 4, CELL, &a)) return false;
    float cx = (float)(a.x0 + a.x1 + 1) * 0.5f, cz = (float)(a.z0 + a.z1 + 1) * 0.5f, r0 = steel ? 3.6f : 2.8f, r1 = steel ? 2.4f : 1.8f;
    int xc = (int)cx, zc = (int)cz, side = (int)(cz + r0);
    vox_cylinder(k->g, cx, cz, 0, k->gy, r0 + 1.0f, r0 + 1.0f, M_STONE);
    vox_cylinder(k->g, cx, cz, k->gy + 1, k->gy + H, r0, r1, M_CLAY);
    if (steel) vox_cylinder(k->g, cx, cz, k->gy + 1, k->gy + H / 2, r0 + 0.4f, r0 - 0.2f, M_STONE);
    vox_cylinder(k->g, cx, cz, k->gy + H - 3, k->gy + H, r1 - 1.0f, r1 - 1.0f, M_NONE); // la boca de arriba
    put(k, xc, k->gy + H - 3, zc, M_RED), put(k, xc - 1, k->gy + H - 3, zc - 1, M_YELLOW);
    put(k, xc, k->gy + 2, side, M_CLAY), put(k, xc, k->gy + 2, side + 1, M_CLAY); // la tobera
    fill(k, xc - 1, k->gy, side + 2, xc + 1, k->gy + 1, side + 3, M_HIDE);         // el fuelle
    put(k, xc, k->gy + 2, side + 3, M_WOOD);
    int fx = (int)(cx + r0); // la boca de sangrado, con la escoria que corrio
    hole(k, fx - 1, k->gy + 1, zc - 1, fx, k->gy + 1, zc);
    put(k, fx - 2, k->gy + 1, zc - 1, M_RED);
    fill(k, fx + 1, k->gy, zc - 1, fx + ki(k, 1, 2), k->gy, zc, M_SLAG);
    vox_ellipsoid(k->g, cx - (float)ki(k, 0, 2), (float)k->gy, (float)a.z0 - 0.5f, 1.4f + kr(k) * 0.6f, 1.4f, 1.5f, M_SLAG, true); // la escoria
    vox_ellipsoid(k->g, (float)a.x0 - 0.5f, (float)k->gy, cz + (float)ki(k, -2, 2), 1.5f, 1.4f, 1.5f, M_BASALT, true);              // el carbon
    stones(k, k->s);
    return true;
}

// La atalaya: cuatro postes con riostras en cruz, la plataforma de tablas con su baranda a 4 m, la
// escalera de mano al frente y el techo de cuero en punta, con el banderin.
static bool watchtower(Kit *k) {
    Rect a;
    if (!grid_rect(k, 12, 12, 26, 2, CELL, &a)) return false;
    int deck = k->gy + 15, top = deck + 4, zc = (a.z0 + a.z1) / 2;
    for (int i = 0; i < 4; i++) fill(k, i % 2 ? a.x1 : a.x0, 0, i / 2 ? a.z1 : a.z0, i % 2 ? a.x1 : a.x0, top, i / 2 ? a.z1 : a.z0, M_WOOD_DARK);
    for (int s = 0; s < 3; s++) { // las riostras en cruz: los costados y el fondo
        int y1 = deck - 1 - (s == 2 ? ki(k, 0, 2) : 0);
        if (s < 2) {
            int z = s ? a.z1 : a.z0;
            pole(k, a.x0, k->gy, z, a.x1, y1, z, M_WOOD), pole(k, a.x1, k->gy, z, a.x0, y1, z, M_WOOD);
        } else {
            pole(k, a.x0, k->gy, a.z0, a.x0, y1, a.z1, M_WOOD), pole(k, a.x0, k->gy, a.z1, a.x0, y1, a.z0, M_WOOD);
        }
    }
    fill(k, a.x0, deck, a.z0, a.x1, deck, a.z1, M_WOOD);
    walls(k, a, deck + 1, deck + 3, 1, M_WOOD_DARK); // la baranda, de tablas desparejas
    for (int z = a.z0; z <= a.z1; z++)
        for (int x = a.x0; x <= a.x1; x++)
            if (vox_get(k->g, x, deck + 3, z) && kc(k, 0.2f)) put(k, x, deck + 3, z, M_NONE);
    hole(k, a.x1, deck + 1, zc - 1, a.x1, deck + 3, zc + 1);
    ladder(k, a.x1 + 1, zc - 1, k->gy, deck + 2);
    for (int i = 0; i < 4; i++) fill(k, a.x0 - 1 + i, top + 1 + i, a.z0 - 1 + i, a.x1 + 1 - i, top + 1 + i, a.z1 + 1 - i, i == 3 ? M_FELT_DARK : M_HIDE);
    fill(k, (a.x0 + a.x1) / 2, top + 5, zc, (a.x0 + a.x1) / 2, top + 6, zc, M_WOOD_DARK);
    put(k, (a.x0 + a.x1) / 2 + 1, top + 6, zc, kc(k, 0.5f) ? M_RED : M_BLUE); // el banderin
    if (kc(k, 0.5f)) fill(k, a.x0 + 2, deck + 1, a.z0 + 2, a.x0 + 3, deck + 2, a.z0 + 3, M_CLAY); // el cantaro del vigia
    return true;
}

// El corral: estacas cada metro y cuarto con dos sogas (la de arriba, combada), la entrada al frente
// entre dos postes con la tranca en el suelo, el bebedero y un monton de pasto seco.
static bool corral(Kit *k) {
    Rect a;
    if (!grid_rect(k, 40, 40, 7, 1, CELL, &a)) return false;
    int zc = (a.z0 + a.z1 + 1) / 2;
    for (int e = 0; e < 4; e++)
        for (int i = 0; i < 40; i++) {
            int x = e < 2 ? a.x0 + i : e == 2 ? a.x0 : a.x1, z = e < 2 ? (e ? a.z1 : a.z0) : a.z0 + i;
            if (e == 3 && z >= zc - 4 && z < zc + 4) continue; // la entrada
            if (i % 5 == 0 || i == 39) {
                fill(k, x, 0, z, x, k->gy + 4 + (kc(k, 0.3f) ? 1 : 0), z, M_WOOD_DARK);
                continue;
            }
            int sag = i % 5 == 2 || i % 5 == 3;
            put(k, x, k->gy + 2, z, M_HIDE), put(k, x, k->gy + 4 - sag, z, M_HIDE);
        }
    fill(k, a.x1, 0, zc - 5, a.x1, k->gy + 5, zc - 5, M_WOOD_DARK), fill(k, a.x1, 0, zc + 4, a.x1, k->gy + 5, zc + 4, M_WOOD_DARK);
    fill(k, a.x1 + 1, k->gy, zc - 4, a.x1 + 1, k->gy, zc + 3, M_WOOD); // la tranca
    int tz = kc(k, 0.5f) ? a.z1 - 4 : a.z0 + 3, tx = a.x0 + ki(k, 4, 20); // el bebedero
    fill(k, tx, k->gy, tz, tx + 6, k->gy + 1, tz + 1, M_WOOD_DARK);
    fill(k, tx + 1, k->gy + 1, tz, tx + 5, k->gy + 1, tz + 1, M_WATER);
    vox_ellipsoid(k->g, (float)(a.x0 + ki(k, 6, 14)), (float)k->gy, (float)(tz < zc ? a.z1 - 7 : a.z0 + 7), 3.2f, 2.6f, 3.0f, M_THATCH, true); // el pasto seco
    return true;
}

// El taller del herrero: muros de piedra atras y a los costados, abierto adelante, el techo de tablas
// sobre dos postes; adentro, la fragua de piedra con su campana de barro, el fuelle, el yunque sobre
// el tocon, la pila de templar con agua y las herramientas colgadas.
static bool smithy(Kit *k) {
    Rect a;
    int H = 9;
    if (!grid_rect(k, 16, 16, H + 6, 2, CELL, &a)) return false;
    footing(k, a, M_STONE);
    int zc = (a.z0 + a.z1 + 1) / 2, ax = a.x0 + 9 + ki(k, 0, 1);
    fill(k, a.x0, k->gy, a.z0, a.x0 + 1, k->gy + H, a.z1, M_STONE); // atras
    fill(k, a.x0, k->gy, a.z0, a.x1 - 4, k->gy + H - 2, a.z0, M_STONE), fill(k, a.x0, k->gy, a.z1, a.x1 - 4, k->gy + H - 2, a.z1, M_STONE);
    fill(k, a.x1, k->gy, a.z0, a.x1, k->gy + H - 1, a.z0, M_WOOD_DARK), fill(k, a.x1, k->gy, a.z1, a.x1, k->gy + H - 1, a.z1, M_WOOD_DARK);
    for (int x = a.x0 - 1; x <= a.x1 + 1; x++) { // el techo de tablas, en pendiente hacia el frente
        int y = k->gy + H + 1 - (x - a.x0 + 1) / 6;
        fill(k, x, y, a.z0 - 1, x, y, a.z1 + 1, x % 3 ? M_WOOD : M_WOOD_DARK);
    }
    fill(k, a.x0 + 2, k->gy, zc - 2, a.x0 + 4, k->gy + 3, zc + 1, M_STONE); // la fragua
    fill(k, a.x0 + 2, k->gy + 3, zc - 1, a.x0 + 3, k->gy + 3, zc, M_BASALT);
    put(k, a.x0 + 3, k->gy + 4, zc - 1, M_RED);
    fill(k, a.x0 + 2, k->gy + 6, zc - 2, a.x0 + 4, k->gy + 7, zc + 1, M_CLAY); // la campana y la chimenea
    fill(k, a.x0 + 2, k->gy + 8, zc - 1, a.x0 + 3, k->gy + H + 3, zc, M_CLAY);
    fill(k, a.x0 + 2, k->gy, zc + 3, a.x0 + 4, k->gy + 1, zc + 4, M_HIDE); // el fuelle
    fill(k, ax, k->gy, zc - 1, ax + 1, k->gy + 2, zc, M_WOOD_DARK);        // el tocon y el yunque
    fill(k, ax - 1, k->gy + 3, zc - 1, ax + 1, k->gy + 3, zc, M_SLAG);
    put(k, ax + 2, k->gy + 3, zc, M_SLAG);
    fill(k, a.x0 + 6, k->gy, a.z0 + 2, a.x0 + 11, k->gy + 2, a.z0 + 3, M_WOOD_DARK); // la pila de templar
    fill(k, a.x0 + 7, k->gy + 2, a.z0 + 2, a.x0 + 10, k->gy + 2, a.z0 + 3, M_WATER);
    for (int i = 0; i < 4; i++) fill(k, a.x0 + 4 + 2 * i, k->gy + 4, a.z1 - 1, a.x0 + 4 + 2 * i, k->gy + 5 + (i & 1), a.z1 - 1, M_SLAG); // las herramientas
    if (kc(k, 0.6f)) woodpile(k, a.x1 + 1, a.z0 + 1, a.x1 + 2, a.z0 + 5, 2); // el carbon de leña
    stones(k, k->s);
    return true;
}

// La tienda ligera (las chicas): cuero sobre una vara, a dos aguas, cerrada, con la entrada adelante,
// las estacas con sus vientos y el fardo al lado. La choza de fieltro (las grandes): una cupula de
// fieltro sobre varas curvadas, las sogas, el humero y la puerta pintada.
static bool tent(Kit *k) {
    Rect a;
    static const uint8_t PAINT[3] = { M_RED, M_BLUE, M_YELLOW };
    if (k->small) {
        if (!grid_rect(k, 12, 10, 8, 2, CELL, &a)) return false;
        int ridge = hide_roof(k, a, 6, true), zc = (a.z0 + a.z1 + 1) / 2;
        fill(k, a.x0 - 1, ridge, zc - 1, a.x1 + 1, ridge, zc, M_WOOD_DARK); // la vara
        hole(k, a.x1, k->gy, zc - 1, a.x1, k->gy + 2, zc);                  // la entrada
        put(k, a.x1 + 1, k->gy + 3, zc - 2, M_FELT_DARK);                   // la solapa, recogida
        for (int i = 0; i < 4; i++) // las estacas y los vientos
            pole(k, i % 2 ? a.x1 : a.x0, k->gy + 2, i / 2 ? a.z1 : a.z0, i % 2 ? a.x1 + 1 : a.x0 - 1, k->gy, i / 2 ? a.z1 + 2 : a.z0 - 2, M_WOOD_DARK);
        if (kc(k, 0.6f)) fill(k, a.x1 + 1, k->gy, a.z1 - 2, a.x1 + 2, k->gy + 1, a.z1, PAINT[ki(k, 0, 2)]); // el fardo
        return true;
    }
    if (!grid_rect(k, 12, 12, 10, 2, CELL, &a)) return false;
    float cx = (float)(a.x0 + a.x1 + 1) * 0.5f, cz = (float)(a.z0 + a.z1 + 1) * 0.5f;
    int zc = (int)cz;
    vox_cylinder(k->g, cx, cz, 0, k->gy - 1, 6.0f, 6.0f, M_EARTH);
    vox_ellipsoid(k->g, cx, (float)k->gy, cz, 6.0f, 8.0f, 6.0f, M_FELT, true);
    vox_ellipsoid(k->g, cx, (float)k->gy, cz, 5.0f, 7.0f, 5.0f, M_NONE, true);
    for (int y = k->gy; y < k->g->ny; y++) // el fieltro, de dos tonos
        for (int z = a.z0; z <= a.z1; z++)
            for (int x = a.x0; x <= a.x1; x++)
                if (vox_get(k->g, x, y, z) == M_FELT && vox_noise3((float)x * 0.4f, (float)y * 0.4f, (float)z * 0.4f, k->s) > 0.63f) put(k, x, y, z, M_FELT_DARK);
    for (int b = 0; b < 2; b++) { // las sogas
        int y = k->gy + 2 + b * 3;
        float r = 6.0f * sqrtf(1.0f - ((float)(y - k->gy) + 0.5f) * ((float)(y - k->gy) + 0.5f) / 64.0f) + 0.2f;
        for (int i = 0; i < 48; i++) {
            float ang = (float)i / 48.0f * 6.2831853f;
            put(k, (int)floorf(cx + cosf(ang) * r), y, (int)floorf(cz + sinf(ang) * r), M_HIDE);
        }
    }
    hole(k, (int)cx - 1, k->gy + 7, zc - 1, (int)cx, k->gy + 9, zc); // el humero
    hole(k, a.x1 - 1, k->gy, zc - 1, a.x1 + 1, k->gy + 3, zc);       // la puerta
    uint8_t paint = PAINT[ki(k, 0, 2)];
    fill(k, a.x1, k->gy + 4, zc - 2, a.x1, k->gy + 4, zc + 1, paint), fill(k, a.x1, k->gy, zc - 2, a.x1, k->gy + 3, zc - 2, paint);
    fill(k, a.x1, k->gy, zc + 1, a.x1, k->gy + 3, zc + 1, paint);
    return true;
}

// Lo que queda de una obra: los escombros (las chicas: un monton bajo de piedras, barro y vigas rotas)
// o un muro de piedra en ruinas (las grandes), roto desde arriba, con lo caido al pie.
static bool rubble(Kit *k) {
    Rect a;
    if (k->small) {
        if (!grid_rect(k, 12, 12, 5, 1, CELL, &a)) return false;
        float cx = (float)(a.x0 + a.x1 + 1) * 0.5f, cz = (float)(a.z0 + a.z1 + 1) * 0.5f;
        vox_ellipsoid(k->g, cx, (float)k->gy - 1.0f, cz, 5.8f, 4.0f, 5.6f, M_STONE, true);
        for (int y = k->gy; y < k->g->ny; y++)
            for (int z = a.z0; z <= a.z1; z++)
                for (int x = a.x0; x <= a.x1; x++) {
                    if (!vox_get(k->g, x, y, z)) continue;
                    float n = vox_noise3((float)x * 0.5f, (float)y * 0.5f, (float)z * 0.5f, k->s);
                    if (n < 0.38f + 0.06f * (float)(y - k->gy)) put(k, x, y, z, M_NONE);
                    else if (n > 0.72f) put(k, x, y, z, n > 0.8f ? M_ADOBE : M_EARTH);
                }
        for (int i = 0; i < 3; i++) { // las vigas rotas
            int x0 = a.x0 + ki(k, 1, 10), z0 = a.z0 + ki(k, 1, 10);
            pole(k, x0, k->gy, z0, x0 + ki(k, -4, 4), k->gy + ki(k, 1, 3), z0 + ki(k, -4, 4), i ? M_WOOD_DARK : M_WOOD);
        }
        stones(k, k->s);
        return true;
    }
    if (!grid_rect(k, 4, 24, 13, 2, CELL, &a)) return false;
    int keep = a.z0 + ki(k, 2, 18); // un trozo que quedo entero (una esquina, un pilar)
    for (int z = a.z0; z <= a.z1; z++) {
        float n = vox_noise3(0.5f, 0.5f, (float)z * 0.21f, k->s);
        int h = z >= keep && z < keep + 4 ? 11 + (z & 1) : 1 + (int)(n * n * 16.0f);
        h = h > 12 ? 12 : h;
        fill(k, a.x0, 0, z, a.x1, k->gy + h - 1, z, M_STONE);
        if (kc(k, 0.5f)) put(k, a.x0 + ki(k, 0, 3), k->gy + h - 1, z, M_STONE_LICHEN);
    }
    for (int i = 0; i < 5; i++) { // lo caido, al pie
        int z = a.z0 + ki(k, 0, 23), x = kc(k, 0.5f) ? a.x0 - 1 : a.x1 + 1;
        vox_ellipsoid(k->g, (float)x + 0.5f, (float)k->gy, (float)z + 0.5f, 1.4f, 1.6f, 2.0f + kr(k) * 1.5f, M_STONE, true);
    }
    stones(k, k->s);
    return true;
}

// Lo que deja el fuego: las cenizas de una obra (las chicas: vigas carbonizadas cruzadas sobre la
// ceniza y alguna brasa) o una yurta quemada (las grandes: el anillo del enrejado, negro y roto, las
// varas del techo caidas hacia adentro, la corona en el suelo y la estufa).
static bool ashes(Kit *k) {
    Rect a;
    int D = k->small ? 12 : 18;
    if (!grid_rect(k, D, D, k->small ? 3 : 8, 1, CELL, &a)) return false;
    float cx = (float)(a.x0 + a.x1 + 1) * 0.5f, cz = (float)(a.z0 + a.z1 + 1) * 0.5f, r = (float)D * 0.5f - 0.5f;
    vox_cylinder(k->g, cx, cz, k->gy - 1, k->gy, r, r - 0.5f, M_BASALT_L);
    for (int z = a.z0; z <= a.z1; z++) // la ceniza, a manchas
        for (int x = a.x0; x <= a.x1; x++)
            if (vox_get(k->g, x, k->gy, z) && vox_noise3((float)x * 0.5f, 0.5f, (float)z * 0.5f, k->s) > 0.6f) put(k, x, k->gy, z, M_BASALT);
    if (k->small) {
        for (int i = 0; i < 4; i++) { // las vigas, cruzadas
            float ang = kr(k) * 3.14159265f, l = 3.5f + kr(k) * 1.5f;
            pole(k, (int)(cx - cosf(ang) * l), k->gy + 1, (int)(cz - sinf(ang) * l), (int)(cx + cosf(ang) * l), k->gy + 1 + (i == 3), (int)(cz + sinf(ang) * l), i % 2 ? M_SLAG : M_BASALT);
        }
    } else {
        float rr = r - 0.5f;
        for (int i = 0; i < 72; i++) { // el enrejado: lo que no se quemo del todo
            float ang = (float)i / 72.0f * 6.2831853f;
            int h = (int)(vox_noise3((float)i * 0.35f, 1.5f, 0.5f, k->s) * 9.0f) - 2;
            if (h > 0) fill(k, (int)floorf(cx + cosf(ang) * rr), k->gy + 1, (int)floorf(cz + sinf(ang) * rr), (int)floorf(cx + cosf(ang) * rr), k->gy + h, (int)floorf(cz + sinf(ang) * rr), i % 2 ? M_SLAG : M_BASALT);
        }
        for (int i = 0; i < 9; i++) { // las varas del techo, caidas hacia adentro
            float ang = ((float)i + kr(k) * 0.6f) / 9.0f * 6.2831853f, l = rr - 0.5f - kr(k) * 2.0f;
            pole(k, (int)floorf(cx + cosf(ang) * l), k->gy + 1 + ki(k, 0, 3), (int)floorf(cz + sinf(ang) * l), (int)floorf(cx + cosf(ang) * 2.0f), k->gy + 1, (int)floorf(cz + sinf(ang) * 2.0f), M_SLAG);
        }
        for (int i = 0; i < 24; i++) { // la corona, en el suelo
            float ang = (float)i / 24.0f * 6.2831853f;
            put(k, (int)floorf(cx + 2.4f + cosf(ang) * 2.0f), k->gy + 1, (int)floorf(cz - 1.5f + sinf(ang) * 2.0f), M_SLAG);
        }
        fill(k, (int)cx - 1, k->gy + 1, (int)cz - 1, (int)cx, k->gy + 2, (int)cz, M_STONE_DARK); // la estufa
    }
    for (int i = 0; i < 3; i++) put(k, a.x0 + ki(k, 2, D - 3), k->gy + 1, a.z0 + ki(k, 2, D - 3), M_RED); // las brasas
    return true;
}

// ---------------------------------------------------------------- el adentro
static bool in_vest(const Kit *k, int x, int y, int z) {
    for (int i = 0; i < k->nvest; i++) {
        const Box *b = &k->vest[i];
        if (x >= b->x0 && x <= b->x1 && y >= b->y0 && y <= b->y1 && z >= b->z0 && z <= b->z1) return true;
    }
    return false;
}

// El techo de la columna (x, z) sobre la capa y: la primera celda llena (ny: al cielo).
static int ceiling_at(const VoxGrid *g, int x, int y, int z) {
    int t = y + 1;
    while (t < g->ny && !vox_get(g, x, t, z)) t++;
    return t;
}

// Llena de sombra la columna (x, z) desde y0 (o, con down, desde el piso de abajo) hasta el techo.
static void fill_column(Kit *k, int x, int y0, int z, bool down) {
    int top = ceiling_at(k->g, x, y0, z), bot = y0 - 1;
    if (top >= k->g->ny) return;
    while (down && bot >= 0 && !vox_get(k->g, x, bot, z)) bot--;
    for (int y = bot + 1; y < top; y++)
        if (!vox_get(k->g, x, y, z)) put(k, x, y, z, M_SHADOW);
}

// Lo de adentro no se ve (solo por las puertas y las ventanas): se rellena de sombra, menos los
// vestibulos, y la malla pierde la mitad de sus caras. A la altura del umbral mas uno, con las
// puertas cerradas (los vestibulos, macizos), lo que no se alcanza desde el borde de la rejilla
// son los cuartos y los patios: un cuarto (casi todo con techo) se llena columna por columna, del
// piso al techo; un patio (casi todo al cielo), no. Sobre los vestibulos, tambien, hasta el techo.
static void seal(Kit *k) {
    VoxGrid *g = k->g;
    int nx = g->nx, nz = g->nz, y = k->info->door_y + 1, sp = 0;
    uint8_t *mark = calloc((size_t)nx * (size_t)nz, 1); // 1: afuera; 2: de un cuarto o un patio
    int *stack = malloc(sizeof(int) * (size_t)nx * (size_t)nz), *room = malloc(sizeof(int) * (size_t)nx * (size_t)nz);
    if (!mark || !stack || !room) {
        free(mark), free(stack), free(room);
        return;
    }
    static const int D[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
#define SEAL_OPEN(x, z) (!vox_get(g, x, y, z) && !in_vest(k, x, y, z))
    for (int z = 0; z < nz; z++)
        for (int x = 0; x < nx; x++)
            if ((x == 0 || z == 0 || x == nx - 1 || z == nz - 1) && SEAL_OPEN(x, z)) mark[x + nx * z] = 1, stack[sp++] = x + nx * z;
    while (sp > 0) {
        int c = stack[--sp], x = c % nx, z = c / nx;
        for (int i = 0; i < 4; i++) {
            int a = x + D[i][0], b = z + D[i][1];
            if (a < 0 || b < 0 || a >= nx || b >= nz || mark[a + nx * b] || !SEAL_OPEN(a, b)) continue;
            mark[a + nx * b] = 1, stack[sp++] = a + nx * b;
        }
    }
    for (int c0 = 0; c0 < nx * nz; c0++) {
        if (mark[c0] || !SEAL_OPEN(c0 % nx, c0 / nx)) continue;
        int n = 0, covered = 0;
        mark[c0] = 2, stack[sp++] = c0;
        while (sp > 0) {
            int c = stack[--sp], x = c % nx, z = c / nx;
            room[n++] = c;
            covered += ceiling_at(g, x, y, z) < g->ny;
            for (int i = 0; i < 4; i++) {
                int a = x + D[i][0], b = z + D[i][1];
                if (a < 0 || b < 0 || a >= nx || b >= nz || mark[a + nx * b] || !SEAL_OPEN(a, b)) continue;
                mark[a + nx * b] = 2, stack[sp++] = a + nx * b;
            }
        }
        if (covered * 2 < n) continue; // un patio
        for (int i = 0; i < n; i++) fill_column(k, room[i] % nx, y, room[i] / nx, true);
    }
#undef SEAL_OPEN
    for (int i = 0; i < k->nvest; i++) {
        const Box *b = &k->vest[i];
        for (int z = b->z0; z <= b->z1; z++)
            for (int x = b->x0; x <= b->x1; x++)
                if (!vox_get(g, x, b->y1, z) && !vox_get(g, x, b->y1 + 1, z)) fill_column(k, x, b->y1 + 1, z, false);
    }
    free(mark), free(stack), free(room);
}

// ---------------------------------------------------------------- la interfaz
bool bk_available(BkType t, Region r) {
    if ((unsigned)t >= BK_KINDS || (unsigned)r >= REGION_COUNT) return false;
    if (t == BK_TEMPLE) return r == REGION_DESERT || r == REGION_HIGHLAND;
    if (t == BK_WAGON) return r == REGION_STEPPE;
    if (t == BK_RUIN_CITY) return r == REGION_DESERT;
    if (t >= BK_SHELTER && t <= BK_ASHES) return r == REGION_STEPPE; // las obras de la tribu
    return true;
}

uint32_t bk_variant_seed(BkType t, Region r, int variant) {
    uint32_t h = mix(0xB1D0u + (uint32_t)t * 131u + (uint32_t)r * 17u + (uint32_t)variant * 7919u);
    return t == BK_GATE ? (h & ~1u) | (uint32_t)(variant & 1) : h; // la puerta: la paridad de la variante (de que lado cierra)
}

bool bk_build(BkType t, Region r, uint32_t seed, bool small, VoxGrid *g, BkInfo *info) {
    BkInfo dummy;
    if (!info) info = &dummy;
    memset(info, 0, sizeof(*info));
    info->door_x = -1;
    memset(g, 0, sizeof(*g));
    if (!bk_available(t, r)) return false;
    Kit k = { .g = g, .s = mix(seed ^ (uint32_t)t * 0x9E3779B9u ^ (uint32_t)r * 0x85EBCA6Bu) | 1u, .r = r, .info = info, .small = small };
    bool ok = false;
    switch (t) {
    case BK_HOUSE: ok = house(&k); break;
    case BK_PALACE: ok = palace(&k); break;
    case BK_FORGE: ok = forge(&k); break;
    case BK_BARRACKS: ok = barracks(&k); break;
    case BK_CARPENTRY: ok = carpentry(&k); break;
    case BK_GRANARY: ok = granary(&k); break;
    case BK_TEMPLE: ok = temple(&k); break;
    case BK_WAGON: ok = wagon(&k); break;
    case BK_WALL: ok = wall_piece(&k); break;
    case BK_TOWER: ok = wall_tower_piece(&k); break;
    case BK_GATE: ok = gate_piece(&k, (int)(seed & 1u)); break;
    case BK_RUIN_FORT: ok = ruin_fort(&k); break;
    case BK_RUIN_CITY: ok = ruin_city(&k); break;
    case BK_SHELTER: ok = shelter(&k); break;
    case BK_PALISADE: ok = palisade(&k); break;
    case BK_DRYSTONE: ok = drystone(&k); break;
    case BK_OVEN: ok = oven(&k); break;
    case BK_FURNACE: ok = furnace(&k); break;
    case BK_WATCHTOWER: ok = watchtower(&k); break;
    case BK_CORRAL: ok = corral(&k); break;
    case BK_SMITHY: ok = smithy(&k); break;
    case BK_TENT: ok = tent(&k); break;
    case BK_RUBBLE: ok = rubble(&k); break;
    case BK_ASHES: ok = ashes(&k); break;
    default: break;
    }
    if (!ok) {
        vox_free(g);
        return false;
    }
    if (info->enclosed && info->door_x >= 0) seal(&k);
    // Lo que ocupa sobre el suelo.
    int lo[3] = { g->nx, g->ny, g->nz }, hi[3] = { -1, -1, -1 };
    for (int y = BK_FOUND; y < g->ny; y++)
        for (int z = 0; z < g->nz; z++)
            for (int x = 0; x < g->nx; x++)
                if (vox_get(g, x, y, z)) {
                    int p[3] = { x, y, z };
                    for (int i = 0; i < 3; i++) lo[i] = p[i] < lo[i] ? p[i] : lo[i], hi[i] = p[i] > hi[i] ? p[i] : hi[i];
                }
    float sz = g->size, hx = (float)g->nx * sz * 0.5f, hz = (float)g->nz * sz * 0.5f;
    info->l = (float)(hi[0] - lo[0] + 1) * sz;
    info->h = (float)(hi[1] - BK_FOUND + 1) * sz;
    info->w = (float)(hi[2] - lo[2] + 1) * sz;
    info->found = (float)BK_FOUND * sz;
    info->front = (float)(hi[0] + 1) * sz - hx;
    if (info->door_x >= 0) {
        info->door_along = (float)(info->door_x + 1) * sz - hx;
        info->door_across = (float)(info->door_z0 + info->door_z1 + 1) * 0.5f * sz - hz;
        info->door_up = (float)(info->door_y - BK_FOUND) * sz;
    }
    return true;
}

const char *bk_name(BkType t, Region r) {
    switch (t) {
    case BK_HOUSE:
        switch (r) {
        case REGION_FOREST: return N_("casa de postes con techo de paja");
        case REGION_HIGHLAND: return N_("casa de piedra y adobe");
        case REGION_FJORD: return N_("casa larga de turba");
        case REGION_DESERT: return N_("casa de adobe");
        default: return N_("yurta");
        }
    case BK_PALACE:
        switch (r) {
        case REGION_STEPPE: return N_("la orda del kan");
        case REGION_FOREST:
        case REGION_FJORD: return N_("la sala larga");
        default: return N_("la sala de columnas");
        }
    case BK_FORGE: return N_("la fundición: el horno de cuba");
    case BK_BARRACKS: return N_("el cuartel");
    case BK_CARPENTRY: return N_("la carpintería");
    case BK_GRANARY: return r == REGION_DESERT ? N_("el granero de adobe y sus silos") : N_("el granero sobre postes");
    case BK_TEMPLE: return N_("el templo del fuego");
    case BK_WAGON: return N_("la carreta-casa");
    case BK_WALL: return N_("la muralla");
    case BK_TOWER: return N_("la torre de la muralla");
    case BK_GATE: return N_("la puerta acodada");
    case BK_RUIN_FORT: return N_("la fortaleza derruida");
    case BK_RUIN_CITY: return N_("la ciudad enterrada");
    case BK_SHELTER: return N_("el refugio de ramas y pieles");
    case BK_PALISADE: return N_("la empalizada");
    case BK_DRYSTONE: return N_("el muro de piedra seca");
    case BK_OVEN: return N_("el horno de cocina");
    case BK_FURNACE: return N_("el horno de fundición: el del bronce y el del acero");
    case BK_WATCHTOWER: return N_("la atalaya");
    case BK_CORRAL: return N_("el corral");
    case BK_SMITHY: return N_("el taller del herrero");
    case BK_TENT: return N_("la tienda ligera y la choza de fieltro");
    case BK_RUBBLE: return N_("los escombros y el muro en ruinas");
    case BK_ASHES: return N_("las cenizas y la yurta quemada");
    default: return "?";
    }
}

// Los ids del inventario de las obras y las viviendas de la tribu, y lo que queda de ellas.
typedef struct {
    const char *id;
    BkType t;
    Region r;
    int variant;
} ItemKit;

static const ItemKit ITEM_KITS[] = {
    { "estructura.campamento.refugio", BK_SHELTER, REGION_STEPPE, 0 },
    { "estructura.campamento.empalizada", BK_PALISADE, REGION_STEPPE, 0 },
    { "estructura.defensa.muro_piedra", BK_DRYSTONE, REGION_STEPPE, 0 },
    { "estructura.campamento.horno_cocina", BK_OVEN, REGION_STEPPE, 0 },
    { "estructura.campamento.horno_bronce", BK_FURNACE, REGION_STEPPE, 0 },
    { "estructura.campamento.horno_acero", BK_FURNACE, REGION_STEPPE, 2 },
    { "estructura.campamento.atalaya", BK_WATCHTOWER, REGION_STEPPE, 0 },
    { "estructura.campamento.corral", BK_CORRAL, REGION_STEPPE, 0 },
    { WORKSHOP_ID, BK_SMITHY, REGION_STEPPE, 0 },
    { "estructura.vivienda.tienda_ligera", BK_TENT, REGION_STEPPE, 0 },
    { "estructura.vivienda.choza_fieltro", BK_TENT, REGION_STEPPE, 2 },
    { "estructura.vivienda.yurta_comun", BK_HOUSE, REGION_STEPPE, 0 },
    { "estructura.vivienda.yurta_jefe", BK_HOUSE, REGION_STEPPE, 2 },
    { "estructura.vivienda.casa_adobe", BK_HOUSE, REGION_DESERT, 0 },
    { "estructura.vivienda.cabana_troncos", BK_HOUSE, REGION_FOREST, 0 },
    { "estructura.ruina.escombros", BK_RUBBLE, REGION_STEPPE, 0 },
    { "estructura.ruina.muro", BK_RUBBLE, REGION_STEPPE, 2 },
    { "estructura.ruina.cenizas", BK_ASHES, REGION_STEPPE, 0 },
    { "estructura.ruina.yurta_quemada", BK_ASHES, REGION_STEPPE, 2 },
};

bool bk_for_item(const char *id, BkType *t, Region *r, int *variant) {
    for (size_t i = 0; id && i < sizeof(ITEM_KITS) / sizeof(ITEM_KITS[0]); i++) {
        if (strcmp(ITEM_KITS[i].id, id)) continue;
        if (t) *t = ITEM_KITS[i].t;
        if (r) *r = ITEM_KITS[i].r;
        if (variant) *variant = ITEM_KITS[i].variant;
        return true;
    }
    return false;
}
