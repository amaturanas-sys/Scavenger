#include "audio_game.h"

#include <math.h>
#include <stdio.h>

#include "platform.h"
#include "sim/clock.h"
#include "sim/hazards.h"
#include "sim/rng.h"
#include "sim/world.h"
#include "world/hearth.h"

// Los bucles, en el orden de AmbLayer, y su volumen propio (la lluvia y la tormenta tapan; el viento acompaña).
static const char *const LOOP_FILE[AMB_COUNT] = { "viento", "bosque_dia", "bosque_noche", "lluvia", "tormenta",
                                                  "rio",    "cascada",    "mar",          "cueva",  "fuego" };
static const float LOOP_GAIN[AMB_COUNT] = { 0.6f, 0.7f, 0.7f, 0.8f, 0.9f, 0.8f, 0.9f, 0.8f, 0.8f, 0.7f };

#define VARIANTS 4
// Cada sonido suelto: sus variantes (una al azar, sin repetir la anterior), su volumen, y hasta donde se oye: entero
// hasta `full` metros, nada desde `range`.
typedef struct {
    const char *file[VARIANTS];
    float gain, full, range;
} Group;
static const Group GROUP[SND_COUNT] = {
    [SND_PASO_TIERRA] = { { "paso_tierra_1", "paso_tierra_2", "paso_tierra_3", "paso_tierra_4" }, 0.32f, 2.0f, 20.0f },
    [SND_PASO_PIEDRA] = { { "paso_piedra_1", "paso_piedra_2", "paso_piedra_3", "paso_piedra_4" }, 0.32f, 2.0f, 20.0f },
    [SND_PASO_AGUA] = { { "paso_agua_1", "paso_agua_2", "paso_agua_3", "paso_agua_4" }, 0.4f, 2.0f, 20.0f },
    [SND_PASO_MADERA] = { { "paso_madera_1", "paso_madera_2", "paso_madera_3", "paso_madera_4" }, 0.36f, 2.0f, 20.0f },
    [SND_AVE] = { { "ave" }, 0.4f, 8.0f, 60.0f },
    [SND_CUERVO] = { { "cuervo_1", "cuervo_2", "cuervos" }, 0.42f, 8.0f, 70.0f },
    [SND_GRAJILLA] = { { "grajilla" }, 0.38f, 8.0f, 60.0f },
    [SND_CHAPOTEO] = { { "chapoteo" }, 0.45f, 2.0f, 25.0f },
    [SND_BEBER] = { { "beber" }, 0.55f, 1.0f, 10.0f },
    [SND_COMER] = { { "comer" }, 0.55f, 1.0f, 10.0f },
    [SND_QUEJIDO] = { { "quejido_1", "quejido_2", "quejido_3" }, 0.55f, 2.0f, 25.0f },
    [SND_TAJO] = { { "espada_tajo_1", "espada_tajo_2", "espada_tajo_3" }, 0.45f, 3.0f, 30.0f },
    [SND_IMPACTO] = { { "espada_impacto_1", "espada_impacto_2", "espada_impacto_3" }, 0.65f, 3.0f, 45.0f },
    [SND_BLOQUEO] = { { "espada_bloqueo_1", "espada_bloqueo_2", "espada_bloqueo_3" }, 0.65f, 3.0f, 45.0f },
    [SND_PARRY] = { { "espada_parry" }, 0.65f, 3.0f, 45.0f },
    [SND_ARCO] = { { "arco_disparo_1", "arco_disparo_2" }, 0.5f, 3.0f, 40.0f },
    [SND_FLECHA] = { { "flecha_impacto_1", "flecha_impacto_2" }, 0.5f, 3.0f, 40.0f },
    [SND_PUNO] = { { "golpe_puno" }, 0.55f, 3.0f, 30.0f },
    [SND_PATADA] = { { "patada" }, 0.55f, 3.0f, 30.0f },
    [SND_MARTILLO] = { { "martillo_1", "martillo_2" }, 0.5f, 3.0f, 40.0f },
};

