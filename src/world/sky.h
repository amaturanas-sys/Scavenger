// Cielo y luz del ciclo dia/noche (src/sim/clock.h).
//
// La escena se dibuja con luz de dia y despues se multiplica por un tinte
// (azul de noche, calido en el alba y el ocaso). Las estrellas y el brillo de
// los fuegos se suman encima, de modo que de noche se ve lo que esta cerca
// de una fogata o de la antorcha.
#ifndef ESTEPA_SKY_H
#define ESTEPA_SKY_H

#include "raylib.h"

#define SKY_STARS 320
#define SKY_LUNAR_DAYS 14   // dias de juego de una luna nueva a la siguiente (dos por año)
#define SKY_DISTANCE 2900.0f // m: el firmamento (dentro del plano lejano, detras del horizonte)
#define SKY_MAX_LIGHTS 24

typedef struct {
    Vector3 stars[SKY_STARS]; // direcciones (toda la esfera: giran alrededor del polo norte)
    float star_bright[SKY_STARS];
} Sky;

void sky_init(Sky *s, unsigned seed);
// Color de fondo (antes de multiplicar por el tinte). clouds en [0, 1]: cielo gris.
// Es el del horizonte: tambien la bruma de lo lejano.
Color sky_clear_color(float clouds);
// El sol sale por el este (+X), pasa por el sur (+Z) a mediodia y se pone por el oeste; de
// noche sigue por debajo. La luna recorre el mismo arco, retrasada segun su fase.
Vector3 sky_sun_dir(float t);
Vector3 sky_moon_dir(float t);
// Fase de la luna en [0, 1): 0 luna nueva, 0.5 llena.
float sky_moon_phase(float t);
// El firmamento de fondo (fuera de BeginMode3D, antes de la escena): degradado del cenit al
// horizonte, el resplandor del alba y del ocaso, y el sol (la escena lo tapa).
void sky_draw_background(Camera3D cam, float t, float clouds, int w, int h);
// La luna (despues del tinte, con prueba de profundidad: el terreno y las nubes la tapan).
void sky_draw_moon(Camera3D cam, float t, float clouds);
// Tinte de la escena para el instante t (segundos de juego). Blanco a pleno dia
// despejado; las nubes lo apagan.
Color sky_tint(float t, float clouds);
// Oscurece la escena ya dibujada (llamar fuera de BeginMode3D).
void sky_apply_tint(float t, float clouds, int w, int h);
// Estrellas (con prueba de profundidad: el terreno las tapa). Giran con la noche alrededor del
// polo norte. Las nubes las ocultan.
void sky_draw_stars(const Sky *s, Camera3D cam, float t, float clouds);
// Brillo de las luces (fuera de BeginMode3D). radius en metros.
void sky_draw_lights(Camera3D cam, const Vector3 *pos, const float *radius, int n, float t, float time, int w,
                     int h);

#endif
