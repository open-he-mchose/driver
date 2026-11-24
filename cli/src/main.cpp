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
#include <thread>
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

usage: mchosectl [--device PATH] COMMAND [options]      (mchosectl --version)

reading
  list                               connected keyboards
  info                               firmware, profiles, report rate, lighting
  keymap [--layer L] [--factory]     key mapping of a layer
  trigger                            actuation and rapid-trigger settings of every key
  macros                             stored macros and the keys bound to them

writing (every write is saved to the keyboard's flash; a full backup is made first)
  profile [N]                        show the rotation / switch the active profile
  profile rotation P...              profiles reachable with the next-profile key, in order (e.g. 0 1 2)
  profile copy SRC DST               copy settings, keymaps, colours and advanced keys
  rate [1000|2000|4000|8000]         show / set the report rate (global; the keyboard reboots)
  remap KEY CODE [--layer L]         map KEY (name like CapsLock, or #INDEX) to CODE
                                     (Esc, Ctrl+Shift+T, Mute, none, or raw hex "10 00 29")
  trigger set KEY... [--actuation MM] [--press MM] [--release MM] [--rt off|on|continuous]
                                     KEY = name, index or "all"
  light [set] [--effect NAME|N] [--brightness 0-100] [--speed 0-4] [--color RRGGBB]
  light effects                      effect names
  light color KEY... RRGGBB          stored per-key colours (shown by the static effect); KEY may be "all"
  light live RRGGBB|rainbow [--seconds N]   live colours, RAM only (no flash writes)
  macro set N "TEXT" [--delay MS]    store macro N (1-16), e.g. "Ctrl+C", "+LShift +A 50ms -A -LShift",
                                     "\"text to type\"", "wait 200ms"; delays are after the previous action
  macro clear N|all
  macro bind KEY N [--mode once|hold|toggle|COUNT] [--layer L]
                                     hold = repeat while held, toggle = repeat until pressed again
  adv [show|doctor]                  advanced keys of all profiles (doctor: find broken bindings)
  adv socd|rs|lt KEY1 KEY2 [--mode last-input|key1|key2|cancel]
  adv mt KEY --tap X --hold Y [--time MS]
  adv toggle KEY [--key X]
  adv dks KEY [--shallow 1.0] [--deep 3.0] --action CODE:ds,de,us,ue [--action ...]
                                     ds,de,us,ue = phases the action stays held when it starts at
                                     press-start / bottom / lift-from-bottom / full-lift (0-4,0-3,0-2,0-1)
  adv clear KEY [--layer L]          remove the function (and the partner of a pair)
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
    std::map<std::string, std::string> opt;   // --name value, or "1" for flags (last one wins)
    std::multimap<std::string, std::string> multi;   // every occurrence, for repeatable options

    bool flag(const std::string& n) const { return opt.count(n) != 0; }
    std::vector<std::string> all(const std::string& n) const
    {
        std::vector<std::string> v;
        auto r = multi.equal_range(n);
        for (auto it = r.first; it != r.second; ++it)
            v.push_back(it->second);
        return v;
    }
    std::string get(const std::string& n, const std::string& def = "") const
    {
        auto it = opt.find(n);
        return it == opt.end() ? def : it->second;
    }
};

Args parse_args(int argc, char** argv)
{
    static const char* flags[] = {"--factory", "--off", "--version", "--no-backup", "--allow-reboot", "--allow-dangerous",
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
            if (s == "--device") {
                a.device = argv[++i];
            } else {
                a.opt[s] = argv[i + 1];
                a.multi.emplace(s, argv[i + 1]);
                ++i;
            }
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

    // After a reboot of the keyboard: wait until it is back and open it again.
    void reconnect()
    {
        kb_.reset();
        transport_.reset();
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        for (int i = 0; i < 40; ++i) {
            try {
                kb().info();
                return;
            } catch (const std::exception&) {
                kb_.reset();
                transport_.reset();
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
        throw std::runtime_error("keyboard did not come back after the reboot");
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

    // KEY = a key name of the factory layer 0 ("CapsLock", "1", "F12"), or a matrix index written as "#N".
    // A plain number is an index only when no key has that name (digits are key names).
    int key_index(const std::string& name)
    {
        if (factory_.empty())
            factory_ = kb().factory_keymap(0);
        if (!name.empty() && name[0] == '#') {
            const int i = to_int(name.substr(1), "key index");
            if (i < 0 || i >= kb().model().keys)
                throw std::runtime_error("key index out of range: " + name);
            return i;
        }
        auto want = parse_keycode(name);
        for (int i = 0; i < kb().model().keys; ++i) {
            if (want && factory_[i] == *want)
                return i;
            std::string n = to_string(factory_[i]);
            if (n.size() == name.size() && std::equal(n.begin(), n.end(), name.begin(),
                                                      [](char a, char b) { return std::tolower(a) == std::tolower(b); }))
                return i;
        }
        try {
            const int i = to_int(name, "key");
            if (i >= 0 && i < kb().model().keys)
                return i;
        } catch (const std::exception&) {
        }
        throw std::runtime_error("unknown key '" + name + "' (use a key name such as CapsLock, or #INDEX)");
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

std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop = true; }

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
    const auto& pos = t.args_.pos;
    auto& kb = t.kb();
    auto log = [](const std::string& m) { std::cout << m << "\n"; };
    if (pos.size() < 2) {
        auto b = kb.base_info();
        std::cout << "active profile " << b.runtime_profile() << "; rotation (next-profile key):";
        for (int i = 0; i < b.length() && i < 4; ++i)
            std::cout << " " << int(b.order(i));
        std::cout << "\n";
        return 0;
    }
    if (pos[1] == "rotation") {
        std::vector<int> list;
        for (std::size_t i = 2; i < pos.size(); ++i)
            list.push_back(to_int(pos[i], "profile"));
        kb.set_rotation(list, log);   // base info only; no backup needed
        auto b = kb.base_info();
        std::cout << "rotation:";
        for (int i = 0; i < b.length(); ++i)
            std::cout << " " << int(b.order(i));
        std::cout << "; active profile " << b.runtime_profile() << "\n";
        return 0;
    }
    if (pos[1] == "copy") {
        if (pos.size() != 4)
            throw std::runtime_error("usage: profile copy SRC DST");
        const int src = to_int(pos[2], "profile"), dst = to_int(pos[3], "profile");
        t.before_write();
        kb.copy_profile(src, dst, log, t.write_options());
        std::cout << "profile " << src << " copied to " << dst << "\n";
        return 0;
    }
    const int p = to_int(pos[1], "profile");
    kb.switch_profile(p, t.write_options());   // base info only; no backup needed
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

const char* kEffects[] = {"static", "aurora-mapping", "shadow-steps", "tranquility", "pulsation", "flower-dance",
                          "tidal-rhythm", "surging-tide", "colorful-fountain", "galaxy-roaming", "spiral", "surge-rhythm",
                          "ocean", "ripple-dance", "ripple", "instant-flash", "matrix-shadows", "melody-piano", "flow-dance",
                          "raining", "stars", "fireworks", "ripple-flow"};   // ids 0-22 offered for the Jet75-II

std::string effect_name(int id)
{
    if (id >= 0 && id < static_cast<int>(std::size(kEffects)))
        return kEffects[id];
    return id == 255 ? "off" : "effect " + std::to_string(id);
}

std::uint8_t parse_effect(const std::string& s)
{
    if (s == "off")
        return 255;
    for (std::size_t i = 0; i < std::size(kEffects); ++i)
        if (s == kEffects[i])
            return static_cast<std::uint8_t>(i);
    const int v = to_int(s, "effect");
    if (v < 0 || v > 255)
        throw std::runtime_error("effect out of range");
    return static_cast<std::uint8_t>(v);
}

std::array<std::uint8_t, 3> parse_rgb(const std::string& s)
{
    auto c = from_hex(s.size() == 7 && s[0] == '#' ? s.substr(1) : s);
    if (c.size() != 3)
        throw std::runtime_error("colour must be RRGGBB, got '" + s + "'");
    return {c[0], c[1], c[2]};
}

std::array<std::uint8_t, 3> hsv(double h, double v)
{
    const double c = v, x = c * (1 - std::fabs(std::fmod(h / 60.0, 2) - 1));
    double r = 0, g = 0, b = 0;
    if (h < 60) { r = c; g = x; } else if (h < 120) { r = x; g = c; } else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; } else if (h < 300) { r = x; b = c; } else { r = c; b = x; }
    return {static_cast<std::uint8_t>(r * 255), static_cast<std::uint8_t>(g * 255), static_cast<std::uint8_t>(b * 255)};
}

int cmd_light(Tool& t)
{
    auto& kb = t.kb();
    const auto& pos = t.args_.pos;
    const std::string sub = pos.size() >= 2 ? pos[1] : "show";
    if (sub == "effects") {
        for (std::size_t i = 0; i < std::size(kEffects); ++i)
            std::printf("%3zu  %s\n", i, kEffects[i]);
        std::printf("255  off\n");
        return 0;
    }
    const int p = t.profile();
    if (sub == "color" || sub == "colour") {
        if (pos.size() < 4)
            throw std::runtime_error("usage: light color KEY... RRGGBB   (KEY = name, #INDEX or all)");
        const auto rgb = parse_rgb(pos.back());
        auto colors = kb.colors(p);
        for (std::size_t i = 2; i + 1 < pos.size(); ++i) {
            if (pos[i] == "all")
                for (int k = 0; k < kb.model().keys; ++k)
                    colors[static_cast<std::size_t>(k)] = rgb;
            else
                colors[static_cast<std::size_t>(t.key_index(pos[i]))] = rgb;
        }
        t.before_write();
        kb.set_colors(p, colors);
        auto fc = kb.funcconfig(p);
        std::cout << "stored colours written to profile " << p << "\n";
        if (fc.light_effect() != 0)
            std::cout << "note: stored colours show with the static effect: mchosectl light set --effect static\n";
        return 0;
    }
    if (sub == "live") {
        if (pos.size() < 3)
            throw std::runtime_error("usage: light live RRGGBB|rainbow [--seconds N]   (RAM only, no flash)");
        const double seconds = std::stod(t.args_.get("--seconds", "10"));
        std::signal(SIGINT, on_signal);
        const int n = kb.model().keys;
        const auto start = std::chrono::steady_clock::now();
        std::vector<std::array<std::uint8_t, 3>> frame(static_cast<std::size_t>(n));
        while (!g_stop) {
            const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            if (el >= seconds)
                break;
            for (int k = 0; k < n; ++k)
                frame[static_cast<std::size_t>(k)] = pos[2] == "rainbow" ? hsv(std::fmod(el * 90 + k * 9, 360), 1.0) : parse_rgb(pos[2]);
            kb.live_colors(frame);
            std::this_thread::sleep_for(std::chrono::milliseconds(40));   // ~25 frames per second
        }
        std::cout << "live colours stopped; the keyboard returns to its effect in a few seconds\n";
        return 0;
    }
    FuncConfig fc = kb.funcconfig(p);
    if (sub == "set") {
        if (t.args_.opt.count("--effect")) fc.set_light_effect(parse_effect(t.args_.get("--effect")));
        if (t.args_.opt.count("--brightness")) fc.set_light_brightness(static_cast<std::uint8_t>(to_int(t.args_.get("--brightness"), "brightness")));
        if (t.args_.opt.count("--speed")) fc.set_light_speed(static_cast<std::uint8_t>(to_int(t.args_.get("--speed"), "speed")));
        if (t.args_.opt.count("--color")) {
            auto c = parse_rgb(t.args_.get("--color"));
            fc.set_light_color(c[0], c[1], c[2]);
        }
        t.before_write();
        kb.set_funcconfig(p, fc, t.write_options());
    }
    auto c = fc.light_color();
    std::printf("profile %d: effect %s (%d), brightness %d, speed %d, colour %02x%02x%02x\n", p,
                effect_name(fc.light_effect()).c_str(), fc.light_effect(), fc.light_brightness(), fc.light_speed(), c[0], c[1], c[2]);
    return 0;
}

int cmd_rate(Tool& t)
{
    auto& kb = t.kb();
    const int p = kb.active_profile();
    auto show = [](std::uint8_t v) { auto hz = report_rate_hz(v); return hz ? std::to_string(*hz) + " Hz" : "value " + std::to_string(v); };
    const std::uint8_t cur = kb.funcconfig(p).report_rate();
    if (t.args_.pos.size() < 2) {
        std::cout << "report rate: " << show(cur) << " (global: profile switches keep it)\n";
        return 0;
    }
    const int hz = to_int(t.args_.pos[1], "rate");
    std::uint8_t value = 0;
    for (std::uint8_t v = 1; v <= 4; ++v)
        if (report_rate_hz(v) == hz)
            value = v;
    if (!value)
        throw std::runtime_error("rate must be 1000, 2000, 4000 or 8000");
    if (value == cur) {
        std::cout << "report rate already " << show(cur) << "\n";
        return 0;
    }
    t.before_write();
    std::cout << "setting " << hz << " Hz; the keyboard reboots...\n";
    kb.set_report_rate_and_reboot(value);
    t.reconnect();
    const std::uint8_t now = t.kb().funcconfig(t.kb().active_profile()).report_rate();
    std::cout << "report rate now: " << show(now) << "\n";
    return now == value ? 0 : 1;
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

std::string describe_adv(Tool& t, const AdvBinding& b, const AdvTables& tables, const std::vector<TriggerEntry>& trig)
{
    char head[96];
    std::snprintf(head, sizeof head, "layer %d  %-12s %-6s slot %2d  ", b.layer, t.key_label(b.key).c_str(), to_string(b.kind), b.slot);
    std::string s = head;
    if (b.slot >= kAdvSlots)
        return s + "(slot out of range!)";
    auto code_at = [](const std::vector<std::uint8_t>& v, std::size_t off) { return KeyCode{v[off], v[off + 1], v[off + 2]}; };
    const auto slot = static_cast<std::size_t>(b.slot);
    switch (b.kind) {
    case AdvKind::ModTap:
        return s + "tap " + to_string(code_at(tables.mt, 6 * slot)) + ", hold " + to_string(code_at(tables.mt, 6 * slot + 3)) +
               " after " + std::to_string(b.code2 * 10) + " ms";
    case AdvKind::RapidStrike:
    case AdvKind::Socd:
    case AdvKind::Lt: {
        s += "sends " + to_string(code_at(tables.mt, 6 * slot)) + ", partner " +
             (b.code2 < t.kb().model().keys ? t.key_label(b.code2) : std::string("?")) + " (#" + std::to_string(b.code2) + ")";
        if (b.kind == AdvKind::Socd)
            s += ", " + std::string(to_string(static_cast<SocdMode>(trig[static_cast<std::size_t>(b.key)].priority() & 3)));
        return s;
    }
    case AdvKind::Toggle:
        return s + "toggles " + to_string(code_at(tables.tgl, 3 * slot));
    case AdvKind::Dks: {
        auto d = DksEntry::decode(std::span<const std::uint8_t>(tables.dks).subspan(24 * slot, 24));
        char pts[48];
        std::snprintf(pts, sizeof pts, "points %.1f / %.1f mm:", d.shallow / 10.0, d.deep / 10.0);
        s += pts;
        for (const auto& a : d.actions)
            s += " " + to_string(a.code) + " " + std::to_string(a.spans.down_start) + "," + std::to_string(a.spans.down_end) +
                 "," + std::to_string(a.spans.up_start) + "," + std::to_string(a.spans.up_end) + ";";
        return s;
    }
    }
    return s;
}

KeyCode parse_target(const std::string& s)
{
    auto k = parse_keycode(s);
    if (!k)
        throw std::runtime_error("unknown key code '" + s + "'");
    return *k;
}

int cmd_adv(Tool& t)
{
    const auto& pos = t.args_.pos;
    auto& kb = t.kb();
    const std::string sub = pos.size() >= 2 ? pos[1] : "show";
    if (sub == "show" || sub == "doctor") {
        std::vector<int> profiles;
        if (t.args_.opt.count("--profile"))
            profiles.push_back(t.profile());
        else
            for (int p = 0; p < kb.model().profiles; ++p)
                profiles.push_back(p);
        const int active = kb.active_profile();
        int total = 0;
        for (int p : profiles) {
            auto bindings = kb.adv_bindings(p);
            auto tables = kb.adv_tables(p);
            auto trig = kb.triggers(p);
            std::cout << "profile " << p << (p == active ? " (active)" : "") << ": " << bindings.size()
                      << " advanced binding(s)\n";
            for (const auto& b : bindings)
                std::cout << "  " << describe_adv(t, b, tables, trig) << "\n";
            if (sub == "doctor") {
                std::vector<std::uint8_t> prio;
                for (const auto& e : trig)
                    prio.push_back(e.priority());
                auto problems = adv_problems(bindings, tables, prio);
                total += static_cast<int>(problems.size());
                for (const auto& x : problems)
                    std::cout << "  ! " << x << "\n";
                auto orphans = adv_orphans(bindings, tables);
                if (!orphans.empty()) {
                    std::cout << "  unreferenced table entries (harmless leftovers):";
                    for (const auto& o : orphans)
                        std::cout << " " << o;
                    std::cout << "\n";
                }
            }
        }
        if (sub == "doctor")
            std::cout << (total ? std::to_string(total) + " problem(s)" : std::string("no problems found")) << "\n";
        return 0;
    }
    const int p = t.profile();
    const int layer = to_int(t.args_.get("--layer", "0"), "layer");
    std::vector<int> keys;
    for (std::size_t i = 2; i < pos.size(); ++i)
        keys.push_back(t.key_index(pos[i]));
    auto need = [&](std::size_t n, const char* usage) {
        if (keys.size() != n)
            throw std::runtime_error(std::string("usage: ") + usage);
    };
    if (sub == "clear") {
        need(1, "adv clear KEY [--layer L]");
        t.before_write();
        auto cleared = kb.clear_adv(p, layer, keys[0]);
        if (cleared.empty())
            std::cout << "no advanced function on that key in layer " << layer << "\n";
        for (int k : cleared)
            std::cout << "cleared " << t.key_label(k) << "\n";
        return 0;
    }
    if (sub == "socd" || sub == "rs" || sub == "lt") {
        need(2, "adv socd|rs|lt KEY1 KEY2 [--mode last-input|key1|key2|cancel] [--layer L]");
        AdvKind kind = sub == "socd" ? AdvKind::Socd : sub == "rs" ? AdvKind::RapidStrike : AdvKind::Lt;
        SocdMode mode = SocdMode::LastInput;
        const std::string m = t.args_.get("--mode", "last-input");
        if (m == "key1" || m == "key1-priority") mode = SocdMode::Key1Priority;
        else if (m == "key2" || m == "key2-priority") mode = SocdMode::Key2Priority;
        else if (m == "cancel") mode = SocdMode::Cancel;
        else if (m != "last-input" && m != "last") throw std::runtime_error("--mode: last-input, key1, key2 or cancel");
        t.before_write();
        kb.bind_pair(p, layer, kind, keys[0], keys[1], mode);
        if (kind == AdvKind::Socd)
            std::cout << "note: SOCD output needs the NKRO fix on Linux and does not work on Windows (firmware bug)\n";
    } else if (sub == "mt") {
        need(1, "adv mt KEY --tap X --hold Y [--time MS] [--layer L]");
        if (!t.args_.opt.count("--tap") || !t.args_.opt.count("--hold"))
            throw std::runtime_error("adv mt needs --tap and --hold");
        t.before_write();
        kb.bind_mod_tap(p, layer, keys[0], parse_target(t.args_.get("--tap")), parse_target(t.args_.get("--hold")),
                        to_int(t.args_.get("--time", "200"), "time"));
    } else if (sub == "toggle" || sub == "tgl") {
        need(1, "adv toggle KEY [--key X] [--layer L]");
        std::optional<KeyCode> code;
        if (t.args_.opt.count("--key"))
            code = parse_target(t.args_.get("--key"));
        t.before_write();
        kb.bind_toggle(p, layer, keys[0], code);
    } else if (sub == "dks") {
        need(1, "adv dks KEY [--shallow MM] [--deep MM] --action CODE:ds,de,us,ue [--action ...]");
        DksEntry d;
        d.shallow = static_cast<int>(std::lround(std::stod(t.args_.get("--shallow", "1.0")) * 10));
        d.deep = static_cast<int>(std::lround(std::stod(t.args_.get("--deep", "3.0")) * 10));
        for (const auto& spec : t.args_.all("--action")) {
            const auto colon = spec.rfind(':');
            if (colon == std::string::npos)
                throw std::runtime_error("--action needs CODE:ds,de,us,ue, e.g. W:1,0,0,0");
            DksSpans sp;
            if (std::sscanf(spec.c_str() + colon + 1, "%d,%d,%d,%d", &sp.down_start, &sp.down_end, &sp.up_start, &sp.up_end) != 4 ||
                sp.down_start < 0 || sp.down_start > 4 || sp.down_end < 0 || sp.down_end > 3 || sp.up_start < 0 ||
                sp.up_start > 2 || sp.up_end < 0 || sp.up_end > 1)
                throw std::runtime_error("DKS spans ds,de,us,ue must be within 0-4,0-3,0-2,0-1");
            if (!(dks_unmask(dks_mask(sp)) == sp))
                std::cerr << "warning: spans of " << spec << " are normalised by the keyboard format (same as the official driver)\n";
            d.actions.push_back({parse_target(spec.substr(0, colon)), sp});
        }
        if (d.actions.empty())
            throw std::runtime_error("adv dks needs at least one --action");
        t.before_write();
        kb.bind_dks(p, layer, keys[0], d);
    } else {
        throw std::runtime_error("unknown adv command '" + sub + "'");
    }
    auto bindings = kb.adv_bindings(p);
    auto tables = kb.adv_tables(p);
    auto trig = kb.triggers(p);
    for (const auto& b : bindings)
        if (b.layer == layer && std::find(keys.begin(), keys.end(), b.key) != keys.end())
            std::cout << "set: " << describe_adv(t, b, tables, trig) << "\n";
    return 0;
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
        if (args.flag("--version") || (!args.pos.empty() && args.pos[0] == "version")) {
            std::cout << "mchosectl " << MCHOSE_VERSION << "\n";
            return 0;
        }
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
        if (c == "adv") return cmd_adv(t);
        if (c == "rate") return cmd_rate(t);
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
