#pragma once
// Small deterministic noise helpers shared by the texture kit and the environment builders.
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>

// Pseudo-random 0..1 from integer coordinates (same inputs -> same output)
inline float hash(int x, int y, int seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xffffff) / float(0xffffff);
}

// Smooth value noise 0..1; wraps every periodX / periodY cells (for seamless textures)
inline float valueNoise(float x, float y, int periodX, int periodY, int seed) {
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    float fx = x - x0, fy = y - y0;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    auto h = [&](int i, int j) { return hash(((i % periodX) + periodX) % periodX, ((j % periodY) + periodY) % periodY, seed); };
    float a = h(x0, y0), b = h(x0 + 1, y0), c = h(x0, y0 + 1), d = h(x0 + 1, y0 + 1);
    return glm::mix(glm::mix(a, b, fx), glm::mix(c, d, fx), fy);
}

// Fractal (several octaves) tileable noise for texture coordinates u, v in 0..1.
// cellsX / cellsY = size of the coarsest octave in cells.
inline float fbm(float u, float v, int cellsX, int cellsY, int octaves, int seed) {
    float sum = 0, amp = 1, total = 0;
    for (int o = 0; o < octaves; ++o) {
        sum += amp * valueNoise(u * cellsX, v * cellsY, cellsX, cellsY, seed + o * 31);
        total += amp;
        amp *= 0.5f;
        cellsX *= 2;
        cellsY *= 2;
    }
    return sum / total;
}

// Fractal noise over the world (x, z in meters, `scale` meters per coarsest cell), 0..1, not tiling
inline float worldNoise(float x, float z, float scale, int octaves, int seed) {
    float sum = 0, amp = 1, total = 0, f = 1 / scale;
    for (int o = 0; o < octaves; ++o) {
        sum += amp * valueNoise(x * f + 1000, z * f + 1000, 1 << 20, 1 << 20, seed + o * 31);
        total += amp;
        amp *= 0.5f;
        f *= 2;
    }
    return sum / total;
}
