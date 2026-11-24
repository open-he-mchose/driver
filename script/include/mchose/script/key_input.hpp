// SPDX-License-Identifier: MIT
// Key presses from the operating system, for lighting scripts (the keyboard's depth stream pauses while live
// colours are sent, so it cannot be used for this).
//   Linux:   evdev devices of the MCHOSE keyboard (/dev/input/by-id/*MCHOSE*event*); needs read access,
//            e.g. membership in the "input" group.
//   Windows: GetAsyncKeyState, polled once per frame (any keyboard).
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mchose/glw/keycode.hpp"

namespace mchose::script {

class KeyInput {
public:
    KeyInput();
    ~KeyInput();
    KeyInput(const KeyInput&) = delete;
    KeyInput& operator=(const KeyInput&) = delete;

    bool available() const;   // false: effects run without key presses
    std::string status() const;   // human-readable reason / source

    // HID usages (0x04..0xE7) pressed / released since the last call, and all held now.
    void poll(std::vector<std::uint8_t>& pressed, std::vector<std::uint8_t>& released, std::vector<std::uint8_t>& held);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// HID usage -> matrix index from the factory layer-0 keymap (plain keys and single modifiers).
std::map<std::uint8_t, int> usage_index_map(const std::vector<glw::KeyCode>& factory_layer0, int keys);

}  // namespace mchose::script
