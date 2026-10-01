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

inline WindowResult solveWindow(
    int range,
    std::function<Probe(int)> const& probe
) {
    WindowResult r;

    range = std::max(1, std::min(15, range));

    auto run = [&](int d) {
        ++r.probesRun;
        return probe(d);
    };

    // The real recorded click must survive.
    Probe base = run(0);

    if (base == Probe::Failed) {
        r.validity = Validity::MethodLimitation;
        r.note = "base probe could not run";
        r.probesSkipped = 2 * range;
        return r;
    }

    if (base == Probe::Dies) {
        r.validity = Validity::MethodLimitation;
        r.note = "real recorded input did not reproduce in probe";
        r.probesSkipped = 2 * range;
        return r;
    }

    int lo = 0;
    int hi = 0;

    bool negativeOpen = true;
    bool positiveOpen = true;

    // Walk outward symmetrically.
    // This mirrors the way a frame window is actually searched:
    // test the adjacent frame first, then continue outward until
    // that side dies.
    for (int d = 1; d <= range; ++d) {

        if (negativeOpen) {
            Probe p = run(-d);

            if (p == Probe::Failed) {
                r.validity = Validity::MethodLimitation;
                r.note = "negative-side probe failed";
                return r;
            }

            if (p == Probe::Dies) {
                negativeOpen = false;
            }
            else {
                lo = -d;
            }
        }

        if (positiveOpen) {
            Probe p = run(d);

            if (p == Probe::Failed) {
                r.validity = Validity::MethodLimitation;
                r.note = "positive-side probe failed";
                return r;
            }

            if (p == Probe::Dies) {
                positiveOpen = false;
            }
            else {
                hi = d;
            }
        }

        if (!negativeOpen && !positiveOpen)
            break;
    }

    r.earliest = lo;
    r.latest = hi;

    // Number of integer frame positions in the inclusive window.
    r.exact = hi - lo + 1;

    r.capped =
        negativeOpen ||
        positiveOpen;

    r.validity = Validity::Valid;

    r.probesSkipped =
        (2 * range + 1) - r.probesRun;

    if (r.probesSkipped < 0)
        r.probesSkipped = 0;

    r.note =
        "tested integer offsets [" +
        std::to_string(lo) +
        "," +
        std::to_string(hi) +
        "]";

    return r;
}

} // namespace afc
