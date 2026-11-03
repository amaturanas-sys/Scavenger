#include "terrain.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../sim/city.h"
#include "../sim/hazards.h"
#include "../sim/noise.h"
#include "platform.h"
#include "world/voxstruct.h"
#include "raymath.h"
#include "rlgl.h"

// Por triangulo: altura, pendiente, luz, mancha, y pesos de desierto, bosque, altiplano y
// costa, distancia sobre el agua (para la orilla), que es (suelo, copa o tronco) y cuanto tiene que
// avanzar el glaciar para cubrirlo (world_glacier_reach).
// A_WALL: cuanto es el muro de roca del otro lado del canal del bosque (world_forest_wall, docs/AGUA.md fase 5).
enum { A_H, A_SLOPE, A_LIGHT, A_PATCH, A_DESERT, A_FOREST, A_HIGH, A_FJORD, A_SHORE, A_EDGE, A_BANK, A_KIND, A_GLACIER, A_WALL, TRI_ATTRS };
enum { KIND_GROUND, KIND_CANOPY, KIND_TRUNK, KIND_ROCK, KIND_BUSH };
// Las mallas de agua de cada chunk: el agua (cada vertice con el color de su agua y su hielo), el hielo que
// aguanta (sin el brillo de las ondas) y la espuma (rompientes y rapidos).
enum { W_WATER, W_ICE, W_FOAM };
#define W_MESHES TERRAIN_WATER_MESHES
enum { WC_GLACIAL = 1, WC_RAPIDS = 2 }; // lo de cada celda con agua

// Texturas del suelo (assets/terrain/texturas.png, tools/assets/texturas_terreno.py): un atlas
// low-res de 4x4 celdas; cada celda cubre 8 m. El orden es fijo.
typedef enum {
    TT_TUSSOCK, TT_TALLGRASS, TT_FOREST, TT_TUNDRA, TT_MOSS, TT_SAND, TT_STRATA, TT_GRAVEL,
    TT_SNOW, TT_ROCK, TT_MUD, TT_ICE, TT_BLACKSAND, TT_COBBLES, TT_SERAC, TT_WHITE,
} TerrainTex;
#define TEX_COLS 4
#define TEX_SPAN 8.0f   // m de suelo por celda del atlas
// El detalle oscurece en promedio: el color del suelo se aclara para compensar, segun lo clara
// que sea cada celda (la nieve casi nada: si no, se confunde con el cielo).
static float g_tex_boost[16] = { 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.35f, 1.0f };
static Texture2D g_atlas;
static bool g_atlas_tried;
static float g_perma_snow; // altura de la nieve de verano (las cumbres siempre blancas)

float terrain_height(const Terrain *t, float x, float z) { return world_height(t->world, x, z); }
float terrain_plain_height(const Terrain *t) { return t->world->camp_height; }
Region terrain_region(const Terrain *t, float x, float z) { return world_region(t->world, x, z); }

void terrain_dry_step(const Terrain *t, float x0, float z0, float *x, float *z) {
    const float deep = 0.6f;
    if (!terrain_deep_water(t, *x, *z, deep) || terrain_deep_water(t, x0, z0, deep)) return;
    float dx = *x - x0, dz = *z - z0;
    static const float TURN[8] = { 0.5f, -0.5f, 1.0f, -1.0f, 1.5f, -1.5f, 2.0f, -2.0f };
    for (int k = 0; k < 8; k++) {
        float cs = cosf(TURN[k]), sn = sinf(TURN[k]);
        float nx = x0 + dx * cs - dz * sn, nz = z0 + dx * sn + dz * cs;
        if (!terrain_deep_water(t, nx, nz, deep)) {
            *x = nx, *z = nz;
            return;
        }
    }
    *x = x0, *z = z0;
}

static WaterSeason look_season(const TerrainLook *L) { return (WaterSeason){ L->flood, L->temp, L->temp_late, L->melt, L->sea_ice }; }
WaterSeason terrain_season(const Terrain *t) { return look_season(&t->look); }

WaterStep terrain_water_step(const Terrain *t, float x, float z) {
    WaterSeason s = look_season(&t->look);
    WaterNow n;
    float w = world_water_now(t->world, x, z, &s, &n);
    return hazard_water_step(w - terrain_height(t, x, z), n.ice, WADE_PERSON);
}

float terrain_body_y(const Terrain *t, float x, float z) {
    WaterSeason s = look_season(&t->look);
    WaterNow n;
    float ground = terrain_height(t, x, z), water = world_water_now(t->world, x, z, &s, &n);
    if (water <= ground + WADE_ANKLE) return ground;
    return hazard_ice_walkable(n.ice) ? water : fmaxf(ground, water - 1.25f);
}

void terrain_wade_step(const Terrain *t, float x0, float z0, float *x, float *z) {
    // El agua donde esta frena el paso.
    float k = hazard_water_speed(terrain_water_step(t, x0, z0));
    float dx = (*x - x0) * k, dz = (*z - z0) * k;
    *x = x0 + dx, *z = z0 + dz;
    if (!terrain_deep_water(t, *x, *z, WADE_PERSON) || terrain_deep_water(t, x0, z0, WADE_PERSON)) return; // vadea, o ya nada
    // A lo hondo: un rio se cruza nadando (hay donde hacer pie mas adelante); un lago o el mar, se rodea.
    float l = sqrtf(dx * dx + dz * dz);
    if (l < 1e-4f) return;
    for (float r = 4.0f; r <= TERRAIN_NARROW_WATER; r += 4.0f)
        if (!terrain_deep_water(t, *x + dx / l * r, *z + dz / l * r, WADE_PERSON)) return;
    static const float TURN[8] = { 0.5f, -0.5f, 1.0f, -1.0f, 1.5f, -1.5f, 2.0f, -2.0f };
    for (int i = 0; i < 8; i++) {
        float cs = cosf(TURN[i]), sn = sinf(TURN[i]);
        float nx = x0 + dx * cs - dz * sn, nz = z0 + dx * sn + dz * cs;
        if (!terrain_deep_water(t, nx, nz, WADE_PERSON)) {
            *x = nx, *z = nz;
            return;
        }
    }
    *x = x0, *z = z0;
}

float terrain_water(const Terrain *t, float x, float z) {
    WaterSeason s = look_season(&t->look);
    return world_water_now(t->world, x, z, &s, NULL);
}

float terrain_water_full(const Terrain *t, float x, float z) { return world_water(t->world, x, z, t->look.flood, NULL); }

float terrain_ice(const Terrain *t, float x, float z) {
    WaterSeason s = look_season(&t->look);
    WaterNow n;
    world_water_now(t->world, x, z, &s, &n);
    return n.ice;
}

bool terrain_deep_water(const Terrain *t, float x, float z, float depth) {
    WaterSeason s = look_season(&t->look);
    WaterNow n;
    float w = world_water_now(t->world, x, z, &s, &n);
    return w > terrain_height(t, x, z) + depth && !hazard_ice_walkable(n.ice);
}

static Color mixc(Color a, Color b, float k) {
    k = Clamp(k, 0.0f, 1.0f);
    return (Color){ (unsigned char)Lerp(a.r, b.r, k), (unsigned char)Lerp(a.g, b.g, k), (unsigned char)Lerp(a.b, b.b, k),
                    255 };
}

// Paleta segun la region y la estacion: pasto de estepa (verde, seco, ocre o dormido),
// sotobosque de coniferas, tundra del altiplano, musgo de la costa y arena del desierto;
// tierra y roca en las pendientes; barro con lluvia; orillas; nieve, hielo y glaciares.
// h: altura relativa al llano; patch en [0, 1]: manchas (nieve a medio cubrir).
static Color ground_color(const TerrainLook *L, const float *a, float h) {
    float h_abs = a[A_H], slope = a[A_SLOPE], patch = a[A_PATCH], desert = a[A_DESERT];
    const Color green = { 112, 150, 70, 255 }, dry = { 188, 168, 106, 255 }, ochre = { 172, 124, 68, 255 };
    const Color dormant = { 140, 128, 98, 255 }, dirt = { 140, 110, 76, 255 }, rock = { 120, 116, 108, 255 };
    const Color snow = { 226, 232, 240, 255 }, glacier = { 196, 218, 236, 255 }, mud = { 112, 96, 72, 255 };
    const Color lakebed = { 158, 140, 108, 255 }, needles = { 72, 96, 54, 255 }, tundra = { 132, 136, 104, 255 };
    const Color moss = { 96, 124, 92, 255 }, shingle = { 128, 128, 124, 255 }, moraine = { 146, 138, 124, 255 };
    Color grass = mixc(dry, green, L->greenness);
    grass = mixc(grass, ochre, L->autumn * 0.85f);
    grass = mixc(dormant, grass, 0.35f + 0.65f * Clamp(L->greenness + L->autumn, 0.0f, 1.0f));
    if (h > 6.0f) grass = mixc(grass, dry, 0.45f); // lomas mas secas
    grass = mixc(grass, mixc(needles, ochre, L->autumn * 0.3f), a[A_FOREST]); // el suelo del bosque
    grass = mixc(grass, tundra, a[A_HIGH]);                                    // la tundra del altiplano
    grass = mixc(grass, moss, a[A_FJORD]);                                     // el musgo de la costa
    Color c = grass;
    bool bare = false;
    float steep = 0.45f + 0.15f * a[A_HIGH];
    if (slope > steep) c = dirt, bare = true;
    if (slope > 0.75f || (slope > 0.35f && a[A_HIGH] + a[A_FJORD] > 0.5f && h > 30.0f)) c = rock, bare = true;
    // Orillas: el lecho que el agua deja al bajar, y el barro (o los guijarros del mar) junto al agua.
    float above_water = a[A_SHORE];
    if (above_water > 0.0f && above_water < TERRAIN_LAKE_FLOOD)
        c = above_water < 0.4f ? (a[A_FJORD] > 0.5f ? shingle : mud) : mixc(c, lakebed, 0.7f);
    // Lo que el agua deja ver al bajar o al secarse (docs/AGUA.md, fase 2): la grava de los cauces del
    // altiplano y la costa, y el limo de los lechos.
    else if (above_water <= 0.0f && above_water > -2.5f) c = a[A_HIGH] + a[A_FJORD] > 0.5f ? shingle : mixc(lakebed, mud, 0.5f);
    if (!bare) c = mixc(c, mud, L->wetness * 0.35f * (1.0f - desert)); // suelo mojado (la arena no se embarra)
    // La playa de los fiordos: arena volcanica negra; en los rios, cantos rodados claros sin pasto.
    if (a[A_FJORD] > 0.5f && h_abs < SEA_LEVEL + 4.0f && slope < 0.6f) c = mixc(c, (Color){ 62, 60, 62, 255 }, Clamp((SEA_LEVEL + 4.0f - h_abs) * 0.6f, 0.0f, 1.0f));
    if (a[A_BANK] > 0.0f && desert < 0.5f) c = mixc(c, (Color){ 168, 164, 154, 255 }, Clamp(a[A_BANK] * 1.6f - 0.3f, 0.0f, 1.0f));
    // Desierto: arena dorada con vetas; las mesetas y el muro, roca rojiza en estratos.
    if (desert > 0.0f) {
        const Color sand = { 214, 188, 134, 255 }, sand_dark = { 190, 160, 108, 255 }, redrock = { 178, 104, 70, 255 };
        Color dc = slope > 0.5f ? mixc(redrock, sand_dark, 0.5f + 0.5f * sinf(h_abs * 0.6f)) : mixc(sand, sand_dark, patch * 0.8f);
        c = mixc(c, dc, Clamp(desert * 1.4f, 0.0f, 1.0f));
    }
    // El muro del otro lado del canal del bosque (docs/AGUA.md, fase 5): roca en estratos, grises y ocres que se
    // alternan con la altura; los rellanos, de liquen.
    if (a[A_WALL] > 0.0f) {
        const Color grey = { 120, 116, 110, 255 }, buff = { 150, 128, 98, 255 }, dark = { 88, 84, 80, 255 }, lichen = { 104, 112, 86, 255 };
        float band = sinf(h_abs * 0.42f);
        Color sc = slope > 0.4f ? mixc(mixc(grey, buff, 0.5f + 0.5f * band), dark, Clamp(-band * 1.5f - 0.6f, 0.0f, 1.0f)) : mixc(grey, lichen, 0.6f);
        c = mixc(c, sc, Clamp(a[A_WALL], 0.0f, 1.0f));
    }
    // El muro de hielo del altiplano: hielo azulado, mas en lo empinado.
    if (a[A_HIGH] > 0.5f && a[A_EDGE] < 150.0f) c = mixc(c, (Color){ 176, 206, 232, 255 }, 0.5f + 0.5f * Clamp(slope * 2.0f, 0.0f, 1.0f));
    // Los barrancos del altiplano son permafrost: paredes de hielo todo el año, tambien en verano.
    else if (a[A_HIGH] > 0.5f && slope > 0.6f) c = mixc(c, (Color){ 184, 210, 230, 255 }, 0.75f);
    // La lengua del glaciar: avanza en invierno sobre la morrena (el limo y la grava) que deja al retroceder en
    // verano.
    if (a[A_GLACIER] <= 1.0f) {
        if (!bare) c = mixc(c, moraine, 0.85f);
        c = mixc(c, (Color){ 214, 230, 246, 255 }, (L->glacier - a[A_GLACIER]) / 0.06f + 0.5f);
    }
    // Nieve: cubre el llano segun la estacion (en manchas a medio cubrir); menos en lo empinado y en la arena.
    float cover = L->snow_cover * (slope > 0.75f ? 0.55f : 1.0f) * (1.0f - 0.6f * desert);
    if (patch < cover * 1.15f - 0.05f) c = snow;
    // Glaciares y nieve permanente en las cumbres; una franja de manchas por debajo.
    float above = h_abs - L->snowline;
    if (above > 0.0f) c = slope < 0.7f ? glacier : mixc(rock, snow, 0.5f);
    else if (above > -6.0f && patch < (above + 6.0f) / 6.0f) c = snow;
    return c;
}

