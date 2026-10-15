#include "Breath.h"

#include <algorithm>
#include <cmath>

namespace {

// layout (window points): the row of arrows, the bar under it, the noise meter to the right
constexpr float kArrow = 46, kGap = 10, kBarW = 440, kBarH = 14, kMeterW = 12, kMeterH = 120;
float panelY(float height) { return height * 0.66f; }
float barTime(float bpm) { return 4 * 60.0f / bpm; }

const ter::Key kKeys[4] = {ter::Key::Left, ter::Key::Up, ter::Key::Right, ter::Key::Down};

// an arrow pointing left/up/right/down, centred at (x, y)
void arrowPath(ter::Canvas2D& g, int dir, float x, float y, float r) {
    const float a = dir * ter::PI / 2;   // 0 = left, then clockwise
    auto pt = [&](float px, float py) {   // shape drawn pointing left, rotated
        float c = std::cos(a), s = std::sin(a);
        return glm::vec2(x + px * c - py * s, y + px * s + py * c);
    };
    const glm::vec2 p[7] = {pt(-r, 0), pt(-0.1f * r, -0.75f * r), pt(-0.1f * r, -0.3f * r), pt(r, -0.3f * r),
                            pt(r, 0.3f * r), pt(-0.1f * r, 0.3f * r), pt(-0.1f * r, 0.75f * r)};
    g.beginPath();
    g.moveTo(p[0].x, p[0].y);
    for (int i = 1; i < 7; ++i) g.lineTo(p[i].x, p[i].y);
    g.closePath();
}

}  // namespace

void Breath::start(const BreathSettings& s) {
    s_ = s;
    active_ = true;
    finished_ = caught_ = false;
    noise_ = 0;
    round_ = 0;
    beat_ = 0;
    judge_ = Judge::None;
    newRound();
}

void Breath::stop() {
    active_ = false;
    changed_ = true;
}

void Breath::newRound() {
    t_ = 0;
    typed_ = 0;
    released_ = false;
    int n = s_.arrowsMin + (int)(rng_.f() * (s_.arrowsMax - s_.arrowsMin + 1));
    arrows_.clear();
    for (int i = 0; i < std::max(1, n); ++i) arrows_.push_back((Arrow)std::min(3, (int)(rng_.f() * 4)));
    changed_ = true;
}

void Breath::loud(ter::Audio& audio, float amount, bool gasp) {
    noise_ = std::min(1.0f, noise_ + amount);
    audio.play(gasp ? "breath_gasp" : "breath_loud", {.pitch = 0.9f + rng_.f() * 0.2f});
    if (noise_ >= 1) {
        caught_ = true;
        active_ = false;
    }
}

void Breath::update(float dt, const ter::Input& in, ter::Audio& audio) {
    if (!active_) return;
    const float bar = barTime(s_.bpm);
    t_ += dt;
    judgeTime_ += dt;
    noise_ = std::max(0.0f, noise_ - s_.drain * dt);

    // heartbeat: louder and faster-feeling as the noise rises
    if ((beat_ -= dt) <= 0) {
        beat_ += 60.0f / s_.bpm;
        audio.play("heart", {.volume = 0.6f + 0.6f * noise_});
    }

    // arrows: the next one in the row, anything else is a gasp and the row starts over
    if (!released_) {
        for (int k = 0; k < 4; ++k) {
            bool hit = autoPlay ? typed_ < (int)arrows_.size() && k == arrows_[typed_] : in.pressed(kKeys[k]);
            if (!hit) continue;
            if (typed_ < (int)arrows_.size() && k == arrows_[typed_]) {
                ++typed_;
            } else {
                typed_ = 0;
                judge_ = Judge::Wrong;
                judgeTime_ = 0;
                loud(audio, s_.gaspNoise, true);
            }
            changed_ = true;
            if (!active_) return;
        }
    }

    // Space: let the breath out on the mark
    bool space = autoPlay ? typed_ == (int)arrows_.size() && t_ / bar >= s_.mark - 0.005f : in.pressed(ter::Key::Space);
    if (!released_ && space) {
        released_ = true;
        float off = std::abs(t_ / bar - s_.mark);
        bool ready = typed_ == (int)arrows_.size();
        judge_ = !ready ? Judge::Miss : off <= s_.perfect ? Judge::Perfect : off <= s_.good ? Judge::Good : Judge::Miss;
        judgeTime_ = 0;
        changed_ = true;
        if (judge_ == Judge::Miss) loud(audio, s_.missNoise, false);
        else audio.play("breath_soft", {.volume = judge_ == Judge::Perfect ? 0.6f : 0.9f});
        if (!active_) return;
    }

    // end of the bar: held it too long without letting go = a loud breath; then the next round
    if (t_ >= bar) {
        if (!released_) {
            judge_ = Judge::Miss;
            judgeTime_ = 0;
            loud(audio, s_.missNoise, false);
            if (!active_) return;
        }
        if (++round_ >= s_.rounds) {
            finished_ = true;
            active_ = false;
            changed_ = true;
            return;
        }
        newRound();
    }
    if (judge_ != Judge::None && judgeTime_ > 0.8f) {
        judge_ = Judge::None;
        changed_ = true;
    }
}

