#pragma once
#include "Types.hpp"
#include <optional>
#include <string>

namespace afc {

// Classify an actual measured count into one of the six required buckets.
// above15 (optional) is set when n > 15 (value still lands in 10-15).
Bucket classify(int n, bool* above15 = nullptr);
const char* bucketLabel(Bucket b);   // ASCII hyphen: GD's bigFont lacks an en dash
Bucket bucketFromIndex(int i);

// Display string for a measurement according to the user's display mode.
std::string formatMeasurement(Measurement const& m, bool showExact, bool showBucket);

// ---- search ---------------------------------------------------------------
struct Query {
    std::optional<int> lo, hi;        // exact range (inclusive)
    std::optional<Bucket> bucket;     // bucket filter
    std::optional<int> player;
    std::optional<Method> method;
    std::optional<int> inputIndex;
    std::optional<Kind> kind;
    bool empty() const {
        return !lo && !bucket && !player && !method && !inputIndex && !kind;
    }
};

// Parses text like "3", "4-6", "7<en dash>9", "p2", "m3", "#37", "window", "gap".
// Space separated tokens are ANDed. Returns false (with err) on unknown tokens.
bool parseQuery(std::string const& text, Query& out, std::string* err = nullptr);
bool matches(Measurement const& m, Query const& q);

} // namespace afc
