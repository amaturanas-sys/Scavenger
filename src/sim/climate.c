#include "climate.h"
#include "sim/lang.h"

#include <math.h>
#include <stddef.h>

#include "clock.h"
#include "rng.h"

#define PI_F 3.14159265f
#define YEAR_SECONDS (GAME_SECONDS_PER_DAY * (float)DAYS_PER_YEAR)

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static float smooth01(float x) {
    x = clamp01(x);
    return x * x * (3.0f - 2.0f * x);
}

float climate_year(float t) {
    if (t < 0.0f) t = 0.0f;
    float y = fmodf(t, YEAR_SECONDS) / YEAR_SECONDS;
    return y < 0.0f ? 0.0f : y;
}

// Estepa continental: veranos calurosos y inviernos muy frios. El calor llega
// un poco despues del dia mas largo (inercia termica).
float climate_temp_mean(float year) { return 3.0f + 21.0f * cosf(2.0f * PI_F * (year - 0.40f)); }

// Categoria de un bloque: 0 despejado, 1 nublado, 2 precipitacion leve, 3 fuerte.
static int base_category(int block, uint32_t seed, float *roll2) {
    Rng r;
    rng_seed(&r, seed ^ ((uint32_t)block * 0x9E3779B1u) ^ 0xC11A7Eu);
    rng_next(&r);
    float roll = rng_float(&r);
    if (roll2) *roll2 = rng_float(&r);
    int day = clock_day(((float)block + 0.5f) * WEATHER_BLOCK_SECONDS);
    // Probabilidades acumuladas por estacion: despejado, nublado, leve (el resto es fuerte).
    static const float table[SEASON_COUNT][3] = {
        { 0.35f, 0.60f, 0.90f }, // primavera: lluvias y ultimas nevadas
        { 0.55f, 0.75f, 0.87f }, // verano: seco, con tormentas
        { 0.30f, 0.60f, 0.88f }, // otono: lluvioso
        { 0.35f, 0.60f, 0.88f }, // invierno: nevadas y ventiscas
    };
    const float *p = table[clock_season(day)];
    return roll < p[0] ? 0 : roll < p[1] ? 1 : roll < p[2] ? 2 : 3;
}

WeatherKind climate_block_weather(int block, uint32_t seed) {
    if (block < 0) block = 0;
    float persist;
    int cat = base_category(block, seed, &persist);
    if (block > 0 && persist < 0.35f) cat = base_category(block - 1, seed, NULL); // el tiempo tiende a durar
    if (cat < 2) return cat == 0 ? WEATHER_CLEAR : WEATHER_CLOUDY;
    float temp = climate_temp_mean(climate_year(((float)block + 0.5f) * WEATHER_BLOCK_SECONDS));
    if (temp < 0.5f) return cat == 2 ? WEATHER_SNOW : WEATHER_BLIZZARD;
    return cat == 2 ? WEATHER_RAIN : WEATHER_STORM;
}

const char *weather_name(WeatherKind w) {
    static const char *names[WEATHER_COUNT] = { N_("despejado"), N_("nublado"), N_("lluvia"), N_("tormenta"), N_("nevada"), N_("ventisca") };
    return (unsigned)w < WEATHER_COUNT ? T(names[w]) : "?";
}

typedef struct {
    float clouds, rain, snow, wind, storm;
} WeatherFx;

static WeatherFx fx_of(WeatherKind w) {
    static const WeatherFx fx[WEATHER_COUNT] = {
        { 0.10f, 0.0f, 0.0f, 0.15f, 0.0f }, // despejado
        { 0.70f, 0.0f, 0.0f, 0.30f, 0.0f }, // nublado
        { 0.85f, 0.5f, 0.0f, 0.35f, 0.0f }, // lluvia
        { 1.00f, 1.0f, 0.0f, 0.80f, 1.0f }, // tormenta
        { 0.85f, 0.0f, 0.5f, 0.30f, 0.0f }, // nevada
        { 1.00f, 0.0f, 1.0f, 1.00f, 0.0f }, // ventisca
    };
    return fx[(unsigned)w < WEATHER_COUNT ? w : 0];
}

