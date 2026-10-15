#include "Hiding.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

bool Hiding::load(const std::string& mapPath) {
    std::ifstream in(mapPath);
    json j = json::parse(in, nullptr, false, true);
    if (j.is_discarded()) {
        std::fprintf(stderr, "hiding: could not read %s\n", mapPath.c_str());
        return false;
    }
    for (const json& e : j.value("hideSpots", json::array())) {
        HideSpot s;
        s.id = e.value("id", "");
        auto p = e.value("eye", std::vector<float>{0, 0.5f, 0});
        if (p.size() == 3) s.eye = {p[0], p[1], p[2]};
        s.yaw = e.value("yawDeg", 0.0f) * ter::PI / 180;
        s.radius = e.value("radius", s.radius);
        s.look = e.value("lookDeg", 40.0f) * ter::PI / 180;
        s.sit = e.value("kind", std::string("hide")) == "sit";
        spots_.push_back(s);
    }
    return true;
}

const HideSpot* Hiding::near(glm::vec3 playerPos) const {
    const HideSpot* best = nullptr;
    float bestD = 1e9f;
    for (const HideSpot& s : spots_) {
        float d = glm::length(glm::vec2(playerPos.x - s.eye.x, playerPos.z - s.eye.z));
        if (d < s.radius && d < bestD) {
            best = &s;
            bestD = d;
        }
    }
    return best;
}

void Hiding::enter(const HideSpot& spot, ter::FirstPersonCamera& player) {
    if (from_) return;   // still climbing out
    spot_ = from_ = &spot;
    stand_ = player.position;
    // can't walk, and nothing pushes the eye out of the furniture it is tucked under
    walk_ = player.walkSpeed;
    run_ = player.runSpeed;
    collide_ = player.collide;
    player.walkSpeed = player.runSpeed = 0;
    player.collide = nullptr;
}

void Hiding::leave() { spot_ = nullptr; }

void Hiding::update(float dt, ter::FirstPersonCamera& player) {
    if (!from_) return;
    k_ = std::clamp(k_ + (spot_ ? dt : -dt) / 0.45f, 0.0f, 1.0f);
    float s = glm::smoothstep(0.0f, 1.0f, k_);
    player.position = glm::mix(stand_, from_->eye, s);
    if (spot_) {
        // turn to face out of the spot while getting in, then stay within its range
        float d = ter::wrapAngle(player.yaw - from_->yaw);
        // sitting down: turn round to face the way the seat faces (you walk up to a bench facing it)
        if (from_->sit && k_ < 1) player.yaw = from_->yaw + (d *= 1 - std::min(1.0f, dt * 8));
        float limit = glm::mix(ter::PI, from_->look, s);
        if (std::abs(d) > limit) player.yaw = from_->yaw + std::copysign(limit, d);
        player.pitch = std::clamp(player.pitch, -0.6f, 0.5f);
    } else if (k_ <= 0) {
        // out again: walking and collisions back
        player.position = stand_;
        player.walkSpeed = walk_;
        player.runSpeed = run_;
        player.collide = collide_;
        from_ = nullptr;
    }
}
