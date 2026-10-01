#include "WindowProbe.hpp"
#include <algorithm>

using namespace geode::prelude;

namespace afc {

Probe WindowProbe::survives(PlayLayer* pl, ProbeJob const& job, int offset, int horizon, float stepDt) {
    if (!pl || !job.start || stepDt <= 0.f) return Probe::Failed;
    if (job.targetTick + offset < job.startTick) return Probe::Dies;   // before the snapshot: boundary
    Scope scope;
    s_died = false;

    // Build the shifted event list: the target press and its matching release move together.
    std::vector<InputEvent> ev = job.events;
    bool pressDone = false, releaseDone = false;
    for (auto& e : ev) {
        if (!pressDone && e.down && e.tick == job.targetTick && e.player == job.player && e.button == job.button) {
            e.tick += offset; pressDone = true; continue;
        }
        if (pressDone && !releaseDone && !e.down && e.tick >= job.targetTick &&
            e.player == job.player && e.button == job.button) {
            e.tick += offset; releaseDone = true;
        }
    }
    if (!pressDone) return Probe::Failed;
    std::stable_sort(ev.begin(), ev.end(), [](auto const& a, auto const& b) { return a.tick < b.tick; });

    pl->loadFromCheckpoint(job.start);

    int const end = job.targetTick + horizon;
    int prev = game::tick(pl);
    size_t ei = 0;
    for (int t = job.startTick; t < end; ++t) {
        while (ei < ev.size() && ev[ei].tick < t) ++ei;
        while (ei < ev.size() && ev[ei].tick == t) {
            pl->handleButton(ev[ei].down, ev[ei].button, ev[ei].player == 1);
            ++ei;
        }
        pl->processCommands(stepDt);
        int now = game::tick(pl);
        if (now != prev + 1) return Probe::Failed;   // one call must equal exactly one tick
        prev = now;
        if (s_died) return Probe::Dies;
    }
    return Probe::Survives;
}

WindowResult WindowProbe::measure(PlayLayer* pl, ProbeJob const& job, int range, int horizon, float stepDt) {
    return solveWindow(range, [&](int d) { return survives(pl, job, d, horizon, stepDt); });
}

Measurement WindowProbe::toMeasurement(ProbeJob const& job, WindowResult const& r, Method m) {
    Measurement out;
    out.levelID = job.levelID;
    out.inputIndex = job.inputIndex;
    out.tick = job.targetTick;
    out.player = job.player;
    out.button = job.button;
    out.method = m;
    out.kind = Kind::Window;
    out.validity = r.validity;
    out.exact = r.exact;
    out.capped = r.capped;
    out.earliest = r.earliest;
    out.latest = r.latest;
    out.note = r.note;
    return out;
}

} // namespace afc
