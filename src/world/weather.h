// Efectos del tiempo (src/sim/climate.h): lluvia, nieve, ventisca, relampagos
// y bruma. Las particulas viven en una caja alrededor de la camara, ancladas
// al mundo: al caminar se atraviesa la lluvia en vez de llevarla encima.
#ifndef ESTEPA_WEATHER_H
#define ESTEPA_WEATHER_H

#include "raylib.h"
#include "sim/climate.h"

#define WEATHER_PARTICLES 700

typedef struct {
    Vector3 seed_pos[WEATHER_PARTICLES]; // posicion de partida dentro de la caja
    float phase[WEATHER_PARTICLES];
    float flash;      // brillo del relampago en curso
    float next_flash; // segundos hasta el proximo (en tormenta)
    unsigned rng;
} WeatherFx;

void weather_init(WeatherFx *w, unsigned seed);
// Avanza los relampagos (time: segundos reales).
void weather_update(WeatherFx *w, const Climate *c, float dt);
// Particulas de lluvia o nieve. Dentro de BeginMode3D, despues de lo opaco.
// light en [0, 1]: luz del dia (de noche la lluvia se ve menos).
void weather_draw(const WeatherFx *w, const Climate *c, Camera3D cam, float time, float light);
// Bruma de la precipitacion y destello de los relampagos. Fuera de BeginMode3D.
void weather_draw_screen(const WeatherFx *w, const Climate *c, int width, int height);

#endif
