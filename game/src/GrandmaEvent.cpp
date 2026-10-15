#include "GrandmaEvent.h"
#include <algorithm>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include "MeshKit.h"

using json = nlohmann::json;

namespace {

ter::Object3D* findNode(ter::Object3D& root, const std::string& name) {
    ter::Object3D* found = nullptr;
    root.traverse([&](ter::Object3D& o) {
        if (!found && o.name == name) found = &o;
    });
    return found;
}

ter::MatPtr findMaterial(ter::Object3D& root, const std::string& name) {
    ter::MatPtr found;
    auto& names = ter::gltfMaterialNames();
    root.traverse([&](ter::Object3D& o) {
        for (auto& m : o.materials) {
            auto it = names.find(m.get());
            if (!found && it != names.end() && it->second == name) found = m;
        }
    });
    return found;
}

glm::vec3 vec3Of(const json& j, const char* key, glm::vec3 fallback) {
    if (!j.contains(key) || !j[key].is_array() || j[key].size() < 3) return fallback;
    return {j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>()};
}

// a world rotation whose +Z points along dir (models face +Z)
glm::quat facing(glm::vec3 dir) {
    return glm::quatLookAt(-glm::normalize(dir), glm::vec3(0, 1, 0));
}

float smooth01(float e0, float e1, float x) {
    float t = glm::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}

constexpr int kSegments = 28;
constexpr float kNeckRadius = 0.040f;
constexpr float kCartBehind = 0.40f;   // her root is this far behind the cart's origin (grandma_anims.py)

}  // namespace

bool GrandmaEvent::load(const std::string& path, World& world) {
    std::ifstream in(path);
    json j = json::parse(in, nullptr, false, true);
    if (j.is_discarded()) {
        std::fprintf(stderr, "could not read %s\n", path.c_str());
        return false;
    }
    appearAt = parseTime(j.value("appear", "01:10"), appearAt);
    faceSetting = j.value("face", faceSetting);
    pos = vec3Of(j, "pos", pos);
    yaw = glm::radians(j.value("rotY", glm::degrees(yaw)));
    helpRadius = j.value("helpRadius", helpRadius);
    callEvery = j.value("callEvery", callEvery);
    stretchAfter = j.value("stretchAfter", stretchAfter);
    giveUpAfter = j.value("giveUpAfter", giveUpAfter);
    const json neck = j.value("neck", json::object());
    neckSpeed = neck.value("speed", neckSpeed);
    stareTime = neck.value("stare", stareTime);
    stopShort = neck.value("stopShort", stopShort);
    maxReach = neck.value("maxReach", maxReach);
    const json forest = j.value("forest", json::object());
    for (const json& p : forest.value("path", json::array()))
        if (p.is_array() && p.size() >= 2) path_.push_back({p[0].get<float>(), p[1].get<float>()});
    escortSpeed = forest.value("speed", escortSpeed);
    fogVisibility = forest.value("fogVisibility", fogVisibility);
    fogHeight = forest.value("fogHeight", fogHeight);
    headSpeed = forest.value("headSpeed", headSpeed);
    shrineStare = forest.value("stare", shrineStare);
    chaseSpeed = forest.value("chaseSpeed", chaseSpeed);
    catchRadius = forest.value("catchRadius", catchRadius);
    safeMargin = forest.value("safeMargin", safeMargin);
    if (path_.empty()) path_ = {{pos.x, pos.z}, {pos.x - 3, pos.z}};
    for (const Zone& z : world.zones)
        if (z.id == forest.value("safeZone", std::string("shelter"))) safe_ = z.box;
    safeCenter_ = (safe_.min + safe_.max) * 0.5f;
    shrineDoor_ = world.shrineDoor;
    hasShrine_ = world.hasShrine;

    auto model = ter::loadGltf(ter::assetPath("models/grandma.glb"));
    auto cart = ter::loadGltf(ter::assetPath("models/cart.glb"));
    if (!model || !cart) {
        std::fprintf(stderr, "grandma: could not load grandma.glb / cart.glb\n");
        return false;
    }
    // one group at her feet: the cart sits kCartBehind in front of her
    group_ = ter::makeGroup("grandma_event");
    group_->position = pos;
    group_->rotation.y = yaw;
    group_->visible = false;
    world.root->add(group_);
    g_.root = ter::makeGroup("grandma");
    g_.root->add(model->root);
    group_->add(g_.root);
    cart_ = ter::makeGroup("cart");
    cart_->position = {0, 0, kCartBehind};
    cart_->add(cart->root);
    group_->add(cart_);

    g_.faceNormal = findNode(*model->root, "grandma_face_normal");
    g_.faceScary = findNode(*model->root, "grandma_face_scary");
    g_.anim = std::make_shared<Animator>(model->animations);
    g_.facesPlayer = false;
    cartAnim_ = std::make_shared<Animator>(cart->animations);
    neck_ = findNode(*model->root, "grandma_neck");
    neckSkin_ = findNode(*model->root, "grandma_neck_skin");
    head_ = findNode(*model->root, "grandma_head");
    if (head_) headRest_ = head_->position;

    // the head leaves her body for the shrine: it is moved under this (world-space) holder
    headHolder_ = ter::makeGroup("grandma_head_holder");
    world.root->add(headHolder_);

    // the long neck: a chain of tube segments in world space, bent along a curve every frame
    neckGroup_ = ter::makeGroup("grandma_long_neck");
    neckGroup_->visible = false;
    world.root->add(neckGroup_);
    // her skin, without the glTF's vertex colours (the tube has none; they would read as black), a little darker (AO)
    auto skin = findMaterial(*model->root, "grandma_skin");
    if (skin) {
        skin = skin->clone();
        skin->vertexColors = false;
        skin->color = glm::vec3(0.8f);
    }
    auto geo = tubeGeo({0, 0, 0}, {0, 0, 1}, kNeckRadius, kNeckRadius, 7);
    for (int i = 0; i < kSegments; ++i) {
        auto s = ter::makeMesh(geo, skin);
        neckGroup_->add(s);
        segments_.push_back(s);
    }
    return true;
}

