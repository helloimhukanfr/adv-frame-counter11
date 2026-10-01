#include "ReplayProbeMethod.hpp"
#include "../Core/MeasurementManager.hpp"
#include <chrono>

using namespace geode::prelude;

namespace afc {

bool ReplayProbeMethod::begin(PlayLayer* pl, RecordedRun const& run, std::string& err) {
    if (m_active) { err = "analysis already running"; return false; }
    if (!pl) { err = "no level"; return false; }
    if (!run.validate(err)) return false;
    if (run.levelID != game::levelID(pl)) { err = "recorded run belongs to a different level"; return false; }
    if (run.pressCount() == 0) { err = "recorded run has no presses"; return false; }
    CheckpointObject* cp = pl->createCheckpoint();
    if (!cp) { err = "could not snapshot live state"; return false; }
    m_resume = cp;
    m_run = run;
    m_ei = 0; m_tick = 0; m_pressDone = 0; m_pressIndex = 0; m_aligned = false;
    m_presses = run.pressCount();
    m_ring.clear();
    m_result.clear();
    m_active = true;
    return true;
}

bool ReplayProbeMethod::step(PlayLayer* pl, double budgetMs, int range, int horizon, float stepDt) {
    if (!m_active) return true;
    auto t0 = std::chrono::steady_clock::now();
    WindowProbe::Scope scope;

    if (!m_aligned) {
        pl->resetLevel();                          // attempt hooks are pass-through while probing
        if (game::tick(pl) != 0) { m_result = "level reset did not return to tick 0"; abort(pl, true); return true; }
        m_aligned = true;
    }

    auto& ev = m_run.inputs;
    while (m_tick <= m_run.durationTicks) {
        // snapshot of the state at the start of this tick
        CheckpointObject* cp = pl->createCheckpoint();
        if (!cp) { m_result = "snapshot failed"; abort(pl, true); return true; }
        Snap head{m_tick, Ref<CheckpointObject>(cp)};
        m_ring.push_back(head);
        while (m_ring.size() > static_cast<size_t>(range) + 1) m_ring.pop_front();

        // probe every press that happens on this tick
        for (size_t k = m_ei; k < ev.size() && ev[k].tick == m_tick; ++k) {
            if (!ev[k].down) continue;
            ++m_pressIndex;
            ProbeJob j;
            j.start = m_ring.front().cp; j.startTick = m_ring.front().tick;
            j.targetTick = m_tick; j.inputIndex = m_pressIndex;
            j.player = ev[k].player; j.button = ev[k].button; j.levelID = m_run.levelID;
            for (auto const& e : ev)
                if (e.tick >= j.startTick && e.tick <= m_tick + horizon) j.events.push_back(e);
            WindowResult r = WindowProbe::measure(pl, j, range, horizon, stepDt);
            MeasurementManager::get().add(WindowProbe::toMeasurement(j, r, Method::ReplayProbe));
            ++m_pressDone;
            pl->loadFromCheckpoint(head.cp);       // back to the head of the replay
        }

        // advance the replay one tick with the recorded inputs
        while (m_ei < ev.size() && ev[m_ei].tick == m_tick) {
            pl->handleButton(ev[m_ei].down, ev[m_ei].button, ev[m_ei].player == 1);
            ++m_ei;
        }
        int before = game::tick(pl);
        WindowProbe::clearDeath();
        pl->processCommands(stepDt);
        if (WindowProbe::died()) { m_result = "replay died at tick " + std::to_string(m_tick) + " (desync)"; abort(pl, true); return true; }
        if (game::tick(pl) != before + 1) { m_result = "one step did not equal one tick"; abort(pl, true); return true; }
        ++m_tick;

        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if (ms >= budgetMs) return false;
    }
    m_result = "done";
    abort(pl, true);
    return true;
}

void ReplayProbeMethod::abort(PlayLayer* pl, bool restore) {
    if (m_active && restore && pl && m_resume) {
        WindowProbe::Scope scope;
        pl->loadFromCheckpoint(m_resume);
    }
    m_resume = nullptr;
    m_ring.clear();
    m_active = false;
}

} // namespace afc
