#include "sim/targeting.h"

#include <math.h>

#define TWO_PI 6.28318531f

static float dist(const TargetCand *c, float px, float pz) { return sqrtf((c->x - px) * (c->x - px) + (c->z - pz) * (c->z - pz)); }

bool target_keep(const TargetCand *c, int n, int current, float px, float pz) {
    return current >= 0 && current < n && c[current].alive && dist(&c[current], px, pz) <= TARGET_RANGE;
}

// Angulo del candidato alrededor del jugador, desde su frente, en [0, 2 pi).
static float around(const TargetCand *c, float px, float pz, float yaw) {
    float a = atan2f(c->x - px, c->z - pz) - yaw;
    a = fmodf(a, TWO_PI);
    return a < 0.0f ? a + TWO_PI : a;
}

int target_cycle(const TargetCand *c, int n, float px, float pz, float yaw, int current, int dir) {
    if (!target_keep(c, n, current, px, pz)) {
        // Sin objetivo: el que mejor se ve delante (cerca y poco a un lado).
        int best = -1;
        float bs = 1e9f;
        for (int i = 0; i < n; i++) {
            if (!target_keep(c, n, i, px, pz)) continue;
            float a = around(&c[i], px, pz, yaw);
            float side = a > TWO_PI * 0.5f ? TWO_PI - a : a; // cuanto se aparta del frente
            float s = dist(&c[i], px, pz) + side * 8.0f;
            if (s < bs) bs = s, best = i;
        }
        return best;
    }
    // El siguiente en orden alrededor del jugador (o el anterior), dando la vuelta.
    float from = around(&c[current], px, pz, yaw);
    int best = current;
    float bd = TWO_PI + 1.0f;
    for (int i = 0; i < n; i++) {
        if (i == current || !target_keep(c, n, i, px, pz)) continue;
        float d = around(&c[i], px, pz, yaw) - from;
        if (dir < 0) d = -d;
        if (d <= 0.0f) d += TWO_PI;
        if (d < bd) bd = d, best = i;
    }
    return best;
}
