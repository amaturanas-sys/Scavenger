#include "sim/caves.h"

#include <math.h>
#include <string.h>

#include "sim/lang.h"

#define CAVE_PI 3.14159265f
#define RING_SPAN 3.6652f    // rad: el anillo va de un lado de la boca al otro (210 grados)
#define RING_START 1.8326f   // rad: la primera recamara (a), a 105 grados del frente
#define RING_RMAX 5.5f       // m: la recamara mas grande

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

static float dist_xz(float ax, float az, float bx, float bz) { return sqrtf((ax - bx) * (ax - bx) + (az - bz) * (az - bz)); }

// ---------------------------------------------------------------- la planta
static void room_center(const Cave *c, int room, float *x, float *z) {
    if (room == CAVE_MOUTH) *x = 0.0f, *z = CAVE_VEST_Z;
    else *x = c->rooms[room].x, *z = c->rooms[room].z;
}

// Los extremos de un pasillo: en el borde de sus salas (la boca: el del vestibulo, con el suelo en 0).
void cave_corridor_ends(const Cave *c, int k, float *ax, float *az, float *ay, float *bx, float *bz, float *by) {
    const CaveCorridor *cr = &c->corridors[k];
    float fx, fz, tx, tz;
    room_center(c, cr->from, &fx, &fz);
    room_center(c, cr->to, &tx, &tz);
    float d = dist_xz(fx, fz, tx, tz);
    float ux = d > 1e-4f ? (tx - fx) / d : 0.0f, uz = d > 1e-4f ? (tz - fz) / d : 1.0f;
    float rf = cr->from == CAVE_MOUTH ? CAVE_VEST_R : c->rooms[cr->from].radius, rt = cr->to == CAVE_MOUTH ? CAVE_VEST_R : c->rooms[cr->to].radius;
    *ax = fx + ux * rf, *az = fz + uz * rf, *ay = cr->from == CAVE_MOUTH ? 0.0f : c->rooms[cr->from].floor_y;
    *bx = tx - ux * rt, *bz = tz - uz * rt, *by = cr->to == CAVE_MOUTH ? 0.0f : c->rooms[cr->to].floor_y;
}

static void add_corridor(Cave *c, int from, int to, bool one_way) {
    if (c->corridor_count >= CAVE_CORRIDORS_MAX) return;
    c->corridors[c->corridor_count] = (CaveCorridor){ from, to, 0.0f, one_way };
    float ax, az, ay, bx, bz, by;
    cave_corridor_ends(c, c->corridor_count, &ax, &az, &ay, &bx, &bz, &by);
    c->corridors[c->corridor_count].slope = fabsf(by - ay) / fmaxf(dist_xz(ax, az, bx, bz), 0.5f);
    c->corridor_count++;
}

// ---------------------------------------------------------------- lo de adentro
static CaveObject *add_object(Cave *c, CaveObjectKind kind, int room, float x, float z, float y, float size) {
    if (c->object_count >= CAVE_OBJECTS_MAX || room < 0 || room >= c->room_count) return NULL;
    CaveObject *o = &c->objects[c->object_count++];
    memset(o, 0, sizeof(*o));
    o->kind = (uint8_t)kind, o->room = (uint8_t)room;
    o->x = x, o->z = z, o->y = y, o->size = size;
    return o;
}

// Un lugar en el suelo de una sala (frac: de su radio) o junto a su pared.
static void floor_spot(const Cave *c, int room, uint32_t *s, float fmin, float fmax, float *x, float *z) {
    const CaveRoom *r = &c->rooms[room];
    float a = unit(s) * 2.0f * CAVE_PI, f = fmin + unit(s) * (fmax - fmin);
    *x = r->x + sinf(a) * r->radius * f, *z = r->z + cosf(a) * r->radius * f;
}

static float ceiling(const Cave *c, int room) { return c->rooms[room].floor_y + (room == 0 ? 2.0f : 1.0f) * CAVE_ROOM_HEIGHT; }

// Un lugar de la pared de una sala (angulo a) sin pasillo a menos de un metro: lo de la pared (celdas, antorchas,
// vetas, depositos, pinturas, telarañas) no tapa la entrada de un pasillo.
static bool wall_free(const Cave *c, int room, float a) {
    const CaveRoom *r = &c->rooms[room];
    for (int k = -1; k <= 1; k++) {
        float b = a + (float)k / r->radius, x = r->x + sinf(b) * (r->radius + 0.6f), z = r->z + cosf(b) * (r->radius + 0.6f);
        int where = 0;
        if (cave_floor(c, x, z, NULL, &where) && where >= CAVE_IN_CORRIDOR) return false;
    }
    return true;
}

