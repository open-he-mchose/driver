# Changelog

## 0.2.1 (2026-11-25)

- GUI: the keyboard has a fixed size (no longer changes with the window or the tab); the key picker on the Keymap
  tab has a background that fits the keys
- GUI: Macros tab shows the keys a macro is bound to and can remove it from them (they get their default function
  back); "Delete macro" also frees its keys
- Jet75-II layout: Delete at the top right, below it Home, PgUp, PgDn (checked on the physical board)
- Correction to 0.2.0: on Windows macros and Snap Tap work without a fix (reported by a user); the NKRO fix is
  needed on Linux only

## 0.2.0 (2026-11-24)

First release of **`mchose-gui`**, the graphical configurator (Qt 6 / Qt Quick, GPL-3.0), and Lua lighting effects.
Supported and tested: **Jet75-II** (firmware V116). The GUI package is for Windows (x64); on Linux build it from
source (Qt 6.5+), an AppImage is planned. `mchosectl` packages for Linux and Windows as before.

GUI (`mchose-gui`), system look (the platform's default style and palette):

- visual keyboard: select keys by click, Shift+click or drag; live key depth
- actuation and rapid trigger per key
- keymap of all four layers: pick the new function from lists (no need to press the key on the keyboard), restore
  factory keys, dangerous codes (reset / bootloader) marked
- macro editor: steps list or text, record from the keyboard, play modes, bind to keys
- advanced keys: DKS, Toggle, Mod-Tap, Rapid Strike, Snap Tap, LT; broken bindings listed
- lighting: effects, brightness, speed, colour, per-key colours
- **Effects**: Lua lighting scripts with editor, examples, parameters as sliders and a live preview
- profiles (switch, rotation, copy), report rate, Win / Alt+Tab / Alt+F4 / Menu locks, Win / Mac mode
- backup / restore

Lua lighting effects (also in the CLI: `mchosectl light script FILE.lua`):

- `frame(ctx)` 25 times per second with key positions, key presses from the system, parameters
  (`min`, `max`, `default`, `label`, `step`, `order`)
- sandbox (no files, no OS access) with instruction and memory limits; live colours only (no flash writes)
- examples: rainbow wave, typing ripple, typing speedometer on the top row, heat map, fire, rain, Game of Life

Other changes:

- only one program of the project uses a keyboard at a time (two programs sending live colours made it flicker);
  the second one reports which program holds it

Known limitations: as in 0.1.0 (Snap Tap, macros and more than six keys need the Linux NKRO fix; on Windows SOCD and
macro keys do not reach the system). On Linux, key presses for Lua effects need read access to the keyboard's input
devices (the `input` group). The official web driver is not covered by the lock: close it while using these tools.

## 0.1.0 (2026-11-24)

First release of `mchosectl`, the command-line tool of the open driver for MCHOSE magnetic keyboards.
Supported and tested: **Jet75-II** (firmware V116). Linux (x86_64) and Windows (x64).

Everything the official driver configures on the Jet75-II:

- profiles: switch, rotation of the next-profile key, copy a profile
- report rate 1000 / 2000 / 4000 / 8000 Hz (the keyboard reboots; the tool reconnects and verifies)
- per-key actuation, rapid trigger (press / release distances, continuous mode)
- keymap of all four layers, with readable key names
- lighting: effects, brightness, speed, colour, stored per-key colours, live colours
- macros: text format (`+Key`/`-Key`, `Ctrl+C` taps, `"typed text"`, delays), play modes once / hold / toggle / N times
- advanced keys: DKS, Toggle, Mod-Tap, Rapid Strike, Snap Tap (SOCD), LT, and `adv doctor` to find broken bindings
- backup / restore of everything except calibration; restore writes only what differs
- live key depth (`depth`)

Safety: a full backup before the first write of every run, read-back after every write, a flash-write limit per run,
and explicit flags for report-rate changes on the active profile, reset / bootloader key codes and profiles outside
the rotation.

Known limitations:

- Snap Tap (SOCD), macros and more than six simultaneous keys need the
  [Linux NKRO fix](https://github.com/open-he-mchose/linux-fixes) because of a firmware descriptor bug; on Windows
  SOCD and macro keys do not reach the system at all (no user-side fix known).
- Close the official web driver while using `mchosectl`; two programs on the same interface disturb each other.
- Other MCHOSE models are not supported yet.
