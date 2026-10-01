#pragma once
// Typed accessors for every setting in mod.json. Each one is read by real code.
#include "../Core/Types.hpp"
#include <Geode/Geode.hpp>

namespace afc::cfg {

inline bool b(char const* k) { return geode::Mod::get()->getSettingValue<bool>(k); }
inline int i(char const* k) { return static_cast<int>(geode::Mod::get()->getSettingValue<int64_t>(k)); }
inline float f(char const* k) { return static_cast<float>(geode::Mod::get()->getSettingValue<double>(k)); }
inline std::string s(char const* k) { return geode::Mod::get()->getSettingValue<std::string>(k); }

inline bool enabled() { return b("enabled"); }

inline Method method() {
    auto v = s("method");
    if (v == "probe") return Method::StateProbe;
    if (v == "replay") return Method::ReplayProbe;
    return Method::DirectTick;
}
inline void setMethod(Method m) {
    char const* v = m == Method::StateProbe ? "probe" : (m == Method::ReplayProbe ? "replay" : "direct");
    geode::Mod::get()->setSettingValue<std::string>("method", v);
}

enum class PlayerSel { P1, P2, Both };
inline PlayerSel playerSel() {
    auto v = s("player-select");
    return v == "p1" ? PlayerSel::P1 : (v == "p2" ? PlayerSel::P2 : PlayerSel::Both);
}
inline bool showsPlayer(int p) {
    auto s = playerSel();
    return s == PlayerSel::Both || (s == PlayerSel::P1 && p == 1) || (s == PlayerSel::P2 && p == 2);
}

enum class Mode { Live, Analysis, Compare };
inline Mode mode() {
    auto v = s("analysis-mode");
    return v == "analysis" ? Mode::Analysis : (v == "compare" ? Mode::Compare : Mode::Live);
}

} // namespace afc::cfg
