#include "PhoneScreen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

const float W = PhoneScreen::kWidth, H = PhoneScreen::kHeight;
const float kStatusH = 22;      // status bar
const float kHeaderH = 58;      // thread: sender name bar under the status bar
const float kInputH = 40;       // thread: the (disabled) text field at the bottom
const float kBubbleFont = 22, kBubbleLine = 29, kBubblePad = 11, kBubbleMaxW = 236;

// Canvas textures are uploaded as linear values but the colours below are picked as normal (sRGB) CSS
// colours: convert them, or every dark tone shows up washed-out grey on the screen
ter::RGBA L(ter::RGBA c) {
    auto f = [](float v) { return std::pow(v, 2.2f); };
    return {f(c.r), f(c.g), f(c.b), c.a};
}
ter::RGBA L(const std::string& css) { return L(ter::cssColor(css)); }

// One line, cut at a word (Thai-aware) with an ellipsis if it doesn't fit
std::string firstLine(ter::Canvas2D& c, const std::string& text, float maxW) {
    auto lines = c.wrapText(text, maxW - c.measureText("…"));
    if (lines.empty()) return "";
    return lines.size() == 1 && c.measureText(text) <= maxW ? text : lines[0] + "…";
}

}  // namespace

PhoneScreen::PhoneScreen() : canvas_(kWidth * kSuper, kHeight * kSuper), texture_(std::make_shared<ter::Texture>()) {}

void PhoneScreen::update(const ScreenInfo& info, const Inbox& inbox) {
    std::string key = std::to_string((int)info.page) + "|" + info.time + "|" + std::to_string(info.battery) + "|" +
                      std::to_string(inbox.version()) + "|" + std::to_string((int)info.scroll);
    if (key == painted_) return;
    painted_ = key;

    ter::Canvas2D& c = canvas_;
    c.save();
    c.clear();
    c.scale(kSuper, kSuper);   // lay out in screen units, paint at kSuper x the resolution
    if (info.page == ScreenInfo::Page::Lock) paintLock(info, inbox);
    else paintThread(info, inbox);
    paintStatusBar(info);
    c.restore();
    c.upload(*texture_);
}

void PhoneScreen::paintStatusBar(const ScreenInfo& info) {
    ter::Canvas2D& c = canvas_;
    c.clearShadow();
    c.setTextBaseline(ter::TextBaseline::Middle);
    c.setFill(L("rgba(235,235,240,0.92)"));
    c.setFont("Mali", 14, 500);
    c.setTextAlign(ter::TextAlign::Left);
    c.fillText(info.carrier, 8, kStatusH / 2);
    // battery: percentage + outline with a level bar (red when low)
    float bx = W - 34, by = 6, bw = 24, bh = 11;
    c.setTextAlign(ter::TextAlign::Right);
    c.fillText(std::to_string(info.battery) + "%", bx - 5, kStatusH / 2);
    c.setStroke(L("rgba(235,235,240,0.85)"));
    c.setLineWidth(1);
    c.strokeRect(bx + 0.5f, by + 0.5f, bw, bh);
    c.fillRect(bx + bw + 1, by + 3.5f, 2, 5);
    c.setFill(L(info.battery <= 20 ? "#e5483d" : "rgba(235,235,240,0.92)"));
    c.fillRect(bx + 2, by + 2, (bw - 3) * std::clamp(info.battery, 0, 100) / 100.0f, bh - 3);
    c.setTextBaseline(ter::TextBaseline::Alphabetic);
}