void GrandmaEvent::callNow(const std::string& face) {
    if (state_ != State::Hidden || done_) return;
    if (!face.empty()) faceSetting = face;
    begin();
}

void GrandmaEvent::begin() {
    bool scary = faceSetting == "scary" || (faceSetting == "random" && ter::frand() < 0.5f);
    g_.setScary(scary);
    group_->position = pos;
    group_->rotation.y = yaw;
    group_->visible = true;
    g_.root->position = glm::vec3(0);
    cart_->position = {0, 0, kCartBehind};
    g_.anim->play("push", true, 0);
    cartAnim_->play("stuck", true, 0);
    callTimer_ = 2;
    neckYaw_ = 0;
    setState(State::Struggling);
}

void GrandmaEvent::hide() {
    group_->visible = false;
    neckGroup_->visible = false;
    if (neckSkin_) neckSkin_->visible = true;
    if (head_) head_->position = headRest_;
    if (creak_) creak_->stop();
    creak_.reset();
    done_ = true;
    setState(State::Hidden);
}

void GrandmaEvent::setState(State s) {
    if (std::getenv("GRANDMA_LOG")) std::fprintf(stderr, "grandma: state %d at %.1f s\n", (int)s, time_);
    state_ = s;
    t_ = 0;
}

void GrandmaEvent::toggleFace() { g_.setScary(!g_.scary); }

void GrandmaEvent::say(const std::string& script, Dialogue& dialogue) {
    if (!dialogue.active()) dialogue.start(script);
}

bool GrandmaEvent::canHelp(glm::vec3 p) const {
    if (state_ != State::Struggling) return false;
    glm::vec3 d = p - group_->position;
    return glm::length(glm::vec2(d.x, d.z)) < helpRadius;
}

bool GrandmaEvent::cutscene() const {
    // the chase is not: the player runs for it themselves
    return state_ == State::Escort || state_ == State::Arrived || state_ == State::Vanish || state_ == State::Shrine ||
           state_ == State::HeadOut || state_ == State::Lost;
}

glm::vec3 GrandmaEvent::playerSpot(glm::vec3 current, float dt) const {
    glm::vec3 to = glm::mix(current, spot_, std::min(1.0f, dt * 3.0f));
    to.y = current.y;
    return to;
}

bool GrandmaEvent::steer(float& yaw, float dt) const {
    if (!steering_) return false;
    glm::vec3 d = steerAt_ - spot_;
    float want = std::atan2(-d.x, -d.z);   // the player's forward is (-sin yaw, 0, -cos yaw)
    yaw += ter::wrapAngle(want - yaw) * std::min(1.0f, dt * 2.0f);
    return true;
}

