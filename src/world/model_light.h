// La luz de lo que se mueve o se lleva puesto: los cuerpos, los animales y el equipo. Un shader con la luz del cielo
// y del suelo (hemisferica) y la del sol (direccional), que sigue la pose de cada cuadro (la luz horneada en el color
// quedaba quieta: el brazo que se levantaba seguia con la sombra del reposo). En GLSL 330 y en ES 100 (Android), con su
// respaldo: sin el shader (no compila, u OpenGL 1.1 o 2.1) se ven como antes, con la luz horneada.
#ifndef ESTEPA_MODEL_LIGHT_H
#define ESTEPA_MODEL_LIGHT_H

#include <stdbool.h>

#include "raylib.h"

// Lo compila (despues de InitWindow y antes de cargar los modelos).
void model_light_init(void);
void model_light_unload(void);
bool model_light_ready(void);
// Un modelo recien cargado (src/world/props.c): con el shader, sus materiales lo usan y, si trae la luz horneada en el
// color de sus vertices (los cuerpos y los animales: tools/modelos/humanoide.py y animales.py), se la quita una vez
// (la pone el shader). Sin el shader, no hace nada.
void model_light_apply(Model *m);

#endif
