// Fauna del mundo (C puro; posiciones en el plano XZ, el juego pone la altura
// del terreno y el modelo del inventario). Cinco clases de animales:
//  - monturas (caballo, mula, burro, buey, camello, elefante): presas que se
//    doman con el lazo y se montan con silla;
//  - depredadores domables (lobo, perro, tigre, puma, halcon, cuervo): hostiles;
//    se doman peleando hasta debilitarlos, con el lazo y dandoles de comer;
//  - hostiles (oso, hiena, coyote, jabali): atacan siempre, no se doman;
//  - presas salvajes (antilope, reno, gacela, ciervo, liebre, ibice): se cazan;
//  - ganado (cabra, becerro): se pastorea, da leche, carne y piel.
//
// Relaciones entre animales:
//  - sociales: pastan en grupo y se mantienen juntos; los cazadores sociales
//    eligen la misma presa y la rodean; la alarma se contagia al grupo;
//  - solitarios: los cazadores acechan despacio (cuesta mas notarlos) y saltan
//    de cerca; dos adultos de la misma especie en el mismo territorio pelean y
//    el perdedor se marcha (rivalidad);
//  - huida: las presas huyen de los depredadores que notan, y de las personas
//    solo si les hicieron daño (a ellas o a su grupo). Depredadores y hostiles
//    no huyen salvo con la vida critica.
#ifndef ESTEPA_ANIMALS_H
#define ESTEPA_ANIMALS_H

#include <stdbool.h>

#include "body.h"
#include "health.h"
#include "rng.h"

typedef enum {
    // Monturas.
    SPECIES_HORSE,
    SPECIES_MULE,
    SPECIES_DONKEY,
    SPECIES_OX,
    SPECIES_CAMEL,
    SPECIES_ELEPHANT,
    // Depredadores domables.
    SPECIES_WOLF,
    SPECIES_DOG,
    SPECIES_TIGER,
    SPECIES_PUMA,
    SPECIES_FALCON,
    SPECIES_RAVEN,
    // Siempre hostiles.
    SPECIES_BEAR,
    SPECIES_HYENA,
    SPECIES_COYOTE,
    SPECIES_BOAR,
    // Presas salvajes.
    SPECIES_ANTELOPE,
    SPECIES_REINDEER,
    SPECIES_GAZELLE,
    SPECIES_DEER,
    SPECIES_HARE,
    SPECIES_IBEX,
    // Ganado.
    SPECIES_GOAT,
    SPECIES_CALF,
    SPECIES_COUNT
} Species;

typedef enum { CLASS_MOUNT, CLASS_TAMEABLE, CLASS_HOSTILE, CLASS_PREY, CLASS_LIVESTOCK } AnimalClass;

// Donde vive (para que aparezca): estepa, desierto, frio (alto, cerca de la nieve).
enum { HAB_STEPPE = 1, HAB_DESERT = 2, HAB_COLD = 4 };

typedef struct {
    const char *name;  // UTF-8
    const char *model; // id del inventario de assets
    AnimalClass cls;
    float speed;       // m/s a la carrera
    float walk;        // m/s al paso
    float size;        // m de largo: un depredador caza presas de hasta hunt_max
    float hp, damage, reach, cooldown;
    WoundKind wound;
    float sense;       // m: nota a depredadores (presa) u objetivos (cazador)
    float tame_chance; // monturas: probabilidad del lazo
    bool rideable;
    float ride_speed;  // multiplica la velocidad del jinete
    bool social;       // vive en grupo
    bool flier;        // ave
    bool defends;      // herido, embiste en vez de huir
    float hunt_max;    // talla maxima de presa (0: no caza)
    int meat, hide, milk; // al despiezar (carne, pieles) y leche por dia
    int group_min, group_max;
    int habitat;       // HAB_*
    float rarity;      // peso al elegir que aparece (mayor, mas comun)
} SpeciesDef;

typedef enum {
    ANIMAL_WILD,
    ANIMAL_BOUND,   // atado con el lazo, esperando comida (depredadores domables)
    ANIMAL_TAMED,   // domado: sigue a la tribu
    ANIMAL_SADDLED, // domado y ensillado: se monta
    ANIMAL_DEAD,
} AnimalState;

typedef enum {
    MODE_GRAZE,  // deambula o pasta
    MODE_FLEE,   // huye
    MODE_STALK,  // acecha (solitario, despacio)
    MODE_CHASE,  // persigue a la carrera
    MODE_EAT,    // come una presa abatida
    MODE_RIVAL,  // pelea con un rival de su especie
    MODE_FOLLOW, // sigue al jugador (domado, ganado pastoreado)
} AnimalMode;

// Objetivo: un animal (indice) o una persona (indice en FaunaCtx.humans).
typedef enum { TGT_NONE, TGT_ANIMAL, TGT_HUMAN } TargetKind;

