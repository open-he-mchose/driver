// SPDX-License-Identifier: MIT
#include "json.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace json {

namespace {

void escape(std::string& out, const std::string& s)
{
    out += '"';
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                out += buf;
            } else {
                out += c;
            }
        }
    }
    out += '"';
}

void dump_to(std::string& out, const Value& v, int indent, int level)
{
    const std::string nl = indent ? "\n" : "";
    auto pad = [&](int l) { return std::string(static_cast<std::size_t>(indent * l), ' '); };
    if (std::holds_alternative<std::nullptr_t>(v.v)) out += "null";
    else if (auto* b = std::get_if<bool>(&v.v)) out += *b ? "true" : "false";
    else if (auto* d = std::get_if<double>(&v.v)) {
        char buf[32];
        if (std::floor(*d) == *d && std::fabs(*d) < 1e15) std::snprintf(buf, sizeof buf, "%.0f", *d);
        else std::snprintf(buf, sizeof buf, "%.17g", *d);
        out += buf;
    } else if (auto* s = std::get_if<std::string>(&v.v)) escape(out, *s);
    else if (auto* a = std::get_if<Array>(&v.v)) {
        out += "[";
        for (std::size_t i = 0; i < a->size(); ++i) {
            out += i ? "," + nl : nl;
            out += pad(level + 1);
            dump_to(out, (*a)[i], indent, level + 1);
        }
        out += a->empty() ? "]" : nl + pad(level) + "]";
    } else if (auto* o = std::get_if<Object>(&v.v)) {
        out += "{";
        bool first = true;
        for (const auto& [k, val] : *o) {
            out += first ? nl : "," + nl;
            first = false;
            out += pad(level + 1);
            escape(out, k);
            out += indent ? ": " : ":";
            dump_to(out, val, indent, level + 1);
        }
        out += o->empty() ? "}" : nl + pad(level) + "}";
    }
}

struct Parser {
    const std::string& s;
    std::size_t i = 0;

    [[noreturn]] void error(const char* what) const
    {
        throw std::runtime_error("JSON parse error at byte " + std::to_string(i) + ": " + what);
    }
    void ws()
    {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
            ++i;
    }
    bool eat(char c)
    {
        ws();
        if (i < s.size() && s[i] == c) {
            ++i;
            return true;
        }
        return false;
    }
    Value value()
    {
        ws();
        if (i >= s.size()) error("unexpected end");
        char c = s[i];
        if (c == '{') return object();
        if (c == '[') return array();
        if (c == '"') return Value(string());
        if (s.compare(i, 4, "true") == 0) { i += 4; return Value(true); }
        if (s.compare(i, 5, "false") == 0) { i += 5; return Value(false); }
        if (s.compare(i, 4, "null") == 0) { i += 4; return Value(nullptr); }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            char* end = nullptr;
            double d = std::strtod(s.c_str() + i, &end);
            i = static_cast<std::size_t>(end - s.c_str());
            return Value(d);
        }
        error("unexpected character");
    }
    std::string string()
    {
        if (!eat('"')) error("expected string");
        std::string out;
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c == '\\') {
                if (i >= s.size()) error("bad escape");
                char e = s[i++];
                switch (e) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    if (i + 4 > s.size()) error("bad \\u escape");
                    int code = std::stoi(s.substr(i, 4), nullptr, 16);
                    i += 4;
                    out += code < 0x80 ? static_cast<char>(code) : '?';
                    break;
                }
                default: out += e;
                }
            } else {
                out += c;
            }
        }
        if (i >= s.size()) error("unterminated string");
        ++i;
        return out;
    }
    Value array()
    {
        eat('[');
        Array a;
        if (eat(']')) return Value(std::move(a));
        do a.push_back(value()); while (eat(','));
        if (!eat(']')) error("expected ]");
        return Value(std::move(a));
    }
    Value object()
    {
        eat('{');
        Object o;
        if (eat('}')) return Value(std::move(o));
        do {
            ws();
            std::string k = string();
            if (!eat(':')) error("expected :");
            o.emplace_back(std::move(k), value());
        } while (eat(','));
        if (!eat('}')) error("expected }");
        return Value(std::move(o));
    }
};

}  // namespace

std::string dump(const Value& v, int indent)
{
    std::string out;
    dump_to(out, v, indent, 0);
    return out;
}

Value parse(const std::string& text)
{
    Parser p{text};
    Value v = p.value();
    p.ws();
    if (p.i != text.size())
        p.error("trailing data");
    return v;
}

}  // namespace json
