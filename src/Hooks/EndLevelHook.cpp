#include "../Engine/Engine.hpp"
#include "../Recording/InputRecorder.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <chrono>

using namespace geode::prelude;

namespace {

std::string makeAFCSaveName() {
    auto& rec = afc::InputRecorder::get();

    long long stamp =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    return
        "afc_" +
        std::to_string(rec.run().levelID) +
        "_" +
        std::to_string(stamp);
}

}

class $modify(AfcEndLevelLayer, EndLevelLayer) {

    void customSetup() {
        EndLevelLayer::customSetup();

        auto& rec = afc::InputRecorder::get();

        // Only show the AFC save control when a completed recording exists.
        if (!rec.hasRun() || rec.run().inputs.empty())
            return;

        if (this->getChildByID("afc-end-save"))
            return;

        auto win = CCDirector::get()->getWinSize();

        // Bottom-left is intentionally used instead of the center/right
        // controls so the button does not fight the stock completion UI.
        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        menu->setID("afc-end-save");

        auto buttonSprite =
            ButtonSprite::create(
                "SAVE RECORDING",
                128,
                true,
                "bigFont.fnt",
                "GJ_button_01.png",
                30.f,
                0.52f
            );

        auto button =
            CCMenuItemExt::createSpriteExtra(
                buttonSprite,
                [](CCObject*) {
                    auto& recorder = afc::InputRecorder::get();

                    if (!recorder.hasRun() ||
                        recorder.run().inputs.empty()) {
                        Notification::create(
                            "No AFC recording to save.",
                            NotificationIcon::Error,
                            1.5f
                        )->show();
                        return;
                    }

                    std::string name = makeAFCSaveName();
                    std::string err;

                    if (afc::Engine::get().saveRun(name, err)) {
                        Notification::create(
                            "AFC recording saved.",
                            NotificationIcon::Success,
                            1.8f
                        )->show();
                    }
                    else {
                        Notification::create(
                            ("AFC save failed: " + err).c_str(),
                            NotificationIcon::Error,
                            2.5f
                        )->show();
                    }
                }
            );

        button->setID("afc-end-save-button");

        menu->addChild(button);
        button->setPosition({82.f, 34.f});

        this->addChild(menu, 10000);

        (void)win;
    }

};
