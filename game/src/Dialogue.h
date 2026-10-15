#pragma once
// Scripted conversations (assets/data/dialogue.json): lines with a speaker, and questions the player answers
// with the number keys. Answers are stored as flags (for the rules: "answer_choice") and can trigger actions
// that the game reacts to (the bus driver's "leave", "player_board", ...). Format: game/EVENTS_FORMAT.md
//
//   Dialogue d;  d.load(path);
//   d.start("bus_driver");
//   d.update(dt, input);                       // every frame
//   for (auto& a : d.takeActions()) ...        // actions reached since the last call
//   d.draw(canvas, width, height);             // in app.onUi; call app.redrawUi() when d.changed()
#include <map>
#include <string>
#include <vector>
#include <ter/ter.h>

struct DialogueChoice {
    std::string text, next, action;
    std::map<std::string, std::string> set;   // flags written when this answer is picked
};

struct DialogueNode {
    std::string speaker, text, next, action;
    std::map<std::string, std::string> set;
    std::vector<DialogueChoice> choices;
    float duration = -1;       // seconds a line stays up (-1 = from its length)
    float timeout = 0;         // questions: seconds to answer (0 = wait forever)
    std::string timeoutNext;   // where silence leads
};

struct DialogueScript {
    std::string start;
    std::map<std::string, DialogueNode> nodes;
};

class Dialogue {
public:
    bool load(const std::string& path);
    // Starts a script. An open question (a line with choices) is not cut off: returns false unless `force`
    bool start(const std::string& script, bool force = false);
    void stop();
    void update(float dt, const ter::Input& in);

    bool active() const { return node_ != nullptr; }
    // a question is on screen, waiting for an answer
    bool asking() const { return node_ && !node_->choices.empty(); }
    // the line on screen right now ("" when none)
    const std::string& text() const {
        static const std::string none;
        return node_ ? node_->text : none;
    }
    // who is talking right now ("" when nobody)
    const std::string& speaker() const {
        static const std::string none;
        return node_ ? node_->speaker : none;
    }
    // the UI needs a redraw (new line, choice made)
    bool changed() { bool c = changed_; changed_ = false; return c; }
    std::vector<std::string> takeActions() { return std::move(actions_); }
    // answers and "set" values so far, e.g. flags["bus_answer"] == "board"
    std::map<std::string, std::string> flags;
    // print every line as it shows (scripted playtests, --play)
    bool log = false;

    void draw(ter::Canvas2D& g, float width, float height) const;

private:
    void enter(const std::string& id);
    void apply(const std::map<std::string, std::string>& set, const std::string& action);

    std::map<std::string, DialogueScript> scripts_;
    const DialogueScript* script_ = nullptr;
    const DialogueNode* node_ = nullptr;
    float time_ = 0;
    bool changed_ = false;
    std::vector<std::string> actions_;
};
