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

#include "mchose/glw/advanced.hpp"
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
// Travel in mm without the release lag: the firmware's travel field freezes when a key settles, so a sample whose
// ADC is within 20 of the rest value counts as 0 (verified; docs: Jet75-II switches, "Lag and how to avoid it").
double depth_mm(const DepthSample& s);

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

    // Stored per-key colours (used by the static effect 0). Writes only the keys that differ.
    void set_colors(int profile, std::span<const std::array<std::uint8_t, 3>> rgb);

    // Change the report rate (FuncConfig byte 4 of the active profile; 1 = 8 kHz, 2 = 4 kHz, 3 = 2 kHz,
    // 4 = 1 kHz). The keyboard REBOOTS right after the write and disappears for a moment: this object's
    // transport is unusable afterwards; reconnect and verify with a new Keyboard.
    void set_report_rate_and_reboot(std::uint8_t value);

    // Live colours (RAM only, no flash). rgb[k] = colour of matrix index k.
    void live_colors(std::span<const std::array<std::uint8_t, 3>> rgb);

    // Depth stream on/off: sets the FuncConfig debug bit of the active profile (two flash writes).
    void set_depth_stream(bool on);

    // --- advanced keys (https://open-he-mchose.github.io/protocols/glw/advanced-keys/) ---------------
    std::vector<AdvBinding> adv_bindings(int profile);
    AdvTables adv_tables(int profile);
    // Code an advanced function should send for `key`: its layer-0 mapping if that is a plain key,
    // else the factory code.
    KeyCode plain_code(int profile, int key);
    void set_trigger_priority(int profile, int key, std::uint8_t priority);
    // Each bind_* refuses keys that already have an advanced function in that layer (clear them first).
    void bind_pair(int profile, int layer, AdvKind kind, int key1, int key2, SocdMode mode = SocdMode::LastInput);
    void bind_mod_tap(int profile, int layer, int key, const KeyCode& tap, const KeyCode& hold, int hold_ms);
    void bind_toggle(int profile, int layer, int key, std::optional<KeyCode> code = std::nullopt);
    void bind_dks(int profile, int layer, int key, const DksEntry& entry);
    // Remove the advanced function of `key` (and of its partner for pairs). Layer 0 gets the factory code
    // back, other layers become transparent. Returns the keys that were cleared.
    std::vector<int> clear_adv(int profile, int layer, int key);

    // --- backup / restore ------------------------------------------------------------------------
    using Log = std::function<void(const std::string&)>;
    KeyboardImage backup(const Log& progress = {});
    // Writes only what differs; returns the number of write operations. Never changes the report rate of
    // the active profile unless opt.allow_reboot.
    int restore(const KeyboardImage& image, const Log& log = {}, const WriteOptions& opt = {});

    // One profile: everything a backup holds for it. write_profile writes only what differs.
    ProfileImage read_profile(int profile);
    void write_profile(int profile, const ProfileImage& image, const Log& log = {}, const WriteOptions& opt = {});

    // --- profiles ---------------------------------------------------------------------------------
    // Copy settings, keymaps (Fn layer numbers adjusted), colours and advanced keys from src to dst.
    // The destination keeps its report rate (the rate is global anyway).
    void copy_profile(int src, int dst, const Log& log = {}, const WriteOptions& opt = {});
    // Profiles reachable with the next-profile key, in this order. The active profile stays active if it
    // is in the list; otherwise the keyboard switches to the first one.
    void set_rotation(const std::vector<int>& profiles, const Log& log = {});

private:
    void count_write(std::size_t n = 1);
    void check_profile(int profile) const;
    void check_key(int key) const;
    int sync_region(const char* what, int profile, std::uint8_t rd, std::uint8_t wr, int offset,
                    const std::vector<std::uint8_t>& want, std::size_t entry, const Log& log);
    void check_free(const std::vector<AdvBinding>& bindings, int layer, int key);
    void write_verified(std::uint8_t write_cmd, std::uint8_t read_cmd, int offset, std::span<const std::uint8_t> data);

    Session session_;
    Model model_;
    std::size_t writes_ = 0;
    std::size_t write_limit_ = 4000;
};

}  // namespace mchose::glw
