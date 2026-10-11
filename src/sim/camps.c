#include "camps.h"

#include <math.h>
#include <string.h>

void camps_init(CampSite *c, int n) {
    memset(c, 0, sizeof(*c) * (size_t)n);
    for (int i = 0; i < n; i++) c[i].guardian = -1;
}

static float dist(float ax, float az, float bx, float bz) { return sqrtf((ax - bx) * (ax - bx) + (az - bz) * (az - bz)); }

bool camp_far_enough(const CampSite *c, int n, float x, float z) {
    for (int i = 0; i < n; i++)
        if (c[i].used && dist(c[i].x, c[i].z, x, z) < CAMP_MIN_APART) return false;
    return true;
}

int camp_found(CampSite *c, int n, float x, float z, int day) {
    if (!camp_far_enough(c, n, x, z)) return -1;
    for (int i = 0; i < n; i++) {
        if (c[i].used) continue;
        memset(&c[i], 0, sizeof(c[i]));
        c[i].used = true;
        c[i].x = x, c[i].z = z;
        c[i].guardian = -1;
        c[i].founded_day = day;
        stock_init(&c[i].stock);
        return i;
    }
    return -1;
}

int camp_at(const CampSite *c, int n, float x, float z) {
    int best = -1;
    float bd = CAMP_RADIUS_M;
    for (int i = 0; i < n; i++) {
        if (!c[i].used) continue;
        float d = dist(c[i].x, c[i].z, x, z);
        if (d < bd) bd = d, best = i;
    }
    return best;
}

int camp_nearest(const CampSite *c, int n, float x, float z, float *out) {
    int best = -1;
    float bd = 1e30f;
    for (int i = 0; i < n; i++) {
        if (!c[i].used) continue;
        float d = dist(c[i].x, c[i].z, x, z);
        if (d < bd) bd = d, best = i;
    }
    if (out) *out = bd;
    return best;
}

int camps_count(const CampSite *c, int n) {
    int k = 0;
    for (int i = 0; i < n; i++) k += c[i].used;
    return k;
}

bool role_trainable(Role r) {
    return r == ROLE_SOLDIER || r == ROLE_SMITH || r == ROLE_DRUID || r == ROLE_GOLDSMITH || r == ROLE_HERDER || r == ROLE_HUNTER ||
           r == ROLE_SCOUT;
}

const Ingredient *train_cost(Role r) {
    // Lo que gasta aprender: el equipo del oficio y algo de comida para el tiempo de aprendizaje.
    static const Ingredient SOLDIER[] = { { "arma.corta.sable", 1 }, { "utileria.consumible.carne_seca", 2 }, { NULL, 0 } };
    static const Ingredient SMITH[] = { { "utileria.material.hierro", 1 }, { "utileria.material.carbon", 2 }, { NULL, 0 } };
    static const Ingredient DRUID[] = { { "utileria.consumible.hierbas", 3 }, { "utileria.consumible.miel", 1 }, { NULL, 0 } };
    static const Ingredient GOLDSMITH[] = { { "utileria.material.cobre", 2 }, { "utileria.material.carbon", 1 }, { NULL, 0 } };
    static const Ingredient HERDER[] = { { "utileria.consumible.carne_seca", 1 }, { NULL, 0 } };
    static const Ingredient HUNTER[] = { { "proyectil.flecha.comun", 10 }, { "utileria.consumible.carne_seca", 1 }, { NULL, 0 } };
    static const Ingredient SCOUT[] = { { "utileria.consumible.carne_seca", 2 }, { NULL, 0 } };
    static const Ingredient NONE[] = { { NULL, 0 } };
    switch (r) {
    case ROLE_SOLDIER: return SOLDIER;
    case ROLE_SMITH: return SMITH;
    case ROLE_DRUID: return DRUID;
    case ROLE_GOLDSMITH: return GOLDSMITH;
    case ROLE_HERDER: return HERDER;
    case ROLE_HUNTER: return HUNTER;
    case ROLE_SCOUT: return SCOUT;
    default: return NONE;
    }
}

