// SPDX-License-Identifier: MIT
#include <string>
#include <vector>

#include "check.hpp"
#include "mchose/script/lighting_script.hpp"

using namespace mchose::script;

namespace {

LightingScript make()
{
    std::vector<KeyInfo> keys{{0, "Q", 1.5f, 2.25f, 1}, {10, "A", 1.75f, 3.25f, 1}, {71, "W", 2.5f, 2.25f, 1}};
    return LightingScript(keys, 128, 16, 6.25f);
}

}  // namespace

TEST(script_fill_set_clamp)
{
    auto s = make();
    s.load("function frame(ctx) ctx.fill(10, 20, 30); ctx.set(71, 300, -5, 127.6) end");
    auto& c = s.frame({});
    CHECK_EQ(int(c[0][0]), 10);
    CHECK_EQ(int(c[0][2]), 30);
    CHECK_EQ(int(c[71][0]), 255);
    CHECK_EQ(int(c[71][1]), 0);
    CHECK_EQ(int(c[71][2]), 128);
}

TEST(script_ctx_and_helpers)
{
    auto s = make();
    s.load(R"(
        function frame(ctx)
          local r, g, b = hsv(0, 1, 1)
          ctx.set(0, r, g, b)
          ctx.set(10, #ctx.keys, ctx.keys[3].index, math.floor(ctx.t * 10))
          for _, k in ipairs(ctx.pressed) do ctx.set(k, 1, 2, 3) end
          if ctx.held[71] then ctx.set(1, 9, 9, 9) end
        end)");
    FrameInput in;
    in.t = 1.5;
    in.pressed = {71};
    in.held = {71};
    auto& c = s.frame(in);
    CHECK_EQ(int(c[0][0]), 255);
    CHECK_EQ(int(c[0][1]), 0);
    CHECK_EQ(int(c[10][0]), 3);
    CHECK_EQ(int(c[10][1]), 71);
    CHECK_EQ(int(c[10][2]), 15);
    CHECK_EQ(int(c[71][2]), 3);
    CHECK_EQ(int(c[1][0]), 9);
}

TEST(script_params)
{
    auto s = make();
    s.load(R"(
        params = { speed = { min = 0, max = 10, default = 2, label = "Speed" } }
        function frame(ctx) ctx.fill(ctx.param.speed, 0, 0) end)");
    auto p = s.params();
    CHECK_EQ(p.size(), std::size_t(1));
    CHECK_EQ(p[0].label, std::string("Speed"));
    CHECK_EQ(int(s.frame({})[5][0]), 2);
    s.set_param("speed", 99);   // clamped to max
    CHECK_EQ(int(s.frame({})[5][0]), 10);
}

TEST(script_sandbox)
{
    auto s = make();
    s.load(R"(
        function frame(ctx)
          ctx.fill(io == nil and 1 or 0, os == nil and 1 or 0, (load == nil and dofile == nil and require == nil) and 1 or 0)
        end)");
    auto& c = s.frame({});
    CHECK_EQ(int(c[0][0]), 1);
    CHECK_EQ(int(c[0][1]), 1);
    CHECK_EQ(int(c[0][2]), 1);
}

TEST(script_limits_and_errors)
{
    auto s = make();
    s.set_instruction_limit(200000);
    s.load("function frame(ctx) while true do end end");
    CHECK_THROWS(s.frame({}), ScriptError);
    CHECK_THROWS(s.frame({}), ScriptError);   // stays stopped until loaded again

    s.set_memory_limit(4u << 20);
    s.set_instruction_limit(0);
    s.load("function frame(ctx) local t = {} for i = 1, 1e8 do t[i] = i end end");
    CHECK_THROWS(s.frame({}), ScriptError);

    std::string msg;
    try {
        s.load("function frame(ctx) ctx.fill(1, 2 end", "broken.lua");
    } catch (const ScriptError& e) {
        msg = e.what();
    }
    CHECK(msg.find("broken.lua:1") != std::string::npos);
    CHECK_THROWS(s.load("x = 1"), ScriptError);   // no frame()

    std::vector<std::string> printed;
    s.set_print_handler([&](const std::string& l) { printed.push_back(l); });
    s.load("print('hello', 42) function frame(ctx) end");
    CHECK_EQ(printed.size(), std::size_t(1));
    CHECK_EQ(printed[0], std::string("hello\t42"));
}

#include <filesystem>
#include <fstream>
#include <sstream>

// Every bundled example loads and runs for a while with key presses.
TEST(script_examples_run)
{
    namespace fs = std::filesystem;
    int n = 0;
    for (const auto& e : fs::directory_iterator(MCHOSE_SCRIPT_EXAMPLES)) {
        if (e.path().extension() != ".lua")
            continue;
        std::ifstream f(e.path());
        std::stringstream ss;
        ss << f.rdbuf();
        std::vector<KeyInfo> keys;
        for (int i = 0; i < 80; ++i)
            keys.push_back({i, "K" + std::to_string(i), static_cast<float>(i % 16), static_cast<float>(i / 16), 1});
        LightingScript s(keys, 128, 16, 5);
        std::string err;
        try {
            s.load(ss.str(), e.path().filename().string());
            for (int frame = 0; frame < 60; ++frame) {
                FrameInput in;
                in.t = frame / 25.0;
                in.dt = 1 / 25.0;
                if (frame % 7 == 0)
                    in.pressed = {frame % 80};
                s.frame(in);
            }
        } catch (const ScriptError& ex) {
            err = ex.what();
        }
        if (!err.empty())
            check::fail(__FILE__, __LINE__, e.path().filename().string() + ": " + err);
        ++n;
    }
    CHECK(n >= 6);
}
