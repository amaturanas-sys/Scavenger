// La espalda, los abatidos y los rehenes (C puro): las reglas de la fase 2 del plan grande
// (docs/PLAN_GRAN_ACTUALIZACION.md).
//  - Ganar la espalda: a menos de BACKSTAB_RANGE m, en el cono trasero del enemigo (BACKSTAB_CONE
//    grados), sin que te haya notado o con el aturdido. Ahi K ejecuta en silencio y G toma rehen.
//  - Lo que ve un enemigo depende de donde vienes: de frente, todo; de lado, menos; por detras
//    solo oye (correr se oye; el sigilo, casi nada).
//  - La ejecucion silenciosa solo la oyen los que estan a menos de SILENT_ALERT_RANGE m.
//  - El rehen tapa el tiro: el arquero no dispara si el rehen esta en la linea.
//  - Abatidos: si lo que lo tumbo no es letal, el enemigo queda en el suelo; despierta a los
//    DOWN_WAKE_MIN..DOWN_WAKE_MAX s, herido, y huye.
#ifndef ESTEPA_STEALTH_H
#define ESTEPA_STEALTH_H

#include <stdbool.h>

#include "health.h"
#include "rng.h"

#define BACKSTAB_RANGE 2.0f          // m
#define BACKSTAB_CONE 70.0f          // grados: el cono trasero del enemigo
#define SILENT_ALERT_RANGE 6.0f      // m: la ejecucion silenciosa no se oye mas lejos
#define DOWN_WAKE_MIN 30.0f          // s en el suelo antes de despertar
#define DOWN_WAKE_MAX 90.0f
#define HOSTAGE_SHIELD_RADIUS 0.5f   // m: cuanto tapa el rehen de la linea de tiro

// Coseno entre el frente de quien esta en (ex, ez) mirando a yaw (el frente es (sin yaw, cos yaw))
// y la direccion hacia (px, pz): 1 de frente, -1 a la espalda.
float stealth_facing(float ex, float ez, float eyaw, float px, float pz);
// (px, pz) esta en el cono trasero del enemigo, a menos de BACKSTAB_RANGE.
bool stealth_behind(float ex, float ez, float eyaw, float px, float pz);
// Se le gana la espalda: detras, y sin haberte notado o aturdido.
bool stealth_backstab(float ex, float ez, float eyaw, float px, float pz, bool noticed, bool stunned);
// Cuanto ve (y oye) un enemigo a quien se acerca: facing (stealth_facing hacia el) y su ruido
// [0, 1] (correr 1, caminar 0.5, sigilo 0.15, quieto 0). Multiplica la vista del enemigo.
float stealth_sight_scale(float facing, float noise);
// Un testigo a esa distancia oye la ejecucion silenciosa.
bool stealth_hears_kill(float dist);
// El rehen (hx, hz) tapa el tiro de (sx, sz) a (tx, tz): esta en la linea, entre los dos.
bool hostage_blocks_shot(float sx, float sz, float tx, float tz, float hx, float hz);
// Abatidos: cuanto tarda en despertar, en [DOWN_WAKE_MIN, DOWN_WAKE_MAX].
float down_wake_seconds(Rng *rng);
// Cuenta hacia atras el tiempo en el suelo: true una sola vez, al despertar.
bool down_tick(float *wake, float dt);
// Lo que lo tumbo lo mata (no queda abatido): muerto, casi sin sangre, o una herida abierta
// grave en la cabeza o el cuello, o muy grave en el pecho o el vientre. Los golpes contundentes
// (maza, patada, escudo) no matan: tumban.
bool down_is_lethal(const Health *h);

#endif
