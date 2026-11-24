// SPDX-License-Identifier: MIT
// Advanced keys: DKS, Toggle, Mod-Tap, Rapid Strike, Snap Tap (SOCD), LT
// (https://open-he-mchose.github.io/protocols/glw/advanced-keys/).
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "mchose/glw/keycode.hpp"

namespace mchose::glw {

inline constexpr int kAdvSlots = 32;

enum class AdvKind { Dks, Toggle, ModTap, RapidStrike, Socd, Lt };
enum class AdvTable { Dks, Toggle, Mt };   // MT, RS, SOCD and LT share the MT table

std::optional<AdvKind> adv_kind(std::uint8_t keycode_type);
std::uint8_t adv_keycode_type(AdvKind k);
AdvTable adv_table(AdvKind k);
bool adv_is_pair(AdvKind k);   // RS, SOCD, LT bind two keys to each other
const char* to_string(AdvKind k);

// SOCD mode = the "priority" nibble of both keys' trigger entries.
enum class SocdMode : std::uint8_t { LastInput = 0, Key1Priority = 1, Key2Priority = 2, Cancel = 3 };
std::uint8_t socd_partner_priority(std::uint8_t p);   // 0->0, 1->2, 2->1, 3->3
const char* to_string(SocdMode m);

struct AdvBinding {
    int layer = 0;
    int key = 0;
    AdvKind kind = AdvKind::Dks;
    int slot = 0;
    std::uint8_t code2 = 0;   // Mod-Tap: hold time / 10 ms; pairs: partner key index
};

// All advanced bindings in the given layers (layers[l][key]).
std::vector<AdvBinding> find_adv_bindings(std::span<const std::vector<KeyCode>> layers, int keys);
// First `n` free slots of a table. Throws std::runtime_error if there are not enough.
std::vector<int> free_adv_slots(const std::vector<AdvBinding>& bindings, AdvTable table, int n = 1);

// --- DKS -------------------------------------------------------------------------------------------
// How many phases an action stays active when it starts at press-start / bottom / lift-from-bottom /
// full-lift (0 = not used there). Ranges 0-4 / 0-3 / 0-2 / 0-1.
struct DksSpans {
    int down_start = 0, down_end = 0, up_start = 0, up_end = 0;
    friend bool operator==(const DksSpans&, const DksSpans&) = default;
};
// Same encoding and normalisation as the official driver.
std::uint16_t dks_mask(const DksSpans& s);
DksSpans dks_unmask(std::uint16_t mask);

struct DksAction {
    KeyCode code;
    DksSpans spans;
};

struct DksEntry {
    int shallow = 10;   // 0.1 mm
    int deep = 30;      // 0.1 mm
    std::vector<DksAction> actions;   // up to 4

    std::array<std::uint8_t, 24> encode() const;   // throws std::invalid_argument on bad points / too many actions
    static DksEntry decode(std::span<const std::uint8_t> e);
};

// Readable problems in a profile's advanced bindings (slots out of range, shared slots, empty entries,
// partners that do not point back, invalid SOCD priority pairs). `priority[key]` = trigger priority nibble.
struct AdvTables {
    std::vector<std::uint8_t> mt;    // 32 x 6
    std::vector<std::uint8_t> tgl;   // 32 x 3
    std::vector<std::uint8_t> dks;   // 32 x 24
};
// What a binding does, e.g. "tap Esc, hold LCtrl after 200 ms" or "sends A, partner D (#11), last-input".
// `priority` = the key's trigger priority nibble (SOCD mode); key_name(index) names partner keys.
std::string describe_adv(const AdvBinding& b, const AdvTables& tables, std::uint8_t priority,
                         const std::function<std::string(int)>& key_name);

std::vector<std::string> adv_problems(const std::vector<AdvBinding>& bindings, const AdvTables& tables,
                                      std::span<const std::uint8_t> priority);
// Table entries that are filled but not referenced by any binding (harmless leftovers).
std::vector<std::string> adv_orphans(const std::vector<AdvBinding>& bindings, const AdvTables& tables);

}  // namespace mchose::glw
