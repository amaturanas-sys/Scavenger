// Las vistas de la camara, que N alterna: cerca, como en los juegos de mundo abierto en tercera
// persona (detras del personaje, a la altura de su cabeza: se lo ve de la cabeza a las rodillas, al
// centro y abajo, y adelante el paisaje), y dos mas lejos para ver alrededor. C puro: src/main.c
// mueve la camara con estas medidas y los tests miden el encuadre (donde cae cada altura del cuerpo).
#ifndef ESTEPA_CAMVIEW_H
#define ESTEPA_CAMVIEW_H

typedef enum { CAMVIEW_NEAR, CAMVIEW_MID, CAMVIEW_FAR, CAMVIEW_COUNT } CamView;

typedef struct {
    float dist;  // m: de la camara al punto que mira
    float pitch; // rad: cuanto mira hacia abajo
    float lift;  // m: el punto que mira, sobre los ojos del personaje
} CamPreset;

#define CAMVIEW_FOVY 55.0f    // grados: el campo vertical de la camara del juego
#define CAMVIEW_DIST_MIN 1.8f // m: lo mas cerca que la deja la rueda
#define CAMVIEW_DIST_MAX 16.0f
#define CAMVIEW_MOUNTED 1.6f  // m: a caballo la camara se aleja esto (que el caballo entre en cuadro)

const CamPreset *camview_preset(CamView v);
CamView camview_next(CamView v); // N: cerca, media, lejos y otra vez cerca
CamView camview_prev(CamView v); // Mayus+N
const char *camview_name(CamView v); // para la interfaz (N_: pasa por T)
// "cerca", "media", "lejos" (la opcion --vista); -1 si no es ninguna.
int camview_parse(const char *s);
// La inclinacion que va a la mira de las armas a distancia (src/game/combat_game.c): la de la camara
// medida desde la de su vista, como si fuera la vista lejana (con la que la mira tira casi horizontal).
float camview_aim_pitch(CamView v, float pitch);
// Donde cae en la pantalla (0 arriba, 1 abajo; fuera de [0, 1], fuera de cuadro) un punto del eje del
// personaje a y metros de sus pies, con la camara de la vista detras y sus ojos a eye metros.
float camview_screen_y(const CamPreset *c, float eye, float y);

#endif
