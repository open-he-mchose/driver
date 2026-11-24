// SPDX-License-Identifier: MIT
#include "mchose/hid/hidapi_transport.hpp"

#include <hidapi.h>

#include <algorithm>
#include <cerrno>
#include <cwchar>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <sys/file.h>
#  include <unistd.h>
#endif
#include <utility>

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

// Lock key: VID/PID of the device at `path` (one lock per keyboard model is enough in practice).
std::string lock_key(const std::string& path)
{
    std::string key = "keyboard";
    hid_device_info* list = hid_enumerate(kMchoseVid, 0);
    for (auto* d = list; d; d = d->next)
        if (d->path && path == d->path) {
            char buf[16];
            std::snprintf(buf, sizeof buf, "%04x-%04x", d->vendor_id, d->product_id);
            key = buf;
        }
    hid_free_enumeration(list);
    return key;
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

namespace {

// Process-wide lock per keyboard model, shared by all programs of this project.
class DeviceLock {
public:
    explicit DeviceLock(const std::string& key)
    {
#if defined(_WIN32)
        const std::wstring name = L"Local\\open-he-mchose-" + std::wstring(key.begin(), key.end());
        mutex_ = CreateMutexW(nullptr, TRUE, name.c_str());
        if (mutex_ && GetLastError() == ERROR_ALREADY_EXISTS) {
            CloseHandle(mutex_);
            mutex_ = nullptr;
            throw DeviceBusy("the keyboard is in use by another program (mchose-gui or mchosectl); close it first");
        }
#else
        const char* dir = std::getenv("XDG_RUNTIME_DIR");
        path_ = std::string(dir && *dir ? dir : "/tmp") + "/open-he-mchose-" + key + ".lock";
        fd_ = open(path_.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
        if (fd_ < 0)
            return;   // no lock possible (read-only dir): continue without
        if (flock(fd_, LOCK_EX | LOCK_NB) != 0) {
            char buf[128] = {};
            const ssize_t n = pread(fd_, buf, sizeof buf - 1, 0);
            close(fd_);
            fd_ = -1;
            std::string who = n > 0 ? std::string(buf, static_cast<std::size_t>(n)) : std::string("another program");
            while (!who.empty() && (who.back() == '\n' || who.back() == ' '))
                who.pop_back();
            throw DeviceBusy("the keyboard is in use by " + who + "; close it first");
        }
        char info[96];
        std::snprintf(info, sizeof info, "%s (pid %d)\n", program_name().c_str(), static_cast<int>(getpid()));
        if (ftruncate(fd_, 0) == 0)
            (void)pwrite(fd_, info, std::char_traits<char>::length(info), 0);
#endif
    }
    ~DeviceLock()
    {
#if defined(_WIN32)
        if (mutex_) {
            ReleaseMutex(mutex_);
            CloseHandle(mutex_);
        }
#else
        if (fd_ >= 0)
            close(fd_);   // releases the flock
#endif
    }
    DeviceLock(const DeviceLock&) = delete;
    DeviceLock& operator=(const DeviceLock&) = delete;

private:
#if defined(_WIN32)
    HANDLE mutex_ = nullptr;
#else
    static std::string program_name()
    {
        char buf[256] = {};
        const ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
        std::string p = n > 0 ? std::string(buf, static_cast<std::size_t>(n)) : std::string("mchose program");
        const auto slash = p.rfind('/');
        return slash == std::string::npos ? p : p.substr(slash + 1);
    }
    std::string path_;
    int fd_ = -1;
#endif
};

}  // namespace

struct HidapiTransport::Impl {
    std::unique_ptr<DeviceLock> lock;
    hid_device* dev = nullptr;
};

HidapiTransport::HidapiTransport(const std::string& path) : impl_(std::make_unique<Impl>())
{
    ensure_init();
    impl_->lock = std::make_unique<DeviceLock>(lock_key(path));
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
    errno = 0;
    const int n = hid_read_timeout(impl_->dev, buf, sizeof buf, static_cast<int>(timeout.count()));
    if (n < 0 && errno == EINTR)
        return std::nullopt;   // interrupted by a signal (e.g. Ctrl+C): no data
    if (n < 0)
        throw std::runtime_error("hid_read failed: " + error_text(impl_->dev));
    if (n == 0)
        return std::nullopt;
    Report r{};
    std::copy(buf, buf + std::min<int>(n, 64), r.begin());
    return r;
}

}  // namespace mchose::hid
