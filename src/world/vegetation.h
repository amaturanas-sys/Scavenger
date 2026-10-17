// Sistema de vegetación procedimental para SCAVENGERS THRIVE
// Maneja pasto, maleza y follaje con instanced rendering, LOD y shaders de viento.
// Optimizado para Android: baja resolución, instanciación masiva, rendimiento constante.
#ifndef ESTEPA_VEGETATION_H
#define ESTEPA_VEGETATION_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "sim/clock.h"

// Shader uniform locations for wind effect
typedef struct {
    int time;
    int use_wind;
    int color;
} VegetationShaderUniforms;

// Configuración del sistema de vegetación
#define VEGETATION_MAX_INSTANCES 5000
#define VEGETATION_CROSS_QUAD_VERTICES 4
#define VEGETATION_LOD_NEAR 30.0f
#define VEGETATION_LOD_FAR 80.0f

// Tipos de vegetación
typedef enum {
    VEGETATION_GRASS,
    VEGETATION_WEED,
    VEGETATION_FOLIAGE,
    VEGETATION_COUNT
} VegetationType;

// Tipos de biomas
typedef enum {
    BIOME_STEPPE,
    BIOME_FOREST,
    BIOME_PLATEAU,
    BIOME_COUNT
} Biome;

// Datos de una instancia de vegetación
typedef struct {
    Vector3 position;
    float scale;
    float rotation_y;  // Rotación alrededor del eje Y (grados)
    float wind_phase;  // Fase para animación de viento
    Color color;       // Color con tinte estacional
    int type;          // Tipo de vegetación
    float height;      // Altura para LOD
} VegetationInstance;

// Datos del shader de viento
typedef struct {
    Shader shader;
    VegetationShaderUniforms uniforms;
} VegetationShader;

// Estado del sistema de vegetación
typedef struct {
    Mesh mesh;                    // Malla base (cross-quad)
    VegetationInstance instances[VEGETATION_MAX_INSTANCES];
    int instance_count;
    VegetationShader shader;
    bool initialized;
    bool enabled;
    int seed;                     // Semilla para generación procedimental
    float wind_speed;             // Velocidad del viento (0.0 a 1.0)
} VegetationSystem;

// Inicialización y limpieza
void vegetation_init(VegetationSystem *vs, int seed);
void vegetation_unload(VegetationSystem *vs);

// Generación procedimental
void vegetation_populate(VegetationSystem *vs, const Vector3 *center, float radius,
                          int count, VegetationType type, int seed);
void vegetation_populate_area(VegetationSystem *vs, const BoundingBox *area,
                               int count, VegetationType type, int seed);

// Renderizado con LOD y shaders
void vegetation_draw(VegetationSystem *vs, Camera camera, float time,
                     float wind_intensity, Color tint_override);
void vegetation_draw_all(VegetationSystem *vs, Camera camera, float time,
                          float wind_intensity, Color tint_override);

// Estaciones y biomas
// Note: These enums are already defined in src/sim/clock.h
// Removed duplicates to avoid compilation errors.

Color vegetation_get_season_color(Season season, VegetationType type);
Color vegetation_get_biome_color(Biome biome, VegetationType type);

// Actualización de parámetros dinámicos
void vegetation_set_wind_speed(VegetationSystem *vs, float speed);
void vegetation_set_season_tint(VegetationSystem *vs, Season season);
void vegetation_set_biome(VegetationSystem *vs, Biome biome);

// Funciones de utilidad
static inline float vegetation_lerp(float a, float b, float t) {
    return a + t * (b - a);
}

static inline Color vegetation_color_lerp(Color a, Color b, float t) {
    return (Color){
        (unsigned char)vegetation_lerp(a.r, b.r, t),
        (unsigned char)vegetation_lerp(a.g, b.g, t),
        (unsigned char)vegetation_lerp(a.b, b.b, t),
        a.a
    };
}

#endif