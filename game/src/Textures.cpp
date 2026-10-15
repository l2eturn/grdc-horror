#include "Textures.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <vector>
#include "Noise.h"

namespace {

glm::vec3 rgb(uint32_t c) { return glm::vec3((c >> 16 & 255) / 255.f, (c >> 8 & 255) / 255.f, (c & 255) / 255.f); }

// ---------------------------------------------------------------- image

struct Image {
    int w, h;
    std::vector<glm::vec4> px;
    Image(int w_, int h_, glm::vec4 fill = glm::vec4(0, 0, 0, 1)) : w(w_), h(h_), px((size_t)w_ * h_, fill) {}

    glm::vec4& at(int x, int y) { return px[(size_t)(((y % h) + h) % h) * w + ((x % w) + w) % w]; }
    // u, v in 0..1 for every pixel
    void each(const std::function<glm::vec4(float u, float v, int x, int y)>& fn) {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) px[(size_t)y * w + x] = fn((x + 0.5f) / w, (y + 0.5f) / h, x, y);
    }
    // blend a colour into one pixel (wraps around the edges)
    void blend(int x, int y, glm::vec3 c, float a) {
        glm::vec4& p = at(x, y);
        p = glm::vec4(glm::mix(glm::vec3(p), c, a), p.a);
    }
    // a wandering crack / streak starting at (x, y)
    void walk(int x, int y, int steps, int dx, int dy, glm::vec3 c, float a, int seed) {
        for (int i = 0; i < steps; ++i) {
            blend(x, y, c, a);
            float r = hash(i, seed, 99);
            if (dx) { x += dx; if (r < 0.3f) --y; else if (r > 0.7f) ++y; }
            else { y += dy; if (r < 0.3f) --x; else if (r > 0.7f) ++x; }
        }
    }

    // a filled ellipse (rotated by `angle`), alpha-blended
    void ellipse(glm::vec2 c, glm::vec2 r, float angle, glm::vec4 col) {
        float ca = std::cos(angle), sa = std::sin(angle);
        int ext = (int)std::ceil(std::max(r.x, r.y)) + 1;
        for (int y = (int)c.y - ext; y <= (int)c.y + ext; ++y)
            for (int x = (int)c.x - ext; x <= (int)c.x + ext; ++x) {
                if (x < 0 || y < 0 || x >= w || y >= h) continue;
                glm::vec2 d(x - c.x, y - c.y);
                glm::vec2 q(d.x * ca + d.y * sa, -d.x * sa + d.y * ca);
                if ((q.x * q.x) / (r.x * r.x) + (q.y * q.y) / (r.y * r.y) > 1) continue;
                glm::vec4& p = at(x, y);
                p = glm::vec4(glm::mix(glm::vec3(p), glm::vec3(col), col.a), std::max(p.a, col.a));
            }
    }

    // a straight line with a given width (pixels), alpha-blended over what is there
    void line(glm::vec2 a, glm::vec2 b, float width, glm::vec4 c) {
        int steps = (int)std::ceil(glm::length(b - a) * 2) + 1;
        for (int i = 0; i <= steps; ++i) {
            glm::vec2 p = glm::mix(a, b, (float)i / steps);
            for (int oy = (int)std::floor(-width / 2); oy <= (int)std::ceil(width / 2); ++oy)
                for (int ox = (int)std::floor(-width / 2); ox <= (int)std::ceil(width / 2); ++ox) {
                    int x = (int)std::lround(p.x) + ox, y = (int)std::lround(p.y) + oy;
                    if (x < 0 || y < 0 || x >= w || y >= h) continue;
                    if (glm::length(glm::vec2(x, y) - p) > width / 2 + 0.3f) continue;
                    glm::vec4& d = at(x, y);
                    d = glm::vec4(glm::mix(glm::vec3(d), glm::vec3(c), c.a), std::max(d.a, c.a));
                }
        }
    }

