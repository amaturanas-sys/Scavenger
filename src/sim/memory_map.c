#include "sim/memory_map.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sim/clock.h"

MemoryParams memmap_default_params(void) {
    return (MemoryParams){
        .sight_radius = 22.0f,
        .gain = 0.35f,
        .forget_days = 3.0f, // una zona vista de pasada se borra en una o dos semanas de juego
        .base_cap = 0.3f,
    };
}

void memmap_init(MemoryMap *m) {
    memset(m, 0, sizeof(*m));
    m->params = memmap_default_params();
}

void memmap_free(MemoryMap *m) {
    for (int i = 0; i < m->capacity; i++) free(m->slots[i]);
    free(m->slots);
    m->slots = NULL;
    m->capacity = m->page_count = 0;
}

static int floor_div(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

static unsigned page_hash(int px, int pz) {
    unsigned h = (unsigned)px * 73856093u ^ (unsigned)pz * 19349663u;
    return h ^ (h >> 15);
}

static MemoryPage *find_page(const MemoryMap *m, int px, int pz) {
    if (!m->capacity) return NULL;
    unsigned mask = (unsigned)m->capacity - 1;
    for (unsigned i = page_hash(px, pz) & mask;; i = (i + 1) & mask) {
        MemoryPage *p = m->slots[i];
        if (!p) return NULL;
        if (p->px == px && p->pz == pz) return p;
    }
}

static void insert_slot(MemoryPage **slots, int capacity, MemoryPage *p) {
    unsigned mask = (unsigned)capacity - 1;
    unsigned i = page_hash(p->px, p->pz) & mask;
    while (slots[i]) i = (i + 1) & mask;
    slots[i] = p;
}

// Devuelve la pagina, creandola (y agrandando la tabla) si hace falta.
static MemoryPage *get_page(MemoryMap *m, int px, int pz) {
    MemoryPage *p = find_page(m, px, pz);
    if (p) return p;
    if ((m->page_count + 1) * 10 > m->capacity * 7) { // carga maxima 70 %
        int cap = m->capacity ? m->capacity * 2 : 64;
        MemoryPage **slots = calloc((size_t)cap, sizeof(*slots));
        if (!slots) return NULL;
        for (int i = 0; i < m->capacity; i++)
            if (m->slots[i]) insert_slot(slots, cap, m->slots[i]);
        free(m->slots);
        m->slots = slots;
        m->capacity = cap;
    }
    p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->px = px;
    p->pz = pz;
    insert_slot(m->slots, m->capacity, p);
    m->page_count++;
    return p;
}

static int to_cell(float w) { return (int)floorf(w / MEMMAP_CELL_SIZE); }

static MemoryCell *cell_in(MemoryPage *p, int cx, int cz) {
    return &p->cells[(cz - p->pz * MEMMAP_PAGE) * MEMMAP_PAGE + (cx - p->px * MEMMAP_PAGE)];
}

static const MemoryCell *find_cell(const MemoryMap *m, float x, float z) {
    int cx = to_cell(x), cz = to_cell(z);
    MemoryPage *p = find_page(m, floor_div(cx, MEMMAP_PAGE), floor_div(cz, MEMMAP_PAGE));
    return p ? cell_in(p, cx, cz) : NULL;
}

// Lo familiar se olvida mas despacio: la constante de olvido crece con la familiaridad.
static float decayed(const MemoryParams *p, const MemoryCell *c, float now) {
    float elapsed = now - c->t_last;
    if (elapsed <= 0.0f || c->light <= 0.0f) return c->light;
    float tau = p->forget_days * GAME_SECONDS_PER_DAY * (1.0f + c->familiarity);
    return c->light * expf(-elapsed / tau);
}

// Lo familiar tambien brilla mas: el tope crece con la familiaridad.
static float light_cap(const MemoryParams *p, float familiarity) {
    return p->base_cap + (1.0f - p->base_cap) * familiarity / (familiarity + 2.0f);
}

void memmap_visit(MemoryMap *m, float x, float z, float dt, float now) {
    const MemoryParams *p = &m->params;
    int r = (int)ceilf(p->sight_radius / MEMMAP_CELL_SIZE);
    int pcx = to_cell(x), pcz = to_cell(z);
    MemoryPage *page = NULL;
    for (int cz = pcz - r; cz <= pcz + r; cz++) {
        for (int cx = pcx - r; cx <= pcx + r; cx++) {
            float wx = ((float)cx + 0.5f) * MEMMAP_CELL_SIZE;
            float wz = ((float)cz + 0.5f) * MEMMAP_CELL_SIZE;
            float d = sqrtf((wx - x) * (wx - x) + (wz - z) * (wz - z));
            if (d > p->sight_radius) continue;
            float w = 1.0f - d / p->sight_radius; // mas nitido donde se pisa

            int px = floor_div(cx, MEMMAP_PAGE), pz = floor_div(cz, MEMMAP_PAGE);
            if (!page || page->px != px || page->pz != pz) page = get_page(m, px, pz);
            if (!page) return; // sin memoria: se deja de aprender, no se rompe
            MemoryCell *c = cell_in(page, cx, cz);

            c->light = decayed(p, c, now);
            c->t_last = now;
            c->familiarity += w * dt / 60.0f;
            float cap = light_cap(p, c->familiarity);
            if (c->light < cap) {
                float k = p->gain * w * dt;
                c->light += (cap - c->light) * (k > 1.0f ? 1.0f : k);
            }
        }
    }
}

void memmap_reveal(MemoryMap *m, float x, float z, float radius, float seconds, float now) {
    float saved = m->params.sight_radius;
    m->params.sight_radius = radius;
    memmap_visit(m, x, z, seconds, now);
    m->params.sight_radius = saved;
}

float memmap_light(const MemoryMap *m, float x, float z, float now) {
    const MemoryCell *c = find_cell(m, x, z);
    return c ? decayed(&m->params, c, now) : 0.0f;
}

float memmap_familiarity(const MemoryMap *m, float x, float z) {
    const MemoryCell *c = find_cell(m, x, z);
    return c ? c->familiarity : 0.0f;
}

bool memmap_toggle_marker(MemoryMap *m, float x, float z, MarkerKind kind, float radius) {
    for (int i = 0; i < m->marker_count; i++) {
        float dx = m->markers[i].x - x, dz = m->markers[i].z - z;
        if (dx * dx + dz * dz <= radius * radius) {
            m->markers[i] = m->markers[--m->marker_count];
            return false;
        }
    }
    if (m->marker_count >= MEMMAP_MAX_MARKERS) return false;
    m->markers[m->marker_count++] = (MapMarker){ x, z, kind };
    return true;
}

const MemoryPage *memmap_page_slot(const MemoryMap *m, int i) {
    return i >= 0 && i < m->capacity ? m->slots[i] : NULL;
}

bool memmap_restore_page(MemoryMap *m, const MemoryPage *page) {
    MemoryPage *p = get_page(m, page->px, page->pz);
    if (!p) return false;
    *p = *page;
    return true;
}
