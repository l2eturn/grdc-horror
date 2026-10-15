#include "Trees.h"

#include "MeshKit.h"

#include <cmath>
#include <vector>

namespace {

// The trunk's centre line and radius at height y: it leans and bends a little, flares at the roots
struct TrunkShape {
    float height, radius, bend, phase;
    glm::vec3 center(float y) const {
        float t = y / height;
        return {std::sin(t * 2.1f + phase) * bend * t, y, std::cos(t * 1.7f + phase * 1.3f) * bend * t};
    }
    float radiusAt(float y) const {
        float t = glm::clamp(y / height, 0.0f, 1.0f);
        return radius * std::pow(1 - t, 0.75f) * (1 + 0.6f * std::exp(-y * 3)) + 0.02f;
    }
};

// 8-sided tapered trunk with bark UVs (u around, v = meters / 2 up)
ter::GeoPtr trunkGeometry(const TrunkShape& shape) {
    auto g = std::make_shared<ter::Geometry>();
    const int sides = 8;
    int rings = std::max(4, (int)(shape.height / 0.8f));
    for (int r = 0; r <= rings; ++r) {
        float y = shape.height * r / rings;
        glm::vec3 c = shape.center(y);
        float rad = shape.radiusAt(y);
        for (int s = 0; s <= sides; ++s) {
            float a = ter::PI * 2 * s / sides;
            glm::vec3 out(std::cos(a), 0, std::sin(a));
            g->position.push_back(c + out * rad);
            g->normal.push_back(glm::normalize(out + glm::vec3(0, 0.15f, 0)));
            g->uv.push_back({(float)s / sides, y / 2});
        }
    }
    for (int r = 0; r < rings; ++r)
        for (int s = 0; s < sides; ++s) {
            uint32_t i = r * (sides + 1) + s, k = i + sides + 1;
            g->index.insert(g->index.end(), {i, k, i + 1, i + 1, k, k + 1});
        }
    return g;
}

// One branch card: a quad from `origin` along `along` (length) and `across` (width), narrower at the tip.
// All four corners share `normal`, pointing away from the crown, so the canopy shades as a volume.
void addCard(ter::Geometry& g, glm::vec3 origin, glm::vec3 along, glm::vec3 across, float length, float width, glm::vec3 normal) {
    uint32_t base = (uint32_t)g.position.size();
    glm::vec3 tip = origin + along * length;
    g.position.insert(g.position.end(), {origin - across * width * 0.35f, origin + across * width * 0.35f, tip - across * width * 0.5f, tip + across * width * 0.5f});
    g.uv.insert(g.uv.end(), {{0, 0}, {0, 1}, {1, 0}, {1, 1}});
    g.normal.insert(g.normal.end(), 4, normal);
    g.index.insert(g.index.end(), {base, base + 2, base + 1, base + 1, base + 2, base + 3});
}

}  // namespace