    std::shared_ptr<ter::Texture> upload(bool repeat = true) const {
        std::vector<uint8_t> bytes((size_t)w * h * 4);
        for (size_t i = 0; i < px.size(); ++i)
            for (int k = 0; k < 4; ++k) bytes[i * 4 + k] = (uint8_t)std::lround(glm::clamp(px[i][k], 0.0f, 1.0f) * 255);
        ter::TextureOptions o;
        o.nearest = false;   // PS2: bilinear filtering, not PS1 nearest
        o.repeat = repeat;
        auto t = std::make_shared<ter::Texture>();
        t->upload(w, h, bytes, o);
        return t;
    }
};

glm::vec4 opaque(glm::vec3 c) { return glm::vec4(c, 1); }

// ---------------------------------------------------------------- the textures

// Road surface for one 8 m stretch of a 6 m two-lane road (u = along the road, v = across it):
// aggregate, polished darker wheel tracks in each lane, a darker repair patch, cracks along the edges,
// worn white edge lines and a dashed yellow centre line (3 m dash, 5 m gap).
std::shared_ptr<ter::Texture> makeAsphalt() {
    const int W = 512, H = 128;
    Image img(W, H);
    img.each([](float u, float v, int x, int y) {
        float n = fbm(u, v, 16, 4, 4, 1);
        glm::vec3 c = glm::mix(rgb(0x262729), rgb(0x3d3e40), n);
        float grain = hash(x, y, 2);
        if (grain > 0.9f) c *= 1.25f + (grain - 0.9f) * 3;   // light aggregate stones
        else if (grain < 0.05f) c *= 0.6f;
        // wheel tracks: two per lane, smoother and darker
        for (float t : {0.14f, 0.36f, 0.64f, 0.86f}) c *= 1 - 0.16f * glm::smoothstep(0.07f, 0.0f, std::abs(v - t)) * (0.7f + 0.3f * fbm(u, v, 8, 2, 2, 3));
        // repair patch (newer, darker asphalt with a crisp edge)
        if (u > 0.55f && u < 0.83f && v > 0.58f && v < 0.92f) c = glm::mix(rgb(0x1d1e20), rgb(0x2a2b2d), n) * (grain > 0.95f ? 1.2f : 1.0f);
        return opaque(c);
    });
    // patch seam
    for (int x = (int)(0.55f * W); x <= (int)(0.83f * W); ++x) { img.blend(x, (int)(0.58f * H), rgb(0x101010), 0.8f); img.blend(x, (int)(0.92f * H), rgb(0x101010), 0.8f); }
    for (int y = (int)(0.58f * H); y <= (int)(0.92f * H); ++y) { img.blend((int)(0.55f * W), y, rgb(0x101010), 0.8f); img.blend((int)(0.83f * W), y, rgb(0x101010), 0.8f); }
    // cracks: long ones near the edges, a few across
    for (int i = 0; i < 8; ++i) img.walk((int)(hash(i, 0, 4) * W), (int)((i % 2 ? 0.08f : 0.9f) * H + hash(i, 1, 4) * 8), 160, 1, 0, rgb(0x121212), 0.75f, i);
    for (int i = 0; i < 5; ++i) img.walk((int)(hash(i, 2, 4) * W), (int)(hash(i, 3, 4) * H), 30, 0, 1, rgb(0x141414), 0.6f, i + 20);
    // paint, worn away by traffic
    auto paint = [&](float v0, float v1, float u0, float u1, glm::vec3 c) {
        for (int y = (int)(v0 * H); y < (int)(v1 * H); ++y)
            for (int x = (int)(u0 * W); x < (int)(u1 * W); ++x) {
                float wear = fbm(x / (float)W, y / (float)H, 32, 8, 3, 5);
                if (wear > 0.36f) img.blend(x, y, c, 0.6f * glm::smoothstep(0.36f, 0.52f, wear));
            }
    };
    paint(0.025f, 0.05f, 0, 1, rgb(0xc4c4bc));
    paint(0.95f, 0.975f, 0, 1, rgb(0xc4c4bc));
    paint(0.49f, 0.515f, 0, 0.375f, rgb(0xc0a24c));
    return img.upload();
}

// A puddle shape: soft-edged irregular blob (RGBA white; the material makes it dark and glossy)
std::shared_ptr<ter::Texture> makePuddle() {
    Image img(128, 128);
    img.each([](float u, float v, int, int) {
        float r = glm::length(glm::vec2(u, v) - 0.5f) * 2;
        float shape = fbm(u, v, 3, 3, 4, 210) - r * 0.75f;
        return glm::vec4(1, 1, 1, glm::smoothstep(0.02f, 0.12f, shape));
    });
    return img.upload(false);
}

