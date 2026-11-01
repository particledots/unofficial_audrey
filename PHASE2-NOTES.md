# Phase 2 notes: parameters, MIDI CC, UI, state

## Pitch check (done before anything else, 31 Oct 2026)
Phase 1 settings, notes 16-88, 48 kHz, 6 s per note, spectrum of the last 3 s (`tools/pitch_check.cpp`, `tools/pitch_analyze.py`).
* The loop self-oscillates (input 0, only the -90 dBFS noise floor) and locks onto a tooth of the body delay's comb (multiples of 1/body), not onto the played note: at 1 ms, 69 of 73 notes have their strongest partial on a tooth (56 of 73 between 700 and 1100 Hz); at 5 ms 73 of 73; at 10 ms 71 of 73.
* The strongest partial is the fundamental for only 7 / 15 / 17 of 73 notes at 1 / 5 / 10 ms. Partials sit on the harmonic series of the note (the string is tuned), but the note is not the loudest thing, so it sounds atonal.
* Error of the strongest partial against the nearest harmonic: median 3.8 / 5.9 / 8.0 cents at 1 / 5 / 10 ms; more than 25 cents on 7 / 15 / 21 notes. At 1 ms: under 5 cents up to about note 40, about 24 cents median for notes 77-88 (inferred cause: about half a sample of loop filter delay is not compensated in the string tuning; not isolated).
* Not verified: behaviour of the real hardware, and the pitch with audio at the input (a plucked-string test was inconclusive).
* Decision (Bryn): proceed with the faithful engine; Body defaults to 1 ms.

## Parameters (17) - ranges from Phase 0, curves verified in the firmware source
Curves are DaisySP `fmap`: EXP = min + n^2 (max - min), LOG = min (max/min)^n; Reverb Decay is `ftension(n, -3)`.
| Parameter | Range | Default | Curve |
|---|---|---|---|
| Frequency Offset | -36..+36 st (added to the held note, result clamped to notes 16-88; note 40 when none played) | 0 | linear |
| Feedback Gain | -60..+12 dB | -6 | linear |
| Body | 1 ms..100 ms | 1 ms | EXP |
| Feedback LPF / HPF | 100..18000 Hz / 10..4000 Hz | 8 kHz / 250 Hz | LOG |
| Reverb Mix | 0..1 | 0.3 | linear |
| Reverb Decay | 0.2..1 | 0.85 | anti-exp (ftension -3) |
| Output Volume | 0..1 | 0.5 | EXP |
| Input Volume | 0..5 | 0 | EXP |
| Envelope Shape | 0..1 | 0 | linear |
| LFO Frequency | 0.01..7 Hz | 1 | LOG (the firmware is linear; log is easier to use) |
| LFO Distribution | 0..1 | 0.5 | linear |
| LFO On, Drone On | on/off | off | - |
| Echo Send | 0..1 | 0 | EXP |
| Echo Time | 0.05..5 s | 0.5 | EXP |
| Echo Feedback | 0..1.5 | 0 | linear |
Defaults: Frequency, Echo Time, Echo Feedback, LFO, Envelope, Output, HPF are the firmware's registered defaults; the rest are playable defaults (Phase 1 values), because the firmware overwrites them from the knobs at boot.
Smoothing: 20-50 ms one-pole per parameter, applied every 16 samples; toggles are immediate. Body also glides inside the engine (about 0.2 s, firmware behaviour). While the LFO is on it drives the body, as on the hardware.

## MIDI CC (AudreyTouch `processMIDI` and manual)
7 Output Volume, 12 Reverb Decay, 70 Frequency, 72 Envelope Shape, 74 LPF, 75 Feedback Gain, 76 Body, 77 Input Volume, 81 HPF, 91 Reverb Mix, 123 All Notes Off. 0..127 covers the full range of each parameter through its own curve.
* CC 70 sets the string pitch directly (note 16-88, no offset) until the next note-on or Frequency Offset change (as on the hardware); it has no host parameter.
* A CC moves the sound immediately (audio thread) and the matching host parameter within about 33 ms (message-thread timer, so the UI and automation recording follow). A value set by CC stays until the host or UI moves that parameter (no soft takeover, same as the hardware).
* The firmware's panel-only pad combinations and the LFO/body gesture on S37 have no CC and are not part of this map.

## State
Parameters are saved with the standard JUCE value-tree XML plus a version tag (`audreyStateVersion` = 1). Saved and restored plugins render bit-identical audio (checked in CI).

## Not verified
* How it sounds and feels in Logic, UI layout on screen (only compiled and syntax-checked, not looked at).
* auval, long-run CPU, many instances.
* Behaviour of CC-driven host parameter updates in Logic automation modes.
