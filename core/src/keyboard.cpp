// SPDX-License-Identifier: MIT
#include "mchose/glw/keyboard.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace mchose::glw {

std::optional<DepthSample> parse_depth_sample(const Report& r)
{
    if (r[0] != report::DepthStream)
        return std::nullopt;
    auto be16 = [&](int i) { return (r[i] << 8) | r[i + 1]; };
    DepthSample s;
    s.key = KeyCode{r[1], r[2], r[3]};
    s.normalized = be16(4);
    s.travel = be16(6);
    s.pressed = r[9] == 0x01;
    s.calibrated = r[10] == 0xFF;
    s.max_travel = be16(14);
    s.adc = be16(16);
    s.adc_rest = be16(18) * 4;
    return s;
}

double depth_mm(const DepthSample& s)
{
    if (s.adc_rest - s.adc < 20)
        return 0.0;
    return s.travel / 100.0;
}

Keyboard::Keyboard(Transport& transport, Model model) : session_(transport), model_(std::move(model)) {}

void Keyboard::count_write(std::size_t n)
{
    if (writes_ + n > write_limit_)
        throw SafetyError("flash write limit of " + std::to_string(write_limit_) +
                          " reached in this session (protection against write loops)");
    writes_ += n;
}

void Keyboard::check_profile(int profile) const
{
    if (profile < 0 || profile >= model_.profiles)
        throw std::out_of_range("profile " + std::to_string(profile) + " out of range 0.." +
                                std::to_string(model_.profiles - 1));
}

void Keyboard::check_key(int key) const
{
    if (key < 0 || key >= model_.key_slots)
        throw std::out_of_range("key index " + std::to_string(key) + " out of range");
}

void Keyboard::write_verified(std::uint8_t write_cmd, std::uint8_t read_cmd, int offset, std::span<const std::uint8_t> data)
{
    count_write((data.size() + kMaxChunk - 1) / kMaxChunk);
    session_.write(write_cmd, static_cast<std::uint32_t>(offset), data);
    auto back = session_.read(read_cmd, static_cast<std::uint32_t>(offset), data.size());
    if (!std::equal(data.begin(), data.end(), back.begin()))
        throw ProtocolError("read-back mismatch after writing command " + std::to_string(write_cmd) + " at offset " +
                            std::to_string(offset));
}

// --- reading --------------------------------------------------------------------------------------

FirmwareInfo Keyboard::info() { return FirmwareInfo::parse(session_.transact(cmd::Info, 0, kMaxChunk)); }

BaseInfo Keyboard::base_info() { return BaseInfo::parse(session_.read(cmd::GetBase, 0, 56)); }

int Keyboard::active_profile() { return base_info().runtime_profile(); }

FuncConfig Keyboard::funcconfig(int profile)
{
    check_profile(profile);
    return FuncConfig::parse(session_.read(cmd::GetFuncConfig, model_.funcconfig_offset(profile), 64));
}

std::vector<TriggerEntry> Keyboard::triggers(int profile)
{
    check_profile(profile);
    auto raw = session_.read(cmd::GetTrigger, model_.trigger_offset(profile), 8 * static_cast<std::size_t>(model_.key_slots));
    std::vector<TriggerEntry> out;
    for (int k = 0; k < model_.key_slots; ++k)
        out.push_back(TriggerEntry::parse(std::span<const std::uint8_t>(raw).subspan(8 * k, 8)));
    return out;
}

std::vector<KeyCode> Keyboard::keymap(int profile, int layer)
{
    check_profile(profile);
    auto raw = session_.read(cmd::GetKeymap, model_.keymap_offset(profile, layer), 3 * static_cast<std::size_t>(model_.key_slots));
    std::vector<KeyCode> out;
    for (int k = 0; k < model_.key_slots; ++k)
        out.push_back({raw[3 * k], raw[3 * k + 1], raw[3 * k + 2]});
    return out;
}

std::vector<KeyCode> Keyboard::factory_keymap(int layer)
{
    auto raw = session_.read(cmd::GetFactoryKeymap, 512 * layer, 3 * static_cast<std::size_t>(model_.key_slots));
    std::vector<KeyCode> out;
    for (int k = 0; k < model_.key_slots; ++k)
        out.push_back({raw[3 * k], raw[3 * k + 1], raw[3 * k + 2]});
    return out;
}

std::vector<std::array<std::uint8_t, 3>> Keyboard::colors(int profile)
{
    check_profile(profile);
    auto raw = session_.read(cmd::GetColors, model_.colors_offset(profile), 3 * static_cast<std::size_t>(model_.key_slots));
    std::vector<std::array<std::uint8_t, 3>> out;
    for (int k = 0; k < model_.key_slots; ++k)
        out.push_back({raw[3 * k], raw[3 * k + 1], raw[3 * k + 2]});
    return out;
}

