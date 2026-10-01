#pragma once
// Shared probe core used by Methods 2 and 3. Uses Geometry Dash's OWN physics
// and collision (PlayLayer checkpoints + GJBaseGameLayer::processCommands); no
// hitbox approximation. EXPERIMENTAL: written against the Geode bindings but
// not compiled or run in the authoring environment.
#include "../Core/WindowSolver.hpp"
#include "../Platform/Game.hpp"
#include <vector>

namespace afc {

struct ProbeJob {
    geode::Ref<CheckpointObject> start;  // game state at the beginning of startTick
    int startTick = 0;
    int targetTick = 0;                  // tick of the real press being probed
    int inputIndex = 0;
    int player = 1;
    int button = 1;
    int levelID = 0;
    std::vector<InputEvent> events;      // real inputs in [startTick, targetTick + horizon]
};

class WindowProbe {
public:
    // True while WE are stepping/restoring the game. Hooks must then behave as
    // pass-through: no recording, no pre-roll, deaths suppressed.
    static bool probing() { return s_probing; }
    static void markDeath() { s_died = true; }
    static bool died() { return s_died; }
    static void clearDeath() { s_died = false; }

    struct Scope {   // RAII: sets probing for a synchronous block
        Scope() : prev(s_probing) { s_probing = true; }
        ~Scope() { s_probing = prev; }
        bool prev;
    };

    // Re-simulate from job.start with the target press shifted by `offset` ticks.
    static Probe survives(PlayLayer* pl, ProbeJob const& job, int offset, int horizon, float stepDt);

    // Full window for one job. Probe count is bounded by 2*range+1.
    static WindowResult measure(PlayLayer* pl, ProbeJob const& job, int range, int horizon, float stepDt);

    static Measurement toMeasurement(ProbeJob const& job, WindowResult const& r, Method m);

private:
    static inline bool s_probing = false;
    static inline bool s_died = false;
};

} // namespace afc
