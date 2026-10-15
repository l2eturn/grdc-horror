#include "RuleSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>

using json = nlohmann::json;

namespace {

float xzDistance(glm::vec3 a, glm::vec3 b) { return glm::length(glm::vec2(a.x - b.x, a.z - b.z)); }

std::string flagValue(const RuleInputs& in, const std::string& key) {
    if (!in.flags) return "";
    auto it = in.flags->find(key);
    return it == in.flags->end() ? "" : it->second;
}

// The evaluators, by "type". Each gets the rule's "check" block (c.params), its per-window state, and this
// frame's inputs, and returns why the rule is broken ("" = fine so far).
std::map<std::string, RuleEvaluator> makeEvaluators() {
    std::map<std::string, RuleEvaluator> e;

    // outside map zone "zone" for more than "grace" seconds (rule 2: past the lamp posts)
    e["leave_zone"] = [](const RuleCheck& c, RuleRun& r, const RuleInputs& in, float dt) -> std::string {
        bool inside = in.inZone && in.inZone(c.params.value("zone", ""), in.position);
        r.accum = inside ? 0 : r.accum + dt;
        return r.accum > c.params.value("grace", 3.0f) ? "left zone " + c.params.value("zone", "") : "";
    };

    // inside map zone "zone" for more than "grace" seconds
    e["enter_zone"] = [](const RuleCheck& c, RuleRun& r, const RuleInputs& in, float dt) -> std::string {
        bool inside = in.inZone && in.inZone(c.params.value("zone", ""), in.position);
        r.accum = inside ? r.accum + dt : 0;
        return r.accum > c.params.value("grace", 0.0f) ? "entered zone " + c.params.value("zone", "") : "";
    };

    // must be inside map zone "zone" from "within" seconds after the window opens (rule 8: under the lit lamp)
    e["must_be_in"] = [](const RuleCheck& c, RuleRun& r, const RuleInputs& in, float) -> std::string {
        if (r.t < c.params.value("within", 10.0f)) return "";
        bool inside = in.inZone && in.inZone(c.params.value("zone", ""), in.position);
        return inside ? "" : "not in zone " + c.params.value("zone", "") + " in time";
    };

    // taking the phone out, or opening the messages if it was already in hand (rule 3: it buzzes with no message,
    // don't pick it up to look)
    e["phone_held"] = [](const RuleCheck&, RuleRun& r, const RuleInputs& in, float) -> std::string {
        if (in.phoneReading) return "read the phone";
        if (!in.phoneHeld) r.wasHeld = false;
        return in.phoneHeld && !r.wasHeld ? "picked up the phone" : "";
    };

    // the flash shining
    e["light_on"] = [](const RuleCheck&, RuleRun&, const RuleInputs& in, float) -> std::string {
        return in.flashOn ? "flash on" : "";
    };

    // looking at "target" [x,y,z] (within "coneDeg" of the view) for more than "max" seconds in total (rule 4)
    e["look_at"] = [](const RuleCheck& c, RuleRun& r, const RuleInputs& in, float dt) -> std::string {
        auto t = c.params.value("target", std::vector<float>{0, 0, 0});
        if (t.size() != 3) return "";
        glm::vec3 to = glm::vec3(t[0], t[1], t[2]) - in.position;
        float cone = std::cos(c.params.value("coneDeg", 12.0f) * ter::PI / 180);
        if (glm::length(to) > 0.01f && glm::dot(glm::normalize(to), glm::normalize(in.forward)) > cone) r.accum += dt;
        return r.accum > c.params.value("max", 3.0f) ? "looked too long" : "";
    };

    // hidden in "spot" (any hiding place if omitted) from "within" seconds on, with the phone away; the breath
    // mini-game ("breath": easy/hard) starts once hidden, and being heard breaks the rule (rules 5 and 10)
    e["must_hide"] = [](const RuleCheck& c, RuleRun& r, const RuleInputs& in, float) -> std::string {
        std::string spot = c.params.value("spot", "");
        bool hidden = !in.hideSpot.empty() && (spot.empty() || in.hideSpot == spot);
        r.hid |= hidden;
        if (in.breathEnded && in.breathCaught) return "heard breathing";
        if (in.breathEnded && hidden) r.over = true;   // held out: "it's gone", the lights come back, you may come out
        if (c.params.value("noLight", false) && in.flashOn) return "flash on";   // rule 10: no torch
        if (hidden && in.phoneHeld && c.params.value("phoneAway", true)) return "phone out while hiding";
        if (r.t >= c.params.value("within", 8.0f) && !hidden) return r.hid ? "came out too early" : "did not hide";
        return "";
    };

    // stand still: moving more than "maxMove" m from where the window opened, or being heard in the breath
    // mini-game ("breath", started right away) breaks it (rule 12)
    e["still"] = [](const RuleCheck& c, RuleRun& r, const RuleInputs& in, float) -> std::string {
        if (in.breathEnded && in.breathCaught) return "heard breathing";
        if (in.breathEnded) r.over = true;   // the scream has died away
        return xzDistance(in.position, r.start) > c.params.value("maxMove", 0.2f) ? "moved" : "";
    };

    // a flag (dialogue answer or game state) has "value" (or any value, if omitted). Covers answer_choice
    // (rule 6: the van), interact (rule 11: the bag) and things events report (rule 7: the forest walk)
    e["flag"] = [](const RuleCheck& c, RuleRun&, const RuleInputs& in, float) -> std::string {
        std::string key = c.params.value("flag", ""), v = flagValue(in, key);
        if (v.empty()) return "";
        if (c.params.contains("value") && v != c.params.value("value", "")) return "";
        return key + " = " + v;
    };

    return e;
}

}  // namespace