std::vector<std::uint8_t> Keyboard::macro_area()
{
    std::vector<std::uint8_t> area = session_.read(cmd::GetMacros, 0, 64);
    const std::size_t total = static_cast<std::size_t>(model_.macro_area);
    std::size_t want;
    if (area[32] == 0x81 && area[33] == 0x7E && area[50] == 'M' && area[51] == 'C') {
        // Official driver layout: the header says where the used part ends.
        want = std::min(total, static_cast<std::size_t>(area[52] | (area[53] << 8)) + 6 * kMacroCount);
    } else {
        want = total;   // unknown layout: read everything, then trim
    }
    if (want > area.size()) {
        auto more = session_.read(cmd::GetMacros, static_cast<std::uint32_t>(area.size()), want - area.size());
        area.insert(area.end(), more.begin(), more.end());
    }
    area.resize(std::min(area.size(), macro_area_used_size(area)));
    return area;
}

MacroSet Keyboard::macros() { return decode_macro_area(macro_area()); }

std::vector<std::uint8_t> Keyboard::raw_table(std::uint8_t read_command, int offset, int length)
{
    return session_.read(read_command, static_cast<std::uint32_t>(offset), static_cast<std::size_t>(length));
}

// --- writing --------------------------------------------------------------------------------------

void Keyboard::switch_profile(int profile, const WriteOptions& opt)
{
    check_profile(profile);
    BaseInfo b = base_info();
    if (!b.marker_ok())
        throw SafetyError("base info has no AA BB marker; not touching it");
    auto pos = b.position_of(profile);
    if (!pos)
        throw SafetyError("profile " + std::to_string(profile) + " is not in the base-info order list");
    if (!b.in_rotation(profile) && !opt.allow_outside_rotation)
        throw SafetyError("profile " + std::to_string(profile) + " is outside the rotation (length " +
                          std::to_string(b.length()) + "); the keyboard would fall back at the next boot");
    if (b.runtime_profile() == profile && b.position() == *pos)
        return;
    const std::uint8_t p = static_cast<std::uint8_t>(*pos);
    count_write();
    session_.transact(cmd::SetBase, 0, 1, std::span<const std::uint8_t>(&p, 1));
    if (base_info().position() != p)
        throw ProtocolError("profile switch not confirmed by base info");
}

void Keyboard::set_funcconfig(int profile, const FuncConfig& fc, const WriteOptions& opt)
{
    check_profile(profile);
    if (!fc.marker_ok())
        throw SafetyError("FuncConfig without the AA BB marker would be replaced by firmware defaults");
    const FuncConfig current = funcconfig(profile);
    if (current.raw == fc.raw)
        return;
    if (fc.report_rate() != current.report_rate()) {
        if (!report_rate_hz(fc.report_rate()))
            throw SafetyError("report-rate value " + std::to_string(fc.report_rate()) + " is not valid for this model");
        if (profile == active_profile() && !opt.allow_reboot)
            throw SafetyError("changing the report rate of the active profile reboots the keyboard");
    }
    // Same as the official driver: two overlapping 56-byte packets (offset 0 and 8).
    const int base = model_.funcconfig_offset(profile);
    count_write(2);
    session_.transact(cmd::SetFuncConfig, static_cast<std::uint16_t>(base), kMaxChunk,
                      std::span<const std::uint8_t>(fc.raw.data(), kMaxChunk));
    session_.transact(cmd::SetFuncConfig, static_cast<std::uint16_t>(base + 8), kMaxChunk,
                      std::span<const std::uint8_t>(fc.raw.data() + 8, kMaxChunk));
    if (funcconfig(profile).raw != fc.raw)
        throw ProtocolError("FuncConfig read-back mismatch");
}

void Keyboard::set_trigger(int profile, int key, const TriggerEntry& entry)
{
    check_profile(profile);
    check_key(key);
    write_verified(cmd::SetTrigger, cmd::GetTrigger, model_.trigger_offset(profile, key), entry.raw);
}

void Keyboard::set_key(int profile, int layer, int key, const KeyCode& code, const WriteOptions& opt)
{
    check_profile(profile);
    check_key(key);
    if (layer < 0 || layer >= model_.layers)
        throw std::out_of_range("layer out of range");
    if (code.is_dangerous() && !opt.allow_dangerous)
        throw SafetyError("refusing to bind " + to_string(code) + " (it resets or reflashes the keyboard)");
    const auto b = code.bytes();
    write_verified(cmd::SetKeymap, cmd::GetKeymap, model_.keymap_offset(profile, layer, key), b);
}

