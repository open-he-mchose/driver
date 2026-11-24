// SPDX-License-Identifier: MIT
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>

#include "check.hpp"
#include "mchose/gamepad/gamepad.hpp"

using namespace mchose::gamepad;

namespace {

// Key names of the Jet75-II factory layer 0 used by the presets, with their matrix indices.
const std::map<std::string, int> kKeys = {
    {"W", 71}, {"A", 10}, {"S", 61}, {"D", 11}, {"Q", 0}, {"E", 1}, {"R", 21}, {"Space", 77}, {"LShift", 66},
    {"LCtrl", 46}, {"Tab", 70}, {"Esc", 50}, {"Up", 18}, {"Down", 29}, {"Left", 8}, {"Right", 39},
};

std::optional<int> resolve(std::string_view n)
{
    if (!n.empty() && n[0] == '#')
        return std::stoi(std::string(n.substr(1)));
    auto it = kKeys.find(std::string(n));
    return it == kKeys.end() ? std::nullopt : std::optional<int>(it->second);
}

bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

}  // namespace

TEST(gamepad_parse_and_format)
{
    const Config c = parse_config("# comment\nW = left-up   # stick\n#12 = A\nSpace=a\ndeadzone = 0.5\nfull = 2.5\n", resolve);
    CHECK_EQ(c.bindings.size(), std::size_t(3));
    CHECK_EQ(c.bindings[0].key, 71);
    CHECK(c.bindings[0].target == Target::LeftUp);
    CHECK_EQ(c.bindings[1].key, 12);
    CHECK(c.bindings[2].target == Target::A);
    CHECK(near(c.curve.deadzone, 0.5));
    CHECK(near(c.curve.full, 2.5));
    const std::string text = format_config(c, [](int k) { return "#" + std::to_string(k); });
    const Config again = parse_config(text, resolve);
    CHECK_EQ(again.bindings.size(), std::size_t(3));
    CHECK(near(again.curve.full, 2.5));
}

TEST(gamepad_parse_errors)
{
    bool threw = false;
    try { parse_config("W = sideways\n", resolve); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { parse_config("Nope = a\n", resolve); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { parse_config("deadzone = 2\nfull = 1\n", resolve); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
}

TEST(gamepad_presets_parse)
{
    CHECK(preset_names().size() >= 2);
    for (const auto& n : preset_names()) {
        const Config c = parse_config(preset_text(n), resolve);
        CHECK(!c.bindings.empty());
    }
}

TEST(gamepad_mapper_values)
{
    Config c = parse_config("W = left-up\nS = left-down\nD = left-right\nSpace = a\nLShift = lt\n", resolve);
    c.curve = {0.5, 2.5, 1.0};
    Mapper m(c);
    CHECK(m.state() == State{});
    m.set_depth(71, 1.5);   // half way between deadzone and full
    CHECK(near(m.state().ly, 0.5));
    m.set_depth(61, 2.5);   // S fully: up 0.5 - down 1.0
    CHECK(near(m.state().ly, -0.5));
    m.set_depth(71, 0.3);   // inside the dead zone
    m.set_depth(61, 0);
    CHECK(near(m.state().ly, 0));
    m.set_depth(71, 3.0);   // W + D fully: diagonal, length 1
    m.set_depth(11, 3.0);
    CHECK(near(std::hypot(m.state().lx, m.state().ly), 1.0));
    CHECK(near(m.state().lx, m.state().ly));
    m.set_depth(77, 0.9);
    CHECK(!m.state().pressed(Target::A));
    m.set_depth(77, 1.0);
    CHECK(m.state().pressed(Target::A));
    m.set_depth(66, 1.5);
    CHECK(near(m.state().lt, 0.5));
    CHECK(m.uses(66));
    CHECK(!m.uses(0));
    m.reset();
    CHECK(m.state() == State{});
}
