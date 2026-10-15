#pragma once
// The phone's message thread with the unknown sender (assets/data/rules.json).
// Plain messages and rules arrive at their time ("at"); opening the thread is one read of every rule
// in it, and a rule read more than `readLimit` times blurs to its `fuzzyText` (or is "deleted").
// The "airdrop" block is the companion's fake rules: at its time he offers to AirDrop them (dialogue `script`,
// answered with the actions "airdrop_accept" / "airdrop_decline"); accepted, they appear in the thread as
// photos of a note — they never blur.
#include <string>
#include <vector>
#include "GameClock.h"

struct Message {
    std::string id, text, fuzzyText;
    float at = 0;          // arrival time, minutes after midnight
    bool rule = false;
    bool airdrop = false;  // a photo AirDropped by `from` (the companion), not from the unknown sender
    std::string from;
    int readLimit = -1;    // rules only; -1 = can be read any number of times
    int reads = 0;
    bool arrived = false;
};

class Inbox {
public:
    bool load(const std::string& path);

    // Deliver what is due; returns how many messages just arrived
    int update(const GameClock& clock);
    // The player opened the thread: every rule that has arrived is read once more
    void open();

    // The companion's AirDrop: due (offer it: start airdropScript(), then call airdropOffered()), received
    bool airdropDue(const GameClock& clock) const { return !airdropScript_.empty() && !airdropOffered_ && clock.minutes() >= airdropAt_; }
    const std::string& airdropScript() const { return airdropScript_; }
    void airdropOffered() { airdropOffered_ = true; }
    // The player accepted: the photos arrive now. Returns how many.
    int receiveAirdrop(const GameClock& clock);

    // Arrived messages in order
    std::vector<const Message*> thread() const;
    int unread() const { return unread_; }
    // What the screen shows for a message right now (full, blurred or deleted)
    std::string shownText(const Message& m) const;
    // Bumps whenever the thread or a message's text changes (the screen redraws on change)
    int version() const { return version_; }

private:
    std::vector<Message> messages_;
    int unread_ = 0, version_ = 0;
    float airdropAt_ = 0;
    std::string airdropScript_;
    bool airdropOffered_ = false;
};
