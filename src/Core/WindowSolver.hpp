#pragma once
// Pure window arithmetic shared by Methods 2 and 3 (Geode-free, unit-tested).
//
// The game-facing code supplies a callback survive(d) that answers: "if the
// real input at tick T had instead been processed at tick T+d, does the
// player stay alive for the whole probe horizon?" The solver finds the
// contiguous run of surviving offsets that contains d = 0.
#include "Types.hpp"
#include <functional>

namespace afc {

enum class Probe { Dies, Survives, Failed };   // Failed = probe itself could not run

struct WindowResult {
    Validity validity = Validity::Unavailable;
    int exact = -1;
    int earliest = 0, latest = 0;
    bool capped = false;          // hit +-range on at least one side
    int probesRun = 0;
    int probesSkipped = 0;        // offsets resolved without running (early exit)
    std::string note;
};

inline WindowResult solveWindow(int range, std::function<Probe(int)> const& probe) {
    WindowResult r;
    auto run = [&](int d) { ++r.probesRun; return probe(d); };
    Probe base = run(0);
    if (base == Probe::Failed) {
        r.validity = Validity::MethodLimitation;
        r.note = "probe could not run";
        r.probesSkipped = 2 * range;
        return r;
    }
    if (base == Probe::Dies) {
        // The real input did not survive inside the probe: the state restore or
        // stepping does not reproduce the live game. Report it, never guess.
        r.validity = Validity::MethodLimitation;
        r.note = "recorded input died in probe (state not reproduced)";
        r.probesSkipped = 2 * range;
        return r;
    }
    int lo = 0, hi = 0;
    bool cappedLo = true, cappedHi = true;
    for (int d = -1; d >= -range; --d) {
        Probe p = run(d);
        if (p == Probe::Failed) { r.validity = Validity::MethodLimitation; r.note = "probe failed"; return r; }
        if (p == Probe::Dies) { cappedLo = false; break; }
        lo = d;
    }
    for (int d = 1; d <= range; ++d) {
        Probe p = run(d);
        if (p == Probe::Failed) { r.validity = Validity::MethodLimitation; r.note = "probe failed"; return r; }
        if (p == Probe::Dies) { cappedHi = false; break; }
        hi = d;
    }
    r.earliest = lo; r.latest = hi;
    r.exact = hi - lo + 1;
    r.capped = cappedLo || cappedHi;
    r.validity = Validity::Valid;
    r.probesSkipped = (2 * range + 1) - r.probesRun;
    if (r.probesSkipped < 0) r.probesSkipped = 0;
    return r;
}

} // namespace afc
