// SPDX-License-Identifier: MIT
#include "mchose/glw/macro.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace mchose::glw {

namespace {

constexpr std::size_t kTableSize = 2 * kMacroCount;    // 32: offsets
constexpr std::size_t kHeaderSize = 32;                // 32..63: driver header
constexpr std::size_t kCloudEntry = 6;

std::uint16_t u16(std::span<const std::uint8_t> a, std::size_t i) { return static_cast<std::uint16_t>(a[i] | (a[i + 1] << 8)); }

}  // namespace

std::array<std::uint8_t, 4> encode_action(const MacroAction& a, bool last)
{
    std::uint8_t type = static_cast<std::uint8_t>(a.kind);
    std::uint8_t code = a.code;
    bool press = a.press;
    if (a.kind == MacroAction::Kind::Delay) {   // as the official driver writes a bare delay
        type = 2;
        code = 0;
        press = false;
    }
    return {static_cast<std::uint8_t>(a.delay_ms & 0xFF), static_cast<std::uint8_t>(a.delay_ms >> 8),
            static_cast<std::uint8_t>(type | (press ? 0x40 : 0) | (last ? 0x80 : 0)), code};
}

MacroAction decode_action(std::span<const std::uint8_t, 4> r, bool* last)
{
    MacroAction a;
    a.delay_ms = static_cast<std::uint16_t>(r[0] | (r[1] << 8));
    const std::uint8_t type = r[2] & 0x0F;
    a.press = r[2] & 0x40;
    a.code = r[3];
    if (last)
        *last = r[2] & 0x80;
    switch (type) {
    case 1: a.kind = MacroAction::Kind::Modifier; break;
    case 2: a.kind = a.code == 0 ? MacroAction::Kind::Delay : MacroAction::Kind::Key; break;
    case 3: a.kind = MacroAction::Kind::Mouse; break;
    default: a.kind = MacroAction::Kind::Delay; break;
    }
    if (a.kind == MacroAction::Kind::Delay) {
        a.code = 0;
        a.press = false;
    }
    return a;
}

std::vector<std::uint8_t> encode_macro_area(const MacroSet& macros, const std::array<std::uint8_t, kMacroCount>& types)
{
    std::vector<std::uint8_t> body{0x00, 0x00, 0x80, 0x00};   // shared empty macro at offset 64
    std::array<std::uint16_t, kMacroCount> offsets{};
    for (int i = 0; i < kMacroCount; ++i) {
        const Macro& m = macros[i];
        if (m.empty()) {
            offsets[i] = 64;
            continue;
        }
        // Identical macros share one record list, like the official driver.
        bool shared = false;
        for (int j = 0; j < i; ++j)
            if (!macros[j].empty() && macros[j] == m) {
                offsets[i] = offsets[j];
                shared = true;
                break;
            }
        if (shared)
            continue;
        offsets[i] = static_cast<std::uint16_t>(64 + body.size());
        for (std::size_t n = 0; n < m.size(); ++n) {
            auto rec = encode_action(m[n], n + 1 == m.size());
            body.insert(body.end(), rec.begin(), rec.end());
        }
    }
    std::vector<std::uint8_t> area(kTableSize + kHeaderSize, 0);
    for (int i = 0; i < kMacroCount; ++i) {
        area[2 * i] = static_cast<std::uint8_t>(offsets[i] & 0xFF);
        area[2 * i + 1] = static_cast<std::uint8_t>(offsets[i] >> 8);
    }
    area[32] = 0x81;
    area[33] = 0x7E;
    for (int i = 0; i < kMacroCount; ++i)
        area[34 + i] = types[i];
    area[50] = 'M';
    area[51] = 'C';
    const std::size_t cloud = 64 + body.size();
    area[52] = static_cast<std::uint8_t>(cloud & 0xFF);
    area[53] = static_cast<std::uint8_t>(cloud >> 8);
    area.insert(area.end(), body.begin(), body.end());
    area.insert(area.end(), kCloudEntry * kMacroCount, 0);
    if (area.size() > 0x2000)
        throw std::length_error("macros do not fit into the 8192-byte macro area");
    return area;
}

MacroSet decode_macro_area(std::span<const std::uint8_t> area)
{
    if (area.size() < kTableSize)
        throw std::runtime_error("macro area shorter than its offset table");
    MacroSet out;
    for (int i = 0; i < kMacroCount; ++i) {
        std::size_t pos = u16(area, 2 * i);
        for (;;) {
            if (pos + 4 > area.size())
                throw std::runtime_error("macro " + std::to_string(i + 1) + " runs past the end of the area");
            bool last = false;
            MacroAction a = decode_action(std::span<const std::uint8_t, 4>(area.data() + pos, 4), &last);
            // The shared empty macro (00 00 80 00) is a lone end record without content.
            const bool empty_marker = last && a.kind == MacroAction::Kind::Delay && a.delay_ms == 0;
            if (!empty_marker)
                out[i].push_back(a);
            pos += 4;
            if (last)
                break;
        }
    }
    return out;
}

std::size_t macro_area_used_size(std::span<const std::uint8_t> area)
{
    if (area.size() >= 64 && area[32] == 0x81 && area[33] == 0x7E && area[50] == 'M' && area[51] == 'C') {
        const std::size_t cloud = u16(area, 52);
        return std::min(area.size(), cloud + kCloudEntry * kMacroCount);
    }
    std::size_t end = 0;
    for (int i = 0; i < kMacroCount && area.size() >= kTableSize; ++i) {
        std::size_t pos = u16(area, 2 * i);
        while (pos + 4 <= area.size()) {
            const bool last = area[pos + 2] & 0x80;
            pos += 4;
            if (last)
                break;
        }
        end = std::max(end, pos);
    }
    return std::min(area.size(), std::max(end, kTableSize));
}

}  // namespace mchose::glw