TreeModel makePine(int seed) {
    ter::Rng rng(seed * 7919 + 13);
    TrunkShape shape{10 + rng.f() * 4, 0.2f + rng.f() * 0.08f, 0.25f + rng.f() * 0.35f, rng.f() * 6.28f};

    // trunk plus a few dead twigs below the crown (lower branches of real pines die off in a dense forest)
    std::vector<ter::GeoPtr> wood{trunkGeometry(shape)};
    float crownStart = shape.height * (0.4f + rng.f() * 0.12f);
    for (int i = 0; i < 9; ++i) {
        float y = 1.6f + rng.f() * (crownStart - 1.8f), a = rng.f() * ter::PI * 2;
        glm::vec3 out(std::cos(a), 0, std::sin(a));
        glm::vec3 from = shape.center(y) + out * shape.radiusAt(y) * 0.8f;
        glm::vec3 to = from + glm::normalize(out + glm::vec3(0, -0.25f + rng.f() * 0.3f, 0)) * (0.3f + rng.f() * 0.7f);
        wood.push_back(beamGeo(from, to, 0.035f, 0.035f));
    }

    // whorls of branches from crownStart to the top; long and drooping low, short near the tip
    auto leaves = std::make_shared<ter::Geometry>();
    for (float y = crownStart; y < shape.height - 0.6f; y += 0.45f + rng.f() * 0.35f) {
        float t = (y - crownStart) / (shape.height - crownStart);
        float length = (0.5f + 2.7f * std::pow(1 - t, 1.1f)) * (0.85f + rng.f() * 0.3f);
        int count = 4 + (int)(rng.f() * 3);
        float phase = rng.f() * ter::PI * 2;
        for (int k = 0; k < count; ++k) {
            float a = phase + ter::PI * 2 * k / count + (rng.f() - 0.5f) * 0.6f;
            glm::vec3 out(std::cos(a), 0, std::sin(a));
            float droop = 0.12f + 0.4f * (1 - t) + rng.f() * 0.15f;   // radians below horizontal
            glm::vec3 along = glm::normalize(out * std::cos(droop) - glm::vec3(0, std::sin(droop), 0));
            glm::vec3 side = glm::normalize(glm::cross(along, glm::vec3(0, 1, 0)));
            glm::vec3 up = glm::cross(side, along);
            glm::vec3 origin = shape.center(y) + out * shape.radiusAt(y) * 0.7f;
            glm::vec3 normal = glm::normalize(out * 0.6f + glm::vec3(0, 0.8f, 0));
            float width = 0.45f + 0.45f * length;
            addCard(*leaves, origin, along, side, length, width, normal);                                    // flat spray
            addCard(*leaves, origin, along, glm::normalize(side * 0.35f + up), length * 0.9f, width * 0.6f, normal);   // tilted, for depth
        }
    }
    // the leader at the very top: two crossed upright cards
    glm::vec3 top = shape.center(shape.height - 1.2f);
    addCard(*leaves, top, {0, 1, 0}, {1, 0, 0}, 1.7f, 0.9f, {0, 1, 0});
    addCard(*leaves, top, {0, 1, 0}, {0, 0, 1}, 1.7f, 0.9f, {0, 1, 0});

    return {ter::geo::merge(wood), leaves, shape.radius, false};
}

TreeModel makeDeadTree(int seed) {
    ter::Rng rng(seed * 104729 + 7);
    TrunkShape shape{7 + rng.f() * 3, 0.17f + rng.f() * 0.06f, 0.4f + rng.f() * 0.4f, rng.f() * 6.28f};
    std::vector<ter::GeoPtr> wood{trunkGeometry(shape)};
    // bare branches reaching up and out, some with a fork
    for (int i = 0; i < 14; ++i) {
        float y = shape.height * (0.3f + rng.f() * 0.65f), a = rng.f() * ter::PI * 2;
        glm::vec3 out(std::cos(a), 0, std::sin(a));
        glm::vec3 from = shape.center(y) + out * shape.radiusAt(y) * 0.6f;
        float len = (1.0f - (y / shape.height) * 0.6f) * (1.0f + rng.f() * 1.4f);
        glm::vec3 dir = glm::normalize(out + glm::vec3(0, 0.3f + rng.f() * 0.7f, 0));
        glm::vec3 to = from + dir * len;
        float thick = 0.06f * (1 - y / shape.height) + 0.025f;
        wood.push_back(beamGeo(from, to, thick, thick));
        if (rng.f() < 0.6f) {
            glm::vec3 fork = glm::normalize(dir + glm::vec3(rng.f() - 0.5f, 0.4f, rng.f() - 0.5f));
            glm::vec3 mid = glm::mix(from, to, 0.6f);
            wood.push_back(beamGeo(mid, mid + fork * len * 0.5f, 0.022f, 0.022f));
        }
    }
    return {ter::geo::merge(wood), nullptr, shape.radius, true};
}
