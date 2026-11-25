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
| `gamepad/` | analog keys as a virtual Xbox 360 controller (Linux uinput, Windows ViGEmBus) | ViGEmClient on Windows (downloaded) |
| `gui/` | `mchose-gui`, Qt 6 / Qt Quick configurator (GPL-3.0) | core, hid, script, gamepad, Qt 6.5+ |

`gui/` holds the Qt 6 configurator (`mchose-gui`, GPL-3.0): visual keyboard, actuation and rapid trigger, keymap
of all layers, macros, advanced keys, lighting and per-key colours, Lua effects, virtual gamepad, profiles and
settings, live key depth with a depth graph, backup / restore; English and Russian (follows the system language,
`MCHOSE_LANG=en` overrides it). Packages: Windows zip and Linux AppImage on the releases page; or build it with
`-DMCHOSE_BUILD_GUI=ON` (needs Qt 6.5+ with Qt Quick and Quick Controls 2; Qt LinguistTools for the translation). It uses the system look: the platform's default
Qt Quick Controls style and the system palette, no colours of its own. `core`, `hid` and `cli` stay MIT.

One program runs per user: closing the window keeps it running in the background (tray icon where the desktop has
one: Windows, KDE, XFCE, GNOME with the AppIndicator extension), and starting it again shows the window. The running
Lua effect or gamepad, the script parameters and the editor texts are saved and started again when the keyboard
connects. Device tab: turn off the background mode, start at login, quit. Command line: `--hidden` (start without a
window), `--quit` (stop the running program). While it runs, `mchosectl` cannot use the keyboard (device lock).

## Supported keyboards

| Model | Status |
|---|---|
| Jet75-II (`41E4:211A`) | supported, tested |
| other `glw` models | planned: model data from the official driver's device table ([devices.json](https://github.com/open-he-mchose/docs/blob/main/data/devices.json)) once their layouts are documented |

## Download

Prebuilt `mchosectl` and `mchose-gui` for Linux (x86_64, GUI as AppImage) and Windows (x64):
[Releases](https://github.com/open-he-mchose/driver/releases). No installation needed. On Linux, install the udev rule from the archive (see `INSTALL.txt`) or
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
mchosectl calibrate [KEY...]          switch calibration (see below); calibrate --reset --yes resets all keys
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

Parameter fields: `min`, `max`, `default`, `label`, optional `step` (whole steps; `0..1` with `step = 1` is a
switch in the GUI) and `order` (display order). Helpers: `hsv(h, s, v)`, `mix(a, b, t)`, `clamp(x, lo, hi)`, `ctx.fill(r, g, b)`, `ctx.get(index)`, `print(...)`.
Key presses come from the operating system (Linux: the keyboard's input devices, readable for members of the
`input` group; Windows: the system key state), because the keyboard's depth stream pauses while colours are sent.
Scripts run in a sandbox (no files, no OS access) with instruction and memory limits. Examples in
[`script/examples`](script/examples): rainbow wave, typing ripple, heat map, fire, rain, Game of Life. In the GUI:
the Effects tab (editor, parameters as sliders, live preview on the on-screen keyboard).

## Virtual gamepad

The keyboard has no gamepad mode, so the software reads the live depth stream and feeds a virtual Xbox 360
controller: key travel becomes stick axes and triggers, keys become buttons.

```text
mchosectl gamepad --presets                 wasd-left-stick, racing
mchosectl gamepad --preset racing --show    W = throttle, S = brake, A / D = steering
mchosectl gamepad --map my-game.txt
```

A mapping is plain text, one `KEY = TARGET` per line (key names of the factory layout, or `#INDEX`), plus the
travel settings:

```text
W = left-up          # targets: left-/right-up/down/left/right, lt, rt, a, b, x, y, lb, rb,
A = left-left        #          back, start, guide, l3, r3, dpad-up/down/left/right
Space = a
deadzone = 0.3       # mm ignored at the top of the stroke
full = 3.0           # mm for the full analog value
button = 1.2         # mm at which a button counts as pressed
```

In the GUI: the Gamepad tab (presets, map the selected keys, travel sliders, live sticks and triggers).

- **Linux:** uses `/dev/uinput`; install `packaging/linux/72-mchose-uinput.rules` if your user may not create
  input devices (many distributions already allow it, e.g. for Steam).
- **Windows:** needs the [ViGEmBus](https://github.com/nefarius/ViGEmBus/releases) driver installed.
- The mapped keys still type on the keyboard: disable them in a gaming profile (the Gamepad tab can do it).
- The gamepad and Lua effects cannot run together: the firmware pauses the depth stream for ~0.6 s after every
  packet from the computer, and live colours are sent constantly.

## Calibration

If a key's travel or actuation feels wrong, calibrate it: in the GUI, Device tab → **Calibrate…**, or
`mchosectl calibrate`. Press each key to calibrate fully down twice, holding it at the bottom for a moment; the
keyboard view shows `0/2` → `1/2` → `✓` (`!` = never calibrated). Keys you do not press keep their calibration.
Press firmly: a key held part-way can be stored with a wrong bottom value, and a key pressed but not finished keeps
a default value until it is calibrated again. The GUI ends the session with a mouse click and keeps key presses
away from its controls; `mchosectl calibrate` ends by itself once the pressed (or listed) keys are done and the
keyboard was idle for 8 s (`--idle S`). Lua effects and the gamepad pause during the session. Calibration is
stored in the keyboard and is not part of backups. "Reset all keys" (`0xB1`, like holding Fn+Backspace for 3 s)
asks first and starts a session right after. Protocol: [calibration](https://open-he-mchose.github.io/protocols/glw/calibration/).

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
doctor), backup / restore, depth stream, safety guards, Lua lighting effects, the Qt GUI; unit tests and
Linux / Windows CI; releases; Linux AppImage; depth graph; virtual gamepad; Russian translation; background
mode with saved effects; switch calibration.

Next: precise depth from travel curves, more models.

## License

MIT (`core/`, `hid/`, `cli/`, `tests/`), see `LICENSE`. Part of
[open-he-mchose](https://github.com/open-he-mchose); not affiliated with MCHOSE.
