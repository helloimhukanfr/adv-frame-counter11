#include "MainPanel.hpp"
#include "../Core/MeasurementManager.hpp"
#include "../Engine/Engine.hpp"
#include "../Platform/Settings.hpp"
#include "../Recording/InputRecorder.hpp"

using namespace geode::prelude;

namespace afc {

static constexpr char const* kPanelID = "afc-main-panel";

MainPanel* MainPanel::create() {
    auto* r = new MainPanel();
    if (r && r->initWithColor({0, 0, 0, 150}) && r->init()) { r->autorelease(); return r; }
    delete r;
    return nullptr;
}

void MainPanel::open() {
    auto scene = CCDirector::get()->getRunningScene();
    if (!scene) return;
    if (scene->getChildByID(kPanelID)) return;   // already open
    if (auto* p = MainPanel::create()) {
        p->setID(kPanelID);
        scene->addChild(p, 10000);
    }
}

void MainPanel::close() {
    this->unscheduleUpdate();
    this->removeFromParentAndCleanup(true);
}

void MainPanel::registerWithTouchDispatcher() {
    CCTouchDispatcher::get()->addTargetedDelegate(this, -500, true);
}

bool MainPanel::init() {
    this->setTouchEnabled(true);
    this->setKeypadEnabled(true);
    auto win = CCDirector::get()->getWinSize();
    // Responsive: wide screens (20:9, tablets) get a wider panel; never beyond safe margins.
    m_size.setSize(std::min(win.width - 30.f, 470.f), std::min(win.height - 12.f, 304.f));
    auto bg = cocos2d::extension::CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
    bg->setContentSize(m_size);
    bg->setColor({10, 14, 24});
    bg->setOpacity(225);
    bg->setPosition(win / 2);
    this->addChild(bg);
    m_content = CCNode::create();
    m_content->setPosition(win / 2 - m_size / 2);   // content origin = panel bottom-left
    m_content->setContentSize(m_size);
    this->addChild(m_content);
    this->scheduleUpdate();
    build();
    return true;
}

void MainPanel::go(Page p) {
    m_page = p;
    Ref<MainPanel> self(this);
    queueInMainThread([self] { self->build(); });
}

void MainPanel::build() {
    m_content->removeAllChildrenWithCleanup(true);
    m_status = m_progress = m_recLabel = nullptr;
    switch (m_page) {
        case Page::Main: buildMain(); break;
        case Page::List: buildList(); break;
        case Page::Compare: buildCompare(); break;
        case Page::Record: buildRecord(); break;
        case Page::Settings: buildSettings(); break;
        case Page::Info: buildInfo(); break;
    }
}

CCLabelBMFont* MainPanel::label(std::string const& text, CCPoint pos, float scale, CCPoint anchor) {
    auto l = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    l->setScale(scale);
    l->setAnchorPoint(anchor);
    l->setPosition(pos);
    m_content->addChild(l);
    return l;
}

CCMenuItemSpriteExtra* MainPanel::button(CCMenu* menu, std::string const& text, CCPoint pos, float w,
                                         std::function<void()> cb, bool selected, float h) {
    auto spr = ButtonSprite::create(text.c_str(), static_cast<int>(w), true, "bigFont.fnt",
                                    selected ? "GJ_button_02.png" : "GJ_button_01.png", h, 0.55f);
    auto item = CCMenuItemExt::createSpriteExtra(spr, [cb = std::move(cb)](CCObject*) { cb(); });
    item->setPosition(pos);
    menu->addChild(item);
    return item;
}

void MainPanel::toast(std::string const& t) {
    m_toast = t;
    if (m_status) m_status->setString(t.c_str());
}

void MainPanel::toggleBool(char const* key) {
    Mod::get()->setSettingValue<bool>(key, !Mod::get()->getSettingValue<bool>(key));
    go(m_page);
}

// ------------------------------------------------------------------- pages

static CCMenu* newMenu(CCNode* parent) {
    auto m = CCMenu::create();
    m->setPosition({0, 0});
    m->setTouchPriority(-501);     // above the swallowing layer (-500) and the pause menu
    parent->addChild(m);
    return m;
}

void MainPanel::buildMain() {
    auto& eng = Engine::get();
    float W = m_size.width, H = m_size.height;
    label("FRAME COUNTER", {W / 2, H - 20}, 0.6f);
    auto menu = newMenu(m_content);
    Method cur = eng.active();
    label("FRAME METHOD", {W / 2, H - 46}, 0.38f)->setOpacity(160);
    float bw = (W - 40) / 3.f;
    struct M { Method m; char const* t; } ms[] = {{Method::DirectTick, "1 DIRECT"}, {Method::StateProbe, "2 SIM"}, {Method::ReplayProbe, "3 REPLAY"}};
    for (int i = 0; i < 3; ++i) {
        Method m = ms[i].m;
        button(menu, ms[i].t, {20 + bw * (i + 0.5f), H - 76}, bw - 8, [this, m] {
            Engine::get().switchMethod(m);     // safe live switch
            cfg::setMethod(m);
            go(Page::Main);
        }, cur == m);
    }
    auto d = eng.methodObj(cur).description();
    label(std::string(eng.methodObj(cur).title()), {W / 2, H - 106}, 0.4f);
    // wrap description manually (BMFont has no auto-wrap)
    std::string txt = d, line; int y = 0; size_t pos = 0;
    while (pos < txt.size() && y < 3) {
        size_t cut = std::min(pos + 62, txt.size());
        if (cut < txt.size()) { size_t sp = txt.rfind(' ', cut); if (sp != std::string::npos && sp > pos) cut = sp; }
        label(txt.substr(pos, cut - pos), {W / 2, H - 124 - y * 12.f}, 0.27f)->setOpacity(190);
        pos = cut + (cut < txt.size() ? 1 : 0); ++y;
    }
    m_status = label(eng.statusLine(), {W / 2, H - 170}, 0.3f);
    m_progress = label("", {W / 2, H - 184}, 0.3f);
    float rw = (W - 40) / 4.f;
    float y1 = 84, y2 = 42;
    button(menu, "HISTORY", {20 + rw * 0.5f, y1}, rw - 6, [this] { m_searchMode = false; m_query = {}; m_listPage = 0; go(Page::List); });
    button(menu, "SEARCH", {20 + rw * 1.5f, y1}, rw - 6, [this] { m_searchMode = true; m_listPage = 0; go(Page::List); });
    button(menu, "RECORD", {20 + rw * 2.5f, y1}, rw - 6, [this] { go(Page::Record); });
    button(menu, "COMPARE", {20 + rw * 3.5f, y1}, rw - 6, [this] { go(Page::Compare); });
    float w3 = (W - 40) / 3.f;
    button(menu, "METHOD INFO", {20 + w3 * 0.5f, y2}, w3 - 6, [this] { m_infoMethod = static_cast<int>(Engine::get().active()); go(Page::Info); });
    button(menu, "SETTINGS", {20 + w3 * 1.5f, y2}, w3 - 6, [this] { go(Page::Settings); });
    button(menu, "CLOSE", {20 + w3 * 2.5f, y2}, w3 - 6, [this] { close(); });
    label("gameplay frame = physics tick", {W / 2, 14}, 0.26f)->setOpacity(120);
}

void MainPanel::buildList() {
    auto& mm = MeasurementManager::get();
    float W = m_size.width, H = m_size.height;
    label(m_searchMode ? "SEARCH" : "HISTORY", {W / 2, H - 16}, 0.5f);
    auto menu = newMenu(m_content);
    float top = H - 34;
    if (m_searchMode) {
        // bucket chips
        float cw = (W - 24) / 6.f;
        for (int i = 0; i < kBucketCount; ++i) {
            Bucket b = bucketFromIndex(i);
            bool sel = m_query.bucket && *m_query.bucket == b;
            button(menu, bucketLabel(b), {12 + cw * (i + 0.5f), top - 13}, cw - 4, [this, b, i] {
                static const int lo[] = {10, 7, 4, 3, 2, 1}, hi[] = {15, 9, 6, 3, 2, 1};
                if (m_query.bucket && *m_query.bucket == b) { m_query.lo.reset(); m_query.hi.reset(); m_query.bucket.reset(); }
                else { m_query.lo = lo[i]; m_query.hi = hi[i]; m_query.bucket = b; }
                m_listPage = 0; go(Page::List);
            }, sel, 28.f);
        }
        // player / method chips
        float c2 = (W - 24) / 5.f;
        for (int p = 1; p <= 2; ++p) {
            bool sel = m_query.player && *m_query.player == p;
            button(menu, "P" + std::to_string(p), {12 + c2 * (p - 0.5f), top - 44}, c2 - 4, [this, p] {
                if (m_query.player == p) m_query.player.reset(); else m_query.player = p;
                m_listPage = 0; go(Page::List);
            }, sel, 28.f);
        }
        for (int k = 1; k <= 3; ++k) {
            bool sel = m_query.method && static_cast<int>(*m_query.method) == k;
            button(menu, "M" + std::to_string(k), {12 + c2 * (k + 1.5f), top - 44}, c2 - 4, [this, k] {
                if (m_query.method && static_cast<int>(*m_query.method) == k) m_query.method.reset();
                else m_query.method = static_cast<Method>(k);
                m_listPage = 0; go(Page::List);
            }, sel, 28.f);
        }
        // input index stepper
        std::string idx = m_query.inputIndex ? "#" + std::to_string(*m_query.inputIndex) : "ANY #";
        float sx = W / 2;
        label(idx, {sx, top - 72}, 0.4f);
        auto step = [this](int d) {
            int v = m_query.inputIndex.value_or(0) + d;
            if (v <= 0) m_query.inputIndex.reset(); else m_query.inputIndex = v;
            m_listPage = 0; go(Page::List);
        };
        button(menu, "-10", {sx - 120, top - 72}, 44, [step] { step(-10); }, false, 26.f);
        button(menu, "-1", {sx - 70, top - 72}, 44, [step] { step(-1); }, false, 26.f);
        button(menu, "+1", {sx + 70, top - 72}, 44, [step] { step(1); }, false, 26.f);
        button(menu, "+10", {sx + 120, top - 72}, 44, [step] { step(10); }, false, 26.f);
        top -= 90;
    }
    auto res = mm.search(m_query);
    int perPage = m_searchMode ? 3 : 7;
    int pages = std::max(1, (static_cast<int>(res.size()) + perPage - 1) / perPage);
    m_listPage = std::min(std::max(0, m_listPage), pages - 1);
    if (res.empty()) label("No real measurements match.", {W / 2, top - 30}, 0.35f)->setOpacity(160);
    for (int r = 0; r < perPage; ++r) {
        size_t i = static_cast<size_t>(m_listPage * perPage + r);
        if (i >= res.size()) break;
        auto const& m = res[i];
        std::string txt = "#" + std::to_string(m.inputIndex) + "  T" + std::to_string(m.tick) + "  P" + std::to_string(m.player) +
                          "  " + (m.kind == Kind::Window ? "W " : "G ") + formatMeasurement(m, true, true) + "  M" +
                          std::to_string(static_cast<int>(m.method)) + "  " + (m.validity == Validity::Valid || m.validity == Validity::Measured ? "" : validityName(m.validity));
        label(txt, {16, top - 14 - r * 17.f}, 0.34f, {0, 0.5f});
    }
    label(std::to_string(res.size()) + " results  p" + std::to_string(m_listPage + 1) + "/" + std::to_string(pages), {W / 2, 50}, 0.3f)->setOpacity(150);
    button(menu, "<", {W / 2 - 80, 24}, 50, [this] { --m_listPage; go(Page::List); });
    button(menu, ">", {W / 2 + 80, 24}, 50, [this] { ++m_listPage; go(Page::List); });
    button(menu, "BACK", {W - 52, 24}, 80, [this] { go(Page::Main); });
    if (!m_searchMode) button(menu, "CLEAR", {52, 24}, 80, [this] { MeasurementManager::get().clear(); go(Page::List); });
    label("G = gap (M1)   W = window (M2/M3)", {W / 2, 6}, 0.24f)->setOpacity(110);
}

void MainPanel::buildCompare() {
    auto& mm = MeasurementManager::get();
    float W = m_size.width, H = m_size.height;
    label("METHOD COMPARISON", {W / 2, H - 18}, 0.5f);
    bool ex = true, bk = true;   // comparison always shows both forms
    float y = H - 52;
    for (int p = 1; p <= 2; ++p) {
        label("PLAYER " + std::to_string(p), {16, y}, 0.4f, {0, 0.5f}); y -= 20;
        std::optional<Measurement> vals[3];
        for (int m = 1; m <= 3; ++m) vals[m - 1] = mm.latest(static_cast<Method>(m), p);
        for (int m = 1; m <= 3; ++m) {
            auto const& v = vals[m - 1];
            std::string t = std::string("M") + std::to_string(m) + " " + methodShort(static_cast<Method>(m)) + ":  " +
                            (v ? formatMeasurement(*v, ex, bk) + (v->kind == Kind::Interval ? "  (gap)" : "  (window)") : "-");
            label(t, {28, y}, 0.34f, {0, 0.5f}); y -= 16;
        }
        // Do not force agreement: report it.
        bool a = vals[1] && vals[2] && vals[1]->exact >= 0 && vals[2]->exact >= 0;
        if (a) label(vals[1]->exact == vals[2]->exact ? "M2 and M3 AGREE" : "M2 and M3 DISAGREE", {W - 16, y + 24}, 0.34f, {1, 0.5f});
        y -= 8;
    }
    label("Latest value per method. Methods are never forced to agree.", {W / 2, 56}, 0.26f)->setOpacity(130);
    auto menu = newMenu(m_content);
    button(menu, "BACK", {W - 52, 24}, 80, [this] { go(Page::Main); });
    button(menu, Mod::get()->getSettingValue<std::string>("analysis-mode") == "compare" ? "COMPARE: ON" : "TURN ON", {110, 24}, 190, [this] {
        Mod::get()->setSettingValue<std::string>("analysis-mode", "compare"); go(Page::Compare);
    });
}

void MainPanel::buildRecord() {
    auto& rec = InputRecorder::get();
    auto& eng = Engine::get();

    float W = m_size.width;
    float H = m_size.height;

    label("RECORDING", {W / 2.f, H - 18.f}, 0.52f);

    auto menu = newMenu(m_content);

    // --------------------------------------------------------
    // Status banner
    // --------------------------------------------------------

    std::string state;

    if (rec.recording())
        state = "ARMED  •  LEAVE PAUSE MENU AND START MACRO";
    else if (rec.hasRun())
        state =
            "READY  •  " +
            std::to_string(rec.run().pressCount()) +
            " PRESSES CAPTURED";
    else
        state = "IDLE  •  NO RECORDING";

    auto status =
        label(
            state,
            {W / 2.f, H - 43.f},
            0.27f
        );

    status->setOpacity(220);

    // --------------------------------------------------------
    // Metrics
    // --------------------------------------------------------

    float cardW = (W - 48.f) / 3.f;
    float metricY = H - 73.f;

    auto metric = [&](float x, char const* title, std::string value) {
        label(title, {x, metricY + 9.f}, 0.20f)->setOpacity(125);
        label(value, {x, metricY - 7.f}, 0.32f);
    };

    metric(
        12.f + cardW * 0.5f,
        "PRESSES",
        std::to_string(rec.run().pressCount())
    );

    metric(
        18.f + cardW * 1.5f,
        "EVENTS",
        std::to_string(rec.run().inputs.size())
    );

    metric(
        24.f + cardW * 2.5f,
        "TPS",
        rec.tps() > 0
            ? std::to_string(rec.tps())
            : "AUTO"
    );

    // --------------------------------------------------------
    // Action grid
    // --------------------------------------------------------

    float bw = (W - 40.f) / 2.f;

    float y1 = H - 115.f;
    float y2 = H - 151.f;
    float y3 = H - 187.f;

    button(
        menu,
        rec.recording() ? "CANCEL RECORDING" : "ARM RECORDING",
        {12.f + bw * 0.5f, y1},
        bw - 6.f,
        [this] {
            auto& r = InputRecorder::get();

            if (r.recording()) {
                Engine::get().stopRecording();
                toast("RECORDING CANCELLED");
            }
            else {
                std::string err;

                if (!Engine::get().startRecording(err))
                    toast(err);
                else
                    toast("RECORD ARMED - START MACRO");

            }

            go(Page::Record);
        },
        rec.recording(),
        30.f
    );

    button(
        menu,
        "ANALYZE LAST RUN",
        {28.f + bw * 1.5f, y1},
        bw - 6.f,
        [this] {
            auto& r = InputRecorder::get();

            if (!r.hasRun()) {
                toast("NO COMPLETED RECORDING");
                return;
            }

            Engine::get().switchMethod(Method::ReplayProbe);
            cfg::setMethod(Method::ReplayProbe);

            std::string err;

            if (Engine::get().startAnalysis(err))
                toast("ANALYSIS STARTED: EVERY CLICK, -15 TO +15");
            else
                toast(err);
        },
        false,
        30.f
    );

    button(
        menu,
        "SAVE LAST RUN",
        {12.f + bw * 0.5f, y2},
        bw - 6.f,
        [this] {
            auto& r = InputRecorder::get();

            if (!r.hasRun()) {
                toast("NO COMPLETED RECORDING");
                return;
            }

            std::string name =
                "afc_" +
                std::to_string(r.run().levelID) +
                "_" +
                std::to_string(r.run().pressCount());

            std::string err;

            if (Engine::get().saveRun(name, err))
                toast("RUN SAVED");
            else
                toast("SAVE FAILED: " + err);
        },
        false,
        30.f
    );

    button(
        menu,
        "CLEAR LAST RUN",
        {28.f + bw * 1.5f, y2},
        bw - 6.f,
        [this] {
            InputRecorder::get().clearRun();
            toast("RUN CLEARED");
            go(Page::Record);
        },
        false,
        30.f
    );

    button(
        menu,
        "PLAYBACK",
        {12.f + bw * 0.5f, y3},
        bw - 6.f,
        [this] {
            std::string err;

            if (Engine::get().playbackStart(err))
                toast("PLAYBACK ARMED");
            else
                toast(err);
        },
        false,
        30.f
    );

    button(
        menu,
        "STOP PLAYBACK",
        {28.f + bw * 1.5f, y3},
        bw - 6.f,
        [] {
            Engine::get().playbackStop();
        },
        false,
        30.f
    );

    // --------------------------------------------------------
    // Saved-run information
    // --------------------------------------------------------

    float infoY = 73.f;

    label(
        "LAST CAPTURE",
        {16.f, infoY},
        0.24f,
        {0.f, 0.5f}
    )->setOpacity(130);

    if (rec.hasRun()) {
        label(
            std::to_string(rec.run().pressCount()) +
            " presses  •  " +
            std::to_string(rec.run().inputs.size()) +
            " input events  •  " +
            std::to_string(rec.run().durationTicks) +
            " ticks",
            {16.f, infoY - 18.f},
            0.29f,
            {0.f, 0.5f}
        );
    }
    else {
        label(
            "Recording is kept until you clear it or start another run.",
            {16.f, infoY - 18.f},
            0.25f,
            {0.f, 0.5f}
        )->setOpacity(120);
    }

    label(
        "Each PRESS is re-tested at every integer offset from -15 to +15.",
        {16.f, 37.f},
        0.24f,
        {0.f, 0.5f}
    )->setOpacity(115);

    label(
        "The first death on either side defines the contiguous frame window.",
        {16.f, 22.f},
        0.23f,
        {0.f, 0.5f}
    )->setOpacity(105);

    button(
        menu,
        "BACK",
        {W - 45.f, 18.f},
        72.f,
        [this] {
            go(Page::Main);
        },
        false,
        26.f
    );
}

void MainPanel::buildSettings() {
    float W = m_size.width, H = m_size.height;
    label("QUICK SETTINGS", {W / 2, H - 16}, 0.5f);
    auto menu = newMenu(m_content);
    struct T { char const* key; char const* name; } t[] = {
        {"enabled", "ENABLED"}, {"show-hud", "HUD"}, {"show-label", "PLAYER LABEL"}, {"show-exact", "EXACT"},
        {"show-bucket", "BUCKET"}, {"show-history", "HISTORY"}, {"compact-mode", "COMPACT"}, {"allow-experimental", "EXPERIMENTAL"}};
    float cw = (W - 30) / 2.f;
    for (int i = 0; i < 8; ++i) {
        bool on = Mod::get()->getSettingValue<bool>(t[i].key);
        std::string txt = std::string(t[i].name) + (on ? " ON" : " OFF");
        char const* key = t[i].key;
        button(menu, txt, {15 + cw * (i % 2 + 0.5f), H - 48 - (i / 2) * 36.f}, cw - 6, [this, key] { toggleBool(key); }, on, 30.f);
    }
    float y = H - 48 - 4 * 36.f - 6;
    label("PLAYER", {16, y}, 0.34f, {0, 0.5f});
    std::string ps = cfg::s("player-select");
    char const* pk[] = {"p1", "p2", "both"}; char const* pn[] = {"P1", "P2", "BOTH"};
    for (int i = 0; i < 3; ++i) {
        std::string k = pk[i];
        button(menu, pn[i], {110 + i * 70.f, y}, 64, [this, k] { Mod::get()->setSettingValue<std::string>("player-select", k); go(Page::Settings); }, ps == k, 26.f);
    }
    std::string am = cfg::s("analysis-mode");
    char const* ak[] = {"live", "analysis", "compare"}; char const* an[] = {"LIVE", "ANALYSIS", "COMPARE"};
    for (int i = 0; i < 3; ++i) {
        std::string k = ak[i];
        button(menu, an[i], {W / 2 + 30 + i * 74.f, y}, 70, [this, k] { Mod::get()->setSettingValue<std::string>("analysis-mode", k); go(Page::Settings); }, am == k, 26.f);
    }
    label("All settings: Geode > Advanced Frame Counter > Settings", {W / 2, 54}, 0.26f)->setOpacity(130);
    button(menu, "BACK", {W - 52, 24}, 80, [this] { go(Page::Main); });
}

void MainPanel::buildInfo() {
    float W = m_size.width, H = m_size.height;
    Method m = static_cast<Method>(m_infoMethod);
    auto& mo = Engine::get().methodObj(m);
    label(std::string(methodName(m)) + "  " + methodShort(m), {W / 2, H - 18}, 0.5f);
    auto wrap = [&](std::string txt, float y, float scale, int maxLines, int cols) {
        size_t pos = 0; int n = 0;
        while (pos < txt.size() && n < maxLines) {
            size_t cut = std::min(pos + static_cast<size_t>(cols), txt.size());
            if (cut < txt.size()) { size_t sp = txt.rfind(' ', cut); if (sp != std::string::npos && sp > pos) cut = sp; }
            label(txt.substr(pos, cut - pos), {16, y - n * 13.f}, scale, {0, 0.5f});
            pos = cut + (cut < txt.size() ? 1 : 0); ++n;
        }
    };
    label("HOW IT WORKS", {16, H - 48}, 0.34f, {0, 0.5f})->setColor({255, 220, 90});
    wrap(mo.description(), H - 64, 0.3f, 5, 76);
    label("LIMITATIONS", {16, H - 140}, 0.34f, {0, 0.5f})->setColor({255, 140, 90});
    wrap(mo.limitations(), H - 156, 0.3f, 5, 76);
    auto menu = newMenu(m_content);
    for (int k = 1; k <= 3; ++k)
        button(menu, "M" + std::to_string(k), {40 + (k - 1) * 70.f, 24}, 60, [this, k] { m_infoMethod = k; go(Page::Info); }, m_infoMethod == k, 28.f);
    button(menu, "BACK", {W - 52, 24}, 80, [this] { go(Page::Main); });
}

// ------------------------------------------------------------ live refresh

void MainPanel::update(float) {
    auto& eng = Engine::get();

    if (!eng.isPaused()) {
        close();
        return;
    }

    if (m_progress) {
        auto p = eng.progress();
        m_progress->setString(p.label.c_str());
    }

    if (m_recLabel) {
        auto& rec = InputRecorder::get();

        std::string s;

        if (rec.recording()) {
            s =
                "ARMED   " +
                std::to_string(rec.run().pressCount()) +
                " presses   " +
                std::to_string(rec.run().inputs.size()) +
                " events";
        }
        else if (rec.hasRun()) {
            s =
                "READY   " +
                std::to_string(rec.run().pressCount()) +
                " presses   " +
                std::to_string(rec.run().inputs.size()) +
                " events";
        }
        else {
            s = "NO RECORDING";
        }

        m_recLabel->setString(s.c_str());
    }
}

} // namespace afc
