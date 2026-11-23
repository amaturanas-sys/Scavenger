// Objetos sueltos en el mundo (fogatas, tiendas, lena, obras terminadas...).
// Cada uno es un id del inventario de assets: se dibuja con su modelo si ya
// fue importado; si no, las obras y las viviendas de la tribu con el kit de
// edificios (bk_for_item, src/sim/buildkit.h) y lo demas con un marcador de
// sus medidas.
#ifndef ESTEPA_PROPS_H
#define ESTEPA_PROPS_H

#include "raylib.h"
#include "sim/inventory.h"

#define PROPS_MAX 128
#define MODEL_CACHE_MAX 320 // con el decorado de OpenGameArt (41 modelos, y sus variantes de estacion)

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
        int variant; // -1: modelo base; si no, la estacion de "<nombre>@<estacion>.glb" (o el cuerpo: props_draw_worn)
        bool posed;  // una prenda: su malla quedo con la pose del ultimo que la llevo (props_draw_worn)
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

// Dibuja un id del inventario: modelo si existe, si no el kit o un marcador.
// grow en [0, 1] escala la altura (obras a medio construir: el marcador con su
// andamio, hasta que se terminan).
void props_draw_item(Props *p, const InvItem *item, Vector3 pos, float yaw, float grow);
// La luz con que se tiñen los modelos (WHITE: como siempre). En una cueva (src/game/cave_game.c), la del lugar.
void props_set_tint(Color tint);
Color props_get_tint(void);
// El fantasma de una obra por ubicar (src/game/place_game.c): el modelo (o la caja de sus
// medidas) tenido con tint, translucido.
void props_draw_ghost(Props *p, const InvItem *item, Vector3 pos, float yaw, Color tint);
// Fogata u hoguera (scale: la de hearth.c). Sin modelo, props_draw no las dibuja: lo hace hearth.c.
bool props_is_fire(const InvItem *item, float *scale);
void props_draw(Props *p);
// true si el id ya tiene modelo importado.
bool props_has_model(Props *p, const InvItem *item);
// Dibuja con el clip de animacion pedido (por nombre); si el modelo no lo trae,
// usa "idle"; si no hay modelo, el marcador. time: segundos (el clip se repite).
void props_draw_item_anim(Props *p, const InvItem *item, Vector3 pos, float yaw, const char *clip, float time);
// El clip que esta jugando un personaje y desde cuando: un golpe empieza en su primer cuadro y, si no se repite
// (anim_loops), queda en el ultimo. Uno por personaje (el jugador, cada uno de la tropa, cada enemigo).
typedef struct {
    const char *clip;
    float start, rate; // desde cuando y a que velocidad (1, o la que hace caber el golpe en su ventana)
    float left;        // la ventana que quedaba en el cuadro anterior: si crece, es otro golpe con el mismo clip
} AnimTrack;
// Como props_draw_item_anim, con el arranque del clip en track (si cambia de clip, empieza de cero). fit: los
// segundos que le quedan al golpe en curso (anim_is_stroke), para que el clip quepa entero; 0, a su ritmo.
void props_draw_item_track(Props *p, const InvItem *item, Vector3 pos, float yaw, const char *clip, float time, float fit,
                           AnimTrack *track);
// El modelo del id con una transformacion cualquiera (un arma en la mano: src/game/actions_game.c). false si no
// tiene modelo (el que llama dibuja su marcador).
// El modelo con la piedra de la region biome (RegionKind: estepa, bosque, altiplano, fiordos, desierto) y lo que la
// cubre arriba: las rocas del decorado (v0.15.2), una variante por region, cargada la primera vez que se dibuja.
bool props_draw_item_biome(Props *p, const InvItem *item, Matrix transform, int biome);
// Donde esta un hueso (bone, por su nombre) del modelo animado, con el mismo clip, arranque y giro que
// props_draw_item_track: para poner en la mano lo que se lleva. false si no esta animado o no tiene ese hueso.
bool props_bone_point(Props *p, const InvItem *item, Vector3 pos, float yaw, const AnimTrack *track, float time, const char *bone,
                      Vector3 *out);
// Una prenda (casco, coraza, botas: un GLB con la piel del cuerpo, sus mismos huesos y sin clips propios) puesta en
// body, con la pose de su animacion en este cuadro (la de track, como props_draw_item_track). Usa la variante del
// cuerpo (nombre@mujer.glb, @gigante, @protagonista: tools/modelos/equipo.py) o, si no esta, la del comun.
void props_draw_worn(Props *p, const InvItem *wear, const InvItem *body, Vector3 pos, float yaw, const AnimTrack *track,
                     float time);
// true si el modelo del id es una prenda (con piel y sin clips): en el suelo se dibuja apoyado, en su pose de piel.
bool props_is_worn(Props *p, const InvItem *item);
bool props_draw_item_matrix(Props *p, const InvItem *item, Matrix transform);

#endif
