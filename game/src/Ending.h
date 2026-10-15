#pragma once
// The ending (game-comcept.md §1, "the twist"): after the player boards the 05:30 bus with the companion, a cut
// to inside the bus as it drives off. The player sits on the aisle seat, the companion by the window. He talks
// (dialogue script "ending" in assets/data/dialogue.json); its action "reveal" turns him to the player with his
// true face (assets/textures/npc_face_scary.png + glowing eyes, the cabin light dies), "end" cuts to black and
// the title. The player can only look around (mouse), not move; at the reveal the view is pulled to his face.
//
//   ending.load(world);                       // once, builds the hidden bus interior + companion copy
//   ending.start(dialogue);                   // when the player is on the final bus
//   ending.update(dt, in, cam, audio, lookFree);   // every frame while active(); owns the camera
//   ending.onAction(action, audio);           // actions from the dialogue
#include <memory>
#include <string>
#include <ter/ter.h>
#include "Animator.h"
#include "Dialogue.h"
#include "World.h"

class Ending {
public:
    bool load(World& world);
    void start(Dialogue& dialogue);
    bool active() const { return phase_ != Phase::Off; }
    void update(float dt, const ter::Input& in, ter::Camera& cam, ter::Audio& audio, bool lookFree);
    void onAction(const std::string& action, ter::Audio& audio);

    // black over the screen, 0..1 (fade in, the final cut); the title card paints its own black
    float black() const { return titleShown() ? 0 : black_; }
    // the title card is up (the UI draws it with drawTitle)
    bool titleShown() const { return phase_ == Phase::Title && t_ > 1.5f; }
    void drawTitle(ter::Canvas2D& g, float width, float height) const;

private:
    enum class Phase { Off, Ride, Reveal, Cut, Title };
    ter::ObjPtr bus_, friend_, cabinLight_, eyes_;
    std::shared_ptr<Animator> busAnim_, friendAnim_;
    ter::MatPtr faceMat_;
    std::shared_ptr<ter::Texture> scaryFace_;
    std::vector<ter::ObjPtr> wheels_;
    Phase phase_ = Phase::Off;
    float t_ = 0, x_ = 0, black_ = 0;
    float yaw_ = 0, pitch_ = 0;      // the player's look, relative to facing the front of the bus
    float turn_ = 0;                 // 0..1 the companion turning to the player
    bool endPending_ = false;        // "end" reached: cut once the last line has been read
    ter::SoundPtr hum_;
    Dialogue* dialogue_ = nullptr;
};