void PhoneScreen::paintLock(const ScreenInfo& info, const Inbox& inbox) {
    ter::Canvas2D& c = canvas_;
    // wallpaper: night-blue gradient with a soft glow low in the middle (an old photo of the mountains at dusk)
    c.beginPath();
    c.rect(0, 0, W, H);
    c.fillLinearGradient(0, 0, 0, H, {{0, L("#0b0f16")}, {0.6f, L("#172030")}, {1, L("#2a2a33")}});
    c.beginPath();
    c.rect(0, 0, W, H);
    c.fillRadialGradient(W * 0.5f, H * 0.95f, 10, W * 0.5f, H * 0.95f, H * 0.6f,
                         {{0, L(ter::RGBA{0.55f, 0.38f, 0.3f, 0.35f})}, {1, L(ter::RGBA{0.2f, 0.15f, 0.2f, 0})}});
    // mountain ridges
    c.setFill(L("rgba(5,7,10,0.85)"));
    c.beginPath();
    c.moveTo(0, H * 0.78f);
    c.lineTo(W * 0.22f, H * 0.7f);
    c.lineTo(W * 0.4f, H * 0.75f);
    c.lineTo(W * 0.63f, H * 0.66f);
    c.lineTo(W * 0.85f, H * 0.74f);
    c.lineTo(W, H * 0.71f);
    c.lineTo(W, H);
    c.lineTo(0, H);
    c.closePath();
    c.fill();

    c.setShadow(8, ter::RGBA{0, 0, 0, 0.6f}, 0, 1);
    c.setFill(L("rgba(240,240,245,0.95)"));
    c.setTextAlign(ter::TextAlign::Center);
    c.setFont("Inter", 78, 300);
    c.fillText(info.time, W / 2, 118);
    c.setFont("Mali", 15, 500);
    c.fillText(info.date, W / 2, 146);

    // notifications: the newest unread messages, a one-line preview each (open the thread to read them)
    auto thread = inbox.thread();
    int unread = std::min<int>(inbox.unread(), (int)thread.size());
    int shown = std::min(unread, 3);
    float y = 176;
    for (int i = 0; i < shown; ++i) {
        const Message& m = *thread[thread.size() - 1 - i];
        c.clearShadow();
        c.setFill(L("rgba(38,42,50,0.82)"));
        c.beginPath();
        c.roundRect(10, y, W - 20, 62, 12);
        c.fill();
        c.setTextAlign(ter::TextAlign::Left);
        c.setFont("Mali", 11, 500);
        c.setFill(L("rgba(200,205,215,0.7)"));
        c.fillText(m.airdrop ? "AirDrop" : "ข้อความ", 22, y + 18);
        c.setTextAlign(ter::TextAlign::Right);
        c.fillText(i == 0 ? "ตอนนี้" : "", W - 22, y + 18);
        c.setTextAlign(ter::TextAlign::Left);
        c.setFont("Mali", 14, 700);
        c.setFill(L("rgba(240,240,245,0.95)"));
        c.fillText(m.airdrop ? m.from : info.sender, 22, y + 36);
        c.setFont("Mali", 13, 400);
        c.setFill(L("rgba(225,228,235,0.85)"));
        c.fillText(firstLine(c, inbox.shownText(m), W - 44), 22, y + 53);
        y += 70;
    }
    if (unread > shown) {
        c.setTextAlign(ter::TextAlign::Center);
        c.setFont("Mali", 12, 500);
        c.setFill(L("rgba(220,225,235,0.75)"));
        c.fillText("อีก " + std::to_string(unread - shown) + " ข้อความ", W / 2, y + 10);
    }

    c.clearShadow();
    c.setTextAlign(ter::TextAlign::Center);
    c.setFont("Mali", 14, 400);
    c.setFill(L("rgba(230,230,235,0.55)"));
    c.fillText("›  เลื่อนเพื่อปลดล็อก", W / 2, H - 24);
}

