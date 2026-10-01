// Reloj de juego: el tiempo se mide en segundos de juego desde el inicio.
#ifndef ESTEPA_CLOCK_H
#define ESTEPA_CLOCK_H

// Duracion de un dia de juego, en segundos de juego (20 minutos reales a 1x).
#define GAME_SECONDS_PER_DAY 1200.0f

// Dia de juego (1, 2, 3...) que corresponde al instante t.
static inline int clock_day(float t) { return 1 + (int)(t / GAME_SECONDS_PER_DAY); }

#endif
