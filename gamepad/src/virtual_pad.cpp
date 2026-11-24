// SPDX-License-Identifier: MIT
#include "mchose/gamepad/gamepad.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  ifdef MCHOSE_HAVE_VIGEM
#    include <ViGEm/Client.h>
#  endif
#elif defined(__linux__)
#  include <fcntl.h>
#  include <linux/uinput.h>
#  include <sys/ioctl.h>
#  include <unistd.h>
#endif

namespace mchose::gamepad {

namespace {

int axis16(double v) { return static_cast<int>(std::lround(std::clamp(v, -1.0, 1.0) * 32767.0)); }
int trigger8(double v) { return static_cast<int>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0)); }

}  // namespace

#if defined(__linux__)

struct VirtualPad::Impl {
    int fd = -1;
    std::string status;
    State last;
    bool sent = false;

    void emit(unsigned short type, unsigned short code, int value)
    {
        input_event ev{};
        ev.type = type;
        ev.code = code;
        ev.value = value;
        (void)!write(fd, &ev, sizeof ev);
    }
};

namespace {

// Xbox 360 layout as the Linux xpad driver reports it, so games and SDL map it like a real controller.
constexpr struct {
    Target t;
    unsigned short code;
} kButtons[] = {
    {Target::A, BTN_SOUTH}, {Target::B, BTN_EAST}, {Target::X, BTN_NORTH}, {Target::Y, BTN_WEST},
    {Target::LB, BTN_TL}, {Target::RB, BTN_TR}, {Target::Back, BTN_SELECT}, {Target::Start, BTN_START},
    {Target::Guide, BTN_MODE}, {Target::L3, BTN_THUMBL}, {Target::R3, BTN_THUMBR},
};

}  // namespace

VirtualPad::VirtualPad() : impl_(std::make_unique<Impl>())
{
    auto& d = *impl_;
    d.fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (d.fd < 0) {
        const int err = errno;
        d.status = err == ENOENT ? "no /dev/uinput (load the module: sudo modprobe uinput)"
                 : err == EACCES || err == EPERM
                     ? "no permission to use /dev/uinput (install packaging/linux/72-mchose-uinput.rules, then re-plug or log in again)"
                     : std::string("cannot open /dev/uinput: ") + std::strerror(err);
        return;
    }
    bool ok = ioctl(d.fd, UI_SET_EVBIT, EV_KEY) == 0 && ioctl(d.fd, UI_SET_EVBIT, EV_ABS) == 0;
    for (const auto& b : kButtons)
        ok = ok && ioctl(d.fd, UI_SET_KEYBIT, b.code) == 0;
    auto abs = [&](unsigned short code, int lo, int hi, int flat) {
        uinput_abs_setup a{};
        a.code = code;
        a.absinfo.minimum = lo;
        a.absinfo.maximum = hi;
        a.absinfo.flat = flat;
        ok = ok && ioctl(d.fd, UI_SET_ABSBIT, code) == 0 && ioctl(d.fd, UI_ABS_SETUP, &a) == 0;
    };
    abs(ABS_X, -32768, 32767, 128);
    abs(ABS_Y, -32768, 32767, 128);
    abs(ABS_RX, -32768, 32767, 128);
    abs(ABS_RY, -32768, 32767, 128);
    abs(ABS_Z, 0, 255, 0);
    abs(ABS_RZ, 0, 255, 0);
    abs(ABS_HAT0X, -1, 1, 0);
    abs(ABS_HAT0Y, -1, 1, 0);
    uinput_setup setup{};
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x045E;    // Xbox 360 controller IDs: games and SDL know its button layout
    setup.id.product = 0x028E;
    setup.id.version = 0x0110;
    std::snprintf(setup.name, sizeof setup.name, "MCHOSE keyboard gamepad");
    ok = ok && ioctl(d.fd, UI_DEV_SETUP, &setup) == 0 && ioctl(d.fd, UI_DEV_CREATE) == 0;
    if (!ok) {
        d.status = std::string("cannot create the virtual gamepad: ") + std::strerror(errno);
        close(d.fd);
        d.fd = -1;
        return;
    }
    d.status = "virtual gamepad \"MCHOSE keyboard gamepad\" created (uinput, Xbox 360 layout)";
}

VirtualPad::~VirtualPad()
{
    if (impl_ && impl_->fd >= 0) {
        ioctl(impl_->fd, UI_DEV_DESTROY);
        close(impl_->fd);
    }
}

bool VirtualPad::ok() const { return impl_->fd >= 0; }
std::string VirtualPad::status() const { return impl_->status; }

