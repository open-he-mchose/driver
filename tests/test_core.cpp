// SPDX-License-Identifier: MIT
// Golden values come from the protocol documentation (https://open-he-mchose.github.io/), where they were
// verified on a Jet75-II.
#include <algorithm>
#include <array>
#include <initializer_list>
#include <vector>

#include "check.hpp"
#include "fake_keyboard.hpp"
#include "mchose/glw/keyboard.hpp"
#include "mchose/glw/macro_text.hpp"

using namespace mchose;
using namespace mchose::glw;

namespace {

std::vector<std::uint8_t> head(const Report& r, std::size_t n) { return {r.begin(), r.begin() + static_cast<long>(n)}; }

std::ostream& operator<<(std::ostream& os, const std::vector<std::uint8_t>& v) { return os << to_hex(v, ' '); }

}  // namespace

// framing.md: read firmware info.
TEST(packet_info_request)
{
    auto p = make_request(cmd::Info, 0, 56);
    CHECK_EQ(head(p, 8), from_hex("55 03 00 38 38 00 00 00"));
    for (std::size_t i = 8; i < 64; ++i)
        CHECK_EQ(int(p[i]), 0);
}

// lighting.md worked example: key W (index 71) red.
TEST(packet_live_colour_example)
{
    const std::uint8_t rgb[3] = {0xFF, 0x00, 0x00};
    auto p = make_request(cmd::LiveColorsMain, 3 * 71, 3, rgb);
    CHECK_EQ(head(p, 11), from_hex("55 DD 00 D7 03 D5 00 00 FF 00 00"));
}

TEST(packet_limits)
{
    std::vector<std::uint8_t> big(57, 0);
    CHECK_THROWS(make_request(cmd::SetKeymap, 0, 57, big), std::invalid_argument);
}

TEST(reply_parse)
{
    Report r{};
    auto bytes = from_hex("AA 03 00 8D 16 00 00 00 16 01");
    std::copy(bytes.begin(), bytes.end(), r.begin());
    auto rep = parse_reply(r);
    CHECK(rep.has_value());
    CHECK(rep->ok);
    CHECK_EQ(int(rep->command), 0x03);
    CHECK_EQ(int(rep->size), 0x16);
    CHECK_EQ(int(rep->data[0]), 0x16);
    Report stream{};
    stream[0] = 0xA0;
    CHECK(!parse_reply(stream).has_value());
}

// key-trigger.md example.
TEST(trigger_entry_example)
{
    auto t = TriggerEntry::parse(from_hex("A0 01 95 00 1D 00 1D 00"));
    CHECK(t.marker_ok());
    CHECK_EQ(int(t.switch_type()), 0);
    CHECK(t.mode() == TriggerEntry::Mode::RapidTrigger);
    CHECK_EQ(t.actuation(), 150);
    CHECK_EQ(t.rt_press(), 30);
    CHECK_EQ(t.rt_release(), 30);
    t.set_actuation(341);   // 3.41 mm needs the 9th bit
    CHECK_EQ(t.actuation(), 341);
    CHECK_EQ(int(t.raw[3] & 1), 1);
    CHECK_EQ(int(t.raw[2]), 0x54);
    t.set_mode(TriggerEntry::Mode::Normal);
    t.set_priority(3);
    CHECK_EQ(int(t.raw[1]), 0x30);
    CHECK_THROWS(t.set_actuation(0), std::out_of_range);
}

// macros.md example: Shift + B with 20 ms steps.
TEST(macro_records_shift_b)
{
    using K = MacroAction::Kind;
    Macro m = {{K::Modifier, 0x02, true, 20}, {K::Key, 0x05, true, 20}, {K::Key, 0x05, false, 20},
               {K::Modifier, 0x02, false, 20}};
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i < m.size(); ++i) {
        auto r = encode_action(m[i], i + 1 == m.size());
        bytes.insert(bytes.end(), r.begin(), r.end());
    }
    CHECK_EQ(bytes, from_hex("14 00 41 02 14 00 42 05 14 00 02 05 14 00 81 02"));
}

// macros.md: an empty area as written by the official driver, read back from a Jet75-II.
TEST(macro_area_empty_matches_driver)
{
    std::vector<std::uint8_t> want;
    for (int i = 0; i < 16; ++i) { want.push_back(0x40); want.push_back(0x00); }
    want.push_back(0x81); want.push_back(0x7E);
    want.insert(want.end(), 16, 0);
    want.push_back('M'); want.push_back('C'); want.push_back(0x44); want.push_back(0x00);
    want.insert(want.end(), 10, 0);
    for (auto b : from_hex("00 00 80 00")) want.push_back(b);
    want.insert(want.end(), 96, 0);
    auto area = encode_macro_area(MacroSet{});
    CHECK_EQ(area.size(), std::size_t(164));
    CHECK_EQ(area, want);
    CHECK_EQ(macro_area_used_size(area), std::size_t(164));
    auto decoded = decode_macro_area(area);
    for (const auto& m : decoded)
        CHECK(m.empty());
}