#define PROBE_SECONDS 0.25f // cada cuanto se mide el agua alrededor (no cambia de un cuadro al otro)
#define FOLLOW_SECONDS 0.8f // lo que tarda un bucle en seguir a su volumen (sin saltos al entrar o salir)

static bool g_ready;
static float g_volume = 0.75f;
static Music g_loop[AMB_COUNT];
static bool g_loop_ok[AMB_COUNT], g_loop_started[AMB_COUNT], g_loop_on[AMB_COUNT];
static float g_level[AMB_COUNT], g_pan[AMB_COUNT];
static Sound g_snd[SND_COUNT][VARIANTS];
static int g_nvar[SND_COUNT], g_last[SND_COUNT];
static Rng g_rng;
static Vector3 g_ear, g_right = { 1.0f, 0.0f, 0.0f }; // quien escucha (a la altura de la cabeza) y su derecha
// El agua alrededor, medida cada PROBE_SECONDS: la distancia y el punto mas cercano de cada una.
static float g_probe;
static float g_d_rio = AMB_LEJOS, g_d_cascada = AMB_LEJOS, g_d_mar = AMB_LEJOS;
static Vector3 g_at_rio, g_at_cascada, g_at_mar;
static float g_step; // los segundos hasta el paso siguiente
static float g_bird = 5.0f; // los segundos hasta el ave siguiente
static int g_played[SND_COUNT]; // cuantas veces se pidio cada sonido suelto (el diagnostico)

void au_init(void) {
    InitAudioDevice();
    g_ready = IsAudioDeviceReady();
    if (!g_ready) {
        TraceLog(LOG_WARNING, "Sonido: no hay dispositivo de audio; el juego sigue en silencio");
        return;
    }
    // Medio buffer de cada bucle: 4096 muestras (0,19 s a 22 050 Hz). Un cuadro lento (cargar una cueva, en Android)
    // no deja al bucle sin muestras.
    SetAudioStreamBufferSizeDefault(4096);
    char path[80];
    for (int i = 0; i < AMB_COUNT; i++) {
        snprintf(path, sizeof(path), "assets/sonidos/%s.ogg", LOOP_FILE[i]);
        g_loop[i] = LoadMusicStream(platform_asset_path(path));
        g_loop_ok[i] = IsMusicValid(g_loop[i]);
        platform_note_asset(path, g_loop_ok[i]);
        g_pan[i] = 0.5f;
    }
    for (int g = 0; g < SND_COUNT; g++)
        for (int v = 0; v < VARIANTS && GROUP[g].file[v]; v++) {
            snprintf(path, sizeof(path), "assets/sonidos/%s.ogg", GROUP[g].file[v]);
            Sound s = LoadSound(platform_asset_path(path));
            bool ok = IsSoundValid(s);
            platform_note_asset(path, ok);
            if (ok) g_snd[g][g_nvar[g]++] = s;
        }
    rng_seed(&g_rng, 0x50f1d0u);
    SetMasterVolume(g_volume);
}

void au_unload(void) {
    if (!g_ready) return;
    for (int i = 0; i < AMB_COUNT; i++)
        if (g_loop_ok[i]) UnloadMusicStream(g_loop[i]);
    for (int g = 0; g < SND_COUNT; g++)
        for (int v = 0; v < g_nvar[g]; v++) UnloadSound(g_snd[g][v]);
    CloseAudioDevice();
    g_ready = false;
}

void au_set_volume(float v) {
    g_volume = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
    if (g_ready) SetMasterVolume(g_volume);
}

const char *au_layer_name(int layer) { return layer >= 0 && layer < AMB_COUNT ? LOOP_FILE[layer] : "?"; }
const char *au_sound_name(int id) { return id >= 0 && id < SND_COUNT ? GROUP[id].file[0] : "?"; }
int au_played(int id) { return id >= 0 && id < SND_COUNT ? g_played[id] : 0; }
void au_near(float *rio, float *cascada, float *mar) { *rio = g_d_rio, *cascada = g_d_cascada, *mar = g_d_mar; }

void au_levels(float out[AMB_COUNT]) {
    for (int i = 0; i < AMB_COUNT; i++) out[i] = g_level[i];
}

