#pragma once
#include "IFrameMethod.hpp"

namespace afc {

// METHOD 1: the real gameplay tick at which each input was processed, plus the
// real tick distance to the previous press by the same player. Lightweight and
// live. It does NOT determine a survivable window (see limitations()).
class DirectTickMethod final : public IFrameMethod {
public:
    Method id() const override { return Method::DirectTick; }
    const char* title() const override { return "Direct gameplay tick"; }
    const char* description() const override {
        return "Observes each input at the physics tick it was processed and reports "
               "the tick distance since your previous press (the 'gap').";
    }
    const char* limitations() const override {
        return "Measures spacing between inputs, not how early/late an input could be. "
               "First press of an attempt has no gap (UNAVAILABLE).";
    }
    void onInput(InputEvent const& e, int pressIndex, int levelID) override;
    void onAttemptReset() override {}
    void onDeactivate() override {}
    std::string status() const override { return "METHOD 1 DIRECT"; }
};

} // namespace afc
