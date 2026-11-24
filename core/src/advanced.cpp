// SPDX-License-Identifier: MIT
#include "mchose/glw/advanced.hpp"

#include <algorithm>
#include <cstdio>
#include <cstddef>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace mchose::glw {

std::optional<AdvKind> adv_kind(std::uint8_t t)
{
    switch (t) {
    case KeyCode::Dks: return AdvKind::Dks;
    case KeyCode::Toggle: return AdvKind::Toggle;
    case KeyCode::ModTap: return AdvKind::ModTap;
    case KeyCode::RapidStrike: return AdvKind::RapidStrike;
    case KeyCode::Socd: return AdvKind::Socd;
    case KeyCode::Lt: return AdvKind::Lt;
    default: return std::nullopt;
    }
}

std::uint8_t adv_keycode_type(AdvKind k)
{
    switch (k) {
    case AdvKind::Dks: return KeyCode::Dks;
    case AdvKind::Toggle: return KeyCode::Toggle;
    case AdvKind::ModTap: return KeyCode::ModTap;
    case AdvKind::RapidStrike: return KeyCode::RapidStrike;
    case AdvKind::Socd: return KeyCode::Socd;
    case AdvKind::Lt: return KeyCode::Lt;
    }
    return 0;
}

AdvTable adv_table(AdvKind k)
{
    switch (k) {
    case AdvKind::Dks: return AdvTable::Dks;
    case AdvKind::Toggle: return AdvTable::Toggle;
    default: return AdvTable::Mt;
    }
}

bool adv_is_pair(AdvKind k) { return k == AdvKind::RapidStrike || k == AdvKind::Socd || k == AdvKind::Lt; }

const char* to_string(AdvKind k)
{
    switch (k) {
    case AdvKind::Dks: return "DKS";
    case AdvKind::Toggle: return "Toggle";
    case AdvKind::ModTap: return "ModTap";
    case AdvKind::RapidStrike: return "RS";
    case AdvKind::Socd: return "SOCD";
    case AdvKind::Lt: return "LT";
    }
    return "?";
}

std::uint8_t socd_partner_priority(std::uint8_t p)
{
    switch (p) {
    case 1: return 2;
    case 2: return 1;
    default: return p;
    }
}

const char* to_string(SocdMode m)
{
    switch (m) {
    case SocdMode::LastInput: return "last-input";
    case SocdMode::Key1Priority: return "key1-priority";
    case SocdMode::Key2Priority: return "key2-priority";
    case SocdMode::Cancel: return "cancel";
    }
    return "?";
}

std::vector<AdvBinding> find_adv_bindings(std::span<const std::vector<KeyCode>> layers, int keys)
{
    std::vector<AdvBinding> out;
    for (std::size_t l = 0; l < layers.size(); ++l)
        for (int k = 0; k < keys && k < static_cast<int>(layers[l].size()); ++k)
            if (auto kind = adv_kind(layers[l][static_cast<std::size_t>(k)].type))
                out.push_back({static_cast<int>(l), k, *kind, layers[l][static_cast<std::size_t>(k)].code1,
                               layers[l][static_cast<std::size_t>(k)].code2});
    return out;
}

std::vector<int> free_adv_slots(const std::vector<AdvBinding>& bindings, AdvTable table, int n)
{
    std::set<int> used;
    for (const auto& b : bindings)
        if (adv_table(b.kind) == table)
            used.insert(b.slot);
    std::vector<int> out;
    for (int s = 0; s < kAdvSlots && static_cast<int>(out.size()) < n; ++s)
        if (!used.count(s))
            out.push_back(s);
    if (static_cast<int>(out.size()) < n)
        throw std::runtime_error("no free slot left in the advanced-key table (" + std::to_string(used.size()) + " of 32 used)");
    return out;
}

// --- DKS -------------------------------------------------------------------------------------------

std::uint16_t dks_mask(const DksSpans& s)
{
    int a = s.down_start, r = s.down_end, o = s.up_start;
    const int e = s.up_end;
    if (s.down_start == 4) { a = 3; r = 3; o = 2; }
    if (s.down_start == 3) { a = 3; r = 2; }
    if (s.down_end == 3) o = 2;
    auto th = [](int n) { return (n > 0 ? 1 : 0) | (n > 1 ? 2 : 0) | (n > 2 ? 4 : 0); };
    return static_cast<std::uint16_t>((th(a) & 7) | ((th(r) << 3) & 56) | ((th(o) << 6) & 448) | ((th(e) << 9) & 512));
}