void GrandmaEvent::help(Dialogue& dialogue, ter::Audio& audio) {
    if (state_ != State::Struggling) return;
    if (!dialogue.asking()) dialogue.stop();   // cut her own call short, never someone's open question
    cartAnim_->play("free", false, 0.1f);
    audio.play("cart_creak", {.volume = 0.8f, .spatial = true, .position = group_->position, .maxDistance = 20});
    if (g_.scary) {
        // the right call: the cart comes out of the mud, she thanks you and goes
        dialogue.flags["grandma"] = "helped_scary";
        g_.anim->play("push", true);
        say("grandma_thanks", dialogue);
        setState(State::Freed);
    } else {
        // the wrong call: she asks you to see her home, down the path into the forest
        dialogue.flags["grandma"] = "helped_normal";
        say("grandma_escort", dialogue);
        along_ = 0;
        stepTimer_ = 0;
        spot_ = group_->position;
        setState(State::Escort);
    }
}

void GrandmaEvent::lookAt(glm::vec3 target, float dt) {
    if (!neck_) return;
    glm::vec3 d = target - group_->position;
    float rel = ter::wrapAngle(std::atan2(d.x, d.z) - group_->rotation.y);
    rel = glm::clamp(rel, -1.4f, 1.4f);
    neckYaw_ += (rel - neckYaw_) * std::min(1.0f, dt * 2.0f);
    neck_->rotation = {0, neckYaw_, 0};
}

void GrandmaEvent::setFog(World& world, float k) {
    float a = std::min(k, 1.0f);
    float vis = glm::mix(fog0Vis_, fogVisibility, a);
    if (k > 1) vis = glm::mix(fogVisibility, 1.0f, std::min(k - 1, 1.0f));
    world.env.fogFar = vis;
    world.env.fogNear = glm::mix(fog0Height_, fogHeight, a);
}