float train_work(Role r) {
    switch (r) {
    case ROLE_SMITH:
    case ROLE_GOLDSMITH: return 360.0f; // oficios del fuego: lo que mas cuesta
    case ROLE_DRUID: return 420.0f;
    case ROLE_SOLDIER: return 240.0f;
    case ROLE_HUNTER:
    case ROLE_SCOUT: return 180.0f;
    default: return 120.0f;
    }
}

const Ingredient *recruit_cost(void) {
    static const Ingredient C[] = { { "utileria.consumible.carne_seca", 3 }, { NULL, 0 } }; // regalos y comida para el camino
    return C;
}

float recruit_work(void) { return 240.0f; }

float task_rate(int helpers, int teachers) {
    float r = 1.0f + 0.25f * (float)(helpers > 0 ? helpers : 0);
    if (r > 2.0f) r = 2.0f;
    return r * (1.0f + 0.6f * (float)(teachers > 0 ? teachers : 0));
}

const CampTask *camp_task_of(const CampSite *c, int member) {
    if (member <= 0) return NULL; // los ids empiezan en 1 (0: nadie)
    for (int i = 0; i < c->ntask; i++)
        if (c->task[i].member == member || c->task[i].member2 == member) return &c->task[i];
    return NULL;
}

bool camp_member_busy(const CampSite *c, int member) { return camp_task_of(c, member) != NULL; }

bool camp_add_task2(CampSite *c, CampTaskKind kind, int member, int member2, Role role) {
    if (kind == TASK_TRAIN) member2 = 0; // se aprende de a uno
    if (c->ntask >= CAMP_TASKS || camp_member_busy(c, member) || member2 == member || (member2 > 0 && camp_member_busy(c, member2)))
        return false;
    CampTask *t = &c->task[c->ntask++];
    memset(t, 0, sizeof(*t));
    t->kind = kind;
    t->member = member;
    t->member2 = member2 > 0 ? member2 : 0;
    t->role = role;
    t->work = task_work(kind, role);
    t->done = 0.0f;
    return true;
}

bool camp_add_task(CampSite *c, CampTaskKind kind, int member, Role role) { return camp_add_task2(c, kind, member, 0, role); }

int camp_tick(CampSite *c, float dt, const float *rates, CampTask *done_out, int max) {
    int done = 0;
    for (int i = 0; i < c->ntask;) {
        CampTask *t = &c->task[i];
        t->done += dt * (rates ? rates[i] : 1.0f);
        if (t->done >= t->work) {
            if (done < max && done_out) done_out[done] = *t;
            done++;
            c->task[i] = c->task[--c->ntask];
            continue;
        }
        i++;
    }
    return done;
}

// ---------------------------------------------------------------- las tareas de la tribu
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

float task_work(CampTaskKind kind, Role role) {
    switch (kind) {
    case TASK_RECRUIT: return recruit_work();
    case TASK_TRAIN: return train_work(role);
    case TASK_SCOUT_RESOURCES: return TASK_SCOUT_WORK;
    case TASK_HERD: return TASK_HERD_WORK;
    case TASK_HUNT: return TASK_HUNT_WORK;
    case TASK_GUARD: return TASK_GUARD_WORK;
    default: return 60.0f;
    }
}

bool task_away(CampTaskKind kind) { return kind == TASK_RECRUIT || kind == TASK_SCOUT_RESOURCES || kind == TASK_HUNT; }

Role task_skill(CampTaskKind kind) {
    switch (kind) {
    case TASK_RECRUIT:
    case TASK_SCOUT_RESOURCES: return ROLE_SCOUT; // conoce la estepa y a su gente
    case TASK_HERD: return ROLE_HERDER;
    case TASK_HUNT: return ROLE_HUNTER;
    case TASK_GUARD: return ROLE_GUARD;
    default: return ROLE_NONE;
    }
}

