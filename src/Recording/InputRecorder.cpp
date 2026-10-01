#include "InputRecorder.hpp"

namespace afc {

void InputRecorder::newAttempt(int levelID, std::string name, int tps, bool platformer) {
    bool carryRecording =
        m_recording &&
        m_waitingForFirstInput &&
        m_run.inputs.empty();

    // If a recording already contains real input, this is a genuine
    // new attempt. Finalise it rather than corrupting its tick ordering.
    if (m_recording && !carryRecording) {
        stop(m_run.durationTicks);
    }

    m_ring.clear();
    m_ringBase = 0;
    m_pressIndex = 0;
    m_lastPress = {-1, -1};
    m_gap = {-1, -1};

    m_levelID = levelID;
    m_levelName = std::move(name);
    m_tps = tps;
    m_platformer = platformer;
    m_valid = true;

    m_run = RecordedRun{};
    m_run.levelID = m_levelID;
    m_run.levelName = m_levelName;
    m_run.tps = m_tps;
    m_run.platformer = m_platformer;

    m_hasRun = false;
    m_recording = carryRecording;
    m_waitingForFirstInput = carryRecording;
}

int InputRecorder::push(InputEvent const& e) {
    if (!m_valid || e.player < 1 || e.player > 2) return 0;

    // The recent ring always sees the real event.
    m_ring.push_back(e);
    while (m_ring.size() > kRingMax) {
        m_ringBase = m_ring.front().tick + 1;
        m_ring.pop_front();
    }

    if (m_recording) {
        // Some replay systems can emit a release while restarting.
        // Do not let a ghost release terminate the "armed" phase.
        if (!m_waitingForFirstInput || e.down) {
            if (m_waitingForFirstInput && e.down)
                m_waitingForFirstInput = false;

            m_run.inputs.push_back(e);
        }
    }

    if (!e.down) return 0;

    int p = e.player - 1;
    m_gap[p] =
        (m_lastPress[p] >= 0)
            ? e.tick - m_lastPress[p]
            : -1;

    m_lastPress[p] = e.tick;

    return ++m_pressIndex;
}

std::vector<InputEvent> InputRecorder::window(int from, int to) const {
    std::vector<InputEvent> r;
    for (auto const& e : m_ring) if (e.tick >= from && e.tick <= to) r.push_back(e);
    return r;
}

bool InputRecorder::start() {
    if (!m_valid) return false;

    m_run = RecordedRun{};
    m_run.levelID = m_levelID;
    m_run.levelName = m_levelName;
    m_run.tps = m_tps;
    m_run.platformer = m_platformer;

    m_hasRun = false;
    m_recording = true;
    m_waitingForFirstInput = true;
    return true;
}

bool InputRecorder::stop(int durationTicks) {
    if (!m_recording) return false;

    m_recording = false;
    m_waitingForFirstInput = false;

    m_run.tps = m_tps;

    int last =
        m_run.inputs.empty()
            ? 0
            : m_run.inputs.back().tick;

    m_run.durationTicks =
        durationTicks < last
            ? last
            : durationTicks;

    if (m_run.inputs.empty()) {
        m_hasRun = false;
        return false;
    }

    std::string err;
    m_hasRun = m_run.validate(err);
    return m_hasRun;
}

} // namespace afc
