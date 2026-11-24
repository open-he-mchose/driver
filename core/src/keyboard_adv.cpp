// SPDX-License-Identifier: MIT
// Advanced-key operations of Keyboard.
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "mchose/glw/keyboard.hpp"

namespace mchose::glw {

std::vector<AdvBinding> Keyboard::adv_bindings(int profile)
{
    check_profile(profile);
    std::vector<std::vector<KeyCode>> layers;
    for (int l = 0; l < model_.layers; ++l)
        layers.push_back(keymap(profile, l));
    return find_adv_bindings(layers, model_.keys);
}

AdvTables Keyboard::adv_tables(int profile)
{
    check_profile(profile);
    AdvTables t;
    t.mt = session_.read(cmd::GetMt, model_.mt_offset(profile), 6 * kAdvSlots);
    t.tgl = session_.read(cmd::GetTgl, model_.tgl_offset(profile), 3 * kAdvSlots);
    t.dks = session_.read(cmd::GetDks, model_.dks_offset(profile), 24 * kAdvSlots);
    return t;
}

KeyCode Keyboard::plain_code(int profile, int key)
{
    check_key(key);
    const KeyCode cur = keymap(profile, 0)[static_cast<std::size_t>(key)];
    if (cur.type == KeyCode::Keyboard && !(cur.code1 == 0 && cur.code2 == 0))
        return cur;
    return factory_keymap(0)[static_cast<std::size_t>(key)];
}

void Keyboard::set_trigger_priority(int profile, int key, std::uint8_t priority)
{
    TriggerEntry e = triggers(profile)[static_cast<std::size_t>(key)];
    if (e.priority() == priority)
        return;
    e.set_priority(priority);
    set_trigger(profile, key, e);
}

void Keyboard::check_free(const std::vector<AdvBinding>& bindings, int layer, int key)
{
    for (const auto& b : bindings)
        if (b.layer == layer && b.key == key)
            throw SafetyError("key " + std::to_string(key) + " already has " + to_string(b.kind) + " in layer " +
                              std::to_string(layer) + "; clear it first");
}

void Keyboard::bind_pair(int profile, int layer, AdvKind kind, int key1, int key2, SocdMode mode)
{
    if (!adv_is_pair(kind))
        throw std::invalid_argument("bind_pair: RS, SOCD or LT only");
    check_key(key1);
    check_key(key2);
    if (key1 == key2)
        throw std::invalid_argument("a pair needs two different keys");
    auto bindings = adv_bindings(profile);
    check_free(bindings, layer, key1);
    check_free(bindings, layer, key2);
    const auto slots = free_adv_slots(bindings, AdvTable::Mt, 2);
    const KeyCode k1 = plain_code(profile, key1), k2 = plain_code(profile, key2);
    if (k1.type != KeyCode::Keyboard || k2.type != KeyCode::Keyboard)
        throw SafetyError("pairs must be plain keys (the firmware only sends modifiers and a usage)");
    const auto b1 = k1.bytes(), b2 = k2.bytes();
    const std::array<std::uint8_t, 6> e1{b1[0], b1[1], b1[2], b2[0], b2[1], b2[2]};
    const std::array<std::uint8_t, 6> e2{b2[0], b2[1], b2[2], b1[0], b1[1], b1[2]};
    write_verified(cmd::SetMt, cmd::GetMt, model_.mt_offset(profile) + 6 * slots[0], e1);
    write_verified(cmd::SetMt, cmd::GetMt, model_.mt_offset(profile) + 6 * slots[1], e2);
    if (kind == AdvKind::Socd) {
        const auto p = static_cast<std::uint8_t>(mode);
        set_trigger_priority(profile, key1, p);
        set_trigger_priority(profile, key2, socd_partner_priority(p));
    }
    const std::uint8_t t = adv_keycode_type(kind);
    set_key(profile, layer, key1, KeyCode{t, static_cast<std::uint8_t>(slots[0]), static_cast<std::uint8_t>(key2)});
    set_key(profile, layer, key2, KeyCode{t, static_cast<std::uint8_t>(slots[1]), static_cast<std::uint8_t>(key1)});
}

void Keyboard::bind_mod_tap(int profile, int layer, int key, const KeyCode& tap, const KeyCode& hold, int hold_ms)
{
    check_key(key);
    if (hold_ms < 10 || hold_ms > 2550)
        throw std::invalid_argument("Mod-Tap hold time must be 10-2550 ms");
    auto bindings = adv_bindings(profile);
    check_free(bindings, layer, key);
    const int slot = free_adv_slots(bindings, AdvTable::Mt, 1)[0];
    const auto a = tap.bytes(), b = hold.bytes();
    const std::array<std::uint8_t, 6> e{a[0], a[1], a[2], b[0], b[1], b[2]};
    write_verified(cmd::SetMt, cmd::GetMt, model_.mt_offset(profile) + 6 * slot, e);
    set_key(profile, layer, key, KeyCode{KeyCode::ModTap, static_cast<std::uint8_t>(slot), static_cast<std::uint8_t>(hold_ms / 10)});
}

void Keyboard::bind_toggle(int profile, int layer, int key, std::optional<KeyCode> code)
{
    check_key(key);
    auto bindings = adv_bindings(profile);
    check_free(bindings, layer, key);
    const int slot = free_adv_slots(bindings, AdvTable::Toggle, 1)[0];
    const auto e = (code ? *code : plain_code(profile, key)).bytes();
    write_verified(cmd::SetTgl, cmd::GetTgl, model_.tgl_offset(profile) + 3 * slot, e);
    set_key(profile, layer, key, KeyCode{KeyCode::Toggle, static_cast<std::uint8_t>(slot), 0});
}

void Keyboard::bind_dks(int profile, int layer, int key, const DksEntry& entry)
{
    check_key(key);
    const auto e = entry.encode();
    auto bindings = adv_bindings(profile);
    check_free(bindings, layer, key);
    const int slot = free_adv_slots(bindings, AdvTable::Dks, 1)[0];
    write_verified(cmd::SetDks, cmd::GetDks, model_.dks_offset(profile) + 24 * slot, e);
    set_key(profile, layer, key, KeyCode{KeyCode::Dks, static_cast<std::uint8_t>(slot), 0});
}

std::vector<int> Keyboard::clear_adv(int profile, int layer, int key)
{
    auto bindings = adv_bindings(profile);
    std::vector<AdvBinding> todo;
    for (const auto& b : bindings)
        if (b.layer == layer && b.key == key)
            todo.push_back(b);
    if (todo.empty())
        return {};
    if (adv_is_pair(todo[0].kind))
        for (const auto& b : bindings)
            if (b.layer == layer && b.key == todo[0].code2 && b.key != key)
                todo.push_back(b);
    const auto factory = factory_keymap(0);
    std::vector<int> cleared;
    for (const auto& b : todo) {
        const KeyCode restore = layer == 0 ? factory[static_cast<std::size_t>(b.key)] : KeyCode{KeyCode::Keyboard, 0, 0};
        WriteOptions opt;
        opt.allow_dangerous = true;   // the factory code is what the key had before
        set_key(profile, layer, b.key, restore, opt);
        if (b.kind == AdvKind::Socd)
            set_trigger_priority(profile, b.key, 0);
        if (b.slot < kAdvSlots) {
            switch (adv_table(b.kind)) {
            case AdvTable::Mt: {
                const std::array<std::uint8_t, 6> z{};
                write_verified(cmd::SetMt, cmd::GetMt, model_.mt_offset(profile) + 6 * b.slot, z);
                break;
            }
            case AdvTable::Toggle: {
                const std::array<std::uint8_t, 3> z{};
                write_verified(cmd::SetTgl, cmd::GetTgl, model_.tgl_offset(profile) + 3 * b.slot, z);
                break;
            }
            case AdvTable::Dks: {
                const std::array<std::uint8_t, 24> z{};
                write_verified(cmd::SetDks, cmd::GetDks, model_.dks_offset(profile) + 24 * b.slot, z);
                break;
            }
            }
        }
        cleared.push_back(b.key);
    }
    return cleared;
}

}  // namespace mchose::glw
