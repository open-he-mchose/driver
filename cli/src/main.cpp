// SPDX-License-Identifier: MIT
// mchosectl — command-line tool for MCHOSE magnetic keyboards (glw protocol).
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <chrono>
#include <sstream>
#include <string>
#include <vector>

#include "json.hpp"
#include "mchose/glw/keyboard.hpp"
#include "mchose/glw/macro_text.hpp"
#include "mchose/hid/hidapi_transport.hpp"

using namespace mchose;
using namespace mchose::glw;
namespace fs = std::filesystem;

namespace {

const char* kUsage = R"(mchosectl — configure MCHOSE magnetic keyboards (unofficial; Jet75-II tested)

usage: mchosectl [--device PATH] COMMAND [options]

reading
  list                               connected keyboards
  info                               firmware, profiles, report rate, lighting
  keymap [--layer L] [--factory]     key mapping of a layer
  trigger                            actuation and rapid-trigger settings of every key
  macros                             stored macros and the keys bound to them

writing (every write is saved to the keyboard's flash; a full backup is made first)
  profile N                          switch the active profile
  remap KEY CODE [--layer L]         map KEY (name like CapsLock or matrix index) to CODE
                                     (Esc, Ctrl+Shift+T, Mute, none, or raw hex "10 00 29")
  trigger set KEY... [--actuation MM] [--press MM] [--release MM] [--rt off|on|continuous]
                                     KEY = name, index or "all"
  light set [--effect N] [--brightness 0-100] [--speed 0-4] [--color RRGGBB]
  macro set N "TEXT" [--delay MS]    store macro N (1-16), e.g. "Ctrl+C", "+LShift +A 50ms -A -LShift",
                                     "\"text to type\"", "wait 200ms"; delays are after the previous action
  macro clear N|all
  macro bind KEY N [--mode once|hold|toggle|COUNT] [--layer L]
                                     hold = repeat while held, toggle = repeat until pressed again
  backup [FILE]                      save all profiles, keymaps, triggers, lighting, advanced keys, macros
  restore FILE                       write back what differs from FILE

live
  depth [--off]                      stream key depth until Ctrl+C (sets the FuncConfig debug bit;
                                     --off clears a bit left set)

options
  --profile P                        profile 0-3 (default: the active one)
  --no-backup                        do not save a backup before writing
  --allow-reboot                     allow changing the report rate of the active profile (keyboard reboots)
  --allow-dangerous                  allow binding factory-reset / calibration-reset / bootloader codes
  --allow-outside-rotation           allow activating a profile outside the rotation

Close the official web driver first: two programs on the same interface steal each other's replies.
)";

struct Args {
    std::string device;
    std::vector<std::string> pos;
    std::map<std::string, std::string> opt;   // --name value, or "1" for flags

    bool flag(const std::string& n) const { return opt.count(n) != 0; }
    std::string get(const std::string& n, const std::string& def = "") const
    {
        auto it = opt.find(n);
        return it == opt.end() ? def : it->second;
    }
};

Args parse_args(int argc, char** argv)
{
    static const char* flags[] = {"--factory", "--off", "--no-backup", "--allow-reboot", "--allow-dangerous",
                                  "--allow-outside-rotation", "--help", "-h"};
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        bool is_flag = false;
        for (const char* f : flags)
            if (s == f)
                is_flag = true;
        if (is_flag) {
            a.opt[s] = "1";
        } else if (s.rfind("--", 0) == 0) {
            if (i + 1 >= argc)
                throw std::runtime_error("option " + s + " needs a value");
            if (s == "--device")
                a.device = argv[++i];
            else
                a.opt[s] = argv[++i];
        } else {
            a.pos.push_back(s);
        }
    }
    return a;
}

int to_int(const std::string& s, const char* what)
{
    try {
        std::size_t n = 0;
        int v = std::stoi(s, &n, 0);
        if (n != s.size())
            throw std::invalid_argument(s);
        return v;
    } catch (const std::exception&) {
        throw std::runtime_error(std::string("bad ") + what + ": '" + s + "'");
    }
}