// Junto a la pared, en un angulo libre (si no lo encuentra, en el ultimo probado).
static void wall_spot(const Cave *c, int room, uint32_t *s, float frac, float *x, float *z) {
    const CaveRoom *r = &c->rooms[room];
    float a = unit(s) * 2.0f * CAVE_PI;
    for (int k = 0; k < 16 && !wall_free(c, room, a); k++) a += 0.45f;
    *x = r->x + sinf(a) * r->radius * frac, *z = r->z + cosf(a) * r->radius * frac;
}

// n objetos de un tipo, cada uno en una sala al azar (o en la dada), en el suelo o en la pared.
static void scatter(Cave *c, uint32_t *s, CaveObjectKind kind, int n, int room, bool wall, float above, float size) {
    for (int i = 0; i < n; i++) {
        int rm = room >= 0 ? room : (int)(unit(s) * (float)c->room_count) % c->room_count;
        float x, z;
        if (wall) wall_spot(c, rm, s, 0.86f + unit(s) * 0.04f, &x, &z);
        else floor_spot(c, rm, s, 0.15f, 0.72f, &x, &z);
        CaveObject *o = add_object(c, kind, rm, x, z, c->rooms[rm].floor_y + above, size * (0.75f + 0.5f * unit(s)));
        if (o && (kind == COBJ_TORCH || kind == COBJ_FIRE)) o->lit = c->kind != CAVE_ABANDONED && c->kind != CAVE_ANIMAL;
    }
}

static int count(uint32_t *s, int lo, int hi) { return lo + (int)(unit(s) * (float)(hi - lo + 1)) % (hi - lo + 1); }

