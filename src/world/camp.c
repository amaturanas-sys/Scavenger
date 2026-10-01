#include "camp.h"

#include <math.h>
#include <string.h>

#include "../sim/rng.h"
#include "raymath.h"

void camp_init(Camp *c, const Terrain *t, const char *yurt_path) {
    memset(c, 0, sizeof(*c));
    // En Android los assets viven dentro del APK: FileExists() no los ve,
    // pero LoadModel() si (raylib redirige la lectura al AssetManager).
#if defined(__ANDROID__)
    if (yurt_path) {
#else
    if (yurt_path && FileExists(yurt_path)) {
#endif
        c->yurt = LoadModel(yurt_path);
        c->yurt_loaded = c->yurt.meshCount > 0;
    }

    // Yurtas en semicirculo, con la puerta (+X del modelo) hacia la fogata.
    c->fire = (Vector3){ 0.0f, terrain_height(t, 0.0f, 0.0f), 0.0f };
    const int n = 5;
    for (int i = 0; i < n; i++) {
        float a = PI * 0.15f + i * (PI * 1.7f / (n - 1)); // deja libre la entrada sur
        Vector3 p = { cosf(a) * 11.0f, 0.0f, sinf(a) * 11.0f };
        p.y = terrain_height(t, p.x, p.z);
        c->yurts[c->yurt_count] = p;
        // El modelo mira a +X; lo giramos para que mire al centro.
        c->yurt_rot[c->yurt_count] = atan2f(-p.x, -p.z) * RAD2DEG - 90.0f;
        c->yurt_count++;
    }

    // Arboles dispersos (deterministas) fuera del campamento.
    Rng rng;
    rng_seed(&rng, t->seed * 31u + 7u);
    while (c->tree_count < WORLD_MAX_TREES) {
        float x = (rng_float(&rng) * 2.0f - 1.0f) * 110.0f;
        float z = (rng_float(&rng) * 2.0f - 1.0f) * 110.0f;
        if (x * x + z * z < 30.0f * 30.0f) continue;
        c->trees[c->tree_count] = (Vector3){ x, terrain_height(t, x, z), z };
        c->tree_h[c->tree_count] = 3.0f + rng_float(&rng) * 3.0f;
        c->tree_count++;
    }
}

static void draw_fallback_yurt(Vector3 p) {
    // Respaldo si falta el GLB: misma silueta con primitivas.
    DrawCylinder((Vector3){ p.x, p.y, p.z }, 2.2f, 2.2f, 1.7f, 10, (Color){ 233, 228, 214, 255 });
    DrawCylinder((Vector3){ p.x, p.y + 1.7f, p.z }, 0.45f, 2.35f, 0.9f, 10, (Color){ 216, 208, 189, 255 });
}

void camp_draw(const Camp *c, float time) {
    for (int i = 0; i < c->yurt_count; i++) {
        if (c->yurt_loaded)
            DrawModelEx(c->yurt, c->yurts[i], (Vector3){ 0, 1, 0 }, c->yurt_rot[i], (Vector3){ 1, 1, 1 }, WHITE);
        else
            draw_fallback_yurt(c->yurts[i]);
    }

    // Fogata: piedras + llama que titila.
    for (int i = 0; i < 6; i++) {
        float a = i * PI / 3.0f;
        DrawCube((Vector3){ c->fire.x + cosf(a) * 0.8f, c->fire.y + 0.12f, c->fire.z + sinf(a) * 0.8f },
                 0.35f, 0.25f, 0.35f, (Color){ 110, 106, 100, 255 });
    }
    float flick = 0.85f + 0.15f * sinf(time * 13.0f) * sinf(time * 7.3f);
    DrawCylinder((Vector3){ c->fire.x, c->fire.y + 0.1f, c->fire.z }, 0.0f, 0.45f, 0.9f * flick, 5, (Color){ 240, 140, 40, 255 });
    DrawCylinder((Vector3){ c->fire.x, c->fire.y + 0.1f, c->fire.z }, 0.0f, 0.25f, 0.6f * flick, 5, (Color){ 255, 220, 90, 255 });

    // Arboles low-poly: tronco + copa conica.
    for (int i = 0; i < c->tree_count; i++) {
        Vector3 b = c->trees[i];
        float h = c->tree_h[i];
        DrawCylinder(b, 0.18f, 0.25f, h * 0.35f, 5, (Color){ 96, 70, 46, 255 });
        DrawCylinder((Vector3){ b.x, b.y + h * 0.3f, b.z }, 0.0f, h * 0.32f, h * 0.75f, 6, (Color){ 74, 104, 60, 255 });
    }
}

void camp_unload(Camp *c) {
    if (c->yurt_loaded) UnloadModel(c->yurt);
    c->yurt_loaded = false;
}
