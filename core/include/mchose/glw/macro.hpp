// SPDX-License-Identifier: MIT
// Macro area codec (https://open-he-mchose.github.io/protocols/glw/macros/).
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace mchose::glw {

inline constexpr int kMacroCount = 16;

struct MacroAction {
    enum class Kind : std::uint8_t { Delay = 0, Modifier = 1, Key = 2, Mouse = 3 };
    Kind kind = Kind::Key;
    std::uint8_t code = 0;        // modifier bit mask / HID usage / mouse button mask
    bool press = true;            // false = release
    std::uint16_t delay_ms = 0;   // pause after this action

    friend bool operator==(const MacroAction&, const MacroAction&) = default;
};

using Macro = std::vector<MacroAction>;
using MacroSet = std::array<Macro, kMacroCount>;

// 4-byte record of one action; `last` sets the end bit. A Delay action is written the way the official
// driver does it: type 2 (key), release, code 0.
std::array<std::uint8_t, 4> encode_action(const MacroAction& a, bool last);
MacroAction decode_action(std::span<const std::uint8_t, 4> record, bool* last = nullptr);

// Build the whole area in the official driver's layout: 16 offsets, header (81 7E, types, "MC",
// cloud-info offset), one shared empty macro, the records, 16 empty cloud-info entries.
// `types` = play-mode byte the driver's UI shows per macro (not used by the firmware).
std::vector<std::uint8_t> encode_macro_area(const MacroSet& macros, const std::array<std::uint8_t, kMacroCount>& types = {});

// Decode the 16 macros from an area image (reads through the offset table; stops at end records).
// Throws std::runtime_error on offsets outside the image or a missing end record.
MacroSet decode_macro_area(std::span<const std::uint8_t> area);

// Number of bytes of the area actually used (up to and including the cloud-info block when the
// driver header is present, else up to the last end record).
std::size_t macro_area_used_size(std::span<const std::uint8_t> area);

}  // namespace mchose::glw
