#include <Geode/Geode.hpp>

using namespace geode::prelude;

// All behaviour lives in the hooks (src/Hooks) and the Engine (src/Engine).
// Nothing runs unless the player is inside a level (observation-only by default).
$on_mod(Loaded) {
    log::info("Advanced Frame Counter loaded (frame = Geometry Dash physics tick)");
}
