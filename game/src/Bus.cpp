#include "Bus.h"

#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

ter::Object3D* findNode(ter::Object3D& root, const std::string& name) {
    ter::Object3D* found = nullptr;
    root.traverse([&](ter::Object3D& o) {
        if (!found && o.name == name) found = &o;
    });
    return found;
}

ter::ObjPtr findShared(ter::Object3D& root, const std::string& name) {
    ter::Object3D* o = findNode(root, name);
    return o ? o->shared_from_this() : nullptr;
}

// a material of a loaded glTF by its name in the file
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

std::shared_ptr<ter::Texture> paint(int w, int h, const std::function<void(ter::Canvas2D&)>& draw) {
    ter::Canvas2D c(w, h);
    draw(c);
    auto t = std::make_shared<ter::Texture>();
    c.upload(*t);
    return t;
}

// the bus faces -X (west): model +Z -> world -X, model +X (door side) -> world +Z (the shelter)
constexpr float kYaw = -ter::PI / 2;
glm::vec3 toWorld(glm::vec3 local) { return {-local.z, local.y, local.x}; }

}  // namespace

bool BusEvent::load(const std::string& path, World& world) {
    std::ifstream in(path);
    json j = json::parse(in, nullptr, false, true);
    if (j.is_discarded()) {
        std::fprintf(stderr, "could not read %s\n", path.c_str());
        return false;
    }
    const json route = j.value("route", json::object());
    laneZ = route.value("laneZ", laneZ);
    fromX = route.value("fromX", fromX);
    stopX = route.value("stopX", stopX);
    toX = route.value("toX", toX);
    speed = route.value("speed", speed);
    talkRadius = route.value("talkRadius", talkRadius);
    for (const json& a : j.value("arrivals", json::array())) {
        BusArrival b;
        b.at = parseTime(a.value("at", "03:00"), 180);
        b.script = a.value("script", "");
        b.sign = a.value("sign", "");
        b.headlights = a.value("headlights", true);
        b.wait = a.value("wait", 12.0f);
        b.final = a.value("final", false);
        b.dragScript = a.value("dragScript", "");
        b.manual = a.value("manual", false);
        arrivals_.push_back(b);
    }

    auto model = ter::loadGltf(ter::assetPath("models/bus.glb"));
    if (!model) return false;
    // "model": { "scale": [width, height, length], "frontText": ["line 1", "line 2"] } — the van is the bus shrunk.
    // Keep height = length so the wheels stay round.
    const json look = j.value("model", json::object());
    auto sv = look.value("scale", std::vector<float>{1, 1, 1});
    const glm::vec3 scale = sv.size() == 3 ? glm::vec3(sv[0], sv[1], sv[2]) : glm::vec3(1);
    model->root->scale = scale;
    scale_ = scale;
    wheelRadius_ = 0.47f * scale.y;
    auto frontText = look.value("frontText", std::vector<std::string>{"รถขับเคลื่อนด้วยพลังงานไฟฟ้า", "ELECTRIC BUS"});
    bus_ = ter::makeGroup("bus");
    bus_->rotation.y = kYaw;
    bus_->visible = false;
    bus_->add(model->root);
    world.root->add(bus_);
    collider_ = (int)world.colliders.size();
    world.colliders.push_back({glm::vec3(-1e4f), glm::vec3(-1e4f)});
    busAnim_ = std::make_shared<Animator>(model->animations);
    for (const char* w : {"bus_wheel_FL", "bus_wheel_FR", "bus_wheel_RL", "bus_wheel_RR"})
        if (auto n = findShared(*model->root, w)) wheels_.push_back(n);
    if (auto n = findNode(*model->root, "bus_door_front")) doorLocal_ = n->position * scale;

    // headlights (spot lights a little below the horizon), the cabin's cold light
    for (const char* h : {"bus_headlight_L", "bus_headlight_R"}) {
        if (auto n = findNode(*model->root, h)) {
            auto spot = ter::makeSpotLight(ter::hexc(0xfff1d6), 3.0f, 30.0f, 0.5f, 0.4f);
            spot->rotation.x = 0.08f;
            n->add(spot);
            lights_.push_back(spot);
        }
    }
    if (auto n = findNode(*model->root, "bus_cabin_light")) {
        cabin_ = ter::makePointLight(ter::hexc(0xd8e8ff), 1.2f, 6.0f);
        n->add(cabin_);
    }
    headMat_ = findMaterial(*model->root, "bus_headlight");
    tailMat_ = findMaterial(*model->root, "bus_taillight");
    if ((lampMat_ = findMaterial(*model->root, "bus_cabin_lamp"))) lampGlow_ = lampMat_->emissive;
    signMat_ = findMaterial(*model->root, "bus_sign");
    if (auto front = findMaterial(*model->root, "bus_front_text")) {
        auto tex = paint(512, 96, [&](ter::Canvas2D& c) {
            c.setFill("#060607");
            c.fillRect(0, 0, 512, 96);
            c.setFill("#eeeeee");
            c.setTextAlign(ter::TextAlign::Center);
            c.setFont("Mali", 30, 600);
            if (frontText.size() > 0) c.fillText(frontText[0], 256, 42);
            c.setFont("Mali", 30, 700);
            if (frontText.size() > 1) c.fillText(frontText[1], 256, 82);
        });
        front->map = tex;
        front->color = glm::vec3(1);
    }

    // the driver sits in the cab, hands on the wheel
    if (auto d = ter::loadGltf(ter::assetPath("models/driver.glb"))) {
        driver_ = ter::makeGroup("driver");
        driver_->add(d->root);
        if (auto seat = findNode(*model->root, "bus_driver_seat")) driver_->position = seat->position;
        driver_->scale = 1.0f / scale;   // a full-size driver in a shrunk cab
        model->root->add(driver_);
        driverAnim_ = std::make_shared<Animator>(d->animations);
        driverAnim_->play("drive");
    }
    return true;
}

