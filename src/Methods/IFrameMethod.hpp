#pragma once
#include "../Core/Types.hpp"
#include "../Recording/RecordedRun.hpp"

namespace afc {

// A measurement strategy. Methods are observational: they never alter gameplay
// unless the user explicitly starts probing/playback.
class IFrameMethod {
public:
    virtual ~IFrameMethod() = default;
    virtual Method id() const = 0;
    virtual const char* title() const = 0;        // short name
    virtual const char* description() const = 0;  // technical, evidence-free-of-hype
    virtual const char* limitations() const = 0;

    // A real input was processed. press index is 1-based (0 = release).
    virtual void onInput(InputEvent const& e, int pressIndex, int levelID) = 0;
    virtual void onAttemptReset() = 0;            // death / restart / level exit
    virtual void onDeactivate() = 0;              // method switched away: free everything
    virtual std::string status() const = 0;       // subtle one-line status for UI
};

} // namespace afc