// Rocas: gris (rojiza en el desierto), con liquen; nieve encima en invierno y en lo alto.
static Color rock_color(const TerrainLook *L, const float *a) {
    Color c = mixc((Color){ 118, 114, 106, 255 }, (Color){ 142, 138, 126, 255 }, a[A_PATCH]);
    c = mixc(c, (Color){ 168, 104, 72, 255 }, a[A_DESERT]);
    c = mixc(c, (Color){ 104, 116, 92, 255 }, 0.25f * a[A_FJORD]); // liquen de la costa
    float cover = L->snow_cover * (a[A_SLOPE] < 0.5f ? 1.0f : 0.3f);
    if (a[A_H] > L->snowline - 4.0f) cover = fmaxf(cover, 0.8f);
    return mixc(c, (Color){ 226, 232, 240, 255 }, a[A_SLOPE] < 0.4f ? cover : cover * 0.4f);
}

// Matas y arbustos: verdes en primavera, secos en verano, ocres en otoño; flores de la
// estepa en primavera (las matas de mancha alta); nieve encima.
static Color bush_color(const TerrainLook *L, const float *a) {
    Color green = { 86, 120, 58, 255 }, dry = { 150, 134, 84, 255 }, ochre = { 168, 112, 56, 255 };
    Color c = mixc(dry, green, L->greenness);
    c = mixc(c, ochre, L->autumn * 0.8f);
    c = mixc(c, (Color){ 60, 84, 52, 255 }, a[A_FOREST] * 0.6f);  // sotobosque
    c = mixc(c, (Color){ 132, 118, 80, 255 }, a[A_DESERT] * 0.8f); // saxaul
    if (a[A_PATCH] > 0.86f && a[A_FOREST] < 0.5f && a[A_DESERT] < 0.5f && a[A_HIGH] < 0.5f && L->greenness > 0.5f) {
        static const Color FLOWERS[3] = { { 214, 92, 120, 255 }, { 226, 200, 84, 255 }, { 156, 120, 200, 255 } };
        c = mixc(c, FLOWERS[(int)(a[A_PATCH] * 997.0f) % 3], (L->greenness - 0.5f) * 1.6f);
    }
    return mixc(c, (Color){ 222, 228, 236, 255 }, L->snow_cover * 0.85f);
}

static Color tree_color(const TerrainLook *L, const float *a) {
    if (a[A_KIND] == KIND_ROCK) return rock_color(L, a);
    if (a[A_KIND] == KIND_BUSH) return bush_color(L, a);
    if (a[A_KIND] == KIND_TRUNK) return (Color){ 92, 66, 44, 255 };
    Color c = mixc((Color){ 52, 86, 50, 255 }, (Color){ 74, 92, 58, 255 }, a[A_PATCH]);
    // Los alerces (la mitad del bosque) se doran en otoño, como en las laderas del Altai.
    if (a[A_PATCH] > 0.5f) c = mixc(c, (Color){ 196, 150, 60, 255 }, L->autumn * 0.9f);
    float cover = L->snow_cover * 0.8f;
    if (a[A_H] > L->snowline - 4.0f) cover = fmaxf(cover, 0.7f);
    return mixc(c, (Color){ 222, 228, 236, 255 }, cover * (0.4f + 0.6f * a[A_PATCH])); // nieve en las ramas
}

static Color shade(Color c, float light) {
    return (Color){ (unsigned char)fminf(255.0f, c.r * light), (unsigned char)fminf(255.0f, c.g * light), (unsigned char)fminf(255.0f, c.b * light), 255 };
}

// Que textura lleva un triangulo de suelo (fija: la estacion la tiñe con el color).
static TerrainTex ground_tex(const float *a, float slope) {
    float hi = a[A_HIGH], fo = a[A_FOREST], fj = a[A_FJORD], de = a[A_DESERT];
    if (hi > 0.5f && a[A_EDGE] < 150.0f) return slope > 0.45f ? TT_SERAC : TT_ICE; // el frente del glaciar: seracs
    if (hi > 0.5f && slope > 0.6f) return TT_SERAC;                                // barrancos de permafrost
    if (a[A_GLACIER] <= 1.0f && slope < 0.45f) return TT_SNOW;                     // la lengua del glaciar, o el limo que deja al retroceder
    if (a[A_WALL] > 0.5f) return slope > 0.4f ? TT_STRATA : TT_ROCK;             // el muro tras el canal del bosque
    if (fj > 0.5f && a[A_H] < SEA_LEVEL + 4.0f && slope < 0.6f) return TT_BLACKSAND; // la playa de arena volcanica
    if (a[A_BANK] > 0.45f && de < 0.5f && slope < 0.6f) return TT_COBBLES;       // el lecho de cantos que el rio lava
    if (a[A_H] > g_perma_snow && slope < 0.7f) return TT_SNOW;
    if (a[A_SHORE] > 0.0f && a[A_SHORE] < 0.5f) return hi + fj > 0.5f ? TT_GRAVEL : TT_MUD;
    if (de > 0.5f) return slope > 0.45f ? TT_STRATA : TT_SAND;
    if (slope > 0.6f) return TT_ROCK;
    if (hi > 0.5f) return TT_TUNDRA;
    if (fo > 0.5f) return TT_FOREST;
    if (fj > 0.5f) return TT_MOSS;
    return a[A_PATCH] > 0.55f ? TT_TALLGRASS : TT_TUSSOCK;
}

// Pinta los colores del chunk segun el aspecto actual (sin tocar la geometria).
static void paint_chunk(const Terrain *t, Chunk *ch, bool upload) {
    Mesh *mesh = &ch->model.meshes[0];
    const int tris = mesh->triangleCount;
    for (int i = 0; i < tris; i++) {
        const float *a = &ch->tri[i * TRI_ATTRS];
        Color base = a[A_KIND] == KIND_GROUND ? ground_color(&t->look, a, a[A_H] - t->plain) : tree_color(&t->look, a);
        Color col = shade(base, a[A_LIGHT] * (a[A_KIND] == KIND_GROUND ? g_tex_boost[ground_tex(a, a[A_SLOPE])] : 1.0f));
        for (int k = 0; k < 3; k++) {
            unsigned char *dst = &mesh->colors[(i * 3 + k) * 4];
            dst[0] = col.r;
            dst[1] = col.g;
            dst[2] = col.b;
            dst[3] = 255;
        }
    }
    if (upload) UpdateMeshBuffer(*mesh, 3, mesh->colors, mesh->vertexCount * 4, 0);
}

// ------------------------------------------------------------------ construccion
typedef struct {
    float *v, *n, *attr, *uv;
    int count, cap; // triangulos
    float local[6]; // coordenadas (u, v) dentro de la celda del atlas, de los tres vertices
} TriBuf;

static void push_tri(TriBuf *b, Vector3 p0, Vector3 p1, Vector3 p2, const float *attrs) {
    if (b->count >= b->cap) return;
    Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(p1, p0), Vector3Subtract(p2, p0)));
    Vector3 tri[3] = { p0, p1, p2 };
    for (int k = 0; k < 3; k++) {
        int v = b->count * 3 + k;
        b->v[v * 3 + 0] = tri[k].x, b->v[v * 3 + 1] = tri[k].y, b->v[v * 3 + 2] = tri[k].z;
        b->n[v * 3 + 0] = n.x, b->n[v * 3 + 1] = n.y, b->n[v * 3 + 2] = n.z;
    }
    float *at = &b->attr[b->count * TRI_ATTRS];
    memcpy(at, attrs, sizeof(float) * TRI_ATTRS);
    const Vector3 sun = Vector3Normalize((Vector3){ -0.45f, 1.0f, -0.3f });
    at[A_SLOPE] = 1.0f - n.y;
    at[A_LIGHT] = 0.55f + 0.45f * fmaxf(0.0f, Vector3DotProduct(n, sun)); // iluminacion horneada: cero costo en GPU
    // La celda del atlas (los arboles, la blanca) y un margen para no tomar la vecina.
    TerrainTex tt = at[A_KIND] == KIND_GROUND ? ground_tex(at, at[A_SLOPE]) : TT_WHITE;
    const float cw = 1.0f / TEX_COLS, eps = 0.5f / (32.0f * TEX_COLS);
    float u0 = (float)(tt % TEX_COLS) * cw, v0 = (float)(tt / TEX_COLS) * cw;
    // En lo empinado (paredes, muros) la textura va de pie: v sigue la altura, asi los
    // estratos quedan horizontales.
    bool wall = at[A_KIND] == KIND_GROUND && at[A_SLOPE] > 0.45f;
    float ybase = floorf(fminf(fminf(p0.y, p1.y), p2.y) / TEX_SPAN) * TEX_SPAN;
    float yspan = fmaxf(TEX_SPAN, fmaxf(fmaxf(p0.y, p1.y), p2.y) - ybase); // los muros altos: la celda entera en el triangulo
    for (int k = 0; k < 3; k++) {
        int v = b->count * 3 + k;
        float lu = at[A_KIND] == KIND_GROUND ? b->local[k * 2] : 0.5f, lv = at[A_KIND] == KIND_GROUND ? b->local[k * 2 + 1] : 0.5f;
        if (wall) lv = Clamp((tri[k].y - ybase) / yspan, 0.0f, 1.0f);
        b->uv[v * 2 + 0] = u0 + eps + lu * (cw - 2.0f * eps);
        b->uv[v * 2 + 1] = v0 + eps + lv * (cw - 2.0f * eps);
    }
    b->count++;
}

static uint32_t hash2(int x, int z, uint32_t seed) {
    uint32_t h = (uint32_t)x * 0x8DA6B343u ^ (uint32_t)z * 0xD8163841u ^ seed * 0xCB1AB31Fu;
    h ^= h >> 13;
    h *= 0x5BD1E995u;
    return h ^ (h >> 15);
}

// Un pino: tronco y dos copas conicas.
static void push_tree(TriBuf *b, Vector3 base, float height, float radius, float patch) {
    float at[TRI_ATTRS] = { 0 };
    at[A_H] = base.y + height * 0.6f;
    at[A_PATCH] = patch;
    at[A_KIND] = KIND_TRUNK;
    const int sides = 5;
    float tr = radius * 0.18f, th = height * 0.3f;
    for (int i = 0; i < sides; i++) {
        float a0 = (float)i / sides * 2.0f * PI, a1 = (float)(i + 1) / sides * 2.0f * PI;
        Vector3 b0 = { base.x + cosf(a0) * tr, base.y - 0.3f, base.z + sinf(a0) * tr }, b1 = { base.x + cosf(a1) * tr, base.y - 0.3f, base.z + sinf(a1) * tr };
        Vector3 t0 = { b0.x, base.y + th, b0.z }, t1 = { b1.x, base.y + th, b1.z };
        push_tri(b, b0, t0, b1, at);
        push_tri(b, b1, t0, t1, at);
    }
    at[A_KIND] = KIND_CANOPY;
    for (int tier = 0; tier < 2; tier++) {
        float y0 = base.y + th + tier * height * 0.3f, y1 = y0 + height * (tier ? 0.45f : 0.55f), r = radius * (tier ? 0.7f : 1.0f);
        Vector3 tip = { base.x, y1, base.z };
        for (int i = 0; i < sides + 1; i++) {
            float a0 = (float)i / (sides + 1) * 2.0f * PI, a1 = (float)(i + 1) / (sides + 1) * 2.0f * PI;
            Vector3 p0 = { base.x + cosf(a0) * r, y0, base.z + sinf(a0) * r }, p1 = { base.x + cosf(a1) * r, y0, base.z + sinf(a1) * r };
            push_tri(b, p0, tip, p1, at);
        }
    }
}

// Una roca: un canto rodado facetado (anillo de 5 con alturas al azar y un tope), medio
// enterrado. at trae la region (desierto, costa) para el color.
static void push_rock(TriBuf *b, Vector3 base, float r, uint32_t h, const float *region_at) {
    float at[TRI_ATTRS];
    memcpy(at, region_at, sizeof(at));
    at[A_KIND] = KIND_ROCK;
    at[A_PATCH] = (float)(h & 0xFF) / 255.0f;
    at[A_H] = base.y + r * 0.5f;
    const int sides = 5;
    Vector3 ring[5], top = { base.x + r * 0.15f, base.y + r * (0.7f + 0.5f * (float)((h >> 8) & 0xFF) / 255.0f), base.z - r * 0.1f };
    for (int i = 0; i < sides; i++) {
        float a = (float)i / sides * 2.0f * PI + (float)((h >> (i * 3)) & 7) * 0.08f;
        float rr = r * (0.75f + 0.25f * (float)((h >> (i * 5 + 2)) & 3) / 3.0f);
        ring[i] = (Vector3){ base.x + cosf(a) * rr, base.y + r * (0.15f + 0.2f * (float)((h >> (i * 4 + 1)) & 3) / 3.0f), base.z + sinf(a) * rr };
    }
    for (int i = 0; i < sides; i++) {
        Vector3 p0 = ring[i], p1 = ring[(i + 1) % sides];
        Vector3 g0 = { p0.x * 1.0f, base.y - 0.4f, p0.z }, g1 = { p1.x, base.y - 0.4f, p1.z };
        push_tri(b, p0, top, p1, at);   // cara de arriba
        push_tri(b, g0, p0, p1, at);    // costado hasta el suelo
        push_tri(b, g0, p1, g1, at);
    }
}