int mm_to_units(const std::string& s, const Model& m)
{
    double mm = std::stod(s);
    return static_cast<int>(std::lround(mm * 1000.0 / m.distance_unit_um));
}

std::string units_to_mm(int units, const Model& m)
{
    char buf[16];
    std::snprintf(buf, sizeof buf, "%.2f", units * m.distance_unit_um / 1000.0);
    return buf;
}

fs::path backup_dir()
{
#ifdef _WIN32
    const char* base = std::getenv("LOCALAPPDATA");
    fs::path p = base ? fs::path(base) : fs::current_path();
    return p / "mchose" / "backups";
#else
    if (const char* x = std::getenv("XDG_DATA_HOME"); x && *x)
        return fs::path(x) / "mchose" / "backups";
    const char* home = std::getenv("HOME");
    return (home ? fs::path(home) : fs::current_path()) / ".local" / "share" / "mchose" / "backups";
#endif
}

std::string timestamp(const char* fmt)
{
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof buf, fmt, std::localtime(&t));
    return buf;
}

json::Value image_to_json(const KeyboardImage& img)
{
    json::Array profiles;
    for (const auto& p : img.profiles) {
        json::Array keymaps;
        for (const auto& km : p.keymaps)
            keymaps.push_back(to_hex(km));
        profiles.push_back(json::Object{
            {"funcconfig", to_hex(p.funcconfig)}, {"triggers", to_hex(p.triggers)}, {"keymaps", keymaps},
            {"colors", to_hex(p.colors)}, {"dks", to_hex(p.dks)}, {"mt", to_hex(p.mt)}, {"tgl", to_hex(p.tgl)}});
    }
    return json::Object{{"format", "mchose-backup"}, {"version", 1}, {"model", img.model},
                        {"firmware", img.firmware}, {"created", timestamp("%Y-%m-%d %H:%M:%S")},
                        {"base", to_hex(img.base)}, {"macros", to_hex(img.macros)}, {"profiles", profiles}};
}

KeyboardImage image_from_json(const json::Value& v)
{
    if (v.at("format").str() != "mchose-backup" || v.at("version").num() != 1)
        throw std::runtime_error("not an mchose backup (version 1)");
    KeyboardImage img;
    img.model = v.at("model").str();
    img.firmware = v.at("firmware").str();
    auto base = from_hex(v.at("base").str());
    if (base.size() != img.base.size())
        throw std::runtime_error("backup: base info has the wrong size");
    std::copy(base.begin(), base.end(), img.base.begin());
    img.macros = from_hex(v.at("macros").str());
    for (const auto& pv : v.at("profiles").arr()) {
        ProfileImage p;
        auto fc = from_hex(pv.at("funcconfig").str());
        if (fc.size() != 64)
            throw std::runtime_error("backup: FuncConfig has the wrong size");
        std::copy(fc.begin(), fc.end(), p.funcconfig.begin());
        p.triggers = from_hex(pv.at("triggers").str());
        const auto& kms = pv.at("keymaps").arr();
        for (std::size_t l = 0; l < kms.size() && l < 4; ++l)
            p.keymaps[l] = from_hex(kms[l].str());
        p.colors = from_hex(pv.at("colors").str());
        p.dks = from_hex(pv.at("dks").str());
        p.mt = from_hex(pv.at("mt").str());
        p.tgl = from_hex(pv.at("tgl").str());
        img.profiles.push_back(std::move(p));
    }
    return img;
}

void save_backup(Keyboard& kb, const fs::path& file)
{
    auto img = kb.backup([](const std::string& s) { std::cerr << "\rreading " << s << "...        " << std::flush; });
    std::cerr << "\r";
    if (file.has_parent_path())
        fs::create_directories(file.parent_path());
    std::ofstream out(file);
    out << json::dump(image_to_json(img), 1) << "\n";
    if (!out)
        throw std::runtime_error("cannot write " + file.string());
    std::cerr << "backup saved: " << file.string() << "\n";
}

