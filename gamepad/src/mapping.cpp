// SPDX-License-Identifier: MIT
#include "mchose/gamepad/gamepad.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace mchose::gamepad {

namespace detail {
const std::vector<std::pair<std::string_view, std::string_view>>& presets();
}

namespace {

constexpr std::array<std::string_view, static_cast<std::size_t>(Target::Count)> kNames = {
    "left-up", "left-down", "left-left", "left-right", "right-up", "right-down", "right-left", "right-right",
    "lt", "rt", "a", "b", "x", "y", "lb", "rb", "back", "start", "guide", "l3", "r3",
    "dpad-up", "dpad-down", "dpad-left", "dpad-right",
};

std::string lower(std::string_view s)
{
    std::string r(s);
    for (auto& c : r)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

std::string_view trim(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

}  // namespace

std::string_view target_name(Target t)
{
    const auto i = static_cast<std::size_t>(t);
    return i < kNames.size() ? kNames[i] : std::string_view("?");
}

std::optional<Target> parse_target(std::string_view name)
{
    const std::string n = lower(trim(name));
    for (std::size_t i = 0; i < kNames.size(); ++i)
        if (n == kNames[i])
            return static_cast<Target>(i);
    return std::nullopt;
}

bool is_analog(Target t) { return static_cast<unsigned>(t) <= static_cast<unsigned>(Target::RT); }

std::vector<std::string> target_names() { return {kNames.begin(), kNames.end()}; }

Config parse_config(std::string_view text, const KeyResolver& resolve)
{
    Config c;
    int line_no = 0;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t end = std::min(text.find('\n', pos), text.size());
        std::string_view line = text.substr(pos, end - pos);
        pos = end + 1;
        ++line_no;
        // Comments: '#' at the start or after a space, unless a digit follows ("#12 = a" is a key index).
        for (std::size_t i = 0; i < line.size(); ++i) {
            if (line[i] != '#' || (i > 0 && !std::isspace(static_cast<unsigned char>(line[i - 1]))))
                continue;
            if (i + 1 < line.size() && std::isdigit(static_cast<unsigned char>(line[i + 1])))
                continue;
            line = line.substr(0, i);
            break;
        }
        line = trim(line);
        if (line.empty())
            continue;
        const auto eq = line.find('=', 1);   // the key itself may be "="
        if (eq == std::string_view::npos)
            throw std::runtime_error("line " + std::to_string(line_no) + ": expected KEY = TARGET");
        const std::string_view lhs = trim(line.substr(0, eq)), rhs = trim(line.substr(eq + 1));
        const std::string key = lower(lhs);
        auto number = [&]() {
            try {
                std::size_t used = 0;
                const double v = std::stod(std::string(rhs), &used);
                if (used != rhs.size() || v < 0 || v > 4)
                    throw std::invalid_argument("range");
                return v;
            } catch (const std::exception&) {
                throw std::runtime_error("line " + std::to_string(line_no) + ": expected a travel in mm (0 to 4)");
            }
        };
        if (key == "deadzone") { c.curve.deadzone = number(); continue; }
        if (key == "full") { c.curve.full = number(); continue; }
        if (key == "button") { c.curve.button = number(); continue; }
        const auto t = parse_target(rhs);
        if (!t)
            throw std::runtime_error("line " + std::to_string(line_no) + ": unknown target '" + std::string(rhs) + "'");
        const auto k = resolve(lhs);
        if (!k)
            throw std::runtime_error("line " + std::to_string(line_no) + ": unknown key '" + std::string(lhs) + "'");
        c.bindings.push_back({*k, *t});
    }
    if (c.curve.full <= c.curve.deadzone + 0.05)
        throw std::runtime_error("'full' must be larger than 'deadzone'");
    return c;
}

std::string format_config(const Config& c, const KeyNamer& name)
{
    std::ostringstream o;
    for (const auto& b : c.bindings)
        o << name(b.key) << " = " << target_name(b.target) << "\n";
    o << "deadzone = " << c.curve.deadzone << "\nfull = " << c.curve.full << "\nbutton = " << c.curve.button << "\n";
    return o.str();
}

std::vector<std::string> preset_names()
{
    std::vector<std::string> n;
    for (const auto& p : detail::presets())
        n.emplace_back(p.first);
    return n;
}

std::string preset_text(std::string_view name)
{
    for (const auto& p : detail::presets())
        if (p.first == name)
            return std::string(p.second);
    return {};
}

Mapper::Mapper(Config c) : cfg_(std::move(c))
{
    int n = 0;
    for (const auto& b : cfg_.bindings)
        n = std::max(n, b.key + 1);
    depth_.assign(static_cast<std::size_t>(n), 0.0);
}

void Mapper::set_depth(int key, double mm)
{
    if (key >= 0 && key < static_cast<int>(depth_.size()))
        depth_[static_cast<std::size_t>(key)] = mm;
}

void Mapper::reset() { std::fill(depth_.begin(), depth_.end(), 0.0); }

bool Mapper::uses(int key) const
{
    return std::any_of(cfg_.bindings.begin(), cfg_.bindings.end(), [&](const Binding& b) { return b.key == key; });
}

double Mapper::analog(double mm) const
{
    return std::clamp((mm - cfg_.curve.deadzone) / (cfg_.curve.full - cfg_.curve.deadzone), 0.0, 1.0);
}

State Mapper::state() const
{
    std::array<double, static_cast<std::size_t>(Target::Count)> v{};
    for (const auto& b : cfg_.bindings) {
        const double mm = depth_[static_cast<std::size_t>(b.key)];
        auto& slot = v[static_cast<std::size_t>(b.target)];
        slot = std::max(slot, is_analog(b.target) ? analog(mm) : (mm >= cfg_.curve.button ? 1.0 : 0.0));
    }
    auto at = [&](Target t) { return v[static_cast<std::size_t>(t)]; };
    State s;
    s.lx = at(Target::LeftRight) - at(Target::LeftLeft);
    s.ly = at(Target::LeftUp) - at(Target::LeftDown);
    s.rx = at(Target::RightRight) - at(Target::RightLeft);
    s.ry = at(Target::RightUp) - at(Target::RightDown);
    // Diagonals: keep the stick inside the unit circle (W+D fully pressed = 45 degrees, length 1).
    for (auto* xy : {&s.lx, &s.rx}) {
        double& x = *xy;
        double& y = xy == &s.lx ? s.ly : s.ry;
        const double len = std::hypot(x, y);
        if (len > 1) {
            x /= len;
            y /= len;
        }
    }
    s.lt = at(Target::LT);
    s.rt = at(Target::RT);
    for (auto t = static_cast<unsigned>(Target::A); t < static_cast<unsigned>(Target::Count); ++t)
        if (v[t] > 0.5)
            s.buttons |= 1u << t;
    return s;
}

}  // namespace mchose::gamepad
