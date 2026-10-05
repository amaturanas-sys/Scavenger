// Minimapa circular tipo brujula, girado segun la direccion de la camara (arriba = hacia donde
// mira el jugador). Muestra la geografia que el jugador recuerda (sim/memory_map): las regiones
// (estepa, bosque, altiplano, fiordos, desierto), el relieve sombreado, rios, lagos y mar, la
// nieve de las cumbres, y los pueblos, tribus y guaridas ya vistos. Lo no recorrido o
// olvidado se ve apagado.
#ifndef ESTEPA_UI_MINIMAP_H
#define ESTEPA_UI_MINIMAP_H

#include "raylib.h"
#include "sim/memory_map.h"
#include "sim/world.h"

// Lo que el minimapa sabe de cada celda del mundo (4 m, como el mapa de memoria): su color
// (region, agua, nieve) y su altura (el relieve se sombrea al dibujar). Se calcula de a poco.
typedef struct {
    int cx, cz;   // celda (clave)
    bool ready;
    float h;
    Color c;
    bool water;
} MinimapCell;

typedef struct {
    int radius;            // px (en la textura de 640x360)
    float meters_per_px;
    Texture2D tex;
    Color *pixels;
    const World *world;    // NULL: solo la memoria (sin geografia)
    float plain;           // altura del llano (la nieve de las cumbres va por encima)
    MinimapCell *cache;    // MINIMAP_CACHE x MINIMAP_CACHE celdas, en anillo
} Minimap;

void minimap_init(Minimap *mm, int radius, float meters_per_px);
void minimap_unload(Minimap *mm);
// El mundo que pinta (otra partida: otro mundo, la cache se vacia).
void minimap_set_world(Minimap *mm, const World *w, float plain);

// center: centro en pantalla. cam_yaw: orientacion de la vista (gira el mapa).
// player_yaw: hacia donde mira el personaje (gira la flecha).
void minimap_draw(Minimap *mm, const MemoryMap *map, Vector2 center, Vector3 player_pos,
                  float cam_yaw, float player_yaw, float now);

#endif
