// Minimapa circular tipo brujula: muestra el mapa de memoria (sim/memory_map)
// girado segun la direccion de la camara; arriba = hacia donde mira el jugador.
#ifndef ESTEPA_UI_MINIMAP_H
#define ESTEPA_UI_MINIMAP_H

#include "raylib.h"
#include "sim/memory_map.h"

typedef struct {
    int radius;            // px (en la textura de 640x360)
    float meters_per_px;
    Texture2D tex;
    Color *pixels;
} Minimap;

void minimap_init(Minimap *mm, int radius, float meters_per_px);
void minimap_unload(Minimap *mm);

// center: centro en pantalla. cam_yaw: orientacion de la vista (gira el mapa).
// player_yaw: hacia donde mira el personaje (gira la flecha).
void minimap_draw(Minimap *mm, const MemoryMap *map, Vector2 center, Vector3 player_pos,
                  float cam_yaw, float player_yaw, float now);

#endif
