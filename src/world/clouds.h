// Nubes: una capa a altura fija (sobre el llano del campamento) que el viento arrastra.
// Cada nube es un racimo de bollos achatados; cuantas hay sale de la cobertura del clima.
// Las grandes cumbres del altiplano llegan a la capa y quedan envueltas: ademas, cada cumbre
// alta lleva su gorro de nubes aunque el cielo este despejado. Si la camara entra en una nube,
// la niebla tapa la vista.
#ifndef ESTEPA_CLOUDS_H
#define ESTEPA_CLOUDS_H

#include "raylib.h"
#include "world/terrain.h"

#define CLOUD_ABOVE_PLAIN 80.0f // m: la base de la capa sobre el llano del campamento
#define CLOUD_VIEW 1500.0f      // m: hasta donde se dibujan

// La base de la capa (altura absoluta).
float clouds_base(const Terrain *t);
// Dibuja la capa (dentro de BeginMode3D, antes del tinte: de noche se oscurecen).
// time: segundos de juego (el viento las mueve); cover y wind del clima; haze: el color del
// horizonte (lo lejano se pierde en el).
void clouds_draw(const Terrain *t, Camera3D cam, float time, float cover, float wind, Color haze);
// Cuanto esta la camara dentro de una nube, en [0, 1].
float clouds_mist(const Terrain *t, Vector3 pos, float time, float cover);
// La niebla de estar dentro de una nube (fuera de BeginMode3D, antes del tinte).
void clouds_draw_mist(float mist, int w, int h);
void clouds_unload(void);
void clouds_forget_gpu(void); // ver terrain_forget_gpu

#endif
