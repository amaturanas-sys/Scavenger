// Combate, salud y heridas en el juego. Conecta src/sim/health.h y
// src/sim/combat.h con el jugador, la tribu, los enemigos y la interfaz:
//  - salud del jugador: vida, sangre y heridas; abatido, lo levanta la escolta
//    o despierta en el campamento;
//  - golpear (V o clic izquierdo) con el arma empunada; cubrirse (Z);
//  - armas a distancia: mantener V (o clic) para tensar y soltar para disparar;
//    la camara alza o baja la mira y se ve la curva que hara el proyectil;
//    L junto a un fuego enciende la flecha: quema al que alcanza y prende el pasto,
//    los arboles y las estructuras donde cae (la lluvia la apaga);
//  - armadura por piezas en todos los humanos (src/sim/armor.h);
//  - cuerpo humano articulado por zonas (src/sim/body.h): los proyectiles
//    impactan en la zona que tocan;
//  - vendar (B) con hierbas curativas: a uno mismo o a un companero cercano;
//    el curandero del campamento atiende al jugador cuando esta cerca;
//  - enemigos (bandidos, arqueros, fanaticos y captores del culto) con su salud,
//    deteccion, persecucion, huida y heridas; la escolta pelea a tu lado;
//  - salud de todos los integrantes de la tribu: sangran, sanan, mueren;
//  - golpes y flechas tambien contra los animales (src/game/fauna_game.c).
#ifndef ESTEPA_COMBAT_GAME_H
#define ESTEPA_COMBAT_GAME_H

#include <stddef.h>

#include "game/actions_game.h"
#include "game/player.h"
#include "sim/armor.h"
#include "sim/ballistics.h"
#include "sim/combat.h"
#include "sim/health.h"
#include "sim/troop.h"
#include "world/props.h"
#include "world/terrain.h"

#define CB_MAX_ENEMIES 16
#define CB_MAX_SHOTS 48

typedef enum { EN_WANDER, EN_CHASE, EN_FLEE, EN_DEAD } EnemyState;

typedef struct {
    bool used;
    EnemyKind kind;
    Vector3 pos, home, wander_to;
    float yaw, speed;
    Health h;
    EnemyState state;
    float timer, cooldown, attack_anim, hit_anim, corpse, shown, reload;
    int target; // 0 jugador, >0 id del integrante, -1 nadie
    Armor armor;
} Enemy;

typedef struct {
    Projectile p;
    int owner;  // 0 jugador, >0 id del integrante, -(1 + i) enemigo i
    float life; // segundos en vuelo o clavado
    bool stuck; // clavado en el suelo
    V3 dir;     // direccion al clavarse (para dibujarlo)
    bool burning; // flecha encendida: quema y prende lo que toca
} Shot;

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
    Armor armor; // armadura del jugador
    // Armas a distancia del jugador.
    Shot shots[CB_MAX_SHOTS];
    bool aiming;
    float draw, reload, aim_pitch;
    bool arrow_lit;       // la proxima flecha sale encendida (L junto a un fuego)
    float arrow_lit_timer;
} Combat;

void cb_init(Combat *cb, unsigned seed);
bool cb_blocks_input(const Combat *cb); // abatido
float cb_speed_scale(const Combat *cb);
// input_ok: el jugador puede atacar/vendar (sin menu ni minijuego). night/winter: cambian lo que aparece.
// cam_yaw / cam_pitch: la camara (la mira de las armas a distancia).
void cb_update(Combat *cb, Player *p, GameActions *ga, Troop *troop, const Terrain *t, Vector3 camp_fire, bool input_ok,
               bool night, bool winter, float cam_yaw, float cam_pitch, float dt, char *log, size_t log_len);
// Cada amanecer: el jugador descansa como la tribu (con curandero, mejor) y se reparan las armaduras.
void cb_new_day(Combat *cb, Troop *troop);
// Enemigos, proyectiles, la curva de la mira y el cuerpo del jugador (sin modelo). Dentro de BeginMode3D.
void cb_draw_world(const Combat *cb, Props *props, const GameActions *ga, const Terrain *t, const Player *p,
                   bool player_has_model, float time);
// Barras de vida sobre enemigos y companeros heridos. Fuera de BeginMode3D.
void cb_draw_overlay(const Combat *cb, const GameActions *ga, const Troop *troop, Camera3D cam, int w, int h);
// Vida del jugador (bajo el calor), panel de heridas (P) y aviso de abatido.
void cb_draw_hud(const Combat *cb, const GameActions *ga, const Troop *troop, int right_x, int y, int w, int h);
// Prueba: hace aparecer enemigos cerca del jugador ("bandidos", "culto", "arqueros"),
// o fieras por nombre ("lobos", "tigre", "jabali"...).
void cb_spawn_group(Combat *cb, GameActions *ga, const char *what, const Player *p, const Terrain *t, float dist, char *log,
                    size_t len);
// Un animal ataca a una persona: kind 0 el jugador, 1 el integrante id, 2 el enemigo id (indice).
void cb_beast_strike(Combat *cb, Player *p, GameActions *ga, Troop *troop, int kind, int id, Vector3 from, float dmg,
                     WoundKind wound, float venom, const char *who, char *log, size_t len);

#endif
