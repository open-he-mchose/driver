// SPDX-License-Identifier: MIT
// Packet framing of the glw protocol family (https://open-he-mchose.github.io/protocols/glw/framing/).
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "mchose/transport.hpp"

namespace mchose::glw {

inline constexpr std::size_t kMaxChunk = 56;      // data bytes per packet
inline constexpr std::uint8_t kRequestFlag = 0x55;
inline constexpr std::uint8_t kReplyOk = 0xAA;
inline constexpr std::uint8_t kReplyError = 0xAB;

// Command numbers (https://open-he-mchose.github.io/protocols/glw/commands/).
namespace cmd {
inline constexpr std::uint8_t Info = 0x03;
inline constexpr std::uint8_t GetBase = 0x04;
inline constexpr std::uint8_t GetFuncConfig = 0x05;
inline constexpr std::uint8_t SetFuncConfig = 0x06;
inline constexpr std::uint8_t GetFactoryKeymap = 0x07;
inline constexpr std::uint8_t GetKeymap = 0x08;
inline constexpr std::uint8_t SetKeymap = 0x09;
inline constexpr std::uint8_t GetColors = 0x0A;
inline constexpr std::uint8_t SetColors = 0x0B;
inline constexpr std::uint8_t GetMacros = 0x0C;
inline constexpr std::uint8_t SetMacros = 0x0D;
inline constexpr std::uint8_t SetBase = 0x0E;
inline constexpr std::uint8_t GetTrigger = 0xA0;
inline constexpr std::uint8_t SetTrigger = 0xA1;
inline constexpr std::uint8_t GetDks = 0xA2;
inline constexpr std::uint8_t SetDks = 0xA3;
inline constexpr std::uint8_t GetMt = 0xA4;
inline constexpr std::uint8_t SetMt = 0xA5;
inline constexpr std::uint8_t GetTgl = 0xA6;
inline constexpr std::uint8_t SetTgl = 0xA7;
inline constexpr std::uint8_t CalibrationStart = 0xA8;
inline constexpr std::uint8_t CalibrationEnd = 0xA9;
inline constexpr std::uint8_t CalibrationReset = 0xB1;
inline constexpr std::uint8_t LiveColorsSide2 = 0xDC;
inline constexpr std::uint8_t LiveColorsMain = 0xDD;
inline constexpr std::uint8_t LiveColorsSide = 0xDF;
inline constexpr std::uint8_t GetCustom = 0xF1;
inline constexpr std::uint8_t SetCustom = 0xF2;
}  // namespace cmd

// First byte of unsolicited input reports.
namespace report {
inline constexpr std::uint8_t DepthStream = 0xA0;
inline constexpr std::uint8_t ProfileSwitched = 0xA1;
inline constexpr std::uint8_t Reset = 0xA2;
inline constexpr std::uint8_t FuncConfigSaved = 0xA3;
}  // namespace report

// Sum of bytes 4 .. 7+size, & 0xFF.
std::uint8_t checksum(const Report& packet);

// Build a request. For reads pass the requested size and no data; for writes size = data.size().
// Throws std::invalid_argument if size > 56 or data does not fit.
Report make_request(std::uint8_t command, std::uint16_t offset, std::uint8_t size,
                    std::span<const std::uint8_t> data = {});

struct Reply {
    bool ok = false;
    std::uint8_t command = 0;
    std::uint8_t size = 0;
    std::uint16_t offset = 0;
    std::span<const std::uint8_t> data;   // points into the report it was parsed from
};

// Parse a reply report (0xAA / 0xAB). Returns nothing for other input reports.
std::optional<Reply> parse_reply(const Report& report);

}  // namespace mchose::glw