void BusEvent::paintSign(const std::string& text) {
    if (!signMat_) return;
    auto tex = paint(512, 64, [&](ter::Canvas2D& c) {
        c.setFill("#050403");
        c.fillRect(0, 0, 512, 64);
        c.setFill("#ff8a1c");
        c.setFont("Mali", 34, 700);
        c.setTextAlign(ter::TextAlign::Center);
        c.fillText(text, 256, 46);
    });
    signMat_->map = tex;
    signMat_->emissiveMap = tex;
    signMat_->color = glm::vec3(1);
    signMat_->emissive = glm::vec3(1);
}

void BusEvent::setLights(bool on) {
    for (auto& l : lights_) l->visible = on;
    if (cabin_) cabin_->visible = on;   // a bus "without lights" is dark inside too
    if (lampMat_) lampMat_->emissive = on ? lampGlow_ : glm::vec3(0);
    if (headMat_) headMat_->emissive = on ? glm::vec3(1.0f, 0.95f, 0.85f) * 2.0f : glm::vec3(0);
    if (tailMat_) tailMat_->emissive = on ? glm::vec3(1.0f, 0.1f, 0.05f) : glm::vec3(0);
}

void BusEvent::callNow() {
    if (state_ != State::Away) return;
    begin(arrivals_.empty() ? BusArrival{} : arrivals_.front());
}

void BusEvent::begin(const BusArrival& a) {
    current_ = a;
    state_ = State::Arriving;
    t_ = 0;
    x_ = fromX;
    bus_->visible = true;
    setLights(a.headlights);
    paintSign(a.sign);
    if (driverAnim_) driverAnim_->play("drive", true, 0);
    busAnim_->play("doors_close", false, 0);   // doors shut
}

void BusEvent::startLeaving() {
    if (companion_ == Companion::Walking || companion_ == Companion::Boarding) {
        leaveAfterCompanion_ = true;   // wait for him to get on
        return;
    }
    if (state_ == State::Closing || state_ == State::Leaving) return;
    state_ = State::Closing;
    t_ = 0;
    busAnim_->play("doors_close", false, 0);
    if (driverAnim_) driverAnim_->play("drive");
}

void BusEvent::onAction(const std::string& action, World& world) {
    if (action == "leave") startLeaving();
    else if (action == "player_board") {
        playerBoarded_ = true;
        startLeaving();
    } else if (action == "companion_board" && world.companion && companion_ == Companion::Stay) {
        companion_ = Companion::Walking;
        world.companionFacesPlayer = false;
        if (world.companionAnim) world.companionAnim->play("walk");
    } else {
        std::fprintf(stderr, "bus: unknown action '%s'\n", action.c_str());
    }
}

