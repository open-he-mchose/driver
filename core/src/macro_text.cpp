// SPDX-License-Identifier: MIT
#include "mchose/glw/macro_text.hpp"

#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "mchose/glw/keycode.hpp"

namespace mchose::glw {

namespace {

using Kind = MacroAction::Kind;

constexpr const char* kModifierNames[8] = {"LCtrl", "LShift", "LAlt", "LGui", "RCtrl", "RShift", "RAlt", "RGui"};

int single_bit(std::uint8_t mask)
{
    for (int i = 0; i < 8; ++i)
        if (mask == (1u << i))
            return i;
    return -1;
}

// US layout: character -> (usage, needs shift).
bool char_to_usage(char c, std::uint8_t& usage, bool& shift)
{
    static const char* kNormal = "1234567890";
    static const char* kShifted = "!@#$%^&*()";
    shift = false;
    if (c >= 'a' && c <= 'z') { usage = static_cast<std::uint8_t>(0x04 + (c - 'a')); return true; }
    if (c >= 'A' && c <= 'Z') { usage = static_cast<std::uint8_t>(0x04 + (c - 'A')); shift = true; return true; }
    for (int i = 0; i < 10; ++i) {
        if (c == kNormal[i]) { usage = static_cast<std::uint8_t>(0x1E + i); return true; }
        if (c == kShifted[i]) { usage = static_cast<std::uint8_t>(0x1E + i); shift = true; return true; }
    }
    struct P { char normal, shifted; std::uint8_t usage; };
    static const P kPunct[] = {{'-', '_', 0x2D}, {'=', '+', 0x2E}, {'[', '{', 0x2F}, {']', '}', 0x30},
                               {'\\', '|', 0x31}, {';', ':', 0x33}, {'\'', '"', 0x34}, {'`', '~', 0x35},
                               {',', '<', 0x36}, {'.', '>', 0x37}, {'/', '?', 0x38}};
    for (const auto& p : kPunct) {
        if (c == p.normal) { usage = p.usage; return true; }
        if (c == p.shifted) { usage = p.usage; shift = true; return true; }
    }
    if (c == ' ') { usage = 0x2C; return true; }
    if (c == '\n') { usage = 0x28; return true; }
    if (c == '\t') { usage = 0x2B; return true; }
    return false;
}

std::vector<std::string> tokenize(const std::string& text)
{
    std::vector<std::string> out;
    std::string cur;
    bool quoted = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (quoted) {
            if (c == '\\' && i + 1 < text.size()) {
                char n = text[++i];
                cur += n == 'n' ? '\n' : n == 't' ? '\t' : n;
            } else if (c == '"') {
                cur += '"';
                out.push_back(cur);
                cur.clear();
                quoted = false;
            } else {
                cur += c;
            }
        } else if (c == '"') {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            cur = "\"";
            quoted = true;
        } else if (std::isspace(static_cast<unsigned char>(c)) || c == ',') {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (quoted)
        throw std::invalid_argument("macro: unterminated quote");
    if (!cur.empty())
        out.push_back(cur);
    return out;
}

bool parse_delay(const std::string& t, std::uint16_t& ms)
{
    if (t.size() < 3 || t.compare(t.size() - 2, 2, "ms") != 0)
        return false;
    const std::string num = t.substr(0, t.size() - 2);
    if (num.empty() || num.find_first_not_of("0123456789") != std::string::npos)
        return false;
    const unsigned long v = std::stoul(num);
    if (v > 65535)
        throw std::invalid_argument("macro: delay above 65535 ms: " + t);
    ms = static_cast<std::uint16_t>(v);
    return true;
}

}  // namespace

Macro parse_macro(const std::string& text, std::uint16_t default_delay_ms)
{
    Macro m;
    auto add = [&](Kind kind, std::uint8_t code, bool press) { m.push_back({kind, code, press, default_delay_ms}); };

    for (const std::string& tok : tokenize(text)) {
        std::uint16_t ms = 0;
        if (tok == "wait") {   // explicit pause record; its length follows as "Nms"
            m.push_back({Kind::Delay, 0, false, default_delay_ms});
            continue;
        }
        if (parse_delay(tok, ms)) {
            if (m.empty())
                m.push_back({Kind::Delay, 0, false, ms});
            else
                m.back().delay_ms = ms;
            continue;
        }
        if (tok.front() == '"') {
            const std::string s = tok.substr(1, tok.size() - 2);
            for (char c : s) {
                std::uint8_t usage = 0;
                bool shift = false;
                if (!char_to_usage(c, usage, shift))
                    throw std::invalid_argument(std::string("macro: cannot type character '") + c + "' (US layout)");
                if (shift) add(Kind::Modifier, 0x02, true);
                add(Kind::Key, usage, true);
                add(Kind::Key, usage, false);
                if (shift) add(Kind::Modifier, 0x02, false);
            }
            continue;
        }
        const bool press = tok.front() == '+';
        const bool release = tok.front() == '-' && tok.size() > 1;
        const std::string name = (press || release) ? tok.substr(1) : tok;
        auto k = parse_keycode(name);
        if (!k || k->type != KeyCode::Keyboard || (k->code1 == 0 && k->code2 == 0))
            throw std::invalid_argument("macro: unknown key '" + name + "'");
        if (press || release) {
            if (k->code1 && k->code2)
                throw std::invalid_argument("macro: +/- take a single key, not a combination: '" + tok + "'");
            if (k->code1) {
                if (single_bit(k->code1) < 0)
                    throw std::invalid_argument("macro: one modifier at a time: '" + tok + "'");
                add(Kind::Modifier, k->code1, press);
            } else {
                add(Kind::Key, k->code2, press);
            }
            continue;
        }
        // Tap: modifiers down (one record per modifier), key down, key up, modifiers up.
        std::vector<std::uint8_t> mods;
        for (int i = 0; i < 8; ++i)
            if (k->code1 & (1u << i))
                mods.push_back(static_cast<std::uint8_t>(1u << i));
        for (auto b : mods) add(Kind::Modifier, b, true);
        if (k->code2) {
            add(Kind::Key, k->code2, true);
            add(Kind::Key, k->code2, false);
        }
        for (auto it = mods.rbegin(); it != mods.rend(); ++it) add(Kind::Modifier, *it, false);
    }
    return m;
}

std::string format_macro(const Macro& m)
{
    std::string out;
    for (std::size_t i = 0; i < m.size(); ++i) {
        const auto& a = m[i];
        if (!out.empty())
            out += ' ';
        switch (a.kind) {
        case Kind::Delay:
            out += "wait";
            break;
        case Kind::Modifier: {
            const int bit = single_bit(a.code);
            out += a.press ? '+' : '-';
            out += bit >= 0 ? kModifierNames[bit] : "mods" + std::to_string(a.code);
            break;
        }
        case Kind::Key: {
            const char* n = hid_usage_name(a.code);
            out += a.press ? '+' : '-';
            if (a.code == 0x36)
                n = "Comma";   // "," separates tokens
            out += n ? n : "usage" + std::to_string(a.code);
            break;
        }
        case Kind::Mouse:
            out += a.press ? "+mouse" : "-mouse";
            out += std::to_string(a.code);
            break;
        }
        out += ' ';
        out += std::to_string(a.delay_ms) + "ms";
    }
    return out;
}

}  // namespace mchose::glw
