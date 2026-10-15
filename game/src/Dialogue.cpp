#include "Dialogue.h"

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

std::map<std::string, std::string> flagsOf(const json& j) {
    std::map<std::string, std::string> out;
    const json set = j.value("set", json::object());   // keep it alive: items() only views it
    for (auto& [k, v] : set.items()) out[k] = v.is_string() ? v.get<std::string>() : v.dump();
    return out;
}

// seconds a line stays on screen: long enough to read Thai at a calm pace
float readTime(const std::string& text) {
    int chars = 0;
    for (unsigned char c : text) chars += (c & 0xC0) != 0x80;   // count UTF-8 characters, not bytes
    return std::max(2.0f, 1.2f + chars * 0.07f);
}

}  // namespace

bool Dialogue::load(const std::string& path) {
    std::ifstream in(path);
    json j = json::parse(in, nullptr, false, true);
    if (j.is_discarded()) {
        std::fprintf(stderr, "could not read %s\n", path.c_str());
        return false;
    }
    const json scripts = j.value("scripts", json::object());
    for (auto& [name, s] : scripts.items()) {
        DialogueScript script;
        script.start = s.value("start", "");
        const json nodes = s.value("nodes", json::object());
        for (auto& [id, n] : nodes.items()) {
            DialogueNode node;
            node.speaker = n.value("speaker", "");
            node.text = n.value("text", "");
            node.next = n.value("next", "");
            node.action = n.value("action", "");
            node.set = flagsOf(n);
            node.duration = n.value("duration", -1.0f);
            node.timeout = n.value("timeout", 0.0f);
            node.timeoutNext = n.value("timeoutNext", "");
            for (const json& c : n.value("choices", json::array()))
                node.choices.push_back({c.value("text", ""), c.value("next", ""), c.value("action", ""), flagsOf(c)});
            if (node.duration < 0) node.duration = readTime(node.text);
            script.nodes[id] = std::move(node);
        }
        scripts_[name] = std::move(script);
    }
    return true;
}

bool Dialogue::start(const std::string& name, bool force) {
    if (node_ && !node_->choices.empty() && !force) return false;   // the player is still answering something
    auto it = scripts_.find(name);
    if (it == scripts_.end()) {
        std::fprintf(stderr, "dialogue: no script '%s'\n", name.c_str());
        return false;
    }
    script_ = &it->second;
    enter(script_->start);
    return true;
}

void Dialogue::stop() {
    if (node_) changed_ = true;
    node_ = nullptr;
    script_ = nullptr;
}

void Dialogue::apply(const std::map<std::string, std::string>& set, const std::string& action) {
    for (auto& [k, v] : set) flags[k] = v;
    if (!action.empty()) actions_.push_back(action);
}

void Dialogue::enter(const std::string& id) {
    changed_ = true;
    time_ = 0;
    if (!script_ || id.empty() || !script_->nodes.count(id)) {
        if (!id.empty()) std::fprintf(stderr, "dialogue: no node '%s'\n", id.c_str());
        node_ = nullptr;
        return;
    }
    node_ = &script_->nodes.at(id);
    if (log) std::printf("[say] %s: %s\n", node_->speaker.c_str(), node_->text.c_str());
    apply(node_->set, node_->action);
}

void Dialogue::update(float dt, const ter::Input& in) {
    if (!node_) return;
    time_ += dt;
    if (!node_->choices.empty()) {
        // answer with 1..n
        for (size_t i = 0; i < node_->choices.size() && i < 9; ++i) {
            if (!in.pressed((ter::Key)((int)ter::Key::Num1 + (int)i))) continue;
            const DialogueChoice& c = node_->choices[i];
            apply(c.set, c.action);
            enter(c.next);
            return;
        }
        if (node_->timeout > 0 && time_ >= node_->timeout) enter(node_->timeoutNext);
        return;
    }
    // a line: Enter skips, otherwise it moves on by itself
    if (time_ >= node_->duration || in.pressed(ter::Key::Enter)) enter(node_->next);
}

void Dialogue::draw(ter::Canvas2D& g, float width, float height) const {
    if (!node_) return;
    const float boxW = std::min(width - 80.0f, 820.0f), x = (width - boxW) / 2;
    const int lines = 1 + (int)node_->choices.size();
    const float boxH = 34.0f + lines * 30.0f + (node_->speaker.empty() ? 0 : 26.0f);
    const float y = height - 90.0f - boxH;
    g.setShadow(0, ter::RGBA{0, 0, 0, 0}, 0, 0);
    g.setFill("rgba(8,9,12,0.72)");
    g.fillRect(x, y, boxW, boxH);
    g.setShadow(6, ter::cssColor("rgba(0,0,0,0.8)"), 0, 2);
    g.setTextAlign(ter::TextAlign::Left);
    float ty = y + 32;
    if (!node_->speaker.empty()) {
        g.setFont("Mali", 18, 600);
        g.setFill("rgba(232,190,120,0.95)");
        g.fillText(node_->speaker, x + 22, ty);
        ty += 26;
    }
    g.setFont("Mali", 22, 500);
    g.setFill("rgba(232,228,214,0.95)");
    g.fillText(node_->text, x + 22, ty);
    g.setFont("Mali", 20, 500);
    for (size_t i = 0; i < node_->choices.size(); ++i) {
        ty += 30;
        g.setFill("rgba(190,205,225,0.95)");
        g.fillText(std::to_string(i + 1) + ".  " + node_->choices[i].text, x + 40, ty);
    }
}
