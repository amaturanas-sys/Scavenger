// Plantas deterministas de cuevas: reglas puras, sin raylib ni memoria dinamica.
#ifndef ESTEPA_CAVES_H
#define ESTEPA_CAVES_H

#include <stdbool.h>
#include <stdint.h>

#define CAVE_ROOMS_MAX 10
#define CAVE_CORRIDORS_MAX (CAVE_ROOMS_MAX + 2)

typedef enum {
    CAVE_ANIMAL,
    CAVE_ABANDONED,
    CAVE_BANDITS,
    CAVE_TRIBE,
    CAVE_EMPIRE,
    CAVE_KINDS
} CaveKind;

typedef struct {
    int stalactites, stalagmites, webs, insects, mineral_veins, blood, bones, puddles;
    int nests, eggs, prey, abandoned_weapons, cistern_loot;
    int wall_torches, brick_columns, chests, amphorae, weapon_stores, prisoner_cells;
} CaveRemains;

typedef struct {
    float x, z;       // metros, relativos a la boca
    float floor_y;    // altura relativa: la Cisterna esta en 0
    float radius;
} CaveRoom;

typedef struct {
    int from, to;     // -1 es la boca
    float slope;      // diferencia de altura por metro
    bool one_way;     // la bajada desde la boca no se puede subir
} CaveCorridor;

typedef struct {
    float x, z;       // posicion de la boca en el mundo
    float yaw;        // direccion hacia dentro
    int region;
    CaveKind kind;
    int animal_species;
    int prisoner_count;
    int traitor_risk;
    CaveRemains remains;
    int room_count;
    CaveRoom rooms[CAVE_ROOMS_MAX]; // 0: Cisterna Magna; 1..n: anillo
    CaveCorridor corridors[CAVE_CORRIDORS_MAX];
    int corridor_count;
} Cave;

void cave_generate(Cave *c, uint32_t world_seed, int index, float x, float z, int region, int species);
bool cave_layout_valid(const Cave *c);
bool cave_room_reachable(const Cave *c, int from, int to);

#endif