// El paneo de raylib 5.5: 0,5 al centro, 1 todo a la izquierda y 0 todo a la derecha. Lo que esta muy cerca (dentro de
// `full`) envuelve y suena al centro; desde ahi, cada vez mas de su lado.
static float pan_to(float dx, float dz, float d, float full) {
    if (d < 1e-3f) return 0.5f;
    float side = (dx * g_right.x + dz * g_right.z) / d; // 1: a la derecha
    float spread = fminf(1.0f, fmaxf(0.0f, (d - 0.5f * full) / fmaxf(full, 1.0f)));
    return 0.5f - 0.38f * side * spread;
}

void au_play(SoundId id, const Vector3 *at, float vol) {
    if ((unsigned)id >= SND_COUNT) return;
    g_played[id]++;
    if (!g_ready || !g_nvar[id] || g_volume <= 0.0f) return;
    const Group *g = &GROUP[id];
    float v = g->gain * vol, pan = 0.5f;
    if (at) {
        float dx = at->x - g_ear.x, dy = at->y - g_ear.y, dz = at->z - g_ear.z, d = sqrtf(dx * dx + dy * dy + dz * dz);
        v *= amb_near(d, g->full, g->range);
        pan = pan_to(dx, dz, d, g->full);
    }
    if (v < 0.01f) return;
    int n = g_nvar[id], k = n > 1 ? (g_last[id] + 1 + rng_range(&g_rng, n - 1)) % n : 0; // otra que la anterior
    g_last[id] = k;
    Sound s = g_snd[id][k];
    SetSoundVolume(s, fminf(v, 1.0f));
    SetSoundPan(s, pan);
    SetSoundPitch(s, 0.94f + 0.12f * rng_float(&g_rng)); // ninguno sale igual a otro
    PlaySound(s);
}

// La distancia de (px, pz) al tramo a-b, y su punto mas cercano.
static float seg_dist(float px, float pz, float ax, float az, float bx, float bz, float *cx, float *cz) {
    float vx = bx - ax, vz = bz - az, l2 = vx * vx + vz * vz;
    float t = l2 > 1e-6f ? ((px - ax) * vx + (pz - az) * vz) / l2 : 0.0f;
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    *cx = ax + vx * t, *cz = az + vz * t;
    return sqrtf((px - *cx) * (px - *cx) + (pz - *cz) * (pz - *cz));
}

// El agua alrededor: el rio que corre mas cercano (ni seco ni helado; el de poca agua se oye menos, como si estuviera
// mas lejos), el pie de la cascada mas cercana y el mar sin helar.
static void probe_water(const AuFrame *f) {
    const World *w = f->terrain->world;
    WaterSeason s = terrain_season(f->terrain);
    float px = f->pos.x, pz = f->pos.z, cx, cz;
    g_d_rio = g_d_cascada = g_d_mar = AMB_LEJOS;
    for (int i = 0; i < w->river_count; i++) {
        const River *r = &w->rivers[i];
        if (px < r->minx - 50.0f || px > r->maxx + 50.0f || pz < r->minz - 50.0f || pz > r->maxz + 50.0f) continue;
        if (world_channel_state(w, i, &s) != CHANNEL_WET) continue;
        float loud = 0.4f + 0.6f * fminf(1.0f, world_river_share(w, i, &s) / WORLD_FULL_SHARE);
        for (int k = 0; k + 1 < r->n; k++) {
            float d = fmaxf(0.0f, seg_dist(px, pz, r->x[k], r->z[k], r->x[k + 1], r->z[k + 1], &cx, &cz) - 0.5f * r->width) / loud;
            if (d < g_d_rio) g_d_rio = d, g_at_rio = (Vector3){ cx, f->pos.y, cz };
        }
    }
    for (int i = 0; i < w->fall_count; i++) {
        const Waterfall *fa = &w->falls[i];
        if (fa->river >= 0 && world_channel_state(w, fa->river, &s) != CHANNEL_WET) continue;
        float d = seg_dist(px, pz, fa->x, fa->z, fa->bx, fa->bz, &cx, &cz);
        if (d < g_d_cascada) g_d_cascada = d, g_at_cascada = (Vector3){ cx, fa->bottom, cz };
    }
    // El mar no tiene un trazo: se lo busca en anillos, del mas cercano al mas lejano.
    static const float RING[] = { 0.0f, 12.0f, 25.0f, 40.0f, 60.0f, 85.0f, 115.0f };
    for (int r = 0; r < (int)(sizeof(RING) / sizeof(RING[0])) && g_d_mar >= AMB_LEJOS; r++)
        for (int k = 0; k < (r ? 12 : 1); k++) {
            float a = (float)k * (2.0f * PI / 12.0f), x = px + cosf(a) * RING[r], z = pz + sinf(a) * RING[r];
            WaterNow wn;
            world_water_now(w, x, z, &s, &wn);
            if (wn.kind == WATER_SEA && wn.ice < ICE_WALKABLE && RING[r] < g_d_mar) g_d_mar = RING[r], g_at_mar = (Vector3){ x, wn.level, z };
        }
}

