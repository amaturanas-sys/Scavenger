#include "sky.h"

#include <math.h>

#include "raymath.h"
#include "sim/clock.h"
#include "sim/rng.h"

void sky_init(Sky *s, unsigned seed) {
    Rng r;
    rng_seed(&r, seed ^ 0x51A75u);
    for (int i = 0; i < SKY_STARS; i++) {
        float yaw = rng_float(&r) * 2.0f * PI;
        float el = asinf(0.06f + 0.94f * rng_float(&r)); // sobre el horizonte
        s->stars[i] = (Vector3){ cosf(el) * sinf(yaw), sinf(el), cosf(el) * cosf(yaw) };
        s->star_bright[i] = 0.35f + 0.65f * rng_float(&r) * rng_float(&r);
    }
}

Color sky_clear_color(void) { return (Color){ 168, 196, 214, 255 }; } // cielo de estepa

static Color mix(Color a, Color b, float k) {
    return (Color){ (unsigned char)Lerp(a.r, b.r, k), (unsigned char)Lerp(a.g, b.g, k), (unsigned char)Lerp(a.b, b.b, k),
                    255 };
}

Color sky_tint(float t) {
    const Color night = { 38, 48, 92, 255 }, warm = { 255, 168, 122, 255 }, day = { 255, 255, 255, 255 };
    float light = clock_light(t);
    Color base = mix(night, day, light);
    // Calido en la penumbra y, mas suave, con el sol bajo.
    float glow = 1.0f - fabsf(2.0f * light - 1.0f);
    float sun = clock_sun_height(t);
    if (sun > 0.0f && sun < 0.3f) glow = fmaxf(glow, 0.45f * (1.0f - sun / 0.3f));
    Color tinted = { (unsigned char)(base.r * warm.r / 255), (unsigned char)(base.g * warm.g / 255),
                     (unsigned char)(base.b * warm.b / 255), 255 };
    return mix(base, tinted, glow);
}

void sky_apply_tint(float t, int w, int h) {
    Color c = sky_tint(t);
    if (c.r == 255 && c.g == 255 && c.b == 255) return;
    BeginBlendMode(BLEND_MULTIPLIED);
    DrawRectangle(0, 0, w, h, c);
    EndBlendMode();
}

void sky_draw_stars(const Sky *s, Camera3D cam, float t) {
    float dark = 1.0f - clock_light(t);
    if (dark <= 0.05f) return;
    for (int i = 0; i < SKY_STARS; i++) {
        Vector3 p = Vector3Add(cam.position, Vector3Scale(s->stars[i], 600.0f));
        unsigned char v = (unsigned char)(255.0f * dark * s->star_bright[i]);
        DrawCube(p, 1.6f, 1.6f, 1.6f, (Color){ v, v, (unsigned char)fminf(255.0f, v * 1.1f + 10.0f), 255 });
    }
}

void sky_draw_lights(Camera3D cam, const Vector3 *pos, const float *radius, int n, float t, float time, int w,
                     int h) {
    float dark = 1.0f - clock_light(t);
    if (dark <= 0.02f) return;
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    float focal = (h * 0.5f) / tanf(cam.fovy * 0.5f * DEG2RAD);
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < n; i++) {
        Vector3 to = Vector3Subtract(pos[i], cam.position);
        float depth = Vector3DotProduct(to, fwd);
        if (depth < 0.5f) continue; // detras de la camara
        Vector2 sp = GetWorldToScreenEx(pos[i], cam, w, h);
        float flick = 0.92f + 0.08f * sinf(time * 11.0f + i * 1.7f) * sinf(time * 6.1f + i);
        float r = fminf(radius[i] * focal / depth, 280.0f) * flick;
        if (r < 2.0f) continue;
        unsigned char a = (unsigned char)(200.0f * dark);
        DrawCircleGradient((int)sp.x, (int)sp.y, r, (Color){ 255, 150, 64, a }, (Color){ 255, 120, 40, 0 });
        DrawCircleGradient((int)sp.x, (int)sp.y, r * 0.3f, (Color){ 255, 220, 140, a }, (Color){ 255, 170, 80, 0 });
    }
    EndBlendMode();
}