bool role_suits(CampTaskKind kind, Role r) {
    if (kind == TASK_GUARD && r == ROLE_SOLDIER) return true; // el oficio de las armas
    Role s = task_skill(kind);
    return s != ROLE_NONE && r == s;
}

const Ingredient *task_cost(CampTaskKind kind, Role role) {
    static const Ingredient SCOUT[] = { { "utileria.consumible.carne_seca", 2 }, { NULL, 0 } }; // provisiones para el camino
    static const Ingredient HUNT[] = { { "utileria.consumible.carne_seca", 1 }, { NULL, 0 } };
    static const Ingredient NONE[] = { { NULL, 0 } };
    switch (kind) {
    case TASK_RECRUIT: return recruit_cost();
    case TASK_TRAIN: return train_cost(role);
    case TASK_SCOUT_RESOURCES: return SCOUT;
    case TASK_HUNT: return HUNT;
    default: return NONE;
    }
}

bool task_pay(Stockpile *s, CampTaskKind kind, Role role, int people) {
    const Ingredient *cost = task_cost(kind, role);
    if (people < 1) people = 1;
    for (const Ingredient *m = cost; m->id; m++)
        if (stock_count(s, m->id) < m->count * people) return false;
    for (const Ingredient *m = cost; m->id; m++) stock_take(s, m->id, m->count * people);
    return true;
}

float task_risk(CampTaskKind kind, int skilled, int people, bool night) {
    float r;
    switch (kind) {
    case TASK_RECRUIT: r = 0.10f; break;         // caminos y campamentos ajenos
    case TASK_SCOUT_RESOURCES: r = 0.14f; break; // lejos, y hasta donde acampan los rivales
    case TASK_HUNT: r = 0.20f; break;            // fieras, caidas, la presa que embiste
    default: return 0.0f;                        // en el campamento: el riesgo es el de verdad
    }
    for (int i = 0; i < clampi(skilled, 0, 2); i++) r *= 0.6f;
    if (people >= 2) r *= 0.55f;
    if (night) r *= 1.5f;
    return r > 0.9f ? 0.9f : r;
}

void task_fates(CampTaskKind kind, int skilled, int people, bool night, Rng *rng, Fate *fate) {
    people = clampi(people, 1, 2);
    for (int i = 0; i < people; i++) fate[i] = FATE_OK;
    if (rng_float(rng) >= task_risk(kind, skilled, people, night)) return;
    // Percance: solo, le toca de lleno; acompañados, no siempre a los dos, y se sacan del apuro.
    for (int i = 0; i < people; i++) {
        if (people >= 2 && rng_float(rng) >= 0.6f) continue;
        fate[i] = rng_float(rng) < (people >= 2 ? 0.2f : 0.35f) ? FATE_DEAD : FATE_WOUNDED;
    }
}

int recruit_found(int skilled, int people, Rng *rng) {
    skilled = clampi(skilled, 0, 2);
    float first = 0.75f + 0.1f * (float)skilled + (people >= 2 ? 0.1f : 0.0f);
    float second = people >= 2 ? 0.3f + 0.15f * (float)skilled : 0.05f * (float)skilled;
    int n = rng_float(rng) < (first > 0.97f ? 0.97f : first);
    if (n && rng_float(rng) < second) n++;
    return n;
}

float scout_radius(int skilled, int people) { return 900.0f + 400.0f * (float)clampi(skilled, 0, 2) + (people >= 2 ? 300.0f : 0.0f); }

int scout_finds(int skilled, int people, Rng *rng) {
    int n = 1 + clampi(skilled, 0, 2) + (people >= 2);
    if (rng_float(rng) < 0.35f) n++;
    return clampi(n, 1, 4);
}

