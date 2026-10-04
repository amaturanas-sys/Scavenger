#include "travel.h"

#include <math.h>
#include <string.h>

float travel_speed(TravelMode m) { return m == TRAVEL_MOUNTED ? 8.0f : 3.5f; }

float journey_eta(float dist, TravelMode m) { return 10.0f + dist / travel_speed(m) * 1.3f; } // el camino no es recto

float journey_risk(float dist, float knowledge, int group, bool night) {
    if (knowledge <= 0.0f) return 0.9f; // nadie conoce el camino: casi seguro se pierden
    float r = 0.04f + dist / 2500.0f;                // mas lejos, mas peligro
    r *= 1.6f - 0.8f * fminf(1.0f, knowledge);       // si todos lo conocen, la mitad
    r /= 1.0f + 0.15f * (float)(group > 1 ? group - 1 : 0); // en grupo se cuidan
    if (night) r *= 1.4f;
    return fminf(0.9f, fmaxf(0.0f, r));
}

int journey_resolve(Journey *j, Rng *rng, bool night) {
    float dist = sqrtf((j->x1 - j->x0) * (j->x1 - j->x0) + (j->z1 - j->z0) * (j->z1 - j->z0));
    float risk = journey_risk(dist, j->knowledge, j->n, night);
    bool trouble = rng_float(rng) < risk; // un percance: emboscada, fieras, extraviados
    int alive = 0;
    for (int i = 0; i < j->n; i++) {
        j->fate[i] = FATE_OK;
        if (trouble) {
            float r = rng_float(rng);
            if (r < 0.25f) j->fate[i] = FATE_DEAD;
            else if (r < 0.75f) j->fate[i] = FATE_WOUNDED;
        }
        alive += j->fate[i] != FATE_DEAD;
    }
    return alive;
}

int journey_start(Journey *list, int max, JourneyKind kind, const int *people, int n, int site, float x0, float z0, float x1, float z1,
                  TravelMode mode, float knowledge) {
    if (n <= 0) return -1;
    for (int i = 0; i < max; i++) {
        if (list[i].used) continue;
        Journey *j = &list[i];
        memset(j, 0, sizeof(*j));
        j->used = true;
        j->kind = kind;
        j->n = n > JOURNEY_PEOPLE ? JOURNEY_PEOPLE : n;
        memcpy(j->people, people, sizeof(int) * (size_t)j->n);
        j->site = site;
        j->x0 = x0, j->z0 = z0, j->x1 = x1, j->z1 = z1;
        j->mode = mode;
        j->knowledge = knowledge;
        j->eta = journey_eta(sqrtf((x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0)), mode);
        return i;
    }
    return -1;
}
