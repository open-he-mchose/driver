// SPDX-License-Identifier: MIT
#include "mchose/glw/keycode.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "mchose/glw/structs.hpp"

namespace mchose::glw {

namespace {

struct UsageName {
    std::uint8_t usage;
    const char* name;
};

constexpr UsageName kUsages[] = {
    {0x04, "A"}, {0x05, "B"}, {0x06, "C"}, {0x07, "D"}, {0x08, "E"}, {0x09, "F"}, {0x0A, "G"}, {0x0B, "H"},
    {0x0C, "I"}, {0x0D, "J"}, {0x0E, "K"}, {0x0F, "L"}, {0x10, "M"}, {0x11, "N"}, {0x12, "O"}, {0x13, "P"},
    {0x14, "Q"}, {0x15, "R"}, {0x16, "S"}, {0x17, "T"}, {0x18, "U"}, {0x19, "V"}, {0x1A, "W"}, {0x1B, "X"},
    {0x1C, "Y"}, {0x1D, "Z"}, {0x1E, "1"}, {0x1F, "2"}, {0x20, "3"}, {0x21, "4"}, {0x22, "5"}, {0x23, "6"},
    {0x24, "7"}, {0x25, "8"}, {0x26, "9"}, {0x27, "0"}, {0x28, "Enter"}, {0x29, "Esc"}, {0x2A, "Backspace"},
    {0x2B, "Tab"}, {0x2C, "Space"}, {0x2D, "-"}, {0x2E, "="}, {0x2F, "["}, {0x30, "]"}, {0x31, "\\"},
    {0x33, ";"}, {0x34, "'"}, {0x35, "`"}, {0x36, ","}, {0x37, "."}, {0x38, "/"}, {0x39, "CapsLock"},
    {0x3A, "F1"}, {0x3B, "F2"}, {0x3C, "F3"}, {0x3D, "F4"}, {0x3E, "F5"}, {0x3F, "F6"}, {0x40, "F7"},
    {0x41, "F8"}, {0x42, "F9"}, {0x43, "F10"}, {0x44, "F11"}, {0x45, "F12"}, {0x46, "PrtSc"},
    {0x47, "ScrLk"}, {0x48, "Pause"}, {0x49, "Insert"}, {0x4A, "Home"}, {0x4B, "PgUp"}, {0x4C, "Delete"},
    {0x4D, "End"}, {0x4E, "PgDn"}, {0x4F, "Right"}, {0x50, "Left"}, {0x51, "Down"}, {0x52, "Up"},
    {0x53, "NumLock"}, {0x54, "KP/"}, {0x55, "KP*"}, {0x56, "KP-"}, {0x57, "KP+"}, {0x58, "KPEnter"},
    {0x59, "KP1"}, {0x5A, "KP2"}, {0x5B, "KP3"}, {0x5C, "KP4"}, {0x5D, "KP5"}, {0x5E, "KP6"}, {0x5F, "KP7"},
    {0x60, "KP8"}, {0x61, "KP9"}, {0x62, "KP0"}, {0x63, "KP."}, {0x64, "NonUS\\"}, {0x65, "Menu"},
    {0x68, "F13"}, {0x69, "F14"}, {0x6A, "F15"}, {0x6B, "F16"}, {0x6C, "F17"}, {0x6D, "F18"}, {0x6E, "F19"},
    {0x6F, "F20"}, {0x70, "F21"}, {0x71, "F22"}, {0x72, "F23"}, {0x73, "F24"},
};

constexpr const char* kModifiers[8] = {"LCtrl", "LShift", "LAlt", "LGui", "RCtrl", "RShift", "RAlt", "RGui"};

struct ConsumerName {
    std::uint16_t usage;
    const char* name;
};

constexpr ConsumerName kConsumer[] = {
    {0xE2, "Mute"}, {0xE9, "VolumeUp"}, {0xEA, "VolumeDown"}, {0xCD, "PlayPause"},
    {0xB5, "NextTrack"}, {0xB6, "PrevTrack"}, {0xB7, "Stop"}, {0x6F, "ScreenBrightnessUp"},
    {0x70, "ScreenBrightnessDown"}, {0x183, "MediaPlayer"}, {0x18A, "Mail"}, {0x192, "Calculator"},
    {0x194, "MyComputer"}, {0x221, "WebSearch"}, {0x223, "WebHome"},
};

struct SpecialName {
    std::uint8_t code;
    const char* name;
};

// Behaviour on Jet75-II firmware V116 (https://open-he-mchose.github.io/protocols/glw/keymap-and-keycodes/#special-codes-f0-xx-yy).
constexpr SpecialName kSpecial[] = {
    {0xFA, "next-profile"}, {0x04, "windows-mode"}, {0x05, "mac-mode"}, {0x02, "win-lock"},
    {0x32, "light-brightness+"}, {0x33, "light-brightness-"}, {0x36, "light-speed+"}, {0x37, "light-speed-"},
    {0x2F, "light-effect+"}, {0x30, "light-effect+"}, {0x2E, "light-effect-"}, {0x3E, "light-color+"},
    {0x3C, "light-color+"}, {0x3D, "light-color-"}, {0x3B, "light-direction"}, {0x35, "light-on/off"},
    {0x2C, "light-effect-off"}, {0xA0, "side-effect+"}, {0x15, "side-brightness+"}, {0x16, "side-brightness-"},
    {0x18, "side-speed+"}, {0x19, "side-speed+"}, {0x17, "side-speed-"}, {0x1A, "side-color+"},
    {0x1B, "side-color-"}, {0x1C, "side-on/off"}, {0x03, "wasd-swap"}, {0x2B, "per-key-color-mode"},
    {0x51, "Win+Space"}, {0x53, "Shift+Win+3"}, {0x54, "Shift+Win+4"}, {0x57, "Shift+F12"}, {0x58, "Win+Q"},
    {0x08, "FACTORY-RESET(hold 3s)"}, {0xFE, "CALIBRATION-RESET(hold 3s)"}, {0x07, "BOOTLOADER"},
};

std::string raw_name(const KeyCode& k)
{
    char buf[16];
    std::snprintf(buf, sizeof buf, "[%02x %02x %02x]", k.type, k.code1, k.code2);
    return buf;
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(const std::string& s)
{
    auto b = s.find_first_not_of(" \t");
    auto e = s.find_last_not_of(" \t");
    return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
}

std::optional<std::uint8_t> modifier_bit(const std::string& name)
{
    const std::string n = lower(name);
    if (n == "ctrl" || n == "control") return 0x01;
    if (n == "shift") return 0x02;
    if (n == "alt") return 0x04;
    if (n == "gui" || n == "win" || n == "super" || n == "cmd") return 0x08;
    for (int i = 0; i < 8; ++i)
        if (n == lower(kModifiers[i]))
            return static_cast<std::uint8_t>(1u << i);
    return std::nullopt;
}

std::optional<std::uint8_t> usage_by_name(const std::string& name)
{
    const std::string n = lower(name);
    for (const auto& u : kUsages)
        if (lower(u.name) == n)
            return u.usage;
    if (n == "return") return 0x28;
    if (n == "escape") return 0x29;
    if (n == "del") return 0x4C;
    return std::nullopt;
}

}  // namespace

const char* hid_usage_name(std::uint8_t usage)
{
    for (const auto& u : kUsages)
        if (u.usage == usage)
            return u.name;
    return nullptr;
}

bool KeyCode::is_dangerous() const
{
    return type == Special && (code1 == 0x07 || code1 == 0x08 || code1 == 0xFE);
}

std::string to_string(const KeyCode& k)
{
    switch (k.type) {
    case 0x00:
        if (k.code1 == 0 && k.code2 == 0)
            return "none";
        break;
    case KeyCode::Keyboard: {
        if (k.code1 == 0 && k.code2 == 0)
            return "transparent";
        std::string s;
        for (int i = 0; i < 8; ++i)
            if (k.code1 & (1u << i)) {
                if (!s.empty())
                    s += '+';
                s += kModifiers[i];
            }
        if (k.code2) {
            const char* n = hid_usage_name(k.code2);
            char buf[16];
            if (!n) {
                std::snprintf(buf, sizeof buf, "usage%02X", k.code2);
                n = buf;
            }
            if (!s.empty())
                s += '+';
            s += n;
        }
        return s;
    }
    case KeyCode::Consumer: {
        const std::uint16_t usage = static_cast<std::uint16_t>(k.code1 | (k.code2 << 8));
        for (const auto& c : kConsumer)
            if (c.usage == usage)
                return c.name;
        char buf[24];
        std::snprintf(buf, sizeof buf, "consumer%04X", usage);
        return buf;
    }
    case KeyCode::Macro: {
        std::string mode = k.code2 == 0 ? "while-held" : k.code2 == 0xFF ? "toggle" : std::to_string(k.code2) + "x";
        return "M" + std::to_string(k.code1 + 1) + " " + mode;
    }
    case KeyCode::Dks: return "DKS slot " + std::to_string(k.code1);
    case KeyCode::Toggle: return "Toggle slot " + std::to_string(k.code1);
    case KeyCode::ModTap: return "ModTap slot " + std::to_string(k.code1) + " " + std::to_string(k.code2 * 10) + "ms";
    case KeyCode::RapidStrike: return "RS slot " + std::to_string(k.code1) + " with key " + std::to_string(k.code2);
    case KeyCode::Socd: return "SOCD slot " + std::to_string(k.code1) + " with key " + std::to_string(k.code2);
    case KeyCode::Lt: return "LT slot " + std::to_string(k.code1) + " with key " + std::to_string(k.code2);
    case KeyCode::Special:
        if (k.is_unused_slot())
            return "unused";
        if (k.code1 == 0xFF)
            return "Fn(layer " + std::to_string(k.code2) + ")";
        for (const auto& s : kSpecial)
            if (s.code == k.code1)
                return s.name;
        break;
    default: break;
    }
    return raw_name(k);
}

std::optional<KeyCode> parse_keycode(const std::string& input)
{
    const std::string text = trim(input);
    if (text.empty())
        return std::nullopt;
    const std::string l = lower(text);
    if (l == "none" || l == "disabled")
        return KeyCode{0, 0, 0};
    if (l == "transparent")
        return KeyCode{KeyCode::Keyboard, 0, 0};

    // Raw hex: exactly three bytes.
    try {
        auto raw = from_hex(text);
        bool looks_hex = text.find(' ') != std::string::npos || text.find(':') != std::string::npos ||
                         text.size() == 6;
        if (looks_hex && raw.size() == 3)
            return KeyCode{raw[0], raw[1], raw[2]};
    } catch (const std::invalid_argument&) {
    }

    for (const auto& c : kConsumer)
        if (lower(c.name) == l)
            return KeyCode{KeyCode::Consumer, static_cast<std::uint8_t>(c.usage & 0xFF),
                           static_cast<std::uint8_t>(c.usage >> 8)};

    // Modifier combinations: "Ctrl+Shift+T", "LShift", "A". The key itself may be "+" or "-".
    std::vector<std::string> parts;
    std::string cur;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '+' && !cur.empty()) {
            parts.push_back(trim(cur));
            cur.clear();
        } else {
            cur += text[i];
        }
    }
    parts.push_back(trim(cur));

    KeyCode k{KeyCode::Keyboard, 0, 0};
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (auto bit = modifier_bit(parts[i])) {
            k.code1 |= *bit;
            continue;
        }
        auto usage = usage_by_name(parts[i]);
        if (!usage || k.code2 != 0)
            return std::nullopt;
        k.code2 = *usage;
    }
    if (k.code1 == 0 && k.code2 == 0)
        return std::nullopt;
    return k;
}

}  // namespace mchose::glw
