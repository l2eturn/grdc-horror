#pragma once
// First-person phone: the player's only light and where the rules arrive (game-comcept.md §3.3).
// It starts in the pocket. Tab takes it out: held upright in the right hand, lower right of the view,
// screen toward the player (lock screen: clock, "no service", notifications). E / click brings it up close
// to read the message thread. F switches the camera flash on the back, used as a torch.
// The battery drains with the screen and the flash; when it is empty the phone is dark and useless.
//  - model: assets/models/phone.glb (source modelResource/phone_source.blend); the flash LED is the
//    `phone_flash` mesh, the light comes out of the empty `phone_flash_light`, the screen is `phone_screen`
//  - it lags a little behind mouse movement (sway), bobs with walking, pulls back near walls
// The walk cycle it keeps also drives the camera head-bob and the footstep timing (see main.cpp).
// Rules about holding things / light can check held(), reading() and lightOn().
#include <vector>
#include <ter/ter.h>
#include "GameClock.h"
#include "Inbox.h"
#include "PhoneScreen.h"

struct Battery {
    float level = 100;                    // percent
    float idlePerMinute = 0.1f;           // drain per real minute: always
    float screenPerMinute = 0.8f;         // + while the screen is on (phone in hand)
    float flashPerMinute = 2.0f;          // + while the flash shines
};

class Phone {
public:
    // Loads the model, builds the hand, adds both and the phone's lights to `scene`
    explicit Phone(ter::Object3D& scene);
    bool ok() const { return model_ != nullptr; }

    Battery battery;
    std::string sender = "?", date, carrier = "ไม่มีบริการ";   // what the screens show (assets/data/game.json)

    // Take the phone out / put it back in the pocket (animated; putting it away also closes the thread)
    void toggleHeld();
    // In the hand (true from the moment it is being raised until it is put away)
    bool held() const { return held_; }

    // Bring it up close and show the message thread / lower it back to the lock screen.
    // Returns false when it can't be read (not in hand or the battery is empty).
    bool setReading(bool on);
    bool reading() const { return reading_; }
    // Scroll the thread (positive = toward older messages)
    void scroll(float pixels);

    // Flash switch (torch); remembered while the phone is away
    void toggleFlash() { flash_ = !flash_; }
    bool flashSwitch() const { return flash_; }
    // The flash is actually shining: switched on, in the hand, battery not empty
    bool lightOn() const { return flash_ && held_ && !dead(); }
    bool screenOn() const { return held_ && !dead(); }
    bool dead() const { return battery.level <= 0; }

    // Put it away at once, no animation (cut scenes)
    void stow();

    // Vibrate (a message arrived): the phone shakes in the hand for a moment
    void buzz() { buzz_ = 0.6f; }

    // Every frame after the player moved. `walked` = meters moved this frame.
    void update(float dt, const ter::FirstPersonCamera& player, float walked, const std::vector<ter::Box>& colliders,
                const GameClock& clock, const Inbox& inbox);

    // Walk cycle: one full cycle = two steps. Increases with distance walked.
    float walkPhase() const { return phase_; }
    // Camera offset for head-bob this frame (add to the camera position)
    glm::vec3 headBob() const { return bob_; }

private:
    void buildHand();

    ter::ObjPtr model_, light_, glow_;
    ter::Object3D* flashPoint_ = nullptr;   // where the beam starts (phone_flash_light)
    ter::MatPtr led_, screenMat_;
    PhoneScreen screen_;
    bool held_ = false, reading_ = false, flash_ = true, started_ = false;
    float raise_ = 0, read_ = 0;            // 0..1: pocket -> hand, hand -> close to the face
    float scroll_ = 0, buzz_ = 0;
    float swayYaw_ = 0, swayPitch_ = 0;     // where the hand points (lags behind the view)
    float phase_ = 0, moving_ = 0, pull_ = 0, time_ = 0;
    glm::vec3 bob_{0};
};