// Una mata o arbusto: un domo bajo de 5 caras.
static void push_bush(TriBuf *b, Vector3 base, float r, float hgt, float patch, const float *region_at) {
    float at[TRI_ATTRS];
    memcpy(at, region_at, sizeof(at));
    at[A_KIND] = KIND_BUSH;
    at[A_PATCH] = patch;
    at[A_H] = base.y + hgt * 0.5f;
    const int sides = 5;
    Vector3 top = { base.x, base.y + hgt, base.z };
    for (int i = 0; i < sides; i++) {
        float a0 = (float)i / sides * 2.0f * PI + patch, a1 = (float)(i + 1) / sides * 2.0f * PI + patch;
        Vector3 p0 = { base.x + cosf(a0) * r, base.y + hgt * 0.35f, base.z + sinf(a0) * r }, p1 = { base.x + cosf(a1) * r, base.y + hgt * 0.35f, base.z + sinf(a1) * r };
        Vector3 g0 = { base.x + cosf(a0) * r * 0.7f, base.y - 0.1f, base.z + sinf(a0) * r * 0.7f }, g1 = { base.x + cosf(a1) * r * 0.7f, base.y - 0.1f, base.z + sinf(a1) * r * 0.7f };
        push_tri(b, p0, top, p1, at);
        push_tri(b, g0, p0, p1, at);
        push_tri(b, g0, p1, g1, at);
    }
}

// Rocas y matas por region (por celda de 3 m): pedreros en el altiplano y la costa,
// cantos rojizos en el desierto; matas en la estepa (mas junto al agua), sotobosque.
static float rock_density(const float *k) {
    return k[REGION_HIGHLAND] * 0.09f + k[REGION_FJORD] * 0.07f + k[REGION_DESERT] * 0.04f + k[REGION_FOREST] * 0.025f + k[REGION_STEPPE] * 0.018f;
}
static float bush_density(const float *k, float near_water) {
    return k[REGION_STEPPE] * (0.07f + 0.18f * near_water) + k[REGION_FOREST] * 0.10f + k[REGION_FJORD] * 0.08f + k[REGION_HIGHLAND] * 0.04f +
           k[REGION_DESERT] * (0.02f + 0.2f * near_water);
}

// Densidad de arboles: bosque cerrado; algunos en la costa y en las faldas del altiplano;
// pocos en la estepa (mas junto al agua); ninguno en el desierto.
static float tree_density(const float *k, float near_water) {
    return k[REGION_FOREST] * 0.85f + k[REGION_FJORD] * 0.20f + k[REGION_HIGHLAND] * 0.10f + k[REGION_STEPPE] * (0.01f + 0.35f * near_water) +
           k[REGION_DESERT] * 0.25f * near_water; // sotos junto a rios y oasis
}

// Claro alrededor de las estructuras y de los campamentos de tribus (ni arboles ni rocas encima).
static bool near_site(const World *w, float x, float z) {
    for (int i = 0; i < w->site_count; i++) {
        float dx = x - w->sites[i].x, dz = z - w->sites[i].z;
        if (dx * dx + dz * dz < 32.0f * 32.0f) return true;
    }
    for (int i = 0; i < w->tribe_count; i++) {
        float dx = x - w->tribes[i].x, dz = z - w->tribes[i].z;
        if (dx * dx + dz * dz < 26.0f * 26.0f) return true;
    }
    return false;
}

static Model make_model(TriBuf *b) {
    Mesh mesh = { 0 };
    mesh.triangleCount = b->count;
    mesh.vertexCount = b->count * 3;
    mesh.vertices = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.normals = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.colors = MemAlloc(mesh.vertexCount * 4 * sizeof(unsigned char));
    mesh.texcoords = MemAlloc(mesh.vertexCount * 2 * sizeof(float));
    memcpy(mesh.vertices, b->v, mesh.vertexCount * 3 * sizeof(float));
    memcpy(mesh.normals, b->n, mesh.vertexCount * 3 * sizeof(float));
    memcpy(mesh.texcoords, b->uv, mesh.vertexCount * 2 * sizeof(float));
    Model m = LoadModelFromMesh(mesh);
    if (g_atlas.id) m.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = g_atlas;
    return m;
}

// Agua de un chunk: una malla continua sobre la rejilla del suelo (2 m), con el nivel del agua de la estacion
// en cada vertice. Los rios bajan en pendiente (sin escalones) y la orilla queda bajo el suelo (sin huecos): un
// vertice seco toma el nivel de sus vecinos con agua, asi el borde del agua se mete bajo la orilla. Cada
// vertice lleva el color de su agua (lago, rio, mar o deshielo turquesa) mezclado con el de su hielo; el
// hielo que aguanta va en su propia malla (sin el brillo de las ondas), y la espuma en otra.
static Texture2D g_water_tex, g_ice_tex, g_foam_tex;
static bool g_water_tex_tried;

// ---------------------------------------------------------------- el shader del agua (docs/AGUA.md, fase 4)
// Cada vertice de la malla del agua lleva (con el shader):
//  - texcoord: x la hondura (el agua sobre el suelo, m; negativa bajo la orilla), y la espuma de las rocas y los
//    rapidos [0, 1] (-1: un charco, sin espuma);
//  - texcoord2: la corriente (m/s; en los lagos y el mar, una deriva leve);
//  - normal: x el agua (0 lago o canal, 1 rio, 2 mar), y su hielo [0, 1] (calma las olas y la espuma).
// Las ondas y la espuma se piden respecto de un origen cerca del jugador (el chunk donde esta: salta de a 48 m),
// asi los numeros quedan chicos (mediump en Android). Por eso las ondas caben justas en 48 m (K1) y el tiempo
// del vertice vuelve a empezar cada 120 s (W1): al saltar, nada se mueve.
#define WATER_GLSL_VS \
    "ATTR vec3 vertexPosition;\n" \
    "ATTR vec2 vertexTexCoord;\n" \
    "ATTR vec2 vertexTexCoord2;\n" \
    "ATTR vec3 vertexNormal;\n" \
    "ATTR vec4 vertexColor;\n" \
    "uniform mat4 mvp;\n" \
    "uniform vec4 uOrigin;\n" /* xz: el origen; w: el tiempo (s, de 0 a 120) */ \
    "VARY vec2 vRel;\n" \
    "VARY vec2 vFlow;\n" \
    "VARY vec4 vColor;\n" \
    "VARY vec4 vInfo;\n" \
    "VARY float vGust;\n" \
    "const float K1 = 0.1308997;\n" \
    "const float W1 = 0.0523599;\n" \
    "void main() {\n" \
    "    vec3 p = vertexPosition;\n" \
    "    vec2 rel = p.xz - uOrigin.xz;\n" \
    "    float t = uOrigin.w;\n" \
    "    float sea = step(1.5, vertexNormal.x);\n" \
    "    float river = step(0.5, vertexNormal.x) - sea;\n" \
    "    float calm = 1.0 - clamp(vertexNormal.y * 2.0, 0.0, 1.0);\n" \
    "    float depth = vertexTexCoord.x;\n" \
    "    float bob = 0.035 * sin(t * W1 * 22.0 + rel.x * K1 * 2.0 + rel.y * K1) * (1.0 - river);\n" \
    "    float swell = 0.12 * sin(t * W1 * 12.0 + rel.x * K1 - rel.y * K1) * smoothstep(1.0, 6.0, depth);\n" \
    "    float surge = 0.28 * sin(t * W1 * 16.0 + depth * 1.4 + 0.6 * sin(rel.x * K1 + rel.y * K1 * 2.0))\n" \
    "                * (1.0 - smoothstep(0.5, 3.5, depth));\n" \
    "    float lift = (bob + sea * (swell + surge)) * calm;\n" \
    "    p.y += lift;\n" \
    "    vRel = rel;\n" \
    "    vFlow = vertexTexCoord2;\n" \
    "    vColor = vertexColor;\n" \
    "    vInfo = vec4(depth + lift, vertexTexCoord.y, sea, vertexNormal.y);\n" \
    "    vGust = 0.07 * sin(rel.x * K1 * 2.0 + rel.y * K1 - t * W1 * 8.0) * (1.0 - river) * calm;\n" \
    "    gl_Position = mvp * vec4(p, 1.0);\n" \
    "}\n"
#define WATER_GLSL_FS \
    "uniform sampler2D texture0;\n" /* las ondas (assets/terrain/agua.png) */ \
    "uniform sampler2D texture1;\n" /* la espuma (assets/terrain/espuma.png) */ \
    "uniform vec4 colDiffuse;\n" \
    "uniform vec4 uPhase;\n" /* xy: las dos fases de la corriente [0, 1); zw: la deriva de la segunda onda */ \
    "uniform vec4 uPulse;\n" /* x: el latido de la espuma [-1, 1] */ \
    "uniform vec4 uBodies[8];\n" /* quien esta en el agua: xy donde, z el radio, w cuanta espuma */ \
    "VARY vec2 vRel;\n" \
    "VARY vec2 vFlow;\n" \
    "VARY vec4 vColor;\n" \
    "VARY vec4 vInfo;\n" \
    "VARY float vGust;\n" \
    "void main() {\n" \
    "    vec2 uv = vRel / 6.0;\n" \
    "    vec2 fuv = vRel / 12.0;\n" \
    "    vec2 f = vFlow * (1.6 / 6.0);\n" /* lo que avanza la corriente en un ciclo de 1,6 s */ \
    "    float blend = abs(1.0 - 2.0 * uPhase.x);\n" \
    "    float ra = TEX(texture0, uv - f * uPhase.x).r;\n" \
    "    float rb = TEX(texture0, uv - f * uPhase.y + vec2(0.37, 0.61)).r;\n" \
    "    float ripple = mix(ra, rb, blend);\n" \
    "    float r2 = TEX(texture0, uv * 0.75 + uPhase.zw).r;\n" \
    "    float shade = 0.5 * (ripple + r2) + vGust;\n" /* como antes: la textura (0,84 de promedio) por el color */ \
    "    float glint = smoothstep(0.9, 0.98, min(ripple, r2 + 0.03));\n" \
    "    float na = TEX(texture1, fuv - f * 0.5 * uPhase.x).r;\n" \
    "    float nb = TEX(texture1, fuv - f * 0.5 * uPhase.y + vec2(0.5, 0.5)).r;\n" \
    "    float noise = mix(na, nb, blend);\n" \
    "    float sea = vInfo.z;\n" \
    "    float ice = vInfo.w;\n" \
    "    float shore = vInfo.y < 0.0 ? 0.0 : 1.0 - smoothstep(0.0, mix(0.3, 1.1, sea), vInfo.x);\n" \
    "    float foam = shore * smoothstep(0.62 - 0.32 * shore, 0.8, noise + 0.12 * uPulse.x * (0.4 + sea));\n" \
    "    foam = max(foam, max(vInfo.y, 0.0) * smoothstep(0.42, 0.72, noise));\n" \
    "    for (int i = 0; i < 8; i++) {\n" \
    "        vec4 b = uBodies[i];\n" \
    "        float d = length(vRel - b.xy);\n" \
    "        float ring = 1.0 - smoothstep(0.0, 0.2, abs(d - b.z));\n" \
    "        float inner = 1.0 - smoothstep(0.0, b.z, d);\n" \
    "        foam = max(foam, b.w * max(ring, 0.6 * inner) * smoothstep(0.25, 0.55, noise + 0.2));\n" \
    "    }\n" \
    "    foam *= 1.0 - clamp(ice * 2.0, 0.0, 1.0);\n" \
    "    vec3 col = vColor.rgb * shade + glint * 0.16 * (1.0 - ice);\n" \
    "    col = mix(col, vec3(0.96, 0.98, 1.0), foam * 0.92);\n" \
    "    FRAG = vec4(col, mix(vColor.a, 0.96, foam)) * colDiffuse;\n" \
    "}\n"
static const char WATER_VS_330[] = "#version 330\n#define ATTR in\n#define VARY out\n" WATER_GLSL_VS;
static const char WATER_FS_330[] = "#version 330\n#define VARY in\n#define TEX texture\n#define FRAG finalColor\nout vec4 finalColor;\n" WATER_GLSL_FS;
// En GLSL ES el vertice va en mediump, como el shader por defecto de raylib con el que se dibuja todo lo demas en
// Android (en los moviles es de 32 bits; y con highp, llvmpipe de Mesa no dibuja nada en OpenGL ES).
static const char WATER_VS_100[] = "#version 100\nprecision mediump float;\n#define ATTR attribute\n#define VARY varying\n" WATER_GLSL_VS;
static const char WATER_FS_100[] = "#version 100\n#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n#else\nprecision mediump float;\n#endif\n"
                                   "#define VARY varying\n#define TEX texture2D\n#define FRAG gl_FragColor\n" WATER_GLSL_FS;

static Shader g_water_shader;
static bool g_water_shader_ok, g_water_shader_off;
static int g_wloc_origin, g_wloc_phase, g_wloc_pulse, g_wloc_bodies;
static Vector4 g_ripples[TERRAIN_RIPPLES]; // x, z del mundo, radio y cuanta espuma
static int g_ripple_count;

