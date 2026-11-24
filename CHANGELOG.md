# Changelog

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
