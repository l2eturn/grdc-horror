#pragma once
// Jimmy, the companion (assets/data/companion.json; the model and its clips are world.companion / companionAnim).
// He behaves like a friend waiting with you — and is the ghost all along (game-comcept.md §1, §3.4):
//  - sits on the bench; when the player wanders off he gets up and follows, and sits again once they are back
//  - says something now and then (lines change with the hour: friendly -> off -> eerie); E next to him talks
//  - when a rule's sign shows up he nudges the player toward breaking it ("your phone's buzzing, look!")
//  - while some rules' windows are open he does his own thing (crouches with you for rule 5, but stands in the
//    open for rule 10 — the dark bus doesn't see him)
//  - every broken rule he comes a step closer (follows nearer, says something)
// The bus (boarding) and the ending move him themselves: pass paused = true while they do.
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <ter/ter.h>
#include "Dialogue.h"
#include "World.h"

struct CompanionInputs {
    glm::vec3 player{0};      // the player's eye position
    bool playerInShelter = false;
    float clock = 0;          // GameClock::minutes()
    bool paused = false;      // something else moves him right now (bus, forest walk)
};

class Companion {
public:
    bool load(const std::string& path, World& world);
    // every frame after updateWorld (the clip has been applied; the head turn goes on top of it)
    void update(float dt, const CompanionInputs& in, World& world, Dialogue& dialogue, ter::Audio& audio);

    void onRuleOpened(const std::string& rule);
    void onRuleClosed(const std::string& rule);
    void onSignal(const std::string& signal);
    void onStrike();
    // grab the player's arm (the 23:00 bus): walk right up to them for a few seconds
    void holdPlayer() { following_ = true, holdTimer_ = 6; }

    // E: close enough to talk to (and not busy)
    bool canTalk(glm::vec3 player, const World& world) const;
    void talk(float clock, Dialogue& dialogue);
    const std::string& name() const { return name_; }

private:
    enum class Pose { Sit, Stand, Crouch };
    struct Phase {
        float until = 0;
        std::vector<std::string> scripts;
        int next = 0;
    };
    std::string pick(std::vector<Phase>& phases, float clock);
    // queue a script; it waits `delay` seconds (and for the dialogue box to be free)
    void say(const std::string& script, float delay = 0);
    void setAnim(World& world, const std::string& clip);

    std::string name_ = "จิมมี่";
    glm::vec3 seat_{0};
    float seatYaw_ = 0, walkSpeed_ = 1.5f, followDist_ = 1.8f, closerPerStrike_ = 0.3f, minDist_ = 0.7f;
    float followAfter_ = 4.5f, sitAfter_ = 8;
    bool footsteps_ = false;
    float chatterMin_ = 28, chatterMax_ = 50, chatterRadius_ = 6, talkRadius_ = 1.8f;
    std::vector<Phase> chatter_, talk_;
    std::map<std::string, std::string> signals_;
    nlohmann::json rules_;
    std::vector<std::string> strikeScripts_;

    // state
    bool following_ = false;
    std::string rule_;             // the rule whose window drives him right now ("" = his own routine)
    glm::vec3 freezeAt_{0};
    bool walking_ = false;
    int strikes_ = 0;
    float holdTimer_ = 0;
    float chatterTimer_ = 20, backTimer_ = 0, stepTimer_ = 0, stuckTimer_ = 0, ghostTimer_ = 0;
    std::vector<std::pair<std::string, float>> pending_;   // scripts waiting for the dialogue box (age; < 0 = delayed)
    int collider_ = -1;            // his box in world.colliders (moves with him)
    ter::Object3D* head_ = nullptr;
    glm::quat headSet_{1, 0, 0, 0}, headBase_{1, 0, 0, 0};
    float headYaw_ = 0;
};
