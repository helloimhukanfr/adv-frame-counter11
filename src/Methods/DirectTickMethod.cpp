#include "DirectTickMethod.hpp"
#include "../Core/MeasurementManager.hpp"
#include "../Recording/InputRecorder.hpp"

namespace afc {

void DirectTickMethod::onInput(InputEvent const& e, int pressIndex, int levelID) {
    if (!e.down || pressIndex <= 0) return;
    Measurement m;
    m.levelID = levelID;
    m.inputIndex = pressIndex;
    m.tick = e.tick;
    m.player = e.player;
    m.button = e.button;
    m.method = Method::DirectTick;
    m.kind = Kind::Interval;
    int gap = InputRecorder::get().ticksSincePreviousPress(e.player);
    if (gap > 0) {
        m.exact = gap;
        m.validity = Validity::Measured;
    } else {
        m.validity = Validity::Unavailable;
        m.note = "first press of attempt (no previous press)";
    }
    MeasurementManager::get().add(std::move(m));
}

} // namespace afc