// Fallen pine needles, a few cones and dead leaves (RGBA) to scatter on the road edges
std::shared_ptr<ter::Texture> makeLitter() {
    Image img(128, 128, glm::vec4(rgb(0x3a2c1c), 0));
    ter::Rng rng(220);
    for (int i = 0; i < 90; ++i) {
        glm::vec2 a(8 + rng.f() * 112, 8 + rng.f() * 112);
        float ang = rng.f() * ter::PI * 2, len = 5 + rng.f() * 7;
        glm::vec3 c = rng.f() < 0.7f ? glm::mix(rgb(0x4a3420), rgb(0x7a5a34), rng.f()) : glm::mix(rgb(0x24301e), rgb(0x3c4a2a), rng.f());
        img.line(a, a + glm::vec2(std::cos(ang), std::sin(ang)) * len, 1.0f, glm::vec4(c, 1));
    }
    for (int i = 0; i < 6; ++i)   // dead leaves
        img.ellipse({10 + rng.f() * 108, 10 + rng.f() * 108}, {5 + rng.f() * 3, 2.5f + rng.f()}, rng.f() * 3.14f, glm::vec4(glm::mix(rgb(0x5a3a1c), rgb(0x8a5a26), rng.f()), 1));
    for (int i = 0; i < 3; ++i) {   // pine cones: dark body with lighter scales
        glm::vec2 c(16 + rng.f() * 96, 16 + rng.f() * 96);
        float ang = rng.f() * 3.14f;
        img.ellipse(c, {7, 4}, ang, glm::vec4(rgb(0x3a2614), 1));
        for (int k = -2; k <= 2; ++k) img.ellipse(c + glm::vec2(std::cos(ang), std::sin(ang)) * (k * 2.5f), {1.5f, 3.2f}, ang, glm::vec4(rgb(0x6a4a2a), 1));
    }
    return img.upload(false);
}

// Dark oil / water stain with a soft noisy edge (RGBA)
std::shared_ptr<ter::Texture> makeStain() {
    Image img(128, 128);
    img.each([](float u, float v, int, int) {
        float r = glm::length(glm::vec2(u, v) - 0.5f) * 2;
        float a = glm::clamp(fbm(u, v, 4, 4, 4, 230) * 1.4f - r, 0.0f, 1.0f);
        return glm::vec4(rgb(0x0a0a0b), glm::smoothstep(0.0f, 0.4f, a) * 0.7f);
    });
    return img.upload(false);
}

// A pair of tyre skid marks (RGBA), running along u
std::shared_ptr<ter::Texture> makeSkid() {
    Image img(256, 64);
    img.each([](float u, float v, int, int) {
        float tracks = std::max(glm::smoothstep(0.1f, 0.0f, std::abs(v - 0.28f)), glm::smoothstep(0.1f, 0.0f, std::abs(v - 0.72f)));
        float ends = glm::smoothstep(0.0f, 0.15f, u) * glm::smoothstep(1.0f, 0.7f, u);
        float broken = 0.5f + 0.5f * fbm(u, v, 24, 4, 3, 240);
        return glm::vec4(rgb(0x070707), tracks * ends * broken * 0.75f);
    });
    return img.upload(false);
}

std::shared_ptr<ter::Texture> makeGrass() {
    Image img(128, 128);
    img.each([](float u, float v, int x, int y) {
        float patch = fbm(u, v, 3, 3, 3, 10), n = fbm(u, v, 16, 16, 3, 11);
        glm::vec3 c = glm::mix(rgb(0x26331f), rgb(0x3a4628), n);
        c = glm::mix(c, rgb(0x4a4030), glm::smoothstep(0.55f, 0.75f, patch) * 0.7f);   // dry brown patches
        return opaque(c);
    });
    for (int i = 0; i < 260; ++i)   // short blade strokes
        img.walk((int)(hash(i, 0, 12) * 128), (int)(hash(i, 1, 12) * 128), 3 + (int)(hash(i, 2, 12) * 4), 0, -1,
                 hash(i, 3, 12) > 0.5f ? rgb(0x4d5a34) : rgb(0x18200f), 0.5f, i);
    return img.upload();
}