void GrandmaEvent::update(float dt, const GameClock& clock, glm::vec3 playerEye, World& world, Dialogue& dialogue, ter::Audio& audio) {
    if (state_ == State::Hidden) {
        if (!done_ && clock.minutes() >= appearAt && !hold) begin();     // late if something else was on
        if (state_ == State::Hidden) return;
    }
    t_ += dt;
    time_ += dt;
    eye_ = playerEye;
    if (state_ != State::Lost) blackout_ = std::max(0.0f, blackout_ - dt * 0.7f);
    const glm::vec3 fwd(std::sin(group_->rotation.y), 0, std::cos(group_->rotation.y));

    switch (state_) {
        // ---------------------------------------------------------------- on the roadside
        case State::Struggling:
            // calls out now and then, looking over at you while she does
            if ((callTimer_ -= dt) <= 0) {
                callTimer_ = callEvery;
                if (glm::length(playerEye - group_->position) < 30) say("grandma_call", dialogue);
            }
            lookAt(callTimer_ > callEvery - 4 ? playerEye : group_->position + fwd, dt);
            if (!g_.scary && t_ > stretchAfter) {
                dialogue.flags["grandma"] = "ignored_normal";
                g_.anim->play("neck_stretch");
                reach_ = 0;
                target_ = glm::vec3(0);
                if (neckSkin_) neckSkin_->visible = false;
                neckGroup_->visible = true;
                creak_ = audio.play("neck_creak", {.volume = 0.9f, .loop = true, .spatial = true, .position = group_->position, .maxDistance = 40});
                setState(State::Stretching);
            } else if (g_.scary && t_ > giveUpAfter) {
                dialogue.flags["grandma"] = "ignored_scary";
                say("grandma_giveup", dialogue);
                g_.anim->play("idle");
                setState(State::GivingUp);
            }
            break;
        case State::Freed:
            lookAt(playerEye, dt);
            if (cartAnim_->finished() && t_ > 2.5f) {
                g_.anim->play("push_walk");
                setState(State::Leaving);
            }
            break;
        case State::Leaving:
            lookAt(group_->position + fwd, dt);
            group_->position += fwd * (0.5f * dt);
            if (t_ > 14) hide();
            break;
        case State::GivingUp:
            lookAt(playerEye, dt);
            if (t_ > 5) hide();
            break;

        // ---------------------------------------------------------------- ignored with an ordinary face: the long neck
        case State::Stretching:
        case State::Staring:
        case State::Retracting:
            updateRoadNeck(dt, playerEye);
            if (state_ == State::Stretching && reach_ >= 1) {
                if (creak_) creak_->stop();
                creak_.reset();
                say("grandma_neck", dialogue);
                audio.play("whisper", {.volume = 1.0f, .spatial = true, .position = tip_, .maxDistance = 10});
                setState(State::Staring);
            } else if (state_ == State::Staring && t_ > stareTime) {
                creak_ = audio.play("neck_creak", {.volume = 0.7f, .pitch = 0.8f, .loop = true, .spatial = true, .position = tip_, .maxDistance = 40});
                setState(State::Retracting);
            } else if (state_ == State::Retracting && reach_ <= 0) {
                if (creak_) creak_->stop();
                creak_.reset();
                neckGroup_->visible = false;
                if (neckSkin_) neckSkin_->visible = true;
                if (head_) head_->position = headRest_;
                g_.anim->play("push");
                setState(State::AfterNeck);
            }
            if (creak_) creak_->setPosition(tip_);
            break;
        case State::AfterNeck:
            lookAt(group_->position + fwd, dt);
            if (t_ > 8) hide();
            break;

        // ---------------------------------------------------------------- helped with an ordinary face: into the forest
        case State::Escort: {
            if (t_ < 1.5f) {   // the cart comes out of the mud first
                spot_ = group_->position + glm::vec3(fwd.z, 0, -fwd.x) * 0.8f + fwd * 0.3f;
                if (t_ < dt * 1.5f) fog0Vis_ = world.env.fogFar, fog0Height_ = world.env.fogNear;
                break;
            }
            if (g_.anim->current() != "push_walk") g_.anim->play("push_walk");
            lookAt(group_->position + fwd, dt);
            // her root follows the path; she faces a point a little further on, so the cart swings round the bends
            float total = 0;
            for (size_t i = 1; i < path_.size(); ++i) total += glm::length(path_[i] - path_[i - 1]);
            along_ = std::min(total, along_ + escortSpeed * dt);
            auto pointAt = [&](float s) {
                for (size_t i = 1; i < path_.size(); ++i) {
                    float l = glm::length(path_[i] - path_[i - 1]);
                    if (s <= l || i + 1 == path_.size()) return glm::mix(path_[i - 1], path_[i], glm::clamp(s / std::max(l, 1e-4f), 0.0f, 1.0f));
                    s -= l;
                }
                return path_.back();
            };
            glm::vec2 p = pointAt(along_), ahead = pointAt(along_ + 1.5f);
            group_->position = {p.x, 0, p.y};
            if (glm::length(ahead - p) > 0.05f) {
                float want = std::atan2(ahead.x - p.x, ahead.y - p.y);
                group_->rotation.y += ter::wrapAngle(want - group_->rotation.y) * std::min(1.0f, dt * 1.5f);
            }
            // you walk on her left, a hand on the cart
            spot_ = group_->position + glm::vec3(fwd.z, 0, -fwd.x) * 0.8f + fwd * 0.3f;
            setFog(world, smooth01(0.1f, 0.9f, along_ / total));
            if ((stepTimer_ -= dt) <= 0) {
                stepTimer_ = 0.62f;
                audio.play("step", {.volume = 0.7f, .pitch = 0.75f + ter::frand() * 0.3f});
            }
            if (along_ >= total) {
                g_.anim->play("push");
                say("grandma_arrive", dialogue);
                setState(State::Arrived);
            }
            break;
        }
        case State::Arrived:
            lookAt(playerEye, dt);
            if (t_ > 4) {
                audio.play("whisper", {.volume = 0.8f});
                setState(State::Vanish);
            }
            break;
        case State::Vanish:
            // the fog swallows everything; when it thins again she and the cart are gone, only the spirit house is left
            setFog(world, t_ < 2 ? 1 + smooth01(0, 2, t_) : 2 - smooth01(2.5f, 5.5f, t_));
            if (t_ > 2 && group_->visible) group_->visible = false;
            steering_ = hasShrine_ && t_ > 2.5f;
            steerAt_ = shrineDoor_;
            if (t_ > 6) setState(State::Shrine);
            break;
        case State::Shrine:
            if (t_ > 2.5f && hasShrine_ && head_) {
                // her head comes out of the shrine's door, slowly, on its long neck
                ter::ObjPtr head = head_->shared_from_this();
                head_->removeFromParent();
                headHolder_->add(head);
                if (neckSkin_) neckSkin_->visible = false;
                neckGroup_->visible = true;
                tip_ = shrineDoor_;
                say("grandma_shrine", dialogue);
                creak_ = audio.play("neck_creak", {.volume = 0.6f, .pitch = 0.7f, .loop = true, .spatial = true, .position = shrineDoor_, .maxDistance = 30});
                setState(State::HeadOut);
            }
            break;
        case State::HeadOut:
        case State::Chase:
        case State::Escaped: {
            // her head on its long neck: out of the shrine to the player's face, then after them up the path;
            // once they are safe it pulls back into the shrine
            // (the chase starts with the head snapping back to the shrine with a shriek: then it comes)
            const bool recoil = state_ == State::Chase && t_ < 1.2f;
            glm::vec3 to = state_ == State::HeadOut         ? playerEye + glm::normalize(shrineDoor_ - playerEye) * 0.7f
                         : state_ == State::Chase && !recoil ? playerEye
                                                             : shrineDoor_;
            glm::vec3 d = to - tip_;
            float hs = state_ == State::HeadOut         ? headSpeed
                     : state_ == State::Chase && !recoil ? glm::mix(0.8f, chaseSpeed, smooth01(1.2f, 5.0f, t_))   // slow start: time to run
                                                         : 6.0f;
            if (glm::length(d) > 0.01f) tip_ += d * std::min(1.0f, hs * dt / glm::length(d));
            steering_ = state_ == State::HeadOut;
            steerAt_ = tip_;
            if (creak_) creak_->setPosition(tip_);
            if (state_ == State::HeadOut && glm::length(d) < 0.05f && t_ > 1) {
                if (creak_) creak_->stop();
                creak_.reset();
                if (t_ > 1 + shrineStare) {
                    // she comes after you: the player has their legs back and must run for the shelter
                    audio.play("scream", {.volume = 0.6f});
                    creak_ = audio.play("neck_creak", {.volume = 0.9f, .loop = true, .spatial = true, .position = tip_, .maxDistance = 40});
                    setState(State::Chase);
                }
            } else if (state_ == State::Chase) {
                // the fog thins as the player gets back toward the shelter
                float home = glm::length(glm::vec2(shrineDoor_.x - safeCenter_.x, shrineDoor_.z - safeCenter_.z));
                float left = glm::length(glm::vec2(playerEye.x - safeCenter_.x, playerEye.z - safeCenter_.z));
                setFog(world, glm::clamp(left / std::max(home, 1.0f), 0.0f, 1.0f));
                float dist = glm::length(playerEye - tip_);
                if (!recoil && dist < catchRadius + 0.9f) blackout_ = std::max(blackout_, 1 - (dist - catchRadius) / 0.9f);
                if (safe(playerEye)) {
                    // made it: the head goes back, the fog lifts, the night goes on
                    dialogue.flags["grandma"] = "escaped";
                    audio.play("whisper", {.volume = 0.6f});
                    setState(State::Escaped);
                } else if (!recoil && dist < catchRadius) {
                    dialogue.flags["grandma"] = "caught";
                    blackout_ = 1;
                    audio.play("scream");
                    setState(State::Lost);
                }
            } else if (state_ == State::Escaped) {
                setFog(world, 1 - smooth01(0, 4, t_));
                if (glm::length(d) < 0.05f && t_ > 4) {
                    setFog(world, 0);
                    headHolder_->visible = false;
                    hide();
                }
            }
            break;
        }
        case State::Lost:
            // caught: nobody dies in this game. After a while in the dark the player comes to back at the
            // shelter (main.cpp moves them, see takeWokeUp) and the night goes on
            blackout_ = 1;
            if (t_ > 5) {
                setFog(world, 0);
                headHolder_->visible = false;
                hide();
                wokeUp_ = true;
            }
            break;
        case State::Hidden: break;
    }
    if (group_->visible) {
        g_.anim->update(dt);
        cartAnim_->update(dt);
    }
    // the neck/head are placed after the clips so they win over the head's own key
    if (state_ == State::Stretching || state_ == State::Staring || state_ == State::Retracting) updateRoadNeck(0, playerEye);
    if ((state_ == State::HeadOut || state_ == State::Chase || state_ == State::Escaped) && neckGroup_->visible) drawNeck(shrineDoor_, tip_, playerEye);
}

