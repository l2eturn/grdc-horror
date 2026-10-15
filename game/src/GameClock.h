#pragma once
// In-game time for the night: runs from `start` to `end` (22:00 -> 05:30 by default, assets/data/game.json),
// one in-game hour every `secondsPerHour` real seconds. The night crosses midnight, so times are minutes after
// the noon before: 22:00 = 1320, 01:10 = 1510 (see parseTime).
#include <string>

class GameClock {
public:
    float start = 22 * 60, end = 29 * 60 + 30, secondsPerHour = 80;

    void reset() { now_ = prev_ = start; }
    void update(float dt);
    // jump to a time (debug / checkpoints)
    void set(float minutes) { now_ = prev_ = minutes; }

    float minutes() const { return now_; }
    // "02:13"
    std::string text() const;
    // the clock passed `minutes` during the last update (fire once-per-night events)
    bool crossed(float minutes) const { return prev_ < minutes && now_ >= minutes; }
    bool finished() const { return now_ >= end; }

private:
    float now_ = 0, prev_ = 0;
};

// Night time: "22:00" -> 1320, "05:30" -> 1770 (00:00-11:59 counts as after midnight, so it adds 24 h).
// Returns `fallback` when the text isn't HH:MM.
float parseTime(const std::string& hhmm, float fallback = 0);
