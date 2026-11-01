# End-to-end check of Body Tune through the real control layer (needs numpy; build target tune_check first).
#   TUNE_CHECK=build/tune_check python3 tools/tune_sweep.py
# Renders 4 s per note (16-88 every 3 semitones), tune off/on, several Body and Feedback Gain settings, with the
# plugin's default HPF (250 Hz) and with HPF 10 Hz. Classifies the pitch heard (autocorrelation of seconds 1.5-2.5):
#   pitch  = the played note (within 30 cents); octave = the note an octave (2x, 4x, 0.5x) away; silent / noisy / other.
import json, multiprocessing as mp, os, subprocess, sys, tempfile
import numpy as np

EXE = os.environ.get("TUNE_CHECK", "build/tune_check")
SR = 48000
TMP = tempfile.mkdtemp(prefix="audrey_tune_")
NOTES = list(range(16, 89, 3))

def mtof(n): return 440.0 * 2 ** ((n - 69) / 12)

def ac_pitch(seg, sr=SR):
    x = seg[: int(sr * 1.0)].astype(np.float64); x = x - x.mean()
    n = len(x); ac = np.fft.irfft(np.abs(np.fft.rfft(x, 2 * n)) ** 2)[:n]; ac /= ac[0] + 1e-30
    lmin = int(sr / 4000); lmax = min(n - 2, int(sr / 15.0))
    m = float(ac[lmin:lmax].max())
    if m < 0.5: return None
    for p in range(lmin + 1, lmax):
        if ac[p] >= 0.9 * m and ac[p] >= ac[p - 1] and ac[p] >= ac[p + 1]:
            a, b, c = ac[p - 1], ac[p], ac[p + 1]; d = a - 2 * b + c
            return sr / (p + 0.5 * (a - c) / d if d != 0 else p)
    return None

def job(a):
    note, tune, body, gain, hpf, exc = a
    out = os.path.join(TMP, f"{os.getpid()}.raw")
    subprocess.run([EXE, str(note), str(tune), str(body), str(gain), str(hpf), str(exc), out], check=True)
    seg = np.fromfile(out, dtype=np.float32)[int(SR * 1.5):int(SR * 3.5)]
    if float(np.sqrt(np.mean(seg ** 2))) < 0.003: return a, "silent"
    f = ac_pitch(seg)
    if f is None: return a, "noisy"
    f0 = mtof(note); cents = lambda x, r: 1200 * np.log2(x / r)
    if abs(cents(f, f0)) <= 30: return a, "pitch"
    if any(abs(cents(f, f0 * r)) <= 30 for r in (2, 4, 0.5)): return a, "octave"
    return a, "other"

if __name__ == "__main__":
    jobs = [(n, t, b, g, hp, e) for e in (0, 2) for hp in (250, 10) for g in (-12, -6, 0, 6) for b in (1, 5, 20) for t in (0, 1) for n in NOTES]
    with mp.Pool(os.cpu_count() or 2) as pool:
        res = pool.map(job, jobs, chunksize=8)
    rows = [dict(zip(("note", "tune", "body", "gain", "hpf", "exc"), a), cls=c) for a, c in res]
    if len(sys.argv) > 1: json.dump(rows, open(sys.argv[1], "w"))
    def share(sel):
        p = sum(r["cls"] == "pitch" for r in sel); o = sum(r["cls"] == "octave" for r in sel)
        return f"{100 * p // len(sel):3d}% / {100 * (p + o) // len(sel):3d}%"
    print("share of notes heard at the played pitch / at pitch or octave (25 notes per cell)")
    for e, name in ((0, "no input"), (2, "noise burst at input")):
        for hp in (250, 10):
            print(f"\n{name}, HPF {hp} Hz        Body Tune OFF    ->    ON")
            for b in (1, 5, 20):
                for g in (-12, -6, 0, 6):
                    f = lambda t: share([r for r in rows if (r["exc"], r["hpf"], r["body"], r["gain"], r["tune"]) == (e, hp, b, g, t)])
                    print(f"  body {b:2d} ms, gain {g:+3d} dB:   {f(0)}   ->   {f(1)}")
    print("\nBy note range, all settings with gain >= -6 dB, tune ON vs OFF:")
    for lo, hi in ((16, 36), (37, 57), (58, 70), (71, 88)):
        f = lambda t: share([r for r in rows if r["tune"] == t and r["gain"] >= -6 and lo <= r["note"] <= hi])
        print(f"  notes {lo}-{hi}: OFF {f(0)}   ON {f(1)}")
