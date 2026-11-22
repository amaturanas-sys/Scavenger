// El sonido del ambiente (v0.16.0): cuanto suena cada bucle segun donde esta quien escucha y que tiempo hace, y el
// suelo de los pasos. Sin musica: el viento, el bosque de dia y de noche, la lluvia, la tormenta, el rio, la cascada,
// el mar, la cueva y el fuego. C puro (las reglas, con sus pruebas); src/game/audio_game.c lo toca con raylib.
#ifndef ESTEPA_SONIDO_H
#define ESTEPA_SONIDO_H

#include <stdbool.h>

typedef enum {
    AMB_VIENTO,
    AMB_BOSQUE_DIA,
    AMB_BOSQUE_NOCHE,
    AMB_LLUVIA,
    AMB_TORMENTA,
    AMB_RIO,
    AMB_CASCADA,
    AMB_MAR,
    AMB_CUEVA,
    AMB_FUEGO,
    AMB_COUNT
} AmbLayer;

#define AMB_LEJOS 1.0e9f // la distancia a lo que no hay cerca

// Donde esta quien escucha y que tiempo hace. Las distancias, en metros.
typedef struct {
    float bosque;   // cuanto bosque hay alrededor [0, 1]
    float abierto;  // cuanto campo abierto [0, 1] (la estepa, el altiplano, el desierto): el viento corre
    float luz;      // la luz del dia [0, 1] (clock_light): de noche, el bosque de noche
    float viento;   // la fuerza del viento [0, 1] (Climate.wind)
    float lluvia;   // [0, 1] (Climate.rain)
    float tormenta; // [0, 1]: los truenos (Climate.storm)
    float ventisca; // [0, 1]: la nevada con viento (la nieve que cae sola es callada)
    float rio;      // al agua que corre mas cercana (rio o canal sin helar)
    float cascada;  // al pie de la cascada mas cercana
    float mar;      // al mar
    float fuego;    // al fuego encendido mas cercano (fogata, hoguera, antorcha)
    bool cueva;     // adentro de una cueva: solo la cueva y el fuego
} AmbIn;

// El volumen de cada bucle [0, 1] (sin el volumen general ni el de cada sonido, que pone el juego).
void amb_mix(const AmbIn *in, float out[AMB_COUNT]);
// Cuanto se oye algo a d metros: 1 hasta `lleno`, 0 desde `nada`, y entre medio una curva suave (sin escalones al
// acercarse). La de los bucles del agua y del fuego, y la de los sonidos sueltos con lugar (un golpe, un ave).
float amb_near(float d, float lleno, float nada);
// Un volumen que sigue a su objetivo sin saltos (tau: en cuantos segundos llega a casi dos tercios del camino).
float amb_follow(float now, float target, float dt, float tau);

// El suelo de los pasos.
typedef enum { SUELO_TIERRA, SUELO_PIEDRA, SUELO_AGUA, SUELO_MADERA, SUELO_COUNT } Suelo;
// Con los pies en el agua, agua; sobre un puente, un muelle o una tarima, madera; sobre una roca o en una cueva,
// piedra; si no, tierra (la estepa, el bosque, el desierto, la nieve).
Suelo amb_suelo(bool en_agua, bool en_madera, bool en_roca);
// Los segundos entre un paso y el siguiente: medio ciclo de la animacion de los cuerpos (caminar dura 1,33 s y correr
// 0,67 s, con dos pasos cada una), asi el paso suena cuando el pie toca el suelo.
float amb_paso_cada(bool corre);

#endif
