// SPDX-License-Identifier: MIT
// Runs a lighting script on a keyboard: key presses from the OS -> frame(ctx) -> live colours.
#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>

#include "mchose/glw/keyboard.hpp"
#include "mchose/script/key_input.hpp"
#include "mchose/script/lighting_script.hpp"

namespace mchose::script {

class ScriptPlayer {
public:
    explicit ScriptPlayer(glw::Keyboard& kb);

    void load(const std::string& source, const std::string& name = "script");
    LightingScript& script() { return *script_; }
    KeyInput& input() { return input_; }

    // One frame: poll the keys, run frame(ctx), send the colours (RAM only). Call 25-30 times per second.
    // Throws ScriptError (script problems) or ProtocolError (keyboard problems).
    void step();

private:
    glw::Keyboard& kb_;
    std::unique_ptr<LightingScript> script_;
    KeyInput input_;
    std::map<std::uint8_t, int> usage_index_;
    std::chrono::steady_clock::time_point start_, last_;
    bool started_ = false;
};

}  // namespace mchose::script
