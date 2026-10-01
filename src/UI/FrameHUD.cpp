#include "FrameHUD.hpp"
#include "../Core/FrameBucket.hpp"
#include "../Core/MeasurementManager.hpp"
#include "../Engine/Engine.hpp"
#include "../Platform/Game.hpp"
#include "../Platform/Settings.hpp"
#include <chrono>

using namespace geode::prelude;

namespace afc {

static double nowSec() {   // UI fade only; never used as a frame source
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static ccColor3B bucketColor(int exact) {
    switch (classify(exact)) {
        case Bucket::B10_15: return {80, 230, 120};
        case Bucket::B7_9: return {170, 230, 80};
        case Bucket::B4_6: return {250, 220, 70};
        case Bucket::B3: return {255, 160, 50};
        case Bucket::B2: return {255, 100, 60};
        case Bucket::B1: return {255, 60, 60};
        default: return {200, 200, 200};
    }
}

FrameHUD* FrameHUD::create() {
    auto* r = new FrameHUD();
    if (r && r->init()) { r->autorelease(); return r; }
    delete r;
    return nullptr;
}

bool FrameHUD::init() {
    if (!CCNode::init()) return false;
    this->setID("afc-hud"_spr);
    m_main = CCLabelBMFont::create("", "bigFont.fnt");
    m_main->setAlignment(kCCTextAlignmentLeft);
    m_sub = CCLabelBMFont::create("", "chatFont.fnt");
    this->addChild(m_main);
    this->addChild(m_sub);
    for (auto& l : m_p) {
        l = CCLabelBMFont::create("", "bigFont.fnt");
        l->setVisible(false);
        this->addChild(l, 5);
    }
    return true;
}

void FrameHUD::setPausedHidden(bool hidden) {
    m_hiddenByPause = hidden;
    this->setVisible(!hidden);
}

static std::string line(Measurement const& m, bool ex, bool bk) {
    std::string t = formatMeasurement(m, ex, bk);
    if (t.empty()) t = "-";
    return t;
}

void FrameHUD::rebuildText() {
    auto& mm = MeasurementManager::get();
    bool ex = cfg::b("show-exact"), bk = cfg::b("show-bucket");
    bool compact = cfg::b("compact-mode");
    Method me = Engine::get().active();
    std::string out;

    auto head = [&](char const* t) { if (!compact) out += std::string(t) + "\n"; };
    auto playersLine = [&](Method m) {
        for (int p = 1; p <= 2; ++p) {
            if (!cfg::showsPlayer(p)) continue;
            auto v = mm.latest(m, p);
            if (!v) continue;
            out += (cfg::playerSel() == cfg::PlayerSel::Both || compact ? "P" + std::to_string(p) + " " : std::string()) + line(*v, ex, bk) + "\n";
        }
    };

    switch (cfg::mode()) {
        case cfg::Mode::Live:
            head(me == Method::DirectTick ? "INPUT GAP" : "FRAME WINDOW");
            playersLine(me);
            break;
        case cfg::Mode::Analysis: {
            head("WINDOW COUNTS");
            int pl = cfg::playerSel() == cfg::PlayerSel::P1 ? 1 : (cfg::playerSel() == cfg::PlayerSel::P2 ? 2 : 0);
            auto c = mm.bucketCounts(me, pl);
            for (int i = 0; i < kBucketCount; ++i)
                out += std::string(bucketLabel(bucketFromIndex(i))) + ": " + std::to_string(c[i]) + "\n";
            break;
        }
        case cfg::Mode::Compare:
            head("COMPARE");
            for (Method m : {Method::DirectTick, Method::StateProbe, Method::ReplayProbe}) {
                for (int p = 1; p <= 2; ++p) {
                    if (!cfg::showsPlayer(p)) continue;
                    auto v = mm.latest(m, p);
                    out += std::string("M") + std::to_string(static_cast<int>(m)) + (cfg::playerSel() == cfg::PlayerSel::Both ? " P" + std::to_string(p) : std::string()) +
                           " " + (v ? line(*v, ex, bk) : std::string("-")) + "\n";
                }
            }
            break;
    }

    if (cfg::b("show-history")) {
        int shown = 0;
        auto const& all = mm.all();
        out += "\n";
        for (auto it = all.rbegin(); it != all.rend() && shown < 3; ++it) {
            if (it->method != me || !cfg::showsPlayer(it->player)) continue;
            out += "#" + std::to_string(it->inputIndex) + " " + line(*it, ex, bk) + "\n";
            ++shown;
        }
    }
    if (!out.empty() && out.back() == '\n') out.pop_back();
    m_main->setString(out.c_str());
    m_sub->setString(Engine::get().statusLine().c_str());
    m_sub->setOpacity(150);
}

void FrameHUD::placeHud() {
    auto win = CCDirector::get()->getWinSize();
    float s = cfg::f("hud-scale");
    float op = cfg::f("hud-opacity");
    m_main->setScale(0.4f * s);
    m_sub->setScale(0.45f * s);
    m_main->setOpacity(static_cast<GLubyte>(255.f * op));
    m_sub->setOpacity(static_cast<GLubyte>(150.f * op));
    auto corner = cfg::s("hud-corner");
    bool right = corner.find("right") != std::string::npos;
    bool top = corner.find("top") != std::string::npos;
    float margin = 8.f;
    float x = right ? win.width - margin - cfg::f("hud-x") : margin + cfg::f("hud-x");
    float y = top ? win.height - margin - cfg::f("hud-y") : margin + cfg::f("hud-y");
    float mainH = m_main->getContentSize().height * m_main->getScale();
    m_main->setAnchorPoint({right ? 1.f : 0.f, top ? 1.f : 0.f});
    m_sub->setAnchorPoint({right ? 1.f : 0.f, top ? 1.f : 0.f});
    if (top) { m_main->setPosition({x, y}); m_sub->setPosition({x, y - mainH - 4.f}); }
    else { m_sub->setPosition({x, y}); m_main->setPosition({x, y + m_sub->getContentSize().height * m_sub->getScale() + 4.f}); }
}

void FrameHUD::placeLabels(PlayLayer* pl) {
    bool ex = cfg::b("show-exact"), bk = cfg::b("show-bucket");
    Method me = Engine::get().active();
    float hold = std::max(0.2f, cfg::f("label-hold-seconds"));
    double age = nowSec() - m_lastChangeTime;
    float fade = age <= hold ? 1.f : std::max(0.f, 1.f - static_cast<float>((age - hold) / 0.5));
    for (int p = 1; p <= 2; ++p) {
        auto* lab = m_p[p - 1];
        PlayerObject* po = p == 1 ? pl->m_player1 : pl->m_player2;
        bool show = cfg::b("show-label") && po && cfg::showsPlayer(p) && (p == 1 || game::dual(pl)) && fade > 0.f;
        std::optional<Measurement> v;
        if (show) v = MeasurementManager::get().latest(me, p);
        if (!show || !v) { lab->setVisible(false); continue; }
        std::string t = formatMeasurement(*v, ex, bk);
        if (t.empty()) { lab->setVisible(false); continue; }
        lab->setString(t.c_str());
        lab->setColor(v->exact >= 0 ? bucketColor(v->exact) : ccColor3B{200, 200, 200});
        lab->setScale(0.35f * cfg::f("label-scale"));
        lab->setOpacity(static_cast<GLubyte>(255.f * cfg::f("label-opacity") * fade));
        // world -> screen through the live node tree (handles camera pan/zoom/rotation/flip)
        CCNode* par = po->getParent();
        if (!par) { lab->setVisible(false); continue; }
        CCPoint world = par->convertToWorldSpace(po->getPosition());
        CCPoint local = this->getParent() ? this->getParent()->convertToNodeSpace(world) : world;
        float sign = po->m_isUpsideDown ? -1.f : 1.f;   // follows gravity
        lab->setPosition({local.x + cfg::f("label-x"), local.y + sign * (cfg::f("label-y") + 18.f)});
        lab->setVisible(true);
    }
}

void FrameHUD::refresh(PlayLayer* pl) {
    if (!pl) return;
    bool on = cfg::enabled();
    if (!on) { this->setVisible(false); return; }
    this->setVisible(!m_hiddenByPause);
    bool showHud = cfg::b("show-hud");
    m_main->setVisible(showHud);
    m_sub->setVisible(showHud);

    auto ver = MeasurementManager::get().version();
    int sig = static_cast<int>(cfg::mode()) * 1000 + static_cast<int>(cfg::playerSel()) * 100 +
              static_cast<int>(Engine::get().active()) * 10 + (cfg::b("show-exact") ? 1 : 0) + (cfg::b("show-bucket") ? 2 : 0) +
              (cfg::b("compact-mode") ? 4 : 0) + (cfg::b("show-history") ? 8 : 0);
    if (ver != m_lastVersion || sig != m_lastSig) {
        if (ver != m_lastVersion) m_lastChangeTime = nowSec();
        m_lastVersion = ver; m_lastSig = sig;
        rebuildText();
    }
    if (showHud && (++m_frames % 15) == 0) m_sub->setString(Engine::get().statusLine().c_str());
    if (showHud) placeHud();
    placeLabels(pl);
}

} // namespace afc
