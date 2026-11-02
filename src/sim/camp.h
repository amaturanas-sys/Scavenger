// Generador Nodal de Estructuras (Yurtas y Campamentos)
// Sistema de nodos que genera disposiciones espaciales de estructuras.

#ifndef ESTEPA_CAMP_H
#define ESTEPA_CAMP_H

#include <stdbool.h>
#include <stdint.h>

#include "voxmat.h"

// Tipos de estructuras
typedef enum {
    CAMP_YURT,
    CAMP_CARAVAN,
    CAMP_PALISADE,
    CAMP_FORT,
    CAMP_KINDS
} CampStructType;

// Posición y orientación de un nodo
typedef struct {
    float x, y, z;
    float yaw;
    float pitch;
} CampNodePos;

// Información de un nodo
typedef struct {
    CampNodePos pos;
    CampStructType type;
    uint32_t seed;
    float scale;
} CampNode;

// Campamento generado
typedef struct {
    CampNode nodes[256];
    int node_count;
    float area_min_x, area_min_z;
    float area_max_x, area_max_z;
} Camp;

// Configuración del generador
typedef struct {
    uint32_t seed;
    int yurt_count;
    int caravan_count;
    int palisade_count;
    int fort_count;
    float circle_radius;
    bool circular;
} CampGenConfig;

// Constantes
#define CAMP_MAX_NODES 256
#define CAMP_DEFAULT_RADIUS 50.0f
#define CAMP_PI 3.14159265358979323846f

// Funciones principales
void camp_generate(Camp *camp, const CampGenConfig *config, uint32_t node_seed);
void camp_clear(Camp *camp);

// Acceso a nodos
const CampNode* camp_get_node(const Camp *camp, int index);
int camp_find_nearest(const Camp *camp, float x, float z, float max_dist);

// Cálculos de geometría
float camp_node_distance(const Camp *camp, int i, int j);
bool camp_nodes_overlap(const Camp *camp, int i, int j, float min_spacing);
bool camp_nodes_fit(const Camp *camp, int i, int j, float min_spacing);

// Serialización
typedef struct {
    float x, y, z, yaw, pitch, scale;
    uint8_t type;
    uint32_t seed;
} CampNodeCompact;

void camp_serialize(const Camp *camp, CampNodeCompact *out);
int camp_deserialize(const CampNodeCompact *nodes, int count, Camp *out);

#endif