static void furnish(Cave *c, uint32_t *s) {
    bool animal = c->kind == CAVE_ANIMAL, ruin = c->kind == CAVE_ABANDONED;
    bool lived = c->kind >= CAVE_BANDITS; // forajidos, tribu, imperio
    int deep = 1 + (c->room_count - 2) / 2; // una recamara del fondo: el calabozo
    // Lo que se toma o se rescata, primero (si no caben todos, sobran adornos, no esto).
    if (lived || ruin) { // el calabozo: celdas con cadenas; en las habitadas, a veces con un prisionero
        int cells = count(s, 1, 3);
        float a = unit(s) * 2.0f * CAVE_PI;
        for (int i = 0; i < cells; i++, a += 2.1f / c->rooms[deep].radius) { // una junto a otra, sin tapar un pasillo
            const CaveRoom *r = &c->rooms[deep];
            for (int k = 0; k < 16 && !wall_free(c, deep, a); k++) a += 0.45f;
            CaveObject *o = add_object(c, COBJ_CELL, deep, r->x + sinf(a) * r->radius * 0.78f, r->z + cosf(a) * r->radius * 0.78f, r->floor_y, 1.0f);
            if (o && lived && unit(s) < 0.65f) {
                float k = unit(s);
                PrisonerKind pk = c->kind == CAVE_BANDITS ? (k < 0.35f ? PRISONER_SHEPHERD : k < 0.6f ? PRISONER_HUNTER : k < 0.75f ? PRISONER_SMITH : PRISONER_OUTLAW)
                                                          : (k < 0.3f ? PRISONER_SHEPHERD : k < 0.55f ? PRISONER_HUNTER : k < 0.8f ? PRISONER_SMITH : PRISONER_OUTLAW);
                o->prisoner = (uint8_t)(1 + pk);
            }
        }
    }
    if (lived) {
        scatter(c, s, COBJ_CHEST, count(s, 1, 3), -1, false, 0.0f, 1.0f);
        scatter(c, s, COBJ_ARMS, c->kind == CAVE_TRIBE ? count(s, 0, 1) : count(s, 1, 2), -1, true, 0.0f, 1.0f);
        scatter(c, s, COBJ_AMPHORA, count(s, 1, 3), -1, false, 0.0f, 1.0f);
    } else if (ruin) {
        scatter(c, s, COBJ_CHEST, count(s, 0, 1), -1, false, 0.0f, 1.0f);
        scatter(c, s, COBJ_AMPHORA, count(s, 0, 2), -1, false, 0.0f, 1.0f);
    }
    if (animal || ruin) {
        scatter(c, s, COBJ_LOOT, count(s, 1, 2), 0, false, 0.0f, 1.0f); // el botin, en la Cisterna
        scatter(c, s, COBJ_WEAPON, count(s, 0, 2), -1, false, 0.0f, 1.0f);
        int nests = count(s, 1, 2);
        for (int i = 0; i < nests; i++) {
            int rm = 1 + (int)(unit(s) * (float)(c->room_count - 1)) % (c->room_count - 1);
            float x, z;
            floor_spot(c, rm, s, 0.3f, 0.7f, &x, &z);
            add_object(c, COBJ_NEST, rm, x, z, c->rooms[rm].floor_y, 1.0f);
            if (animal) add_object(c, COBJ_EGGS, rm, x, z, c->rooms[rm].floor_y + 0.18f, (float)count(s, 2, 5));
        }
    }
    scatter(c, s, COBJ_BONES, animal ? count(s, 2, 4) : count(s, 1, 2), -1, false, 0.0f, 1.0f);
    scatter(c, s, COBJ_VEIN, count(s, 1, 3), -1, true, 1.2f, 1.0f);
    // La luz y lo de quien vivio alli.
    if (lived || ruin) {
        for (int rm = 0; rm < c->room_count; rm++) // antorchas en la Cisterna y en la mitad de las recamaras
            if (rm == 0 || unit(s) < 0.5f) scatter(c, s, COBJ_TORCH, rm == 0 ? 2 : 1, rm, true, 1.8f, 1.0f);
        scatter(c, s, COBJ_FIRE, 1, 0, false, 0.0f, 1.0f);
        scatter(c, s, COBJ_COLUMN, c->kind == CAVE_EMPIRE ? count(s, 2, 4) : count(s, 0, 2), 0, false, 0.0f, 1.0f);
        scatter(c, s, COBJ_PAINTING, c->kind == CAVE_EMPIRE ? 1 : count(s, 1, 3), 0, true, 1.5f, 1.0f);
    }
    if (animal) scatter(c, s, COBJ_PREY, count(s, 1, 3), -1, false, 0.0f, 1.0f);
    // Lo comun a todas: la cueva misma.
    scatter(c, s, COBJ_BLOOD, animal ? count(s, 1, 2) : count(s, 0, 1), -1, false, 0.02f, 1.0f);
    scatter(c, s, COBJ_PUDDLE, count(s, 0, 2), 0, false, 0.02f, 1.0f);
    scatter(c, s, COBJ_WEB, count(s, 1, 3), -1, true, 2.4f, 1.0f);
    scatter(c, s, COBJ_INSECTS, count(s, 1, 2), -1, false, 0.6f, 1.0f);
    for (int rm = 0; rm < c->room_count; rm++) {
        int n = count(s, 1, 2);
        for (int i = 0; i < n; i++) {
            float x, z;
            floor_spot(c, rm, s, 0.1f, 0.8f, &x, &z);
            add_object(c, COBJ_STALACTITE, rm, x, z, ceiling(c, rm), 0.7f + unit(s) * 0.8f);
        }
        if (unit(s) < 0.6f) scatter(c, s, COBJ_STALAGMITE, 1, rm, false, 0.0f, 1.0f);
    }
}

