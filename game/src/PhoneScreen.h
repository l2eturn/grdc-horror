#pragma once
// What the phone's screen shows, painted with Canvas2D into a texture that goes on the model's
// `phone_screen` mesh: the lock screen (clock, "no service", notifications) or the message thread.
// Repaints only when something on it changes.
#include <memory>
#include <string>
#include <ter/ter.h>
#include "Inbox.h"

struct ScreenInfo {
    enum class Page { Lock, Thread } page = Page::Lock;
    std::string time, date, carrier, sender;
    int battery = 100;      // percent
    float scroll = 0;       // thread: pixels scrolled up from the newest message
};

class PhoneScreen {
public:
    static constexpr int kWidth = 292, kHeight = 512;   // same aspect as the model's screen (51.6 x 90.3 mm)
    // The texture has kSuper x that many pixels: close to the face the screen covers ~900 px of a Retina
    // window, at 1x the Thai text was magnified and blurred
    static constexpr int kSuper = 2;

    PhoneScreen();
    const std::shared_ptr<ter::Texture>& texture() const { return texture_; }

    // Repaint if anything shown changed
    void update(const ScreenInfo& info, const Inbox& inbox);
    // Thread page: how far it can scroll (content taller than the view), from the last paint
    float maxScroll() const { return maxScroll_; }

private:
    void paintLock(const ScreenInfo& info, const Inbox& inbox);
    void paintThread(const ScreenInfo& info, const Inbox& inbox);
    void paintStatusBar(const ScreenInfo& info);

    ter::Canvas2D canvas_;
    std::shared_ptr<ter::Texture> texture_;
    std::string painted_;   // key of what is on the screen now
    float maxScroll_ = 0;
};
