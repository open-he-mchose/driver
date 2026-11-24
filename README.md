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

A Qt GUI will follow in `gui/` (GPL-3.0); `core`, `hid` and `cli` stay MIT.

## Supported keyboards

| Model | Status |
|---|---|
| Jet75-II (`41E4:211A`) | supported, tested |
| other `glw` models | planned: model data from the official driver's device table ([devices.json](https://github.com/open-he-mchose/docs/blob/main/data/devices.json)) once their layouts are documented |

## mchosectl

```text
mchosectl list                        connected keyboards
mchosectl info                        firmware, profiles, report rate, lighting
mchosectl keymap [--layer L]          key mapping (also --factory)
mchosectl trigger                     actuation / rapid trigger of every key
mchosectl macros                      stored macros
mchosectl profile N                   switch the active profile
mchosectl remap CapsLock Esc          remap a key (names, Ctrl+Shift+T, Mute, none, raw hex)
mchosectl trigger set W A S D --actuation 1.2 --rt on --press 0.15 --release 0.15
mchosectl light set --effect 0 --brightness 60
mchosectl backup [FILE]               everything except calibration, as JSON
mchosectl restore FILE                write back only what differs
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

Done: protocol core, profiles, FuncConfig (lighting, report rate, locks), trigger settings, keymap, macros
(codec), backup / restore, depth stream, safety guards, unit tests.

Next: macro and advanced-key editing in the CLI, live lighting, precise depth from travel curves, more models,
releases, then the Qt GUI.

## License

MIT (`core/`, `hid/`, `cli/`, `tests/`), see `LICENSE`. Part of
[open-he-mchose](https://github.com/open-he-mchose); not affiliated with MCHOSE.
