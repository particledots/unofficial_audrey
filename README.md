# Unofficial Audrey

AU + VST3 plugin (macOS, universal arm64 + x86_64) built from the open-source **Audrey** feedback-synth engine
(AudreyTouch / Audrey-II firmware by Infrasonic Audio and Synthux Academy, MIT). Not a Synthux Academy product.

**Status: Phase 2.** 17 host-automatable, smoothed parameters, the AudreyTouch MIDI CC map, an editor, and saved state.
See `PORTING-NOTES.md` for what was changed from the firmware and `PHASE2-NOTES.md` for the parameter and CC reference.

## Get a build
Every push builds on GitHub: Actions tab > latest run > Artifacts > `UnofficialAudrey-macOS`.
Unzip it and double-click `Install-Unofficial-Plugin.command` (see `INSTALL.txt` inside). In Logic the plugin shows as
**particledots: Unofficial Audrey** in a *MIDI-controlled effect* slot.

## Tests
Headless engine + control-layer test (no JUCE):
```
cmake -S . -B build -DAUDREY_BUILD_PLUGIN=OFF && cmake --build build --target engine_test && build/engine_test
```
CI also builds and runs `AudreyStateTest` (JUCE: parameters, state save/restore, CC -> host parameter) on macOS.

Offline pitch measurement (Phase 2 finding, see `PHASE2-NOTES.md`): `cmake --build build --target pitch_check`, then `python3 tools/pitch_analyze.py`.

## Licensing
`third_party/` holds MIT code (DaisySP) and two LGPL-2.1 files (ReverbSc, Tone) from DaisySP-LGPL. The engine in `src/engine`
is derived from MIT-licensed Audrey firmware. Notices are in `THIRD-PARTY-LICENSES.txt`. The new wrapper code in `src/Plugin*`
has no licence assigned yet. JUCE is used under its own licence terms (AGPLv3 / commercial); check them before distributing builds.
