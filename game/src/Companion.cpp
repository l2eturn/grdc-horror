#include "Companion.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include "GameClock.h"

using json = nlohmann::json;

namespace {

glm::vec3 vec3Of(const json& j, const char* key, glm::vec3 fallback) {
    auto v = j.value(key, std::vector<float>{});
    return v.size() == 3 ? glm::vec3(v[0], v[1], v[2]) : fallback;
}

float flatDistance(glm::vec3 a, glm::vec3 b) { return glm::length(glm::vec2(a.x - b.x, a.z - b.z)); }

// the shelter's raised concrete floor (sala.glb at [0, 0, 1.5]): he stands 0.18 m up on it
float groundAt(glm::vec3 p) { return p.x > -2.9f && p.x < 2.9f && p.z > -0.45f && p.z < 3.1f ? 0.18f : 0.0f; }

std::vector<std::string> strings(const json& j) {
    std::vector<std::string> out;
    for (const json& s : j)
        if (s.is_string()) out.push_back(s.get<std::string>());
    return out;
}

}  // namespace

bool Companion::load(const std::string& path, World& world) {
    std::ifstream in(path);
    json j = json::parse(in, nullptr, false, true);   // allow // comments
    if (j.is_discarded()) {
        std::fprintf(stderr, "companion: could not read %s\n", path.c_str());
        return false;
    }
    name_ = j.value("name", name_);
    const json seat = j.value("seat", json::object());
    seat_ = vec3Of(seat, "pos", seat_);
    seatYaw_ = seat.value("rotY", 180.0f) * ter::PI / 180;
    walkSpeed_ = j.value("walkSpeed", walkSpeed_);
    followDist_ = j.value("followDist", followDist_);
    closerPerStrike_ = j.value("closerPerStrike", closerPerStrike_);
    minDist_ = j.value("minDist", minDist_);
    followAfter_ = j.value("followAfter", followAfter_);
    sitAfter_ = j.value("sitAfter", sitAfter_);
    footsteps_ = j.value("footsteps", footsteps_);
    auto phases = [](const json& block) {
        std::vector<Phase> out;
        for (const json& p : block.value("phases", json::array()))
            out.push_back({parseTime(p.value("until", "05:30")), strings(p.value("scripts", json::array())), 0});
        return out;
    };
    const json chatter = j.value("chatter", json::object());
    auto every = chatter.value("every", std::vector<float>{28, 50});
    if (every.size() == 2) chatterMin_ = every[0], chatterMax_ = every[1];
    chatterRadius_ = chatter.value("radius", chatterRadius_);
    chatter_ = phases(chatter);
    const json talk = j.value("talk", json::object());
    talkRadius_ = talk.value("radius", talkRadius_);
    talk_ = phases(talk);
    const json signals = j.value("signals", json::object());   // named: items() of a temporary would dangle
    for (auto& [signal, script] : signals.items())
        if (script.is_string()) signals_[signal] = script.get<std::string>();
    rules_ = j.value("rules", json::object());
    strikeScripts_ = strings(j.value("strikes", json::array()));

    if (!world.companion) return true;   // no companion in the map: nothing to drive
    // his collider from the map builder (a box around where he was placed) moves with him from now on
    glm::vec3 placed = world.companion->position + glm::vec3(0, 0.9f, 0);
    for (size_t i = 0; i < world.colliders.size(); ++i) {
        const ter::Box& b = world.colliders[i];
        if (glm::length((b.min + b.max) * 0.5f - placed) < 0.01f) collider_ = (int)i;
    }
    world.companion->traverse([&](ter::Object3D& o) {
        if (o.name == "npc_head") head_ = &o;
    });
    // start on his seat; he turns himself from now on (World's "face the player" is off)
    world.companionFacesPlayer = false;
    world.companion->position = seat_;
    world.companion->rotation.y = seatYaw_;
    if (world.companionAnim) world.companionAnim->play("sit", true, 0);
    chatterTimer_ = 15;
    return true;
}

void Companion::setAnim(World& world, const std::string& clip) {
    if (world.companionAnim && world.companionAnim->current() != clip) world.companionAnim->play(clip);
}

