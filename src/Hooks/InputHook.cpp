// Observes the REAL input path (GJBaseGameLayer::handleButton) and the real
// physics tick (GJBaseGameLayer::processCommands). Both calls always forward to
// the original, so external macros that reach this path are seen unchanged.
#include "../Engine/Engine.hpp"
#include "../Platform/Game.hpp"
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

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        PlayLayer* pl = PlayLayer::get();

        bool track = pl && static_cast<GJBaseGameLayer*>(pl) == this;
        int beforeTick = -1;

        if (track) {
            beforeTick = afc::game::tick(pl);
            afc::Engine::get().tickPre(pl, dt);
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        if (track) {
            int afterTick = afc::game::tick(pl);
            afc::Engine::get().tickPost(pl, beforeTick, afterTick, dt);
        }
    }
};