// El shader del agua: GLSL ES 100 con OpenGL ES (Android), GLSL 330 con OpenGL 3.3. Sin el (no compila, o
// falta la textura de la espuma), el agua de antes.
static void load_water_shader(void) {
    if (g_water_shader_off) return;
    int v = rlGetVersion();
    bool es = v == RL_OPENGL_ES_20 || v == RL_OPENGL_ES_30;
    if (!es && v != RL_OPENGL_33 && v != RL_OPENGL_43) return; // OpenGL 1.1 o 2.1: sin shader
    Shader sh = LoadShaderFromMemory(es ? WATER_VS_100 : WATER_VS_330, es ? WATER_FS_100 : WATER_FS_330);
    if (!IsShaderValid(sh) || sh.id == rlGetShaderIdDefault()) {
        TraceLog(LOG_WARNING, "ESTEPA: el shader del agua no compilo: el agua de antes");
        return;
    }
    g_water_shader = sh;
    g_wloc_origin = GetShaderLocation(sh, "uOrigin");
    g_wloc_phase = GetShaderLocation(sh, "uPhase");
    g_wloc_pulse = GetShaderLocation(sh, "uPulse");
    g_wloc_bodies = GetShaderLocation(sh, "uBodies[0]");
    if (g_wloc_bodies < 0) g_wloc_bodies = GetShaderLocation(sh, "uBodies");
    g_water_shader_ok = g_water_tex.id && g_foam_tex.id;
    if (!g_water_shader_ok) UnloadShader(sh), g_water_shader = (Shader){ 0 };
    else TraceLog(LOG_INFO, "ESTEPA: el agua con su shader (GLSL %s)", es ? "ES 100" : "330"); // el CI lo busca
}

bool terrain_water_shader(void) { return g_water_shader_ok; }

// El radio aproximado de cada formacion, en m (antes de su escala).
static float feature_radius(int kind) {
    return kind == VF_BOULDER ? 1.1f : kind == VF_BASALT ? 3.2f : kind == VF_SEA_STACK ? 4.4f : kind == VF_ICEBERG ? 4.0f : kind == VF_FLOE ? 3.0f : 3.5f;
}

bool terrain_near_feature(const Terrain *t, float x, float z, float margin) {
    for (int i = 0; i < CHUNK_SLOTS; i++) {
        const Chunk *ch = &t->slots[i];
        if (!ch->loaded) continue;
        for (int k = 0; k < ch->feat_count; k++) {
            const ChunkFeature *f = &ch->feat[k];
            float r = feature_radius(f->kind) * f->scale + margin;
            if ((x - f->x) * (x - f->x) + (z - f->z) * (z - f->z) < r * r) return true;
        }
    }
    return false;
}
void terrain_water_shader_off(void) { g_water_shader_off = true; }

void terrain_set_ripples(const Vector4 *bodies, int count) {
    g_ripple_count = count < 0 ? 0 : count > TERRAIN_RIPPLES ? TERRAIN_RIPPLES : count;
    for (int i = 0; i < g_ripple_count; i++) g_ripples[i] = bodies[i];
}

// Las aguas de cada vertice (src/sim/world.h, WaterBodies) y lo de cada celda: se piden una vez por chunk.
// gh: el suelo de los vertices (el de la malla del chunk).
static void sample_water(const Terrain *t, Chunk *ch, int cx, int cz, const float (*gh)[CHUNK_CELLS + 1]) {
    enum { N = CHUNK_CELLS, V = CHUNK_CELLS + 1 };
    const float cell = CHUNK_SIZE / N, ox = cx * CHUNK_SIZE, oz = cz * CHUNK_SIZE;
    static WaterBodies wb[V * V];
    bool any = false;
    for (int j = 0; j < V; j++)
        for (int i = 0; i < V; i++) {
            WaterBodies *b = &wb[j * V + i];
            world_water_bodies(t->world, ox + i * cell, oz + j * cell, b);
            any = any || b->sea_floor > -1e8f || b->canal > -1e8f || b->lake >= 0 || b->river[0] >= 0;
        }
    if (!any) return; // tierra adentro: sin agua (la mayoria de los chunks)
    ch->wbody = MemAlloc(V * V * sizeof(WaterBodies));
    ch->wground = MemAlloc(V * V * sizeof(float));
    ch->wcell = MemAlloc(N * N);
    memcpy(ch->wbody, wb, sizeof(wb));
    for (int j = 0; j < V; j++)
        for (int i = 0; i < V; i++) ch->wground[j * V + i] = gh[j][i];
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++) {
            bool wet = false;
            for (int c = 0; c < 4; c++) {
                const WaterBodies *b = &wb[(j + (c >> 1)) * V + i + (c & 1)];
                wet = wet || b->sea_floor > -1e8f || b->canal > -1e8f || b->lake >= 0 || b->river[0] >= 0;
            }
            unsigned char f = 0;
            float mx = ox + (i + 0.5f) * cell, mz = oz + (j + 0.5f) * cell;
            if (wet && world_region_weight(t->world, REGION_HIGHLAND, mx, mz) > 0.5f) f |= WC_GLACIAL; // deshielo turquesa
            if (wet && world_river_bank(t->world, mx, mz, NULL) > 0.85f) f |= WC_RAPIDS;
            ch->wcell[j * N + i] = f;
        }
    // La espuma de cada vertice (el shader del agua) que rodea las rocas del agua: los cantos del cauce, los
    // farallones y el basalto del mar, los tempanos de los lagos. La de los rapidos depende de la hondura (y la
    // hondura, de la estacion): mesh_water.
    ch->wfoam = MemAlloc(V * V);
    for (int j = 0; j < V; j++)
        for (int i = 0; i < V; i++) {
            float f = 0.0f, x = ox + i * cell, z = oz + j * cell;
            for (int k = 0; k < ch->feat_count; k++) {
                const ChunkFeature *ft = &ch->feat[k];
                float r = ft->kind == VF_BOULDER ? 1.1f : ft->kind == VF_BASALT ? 3.2f : ft->kind == VF_SEA_STACK ? 4.4f : ft->kind == VF_ICEBERG ? 4.0f : 0.0f;
                if (r <= 0.0f) continue;
                r *= ft->scale;
                float d = sqrtf((x - ft->x) * (x - ft->x) + (z - ft->z) * (z - ft->z));
                f = fmaxf(f, 1.0f - Clamp((d - r) / 1.8f, 0.0f, 1.0f));
            }
            ch->wfoam[j * V + i] = (unsigned char)(255.0f * Clamp(f, 0.0f, 1.0f));
        }
}

static void free_water(Chunk *ch) {
    for (int m = 0; m < W_MESHES; m++)
        if (ch->has_water[m]) UnloadModel(ch->water[m]);
    memset(ch->has_water, 0, sizeof(ch->has_water));
}

// Las mallas del agua del chunk en la estacion de las mallas (t->meshed).
static void mesh_water(const Terrain *t, Chunk *ch) {
    enum { N = CHUNK_CELLS, V = CHUNK_CELLS + 1, MAXT = N * N * 2 };
    free_water(ch);
    ch->water_dirty = false;
    if (!ch->wbody) return;
    if (!g_water_tex_tried) { // las texturas del agua (ondas), del hielo (grietas) y de la espuma, y el shader, una vez
        g_water_tex_tried = true;
        static const char *FILES[3] = { "assets/terrain/agua.png", "assets/terrain/hielo.png", "assets/terrain/espuma.png" };
        Texture2D *tex[3] = { &g_water_tex, &g_ice_tex, &g_foam_tex };
        for (int k = 0; k < 3; k++) {
            const char *path = platform_asset_path(FILES[k]);
            if (platform_asset_exists(path)) *tex[k] = LoadTexture(path);
            if (tex[k]->id) SetTextureFilter(*tex[k], TEXTURE_FILTER_POINT), SetTextureWrap(*tex[k], TEXTURE_WRAP_REPEAT);
            platform_note_asset(FILES[k], tex[k]->id != 0);
        }
        load_water_shader();
    }
    const bool shaded = g_water_shader_ok;
    const float cell = CHUNK_SIZE / N, ox = ch->cx * CHUNK_SIZE, oz = ch->cz * CHUNK_SIZE;
    static float lv[V][V], ice[V][V], flow[V][V][2];
    static unsigned char wk[V][V], dry[V][V];
    static float verts[W_MESHES][MAXT * 9];
    static unsigned char cols[W_MESHES][MAXT * 12];
    static float wuv[MAXT * 6], wuv2[MAXT * 6], wnorm[MAXT * 9]; // la malla del agua, con el shader
    int count[W_MESHES] = { 0 };
    const WaterSeason *s = &t->meshed;
    const World *w = t->world;
    for (int j = 0; j < V; j++)
        for (int i = 0; i < V; i++) {
            WaterNow n;
            const WaterBodies *wb = &ch->wbody[j * V + i];
            lv[j][i] = world_water_resolve(w, wb, ox + i * cell, oz + j * cell, s, &n);
            wk[j][i] = (unsigned char)n.kind, ice[j][i] = n.ice;
            dry[j][i] = n.kind == WATER_RIVER && world_river_share(w, n.river, s) < WORLD_DRY_SHARE; // un charco
            // La corriente que se ve: aguas abajo, mas rapida en lo empinado y en los arroyos, y mas lenta con
            // poca agua. Los lagos, el canal y el mar, una deriva leve (con el viento del oeste).
            flow[j][i][0] = flow[j][i][1] = 0.0f;
            if (!shaded || n.kind == WATER_NONE || dry[j][i]) continue;
            if (n.kind != WATER_RIVER || n.river < 0) {
                float drift = n.kind == WATER_SEA ? 0.2f : 0.11f;
                flow[j][i][0] = drift * 0.86f, flow[j][i][1] = drift * 0.5f;
                continue;
            }
            float along = 0.0f, dx, dz, slope;
            for (int k = 0; k < WATER_RIVERS_HERE; k++)
                if (wb->river[k] == n.river) along = wb->river_along[k];
            if (!world_river_flow(w, n.river, along, &dx, &dz, &slope)) continue;
            const River *rv = &w->rivers[n.river];
            float base = rv->kind == RIVER_CREEK ? 0.9f : rv->kind == RIVER_BRAIDED ? 0.75f : rv->kind == RIVER_TRIBUTARY ? 0.6f : 0.45f;
            float full = Clamp(world_river_share(w, n.river, s) / WORLD_FULL_SHARE, 0.0f, 1.0f);
            float v = Clamp(base + 30.0f * slope, 0.3f, 2.2f) * (0.45f + 0.55f * full);
            flow[j][i][0] = dx * v, flow[j][i][1] = dz * v;
        }
    // Los colores del agua (con el brillo de las ondas a la mitad: ver terrain_draw_water) y del hielo.
    const Color lake = { 52, 96, 128, 200 }, river = { 62, 112, 140, 200 }, sea = { 36, 72, 104, 220 }, glacial = { 84, 168, 178, 210 };
    const Color icec = { 184, 210, 232, 240 };
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++) {
            const int ci[4][2] = { { i, j }, { i, j + 1 }, { i + 1, j }, { i + 1, j + 1 } };
            float sum = 0.0f, ice_sum = 0.0f, lowest = 1e9f, low_lv = 1e9f;
            int wet = 0, kind = WATER_NONE;
            bool use[4] = { false, false, false, false }, puddle = false;
            for (int c = 0; c < 4; c++) {
                int x = ci[c][0], z = ci[c][1];
                lowest = fminf(lowest, ch->wground[z * V + x]);
                if (wk[z][x] != WATER_NONE) low_lv = fminf(low_lv, lv[z][x]);
            }
            // Dos aguas a niveles muy distintos en una celda (un lago al borde de un cañon con su
            // rio): solo la de abajo, que se mete bajo el barranco. Si no, una cortina de agua.
            for (int c = 0; c < 4; c++) {
                int x = ci[c][0], z = ci[c][1];
                if (wk[z][x] == WATER_NONE || lv[z][x] > low_lv + 3.0f) continue;
                use[c] = true;
                sum += lv[z][x], ice_sum += ice[z][x], wet++;
                puddle = puddle || dry[z][x];
                if (kind == WATER_NONE || wk[z][x] == WATER_SEA) kind = wk[z][x];
            }
            if (!wet) continue;
            float avg = sum / (float)wet, avg_ice = ice_sum / (float)wet;
            if (avg <= lowest) continue; // el agua no llega a esta celda
            Color base = kind == WATER_SEA ? sea : kind == WATER_LAKE ? lake : river;
            if (kind != WATER_SEA && (ch->wcell[j * N + i] & WC_GLACIAL)) base = glacial;
            int m = hazard_ice_walkable(avg_ice) ? W_ICE : W_WATER;
            float q[4][3], qi[4];
            for (int c = 0; c < 4; c++) {
                int x = ci[c][0], z = ci[c][1];
                q[c][0] = ox + x * cell, q[c][1] = use[c] ? lv[z][x] : avg, q[c][2] = oz + z * cell; // seco: bajo la orilla
                qi[c] = use[c] ? ice[z][x] : avg_ice;
            }
            const int tri[6] = { 0, 1, 2, 2, 1, 3 };
            float code = kind == WATER_SEA ? 2.0f : kind == WATER_RIVER && !puddle ? 1.0f : 0.0f;
            for (int v = 0; v < 6; v++) {
                int o = count[m] * 3 + v, c = tri[v];
                memcpy(&verts[m][o * 3], q[c], sizeof(q[0]));
                Color k = mixc(base, icec, qi[c]);
                float boost = m == W_WATER ? 1.0f / 0.94f : 1.0f;
                cols[m][o * 4 + 0] = (unsigned char)fminf(255.0f, k.r * boost), cols[m][o * 4 + 1] = (unsigned char)fminf(255.0f, k.g * boost);
                cols[m][o * 4 + 2] = (unsigned char)fminf(255.0f, k.b * boost), cols[m][o * 4 + 3] = (unsigned char)Lerp(base.a, icec.a, Clamp(qi[c], 0.0f, 1.0f));
                if (shaded && m == W_WATER) { // hondura, espuma, corriente, que agua y su hielo (ver el shader)
                    int x = ci[c][0], z = ci[c][1];
                    float depth = q[c][1] - ch->wground[z * V + x], rocks = (float)ch->wfoam[z * V + x] / 255.0f;
                    // Los rapidos: en lo bajo de los rios con cantos (como la malla de espuma de antes).
                    float rapids = (ch->wcell[j * N + i] & WC_RAPIDS) && kind == WATER_RIVER ? 0.6f * (1.0f - Clamp((depth - 0.25f) / 0.6f, 0.0f, 1.0f)) : 0.0f;
                    wuv[o * 2 + 0] = depth;
                    wuv[o * 2 + 1] = puddle ? -1.0f : fmaxf(rocks, rapids);
                    wuv2[o * 2 + 0] = flow[z][x][0], wuv2[o * 2 + 1] = flow[z][x][1];
                    wnorm[o * 3 + 0] = code, wnorm[o * 3 + 1] = Clamp(qi[c], 0.0f, 1.0f), wnorm[o * 3 + 2] = 0.0f;
                }
            }
            count[m] += 2;
            // Espuma: la rompiente del mar y los rapidos de los rios con cantos (no los trenzados ni los
            // charcos); la opacidad sale de la profundidad en cada esquina (la franja sigue la orilla). Con
            // hielo, menos; helada, ninguna. Con el shader del agua, la espuma la hace el (esta malla no va).
            bool surf = !shaded && m == W_WATER && !puddle &&
                        (kind == WATER_SEA ? lowest > avg - 2.0f : kind == WATER_RIVER && lowest > avg - 0.8f && (ch->wcell[j * N + i] & WC_RAPIDS));
            if (surf) {
                for (int v = 0; v < 6; v++) {
                    const float *p = q[tri[v]];
                    int o = count[W_FOAM] * 3 + v;
                    verts[W_FOAM][o * 3 + 0] = p[0], verts[W_FOAM][o * 3 + 1] = p[1] + 0.06f, verts[W_FOAM][o * 3 + 2] = p[2];
                    int gx = (int)roundf((p[0] - ox) / cell), gz = (int)roundf((p[2] - oz) / cell);
                    float depth = p[1] - ch->wground[gz * V + gx], reach = kind == WATER_SEA ? 2.0f : 0.8f;
                    float wob = 0.6f + 0.4f * noise2d(p[0] * 0.15f, p[2] * 0.15f, 777u); // jirones
                    cols[W_FOAM][o * 4 + 0] = cols[W_FOAM][o * 4 + 1] = cols[W_FOAM][o * 4 + 2] = 255;
                    cols[W_FOAM][o * 4 + 3] = (unsigned char)(230.0f * Clamp(1.0f - depth / reach, 0.0f, 1.0f) * wob * (1.0f - 2.0f * Clamp(qi[tri[v]], 0.0f, 0.5f)));
                }
                count[W_FOAM] += 2;
            }
        }
    for (int m = 0; m < W_MESHES; m++) {
        ch->has_water[m] = count[m] > 0;
        if (!count[m]) continue;
        Mesh mesh = { 0 };
        mesh.triangleCount = count[m];
        mesh.vertexCount = count[m] * 3;
        mesh.vertices = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
        mesh.normals = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
        mesh.texcoords = MemAlloc(mesh.vertexCount * 2 * sizeof(float));
        mesh.colors = MemAlloc(mesh.vertexCount * 4);
        memcpy(mesh.vertices, verts[m], mesh.vertexCount * 3 * sizeof(float));
        memcpy(mesh.colors, cols[m], mesh.vertexCount * 4);
        bool own = shaded && m == W_WATER; // con el shader: sus datos (las ondas las calcula el)
        if (own) {
            mesh.texcoords2 = MemAlloc(mesh.vertexCount * 2 * sizeof(float));
            memcpy(mesh.texcoords, wuv, mesh.vertexCount * 2 * sizeof(float));
            memcpy(mesh.texcoords2, wuv2, mesh.vertexCount * 2 * sizeof(float));
            memcpy(mesh.normals, wnorm, mesh.vertexCount * 3 * sizeof(float));
        } else {
            float span = m == W_ICE ? 10.0f : 6.0f; // ondas cada 6 m; las placas del hielo, de 10 m (las texturas se repiten)
            for (int i = 0; i < mesh.vertexCount; i++) {
                mesh.normals[i * 3 + 1] = 1.0f;
                mesh.texcoords[i * 2 + 0] = mesh.vertices[i * 3 + 0] / span;
                mesh.texcoords[i * 2 + 1] = mesh.vertices[i * 3 + 2] / span;
            }
        }
        UploadMesh(&mesh, false);
        ch->water[m] = LoadModelFromMesh(mesh);
        Texture2D tex = m == W_ICE && g_ice_tex.id ? g_ice_tex : g_water_tex;
        if (m != W_FOAM && tex.id) ch->water[m].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = tex;
        if (own) {
            ch->water[m].materials[0].shader = g_water_shader;
            ch->water[m].materials[0].maps[MATERIAL_MAP_SPECULAR].texture = g_foam_tex;
        }
    }
}

