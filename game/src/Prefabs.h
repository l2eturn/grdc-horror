#pragma once
// Internal to the world loader: the Builder that turns map.json entries into scene objects.
// Props (box, bus_shelter, lamp_post, sign, bin, companion) live in Props.cpp, trees in World.cpp + Trees.cpp,
// landscape pieces (ground, road strips, cliff, guardrail, power lines, rocks, grass, sky, mist) in Environment.cpp.
#include <functional>
#include <vector>
#include <nlohmann/json.hpp>
#include "World.h"

using json = nlohmann::json;

// ---------------------------------------------------------------- JSON helpers
float radians(float degrees);
glm::vec3 vec3Of(const json& j, const char* key, glm::vec3 fallback = glm::vec3(0));
glm::vec2 vec2Of(const json& j, const char* key, glm::vec2 fallback = glm::vec2(0));
// "#rrggbb" -> linear colour (same meaning as ter::hexc(0xrrggbb))
glm::vec3 colorOf(const json& j, const char* key, glm::vec3 fallback);
// The axis-aligned box around a (possibly rotated) box, used as its collider
ter::Box boundsOf(glm::vec3 center, glm::vec3 size, glm::vec3 rotation);

struct Builder {
    World& world;
    // trees are collected first and drawn as instanced meshes, a few draw calls per tree variant
    struct TreeInstance {
        int variant;   // 0-2 pines, 3 dead tree
        glm::mat4 matrix;
        glm::vec3 base;
    };
    std::vector<TreeInstance> trees;
    ter::GeoPtr groundGeo;   // kept so finishTrees() can darken the ground under the trees

    ter::ObjPtr add(const ter::ObjPtr& o) {
        world.root->add(o);
        return o;
    }
    void collide(glm::vec3 center, glm::vec3 size) { world.colliders.push_back(ter::Box::centered(center, size)); }

    // Random points (x, z) in the rectangle "min".."max" of j, outside its "clear" rectangles.
    // Calls place(point, rng) until "count" points are placed. Deterministic for a given "seed".
    void scatter(const json& j, int defaultCount, const std::function<void(glm::vec2, ter::Rng&)>& place);

    // ---- Props.cpp
    void box(const json& j);
    void busShelter(const json& j);
    void lampPost(const json& j);
    void sign(const json& j);
    void bin(const json& j);
    void bag(const json& j);
    void shadowFigure(const json& j);
    void kmPost(const json& j);
    void roadSign(const json& j);
    void companion(const json& j);
    void grandma(const json& j);
    void spiritShrine(const json& j);
    void model(const json& j);

    // ---- World.cpp (trees)
    void tree(glm::vec3 base, float scale, float yaw, int variant, glm::vec2 tilt = glm::vec2(0));
    void forest(const json& j);
    void finishTrees();

    // ---- Environment.cpp
    void ground(const json& j);
    void strip(const json& j);
    void cliff(const json& j);
    void guardrail(const json& j);
    void delineators(const json& j);
    void powerLine(const json& j);
    void rocks(const json& j);
    void grass(const json& j);
    void mist(const json& j);
    void smoke(const json& j);
    void decals(const json& j);
    void puddles(const json& j);
    void gutter(const json& j);
    void sky(const json& j);
};
