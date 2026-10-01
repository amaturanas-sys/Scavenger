// Ruido de valor 2D con fBm, sin dependencias. Se muestrea en coordenadas
// de mundo, asi los chunks del terreno empalman sin costuras.
#ifndef ESTEPA_NOISE_H
#define ESTEPA_NOISE_H

#include <stdint.h>

// Ruido de valor suavizado en [-1, 1].
float noise2d(float x, float y, uint32_t seed);

// Suma de octavas (fBm), normalizada a [-1, 1].
float fbm2d(float x, float y, uint32_t seed, int octaves);

#endif
