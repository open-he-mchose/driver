// SPDX-License-Identifier: MIT
// Analog keys as a gamepad. The keyboard has no gamepad mode: the depth stream gives the travel of every key,
// a Mapper turns it into stick / trigger / button values and a VirtualPad hands them to the system as an
// Xbox 360-style controller (Linux uinput, Windows ViGEmBus).
//
// Mapping text (one binding per line, '#' starts a comment):
//     W = left-up          # key name (factory layer 0) or #INDEX, then the target
//     Space = a
//     deadzone = 0.3       # mm of travel ignored at the top
//     full = 3.0           # travel (mm) that gives the full analog value
//     button = 1.2         # travel (mm) at which a button counts as pressed
// Targets: left-up/down/left/right, right-up/down/left/right, lt, rt, a, b, x, y, lb, rb, back, start, guide,
//          l3, r3, dpad-up/down/left/right.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mchose::gamepad {

enum class Target : std::uint8_t {
    LeftUp, LeftDown, LeftLeft, LeftRight, RightUp, RightDown, RightLeft, RightRight, LT, RT,
    A, B, X, Y, LB, RB, Back, Start, Guide, L3, R3, DpadUp, DpadDown, DpadLeft, DpadRight,
    Count
};

std::string_view target_name(Target t);
std::optional<Target> parse_target(std::string_view name);
bool is_analog(Target t);   // sticks and triggers
std::vector<std::string> target_names();

struct Binding {
    int key = 0;   // matrix index
    Target target = Target::A;
};

struct Curve {
    double deadzone = 0.3;   // mm
    double full = 3.0;       // mm
    double button = 1.2;     // mm
};

struct Config {
    std::vector<Binding> bindings;
    Curve curve;
};

// Resolves a key name or "#N" to a matrix index; nullopt = unknown.
using KeyResolver = std::function<std::optional<int>(std::string_view)>;
using KeyNamer = std::function<std::string(int)>;

Config parse_config(std::string_view text, const KeyResolver& resolve);   // throws std::runtime_error (line number)
std::string format_config(const Config& c, const KeyNamer& name);

std::vector<std::string> preset_names();
std::string preset_text(std::string_view name);   // empty if unknown

struct State {
    double lx = 0, ly = 0, rx = 0, ry = 0;   // -1..1; +y = up
    double lt = 0, rt = 0;                   // 0..1
    std::uint32_t buttons = 0;               // bit (1 << Target) for A..DpadRight
    bool pressed(Target t) const { return buttons & (1u << static_cast<unsigned>(t)); }
    bool operator==(const State&) const = default;
};

class Mapper {
public:
    explicit Mapper(Config c);
    void set_depth(int key, double mm);   // from the depth stream (mm of travel)
    void reset();                         // all keys up
    State state() const;
    const Config& config() const { return cfg_; }
    bool uses(int key) const;

private:
    double analog(double mm) const;
    Config cfg_;
    std::vector<double> depth_;   // per matrix index
};

class VirtualPad {
public:
    VirtualPad();   // creates the controller; check ok()
    ~VirtualPad();
    VirtualPad(const VirtualPad&) = delete;
    VirtualPad& operator=(const VirtualPad&) = delete;

    bool ok() const;
    std::string status() const;   // what was created, or why not (with a hint)
    void send(const State& s);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace mchose::gamepad