class Tool {
public:
    explicit Tool(const Args& a) : args_(a) {}

    Keyboard& kb()
    {
        if (!kb_) {
            auto devs = hid::enumerate();
            const hid::DeviceInfo* chosen = nullptr;
            for (const auto& d : devs)
                if (args_.device.empty() || d.path == args_.device)
                    if (!chosen)
                        chosen = &d;
            if (!chosen)
                throw std::runtime_error(args_.device.empty() ? "no MCHOSE keyboard found" : "device not found: " + args_.device);
            if (!chosen->model)
                throw std::runtime_error("keyboard " + to_pid(chosen->pid) + " is not supported yet (only the Jet75-II is)");
            transport_ = std::make_unique<hid::HidapiTransport>(chosen->path);
            kb_ = std::make_unique<Keyboard>(*transport_, *chosen->model);
        }
        return *kb_;
    }

    WriteOptions write_options() const
    {
        WriteOptions o;
        o.allow_reboot = args_.flag("--allow-reboot");
        o.allow_dangerous = args_.flag("--allow-dangerous");
        o.allow_outside_rotation = args_.flag("--allow-outside-rotation");
        return o;
    }

    int profile()
    {
        if (args_.opt.count("--profile"))
            return to_int(args_.get("--profile"), "profile");
        return kb().active_profile();
    }

    // Full backup before the first write of this run.
    void before_write()
    {
        if (backed_up_ || args_.flag("--no-backup"))
            return;
        save_backup(kb(), backup_dir() / ("backup-" + timestamp("%Y%m%d-%H%M%S") + ".json"));
        backed_up_ = true;
    }

    int key_index(const std::string& name)
    {
        try {
            int i = to_int(name, "key");
            if (i >= 0 && i < kb().model().key_slots)
                return i;
        } catch (const std::exception&) {
        }
        if (factory_.empty())
            factory_ = kb().factory_keymap(0);
        auto want = parse_keycode(name);
        for (int i = 0; i < kb().model().keys; ++i) {
            if (want && factory_[i] == *want)
                return i;
            std::string n = to_string(factory_[i]);
            if (n.size() == name.size() && std::equal(n.begin(), n.end(), name.begin(),
                                                      [](char a, char b) { return std::tolower(a) == std::tolower(b); }))
                return i;
        }
        throw std::runtime_error("unknown key '" + name + "' (use a key name such as CapsLock or a matrix index)");
    }

    std::string key_label(int index)
    {
        if (factory_.empty())
            factory_ = kb().factory_keymap(0);
        return to_string(factory_[index]);
    }

    static std::string to_pid(std::uint16_t pid)
    {
        char buf[16];
        std::snprintf(buf, sizeof buf, "41E4:%04X", pid);
        return buf;
    }

    const Args& args_;

private:
    std::unique_ptr<hid::HidapiTransport> transport_;
    std::unique_ptr<Keyboard> kb_;
    std::vector<KeyCode> factory_;
    bool backed_up_ = false;
};

int cmd_list()
{
    auto devs = hid::enumerate();
    if (devs.empty()) {
        std::cout << "no MCHOSE keyboard found\n";
        return 1;
    }
    for (const auto& d : devs)
        std::cout << Tool::to_pid(d.pid) << "  " << (d.model ? d.model->name : d.product + " (not supported yet)")
                  << "  " << d.path << "\n";
    return 0;
}

