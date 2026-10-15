#include "Ending.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>

namespace {

// the bus drives west along the lane like the one at the stop (model +Z -> world -X, model +X -> world +Z)
constexpr float kYaw = -ter::PI / 2, kLaneZ = -2.6f, kSpeed = 4.0f, kFromX = 70, kToX = -80;
glm::vec3 toWorld(glm::vec3 local) { return {-local.z, local.y, local.x}; }

// seats in the bus model (modelResource/scripts/bus_build.py, glTF axes): a pair on the driver's side, 0.8 m behind
// the middle. The player on the aisle, the companion by the window (on the player's right).
const glm::vec3 kEye(-0.5f, 1.58f, -0.78f);
const glm::vec3 kFriendRoot(-0.95f, 0.42f, -0.8f);   // feet on the floor (0.36) + the seat is 6 cm higher than the bench his "sit" clip expects
constexpr float kRevealTurn = 1.4f;                  // seconds he takes to turn to the player
constexpr float kHeadTwist = 1.15f;                  // radians his head turns beyond his shoulders (too far for a person)

ter::Object3D* findNode(ter::Object3D& root, const std::string& name) {
    ter::Object3D* found = nullptr;
    root.traverse([&](ter::Object3D& o) {
        if (!found && o.name == name) found = &o;
    });
    return found;
}

}  // namespace

bool Ending::load(World& world) {
    auto bus = ter::loadGltf(ter::assetPath("models/bus.glb"));
    auto npc = ter::loadGltf(ter::assetPath("models/npc.glb"));
    if (!bus || !npc) {
        std::fprintf(stderr, "ending: could not load the bus or the companion model\n");
        return false;
    }
    bus_ = ter::makeGroup("ending_bus");
    bus_->rotation.y = kYaw;
    bus_->visible = false;
    bus_->add(bus->root);
    world.root->add(bus_);
    // no fog inside the cabin (the windows still look out into it)
    busAnim_ = std::make_shared<Animator>(bus->animations);
    for (const char* w : {"bus_wheel_FL", "bus_wheel_FR", "bus_wheel_RL", "bus_wheel_RR"})
        if (auto n = findNode(*bus->root, w)) wheels_.push_back(n->shared_from_this());
    // the destination sign stays dark: this bus goes nowhere anyone knows
    if (auto n = findNode(*bus->root, "bus_cabin_light")) {
        cabinLight_ = ter::makePointLight(ter::hexc(0xd8e8ff), 1.0f, 6.0f);
        n->add(cabinLight_);
    }

    // the companion, sitting
    friend_ = ter::makeGroup("ending_friend");
    friend_->position = kFriendRoot;
    friend_->add(npc->root);
    bus->root->add(friend_);
    friendAnim_ = std::make_shared<Animator>(npc->animations);
    friendAnim_->play("sit", true, 0);
    auto& names = ter::gltfMaterialNames();
    npc->root->traverse([&](ter::Object3D& o) {
        for (auto& m : o.materials)
            if (auto it = names.find(m.get()); it != names.end() && it->second == "npc_face") faceMat_ = m;
    });
    scaryFace_ = std::make_shared<ter::Texture>();
    if (!scaryFace_->load(ter::assetPath("textures/npc_face_scary.png"), {.srgb = true, .nearest = false, .flipY = false})) {
        std::fprintf(stderr, "ending: could not load textures/npc_face_scary.png\n");
        scaryFace_.reset();
    }
    // his true eyes: two small glowing dots in front of the painted ones (head-local; the face layout puts the
    // eyes at u 0.5 +- 0.165, v 0.51 -> +-3.3 cm, 9.5 cm above the neck pivot, 9.4 cm in front of it)
    if (auto head = findNode(*npc->root, "npc_head")) {
        eyes_ = ter::makeGroup("ending_eyes");
        auto glow = ter::basic(glm::vec3(1.0f, 0.95f, 0.85f) * 3.0f);
        for (float s : {-1.0f, 1.0f}) {
            auto eye = ter::makeMesh(ter::geo::sphere(0.0075f, 8, 6), glow);
            eye->position = {s * 0.033f, 0.095f, 0.095f};
            eye->scale = {1, 1, 0.4f};
            eyes_->add(eye);
        }
        eyes_->visible = false;
        head->add(eyes_);
    }
    bus_->traverse([](ter::Object3D& o) {
        for (auto& m : o.materials) m->fog = false;
    });
    return true;
}

void Ending::start(Dialogue& dialogue) {
    dialogue_ = &dialogue;
    phase_ = Phase::Ride;
    t_ = 0;
    x_ = kFromX;
    black_ = 1;
    yaw_ = 0.8f;     // looking ahead and toward him and the window
    pitch_ = -0.05f;
    turn_ = 0;
    bus_->visible = true;
    busAnim_->play("doors_close", false, 0);
    friendAnim_->play("sit", true, 0);
}

void Ending::onAction(const std::string& action, ter::Audio& audio) {
    if (action == "reveal" && phase_ == Phase::Ride) {
        phase_ = Phase::Reveal;
        t_ = 0;
        friendAnim_->play("sit_talk", true, 0.6f);
    } else if (action == "end") {
        endPending_ = true;
    }
    (void)audio;
}

