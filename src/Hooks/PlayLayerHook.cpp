#include "../Engine/Engine.hpp"
#include "../Methods/WindowProbe.hpp"
#include "../Platform/Settings.hpp"
#include "../UI/FrameHUD.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(AfcPlayLayer, PlayLayer) {
    struct Fields {
        afc::FrameHUD* hud = nullptr;   // owned by m_uiLayer
    };

    void ensureHud() {
        if (m_fields->hud || !m_uiLayer) return;
        if (auto* h = afc::FrameHUD::create()) {
            m_uiLayer->addChild(h, 1000);
            m_fields->hud = h;
        }
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        ensureHud();
        afc::Engine::get().attemptStart(this);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        if (afc::WindowProbe::probing()) return;      // Method 3 resets the level itself
        ensureHud();
        if (m_fields->hud) m_fields->hud->setPausedHidden(false);
        afc::Engine::get().attemptStart(this);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (m_fields->hud && !afc::WindowProbe::probing()) m_fields->hud->refresh(this);
    }

    GameObject* destroyPlayer(PlayerObject* p, GameObject* g) {
        if (afc::WindowProbe::probing()) {            // simulated death: record it, do not play it
            afc::WindowProbe::markDeath();
            return nullptr;
        }
        auto* r = PlayLayer::destroyPlayer(p, g);
        afc::Engine::get().attemptEnd(this, false);
        return r;
    }

    void levelComplete() {
        if (afc::WindowProbe::probing()) { afc::WindowProbe::markDeath(); return; }   // replay must not finish the level
        afc::Engine::get().attemptEnd(this, false);
        PlayLayer::levelComplete();
    }

    void resume() {
        afc::Engine::get().resumed(this);             // restore live state BEFORE the game continues
        if (m_fields->hud) m_fields->hud->setPausedHidden(false);
        PlayLayer::resume();
    }

    void onQuit() {
        afc::Engine::get().attemptEnd(this, true);
        m_fields->hud = nullptr;
        PlayLayer::onQuit();
    }
};
