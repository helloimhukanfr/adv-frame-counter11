#include "Engine.hpp"
#include "../Core/MeasurementManager.hpp"
#include "../Platform/Game.hpp"
#include "../Platform/Settings.hpp"
#include "../Recording/InputRecorder.hpp"
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace afc {

// Scheduler target that drives time-sliced analysis even while the game is paused.
class PumpTarget : public CCObject {
public:
    void tick(float) { Engine::get().pumpAnalysis(); }
};

Engine::Engine() = default;

Method Engine::cfg_method() { return cfg::method(); }

bool Engine::experimentalAllowed() const { return cfg::b("allow-experimental"); }

IFrameMethod& Engine::methodObj(Method m) {
    switch (m) {
        case Method::StateProbe: return m_m2;
        case Method::ReplayProbe: return m_m3;
        default: return m_m1;
    }
}

void Engine::ensurePump() {
    if (m_pump) return;
    auto* t = new PumpTarget();
    t->autorelease();
    t->retain();
    m_pump = t;
    CCDirector::get()->getScheduler()->scheduleSelector(
        schedule_selector(PumpTarget::tick), t, 0.f, false);
}

// ------------------------------------------------------------------ lifecycle

void Engine::attemptStart(PlayLayer* pl) {
    if (WindowProbe::probing()) return;    // our own resetLevel() during Method 3
    cancelAnalysis(false);
    m_pl = pl;
    m_paused = false;
    m_tick = m_prevTick = -1;
    m_m2.onAttemptReset();
    m_m3.onAttemptReset();
    MeasurementManager::get().setCapacity(static_cast<size_t>(std::max(8, cfg::i("history-size"))));
    int tps = static_cast<int>(std::lround(1.f / m_stepDt));
    InputRecorder::get().newAttempt(game::levelID(pl), game::levelName(pl), tps, game::platformer(pl));
    m_lastMethod = cfg_method();
    ensurePump();

    if (m_pendingPlay) {                      // playback requested via resetLevel()
        m_pendingPlay = false;
        m_pb = PlaybackState::Playing;
        m_pbIdx = 0;
    } else if (m_pb != PlaybackState::Idle) {
        m_pb = PlaybackState::Idle;           // natural respawn cancels playback
    }
    if (cfg::b("record-auto") && cfg::enabled() && m_pb == PlaybackState::Idle) {
        std::string err; startRecording(err);   // never clobber the run that is being played back
    }
}

void Engine::attemptEnd(PlayLayer* pl, bool levelExit) {
    if (WindowProbe::probing()) return;
    cancelAnalysis(false);
    if (InputRecorder::get().recording()) {
        stopRecording();
        // rec-auto-restart: a new recording begins with the next attempt (attemptStart)
        if (!cfg::b("rec-auto-restart") && !levelExit) { /* stays stopped */ }
    }
    playbackStop();
    m_m2.onAttemptReset();
    if (levelExit) {
        InputRecorder::get().shutdownAttempt();
        m_pl = nullptr;
    }
}

void Engine::tickPre(PlayLayer* pl, float dt) {
    if (WindowProbe::probing() || !pl || !cfg::enabled()) return;
    m_pl = pl;
    if (game::practice(pl) && !cfg::b("practice-support")) return;
    int t = game::tick(pl);
    // Measure the real physics step: dt of the previous call divided by ticks it advanced.
    if (m_prevTick >= 0 && t > m_prevTick && m_lastDt > 0.f) {
        m_stepDt = m_lastDt / static_cast<float>(t - m_prevTick);
        m_stepKnown = true;
        InputRecorder::get().setTps(static_cast<int>(std::lround(1.f / m_stepDt)));
    }
    m_prevTick = t; m_lastDt = dt; m_tick = t;

    Method m = cfg_method();
    if (m != m_lastMethod) switchMethod(m);

    bool compare = cfg::mode() == cfg::Mode::Compare;
    if ((m == Method::StateProbe || compare) && experimentalAllowed())
        m_m2.preTick(pl, t, std::min(15, std::max(1, cfg::i("probe-range"))));

    if (m_pb == PlaybackState::Playing && cfg::b("playback-enabled")) {
        auto const& ev = InputRecorder::get().run().inputs;
        m_injecting = true;
        while (m_pbIdx < ev.size() && ev[m_pbIdx].tick <= t) {
            if (ev[m_pbIdx].tick == t) pl->handleButton(ev[m_pbIdx].down, ev[m_pbIdx].button, ev[m_pbIdx].player == 1);
            ++m_pbIdx;
        }
        m_injecting = false;
        if (m_pbIdx >= ev.size()) m_pb = PlaybackState::Idle;
    }
}

void Engine::input(PlayLayer* pl, bool down, int button, bool p1) {
    if (WindowProbe::probing() || m_injecting || !pl || !cfg::enabled()) return;
    if (game::practice(pl) && !cfg::b("practice-support")) return;
    InputEvent e = game::makeEvent(pl, down, button, p1);
    int idx = InputRecorder::get().push(e);       // observation only: nothing is injected or altered
    int lvl = game::levelID(pl);
    Method m = cfg_method();
    bool compare = cfg::mode() == cfg::Mode::Compare;
    if (m == Method::DirectTick || compare) m_m1.onInput(e, idx, lvl);
    if ((m == Method::StateProbe || compare) && experimentalAllowed()) m_m2.onInput(e, idx, lvl);
}

void Engine::paused(PlayLayer* pl) {
    m_paused = true;
    m_pl = pl;
    Method m = cfg_method();
    if (cfg::b("probe-auto-pause") && m == Method::StateProbe && experimentalAllowed() && !cfg::b("playback-enabled")) {
        std::string err; startAnalysis(err);
    }
}

void Engine::resumed(PlayLayer* pl) {
    m_paused = false;
    cancelAnalysis(true);   // restores the live state before the game continues
    (void)pl;
}

// ------------------------------------------------------------------- methods

void Engine::switchMethod(Method m) {
    if (m != m_lastMethod) {
        cancelAnalysis(true);
        methodObj(m_lastMethod).onDeactivate();   // frees snapshots/jobs of the old method only
    }
    m_lastMethod = m;
    if (cfg_method() != m) cfg::setMethod(m);
    MeasurementManager::get().setCapacity(static_cast<size_t>(std::max(8, cfg::i("history-size"))));
}

std::string Engine::statusLine() const {
    Method m = cfg_method();
    if ((m == Method::StateProbe || m == Method::ReplayProbe) && !experimentalAllowed())
        return "METHOD LIMITATION: enable 'Allow experimental probing'";
    return const_cast<Engine*>(this)->methodObj(m).status();
}

// ------------------------------------------------------------------ analysis

bool Engine::startAnalysis(std::string& err) {
    if (!m_pl) { err = "no level"; return false; }
    if (!m_paused) { err = "analysis runs only while the game is paused"; return false; }
    if (cfg::b("playback-enabled") && m_pb != PlaybackState::Idle) { err = "stop playback first"; return false; }
    Method m = cfg_method();
    int horizon = std::max(5, cfg::i("probe-horizon"));
    auto& rec = InputRecorder::get();
    if (m == Method::DirectTick) {
        // Method 1 on a recorded run: pure data pass, no simulation.
        if (!rec.hasRun()) { err = "no recorded run"; return false; }
        auto const& run = rec.run();
        int idx = 0; int last[2] = {-1, -1};
        for (auto const& e : run.inputs) {
            if (!e.down) continue;
            Measurement x; x.levelID = run.levelID; x.inputIndex = ++idx; x.tick = e.tick;
            x.player = e.player; x.button = e.button; x.method = Method::DirectTick; x.kind = Kind::Interval;
            int g = last[e.player - 1] >= 0 ? e.tick - last[e.player - 1] : -1;
            if (g > 0) { x.exact = g; x.validity = Validity::Measured; }
            else { x.validity = Validity::Unavailable; x.note = "first press"; }
            last[e.player - 1] = e.tick;
            MeasurementManager::get().add(std::move(x));
        }
        m_analysisNote = "Method 1 analysis done (" + std::to_string(idx) + " presses)";
        return true;
    }
    if (!experimentalAllowed()) { err = "enable 'Allow experimental probing' first"; return false; }
    if (m == Method::StateProbe) return m_m2.beginBatch(m_pl, m_tick, horizon, err);
    if (!rec.hasRun()) { err = "no recorded run (RECORD, play, STOP first)"; return false; }
    return m_m3.begin(m_pl, rec.run(), err);
}

void Engine::cancelAnalysis(bool restore) {
    if (m_m2.batchActive()) m_m2.abortBatch(m_pl, restore);
    if (m_m3.active()) m_m3.abort(m_pl, restore);
}

void Engine::pumpAnalysis() {
    if (m_pumping || !m_pl) return;
    if (!m_m2.batchActive() && !m_m3.active()) return;
    if (!m_paused) { cancelAnalysis(true); return; }   // never probe while the live game runs
    m_pumping = true;
    double budget = std::max(1.f, cfg::f("analysis-budget-ms"));
    int range = std::min(15, std::max(1, cfg::i("probe-range")));
    int horizon = std::max(5, cfg::i("probe-horizon"));
    if (m_m2.batchActive()) m_m2.stepBatch(m_pl, budget, range, horizon, m_stepDt * 1.001f);
    else if (m_m3.active()) m_m3.step(m_pl, budget, range, horizon, m_stepDt * 1.001f);
    m_pumping = false;
}

ProgressInfo Engine::progress() const {
    ProgressInfo p;
    if (m_m2.batchActive()) {
        p.active = true; p.fraction = m_m2.fraction();
        p.label = "Analyzing " + std::to_string(m_m2.done()) + "/" + std::to_string(m_m2.total());
    } else if (m_m3.active()) {
        p.active = true; p.fraction = m_m3.fraction();
        p.label = "Replay analysis " + std::to_string(static_cast<int>(p.fraction * 100.f)) + "%";
    } else if (!m_m3.result().empty()) {
        p.label = "Replay: " + m_m3.result();
    } else p.label = m_analysisNote;
    return p;
}

// ----------------------------------------------------------------- recording

bool Engine::startRecording(std::string& err) {
    if (!InputRecorder::get().start()) { err = "no active level attempt"; return false; }
    return true;
}

void Engine::stopRecording() {
    InputRecorder::get().stop(m_tick < 0 ? 0 : m_tick);
}

RunStorage Engine::storage() const { return RunStorage(Mod::get()->getSaveDir()); }

bool Engine::saveRun(std::string& name, std::string& err) {
    auto& rec = InputRecorder::get();
    if (!rec.hasRun()) { err = "nothing recorded"; return false; }
    return storage().save(rec.run(), name, err);
}

// ------------------------------------------------------------------ playback

bool Engine::playbackStart(std::string& err) {
    if (!cfg::b("playback-enabled")) { err = "playback is disabled in settings (it injects inputs)"; return false; }
    auto& rec = InputRecorder::get();
    if (!rec.hasRun()) { err = "no recorded run"; return false; }
    if (!m_pl) { err = "no level"; return false; }
    if (rec.run().levelID != game::levelID(m_pl)) { err = "run belongs to a different level"; return false; }
    if (m_pb == PlaybackState::Paused) { m_pb = PlaybackState::Playing; return true; }
    m_pendingPlay = true;
    m_pl->resetLevel();               // attemptStart() then enters Playing at tick 0
    if (m_pendingPlay) { m_pendingPlay = false; err = "level did not restart"; return false; }
    return true;
}

void Engine::playbackPause() { if (m_pb == PlaybackState::Playing) m_pb = PlaybackState::Paused; }
void Engine::playbackStop() { m_pb = PlaybackState::Idle; m_pendingPlay = false; m_pbIdx = 0; }

} // namespace afc
