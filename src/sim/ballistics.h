// Armas a distancia y trayectoria de proyectiles (C puro).
//
// La potencia del arma (energia en julios) y la masa del proyectil dan la
// velocidad de salida: v = sqrt(2 E / m). En vuelo actuan la gravedad y la
// resistencia del aire (a = -c/m |v| v): un proyectil liviano sale rapido pero
// frena antes; uno pesado conserva la velocidad. Asi cada arma traza su curva
// y cae segun la distancia. El daño sale de la energia que llega al blanco.
#ifndef ESTEPA_BALLISTICS_H
#define ESTEPA_BALLISTICS_H

#include <stdbool.h>

#include "body.h"
#include "health.h"

#define GRAVITY 9.81f

typedef enum { PROJ_ARROW, PROJ_BOLT, PROJ_BALL, PROJ_STONE, PROJ_COUNT } ProjectileKind;

typedef struct {
    const char *name;   // UTF-8
    const char *ammo;   // id del inventario (municion en el acopio)
    float mass;         // kg
    float drag;         // c: coeficiente de resistencia (kg/m)
    float damage_scale; // daño por raiz de julio
    WoundKind wound;
} ProjectileDef;

typedef struct {
    const char *weapon; // id del inventario
    ProjectileKind projectile;
    float power;     // julios a plena tension
    float draw_time; // segundos para tensar del todo (0: ballesta y armas de fuego)
    float reload;    // segundos entre disparos
    float spread;    // desviacion (radianes) del disparo
} RangedDef;

const ProjectileDef *projectile_def(ProjectileKind k);
// Definicion del arma a distancia por id del inventario, o NULL si no dispara.
const RangedDef *ranged_def(const char *weapon_id);

typedef struct {
    V3 pos, vel;
    ProjectileKind kind;
    bool alive;
} Projectile;

// Velocidad de salida (m/s) con una fraccion de tension [0, 1].
float ranged_muzzle_speed(const RangedDef *w, float charge);
void projectile_launch(Projectile *p, ProjectileKind kind, V3 origin, float yaw, float pitch, float speed);
void projectile_step(Projectile *p, float dt);
float projectile_energy(const Projectile *p); // julios
float projectile_damage(const Projectile *p);
// Recorrido para la mira: hasta n puntos cada dt (o hasta caer por debajo de min_y). Devuelve cuantos.
int ballistic_trace(ProjectileKind kind, V3 origin, float yaw, float pitch, float speed, float dt, float min_y,
                    V3 *out, int n);
// Inclinacion (radianes, tiro bajo) para dar en el blanco; false si no llega.
bool ballistic_solve(ProjectileKind kind, V3 origin, V3 target, float speed, float *pitch);

#endif