// Las casillas de arboles y de rocas y matas, deterministas por posicion: las usan el trozo que las dibuja
// (build_chunk) y la cobertura (terrain_cover_near, A4), asi el arbol o la roca donde te cubres es el que se ve.
#define TREE_GRID 12 // casillas de arboles por lado de trozo
#define PROP_GRID 16 // casillas de rocas y matas por lado de trozo

static int grid_chunk(int g, int n) { return g >= 0 ? g / n : -((-g + n - 1) / n); }

// Un arbol en la casilla (gx, gz): su base, su altura y su copa.
static bool tree_slot(const Terrain *t, int gx, int gz, Vector3 *base, float *height, float *radius, float *patch) {
    const float slot = CHUNK_SIZE / TREE_GRID;
    int cx = grid_chunk(gx, TREE_GRID), cz = grid_chunk(gz, TREE_GRID), ix = gx - cx * TREE_GRID, iz = gz - cz * TREE_GRID;
    const float ox = cx * CHUNK_SIZE, oz = cz * CHUNK_SIZE;
    uint32_t h = hash2(gx, gz, t->seed);
    float r0 = (float)(h & 0xFFFF) / 65535.0f, r1 = (float)((h >> 16) & 0xFF) / 255.0f, r2 = (float)(h >> 24) / 255.0f;
    float x = ox + (ix + 0.15f + 0.7f * r1) * slot, z = oz + (iz + 0.15f + 0.7f * r2) * slot;
    if (x * x + z * z < 120.0f * 120.0f || world_edge_distance(t->world, x, z) < 110.0f) return false;
    if (world_forest_wall(t->world, x, z) > 0.2f) return false; // el muro tras el canal: roca desnuda
    if (world_canal_near(t->world, x, z, 14.0f)) return false;   // la orilla del canal, despejada: se ve el muro
    float k[REGION_COUNT];
    world_region_weights(t->world, x, z, k);
    float y = terrain_height(t, x, z), wl = world_water(t->world, x, z, 0.0f, NULL);
    float near_water = wl > -1e8f && y - wl < 6.0f ? 1.0f : 0.0f;
    if (r0 > tree_density(k, near_water)) return false;
    if (wl > y - 0.8f || y > t->look.snowline + 8.0f) return false; // en el agua o en el glaciar, no
    if (world_river_bank(t->world, x, z, NULL) > 0.45f) return false; // el lecho que el rio lava
    if (world_glacier_reach(t->world, x, z) <= 1.0f) return false;    // ni en la morrena (el glaciar avanza en invierno)
    float sx = terrain_height(t, x + 2.0f, z) - y, sz = terrain_height(t, x, z + 2.0f) - y;
    if (sx * sx + sz * sz > 2.2f) return false;
    float dist;
    int si = world_nearest_settlement(t->world, x, z, &dist);
    if (si >= 0 && dist < (t->world->settlements[si].kind == SETTLE_CAPITAL ? CITY_FIELDS_R1 + 8.0f : VILLAGE_FIELDS_R1 + 6.0f)) return false; // claros de los pueblos (y sus campos)
    if (near_site(t->world, x, z)) return false;
    *base = (Vector3){ x, y, z };
    *height = 5.0f + 5.0f * r1 * (0.6f + 0.4f * k[REGION_FOREST]);
    *radius = 1.4f + 0.8f * r2;
    *patch = r2;
    return true;
}

// Una roca (1) o una mata (2) en la casilla (gx, gz), o nada (0): su base, tamaño y lo que pide su dibujo.
typedef struct {
    Vector3 base;
    float size, size2, patch, slope;
    uint32_t hash;
    float k[REGION_COUNT];
} PropSlot;

static int prop_slot(const Terrain *t, int gx, int gz, PropSlot *o) {
    const float pslot = CHUNK_SIZE / PROP_GRID;
    int cx = grid_chunk(gx, PROP_GRID), cz = grid_chunk(gz, PROP_GRID), ix = gx - cx * PROP_GRID, iz = gz - cz * PROP_GRID;
    const float ox = cx * CHUNK_SIZE, oz = cz * CHUNK_SIZE;
    uint32_t h = hash2(gx, gz, t->seed ^ 0x5EEDu);
    float r0 = (float)(h & 0xFFFF) / 65535.0f, r1 = (float)((h >> 16) & 0xFF) / 255.0f, r2 = (float)(h >> 24) / 255.0f;
    float x = ox + (ix + 0.1f + 0.8f * r1) * pslot, z = oz + (iz + 0.1f + 0.8f * r2) * pslot;
    if (x * x + z * z < 60.0f * 60.0f || world_edge_distance(t->world, x, z) < 90.0f) return 0;
    float *k = o->k;
    world_region_weights(t->world, x, z, k);
    float rd = rock_density(k), wl = world_water(t->world, x, z, 0.0f, NULL), y = terrain_height(t, x, z);
    float near_water = wl > -1e8f && y - wl < 5.0f ? 1.0f : 0.0f;
    float bd = bush_density(k, near_water);
    if (r0 > rd + bd) return 0;
    if (wl > y - 0.4f) return 0; // en el agua, no
    if (world_river_bank(t->world, x, z, NULL) > 0.45f || (k[REGION_FJORD] > 0.5f && y < SEA_LEVEL + 4.0f)) return 0; // ni en el lecho ni en la playa
    float dist;
    int si = world_nearest_settlement(t->world, x, z, &dist);
    if (si >= 0 && dist < (t->world->settlements[si].kind == SETTLE_CAPITAL ? CITY_FIELDS_R1 + 4.0f : VILLAGE_FIELDS_R1 + 4.0f)) return 0;
    if (near_site(t->world, x, z)) return 0;
    float sx = terrain_height(t, x + 1.5f, z) - y, sz = terrain_height(t, x, z + 1.5f) - y;
    o->slope = sqrtf(sx * sx + sz * sz) / 1.5f;
    o->base = (Vector3){ x, y, z };
    o->hash = h;
    o->patch = r2;
    if (r0 < rd) {
        o->size = 0.4f + 1.4f * r1 * r1 + (k[REGION_HIGHLAND] + k[REGION_FJORD]) * 0.8f * r2;
        return 1;
    }
    if (o->slope < 0.9f && y < t->look.snowline - 6.0f && world_glacier_reach(t->world, x, z) > 1.0f) { // ni en el glaciar ni en su morrena
        o->size = 0.5f + 0.7f * r1;
        o->size2 = 0.4f + 0.8f * r2 * (0.5f + k[REGION_FOREST]);
        return 2;
    }
    return 0;
}

bool terrain_cover_near(const Terrain *t, float x, float z, float reach, Vector3 *center, float *radius) {
    float best = 1e9f;
    Vector3 bc = { 0 };
    float br = 0.0f;
    // Los troncos de los arboles (el tronco cubre; la copa no frena una flecha baja).
    const float tslot = CHUNK_SIZE / TREE_GRID, pslot = CHUNK_SIZE / PROP_GRID;
    int tgx = (int)floorf(x / tslot), tgz = (int)floorf(z / tslot);
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            Vector3 base;
            float height, rad, patch;
            if (!tree_slot(t, tgx + dx, tgz + dz, &base, &height, &rad, &patch)) continue;
            float d = sqrtf((base.x - x) * (base.x - x) + (base.z - z) * (base.z - z)) - 0.35f;
            if (d < reach && d < best) best = d, bc = base, br = 0.35f;
        }
    // Las rocas de un metro o mas y las matas altas.
    int pgx = (int)floorf(x / pslot), pgz = (int)floorf(z / pslot);
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            PropSlot ps;
            int kind = prop_slot(t, pgx + dx, pgz + dz, &ps);
            float rad = kind == 1 ? ps.size * 0.6f : kind == 2 ? ps.size * 0.8f : 0.0f;
            if (!kind || (kind == 1 && ps.size < 0.9f) || (kind == 2 && ps.size2 < 0.75f)) continue;
            float d = sqrtf((ps.base.x - x) * (ps.base.x - x) + (ps.base.z - z) * (ps.base.z - z)) - rad;
            if (d < reach && d < best) best = d, bc = ps.base, br = rad;
        }
    // Las formaciones de voxeles (peñas, basaltos, cantos).
    for (int i = 0; i < CHUNK_SLOTS; i++) {
        const Chunk *ch = &t->slots[i];
        if (!ch->loaded) continue;
        for (int k = 0; k < ch->feat_count; k++) {
            const ChunkFeature *f = &ch->feat[k];
            float rad = feature_radius(f->kind) * f->scale;
            float d = sqrtf((x - f->x) * (x - f->x) + (z - f->z) * (z - f->z)) - rad;
            if (d < reach && d < best) best = d, bc = (Vector3){ f->x, terrain_height(t, f->x, f->z), f->z }, br = rad;
        }
    }
    if (best > 1e8f) return false;
    if (center) *center = bc;
    if (radius) *radius = br;
    return true;
}

