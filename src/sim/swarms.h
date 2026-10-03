// Enjambres y bancos (C puro): peces en los lagos, abejas y avispas en sus nidos,
// mosquitos junto al agua y moscas sobre los cadaveres. Cada enjambre es un
// centro que se mueve (deambula, persigue o huye) y unos cuantos individuos que
// lo rodean con reglas de bandada (cohesion, separacion y un poco de azar).
//  - peces: nadan bajo la superficie, huyen de quien se acerca; se pescan;
//  - abejas: tranquilas salvo que molestes la colmena; el humo de una antorcha
//    las calma (y deja tomar la miel); bajo el agua te pierden;
//  - avispas: se enfadan con solo acercarte a su nido; pican mas y envenenan;
//  - mosquitos: con calor, al atardecer y de noche, junto al agua; el fuego los espanta;
//  - moscas: zumban sobre los cadaveres; no hacen daño.
#ifndef ESTEPA_SWARMS_H
#define ESTEPA_SWARMS_H

#include <stdbool.h>

#include "rng.h"

typedef enum { SWARM_FISH, SWARM_BEES, SWARM_WASPS, SWARM_MOSQUITOES, SWARM_FLIES, SWARM_KIND_COUNT } SwarmKind;

#define SWARM_MEMBERS 14

typedef struct {
    const char *name;  // UTF-8
    const char *model; // id del inventario
    const char *nest;  // id del nido (colmena, avispero) o NULL
    float radius;      // m: lo que se separan del centro
    float speed;       // m/s del enjambre al perseguir o huir
    float sting;       // daño por picadura
    float venom;       // veneno por picadura
    float sting_cd;    // s entre picaduras del enjambre
    float provoke;     // m: acercarse tanto al nido lo enfada (0: nunca)
    float chase;       // s que dura el enfado
    float leash;       // m: no persigue mas lejos de su nido
    int members;
} SwarmDef;

typedef struct {
    float x, y, z, vx, vy, vz;
    bool alive;
} SwarmBody;

typedef struct {
    bool used;
    SwarmKind kind;
    float hx, hy, hz;   // nido o sitio (y: el suelo, o la superficie del agua para los peces)
    float cx, cy, cz;   // centro actual
    float tx, tz, timer;
    float anger;        // s que le quedan de enfado
    float calm;         // s calmado por el humo
    float cooldown;
    bool active;        // visible y despierto (los mosquitos de dia, las abejas de noche: no)
    bool honey_taken;   // la miel de hoy ya se tomo
    int n;
    SwarmBody b[SWARM_MEMBERS];
    int cell_x, cell_z; // celda del mundo donde nacio (para no repetirlo)
} Swarm;

typedef struct {
    float px, py, pz;  // jugador
    bool player_down;
    bool torch;        // antorcha encendida (humo y fuego)
    bool submerged;    // el jugador esta metido en agua honda
    float temp;        // °C
    bool night, dusk;  // de noche; al atardecer
    // Profundidad del agua en (x, z) (<= 0 en seco); NULL: no hay agua.
    float (*water_depth)(void *ud, float x, float z);
    void *water_ud;
} SwarmCtx;

typedef struct {
    int stings;
    float damage, venom;
} SwarmHit;

const SwarmDef *swarm_def(SwarmKind k);
void swarm_init(Swarm *s, SwarmKind k, float x, float y, float z, Rng *rng);
// Un paso: devuelve las picaduras que recibio el jugador.
SwarmHit swarm_update(Swarm *s, const SwarmCtx *c, Rng *rng, float dt);
// Molestaron el nido (un golpe, una flecha): se enfadan.
void swarm_provoke(Swarm *s);
// Calmar con humo (antorcha): no pican un rato.
void swarm_smoke(Swarm *s);
// Pescar: quita los peces a menos de r de (x, y, z); devuelve cuantos.
int swarm_catch(Swarm *s, float x, float y, float z, float r, int max);
int swarm_alive(const Swarm *s);

#endif
