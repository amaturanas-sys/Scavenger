// Objetos sueltos en el mundo (fogatas, tiendas, lena, obras terminadas...).
// Cada uno es un id del inventario de assets: se dibuja con su modelo si ya
// fue importado, o con un marcador de sus medidas si no.
#ifndef ESTEPA_PROPS_H
#define ESTEPA_PROPS_H

#include "raylib.h"
#include "sim/inventory.h"

#define PROPS_MAX 128
#define MODEL_CACHE_MAX 64

typedef struct {
    const InvItem *item;
    Vector3 pos;
    float yaw;
    bool flying;  // lanzado, en el aire
    Vector3 vel;
} Prop;

typedef struct {
    const Inventory *inv;
    Prop items[PROPS_MAX];
    int count;
    struct {
        const InvItem *item;
        Model model;
        bool loaded;
    } cache[MODEL_CACHE_MAX];
    int cached;
} Props;

void props_init(Props *p, const Inventory *inv);
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
void props_draw(Props *p);

#endif
