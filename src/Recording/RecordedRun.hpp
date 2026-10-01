#pragma once
#include "../Core/MiniJson.hpp"
#include <string>
#include <vector>

namespace afc {

// Real input as processed by GJBaseGameLayer::handleButton.
struct InputEvent {
    int tick = 0;       // gameState.m_currentProgress when the input was processed
    int player = 1;     // 1 or 2
    int button = 1;     // 1 = jump/primary; platformer: 2 = left, 3 = right
    bool down = true;   // true = press, false = release
    float x = 0, y = 0; // player position when processed (informational)
    int mode = 0;       // 0 cube,1 ship,2 ball,3 ufo,4 wave,5 robot,6 spider,7 swing
    bool dual = false;
};

inline const char* modeName(int m) {
    static const char* n[] = {"Cube", "Ship", "Ball", "UFO", "Wave", "Robot", "Spider", "Swing"};
    return (m >= 0 && m < 8) ? n[m] : "?";
}

// Deterministic: identity of an input is its tick, never wall-clock time.
struct RecordedRun {
    static constexpr int kVersion = 1;
    int version = kVersion;
    int levelID = 0;
    std::string levelName;
    int tps = 240;                 // physics ticks per second observed (1 / step dt)
    int durationTicks = 0;
    bool platformer = false;
    std::string timingConvention = "gd-physics-tick:GJBaseGameLayer::m_gameState.m_currentProgress";
    std::string recordedWithMethod = "observe";
    std::string source = "observed-real-input";
    std::vector<InputEvent> inputs;

    int pressCount() const {
        int n = 0; for (auto const& e : inputs) if (e.down) ++n; return n;
    }

    json::Value toJson() const;
    static bool fromJson(json::Value const& v, RecordedRun& out, std::string& err);
    // Structural validation (ordering, ranges). Returns false + reason if invalid.
    bool validate(std::string& err) const;
};

} // namespace afc
