#include "hydro.h"

#include <math.h>
#include <stddef.h>

#define N_MAX (HYDRO_SIDE * HYDRO_SIDE)
#define ITEMS_MAX 4096

static HydroGrid g_grid;
static int g_n;
static float g_ground[N_MAX], g_surface[N_MAX], g_flow[N_MAX], g_rain[N_MAX], g_melt[N_MAX], g_meltflow[N_MAX];
static int g_down[N_MAX], g_order[N_MAX], g_basin[N_MAX], g_onpath[N_MAX], g_queue[N_MAX];
static unsigned char g_kind[N_MAX], g_seen[N_MAX];

// Las ocho vecinas: primero las de los lados, despues las diagonales.
static const int DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
static const int DZ[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

static int neighbor(int c, int k) {
    int s = g_grid.side, x = c % s + DX[k], z = c / s + DZ[k];
    return x < 0 || z < 0 || x >= s || z >= s ? -1 : z * s + x;
}

// ------------------------------------------------------------------ la cola de prioridad (un monticulo)
// La de mas abajo primero; a igual altura, la que entro antes: asi el resultado no depende del orden en
// que la maquina compara (el mismo mundo en cada maquina).
static float h_key[N_MAX];
static int h_cell[N_MAX], h_seq[N_MAX], h_n, h_next;

static int heap_before(int a, int b) { return h_key[a] < h_key[b] || (h_key[a] == h_key[b] && h_seq[a] < h_seq[b]); }

static void heap_swap(int a, int b) {
    float k = h_key[a];
    int c = h_cell[a], q = h_seq[a];
    h_key[a] = h_key[b], h_cell[a] = h_cell[b], h_seq[a] = h_seq[b];
    h_key[b] = k, h_cell[b] = c, h_seq[b] = q;
}

static void heap_push(int c, float key) {
    int i = h_n++;
    h_key[i] = key, h_cell[i] = c, h_seq[i] = h_next++;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (!heap_before(i, p)) break;
        heap_swap(i, p);
        i = p;
    }
}

static int heap_pop(void) {
    int top = h_cell[0];
    h_n--;
    if (h_n > 0) {
        h_key[0] = h_key[h_n], h_cell[0] = h_cell[h_n], h_seq[0] = h_seq[h_n];
        int i = 0;
        for (;;) {
            int l = 2 * i + 1, r = l + 1, m = i;
            if (l < h_n && heap_before(l, m)) m = l;
            if (r < h_n && heap_before(r, m)) m = r;
            if (m == i) break;
            heap_swap(i, m);
            i = m;
        }
    }
    return top;
}

// ------------------------------------------------------------------ 1 y 2: llenar, ordenar y sumar
bool hydro_run(const HydroGrid *g, HydroCellFn cell, const void *ud) {
    if (!g || !cell || g->side < 2 || g->side > HYDRO_SIDE || g->cell <= 0.0f) return false;
    g_grid = *g;
    g_n = g->side * g->side;
    int s = g->side;
    for (int c = 0; c < g_n; c++) {
        float x, z, h = 0.0f, rain = 0.0f, melt = 0.0f;
        hydro_center(c, &x, &z);
        HydroCell k = cell(ud, x, z, &h, &rain, &melt);
        int ix = c % s, iz = c / s;
        if ((unsigned)k >= HCELL_KINDS) k = HCELL_OUT;
        if (k == HCELL_LAND && (ix == 0 || iz == 0 || ix == s - 1 || iz == s - 1)) k = HCELL_OUT;
        g_kind[c] = (unsigned char)k;
        g_ground[c] = h;
        g_rain[c] = k == HCELL_LAND && rain > 0.0f ? rain : 0.0f;
        g_melt[c] = k == HCELL_LAND && melt > 0.0f ? melt : 0.0f;
        g_down[c] = -1, g_flow[c] = g_meltflow[c] = 0.0f, g_seen[c] = 0, g_basin[c] = -1;
    }
    // Priority-Flood: las salidas primero; despues, de lo mas bajo a lo mas alto. La celda que alcanza a
    // otra es adonde corre el agua de esa otra; en una depresion, el agua sube hasta el desborde.
    h_n = h_next = 0;
    for (int c = 0; c < g_n; c++)
        if (g_kind[c] != HCELL_LAND) {
            g_surface[c] = g_ground[c];
            g_seen[c] = 1;
            heap_push(c, g_surface[c]);
        }
    int norder = 0;
    while (h_n > 0) {
        int c = heap_pop();
        g_order[norder++] = c;
        for (int k = 0; k < 8; k++) {
            int nb = neighbor(c, k);
            if (nb < 0 || g_seen[nb]) continue;
            g_seen[nb] = 1;
            g_surface[nb] = fmaxf(g_ground[nb], g_surface[c] + HYDRO_EPS);
            g_down[nb] = c;
            heap_push(nb, g_surface[nb]);
        }
    }
    // El agua: cada celda suma la suya y la pasa abajo. Al reves del orden del llenado, cada una llega
    // despues de todas las que le vierten.
    for (int i = norder - 1; i >= 0; i--) {
        int c = g_order[i];
        g_flow[c] += g_rain[c] + g_melt[c];
        g_meltflow[c] += g_melt[c];
        if (g_down[c] >= 0) g_flow[g_down[c]] += g_flow[c], g_meltflow[g_down[c]] += g_meltflow[c];
    }
    return true;
}

