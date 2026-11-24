// SPDX-License-Identifier: MIT
#include "mchose/glw/structs.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <stdexcept>

namespace mchose::glw {

namespace {

template <std::size_t N>
std::array<std::uint8_t, N> copy_exact(std::span<const std::uint8_t> data, const char* what)
{
    if (data.size() < N)
        throw std::invalid_argument(std::string(what) + ": " + std::to_string(data.size()) + " bytes, expected " +
                                    std::to_string(N));
    std::array<std::uint8_t, N> a{};
    std::copy_n(data.begin(), N, a.begin());
    return a;
}

}  // namespace

FirmwareInfo FirmwareInfo::parse(std::span<const std::uint8_t> data)
{
    FirmwareInfo fi;
    if (data.size() >= 2)
        fi.version = static_cast<std::uint16_t>(data[0] | (data[1] << 8));
    for (std::size_t i = 2; i < data.size() && data[i] != 0; ++i)
        fi.build.push_back(static_cast<char>(data[i]));
    return fi;
}

std::string FirmwareInfo::version_string() const
{
    char buf[8];
    std::snprintf(buf, sizeof buf, "%x", version);
    return buf;
}

BaseInfo BaseInfo::parse(std::span<const std::uint8_t> data)
{
    BaseInfo b;
    b.raw = copy_exact<56>(data, "base info");
    return b;
}

int BaseInfo::boot_profile() const
{
    const int pos = position() < length() ? position() : 0;
    const int p = order(pos > 3 ? 0 : pos);
    return p < 4 ? p : 0;
}

int BaseInfo::runtime_profile() const
{
    if (position() < 4 && order(position()) < 4)
        return order(position());
    return boot_profile();
}

std::optional<int> BaseInfo::position_of(int profile) const
{
    for (int i = 0; i < 4; ++i)
        if (order(i) == profile)
            return i;
    return std::nullopt;
}

bool BaseInfo::in_rotation(int profile) const
{
    auto pos = position_of(profile);
    return pos && *pos < length();
}

std::optional<int> report_rate_hz(std::uint8_t value)
{
    switch (value) {
    case 1: return 8000;
    case 2: return 4000;
    case 3: return 2000;
    case 4: return 1000;
    default: return std::nullopt;   // 0 / 5 are 16k / 32k on USB 3 models; others invalid
    }
}

FuncConfig FuncConfig::parse(std::span<const std::uint8_t> data)
{
    FuncConfig f;
    f.raw = copy_exact<64>(data, "FuncConfig");
    return f;
}

TriggerEntry TriggerEntry::parse(std::span<const std::uint8_t> data)
{
    TriggerEntry t;
    t.raw = copy_exact<8>(data, "trigger entry");
    return t;
}

void TriggerEntry::set9(int i, int v)
{
    if (v < 0 || v > 0x1FF)
        throw std::out_of_range("trigger distance out of range (0.01 .. 5.12 mm)");
    raw[i] = static_cast<std::uint8_t>(v & 0xFF);
    raw[i + 1] = static_cast<std::uint8_t>((raw[i + 1] & 0xFE) | ((v >> 8) & 0x01));
}

const char* to_string(TriggerEntry::Mode m)
{
    switch (m) {
    case TriggerEntry::Mode::Normal: return "normal";
    case TriggerEntry::Mode::RapidTrigger: return "rapid-trigger";
    case TriggerEntry::Mode::ContinuousRapidTrigger: return "continuous-rt";
    }
    return "unknown";
}

std::string to_hex(std::span<const std::uint8_t> data, char separator)
{
    static const char* digits = "0123456789abcdef";
    std::string s;
    s.reserve(data.size() * (separator ? 3 : 2));
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (separator && i)
            s.push_back(separator);
        s.push_back(digits[data[i] >> 4]);
        s.push_back(digits[data[i] & 0x0F]);
    }
    return s;
}

std::vector<std::uint8_t> from_hex(const std::string& text)
{
    std::vector<std::uint8_t> out;
    int hi = -1;
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == ':')
            continue;
        int v;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else throw std::invalid_argument(std::string("bad hex character '") + c + "'");
        if (hi < 0) {
            hi = v;
        } else {
            out.push_back(static_cast<std::uint8_t>(hi << 4 | v));
            hi = -1;
        }
    }
    if (hi >= 0)
        throw std::invalid_argument("odd number of hex digits");
    return out;
}

}  // namespace mchose::glw
