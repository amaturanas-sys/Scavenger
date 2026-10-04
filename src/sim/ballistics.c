#include "ballistics.h"
#include "sim/lang.h"

#include <math.h>
#include <string.h>

const ProjectileDef *projectile_def(ProjectileKind k) {
    static const ProjectileDef defs[PROJ_COUNT] = {
        [PROJ_ARROW] = { N_("Flecha"), "proyectil.flecha.comun", 0.030f, 0.00012f, 3.2f, WOUND_CUT },
        [PROJ_BOLT] = { N_("Virote"), "proyectil.virote.comun", 0.060f, 0.00015f, 3.2f, WOUND_CUT },
        [PROJ_BALL] = { N_("Bala"), "proyectil.bala.mosquete", 0.030f, 0.00008f, 3.0f, WOUND_CUT },
        [PROJ_STONE] = { N_("Piedra"), "proyectil.piedra.honda", 0.050f, 0.00030f, 3.0f, WOUND_BRUISE },
    };
    return &defs[(unsigned)k < PROJ_COUNT ? k : 0];
}

const RangedDef *ranged_def(const char *id) {
    static const RangedDef defs[] = {
        { "arma.distancia.arco_compuesto", PROJ_ARROW, 75.0f, 0.8f, 0.6f, 0.012f },
        { "arma.distancia.arco_largo", PROJ_ARROW, 95.0f, 1.0f, 0.7f, 0.010f },
        { "arma.especial.arco_cuerno", PROJ_ARROW, 105.0f, 0.9f, 0.6f, 0.008f },
        { "arma.distancia.ballesta", PROJ_BOLT, 130.0f, 0.0f, 3.0f, 0.008f },
        { "arma.especial.ballesta_repeticion", PROJ_BOLT, 70.0f, 0.0f, 0.7f, 0.020f },
        { "arma.distancia.honda", PROJ_STONE, 40.0f, 0.6f, 0.9f, 0.025f },
        { "arma.distancia.mosquete", PROJ_BALL, 1500.0f, 0.0f, 8.0f, 0.020f },
    };
    if (!id || !id[0]) return NULL;
    for (size_t i = 0; i < sizeof(defs) / sizeof(defs[0]); i++)
        if (!strcmp(defs[i].weapon, id)) return &defs[i];
    return NULL;
}

float ranged_muzzle_speed(const RangedDef *w, float charge) {
    const ProjectileDef *pd = projectile_def(w->projectile);
    if (w->draw_time <= 0.0f) charge = 1.0f;
    if (charge < 0.15f) charge = 0.15f;
    if (charge > 1.0f) charge = 1.0f;
    return sqrtf(2.0f * w->power * charge / pd->mass);
}

void projectile_launch(Projectile *p, ProjectileKind kind, V3 origin, float yaw, float pitch, float speed) {
    p->kind = kind;
    p->pos = origin;
    p->vel = (V3){ sinf(yaw) * cosf(pitch) * speed, sinf(pitch) * speed, cosf(yaw) * cosf(pitch) * speed };
    p->alive = true;
}

void projectile_step(Projectile *p, float dt) {
    const ProjectileDef *pd = projectile_def(p->kind);
    // Pasos cortos para que la curva no dependa de los cuadros por segundo.
    int steps = (int)ceilf(dt / 0.01f);
    float h = dt / (float)(steps > 0 ? steps : 1);
    for (int i = 0; i < steps; i++) {
        float v = sqrtf(p->vel.x * p->vel.x + p->vel.y * p->vel.y + p->vel.z * p->vel.z);
        float k = pd->drag / pd->mass * v;
        p->vel.x -= k * p->vel.x * h;
        p->vel.y -= (GRAVITY + k * p->vel.y) * h;
        p->vel.z -= k * p->vel.z * h;
        p->pos.x += p->vel.x * h;
        p->pos.y += p->vel.y * h;
        p->pos.z += p->vel.z * h;
    }
}

float projectile_energy(const Projectile *p) {
    float v2 = p->vel.x * p->vel.x + p->vel.y * p->vel.y + p->vel.z * p->vel.z;
    return 0.5f * projectile_def(p->kind)->mass * v2;
}

float projectile_damage(const Projectile *p) { return projectile_def(p->kind)->damage_scale * sqrtf(projectile_energy(p)); }

int ballistic_trace(ProjectileKind kind, V3 origin, float yaw, float pitch, float speed, float dt, float min_y, V3 *out,
                    int n) {
    Projectile p;
    projectile_launch(&p, kind, origin, yaw, pitch, speed);
    int i = 0;
    for (; i < n; i++) {
        out[i] = p.pos;
        if (p.pos.y < min_y) return i + 1;
        projectile_step(&p, dt);
    }
    return i;
}

// Altura del proyectil al llegar a la distancia horizontal d (o -1e9 si no llega).
static float height_at(ProjectileKind kind, float pitch, float speed, float d) {
    Projectile p;
    projectile_launch(&p, kind, (V3){ 0, 0, 0 }, 0.0f, pitch, speed);
    for (int i = 0; i < 2000; i++) {
        V3 prev = p.pos;
        projectile_step(&p, 0.01f);
        if (p.pos.z >= d) {
            float f = (d - prev.z) / (p.pos.z - prev.z);
            return prev.y + (p.pos.y - prev.y) * f;
        }
        if (p.pos.y < -200.0f || p.vel.z <= 0.0f) break;
    }
    return -1e9f;
}

bool ballistic_solve(ProjectileKind kind, V3 origin, V3 target, float speed, float *pitch) {
    float dx = target.x - origin.x, dz = target.z - origin.z, dy = target.y - origin.y;
    float d = sqrtf(dx * dx + dz * dz);
    // Tiro bajo: la altura al llegar crece con la inclinacion hasta el alcance maximo.
    float lo = -0.6f, hi = 0.75f;
    if (height_at(kind, hi, speed, d) < dy) return false; // no llega
    for (int i = 0; i < 40; i++) {
        float mid = 0.5f * (lo + hi);
        if (height_at(kind, mid, speed, d) < dy) lo = mid;
        else hi = mid;
    }
    *pitch = 0.5f * (lo + hi);
    return true;
}
