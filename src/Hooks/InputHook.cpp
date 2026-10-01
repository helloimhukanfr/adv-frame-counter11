// Observes the REAL input path (GJBaseGameLayer::handleButton) and the real
// physics tick (GJBaseGameLayer::processCommands). Both calls always forward to
// the original, so external macros that reach this path are seen unchanged.
#include "../Engine/Engine.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

class $modify(AfcBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        PlayLayer* pl = PlayLayer::get();
        if (pl && static_cast<GJBaseGameLayer*>(pl) == this)
            afc::Engine::get().input(pl, down, button, isPlayer1);   // observe BEFORE the game applies it
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt) {
        PlayLayer* pl = PlayLayer::get();
        if (pl && static_cast<GJBaseGameLayer*>(pl) == this)
            afc::Engine::get().tickPre(pl, dt);
        GJBaseGameLayer::processCommands(dt);
    }
};
