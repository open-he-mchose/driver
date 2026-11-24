// SPDX-License-Identifier: MIT
// hidapi transport for the keyboard's configuration interface (Linux hidraw, Windows HID).
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "mchose/model.hpp"
#include "mchose/transport.hpp"

namespace mchose::hid {

struct DeviceInfo {
    std::string path;
    std::uint16_t vid = 0;
    std::uint16_t pid = 0;
    int interface_number = -1;
    std::uint16_t usage_page = 0;
    std::uint16_t usage = 0;
    std::string product;
    std::string serial;
    std::optional<Model> model;   // nothing for MCHOSE devices without a built-in model
};

// Configuration interfaces of connected MCHOSE keyboards (vendor ID 0x41E4): the HID collection with
// usage page 0x0001 / usage 0x0000 (interface 1 on the Jet75-II).
std::vector<DeviceInfo> enumerate();

class HidapiTransport : public Transport {
public:
    explicit HidapiTransport(const std::string& path);
    ~HidapiTransport() override;
    HidapiTransport(const HidapiTransport&) = delete;
    HidapiTransport& operator=(const HidapiTransport&) = delete;

    void write(const Report& report) override;
    std::optional<Report> read(std::chrono::milliseconds timeout) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace mchose::hid
