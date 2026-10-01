// Generador pseudoaleatorio determinista (xorshift32).
// Misma semilla => misma partida: imprescindible para tests y para
// reproducir errores de la simulacion.
#ifndef ESTEPA_RNG_H
#define ESTEPA_RNG_H

#include <stdint.h>

typedef struct {
    uint32_t state;
} Rng;

static inline void rng_seed(Rng *r, uint32_t seed) { r->state = seed ? seed : 0x9E3779B9u; }

static inline uint32_t rng_next(Rng *r) {
    uint32_t x = r->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return r->state = x;
}

// Flotante uniforme en [0, 1).
static inline float rng_float(Rng *r) { return (rng_next(r) >> 8) * (1.0f / 16777216.0f); }

// Entero uniforme en [0, n). n debe ser > 0.
static inline int rng_range(Rng *r, int n) { return (int)(rng_next(r) % (uint32_t)n); }

#endif