// the player is back at the bus stop: in the safe zone or within safeMargin of it (on the ground plane)
bool GrandmaEvent::safe(glm::vec3 p) const {
    float dx = std::max({safe_.min.x - p.x, 0.0f, p.x - safe_.max.x});
    float dz = std::max({safe_.min.z - p.z, 0.0f, p.z - safe_.max.z});
    return dx * dx + dz * dz <= safeMargin * safeMargin;
}

// the roadside long neck: reach_ 0..1 slides the head from her shoulders to just in front of the player's face
void GrandmaEvent::updateRoadNeck(float dt, glm::vec3 playerEye) {
    if (!neck_ || !head_) return;
    g_.root->updateWorldFromParents();
    neck_->updateWorldFromParents();
    const glm::vec3 base = neck_->worldPosition();
    const glm::vec3 home = neck_->localToWorld(headRest_);

    // where the face goes: just short of the player's eyes, a little above and off to one side of the line from her,
    // so the neck curls in from the side and you can see it (never further than maxReach from her)
    glm::vec3 toHer = base - playerEye;
    toHer.y = 0;
    glm::vec3 dir = glm::angleAxis(0.42f, glm::vec3(0, 1, 0)) * glm::normalize(toHer + glm::vec3(1e-4f, 0, 0));
    glm::vec3 want = playerEye + glm::normalize(dir + glm::vec3(0, 0.12f, 0)) * stopShort;
    glm::vec3 off = want - home;
    if (glm::length(off) > maxReach) want = home + glm::normalize(off) * maxReach;
    if (target_ == glm::vec3(0)) target_ = want;
    if (dt > 0) target_ = glm::mix(target_, want, std::min(1.0f, dt * 1.5f));

    if (dt > 0) {
        float dist = std::max(0.5f, glm::length(target_ - home));
        if (state_ == State::Stretching) reach_ = std::min(1.0f, reach_ + neckSpeed * dt / dist);
        if (state_ == State::Retracting) reach_ = std::max(0.0f, reach_ - neckSpeed * 1.4f * dt / dist);
    }
    // ease: slow out of her shoulders, fast across, slow arrival; the head rises in an arc on the way
    float e = reach_ * reach_ * (3 - 2 * reach_);
    glm::vec3 lift(0, 0.5f * std::sin(e * ter::PI) * std::min(1.0f, glm::length(target_ - home) / 6), 0);
    tip_ = glm::mix(home, target_, e) + lift;
    drawNeck(base, tip_, playerEye);
}