int cmd_info(Tool& t)
{
    auto& kb = t.kb();
    auto fi = kb.info();
    auto b = kb.base_info();
    const int active = b.runtime_profile();
    std::cout << "model      : " << kb.model().name << "\n";
    std::cout << "firmware   : " << fi.version_string() << " (" << fi.build << ")\n";
    std::cout << "profile    : " << active << " active; rotation";
    for (int i = 0; i < b.length() && i < 4; ++i)
        std::cout << " " << int(b.order(i));
    if (!b.in_rotation(active))
        std::cout << "  (active profile is outside the rotation)";
    std::cout << "\n";
    for (int p = 0; p < kb.model().profiles; ++p) {
        auto fc = kb.funcconfig(p);
        auto hz = report_rate_hz(fc.report_rate());
        std::cout << "profile " << p << (p == active ? "* " : "  ") << ": report rate "
                  << (hz ? std::to_string(*hz) + " Hz" : "value " + std::to_string(fc.report_rate()))
                  << ", " << (fc.mac_mode() ? "Mac" : "Windows") << " mode"
                  << (fc.win_lock() ? ", Win lock" : "") << ", light effect " << int(fc.light_effect())
                  << " brightness " << int(fc.light_brightness()) << " speed " << int(fc.light_speed())
                  << (fc.debug_mode() ? ", depth stream ON" : "") << (fc.marker_ok() ? "" : ", NO MARKER") << "\n";
    }
    std::cout << "note       : the report rate of the active profile applies to all profiles\n";
    return 0;
}

int cmd_profile(Tool& t)
{
    if (t.args_.pos.size() < 2) {
        std::cout << t.kb().active_profile() << "\n";
        return 0;
    }
    const int p = to_int(t.args_.pos[1], "profile");
    t.kb().switch_profile(p, t.write_options());   // base info only; no backup needed
    std::cout << "active profile: " << p << "\n";
    return 0;
}

int cmd_keymap(Tool& t)
{
    const int layer = to_int(t.args_.get("--layer", "0"), "layer");
    auto& kb = t.kb();
    std::vector<KeyCode> km;
    if (t.args_.flag("--factory")) {
        km = kb.factory_keymap(layer);
        std::cout << "factory keymap, layer " << layer << "\n";
    } else {
        const int p = t.profile();
        km = kb.keymap(p, layer);
        std::cout << "profile " << p << ", layer " << layer << "\n";
    }
    for (int i = 0; i < kb.model().keys; ++i)
        std::printf("%3d  %-12s  %s%s\n", i, t.key_label(i).c_str(), to_string(km[i]).c_str(),
                    km[i].is_dangerous() ? "   <- resets the keyboard when held" : "");
    return 0;
}

int cmd_remap(Tool& t)
{
    if (t.args_.pos.size() < 3)
        throw std::runtime_error("usage: remap KEY CODE [--layer L] [--profile P]");
    const int key = t.key_index(t.args_.pos[1]);
    auto code = parse_keycode(t.args_.pos[2]);
    if (!code)
        throw std::runtime_error("unknown key code '" + t.args_.pos[2] + "'");
    const int layer = to_int(t.args_.get("--layer", "0"), "layer");
    const int p = t.profile();
    t.before_write();
    t.kb().set_key(p, layer, key, *code, t.write_options());
    std::cout << "profile " << p << " layer " << layer << ": " << t.key_label(key) << " -> " << to_string(*code) << "\n";
    return 0;
}