void Keyboard::set_color(int profile, int key, std::array<std::uint8_t, 3> rgb)
{
    check_profile(profile);
    check_key(key);
    write_verified(cmd::SetColors, cmd::GetColors, model_.colors_offset(profile, key), rgb);
}

void Keyboard::set_macros(const MacroSet& macros)
{
    const auto image = encode_macro_area(macros);
    if (static_cast<int>(image.size()) > model_.macro_area)
        throw std::length_error("macros do not fit into the macro area");
    const auto current = macro_area();
    if (current.size() >= image.size() && std::equal(image.begin(), image.end(), current.begin()))
        return;
    write_verified(cmd::SetMacros, cmd::GetMacros, 0, image);
}

void Keyboard::write_raw_table(std::uint8_t write_command, std::uint8_t read_command, int offset, std::span<const std::uint8_t> data)
{
    write_verified(write_command, read_command, offset, data);
}

void Keyboard::set_colors(int profile, std::span<const std::array<std::uint8_t, 3>> rgb)
{
    check_profile(profile);
    if (static_cast<int>(rgb.size()) > model_.key_slots)
        throw std::out_of_range("more colours than key slots");
    auto want = session_.read(cmd::GetColors, model_.colors_offset(profile), 3 * static_cast<std::size_t>(model_.key_slots));
    for (std::size_t k = 0; k < rgb.size(); ++k)
        std::copy(rgb[k].begin(), rgb[k].end(), want.begin() + static_cast<std::ptrdiff_t>(3 * k));
    sync_region("colours", profile, cmd::GetColors, cmd::SetColors, model_.colors_offset(profile), want, 3, {});
}

void Keyboard::set_report_rate_and_reboot(std::uint8_t value)
{
    if (!report_rate_hz(value))
        throw SafetyError("report-rate value " + std::to_string(value) + " is not valid for this model");
    const int p = active_profile();
    FuncConfig fc = funcconfig(p);
    if (!fc.marker_ok())
        throw SafetyError("FuncConfig without the AA BB marker; not touching it");
    if (fc.report_rate() == value)
        return;
    fc.raw[4] = static_cast<std::uint8_t>((fc.raw[4] & 0xF0) | value);
    // Byte 4 is in the first of the driver's two packets; the keyboard reboots right after it, so the
    // second packet (bytes 8..63, unchanged) is not needed. The reply may never come.
    count_write();
    session_.set_retries(0);
    try {
        session_.transact(cmd::SetFuncConfig, static_cast<std::uint16_t>(model_.funcconfig_offset(p)), kMaxChunk,
                          std::span<const std::uint8_t>(fc.raw.data(), kMaxChunk));
    } catch (const std::exception&) {
        // expected when the keyboard resets before replying
    }
}

void Keyboard::live_colors(std::span<const std::array<std::uint8_t, 3>> rgb)
{
    std::vector<std::uint8_t> flat;
    flat.reserve(rgb.size() * 3);
    for (const auto& c : rgb)
        flat.insert(flat.end(), c.begin(), c.end());
    constexpr std::size_t kChunk = 54;   // 18 keys per packet
    for (std::size_t pos = 0; pos < flat.size(); pos += kChunk) {
        const auto n = std::min(kChunk, flat.size() - pos);
        session_.transact(cmd::LiveColorsMain, static_cast<std::uint16_t>(pos), static_cast<std::uint8_t>(n),
                          std::span<const std::uint8_t>(flat).subspan(pos, n));
    }
}

void Keyboard::set_depth_stream(bool on)
{
    const int p = active_profile();
    FuncConfig fc = funcconfig(p);
    if (fc.debug_mode() == on)
        return;
    fc.set_debug_mode(on);
    set_funcconfig(p, fc);
}

// --- backup / restore -----------------------------------------------------------------------------

ProfileImage Keyboard::read_profile(int p)
{
    check_profile(p);
    const std::size_t slots = static_cast<std::size_t>(model_.key_slots);
    ProfileImage pi;
    pi.funcconfig = funcconfig(p).raw;
    pi.triggers = session_.read(cmd::GetTrigger, model_.trigger_offset(p), 8 * slots);
    for (int l = 0; l < model_.layers && l < 4; ++l)
        pi.keymaps[l] = session_.read(cmd::GetKeymap, model_.keymap_offset(p, l), 3 * slots);
    pi.colors = session_.read(cmd::GetColors, model_.colors_offset(p), 3 * slots);
    pi.dks = session_.read(cmd::GetDks, model_.dks_offset(p), 768);
    pi.mt = session_.read(cmd::GetMt, model_.mt_offset(p), 256);
    pi.tgl = session_.read(cmd::GetTgl, model_.tgl_offset(p), 128);
    return pi;
}

