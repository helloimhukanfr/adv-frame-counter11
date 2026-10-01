#pragma once
// Tiny self-contained JSON (no dependencies) so the on-disk run format is
// independent of any Geode/matjson API revision and is unit-testable.
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace afc::json {

struct Value {
    enum class T { Null, Bool, Num, Str, Arr, Obj } t = T::Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Value> a;
    std::vector<std::pair<std::string, Value>> o;

    static Value number(double v) { Value x; x.t = T::Num; x.n = v; return x; }
    static Value boolean(bool v) { Value x; x.t = T::Bool; x.b = v; return x; }
    static Value string(std::string v) { Value x; x.t = T::Str; x.s = std::move(v); return x; }
    static Value array() { Value x; x.t = T::Arr; return x; }
    static Value object() { Value x; x.t = T::Obj; return x; }

    Value& set(std::string k, Value v) { o.emplace_back(std::move(k), std::move(v)); return *this; }
    Value& push(Value v) { a.push_back(std::move(v)); return *this; }

    Value const* get(std::string const& k) const {
        for (auto const& kv : o) if (kv.first == k) return &kv.second;
        return nullptr;
    }
    double num(std::string const& k, double def = 0) const {
        auto v = get(k); return (v && v->t == T::Num) ? v->n : def;
    }
    bool flag(std::string const& k, bool def = false) const {
        auto v = get(k); return (v && v->t == T::Bool) ? v->b : def;
    }
    std::string str(std::string const& k, std::string def = "") const {
        auto v = get(k); return (v && v->t == T::Str) ? v->s : def;
    }

    static void esc(std::string& out, std::string const& s) {
        out += '"';
        for (unsigned char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
                    else out += static_cast<char>(c);
            }
        }
        out += '"';
    }

    void dump(std::string& out) const {
        switch (t) {
            case T::Null: out += "null"; break;
            case T::Bool: out += b ? "true" : "false"; break;
            case T::Num: {
                char buf[40];
                if (n == static_cast<double>(static_cast<long long>(n)))
                    std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(n));
                else std::snprintf(buf, sizeof buf, "%.9g", n);
                out += buf; break;
            }
            case T::Str: esc(out, s); break;
            case T::Arr:
                out += '[';
                for (size_t i = 0; i < a.size(); ++i) { if (i) out += ','; a[i].dump(out); }
                out += ']'; break;
            case T::Obj:
                out += '{';
                for (size_t i = 0; i < o.size(); ++i) {
                    if (i) out += ',';
                    esc(out, o[i].first); out += ':'; o[i].second.dump(out);
                }
                out += '}'; break;
        }
    }
    std::string dump() const { std::string r; dump(r); return r; }
};

class Parser {
public:
    explicit Parser(std::string const& src) : p(src.c_str()), end(src.c_str() + src.size()) {}
    bool run(Value& out, std::string* err) {
        ws();
        if (!value(out, 0)) { if (err) *err = msg; return false; }
        ws();
        if (p != end) { if (err) *err = "trailing characters"; return false; }
        return true;
    }
private:
    const char* p; const char* end; std::string msg;
    bool fail(const char* m) { msg = m; return false; }
    void ws() { while (p < end && std::isspace(static_cast<unsigned char>(*p))) ++p; }
    bool lit(const char* w) {
        size_t n = std::char_traits<char>::length(w);
        if (static_cast<size_t>(end - p) >= n && std::string(p, n) == w) { p += n; return true; }
        return false;
    }
    bool str(std::string& r) {
        if (p >= end || *p != '"') return fail("expected string");
        ++p;
        while (p < end && *p != '"') {
            if (*p == '\\') {
                if (++p >= end) return fail("bad escape");
                switch (*p) {
                    case 'n': r += '\n'; break; case 't': r += '\t'; break; case 'r': r += '\r'; break;
                    case '"': r += '"'; break; case '\\': r += '\\'; break; case '/': r += '/'; break;
                    case 'u': {
                        if (end - p < 5) return fail("bad unicode");
                        unsigned v = static_cast<unsigned>(std::strtoul(std::string(p + 1, 4).c_str(), nullptr, 16));
                        if (v < 0x80) r += static_cast<char>(v);
                        else if (v < 0x800) { r += static_cast<char>(0xC0 | (v >> 6)); r += static_cast<char>(0x80 | (v & 0x3F)); }
                        else { r += static_cast<char>(0xE0 | (v >> 12)); r += static_cast<char>(0x80 | ((v >> 6) & 0x3F)); r += static_cast<char>(0x80 | (v & 0x3F)); }
                        p += 4; break;
                    }
                    default: return fail("bad escape");
                }
                ++p;
            } else r += *p++;
        }
        if (p >= end) return fail("unterminated string");
        ++p; return true;
    }
    bool value(Value& v, int depth) {
        if (depth > 64) return fail("too deep");
        ws();
        if (p >= end) return fail("unexpected end");
        if (*p == '{') {
            ++p; v = Value::object(); ws();
            if (p < end && *p == '}') { ++p; return true; }
            while (true) {
                ws(); std::string k; if (!str(k)) return false;
                ws(); if (p >= end || *p != ':') return fail("expected ':'"); ++p;
                Value c; if (!value(c, depth + 1)) return false;
                v.o.emplace_back(std::move(k), std::move(c)); ws();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == '}') { ++p; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (*p == '[') {
            ++p; v = Value::array(); ws();
            if (p < end && *p == ']') { ++p; return true; }
            while (true) {
                Value c; if (!value(c, depth + 1)) return false;
                v.a.push_back(std::move(c)); ws();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == ']') { ++p; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (*p == '"') { v.t = Value::T::Str; return str(v.s); }
        if (lit("true")) { v = Value::boolean(true); return true; }
        if (lit("false")) { v = Value::boolean(false); return true; }
        if (lit("null")) { v = Value(); return true; }
        char* e = nullptr;
        double d = std::strtod(p, &e);
        if (e == p) return fail("unexpected character");
        v = Value::number(d); p = e; return true;
    }
};

inline bool parse(std::string const& src, Value& out, std::string* err = nullptr) {
    return Parser(src).run(out, err);
}

} // namespace afc::json
