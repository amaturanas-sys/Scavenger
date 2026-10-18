#include "sim/buildkit.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

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
    return true;
}

uint32_t bk_variant_seed(BkType t, Region r, int variant) {
    return mix(0xB1D0u + (uint32_t)t * 131u + (uint32_t)r * 17u + (uint32_t)variant * 7919u);
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
    default: return "?";
    }
}
