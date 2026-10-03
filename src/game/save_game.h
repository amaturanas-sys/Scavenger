// Partidas guardadas: tres huecos, cada uno con su minifoto y la fecha en que se guardo.
// El archivo es binario y versionado: si el formato del juego cambia (otra version),
// la partida no se carga y se avisa en vez de leer basura.
#ifndef ESTEPA_SAVE_GAME_H
#define ESTEPA_SAVE_GAME_H

#include <stdbool.h>
#include <stddef.h>

#include "game/actions_game.h"
#include "game/combat_game.h"
#include "game/disasters_game.h"
#include "game/hazards_game.h"
#include "game/player.h"
#include "raylib.h"
#include "sim/memory_map.h"
#include "sim/troop.h"

#define SAVE_SLOTS 3
#define SAVE_THUMB_W 128
#define SAVE_THUMB_H 72

// Todo lo que hace falta para seguir la partida.
typedef struct {
    float *world_time;
    int *day;
    int *last_champion;
    float *cam_yaw;
    Rng *rng;
    Player *player;
    Kingdom *overlord;
    Troop *troop;
    GameActions *ga;
    Props *props;
    Combat *cb;
    Hazards *hz;
    Disasters *dz;
    MemoryMap *mem;
    const Inventory *inv;
} GameState;

typedef struct {
    bool used;
    bool compatible;     // misma version del formato
    long long saved_at;  // fecha real (segundos desde 1970)
    int day;             // dia de juego
    int tribe;           // integrantes activos
    char place[48];      // estacion y momento del dia
    Texture2D thumb;     // minifoto (id 0 si no hay)
} SaveInfo;

const char *save_path(int slot, bool thumb);
// Guarda el estado en el hueco (y la minifoto). false con el motivo en err.
bool save_write(int slot, const GameState *g, Image thumb, char *err, size_t len);
// Carga el hueco sobre el estado. false (sin tocar el estado) con el motivo en err.
bool save_read(int slot, GameState *g, char *err, size_t len);
// Lee los encabezados y las minifotos de los huecos (llamar a save_list_unload despues).
void save_list(SaveInfo out[SAVE_SLOTS]);
void save_list_unload(SaveInfo out[SAVE_SLOTS]);
// "3 oct 2026, 19:30"
const char *save_date_text(long long when);

#endif
