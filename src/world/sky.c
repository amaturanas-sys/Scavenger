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
        float el = asinf(-1.0f + 2.0f * rng_float(&r)); // toda la esfera
        s->stars[i] = (Vector3){ cosf(el) * sinf(yaw), sinf(el), cosf(el) * cosf(yaw) };
        s->star_bright[i] = 0.35f + 0.65f * rng_float(&r) * rng_float(&r);
    }
}

static Color mix(Color a, Color b, float k) {
    return (Color){ (unsigned char)Lerp(a.r, b.r, k), (unsigned char)Lerp(a.g, b.g, k), (unsigned char)Lerp(a.b, b.b, k),
                    255 };
}

// Cielo de estepa; con nubes, gris plomizo.
Color sky_clear_color(float clouds) {
    return mix((Color){ 168, 196, 214, 255 }, (Color){ 150, 156, 162, 255 }, Clamp((clouds - 0.2f) / 0.8f, 0.0f, 1.0f));
}

static Color day_tint(float t) {
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

Color sky_tint(float t, float clouds) {
    // Las nubes apagan la luz del dia y el calor del ocaso.
    Color c = day_tint(t);
    float k = 1.0f - 0.32f * Clamp((clouds - 0.2f) / 0.8f, 0.0f, 1.0f);
    return (Color){ (unsigned char)(c.r * k), (unsigned char)(c.g * k), (unsigned char)(c.b * k), 255 };
}

void sky_apply_tint(float t, float clouds, int w, int h) {
    Color c = sky_tint(t, clouds);
    if (c.r == 255 && c.g == 255 && c.b == 255) return;
    BeginBlendMode(BLEND_MULTIPLIED);
    DrawRectangle(0, 0, w, h, c);
    EndBlendMode();
}

// El eje del cielo: el polo norte, a 45 grados sobre el horizonte del norte (-Z).
static Vector3 sky_pole(void) { return Vector3Normalize((Vector3){ 0.0f, 1.0f, -1.0f }); }

void sky_draw_stars(const Sky *s, Camera3D cam, float t, float clouds) {
    float dark = (1.0f - clock_light(t)) * Clamp(1.0f - clouds * 1.2f, 0.0f, 1.0f);
    if (dark <= 0.05f) return;
    float turn = -2.0f * PI * t / GAME_SECONDS_PER_DAY; // una vuelta por dia, de este a oeste
    Vector3 pole = sky_pole();
    float size = 2.6f * SKY_DISTANCE / 600.0f; // unos 2 pixeles
    for (int i = 0; i < SKY_STARS; i++) {
        Vector3 d = Vector3RotateByAxisAngle(s->stars[i], pole, turn);
        if (d.y < 0.03f) continue; // bajo el horizonte
        Vector3 p = Vector3Add(cam.position, Vector3Scale(d, SKY_DISTANCE));
        // Cerca del horizonte la bruma las apaga.
        unsigned char v = (unsigned char)fminf(255.0f, 320.0f * dark * s->star_bright[i] * Clamp(d.y * 6.0f, 0.0f, 1.0f));
        DrawCube(p, size, size, size, (Color){ v, v, (unsigned char)fminf(255.0f, v * 1.1f + 10.0f), 255 });
    }
}

// Un punto del arco del sol: a = 0 al salir (este), PI/2 al mediodia (alto, hacia el sur),
// PI al ponerse (oeste); de PI a 2 PI, bajo el horizonte.
static Vector3 arc_dir(float a) { return Vector3Normalize((Vector3){ cosf(a), sinf(a) * 0.9f, sinf(a) * 0.45f }); }

static float sun_angle(float t) {
    float x = clock_seconds_into_day(t), d = clock_daylight_seconds(clock_day(t));
    if (x < d) return PI * x / d;
    return PI + PI * (x - d) / (GAME_SECONDS_PER_DAY - d);
}

Vector3 sky_sun_dir(float t) { return arc_dir(sun_angle(t)); }

float sky_moon_phase(float t) {
    float days = t / GAME_SECONDS_PER_DAY + 3.0f; // el juego empieza con la luna creciente
    float p = fmodf(days / SKY_LUNAR_DAYS, 1.0f);
    return p < 0.0f ? p + 1.0f : p;
}

// Luna nueva: junto al sol; llena: opuesta (sale al ponerse el sol).
Vector3 sky_moon_dir(float t) { return arc_dir(sun_angle(t) - 2.0f * PI * sky_moon_phase(t)); }

void sky_draw_background(Camera3D cam, float t, float clouds, int w, int h) {
    Color horizon = sky_clear_color(clouds);
    Color zenith = mix((Color){ 92, 140, 196, 255 }, (Color){ 128, 134, 142, 255 }, Clamp((clouds - 0.2f) / 0.8f, 0.0f, 1.0f));
    // Donde cae el horizonte en la pantalla (la camara mira arriba o abajo).
    Vector3 fwd = Vector3Subtract(cam.target, cam.position);
    fwd.y = 0.0f;
    if (Vector3Length(fwd) < 1e-3f) fwd = (Vector3){ 0, 0, -1 };
    fwd = Vector3Normalize(fwd);
    Vector2 hp = GetWorldToScreenEx(Vector3Add(cam.position, Vector3Scale(fwd, 2000.0f)), cam, w, h);
    int hy = (int)Clamp(hp.y, -h, 2 * h);
    if (hy > 0) {
        int top = hy - (int)(h * 0.9f);
        DrawRectangle(0, 0, w, top > 0 ? top : 0, zenith);
        DrawRectangleGradientV(0, top, w, hy - top, zenith, horizon);
    }
    if (hy < h) DrawRectangle(0, hy > 0 ? hy : 0, w, h - (hy > 0 ? hy : 0), horizon);
    // El sol (y su resplandor, calido cuando esta bajo).
    Vector3 sd = sky_sun_dir(t);
    if (sd.y < -0.12f) return;
    Vector3 to = Vector3Scale(sd, 1000.0f);
    Vector3 look = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    if (Vector3DotProduct(to, look) <= 0.0f) return; // detras de la camara
    Vector2 sp = GetWorldToScreenEx(Vector3Add(cam.position, to), cam, w, h);
    float haze = Clamp((clouds - 0.3f) / 0.6f, 0.0f, 1.0f); // con nubes el sol se ve velado
    float low = 1.0f - Clamp(sd.y / 0.35f, 0.0f, 1.0f);
    unsigned char ga = (unsigned char)(150.0f * (0.35f + 0.65f * low) * (1.0f - 0.5f * haze));
    DrawCircleGradient((int)sp.x, (int)sp.y, 40.0f + 50.0f * low, (Color){ 255, (unsigned char)(220 - 70 * low), (unsigned char)(160 - 90 * low), ga },
                       (Color){ 255, 200, 140, 0 });
    if (haze < 0.95f) {
        unsigned char a = (unsigned char)(255.0f * (1.0f - haze));
        DrawCircle((int)sp.x, (int)sp.y, 7.0f, (Color){ 255, (unsigned char)(246 - 40 * low), (unsigned char)(214 - 80 * low), a });
    }
}

void sky_draw_moon(Camera3D cam, float t, float clouds) {
    Vector3 md = sky_moon_dir(t);
    if (md.y < -0.05f) return;
    float dark = 1.0f - clock_light(t), veil = Clamp((clouds - 0.45f) / 0.45f, 0.0f, 1.0f);
    float phase = sky_moon_phase(t), lit = 0.5f - 0.5f * cosf(2.0f * PI * phase); // fraccion iluminada
    if (lit < 0.04f || veil >= 1.0f) return;
    // De dia se ve palida, casi del color del cielo.
    Color sky = sky_clear_color(clouds), tint = sky_tint(t, clouds);
    Color sky_now = { (unsigned char)(sky.r * tint.r / 255), (unsigned char)(sky.g * tint.g / 255), (unsigned char)(sky.b * tint.b / 255), 255 };
    Color moon = mix(sky_now, (Color){ 236, 234, 218, 255 }, (0.3f + 0.7f * dark) * (1.0f - veil));
    const float dist = SKY_DISTANCE - 200.0f, r = 46.0f;
    Vector3 c = Vector3Add(cam.position, Vector3Scale(md, dist));
    DrawSphereEx(c, r, 8, 12, moon);
    // La sombra: un disco del color del cielo, delante, corrido hacia el lado opuesto al sol.
    if (lit < 0.97f) {
        Vector3 sd = sky_sun_dir(t);
        Vector3 side = Vector3Subtract(sd, Vector3Scale(md, Vector3DotProduct(sd, md)));
        if (Vector3Length(side) < 1e-3f) side = (Vector3){ 1, 0, 0 };
        side = Vector3Normalize(side);
        Vector3 sc = Vector3Add(Vector3Subtract(c, Vector3Scale(md, r * 1.6f)), Vector3Scale(side, -2.1f * r * lit));
        DrawSphereEx(sc, r * 1.02f, 8, 12, sky_now);
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
