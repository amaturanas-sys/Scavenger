// El sonido del juego (v0.16.0), con raylib: sin musica, el ambiente natural en bucle (el viento, el bosque de dia y
// de noche, la lluvia, la tormenta, el rio, la cascada, el mar, la cueva y el fuego) y los sonidos de lo que pasa (los
// pasos segun el suelo, las aves, el agua, los golpes, el arco, los quejidos). Cuanto suena cada bucle lo deciden las
// reglas de src/sim/sonido.h; aca se mide el mundo alrededor (el agua que corre, las cascadas, el mar, las fogatas, el
// bosque y el tiempo) y se mezcla. Lo que esta cerca suena fuerte y lo de un costado suena de ese lado.
//
// Sin dispositivo de audio (un servidor sin tarjeta de sonido, el CI) todo se calla sin fallar.
#ifndef ESTEPA_AUDIO_GAME_H
#define ESTEPA_AUDIO_GAME_H

#include <stdbool.h>

#include "raylib.h"
#include "sim/climate.h"
#include "sim/sonido.h"
#include "world/terrain.h"

// Los sonidos sueltos (cada uno con sus variantes en assets/sonidos, de tools/sonido/sonidos.py).
typedef enum {
    SND_PASO_TIERRA, // los cuatro pasos, en el orden de Suelo (src/sim/sonido.h)
    SND_PASO_PIEDRA,
    SND_PASO_AGUA,
    SND_PASO_MADERA,
    SND_AVE,
    SND_CUERVO,
    SND_GRAJILLA,
    SND_CHAPOTEO,
    SND_BEBER,
    SND_COMER,
    SND_QUEJIDO,
    SND_TAJO,     // el arma que corta el aire
    SND_IMPACTO,  // el arma que da
    SND_BLOQUEO,  // el escudo o el arma que la para
    SND_PARRY,
    SND_ARCO,     // la cuerda al soltar
    SND_FLECHA,   // la flecha que se clava
    SND_PUNO,
    SND_PATADA,
    SND_MARTILLO, // martillar, forjar, picar piedra
    SND_COUNT
} SoundId;

// Lo que oye el jugador en este cuadro.
typedef struct {
    const Terrain *terrain;
    const Climate *climate;
    float world_time;
    Vector3 pos;   // los pies del jugador: de ahi se oye
    Camera3D cam;  // hacia donde mira la camara: lo de la derecha suena a la derecha
    bool playing;  // en la partida (si no, el titulo: el viento de la estepa)
    bool paused;   // un menu sobre la partida: el ambiente baja y las aves callan
    bool cave;     // adentro de una cueva
    bool moving;   // anda (a pie, en el suelo): pasos
    bool running;
    float noise;   // cuanto ruido hace [0, 1] (Player.noise): en sigilo, los pasos casi no se oyen
    bool mounted;  // a caballo: sin pasos
    bool wading;   // con los pies en el agua
    bool swimming; // nadando: brazadas en vez de pasos
} AuFrame;

void au_init(void); // despues de InitWindow: abre el audio y carga los sonidos (si no hay dispositivo, nada)
void au_unload(void);
void au_set_volume(float v); // el volumen general [0, 1] (src/game/settings.c)
void au_update(const AuFrame *f, float dt);
// Un sonido suelto en un lugar del mundo (baja con la distancia y suena del lado en que esta) o, con at NULL, el del
// propio jugador. vol [0, 1] sobre el de su grupo.
void au_play(SoundId id, const Vector3 *at, float vol);
// El diagnostico de --screenshot: cuanto suena ahora cada bucle, y cuantas veces se pidio cada sonido suelto (aun sin
// dispositivo de audio: se mezcla igual).
void au_levels(float out[AMB_COUNT]);
const char *au_layer_name(int layer);
const char *au_sound_name(int id); // el archivo de su primera variante
int au_played(int id);
void au_near(float *rio, float *cascada, float *mar); // a que distancia oye el agua (m; AMB_LEJOS si no hay)

#endif