std::shared_ptr<ter::Texture> makeDirt() {
    Image img(128, 128);
    img.each([](float u, float v, int x, int y) {
        float n = fbm(u, v, 6, 6, 4, 20);
        return opaque(glm::mix(rgb(0x3f3326), rgb(0x5e4c38), n));
    });
    for (int i = 0; i < 90; ++i) {   // pebbles: a light pixel with a shadow under it
        int x = (int)(hash(i, 0, 21) * 128), y = (int)(hash(i, 1, 21) * 128);
        img.blend(x, y, rgb(0x8a7a66), 0.8f);
        img.blend(x + 1, y, rgb(0x7a6a56), 0.6f);
        img.blend(x, y + 1, rgb(0x231a12), 0.6f);
    }
    return img.upload();
}

std::shared_ptr<ter::Texture> makeRock(bool cliff) {
    Image img(128, 128);
    img.each([cliff](float u, float v, int x, int y) {
        float n = cliff ? fbm(u, v, 12, 2, 4, 30) * 0.6f + fbm(u, v, 4, 4, 3, 31) * 0.4f : fbm(u, v, 5, 5, 5, 32);
        glm::vec3 c = glm::mix(rgb(0x3e3f41), rgb(0x77787a), n);
        float lichen = fbm(u, v, 4, 4, 3, 33);
        c = glm::mix(c, rgb(0x4a5236), glm::smoothstep(0.62f, 0.8f, lichen) * 0.6f);
        return opaque(c);
    });
    for (int i = 0; i < 10; ++i)
        img.walk((int)(hash(i, 0, 34) * 128), (int)(hash(i, 1, 34) * 128), 40, cliff ? 0 : 1, cliff ? 1 : 0, rgb(0x1e1e1f), 0.6f, i + 50);
    return img.upload();
}

// Pine bark: rough plates separated by dark vertical fissures, with some moss low down.
// Wraps horizontally around the trunk (u) and tiles every 2 m up it (v).
std::shared_ptr<ter::Texture> makeBark() {
    Image img(128, 256);
    img.each([](float u, float v, int x, int y) {
        float plates = fbm(u, v, 10, 3, 4, 40);
        float fissure = std::abs(fbm(u, v, 14, 2, 3, 41) - 0.5f) * 2;          // 0 in the cracks
        float rough = fbm(u, v, 32, 32, 2, 42);
        glm::vec3 c = glm::mix(rgb(0x2e241c), rgb(0x5a4a3a), plates * 0.7f + rough * 0.3f);
        c *= 0.35f + 0.65f * glm::smoothstep(0.05f, 0.3f, fissure);
        float moss = fbm(u, v, 6, 3, 3, 43);
        c = glm::mix(c, rgb(0x2f3a22), glm::smoothstep(0.62f, 0.8f, moss) * 0.7f);
        return opaque(c);
    });
    return img.upload();
}

// A pine branch for alpha cards: the twig runs left (trunk) to right (tip) along the middle,
// needles fan out from it, side twigs branch off. Transparent elsewhere.
std::shared_ptr<ter::Texture> makeBranch() {
    Image img(256, 128, glm::vec4(rgb(0x1a2618), 0));   // transparent; edge colour bleeds sensibly when filtered
    ter::Rng rng(77);
    auto needleColor = [&]() {
        float k = rng.f();
        glm::vec3 c = glm::mix(rgb(0x142014), rgb(0x3a5232), k * k);
        return glm::vec4(c, 1);
    };
    // a twig from a to b with needles along it; length of needles shrinks toward the tip
    std::function<void(glm::vec2, glm::vec2, float, int)> twig = [&](glm::vec2 a, glm::vec2 b, float needle, int depth) {
        glm::vec2 d = b - a;
        float len = glm::length(d);
        glm::vec2 dir = d / len, side(-dir.y, dir.x);
        int count = (int)(len * 1.6f);
        for (int i = 0; i < count; ++i) {
            float t = rng.f();
            glm::vec2 p = a + d * t;
            float s = (rng.f() < 0.5f ? -1.0f : 1.0f);
            float nl = needle * (1.0f - 0.55f * t) * (0.6f + 0.6f * rng.f());
            glm::vec2 nd = glm::normalize(dir * (0.35f + rng.f() * 0.5f) + side * s);
            img.line(p, p + nd * nl, 1.2f, needleColor());
        }
        img.line(a, b, depth == 0 ? 2.6f : 1.6f, glm::vec4(rgb(0x3a2c20), 1));
        if (depth == 0)
            for (int k = 0; k < 7; ++k) {
                float t = 0.15f + 0.75f * (k / 7.0f) + rng.f() * 0.05f;
                glm::vec2 p = a + d * t;
                float s = (k % 2 ? -1.0f : 1.0f);
                glm::vec2 e = p + glm::normalize(dir * 0.9f + side * s * (0.6f + rng.f() * 0.4f)) * (len * (1 - t) * 0.55f + 14);
                twig(p, e, needle * 0.85f, 1);
            }
    };
    twig({4, 64}, {250, 60}, 26, 0);
    return img.upload(false);
}