typedef struct {
    bool used;
    Species species;
    AnimalState state;
    AnimalMode mode;
    float x, z, yaw;
    float alt;            // aves: altura de vuelo sobre el suelo
    float home_x, home_z; // centro de su territorio (o donde lo dejo el pastor)
    float tx, tz;         // destino al deambular
    float flee_x, flee_z; // de donde huye
    float timer;          // hasta elegir otro destino
    bool fleeing;
    bool ridden;
    float speed;          // m/s en el ultimo paso (para la animacion)
    Health h;
    int group;            // grupo (manada, rebaño); -1 solitario
    TargetKind tkind;
    int target;
    float cooldown, attack_anim, hit_anim;
    float alert;          // s: asustado (huye de lo que lo asusto)
    float fear_humans;    // s: huye de las personas (le hicieron daño)
    float hunger;         // [0, 1+]: con hambre (> 0.5) un cazador sale a cazar
    float stamina;        // [0, 1]: la carrera cansa (la manada caza por agotamiento)
    float rival_timer;    // s: pelea de rivales en curso
    float clock;          // s de vida (aves domadas: vuelo en circulos)
    float bound_timer;    // s: atado; si no come, se suelta
    float corpse;         // s que lleva muerto
    float eaten;          // [0, 1]: cuanto se comieron del cadaver
    bool butchered;       // despiezado
    bool milked;          // ordeñado hoy
    bool weakened;        // con poca vida: el lazo puede atarlo
    bool killed_by_human; // muerto a manos de una persona (para el registro)
} Animal;

// Personas cerca de la fauna (jugador, tribu, enemigos). El indice 0 suele ser el jugador.
typedef struct {
    float x, z;
    bool down;     // abatida: los animales la ignoran
    bool sneaking; // acechando: cuesta mas verla
    bool enemy;    // enemigo de la tribu: los animales domados la atacan
} FaunaHuman;

typedef struct {
    const FaunaHuman *humans;
    int human_count;
    float px, pz;       // jugador (lo siguen los domados y el ganado)
    bool player_moving; // pastorear: el ganado sigue al jugador que camina cerca
    float camp_x, camp_z;
    bool night;
} FaunaCtx;

typedef enum {
    FEV_BITE_HUMAN,  // un animal ataca a una persona (el juego aplica el daño)
    FEV_KILL,        // un depredador abatio una presa
    FEV_RIVAL_WON,   // un rival echa a otro de su territorio
    FEV_BREAK_FREE,  // un animal atado se solto del lazo
    FEV_PREY_ALARM,  // una presa noto a un cazador y su grupo huye
} FaunaEventKind;

typedef struct {
    FaunaEventKind kind;
    int animal; // quien
    int other;  // a quien (animal o persona)
    float damage;
    WoundKind wound;
} FaunaEvent;

#define FAUNA_EVENTS_MAX 32
typedef struct {
    FaunaEvent ev[FAUNA_EVENTS_MAX];
    int n;
} FaunaEvents;

const SpeciesDef *species_def(Species s);
bool species_is_prey(Species s); // presa de los cazadores (monturas, presas y ganado)
// Busca una especie por el final de su id ("lobo", "caballo_estepario"); -1 si no.
int species_find(const char *name);

void animal_init(Animal *a, Species s, float x, float z);

// Toda la fauna a la vez (las relaciones entre animales necesitan verlos a todos).
// events puede ser NULL.
void fauna_update(Animal *a, int n, const FaunaCtx *ctx, Rng *rng, float dt, FaunaEvents *events);
// Un solo animal con el jugador en (px, pz) (sin otros animales).
void animal_update(Animal *a, float dt, float px, float pz, Rng *rng);

// Daño a un animal. by_human: lo hirio una persona (las presas le temen, la alarma
// se contagia al grupo: pasa a todos los del mismo grupo en a[0..n)).
// Devuelve el indice de la herida o -1.
int animal_hurt(Animal *a, int n, int idx, Rng *rng, float damage, WoundKind kind, int part, bool by_human,
                int attacker_human);

typedef enum {
    TAME_OK,         // domado (montura) o atado (depredador)
    TAME_FAILED,     // se zafo del lazo
    TAME_TOO_STRONG, // un depredador entero: hay que debilitarlo antes
    TAME_NEVER,      // no se puede domar (hostil, presa salvaje)
    TAME_ALREADY,    // ya es de la tribu
} TameResult;

// Lazo. bonus suma a la probabilidad (p. ej., un jinete habil). Una montura queda
// domada; un depredador debilitado queda atado (ANIMAL_BOUND) hasta que coma.
TameResult animal_lasso(Animal *a, Rng *rng, float bonus, float camp_x, float camp_z);
// Compatibilidad: true si el lazo lo deja domado o atado.
bool animal_try_tame(Animal *a, Rng *rng, float bonus, float camp_x, float camp_z);
// Dar de comer a un animal atado: queda domado y sigue a la tribu.
bool animal_feed(Animal *a, float camp_x, float camp_z);
// Ensillar: solo animales domados y montables.
bool animal_saddle(Animal *a);
bool animal_can_ride(const Animal *a);
// Ordeñar (ganado, una vez al dia): devuelve la leche obtenida.
int animal_milk(Animal *a);
// Sacrificar un animal de la tribu (ganado).
bool animal_slaughter(Animal *a);
// Despiezar un cadaver: carne y pieles (menos si se lo comieron). false si no se puede.
bool animal_butcher(Animal *a, int *meat, int *hide);
// Nuevo dia: se puede volver a ordeñar.
void animal_new_day(Animal *a);

// Cuerpo aproximado para los impactos (zonas de fiera), segun su talla.
// Local: +Y arriba, +Z hacia delante (src/sim/body.h).
void animal_body(const Animal *a, BodyPose *out);

#endif
