#pragma once
// Hiding places (map.json "hideSpots"): walk up to one and press E to crouch into it — the view drops to the
// spot's eye, the player can't walk, only look around a little (± `look` from the spot's direction).
// E again climbs back out to where they were standing. Rules 5 and 10 and the breath mini-game use it.
// A spot with "kind": "sit" is a seat instead (rule 9: up on the bench): same controls, the view doesn't darken.
#include <string>
#include <vector>
#include <ter/ter.h>

struct HideSpot {
    std::string id;
    glm::vec3 eye{0};      // camera position while hidden
    float yaw = 0;         // radians, the way the spot faces (FirstPersonCamera convention)
    float radius = 1.3f;   // how close (x/z, to the eye) the player has to be to get in
    float look = 0.7f;     // radians the view may turn away from `yaw`
    bool sit = false;      // a seat, not a hiding place
};

class Hiding {
public:
    bool load(const std::string& mapPath);

    const HideSpot* find(const std::string& id) const {
        for (auto& s : spots_)
            if (s.id == id) return &s;
        return nullptr;
    }
    // the spot the player could get into from here (nullptr = none)
    const HideSpot* near(glm::vec3 playerPos) const;
    void enter(const HideSpot& spot, ter::FirstPersonCamera& player);
    void leave();
    bool hidden() const { return spot_ != nullptr; }
    const HideSpot* spot() const { return spot_; }
    // the spot the player is in, getting into or still climbing out of (nullptr = standing)
    const HideSpot* current() const { return from_; }
    // 0..1 how far into the crouch (the view darkens with it)
    float amount() const { return k_; }

    // every frame after player.update: moves the player's eye in/out of the spot, keeps the view in range
    void update(float dt, ter::FirstPersonCamera& player);

private:
    std::vector<HideSpot> spots_;
    const HideSpot* spot_ = nullptr;   // hidden in (or climbing out of, while k_ > 0)
    const HideSpot* from_ = nullptr;
    glm::vec3 stand_{0};               // where the player stood before hiding
    float k_ = 0;
    std::function<glm::vec3(glm::vec3, glm::vec3)> collide_;
    float walk_ = 0, run_ = 0;
};