static Chunk build_chunk(const Terrain *t, int cx, int cz) {
    enum { GROUND = CHUNK_CELLS * CHUNK_CELLS * 2, TREE_SLOTS = TREE_GRID, TREES_MAX = TREE_SLOTS * TREE_SLOTS, TREE_TRIS = 22,
           PROP_SLOTS = PROP_GRID, PROP_TRIS = 15, CAP = GROUND + TREES_MAX * TREE_TRIS + PROP_SLOTS * PROP_SLOTS * PROP_TRIS };
    static float vbuf[CAP * 9], nbuf[CAP * 9];
    static float abuf[CAP * TRI_ATTRS], uvbuf[CAP * 6];
    TriBuf b = { vbuf, nbuf, abuf, uvbuf, 0, CAP, { 0 } };
    g_perma_snow = t->plain + 40.0f;
    const float cell = CHUNK_SIZE / CHUNK_CELLS;
    const float ox = cx * CHUNK_SIZE, oz = cz * CHUNK_SIZE;
    // El suelo: dos triangulos por celda, con los pesos de region de su centro.
    static float hgrid[CHUNK_CELLS + 1][CHUNK_CELLS + 1];
    for (int iz = 0; iz <= CHUNK_CELLS; iz++)
        for (int ix = 0; ix <= CHUNK_CELLS; ix++) hgrid[iz][ix] = terrain_height(t, ox + ix * cell, oz + iz * cell);
    for (int iz = 0; iz < CHUNK_CELLS; iz++) {
        for (int ix = 0; ix < CHUNK_CELLS; ix++) {
            float x0 = ox + ix * cell, x1 = x0 + cell, z0 = oz + iz * cell, z1 = z0 + cell;
            Vector3 p00 = { x0, hgrid[iz][ix], z0 }, p10 = { x1, hgrid[iz][ix + 1], z0 };
            Vector3 p01 = { x0, hgrid[iz + 1][ix], z1 }, p11 = { x1, hgrid[iz + 1][ix + 1], z1 };
            float mx = x0 + cell * 0.5f, mz = z0 + cell * 0.5f, k[REGION_COUNT];
            world_region_weights(t->world, mx, mz, k);
            float at[TRI_ATTRS] = { 0 };
            at[A_PATCH] = 0.5f + 0.5f * fbm2d(mx * 0.08f, mz * 0.08f, t->seed + 303u, 2); // manchas
            at[A_DESERT] = k[REGION_DESERT], at[A_FOREST] = k[REGION_FOREST], at[A_HIGH] = k[REGION_HIGHLAND], at[A_FJORD] = k[REGION_FJORD];
            float hm = (p00.y + p10.y + p01.y + p11.y) * 0.25f, wl = world_water(t->world, mx, mz, 0.0f, NULL);
            at[A_SHORE] = wl > -1e8f ? hm - wl : 99.0f;
            at[A_EDGE] = world_edge_distance(t->world, mx, mz);
            at[A_BANK] = world_river_bank(t->world, mx, mz, NULL);
            at[A_KIND] = KIND_GROUND;
            at[A_GLACIER] = world_glacier_reach(t->world, mx, mz);
            at[A_WALL] = world_forest_wall(t->world, mx, mz);
            // La celda del atlas se repite cada TEX_SPAN m: cuatro celdas del suelo por textura.
            const int per = (int)(TEX_SPAN / cell);
            float u0 = (float)(((cx * CHUNK_CELLS + ix) % per + per) % per) / per, v0 = (float)(((cz * CHUNK_CELLS + iz) % per + per) % per) / per;
            float du = 1.0f / per;
            at[A_H] = (p00.y + p01.y + p10.y) / 3.0f;
            float l1[6] = { u0, v0, u0, v0 + du, u0 + du, v0 };
            memcpy(b.local, l1, sizeof(l1));
            push_tri(&b, p00, p01, p10, at);
            at[A_H] = (p10.y + p01.y + p11.y) / 3.0f;
            float l2[6] = { u0 + du, v0, u0, v0 + du, u0 + du, v0 + du };
            memcpy(b.local, l2, sizeof(l2));
            push_tri(&b, p10, p01, p11, at);
        }
    }
    // Arboles (deterministas por posicion, tree_slot): segun la region, nunca en el agua, el campamento ni lo empinado.
    for (int iz = 0; iz < TREE_SLOTS; iz++)
        for (int ix = 0; ix < TREE_SLOTS; ix++) {
            Vector3 base;
            float height, radius, patch;
            if (tree_slot(t, cx * TREE_SLOTS + ix, cz * TREE_SLOTS + iz, &base, &height, &radius, &patch)) push_tree(&b, base, height, radius, patch);
        }
    // Rocas y matas (deterministas por posicion, prop_slot): ni en el agua, ni en el campamento, ni en los pueblos.
    for (int iz = 0; iz < PROP_SLOTS; iz++)
        for (int ix = 0; ix < PROP_SLOTS; ix++) {
            PropSlot ps;
            int kind = prop_slot(t, cx * PROP_SLOTS + ix, cz * PROP_SLOTS + iz, &ps);
            if (!kind) continue;
            float at[TRI_ATTRS] = { 0 };
            at[A_DESERT] = ps.k[REGION_DESERT], at[A_FOREST] = ps.k[REGION_FOREST], at[A_HIGH] = ps.k[REGION_HIGHLAND], at[A_FJORD] = ps.k[REGION_FJORD];
            at[A_SLOPE] = ps.slope;
            if (kind == 1) push_rock(&b, ps.base, ps.size, ps.hash, at);
            else push_bush(&b, ps.base, ps.size, ps.size2, ps.patch, at);
        }
    Chunk ch = { .loaded = true, .cx = cx, .cz = cz };
    // Formaciones de voxeles (de las fotos de cada region), por casillas de 6 m:
    //  - fiordos: roquerios de basalto en la playa negra y las rompientes, farallones en el mar;
    //  - rios: cantos rodados claros en el cauce y la orilla;
    //  - desierto: farallones en estratos junto al cañon, torres de arenisca y creta blanca;
    //  - altiplano: seracs en el frente del glaciar y tempanos en los lagos de deshielo;
    //  - el mar: tempanos que flotan en invierno (draw_features los muestra segun la estacion).
    const int FS = 8;
    const float fslot = CHUNK_SIZE / FS;
    for (int iz = 0; iz < FS && ch.feat_count < CHUNK_FEATS; iz++)
        for (int ix = 0; ix < FS && ch.feat_count < CHUNK_FEATS; ix++) {
            uint32_t h = hash2(cx * FS + ix, cz * FS + iz, t->seed ^ 0xFEA7u);
            float r0 = (float)(h & 0xFFFF) / 65535.0f, r1 = (float)((h >> 16) & 0xFF) / 255.0f, r2 = (float)(h >> 24) / 255.0f;
            float x = ox + (ix + 0.15f + 0.7f * r1) * fslot, z = oz + (iz + 0.15f + 0.7f * r2) * fslot;
            if (x * x + z * z < 90.0f * 90.0f || near_site(t->world, x, z)) continue;
            float k[REGION_COUNT];
            world_region_weights(t->world, x, z, k);
            float y = terrain_height(t, x, z), e = world_edge_distance(t->world, x, z);
            WaterKind wk;
            float wl = world_water(t->world, x, z, 0.0f, &wk);
            int kind = -1;
            float scale = 0.8f + 0.5f * r1;
            if (k[REGION_FJORD] > 0.5f && y > SEA_LEVEL - 4.0f && y < SEA_LEVEL + 3.0f) { // la orilla del mar
                if (y < SEA_LEVEL - 1.5f && r0 < 0.06f) kind = VF_SEA_STACK;
                else if (r0 < 0.22f) kind = VF_BASALT;
            } else if (wk == WATER_SEA && y < SEA_LEVEL - 4.0f) { // mar adentro
                if (r0 < 0.09f) kind = VF_FLOE, y = SEA_LEVEL, scale = 0.7f + 0.8f * r1;
            } else if (k[REGION_DESERT] < 0.5f && wk == WATER_RIVER && r0 < 0.3f && world_river_bank(t->world, x, z, NULL) > 0.5f) {
                kind = VF_BOULDER; // en el cauce
            } else if (k[REGION_DESERT] < 0.5f && wl < -1e8f && r0 < 0.22f && world_river_bank(t->world, x, z, NULL) > 0.6f) {
                kind = VF_BOULDER; // en la orilla
            } else if (k[REGION_DESERT] > 0.6f && wl < -1e8f) {
                bool canyon = e > 85.0f && e < 260.0f; // el cañon al pie del muro de estratos
                if (canyon && r0 < 0.07f) kind = VF_MESA, scale = 0.8f + 0.6f * r1;
                else if (r0 < 0.012f) kind = VF_HOODOO;
                else if (r0 < 0.02f) kind = VF_CHALK;
                else if (r0 < 0.024f) kind = VF_MESA;
            } else if (k[REGION_HIGHLAND] > 0.5f) {
                if (e < 210.0f && wl < -1e8f && r0 < 0.28f) kind = VF_SERAC; // el frente del glaciar
                else if (wk == WATER_LAKE && wl > y + 2.0f && r0 < 0.05f) kind = VF_ICEBERG, y = wl;
            }
            if (kind < 0) continue;
            ch.feat[ch.feat_count++] = (ChunkFeature){ x, y, z, r2 * 2.0f * PI, scale, (unsigned char)kind, (unsigned char)(h >> 8) };
        }
    ch.model = make_model(&b);
    ch.tri = MemAlloc(b.count * TRI_ATTRS * sizeof(float));
    memcpy(ch.tri, abuf, b.count * TRI_ATTRS * sizeof(float));
    paint_chunk(t, &ch, false);
    // Los colores cambian con las estaciones: buffer dinamico.
    UploadMesh(&ch.model.meshes[0], true);
    sample_water(t, &ch, cx, cz, (const float (*)[CHUNK_CELLS + 1])hgrid);
    mesh_water(t, &ch);
    return ch;
}

static void free_chunk(Chunk *c) {
    UnloadModel(c->model);
    free_water(c);
    MemFree(c->tri);
    c->tri = NULL;
    if (c->wbody) MemFree(c->wbody), MemFree(c->wground), MemFree(c->wcell), MemFree(c->wfoam);
    c->wbody = NULL, c->wground = NULL, c->wcell = NULL, c->wfoam = NULL;
    c->loaded = false;
}

// ------------------------------------------------------------------ el horizonte lejano
// Una malla gruesa (una celda por chunk) que cubre FAR_CELLS chunks por lado alrededor del
// jugador, con un hueco donde estan los chunks cargados: asi las montañas, el muro, el hielo
// y el mar se ven a lo lejos. Sin arboles ni textura; el bosque se tiñe de copas oscuras y lo
// lejano se pierde en la bruma del cielo. Los datos del mundo de cada vertice se guardan y,
// al moverse el jugador, solo se piden los de las filas nuevas.
#define FAR_CELLS 100
#define FAR_VERTS (FAR_CELLS + 1)
#define FAR_HAZE_NEAR 350.0f
typedef struct {
    float h, k[REGION_COUNT], edge, glacier, wall;
    WaterBodies wb; // sus aguas: far_fill las resuelve en la estacion de las mallas
} FarVert;
static struct {
    bool ready, valid;
    int gx0, gz0; // chunk de la esquina
    FarVert v[FAR_VERTS * FAR_VERTS], tmp[FAR_VERTS * FAR_VERTS];
    Model model;
    Color haze;
    TerrainLook look;
} g_far;

static void far_sample(const Terrain *t, FarVert *f, int gx, int gz) {
    float x = gx * CHUNK_SIZE, z = gz * CHUNK_SIZE;
    world_water_bodies(t->world, x, z, &f->wb);
    f->h = f->wb.ground > -1e8f ? f->wb.ground : world_height(t->world, x, z);
    world_region_weights(t->world, x, z, f->k);
    f->edge = world_edge_distance(t->world, x, z);
    f->glacier = world_glacier_reach(t->world, x, z);
    f->wall = world_forest_wall(t->world, x, z);
}

// Mueve la ventana de datos a la nueva esquina: copia lo que ya estaba y pide lo nuevo.
static void far_shift(const Terrain *t, int gx0, int gz0) {
    for (int j = 0; j < FAR_VERTS; j++)
        for (int i = 0; i < FAR_VERTS; i++) {
            int oi = gx0 + i - g_far.gx0, oj = gz0 + j - g_far.gz0;
            FarVert *dst = &g_far.tmp[j * FAR_VERTS + i];
            if (g_far.valid && oi >= 0 && oj >= 0 && oi < FAR_VERTS && oj < FAR_VERTS) *dst = g_far.v[oj * FAR_VERTS + oi];
            else far_sample(t, dst, gx0 + i, gz0 + j);
        }
    memcpy(g_far.v, g_far.tmp, sizeof(g_far.v));
    g_far.gx0 = gx0, g_far.gz0 = gz0, g_far.valid = true;
}