static float lerpf(float a, float b, float k) { return a + (b - a) * k; }

Climate climate_at(float t, uint32_t seed) {
    if (t < 0.0f) t = 0.0f;
    Climate c = { 0 };
    c.year = climate_year(t);
    c.temp_mean = climate_temp_mean(c.year);

    // Tiempo ahora: el del bloque, fundido con el siguiente al final.
    int block = (int)(t / WEATHER_BLOCK_SECONDS);
    float into = t - (float)block * WEATHER_BLOCK_SECONDS;
    WeatherKind now = climate_block_weather(block, seed), next = climate_block_weather(block + 1, seed);
    float k = smooth01((into - (WEATHER_BLOCK_SECONDS - WEATHER_BLEND_SECONDS)) / WEATHER_BLEND_SECONDS);
    WeatherFx a = fx_of(now), b = fx_of(next);
    c.weather = k < 0.5f ? now : next;
    c.clouds = lerpf(a.clouds, b.clouds, k);
    c.rain = lerpf(a.rain, b.rain, k);
    c.snow = lerpf(a.snow, b.snow, k);
    c.wind = lerpf(a.wind, b.wind, k);
    c.storm = lerpf(a.storm, b.storm, k);
    c.temperature = c.temp_mean + 5.0f * clock_sun_height(t) * (1.0f - 0.5f * c.clouds);

    // Memoria del suelo: nieve y lluvia de los ultimos dos dias, que se desvanecen.
    float melt = clamp01(1.0f - (c.temp_mean - 2.0f) / 4.0f); // con calor la nieve no dura
    float recent_snow = c.snow, recent_rain = c.rain, rain_sum = 0.0f;
    for (int j = 1; j <= WEATHER_MEMORY_BLOCKS && block - j >= 0; j++) {
        WeatherFx past = fx_of(climate_block_weather(block - j, seed));
        float fade = 1.0f - (float)j / (WEATHER_MEMORY_BLOCKS + 1);
        recent_snow = fmaxf(recent_snow, past.snow * fade * melt);
        recent_rain = fmaxf(recent_rain, past.rain * fade);
        rain_sum += past.rain;
    }
    // Nieve: el invierno la deja todo el tiempo; en primavera y otono, solo tras nevar.
    float season_snow = clamp01((1.0f - c.temp_mean) / 7.0f);
    c.snow_cover = clamp01(season_snow + 0.7f * recent_snow);
    // Deshielo de primavera: el suelo se embarra al pasar de cero grados.
    float thaw = (c.year < 0.3f) ? clamp01(1.0f - fabsf(c.temp_mean - 3.0f) / 6.0f) : 0.0f;
    c.wetness = clamp01(recent_rain + 0.5f * thaw);

    // Vegetacion: verde al final de la primavera, seca al final del verano, ocre en otono.
    float dormant = clamp01((c.temp_mean + 2.0f) / 6.0f); // con helada el pasto duerme
    c.greenness = clamp01(0.5f + 0.6f * cosf(2.0f * PI_F * (c.year - 0.22f)) + 0.15f * c.wetness) * dormant;
    c.autumn = clamp01(1.6f * cosf(2.0f * PI_F * (c.year - 0.66f)) - 0.6f);

    // Lagos: crecida por el deshielo de primavera, minimo al final del verano; la lluvia suma.
    c.water_level = 0.4f + 2.0f * cosf(2.0f * PI_F * (c.year - 0.20f)) + 0.6f * clamp01(rain_sum / 4.0f);
    c.ice = clamp01((-c.temp_mean - 1.0f) / 5.0f);
    // Glaciares: la nieve permanente baja en invierno y sube en verano.
    c.snowline = 22.0f + 0.75f * c.temp_mean;
    return c;
}
