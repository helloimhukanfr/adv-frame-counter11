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

    std::vector<Probe> outcomes(
        static_cast<size_t>(range * 2 + 1),
        Probe::Failed
    );

    auto indexOf = [range](int offset) {
        return offset + range;
    };

    // ALWAYS test the real click first.
    outcomes[indexOf(0)] = probe(0);
    ++r.probesRun;

    if (outcomes[indexOf(0)] == Probe::Failed) {
        r.validity = Validity::MethodLimitation;
        r.note = "base probe failed";
        r.probesSkipped = range * 2;
        return r;
    }

    if (outcomes[indexOf(0)] == Probe::Dies) {
        r.validity = Validity::MethodLimitation;
        r.note = "recorded click did not reproduce";
        r.probesSkipped = range * 2;
        return r;
    }

    // Test EVERY offset, even after one side has already died.
    // This is more expensive but gives a complete -15..+15 map.
    for (int d = -range; d <= range; ++d) {
        if (d == 0)
            continue;

        outcomes[indexOf(d)] = probe(d);
        ++r.probesRun;

        if (outcomes[indexOf(d)] == Probe::Failed) {
            r.validity = Validity::MethodLimitation;
            r.note =
                "probe failed at offset " +
                std::to_string(d);
            r.probesSkipped =
                range * 2 + 1 - r.probesRun;
            return r;
        }
    }

    int lo = 0;
    int hi = 0;

    // Find the first death on the negative side.
    for (int d = -1; d >= -range; --d) {
        if (outcomes[indexOf(d)] == Probe::Dies)
            break;

        if (outcomes[indexOf(d)] == Probe::Survives)
            lo = d;
    }

    // Find the first death on the positive side.
    for (int d = 1; d <= range; ++d) {
        if (outcomes[indexOf(d)] == Probe::Dies)
            break;

        if (outcomes[indexOf(d)] == Probe::Survives)
            hi = d;
    }

    // Detect weird non-contiguous survival. That is useful diagnostic
    // information instead of silently pretending the window is monotonic.
    bool nonContiguous = false;

    bool sawNegativeDeath = false;
    for (int d = -1; d >= -range; --d) {
        if (outcomes[indexOf(d)] == Probe::Dies)
            sawNegativeDeath = true;
        else if (sawNegativeDeath &&
                 outcomes[indexOf(d)] == Probe::Survives) {
            nonContiguous = true;
        }
    }

    bool sawPositiveDeath = false;
    for (int d = 1; d <= range; ++d) {
        if (outcomes[indexOf(d)] == Probe::Dies)
            sawPositiveDeath = true;
        else if (sawPositiveDeath &&
                 outcomes[indexOf(d)] == Probe::Survives) {
            nonContiguous = true;
        }
    }

    r.earliest = lo;
    r.latest = hi;
    r.exact = hi - lo + 1;

    r.capped =
        lo == -range ||
        hi == range;

    r.validity = Validity::Valid;
    r.probesSkipped =
        range * 2 + 1 - r.probesRun;

    r.note =
        "tested all offsets [" +
        std::to_string(-range) +
        "," +
        std::to_string(range) +
        "]";

    if (nonContiguous)
        r.note += "; non-contiguous survival detected";

    return r;
}

} // namespace afc
