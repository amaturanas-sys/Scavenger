// La vegetacion baja alrededor del jugador (las reglas: src/sim/flora.h): el pasto, la stipa con
// sus plumas plateadas, las flores de la primavera, los helechos del bosque y los juncos de las
// orillas, del color del suelo de su region y su estacion (src/world/terrain.c). Se inclinan con el
// viento en olas que cruzan el pasto, se apartan al pasar y la nieve los tapa (la stipa y los juncos
// secos asoman). Una ventana de celdas de 2 m que sigue al jugador: al moverse solo se siembran las
// celdas nuevas, las cercanas primero. Sin mallas ni texturas: triangulos con color por vertice
// (rlgl), sin memoria por cuadro; en Android, un radio menor.
#ifndef ESTEPA_VEGETATION_H
#define ESTEPA_VEGETATION_H

#include "raylib.h"
#include "sim/climate.h"
#include "sim/fire.h"
#include "world/terrain.h"

// Un claro sin vegetacion: un campamento de la tribu (el pasto pisoteado).
typedef struct {
    float x, z, r;
} VegClearing;

// Siembra las celdas que faltan alrededor de pos (unas pocas por cuadro, las cercanas primero).
void vegetation_update(const Terrain *t, Vector3 pos);
// Las matas cercanas (dentro de BeginMode3D, con lo opaco). time: s (el viento); fire (o NULL): no
// crece en lo quemado ni donde arde; clear: los campamentos; walkers: quienes la apartan al pasar.
void vegetation_draw(const Terrain *t, Camera3D cam, const Climate *c, float time, const FireField *fire, const VegClearing *clear,
                     int nclear, const Vector3 *walkers, int nwalkers);
// Cuantas matas hay sembradas en la ventana y cuantas se dibujaron en el ultimo cuadro.
void vegetation_stats(int *planted, int *drawn);

#endif
