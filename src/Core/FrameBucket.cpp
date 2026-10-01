#include "FrameBucket.hpp"
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace afc {

Bucket classify(int n, bool* above15) {
    if (above15) *above15 = n > 15;
    if (n >= 10) return Bucket::B10_15;
    if (n >= 7) return Bucket::B7_9;
    if (n >= 4) return Bucket::B4_6;
    if (n == 3) return Bucket::B3;
    if (n == 2) return Bucket::B2;
    if (n == 1) return Bucket::B1;
    return Bucket::None;
}

const char* bucketLabel(Bucket b) {
    switch (b) {
        case Bucket::B10_15: return "10-15";
        case Bucket::B7_9: return "7-9";
        case Bucket::B4_6: return "4-6";
        case Bucket::B3: return "3";
        case Bucket::B2: return "2";
        case Bucket::B1: return "1";
        default: return "-";
    }
}

Bucket bucketFromIndex(int i) {
    return (i >= 0 && i < kBucketCount) ? static_cast<Bucket>(i) : Bucket::None;
}

std::string formatMeasurement(Measurement const& m, bool showExact, bool showBucket) {
    if (m.exact < 0 || m.validity == Validity::Unavailable ||
        m.validity == Validity::MethodLimitation ||
        m.validity == Validity::AnalysisRequired) {
        return validityName(m.validity);
    }
    Bucket b = classify(m.exact);
    std::string exact = std::to_string(m.exact) + (m.capped ? "+" : "");
    std::string bucket = bucketLabel(b);
    if (showExact && showBucket) {
        if (exact == bucket) return exact;
        return exact + " / " + bucket;
    }
    if (showExact) return exact;
    if (showBucket) return bucket;
    return "";
}

static bool parseInt(std::string const& s, int& out) {
    if (s.empty() || s.size() > 6) return false;
    for (char c : s) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    out = std::atoi(s.c_str());
    return true;
}

// Normalise the UTF-8 en dash (E2 80 93) to '-'.
static std::string normDash(std::string const& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i + 2 < s.size() && (unsigned char)s[i] == 0xE2 &&
            (unsigned char)s[i + 1] == 0x80 && (unsigned char)s[i + 2] == 0x93) {
            r += '-'; i += 2;
        } else r += s[i];
    }
    return r;
}

bool parseQuery(std::string const& text, Query& out, std::string* err) {
    out = Query{};
    std::istringstream ss(normDash(text));
    std::string tok;
    while (ss >> tok) {
        for (auto& c : tok) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        int a = 0, b = 0;
        if (tok == "p1" || tok == "p2") { out.player = tok[1] - '0'; continue; }
        if (tok == "m1" || tok == "m2" || tok == "m3") {
            out.method = static_cast<Method>(tok[1] - '0'); continue;
        }
        if (tok == "window") { out.kind = Kind::Window; continue; }
        if (tok == "gap" || tok == "interval") { out.kind = Kind::Interval; continue; }
        if (tok[0] == '#' && parseInt(tok.substr(1), a)) { out.inputIndex = a; continue; }
        auto dash = tok.find('-');
        if (dash != std::string::npos && parseInt(tok.substr(0, dash), a) &&
            parseInt(tok.substr(dash + 1), b) && a <= b) {
            out.lo = a; out.hi = b;
            if (a == 10 && b == 15) out.bucket = Bucket::B10_15;
            else if (a == 7 && b == 9) out.bucket = Bucket::B7_9;
            else if (a == 4 && b == 6) out.bucket = Bucket::B4_6;
            continue;
        }
        if (parseInt(tok, a)) { out.lo = out.hi = a; continue; }
        if (err) *err = "unknown token: " + tok;
        return false;
    }
    return true;
}

bool matches(Measurement const& m, Query const& q) {
    if (q.player && m.player != *q.player) return false;
    if (q.method && m.method != *q.method) return false;
    if (q.kind && m.kind != *q.kind) return false;
    if (q.inputIndex && m.inputIndex != *q.inputIndex) return false;
    if (q.lo) {
        if (m.exact < 0) return false;
        if (q.bucket) {
            if (classify(m.exact) != *q.bucket) return false;
        } else if (m.exact < *q.lo || m.exact > *q.hi) return false;
    }
    return true;
}

} // namespace afc
