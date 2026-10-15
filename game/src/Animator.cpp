#include "Animator.h"

#include <algorithm>
#include <cmath>

Animator::Animator(std::vector<ter::GltfAnimation> clips) : clips_(std::move(clips)) {}

bool Animator::has(const std::string& name) const {
    return std::any_of(clips_.begin(), clips_.end(), [&](const ter::GltfAnimation& c) { return c.name == name; });
}

float Animator::duration(const std::string& name) const {
    for (const auto& c : clips_)
        if (c.name == name) return c.duration;
    return 0;
}

bool Animator::play(const std::string& name, bool loop, float fade) {
    for (size_t i = 0; i < clips_.size(); ++i) {
        if (clips_[i].name != name) continue;
        if (current_.clip == (int)i && current_.loop == loop && loop) return true;   // already playing
        previous_ = current_;
        current_ = {name, (int)i, 0, loop};
        fadeTime_ = previous_.clip >= 0 ? fade : 0;
        fade_ = 0;
        return true;
    }
    std::fprintf(stderr, "animator: no clip named '%s'\n", name.c_str());
    return false;
}

bool Animator::finished() const {
    if (current_.clip < 0 || current_.loop) return false;
    return current_.time >= clips_[current_.clip].duration;
}

// Value of every channel of the clip at the clip's time (same maths as ter::AnimationMixer)
void Animator::sample(const Playing& p, std::map<ter::Object3D*, Pose>& out) const {
    const ter::GltfAnimation& clip = clips_[p.clip];
    float t = p.loop && clip.duration > 0 ? std::fmod(p.time, clip.duration) : std::min(p.time, clip.duration);
    for (const auto& ch : clip.channels) {
        size_t n = ch.times.size();
        if (!n) continue;
        size_t k = 0;
        while (k + 1 < n && ch.times[k + 1] <= t) ++k;
        auto value = [&](size_t i) { return ch.interp == 2 ? ch.values[i * 3 + 1] : ch.values[i]; };
        glm::vec4 v;
        if (k + 1 >= n || ch.interp == 1 || t <= ch.times[0]) {
            v = value(t <= ch.times[0] ? 0 : k);
        } else {
            float f = (t - ch.times[k]) / std::max(1e-6f, ch.times[k + 1] - ch.times[k]);
            glm::vec4 a = value(k), b = value(k + 1);
            if (ch.path == 1) {
                glm::quat q = glm::slerp(glm::quat(a.w, a.x, a.y, a.z), glm::quat(b.w, b.x, b.y, b.z), f);
                v = {q.x, q.y, q.z, q.w};
            } else {
                v = glm::mix(a, b, f);
            }
        }
        // start from the node's current transform so channels a clip lacks keep their value
        auto [it, fresh] = out.try_emplace(ch.node, Pose{ch.node->position, ch.node->quaternion, ch.node->scale});
        if (fresh && !ch.node->useQuaternion) it->second.rotation = glm::quat(ch.node->rotation);
        if (ch.path == 0) it->second.position = glm::vec3(v);
        else if (ch.path == 1) it->second.rotation = glm::normalize(glm::quat(v.w, v.x, v.y, v.z));
        else it->second.scale = glm::vec3(v);
    }
}

void Animator::update(float dt) {
    if (current_.clip < 0) return;
    current_.time += dt;
    std::map<ter::Object3D*, Pose> now;
    sample(current_, now);
    if (fade_ < fadeTime_ && previous_.clip >= 0) {
        fade_ += dt;
        previous_.time += dt;
        std::map<ter::Object3D*, Pose> before;
        sample(previous_, before);
        float w = glm::smoothstep(0.0f, 1.0f, fade_ / fadeTime_);
        for (auto& [node, pose] : now) {
            auto b = before.find(node);
            if (b == before.end()) continue;
            pose.position = glm::mix(b->second.position, pose.position, w);
            pose.rotation = glm::slerp(b->second.rotation, pose.rotation, w);
            pose.scale = glm::mix(b->second.scale, pose.scale, w);
        }
    }
    for (auto& [node, pose] : now) {
        node->position = pose.position;
        node->setQuaternion(pose.rotation);
        node->scale = pose.scale;
    }
}
