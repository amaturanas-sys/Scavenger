#include "sim/camview.h"

#include <math.h>
#include <string.h>

#include "sim/lang.h"

// Cerca: a 2,4 m y apenas mirando hacia abajo, el punto un poco sobre los ojos. Con el campo de 55
// grados la coronilla queda a la mitad de la pantalla y el cuadro corta por las rodillas, como en la
// imagen de referencia del usuario (la camara de exploracion de un juego de mundo abierto). Lejos es
// la camara de siempre (11 m, 0,38 rad): la de las capturas viejas y la de la mira.
static const CamPreset PRESETS[CAMVIEW_COUNT] = {
    [CAMVIEW_NEAR] = { 2.4f, 0.10f, 0.10f },
    [CAMVIEW_MID] = { 5.5f, 0.25f, 0.0f },
    [CAMVIEW_FAR] = { 11.0f, 0.38f, 0.0f },
};

static const char *NAMES[CAMVIEW_COUNT] = { N_("cerca, sobre el hombro"), N_("a media distancia"), N_("lejos") };
static const char *IDS[CAMVIEW_COUNT] = { "cerca", "media", "lejos" };

static CamView clamp_view(CamView v) { return (unsigned)v < CAMVIEW_COUNT ? v : CAMVIEW_NEAR; }

const CamPreset *camview_preset(CamView v) { return &PRESETS[clamp_view(v)]; }
CamView camview_next(CamView v) { return (CamView)((clamp_view(v) + 1) % CAMVIEW_COUNT); }
CamView camview_prev(CamView v) { return (CamView)((clamp_view(v) + CAMVIEW_COUNT - 1) % CAMVIEW_COUNT); }
const char *camview_name(CamView v) { return NAMES[clamp_view(v)]; }

int camview_parse(const char *s) {
    for (int v = 0; s && v < CAMVIEW_COUNT; v++)
        if (!strcmp(s, IDS[v])) return v;
    return -1;
}

float camview_aim_pitch(CamView v, float pitch) { return pitch - camview_preset(v)->pitch + PRESETS[CAMVIEW_FAR].pitch; }

float camview_screen_y(const CamPreset *c, float eye, float y) {
    // La camara detras (-z) y arriba del punto que mira; adelante f, arriba u (perpendicular a f).
    float sp = sinf(c->pitch), cp = cosf(c->pitch);
    float dy = y - (eye + c->lift) - c->dist * sp, dz = c->dist * cp; // del ojo de la camara al punto
    float depth = -dy * sp + dz * cp, up = dy * cp + dz * sp;
    if (depth <= 1e-4f) return up >= 0.0f ? -1.0f : 2.0f; // detras de la camara
    float ndc = up / (depth * tanf(CAMVIEW_FOVY * 0.5f * 3.14159265f / 180.0f));
    return 0.5f - 0.5f * ndc;
}
