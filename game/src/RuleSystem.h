#pragma once
// RuleSystem: checks whether the player keeps the 12 rules (game-comcept.md §1.0). Each rule in
// assets/data/rules.json may carry a "check" block (format: game/DATA_FORMAT.md):
//
//   "check": { "at": "22:30", "seconds": 10, "signal": "phone_buzz",
//              "type": "phone_held", "onViolate": { "sound": "sting" } }
//
//  - window: from "at" (in-game time) for "seconds" real seconds; no "at" = checked all night
//  - signal: what tells the player the rule applies now (a buzz, the lights going out). At the window's start the
//    game is asked to play it; a signal the game can't do yet (M6) skips the rule for that window, so nobody is
//    punished for a sign they never saw
//  - type: which evaluator decides (the registry in RuleSystem.cpp; add new types there)
//  - breaking a rule never kills: it is reported once per window (takeViolations) and counts a strike
//
//   rules.load(path);
//   rules.update(dt, clock, inputs, [&](const std::string& signal) { ...play it, return true if done... });
//   for (auto& v : rules.takeViolations()) ...
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <ter/ter.h>
#include "GameClock.h"

// What the checks can see this frame (main.cpp fills it in)
struct RuleInputs {
    glm::vec3 position{0}, forward{0, 0, -1};   // the player's eye and view direction
    bool phoneHeld = false, phoneReading = false, flashOn = false;
    std::string hideSpot;                        // "" = not hidden
    bool breathActive = false;
    bool breathEnded = false, breathCaught = false;   // the breath mini-game finished this frame (and how)
    const std::map<std::string, std::string>* flags = nullptr;   // dialogue answers + game state flags
    std::function<bool(const std::string& zone, glm::vec3 p)> inZone;
    // another event has the player right now (a bus at the stop, a question on screen, the forest walk):
    // windows that come due wait until it is over
    bool busy = false;
};

struct RuleCheck;

// Per-window state an evaluator keeps
struct RuleRun {
    float t = 0;            // seconds into the window
    float accum = 0;        // seconds counted so far (looking, outside, ...)
    glm::vec3 start{0};     // where the player was when the window opened
    bool hid = false;       // was hidden at some point in this window
    bool breathAsked = false;
    bool wasHeld = false;   // the phone was in hand when the window opened
    bool over = false;      // the rule is kept for good this window (it closes early)
};

// Returns a non-empty reason when the rule is broken this frame
using RuleEvaluator = std::function<std::string(const RuleCheck&, RuleRun&, const RuleInputs&, float dt)>;

struct RuleCheck {
    std::string id, type, signal;
    std::string text;                 // the rule's message (for the debug overlay)
    float at = -1;                    // window start, minutes (GameClock); -1 = all night
    float seconds = 0;                // window length (real seconds)
    nlohmann::json params;            // the whole "check" block; evaluators read their own keys
    nlohmann::json onViolate;
    // state
    enum class State { Waiting, Active, Skipped, Done } state = State::Waiting;
    RuleRun run;
    bool brokenThisWindow = false;
    int broken = 0;                   // times broken tonight
};

struct RuleViolation {
    std::string rule, reason;
    nlohmann::json onViolate;
};

class RuleSystem {
public:
    bool load(const std::string& path);
    // `signal` plays a rule's signal when its window opens; returns false if the game can't do it yet
    void update(float dt, const GameClock& clock, const RuleInputs& in, const std::function<bool(const std::string&)>& signal);
    std::vector<RuleViolation> takeViolations() { return std::move(violations_); }
    // signals of the windows that closed since the last call (take their props away again)
    std::vector<std::string> takeEndedSignals() { return std::move(ended_); }
    // ids of the timed rules whose window opened / closed since the last call (the companion reacts)
    std::vector<std::string> takeOpened() { return std::move(opened_); }
    std::vector<std::string> takeClosed() { return std::move(closed_); }

    // Open a rule's window now, even without its signal (debug: --rule <id>, J = the next one still waiting).
    // Returns false if unknown.
    bool startNow(const std::string& id);
    bool startNext();
    // the clock jumped (--time, a playtest): windows due before `minutes` are not played late
    void skipBefore(float minutes);

    // A rule whose window is open asks for the breath mini-game ("easy" / "hard") once the player hides
    std::string breathWanted();

    int strikes() const { return strikes_; }
    bool isActive(const std::string& id) const {
        for (const RuleCheck& c : checks_)
            if (c.id == id) return c.state == RuleCheck::State::Active;
        return false;
    }
    // a timed rule's window is open (events wait for it to close)
    bool windowOpen() const {
        for (const RuleCheck& c : checks_)
            if (c.at >= 0 && c.state == RuleCheck::State::Active) return true;
        return false;
    }
    const std::vector<RuleCheck>& checks() const { return checks_; }
    // one line per rule for the debug overlay
    std::vector<std::string> debugLines() const;

private:
    // forced (debug) windows open even when their signal isn't built yet
    void open(RuleCheck& c, const RuleInputs& in, const std::function<bool(const std::string&)>& signal, bool forced);

    std::vector<RuleCheck> checks_;
    std::vector<RuleViolation> violations_;
    std::vector<std::string> ended_, opened_, closed_;
    std::map<std::string, RuleEvaluator> evaluators_;
    std::vector<std::string> forced_;   // ids to open on the next update
    std::string breathWanted_;
    int strikes_ = 0;
};
