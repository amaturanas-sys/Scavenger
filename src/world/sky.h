// Cielo y luz del ciclo dia/noche (src/sim/clock.h).
//
// La escena se dibuja con luz de dia y despues se multiplica por un tinte
// (azul de noche, calido en el alba y el ocaso). Las estrellas y el brillo de
// los fuegos se suman encima, de modo que de noche se ve lo que esta cerca
// de una fogata o de la antorcha.
#ifndef ESTEPA_SKY_H
#define ESTEPA_SKY_H

#include "raylib.h"

#define SKY_STARS 160
#define SKY_MAX_LIGHTS 24

typedef struct {
    Vector3 stars[SKY_STARS]; // direcciones (hemisferio superior)
    float star_bright[SKY_STARS];
} Sky;

void sky_init(Sky *s, unsigned seed);
// Color de fondo (antes de multiplicar por el tinte).
Color sky_clear_color(void);
// Tinte de la escena para el instante t (segundos de juego). Blanco a pleno dia.
Color sky_tint(float t);
// Oscurece la escena ya dibujada (llamar fuera de BeginMode3D).
void sky_apply_tint(float t, int w, int h);
// Estrellas (con prueba de profundidad: el terreno las tapa). Dentro de BeginMode3D.
void sky_draw_stars(const Sky *s, Camera3D cam, float t);
// Brillo de las luces (fuera de BeginMode3D). radius en metros.
void sky_draw_lights(Camera3D cam, const Vector3 *pos, const float *radius, int n, float t, float time, int w,
                     int h);

#endif
