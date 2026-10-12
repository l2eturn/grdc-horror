#pragma once
// Small collision helpers for a walking player: circles against axis-aligned boxes on the ground plane,
// and a ray against boxes for "what am I looking at" (interaction, line of sight).
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <glm/glm.hpp>

namespace ter {

// axis-aligned box in world space
struct Box {
    glm::vec3 min, max;
    // a box from its centre and full size
    static Box centered(glm::vec3 c, glm::vec3 size) { return {c - size * 0.5f, c + size * 0.5f}; }
    bool contains(glm::vec3 p) const { return glm::all(glm::greaterThanEqual(p, min)) && glm::all(glm::lessThanEqual(p, max)); }
};

// Push a circle (x/z, radius r) out of every box whose height range overlaps [y0, y1]. Returns the new x/z.
inline glm::vec3 resolveCircle(glm::vec3 p, float r, const std::vector<Box>& boxes, float y0 = 0.1f, float y1 = 1.8f) {
    // a few passes so corners between two boxes settle
    for (int pass = 0; pass < 3; ++pass)
        for (const Box& b : boxes) {
            if (b.max.y < y0 || b.min.y > y1) continue;
            float cx = std::clamp(p.x, b.min.x, b.max.x), cz = std::clamp(p.z, b.min.z, b.max.z);
            float dx = p.x - cx, dz = p.z - cz, d2 = dx * dx + dz * dz;
            if (d2 >= r * r) continue;
            if (d2 > 1e-8f) {
                float d = std::sqrt(d2);
                p.x = cx + dx / d * r;
                p.z = cz + dz / d * r;
            } else {
                // centre inside the box: leave through the nearest side
                float l = p.x - b.min.x, rr = b.max.x - p.x, n = p.z - b.min.z, s = b.max.z - p.z;
                float m = std::min({l, rr, n, s});
                if (m == l) p.x = b.min.x - r;
                else if (m == rr) p.x = b.max.x + r;
                else if (m == n) p.z = b.min.z - r;
                else p.z = b.max.z + r;
            }
        }
    return p;
}

// Distance along the ray (dir normalised) to the box, or -1 when it misses / is beyond maxDist
inline float rayBox(glm::vec3 origin, glm::vec3 dir, const Box& b, float maxDist = std::numeric_limits<float>::max()) {
    float t0 = 0, t1 = maxDist;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(dir[i]) < 1e-8f) {
            if (origin[i] < b.min[i] || origin[i] > b.max[i]) return -1;
            continue;
        }
        float a = (b.min[i] - origin[i]) / dir[i], c = (b.max[i] - origin[i]) / dir[i];
        if (a > c) std::swap(a, c);
        t0 = std::max(t0, a);
        t1 = std::min(t1, c);
        if (t0 > t1) return -1;
    }
    return t0;
}

// Index of the nearest box the ray hits (and its distance), or -1
inline int raycast(glm::vec3 origin, glm::vec3 dir, const std::vector<Box>& boxes, float maxDist, float* hitDist = nullptr) {
    int best = -1;
    float bestT = maxDist;
    for (size_t i = 0; i < boxes.size(); ++i) {
        float t = rayBox(origin, dir, boxes[i], bestT);
        if (t >= 0 && t <= bestT) {
            best = (int)i;
            bestT = t;
        }
    }
    if (hitDist) *hitDist = bestT;
    return best;
}

}  // namespace ter
