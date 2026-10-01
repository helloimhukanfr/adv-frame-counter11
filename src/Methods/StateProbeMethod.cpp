#include "StateProbeMethod.hpp"
#include "../Core/MeasurementManager.hpp"
#include "../Recording/InputRecorder.hpp"
#include <algorithm>
#include <chrono>

using namespace geode::prelude;

namespace afc {

void StateProbeMethod::drop() {
    m_ring.clear();
    m_jobs.clear();
    m_work.clear();
    m_resume = nullptr;
    m_batchActive = false;
    m_idx = 0; m_total = 0; m_done = 0; m_dropped = 0; m_lastTick = -1;
}

void StateProbeMethod::preTick(PlayLayer* pl, int tick, int range) {
    if (!pl || WindowProbe::probing()) return;
    m_range = range;
    m_lastTick = tick;
    if (!m_ring.empty() && m_ring.back().tick >= tick) m_ring.clear();   // tick went backwards: new attempt
    CheckpointObject* cp = pl->createCheckpoint();
    if (!cp) return;
    m_ring.push_back({tick, Ref<CheckpointObject>(cp)});
    while (m_ring.size() > static_cast<size_t>(range) + 1) m_ring.pop_front();
}

void StateProbeMethod::onInput(InputEvent const& e, int pressIndex, int levelID) {
    if (!e.down || pressIndex <= 0) return;
    int want = std::max(0, e.tick - m_range);
    Snap* found = nullptr;
    for (auto& s : m_ring) if (s.tick == want) { found = &s; break; }
    if (!found) {
        Measurement m;
        m.levelID = levelID; m.inputIndex = pressIndex; m.tick = e.tick; m.player = e.player;
        m.button = e.button; m.method = Method::StateProbe; m.kind = Kind::Window;
        m.validity = Validity::Unavailable;
        m.note = "no pre-roll snapshot (method was just enabled?)";
        MeasurementManager::get().add(std::move(m));
        return;
    }
    if (m_jobs.size() >= 24) { m_jobs.pop_front(); ++m_dropped; }   // bounded
    m_jobs.push_back({*found, e.tick, pressIndex, e.player, e.button, levelID});
}

size_t StateProbeMethod::ready(int curTick, int horizon) const {
    size_t n = 0;
    for (auto const& j : m_jobs) if (curTick >= j.targetTick + horizon) ++n;
    return n;
}

std::string StateProbeMethod::status() const {
    std::string s = "METHOD 2 SIMULATION  pending " + std::to_string(m_jobs.size());
    if (m_dropped) s += "  dropped " + std::to_string(m_dropped);
    return s;
}

bool StateProbeMethod::beginBatch(PlayLayer* pl, int curTick, int horizon, std::string& err) {
    if (m_batchActive) { err = "analysis already running"; return false; }
    if (!pl) { err = "no level"; return false; }
    m_work.clear();
    auto& rec = InputRecorder::get();
    std::deque<Pending> keep;
    for (auto& p : m_jobs) {
        if (curTick < p.targetTick + horizon) { keep.push_back(p); continue; }   // not ready yet
        if (!rec.ringCovers(p.start.tick)) continue;                           // events evicted
        ProbeJob j;
        j.start = p.start.cp; j.startTick = p.start.tick; j.targetTick = p.targetTick;
        j.inputIndex = p.index; j.player = p.player; j.button = p.button; j.levelID = p.levelID;
        j.events = rec.window(p.start.tick, p.targetTick + horizon);
        m_work.push_back(std::move(j));
    }
    m_jobs = std::move(keep);
    if (m_work.empty()) { err = "no inputs are ready yet (need the probe horizon to elapse after a press)"; return false; }
    CheckpointObject* cp = pl->createCheckpoint();
    if (!cp) { err = "could not snapshot live state"; m_work.clear(); return false; }
    m_resume = cp;
    m_idx = 0; m_done = 0; m_total = static_cast<int>(m_work.size());
    m_batchActive = true;
    return true;
}

bool StateProbeMethod::stepBatch(PlayLayer* pl, double budgetMs, int range, int horizon, float stepDt) {
    if (!m_batchActive) return true;
    auto t0 = std::chrono::steady_clock::now();
    WindowProbe::Scope scope;
    while (m_idx < m_work.size()) {
        auto& j = m_work[m_idx];
        WindowResult r = WindowProbe::measure(pl, j, range, horizon, stepDt);
        MeasurementManager::get().add(WindowProbe::toMeasurement(j, r, Method::StateProbe));
        ++m_idx; ++m_done;
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if (ms >= budgetMs) break;
    }
    if (m_idx >= m_work.size()) { abortBatch(pl, true); return true; }
    return false;
}

void StateProbeMethod::abortBatch(PlayLayer* pl, bool restore) {
    if (m_batchActive && restore && pl && m_resume) {
        WindowProbe::Scope scope;
        pl->loadFromCheckpoint(m_resume);
    }
    m_resume = nullptr;
    m_work.clear();
    m_batchActive = false;
}

} // namespace afc
