#include "sonido.h"

#include <math.h>

static float clamp01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }

float amb_near(float d, float lleno, float nada) {
    if (d <= lleno) return 1.0f;
    if (d >= nada) return 0.0f;
    float t = 1.0f - (d - lleno) / (nada - lleno);
    return t * t * (3.0f - 2.0f * t);
}

void amb_mix(const AmbIn *in, float out[AMB_COUNT]) {
    for (int i = 0; i < AMB_COUNT; i++) out[i] = 0.0f;
    out[AMB_FUEGO] = amb_near(in->fuego, 2.0f, 16.0f); // la fogata crepita de cerca, adentro o afuera
    if (in->cueva) {                               // adentro de la roca no llega lo de afuera
        out[AMB_CUEVA] = 1.0f;
        return;
    }
    float lluvia = clamp01(in->lluvia), tormenta = clamp01(in->tormenta);
    out[AMB_TORMENTA] = tormenta;                  // la tormenta trae su lluvia
    out[AMB_LLUVIA] = lluvia * (1.0f - tormenta);
    float calla = 1.0f - 0.7f * fmaxf(lluvia, tormenta); // con lluvia, los pajaros y los grillos callan
    float bosque = clamp01(in->bosque), luz = clamp01(in->luz);
    out[AMB_BOSQUE_DIA] = bosque * luz * calla;
    out[AMB_BOSQUE_NOCHE] = bosque * (1.0f - luz) * calla;
    // El viento: siempre algo, mas en lo abierto y con viento fuerte; el bosque lo frena. La ventisca ruge entera en lo
    // abierto, y el bosque la frena menos que al viento.
    float viento = (0.25f + 0.75f * clamp01(in->viento)) * (0.35f + 0.65f * clamp01(in->abierto)) * (1.0f - 0.6f * bosque);
    out[AMB_VIENTO] = fmaxf(clamp01(viento), clamp01(in->ventisca) * (1.0f - 0.4f * bosque));
    out[AMB_RIO] = amb_near(in->rio, 6.0f, 45.0f);
    out[AMB_CASCADA] = amb_near(in->cascada, 10.0f, 90.0f);
    out[AMB_MAR] = amb_near(in->mar, 15.0f, 120.0f);
}

float amb_follow(float now, float target, float dt, float tau) {
    if (tau <= 0.0f || dt <= 0.0f) return tau <= 0.0f ? target : now;
    return now + (target - now) * (1.0f - expf(-dt / tau));
}

Suelo amb_suelo(bool en_agua, bool en_madera, bool en_roca) {
    if (en_agua) return SUELO_AGUA;
    if (en_madera) return SUELO_MADERA;
    return en_roca ? SUELO_PIEDRA : SUELO_TIERRA;
}

float amb_paso_cada(bool corre) { return corre ? 0.667f * 0.5f : 1.333f * 0.5f; }