DksSpans dks_unmask(std::uint16_t v)
{
    const int a0 = v & 7, a1 = (v >> 3) & 7, a2 = (v >> 6) & 7, a3 = (v >> 9) & 1;
    auto hi = [](int b) { return (b & 4) ? 3 : (b & 2) ? 2 : (b & 1) ? 1 : 0; };
    DksSpans s{hi(a0), hi(a1), (a2 & 2) ? 2 : (a2 & 1) ? 1 : 0, a3};
    if (s.down_start == 3) {
        if (s.down_end == 3) {
            s = {4, 0, 0, s.up_end};
        } else {
            s.down_end = 0;
        }
    } else if (s.down_end == 3) {
        s.up_start = 0;
    }
    return s;
}

std::array<std::uint8_t, 24> DksEntry::encode() const
{
    if (!(1 <= shallow && shallow < deep && deep <= 34))
        throw std::invalid_argument("DKS points must satisfy 0.1 <= shallow < deep <= 3.4 mm");
    if (actions.size() > 4)
        throw std::invalid_argument("DKS takes at most 4 actions");
    std::array<std::uint8_t, 24> e{};
    e[0] = static_cast<std::uint8_t>(shallow);
    e[1] = static_cast<std::uint8_t>(deep);
    e[2] = static_cast<std::uint8_t>(deep);
    e[3] = static_cast<std::uint8_t>(shallow);
    for (std::size_t n = 0; n < actions.size(); ++n) {
        const std::size_t b = 4 + 5 * n;
        const auto c = actions[n].code.bytes();
        std::copy(c.begin(), c.end(), e.begin() + static_cast<long>(b));
        const std::uint16_t m = dks_mask(actions[n].spans);
        e[b + 3] = static_cast<std::uint8_t>(m & 0xFF);
        e[b + 4] = static_cast<std::uint8_t>(m >> 8);
    }
    return e;
}

DksEntry DksEntry::decode(std::span<const std::uint8_t> e)
{
    if (e.size() < 24)
        throw std::invalid_argument("DKS entry needs 24 bytes");
    DksEntry d;
    d.shallow = e[0];
    d.deep = e[1];
    for (std::size_t n = 0; n < 4; ++n) {
        const std::size_t b = 4 + 5 * n;
        KeyCode code{e[b], e[b + 1], e[b + 2]};
        const std::uint16_t m = static_cast<std::uint16_t>(e[b + 3] | (e[b + 4] << 8));
        if (code != KeyCode{} || m)
            d.actions.push_back({code, dks_unmask(m)});
    }
    return d;
}

// --- description ----------------------------------------------------------------------------------

std::string describe_adv(const AdvBinding& b, const AdvTables& tables, std::uint8_t priority,
                         const std::function<std::string(int)>& key_name)
{
    if (b.slot >= kAdvSlots)
        return "(slot out of range!)";
    const auto slot = static_cast<std::size_t>(b.slot);
    auto code_at = [](const std::vector<std::uint8_t>& v, std::size_t off) {
        return off + 2 < v.size() ? KeyCode{v[off], v[off + 1], v[off + 2]} : KeyCode{};
    };
    switch (b.kind) {
    case AdvKind::ModTap:
        return "tap " + to_string(code_at(tables.mt, 6 * slot)) + ", hold " + to_string(code_at(tables.mt, 6 * slot + 3)) +
               " after " + std::to_string(b.code2 * 10) + " ms";
    case AdvKind::RapidStrike:
    case AdvKind::Socd:
    case AdvKind::Lt: {
        std::string s = "sends " + to_string(code_at(tables.mt, 6 * slot)) + ", partner " + key_name(b.code2) + " (#" +
                        std::to_string(b.code2) + ")";
        if (b.kind == AdvKind::Socd)
            s += ", " + std::string(to_string(static_cast<SocdMode>(priority & 3)));
        return s;
    }
    case AdvKind::Toggle:
        return "toggles " + to_string(code_at(tables.tgl, 3 * slot));
    case AdvKind::Dks: {
        if (tables.dks.size() < 24 * (slot + 1))
            return "?";
        auto d = DksEntry::decode(std::span<const std::uint8_t>(tables.dks).subspan(24 * slot, 24));
        char pts[48];
        std::snprintf(pts, sizeof pts, "points %.1f / %.1f mm:", d.shallow / 10.0, d.deep / 10.0);
        std::string s = pts;
        for (const auto& a : d.actions)
            s += " " + to_string(a.code) + " " + std::to_string(a.spans.down_start) + "," + std::to_string(a.spans.down_end) +
                 "," + std::to_string(a.spans.up_start) + "," + std::to_string(a.spans.up_end) + ";";
        return s;
    }
    }
    return {};
}

