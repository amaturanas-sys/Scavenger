// Personaje del jugador: caminar, correr, acechar (sigilo) y saltar.
// Trepar, montar y nadar llegan en fases posteriores (ver docs/ROADMAP.md).
#ifndef ESTEPA_PLAYER_H
#define ESTEPA_PLAYER_H

#include <stdbool.h>

#include "../world/terrain.h"
#include "raylib.h"

typedef enum { STANCE_WALK, STANCE_RUN, STANCE_SNEAK } Stance;

typedef struct {
    Vector3 pos;      // pies del personaje
    float yaw;        // hacia donde mira (radianes)
    float vy;         // velocidad vertical
    bool grounded;
    bool sneaking;    // alternado con C / Ctrl
    bool moving;
    Stance stance;
    float noise;      // 0..1: cuanto ruido hace (lo escuchara la IA de sigilo)
    float speed_scale; // multiplica la velocidad (montado: la de la montura); 1 por defecto
    float draw_lift;   // m: altura extra al dibujar (montado, sobre el lomo)
} Player;

typedef struct {
    float move_x, move_z; // ejes de movimiento en [-1, 1]
    bool run, toggle_sneak, jump;
} PlayerInput;

PlayerInput player_read_input(void);
void player_init(Player *p, const Terrain *t);
// cam_yaw: orientacion de la camara; el movimiento es relativo a ella.
void player_update(Player *p, const Terrain *t, PlayerInput in, float cam_yaw, float dt);
void player_draw(const Player *p);
float player_eye_height(const Player *p);
// Aguante del jugador [0, 1] (correr y saltar lo gastan; no se guarda). winded: agotado, sin correr.
float player_stamina(void);
bool player_winded(void);
void player_stamina_spend(float amount);
const char *stance_name(Stance s);

#endif
