#include "Phone.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include "MeshKit.h"

namespace {

const float kStepLength = 0.65f;   // meters per footstep; one walk cycle = two steps
const float kBeamIntensity = 3.0f;
const float kRaiseTime = 0.35f;    // seconds to take the phone out or put it away
const float kReadTime = 0.25f;     // seconds to bring it up to the face / back
const glm::vec3 kLedOn = ter::hexc(0xfff6e4);
const glm::vec3 kScreenOff(0.003f, 0.004f, 0.006f);   // dark screen: almost black glass (linear colour)

glm::vec3 lookDir(float yaw, float pitch) {
    return {-std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
}

// The phone is so close to the camera that the scene's lights barely reach it; a faint emissive term
// keeps its silhouette readable in the dark (like a hand catching spill light from the flash).
ter::MatPtr bodyMat(const std::string& tex, glm::vec3 tint, glm::vec2 repeat, float glow) {
    auto m = kitMat(tex, tint, repeat);
    m->emissive = tint * glow;
    return m;
}

}  // namespace

Phone::Phone(ter::Object3D& scene) {
    // model in its own space (meters): 5.9 x 12.4 x 0.8 cm, long side along Y,
    // screen facing +Z (toward the player), back with the camera and flash facing -Z (forward)
    auto gltf = ter::loadGltf(ter::assetPath("models/phone.glb"));
    if (!gltf) {
        std::fprintf(stderr, "phone: could not load models/phone.glb\n");
        return;
    }
    model_ = ter::makeGroup("phone");
    model_->add(gltf->root);
    gltf->root->traverse([&](ter::Object3D& o) {
        if (o.name == "phone_flash_light") flashPoint_ = &o;
        // the glass sits only 0.06 mm above the white front panel, too close for the depth buffer: lift it a little
        if (o.name == "phone_screen") o.position.z += 0.0003f;
        for (auto& m : o.materials) {
            m->emissive = m->color * 0.012f;
            if (o.name == "phone_flash") led_ = m;
            if (o.name == "phone_screen") {
                // a screen makes its own light: unlit, so the glossy glass doesn't pick up a grey sheen
                m->type = ter::MaterialType::Basic;
                m->color = kScreenOff;
                // canvas textures come out upside down on glTF UVs (v = 0 at the top): flip v
                m->mapRepeat = {1, -1};
                m->mapOffset = {0, 1};
                screenMat_ = m;
            }
        }
    });
    buildHand();
    // it's right in front of the eyes: no fog on it (the volumetric fog would wash it out grey)
    model_->traverse([](ter::Object3D& o) {
        for (auto& m : o.materials) m->fog = false;
    });
    scene.add(model_);

    // the flash: a wide soft cone (an LED, not a focused torch)
    light_ = ter::makeSpotLight(ter::hexc(0xf6f2ea), kBeamIntensity, 14, 0.5f, 0.65f);
    scene.add(light_);
    // the screen's own glow: a faint cold light on the hand and whatever is just in front of the player
    glow_ = ter::makePointLight(ter::hexc(0xaabbe0), 0, 1.6f);
    scene.add(glow_);
    model_->visible = false;
}

// The player's right hand and forearm, in the phone's own space (phone: x ±0.029, y ±0.062, z ±0.004).
// The phone rests upright in the palm: palm and fingers behind it (the fingers wrap around the left edge,
// tips showing on the front), thumb up the right edge resting on the bezel, flash at the top left clear.
// The forearm (jacket sleeve) runs down and back toward the body, off-screen.
void Phone::buildHand() {
    auto skin = bodyMat("skin", glm::vec3(1), {1, 1}, 0.02f);
    auto sleeve = bodyMat("cloth", ter::hexc(0x5a5f4a), {2, 3}, 0.012f);
    auto cuff = bodyMat("cloth", ter::hexc(0x3a3d30), {2, 1}, 0.01f);
    auto add = [&](const ter::GeoPtr& g, const ter::MatPtr& m) { model_->add(ter::makeMesh(g, m)); };

    // fingers, index (top) to little finger: knuckle behind the right half -> across the back -> around the left edge
    const float fingerY[4] = {0.006f, -0.014f, -0.033f, -0.05f};
    const float fingerR[4] = {0.0085f, 0.0088f, 0.0083f, 0.0072f};
    const float reach[4] = {1.0f, 1.0f, 0.96f, 0.86f};   // the little finger doesn't get as far round
    for (int f = 0; f < 4; ++f) {
        float r = fingerR[f], y = fingerY[f], back = -0.0038f - r * 0.95f;
        glm::vec3 pts[4] = {
            {0.022f, y + 0.002f, back - 0.002f},
            {-0.012f, y, back},
            {-0.0293f - r * 0.8f, y - 0.002f, back * 0.45f},
            {-0.0293f - r * 0.6f + (1 - reach[f]) * 0.003f, y - 0.004f, 0.002f * reach[f]},
        };
        for (int k = 0; k < 3; ++k) add(tubeGeo(pts[k], pts[k + 1], r * (1 - 0.04f * k), r * (0.96f - 0.04f * k), 6), skin);
        model_->add(meshAt(ter::geo::sphere(r * 0.86f, 6, 4), skin, pts[3]));    // fingertip
        model_->add(meshAt(ter::geo::sphere(r * 1.08f, 6, 4), skin, pts[0]));    // knuckle
    }
    // palm: a rounded pad behind the lower right of the phone, its edge showing past the right side
    auto palm = ter::geo::sphere(1, 12, 8);
    palm->scale(0.03f, 0.042f, 0.013f);
    model_->add(meshAt(palm, skin, {0.026f, -0.03f, -0.018f}, {0, 0, -0.25f}));
    // thumb: from the heel of the hand, around the right edge, resting on the bezel beside the screen
    glm::vec3 t0(0.044f, -0.062f, -0.012f), t1(0.038f, -0.044f, 0.002f), t2(0.033f, -0.024f, 0.006f);
    add(tubeGeo(t0, t1, 0.012f, 0.0105f, 6), skin);
    add(tubeGeo(t1, t2, 0.0105f, 0.009f, 6), skin);
    model_->add(meshAt(ter::geo::sphere(0.009f, 6, 4), skin, t2));
    // wrist and forearm, then the jacket sleeve with a darker cuff
    glm::vec3 wrist(0.045f, -0.088f, -0.01f), elbow(0.13f, -0.24f, 0.4f);
    add(tubeGeo({0.034f, -0.06f, -0.016f}, wrist, 0.025f, 0.022f, 8), skin);
    glm::vec3 cuffStart = glm::mix(wrist, elbow, 0.08f), cuffEnd = glm::mix(wrist, elbow, 0.16f);
    add(tubeGeo(wrist, cuffStart, 0.022f, 0.025f, 8), skin);
    add(tubeGeo(cuffStart, cuffEnd, 0.04f, 0.042f, 10), cuff);
    add(tubeGeo(cuffEnd, elbow, 0.043f, 0.058f, 10), sleeve);
}

void Phone::toggleHeld() {
    held_ = !held_;
    if (!held_) reading_ = false;
}

bool Phone::setReading(bool on) {
    if (on && (!held_ || dead())) return false;
    if (on && !reading_) scroll_ = 0;   // open at the newest message
    reading_ = on;
    return true;
}

void Phone::scroll(float pixels) {
    scroll_ = glm::clamp(scroll_ + pixels, 0.0f, screen_.maxScroll());
}

void Phone::update(float dt, const ter::FirstPersonCamera& player, float walked, const std::vector<ter::Box>& colliders,
                   const GameClock& clock, const Inbox& inbox) {
    time_ += dt;
    buzz_ = std::max(0.0f, buzz_ - dt);

    // battery
    float drain = battery.idlePerMinute + (screenOn() ? battery.screenPerMinute : 0) + (lightOn() ? battery.flashPerMinute : 0);
    battery.level = std::max(0.0f, battery.level - drain * dt / 60.0f);
    if (dead()) reading_ = false;

    // screen: repaint when the time / battery / messages change; black when the battery is empty
    if (screenMat_) {
        if (screenOn()) {
            ScreenInfo info;
            info.page = reading_ ? ScreenInfo::Page::Thread : ScreenInfo::Page::Lock;
            info.time = clock.text();
            info.date = date;
            info.carrier = carrier;
            info.sender = sender;
            info.battery = (int)std::ceil(battery.level);
            info.scroll = scroll_;
            screen_.update(info, inbox);
            screenMat_->map = screen_.texture();
            screenMat_->color = glm::vec3(1);
        } else {
            screenMat_->map = nullptr;
            screenMat_->color = kScreenOff;
        }
    }
    if (!started_) {
        swayYaw_ = player.yaw;
        swayPitch_ = player.pitch;
        started_ = true;
    }
    // the hand follows the view with a short delay
    float follow = 1 - std::exp(-dt * 14);
    swayYaw_ += ter::wrapAngle(player.yaw - swayYaw_) * follow;
    swayPitch_ += (player.pitch - swayPitch_) * follow;
    float lagYaw = ter::wrapAngle(player.yaw - swayYaw_), lagPitch = player.pitch - swayPitch_;

    // walk cycle; `moving_` eases in and out so the bob doesn't snap on and off
    phase_ += walked / (kStepLength * 2) * ter::PI * 2;
    float speed = dt > 0 ? walked / dt : 0;
    moving_ += (glm::clamp(speed / 2.2f, 0.0f, 1.6f) - moving_) * (1 - std::exp(-dt * 8));

    glm::vec3 fwd = player.forward(), right = player.right();
    glm::vec3 up = glm::normalize(glm::cross(right, fwd));

    // close to a wall: pull the phone back toward the body
    float wallDist = 1;
    float wantPull = ter::raycast(player.position, fwd, colliders, 1.0f, &wallDist) >= 0 ? glm::clamp(0.7f - wallDist, 0.0f, 0.2f) : 0;
    pull_ += (wantPull - pull_) * (1 - std::exp(-dt * 10));

    // taking out / putting away: slides up from below the view, tipped forward on the way
    raise_ = glm::clamp(raise_ + (held_ ? dt : -dt) / kRaiseTime, 0.0f, 1.0f);
    float down = 1 - glm::smoothstep(0.0f, 1.0f, raise_);
    read_ = glm::clamp(read_ + (reading_ ? dt : -dt) / kReadTime, 0.0f, 1.0f);
    float close = glm::smoothstep(0.0f, 1.0f, read_);

    float bobSide = std::sin(phase_) * 0.01f * moving_;
    float bobUp = -std::abs(std::cos(phase_)) * 0.012f * moving_ + std::sin(time_ * 1.7f) * 0.002f;   // walk + breathing
    glm::vec3 pos = player.position + right * (0.15f + bobSide + lagYaw * 0.2f) + up * (-0.15f + bobUp - lagPitch * 0.2f - down * 0.32f) +
                    fwd * (0.34f - pull_ - down * 0.08f);

    // aim between where the view looks (25 m ahead) and where the lagging hand points
    glm::vec3 toTarget = glm::normalize(player.position + fwd * 25.0f - pos);
    glm::vec3 dir = glm::normalize(glm::mix(toTarget, lookDir(swayYaw_, swayPitch_), 0.5f));
    // the back (-Z, flash side) faces along dir, screen toward the player; tipped forward while lowered
    glm::quat q = glm::quatLookAt(dir, up) * glm::angleAxis(-down * 0.9f, glm::vec3(1, 0, 0));

    // reading: up in front of the face, screen square to the eyes (still swaying and bobbing a little)
    glm::vec3 readPos = player.position + fwd * 0.112f + right * (0.004f + bobSide * 0.3f + lagYaw * 0.06f) +
                        up * (-0.008f + bobUp * 0.3f - lagPitch * 0.06f);
    glm::quat readQ = glm::quatLookAt(glm::normalize(readPos - player.position), up);
    pos = glm::mix(pos, readPos, close);
    q = glm::slerp(q, readQ, close);
    // vibration: a fast small shake
    if (buzz_ > 0) pos += (right * std::sin(time_ * 170.0f) + up * std::sin(time_ * 133.0f)) * 0.0012f * std::min(1.0f, buzz_ * 4);
    if (model_) {
        model_->visible = raise_ > 0;
        model_->position = pos;
        model_->setQuaternion(q);
        glm::vec3 flash = pos + q * (flashPoint_ ? flashPoint_->position : glm::vec3(0));
        light_->position = flash + dir * 0.01f;
        light_->lookAt(flash + dir * 5.0f);
        // the beam comes up with the phone (no light from inside the pocket)
        light_->intensity = lightOn() ? kBeamIntensity * glm::smoothstep(0.5f, 1.0f, raise_) : 0;
        if (led_) led_->emissive = lightOn() ? kLedOn : led_->color * 0.012f;
        // screen glow, a little in front of the glass
        glow_->position = pos + q * glm::vec3(0, 0, 0.06f);
        glow_->intensity = screenOn() ? (0.12f + 0.1f * close) * raise_ : 0;
    }

    // camera head-bob: down at each footfall, a little side sway
    bob_ = up * (std::abs(std::sin(phase_)) - 0.5f) * 0.035f * moving_ + right * std::sin(phase_) * 0.01f * moving_;
}

void Phone::stow() {
    held_ = reading_ = false;
    raise_ = read_ = 0;
    if (!model_) return;
    model_->visible = false;
    light_->intensity = 0;
    glow_->intensity = 0;
}