// --- checks ----------------------------------------------------------------------------------------

namespace {

std::span<const std::uint8_t> entry_of(const AdvTables& t, AdvTable table, int slot)
{
    switch (table) {
    case AdvTable::Mt: return std::span<const std::uint8_t>(t.mt).subspan(static_cast<std::size_t>(6 * slot), 6);
    case AdvTable::Toggle: return std::span<const std::uint8_t>(t.tgl).subspan(static_cast<std::size_t>(3 * slot), 3);
    case AdvTable::Dks: return std::span<const std::uint8_t>(t.dks).subspan(static_cast<std::size_t>(24 * slot), 24);
    }
    return {};
}

const char* table_name(AdvTable t) { return t == AdvTable::Mt ? "MT" : t == AdvTable::Toggle ? "Toggle" : "DKS"; }

std::string where(const AdvBinding& b)
{
    return "layer " + std::to_string(b.layer) + " key " + std::to_string(b.key) + " " + to_string(b.kind);
}

}  // namespace

std::vector<std::string> adv_problems(const std::vector<AdvBinding>& bindings, const AdvTables& tables,
                                      std::span<const std::uint8_t> priority)
{
    std::vector<std::string> out;
    std::map<std::pair<AdvTable, int>, std::string> seen;
    for (const auto& b : bindings) {
        const AdvTable t = adv_table(b.kind);
        if (b.slot >= kAdvSlots) {
            out.push_back(where(b) + ": slot " + std::to_string(b.slot) + " beyond the 32-slot table");
            continue;
        }
        auto key = std::make_pair(t, b.slot);
        if (seen.count(key))
            out.push_back(where(b) + " shares " + table_name(t) + " slot " + std::to_string(b.slot) + " with " + seen[key]);
        seen[key] = where(b);
        auto e = entry_of(tables, t, b.slot);
        if (std::all_of(e.begin(), e.end(), [](std::uint8_t x) { return x == 0; }))
            out.push_back(where(b) + ": " + table_name(t) + " slot " + std::to_string(b.slot) + " is empty");
        if (adv_is_pair(b.kind)) {
            auto partner = std::find_if(bindings.begin(), bindings.end(), [&](const AdvBinding& x) {
                return x.layer == b.layer && x.key == b.code2;
            });
            if (partner == bindings.end() || partner->kind != b.kind || partner->code2 != b.key)
                out.push_back(where(b) + ": partner key " + std::to_string(b.code2) + " does not point back");
            if (!e.empty() && e[0] != KeyCode::Keyboard)
                out.push_back(where(b) + ": the entry is not a plain key; the firmware only uses modifiers and usage");
            if (b.kind == AdvKind::Socd && partner != bindings.end() && b.key < static_cast<int>(priority.size()) &&
                partner->key < static_cast<int>(priority.size()) &&
                socd_partner_priority(priority[static_cast<std::size_t>(b.key)]) != priority[static_cast<std::size_t>(partner->key)])
                out.push_back(where(b) + ": SOCD priorities " + std::to_string(priority[static_cast<std::size_t>(b.key)]) + "/" +
                              std::to_string(priority[static_cast<std::size_t>(partner->key)]) + " are not a valid pair");
        }
    }
    return out;
}

std::vector<std::string> adv_orphans(const std::vector<AdvBinding>& bindings, const AdvTables& tables)
{
    std::vector<std::string> out;
    for (AdvTable t : {AdvTable::Mt, AdvTable::Toggle, AdvTable::Dks}) {
        std::set<int> used;
        for (const auto& b : bindings)
            if (adv_table(b.kind) == t)
                used.insert(b.slot);
        for (int s = 0; s < kAdvSlots; ++s) {
            auto e = entry_of(tables, t, s);
            if (e.size() && std::any_of(e.begin(), e.end(), [](std::uint8_t x) { return x != 0; }) && !used.count(s))
                out.push_back(std::string(table_name(t)) + " " + std::to_string(s));
        }
    }
    return out;
}

}  // namespace mchose::glw