void VirtualPad::send(const State& s)
{
    auto& d = *impl_;
    if (d.fd < 0 || (d.sent && s == d.last))
        return;
    d.emit(EV_ABS, ABS_X, axis16(s.lx));
    d.emit(EV_ABS, ABS_Y, -axis16(s.ly));   // evdev: down is positive
    d.emit(EV_ABS, ABS_RX, axis16(s.rx));
    d.emit(EV_ABS, ABS_RY, -axis16(s.ry));
    d.emit(EV_ABS, ABS_Z, trigger8(s.lt));
    d.emit(EV_ABS, ABS_RZ, trigger8(s.rt));
    d.emit(EV_ABS, ABS_HAT0X, (s.pressed(Target::DpadRight) ? 1 : 0) - (s.pressed(Target::DpadLeft) ? 1 : 0));
    d.emit(EV_ABS, ABS_HAT0Y, (s.pressed(Target::DpadDown) ? 1 : 0) - (s.pressed(Target::DpadUp) ? 1 : 0));
    for (const auto& b : kButtons)
        d.emit(EV_KEY, b.code, s.pressed(b.t) ? 1 : 0);
    d.emit(EV_SYN, SYN_REPORT, 0);
    d.last = s;
    d.sent = true;
}

#elif defined(_WIN32) && defined(MCHOSE_HAVE_VIGEM)

struct VirtualPad::Impl {
    PVIGEM_CLIENT client = nullptr;
    PVIGEM_TARGET pad = nullptr;
    bool added = false;
    std::string status;
    State last;
    bool sent = false;
};

VirtualPad::VirtualPad() : impl_(std::make_unique<Impl>())
{
    auto& d = *impl_;
    d.client = vigem_alloc();
    if (!d.client) {
        d.status = "out of memory";
        return;
    }
    const VIGEM_ERROR e = vigem_connect(d.client);
    if (!VIGEM_SUCCESS(e)) {
        d.status = e == VIGEM_ERROR_BUS_NOT_FOUND
                       ? "the ViGEmBus driver is not installed (install it from https://github.com/nefarius/ViGEmBus/releases)"
                       : "cannot connect to ViGEmBus (error 0x" + [&] { char b[16]; std::snprintf(b, sizeof b, "%08X", static_cast<unsigned>(e)); return std::string(b); }() + ")";
        return;
    }
    d.pad = vigem_target_x360_alloc();
    const VIGEM_ERROR a = d.pad ? vigem_target_add(d.client, d.pad) : VIGEM_ERROR_BUS_NOT_FOUND;
    if (!VIGEM_SUCCESS(a)) {
        d.status = "cannot create the virtual Xbox 360 controller (ViGEmBus)";
        return;
    }
    d.added = true;
    d.status = "virtual Xbox 360 controller created (ViGEmBus)";
}

VirtualPad::~VirtualPad()
{
    auto& d = *impl_;
    if (d.added)
        vigem_target_remove(d.client, d.pad);
    if (d.pad)
        vigem_target_free(d.pad);
    if (d.client) {
        vigem_disconnect(d.client);
        vigem_free(d.client);
    }
}

bool VirtualPad::ok() const { return impl_->added; }
std::string VirtualPad::status() const { return impl_->status; }

void VirtualPad::send(const State& s)
{
    auto& d = *impl_;
    if (!d.added || (d.sent && s == d.last))
        return;
    XUSB_REPORT r{};
    constexpr struct {
        Target t;
        USHORT bit;
    } map[] = {
        {Target::A, XUSB_GAMEPAD_A}, {Target::B, XUSB_GAMEPAD_B}, {Target::X, XUSB_GAMEPAD_X},
        {Target::Y, XUSB_GAMEPAD_Y}, {Target::LB, XUSB_GAMEPAD_LEFT_SHOULDER}, {Target::RB, XUSB_GAMEPAD_RIGHT_SHOULDER},
        {Target::Back, XUSB_GAMEPAD_BACK}, {Target::Start, XUSB_GAMEPAD_START}, {Target::Guide, XUSB_GAMEPAD_GUIDE},
        {Target::L3, XUSB_GAMEPAD_LEFT_THUMB}, {Target::R3, XUSB_GAMEPAD_RIGHT_THUMB},
        {Target::DpadUp, XUSB_GAMEPAD_DPAD_UP}, {Target::DpadDown, XUSB_GAMEPAD_DPAD_DOWN},
        {Target::DpadLeft, XUSB_GAMEPAD_DPAD_LEFT}, {Target::DpadRight, XUSB_GAMEPAD_DPAD_RIGHT},
    };
    for (const auto& m : map)
        if (s.pressed(m.t))
            r.wButtons = static_cast<USHORT>(r.wButtons | m.bit);
    r.bLeftTrigger = static_cast<BYTE>(trigger8(s.lt));
    r.bRightTrigger = static_cast<BYTE>(trigger8(s.rt));
    r.sThumbLX = static_cast<SHORT>(axis16(s.lx));
    r.sThumbLY = static_cast<SHORT>(axis16(s.ly));   // XInput: up is positive
    r.sThumbRX = static_cast<SHORT>(axis16(s.rx));
    r.sThumbRY = static_cast<SHORT>(axis16(s.ry));
    vigem_target_x360_update(d.client, d.pad, r);
    d.last = s;
    d.sent = true;
}

#else

struct VirtualPad::Impl {};
VirtualPad::VirtualPad() : impl_(std::make_unique<Impl>()) {}
VirtualPad::~VirtualPad() = default;
bool VirtualPad::ok() const { return false; }
std::string VirtualPad::status() const { return "virtual gamepads are not supported on this system"; }
void VirtualPad::send(const State&) {}

#endif

}  // namespace mchose::gamepad
