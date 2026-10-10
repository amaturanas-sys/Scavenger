// Partidas guardadas: tres huecos, cada uno con su minifoto y la fecha en que se guardo.
// El archivo es una lista de bloques, uno por modulo, cada uno con su version y el esquema de
// sus campos (src/sim/save_format.h, docs/PARTIDAS.md): las versiones nuevas del juego cargan
// las partidas viejas aunque los structs hayan cambiado. Las partidas planas de antes (hasta la
// v0.4.3) se siguen leyendo con su esquema congelado (src/game/save_v1.inc).
#ifndef ESTEPA_SAVE_GAME_H
#define ESTEPA_SAVE_GAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

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

// Lo suelto de la partida (bloque PART).
typedef struct {
    int last_champion; // id del ultimo gran guerrero encontrado
    float cam_yaw;
    Rng rng;
} SavedMisc;

// Un objeto del mundo (bloque OBJE): por id, porque los punteros al inventario no se guardan.
typedef struct {
    char id[INV_ID_LEN];
    Vector3 pos;
    float yaw, condition;
} SavedProp;

typedef struct {
    bool used;
    bool compatible;     // este build la puede cargar
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
// Lo mismo con un archivo cualquiera (saved_at: la fecha que va en el encabezado).
bool save_write_file(const char *path, const GameState *g, long long saved_at, char *err, size_t len);
bool save_read_file(const char *path, GameState *g, char *err, size_t len);
// El encabezado de un archivo (sin la minifoto). false si no es una partida.
bool save_peek(const char *path, SaveInfo *out);
// Lee los encabezados y las minifotos de los huecos (llamar a save_list_unload despues).
void save_list(SaveInfo out[SAVE_SLOTS]);
void save_list_unload(SaveInfo out[SAVE_SLOTS]);
// "3 oct 2026, 19:30"
const char *save_date_text(long long when);

// Pruebas (src/game/save_check.c): un resumen legible del estado, y las partidas de referencia
// de un directorio (cada NOMBRE.sav se carga y su resumen tiene que ser NOMBRE.txt; rewrite:
// reescribe los .txt). Devuelve cuantas fallan.
void save_summary(const GameState *g, FILE *out);
int save_selftest(const char *dir, GameState *g, bool rewrite);

#endif