int cmd_trigger(Tool& t)
{
    auto& kb = t.kb();
    const auto& m = kb.model();
    const int p = t.profile();
    if (t.args_.pos.size() >= 2 && t.args_.pos[1] == "set") {
        std::vector<int> keys;
        for (std::size_t i = 2; i < t.args_.pos.size(); ++i) {
            if (t.args_.pos[i] == "all")
                for (int k = 0; k < m.keys; ++k)
                    keys.push_back(k);
            else
                keys.push_back(t.key_index(t.args_.pos[i]));
        }
        if (keys.empty())
            throw std::runtime_error("trigger set: no keys given");
        auto entries = kb.triggers(p);
        t.before_write();
        int changed = 0;
        for (int k : keys) {
            TriggerEntry e = entries[k];
            if (t.args_.opt.count("--actuation")) e.set_actuation(mm_to_units(t.args_.get("--actuation"), m));
            if (t.args_.opt.count("--press")) e.set_rt_press(mm_to_units(t.args_.get("--press"), m));
            if (t.args_.opt.count("--release")) e.set_rt_release(mm_to_units(t.args_.get("--release"), m));
            if (t.args_.opt.count("--rt")) {
                const std::string rt = t.args_.get("--rt");
                if (rt == "off") e.set_mode(TriggerEntry::Mode::Normal);
                else if (rt == "on") e.set_mode(TriggerEntry::Mode::RapidTrigger);
                else if (rt == "continuous") e.set_mode(TriggerEntry::Mode::ContinuousRapidTrigger);
                else throw std::runtime_error("--rt must be off, on or continuous");
            }
            if (e.raw != entries[k].raw) {
                kb.set_trigger(p, k, e);
                ++changed;
            }
        }
        std::cout << "profile " << p << ": " << changed << " key(s) changed\n";
        return 0;
    }
    auto entries = kb.triggers(p);
    std::cout << "profile " << p << "   (mm)\n idx  key           actuation  mode           press  release  type\n";
    for (int k = 0; k < m.keys; ++k) {
        const auto& e = entries[k];
        std::printf("%4d  %-12s  %9s  %-13s  %5s  %7s  %4d%s\n", k, t.key_label(k).c_str(),
                    units_to_mm(e.actuation(), m).c_str(), to_string(e.mode()), units_to_mm(e.rt_press(), m).c_str(),
                    units_to_mm(e.rt_release(), m).c_str(), e.switch_type(), e.marker_ok() ? "" : "  (no marker)");
    }
    return 0;
}

int cmd_light(Tool& t)
{
    auto& kb = t.kb();
    const int p = t.profile();
    FuncConfig fc = kb.funcconfig(p);
    if (t.args_.pos.size() >= 2 && t.args_.pos[1] == "set") {
        if (t.args_.opt.count("--effect")) fc.set_light_effect(static_cast<std::uint8_t>(to_int(t.args_.get("--effect"), "effect")));
        if (t.args_.opt.count("--brightness")) fc.set_light_brightness(static_cast<std::uint8_t>(to_int(t.args_.get("--brightness"), "brightness")));
        if (t.args_.opt.count("--speed")) fc.set_light_speed(static_cast<std::uint8_t>(to_int(t.args_.get("--speed"), "speed")));
        if (t.args_.opt.count("--color")) {
            auto c = from_hex(t.args_.get("--color"));
            if (c.size() != 3)
                throw std::runtime_error("--color needs RRGGBB");
            fc.set_light_color(c[0], c[1], c[2]);
        }
        t.before_write();
        kb.set_funcconfig(p, fc, t.write_options());
    }
    auto c = fc.light_color();
    std::printf("profile %d: effect %d, brightness %d, speed %d, colour %02x%02x%02x\n", p, fc.light_effect(),
                fc.light_brightness(), fc.light_speed(), c[0], c[1], c[2]);
    return 0;
}

// Keys bound to macros in all layers of a profile: macro index -> "layer/key mode" list.
std::map<int, std::vector<std::string>> macro_bindings(Tool& t, int profile)
{
    std::map<int, std::vector<std::string>> out;
    for (int l = 0; l < t.kb().model().layers; ++l) {
        auto km = t.kb().keymap(profile, l);
        for (int k = 0; k < t.kb().model().keys; ++k)
            if (km[k].type == KeyCode::Macro)
                out[km[k].code1].push_back((l ? "Fn-layer " + std::to_string(l) + " " : std::string()) + t.key_label(k) +
                                           " (" + to_string(km[k]).substr(to_string(km[k]).find(' ') + 1) + ")");
    }
    return out;
}