int scout_pick(const ScoutCand *c, int n, const bool *known, float cx, float cz, float radius, int max, int *out) {
    int got = 0;
    bool kind_done[FIND_KINDS] = { false };
    while (got < max) {
        int best = -1, best_any = -1;
        float bd = radius, bd_any = radius;
        for (int i = 0; i < n; i++) {
            if ((known && known[i]) || c[i].kind < 0 || c[i].kind >= FIND_KINDS) continue;
            bool taken = false;
            for (int k = 0; k < got && !taken; k++) taken = out[k] == i;
            if (taken) continue;
            float d = dist(c[i].x, c[i].z, cx, cz);
            if (d < bd_any) bd_any = d, best_any = i;
            if (!kind_done[c[i].kind] && d < bd) bd = d, best = i; // primero, una de cada clase
        }
        if (best < 0) best = best_any;
        if (best < 0) break;
        kind_done[c[best].kind] = true;
        out[got++] = best;
    }
    return got;
}

bool find_danger(FindKind k) { return k == FIND_RIVAL || k == FIND_CITADEL; }

int hunt_prey(int habitat, Rng *rng) {
    float total = 0.0f;
    for (int s = 0; s < SPECIES_COUNT; s++) {
        const SpeciesDef *d = species_def((Species)s);
        if (d->cls == CLASS_PREY && !d->aquatic && (d->habitat & habitat)) total += d->rarity;
    }
    if (total <= 0.0f) return -1;
    float pick = rng_float(rng) * total;
    int last = -1;
    for (int s = 0; s < SPECIES_COUNT; s++) {
        const SpeciesDef *d = species_def((Species)s);
        if (d->cls != CLASS_PREY || d->aquatic || !(d->habitat & habitat)) continue;
        last = s;
        if ((pick -= d->rarity) < 0.0f) return s;
    }
    return last;
}

HuntBag hunt_bag(int habitat, Season season, int skilled, int people, Rng *rng) {
    // Presas por estacion: el verano y el otoño son buenos; en invierno escasean. La grasa, en otoño.
    static const float GAME[SEASON_COUNT] = { 1.0f, 1.15f, 1.25f, 0.55f };
    static const int FAT[SEASON_COUNT] = { 0, 1, 2, 1 };
    HuntBag b = { 0, -1, 0, 0, 0, 0, 0 };
    int si = clampi((int)season, 0, SEASON_COUNT - 1);
    b.prey = hunt_prey(habitat, rng);
    if (b.prey < 0) return b;
    float expect = (1.0f + 0.8f * (float)clampi(skilled, 0, 2) + (people >= 2 ? 0.6f : 0.0f)) * GAME[si];
    b.kills = (int)(expect + rng_float(rng));
    const SpeciesDef *d = species_def((Species)b.prey);
    b.meat = b.kills * d->meat;
    b.hides = b.kills * d->hide;
    b.bones = b.kills * (d->size >= 1.3f ? 2 : 1);
    b.sinew = b.kills;
    b.fat = b.kills * FAT[si];
    return b;
}

#define HERD_EAT (1.0f / 120.0f)  // hambre que baja por s de pastoreo (con un pastor)
#define HERD_DRINK (1.0f / 90.0f) // sed que baja por s: los lleva a beber

bool herd_graze(Animal *a, float dt, int herders, bool grass) {
    if (!animal_domestic(a) || species_eats_meat(a->species) || herders <= 0) return false;
    float k = herders >= 2 ? 1.5f : 1.0f;
    a->hunger = fmaxf(0.0f, a->hunger - dt * HERD_EAT * k * (grass ? 1.0f : 0.5f));
    a->thirst = fmaxf(0.0f, a->thirst - dt * HERD_DRINK * k);
    return true;
}

int herd_milk(Animal *a, int herders) {
    int m = animal_milk(a);
    return m > 0 && herders >= 2 ? m + 1 : m;
}

float herd_loss_chance(int herders, int skilled, bool night) {
    float p = 0.18f;
    if (herders >= 2) p *= 0.4f;
    for (int i = 0; i < clampi(skilled, 0, 2); i++) p *= 0.6f;
    if (night) p *= 1.6f;
    return p > 0.9f ? 0.9f : p;
}
