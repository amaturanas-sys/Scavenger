#include "troop.h"

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Tabla de balance: efecto de cada accion sobre la moral y la lealtad de los
// integrantes activos, con modificadores por rasgo. Es el lugar para tunear.
// ---------------------------------------------------------------------------
typedef struct {
    float morale;          // efecto base en todos
    float loyalty;         // efecto base en la lealtad
    float merciful;        // extra para compasivos
    float bloodthirsty;    // extra para sanguinarios
    float ambitious;       // extra para ambiciosos
    float devout;          // extra para devotos
    float loyalist;        // extra para leales
} ActionEffect;

static const ActionEffect EFFECTS[ACT_COUNT] = {
    [ACT_RECRUIT]          = { .morale = +1.0f },
    [ACT_TAKE_PRISONER]    = { .morale = +2.0f, .bloodthirsty = +1.0f },
    [ACT_RELEASE_PRISONER] = { .morale = -1.0f, .merciful = +4.0f, .bloodthirsty = -3.0f, .devout = +2.0f },
    [ACT_BANISH]           = { .morale = -3.0f, .loyalty = -2.0f, .ambitious = +1.0f, .loyalist = -2.0f },
    [ACT_EXECUTE_MEMBER]   = { .morale = -8.0f, .loyalty = -4.0f, .merciful = -10.0f, .bloodthirsty = +6.0f, .loyalist = -3.0f },
    [ACT_EXECUTE_PRISONER] = { .morale = -2.0f, .merciful = -6.0f, .bloodthirsty = +4.0f, .devout = -3.0f },
    [ACT_SHARE_LOOT]       = { .morale = +5.0f, .loyalty = +3.0f, .ambitious = +3.0f },
};

// Umbrales de desercion y rebelion.
#define DESERT_THRESHOLD 40.0f
#define DESERT_MAX 0.9f
#define REBEL_MORALE_THRESHOLD 45.0f
#define REBEL_MIN_ACTIVE 3
#define REBEL_AMBITIOUS_BONUS 0.05f
#define REBEL_AMBITIOUS_LOYALTY 30.0f
#define REBEL_MAX 0.9f

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void apply_action(Troop *t, CampAction action) {
    const ActionEffect *e = &EFFECTS[action];
    for (int i = 0; i < t->count; i++) {
        Member *m = &t->members[i];
        if (m->status != STATUS_ACTIVE) continue;
        float d = e->morale;
        if (m->traits & TRAIT_MERCIFUL) d += e->merciful;
        if (m->traits & TRAIT_BLOODTHIRSTY) d += e->bloodthirsty;
        if (m->traits & TRAIT_AMBITIOUS) d += e->ambitious;
        if (m->traits & TRAIT_DEVOUT) d += e->devout;
        if (m->traits & TRAIT_LOYALIST) d += e->loyalist;
        m->morale = clampf(m->morale + d, 0.0f, 100.0f);
        m->loyalty = clampf(m->loyalty + e->loyalty, 0.0f, 100.0f);
    }
    if (t->overlord) {
        t->overlord->relation = clampf(t->overlord->relation + t->overlord->stance[action], -100.0f, 100.0f);
    }
}

void troop_init(Troop *t, Kingdom *overlord) {
    memset(t, 0, sizeof(*t));
    t->next_id = 1;
    t->overlord = overlord;
}

static int add_member(Troop *t, const char *name, unsigned traits, MemberStatus status) {
    if (t->count >= TROOP_MAX) return -1;
    Member *m = &t->members[t->count++];
    memset(m, 0, sizeof(*m));
    m->id = t->next_id++;
    snprintf(m->name, NAME_LEN, "%s", name ? name : "");
    m->traits = traits;
    m->status = status;
    m->role = ROLE_NONE;
    m->morale = 60.0f;
    m->loyalty = status == STATUS_PRISONER ? 10.0f : 50.0f;
    return m->id;
}

int troop_recruit(Troop *t, const char *name, unsigned traits) {
    int id = add_member(t, name, traits, STATUS_ACTIVE);
    if (id >= 0) apply_action(t, ACT_RECRUIT);
    return id;
}

int troop_take_prisoner(Troop *t, const char *name, unsigned traits) {
    int id = add_member(t, name, traits, STATUS_PRISONER);
    if (id >= 0) apply_action(t, ACT_TAKE_PRISONER);
    return id;
}

Member *troop_find(Troop *t, int id) {
    for (int i = 0; i < t->count; i++)
        if (t->members[i].id == id) return &t->members[i];
    return NULL;
}

bool troop_assign_role(Troop *t, int id, Role role) {
    Member *m = troop_find(t, id);
    if (!m || m->status != STATUS_ACTIVE || role < 0 || role >= ROLE_COUNT) return false;
    m->role = role;
    return true;
}

bool troop_banish(Troop *t, int id) {
    Member *m = troop_find(t, id);
    if (!m || m->status != STATUS_ACTIVE) return false;
    m->status = STATUS_BANISHED;
    m->role = ROLE_NONE;
    apply_action(t, ACT_BANISH);
    return true;
}

bool troop_execute(Troop *t, int id) {
    Member *m = troop_find(t, id);
    if (!m) return false;
    CampAction action;
    if (m->status == STATUS_ACTIVE) action = ACT_EXECUTE_MEMBER;
    else if (m->status == STATUS_PRISONER) action = ACT_EXECUTE_PRISONER;
    else return false;
    m->status = STATUS_EXECUTED; // fuera del grupo antes de que la tropa reaccione
    m->role = ROLE_NONE;
    apply_action(t, action);
    return true;
}

