// SPDX-License-Identifier: MIT
// 3-byte key codes `[type, code1, code2]` (https://open-he-mchose.github.io/protocols/glw/keymap-and-keycodes/).
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace mchose::glw {

struct KeyCode {
    std::uint8_t type = 0;
    std::uint8_t code1 = 0;
    std::uint8_t code2 = 0;

    // Key code types.
    static constexpr std::uint8_t Keyboard = 0x10;   // code1 = modifier bits, code2 = HID usage
    static constexpr std::uint8_t Consumer = 0x30;   // code1/code2 = usage low/high
    static constexpr std::uint8_t Macro = 0x70;      // code1 = macro index, code2 = play mode
    static constexpr std::uint8_t Dks = 0x90;
    static constexpr std::uint8_t Toggle = 0x91;
    static constexpr std::uint8_t ModTap = 0x92;
    static constexpr std::uint8_t RapidStrike = 0x93;
    static constexpr std::uint8_t Socd = 0x94;
    static constexpr std::uint8_t Lt = 0x95;
    static constexpr std::uint8_t Special = 0xF0;

    friend bool operator==(const KeyCode&, const KeyCode&) = default;

    std::array<std::uint8_t, 3> bytes() const { return {type, code1, code2}; }

    bool is_advanced() const { return type >= Dks && type <= Lt; }
    bool is_unused_slot() const { return type == 0xF0 && code1 == 0xF0 && code2 == 0xF0; }
    // Special codes that reset or reflash the keyboard (F0 07 bootloader, F0 08 factory reset when held,
    // F0 FE calibration reset when held).
    bool is_dangerous() const;
};

// Readable name: "A", "Ctrl+Shift+C", "Mute", "Fn(layer 1)", "M1 once", "DKS slot 3", "next-profile" ...
// Unknown codes are shown as "[tt cc cc]".
std::string to_string(const KeyCode& k);

// Parse a key name as produced by to_string() for keyboard keys, modifiers and combinations
// ("A", "F12", "LCtrl", "Ctrl+Shift+T"), "none", "transparent", or raw hex "10 00 04" / "10:00:04".
std::optional<KeyCode> parse_keycode(const std::string& text);

// Name of a HID keyboard usage, or nullptr.
const char* hid_usage_name(std::uint8_t usage);

}  // namespace mchose::glw
