// Reloj de juego: el tiempo se mide en segundos de juego desde el inicio.
//
// Un ciclo completo (dia + noche) dura 30 minutos reales a 1x. El reparto
// entre luz y oscuridad cambia con la estacion: en pleno invierno la noche
// alcanza 20 minutos y el dia 10; en pleno verano, al reves. En los
// equinoccios (mitad de la primavera y del otono) son 15 y 15.
//
// Cada dia de juego empieza al amanecer: primero la luz, despues la noche.
#ifndef ESTEPA_CLOCK_H
#define ESTEPA_CLOCK_H

// Duracion de un ciclo completo, en segundos de juego (30 minutos reales a 1x).
#define GAME_SECONDS_PER_DAY 1800.0f
// Dias por estacion y por ano.
#define DAYS_PER_SEASON 7
#define DAYS_PER_YEAR (4 * DAYS_PER_SEASON)
// Fraccion del ciclo con luz en los extremos del ano (10 y 20 minutos de 30).
#define DAYLIGHT_MIN (1.0f / 3.0f)
#define DAYLIGHT_MAX (2.0f / 3.0f)
// Duracion del alba y del ocaso (penumbra), en segundos de juego.
#define TWILIGHT_SECONDS 90.0f

typedef enum { SEASON_SPRING, SEASON_SUMMER, SEASON_AUTUMN, SEASON_WINTER, SEASON_COUNT } Season;

typedef enum { PHASE_DAWN, PHASE_DAY, PHASE_DUSK, PHASE_NIGHT } DayPhase;

// Dia de juego (1, 2, 3...) que corresponde al instante t.
static inline int clock_day(float t) { return 1 + (int)(t / GAME_SECONDS_PER_DAY); }
// Segundos transcurridos desde el amanecer del dia en curso.
float clock_seconds_into_day(float t);

// Estacion del dia de juego `day` (el juego empieza el primer dia de la primavera).
Season clock_season(int day);
const char *season_name(Season s); // UTF-8, en minuscula
// Dia dentro de la estacion (1..DAYS_PER_SEASON).
int clock_day_of_season(int day);

// Fraccion del ciclo con luz para ese dia, en [DAYLIGHT_MIN, DAYLIGHT_MAX].
// Varia suavemente: maxima a mitad del verano, minima a mitad del invierno.
float clock_daylight_fraction(int day);
// Segundos de luz y de noche de ese dia (suman GAME_SECONDS_PER_DAY).
float clock_daylight_seconds(int day);
float clock_night_seconds(int day);

DayPhase clock_phase(float t);
const char *phase_name(DayPhase p); // UTF-8, en minuscula
// Altura del sol en [-1, 1]: 0 al amanecer y al ocaso, 1 a mediodia, -1 a medianoche.
float clock_sun_height(float t);
// Luz natural en [0, 1]: 0 en plena noche, 1 de dia, rampa suave en la penumbra.
float clock_light(float t);
// Avance del dia (0 al amanecer, 1 al ocaso) o de la noche (0 al ocaso, 1 al alba).
float clock_phase_progress(float t);
// true de noche (incluye la penumbra del ocaso hasta el alba).
static inline int clock_is_night(float t) { return clock_light(t) < 0.5f; }

#endif
