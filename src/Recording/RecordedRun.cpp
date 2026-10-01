#include "RecordedRun.hpp"

namespace afc {

json::Value RecordedRun::toJson() const {
    using json::Value;
    Value root = Value::object();
    root.set("format", Value::string("advanced-frame-counter-run"));
    root.set("version", Value::number(version));
    root.set("timingConvention", Value::string(timingConvention));
    root.set("source", Value::string(source));
    root.set("method", Value::string(recordedWithMethod));
    Value lvl = Value::object();
    lvl.set("id", Value::number(levelID));
    lvl.set("name", Value::string(levelName));
    lvl.set("platformer", Value::boolean(platformer));
    root.set("level", std::move(lvl));
    root.set("tps", Value::number(tps));
    root.set("durationTicks", Value::number(durationTicks));
    Value arr = Value::array();
    for (auto const& e : inputs) {
        Value o = Value::object();
        o.set("t", Value::number(e.tick));
        o.set("p", Value::number(e.player));
        o.set("b", Value::number(e.button));
        o.set("d", Value::boolean(e.down));
        o.set("x", Value::number(e.x));
        o.set("y", Value::number(e.y));
        o.set("m", Value::number(e.mode));
        o.set("u", Value::boolean(e.dual));
        arr.push(std::move(o));
    }
    root.set("inputs", std::move(arr));
    return root;
}

bool RecordedRun::validate(std::string& err) const {
    if (tps < 1 || tps > 100000) { err = "invalid tps"; return false; }
    if (durationTicks < 0) { err = "negative duration"; return false; }
    int last = 0;
    for (size_t i = 0; i < inputs.size(); ++i) {
        auto const& e = inputs[i];
        if (e.tick < 0) { err = "negative tick at input " + std::to_string(i); return false; }
        if (e.tick < last) { err = "ticks not ordered at input " + std::to_string(i); return false; }
        if (e.player != 1 && e.player != 2) { err = "bad player at input " + std::to_string(i); return false; }
        if (e.button < 1 || e.button > 3) { err = "bad button at input " + std::to_string(i); return false; }
        last = e.tick;
    }
    if (!inputs.empty() && inputs.back().tick > durationTicks) { err = "input beyond duration"; return false; }
    return true;
}

bool RecordedRun::fromJson(json::Value const& v, RecordedRun& out, std::string& err) {
    using json::Value;
    if (v.t != Value::T::Obj) { err = "not an object"; return false; }
    if (v.str("format") != "advanced-frame-counter-run") { err = "wrong format tag"; return false; }
    RecordedRun r;
    r.version = static_cast<int>(v.num("version", 0));
    if (r.version < 1) { err = "missing version"; return false; }
    if (r.version > kVersion) { err = "run was saved by a newer version"; return false; }
    r.timingConvention = v.str("timingConvention", r.timingConvention);
    r.source = v.str("source", r.source);
    r.recordedWithMethod = v.str("method", r.recordedWithMethod);
    if (auto l = v.get("level"); l && l->t == Value::T::Obj) {
        r.levelID = static_cast<int>(l->num("id", 0));
        r.levelName = l->str("name");
        r.platformer = l->flag("platformer");
    } else { err = "missing level info"; return false; }
    r.tps = static_cast<int>(v.num("tps", 0));
    r.durationTicks = static_cast<int>(v.num("durationTicks", -1));
    auto arr = v.get("inputs");
    if (!arr || arr->t != Value::T::Arr) { err = "missing inputs"; return false; }
    r.inputs.reserve(arr->a.size());
    for (auto const& o : arr->a) {
        if (o.t != Value::T::Obj) { err = "bad input entry"; return false; }
        InputEvent e;
        e.tick = static_cast<int>(o.num("t", -1));
        e.player = static_cast<int>(o.num("p", 1));
        e.button = static_cast<int>(o.num("b", 1));
        e.down = o.flag("d", true);
        e.x = static_cast<float>(o.num("x", 0));
        e.y = static_cast<float>(o.num("y", 0));
        e.mode = static_cast<int>(o.num("m", 0));
        e.dual = o.flag("u", false);
        r.inputs.push_back(e);
    }
    if (!r.validate(err)) return false;
    out = std::move(r);
    return true;
}

} // namespace afc
