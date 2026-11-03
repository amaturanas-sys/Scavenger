#include "caves.h"

#include <math.h>
#include <string.h>

static uint32_t mix(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    return x ^ (x >> 16);
}

static float unit(uint32_t *state) {
    *state = mix(*state + 0x9E3779B9u);
    return (float)(*state & 0x00FFFFFFu) / 16777215.0f;
}

static float distance_xz(const CaveRoom *a, const CaveRoom *b) {
    float dx = a->x - b->x, dz = a->z - b->z;
    return sqrtf(dx * dx + dz * dz);
}

void cave_generate(Cave *c, uint32_t world_seed, int index, float x, float z, int region, int species) {
    memset(c, 0, sizeof(*c));
    uint32_t state = world_seed ^ (uint32_t)(index + 1) * 0xA511E9B3u;
    int ring = 3 + (int)(unit(&state) * 7.0f);
    c->x = x;
    c->z = z;
    c->yaw = (unit(&state) * 2.0f - 1.0f) * 3.14159265f;
    c->region = region;
    c->kind = CAVE_ANIMAL;
    c->animal_species = species;
    c->room_count = ring + 1;

    c->rooms[0] = (CaveRoom){ 0.0f, 18.0f, 0.0f, 7.0f };
    for (int i = 0; i < ring; i++) {
        float a = (float)i / (float)ring * 6.2831853f;
        float radius = 13.0f + unit(&state) * 8.0f;
        c->rooms[i + 1] = (CaveRoom){ cosf(a) * radius, sinf(a) * radius, -1.5f - unit(&state) * 7.0f, 3.5f + unit(&state) * 2.0f };
    }

    // La bajada es el unico acceso a la Cisterna y solo se recorre hacia dentro.
    c->corridors[0] = (CaveCorridor){ -1, 0, 0.20f, true };
    c->corridor_count = 1;
    c->corridors[c->corridor_count++] = (CaveCorridor){ 0, 1, 0.0f, false };
    for (int i = 0; i < ring; i++) {
        int next = (i + 1) % ring;
        CaveRoom *a = &c->rooms[i + 1], *b = &c->rooms[next + 1];
        c->corridors[c->corridor_count++] = (CaveCorridor){ i + 1, next + 1, fabsf(a->floor_y - b->floor_y) / fmaxf(distance_xz(a, b), 1.0f), false };
    }
}

bool cave_room_reachable(const Cave *c, int from, int to) {
    bool seen[CAVE_ROOMS_MAX] = { false };
    int queue[CAVE_ROOMS_MAX], head = 0, tail = 0;
    if (from < 0 || from >= c->room_count || to < 0 || to >= c->room_count) return false;
    seen[from] = true;
    queue[tail++] = from;
    while (head < tail) {
        int room = queue[head++];
        if (room == to) return true;
        for (int i = 0; i < c->corridor_count; i++) {
            const CaveCorridor *p = &c->corridors[i];
            int next = p->from == room ? p->to : p->to == room && !p->one_way ? p->from : -1;
            if (next >= 0 && next < c->room_count && !seen[next]) seen[next] = true, queue[tail++] = next;
        }
    }
    return false;
}

bool cave_layout_valid(const Cave *c) {
    if (c->room_count < 4 || c->room_count > CAVE_ROOMS_MAX || c->corridor_count != c->room_count + 1) return false;
    if (c->corridors[0].from != -1 || c->corridors[0].to != 0 || !c->corridors[0].one_way) return false;
    for (int i = 0; i < c->room_count; i++) {
        if (!cave_room_reachable(c, 0, i)) return false;
        if (c->rooms[i].radius <= 0.0f) return false;
    }
    for (int i = 2; i < c->corridor_count; i++)
        if (c->corridors[i].from <= 0 || c->corridors[i].to <= 0) return false;
    return true;
}
