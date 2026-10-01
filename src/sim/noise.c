#include "noise.h"

#include <math.h>

static float hash2(int x, int y, uint32_t seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFFFF) * (2.0f / 16777215.0f) - 1.0f;
}

static float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

float noise2d(float x, float y, uint32_t seed) {
    int x0 = (int)floorf(x), y0 = (int)floorf(y);
    float fx = smooth(x - x0), fy = smooth(y - y0);
    float a = hash2(x0, y0, seed), b = hash2(x0 + 1, y0, seed);
    float c = hash2(x0, y0 + 1, seed), d = hash2(x0 + 1, y0 + 1, seed);
    float top = a + (b - a) * fx;
    float bot = c + (d - c) * fx;
    return top + (bot - top) * fy;
}

float fbm2d(float x, float y, uint32_t seed, int octaves) {
    float sum = 0.0f, amp = 1.0f, norm = 0.0f;
    for (int i = 0; i < octaves; i++) {
        sum += noise2d(x, y, seed + (uint32_t)i * 101u) * amp;
        norm += amp;
        amp *= 0.5f;
        x *= 2.0f;
        y *= 2.0f;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}