bool RuleSystem::load(const std::string& path) {
    evaluators_ = makeEvaluators();
    std::ifstream in(path);
    json j = json::parse(in, nullptr, false, true);   // allow // comments
    if (j.is_discarded()) {
        std::fprintf(stderr, "rules: could not read %s\n", path.c_str());
        return false;
    }
    for (const json& r : j.value("rules", json::array())) {
        if (!r.contains("check")) continue;
        const json& k = r["check"];
        RuleCheck c;
        c.id = r.value("id", "");
        c.text = r.value("text", "");
        c.type = k.value("type", "");
        c.signal = k.value("signal", "");
        c.at = k.contains("at") ? parseTime(k.value("at", ""), -1) : -1;
        c.seconds = k.value("seconds", 0.0f);
        c.params = k;
        c.onViolate = k.value("onViolate", json::object());
        if (!evaluators_.count(c.type)) {
            std::fprintf(stderr, "rules: %s has unknown check type '%s' (ignored)\n", c.id.c_str(), c.type.c_str());
            continue;
        }
        checks_.push_back(std::move(c));
    }
    return true;
}

void RuleSystem::open(RuleCheck& c, const RuleInputs& in, const std::function<bool(const std::string&)>& signal,
                      bool forced) {
    if (!c.signal.empty() && !signal(c.signal) && !forced) {
        std::printf("rule %s: signal '%s' not built yet, skipped\n", c.id.c_str(), c.signal.c_str());
        c.state = RuleCheck::State::Skipped;
        return;
    }
    c.state = RuleCheck::State::Active;
    c.run = RuleRun{};
    c.run.start = in.position;
    c.run.wasHeld = in.phoneHeld;
    c.brokenThisWindow = false;
    opened_.push_back(c.id);
    std::printf("rule %s: window open\n", c.id.c_str());
}

void RuleSystem::update(float dt, const GameClock& clock, const RuleInputs& in,
                        const std::function<bool(const std::string&)>& signal) {
    for (RuleCheck& c : checks_) {
        bool forced = std::find(forced_.begin(), forced_.end(), c.id) != forced_.end();
        if (c.state == RuleCheck::State::Waiting || forced) {
            if (c.at < 0 && !forced) {
                c.state = RuleCheck::State::Active;   // all night
                c.run.start = in.position;
            } else if (forced) {
                open(c, in, signal, true);
            } else if (clock.minutes() >= c.at && !in.busy) {
                open(c, in, signal, false);           // due (possibly a little late, after another event)
            }
        }
        if (c.state != RuleCheck::State::Active) continue;

        c.run.t += dt;
        std::string reason = evaluators_[c.type](c, c.run, in, dt);
        if (!reason.empty() && !c.brokenThisWindow) {
            c.brokenThisWindow = true;
            ++c.broken;
            ++strikes_;
            violations_.push_back({c.id, reason, c.onViolate});
            std::printf("rule %s broken: %s (strikes %d)\n", c.id.c_str(), reason.c_str(), strikes_);
        } else if (reason.empty() && c.brokenThisWindow && c.params.value("repeat", false)) {
            c.brokenThisWindow = false;   // all-night rules that can be broken again once the player is back
            c.run.accum = 0;
        }
        if (c.at >= 0 && (c.run.over || (c.seconds > 0 && c.run.t >= c.seconds))) {
            c.state = RuleCheck::State::Done;
            if (!c.signal.empty()) ended_.push_back(c.signal);
            closed_.push_back(c.id);
        }
    }
    forced_.clear();
}

void RuleSystem::skipBefore(float minutes) {
    for (RuleCheck& c : checks_)
        if (c.at >= 0 && c.at < minutes && c.state == RuleCheck::State::Waiting) c.state = RuleCheck::State::Skipped;
}

bool RuleSystem::startNow(const std::string& id) {
    for (const RuleCheck& c : checks_)
        if (c.id == id) {
            forced_.push_back(id);
            return true;
        }
    return false;
}

bool RuleSystem::startNext() {
    for (const RuleCheck& c : checks_)
        if (c.at >= 0 && c.state == RuleCheck::State::Waiting) return startNow(c.id);
    return false;
}

std::string RuleSystem::breathWanted() {
    for (RuleCheck& c : checks_) {
        if (c.state != RuleCheck::State::Active || c.run.breathAsked || c.brokenThisWindow) continue;
        std::string level = c.params.value("breath", "");
        if (level.empty()) continue;
        // hiding rules start it once the player is hidden, standing-still rules right away
        if (c.type == "must_hide" && !c.run.hid) continue;
        c.run.breathAsked = true;
        return level;
    }
    return "";
}

std::vector<std::string> RuleSystem::debugLines() const {
    std::vector<std::string> out;
    char buf[160];
    std::snprintf(buf, sizeof buf, "RULES  strikes: %d   (J = open next window)", strikes_);
    out.push_back(buf);
    for (const RuleCheck& c : checks_) {
        const char* st = c.state == RuleCheck::State::Waiting ? "waiting"
                         : c.state == RuleCheck::State::Active ? "ACTIVE"
                         : c.state == RuleCheck::State::Skipped ? "skipped"
                                                                 : "done";
        int m = c.at < 0 ? -1 : (int)c.at;
        std::string when = m < 0 ? "all night" : (std::snprintf(buf, sizeof buf, "%02d:%02d", (m / 60) % 24, m % 60), std::string(buf));
        std::snprintf(buf, sizeof buf, "%-13s %-9s %-7s %-11s t%5.1f  n%4.1f  broken %d", c.id.c_str(), when.c_str(), st,
                      c.type.c_str(), c.run.t, c.run.accum, c.broken);
        out.push_back(buf);
    }
    return out;
}