TEST(macro_area_roundtrip_and_sharing)
{
    using K = MacroAction::Kind;
    MacroSet set;
    set[0] = {{K::Key, 0x04, true, 1000}, {K::Key, 0x04, false, 500}};
    set[2] = {{K::Key, 0x06, true, 50}, {K::Key, 0x06, false, 50}, {K::Delay, 0, false, 1000}, {K::Key, 0x07, true, 50},
              {K::Key, 0x07, false, 50}};
    set[5] = set[0];   // identical macros share their records
    auto area = encode_macro_area(set);
    CHECK_EQ(int(area[0] | area[1] << 8), int(area[10] | area[11] << 8));
    auto back = decode_macro_area(area);
    CHECK(back[0] == set[0]);
    CHECK(back[2] == set[2]);
    CHECK(back[5] == set[0]);
    CHECK(back[1].empty());
    // The bare delay is stored the way the driver does it: type 2, release, code 0.
    std::size_t off = area[4] | area[5] << 8;
    CHECK_EQ(int(area[off + 8 + 2]), 0x02);
    CHECK_EQ(int(area[off + 8 + 3]), 0x00);
}

TEST(keycode_names)
{
    CHECK_EQ(to_string(KeyCode{0x10, 0x00, 0x04}), std::string("A"));
    CHECK_EQ(to_string(KeyCode{0x10, 0x03, 0x17}), std::string("LCtrl+LShift+T"));
    CHECK_EQ(to_string(KeyCode{0x10, 0x08, 0x00}), std::string("LGui"));
    CHECK_EQ(to_string(KeyCode{0xF0, 0xFF, 0x01}), std::string("Fn(layer 1)"));
    CHECK_EQ(to_string(KeyCode{0xF0, 0xFA, 0x00}), std::string("next-profile"));
    CHECK_EQ(to_string(KeyCode{0x70, 0x02, 0x00}), std::string("M3 while-held"));
    CHECK_EQ(to_string(KeyCode{0x30, 0xE2, 0x00}), std::string("Mute"));
    CHECK_EQ(to_string(KeyCode{0xF0, 0xF0, 0xF0}), std::string("unused"));
    CHECK(KeyCode({0xF0, 0x08, 0x00}).is_dangerous());
    CHECK(!KeyCode({0xF0, 0x32, 0x00}).is_dangerous());
}

TEST(keycode_parse)
{
    CHECK(parse_keycode("A") == (KeyCode{0x10, 0x00, 0x04}));
    CHECK(parse_keycode("ctrl+shift+t") == (KeyCode{0x10, 0x03, 0x17}));
    CHECK(parse_keycode("LCtrl+LShift+T") == (KeyCode{0x10, 0x03, 0x17}));
    CHECK(parse_keycode("Esc") == (KeyCode{0x10, 0x00, 0x29}));
    CHECK(parse_keycode("F12") == (KeyCode{0x10, 0x00, 0x45}));
    CHECK(parse_keycode("Shift++") == std::nullopt);   // '+' is not a key name here
    CHECK(parse_keycode("10 00 29") == (KeyCode{0x10, 0x00, 0x29}));
    CHECK(parse_keycode("Mute") == (KeyCode{0x30, 0xE2, 0x00}));
    CHECK(parse_keycode("none") == (KeyCode{0, 0, 0}));
    CHECK(!parse_keycode("NoSuchKey").has_value());
}

TEST(base_info_profiles)
{
    std::vector<std::uint8_t> raw(56, 0xA2);
    auto set = [&](std::initializer_list<int> v) { int i = 0; for (int x : v) raw[i++] = static_cast<std::uint8_t>(x); };
    set({0, 2, 0, 1, 2, 3, 0xAA, 0xBB});
    auto b = BaseInfo::parse(raw);
    CHECK(b.marker_ok());
    CHECK_EQ(b.runtime_profile(), 0);
    CHECK(b.in_rotation(1));
    CHECK(!b.in_rotation(2));
    set({2, 2, 0, 1, 2, 3, 0xAA, 0xBB});   // host selected position 2 outside the rotation
    b = BaseInfo::parse(raw);
    CHECK_EQ(b.runtime_profile(), 2);
    CHECK_EQ(b.boot_profile(), 0);
}

