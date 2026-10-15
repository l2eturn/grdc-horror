#pragma once
// Holding your breath while hiding (game-comcept.md §1.0.1), played like Audition: each round shows a row of
// arrows to type with the arrow keys, while a needle sweeps a bar in time with the heartbeat; Space lets the
// breath out when the needle crosses the mark.
//   right arrows + Space on the mark  -> PERFECT / GOOD, a quiet breath
//   a wrong arrow                     -> the row starts over, a loud gasp
//   Space off the mark / too early / never -> MISS, a loud breath
// Every loud breath fills the noise meter (it drains slowly); full = you are heard (caught()).
// Settings per difficulty come from assets/data/game.json "breath".
//
//   breath.start(settings);  breath.update(dt, in, audio);  if (breath.changed()) app.redrawUi();
//   breath.draw(canvas, w, h) in onUi;  breath.drawOverlay(app.overlay(), w, h) every frame (needle, meter)
#include <string>
#include <vector>
#include <ter/ter.h>

struct BreathSettings {
    int arrowsMin = 3, arrowsMax = 4;   // arrows per round
    float bpm = 64;                     // heartbeat; one round = 4 beats
    int rounds = 4;                     // rounds to get through
    float mark = 0.8f;                  // where on the bar (0..1) Space has to land
    float perfect = 0.035f, good = 0.09f;   // half-widths of the PERFECT / GOOD windows, in bar lengths
    float missNoise = 0.34f, gaspNoise = 0.2f, drain = 0.04f;   // noise per loud breath / wrong arrow, per second
};

class Breath {
public:
    void start(const BreathSettings& s);
    // scripted playtests (--play, "breathbot on"): types the arrows and lets go right on the mark by itself
    bool autoPlay = false;
    void stop();
    bool active() const { return active_; }
    void update(float dt, const ter::Input& in, ter::Audio& audio);

    // 0..1; at 1 whatever is out there heard you
    float noise() const { return noise_; }
    bool caught() const { return caught_; }
    // all rounds played without being caught
    bool passed() const { return !active_ && finished_ && !caught_; }
    // the canvas part changed (arrows typed, a judgement): redraw the UI
    bool changed() {
        bool c = changed_;
        changed_ = false;
        return c;
    }

    void draw(ter::Canvas2D& g, float width, float height) const;
    void drawOverlay(ter::Overlay& o, float width, float height) const;

private:
    enum Arrow { Left, Up, Right, Down };
    enum class Judge { None, Perfect, Good, Miss, Wrong };
    void newRound();
    void loud(ter::Audio& audio, float amount, bool gasp);

    BreathSettings s_;
    bool active_ = false, finished_ = false, caught_ = false, changed_ = false;
    std::vector<Arrow> arrows_;
    int typed_ = 0, round_ = 0;
    bool released_ = false;      // Space pressed this round
    float t_ = 0;                // seconds into the round
    float beat_ = 0;             // seconds to the next heartbeat
    float noise_ = 0;
    Judge judge_ = Judge::None;
    float judgeTime_ = 0;        // how long the last judgement has been up
    ter::Rng rng_{7};
};