int Keyboard::sync_region(const char* what, int p, std::uint8_t rd, std::uint8_t wr, int offset,
                          const std::vector<std::uint8_t>& want, std::size_t entry, const Log& log)
{
    if (want.empty())
        return 0;
    auto cur = session_.read(rd, static_cast<std::uint32_t>(offset), want.size());
    // Collect runs of consecutive differing entries and write each run in 56-byte packets: far fewer
    // flash writes than one packet per entry.
    int n = 0;
    std::size_t run_start = 0;
    bool in_run = false;
    auto flush = [&](std::size_t end) {
        write_verified(wr, rd, offset + static_cast<int>(run_start),
                       std::span<const std::uint8_t>(want).subspan(run_start, end - run_start));
        in_run = false;
    };
    for (std::size_t i = 0; i < want.size(); i += entry) {
        const std::size_t len = std::min(entry, want.size() - i);
        const auto at = static_cast<std::ptrdiff_t>(i);
        const bool differs = !std::equal(want.begin() + at, want.begin() + at + static_cast<std::ptrdiff_t>(len), cur.begin() + at);
        if (differs) {
            ++n;
            if (!in_run) {
                run_start = i;
                in_run = true;
            }
        } else if (in_run) {
            flush(i);
        }
    }
    if (in_run)
        flush(want.size());
    if (n && log)
        log("profile " + std::to_string(p) + ": " + what + ": " + std::to_string(n) + " entr" + (n == 1 ? "y" : "ies") + " written");
    return n;
}

void Keyboard::write_profile(int p, const ProfileImage& pi, const Log& log, const WriteOptions& opt)
{
    check_profile(p);
    FuncConfig want = FuncConfig::parse(pi.funcconfig);
    FuncConfig cur = funcconfig(p);
    if (want.raw != cur.raw) {
        if (p == active_profile() && want.report_rate() != cur.report_rate() && !opt.allow_reboot) {
            want.raw[4] = static_cast<std::uint8_t>((want.raw[4] & 0xF0) | cur.report_rate());
            if (log)
                log("profile " + std::to_string(p) + ": report rate kept (changing it on the active profile reboots the keyboard)");
        }
        if (want.raw != cur.raw) {
            set_funcconfig(p, want, opt);
            if (log)
                log("profile " + std::to_string(p) + ": FuncConfig written");
        }
    }
    sync_region("triggers", p, cmd::GetTrigger, cmd::SetTrigger, model_.trigger_offset(p), pi.triggers, 8, log);
    for (int l = 0; l < model_.layers && l < 4; ++l)
        sync_region(("keymap layer " + std::to_string(l)).c_str(), p, cmd::GetKeymap, cmd::SetKeymap,
                    model_.keymap_offset(p, l), pi.keymaps[static_cast<std::size_t>(l)], 3, log);
    sync_region("colours", p, cmd::GetColors, cmd::SetColors, model_.colors_offset(p), pi.colors, 3, log);
    sync_region("DKS", p, cmd::GetDks, cmd::SetDks, model_.dks_offset(p), pi.dks, 24, log);
    sync_region("MT/RS/SOCD/LT", p, cmd::GetMt, cmd::SetMt, model_.mt_offset(p), pi.mt, 6, log);
    sync_region("Toggle", p, cmd::GetTgl, cmd::SetTgl, model_.tgl_offset(p), pi.tgl, 3, log);
}

KeyboardImage Keyboard::backup(const Log& progress)
{
    auto say = [&](const std::string& s) { if (progress) progress(s); };
    KeyboardImage img;
    img.model = model_.name;
    say("info");
    img.firmware = info().version_string();
    img.base = base_info().raw;
    say("macros");
    img.macros = macro_area();
    for (int p = 0; p < model_.profiles; ++p) {
        say("profile " + std::to_string(p));
        img.profiles.push_back(read_profile(p));
    }
    return img;
}

