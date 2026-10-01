#include "MeasurementManager.hpp"

namespace afc {

uint64_t MeasurementManager::add(Measurement m) {
    m.id = m_nextId++;
    m_hist.push_back(std::move(m));
    trim();
    ++m_version;
    return m_hist.back().id;
}

void MeasurementManager::clear() { m_hist.clear(); ++m_version; }

void MeasurementManager::clearMethod(Method me) {
    for (auto it = m_hist.begin(); it != m_hist.end();) {
        if (it->method == me) it = m_hist.erase(it); else ++it;
    }
    ++m_version;
}

std::optional<Measurement> MeasurementManager::latest(Method me, int player) const {
    for (auto it = m_hist.rbegin(); it != m_hist.rend(); ++it)
        if (it->method == me && it->player == player) return *it;
    return std::nullopt;
}

std::vector<Measurement> MeasurementManager::search(Query const& q, size_t limit) const {
    std::vector<Measurement> out;
    for (auto it = m_hist.rbegin(); it != m_hist.rend() && out.size() < limit; ++it)
        if (matches(*it, q)) out.push_back(*it);
    return out;
}

std::array<int, kBucketCount> MeasurementManager::bucketCounts(Method me, int player) const {
    std::array<int, kBucketCount> c{};
    for (auto const& m : m_hist) {
        if (m.method != me || m.exact < 0) continue;
        if (m.validity != Validity::Valid && m.validity != Validity::Measured) continue;
        if (player && m.player != player) continue;
        Bucket b = classify(m.exact);
        if (b != Bucket::None) ++c[static_cast<int>(b)];
    }
    return c;
}

} // namespace afc
