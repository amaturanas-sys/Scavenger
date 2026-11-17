// Lo de adentro de las cuevas, de voxeles (docs/PLAN_CUEVAS_GRANDES.md): las estalactitas, las estalagmitas y las
// rocas, y lo que agreguen las tareas del plan (columnas, cofres, fuentes, coladas...). Cada modelo se arma al cargar la
// cueva (vc_preload, desde src/game/cave_scene.c) y se libera al salir (vc_unload): afuera no ocupa nada.
//
// Para agregar uno: su VoxCaveKind al final, su receta en build_kind (src/world/voxcave.c: una rejilla en celdas de
// 0,1 a 0,15 m, con el pie en el centro de la base) y su alto en KIND_HEIGHT. Lo que tapa el paso o se toma sigue
// siendo de src/sim/caves.c; aqui solo se ve.
#ifndef ESTEPA_VOXCAVE_H
#define ESTEPA_VOXCAVE_H

#include <stdbool.h>

#include "raylib.h"
#include "sim/world.h"

typedef enum {
    VC_STALACTITE, VC_STALAGMITE, VC_ROCK,
    VC_STALACTITES, // un racimo colgado de una placa de la boveda
    VC_STALAGMITES, // un racimo sobre su colada
    VC_PILLAR,      // una columna natural (vc_draw_scaled: del suelo a la boveda)
    VC_RUBBLE,      // un derrumbe de bloques y piedras (size: el ancho)
    VC_CRATES,      // cajas y un tonel
    VC_SACKS,       // sacos de grano
    VC_STELE,       // una estela sobre su tortuga
    VC_KINDS
} VoxCaveKind; // crecen al final
#define VC_VARIANTS 4

// La roca de las cuevas de una region (sin el tipo de la cueva).
Color vc_rock_color(Region r);
// La roca de los modelos que se armen desde ahora (la de las paredes de la cueva que se carga: src/game/cave_look.c).
// Se pone antes de los vc_preload de cada cueva; vc_unload la devuelve a la de la region.
void vc_set_rock(Color rock);
// La calcita de las formaciones (hacia que color se aclara la roca) y el musgo sobre lo que quedo arriba de los bloques y
// los cantos (alfa 0: el de siempre, sin musgo), para los modelos que se armen desde ahora; vc_unload lo devuelve.
void vc_set_growth(Color chalk, Color moss);
// Arma el modelo si falta (en la carga de la cueva, nunca en medio del juego). false si no se pudo.
bool vc_preload(VoxCaveKind k, Region r, int variant);
// Lo dibuja con el pie en at (una estalactita: colgada de at), girado yaw (rad), de size m de alto, teñido con la luz
// de la cueva en ese punto. Si no se armo antes, no dibuja nada.
void vc_draw(VoxCaveKind k, Region r, int variant, Vector3 at, float yaw, float size, Color tint);
// Igual, con otra escala de alto: width m de ancho y height m de alto (las columnas, del suelo a la boveda).
void vc_draw_scaled(VoxCaveKind k, Region r, int variant, Vector3 at, float yaw, float width, float height, Color tint);
// Los triangulos de un modelo armado (0 si no esta): para los presupuestos de la carga.
int vc_triangles(VoxCaveKind k, Region r, int variant);
// Los modelos armados ahora y sus triangulos (tris puede ser NULL); vc_largest: los del mas grande.
int vc_loaded(int *tris);
int vc_largest(void);
void vc_unload(void);     // al salir de la cueva
void vc_forget_gpu(void); // ver terrain_forget_gpu

#endif
