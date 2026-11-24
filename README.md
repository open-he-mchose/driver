# driver

*AI-generated (Claude, Anthropic) together with the maintainer; tested on one Jet75-II (firmware V116) only.
Early development: expect bugs, keep backups. Every settings write goes to the keyboard's flash.*

Open driver for MCHOSE magnetic (Hall-effect) keyboards: a C++ library and the `mchosectl` command-line tool
for **Linux and Windows**. It talks to the keyboard's own configuration interface with the protocol documented
at <https://open-he-mchose.github.io/>; the firmware is not modified.

| Directory | Content | Dependencies |
|---|---|---|
| `core/` | protocol: packets, checksums, structures, key codes, macros, profiles, backup / restore | none (C++20) |
| `hid/` | transport through [hidapi](https://github.com/libusb/hidapi) | hidapi |
| `cli/` | `mchosectl` | core, hid |
| `tests/` | unit tests with a simulated keyboard; golden values from the documentation | core |
| `script/` | Lua 5.4 lighting effects: sandboxed scripts, key presses from the OS, live colours | core, Lua (downloaded) |
| `gui/` | `mchose-gui`, Qt 6 / Qt Quick configurator (GPL-3.0) | core, hid, script, Qt 6.5+ |

`gui/` holds the Qt 6 configurator (`mchose-gui`, GPL-3.0, early): visual keyboard, actuation and rapid trigger,
remapping, lighting, live key depth, backup / restore. Build it with `-DMCHOSE_BUILD_GUI=ON` (needs Qt 6.5+ with
Qt Quick and Quick Controls 2). It uses the system look: the platform's default
Qt Quick Controls style and the system palette, no colours of its own. `core`, `hid` and `cli` stay MIT.

## Supported keyboards

| Model | Status |
|---|---|
| Jet75-II (`41E4:211A`) | supported, tested |
| other `glw` models | planned: model data from the official driver's device table ([devices.json](https://github.com/open-he-mchose/docs/blob/main/data/devices.json)) once their layouts are documented |

## Download

Prebuilt `mchosectl` for Linux (x86_64) and Windows (x64): [Releases](https://github.com/open-he-mchose/driver/releases).
No installation needed. On Linux, install the udev rule from the archive (see `INSTALL.txt`) or
[linux-fixes](https://github.com/open-he-mchose/linux-fixes). Changes: [CHANGELOG.md](CHANGELOG.md).

## mchosectl

```text
mchosectl list | info                 connected keyboards; firmware, profiles, report rate, lighting
mchosectl keymap [--layer L]          key mapping (also --factory)
mchosectl remap CapsLock Esc          remap a key (names, Ctrl+Shift+T, Mute, none, raw hex; #N = matrix index)
mchosectl trigger                     actuation / rapid trigger of every key
mchosectl trigger set W A S D --actuation 1.2 --rt on --press 0.15 --release 0.15
mchosectl profile [N]                 show the rotation / switch profile
mchosectl profile rotation 0 1 2      profiles reachable with the next-profile key
mchosectl profile copy 0 3            copy a profile (Fn layer numbers adjusted)
mchosectl rate 8000                   report rate 1000/2000/4000/8000 (global; the keyboard reboots)
mchosectl light set --effect static --brightness 60
mchosectl light color W A S D ff0000  stored per-key colours; light effects lists the effects
mchosectl light live rainbow          live colours (RAM only)
mchosectl light script script/examples/typing_ripple.lua   Lua lighting effect (see below)
mchosectl macro set 1 '"Hello!" Enter'          macros: +Key/-Key, Ctrl+C taps, "typed text", 50ms delays
mchosectl macro bind F10 1 --mode once          once / hold / toggle / COUNT
mchosectl macros                      stored macros and the keys bound to them
mchosectl adv socd A D --mode last-input        Snap Tap; also rs, lt
mchosectl adv mt CapsLock --tap Esc --hold LCtrl --time 200
mchosectl adv toggle F12 --key LShift
mchosectl adv dks 1 --action W:1,0,0,0 --action E:0,1,0,0 --action R:0,0,1,0 --action T:0,0,0,1
mchosectl adv [show|doctor] | adv clear KEY
mchosectl backup [FILE] | restore FILE          everything except calibration; restore writes only differences
mchosectl depth                       live key depth (Ctrl+C to stop)
```

Close the official web driver while using it: two programs on the same interface take each other's replies.

### Safety

- **A full backup is saved before the first write** of every run (`~/.local/share/mchose/backups/` on Linux,
  `%LOCALAPPDATA%\mchose\backups\` on Windows). Disable with `--no-backup`.
- Every write is **read back and compared**.
- A **write limit** per run protects the flash against accidental loops.
- Refused unless explicitly allowed:
  - changing the report rate of the active profile — the keyboard reboots (`--allow-reboot`);
  - binding the codes that reset the keyboard when held (factory reset, calibration reset) or reboot it into the
    bootloader (`--allow-dangerous`);
  - activating a profile outside the rotation (`--allow-outside-rotation`).
- Restore never touches calibration.

## Lighting effects in Lua

Effects run on the computer and are shown with live colours (no flash writes; the keyboard returns to its own
effect a few seconds after they stop). A script defines `frame(ctx)`, called 25 times per second:

```lua
params = { speed = { min = 0.1, max = 5, default = 1, label = "Speed" } }

function frame(ctx)                      -- ctx.t, ctx.dt, ctx.keys, ctx.pressed, ctx.released, ctx.held, ctx.param
  for _, key in ipairs(ctx.keys) do      -- key.index, key.name, key.x, key.y, key.w (key units)
    ctx.set(key.index, hsv(key.x * 30 - ctx.t * 90 * ctx.param.speed, 1, 0.7))
  end
end
```

Helpers: `hsv(h, s, v)`, `mix(a, b, t)`, `clamp(x, lo, hi)`, `ctx.fill(r, g, b)`, `ctx.get(index)`, `print(...)`.
Key presses come from the operating system (Linux: the keyboard's input devices, readable for members of the
`input` group; Windows: the system key state), because the keyboard's depth stream pauses while colours are sent.
Scripts run in a sandbox (no files, no OS access) with instruction and memory limits. Examples in
[`script/examples`](script/examples): rainbow wave, typing ripple, heat map, fire, rain, Game of Life. In the GUI:
the Effects tab (editor, parameters as sliders, live preview on the on-screen keyboard).

## Build

Requirements: CMake ≥ 3.21, a C++20 compiler (GCC 11+, Clang 14+, MSVC 2022), hidapi. If hidapi is not
installed, CMake downloads and builds it (disable with `-DMCHOSE_FETCH_HIDAPI=OFF`).

```sh
# Debian / Ubuntu: sudo apt install cmake ninja-build pkg-config libhidapi-dev
# Fedora:          sudo dnf install cmake ninja-build hidapi-devel
# Arch:            sudo pacman -S cmake ninja hidapi
# Void:            sudo xbps-install cmake ninja pkg-config hidapi-devel
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/cli/mchosectl info
```

Windows (Visual Studio 2022): `cmake -S . -B build` then `cmake --build build --config Release`. No driver
installation is needed; the configuration interface is a standard HID device.

On Linux the user needs access to the keyboard's hidraw nodes: install the udev rule from
[linux-fixes](https://github.com/open-he-mchose/linux-fixes) (which also fixes Snap Tap, macros and >6 keys).
CI builds for Linux and Windows are attached to every run of the `build` workflow.

## Status and plans

Done and tested on a Jet75-II: info, profiles (switch, rotation, copy), report rate, lighting (effects,
stored and live colours), trigger settings, keymap, macros, advanced keys (DKS, Toggle, Mod-Tap, RS, SOCD, LT,
doctor), backup / restore, depth stream, safety guards; unit tests and Linux / Windows CI.

Next: releases, precise depth from travel curves, more models, then the Qt GUI.

## License

MIT (`core/`, `hid/`, `cli/`, `tests/`), see `LICENSE`. Part of
[open-he-mchose](https://github.com/open-he-mchose); not affiliated with MCHOSE.
