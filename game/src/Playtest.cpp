#include "Playtest.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {

bool keyOf(const std::string& name, ter::Key& out) {
    if (name.size() == 1 && name[0] >= 'A' && name[0] <= 'Z') return out = (ter::Key)name[0], true;
    if (name.size() == 1 && name[0] >= '0' && name[0] <= '9') return out = (ter::Key)name[0], true;
    static const std::pair<const char*, ter::Key> named[] = {
        {"Space", ter::Key::Space}, {"Tab", ter::Key::Tab}, {"Up", ter::Key::Up}, {"Down", ter::Key::Down},
        {"Left", ter::Key::Left}, {"Right", ter::Key::Right}, {"Grave", ter::Key::GraveAccent},
        {"Shift", ter::Key::LeftShift}, {"Enter", ter::Key::Enter}};
    for (auto& [n, k] : named)
        if (name == n) return out = k, true;
    return false;
}

float num(const std::vector<std::string>& args, size_t i, float fallback = 0) {
    return i < args.size() ? (float)std::atof(args[i].c_str()) : fallback;
}

}  // namespace

bool Playtest::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "playtest: could not read %s\n", path.c_str());
        return false;
    }
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        ++n;
        if (auto hash = line.find('#'); hash != std::string::npos) line.erase(hash);
        std::istringstream words(line);
        std::string when;
        if (!(words >> when) || (when[0] != '+' && when[0] != '@')) continue;
        PlayCommand c;
        c.relative = when[0] == '+';
        c.at = (float)std::atof(when.c_str() + 1);
        c.line = n;
        if (!(words >> c.name)) continue;
        for (std::string a; words >> a;) c.args.push_back(a);
        commands_.push_back(c);
    }
    loaded_ = true;
    std::printf("playtest: %zu commands from %s\n", commands_.size(), path.c_str());
    return true;
}

std::vector<PlayCommand> Playtest::update(float dt, ter::Input& in, ter::FirstPersonCamera& player,
                                          const std::function<bool(const std::string&)>& condition) {
    std::vector<PlayCommand> out;
    if (!loaded_) return out;
    time_ += dt;

    for (auto it = holds_.begin(); it != holds_.end();) {
        if (time_ >= it->second) {
            in.simulate(it->first, false);
            it = holds_.erase(it);
        } else {
            ++it;
        }
    }

    // walking: steer toward the point, W held until close (or stuck)
    if (walking_) {
        glm::vec2 d = walkTo_ - glm::vec2(player.position.x, player.position.z);
        if (glm::length(d) < 0.35f || time_ - walkStart_ > 15) {
            if (glm::length(d) >= 0.35f)
                std::printf("[play %.1f] walkto stuck at (%.2f, %.2f), %.2f m short\n", time_, player.position.x, player.position.z, glm::length(d));
            in.simulate(ter::Key::W, false);
            walking_ = false;
            last_ = time_;
        } else {
            player.yaw = std::atan2(-d.x, -d.y);   // forward = (-sin yaw, -cos yaw)
            return out;                              // the next command waits for the walk
        }
    }

    // waiting for something to happen in the game
    if (!waitFor_.empty()) {
        if (condition(waitFor_)) {
            std::printf("[play %.1f] waited for %s\n", time_, waitFor_.c_str());
            waitFor_.clear();
            last_ = time_;
        } else if (time_ - waitStart_ > waitMax_) {
            std::printf("[play %.1f] gave up waiting for %s\n", time_, waitFor_.c_str());
            waitFor_.clear();
            last_ = time_;
        } else {
            return out;
        }
    }

    while (next_ < commands_.size()) {
        const PlayCommand& c = commands_[next_];
        float due = c.relative ? last_ + c.at : c.at;
        if (time_ < due) break;
        ++next_;
        last_ = time_;
        ter::Key k;
        if (c.name == "key" && !c.args.empty() && keyOf(c.args[0], k)) {
            in.simulate(k, true);
            holds_.push_back({k, time_ + 0.05f});
        } else if (c.name == "hold" && !c.args.empty() && keyOf(c.args[0], k)) {
            in.simulate(k, true);
            holds_.push_back({k, time_ + num(c.args, 1, 1)});
        } else if (c.name == "look") {
            player.yaw = num(c.args, 0) * ter::PI / 180;
            player.pitch = num(c.args, 1) * ter::PI / 180;
        } else if (c.name == "lookat") {
            glm::vec3 d = glm::vec3(num(c.args, 0), num(c.args, 1), num(c.args, 2)) - player.position;
            player.yaw = std::atan2(-d.x, -d.z);
            player.pitch = std::atan2(d.y, glm::length(glm::vec2(d.x, d.z)));
        } else if (c.name == "goto") {
            player.position.x = num(c.args, 0);
            player.position.z = num(c.args, 1);
        } else if (c.name == "waitfor" && !c.args.empty()) {
            waitFor_ = c.args[0];
            waitStart_ = time_;
            waitMax_ = num(c.args, 1, 150);
            break;
        } else if (c.name == "walkto") {
            walkTo_ = {num(c.args, 0), num(c.args, 1)};
            walking_ = true;
            walkStart_ = time_;
            in.simulate(ter::Key::W, true);
            break;
        } else if (c.name == "key" || c.name == "hold") {
            std::printf("[play] line %d: unknown key '%s'\n", c.line, c.args.empty() ? "" : c.args[0].c_str());
        } else {
            out.push_back(c);
        }
    }
    return out;
}
