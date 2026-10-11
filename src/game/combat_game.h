// Combate, salud y heridas en el juego. Conecta src/sim/health.h y
// src/sim/combat.h con el jugador, la tribu, los enemigos y la interfaz:
//  - salud del jugador: vida, sangre y heridas; abatido, lo levanta la escolta
//    o despierta en el campamento;
//  - cuerpo a cuerpo (src/sim/melee.h) con cuatro botones: H ataque (combo;
//    mantener: pesado), J bloqueo (escudo, o el arma, que cansa), K parry en la
//    ventana justa del golpe que anuncia el rival (engancha, hace la llave o
//    desvia, segun el arma), L carga con escudo o patada; J + H golpe de escudo;
//    los enemigos y la escolta usan las mismas reglas;
//  - objetivo: Tab (en combate) pasa al enemigo siguiente, un toque o un clic lo
//    elige; anillo bajo los pies y su barra de vida resaltada; se pierde a 25 m;
//  - armas a distancia: mantener H (o clic) para tensar y soltar para disparar;
//    con objetivo, la mira calcula la caida sola; sin el, la camara alza o baja
//    la mira y se ve la curva que hara el proyectil; J baja el arma;
//    Mayus+L junto a un fuego enciende la flecha: quema al que alcanza y prende el
//    pasto, los arboles y las estructuras donde cae (la lluvia la apaga);
//  - armadura por piezas en todos los humanos (src/sim/armor.h);
//  - cuerpo humano articulado por zonas (src/sim/body.h): los proyectiles
//    impactan en la zona que tocan;
//  - vendar (B) con hierbas curativas: a uno mismo o a un companero cercano;
//    el curandero del campamento atiende al jugador cuando esta cerca;
//  - enemigos (bandidos, arqueros, fanaticos y captores del culto) con su salud,
//    deteccion, persecucion, huida y heridas; la escolta pelea a tu lado;
//  - salud de todos los integrantes de la tribu: sangran, sanan, mueren;
//  - golpes y flechas tambien contra los animales (src/game/fauna_game.c);
//  - combate montado: a caballo solo golpe y golpe pesado, con mas alcance y la
//    inercia del galope (la lanza derriba); al galope se arrolla a quien este
//    delante; los golpes enemigos a veces dan a la montura y un derribo te tira;
//    a distancia, la velocidad dispersa el tiro (la monta lo corrige);
//  - jinetes bandidos: derribados pierden el caballo (queda suelto);
//  - abatidos: lo que tumba sin herida letal (mazas, patadas, escudos) deja al enemigo en el
//    suelo; despierta a los 30-90 s y huye a avisar a los suyos. F lo toma prisionero; H o K lo
//    rematan (a la tribu le pesa segun sus rasgos);
//  - la espalda (src/sim/stealth.h): por detras y sin que te note (o aturdido), una daga sobre
//    el enemigo: K lo ejecuta en silencio, G lo toma de rehen (escudo humano);
//  - botin: los enemigos muertos dejan una bolsa (F) con lo suyo.
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

// Los estados crecen al final (se guardan como numeros, docs/PARTIDAS.md).
typedef enum {
    EN_WANDER,
    EN_CHASE,
    EN_FLEE,
    EN_DEAD,
    EN_DOWN,    // abatido: en el suelo, vivo; despierta a los 30-90 s y huye (src/sim/stealth.h)
    EN_HOSTAGE, // rehen del jugador: sujeto por el cuello, delante de el
} EnemyState;

typedef struct {
    bool used;
    EnemyKind kind;
    Vector3 pos, home, wander_to;
    float yaw, speed;
    Health h;
    EnemyState state;
    float timer, cooldown, attack_anim, hit_anim, corpse, shown, reload; // timer: abatido, lo que falta para despertar
    int target; // 0 jugador, >0 id del integrante, -1 nadie
    Armor armor;
    // Cuerpo a cuerpo (src/sim/melee.h).
    bool shield, armed;       // lleva escudo; tiene su arma (se la pueden arrancar)
    bool blocking;
    float block_timer, stagger, knock; // cubriendose; desequilibrado; en el suelo
    int move, combo;          // movimiento en curso (MeleeMove + 1) y paso del combo
    float move_anim;
    bool mounted;      // a caballo (jinete): derribado, pierde el caballo
    bool loot_dropped; // ya dejo su botin al caer
    bool awed;         // ya tiro el escarmiento al ver a su objetivo (pieles de depredador)
    float windup;      // s que le quedan al golpe que anuncia (levanta el arma); 0: ninguno
    int windup_move;   // el golpe anunciado (MeleeMove + 1)
    float alert;       // s que busca al jugador aunque no lo vea (lo avisaron: un abatido que huyo, un grito)
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
    int ammo_shown;       // municion a mano del arma a distancia (para el HUD)
    bool arrow_lit;       // la proxima flecha sale encendida (L junto a un fuego)
    float arrow_lit_timer;
    // Cuerpo a cuerpo del jugador (src/sim/melee.h).
    float v_hold;         // s con V apretada (golpe pesado)
    float combo_timer;    // ventana para encadenar el combo
    int move;             // movimiento en curso (MeleeMove + 1)
    float move_anim;
    float stagger, knock; // desequilibrado; derribado
    float charge_timer;   // carga con escudo en curso
    bool charge_hit;
    // Combate montado.
    Vector3 last_pos;     // para la velocidad real del jugador
    float pl_speed;       // m/s
    float trample_cd;     // arrollar al galope
    Vector3 loose_horse[4]; // caballos de jinetes derribados (los suelta la fauna)
    float loose_yaw[4];
    int loose_n;
    // Los cuatro botones.
    int target_lock;   // enemigo elegido + 1 (0: ninguno)
    float parry_cd;    // espera hasta el proximo parry
    float exposed;     // s expuesto tras un parry a destiempo (ni se cubre ni ataca)
    // La espalda y los rehenes (fase 2).
    int hostage;       // enemigo tomado de rehen + 1 (0: ninguno)
} Combat;

