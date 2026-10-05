// Voxeles (C puro, sin raylib): una rejilla de celdas cubicas con un material (o una densidad)
// por celda, figuras para llenarla, erosion por ruido y un mallador que solo emite las caras
// expuestas (como en la representacion por voxeles de nubes de puntos: lo que esta rodeado no se
// ve), con oclusion ambiental por vertice y sombreado por cara.
//
// Dos usos en el juego:
//  - estructuras (ruinas, yurtas, torres...): materiales con paleta; las ruinas se erosionan
//    con ruido de abajo hacia arriba (lo alto cae primero);
//  - nubes, al modo de Nubis (Guerrilla, Horizon): la densidad sale de un perfil dimensional
//    (gradientes de abajo, de arriba y del borde) erosionado por ruido,
//      densidad = saturar(ruido - (1 - perfil)),
//    y la luz de cada voxel es directa (transmitancia de Beer-Lambert hacia el sol) mas ambiente
//    (pow(perfil invertido, 0.5): entra por arriba y por los bordes). Se hornea en el color.
#ifndef ESTEPA_VOXEL_H
#define ESTEPA_VOXEL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int nx, ny, nz; // celdas por eje (y hacia arriba)
    float size;     // metros por celda
    uint8_t *m;     // material (0 vacio) o densidad (1..255) de cada celda: x + nx * (z + nz * y)
    uint8_t *light; // opcional: luz horneada por celda (0..255), para las nubes
} VoxGrid;

bool vox_init(VoxGrid *g, int nx, int ny, int nz, float size);
void vox_free(VoxGrid *g);
uint8_t vox_get(const VoxGrid *g, int x, int y, int z); // 0 fuera de la rejilla
void vox_set(VoxGrid *g, int x, int y, int z, uint8_t v);
int vox_count(const VoxGrid *g); // celdas llenas

// Figuras (coordenadas en celdas; incluyen los extremos).
void vox_box(VoxGrid *g, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t mat);
// Cilindro o tronco de cono vertical: centro (cx, cz), de y0 a y1, radio r0 abajo y r1 arriba.
void vox_cylinder(VoxGrid *g, float cx, float cz, int y0, int y1, float r0, float r1, uint8_t mat);
// Elipsoide (o media: solo y >= cy si half).
void vox_ellipsoid(VoxGrid *g, float cx, float cy, float cz, float rx, float ry, float rz, uint8_t mat, bool half);
// Vacia una caja (puertas, ventanas, patios).
void vox_carve(VoxGrid *g, int x0, int y0, int z0, int x1, int y1, int z1);
// Ruinas: quita celdas donde ruido < amount * (altura relativa)^0.7 (lo alto cae antes).
void vox_erode(VoxGrid *g, uint32_t seed, float amount);

// Ruido de valores 3D en [0, 1] (suave), para la erosion y las nubes.
float vox_noise3(float x, float y, float z, uint32_t seed);

typedef enum { VCLOUD_CUMULUS, VCLOUD_STRATUS, VCLOUD_STORM, VCLOUD_CAP, VCLOUD_KINDS } VoxCloudKind;
// Llena la rejilla con una nube al modo de Nubis: perfil dimensional (abajo, arriba y borde)
// erosionado por ruido. La densidad queda en m (0 = vacio).
void vox_cloud_shape(VoxGrid *g, VoxCloudKind kind, uint32_t seed);
// Luz horneada de cada celda de la nube (en g->light, que se reserva si falta): transmitancia
// de Beer-Lambert hacia el sol (sx, sy, sz, unitario) por la densidad de la rejilla, mas ambiente.
void vox_cloud_light(VoxGrid *g, float sx, float sy, float sz);

// Malla: triangulos (tris) con posicion, normal y color RGBA por vertice (3 por triangulo).
// El origen es el centro de la base de la rejilla. palette: RGBA de cada material (si light
// existe, se usa la luz en gris azulado en lugar de la paleta).
typedef struct {
    int tris, cap;
    float *pos, *nrm;
    uint8_t *col;
} VoxMesh;
// Cuenta las caras expuestas (para reservar). Dos triangulos por cara.
int vox_exposed_faces(const VoxGrid *g);
// Malla con caras expuestas, oclusion ambiental y sombreado por cara. false si no cupo.
bool vox_mesh(const VoxGrid *g, const uint8_t (*palette)[4], VoxMesh *out);
void vox_mesh_free(VoxMesh *m);

#endif