TEST(funcconfig_fields)
{
    auto fc = FuncConfig::parse(from_hex("5000aabb02010006065004000000000000000000000000000164020000565656"
                                         "00000000000000000000000000000000000000000000000000000000000000000000"
                                         "000000000000000000000000000000000000000000000000000000"));
    CHECK(fc.marker_ok());
    CHECK_EQ(int(fc.report_rate()), 2);
    CHECK_EQ(*report_rate_hz(fc.report_rate()), 4000);
    CHECK_EQ(int(fc.light_effect()), 6);
    CHECK_EQ(int(fc.light_brightness()), 80);
    CHECK(!fc.debug_mode());
    fc.set_debug_mode(true);
    CHECK_EQ(int(fc.raw[7]), 0x0E);
    CHECK(!report_rate_hz(0).has_value());
}

TEST(session_chunks_and_skips_unsolicited)
{
    FakeKeyboard kb;
    Session s(kb);
    int unsolicited = 0;
    s.set_unsolicited_handler([&](const Report&) { ++unsolicited; });
    Report stream{};
    stream[0] = 0xA0;                // depth stream packet
    Report foreign{};
    foreign[0] = 0xAA; foreign[1] = 0x05;   // reply meant for another program
    kb.inject.push_back(stream);
    kb.inject.push_back(foreign);
    auto data = s.read(cmd::GetTrigger, 0, 120);
    CHECK_EQ(data.size(), std::size_t(120));
    CHECK_EQ(kb.sent.size(), std::size_t(3));   // 56 + 56 + 8
    CHECK_EQ(int(kb.sent[1][5]), 56);
    CHECK_EQ(int(kb.sent[2][4]), 8);
    CHECK_EQ(unsolicited, 2);
    CHECK_EQ(int(data[0]), 0xA0);
}

TEST(session_resends_lost_request)
{
    FakeKeyboard kb;
    Session s(kb);
    s.set_timeout(std::chrono::milliseconds(5));
    kb.drop_requests = 2;
    auto data = s.read(cmd::GetBase, 0, 8);
    CHECK_EQ(int(data[6]), 0xAA);
    CHECK_EQ(s.retries_used(), std::size_t(2));
    kb.drop_requests = 10;
    CHECK_THROWS(s.read(cmd::GetBase, 0, 8), ProtocolError);
}

TEST(session_error_reply)
{
    FakeKeyboard kb;
    Session s(kb);
    CHECK_THROWS(s.transact(0x77, 0, 1), ProtocolError);   // unknown command -> "unknw"
}

TEST(keyboard_reads)
{
    FakeKeyboard kb;
    Keyboard k(kb, *find_model(0x41E4, 0x211A));
    auto fi = k.info();
    CHECK_EQ(fi.version_string(), std::string("116"));
    CHECK_EQ(fi.build, std::string("Aug 28 2025,10:45:06"));
    CHECK_EQ(k.active_profile(), 0);
    CHECK_EQ(k.triggers(0)[5].actuation(), 150);
    CHECK(k.keymap(0, 0)[0] == (KeyCode{0x10, 0x00, 0x04}));
    CHECK_EQ(int(k.funcconfig(1).base_layer()), 4);
}

TEST(keyboard_safety_guards)
{
    FakeKeyboard kb;
    Keyboard k(kb, *find_model(0x41E4, 0x211A));
    CHECK_THROWS(k.switch_profile(2), SafetyError);          // outside the rotation
    WriteOptions outside;
    outside.allow_outside_rotation = true;
    k.switch_profile(2, outside);
    CHECK_EQ(kb.active(), 2);
    k.switch_profile(0, outside);

    FuncConfig fc = k.funcconfig(0);
    fc.raw[4] = 0x01;                                         // 8 kHz on the active profile
    CHECK_THROWS(k.set_funcconfig(0, fc), SafetyError);
    k.set_funcconfig(2, fc);                                  // inactive profile: allowed
    CHECK_EQ(int(k.funcconfig(2).report_rate()), 1);
    fc.raw[2] = 0;                                            // marker missing
    CHECK_THROWS(k.set_funcconfig(3, fc), SafetyError);

    CHECK_THROWS(k.set_key(0, 1, 50, KeyCode{0xF0, 0x08, 0x00}), SafetyError);
    k.set_key(0, 0, 3, *parse_keycode("Esc"));
    CHECK(k.keymap(0, 0)[3] == (KeyCode{0x10, 0x00, 0x29}));

    k.set_write_limit(k.writes() + 1);
    k.set_key(0, 0, 4, *parse_keycode("B"));
    CHECK_THROWS(k.set_key(0, 0, 5, *parse_keycode("C")), SafetyError);
}