void cb_init(Combat *cb, unsigned seed);
// Un clic o un toque sobre un enemigo lo elige como objetivo (y ese clic no es un golpe).
// pointer: en la pantalla virtual de w x h. true si eligio a alguien.
bool cb_pick_target(Combat *cb, Camera3D cam, Vector2 pointer, int w, int h, char *log, size_t len);
// Tab: el objetivo siguiente (dir 1) o el anterior (-1), en orden alrededor del jugador.
void cb_cycle_target(Combat *cb, const Player *p, int dir, char *log, size_t len);
// El enemigo elegido (o NULL).
const Enemy *cb_target(const Combat *cb);
// Hay un golpe enemigo en la ventana del parry (la casilla destella).
bool cb_parry_cue(const Combat *cb);
// Los cuatro botones de combate, abajo a la derecha (ataque, bloqueo, parry, carga o patada):
// dibuja y atiende los toques y clics, que valen como H J K L en el proximo cb_update.
void cb_draw_buttons(const Combat *cb, const GameActions *ga, int w, int h);
bool cb_blocks_input(const Combat *cb); // abatido
float cb_speed_scale(const Combat *cb);
// input_ok: el jugador puede atacar/vendar (sin menu ni minijuego). night/winter: cambian lo que aparece.
// cam_yaw / cam_pitch: la camara (la mira de las armas a distancia).
void cb_update(Combat *cb, Player *p, GameActions *ga, Troop *troop, Props *props, const Terrain *t, Vector3 camp_fire,
               bool input_ok, bool night, bool winter, float cam_yaw, float cam_pitch, float dt, char *log, size_t log_len);
// Cada amanecer: el jugador descansa como la tribu (con curandero, mejor) y se reparan las armaduras.
void cb_new_day(Combat *cb, Troop *troop);
// Enemigos, proyectiles, la curva de la mira y el cuerpo del jugador (sin modelo). Dentro de BeginMode3D.
void cb_draw_world(const Combat *cb, Props *props, const GameActions *ga, const Terrain *t, const Player *p,
                   bool player_has_model, float time);
// Barras de vida sobre enemigos y companeros heridos. Fuera de BeginMode3D.
void cb_draw_overlay(const Combat *cb, const GameActions *ga, const Troop *troop, Camera3D cam, int w, int h);
// Vida del jugador (bajo el calor), panel de heridas (P) y aviso de abatido.
// Capturar con F a un abatido (src/game/actions_game.c decide si F va a el o a otra cosa):
// la distancia al abatido mas cercano en el ultimo cuadro, y el pedido.
#define CB_DOWNED_REACH 1.8f // m: F lo toma prisionero; H o K lo rematan
float cb_capture_dist(void);
void cb_request_capture(void);
void cb_request_bandage(void);     // como pulsar B (desde el HUD)
void cb_request_light_arrow(void); // como Mayus+L (desde la columna del HUD)
void cb_draw_hud(const Combat *cb, const GameActions *ga, const Troop *troop, int right_x, int y, int w, int h);
// Prueba: hace aparecer enemigos cerca del jugador ("bandidos", "culto", "arqueros"),
// o fieras por nombre ("lobos", "tigre", "jabali"...).
void cb_spawn_group(Combat *cb, GameActions *ga, const char *what, const Player *p, const Terrain *t, float dist, char *log,
                    size_t len);
// Un animal ataca a una persona: kind 0 el jugador, 1 el integrante id, 2 el enemigo id (indice).
// Un caballo bajo un jinete (enemigos a caballo, la escolta que parte montada).
void cb_draw_horse(Vector3 pos, float yaw, float speed, float time, bool hit);
void cb_beast_strike(Combat *cb, Player *p, GameActions *ga, Troop *troop, int kind, int id, Vector3 from, float dmg,
                     WoundKind wound, float venom, const char *who, char *log, size_t len);

#endif
