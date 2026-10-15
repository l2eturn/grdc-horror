#pragma once
// Plays one named glTF animation clip at a time on a loaded model, with a short cross-fade between clips.
// (ter::AnimationMixer plays every clip at once; characters need "play walk", "play sit", ...)
//
//   auto model = ter::loadGltf(path);
//   Animator anim(model->animations);
//   anim.play("walk");            // loops
//   anim.play("board_bus", false); // plays once, then holds the last frame; anim.finished() becomes true
//   anim.update(dt);              // every frame
#include <map>
#include <string>
#include <vector>
#include <ter/ter.h>

class Animator {
public:
    Animator() = default;
    explicit Animator(std::vector<ter::GltfAnimation> clips);

    // Start a clip by name. fade = seconds to blend from the current pose. Returns false if there is no such clip.
    bool play(const std::string& name, bool loop = true, float fade = 0.3f);
    void update(float dt);

    bool has(const std::string& name) const;
    const std::string& current() const { return current_.name; }
    // a clip played with loop = false has reached its end
    bool finished() const;
    float duration(const std::string& name) const;

private:
    struct Pose {
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
    };
    struct Playing {
        std::string name;
        int clip = -1;
        float time = 0;
        bool loop = true;
    };
    void sample(const Playing& p, std::map<ter::Object3D*, Pose>& out) const;

    std::vector<ter::GltfAnimation> clips_;
    Playing current_, previous_;
    float fade_ = 0, fadeTime_ = 0;
};
