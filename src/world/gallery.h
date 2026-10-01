// Galeria del inventario (modo --galeria): coloca cada objeto de
// assets/inventario.tsv en filas por categoria para revisar los modelos
// importados a escala real. Lo que aun no tiene modelo se ve como un
// marcador con las medidas del inventario.
#ifndef ESTEPA_GALLERY_H
#define ESTEPA_GALLERY_H

#include "raylib.h"
#include "sim/inventory.h"
#include "terrain.h"

typedef struct {
    const InvItem *item;
    Vector3 pos;
    bool loaded;
    Model model;
} GalleryEntry;

typedef struct {
    Inventory inv;
    GalleryEntry *entries;
    int count, loaded;
    Vector3 start; // donde conviene poner al jugador
} Gallery;

// origin: esquina de la galeria. Devuelve false si no encuentra el inventario.
bool gallery_init(Gallery *g, const Terrain *t, Vector3 origin);
void gallery_draw(const Gallery *g);
// Etiquetas de los objetos cercanos y ficha del mas cercano (coordenadas de la textura low-res).
void gallery_draw_labels(const Gallery *g, Camera3D cam, Vector3 viewer, int width, int height);
void gallery_unload(Gallery *g);

#endif
