// SPDX-License-Identifier: MIT
// Request / reply exchange with chunking, reply matching and handling of unsolicited reports.
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "mchose/glw/packet.hpp"
#include "mchose/transport.hpp"

namespace mchose::glw {

class ProtocolError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Session {
public:
    explicit Session(Transport& transport) : transport_(transport) {}

    // Called for every input report that is not the awaited reply: depth stream (A0), notifications
    // (A1, A3 ...), replies meant for another program.
    void set_unsolicited_handler(std::function<void(const Report&)> handler) { unsolicited_ = std::move(handler); }

    void set_timeout(std::chrono::milliseconds t) { timeout_ = t; }

    // One request, one reply. Returns the reply data (`size` bytes as echoed by the keyboard).
    std::vector<std::uint8_t> transact(std::uint8_t command, std::uint16_t offset, std::uint8_t size,
                                       std::span<const std::uint8_t> data = {});

    // Read `length` bytes starting at `offset`, in 56-byte chunks.
    std::vector<std::uint8_t> read(std::uint8_t command, std::uint32_t offset, std::size_t length);

    // Write `data` starting at `offset`, in 56-byte chunks. Every chunk is a flash write for most commands.
    void write(std::uint8_t command, std::uint32_t offset, std::span<const std::uint8_t> data);

    // Direct access for reading unsolicited reports (depth stream) without sending requests.
    Transport& transport() { return transport_; }

    // Number of requests sent so far (for tests and flash-wear accounting).
    std::size_t requests_sent() const { return requests_; }

private:
    Transport& transport_;
    std::function<void(const Report&)> unsolicited_;
    std::chrono::milliseconds timeout_{1000};
    std::size_t requests_ = 0;
};

}  // namespace mchose::glw