void cave_generate(Cave *c, uint32_t world_seed, int index, float x, float z, int region, int species) {
    memset(c, 0, sizeof(*c));
    uint32_t s = mix(world_seed ^ (uint32_t)(index + 1) * 0xA511E9B3u);
    c->x = x, c->z = z, c->region = region, c->animal_species = species;
    c->yaw = (unit(&s) * 2.0f - 1.0f) * CAVE_PI;
    float k = unit(&s);
    c->kind = k < 0.40f ? CAVE_ANIMAL : k < 0.55f ? CAVE_ABANDONED : k < 0.75f ? CAVE_BANDITS : k < 0.87f ? CAVE_TRIBE : CAVE_EMPIRE;
    // La bajada, la mas empinada: del vestibulo a la Cisterna, D metros abajo con una pendiente de 1,1 a 1,3.
    float cr = 7.0f + unit(&s) * 2.0f, depth = 14.0f + unit(&s) * 6.0f, steep = 1.1f + unit(&s) * 0.2f;
    const float start = CAVE_VEST_Z + CAVE_VEST_R; // donde empieza la bajada
    float cz = start + cr + depth / steep;
    int ring = 3 + (int)(unit(&s) * 7.0f);
    if (ring > 9) ring = 9;
    c->room_count = 1 + ring;
    // El anillo, de un lado de la boca al otro por el fondo: alto junto a la boca (a e i, a 4 m) y hondo al
    // fondo (a 1,5 m de la Cisterna). Lo bastante ancho para que las salas no se pisen y sus pasillos no
    // empinen mas de 0,8.
    float step = RING_SPAN / (float)(ring - 1);
    int half = (ring - 1) / 2;
    float drop = (depth - 5.5f) / (float)(half > 0 ? half : 1); // lo que baja de una recamara a la siguiente
    float need = (2.0f * RING_RMAX + drop / 0.8f + 0.5f) / (2.0f * sinf(step * 0.5f));
    float R = fmaxf(cr + 12.0f + unit(&s) * 3.0f, need);
    float rr[CAVE_ROOMS_MAX];
    for (int i = 0; i < ring; i++) rr[i] = 3.5f + unit(&s) * 2.0f;
    for (int i = 0; i < ring; i++) {
        float phi = RING_START - (float)i * step;
        int e = i < ring - 1 - i ? i : ring - 1 - i; // pasos desde el extremo mas cercano
        float fy = -4.0f - drop * (float)(e < half ? e : half) + (unit(&s) - 0.5f) * 0.6f;
        c->rooms[1 + i] = (CaveRoom){ sinf(phi) * R, cz + cosf(phi) * R, fy, rr[i] };
    }
    // La Cisterna, 1,5 m bajo la recamara mas honda del fondo.
    float deepest = 0.0f;
    for (int i = 1; i < c->room_count; i++) deepest = fminf(deepest, c->rooms[i].floor_y);
    c->rooms[0] = (CaveRoom){ 0.0f, cz, deepest - 1.5f, cr };
    // La bajada se ajusta a la Cisterna: siempre la mas empinada.
    c->rooms[0].z = start + cr + (1.5f - deepest) / steep;
    for (int i = 1; i < c->room_count; i++) c->rooms[i].z += c->rooms[0].z - cz;
    // Los pasillos: la bajada (sin vuelta), las dos salidas (a e i), el anillo y las subidas de la Cisterna.
    add_corridor(c, CAVE_MOUTH, 0, true);
    add_corridor(c, CAVE_MOUTH, 1, false);
    add_corridor(c, CAVE_MOUTH, ring, false);
    for (int i = 1; i < ring; i++) add_corridor(c, i, i + 1, false);
    int mid = 1 + (ring - 1) / 2;
    add_corridor(c, 0, mid, false);
    if (ring % 2 == 0) add_corridor(c, 0, mid + 1, false); // con un anillo par, las dos del fondo estan igual de hondas
    furnish(c, &s);
}

// ---------------------------------------------------------------- las reglas
bool cave_room_reachable(const Cave *c, int from, int to) {
    // Nodos: la boca (0) y las salas (1..n).
    bool seen[CAVE_ROOMS_MAX + 1] = { false };
    int queue[CAVE_ROOMS_MAX + 1], head = 0, tail = 0;
    int nf = from + 1, nt = to + 1;
    if (nf < 0 || nf > c->room_count || nt < 0 || nt > c->room_count) return false;
    seen[nf] = true;
    queue[tail++] = nf;
    while (head < tail) {
        int node = queue[head++];
        if (node == nt) return true;
        for (int i = 0; i < c->corridor_count; i++) {
            const CaveCorridor *p = &c->corridors[i];
            int a = p->from + 1, b = p->to + 1;
            int next = a == node ? b : (b == node && !p->one_way) ? a : -1;
            if (next >= 0 && next <= c->room_count && !seen[next]) seen[next] = true, queue[tail++] = next;
        }
    }
    return false;
}

