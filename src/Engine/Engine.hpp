#pragma once
#include "../Methods/DirectTickMethod.hpp"
#include "../Methods/ReplayProbeMethod.hpp"
#include "../Methods/StateProbeMethod.hpp"
#include "../Storage/RunStorage.hpp"
#include <Geode/Geode.hpp>
#include <memory>

namespace afc {

struct ProgressInfo {
    bool active = false;
    float fraction = 0.f;        // REAL: finished work units / total work units
    std::string label;
};

enum class PlaybackState { Idle, Playing, Paused };

// Glue between hooks, methods, recorder and UI. Main thread only. No Cocos node
// is ever touched from a worker thread (there are none: analysis is time-sliced
// on the main thread, see README "Threading").
class Engine {
public:
    static Engine& get() { static Engine e; return e; }

    // ---- hook entry points ----
    void attemptStart(PlayLayer* pl);
    void attemptEnd(PlayLayer* pl, bool levelExit);   // death / complete / quit
    void tickPre(PlayLayer* pl, float dt);            // before the game processes a tick
    void input(PlayLayer* pl, bool down, int button, bool p1);
    void paused(PlayLayer* pl);
    void resumed(PlayLayer* pl);
    bool isPaused() const { return m_paused; }

    // ---- methods ----
    Method active() const { return cfg_method(); }
    void switchMethod(Method m);          // safe: stops workers, frees snapshots, keeps history/recording
    IFrameMethod& methodObj(Method m);
    bool experimentalAllowed() const;
    std::string statusLine() const;

    // ---- analysis (paused only) ----
    bool startAnalysis(std::string& err);
    void cancelAnalysis(bool restore);
    ProgressInfo progress() const;
    void pumpAnalysis();                  // called by the global scheduler

    // ---- recording ----
    bool startRecording(std::string& err);
    void stopRecording();
    bool saveRun(std::string& name, std::string& err);
    RunStorage storage() const;

    // ---- playback (opt-in; injects the recorded inputs) ----
    bool playbackStart(std::string& err);
    void playbackPause();
    void playbackStop();
    PlaybackState playback() const { return m_pb; }

    PlayLayer* playLayer() const { return m_pl; }
    float stepDt() const { return m_stepDt; }
    int currentTick() const { return m_tick; }

private:
    Engine();
    static Method cfg_method();
    void ensurePump();

    DirectTickMethod m_m1;
    StateProbeMethod m_m2;
    ReplayProbeMethod m_m3;
    Method m_lastMethod = Method::DirectTick;

    PlayLayer* m_pl = nullptr;
    bool m_paused = false, m_injecting = false, m_pumping = false;
    int m_tick = -1, m_prevTick = -1;
    float m_lastDt = 0.f, m_stepDt = 1.f / 240.f;
    bool m_stepKnown = false;

    PlaybackState m_pb = PlaybackState::Idle;
    bool m_pendingPlay = false;
    size_t m_pbIdx = 0;

    std::string m_analysisNote;
    cocos2d::CCObject* m_pump = nullptr;
};

} // namespace afc
