// SPDX-License-Identifier: MIT
// High-level operations on a glw keyboard. Every write is verified by reading it back. All settings
// writes go to the keyboard's flash; a write counter guards against accidental loops.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "mchose/glw/keycode.hpp"
#include "mchose/glw/macro.hpp"
#include "mchose/glw/session.hpp"
#include "mchose/glw/structs.hpp"
#include "mchose/model.hpp"

namespace mchose::glw {

class SafetyError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// One depth-stream packet (https://open-he-mchose.github.io/protocols/glw/depth-stream/).
struct DepthSample {
    KeyCode key;                // factory layer-0 code of the key
    int normalized = 0;         // 0..1600, input of the travel curve
    int travel = 0;             // 0.01 mm, as computed by the firmware (lags on fast releases)
    bool pressed = false;
    bool calibrated = false;
    int max_travel = 0;         // 0.01 mm
    int adc = 0;
    int adc_rest = 0;
};
std::optional<DepthSample> parse_depth_sample(const Report& report);

struct ProfileImage {
    std::array<std::uint8_t, 64> funcconfig{};
    std::vector<std::uint8_t> triggers;                 // 8 x key_slots
    std::array<std::vector<std::uint8_t>, 4> keymaps;   // 3 x key_slots per layer
    std::vector<std::uint8_t> colors;                   // 3 x key_slots
    std::vector<std::uint8_t> dks;                      // 768
    std::vector<std::uint8_t> mt;                       // 256
    std::vector<std::uint8_t> tgl;                      // 128
};

// Everything that a backup restores. Calibration is deliberately not included.
struct KeyboardImage {
    std::string model;
    std::string firmware;
    std::array<std::uint8_t, 56> base{};
    std::vector<std::uint8_t> macros;   // used part of the macro area
    std::vector<ProfileImage> profiles;
};

struct WriteOptions {
    bool allow_reboot = false;             // allow changing the report rate of the active profile
    bool allow_dangerous = false;          // allow binding F0 07 / F0 08 / F0 FE
    bool allow_outside_rotation = false;   // allow activating a profile outside the rotation
};

class Keyboard {
public:
    Keyboard(Transport& transport, Model model);

    Session& session() { return session_; }
    const Model& model() const { return model_; }

    // Flash-wear guard: writes beyond this number throw SafetyError (default 4000 per object).
    void set_write_limit(std::size_t n) { write_limit_ = n; }
    std::size_t writes() const { return writes_; }

    // --- reading -----------------------------------------------------------------------------
    FirmwareInfo info();
    BaseInfo base_info();
    int active_profile();
    FuncConfig funcconfig(int profile);
    std::vector<TriggerEntry> triggers(int profile);
    std::vector<KeyCode> keymap(int profile, int layer);
    std::vector<KeyCode> factory_keymap(int layer);
    std::vector<std::array<std::uint8_t, 3>> colors(int profile);
    std::vector<std::uint8_t> macro_area();   // used part only
    MacroSet macros();
    std::vector<std::uint8_t> raw_table(std::uint8_t read_command, int offset, int length);

    // --- writing (flash) -------------------------------------------------------------------------
    // Switch the active profile (cmd 0x0E). The profile must be in the rotation unless allowed.
    void switch_profile(int profile, const WriteOptions& opt = {});
    void set_funcconfig(int profile, const FuncConfig& fc, const WriteOptions& opt = {});
    void set_trigger(int profile, int key, const TriggerEntry& entry);
    void set_key(int profile, int layer, int key, const KeyCode& code, const WriteOptions& opt = {});
    void set_color(int profile, int key, std::array<std::uint8_t, 3> rgb);
    void set_macros(const MacroSet& macros);
    void write_raw_table(std::uint8_t write_command, std::uint8_t read_command, int offset, std::span<const std::uint8_t> data);

    // Live colours (RAM only, no flash). rgb[k] = colour of matrix index k.
    void live_colors(std::span<const std::array<std::uint8_t, 3>> rgb);

    // Depth stream on/off: sets the FuncConfig debug bit of the active profile (two flash writes).
    void set_depth_stream(bool on);

    // --- backup / restore ------------------------------------------------------------------------
    KeyboardImage backup(const std::function<void(const std::string&)>& progress = {});
    // Writes only what differs; returns the number of write operations. Never changes the report rate of
    // the active profile unless opt.allow_reboot.
    int restore(const KeyboardImage& image, const std::function<void(const std::string&)>& log = {},
                const WriteOptions& opt = {});

private:
    void count_write(std::size_t n = 1);
    void check_profile(int profile) const;
    void check_key(int key) const;
    void write_verified(std::uint8_t write_cmd, std::uint8_t read_cmd, int offset, std::span<const std::uint8_t> data);

    Session session_;
    Model model_;
    std::size_t writes_ = 0;
    std::size_t write_limit_ = 4000;
};

}  // namespace mchose::glw