int cmd_macros(Tool& t)
{
    auto set = t.kb().macros();
    const int p = t.profile();
    auto bound = macro_bindings(t, p);
    bool any = false;
    for (int i = 0; i < kMacroCount; ++i) {
        if (set[i].empty() && !bound.count(i))
            continue;
        any = true;
        std::cout << "M" << (i + 1) << ": " << (set[i].empty() ? "(empty)" : format_macro(set[i])) << "\n";
        if (bound.count(i)) {
            std::cout << "    bound in profile " << p << ":";
            for (const auto& b : bound[i])
                std::cout << " " << b;
            std::cout << "\n";
        }
    }
    if (!any)
        std::cout << "no macros stored\n";
    std::cout << "(macros are shared by all profiles; bindings belong to a profile)\n";
    return 0;
}

int macro_number(const std::string& s)
{
    const int n = to_int(s, "macro number");
    if (n < 1 || n > kMacroCount)
        throw std::runtime_error("macro number must be 1-" + std::to_string(kMacroCount));
    return n;
}

int cmd_macro(Tool& t)
{
    const auto& pos = t.args_.pos;
    if (pos.size() < 2)
        throw std::runtime_error("usage: macro set N TEXT | macro clear N|all | macro bind KEY N [--mode ...]");
    const std::string& sub = pos[1];
    auto& kb = t.kb();
    if (sub == "set") {
        if (pos.size() < 4)
            throw std::runtime_error("usage: macro set N \"TEXT\" [--delay MS]");
        const int n = macro_number(pos[2]);
        std::string text;
        for (std::size_t i = 3; i < pos.size(); ++i)
            text += (i > 3 ? " " : "") + pos[i];
        const int delay = to_int(t.args_.get("--delay", "10"), "delay");
        Macro m = parse_macro(text, static_cast<std::uint16_t>(delay));
        if (m.empty())
            throw std::runtime_error("macro is empty");
        auto set = kb.macros();
        set[n - 1] = m;
        t.before_write();
        kb.set_macros(set);
        std::cout << "M" << n << ": " << format_macro(kb.macros()[n - 1]) << "\n";
        if (!macro_bindings(t, t.profile()).count(n - 1))
            std::cout << "not bound to a key yet: mchosectl macro bind KEY " << n << " [--mode once|hold|toggle|N]\n";
        return 0;
    }
    if (sub == "clear") {
        if (pos.size() < 3)
            throw std::runtime_error("usage: macro clear N|all");
        auto set = kb.macros();
        if (pos[2] == "all")
            set = MacroSet{};
        else
            set[macro_number(pos[2]) - 1].clear();
        t.before_write();
        kb.set_macros(set);
        std::cout << "cleared (key bindings are kept; remap the keys to remove them)\n";
        return 0;
    }
    if (sub == "bind") {
        if (pos.size() < 4)
            throw std::runtime_error("usage: macro bind KEY N [--mode once|hold|toggle|COUNT] [--layer L]");
        const int key = t.key_index(pos[2]);
        const int n = macro_number(pos[3]);
        const std::string mode = t.args_.get("--mode", "once");
        std::uint8_t code2;
        if (mode == "once") code2 = 1;
        else if (mode == "hold") code2 = 0;
        else if (mode == "toggle") code2 = 0xFF;
        else {
            const int c = to_int(mode, "mode");
            if (c < 1 || c > 254)
                throw std::runtime_error("--mode COUNT must be 1-254");
            code2 = static_cast<std::uint8_t>(c);
        }
        const int layer = to_int(t.args_.get("--layer", "0"), "layer");
        const int p = t.profile();
        if (kb.macros()[n - 1].empty())
            std::cerr << "warning: M" << n << " is empty\n";
        t.before_write();
        KeyCode code{KeyCode::Macro, static_cast<std::uint8_t>(n - 1), code2};
        kb.set_key(p, layer, key, code, t.write_options());
        std::cout << "profile " << p << " layer " << layer << ": " << t.key_label(key) << " -> " << to_string(code) << "\n";
        std::cout << "note: on Linux macro keys need the NKRO fix (open-he-mchose/linux-fixes) to be typed\n";
        return 0;
    }
    throw std::runtime_error("unknown macro command '" + sub + "'");
}

