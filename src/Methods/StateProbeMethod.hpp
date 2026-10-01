#pragma once
#include "IFrameMethod.hpp"
#include "WindowProbe.hpp"
#include <deque>

namespace afc {

// METHOD 2: gameplay-state window analysis.
// While you play it keeps a short ring of real game-state snapshots (one per
// tick) and queues one job per press. While the game is PAUSED the jobs are
// analysed: each job restores the snapshot taken `range` ticks before the press,
// re-runs the game's own physics with the press shifted by -range..+range ticks
// and records which shifts survive `horizon` ticks. The contiguous surviving
// run around the real press is the window.
class StateProbeMethod final : public IFrameMethod {
public:
    Method id() const override { return Method::StateProbe; }
    const char* title() const override { return "Gameplay-state window analysis"; }
    const char* description() const override {
        return "Re-simulates the game's own physics from a snapshot taken before each "
               "press with the press moved earlier/later, to find how many ticks it could shift and still survive.";
    }
    const char* limitations() const override {
        return "EXPERIMENTAL. Analysis runs while paused. Held-button state comes from the game's "
               "checkpoint restore. Randomness/trigger side effects may differ. Needs 'Allow experimental probing'.";
    }
    void onInput(InputEvent const& e, int pressIndex, int levelID) override;
    void onAttemptReset() override { drop(); }
    void onDeactivate() override { drop(); }
    std::string status() const override;

    // called every real tick BEFORE the game processes it (not while probing)
    void preTick(PlayLayer* pl, int tick, int range);

    size_t pending() const { return m_jobs.size(); }
    size_t ready(int curTick, int horizon) const;

    // Batch analysis (paused game). Time-sliced; progress = jobs finished / jobs total.
    bool beginBatch(PlayLayer* pl, int curTick, int horizon, std::string& err);
    bool stepBatch(PlayLayer* pl, double budgetMs, int range, int horizon, float stepDt); // true when finished
    void abortBatch(PlayLayer* pl, bool restore);
    bool batchActive() const { return m_batchActive; }
    float fraction() const { return m_total ? static_cast<float>(m_done) / static_cast<float>(m_total) : 0.f; }
    int total() const { return m_total; }
    int done() const { return m_done; }

private:
    struct Snap { int tick; geode::Ref<CheckpointObject> cp; };
    struct Pending { Snap start; int targetTick, index, player, button, levelID; };
    void drop();

    std::deque<Snap> m_ring;
    std::deque<Pending> m_jobs;
    int m_dropped = 0;
    int m_lastTick = -1;
    int m_range = 15;

    bool m_batchActive = false;
    std::vector<ProbeJob> m_work;
    size_t m_idx = 0;
    int m_total = 0, m_done = 0;
    geode::Ref<CheckpointObject> m_resume;
};

} // namespace afc
