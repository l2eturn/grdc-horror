#include "Searcher.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kFar = 4.5f, kNear = 1.1f;   // how far she circles: silent .. noise almost full
}  // namespace

bool Searcher::load(World& world, const SearcherStyle& style) {
    style_ = style;
    colliders_ = &world.colliders;
    auto model = ter::loadGltf(ter::assetPath(style.model));
    if (!model) {
        std::fprintf(stderr, "searcher: could not load %s\n", style.model.c_str());
        return false;
    }
    root_ = ter::makeGroup("searcher");
    root_->add(model->root);
    root_->visible = false;
    world.root->add(root_);
    light_ = ter::makePointLight(ter::hexc(0xb8c8d8), 0.0f, 2.0f);
    light_->position = {0, 1.75f, 0.45f};
    root_->add(light_);
    anim_ = std::make_shared<Animator>(model->animations);
    return true;
}

void Searcher::start(glm::vec3 eye, glm::vec3 facing) {
    state_ = State::Search;
    center_ = {eye.x, 0, eye.z};
    facing_ = glm::normalize(glm::vec3(facing.x, 0, facing.z) + glm::vec3(0, 0, 1e-4f));
    angle_ = std::atan2(facing_.x, facing_.z);   // starts straight out in front of the hiding place
    dist_ = kFar;
    t_ = 0;
    root_->visible = true;
    anim_->play(style_.searchClip, true, 0);
}

void Searcher::finish(bool caught, glm::vec3 eye, glm::vec3 facing, ter::Audio& audio) {
    if (state_ == State::Off) return;
    t_ = 0;
    if (music_) music_->stop();
    music_.reset();
    if (caught) {
        // right in front of the face, looking into it
        state_ = State::Found;
        glm::vec3 f = glm::normalize(glm::vec3(facing.x, 0, facing.z) + glm::vec3(0, 0, 1e-4f));
        root_->position = glm::vec3(eye.x, 0, eye.z) + f * 0.55f;
        root_->position.y = eye.y - 1.55f;   // her eyes level with the player's, wherever they crouch
        root_->rotation.y = std::atan2(-f.x, -f.z);
        anim_->play(style_.foundClip, true, 0);
        light_->intensity = 0.9f;
        audio.play("scream");
    } else {
        state_ = State::Leave;
    }
}

void Searcher::update(float dt, float noise, glm::vec3 eye, ter::Audio& audio) {
    if (state_ == State::Off) return;
    t_ += dt;
    anim_->update(dt);
    if (state_ == State::Found) {
        if (t_ > 1.6f) {
            light_->intensity = 0;
            state_ = State::Off;
            root_->visible = false;
        }
        return;
    }
    if (!music_ && state_ == State::Search && !style_.music.empty())
        music_ = audio.play(style_.music, {.volume = 0.7f, .loop = true, .spatial = true, .position = root_->position, .maxDistance = 25, .fadeIn = 2});

    // circle in front of the hiding place (never behind the wall it hides behind), drifting closer with the noise
    float want = state_ == State::Leave ? kFar + 6 : glm::mix(kFar, kNear, std::clamp(noise, 0.0f, 1.0f));
    dist_ += (want - dist_) * std::min(1.0f, dt * (state_ == State::Leave ? 0.6f : 0.8f));
    float base = std::atan2(facing_.x, facing_.z);
    angle_ = base + std::sin(t_ * 0.35f) * 1.0f;
    glm::vec3 pos = center_ + glm::vec3(std::sin(angle_), 0, std::cos(angle_)) * dist_;
    // around the posts and benches, not through them (the circle point slides along whatever is in the way)
    if (colliders_) pos = ter::resolveCircle(pos, 0.3f, *colliders_);
    pos.y = style_.hover + std::sin(t_ * 1.7f) * style_.bob;
    root_->position = pos;
    // she faces the hiding place once she is close; far off she drifts along her circle
    glm::vec3 to = glm::vec3(eye.x, 0, eye.z) - glm::vec3(pos.x, 0, pos.z);
    root_->rotation.y = std::atan2(to.x, to.z);
    if (state_ == State::Search && dist_ < 3.5f && anim_->current() != style_.nearClip) anim_->play(style_.nearClip, true, 0.5f);
    if (music_) music_->setPosition(pos);

    if (state_ == State::Leave && dist_ > kFar + 4) {
        state_ = State::Off;
        root_->visible = false;
        if (music_) music_->stop();
        music_.reset();
    }
}
