# Porting notes: Audrey engine to a plugin

Source: AudreyTouch (MIT, Infrasonic Audio) and Audrey-II (MIT, Synthux Academy), read in full, engine files only.
The hardware engine was compiled and run on Linux before porting, so the engine is known to be separable from the Daisy hardware.

## What changed from the firmware engine
| # | Firmware behaviour | Plugin |
|---|---|---|
| 1 | `SDRAM::allocate<>` (Daisy only) | `std::make_unique` |
| 2 | ReverbSc buffer fixed for 48 kHz; at 88.2 kHz and above `Init` fails and the engine ignored it, which overran the buffer (found with AddressSanitizer) | buffer size x4 (192 kHz), `Init` result checked |
| 3 | LFO state in function-local `static` variables, shared by all instances | per-instance members |
| 4 | `std::random_device` called on the audio thread | small xorshift PRNG |
| 5 | `lfo_freq_`, `lfo_on_` uninitialised | initialised |
| 6 | delay buffers sized for 48 kHz (echo max would shrink to 2.5 s at 96 kHz) | sized for 192 kHz |
| 7 | `WhiteNoise` multiplies a signed int and overflows (undefined behaviour) | same generator with unsigned arithmetic, identical output |
| 8 | `std::tanf` does not compile with GCC | `std::tan` |
| 9 | Output limiter lives in the firmware main loop, outside the engine | `Limiter` at pre-gain 0.7 added at the end of `Engine::Process` |

Kept as the hardware does it: output is always scaled by 0.5 in the final mix; echo feedback can exceed 1 and is soft-clipped; the reverb is the DaisySP-LGPL `ReverbSc`.

## Not verified yet
* How it sounds compared with the hardware (no hardware to compare).
* The lowest notes at very high sample rates (KarplusString has a fixed 8192-sample line; inferred to degrade near 192 kHz, not measured).
