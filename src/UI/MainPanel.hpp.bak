#pragma once
#include "../Core/FrameBucket.hpp"
#include <Geode/Geode.hpp>
#include <functional>

namespace afc {

// Touch-first control panel opened from the pause menu. Built on a plain
// CCLayerColor (no Popup template) so it does not depend on Popup API revisions.
class MainPanel : public cocos2d::CCLayerColor {
public:
    enum class Page { Main, List, Compare, Record, Settings, Info };
    static MainPanel* create();
    static void open();              // idempotent: never stacks two panels
    void close();

private:
    bool init() override;
    void registerWithTouchDispatcher() override;
    bool ccTouchBegan(cocos2d::CCTouch*, cocos2d::CCEvent*) override { return true; }   // swallow
    void keyBackClicked() override { close(); }
    void update(float dt) override;

    void go(Page p);                 // deferred rebuild (never inside the tapped button's callback)
    void build();
    void buildMain(); void buildList(); void buildCompare(); void buildRecord(); void buildSettings(); void buildInfo();

    cocos2d::CCMenuItemSpriteExtra* button(cocos2d::CCMenu* menu, std::string const& text, cocos2d::CCPoint pos,
                                           float w, std::function<void()> cb, bool selected = false, float h = 34.f);
    cocos2d::CCLabelBMFont* label(std::string const& text, cocos2d::CCPoint pos, float scale, cocos2d::CCPoint anchor = {0.5f, 0.5f});
    void toast(std::string const& t);
    void toggleBool(char const* key);

    cocos2d::CCNode* m_content = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    cocos2d::CCLabelBMFont* m_progress = nullptr;
    cocos2d::CCLabelBMFont* m_recLabel = nullptr;
    cocos2d::CCSize m_size;
    Page m_page = Page::Main;
    Query m_query;
    int m_listPage = 0;
    bool m_searchMode = false;
    int m_infoMethod = 1;
    int m_savedIdx = 0;
    std::string m_toast;
};

} // namespace afc