int hydro_cells(void) { return g_n; }

void hydro_center(int c, float *x, float *z) {
    int s = g_grid.side > 0 ? g_grid.side : 1;
    *x = g_grid.x0 + ((float)(c % s) + 0.5f) * g_grid.cell;
    *z = g_grid.z0 + ((float)(c / s) + 0.5f) * g_grid.cell;
}

int hydro_cell_at(float x, float z) {
    if (g_grid.side <= 0) return -1;
    int ix = (int)floorf((x - g_grid.x0) / g_grid.cell), iz = (int)floorf((z - g_grid.z0) / g_grid.cell);
    return ix < 0 || iz < 0 || ix >= g_grid.side || iz >= g_grid.side ? -1 : iz * g_grid.side + ix;
}

HydroCell hydro_kind(int c) { return c >= 0 && c < g_n ? (HydroCell)g_kind[c] : HCELL_OUT; }
float hydro_ground(int c) { return c >= 0 && c < g_n ? g_ground[c] : 0.0f; }
float hydro_surface(int c) { return c >= 0 && c < g_n ? g_surface[c] : 0.0f; }
int hydro_down(int c) { return c >= 0 && c < g_n ? g_down[c] : -1; }
float hydro_flow(int c) { return c >= 0 && c < g_n ? g_flow[c] : 0.0f; }
float hydro_melt(int c) { return c >= 0 && c < g_n ? g_meltflow[c] : 0.0f; }
int hydro_basin_of(int c) { return c >= 0 && c < g_n ? g_basin[c] : -1; }

// ------------------------------------------------------------------ las depresiones
static int flooded(int c) { return g_kind[c] == HCELL_LAND && g_surface[c] - g_ground[c] > 0.05f; }

