# Unofficial Audrey

AU + VST3 plugin (macOS, universal arm64 + x86_64) built from the open-source **Audrey** feedback-synth engine
(AudreyTouch / Audrey-II firmware by Infrasonic Audio and Synthux Academy, MIT). Not a Synthux Academy product.

**Status: Phase 1 skeleton.** The engine runs with fixed settings and responds to MIDI notes. Host parameters, UI and presets come next.
See `PORTING-NOTES.md` for what was changed from the firmware.

## Get a build
Every push builds on GitHub: Actions tab > latest run > Artifacts > `UnofficialAudrey-macOS`.
Unzip it and double-click `Install-Unofficial-Plugin.command` (see `INSTALL.txt` inside). In Logic the plugin shows as
**particledots: Unofficial Audrey** in a *MIDI-controlled effect* slot.

## Headless test
```
cmake -S . -B build -DAUDREY_BUILD_PLUGIN=OFF && cmake --build build --target engine_test && build/engine_test
```

## Licensing
`third_party/` holds MIT code (DaisySP) and two LGPL-2.1 files (ReverbSc, Tone) from DaisySP-LGPL. The engine in `src/engine`
is derived from MIT-licensed Audrey firmware. Notices are in `THIRD-PARTY-LICENSES.txt`. The new wrapper code in `src/Plugin*`
has no licence assigned yet. JUCE is used under its own licence terms (AGPLv3 / commercial); check them before distributing builds.