bool cave_layout_valid(const Cave *c) {
    if (c->room_count < 4 || c->room_count > CAVE_ROOMS_MAX || c->corridor_count < c->room_count + 1) return false;
    const CaveCorridor *down = &c->corridors[0];
    if (down->from != CAVE_MOUTH || down->to != 0 || !down->one_way) return false;
    for (int i = 0; i < c->room_count; i++) {
        const CaveRoom *a = &c->rooms[i];
        if (a->radius <= 0.0f || a->floor_y >= 0.0f || a->z <= 0.0f) return false; // todo adentro y abajo
        if (dist_xz(a->x, a->z, 0.0f, CAVE_VEST_Z) < a->radius + CAVE_VEST_R + 2.0f) return false; // ni sobre el vestibulo
        if (i > 0 && a->floor_y <= c->rooms[0].floor_y) return false; // la Cisterna, lo mas hondo
        for (int j = i + 1; j < c->room_count; j++) // las salas no se pisan
            if (dist_xz(a->x, a->z, c->rooms[j].x, c->rooms[j].z) < a->radius + c->rooms[j].radius + 2.0f) return false;
        if (!cave_room_reachable(c, CAVE_MOUTH, i)) return false; // se llega
        if (!cave_room_reachable(c, i, CAVE_MOUTH)) return false; // y se sale
    }
    for (int i = 1; i < c->corridor_count; i++) // la bajada es la unica sin vuelta y la mas empinada
        if (c->corridors[i].one_way || c->corridors[i].slope >= down->slope) return false;
    return true;
}

static bool in_corridor(const Cave *c, int k, float x, float z, float *y, float *t) {
    float ax, az, ay, bx, bz, by;
    cave_corridor_ends(c, k, &ax, &az, &ay, &bx, &bz, &by);
    float dx = bx - ax, dz = bz - az, len2 = dx * dx + dz * dz;
    float u = len2 > 1e-6f ? ((x - ax) * dx + (z - az) * dz) / len2 : 0.0f;
    if (t) *t = u;
    float cl = u < 0.0f ? 0.0f : u > 1.0f ? 1.0f : u;
    if (dist_xz(x, z, ax + dx * cl, az + dz * cl) > CAVE_CORRIDOR_HALF) return false;
    if (y) *y = ay + (by - ay) * cl;
    return true;
}

bool cave_floor(const Cave *c, float x, float z, float *y, int *where) {
    for (int i = 0; i < c->room_count; i++) {
        const CaveRoom *r = &c->rooms[i];
        if (dist_xz(x, z, r->x, r->z) < r->radius - 0.15f) {
            if (y) *y = r->floor_y;
            if (where) *where = i;
            return true;
        }
    }
    if (dist_xz(x, z, 0.0f, CAVE_VEST_Z) < CAVE_VEST_R - 0.15f) { // el vestibulo, llano
        if (y) *y = 0.0f;
        if (where) *where = CAVE_VESTIBULE;
        return true;
    }
    // Donde se cruzan (junto a una sala), cuenta el pasillo que se puede subir.
    for (int pass = 0; pass < 2; pass++)
        for (int k = 0; k < c->corridor_count; k++) {
            if (c->corridors[k].one_way != (pass == 1)) continue;
            if (in_corridor(c, k, x, z, y, NULL)) {
                if (where) *where = CAVE_IN_CORRIDOR + k;
                return true;
            }
        }
    return false;
}

bool cave_step_ok(const Cave *c, float x, float z, float nx, float nz) {
    int where;
    if (!cave_floor(c, nx, nz, NULL, &where)) return false;
    if (where == CAVE_IN_CORRIDOR + 0) { // la bajada: solo hacia la Cisterna
        float t0, t1;
        in_corridor(c, 0, x, z, NULL, &t0);
        in_corridor(c, 0, nx, nz, NULL, &t1);
        if (t1 < t0 - 1e-5f) return false;
    }
    return true;
}

void cave_entry(const Cave *c, float *x, float *z, float *yaw) {
    (void)c;
    *x = 0.0f, *z = 0.9f, *yaw = 0.0f;
}

bool cave_at_exit(const Cave *c, float x, float z) {
    (void)c;
    return z < 0.1f && fabsf(x) < CAVE_CORRIDOR_HALF + 0.3f;
}

void cave_to_world(const Cave *c, float lx, float lz, float *wx, float *wz) {
    float s = sinf(c->yaw), co = cosf(c->yaw); // +z de la cueva: (sin yaw, cos yaw) del mundo
    *wx = c->x + lz * s + lx * co;
    *wz = c->z + lz * co - lx * s;
}

void cave_from_world(const Cave *c, float wx, float wz, float *lx, float *lz) {
    float s = sinf(c->yaw), co = cosf(c->yaw), dx = wx - c->x, dz = wz - c->z;
    *lz = dx * s + dz * co;
    *lx = dx * co - dz * s;
}

float cave_ceiling(const Cave *c, int where, float floor_y) {
    if (where >= 0 && where < c->room_count) return ceiling(c, where);
    if (where == CAVE_VESTIBULE) return CAVE_VEST_HEIGHT;
    return floor_y + CAVE_CORRIDOR_HEIGHT;
}

