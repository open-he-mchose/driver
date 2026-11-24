// SPDX-License-Identifier: MIT
// Keyboard models. Values for models other than the Jet75-II are taken from the official driver's device
// table (docs/data/devices.json) and are not verified on hardware.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace mchose {

enum class Protocol { Glw, Qhw };

// One key of the physical layout, in key units (1 u = one standard key), origin top-left.
struct KeyCap {
    int index;           // matrix index
    const char* label;
    float x, y, w;
};

struct Model {
    std::string name;            // "Jet75-II"
    std::uint16_t vid = 0;
    std::uint16_t pid = 0;
    Protocol protocol = Protocol::Glw;
    int keys = 80;               // matrix indices in use
    int key_slots = 128;         // table size per layer / profile
    int layers = 4;
    int profiles = 4;
    int macro_area = 8192;       // bytes, shared by all profiles
    int distance_unit_um = 10;   // trigger distances: 1 unit = 10 um = 0.01 mm
    bool verified = false;       // tested on real hardware
    std::span<const KeyCap> layout;   // physical layout (empty if unknown)

    int keymap_offset(int profile, int layer, int key = 0) const { return 512 * (layers * profile + layer) + 3 * key; }
    int trigger_offset(int profile, int key = 0) const { return 1024 * profile + 8 * key; }
    int funcconfig_offset(int profile) const { return 64 * profile; }
    int colors_offset(int profile, int key = 0) const { return 512 * profile + 3 * key; }
    int dks_offset(int profile) const { return 768 * profile; }
    int mt_offset(int profile) const { return 256 * profile; }
    int tgl_offset(int profile) const { return 128 * profile; }
};

inline constexpr std::uint16_t kMchoseVid = 0x41E4;

// Built-in models.
std::span<const Model> known_models();
// Model for a USB VID/PID, or nothing.
std::optional<Model> find_model(std::uint16_t vid, std::uint16_t pid);

}  // namespace mchose
