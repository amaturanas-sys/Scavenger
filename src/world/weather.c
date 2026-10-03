#include "weather.h"

#include <math.h>

#include "raymath.h"
#include "sim/rng.h"

#define BOX_W 36.0f // ancho de la caja de particulas (X y Z), metros
#define BOX_H 18.0f // alto

void weather_init(WeatherFx *w, unsigned seed) {
    Rng r;
    rng_seed(&r, seed ^ 0xA1E7u);
    for (int i = 0; i < WEATHER_PARTICLES; i++) {
        w->seed_pos[i] = (Vector3){ rng_float(&r) * BOX_W, rng_float(&r) * BOX_H, rng_float(&r) * BOX_W };
        w->phase[i] = rng_float(&r) * 6.2831853f;
    }
    w->flash = 0.0f;
    w->next_flash = 3.0f;
    w->rng = seed | 1u;
}

static float posmod(float v, float m) {
    float r = fmodf(v, m);
    return r < 0.0f ? r + m : r;
}

// Coordenada anclada al mundo que se repite en una caja centrada en `center`.
static float wrap_around(float v, float center, float size) {
    float lo = center - size * 0.5f;
    return lo + posmod(v - lo, size);
}

void weather_update(WeatherFx *w, const Climate *c, float dt) {
    w->flash *= expf(-dt * 9.0f);
    if (c->storm < 0.5f) return;
    w->next_flash -= dt;
    if (w->next_flash > 0.0f) return;
    Rng r = { w->rng };
    w->flash = 0.7f + 0.3f * rng_float(&r);
    w->next_flash = 4.0f + rng_float(&r) * 10.0f; // un relampago cada 4 a 14 segundos
    w->rng = r.state;
}

void weather_draw(const WeatherFx *w, const Climate *c, Camera3D cam, float time, float light) {
    const Vector3 o = cam.position;
    const float bright = 0.35f + 0.65f * light;
    // Viento: empuja en diagonal; en la ventisca la nieve viaja casi horizontal.
    const Vector2 wind_dir = { 0.86f, 0.5f };
    if (c->rain > 0.02f) {
        int n = (int)(WEATHER_PARTICLES * c->rain);
        const float fall = 16.0f, push = 2.0f + 6.0f * c->wind;
        Vector3 vel = { wind_dir.x * push, -fall, wind_dir.y * push };
        Vector3 streak = Vector3Scale(vel, 0.045f); // estela del movimiento
        Color col = { (unsigned char)(170 * bright), (unsigned char)(182 * bright), (unsigned char)(200 * bright), 150 };
        for (int i = 0; i < n; i++) {
            Vector3 s = w->seed_pos[i];
            float y = o.y - BOX_H * 0.5f + posmod(s.y - fall * time, BOX_H);
            Vector3 p = { wrap_around(s.x + vel.x * time, o.x, BOX_W), y, wrap_around(s.z + vel.z * time, o.z, BOX_W) };
            DrawLine3D(p, Vector3Subtract(p, streak), col);
        }
    }
    if (c->snow > 0.02f) {
        int n = (int)(WEATHER_PARTICLES * c->snow);
        const float fall = 1.6f + 1.4f * c->wind, push = 0.6f + 9.0f * c->wind * c->wind;
        unsigned char v = (unsigned char)(245 * bright);
        Color col = { v, v, (unsigned char)fminf(255.0f, v * 1.03f), 235 };
        for (int i = 0; i < n; i++) {
            Vector3 s = w->seed_pos[i];
            float sway = sinf(time * 1.3f + w->phase[i]) * 0.6f; // los copos se mecen
            float y = o.y - BOX_H * 0.5f + posmod(s.y - fall * time, BOX_H);
            Vector3 p = { wrap_around(s.x + wind_dir.x * push * time + sway, o.x, BOX_W), y,
                          wrap_around(s.z + wind_dir.y * push * time + sway * 0.5f, o.z, BOX_W) };
            if (c->wind > 0.7f) // ventisca: copos estirados por el viento
                DrawLine3D(p, (Vector3){ p.x - wind_dir.x * 0.25f, p.y + 0.03f, p.z - wind_dir.y * 0.25f }, col);
            else
                DrawCube(p, 0.07f, 0.07f, 0.07f, col);
        }
    }
}

void weather_draw_screen(const WeatherFx *w, const Climate *c, int width, int height) {
    // Bruma: la lluvia apaga el paisaje; la ventisca lo blanquea.
    float haze = fmaxf(c->rain, c->snow) * 60.0f;
    if (haze > 1.0f) DrawRectangle(0, 0, width, height, (Color){ 140, 146, 156, (unsigned char)haze });
    float whiteout = c->snow * c->wind * c->wind * 110.0f;
    if (whiteout > 1.0f) DrawRectangle(0, 0, width, height, (Color){ 222, 228, 236, (unsigned char)whiteout });
    if (w->flash > 0.02f) {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawRectangle(0, 0, width, height, (Color){ 210, 220, 255, (unsigned char)(170.0f * w->flash) });
        EndBlendMode();
    }
}