static void far_fill(const Terrain *t) {
    Mesh *m = &g_far.model.meshes[0];
    const Vector3 sun = Vector3Normalize((Vector3){ -0.45f, 1.0f, -0.3f });
    const Color sea = { 36, 72, 104, 255 }, lake = { 52, 96, 128, 255 }, glacial = { 84, 168, 178, 255 };
    const Color ice = { 198, 220, 236, 255 };
    float ccx = (t->center_cx + 0.5f) * CHUNK_SIZE, ccz = (t->center_cz + 0.5f) * CHUNK_SIZE;
    g_perma_snow = t->plain + 40.0f;
    // El agua de cada vertice en la estacion de las mallas: su nivel (un cauce seco, ninguno) y su hielo.
    static float wlv[FAR_VERTS * FAR_VERTS], wice[FAR_VERTS * FAR_VERTS];
    static unsigned char wkind[FAR_VERTS * FAR_VERTS];
    for (int j = 0; j < FAR_VERTS; j++)
        for (int i = 0; i < FAR_VERTS; i++) {
            int v = j * FAR_VERTS + i;
            WaterNow n;
            wlv[v] = world_water_resolve(t->world, &g_far.v[v].wb, (g_far.gx0 + i) * CHUNK_SIZE, (g_far.gz0 + j) * CHUNK_SIZE, &t->meshed, &n);
            wice[v] = n.ice, wkind[v] = (unsigned char)n.kind;
        }
    int tri = 0;
    for (int j = 0; j < FAR_CELLS; j++)
        for (int i = 0; i < FAR_CELLS; i++) {
            int gx = g_far.gx0 + i, gz = g_far.gz0 + j;
            bool hole = abs(gx - t->center_cx) <= CHUNK_RADIUS && abs(gz - t->center_cz) <= CHUNK_RADIUS;
            const int vi[4] = { j * FAR_VERTS + i, j * FAR_VERTS + i + 1, (j + 1) * FAR_VERTS + i, (j + 1) * FAR_VERTS + i + 1 };
            const FarVert *q[4] = { &g_far.v[vi[0]], &g_far.v[vi[1]], &g_far.v[vi[2]], &g_far.v[vi[3]] };
            Vector3 p[4];
            for (int c = 0; c < 4; c++)
                p[c] = (Vector3){ (gx + (c & 1)) * CHUNK_SIZE, fmaxf(q[c]->h, wlv[vi[c]]), (gz + (c >> 1)) * CHUNK_SIZE };
            const int idx[2][3] = { { 0, 2, 1 }, { 1, 2, 3 } };
            for (int s = 0; s < 2; s++, tri++) {
                float *v = &m->vertices[tri * 9];
                unsigned char *col = &m->colors[tri * 12];
                if (hole) { // triangulo degenerado: el hueco de los chunks cargados
                    memset(v, 0, 9 * sizeof(float));
                    continue;
                }
                const FarVert *a = q[idx[s][0]], *b = q[idx[s][1]], *c = q[idx[s][2]];
                const int va = vi[idx[s][0]], vb = vi[idx[s][1]], vc = vi[idx[s][2]];
                Vector3 p0 = p[idx[s][0]], p1 = p[idx[s][1]], p2 = p[idx[s][2]];
                Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(p1, p0), Vector3Subtract(p2, p0)));
                float at[TRI_ATTRS] = { 0 };
                at[A_H] = (a->h + b->h + c->h) / 3.0f;
                at[A_SLOPE] = 1.0f - n.y;
                at[A_PATCH] = 0.5f;
                at[A_DESERT] = (a->k[REGION_DESERT] + b->k[REGION_DESERT] + c->k[REGION_DESERT]) / 3.0f;
                at[A_FOREST] = (a->k[REGION_FOREST] + b->k[REGION_FOREST] + c->k[REGION_FOREST]) / 3.0f;
                at[A_HIGH] = (a->k[REGION_HIGHLAND] + b->k[REGION_HIGHLAND] + c->k[REGION_HIGHLAND]) / 3.0f;
                at[A_FJORD] = (a->k[REGION_FJORD] + b->k[REGION_FJORD] + c->k[REGION_FJORD]) / 3.0f;
                at[A_SHORE] = 99.0f;
                at[A_EDGE] = (a->edge + b->edge + c->edge) / 3.0f;
                at[A_GLACIER] = (a->glacier + b->glacier + c->glacier) / 3.0f;
                at[A_WALL] = (a->wall + b->wall + c->wall) / 3.0f;
                float water = (wlv[va] + wlv[vb] + wlv[vc]) / 3.0f;
                Color base;
                if (water > at[A_H]) { // agua: el mar, los lagos y, en el altiplano, el deshielo turquesa; con su hielo
                    int w = wlv[va] > -1e8f ? va : wlv[vb] > -1e8f ? vb : vc;
                    base = wkind[w] == WATER_SEA ? sea : at[A_HIGH] > 0.5f ? glacial : lake;
                    base = mixc(base, ice, wice[w]);
                } else {
                    base = ground_color(&t->look, at, at[A_H] - t->plain);
                    float trees = Clamp(tree_density(a->k, 0.0f) / 0.62f, 0.0f, 1.0f) * (at[A_H] < t->look.snowline ? 0.85f : 0.0f) *
                                  (1.0f - Clamp(at[A_WALL] * 2.0f, 0.0f, 1.0f)); // el muro tras el canal: roca, sin bosque
                    base = mixc(base, tree_color(&t->look, (float[TRI_ATTRS]){ [A_PATCH] = 0.6f, [A_KIND] = KIND_CANOPY, [A_H] = at[A_H] }), trees);
                    base = shade(base, 0.55f + 0.45f * fmaxf(0.0f, Vector3DotProduct(n, sun)));
                }
                // La bruma: lo lejano toma el color del cielo.
                float mx = (p0.x + p1.x + p2.x) / 3.0f - ccx, mz = (p0.z + p1.z + p2.z) / 3.0f - ccz;
                float d = sqrtf(mx * mx + mz * mz), hz = Clamp((d - FAR_HAZE_NEAR) / (FAR_CELLS * 0.5f * CHUNK_SIZE - FAR_HAZE_NEAR), 0.0f, 1.0f);
                Color fin = mixc(base, g_far.haze, 0.8f * hz * (2.0f - hz));
                Vector3 pts[3] = { p0, p1, p2 };
                for (int k = 0; k < 3; k++) {
                    v[k * 3 + 0] = pts[k].x, v[k * 3 + 1] = pts[k].y, v[k * 3 + 2] = pts[k].z;
                    col[k * 4 + 0] = fin.r, col[k * 4 + 1] = fin.g, col[k * 4 + 2] = fin.b, col[k * 4 + 3] = 255;
                }
            }
        }
    UpdateMeshBuffer(*m, 0, m->vertices, m->vertexCount * 3 * sizeof(float), 0);
    UpdateMeshBuffer(*m, 3, m->colors, m->vertexCount * 4, 0);
    g_far.look = t->look;
}

static void far_update(const Terrain *t) {
    if (!g_far.ready) {
        Mesh mesh = { 0 };
        mesh.triangleCount = FAR_CELLS * FAR_CELLS * 2;
        mesh.vertexCount = mesh.triangleCount * 3;
        mesh.vertices = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
        mesh.colors = MemAlloc(mesh.vertexCount * 4);
        UploadMesh(&mesh, true);
        g_far.model = LoadModelFromMesh(mesh);
        g_far.ready = true;
        if (!g_far.haze.a) g_far.haze = (Color){ 168, 196, 214, 255 };
    }
    far_shift(t, t->center_cx - FAR_CELLS / 2, t->center_cz - FAR_CELLS / 2);
    far_fill(t);
}

void terrain_set_haze(Terrain *t, Color haze) {
    Color o = g_far.haze;
    if (abs(o.r - haze.r) + abs(o.g - haze.g) + abs(o.b - haze.b) < 9) return;
    g_far.haze = haze;
    if (g_far.ready && t->has_center) far_fill(t);
}

void terrain_init(Terrain *t, unsigned seed) {
    if (!g_atlas_tried) { // una vez: el atlas sirve para cualquier mundo
        g_atlas_tried = true;
        Image img = LoadImage(platform_asset_path("assets/terrain/texturas.png"));
        platform_note_asset("assets/terrain/texturas.png", img.data != NULL);
        if (img.data) {
            // Cuanto aclarar cada celda: lo que la lleve a un gris medio de 205.
            Color *px = LoadImageColors(img);
            int cell = img.width / TEX_COLS;
            for (int i = 0; i < 16 && cell > 0; i++) {
                float sum = 0.0f;
                for (int y = 0; y < cell; y++)
                    for (int x = 0; x < cell; x++) {
                        Color c = px[((i / TEX_COLS) * cell + y) * img.width + (i % TEX_COLS) * cell + x];
                        sum += (c.r + c.g + c.b) / 3.0f;
                    }
                g_tex_boost[i] = Clamp(205.0f / fmaxf(sum / (cell * cell), 1.0f), 1.0f, 1.6f);
            }
            UnloadImageColors(px);
            g_atlas = LoadTextureFromImage(img);
            SetTextureFilter(g_atlas, TEXTURE_FILTER_POINT); // pixeles nitidos (low-res)
            UnloadImage(img);
        }
    }
    memset(t, 0, sizeof(*t));
    g_far.valid = false; // otro mundo: el horizonte se vuelve a pedir
    t->seed = seed;
    t->world = world_for_seed(seed);
    t->plain = terrain_plain_height(t);
    // Aspecto inicial: verano seco, sin nieve, el glaciar atras y los cauces llenos.
    t->look = (TerrainLook){ .greenness = 0.4f, .snowline = t->plain + 40.0f, .temp = 20.0f, .temp_late = 20.0f, .melt = 1.0f };
    t->meshed = look_season(&t->look);
}

static bool look_changed(const TerrainLook *a, const TerrainLook *b) {
    return fabsf(a->greenness - b->greenness) > 0.02f || fabsf(a->autumn - b->autumn) > 0.02f ||
           fabsf(a->snow_cover - b->snow_cover) > 0.02f || fabsf(a->wetness - b->wetness) > 0.03f ||
           fabsf(a->snowline - b->snowline) > 0.25f || fabsf(a->glacier - b->glacier) > 0.01f;
}

// ¿Cambio el agua lo bastante para rehacer sus mallas? (la crecida, unos cm; el hielo, un cuarto de grado)
static bool season_changed(const WaterSeason *a, const WaterSeason *b) {
    return fabsf(a->flood - b->flood) > 0.03f || fabsf(a->temp - b->temp) > 0.25f || fabsf(a->temp_late - b->temp_late) > 0.25f ||
           fabsf(a->melt - b->melt) > 0.02f || fabsf(a->sea_ice - b->sea_ice) > 0.02f;
}

void terrain_set_look(Terrain *t, const TerrainLook *look) {
    bool repaint = look_changed(&t->look, look);
    // El agua que se consulta (terrain_water, terrain_ice) sigue al clima en cada cuadro; sus mallas, de a saltos.
    t->look.flood = look->flood, t->look.temp = look->temp, t->look.temp_late = look->temp_late;
    t->look.melt = look->melt, t->look.sea_ice = look->sea_ice, t->look.floes = look->floes;
    WaterSeason now = look_season(&t->look);
    bool water = season_changed(&t->meshed, &now);
    if (water) {
        t->meshed = now;
        for (int i = 0; i < CHUNK_SLOTS; i++) t->slots[i].water_dirty = t->slots[i].loaded;
    }
    if (repaint) {
        t->look = *look;
        for (int i = 0; i < CHUNK_SLOTS; i++)
            if (t->slots[i].loaded) paint_chunk(t, &t->slots[i], true);
    }
    if ((repaint || water) && g_far.ready && t->has_center) far_fill(t);
    // Las mallas del agua, de a unos chunks por cuadro: la estacion cambia despacio.
    for (int i = 0, budget = 8; i < CHUNK_SLOTS && budget > 0; i++)
        if (t->slots[i].loaded && t->slots[i].water_dirty) mesh_water(t, &t->slots[i]), budget--;
}

static float fract1(float x) { return x - floorf(x); }

void terrain_draw_water(const Terrain *t, float time) {
    if (!t->has_center) return;
    // Oleaje leve: el brillo del agua respira (los colores de sus vertices van con 1/0,94: mesh_water); el
    // hielo no. La espuma late con las olas. Con el shader, todo eso lo hace el (los uniformes de este cuadro).
    unsigned char glow = (unsigned char)(255.0f * (0.94f + 0.06f * sinf(time * 1.3f)));
    unsigned char foam = (unsigned char)(255.0f * (0.45f + 0.25f * sinf(time * 1.7f)));
    if (g_water_shader_ok) {
        float ox = (float)t->center_cx * CHUNK_SIZE, oz = (float)t->center_cz * CHUNK_SIZE;
        Vector4 origin = { ox, 0.0f, oz, fmodf(time, 120.0f) };
        float a = fract1(time / 1.6f);
        Vector4 phase = { a, fract1(a + 0.5f), fract1(time * 0.021f), fract1(time * 0.013f) };
        Vector4 pulse = { sinf(time * 1.7f), cosf(time * 1.7f), 0.0f, 0.0f };
        Vector4 bodies[TERRAIN_RIPPLES];
        for (int i = 0; i < TERRAIN_RIPPLES; i++) { // sin nadie: lejos, sin espuma (y radio 1: el shader divide)
            bodies[i] = (Vector4){ 500.0f, 500.0f, 1.0f, 0.0f };
            if (i >= g_ripple_count) continue;
            const Vector4 *r = &g_ripples[i];
            float pulse_r = 1.0f + 0.12f * sinf(time * 3.1f + (float)i * 1.7f); // la onda que va y viene
            bodies[i] = (Vector4){ r->x - ox, r->y - oz, fmaxf(0.3f, r->z * pulse_r), Clamp(r->w, 0.0f, 1.0f) };
        }
        SetShaderValue(g_water_shader, g_wloc_origin, &origin, SHADER_UNIFORM_VEC4);
        SetShaderValue(g_water_shader, g_wloc_phase, &phase, SHADER_UNIFORM_VEC4);
        SetShaderValue(g_water_shader, g_wloc_pulse, &pulse, SHADER_UNIFORM_VEC4);
        if (g_wloc_bodies >= 0) SetShaderValueV(g_water_shader, g_wloc_bodies, bodies, SHADER_UNIFORM_VEC4, TERRAIN_RIPPLES);
        glow = 255;
    }
    const Color tint[W_MESHES] = { { glow, glow, glow, 255 }, WHITE, { 255, 255, 255, foam } };
    for (int m = 0; m < W_MESHES; m++)
        for (int i = 0; i < CHUNK_SLOTS; i++) {
            const Chunk *ch = &t->slots[i];
            if (ch->loaded && ch->has_water[m]) DrawModel(ch->water[m], (Vector3){ 0 }, 1.0f, tint[m]);
        }
}

