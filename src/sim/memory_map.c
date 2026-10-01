#include "sim/memory_map.h"

#include <math.h>
#include <string.h>

MemoryParams memmap_default_params(void) {
    return (MemoryParams){
        .sight_radius = 22.0f,
        .gain = 0.35f,
        .forget_rate = 1.0f / 240.0f, // sin familiaridad: se apaga en unos minutos
        .base_cap = 0.3f,
    };
}

void memmap_init(MemoryMap *m) {
    memset(m, 0, sizeof(*m));
    m->params = memmap_default_params();
}

static MemoryCell *cell_at(MemoryMap *m, int cx, int cz) {
    if (cx < 0 || cz < 0 || cx >= MEMMAP_CELLS || cz >= MEMMAP_CELLS) return NULL;
    return &m->cells[cz * MEMMAP_CELLS + cx];
}

static int to_cell(float w) { return (int)floorf(w / MEMMAP_CELL_SIZE) + MEMMAP_CELLS / 2; }

// Lo familiar se olvida mas despacio: la tasa cae con la familiaridad.
static float decayed(const MemoryParams *p, const MemoryCell *c, float now) {
    float elapsed = now - c->t_last;
    if (elapsed <= 0.0f || c->light <= 0.0f) return c->light;
    return c->light * expf(-p->forget_rate * elapsed / (1.0f + c->familiarity));
}

// Lo familiar tambien brilla mas: el tope crece con la familiaridad.
static float light_cap(const MemoryParams *p, float familiarity) {
    return p->base_cap + (1.0f - p->base_cap) * familiarity / (familiarity + 2.0f);
}

void memmap_visit(MemoryMap *m, float x, float z, float dt, float now) {
    const MemoryParams *p = &m->params;
    int r = (int)ceilf(p->sight_radius / MEMMAP_CELL_SIZE);
    int pcx = to_cell(x), pcz = to_cell(z);
    for (int dz = -r; dz <= r; dz++) {
        for (int dx = -r; dx <= r; dx++) {
            MemoryCell *c = cell_at(m, pcx + dx, pcz + dz);
            if (!c) continue;
            float wx = ((float)(pcx + dx - MEMMAP_CELLS / 2) + 0.5f) * MEMMAP_CELL_SIZE;
            float wz = ((float)(pcz + dz - MEMMAP_CELLS / 2) + 0.5f) * MEMMAP_CELL_SIZE;
            float d = sqrtf((wx - x) * (wx - x) + (wz - z) * (wz - z));
            if (d > p->sight_radius) continue;
            float w = 1.0f - d / p->sight_radius; // mas nitido donde se pisa

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

float memmap_light(const MemoryMap *m, float x, float z, float now) {
    int cx = to_cell(x), cz = to_cell(z);
    if (cx < 0 || cz < 0 || cx >= MEMMAP_CELLS || cz >= MEMMAP_CELLS) return 0.0f;
    return decayed(&m->params, &m->cells[cz * MEMMAP_CELLS + cx], now);
}

float memmap_familiarity(const MemoryMap *m, float x, float z) {
    int cx = to_cell(x), cz = to_cell(z);
    if (cx < 0 || cz < 0 || cx >= MEMMAP_CELLS || cz >= MEMMAP_CELLS) return 0.0f;
    return m->cells[cz * MEMMAP_CELLS + cx].familiarity;
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
