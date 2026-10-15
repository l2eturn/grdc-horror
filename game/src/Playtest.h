#pragma once
// Scripted playtests (--play script.txt [--play-out dir]): the game plays itself from a text script — keys,
// walking, looking, jumping the clock, screenshots — and prints what happens ([say] lines, rule windows, broken
// rules), so a whole night can be checked without a person at the keyboard. Scripts live in game/playtests/.
//
// One command per line, '#' starts a comment. Each line starts with when it runs:
//   +2.5 cmd ...   2.5 s after the previous command finished      @30 cmd ...   30 s after the start
// Commands handled here:
//   key E              tap a key (letters, digits, Space, Tab, Up/Down/Left/Right, Grave)
//   hold W 1.5         hold a key for 1.5 s
//   look 90 -10        set the view: yaw degrees (0 = north, 90 = west), pitch degrees
//   lookat x y z       turn the view toward a point
//   goto x z           put the player there
//   walkto x z         walk there (holds W, steering); logs if stuck for 15 s. The next command waits for it
//   waitfor cond       hold the script until main.cpp says the condition is true (rule:<id>, asking, say:<text>,
//                      notice:<text>, quiet, bus, van, grandma, ending); gives up after 150 s (or `waitfor cond 8`)
//                      with a log line
// Everything else (clock, shot, rule, grandma, bus, van, log, quit, ...) is handed back to main.cpp.
#include <functional>
#include <string>
#include <vector>
#include <ter/ter.h>

struct PlayCommand {
    float at = 0;          // seconds (absolute once loaded)
    bool relative = false;
    std::string name;
    std::vector<std::string> args;
    int line = 0;
};

class Playtest {
public:
    bool load(const std::string& path);
    bool active() const { return loaded_; }
    // every frame before the game reads input: plays the commands that are due and returns the ones for main.cpp
    std::vector<PlayCommand> update(float dt, ter::Input& in, ter::FirstPersonCamera& player,
                                    const std::function<bool(const std::string&)>& condition);
    float time() const { return time_; }

private:
    std::vector<PlayCommand> commands_;
    size_t next_ = 0;
    float time_ = 0, last_ = 0;    // last_ = when the previous command finished
    bool loaded_ = false;
    // keys held down for a while: key, release time
    std::vector<std::pair<ter::Key, float>> holds_;
    // walking to a point
    bool walking_ = false;
    glm::vec2 walkTo_{0};
    float walkStart_ = 0;
    std::string waitFor_;
    float waitStart_ = 0, waitMax_ = 150;
};
