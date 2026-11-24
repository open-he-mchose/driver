// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

namespace mchose {

// One HID report on the configuration interface: 64 bytes, no report ID.
using Report = std::array<std::uint8_t, 64>;

// Byte transport to the keyboard's configuration interface. Implemented with hidapi in mchose::hid,
// and by a simulated keyboard in the tests.
class Transport {
public:
    virtual ~Transport() = default;

    // Send one 64-byte output report. Throws on I/O errors.
    virtual void write(const Report& report) = 0;

    // Wait up to `timeout` for one input report. Returns nothing on timeout. Throws on I/O errors.
    virtual std::optional<Report> read(std::chrono::milliseconds timeout) = 0;
};

}  // namespace mchose