TEST(keyboard_macros_write_and_read)
{
    using K = MacroAction::Kind;
    FakeKeyboard kb;
    Keyboard k(kb, *find_model(0x41E4, 0x211A));
    k.set_macros(MacroSet{});
    CHECK_EQ(k.macro_area().size(), std::size_t(164));
    MacroSet set;
    set[3] = {{K::Modifier, 0x02, true, 20}, {K::Key, 0x05, true, 20}, {K::Key, 0x05, false, 20},
              {K::Modifier, 0x02, false, 20}};
    k.set_macros(set);
    CHECK(k.macros()[3] == set[3]);
    const int writes = kb.flash_writes;
    k.set_macros(set);                                        // unchanged: no flash write
    CHECK_EQ(kb.flash_writes, writes);
}

TEST(keyboard_backup_restore_only_writes_differences)
{
    FakeKeyboard kb;
    Keyboard k(kb, *find_model(0x41E4, 0x211A));
    k.set_macros(MacroSet{});
    auto img = k.backup();
    CHECK_EQ(img.firmware, std::string("116"));
    CHECK_EQ(img.profiles.size(), std::size_t(4));

    const auto snapshot = kb.mem;
    k.set_key(1, 2, 10, *parse_keycode("Ctrl+C"));
    TriggerEntry t = k.triggers(3)[7];
    t.set_actuation(200);
    k.set_trigger(3, 7, t);
    FuncConfig fc = k.funcconfig(1);
    fc.set_light_effect(9);
    k.set_funcconfig(1, fc);

    int writes = k.restore(img);
    CHECK(kb.mem == snapshot);
    CHECK_EQ(writes, 1 + 1 + 2);                             // key, trigger entry, FuncConfig (2 packets)
    CHECK_EQ(k.restore(img), 0);                             // nothing left to do
}

TEST(depth_sample_parse)
{
    Report r{};
    auto b = from_hex("A0 10 00 04 03 E8 01 2C DD 01 FF 01 08 05 01 55 0B B8 03 E8");
    std::copy(b.begin(), b.end(), r.begin());
    auto s = parse_depth_sample(r);
    CHECK(s.has_value());
    CHECK(s->key == (KeyCode{0x10, 0x00, 0x04}));
    CHECK_EQ(s->normalized, 1000);
    CHECK_EQ(s->travel, 300);
    CHECK(s->pressed);
    CHECK(s->calibrated);
    CHECK_EQ(s->max_travel, 341);
    CHECK_EQ(s->adc, 3000);
    CHECK_EQ(s->adc_rest, 4000);
}

TEST(macro_text_parse)
{
    using K = MacroAction::Kind;
    // A combination tap expands like the official driver's records.
    auto m = parse_macro("Ctrl+C", 20);
    Macro want = {{K::Modifier, 0x01, true, 20}, {K::Key, 0x06, true, 20}, {K::Key, 0x06, false, 20},
                  {K::Modifier, 0x01, false, 20}};
    CHECK(m == want);
    // Explicit presses, delays after the previous action, a leading pause, a wait record.
    m = parse_macro("500ms +LShift +B 30ms -B wait 1000ms -LShift", 10);
    Macro want2 = {{K::Delay, 0, false, 500}, {K::Modifier, 0x02, true, 10}, {K::Key, 0x05, true, 30},
                   {K::Key, 0x05, false, 10}, {K::Delay, 0, false, 1000}, {K::Modifier, 0x02, false, 10}};
    CHECK(m == want2);
    // Typing adds Shift where needed.
    m = parse_macro("\"Hi!\"", 5);
    CHECK_EQ(m.size(), std::size_t(10));   // Shift+H (4), i (2), Shift+1 (4)
    CHECK(m[0] == (MacroAction{K::Modifier, 0x02, true, 5}));
    CHECK(m[1] == (MacroAction{K::Key, 0x0B, true, 5}));
    CHECK(m[4] == (MacroAction{K::Key, 0x0C, true, 5}));
    CHECK(m[7] == (MacroAction{K::Key, 0x1E, true, 5}));
    CHECK_THROWS(parse_macro("NoSuchKey"), std::invalid_argument);
    CHECK_THROWS(parse_macro("+Ctrl+C"), std::invalid_argument);
    CHECK_THROWS(parse_macro("\"unterminated"), std::invalid_argument);
}

TEST(macro_text_roundtrip)
{
    for (const char* text : {"Ctrl+Shift+Esc", "500ms +LShift +B 30ms -B wait 1000ms -LShift", "\"a,b. C\"",
                             "+Comma -Comma - = 200ms"}) {
        auto m = parse_macro(text, 15);
        auto back = parse_macro(format_macro(m), 99);
        CHECK(back == m);
    }
    CHECK_EQ(format_macro(parse_macro("Ctrl+C", 20)), std::string("+LCtrl 20ms +C 20ms -C 20ms -LCtrl 20ms"));
}
