// SPDX-License-Identifier: MIT
#include "mchose/glw/session.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace mchose::glw {

namespace {

std::string hex2(unsigned v)
{
    char buf[8];
    std::snprintf(buf, sizeof buf, "0x%02X", v & 0xFF);
    return buf;
}

std::uint16_t checked_offset(std::uint32_t offset)
{
    if (offset > 0xFFFF)
        throw std::invalid_argument("offset above 0xFFFF");
    return static_cast<std::uint16_t>(offset);
}

}  // namespace

std::vector<std::uint8_t> Session::transact(std::uint8_t command, std::uint16_t offset, std::uint8_t size,
                                            std::span<const std::uint8_t> data)
{
    transport_.write(make_request(command, offset, size, data));
    ++requests_;
    const auto deadline = std::chrono::steady_clock::now() + timeout_;
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            throw ProtocolError("no reply to command " + hex2(command) + " at offset " + std::to_string(offset));
        auto rep = transport_.read(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
        if (!rep)
            continue;
        auto reply = parse_reply(*rep);
        if (reply && reply->command == command && reply->offset == offset) {
            if (!reply->ok) {
                std::string why(reply->data.begin(), std::find(reply->data.begin(), reply->data.end(), 0));
                throw ProtocolError("keyboard rejected command " + hex2(command) + ": " +
                                    (why.empty() ? std::string("error") : why));
            }
            return {reply->data.begin(), reply->data.end()};
        }
        if (unsolicited_)
            unsolicited_(*rep);
    }
}

std::vector<std::uint8_t> Session::read(std::uint8_t command, std::uint32_t offset, std::size_t length)
{
    std::vector<std::uint8_t> out;
    out.reserve(length);
    for (std::size_t pos = 0; pos < length; pos += kMaxChunk) {
        const auto n = static_cast<std::uint8_t>(std::min(kMaxChunk, length - pos));
        auto chunk = transact(command, checked_offset(offset + static_cast<std::uint32_t>(pos)), n);
        if (chunk.size() < n)
            throw ProtocolError("short reply to command " + hex2(command) + ": " + std::to_string(chunk.size()) +
                                " of " + std::to_string(n) + " bytes");
        out.insert(out.end(), chunk.begin(), chunk.begin() + n);
    }
    return out;
}

void Session::write(std::uint8_t command, std::uint32_t offset, std::span<const std::uint8_t> data)
{
    for (std::size_t pos = 0; pos < data.size(); pos += kMaxChunk) {
        const auto n = std::min(kMaxChunk, data.size() - pos);
        transact(command, checked_offset(offset + static_cast<std::uint32_t>(pos)), static_cast<std::uint8_t>(n),
                 data.subspan(pos, n));
    }
}

}  // namespace mchose::glw