std::string Companion::pick(std::vector<Phase>& phases, float clock) {
    for (Phase& p : phases) {
        if (clock >= p.until || p.scripts.empty()) continue;
        // start each phase at a random line, then go round them all before any repeats
        if (p.next == 0) p.next = 1 + (int)(ter::frand() * p.scripts.size());
        return p.scripts[p.next++ % p.scripts.size()];
    }
    return "";
}

void Companion::say(const std::string& script, float delay) {
    if (script.empty()) return;
    pending_.push_back({script, -delay});
}

void Companion::onRuleOpened(const std::string& rule) {
    if (!rules_.contains(rule)) return;
    rule_ = rule;
    following_ = false;
    say(rules_[rule].value("say", ""));
}

void Companion::onRuleClosed(const std::string& rule) {
    if (rule_ != rule) return;
    say(rules_[rule].value("after", ""));
    rule_.clear();
    following_ = true;   // back to his routine: come over to the player first
}

void Companion::onSignal(const std::string& signal) {
    auto it = signals_.find(signal);
    if (it != signals_.end()) say(it->second);
}

void Companion::onStrike() {
    ++strikes_;
    // a beat after the scare, not on top of it
    if (!strikeScripts_.empty()) say(strikeScripts_[std::min((int)strikeScripts_.size(), strikes_) - 1], 4);
    if (rule_.empty()) following_ = true;   // he comes over to you
}

bool Companion::canTalk(glm::vec3 player, const World& world) const {
    return world.companion && world.companion->visible && flatDistance(player, world.companion->position) < talkRadius_;
}

void Companion::talk(float clock, Dialogue& dialogue) {
    std::string script = pick(talk_, clock);
    if (!script.empty() && !dialogue.active()) dialogue.start(script);
    chatterTimer_ = std::max(chatterTimer_, 15.0f);
}

