#include "clock.h"
#include "sim/lang.h"

#include <math.h>

#define PI_F 3.14159265f

float clock_seconds_into_day(float t) {
    if (t < 0.0f) t = 0.0f;
    float x = fmodf(t, GAME_SECONDS_PER_DAY);
    return x < 0.0f ? 0.0f : x;
}

static int day_of_year(int day) { // 0..DAYS_PER_YEAR-1
    int d = (day - 1) % DAYS_PER_YEAR;
    return d < 0 ? d + DAYS_PER_YEAR : d;
}

Season clock_season(int day) { return (Season)(day_of_year(day) / DAYS_PER_SEASON); }

int clock_day_of_season(int day) { return day_of_year(day) % DAYS_PER_SEASON + 1; }

const char *season_name(Season s) {
    static const char *names[SEASON_COUNT] = { N_("primavera"), N_("verano"), N_("otoño"), N_("invierno") };
    return (unsigned)s < SEASON_COUNT ? T(names[s]) : "?";
}

float clock_daylight_fraction(int day) {
    // Posicion en el ano, a mitad del dia: 0 al empezar la primavera.
    float s = ((float)day_of_year(day) + 0.5f) / (float)DAYS_PER_YEAR;
    // Maximo a mitad del verano (1.5 estaciones = 0.375 del ano), minimo a mitad del invierno.
    const float mid = 0.5f * (DAYLIGHT_MIN + DAYLIGHT_MAX), amp = 0.5f * (DAYLIGHT_MAX - DAYLIGHT_MIN);
    return mid + amp * cosf(2.0f * PI_F * (s - 0.375f));
}

float clock_daylight_seconds(int day) { return clock_daylight_fraction(day) * GAME_SECONDS_PER_DAY; }

float clock_night_seconds(int day) { return GAME_SECONDS_PER_DAY - clock_daylight_seconds(day); }

// Distancia con signo al horizonte mas cercano: positiva de dia, negativa de noche.
// sunrise: true si ese horizonte es el amanecer.
static float horizon_distance(float t, int *sunrise) {
    float x = clock_seconds_into_day(t), d = clock_daylight_seconds(clock_day(t));
    if (x < d) {
        *sunrise = x < 0.5f * d;
        return *sunrise ? x : d - x;
    }
    float after_dusk = x - d, before_dawn = GAME_SECONDS_PER_DAY - x;
    *sunrise = before_dawn < after_dusk;
    return -(*sunrise ? before_dawn : after_dusk);
}

DayPhase clock_phase(float t) {
    int sunrise;
    float h = horizon_distance(t, &sunrise);
    if (h > 0.5f * TWILIGHT_SECONDS) return PHASE_DAY;
    if (h < -0.5f * TWILIGHT_SECONDS) return PHASE_NIGHT;
    return sunrise ? PHASE_DAWN : PHASE_DUSK;
}

const char *phase_name(DayPhase p) {
    static const char *names[] = { N_("alba"), N_("día"), N_("ocaso"), N_("noche") };
    return (unsigned)p <= PHASE_NIGHT ? T(names[p]) : "?";
}

float clock_sun_height(float t) {
    float x = clock_seconds_into_day(t), d = clock_daylight_seconds(clock_day(t));
    if (x < d) return sinf(PI_F * x / d);
    return -sinf(PI_F * (x - d) / (GAME_SECONDS_PER_DAY - d));
}

float clock_light(float t) {
    int sunrise;
    float h = horizon_distance(t, &sunrise);
    float u = h / TWILIGHT_SECONDS + 0.5f; // 0..1 a lo largo de la penumbra
    if (u <= 0.0f) return 0.0f;
    if (u >= 1.0f) return 1.0f;
    return u * u * (3.0f - 2.0f * u);
}

float clock_phase_progress(float t) {
    float x = clock_seconds_into_day(t), d = clock_daylight_seconds(clock_day(t));
    return x < d ? x / d : (x - d) / (GAME_SECONDS_PER_DAY - d);
}