int cmd_backup(Tool& t)
{
    fs::path file = t.args_.pos.size() >= 2 ? fs::path(t.args_.pos[1])
                                            : backup_dir() / ("backup-" + timestamp("%Y%m%d-%H%M%S") + ".json");
    save_backup(t.kb(), file);
    return 0;
}

int cmd_restore(Tool& t)
{
    if (t.args_.pos.size() < 2)
        throw std::runtime_error("usage: restore FILE");
    std::ifstream in(t.args_.pos[1]);
    if (!in)
        throw std::runtime_error("cannot read " + t.args_.pos[1]);
    std::stringstream ss;
    ss << in.rdbuf();
    auto img = image_from_json(json::parse(ss.str()));
    t.before_write();
    int n = t.kb().restore(img, [](const std::string& s) { std::cout << s << "\n"; }, t.write_options());
    std::cout << (n ? std::to_string(n) + " write(s)" : std::string("keyboard already matches the backup")) << "\n";
    return 0;
}

std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop = true; }

// Turns the depth stream off again on every way out of cmd_depth (Ctrl+C, errors).
struct DepthStreamGuard {
    Keyboard& kb;
    ~DepthStreamGuard()
    {
        try {
            kb.set_depth_stream(false);
            std::cerr << "\ndepth stream off\n";
        } catch (const std::exception& e) {
            std::cerr << "\ncould not turn the depth stream off: " << e.what()
                      << " (the keyboard clears it at the next power-up; or run: mchosectl depth --off)\n";
        }
    }
};

int cmd_depth(Tool& t)
{
    auto& kb = t.kb();
    if (t.args_.flag("--off")) {
        kb.set_depth_stream(false);
        std::cout << "depth stream off\n";
        return 0;
    }
    std::signal(SIGINT, on_signal);
#ifdef SIGTERM
    std::signal(SIGTERM, on_signal);
#endif
    kb.set_depth_stream(true);
    DepthStreamGuard guard{kb};
    std::cerr << "depth stream on (Ctrl+C to stop; the stream pauses ~0.6 s after any host command)\n";
    // Read reports directly: no requests are sent while streaming.
    while (!g_stop) {
        auto rep = kb.session().transport().read(std::chrono::milliseconds(200));
        if (!rep)
            continue;
        if (auto s = parse_depth_sample(*rep))
            std::printf("%-12s %5.2f mm  %s\n", to_string(s->key).c_str(), s->travel / 100.0,
                        s->pressed ? "pressed" : "");
        std::fflush(stdout);
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    try {
        Args args = parse_args(argc, argv);
        if (args.pos.empty() || args.flag("--help") || args.flag("-h")) {
            std::cout << kUsage;
            return args.pos.empty() && !args.flag("--help") && !args.flag("-h") ? 2 : 0;
        }
        Tool t(args);
        const std::string& c = args.pos[0];
        if (c == "list") return cmd_list();
        if (c == "info") return cmd_info(t);
        if (c == "profile") return cmd_profile(t);
        if (c == "keymap") return cmd_keymap(t);
        if (c == "remap") return cmd_remap(t);
        if (c == "trigger") return cmd_trigger(t);
        if (c == "light") return cmd_light(t);
        if (c == "macros") return cmd_macros(t);
        if (c == "macro") return cmd_macro(t);
        if (c == "backup") return cmd_backup(t);
        if (c == "restore") return cmd_restore(t);
        if (c == "depth") return cmd_depth(t);
        std::cerr << "unknown command '" << c << "'\n\n" << kUsage;
        return 2;
    } catch (const SafetyError& e) {
        std::cerr << "refused: " << e.what() << "\n";
        return 3;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
