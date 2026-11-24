// SPDX-License-Identifier: MIT
#include "mchose/glw/packet.hpp"

#include <algorithm>
#include <stdexcept>

namespace mchose::glw {

std::uint8_t checksum(const Report& packet)
{
    const std::size_t size = std::min<std::size_t>(packet[4], kMaxChunk);
    unsigned sum = 0;
    for (std::size_t i = 4; i < 8 + size; ++i)
        sum += packet[i];
    return static_cast<std::uint8_t>(sum & 0xFF);
}

Report make_request(std::uint8_t command, std::uint16_t offset, std::uint8_t size,
                    std::span<const std::uint8_t> data)
{
    if (size > kMaxChunk)
        throw std::invalid_argument("packet size above 56 bytes");
    if (data.size() > size)
        throw std::invalid_argument("packet data larger than its size field");
    Report p{};
    p[0] = kRequestFlag;
    p[1] = command;
    p[2] = 0;   // XOR key: none
    p[4] = size;
    p[5] = static_cast<std::uint8_t>(offset & 0xFF);
    p[6] = static_cast<std::uint8_t>(offset >> 8);
    p[7] = 0;
    std::copy(data.begin(), data.end(), p.begin() + 8);
    // A read covers only size/offset/reserved in its checksum: the data area is zero there anyway.
    p[3] = checksum(p);
    return p;
}

std::optional<Reply> parse_reply(const Report& report)
{
    if (report[0] != kReplyOk && report[0] != kReplyError)
        return std::nullopt;
    Reply r;
    r.ok = report[0] == kReplyOk;
    r.command = report[1];
    r.size = static_cast<std::uint8_t>(std::min<std::size_t>(report[4], kMaxChunk));
    r.offset = static_cast<std::uint16_t>(report[5] | (report[6] << 8));
    r.data = std::span<const std::uint8_t>(report.data() + 8, r.size);
    return r;
}

}  // namespace mchose::glw
