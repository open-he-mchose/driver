// SPDX-License-Identifier: MIT
// Backup file format (JSON, "mchose-backup" version 1): hex strings of every region a restore writes.
#pragma once

#include <string>

#include "mchose/glw/keyboard.hpp"

namespace mchose::glw {

std::string backup_to_json(const KeyboardImage& image, const std::string& created);
// Throws std::runtime_error on files that are not a version-1 backup.
KeyboardImage backup_from_json(const std::string& text);

}  // namespace mchose::glw
