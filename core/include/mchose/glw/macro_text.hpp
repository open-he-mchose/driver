// SPDX-License-Identifier: MIT
// Text form of macros, shared by the CLI and the GUI.
//
//   +LCtrl 20ms +C 20ms -C 20ms -LCtrl     press / release single keys; "20ms" = delay after the previous action
//   Ctrl+C                                 tap a key or combination (modifiers down, key down, key up, modifiers up)
//   "Hello, world!"                        type text (US layout; Shift added where needed)
//   wait 500ms                             a separate pause record (also "500ms" at the very start)
//
// Tokens are separated by spaces or commas. Actions without an explicit delay get the default delay.
#pragma once

#include <cstdint>
#include <string>

#include "mchose/glw/macro.hpp"

namespace mchose::glw {

// Throws std::invalid_argument with a readable message on errors.
Macro parse_macro(const std::string& text, std::uint16_t default_delay_ms = 10);

// Exact text form (+/- with every delay): parse_macro(format_macro(m)) == m.
std::string format_macro(const Macro& m);

}  // namespace mchose::glw