void Ending::update(float dt, const ter::Input& in, ter::Camera& cam, ter::Audio& audio, bool lookFree) {
    if (phase_ == Phase::Off) return;
    t_ += dt;

    // the bus rolls on through the fog (it comes round again if the talk runs long; nobody can tell in the fog)
    float oldX = x_;
    x_ -= kSpeed * dt;
    if (x_ < kToX) x_ = kFromX;
    bus_->position = {x_, 0, kLaneZ};
    for (auto& w : wheels_) w->rotation.x += (oldX - x_) / 0.47f;
    if (!hum_ && phase_ != Phase::Cut && phase_ != Phase::Title)
        hum_ = audio.play("bus_hum", {.volume = 0.6f, .loop = true, .fadeIn = 1.0f});
    busAnim_->update(dt);
    friendAnim_->update(dt);

    // the companion turns to the player: body a little, the head much further than a neck can
    if (phase_ == Phase::Reveal) turn_ = std::min(1.0f, turn_ + dt / kRevealTurn);
    float s = glm::smoothstep(0.0f, 1.0f, turn_);
    friend_->rotation.y = s * 0.45f;
    if (auto head = findNode(*friend_, "npc_head")) head->setQuaternion(head->quaternion * glm::angleAxis(s * kHeadTwist, glm::vec3(0, 1, 0)));

    // reveal: the cabin light dies as he finishes turning, comes back cold and weak on his true face
    if (phase_ == Phase::Reveal) {
        bool dark = t_ > kRevealTurn && t_ < kRevealTurn + 0.45f;
        if (cabinLight_) cabinLight_->intensity = dark ? 0.0f : (t_ > kRevealTurn ? 0.45f + 0.1f * std::sin(t_ * 37.0f) : 1.0f);
        if (t_ > kRevealTurn + 0.3f && eyes_ && !eyes_->visible) {
            eyes_->visible = true;
            if (faceMat_ && scaryFace_) faceMat_->map = scaryFace_;
            audio.play("sting");
        }
    }

    // camera: seated, a gentle ride sway; free look until the reveal pulls the view to his face
    if (lookFree && phase_ == Phase::Ride) {
        yaw_ = std::clamp(yaw_ - in.mouseDelta().x * 0.0025f, -1.6f, 1.6f);
        pitch_ = std::clamp(pitch_ - in.mouseDelta().y * 0.0025f, -0.7f, 0.6f);
    }
    glm::vec3 sway(0, std::sin(t_ * 7.3f) * 0.004f + std::sin(t_ * 2.1f) * 0.006f, 0);
    glm::vec3 eye = bus_->position + toWorld(kEye + sway);
    // local look: yaw 0 = toward the front of the bus (+Z), positive = toward the window on the right (-X)
    glm::vec3 lookLocal(-std::sin(yaw_) * std::cos(pitch_), std::sin(pitch_), std::cos(yaw_) * std::cos(pitch_));
    glm::vec3 target = eye + toWorld(lookLocal);
    if (phase_ == Phase::Reveal) {
        glm::vec3 face = bus_->position + toWorld(kFriendRoot + glm::vec3(0.06f, 1.17f, 0.05f));
        float pull = glm::smoothstep(0.0f, 0.9f, t_);
        target = glm::mix(target, face, pull);
        // after the reveal, lean in a little: he is closer than he should be
        eye = glm::mix(eye, face, glm::smoothstep(kRevealTurn, kRevealTurn + 4.0f, t_) * 0.25f);
    }
    cam.position = eye;
    cam.lookAt(target);

    // fade in from the boarding blackout; the last line cuts to black and the title
    if (phase_ == Phase::Ride || phase_ == Phase::Reveal) black_ = std::max(0.0f, black_ - dt / 2.0f);
    if (endPending_ && (!dialogue_ || !dialogue_->active())) {
        endPending_ = false;
        phase_ = Phase::Cut;
        t_ = 0;
        black_ = 1;
        if (hum_) hum_->stop();
        hum_.reset();
        audio.play("boom");
    }
    if (phase_ == Phase::Cut && t_ > 1.2f) {
        phase_ = Phase::Title;
        t_ = 0;
    }
}

void Ending::drawTitle(ter::Canvas2D& g, float width, float height) const {
    if (!titleShown()) return;
    g.clearShadow();
    g.setFill("#000000");
    g.fillRect(0, 0, width, height);
    g.setShadow(18, ter::cssColor("rgba(120,0,0,0.6)"), 0, 0);
    g.setTextAlign(ter::TextAlign::Center);
    g.setFill("rgba(200,190,175,0.95)");
    g.setFont("Mali", 84, 700);
    g.fillText("ผีหลอก", width / 2, height / 2 - 10);
    g.setShadow(6, ter::cssColor("rgba(0,0,0,0.8)"), 0, 2);
    g.setFont("Mali", 20, 500);
    g.setFill("rgba(200,195,185,0.75)");
    g.fillText("ขอบคุณที่เล่น", width / 2, height / 2 + 50);
    g.setFont("Mali", 16, 500);
    g.setFill("rgba(200,195,185,0.5)");
    g.fillText("Esc ออกจากเกม", width / 2, height - 40);
}