// Lo de afuera: el bosque o el campo abierto, la hora, el tiempo y el agua.
static void listen_outside(const AuFrame *f, AmbIn *in, float dt) {
    g_probe -= dt;
    if (g_probe <= 0.0f) {
        g_probe = PROBE_SECONDS;
        probe_water(f);
    }
    float wt[REGION_COUNT];
    world_region_weights_soft(f->terrain->world, f->pos.x, f->pos.z, wt);
    in->bosque = wt[REGION_FOREST] + 0.3f * wt[REGION_FJORD];
    in->abierto = wt[REGION_STEPPE] + wt[REGION_HIGHLAND] + wt[REGION_DESERT] + 0.5f * wt[REGION_FJORD];
    in->luz = clock_light(f->world_time);
    const Climate *c = f->climate;
    in->viento = c->wind;
    in->lluvia = fminf(1.0f, 2.0f * c->rain); // la lluvia comun (0,5) ya suena entera
    in->tormenta = c->storm;
    in->ventisca = fminf(1.0f, 1.6f * c->snow * c->snow * c->wind); // la nevada comun casi no suena; la ventisca ruge
    in->rio = g_d_rio;
    in->cascada = g_d_cascada;
    in->mar = g_d_mar;
}

// El paneo de un bucle con lugar (el rio, la cascada, el mar, el fuego): hacia su punto mas cercano.
static float loop_pan(int layer, const AuFrame *f) {
    const Vector3 *at = layer == AMB_RIO ? &g_at_rio : layer == AMB_CASCADA ? &g_at_cascada : layer == AMB_MAR ? &g_at_mar : NULL;
    float d = layer == AMB_RIO ? g_d_rio : layer == AMB_CASCADA ? g_d_cascada : layer == AMB_MAR ? g_d_mar : AMB_LEJOS;
    if (!at || f->cave || d >= AMB_LEJOS) return 0.5f;
    float dx = at->x - g_ear.x, dz = at->z - g_ear.z;
    return pan_to(dx, dz, sqrtf(dx * dx + dz * dz), layer == AMB_RIO ? 8.0f : 14.0f);
}

// Los pasos del jugador, al ritmo de la animacion (amb_paso_cada) y segun lo que pisa; nadando, una brazada por ciclo.
static void footsteps(const AuFrame *f, float dt) {
    float every = f->swimming ? 1.667f : amb_paso_cada(f->running);
    if (!f->moving || f->mounted) {
        g_step = 0.35f * amb_paso_cada(false); // el primero, cuando el pie toca el suelo por primera vez
        return;
    }
    g_step -= dt;
    if (g_step > 0.0f) return;
    g_step = fmaxf(g_step + every, 0.0f);
    if (f->swimming) {
        au_play(SND_CHAPOTEO, NULL, 0.7f);
        return;
    }
    Vector3 p = f->pos;
    int kind = f->cave ? 2 : terrain_ground_kind(f->terrain, p.x, p.z, p.y);
    Suelo s = amb_suelo(f->wading, kind == 1, kind == 2 || f->cave);
    au_play((SoundId)(SND_PASO_TIERRA + s), NULL, (0.35f + 0.65f * f->noise) * (f->running ? 1.0f : 0.85f));
}

