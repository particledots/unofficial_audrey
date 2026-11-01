# Sweep: where does the played note ring at its own pitch? (needs numpy; build target pitch_check first)
#   PITCH_CHECK=build/pitch_check python3 tools/pitch_sweep.py [out.json]
# For each (excitation, body, feedback gain, HPF, note) it renders 4 s with the plugin's other defaults,
# takes the spectrum of seconds 1.5-3.5 and classifies the strongest partial:
#   silent : rms < 0.003 (nothing sustains)
#   pitch  : the played note's fundamental (within 30 cents)
#   octave : the played note an octave or two up/down in harmonic terms (2nd/4th/8th harmonic, within 30 cents)
#   other  : some other partial (the note is not what you hear)
import itertools, json, multiprocessing as mp, os, subprocess, sys, tempfile
import numpy as np

EXE = os.environ.get("PITCH_CHECK", "build/pitch_check")
SR, SECS = 48000, 4.0
BODIES = [1, 5, 20]                 # ms
GAINS = [-18, -12, -6, 0, 6]        # dB
HPFS = [10, 60, 250]                # Hz
NOTES = list(range(16, 89, 6))      # 16, 22, ... 88
MODES = {"self-oscillation (no input)": 0, "noise burst at input": 2}
TMP = tempfile.mkdtemp(prefix="audrey_sweep_")

def mtof(n): return 440.0 * 2 ** ((n - 69) / 12)

def ac_pitch(seg, f0, sr=SR):
    """Pitch a listener would hear, from the autocorrelation of 1 s of signal: the shortest period that is nearly as
    well repeated as the best one (so a tone made of many harmonics of a low note counts as that note)."""
    x = seg[: int(sr * 1.0)].astype(np.float64); x = x - x.mean()
    n = len(x); ac = np.fft.irfft(np.abs(np.fft.rfft(x, 2 * n)) ** 2)[:n]; ac /= ac[0] + 1e-30
    lmin = int(sr / 4000); lmax = min(n - 2, int(sr / 15.0))
    m = float(ac[lmin:lmax].max())
    if m < 0.5: return None, m
    for p in range(lmin + 1, lmax):
        if ac[p] >= 0.9 * m and ac[p] >= ac[p - 1] and ac[p] >= ac[p + 1]:
            a, b, c = ac[p - 1], ac[p], ac[p + 1]; d = a - 2 * b + c
            p = p + 0.5 * (a - c) / d if d != 0 else p
            return sr / p, m
    return None, m

def job(args):
    mode, body, gain, hpf, note = args
    out = os.path.join(TMP, f"{os.getpid()}.raw")
    subprocess.run([EXE, str(note), str(body), str(SR), str(SECS), out, str(gain), "8000", str(hpf), str(mode)], check=True)
    x = np.fromfile(out, dtype=np.float32)
    seg = x[int(SR * 1.5):int(SR * 3.5)]
    rms = float(np.sqrt(np.mean(seg ** 2)))
    if rms < 0.003:
        return args, dict(cls="silent", rms=rms)
    f0 = mtof(note)
    N = 1 << 19
    S = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), N)); fr = np.fft.rfftfreq(N, 1 / SR)
    S[fr < 15] = 0
    dom = float(fr[int(np.argmax(S))])
    h = max(1, round(dom / f0))
    err = 1200 * np.log2(dom / (h * f0))
    if abs(err) <= 30 and h == 1: cls = "pitch"
    elif abs(err) <= 30 and h in (2, 4, 8): cls = "octave"
    else: cls = "other"
    fac, m = ac_pitch(seg, f0)
    cents = (lambda f, r: 1200 * np.log2(f / r))
    if fac is None: acls = "noisy"
    elif abs(cents(fac, f0)) <= 30: acls = "pitch"
    elif any(abs(cents(fac, f0 * r)) <= 30 for r in (2, 4, 0.5)): acls = "octave"
    else: acls = "other"
    return args, dict(cls=cls, acls=acls, rms=rms, dom=dom, h=int(h), err=float(err), fac=fac, acm=m)

if __name__ == "__main__":
    jobs = [(m, b, g, hp, n) for m in MODES.values() for b in BODIES for g in GAINS for hp in HPFS for n in NOTES]
    with mp.Pool(max(1, (os.cpu_count() or 2))) as pool:
        res = pool.map(job, jobs, chunksize=8)
    rows = [dict(mode=a[0], body=a[1], gain=a[2], hpf=a[3], note=a[4], **r) for a, r in res]
    json.dump(rows, open(sys.argv[1] if len(sys.argv) > 1 else os.path.join(TMP, "sweep.json"), "w"))
    for name, m in MODES.items():
        print(f"\n=== {name} === (share of {len(NOTES)} notes ringing at the played pitch, incl. octave harmonics; [silent])")
        print("body gain |" + "".join(f" HPF{hp:>4}   " for hp in HPFS))
        for b in BODIES:
            for g in GAINS:
                cells = []
                for hp in HPFS:
                    sel = [r for r in rows if (r["mode"], r["body"], r["gain"], r["hpf"]) == (m, b, g, hp)]
                    ok = sum(r["cls"] in ("pitch", "octave") for r in sel); sil = sum(r["cls"] == "silent" for r in sel)
                    cells.append(f"{100 * ok // len(sel):3d}% [{sil:2d}]")
                print(f"{b:3d}ms {g:+3d}dB |  " + "   ".join(cells))