void Companion::update(float dt, const CompanionInputs& in, World& world, Dialogue& dialogue, ter::Audio& audio) {
    if (!world.companion || !world.companion->visible || !world.companionAnim) return;
    ter::Object3D& self = *world.companion;

    // lines waiting for the dialogue box (signals, strikes, rule windows); stale ones are dropped
    for (auto& p : pending_) p.second += dt;
    std::erase_if(pending_, [](const auto& p) { return p.second > 20; });
    if (!pending_.empty() && pending_.front().second >= 0 && !dialogue.active()) {
        dialogue.start(pending_.front().first);
        pending_.erase(pending_.begin());
        chatterTimer_ = std::max(chatterTimer_, 12.0f);
    }
    if (in.paused) {
        walking_ = false;
        return;
    }

    // ---- where he wants to be, and how
    glm::vec3 goal = self.position;
    float goalYaw = self.rotation.y;
    Pose pose = Pose::Stand;
    bool facePlayer = true;
    float dPlayer = flatDistance(in.player, self.position);
    float near = std::max(minDist_, followDist_ - closerPerStrike_ * strikes_);
    if (holdTimer_ > 0) holdTimer_ -= dt, near = minDist_;   // holding on to the player's arm
    const json order = rule_.empty() ? json() : rules_[rule_];
    std::string what = order.is_object() ? order.value("do", "") : "";
    if (what == "sit" || (what.empty() && !following_)) {
        goal = seat_;
        goalYaw = seatYaw_;
        pose = Pose::Sit;
        facePlayer = false;
        if (what.empty() && dPlayer > followAfter_) following_ = true;
    } else if (what == "crouch" || what == "stand") {
        goal = vec3Of(order, "pos", self.position);
        goalYaw = order.value("rotY", 0.0f) * ter::PI / 180;
        pose = what == "crouch" ? Pose::Crouch : Pose::Stand;
        facePlayer = !order.contains("pos");   // "stand" without a place: freeze where he is, staring at you
    } else {
        // following: keep `near` meters from the player, on his side of them
        glm::vec3 away = self.position - in.player;
        away.y = 0;
        if (glm::length(away) < 0.01f) away = {0, 0, 1};
        goal = in.player + glm::normalize(away) * near;
        // only set off when the player is clearly further than that (no shuffling on every step they take)
        if (!walking_ && dPlayer < near + (holdTimer_ > 0 ? 0.1f : 0.7f)) goal = self.position;
        // the player has been back in the shelter a while: go and sit down again
        backTimer_ = in.playerInShelter ? backTimer_ + dt : 0;
        if (backTimer_ > sitAfter_ && holdTimer_ <= 0) following_ = false;
    }

    // ---- walk there (around colliders; his own box is moved out of the way meanwhile)
    float dGoal = flatDistance(goal, self.position);
    walking_ = dGoal > (walking_ ? 0.08f : 0.25f);
    if (walking_) {
        glm::vec3 dir = glm::normalize(glm::vec3(goal.x - self.position.x, 0, goal.z - self.position.z));
        glm::vec3 to = self.position + dir * std::min(dGoal, walkSpeed_ * dt);
        // the player is in the way: wait for them rather than push them (his box moves with him)
        if (flatDistance(to, in.player) < 0.65f && flatDistance(to, in.player) < flatDistance(self.position, in.player)) to = self.position;
        if (ghostTimer_ > 0) {
            ghostTimer_ -= dt;   // stuck on a corner for a while: he slips through (nobody sees it in the fog)
        } else {
            ter::Box saved;
            if (collider_ >= 0) saved = world.colliders[collider_], world.colliders[collider_] = ter::Box{glm::vec3(-1e4f), glm::vec3(-1e4f)};
            glm::vec3 pushed = ter::resolveCircle(to, 0.3f, world.colliders);
            if (collider_ >= 0) world.colliders[collider_] = saved;
            stuckTimer_ = glm::length(pushed - self.position) < walkSpeed_ * dt * 0.2f ? stuckTimer_ + dt : 0;
            if (stuckTimer_ > 1.5f) ghostTimer_ = 1.2f, stuckTimer_ = 0;
            to = pushed;
        }
        to.y = groundAt(to);
        self.position = to;
        float yaw = std::atan2(dir.x, dir.z);
        self.rotation.y += ter::wrapAngle(yaw - self.rotation.y) * std::min(1.0f, dt * 8);
        setAnim(world, "walk");
        if (footsteps_ && (stepTimer_ -= dt) <= 0) {
            stepTimer_ = 0.45f;
            audio.play("step", {.volume = 0.6f, .pitch = 0.8f + ter::frand() * 0.2f, .spatial = true, .position = self.position, .maxDistance = 15});
        }
    } else {
        if (pose == Pose::Sit) self.position = goal;   // settle exactly onto the seat
        self.position.y = groundAt(self.position);
        float yaw = facePlayer ? std::atan2(in.player.x - self.position.x, in.player.z - self.position.z) : goalYaw;
        self.rotation.y += ter::wrapAngle(yaw - self.rotation.y) * std::min(1.0f, dt * (facePlayer ? 3.0f : 6.0f));
        bool speaking = dialogue.active() && dialogue.speaker() == name_;
        setAnim(world, pose == Pose::Sit ? (speaking ? "sit_talk" : "sit") : pose == Pose::Crouch ? "crouch" : speaking ? "talk" : "idle");
    }
    if (collider_ >= 0) world.colliders[collider_] = ter::Box::centered(self.position + glm::vec3(0, 0.9f, 0), {0.5f, 1.8f, 0.5f});

    // ---- his head follows the player when his body doesn't (sitting, standing somewhere on purpose)
    float want = 0;
    if (!walking_ && !facePlayer) {
        glm::vec3 d = in.player - self.position;
        want = std::clamp(ter::wrapAngle(std::atan2(d.x, d.z) - self.rotation.y), -1.1f, 1.1f);
        if (std::abs(ter::wrapAngle(std::atan2(d.x, d.z) - self.rotation.y)) > 2.2f) want = 0;   // behind him: don't
    }
    headYaw_ += (want - headYaw_) * std::min(1.0f, dt * 2.5f);
    if (head_) {
        // the clip sets the head every frame; if it didn't this frame, turn from the last clip pose instead
        if (head_->quaternion != headSet_) headBase_ = head_->quaternion;
        headSet_ = glm::angleAxis(headYaw_, glm::vec3(0, 1, 0)) * headBase_;
        head_->setQuaternion(headSet_);
    }

    // ---- chatter now and then, when the player is near and nothing else is being said
    if (rule_.empty() && pending_.empty() && dPlayer < chatterRadius_ && !dialogue.active() && (chatterTimer_ -= dt) <= 0) {
        chatterTimer_ = chatterMin_ + ter::frand() * (chatterMax_ - chatterMin_);
        std::string script = pick(chatter_, in.clock);
        if (!script.empty()) dialogue.start(script);
    }
}
