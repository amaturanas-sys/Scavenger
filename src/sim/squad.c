#include "sim/squad.h"

#include <math.h>

static float dist(float ax, float az, float bx, float bz) { return sqrtf((ax - bx) * (ax - bx) + (az - bz) * (az - bz)); }

SquadChoice squad_escort_choose(SquadOrder order, float nx, float nz, float px, float pz, float hp01, const TargetCand *foes,
                                int n, int player_target) {
    SquadChoice c = { -1, false };
    if (hp01 < SQUAD_RETREAT_HP) { // herido: se aparta, ya no pelea
        c.retreat = true;
        return c;
    }
    if (order == ORDER_FOLLOW) return c;
    // Atacar: primero el que eligio el jugador, si no esta lejos de el.
    if (order == ORDER_ATTACK && player_target >= 0 && player_target < n && foes[player_target].alive &&
        dist(foes[player_target].x, foes[player_target].z, px, pz) <= SQUAD_LOCK_RANGE) {
        c.foe = player_target;
        return c;
    }
    // Si no, el mas cercano: atacando, a el o al jugador; defendiendo, solo el que se le acerca al jugador.
    float best = order == ORDER_DEFEND ? SQUAD_DEFEND_RADIUS : SQUAD_ATTACK_RANGE;
    for (int i = 0; i < n; i++) {
        if (!foes[i].alive) continue;
        float to_player = dist(foes[i].x, foes[i].z, px, pz);
        float d = order == ORDER_DEFEND ? to_player : fminf(dist(foes[i].x, foes[i].z, nx, nz), to_player);
        if (d < best) best = d, c.foe = i;
    }
    return c;
}

void squad_patrol_point(float cx, float cz, float start, int step, float *x, float *z) {
    float a = start + (float)step * 0.5235988f; // 30 grados por punto
    float r = step % 2 ? SQUAD_RING_MAX : SQUAD_RING_MIN;
    *x = cx + cosf(a) * r;
    *z = cz + sinf(a) * r;
}

int squad_guard_intercept(float gx, float gz, float cx, float cz, const TargetCand *foes, int n) {
    int best = -1;
    float bd = SQUAD_GUARD_REACT;
    for (int i = 0; i < n; i++) {
        float to_camp = dist(foes[i].x, foes[i].z, cx, cz);
        if (!foes[i].alive || to_camp > SQUAD_RING_MAX + 10.0f) continue; // aun fuera del anillo
        float d = dist(foes[i].x, foes[i].z, gx, gz);
        if (d < bd || (to_camp < SQUAD_RING_MIN && best < 0)) bd = d, best = i; // dentro del campamento: siempre
    }
    return best;
}

bool squad_villager_alarm(float vx, float vz, float cx, float cz, const TargetCand *foes, int n) {
    for (int i = 0; i < n; i++)
        if (foes[i].alive && (dist(foes[i].x, foes[i].z, cx, cz) < SQUAD_ALARM || dist(foes[i].x, foes[i].z, vx, vz) < SQUAD_ALARM))
            return true;
    return false;
}
