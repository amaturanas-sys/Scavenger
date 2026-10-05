// Objetos sueltos en el mundo (fogatas, tiendas, lena, obras terminadas...).
// Cada uno es un id del inventario de assets: se dibuja con su modelo si ya
// fue importado, o con un marcador de sus medidas si no.
#ifndef ESTEPA_PROPS_H
#define ESTEPA_PROPS_H

#include "raylib.h"
#include "sim/inventory.h"

#define PROPS_MAX 128
#define MODEL_CACHE_MAX 128

typedef struct {
    const InvItem *item;
    Vector3 pos;
    float yaw;
    bool flying;  // lanzado, en el aire
    Vector3 vel;
    float condition; // estructuras: mantenimiento [0, 1] (src/game/disasters_game.c)
} Prop;

typedef struct {
    const Inventory *inv;
    Prop items[PROPS_MAX];
    int count;
    struct {
        const InvItem *item;
        Model model;
        bool loaded;
        ModelAnimation *anims; // clips del GLB (indice: assets/animaciones.tsv)
        int anim_count;
        int variant; // -1: modelo base; si no, la estacion de "<nombre>@<estacion>.glb"
    } cache[MODEL_CACHE_MAX];
    int cached;
    int season; // estacion actual (Season): elige la variante estacional si existe
} Props;

void props_init(Props *p, const Inventory *inv);
// Estacion actual. Un modelo puede traer variantes por estacion junto al base:
// yurta_comun@invierno.glb (nevada), arbusto@otono.glb... Si no hay, se usa el base.
void props_set_season(Props *p, int season);
// Sufijo de archivo de cada estacion (ASCII): primavera, verano, otono, invierno.
const char *props_season_suffix(int season);
void props_unload(Props *p);
// Agrega un objeto por id. Devuelve su indice, o -1 si el id no existe o no hay lugar.
int props_add(Props *p, const char *id, Vector3 pos, float yaw);
void props_remove(Props *p, int index);
// Indice del objeto mas cercano dentro de radius (opcionalmente solo los "tomables"), o -1.
int props_nearest(const Props *p, Vector3 from, float radius, bool takeable_only);
// Un objeto se puede tomar con la mano si es pequeno.
bool props_takeable(const InvItem *item);

// Dibuja un id del inventario: modelo si existe, si no un marcador.
// grow en [0, 1] escala la altura (obras a medio construir).
void props_draw_item(Props *p, const InvItem *item, Vector3 pos, float yaw, float grow);
// Fogata u hoguera (scale: la de hearth.c). Sin modelo, props_draw no las dibuja: lo hace hearth.c.
bool props_is_fire(const InvItem *item, float *scale);
void props_draw(Props *p);
// true si el id ya tiene modelo importado.
bool props_has_model(Props *p, const InvItem *item);
// Dibuja con el clip de animacion pedido (por nombre); si el modelo no lo trae,
// usa "idle"; si no hay modelo, el marcador. time: segundos (el clip se repite).
void props_draw_item_anim(Props *p, const InvItem *item, Vector3 pos, float yaw, const char *clip, float time);

#endif