std::shared_ptr<ter::Texture> makeNeedles() {
    Image img(128, 128);
    img.each([](float u, float v, int x, int y) {
        float n = fbm(u, v, 8, 8, 4, 50);
        return opaque(glm::mix(rgb(0x121c13), rgb(0x2c3d29), n));
    });
    for (int i = 0; i < 400; ++i)
        img.walk((int)(hash(i, 0, 51) * 128), (int)(hash(i, 1, 51) * 128), 4, 1, 0, hash(i, 2, 51) > 0.5f ? rgb(0x3a4d33) : rgb(0x0a110a), 0.5f, i);
    return img.upload();
}

std::shared_ptr<ter::Texture> makeConcrete() {
    Image img(128, 128);
    img.each([](float u, float v, int x, int y) {
        float n = fbm(u, v, 6, 6, 4, 60);
        glm::vec3 c = glm::mix(rgb(0x6c6a64), rgb(0x8e8b84), n);
        if (hash(x, y, 61) > 0.95f) c *= 0.8f;
        return opaque(c);
    });
    for (int i = 0; i < 14; ++i)   // rain stains running down
        img.walk((int)(hash(i, 0, 62) * 128), 0, 30 + (int)(hash(i, 1, 62) * 90), 0, 1, rgb(0x3e3c38), 0.35f, i + 70);
    return img.upload();
}

std::shared_ptr<ter::Texture> makeMetal() {
    Image img(128, 32);
    img.each([](float u, float v, int x, int y) {
        float n = fbm(u, v, 8, 2, 3, 80);
        glm::vec3 c = glm::mix(rgb(0x7d8083), rgb(0xa2a5a8), n);
        if (y == 10 || y == 21) c *= 0.55f;   // the two grooves of a W-beam rail
        if (y == 11 || y == 22) c *= 1.15f;
        float rust = fbm(u, v, 6, 2, 4, 81);
        return opaque(glm::mix(c, rgb(0x6a3a1e), glm::smoothstep(0.6f, 0.78f, rust) * 0.8f));
    });
    return img.upload();
}

std::shared_ptr<ter::Texture> makeGrassCard() {
    Image img(64, 64, glm::vec4(0.15f, 0.2f, 0.1f, 0));   // transparent; colour bleeds sensibly into filtered edges
    for (int i = 0; i < 22; ++i) {
        float x = 4 + hash(i, 0, 90) * 56, lean = (hash(i, 1, 90) - 0.5f) * 0.6f;
        int height = 26 + (int)(hash(i, 2, 90) * 34);
        glm::vec3 c = glm::mix(rgb(0x23301a), rgb(0x56603a), hash(i, 3, 90));
        for (int y = 0; y < height; ++y) {
            float t = (float)y / height;
            int px = (int)(x + lean * y * t * 1.5f);
            glm::vec3 shade = c * (0.55f + 0.45f * t);   // darker at the root
            img.at(px, 63 - y) = glm::vec4(shade, 1);
            if (t < 0.6f) img.at(px + 1, 63 - y) = glm::vec4(shade * 0.9f, 1);
        }
    }
    return img.upload(false);
}

