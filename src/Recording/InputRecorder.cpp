#include "InputRecorder.hpp"

namespace afc {

void InputRecorder::newAttempt(int levelID, std::string name, int tps, bool platformer) {
    // RECORD is a session, not a pause-menu state.
    //
    // XDBot and other replay systems commonly reset the PlayLayer when
    // playback begins. If RECORD was armed before that reset, the session
    // must survive it and wait for the first genuine macro input.
    bool carryRecording =
        m_recording &&
        m_run.inputs.empty();

    if (m_recording && !carryRecording) {
        // A new attempt after real recorded data means the previous
        // recording is already complete. Keep it rather than destroying it.
        stop(m_run.durationTicks);
        carryRecording = false;
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

    if (carryRecording) {
        // Preserve the armed session and create fresh attempt metadata.
        m_run = RecordedRun{};
        m_run.levelID = m_levelID;
        m_run.levelName = m_levelName;
        m_run.tps = m_tps;
        m_run.platformer = m_platformer;

        m_hasRun = false;
        m_recording = true;
        m_waitingForFirstInput = true;
        return;
    }

    m_run = RecordedRun{};
    m_run.levelID = m_levelID;
    m_run.levelName = m_levelName;
    m_run.tps = m_tps;
    m_run.platformer = m_platformer;

    m_hasRun = false;
    m_recording = false;
    m_waitingForFirstInput = false;
}

int InputRecorder::push(InputEvent const& e) {
    if (!m_valid || e.player < 1 || e.player > 2)
        return 0;

    // Keep the rolling context independently of explicit recording.
    if (m_ring.empty() ||
        m_ring.back().tick != e.tick ||
        m_ring.back().player != e.player ||
        m_ring.back().button != e.button ||
        m_ring.back().down != e.down) {

        m_ring.push_back(e);

        while (m_ring.size() > kRingMax) {
            m_ringBase = m_ring.front().tick + 1;
            m_ring.pop_front();
        }
    }

    if (m_recording) {
        // Some macro implementations can make the exact same event visible
        // through more than one hooked path. Collapse only consecutive
        // identical events at the same tick.
        bool duplicate =
            !m_run.inputs.empty() &&
            m_run.inputs.back().tick == e.tick &&
            m_run.inputs.back().player == e.player &&
            m_run.inputs.back().button == e.button &&
            m_run.inputs.back().down == e.down;

        if (!duplicate) {
            if (m_waitingForFirstInput && !e.down)
                return 0;

            if (m_waitingForFirstInput && e.down)
                m_waitingForFirstInput = false;

            m_run.inputs.push_back(e);
        }
    }

    if (!e.down)
        return 0;

    int p = e.player - 1;

    m_gap[p] =
        m_lastPress[p] >= 0
            ? e.tick - m_lastPress[p]
            : -1;

    m_lastPress[p] = e.tick;

    if (!m_recording)
        return ++m_pressIndex;

    return ++m_pressIndex;
}

std::vector<InputEvent> InputRecorder::window(int from, int to) const {
    std::vector<InputEvent> r;
    for (auto const& e : m_ring) if (e.tick >= from && e.tick <= to) r.push_back(e);
    return r;
}

bool InputRecorder::start() {
    if (!m_valid)
        return false;

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
    if (!m_recording)
        return m_hasRun;

    m_recording = false;
    m_waitingForFirstInput = false;

    m_run.tps = m_tps;

    int lastTick =
        m_run.inputs.empty()
            ? 0
            : m_run.inputs.back().tick;

    m_run.durationTicks =
        durationTicks < lastTick
            ? lastTick
            : durationTicks;

    // A recording is useful even if timing metadata wasn't sampled.
    // Raw captured inputs are the source of truth for this stage.
    if (m_run.inputs.empty()) {
        m_hasRun = false;
        return false;
    }

    std::string err;
    m_hasRun = m_run.validate(err);

    return m_hasRun;
}

} // namespace afc