bool troop_release_prisoner(Troop *t, int id) {
    Member *m = troop_find(t, id);
    if (!m || m->status != STATUS_PRISONER) return false;
    m->status = STATUS_BANISHED; // liberado: deja el campamento
    apply_action(t, ACT_RELEASE_PRISONER);
    return true;
}

void troop_share_loot(Troop *t) { apply_action(t, ACT_SHARE_LOOT); }

int troop_count_with_status(const Troop *t, MemberStatus status) {
    int n = 0;
    for (int i = 0; i < t->count; i++)
        if (t->members[i].status == status) n++;
    return n;
}

static float avg_field(const Troop *t, int loyalty) {
    float sum = 0.0f;
    int n = 0;
    for (int i = 0; i < t->count; i++) {
        const Member *m = &t->members[i];
        if (m->status != STATUS_ACTIVE) continue;
        sum += loyalty ? m->loyalty : m->morale;
        n++;
    }
    return n ? sum / n : 0.0f;
}

float troop_avg_morale(const Troop *t) { return avg_field(t, 0); }
float troop_avg_loyalty(const Troop *t) { return avg_field(t, 1); }

float troop_desertion_chance(const Member *m) {
    if (m->status != STATUS_ACTIVE) return 0.0f;
    float a = (DESERT_THRESHOLD - m->morale) / DESERT_THRESHOLD;
    float b = (DESERT_THRESHOLD - m->loyalty) / DESERT_THRESHOLD;
    float p = 0.25f * clampf(a, 0.0f, 1.0f) + 0.25f * clampf(b, 0.0f, 1.0f);
    return clampf(p, 0.0f, DESERT_MAX);
}

float troop_rebellion_chance(const Troop *t) {
    if (troop_count_with_status(t, STATUS_ACTIVE) < REBEL_MIN_ACTIVE) return 0.0f;
    float avg = troop_avg_morale(t);
    if (avg >= REBEL_MORALE_THRESHOLD) return 0.0f;
    float p = (REBEL_MORALE_THRESHOLD - avg) / REBEL_MORALE_THRESHOLD * 0.5f;
    for (int i = 0; i < t->count; i++) {
        const Member *m = &t->members[i];
        if (m->status == STATUS_ACTIVE && (m->traits & TRAIT_AMBITIOUS) && m->loyalty < REBEL_AMBITIOUS_LOYALTY)
            p += REBEL_AMBITIOUS_BONUS;
    }
    return clampf(p, 0.0f, REBEL_MAX);
}

DayReport troop_process_day(Troop *t, Rng *rng) {
    DayReport r = { .deserted = 0, .rebellion = false, .rebellion_leader = -1 };
    // La rebelion se evalua con el animo de la manana, antes de las deserciones.
    float rebel_p = troop_rebellion_chance(t);
    for (int i = 0; i < t->count; i++) {
        Member *m = &t->members[i];
        float p = troop_desertion_chance(m);
        if (p > 0.0f && rng_float(rng) < p) {
            m->status = STATUS_DESERTED;
            m->role = ROLE_NONE;
            r.deserted++;
        }
    }
    if (rebel_p > 0.0f && rng_float(rng) < rebel_p) {
        r.rebellion = true;
        // Instiga el activo menos leal (preferentemente ambicioso).
        float worst = 1e9f;
        for (int i = 0; i < t->count; i++) {
            const Member *m = &t->members[i];
            if (m->status != STATUS_ACTIVE) continue;
            float score = m->loyalty - ((m->traits & TRAIT_AMBITIOUS) ? 25.0f : 0.0f);
            if (score < worst) { worst = score; r.rebellion_leader = m->id; }
        }
    }
    return r;
}

void kingdom_init_iron_khanate(Kingdom *k) {
    memset(k, 0, sizeof(*k));
    snprintf(k->name, NAME_LEN, "Kanato de Hierro");
    k->relation = 10.0f;
    k->stance[ACT_EXECUTE_PRISONER] = +3.0f;
    k->stance[ACT_EXECUTE_MEMBER] = +1.0f;  // disciplina
    k->stance[ACT_RELEASE_PRISONER] = -4.0f;
    k->stance[ACT_TAKE_PRISONER] = +2.0f;
    k->stance[ACT_BANISH] = +1.0f;
}

void kingdom_init_jade_dynasty(Kingdom *k) {
    memset(k, 0, sizeof(*k));
    snprintf(k->name, NAME_LEN, "Dinastia de Jade");
    k->relation = 10.0f;
    k->stance[ACT_EXECUTE_PRISONER] = -6.0f;
    k->stance[ACT_EXECUTE_MEMBER] = -3.0f;
    k->stance[ACT_RELEASE_PRISONER] = +4.0f;
    k->stance[ACT_TAKE_PRISONER] = +1.0f;
    k->stance[ACT_SHARE_LOOT] = +1.0f;
}

const char *role_name(Role r) {
    static const char *N[ROLE_COUNT] = { "sin funcion", "explorador", "cazador", "cocinero",
                                         "herrero", "guardia", "curandero", "lugarteniente" };
    return (r >= 0 && r < ROLE_COUNT) ? N[r] : "?";
}

const char *status_name(MemberStatus s) {
    switch (s) {
    case STATUS_ACTIVE: return "activo";
    case STATUS_PRISONER: return "prisionero";
    case STATUS_BANISHED: return "desterrado";
    case STATUS_EXECUTED: return "ejecutado";
    case STATUS_DESERTED: return "desertor";
    }
    return "?";
}
