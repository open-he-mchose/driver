// SPDX-License-Identifier: MIT
// Data structures of the glw protocol. Each wraps the raw bytes and exposes the documented fields;
// unknown bits are preserved when a field is changed.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace mchose::glw {

// Command 0x03.
struct FirmwareInfo {
    std::uint16_t version = 0;   // e.g. 0x0116 = "116"
    std::string build;           // e.g. "Aug 28 2025,10:45:06"

    static FirmwareInfo parse(std::span<const std::uint8_t> data);
    std::string version_string() const;   // "116"
};

// Commands 0x04 / 0x0E (https://open-he-mchose.github.io/protocols/glw/profiles/).
struct BaseInfo {
    std::array<std::uint8_t, 56> raw{};

    static BaseInfo parse(std::span<const std::uint8_t> data);

    std::uint8_t position() const { return raw[0]; }
    std::uint8_t length() const { return raw[1]; }
    std::uint8_t order(int i) const { return raw[2 + i]; }
    bool marker_ok() const { return raw[6] == 0xAA && raw[7] == 0xBB; }

    // Active profile as the firmware computes it at boot (position >= length -> position 0;
    // profile >= 4 -> profile 0). At runtime a host may have selected a position outside the rotation;
    // use runtime_profile() for that case.
    int boot_profile() const;
    // Active profile right now: order[position] even if position >= length (if valid).
    int runtime_profile() const;
    // Position of `profile` in the order list, or nothing.
    std::optional<int> position_of(int profile) const;
    // True if `profile` is reachable with the next-profile key (inside the rotation).
    bool in_rotation(int profile) const;
};

// Report rate values of FuncConfig byte 4 (bits 0-3).
std::optional<int> report_rate_hz(std::uint8_t value);   // 1 -> 8000, 2 -> 4000, 3 -> 2000, 4 -> 1000

// Commands 0x05 / 0x06, 64 bytes per profile (https://open-he-mchose.github.io/protocols/glw/funcconfig/).
struct FuncConfig {
    std::array<std::uint8_t, 64> raw{};

    static FuncConfig parse(std::span<const std::uint8_t> data);

    bool marker_ok() const { return raw[2] == 0xAA && raw[3] == 0xBB; }
    std::uint8_t base_layer() const { return raw[1] & 0xFE; }    // absolute layer, bit 0 ignored
    bool mac_mode() const { return (base_layer() & 0x03) == 2; }
    std::uint8_t report_rate() const { return raw[4] & 0x0F; }
    bool win_lock() const { return raw[6] & 0x01; }
    bool alt_tab_lock() const { return raw[6] & 0x02; }
    bool alt_f4_lock() const { return raw[6] & 0x04; }
    bool menu_lock() const { return raw[6] & 0x08; }
    bool debug_mode() const { return raw[7] & 0x08; }    // enables the depth stream
    std::uint8_t light_effect() const { return raw[8]; }
    std::uint8_t light_brightness() const { return raw[9]; }
    std::uint8_t light_speed() const { return static_cast<std::uint8_t>(4 - (raw[10] & 0x07)); }
    std::array<std::uint8_t, 3> light_color() const { return {raw[14], raw[15], raw[16]}; }

    void set_debug_mode(bool on) { raw[7] = static_cast<std::uint8_t>(on ? raw[7] | 0x08 : raw[7] & ~0x08); }
    void set_win_lock(bool on) { raw[6] = static_cast<std::uint8_t>(on ? raw[6] | 0x01 : raw[6] & ~0x01); }
    void set_light_effect(std::uint8_t e) { raw[8] = e; }
    void set_light_brightness(std::uint8_t b) { raw[9] = b > 100 ? 100 : b; }
    void set_light_speed(std::uint8_t s) { raw[10] = static_cast<std::uint8_t>(4 - (s > 4 ? 4 : s)); }
    void set_light_color(std::uint8_t r, std::uint8_t g, std::uint8_t b) { raw[14] = r; raw[15] = g; raw[16] = b; }
};

// Commands 0xA0 / 0xA1, 8 bytes per key (https://open-he-mchose.github.io/protocols/glw/key-trigger/).
// Distances in 0.01 mm (Jet75-II precision); stored as value - 1 in 9 bits.
struct TriggerEntry {
    std::array<std::uint8_t, 8> raw{};

    static TriggerEntry parse(std::span<const std::uint8_t> data);

    enum class Mode : std::uint8_t { Normal = 0, RapidTrigger = 1, ContinuousRapidTrigger = 2 };

    bool marker_ok() const { return (raw[0] >> 4) == 0xA; }
    std::uint8_t switch_type() const { return raw[0] & 0x0F; }
    Mode mode() const { return static_cast<Mode>(raw[1] & 0x0F); }
    std::uint8_t priority() const { return raw[1] >> 4; }        // SOCD mode
    int actuation() const { return get9(2) + 1; }                // 0.01 mm
    int rt_press() const { return get9(4) + 1; }
    int rt_release() const { return get9(6) + 1; }
    std::uint8_t press_dead_zone() const { return raw[5] >> 1; }
    std::uint8_t release_dead_zone() const { return raw[7] >> 1; }

    void set_mode(Mode m) { raw[1] = static_cast<std::uint8_t>((raw[1] & 0xF0) | static_cast<std::uint8_t>(m)); }
    void set_priority(std::uint8_t p) { raw[1] = static_cast<std::uint8_t>((raw[1] & 0x0F) | ((p & 0x0F) << 4)); }
    void set_actuation(int v) { set9(2, v - 1); }
    void set_rt_press(int v) { set9(4, v - 1); }
    void set_rt_release(int v) { set9(6, v - 1); }
    void set_press_dead_zone(std::uint8_t v) { raw[5] = static_cast<std::uint8_t>((raw[5] & 0x01) | ((v & 0x7F) << 1)); }
    void set_release_dead_zone(std::uint8_t v) { raw[7] = static_cast<std::uint8_t>((raw[7] & 0x01) | ((v & 0x7F) << 1)); }

private:
    int get9(int i) const { return raw[i] | ((raw[i + 1] & 0x01) << 8); }
    void set9(int i, int v);
};

const char* to_string(TriggerEntry::Mode m);

// Hex helpers used by the tools and the backup format.
std::string to_hex(std::span<const std::uint8_t> data, char separator = 0);
std::vector<std::uint8_t> from_hex(const std::string& text);   // accepts spaces; throws on bad input

}  // namespace mchose::glw
