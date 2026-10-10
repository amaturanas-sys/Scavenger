// Personaje del jugador: caminar, correr, sigilo (C), agacharse (X) y saltar.
//  - sigilo: pasos lentos y silenciosos;
//  - agacharse: mas bajo y lento; tras la hierba alta, una roca o una mata se esconde, y cuesta
//    mas verlo que de pie. El sigilo suma: agachado y en sigilo es lo mas dificil de ver.
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
    bool sneaking;    // sigilo, alternado con C
    bool crouching;   // agachado, alternado con X
    bool moving;
    Stance stance;
    float noise;      // 0..1: cuanto ruido hace (lo escuchara la IA de sigilo)
    float speed_scale; // multiplica la velocidad (montado: la de la montura); 1 por defecto
    float draw_lift;   // m: altura extra al dibujar (montado, sobre el lomo)
} Player;

typedef struct {
    float move_x, move_z; // ejes de movimiento en [-1, 1]
    bool run, toggle_sneak, toggle_crouch, jump;
} PlayerInput;

PlayerInput player_read_input(void);
void player_init(Player *p, const Terrain *t);
// cam_yaw: orientacion de la camara; el movimiento es relativo a ella.
void player_update(Player *p, const Terrain *t, PlayerInput in, float cam_yaw, float dt);
void player_draw(const Player *p);
float player_eye_height(const Player *p);
// Cuanto se lo ve [0, 1] (multiplica la vista de quien lo busca): sigilo, agachado y a cubierto
// (cover: GameActions.hidden, tras la hierba alta, una roca o una mata).
float player_visibility(const Player *p, bool cover);
// Aguante del jugador [0, 1] (correr y saltar lo gastan; no se guarda). winded: agotado, sin correr.
float player_stamina(void);
bool player_winded(void);
void player_stamina_spend(float amount);
const char *stance_name(Stance s);

#endif
