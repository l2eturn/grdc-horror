#include "Inbox.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

bool Inbox::load(const std::string& path) {
    std::ifstream in(path);
    json j = json::parse(in, nullptr, false, true);   // allow // comments
    if (j.is_discarded()) {
        std::fprintf(stderr, "inbox: could not read %s\n", path.c_str());
        return false;
    }
    auto add = [&](const json& e, bool rule) {
        Message m;
        m.id = e.value("id", "");
        m.text = e.value("text", "");
        m.fuzzyText = e.value("fuzzyText", "");
        m.at = parseTime(e.value("at", "00:00"));
        m.rule = rule;
        m.readLimit = e.value("readLimit", -1);
        messages_.push_back(m);
    };
    for (auto& e : j.value("messages", json::array())) add(e, false);
    for (auto& e : j.value("rules", json::array())) add(e, true);
    if (j.contains("airdrop")) {
        const json& a = j["airdrop"];
        airdropAt_ = parseTime(a.value("at", "01:05"), 0);
        airdropScript_ = a.value("script", "");
        std::string from = a.value("from", "");
        for (auto& e : a.value("rules", json::array())) {
            add(e, true);
            messages_.back().airdrop = true;
            messages_.back().from = from;
            messages_.back().readLimit = -1;   // a photo does not fade
            messages_.back().at = 1e9f;        // arrives when accepted
        }
    }
    std::stable_sort(messages_.begin(), messages_.end(), [](const Message& a, const Message& b) { return a.at < b.at; });
    return true;
}

int Inbox::update(const GameClock& clock) {
    int n = 0;
    for (auto& m : messages_)
        if (!m.arrived && !m.airdrop && clock.minutes() >= m.at) {
            m.arrived = true;
            ++n;
        }
    if (n) {
        unread_ += n;
        ++version_;
    }
    return n;
}

int Inbox::receiveAirdrop(const GameClock& clock) {
    int n = 0;
    for (auto& m : messages_)
        if (m.airdrop && !m.arrived) {
            m.arrived = true;
            m.at = clock.minutes();
            ++n;
        }
    std::stable_sort(messages_.begin(), messages_.end(), [](const Message& a, const Message& b) { return a.at < b.at; });
    if (n) {
        unread_ += n;
        ++version_;
    }
    return n;
}

void Inbox::open() {
    for (auto& m : messages_)
        if (m.arrived && m.rule) ++m.reads;
    unread_ = 0;
    ++version_;
}

std::vector<const Message*> Inbox::thread() const {
    std::vector<const Message*> out;
    for (auto& m : messages_)
        if (m.arrived) out.push_back(&m);
    return out;
}

std::string Inbox::shownText(const Message& m) const {
    if (!m.rule || m.airdrop || m.readLimit < 0 || m.reads <= m.readLimit) return m.text;
    return m.fuzzyText.empty() ? "ข้อความนี้ถูกลบแล้ว" : m.fuzzyText;
}
