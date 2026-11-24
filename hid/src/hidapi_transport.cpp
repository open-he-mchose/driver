// SPDX-License-Identifier: MIT
#include "mchose/hid/hidapi_transport.hpp"

#include <hidapi.h>

#include <algorithm>
#include <cwchar>
#include <stdexcept>

namespace mchose::hid {

namespace {

struct HidLibrary {
    HidLibrary()
    {
        if (hid_init() != 0)
            throw std::runtime_error("hid_init failed");
    }
    ~HidLibrary() { hid_exit(); }
};

void ensure_init()
{
    static HidLibrary lib;
}

std::string narrow(const wchar_t* w)
{
    std::string s;
    if (!w)
        return s;
    for (; *w; ++w)
        s.push_back(*w < 0x80 ? static_cast<char>(*w) : '?');
    return s;
}

std::string error_text(hid_device* dev)
{
    return narrow(hid_error(dev));
}

}  // namespace

std::vector<DeviceInfo> enumerate()
{
    ensure_init();
    std::vector<DeviceInfo> all;
    hid_device_info* list = hid_enumerate(kMchoseVid, 0);
    for (auto* d = list; d; d = d->next) {
        DeviceInfo i;
        i.path = d->path ? d->path : "";
        i.vid = d->vendor_id;
        i.pid = d->product_id;
        i.interface_number = d->interface_number;
        i.usage_page = d->usage_page;
        i.usage = d->usage;
        i.product = narrow(d->product_string);
        i.serial = narrow(d->serial_number);
        i.model = find_model(i.vid, i.pid);
        all.push_back(std::move(i));
    }
    hid_free_enumeration(list);

    // Prefer the collection the official driver uses (usage page 1, usage 0). Some hidapi backends do not
    // report usages; fall back to interface 1 then.
    std::vector<DeviceInfo> out;
    for (const auto& i : all)
        if (i.usage_page == 0x0001 && i.usage == 0x0000)
            out.push_back(i);
    if (out.empty())
        for (const auto& i : all)
            if (i.interface_number == 1)
                out.push_back(i);
    return out;
}

struct HidapiTransport::Impl {
    hid_device* dev = nullptr;
};

HidapiTransport::HidapiTransport(const std::string& path) : impl_(std::make_unique<Impl>())
{
    ensure_init();
    impl_->dev = hid_open_path(path.c_str());
    if (!impl_->dev)
        throw std::runtime_error("cannot open " + path +
                                 " (on Linux: install the udev rule from open-he-mchose/linux-fixes or run as root)");
}

HidapiTransport::~HidapiTransport()
{
    if (impl_ && impl_->dev)
        hid_close(impl_->dev);
}

void HidapiTransport::write(const Report& report)
{
    unsigned char buf[65];
    buf[0] = 0;   // report ID: the interface has none
    std::copy(report.begin(), report.end(), buf + 1);
    if (hid_write(impl_->dev, buf, sizeof buf) < 0)
        throw std::runtime_error("hid_write failed: " + error_text(impl_->dev));
}

std::optional<Report> HidapiTransport::read(std::chrono::milliseconds timeout)
{
    unsigned char buf[65] = {};
    const int n = hid_read_timeout(impl_->dev, buf, sizeof buf, static_cast<int>(timeout.count()));
    if (n < 0)
        throw std::runtime_error("hid_read failed: " + error_text(impl_->dev));
    if (n == 0)
        return std::nullopt;
    Report r{};
    std::copy(buf, buf + std::min<int>(n, 64), r.begin());
    return r;
}

}  // namespace mchose::hid