// Una depresion: las celdas cubiertas, vecinas entre si, cuyo desborde (aguas abajo, la primera celda
// descubierta) esta a la misma altura (HYDRO_SAME_LEVEL). Dos pozos vecinos con salidas a distinta altura son
// dos lagos; un pozo que el llenado alcanzo por dos puntos de su borde casi a la misma altura es uno solo, que
// desborda por el mas bajo.
#define HYDRO_SAME_LEVEL 0.25f
int hydro_basins(float min_depth, int min_cells, HydroBasin *out, int max) {
    static int exitc[N_MAX], label[N_MAX];
    for (int c = 0; c < g_n; c++) g_basin[c] = exitc[c] = label[c] = -1;
    for (int c = 0; c < g_n; c++) { // el desborde de cada celda cubierta (de una vez para todo el camino)
        if (!flooded(c) || exitc[c] >= 0) continue;
        int len = 0, x = c;
        while (x >= 0 && flooded(x) && exitc[x] < 0 && len < g_n) g_queue[len++] = x, x = g_down[x];
        int e = x < 0 ? -1 : flooded(x) ? exitc[x] : x;
        for (int i = 0; i < len; i++) exitc[g_queue[i]] = e;
    }
    int found = 0, id = 0;
    for (int c0 = 0; c0 < g_n && id < HYDRO_BASINS; c0++) {
        if (label[c0] >= 0 || !flooded(c0) || exitc[c0] < 0) continue;
        float lev0 = g_surface[exitc[c0]], depth = 0.0f;
        int head = 0, tail = 0, spill = exitc[c0];
        double sx = 0.0, sz = 0.0, sxx = 0.0, szz = 0.0;
        g_queue[tail++] = c0;
        label[c0] = id;
        while (head < tail) {
            int c = g_queue[head++], e = exitc[c];
            float x, z, d = g_surface[c] - g_ground[c];
            hydro_center(c, &x, &z);
            sx += x, sz += z, sxx += (double)x * x, szz += (double)z * z;
            if (d > depth) depth = d;
            if (g_surface[e] < g_surface[spill] || (g_surface[e] == g_surface[spill] && e < spill)) spill = e;
            for (int k = 0; k < 8; k++) {
                int nb = neighbor(c, k);
                if (nb >= 0 && label[nb] < 0 && flooded(nb) && exitc[nb] >= 0 && fabsf(g_surface[exitc[nb]] - lev0) <= HYDRO_SAME_LEVEL)
                    label[nb] = id, g_queue[tail++] = nb;
            }
        }
        if (depth >= min_depth && tail >= min_cells && found < max) {
            HydroBasin *b = &out[found++];
            b->cells = tail, b->depth = depth, b->spill = spill, b->id = id;
            b->x = (float)(sx / tail), b->z = (float)(sz / tail);
            double var = (sxx + szz) / tail - (double)b->x * b->x - (double)b->z * b->z;
            b->spread = (float)sqrt(var > 0.0 ? var : 0.0);
            b->level = g_surface[spill];
            for (int i = 0; i < tail; i++) g_basin[g_queue[i]] = id;
        }
        id++;
    }
    return found;
}

// ------------------------------------------------------------------ los rios
typedef struct {
    int mouth, tail; // la ultima celda del rio y la de donde termina (otro rio, el lago, la salida)
    HydroEnd end;
    int into;
    float flow;
} Item;
static Item g_items[ITEMS_MAX];
static int g_nitems;
static int g_lakemap[HYDRO_BASINS];
static unsigned char g_lake_seen[HYDRO_BASINS], g_lake_out[HYDRO_BASINS];

static int lake_of(int c) {
    int b = g_basin[c];
    return b >= 0 && b < HYDRO_BASINS ? g_lakemap[b] : -1;
}

static void push_item(int mouth, int tail, HydroEnd end, int into, float flow) {
    if (g_nitems >= ITEMS_MAX) return;
    g_items[g_nitems++] = (Item){ mouth, tail, end, into, flow };
}

// Los que le entran al lago L: las celdas de afuera que vierten adentro, con bastante agua.
static void push_inflows(int lake, float min_flow) {
    for (int c = 0; c < g_n; c++) {
        int d = g_down[c];
        if (g_kind[c] == HCELL_LAND && d >= 0 && lake_of(c) != lake && lake_of(d) == lake && g_flow[c] >= min_flow)
            push_item(c, d, HEND_LAKE, lake, g_flow[c]);
    }
}

static void store(HydroPath *p, const int *up, int n, int tail, HydroEnd end, int into, int from_lake, float flow, int index) {
    p->n = 0;
    for (int i = n - 1; i >= 0 && p->n < HYDRO_PATH_MAX; i--) { // de la cabecera a la boca
        p->cells[p->n++] = up[i];
        g_onpath[up[i]] = index;
    }
    if (tail >= 0 && p->n < HYDRO_PATH_MAX) p->cells[p->n++] = tail;
    p->end = end, p->into = into, p->from_lake = from_lake, p->flow = flow;
}

