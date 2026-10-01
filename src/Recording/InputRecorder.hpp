#pragma once
#include "RecordedRun.hpp"
#include <array>
#include <deque>
#include <string>

namespace afc {

// Observes real inputs (already processed by the game). Never injects anything.
//  * a bounded ring of recent events is ALWAYS kept for the current attempt
//    (Methods 1 and 2 need recent context),
//  * an explicit RECORD session additionally stores the full run.
class InputRecorder {
public:
    static InputRecorder& get() { static InputRecorder r; return r; }

    // Called for every real input. Returns the 1-based press index (0 for releases).
    int push(InputEvent const& e);

    // Interval since the previous press by the same player (-1 when none).
    int ticksSincePreviousPress(int player) const { return m_gap[player - 1]; }

    void newAttempt(int levelID, std::string name, int tps, bool platformer);
    void setTps(int tps) { m_tps = tps; }
    int tps() const { return m_tps; }

    // Events with tick in [from, to] (from the ring).
    std::vector<InputEvent> window(int from, int to) const;
    bool ringCovers(int tick) const { return m_ringBase <= tick; }

    // ---- explicit recording ----
    bool recording() const { return m_recording; }
    bool start();                      // begins capturing; false if no level
    bool stop(int durationTicks);      // finalises m_run; true if run is valid
    bool hasRun() const { return m_hasRun; }
    RecordedRun& run() { return m_run; }
    RecordedRun const& run() const { return m_run; }
    void setRun(RecordedRun r) { m_run = std::move(r); m_hasRun = true; }
    void clearRun() { m_run = RecordedRun{}; m_hasRun = false; }

    int levelID() const { return m_levelID; }
    int pressesThisAttempt() const { return m_pressIndex; }
    void shutdownAttempt() { m_valid = false; m_recording = false; m_ring.clear(); }
    bool attemptValid() const { return m_valid; }

private:
    static constexpr size_t kRingMax = 4096;
    std::deque<InputEvent> m_ring;
    int m_ringBase = 0;
    int m_pressIndex = 0;
    std::array<int, 2> m_lastPress{-1, -1};
    std::array<int, 2> m_gap{-1, -1};
    int m_levelID = 0, m_tps = 240;
    std::string m_levelName;
    bool m_platformer = false, m_valid = false;
    bool m_recording = false, m_hasRun = false;
    RecordedRun m_run;
};

} // namespace afc
