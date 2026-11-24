// SPDX-License-Identifier: MIT
// Lighting effects written in Lua, shown with live colours (RAM only, no flash writes).
//
// A script defines
//   function frame(ctx) ... end      -- called for every frame (~25-30 per second)
//   function init(ctx) ... end       -- optional, called once after loading
//   params = { speed = {min = 0.1, max = 5, default = 1, label = "Speed"}, ... }   -- optional
//
// ctx: t, dt (seconds), keys (array of {index, name, x, y, w}; positions in key units), pressed / released
// (matrix indices since the last frame), held (set: held[index] == true), width / height of the layout,
// param (table of current parameter values), set(index, r, g, b), fill(r, g, b), get(index) -> r, g, b.
// Globals: hsv(h, s, v) -> r, g, b (h in degrees, s/v 0..1), mix(a, b, t), clamp(x, lo, hi), print(...).
// Colours are 0-255; values are clamped and rounded. Colours keep their value between frames.
//
// Sandbox: base library without load / loadfile / dofile, string, table, math, utf8, coroutine. No io, os,
// package or debug. Each call is limited in instructions and the script in memory.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace mchose::script {

class ScriptError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct KeyInfo {
    int index = 0;      // matrix index
    std::string name;   // e.g. "W"
    float x = 0, y = 0, w = 1;
};

struct FrameInput {
    double t = 0;        // seconds since start
    double dt = 0;       // seconds since the previous frame
    std::vector<int> pressed;    // matrix indices pressed since the previous frame
    std::vector<int> released;
    std::vector<int> held;       // currently held
};

struct Param {
    std::string name, label;
    double min = 0, max = 1, value = 0, def = 0;
    double step = 0;   // 0 = continuous; values are rounded to min + k*step
    double order = 1e9;   // display order (ascending, then by name)
};

using Rgb = std::array<std::uint8_t, 3>;

class LightingScript {
public:
    // `slots` = number of colour entries produced (key slots of the model).
    LightingScript(std::vector<KeyInfo> keys, int slots, float width, float height);
    ~LightingScript();
    LightingScript(const LightingScript&) = delete;
    LightingScript& operator=(const LightingScript&) = delete;

    // Load (or reload) a script: runs its main chunk, reads `params`, calls init(). Throws ScriptError.
    void load(const std::string& source, const std::string& name = "script");
    // Run frame(ctx) and return the colours by matrix index. Throws ScriptError (the script is then unusable
    // until loaded again).
    const std::vector<Rgb>& frame(const FrameInput& in);

    std::vector<Param> params() const;
    const std::vector<Rgb>& colors() const { return colors_; }   // last frame
    void set_param(const std::string& name, double value);

    void set_print_handler(std::function<void(const std::string&)> h) { print_ = std::move(h); }
    void set_instruction_limit(long n) { instruction_limit_ = n; }   // per call, default 20 million
    void set_memory_limit(std::size_t bytes) { memory_limit_ = bytes; }   // default 64 MB

    // Implementation details used by the Lua callbacks.
    struct State;

private:
    friend struct ScriptAccess;
    void reset_state();
    void call(const char* function, bool optional);

    std::vector<KeyInfo> keys_;
    float width_, height_;
    std::vector<Rgb> colors_;
    std::vector<Param> params_;
    std::function<void(const std::string&)> print_;
    long instruction_limit_ = 20'000'000;
    std::size_t memory_limit_ = 64u << 20;
    std::unique_ptr<State> state_;
    bool loaded_ = false;
};

}  // namespace mchose::script
