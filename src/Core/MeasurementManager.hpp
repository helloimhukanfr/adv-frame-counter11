#pragma once
#include "FrameBucket.hpp"
#include "Types.hpp"
#include <array>
#include <deque>
#include <optional>
#include <vector>

namespace afc {

// Bounded store of real measurements. Single-threaded (main thread only).
class MeasurementManager {
public:
    static MeasurementManager& get() { static MeasurementManager m; return m; }

    void setCapacity(size_t n) { m_cap = n < 8 ? 8 : (n > 5000 ? 5000 : n); trim(); }
    uint64_t add(Measurement m);
    void clear();
    void clearMethod(Method m);
    uint64_t version() const { return m_version; }   // bumps on every change (UI dirty flag)

    std::deque<Measurement> const& all() const { return m_hist; }
    std::optional<Measurement> latest(Method m, int player) const;
    std::vector<Measurement> search(Query const& q, size_t limit = 500) const; // newest first
    std::array<int, kBucketCount> bucketCounts(Method m, int player /*0 = both*/) const;

private:
    void trim() { while (m_hist.size() > m_cap) m_hist.pop_front(); }
    std::deque<Measurement> m_hist;
    size_t m_cap = 200;
    uint64_t m_nextId = 1, m_version = 0;
};

} // namespace afc