void Breath::draw(ter::Canvas2D& g, float width, float height) const {
    if (!active_) return;
    const float y = panelY(height);
    g.clearShadow();

    // backing
    const float rowW = arrows_.size() * kArrow + (arrows_.size() - 1) * kGap;
    g.setFill("rgba(6,7,9,0.55)");
    g.beginPath();
    g.roundRect(width / 2 - kBarW / 2 - 24, y - kArrow / 2 - 46, kBarW + 48, kArrow + 110, 12);
    g.fill();

    g.setTextAlign(ter::TextAlign::Center);
    g.setFont("Mali", 15, 500);
    g.setFill("rgba(200,200,205,0.75)");
    g.fillText("กดลูกศรตาม แล้วกด Space ตรงจังหวะ  ·  รอบ " + std::to_string(std::min(round_ + 1, s_.rounds)) + "/" +
                   std::to_string(s_.rounds),
               width / 2, y - kArrow / 2 - 18);

    // the row of arrows: typed ones lit
    float x = width / 2 - rowW / 2 + kArrow / 2;
    for (size_t i = 0; i < arrows_.size(); ++i) {
        bool done = (int)i < typed_;
        g.setFill(done ? "rgba(150,210,255,0.95)" : "rgba(70,74,82,0.9)");
        g.beginPath();
        g.roundRect(x - kArrow / 2, y - kArrow / 2, kArrow, kArrow, 8);
        g.fill();
        g.setFill(done ? "#0b1520" : "rgba(230,230,235,0.95)");
        arrowPath(g, arrows_[i], x, y, kArrow * 0.32f);
        g.fill();
        x += kArrow + kGap;
    }

    // the bar: GOOD and PERFECT windows around the mark
    const float bx = width / 2 - kBarW / 2, by = y + kArrow / 2 + 22;
    g.setFill("rgba(40,42,48,0.95)");
    g.fillRect(bx, by, kBarW, kBarH);
    g.setFill("rgba(90,140,190,0.6)");
    g.fillRect(bx + (s_.mark - s_.good) * kBarW, by, 2 * s_.good * kBarW, kBarH);
    g.setFill("rgba(160,220,255,0.9)");
    g.fillRect(bx + (s_.mark - s_.perfect) * kBarW, by, 2 * s_.perfect * kBarW, kBarH);

    // judgement
    if (judge_ != Judge::None) {
        const char* text = judge_ == Judge::Perfect ? "PERFECT" : judge_ == Judge::Good ? "GOOD" : judge_ == Judge::Wrong ? "หายใจดัง!" : "MISS";
        bool bad = judge_ == Judge::Miss || judge_ == Judge::Wrong;
        g.setShadow(8, ter::cssColor("rgba(0,0,0,0.8)"), 0, 2);
        g.setFont("Mali", 26, 700);
        g.setFill(bad ? "rgba(240,90,80,0.95)" : "rgba(170,230,255,0.95)");
        g.fillText(text, width / 2, by + kBarH + 34);
        g.clearShadow();
    }
}

void Breath::drawOverlay(ter::Overlay& o, float width, float height) const {
    if (!active_) return;
    const float y = panelY(height), bx = width / 2 - kBarW / 2, by = y + kArrow / 2 + 22;
    // the needle sweeping the bar
    float p = std::clamp(t_ / barTime(s_.bpm), 0.0f, 1.0f);
    o.rect(bx + p * kBarW - 2, by - 6, 4, kBarH + 12, released_ ? glm::vec4(0.6f, 0.6f, 0.65f, 0.8f) : glm::vec4(1, 0.95f, 0.8f, 1));
    // noise meter, right of the panel: fills up red
    const float mx = width / 2 + kBarW / 2 + 44, my = y - kMeterH / 2 + 10;
    o.rect(mx, my, kMeterW, kMeterH, {0.1f, 0.1f, 0.12f, 0.8f}, 3);
    float h = kMeterH * noise_;
    o.rect(mx, my + kMeterH - h, kMeterW, h, {0.85f, 0.2f + 0.5f * (1 - noise_), 0.15f, 0.95f}, 3);
}
