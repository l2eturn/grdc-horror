#pragma once
// The bus at the stop (assets/data/bus.json, model assets/models/bus.glb + the driver driver.glb):
// at each arrival time it drives in along the lane, stops with its front door at the shelter, opens the doors,
// and the driver runs a dialogue script with the person waiting. The answers' actions decide what happens:
//   "leave"            close the doors and drive off
//   "player_board"     the player gets on (screen fades out; placeholder ending)
//   "companion_board"  the companion walks to the door and gets on, then the bus leaves with him
// Nobody near the door within "wait" seconds -> it leaves. Walking away mid-conversation -> it leaves.
// The same class drives the small van of rule 6 (assets/data/van.json): the bus model shrunk by "model" → "scale".
// Format: game/EVENTS_FORMAT.md
#include <memory>
#include <string>
#include <vector>
#include <ter/ter.h>
#include "Animator.h"
#include "Dialogue.h"
#include "GameClock.h"
#include "World.h"

struct BusArrival {
    float at = 0;                 // minutes after midnight
    std::string script, sign;     // dialogue script id, destination sign text
    bool headlights = true;       // false: a bus driving without lights (rule 10)
    float wait = 12;              // seconds it waits for someone at the door
    // the last bus of the night (05:30): the companion gets on by himself when the doors open, the player can't
    // walk away from the conversation, and if they don't come to the door the companion's `dragScript` runs
    // (he pulls them on). Boarding it starts the ending.
    bool final = false;
    std::string dragScript;
    bool manual = false;          // never on its own: only callNow() (rule 10's dark bus comes with its rule window)
    bool done = false;
};

class BusEvent {
public:
    bool load(const std::string& path, World& world);
    // set every frame: something else is happening (a rule's window, another event); an arrival that comes due
    // waits until it is over
    bool hold = false;
    // arrive now (debug key / --bus), with the first arrival's settings
    void callNow();
    // the driver in his seat (rule 10: he has got out to look for you)
    void setDriverVisible(bool v) {
        if (driver_) driver_->visible = v;
    }
    // the clock jumped: arrivals due before `minutes` don't come late
    void skipBefore(float minutes) {
        for (BusArrival& a : arrivals_)
            if (a.at < minutes) a.done = true;
    }
    void update(float dt, const GameClock& clock, glm::vec3 playerPos, World& world, Dialogue& dialogue, ter::Audio& audio);
    // an action reached in the dialogue
    void onAction(const std::string& action, World& world);

    bool active() const { return state_ != State::Away; }
    // the one stopping now is the last bus of the night (it walks the companion in itself)
    bool isFinal() const { return current_.final; }
    bool playerBoarded() const { return playerBoarded_; }
    // the player got on the final bus (-> the ending)
    bool finalBoarded() const { return playerBoarded_ && current_.final; }
    // take the bus off the road at once (the ending shows its own bus)
    void vanish();

private:
    enum class State { Away, Arriving, Opening, Waiting, Talking, Closing, Leaving };
    void setLights(bool on);
    void paintSign(const std::string& text);
    void begin(const BusArrival& a);
    void startLeaving();

    ter::ObjPtr bus_, driver_, cabin_;
    std::shared_ptr<Animator> busAnim_, driverAnim_;
    std::vector<ter::ObjPtr> wheels_, lights_;
    ter::MatPtr signMat_, headMat_, tailMat_, lampMat_;
    glm::vec3 lampGlow_{0};   // the cabin tubes' glow when lit
    glm::vec3 doorLocal_{0}, doorWorld_{0};
    float wheelRadius_ = 0.47f;
    glm::vec3 scale_{1};
    int collider_ = -1;   // its box in world.colliders: follows the bus, parked far away while it is gone
    ter::SoundPtr hum_;

    std::vector<BusArrival> arrivals_;
    BusArrival current_;
    float laneZ = -2.6f, fromX = 80, stopX = 4.5f, toX = -90, speed = 9, talkRadius = 7, leaveRadius = 12;
    State state_ = State::Away;
    float t_ = 0, x_ = 0;
    bool playerBoarded_ = false, leaveAfterCompanion_ = false;

    // the companion walking to the door and getting on
    enum class Companion { Stay, Walking, Boarding, Aboard } companion_ = Companion::Stay;
    glm::vec3 companionOffset_{0};
};
