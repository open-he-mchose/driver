// SPDX-License-Identifier: MIT
#include "mchose/script/key_input.hpp"

#include <algorithm>
#include <array>
#include <set>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#elif defined(__linux__)
#  include <dirent.h>
#  include <fcntl.h>
#  include <linux/input.h>
#  include <unistd.h>
#endif

namespace mchose::script {

std::map<std::uint8_t, int> usage_index_map(const std::vector<glw::KeyCode>& f, int keys)
{
    std::map<std::uint8_t, int> m;
    for (int i = 0; i < keys && i < static_cast<int>(f.size()); ++i) {
        const auto& k = f[static_cast<std::size_t>(i)];
        if (k.type != glw::KeyCode::Keyboard)
            continue;
        if (k.code1 == 0 && k.code2)
            m.emplace(k.code2, i);
        else if (k.code2 == 0)
            for (int b = 0; b < 8; ++b)
                if (k.code1 == (1u << b))
                    m.emplace(static_cast<std::uint8_t>(0xE0 + b), i);
    }
    return m;
}

namespace {

// HID usage -> platform key code (Linux evdev KEY_* / Windows virtual-key code); 0 = none.
struct UsageMap {
    std::uint8_t usage;
    unsigned short linux_code;
    unsigned char vk;
};

constexpr UsageMap kKeys[] = {
    {0x04, 30, 'A'}, {0x05, 48, 'B'}, {0x06, 46, 'C'}, {0x07, 32, 'D'}, {0x08, 18, 'E'}, {0x09, 33, 'F'},
    {0x0A, 34, 'G'}, {0x0B, 35, 'H'}, {0x0C, 23, 'I'}, {0x0D, 36, 'J'}, {0x0E, 37, 'K'}, {0x0F, 38, 'L'},
    {0x10, 50, 'M'}, {0x11, 49, 'N'}, {0x12, 24, 'O'}, {0x13, 25, 'P'}, {0x14, 16, 'Q'}, {0x15, 19, 'R'},
    {0x16, 31, 'S'}, {0x17, 20, 'T'}, {0x18, 22, 'U'}, {0x19, 47, 'V'}, {0x1A, 17, 'W'}, {0x1B, 45, 'X'},
    {0x1C, 21, 'Y'}, {0x1D, 44, 'Z'}, {0x1E, 2, '1'}, {0x1F, 3, '2'}, {0x20, 4, '3'}, {0x21, 5, '4'},
    {0x22, 6, '5'}, {0x23, 7, '6'}, {0x24, 8, '7'}, {0x25, 9, '8'}, {0x26, 10, '9'}, {0x27, 11, '0'},
    {0x28, 28, 0x0D}, {0x29, 1, 0x1B}, {0x2A, 14, 0x08}, {0x2B, 15, 0x09}, {0x2C, 57, 0x20}, {0x2D, 12, 0xBD},
    {0x2E, 13, 0xBB}, {0x2F, 26, 0xDB}, {0x30, 27, 0xDD}, {0x31, 43, 0xDC}, {0x33, 39, 0xBA}, {0x34, 40, 0xDE},
    {0x35, 41, 0xC0}, {0x36, 51, 0xBC}, {0x37, 52, 0xBE}, {0x38, 53, 0xBF}, {0x39, 58, 0x14}, {0x3A, 59, 0x70},
    {0x3B, 60, 0x71}, {0x3C, 61, 0x72}, {0x3D, 62, 0x73}, {0x3E, 63, 0x74}, {0x3F, 64, 0x75}, {0x40, 65, 0x76},
    {0x41, 66, 0x77}, {0x42, 67, 0x78}, {0x43, 68, 0x79}, {0x44, 87, 0x7A}, {0x45, 88, 0x7B}, {0x46, 99, 0x2C},
    {0x47, 70, 0x91}, {0x48, 119, 0x13}, {0x49, 110, 0x2D}, {0x4A, 102, 0x24}, {0x4B, 104, 0x21}, {0x4C, 111, 0x2E},
    {0x4D, 107, 0x23}, {0x4E, 109, 0x22}, {0x4F, 106, 0x27}, {0x50, 105, 0x25}, {0x51, 108, 0x28}, {0x52, 103, 0x26},
    {0x65, 127, 0x5D}, {0xE0, 29, 0xA2}, {0xE1, 42, 0xA0}, {0xE2, 56, 0xA4}, {0xE3, 125, 0x5B}, {0xE4, 97, 0xA3},
    {0xE5, 54, 0xA1}, {0xE6, 100, 0xA5}, {0xE7, 126, 0x5C},
};

}  // namespace

struct KeyInput::Impl {
    std::set<std::uint8_t> held;
    std::string status;
#if defined(__linux__)
    std::vector<int> fds;
    std::map<unsigned short, std::uint8_t> code_to_usage;
#endif
};

KeyInput::KeyInput() : impl_(std::make_unique<Impl>())
{
#if defined(__linux__)
    for (const auto& k : kKeys)
        impl_->code_to_usage[k.linux_code] = k.usage;
    const char* dir = "/dev/input/by-id";
    int found = 0, denied = 0;
    if (DIR* d = opendir(dir)) {
        while (dirent* e = readdir(d)) {
            const std::string n = e->d_name;
            if (n.find("MCHOSE") == std::string::npos || n.find("event") == std::string::npos)
                continue;
            ++found;
            const int fd = open((std::string(dir) + "/" + n).c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd >= 0)
                impl_->fds.push_back(fd);
            else
                ++denied;
        }
        closedir(d);
    }
    if (!impl_->fds.empty())
        impl_->status = "key presses from " + std::to_string(impl_->fds.size()) + " input device(s) of the keyboard";
    else if (denied)
        impl_->status = "no permission to read the keyboard's input devices (add your user to the 'input' group)";
    else
        impl_->status = found ? "keyboard input devices not readable" : "keyboard input devices not found";
#elif defined(_WIN32)
    impl_->status = "key presses from the system keyboard state";
#else
    impl_->status = "key presses not supported on this system";
#endif
}

KeyInput::~KeyInput()
{
#if defined(__linux__)
    for (int fd : impl_->fds)
        close(fd);
#endif
}

bool KeyInput::available() const
{
#if defined(__linux__)
    return !impl_->fds.empty();
#elif defined(_WIN32)
    return true;
#else
    return false;
#endif
}

std::string KeyInput::status() const { return impl_->status; }

void KeyInput::poll(std::vector<std::uint8_t>& pressed, std::vector<std::uint8_t>& released, std::vector<std::uint8_t>& held)
{
    pressed.clear();
    released.clear();
#if defined(__linux__)
    input_event ev[64];
    for (int fd : impl_->fds) {
        for (;;) {
            const ssize_t n = read(fd, ev, sizeof ev);
            if (n <= 0)
                break;
            for (std::size_t i = 0; i < static_cast<std::size_t>(n) / sizeof(input_event); ++i) {
                if (ev[i].type != EV_KEY || ev[i].value == 2)   // ignore auto-repeat
                    continue;
                auto it = impl_->code_to_usage.find(ev[i].code);
                if (it == impl_->code_to_usage.end())
                    continue;
                if (ev[i].value == 1 && impl_->held.insert(it->second).second)
                    pressed.push_back(it->second);
                else if (ev[i].value == 0 && impl_->held.erase(it->second))
                    released.push_back(it->second);
            }
        }
    }
#elif defined(_WIN32)
    for (const auto& k : kKeys) {
        const bool down = (GetAsyncKeyState(k.vk) & 0x8000) != 0;
        const bool was = impl_->held.count(k.usage) != 0;
        if (down && !was) {
            impl_->held.insert(k.usage);
            pressed.push_back(k.usage);
        } else if (!down && was) {
            impl_->held.erase(k.usage);
            released.push_back(k.usage);
        }
    }
#endif
    held.assign(impl_->held.begin(), impl_->held.end());
}

}  // namespace mchose::script