void PhoneScreen::paintThread(const ScreenInfo& info, const Inbox& inbox) {
    ter::Canvas2D& c = canvas_;
    c.setFill(L("#07080a"));
    c.fillRect(0, 0, W, H);

    // lay out the bubbles from the top, then show the bottom part (newest) shifted by `scroll`
    struct Bubble {
        std::vector<std::string> lines;
        float w, h;
        bool dim;            // blurred or deleted rule
        bool photo;          // AirDropped photo of a note (paper card, dark text)
        std::string stamp;   // time label above it ("" = none)
    };
    c.setFont("Mali", kBubbleFont, 400);
    std::vector<Bubble> bubbles;
    float content = 12, lastAt = -999;
    bool lastPhoto = false;
    for (const Message* m : inbox.thread()) {
        Bubble b;
        std::string text = inbox.shownText(*m);
        b.dim = text != m->text;
        b.photo = m->airdrop;
        b.lines = c.wrapText(text, kBubbleMaxW);
        b.w = 0;
        for (auto& l : b.lines) b.w = std::max(b.w, c.measureText(l));
        b.w += kBubblePad * 2;
        b.h = b.lines.size() * kBubbleLine + kBubblePad * 2 - 4;
        if (m->at - lastAt >= 30 || (b.photo && !lastPhoto)) {
            int t = (int)m->at;
            char buf[8];
            std::snprintf(buf, sizeof buf, "%02d:%02d", (t / 60) % 24, t % 60);
            b.stamp = b.photo ? "AirDrop จาก " + m->from + " · " + buf : buf;
            content += 24;
        }
        lastPhoto = b.photo;
        lastAt = m->at;
        content += b.h + 6;
        bubbles.push_back(b);
    }
    float top = kStatusH + kHeaderH, bottom = H - kInputH, view = bottom - top;
    maxScroll_ = std::max(0.0f, content + 8 - view);
    float scroll = std::clamp(info.scroll, 0.0f, maxScroll_);
    float y = top + (content + 8 > view ? view - (content + 8) + scroll : 0) + 12;

    c.save();
    c.beginPath();
    c.rect(0, top, W, view);
    c.clip();
    for (auto& b : bubbles) {
        if (!b.stamp.empty()) {
            c.setFont("Mali", 14, 500);
            c.setTextAlign(ter::TextAlign::Center);
            c.setFill(L("rgba(150,155,165,0.8)"));
            c.fillText(b.stamp, W / 2, y + 12);
            y += 24;
        }
        c.setFill(L(b.photo ? "#ddd6c4" : b.dim ? "rgba(40,42,46,0.6)" : "#26282d"));
        c.beginPath();
        c.roundRect(8, y, b.w, b.h, b.photo ? 4 : 14);
        c.fill();
        c.setFont("Mali", kBubbleFont, 400);
        c.setTextAlign(ter::TextAlign::Left);
        c.setFill(L(b.photo ? "#2b2620" : b.dim ? "rgba(160,160,165,0.6)" : "#e8e8ea"));
        for (size_t i = 0; i < b.lines.size(); ++i) c.fillText(b.lines[i], 8 + kBubblePad, y + kBubblePad + 18 + i * kBubbleLine);
        y += b.h + 6;
    }
    c.restore();

    // header: back arrow, the sender's grey avatar and name
    c.setFill(L("#121418"));
    c.fillRect(0, kStatusH, W, kHeaderH);
    c.setFill(L("#2c2f35"));
    c.fillRect(0, kStatusH + kHeaderH - 1, W, 1);
    c.setFill(L("#5b616b"));
    c.beginPath();
    c.arc(W / 2, kStatusH + 19, 13, 0, ter::PI * 2);
    c.fill();
    c.setFill(L("#8a9099"));
    c.setFont("Mali", 16, 700);
    c.setTextAlign(ter::TextAlign::Center);
    c.fillText("?", W / 2, kStatusH + 25);
    c.setFont("Mali", 15, 600);
    c.setFill(L("rgba(230,232,236,0.9)"));
    c.fillText(info.sender, W / 2, kStatusH + 49);
    c.setTextAlign(ter::TextAlign::Left);
    c.setFont("Mali", 24, 400);
    c.setFill(L("#4a8cf0"));
    c.fillText("‹", 10, kStatusH + 30);

    // input field: there is no signal, nothing can be sent back
    c.setFill(L("#121418"));
    c.fillRect(0, bottom, W, kInputH);
    c.setStroke(L("#2f3238"));
    c.setLineWidth(1);
    c.beginPath();
    c.roundRect(10.5f, bottom + 7.5f, W - 21, kInputH - 15, 12);
    c.stroke();
    c.setFont("Mali", 14, 400);
    c.setFill(L("rgba(150,150,158,0.7)"));
    c.fillText("ไม่สามารถส่งข้อความได้", 22, bottom + 25);
}