int hydro_paths(float min_flow, const HydroBasin *lakes, int nlakes, HydroPath *out, int max) {
    static int up[HYDRO_PATH_MAX];
    for (int b = 0; b < HYDRO_BASINS; b++) g_lakemap[b] = -1, g_lake_seen[b] = g_lake_out[b] = 0;
    for (int i = 0; i < nlakes && i < HYDRO_BASINS; i++)
        if (lakes[i].id >= 0 && lakes[i].id < HYDRO_BASINS) g_lakemap[lakes[i].id] = i;
    for (int c = 0; c < g_n; c++) g_onpath[c] = -1;
    g_nitems = 0;
    // Las bocas: tierra que vierte en una salida.
    for (int c = 0; c < g_n; c++) {
        int d = g_down[c];
        if (g_kind[c] == HCELL_LAND && d >= 0 && g_kind[d] != HCELL_LAND && g_flow[c] >= min_flow && lake_of(c) < 0)
            push_item(c, d, HEND_OUTLET, -1, g_flow[c]);
    }
    int np = 0;
    while (g_nitems > 0 && np < max) {
        int best = 0;
        for (int i = 1; i < g_nitems; i++)
            if (g_items[i].flow > g_items[best].flow) best = i;
        Item it = g_items[best];
        g_items[best] = g_items[--g_nitems];
        if (g_onpath[it.mouth] >= 0) continue;
        // Aguas arriba por el brazo con mas agua; los otros brazos son afluentes que terminan aca. Si el
        // agua viene de un lago, el rio nace en su desborde.
        int n = 0, cur = it.mouth, from_lake = -1;
        up[n++] = cur;
        for (;;) {
            int next = -1, cand[8], nc = 0;
            for (int k = 0; k < 8; k++) {
                int u = neighbor(cur, k);
                if (u < 0 || g_down[u] != cur || g_kind[u] != HCELL_LAND || g_onpath[u] >= 0) continue;
                int lake = lake_of(u);
                if (lake >= 0) {
                    // Un cauce de un pozo del lago a otro pozo del mismo lago no es su salida: es un afluente.
                    if (from_lake < 0 && !(it.end == HEND_LAKE && it.into == lake)) from_lake = lake;
                    if (!g_lake_seen[lake]) g_lake_seen[lake] = 1, push_inflows(lake, min_flow);
                    continue;
                }
                if (g_flow[u] >= min_flow) cand[nc++] = u;
            }
            if (from_lake < 0)
                for (int i = 0; i < nc; i++)
                    if (next < 0 || g_flow[cand[i]] > g_flow[next]) next = cand[i];
            for (int i = 0; i < nc; i++)
                if (cand[i] != next) push_item(cand[i], cur, HEND_RIVER, np, g_flow[cand[i]]);
            if (next < 0 || n >= HYDRO_PATH_MAX - 1) break;
            up[n++] = cur = next;
        }
        if (from_lake >= 0) g_lake_out[from_lake] = 1;
        store(&out[np], up, n, it.tail, it.end, it.into, from_lake, it.flow, np);
        np++;
    }
    // Los lagos que no dieron un rio grande: igual desbordan, y su arroyo baja hasta el primer rio, otro
    // lago o una salida.
    for (int lake = 0; lake < nlakes && np < max; lake++) {
        if (g_lake_out[lake] || lakes[lake].spill < 0) continue;
        int n = 0, c = lakes[lake].spill, tail = -1, into = -1;
        HydroEnd end = HEND_OUTLET;
        while (c >= 0 && n < HYDRO_PATH_MAX - 1) {
            if (g_onpath[c] >= 0) { tail = c, end = HEND_RIVER, into = g_onpath[c]; break; }
            if (g_kind[c] != HCELL_LAND) { tail = c; break; }
            if (lake_of(c) >= 0 && lake_of(c) != lake) { tail = c, end = HEND_LAKE, into = lake_of(c); break; }
            up[n++] = c;
            c = g_down[c];
        }
        if (n == 0) continue;
        // up va de la cabecera a la boca: store lo da vuelta, asi que se le pasa al reves.
        for (int i = 0, j = n - 1; i < j; i++, j--) {
            int t = up[i];
            up[i] = up[j], up[j] = t;
        }
        g_lake_out[lake] = 1;
        store(&out[np], up, n, tail, end, into, lake, g_flow[lakes[lake].spill], np);
        np++;
    }
    return np;
}