void BusEvent::update(float dt, const GameClock& clock, glm::vec3 playerPos, World& world, Dialogue& dialogue, ter::Audio& audio) {
    if (state_ == State::Away) {
        for (BusArrival& a : arrivals_) {
            if (!a.done && !a.manual && clock.minutes() >= a.at && !hold) {
                a.done = true;
                begin(a);
                break;
            }
        }
        if (state_ == State::Away) {
            if (collider_ >= 0) world.colliders[collider_] = {glm::vec3(-1e4f), glm::vec3(-1e4f)};   // gone (vanish() too)
            return;
        }
    }
    t_ += dt;
    float oldX = x_;
    const glm::vec3 busPos(x_, 0, laneZ);
    doorWorld_ = busPos + toWorld(doorLocal_);
    const float toDoor = glm::length(glm::vec2(playerPos.x - doorWorld_.x, playerPos.z - doorWorld_.z));

    switch (state_) {
        case State::Arriving: {
            // decelerate evenly from `speed` to a stop at stopX
            float T = 2 * (fromX - stopX) / speed, s = std::min(1.0f, t_ / T);
            x_ = stopX + (fromX - stopX) * (1 - s) * (1 - s);
            if (s >= 1) {
                state_ = State::Opening;
                t_ = 0;
                busAnim_->play("doors_open", false, 0);
                audio.play("bus_hiss", {.volume = 0.9f, .spatial = true, .position = doorWorld_, .maxDistance = 25});
            }
            break;
        }
        case State::Opening:
            if (busAnim_->finished()) {
                state_ = State::Waiting;
                t_ = 0;
                if (current_.final) onAction("companion_board", world);
            }
            break;
        case State::Waiting:
            if (toDoor < talkRadius && !current_.script.empty() && dialogue.start(current_.script)) {
                state_ = State::Talking;
                if (driverAnim_) driverAnim_->play("drive_ask");
            } else if (current_.final && t_ > current_.wait) {
                // the last bus does not leave without the player: the companion comes back for them
                if (!dialogue.active() && dialogue.start(current_.dragScript)) state_ = State::Talking;
                else if (!dialogue.active()) onAction("player_board", world);
            } else if (t_ > current_.wait) {
                dialogue.flags["bus_missed"] = "1";
                startLeaving();
            }
            break;
        case State::Talking:
            if (toDoor > leaveRadius && dialogue.active() && !current_.final) {
                dialogue.stop();
                dialogue.flags["bus_walked_away"] = "1";
                startLeaving();
            } else if (!dialogue.active()) {
                if (current_.final && !playerBoarded_) onAction("player_board", world);   // nobody stays behind
                else startLeaving();
            }
            break;
        case State::Closing:
            if (t_ == dt) audio.play("bus_hiss", {.volume = 0.7f, .spatial = true, .position = doorWorld_, .maxDistance = 25});
            if (busAnim_->finished()) {
                state_ = State::Leaving;
                t_ = 0;
            }
            break;
        case State::Leaving: {
            // pull away: speed up over 4 s, then cruise
            const float acc = speed / 4;
            x_ = t_ < 4 ? stopX - 0.5f * acc * t_ * t_ : stopX - 8 * acc - speed * (t_ - 4);
            if (x_ < toX) {
                state_ = State::Away;
                bus_->visible = false;
                if (companion_ == Companion::Aboard && world.companion) world.companion->visible = false;
                if (hum_) hum_->stop();
                hum_.reset();
            }
            break;
        }
        case State::Away: break;
    }

    // move the bus, spin the wheels, hum
    bus_->position = {x_, 0, laneZ};
    // the player (and Jimmy) can't walk through it: the model's bounds (bus.glb, local x ±1.33, y 0..3.24,
    // z -4.32..4.62) scaled, turned to face west (local z -> world -x, local x -> world z)
    if (collider_ >= 0) {
        if (state_ == State::Away) {
            world.colliders[collider_] = {glm::vec3(-1e4f), glm::vec3(-1e4f)};
        } else {
            const glm::vec3 lo = glm::vec3(-1.33f, 0, -4.32f) * scale_, hi = glm::vec3(1.33f, 3.24f, 4.62f) * scale_;
            world.colliders[collider_] = {{x_ - hi.z, lo.y, laneZ + lo.x}, {x_ - lo.z, hi.y, laneZ + hi.x}};
        }
    }
    for (auto& w : wheels_) w->rotation.x += (oldX - x_) / wheelRadius_;
    if (state_ != State::Away) {
        if (!hum_) hum_ = audio.play("bus_hum", {.volume = 0.8f, .loop = true, .spatial = true, .position = bus_->position, .maxDistance = 45, .fadeIn = 1.5f});
        if (hum_) hum_->setPosition(bus_->position);
    }
    busAnim_->update(dt);
    if (driverAnim_) driverAnim_->update(dt);

    // the companion: walk to the door, get on, ride away
    if (world.companion) {
        ter::Object3D& c = *world.companion;
        if (companion_ == Companion::Walking) {
            glm::vec3 d = doorWorld_ - c.position;
            d.y = 0;
            float dist = glm::length(d);
            if (dist < 0.05f) {
                c.position = {doorWorld_.x, 0, doorWorld_.z};
                c.rotation.y = ter::PI;   // facing into the bus (-Z)
                if (world.companionAnim) world.companionAnim->play("board_bus", false, 0.2f);
                companion_ = Companion::Boarding;
            } else {
                glm::vec3 next = c.position + d / dist * std::min(dist, 1.3f * dt);
                // round the shelter's posts, not through them (his own box sits where he is standing: skipped)
                std::vector<ter::Box> around;
                for (const ter::Box& b : world.colliders)
                    if (!(c.position.x > b.min.x - 0.3f && c.position.x < b.max.x + 0.3f && c.position.z > b.min.z - 0.3f &&
                          c.position.z < b.max.z + 0.3f))
                        around.push_back(b);
                next = ter::resolveCircle(next, 0.25f, around);
                c.position = {next.x, std::max(0.0f, c.position.y - dt * 0.5f), next.z};
                c.rotation.y = std::atan2(d.x, d.z);
            }
        } else if (companion_ == Companion::Boarding && world.companionAnim && world.companionAnim->finished()) {
            companion_ = Companion::Aboard;
            companionOffset_ = c.position - bus_->position;
            if (leaveAfterCompanion_) {
                leaveAfterCompanion_ = false;
                startLeaving();
            }
        } else if (companion_ == Companion::Aboard) {
            c.position = bus_->position + companionOffset_;
        }
    }
}

void BusEvent::vanish() {
    state_ = State::Away;
    bus_->visible = false;
    if (hum_) hum_->stop();
    hum_.reset();
}