std::shared_ptr<ter::Texture> makeMist() {
    Image img(128, 128);
    img.each([](float u, float v, int x, int y) {
        float n = fbm(u, v, 3, 3, 4, 100);
        return glm::vec4(rgb(0xa8b2bc), glm::smoothstep(0.35f, 0.75f, n) * 0.55f);
    });
    return img.upload();
}

// Knurled rubber grip of the flashlight: fine diamond pattern, dark grey
std::shared_ptr<ter::Texture> makeGrip() {
    Image img(64, 64);
    img.each([](float u, float v, int x, int y) {
        float a = std::abs(std::fmod((x + y) * 0.5f, 4.0f) - 2) / 2, b = std::abs(std::fmod((x - y + 64) * 0.5f, 4.0f) - 2) / 2;
        float ridge = glm::smoothstep(0.3f, 0.9f, std::min(a, b));
        glm::vec3 c = glm::mix(rgb(0x0f0f10), rgb(0x2e2e30), ridge) * (0.85f + 0.3f * fbm(u, v, 4, 4, 2, 140));
        return opaque(c);
    });
    return img.upload();
}

// Galvanised corrugated roof sheet: ridges along v (u = across the ridges), rust and dirt streaks
std::shared_ptr<ter::Texture> makeCorrugated() {
    Image img(128, 128);
    img.each([](float u, float v, int x, int y) {
        float ridge = 0.5f + 0.5f * std::sin(u * ter::PI * 2 * 8);   // 8 ridges per repeat, shaded like a wave
        glm::vec3 c = glm::mix(rgb(0x55595d), rgb(0x9da2a6), ridge) * (0.85f + 0.3f * fbm(u, v, 8, 8, 3, 150));
        float rust = fbm(u, v, 6, 12, 4, 151);
        c = glm::mix(c, rgb(0x6b3d22), glm::smoothstep(0.58f, 0.78f, rust) * 0.85f);
        float streak = fbm(u, v, 24, 2, 2, 152);
        c *= 0.8f + 0.25f * streak;
        return opaque(c);
    });
    return img.upload();
}

// Weathered wooden planks (grain along u)
std::shared_ptr<ter::Texture> makeWood() {
    Image img(128, 64);
    img.each([](float u, float v, int x, int y) {
        float grain = fbm(u, v, 2, 16, 4, 160);
        glm::vec3 c = glm::mix(rgb(0x3a2a1c), rgb(0x6e5438), grain);
        if (y % 16 == 0) c *= 0.45f;   // plank gaps
        return opaque(c * (0.85f + 0.25f * fbm(u, v, 8, 8, 2, 161)));
    });
    return img.upload();
}

// Painted plaster wall: white paint (tint it with the material colour) with small chips showing the
// plaster underneath, faint water stains from the top and grime near the bottom
std::shared_ptr<ter::Texture> makePaint() {
    Image img(128, 128);
    img.each([](float u, float v, int x, int y) {
        glm::vec3 paint = rgb(0xe6e3dc) * (0.92f + 0.08f * fbm(u, v, 16, 16, 2, 171));
        glm::vec3 bare = glm::mix(rgb(0x9c978c), rgb(0xb4afa4), fbm(u, v, 12, 12, 2, 172));
        float chip = fbm(u, v, 9, 9, 4, 170);                                     // small, scattered chips
        glm::vec3 c = glm::mix(paint, bare, glm::smoothstep(0.7f, 0.74f, chip));
        float stain = fbm(u, v, 14, 2, 3, 176) * glm::smoothstep(0.6f, 0.0f, v);   // streaky, strongest at the top
        c *= 1 - 0.18f * glm::smoothstep(0.5f, 0.8f, stain);
        float grime = glm::smoothstep(0.6f, 1.0f, v) * (0.6f + 0.4f * fbm(u, v, 12, 3, 3, 173));
        return opaque(c * (1 - grime * 0.35f));
    });
    return img.upload();
}

// Galvanised steel (poles, legs): smooth mottled grey, no grooves
std::shared_ptr<ter::Texture> makeSteel() {
    Image img(64, 128);
    img.each([](float u, float v, int x, int y) {
        float n = fbm(u, v, 4, 8, 4, 180);
        glm::vec3 c = glm::mix(rgb(0x7c8084), rgb(0xa6aaae), n);
        float rust = fbm(u, v, 3, 6, 4, 181);
        return opaque(glm::mix(c, rgb(0x5e3a24), glm::smoothstep(0.66f, 0.8f, rust) * 0.6f));
    });
    return img.upload();
}

