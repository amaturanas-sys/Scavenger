// La monta (C puro): la fase 6 del plan grande (docs/PLAN_GRAN_ACTUALIZACION.md).
//  - Arqueria montada: a caballo el tiro se abre con la velocidad, y mas hacia atras (el
//    disparo parto, al galope y de espaldas al blanco); la monta y la punteria lo corrigen.
//  - Carga con lanza en ristre: al galope tendido, el golpe pesa por la velocidad y derriba.
//  - El cansancio del caballo: el galope gasta su aguante, el paso y el descanso lo devuelven;
//    con hambre o sed se cansa antes. Agotado no galopa hasta recobrarse. Lo heredado (pace,
//    grit: src/sim/animals.h) lo hace mas rapido o mas resistente.
//  - Los arqueros a caballo hostigan: rodean al blanco a distancia de tiro y, si se les acercan,
//    se alejan al galope (tirando hacia atras) y vuelven.
#ifndef ESTEPA_RIDING_H
#define ESTEPA_RIDING_H

#include <stdbool.h>

#define RIDE_GALLOP 6.0f   // m/s: por encima, al galope tendido
#define PARTHIAN_ANGLE 1.75f // rad (~100°): mas lejos del rumbo, el tiro es hacia atras

// ---------------------------------------------------------------- arqueria montada
// Cuanto se abre un tiro a caballo (multiplica la dispersion del arma). speed en m/s; rel_yaw,
// lo que se aparta el tiro del rumbo del caballo (rad); riding y archery en [0, 1] (amuletos,
// tatuajes y joyas). Quieto y hacia delante da lo mismo que a pie.
float mounted_shot_spread(float speed, float rel_yaw, float riding, float archery);
// ¿Es un disparo parto? (al galope y hacia atras)
bool shot_is_parthian(float speed, float rel_yaw);

// ---------------------------------------------------------------- carga con lanza
#define LANCE_MIN_SPEED 5.0f // m/s: mas despacio no hay carga
#define LANCE_COUCH_SECONDS 3.0f
// El daño de la lanza en ristre (0 si no va lo bastante rapido): crece con la velocidad, hasta x2,5.
float lance_charge_damage(float base, float speed);
// La probabilidad de derribar: alta al galope; un escudo que la para la baja mucho.
float lance_charge_knock(float speed, bool blocked);

// ---------------------------------------------------------------- el cansancio del caballo
#define MOUNT_GALLOP_COST (1.0f / 45.0f) // ~45 s de galope tendido
#define MOUNT_REGEN_WALK (1.0f / 60.0f)
#define MOUNT_REGEN_REST (1.0f / 25.0f)
#define MOUNT_RECOVER 0.35f // agotado, vuelve a galopar al recobrar esto
// Un paso del aguante [0, 1]. hunger y thirst como en Animal (> 1: con hambre o con sed); grit,
// lo heredado.
float mount_stamina_step(float stamina, bool galloping, bool moving, float hunger, float thirst, float grit, float dt);
// Agotado: sin aguante no galopa hasta recobrar MOUNT_RECOVER.
bool mount_winded(float stamina, bool was_winded);
// Cuanto corre la montura (multiplica su velocidad): lo heredado, y menos con hambre o sed.
float mount_speed_scale(float pace, float hunger, float thirst);

// ---------------------------------------------------------------- arqueros a caballo
#define HA_ORBIT 24.0f // m: rodean al blanco a esta distancia
#define HA_CLOSE 11.0f // si se les acercan a menos, se alejan
#define HA_FAR 38.0f   // y vuelven cuando estan a esta distancia
// A donde va un arquero a caballo en (ex, ez) contra un blanco en (tx, tz): rodea (dir +1 o -1,
// el sentido del giro) o se aleja. *retreat entra con lo que hacia y sale con lo que hace.
void horse_archer_goal(float ex, float ez, float tx, float tz, float dir, bool *retreat, float *gx, float *gz);

#endif
