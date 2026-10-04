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

bool camp_member_busy(const CampSite *c, int member) {
    for (int i = 0; i < c->ntask; i++)
        if (c->task[i].member == member) return true;
    return false;
}

bool camp_add_task(CampSite *c, CampTaskKind kind, int member, Role role) {
    if (c->ntask >= CAMP_TASKS || camp_member_busy(c, member)) return false;
    CampTask *t = &c->task[c->ntask++];
    t->kind = kind;
    t->member = member;
    t->role = role;
    t->work = kind == TASK_RECRUIT ? recruit_work() : train_work(role);
    t->done = 0.0f;
    return true;
}

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