int Keyboard::restore(const KeyboardImage& img, const Log& log, const WriteOptions& opt)
{
    auto say = [&](const std::string& s) { if (log) log(s); };
    if (img.model != model_.name)
        throw SafetyError("backup is for '" + img.model + "', keyboard is '" + model_.name + "'");
    if (static_cast<int>(img.profiles.size()) != model_.profiles)
        throw SafetyError("backup has " + std::to_string(img.profiles.size()) + " profiles");
    const std::size_t before = writes_;
    for (int p = 0; p < model_.profiles; ++p)
        write_profile(p, img.profiles[static_cast<std::size_t>(p)], log, opt);

    if (!img.macros.empty()) {
        auto cur = macro_area();
        if (cur.size() < img.macros.size() || !std::equal(img.macros.begin(), img.macros.end(), cur.begin())) {
            write_verified(cmd::SetMacros, cmd::GetMacros, 0, img.macros);
            say("macros restored");
        }
    }

    // Base info last: changing the position switches the profile.
    BaseInfo cur = base_info();
    if (!std::equal(img.base.begin() + 1, img.base.begin() + 8, cur.raw.begin() + 1)) {
        count_write();
        session_.transact(cmd::SetBase, 1, 7, std::span<const std::uint8_t>(img.base.data() + 1, 7));
        say("profile order / rotation restored");
    }
    if (img.base[0] != base_info().position()) {
        count_write();
        session_.transact(cmd::SetBase, 0, 1, std::span<const std::uint8_t>(img.base.data(), 1));
        say("active profile restored");
    }
    return static_cast<int>(writes_ - before);
}

// --- profiles ---------------------------------------------------------------------------------------

void Keyboard::copy_profile(int src, int dst, const Log& log, const WriteOptions& opt)
{
    check_profile(src);
    check_profile(dst);
    if (src == dst)
        throw std::invalid_argument("source and destination profile are the same");
    ProfileImage pi = read_profile(src);
    // Base layer: same mode (Windows / Mac) inside the destination profile.
    const int within = (pi.funcconfig[1] & 0xFE) - model_.layers * src;
    pi.funcconfig[1] = static_cast<std::uint8_t>(model_.layers * dst + (within >= 0 && within < model_.layers ? within : 0));
    // The report rate is global: keep the destination's.
    const std::uint8_t dst_rate = funcconfig(dst).report_rate();
    pi.funcconfig[4] = static_cast<std::uint8_t>((pi.funcconfig[4] & 0xF0) | dst_rate);
    // Fn keys carry absolute layer numbers: move them into the destination profile.
    for (auto& km : pi.keymaps)
        for (std::size_t i = 0; i + 2 < km.size(); i += 3)
            if (km[i] == KeyCode::Special && km[i + 1] == 0xFF) {
                const int layer = km[i + 2] - model_.layers * src;
                if (layer >= 0 && layer < model_.layers)
                    km[i + 2] = static_cast<std::uint8_t>(model_.layers * dst + layer);
            }
    WriteOptions o = opt;
    o.allow_reboot = false;
    write_profile(dst, pi, log, o);
}

void Keyboard::set_rotation(const std::vector<int>& profiles, const Log& log)
{
    if (profiles.empty() || static_cast<int>(profiles.size()) > model_.profiles)
        throw std::invalid_argument("rotation needs 1-" + std::to_string(model_.profiles) + " profiles");
    std::vector<int> order;
    for (int p : profiles) {
        check_profile(p);
        if (std::find(order.begin(), order.end(), p) != order.end())
            throw std::invalid_argument("profile " + std::to_string(p) + " listed twice");
        order.push_back(p);
    }
    for (int p = 0; p < model_.profiles; ++p)
        if (std::find(order.begin(), order.end(), p) == order.end())
            order.push_back(p);
    BaseInfo b = base_info();
    if (!b.marker_ok())
        throw SafetyError("base info has no AA BB marker; not touching it");
    const int active = b.runtime_profile();
    auto it = std::find(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(profiles.size()), active);
    std::uint8_t position = 0;
    if (it != order.begin() + static_cast<std::ptrdiff_t>(profiles.size())) {
        position = static_cast<std::uint8_t>(it - order.begin());
    } else if (log) {
        log("the active profile " + std::to_string(active) + " is not in the new rotation: switching to profile " +
            std::to_string(order[0]));
    }
    std::array<std::uint8_t, 6> raw{position, static_cast<std::uint8_t>(profiles.size())};
    for (std::size_t i = 0; i < 4; ++i)
        raw[2 + i] = static_cast<std::uint8_t>(order[i]);
    if (std::equal(raw.begin(), raw.end(), b.raw.begin()))
        return;
    count_write();
    session_.transact(cmd::SetBase, 0, 6, raw);
    if (!std::equal(raw.begin(), raw.end(), base_info().raw.begin()))
        throw ProtocolError("base info read-back mismatch");
}

}  // namespace mchose::glw
