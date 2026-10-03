// Clima de la estepa: estaciones, tiempo (lluvia, nieve, tormentas), nieve en
// el suelo, crecidas y hielo de los lagos, y glaciares en las montanas.
//
// Todo es una funcion pura del instante de juego y de la semilla del mundo:
// cualquier sistema (render, IA, guardado) lo consulta sin estado propio y el
// resultado es el mismo en cada maquina.
//
// Las alturas son relativas al llano del campamento (el render suma la altura
// del llano para obtener valores absolutos).
#ifndef ESTEPA_CLIMATE_H
#define ESTEPA_CLIMATE_H

#include <stdint.h>

// El tiempo cambia en bloques de 7.5 minutos (4 por dia), con transicion suave.
#define WEATHER_BLOCK_SECONDS 450.0f
#define WEATHER_BLEND_SECONDS 60.0f
// Bloques hacia atras que recuerdan el suelo (nieve caida, barro): 2 dias.
#define WEATHER_MEMORY_BLOCKS 8

typedef enum {
    WEATHER_CLEAR,    // despejado
    WEATHER_CLOUDY,   // nublado
    WEATHER_RAIN,     // lluvia
    WEATHER_STORM,    // tormenta electrica
    WEATHER_SNOW,     // nevada
    WEATHER_BLIZZARD, // ventisca
    WEATHER_COUNT
} WeatherKind;

typedef struct {
    float year;        // posicion en el ano [0, 1): 0 al empezar la primavera
    float temp_mean;   // temperatura media del dia en la estacion (grados C)
    float temperature; // temperatura ahora (sube de dia, baja de noche)
    // Aspecto del suelo y la vegetacion, en [0, 1].
    float greenness;   // pasto verde (primavera) frente a seco (fin del verano)
    float autumn;      // tonos ocres del otono
    float snow_cover;  // nieve en el llano
    float wetness;     // suelo mojado o embarrado
    // Agua.
    float water_level; // nivel de los lagos respecto de su nivel base (m): crecida de deshielo
    float ice;         // lagos congelados (1 = se puede cruzar)
    // Montanas: por encima de esta altura hay nieve permanente y glaciar.
    float snowline;
    // Tiempo atmosferico ahora.
    WeatherKind weather;
    float clouds; // cobertura de nubes
    float rain;   // intensidad de lluvia
    float snow;   // intensidad de nevada
    float wind;   // fuerza del viento
    float storm;  // probabilidad de relampagos
} Climate;

Climate climate_at(float t, uint32_t seed);
const char *weather_name(WeatherKind w); // UTF-8, en minuscula

// Detalle (expuesto para tests y depuracion).
float climate_year(float t);
float climate_temp_mean(float year);
WeatherKind climate_block_weather(int block, uint32_t seed);

#endif