// Bends the long neck (a chain of tube segments) from `base` to `tip` along a swaying curve, and puts the head on
// the tip, turned to `look`, tilted a little.
void GrandmaEvent::drawNeck(glm::vec3 base, glm::vec3 tip, glm::vec3 look) {
    glm::vec3 span = tip - base;
    float len = glm::length(span);
    // curve: cubic Bezier base -> up out of the body -> above the tip -> tip, with a slow snake-like sway
    glm::vec3 c1 = base + glm::vec3(0, std::min(1.0f, 0.2f * len), 0);
    glm::vec3 c2 = tip + glm::vec3(0, std::min(0.6f, 0.12f * len), 0) - span * 0.3f;
    glm::vec3 side = glm::normalize(glm::cross(len > 1e-3f ? span : glm::vec3(0, 0, 1), glm::vec3(0, 1, 0)) + glm::vec3(1e-4f));
    auto at = [&](float u) {
        float a = 1 - u;
        glm::vec3 p = a * a * a * base + 3 * a * a * u * c1 + 3 * a * u * u * c2 + u * u * u * tip;
        float sway = std::sin(u * 7.0f - time_ * 2.3f) * 0.18f * std::sin(u * ter::PI) * std::min(1.0f, len / 3);
        return p + side * sway + glm::vec3(0, std::cos(u * 5.0f - time_ * 1.7f) * 0.06f * std::sin(u * ter::PI), 0);
    };
    glm::vec3 prev = at(0);
    for (int i = 0; i < kSegments; ++i) {
        glm::vec3 next = at(float(i + 1) / kSegments);
        auto& s = segments_[i];
        s->position = prev;
        float l = glm::length(next - prev);
        if (l > 1e-4f) s->setQuaternion(facing(next - prev));
        s->scale = {1, 1, l * 1.1f};   // a little overlap so the bends don't open
        prev = next;
    }
    // the head (whatever it hangs under now: her neck, or the holder at the shrine)
    ter::Object3D* parent = head_->parent;
    if (!parent) return;
    parent->updateWorldFromParents();
    head_->position = glm::vec3(glm::inverse(parent->matrixWorld) * glm::vec4(prev, 1));
    glm::vec3 view = look - prev;
    glm::quat world = (glm::length(view) > 1e-3f ? facing(view) : glm::quat(1, 0, 0, 0)) *
                      glm::angleAxis(0.35f * std::sin(time_ * 0.9f), glm::vec3(0, 0, 1));
    glm::quat parentRot = glm::quat_cast(glm::mat3(parent->matrixWorld));
    head_->setQuaternion(glm::inverse(parentRot) * world);
}
