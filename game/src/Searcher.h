#pragma once
// The ghost dancer (assets/models/dancer.glb, ผีนางรำ) who comes looking while the player holds their breath.
// She appears out in the fog in front of the hiding place and circles it, gliding; the louder the breathing
// (Breath::noise) the closer she drifts. Held out: she backs off into the fog and is gone. Heard: she is suddenly
// right in front of the player's face, then gone.
// The same class drives whoever else comes looking (rule 10: the dark bus's driver), with a different SearcherStyle.
//   searcher.load(world);  searcher.start(eye, facing);  searcher.update(dt, noise, eye, audio);  searcher.finish(caught);
#include <memory>
#include <ter/ter.h>
#include "Animator.h"
#include "World.h"

// Who is looking: the model, its clips, the music around them, how they move
struct SearcherStyle {
    std::string model = "models/dancer.glb";
    std::string searchClip = "dance_jerk", nearClip = "glide", foundClip = "glide";
    std::string music = "dance_music";   // "" = none
    float hover = 0.06f, bob = 0.02f;   // feet above the ground, up-down drift
};

class Searcher {
public:
    bool load(World& world, const SearcherStyle& style = SearcherStyle{});
    // begin the search around the player's eye (facing: the way they look out of the hiding place)
    void start(glm::vec3 eye, glm::vec3 facing);
    // the mini-game ended: caught = she found them
    void finish(bool caught, glm::vec3 eye, glm::vec3 facing, ter::Audio& audio);
    void update(float dt, float noise, glm::vec3 eye, ter::Audio& audio);
    bool active() const { return state_ != State::Off; }
    glm::vec3 position() const { return root_->position; }

private:
    enum class State { Off, Search, Leave, Found };
    ter::ObjPtr root_, light_;   // light_: a cold glow on her face when she finds you
    std::shared_ptr<Animator> anim_;
    State state_ = State::Off;
    glm::vec3 center_{0}, facing_{0, 0, -1};
    float angle_ = 0, dist_ = 8, t_ = 0;
    ter::SoundPtr music_;
    SearcherStyle style_;
    const std::vector<ter::Box>* colliders_ = nullptr;   // she goes round the shelter's posts and benches, not through
};
