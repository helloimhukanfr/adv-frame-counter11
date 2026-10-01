#pragma once
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

namespace afc {

// Screen HUD (fixed to a corner) + on-player labels. Both are children of the
// PlayLayer's UI layer; label positions are derived every frame from the
// player's REAL position through the engine's own node transforms, so camera
// move/zoom/rotation/flip are handled by Cocos, not by hand-written math.
class FrameHUD : public cocos2d::CCNode {
public:
    static FrameHUD* create();
    // Call once per rendered frame from PlayLayer::postUpdate.
    void refresh(PlayLayer* pl);
    void setPausedHidden(bool hidden);

private:
    bool init() override;
    void rebuildText();
    void placeLabels(PlayLayer* pl);
    void placeHud();

    cocos2d::CCLabelBMFont* m_main = nullptr;
    cocos2d::CCLabelBMFont* m_sub = nullptr;
    cocos2d::CCLabelBMFont* m_p[2] = {nullptr, nullptr};
    uint64_t m_lastVersion = ~0ull;
    int m_lastSig = -1;
    double m_lastChangeTime = 0;
    bool m_hiddenByPause = false;
    int m_frames = 0;
};

} // namespace afc