// La bruma de las cascadas: una mota blanda (un degradado redondo de 16x16), una vez.
static Texture2D g_mist_tex;
static bool g_mist_tried;

void terrain_draw_falls(const Terrain *t, float time) {
    if (!t->has_center || !t->world || t->world->fall_count == 0) return;
    const World *w = t->world;
    const Color sheet = { 150, 196, 220, 200 }, shine = { 236, 246, 252, 230 }, ice = { 198, 220, 236, 240 };
    float cx = ((float)t->center_cx + 0.5f) * CHUNK_SIZE, cz = ((float)t->center_cz + 0.5f) * CHUNK_SIZE;
    float reach = ((float)CHUNK_RADIUS + 0.5f) * CHUNK_SIZE;
    const WaterSeason *season = &t->meshed; // como el agua de las mallas
    if (!g_mist_tried) {
        g_mist_tried = true;
        Image img = GenImageGradientRadial(16, 16, 0.0f, WHITE, (Color){ 255, 255, 255, 0 }); // blanca hasta el borde transparente
        g_mist_tex = LoadTextureFromImage(img);
        UnloadImage(img);
    }
    // Lo que se ve de cada cascada cercana: la cortina baja por la textura de las ondas (ahora, aguas abajo).
    int near[64], nn = 0;
    for (int i = 0; i < w->fall_count && nn < 64; i++) {
        const Waterfall *f = &w->falls[i];
        if (fabsf(f->x - cx) > reach || fabsf(f->z - cz) > reach) continue;
        if (f->river >= 0 && world_channel_state(w, f->river, season) == CHANNEL_DRY) continue; // seco: no cae nada
        near[nn++] = i;
    }
    if (!nn) return;
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling(); // la cortina se ve de los dos lados
    if (g_water_tex.id) rlSetTexture(g_water_tex.id);
    rlBegin(RL_QUADS);
    float slide = fract1(time * 0.9f); // la textura baja 0,9 vueltas por segundo (5 m/s): se repite sin saltos
    for (int n = 0; n < nn; n++) {
        int i = near[n];
        const Waterfall *f = &w->falls[i];
        // Su rio: con poca agua, una cortina angosta; helado, de hielo y quieto, sin espuma ni bruma.
        float full = f->river >= 0 ? Clamp(world_river_share(w, f->river, season) / WORLD_FULL_SHARE, 0.0f, 1.0f) : 1.0f;
        float frozen = f->river >= 0 ? world_river_ice(w, f->river, season) : 0.0f;
        float lift = f->river >= 0 ? world_river_offset(w, f->river, season) : 0.5f * season->flood;
        float ax = cosf(f->yaw), az = -sinf(f->yaw), hw = (f->width * 0.5f + 0.5f) * (0.35f + 0.65f * full); // a lo ancho de la caida
        float top = f->top + lift + 0.05f, bot = f->bottom + lift + 0.05f;
        float v0 = frozen > 0.5f ? 0.0f : -slide, v1 = v0 + (top - bot) / 6.0f;
        for (int s = 0; s < 4; s++) { // cuatro tiras, cada una con su brillo que baja
            float u0 = -1.0f + 0.5f * (float)s, u1 = u0 + 0.5f;
            Color c = mixc(sheet, shine, 0.35f * (0.5f + 0.5f * sinf(time * 6.0f + (float)s * 1.7f + (float)i)));
            c = mixc(c, ice, frozen);
            float tu0 = u0 * hw / 3.0f, tu1 = u1 * hw / 3.0f;
            Vector3 a = { f->x + ax * u0 * hw, top, f->z + az * u0 * hw }, b = { f->x + ax * u1 * hw, top, f->z + az * u1 * hw };
            Vector3 d = { f->bx + ax * u0 * hw, bot, f->bz + az * u0 * hw }, e = { f->bx + ax * u1 * hw, bot, f->bz + az * u1 * hw };
            rlColor4ub(c.r, c.g, c.b, c.a);
            rlTexCoord2f(tu0, v0), rlVertex3f(a.x, a.y, a.z);
            rlTexCoord2f(tu0, v1), rlVertex3f(d.x, d.y, d.z);
            rlTexCoord2f(tu1, v1), rlVertex3f(e.x, e.y, e.z);
            rlTexCoord2f(tu1, v0), rlVertex3f(b.x, b.y, b.z);
        }
    }
    rlEnd();
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    for (int n = 0; n < nn; n++) {
        int i = near[n];
        const Waterfall *f = &w->falls[i];
        float full = f->river >= 0 ? Clamp(world_river_share(w, f->river, season) / WORLD_FULL_SHARE, 0.0f, 1.0f) : 1.0f;
        float frozen = f->river >= 0 ? world_river_ice(w, f->river, season) : 0.0f;
        if (frozen > 0.5f) continue;
        float lift = f->river >= 0 ? world_river_offset(w, f->river, season) : 0.5f * season->flood;
        float hw = (f->width * 0.5f + 0.5f) * (0.35f + 0.65f * full), bot = f->bottom + lift + 0.05f;
        // La espuma al pie: un abanico blanco que late.
        unsigned char alpha = (unsigned char)(200.0f * (0.6f + 0.4f * sinf(time * 3.0f + (float)i)) * (1.0f - frozen));
        float r = hw * 1.6f;
        for (int k = 0; k < 10; k++) {
            float a0 = (float)k / 10.0f * 2.0f * PI, a1 = (float)(k + 1) / 10.0f * 2.0f * PI;
            rlColor4ub(250, 252, 255, alpha);
            rlVertex3f(f->bx, bot + 0.08f, f->bz);
            rlVertex3f(f->bx + cosf(a0) * r, bot + 0.06f, f->bz + sinf(a0) * r);
            rlVertex3f(f->bx + cosf(a1) * r, bot + 0.06f, f->bz + sinf(a1) * r);
        }
    }
    rlEnd();
    // La bruma al pie: motas blandas que suben, se abren y se desvanecen (de cara a la camara, sin tapar lo de
    // atras: no escriben la profundidad). Mas en las caidas altas y anchas.
    if (g_mist_tex.id) {
        Matrix view = rlGetMatrixModelview();
        Vector3 right = { view.m0, view.m4, view.m8 }, up = { view.m1, view.m5, view.m9 };
        rlDrawRenderBatchActive();
        rlDisableDepthMask();
        rlSetTexture(g_mist_tex.id);
        rlBegin(RL_QUADS);
        for (int n = 0; n < nn; n++) {
            int i = near[n];
            const Waterfall *f = &w->falls[i];
            float full = f->river >= 0 ? Clamp(world_river_share(w, f->river, season) / WORLD_FULL_SHARE, 0.0f, 1.0f) : 1.0f;
            float frozen = f->river >= 0 ? world_river_ice(w, f->river, season) : 0.0f;
            if (frozen > 0.5f) continue;
            float lift = f->river >= 0 ? world_river_offset(w, f->river, season) : 0.5f * season->flood;
            float hw = (f->width * 0.5f + 0.5f) * (0.35f + 0.65f * full), bot = f->bottom + lift;
            float drop = f->top - f->bottom, power = Clamp(drop / 8.0f, 0.3f, 1.0f) * (0.4f + 0.6f * full);
            int puffs = 4 + (int)(8.0f * power);
            for (int k = 0; k < puffs; k++) {
                float seed = (float)(hash2(i, k, 0xB2u) & 1023u) / 1023.0f;
                float age = fract1(time * 0.22f + seed); // cada mota vive unos 4,5 s
                float ang = seed * 2.0f * PI + (float)k * 2.4f, spread = hw * (0.5f + 1.3f * age);
                Vector3 at = { f->bx + cosf(ang) * spread, bot + 0.4f + (1.0f + 0.4f * drop) * power * age, f->bz + sinf(ang) * spread };
                float size = (1.2f + 2.4f * age) * (0.6f + 0.4f * power) * (0.6f + 0.25f * hw);
                unsigned char alpha = (unsigned char)(150.0f * power * (1.0f - age) * fminf(1.0f, age * 5.0f) * (1.0f - frozen));
                Vector3 rx = Vector3Scale(right, size), uy = Vector3Scale(up, size);
                rlColor4ub(240, 246, 250, alpha);
                rlTexCoord2f(0.0f, 0.0f), rlVertex3f(at.x - rx.x + uy.x, at.y - rx.y + uy.y, at.z - rx.z + uy.z);
                rlTexCoord2f(0.0f, 1.0f), rlVertex3f(at.x - rx.x - uy.x, at.y - rx.y - uy.y, at.z - rx.z - uy.z);
                rlTexCoord2f(1.0f, 1.0f), rlVertex3f(at.x + rx.x - uy.x, at.y + rx.y - uy.y, at.z + rx.z - uy.z);
                rlTexCoord2f(1.0f, 0.0f), rlVertex3f(at.x + rx.x + uy.x, at.y + rx.y + uy.y, at.z + rx.z + uy.z);
            }
        }
        rlEnd();
        rlSetTexture(0);
        rlDrawRenderBatchActive();
        rlEnableDepthMask();
    }
    rlEnableBackfaceCulling();
}

static bool in_range(int cx, int cz, int ccx, int ccz) {
    return abs(cx - ccx) <= CHUNK_RADIUS && abs(cz - ccz) <= CHUNK_RADIUS;
}

void terrain_update(Terrain *t, Vector3 pos) {
    int ccx = (int)floorf(pos.x / CHUNK_SIZE);
    int ccz = (int)floorf(pos.z / CHUNK_SIZE);
    if (t->has_center && ccx == t->center_cx && ccz == t->center_cz) return;
    t->center_cx = ccx;
    t->center_cz = ccz;
    t->has_center = true;

    // 1) Libera los chunks que quedaron fuera del radio.
    for (int i = 0; i < CHUNK_SLOTS; i++) {
        Chunk *c = &t->slots[i];
        if (c->loaded && !in_range(c->cx, c->cz, ccx, ccz)) free_chunk(c);
    }
    // 2) Genera los que faltan en los huecos libres.
    for (int dz = -CHUNK_RADIUS; dz <= CHUNK_RADIUS; dz++) {
        for (int dx = -CHUNK_RADIUS; dx <= CHUNK_RADIUS; dx++) {
            int cx = ccx + dx, cz = ccz + dz;
            bool present = false;
            for (int i = 0; i < CHUNK_SLOTS && !present; i++)
                present = t->slots[i].loaded && t->slots[i].cx == cx && t->slots[i].cz == cz;
            if (present) continue;
            for (int i = 0; i < CHUNK_SLOTS; i++) {
                if (!t->slots[i].loaded) {
                    t->slots[i] = build_chunk(t, cx, cz);
                    break;
                }
            }
        }
    }
    far_update(t);
}

// Las formaciones de voxeles de los chunks cargados (los tempanos de los lagos suben con la crecida). Los del
// mar salen con el frio (TerrainLook.floes: cada uno desde su umbral) y van a la deriva, meciendose.
static void draw_features(const Terrain *t) {
    float time = (float)GetTime();
    for (int i = 0; i < CHUNK_SLOTS; i++) {
        const Chunk *ch = &t->slots[i];
        if (!ch->loaded) continue;
        for (int k = 0; k < ch->feat_count; k++) {
            const ChunkFeature *f = &ch->feat[k];
            Vector3 at = { f->x, f->y + (f->kind == VF_ICEBERG ? t->look.flood : 0.0f), f->z };
            float yaw = f->yaw;
            if (f->kind == VF_FLOE) {
                float ph = (float)(hash2((int)f->x, (int)f->z, 0xF10Eu) & 1023u) / 1023.0f;
                if (ph >= t->look.floes * 0.8f) continue;
                ph *= 2.0f * PI;
                at.x += 7.0f * sinf(time * 0.021f + ph), at.z += 5.0f * cosf(time * 0.017f + ph * 1.3f);
                at.y += 0.06f * sinf(time * 0.9f + ph);
                yaw += 0.2f * sinf(time * 0.013f + ph);
            }
            voxs_draw_feature((VoxFeature)f->kind, f->variant, at, yaw, f->scale, WHITE);
        }
    }
}

void terrain_draw(const Terrain *t) {
    if (g_far.ready && t->has_center) DrawModel(g_far.model, (Vector3){ 0, -0.4f, 0 }, 1.0f, WHITE); // bajo los chunks: sin costuras a la vista
    for (int i = 0; i < CHUNK_SLOTS; i++)
        if (t->slots[i].loaded) DrawModel(t->slots[i].model, (Vector3){ 0 }, 1.0f, WHITE);
    draw_features(t);
}

void terrain_unload(Terrain *t) {
    for (int i = 0; i < CHUNK_SLOTS; i++) {
        if (t->slots[i].loaded) free_chunk(&t->slots[i]);
    }
    if (g_far.ready) UnloadModel(g_far.model);
    g_far.ready = g_far.valid = false;
    t->has_center = false;
}

void terrain_unload_gpu(void) {
    if (g_atlas.id) UnloadTexture(g_atlas);
    if (g_water_tex.id) UnloadTexture(g_water_tex);
    if (g_ice_tex.id) UnloadTexture(g_ice_tex);
    if (g_foam_tex.id) UnloadTexture(g_foam_tex);
    if (g_water_shader_ok) UnloadShader(g_water_shader);
    if (g_mist_tex.id) UnloadTexture(g_mist_tex);
    terrain_forget_gpu();
}

void terrain_forget_gpu(void) {
    g_atlas = (Texture2D){ 0 }, g_water_tex = (Texture2D){ 0 }, g_ice_tex = (Texture2D){ 0 }, g_foam_tex = (Texture2D){ 0 };
    g_water_shader = (Shader){ 0 }, g_water_shader_ok = false;
    g_mist_tex = (Texture2D){ 0 }, g_mist_tried = false;
    g_atlas_tried = g_water_tex_tried = false;
    g_far.ready = g_far.valid = false;
}