// Las aves, de vez en cuando y de algun lado: de dia y sin lluvia, en el bosque (pajaros y grajillas) y en lo abierto
// (cuervos).
static void birds(const AuFrame *f, const AmbIn *in, float dt) {
    g_bird -= dt;
    if (g_bird > 0.0f) return;
    g_bird = 4.0f + 8.0f * rng_float(&g_rng);
    if (f->cave || in->luz < 0.35f || in->lluvia > 0.3f || in->tormenta > 0.2f || in->ventisca > 0.5f) return;
    float a = rng_float(&g_rng) * 2.0f * PI, r = 12.0f + 25.0f * rng_float(&g_rng);
    Vector3 at = { f->pos.x + cosf(a) * r, f->pos.y + 6.0f, f->pos.z + sinf(a) * r };
    float pick = rng_float(&g_rng) * (in->bosque + in->abierto + 1e-3f);
    if (pick < in->bosque) au_play(rng_float(&g_rng) < 0.75f ? SND_AVE : SND_GRAJILLA, &at, 1.0f);
    else if (in->abierto > 0.3f && rng_float(&g_rng) < 0.6f) au_play(SND_CUERVO, &at, 1.0f);
}

void au_update(const AuFrame *f, float dt) {
    // Se mezcla aunque no haya dispositivo (el diagnostico de --screenshot dice lo que sonaria); solo se saltan las
    // llamadas a raylib.
    g_ear = (Vector3){ f->pos.x, f->pos.y + 1.5f, f->pos.z };
    float fx = f->cam.target.x - f->cam.position.x, fz = f->cam.target.z - f->cam.position.z, fl = sqrtf(fx * fx + fz * fz);
    if (fl > 1e-4f) g_right = (Vector3){ -fz / fl, 0.0f, fx / fl }; // la derecha de la camara, en el plano
    AmbIn in = { .luz = 1.0f, .rio = AMB_LEJOS, .cascada = AMB_LEJOS, .mar = AMB_LEJOS, .fuego = AMB_LEJOS };
    if (!f->playing) { // el titulo: el viento de la estepa
        in.abierto = 1.0f;
        in.viento = 0.3f;
    } else {
        in.cueva = f->cave;
        in.fuego = hearth_nearest(f->pos);
        if (!f->cave) listen_outside(f, &in, dt);
    }
    float target[AMB_COUNT];
    amb_mix(&in, target);
    for (int i = 0; i < AMB_COUNT; i++) {
        if (f->paused) target[i] *= 0.4f;
        g_level[i] = amb_follow(g_level[i], target[i], dt, FOLLOW_SECONDS);
        if (!g_loop_ok[i]) continue;
        bool want = g_level[i] > 0.003f || target[i] > 0.003f;
        if (want && !g_loop_on[i]) {
            if (!g_loop_started[i]) { // la primera vez, desde un punto al azar: dos partidas no suenan igual
                PlayMusicStream(g_loop[i]);
                SeekMusicStream(g_loop[i], rng_float(&g_rng) * 0.9f * GetMusicTimeLength(g_loop[i]));
                g_loop_started[i] = true;
            } else {
                ResumeMusicStream(g_loop[i]);
            }
            g_loop_on[i] = true;
        } else if (!want && g_loop_on[i]) { // callado: no se decodifica
            PauseMusicStream(g_loop[i]);
            g_loop_on[i] = false;
        }
        if (!g_loop_on[i]) continue;
        g_pan[i] = amb_follow(g_pan[i], loop_pan(i, f), dt, 0.3f);
        SetMusicVolume(g_loop[i], g_level[i] * LOOP_GAIN[i]);
        SetMusicPan(g_loop[i], g_pan[i]);
        UpdateMusicStream(g_loop[i]);
    }
    if (f->playing && !f->paused) {
        footsteps(f, dt);
        birds(f, &in, dt);
    }
}