// Skin: warm, slightly uneven tone (the hand holding the flashlight)
std::shared_ptr<ter::Texture> makeSkin() {
    Image img(64, 64);
    img.each([](float u, float v, int x, int y) {
        float n = fbm(u, v, 6, 6, 3, 190);
        return opaque(glm::mix(rgb(0x8a6450), rgb(0xb88e74), n));
    });
    return img.upload();
}

// Jacket fabric: fine weave with worn, lighter folds (tint it)
std::shared_ptr<ter::Texture> makeCloth() {
    Image img(64, 64);
    img.each([](float u, float v, int x, int y) {
        float weave = ((x + y) % 2 == 0) ? 1.0f : 0.86f;
        float folds = fbm(u, v, 3, 6, 3, 195);
        return opaque(glm::vec3(0.55f + 0.5f * folds) * weave);
    });
    return img.upload();
}

// Round light: "glow" fades smoothly from the centre (halos); "disc" is solid with a soft rim (the moon)
std::shared_ptr<ter::Texture> makeRound(bool disc) {
    Image img(64, 64);
    img.each([disc](float u, float v, int, int) {
        float r = glm::length(glm::vec2(u, v) - 0.5f) * 2;
        float a = disc ? 1 - glm::smoothstep(0.8f, 1.0f, r) : std::pow(glm::clamp(1 - r, 0.0f, 1.0f), 2.2f);
        return glm::vec4(1, 1, 1, a);
    });
    return img.upload(false);
}

// A soft puff of smoke: warped cloudy noise inside a round falloff (RGBA, white; tint with the material)
std::shared_ptr<ter::Texture> makeSmoke(int seed) {
    Image img(128, 128);
    img.each([seed](float u, float v, int, int) {
        float wu = u + (fbm(u, v, 3, 3, 3, seed + 1) - 0.5f) * 0.35f;
        float wv = v + (fbm(u, v, 3, 3, 3, seed + 2) - 0.5f) * 0.35f;
        float cloud = glm::smoothstep(0.3f, 0.75f, fbm(wu, wv, 4, 4, 5, seed));
        float r = glm::length(glm::vec2(u, v) - 0.5f) * 2;
        float fall = std::pow(glm::clamp(1 - r, 0.0f, 1.0f), 1.4f);
        return glm::vec4(1, 1, 1, cloud * fall);
    });
    return img.upload(false);
}

}  // namespace

std::shared_ptr<ter::Texture> texture(const std::string& name) {
    static std::map<std::string, std::shared_ptr<ter::Texture>> cache;
    static const std::map<std::string, std::function<std::shared_ptr<ter::Texture>()>> makers = {
        {"asphalt", makeAsphalt},   {"grass", makeGrass},     {"dirt", makeDirt},
        {"rock", [] { return makeRock(false); }},               {"cliff", [] { return makeRock(true); }},
        {"bark", makeBark},         {"needles", makeNeedles}, {"concrete", makeConcrete},
        {"metal", makeMetal},       {"grasscard", makeGrassCard}, {"mist", makeMist},
        {"branch", makeBranch},     {"grip", makeGrip},       {"corrugated", makeCorrugated},
        {"wood", makeWood},         {"paint", makePaint},     {"steel", makeSteel},
        {"skin", makeSkin},         {"cloth", makeCloth},
        {"puddle", makePuddle},     {"litter", makeLitter},   {"stain", makeStain},     {"skid", makeSkid},
        {"smoke", [] { return makeSmoke(200); }},               {"smoke2", [] { return makeSmoke(300); }},
        {"glow", [] { return makeRound(false); }},              {"disc", [] { return makeRound(true); }},
    };
    if (auto it = cache.find(name); it != cache.end()) return it->second;
    auto maker = makers.find(name);
    if (maker == makers.end()) {
        std::fprintf(stderr, "texture: unknown name \"%s\"\n", name.c_str());
        return nullptr;
    }
    return cache[name] = maker->second();
}
