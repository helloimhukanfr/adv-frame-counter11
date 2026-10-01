#pragma once
// ALL direct reads of Geometry Dash fields live here, so API drift (field
// renames between Geode/bindings versions) is fixed in one place.
#include "../Recording/RecordedRun.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

namespace afc::game {

// The tick identity used everywhere: GJBaseGameLayer::m_gameState.m_currentProgress.
inline int tick(GJBaseGameLayer* l) { return static_cast<int>(l->m_gameState.m_currentProgress); }

inline int modeOf(PlayerObject* p) {
    if (!p) return 0;
    if (p->m_isShip) return 1;
    if (p->m_isBall) return 2;
    if (p->m_isBird) return 3;
    if (p->m_isDart) return 4;
    if (p->m_isRobot) return 5;
    if (p->m_isSpider) return 6;
    if (p->m_isSwing) return 7;
    return 0;
}

inline bool practice(PlayLayer* l) { return l && l->m_isPracticeMode; }

inline bool dual(GJBaseGameLayer* l) { return l->m_gameState.m_isDualMode; }

inline InputEvent makeEvent(GJBaseGameLayer* l, bool down, int button, bool p1) {
    InputEvent e;
    e.tick = tick(l);
    e.player = p1 ? 1 : 2;
    e.button = button;
    e.down = down;
    PlayerObject* p = p1 ? l->m_player1 : l->m_player2;
    if (p) { e.x = p->getPositionX(); e.y = p->getPositionY(); e.mode = modeOf(p); }
    e.dual = dual(l);
    return e;
}

inline int levelID(GJBaseGameLayer* l) {
    return (l && l->m_level) ? static_cast<int>(l->m_level->m_levelID.value()) : 0;
}
inline std::string levelName(GJBaseGameLayer* l) {
    return (l && l->m_level) ? std::string(l->m_level->m_levelName) : std::string();
}
inline bool platformer(GJBaseGameLayer* l) {
    return l && l->m_levelSettings && l->m_levelSettings->m_platformerMode;
}

} // namespace afc::game
