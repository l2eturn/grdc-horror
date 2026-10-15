#include "MeshKit.h"

#include "Textures.h"

namespace {

// Columns of a matrix whose Y axis points along `dir` (unit), for tubes
glm::mat4 alongY(glm::vec3 dir, glm::vec3 center) {
    glm::vec3 ref = std::abs(dir.y) > 0.95f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
    glm::vec3 x = glm::normalize(glm::cross(dir, ref));
    glm::vec3 z = glm::cross(x, dir);
    return glm::mat4(glm::vec4(x, 0), glm::vec4(dir, 0), glm::vec4(z, 0), glm::vec4(center, 1));
}

}  // namespace

ter::GeoPtr beamGeo(glm::vec3 a, glm::vec3 b, float width, float height) {
    glm::vec3 x = b - a;
    float len = glm::length(x);
    x /= len;
    glm::vec3 ref = std::abs(x.y) > 0.95f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
    glm::vec3 z = glm::normalize(glm::cross(x, ref));
    glm::vec3 y = glm::cross(z, x);
    glm::mat4 m(glm::vec4(x * len, 0), glm::vec4(y * height, 0), glm::vec4(z * width, 0), glm::vec4((a + b) * 0.5f, 1));
    auto g = ter::geo::box(1, 1, 1);
    g->applyMatrix(m);
    return g;
}

ter::GeoPtr tubeGeo(glm::vec3 a, glm::vec3 b, float ra, float rb, int sides) {
    glm::vec3 d = b - a;
    float len = glm::length(d);
    // three.js cylinder: radiusTop is at +Y, which we point at b
    auto g = ter::geo::cylinder(rb, ra, len, sides);
    g->applyMatrix(alongY(d / len, (a + b) * 0.5f));
    return g;
}

ter::MatPtr kitMat(const std::string& textureName, glm::vec3 tint, glm::vec2 repeat) {
    auto m = ter::lambert(texture(textureName), tint);
    m->mapRepeat = repeat;
    return m;
}

ter::ObjPtr texturedBox(glm::vec3 size, const std::string& textureName, float tile, glm::vec3 tint) {
    auto geo = ter::geo::worldUV(ter::geo::box(size.x, size.y, size.z), 1.0f / tile);
    return ter::makeMesh(geo, ter::lambert(texture(textureName), tint));
}

ter::ObjPtr meshAt(const ter::GeoPtr& geo, const ter::MatPtr& mat, glm::vec3 pos, glm::vec3 rot) {
    auto m = ter::makeMesh(geo, mat);
    m->position = pos;
    m->rotation = rot;
    return m;
}
