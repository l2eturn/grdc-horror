#pragma once
// The old woman with the stuck cart (rule 7), data in assets/data/grandma.json, models grandma.glb + cart.glb
// (+ the map's spirit_shrine for the forest walk). At "appear" she is on the roadside by the path into the
// forest, straining at her junk cart, one wheel deep in mud, calling for help. Her face is "normal" or "scary" (or
// "random"). The rule: scary face -> help her, ordinary face -> pretend not to see.
//   scary  + helped  (E near her)   the cart comes free, she thanks you and pushes it away     flag grandma=helped_scary
//   scary  + ignored (giveUpAfter)  she gives up and is gone                                    flag grandma=ignored_scary
//   normal + ignored (stretchAfter) her neck stretches all the way to you, her face hangs in
//                                   front of yours for a while, then pulls back                 flag grandma=ignored_normal
//   normal + helped                 you push the cart with her down the path into the forest while the fog closes in;
//                                   at the old tree she is gone, only the spirit house is left; her head comes slowly
//                                   out of its door on a long neck, stares, then comes after you up the path: run back
//                                   to the bus shelter. Reach it and the head goes back, the fog lifts, the night goes on
//                                   (flag grandma=escaped); caught = black (placeholder)        flag grandma=caught
// During that walk the event owns the player's position (they can still look around): see cutscene()/playerSpot().
// Flags go into the dialogue's flags (for the rule checks, M3). Format: game/EVENTS_FORMAT.md
#include <memory>
#include <string>
#include <vector>
#include <ter/ter.h>
#include "Animator.h"
#include "Dialogue.h"
#include "GameClock.h"
#include "World.h"

class GrandmaEvent {
public:
    // set every frame: something else is happening; she waits to appear until it is over
    bool hold = false;
    // the clock jumped past her time: she doesn't come late
    void skipBefore(float minutes) {
        if (appearAt < minutes) done_ = true;
    }
    bool load(const std::string& path, World& world);
    // appear now (debug key / --grandma); face "" = the one in the file
    void callNow(const std::string& face = "");
    void update(float dt, const GameClock& clock, glm::vec3 playerEye, World& world, Dialogue& dialogue, ter::Audio& audio);
    // the player is close enough to help (show the hint, E helps)
    bool canHelp(glm::vec3 playerPos) const;
    void help(Dialogue& dialogue, ter::Audio& audio);
    void toggleFace();
    // 0..1 black over the screen
    float blackout() const { return blackout_; }
    bool active() const { return state_ != State::Hidden; }

    // the forest walk owns the player: main moves them to playerSpot() every frame (the mouse still looks around)
    bool cutscene() const;
    glm::vec3 playerSpot(glm::vec3 current, float dt) const;
    // turns the player's view toward what they must see (the shrine, the head); returns true while it does
    bool steer(float& yaw, float dt) const;
    // her head is after the player (show "run back to the shelter")
    bool chasing() const { return state_ == State::Chase; }
    // it caught the player (the screen stays black; placeholder)
    bool lost() const { return state_ == State::Lost; }
    // caught in the forest and the black is over: put the player back at the shelter (true once)
    bool takeWokeUp() {
        bool w = wokeUp_;
        wokeUp_ = false;
        return w;
    }

private:
    enum class State {
        Hidden, Struggling, Freed, Leaving, GivingUp,              // the scary face, or before anyone decides
        Stretching, Staring, Retracting, AfterNeck,                // ordinary face, ignored: the long neck
        Escort, Arrived, Vanish, Shrine, HeadOut, Chase, Escaped, Lost   // ordinary face, helped: into the forest
    };
    void begin();
    void hide();
    void setState(State s);
    void say(const std::string& script, Dialogue& dialogue);
    void lookAt(glm::vec3 target, float dt);   // turn the (short) neck toward a point while she pushes
    // bends the long neck from `base` to the head at `tip`, the face turned to `look`
    void drawNeck(glm::vec3 base, glm::vec3 tip, glm::vec3 look);
    void updateRoadNeck(float dt, glm::vec3 playerEye);
    void setFog(World& world, float k);        // 0 = the night's own fog .. 1 = escort fog .. >1 thicker still

    // models
    Grandma g_;
    ter::ObjPtr group_, cart_, neckGroup_, headHolder_;
    ter::Object3D *neck_ = nullptr, *neckSkin_ = nullptr, *head_ = nullptr;
    std::shared_ptr<Animator> cartAnim_;
    std::vector<ter::ObjPtr> segments_;
    glm::vec3 headRest_{0};
    glm::quat headRestRot_{1, 0, 0, 0};
    float neckYaw_ = 0;

    // data
    float appearAt = 70, stretchAfter = 25, giveUpAfter = 40, helpRadius = 2.2f, callEvery = 14;
    float neckSpeed = 2.5f, stareTime = 7, stopShort = 0.8f, maxReach = 25;
    std::string faceSetting = "random";
    glm::vec3 pos{9, 0, -0.8f};
    float yaw = -ter::PI / 2;
    // the forest walk
    std::vector<glm::vec2> path_;
    float escortSpeed = 0.75f, fogVisibility = 3.5f, fogHeight = 12;
    float headSpeed = 0.35f, shrineStare = 3, chaseSpeed = 3.0f, catchRadius = 0.75f;
    glm::vec3 shrineDoor_{0};
    bool hasShrine_ = false;
    float fog0Vis_ = 25, fog0Height_ = 4;
    ter::Box safe_{glm::vec3(-2.5f, 0, 0), glm::vec3(2.5f, 3, 3)};   // the shelter zone: reach it and she is gone
    glm::vec3 safeCenter_{0, 0, 1.5f};
    float safeMargin = 2.0f;   // metres around the zone that also count (the stop sign, the road edge in front)
    bool safe(glm::vec3 p) const;

    State state_ = State::Hidden;
    bool done_ = false;
    float t_ = 0, callTimer_ = 0, blackout_ = 0, time_ = 0, along_ = 0, stepTimer_ = 0;
    float reach_ = 0;   // 0 = head at home .. 1 = head at the target
    glm::vec3 tip_{0}, target_{0}, eye_{0}, spot_{0}, steerAt_{0};
    bool steering_ = false;
    ter::SoundPtr creak_;
    bool wokeUp_ = false;
};
