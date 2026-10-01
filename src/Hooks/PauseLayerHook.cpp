// Pause-menu button. Safe by construction:
//  * looked up / inserted by node ID, never by child index,
//  * inserted into the existing layout-managed "right-button-menu" (the layout
//    re-flows around other mods' buttons) or, if that node is missing or the
//    user chose "floating", into our own menu at a user-configurable position,
//  * duplicate-checked, recreated with the pause layer, null-checked.
#include "../Engine/Engine.hpp"
#include "../Platform/Settings.hpp"
#include "../UI/MainPanel.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

class $modify(AfcPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        if (!afc::cfg::enabled()) return;

        if (auto* pl = PlayLayer::get()) {
            afc::Engine::get().paused(pl);
            if (afc::cfg::b("hud-hide-when-paused")) {
                // hide screen HUD while paused (UI only)
                if (auto* hud = pl->m_uiLayer ? pl->m_uiLayer->getChildByID("afc-hud"_spr) : nullptr)
                    hud->setVisible(false);
            }
        }
        if (!afc::cfg::b("pause-button")) return;
        if (this->getChildByIDRecursive("afc-pause-button"_spr)) return;   // no duplicates

        CCSprite* spr = CCSprite::create("icon-pause.png"_spr);
        if (!spr) spr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn02_001.png");
        if (!spr) return;
        float h = spr->getContentSize().height;
        if (h > 0.f) spr->setScale(34.f / h);

        auto* btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AfcPauseLayer::onAfc));
        btn->setID("afc-pause-button"_spr);

        auto* host = this->getChildByID("right-button-menu");
        bool floating = afc::cfg::s("pause-button-mode") == "floating";
        if (host && !floating) {
            host->addChild(btn);
            host->updateLayout();
            return;
        }
        auto win = CCDirector::get()->getWinSize();
        auto* menu = CCMenu::create();
        menu->setID("afc-pause-menu"_spr);
        menu->setPosition({0, 0});
        btn->setPosition({win.width * afc::cfg::i("pause-button-x") / 100.f,
                          win.height * afc::cfg::i("pause-button-y") / 100.f});
        menu->addChild(btn);
        this->addChild(menu, 100);
    }

    void onAfc(CCObject*) { afc::MainPanel::open(); }
};
