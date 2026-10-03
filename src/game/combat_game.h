// Combate, salud y heridas en el juego. Conecta src/sim/health.h y
// src/sim/combat.h con el jugador, la tribu, los enemigos y la interfaz:
//  - salud del jugador: vida, sangre y heridas; abatido, lo levanta la escolta
//    o despierta en el campamento;
//  - golpear (V o clic izquierdo) con el arma empunada; cubrirse (Z);
//  - vendar (B) con hierbas curativas: a uno mismo o a un companero cercano;
//    el curandero del campamento atiende al jugador cuando esta cerca;
//  - enemigos (bandidos, fanaticos y captores del culto, lobos) con su salud,
//    deteccion, persecucion, huida y heridas; la escolta pelea a tu lado;
//  - salud de todos los integrantes de la tribu: sangran, sanan, mueren.
#ifndef ESTEPA_COMBAT_GAME_H
#define ESTEPA_COMBAT_GAME_H

#include <stddef.h>

#include "game/actions_game.h"
#include "game/player.h"
#include "sim/combat.h"
#include "sim/health.h"
#include "sim/troop.h"
#include "world/props.h"
#include "world/terrain.h"

#define CB_MAX_ENEMIES 16

typedef enum { EN_WANDER, EN_CHASE, EN_FLEE, EN_DEAD } EnemyState;

typedef struct {
    bool used;
    EnemyKind kind;
    Vector3 pos, home, wander_to;
    float yaw, speed;
    Health h;
    EnemyState state;
    float timer, cooldown, attack_anim, hit_anim, corpse, shown;
    int target; // 0 jugador, >0 id del integrante, -1 nadie
} Enemy;

typedef struct {
    unsigned seed;
    Rng rng;
    Health player;
    Enemy enemies[CB_MAX_ENEMIES];
    float attack_cd, attack_anim, hit_anim;
    int combo;
    bool blocking;
    float spawn_timer, down_timer, healer_timer;
    float comp_cd[TROOP_MAX];
    float frost_timer;
    bool show_panel;
} Combat;

void cb_init(Combat *cb, unsigned seed);
bool cb_blocks_input(const Combat *cb); // abatido
float cb_speed_scale(const Combat *cb);
// input_ok: el jugador puede atacar/vendar (sin menu ni minijuego). night/winter: cambian lo que aparece.
void cb_update(Combat *cb, Player *p, GameActions *ga, Troop *troop, const Terrain *t, Vector3 camp_fire, bool input_ok,
               bool night, bool winter, float dt, char *log, size_t log_len);
// Cada amanecer: el jugador descansa como la tribu (con curandero, mejor).
void cb_new_day(Combat *cb, const Troop *troop);
// Enemigos (con su animacion o un marcador) y el jugador abatido. Dentro de BeginMode3D.
void cb_draw_world(const Combat *cb, Props *props, const GameActions *ga, const Player *p, bool player_has_model,
                   float time);
// Barras de vida sobre enemigos y companeros heridos. Fuera de BeginMode3D.
void cb_draw_overlay(const Combat *cb, const GameActions *ga, const Troop *troop, Camera3D cam, int w, int h);
// Vida del jugador (bajo el calor), panel de heridas (P) y aviso de abatido.
void cb_draw_hud(const Combat *cb, const GameActions *ga, const Troop *troop, int right_x, int y, int w, int h);
// Prueba: hace aparecer enemigos cerca del jugador ("bandidos", "culto", "lobos").
void cb_spawn_group(Combat *cb, const char *what, const Player *p, const Terrain *t, float dist, char *log, size_t len);

#endif
