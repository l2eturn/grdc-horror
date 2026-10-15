#include "GameClock.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

void GameClock::update(float dt) {
    prev_ = now_;
    now_ = std::min(end, now_ + dt / secondsPerHour * 60.0f);
}

std::string GameClock::text() const {
    int m = (int)std::floor(now_);
    char buf[8];
    std::snprintf(buf, sizeof buf, "%02d:%02d", (m / 60) % 24, m % 60);
    return buf;
}

float parseTime(const std::string& hhmm, float fallback) {
    int h = 0, m = 0;
    if (std::sscanf(hhmm.c_str(), "%d:%d", &h, &m) != 2) return fallback;
    return (h < 12 ? h + 24 : h) * 60.0f + m;
}