// ---------------------------------------------------------------- botin y prisioneros
const char *cave_object_loot(const Cave *c, int obj, int *count) {
    if (obj < 0 || obj >= c->object_count) return NULL;
    const CaveObject *o = &c->objects[obj];
    uint32_t h = mix((uint32_t)obj * 0x9E3779B9u ^ (uint32_t)(c->x * 13.0f) ^ (uint32_t)(c->z * 7.0f) * 0x85EBCA6Bu);
    int r = (int)(h % 1000u);
    *count = 1;
    switch (o->kind) {
    case COBJ_BONES: *count = 1 + r % 3; return "utileria.material.hueso";
    case COBJ_VEIN: *count = 1 + r % 2; return r % 3 ? "utileria.material.mineral_hierro" : "utileria.material.cobre";
    case COBJ_NEST: *count = 2 + r % 3; return "utileria.material.plumas";
    case COBJ_WEAPON: {
        static const char *W[] = { "arma.corta.daga", "arma.corta.hacha_mano", "arma.larga.lanza", "arma.corta.cuchillo" };
        return W[r % 4];
    }
    case COBJ_LOOT: {
        static const char *L[] = { "utileria.gema.turquesa", "utileria.gema.cornalina", "utileria.gema.lapislazuli", "utileria.gema.ambar",
                                   "accesorio.anillo.plata", "accesorio.brazalete.plata" };
        *count = 1 + r % 2;
        return L[r % 6];
    }
    case COBJ_CHEST: {
        static const char *BANDITS[] = { "accesorio.anillo.plata", "utileria.gema.granate", "proyectil.flecha.comun", "accesorio.collar.plata" };
        static const char *TRIBE[] = { "utileria.material.pieles", "utileria.consumible.queso_seco", "utileria.consumible.carne_seca", "utileria.gema.turquesa" };
        static const char *EMPIRE[] = { "accesorio.anillo.oro", "utileria.gema.jade", "accesorio.brazalete.plata", "utileria.gema.lapislazuli" };
        const char **t = c->kind == CAVE_EMPIRE ? EMPIRE : c->kind == CAVE_TRIBE ? TRIBE : BANDITS;
        const char *id = t[r % 4];
        *count = !strcmp(id, "proyectil.flecha.comun") ? 6 : strstr(id, "consumible") ? 2 + r % 2 : 1;
        return id;
    }
    case COBJ_AMPHORA: return c->kind == CAVE_EMPIRE ? "utileria.objeto.saco_grano" : "utileria.consumible.airag";
    case COBJ_ARMS: {
        static const char *A[] = { "arma.corta.hacha_mano", "arma.larga.lanza", "armadura.casco.laminar_cuero", "armadura.casco.bronce" };
        return c->kind == CAVE_EMPIRE ? A[2 + r % 2] : A[r % 3];
    }
    default: return NULL;
    }
}

int cave_betray_percent(const Cave *c, int obj) {
    if (obj < 0 || obj >= c->object_count || c->objects[obj].kind != COBJ_CELL || !c->objects[obj].prisoner) return 0;
    static const int BASE[PRISONER_KINDS] = { 10, 18, 14, 55 }; // pastor, cazador, herrero, forajido
    int p = BASE[(c->objects[obj].prisoner - 1) % PRISONER_KINDS];
    p += c->kind == CAVE_BANDITS ? 15 : c->kind == CAVE_TRIBE ? -5 : 0;
    return p < 5 ? 5 : p > 90 ? 90 : p;
}

bool cave_prisoner_joins(const Cave *c, int obj, float roll) { return roll * 100.0f >= (float)cave_betray_percent(c, obj); }

const char *cave_kind_name(CaveKind k) {
    switch (k) {
    case CAVE_ANIMAL: return N_("cueva de fieras");
    case CAVE_ABANDONED: return N_("cueva abandonada");
    case CAVE_BANDITS: return N_("refugio de forajidos");
    case CAVE_TRIBE: return N_("refugio de una tribu");
    case CAVE_EMPIRE: return N_("almacén del imperio");
    default: return "?";
    }
}

const char *cave_prisoner_name(PrisonerKind k) {
    switch (k) {
    case PRISONER_SHEPHERD: return N_("pastor");
    case PRISONER_HUNTER: return N_("cazador");
    case PRISONER_SMITH: return N_("herrero");
    case PRISONER_OUTLAW: return N_("forajido");
    default: return "?";
    }
}
