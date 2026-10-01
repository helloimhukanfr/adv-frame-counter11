#pragma once
#include "IFrameMethod.hpp"
#include "WindowProbe.hpp"
#include <deque>

namespace afc {

// METHOD 3: recorded-input replay analysis.
// Takes a RecordedRun (what the game actually processed while you played or an
// external macro ran), resets the level, replays the recorded inputs tick by
// tick with the game's own physics and, at every recorded press, runs the same
// shift-and-survive probe as Method 2. Unlike Method 2 it covers the WHOLE run
// deterministically, independent of pre-roll snapshots taken while playing.
class ReplayProbeMethod final : public IFrameMethod {
public:
    Method id() const override { return Method::ReplayProbe; }
    const char* title() const override { return "Recorded-input replay analysis"; }
    const char* description() const override {
        return "Replays a recorded run from the level start and probes every recorded press "
               "with the game's own physics. Results exist only after ANALYZE completes.";
    }
    const char* limitations() const override {
        return "EXPERIMENTAL. Needs a recorded run for the same level. Replay must reproduce the original "
               "attempt (random triggers or desync => METHOD LIMITATION). Runs while paused.";
    }
    void onInput(InputEvent const&, int, int) override {}   // results come only from ANALYZE
    void onAttemptReset() override {}
    void onDeactivate() override { m_active = false; m_ring.clear(); m_resume = nullptr; m_run = RecordedRun{}; }
    std::string status() const override { return m_active ? "METHOD 3 RECORDED REPLAY  analyzing" : "METHOD 3 RECORDED REPLAY"; }

    bool begin(PlayLayer* pl, RecordedRun const& run, std::string& err);
    bool step(PlayLayer* pl, double budgetMs, int range, int horizon, float stepDt);   // true when finished
    void abort(PlayLayer* pl, bool restore);
    bool active() const { return m_active; }
    float fraction() const { return m_presses ? static_cast<float>(m_pressDone) / static_cast<float>(m_presses) : 0.f; }
    std::string const& result() const { return m_result; }

private:
    struct Snap { int tick; geode::Ref<CheckpointObject> cp; };
    bool m_active = false, m_aligned = false;
    RecordedRun m_run;
    size_t m_ei = 0;                 // next event index
    int m_tick = 0, m_presses = 0, m_pressDone = 0, m_pressIndex = 0;
    std::deque<Snap> m_ring;
    geode::Ref<CheckpointObject> m_resume;
    std::string m_result;
};

} // namespace afc
