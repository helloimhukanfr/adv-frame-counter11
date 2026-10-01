#pragma once
// Geode-free core types. Everything in src/Core, src/Recording (data) and
// src/Storage is plain C++20 so it can be unit-tested off-device (tests/).
#include <cstdint>
#include <string>
#include <vector>

namespace afc {

// What the mod calls a "frame": one Geometry Dash physics tick, identified by
// GJBaseGameLayer::m_gameState.m_currentProgress. It is NOT a render frame.
enum class Method : int { DirectTick = 1, StateProbe = 2, ReplayProbe = 3 };

enum class Bucket : int { B10_15 = 0, B7_9, B4_6, B3, B2, B1, None };
constexpr int kBucketCount = 6;

enum class Validity { Valid, Measured, Unavailable, AnalysisRequired, MethodLimitation };

// Interval = ticks between two consecutive presses (Method 1).
// Window   = number of consecutive input ticks proven survivable (Methods 2/3).
enum class Kind { Interval, Window };

struct Measurement {
    uint64_t id = 0;
    int levelID = 0;
    int inputIndex = 0;   // 1-based index of the press within the attempt/run
    int tick = 0;         // tick at which the real input was processed
    int player = 1;       // 1 or 2
    int button = 1;
    Method method = Method::DirectTick;
    Kind kind = Kind::Interval;
    Validity validity = Validity::Unavailable;
    int exact = -1;       // measured number (window size or interval); -1 = none
    bool capped = false;  // true when exact is a lower bound (hit probe range)
    int earliest = 0;     // Window only: earliest survivable offset (<= 0)
    int latest = 0;       // Window only: latest survivable offset (>= 0)
    std::string note;
};

inline const char* methodName(Method m) {
    switch (m) {
        case Method::DirectTick: return "Method 1";
        case Method::StateProbe: return "Method 2";
        case Method::ReplayProbe: return "Method 3";
    }
    return "?";
}
inline const char* methodShort(Method m) {
    switch (m) {
        case Method::DirectTick: return "DIRECT";
        case Method::StateProbe: return "SIMULATION";
        case Method::ReplayProbe: return "RECORDED REPLAY";
    }
    return "?";
}
inline const char* validityName(Validity v) {
    switch (v) {
        case Validity::Valid: return "VALID";
        case Validity::Measured: return "MEASURED";
        case Validity::Unavailable: return "UNAVAILABLE";
        case Validity::AnalysisRequired: return "ANALYSIS REQUIRED";
        case Validity::MethodLimitation: return "METHOD LIMITATION";
    }
    return "?";
}

} // namespace afc
