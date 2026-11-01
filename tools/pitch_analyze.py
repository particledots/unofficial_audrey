# Offline pitch analysis of tools/pitch_check.cpp renders (needs numpy). Run: python3 tools/pitch_analyze.py
import subprocess, sys, numpy as np, json, os
import tempfile
SP=os.path.join(tempfile.gettempdir(),"audrey_pitch"); os.makedirs(SP,exist_ok=True)
EXE=os.environ.get("PITCH_CHECK","build/pitch_check")   # cmake --build build --target pitch_check
SR=48000
SECS=6.0
def mtof(n): return 440.0*2**((n-69)/12)
def run(note, body, sr=SR, secs=SECS, extra=()):
    out=f"{SP}/n{note}_b{body}_{sr}.raw"
    subprocess.run([EXE,str(note),str(body),str(sr),str(secs),out,*map(str,extra)],check=True)
    return np.fromfile(out,dtype=np.float32)
def analyse(x, f0, sr=SR):
    seg=x[int(sr*3.0):]                      # skip start-up glide + build-up
    rms=float(np.sqrt(np.mean(seg**2)))
    if rms<1e-5: return dict(rms=rms,silent=True)
    w=np.hanning(len(seg)); N=1<<20
    S=np.abs(np.fft.rfft(seg*w,N)); fr=np.fft.rfftfreq(N,1/sr)
    db=20*np.log10(S/S.max()+1e-12)
    # peaks within 30 dB of max
    pk=[i for i in range(2,len(S)-2) if S[i]>S[i-1] and S[i]>=S[i+1] and db[i]>-30 and fr[i]>15]
    pk=sorted(pk,key=lambda i:-S[i])[:12]
    peaks=[(float(fr[i]),float(db[i])) for i in pk]
    dom=peaks[0][0]
    # lowest strong peak (within 20 dB) as fundamental estimate
    strong=sorted([p for p in peaks if p[1]>-20])
    low=strong[0][0]
    cents=lambda f,ref:1200*np.log2(f/ref)
    # error of dominant peak against nearest harmonic of the intended note
    h=max(1,round(dom/f0)); err_h=cents(dom,h*f0)
    # error of lowest strong peak against nearest harmonic
    hl=max(1,round(low/f0)); err_l=cents(low,hl*f0)
    # does any strong partial sit within 25 cents of a harmonic of f0? fraction
    ok=sum(1 for f,d in peaks if d>-20 and abs(cents(f,max(1,round(f/f0))*f0))<25)
    tot=sum(1 for f,d in peaks if d>-20)
    # autocorrelation pitch
    seg2=seg[:int(sr*1.0)]; seg2=seg2-seg2.mean()
    ac=np.fft.irfft(np.abs(np.fft.rfft(seg2,2*len(seg2)))**2)[:len(seg2)]
    ac/=ac[0]
    lo=int(sr/min(2000,max(f0*4,60))); hi=int(sr/max(15.0,f0/2))
    hi=min(hi,len(ac)-2)
    k=lo+int(np.argmax(ac[lo:hi])) if hi>lo else lo
    if 1<k<len(ac)-1:
        a,b,c=ac[k-1],ac[k],ac[k+1]; k=k+0.5*(a-c)/(a-2*b+c) if (a-2*b+c)!=0 else k
    f_ac=sr/k
    return dict(rms=rms,dom=dom,dom_h=int(h),dom_err=float(err_h),low=low,low_h=int(hl),low_err=float(err_l),
                harm_frac=f"{ok}/{tot}",f_ac=float(f_ac),ac_err=float(cents(f_ac,f0)),ac_peak=float(ac[int(round(sr/f_ac))] if int(round(sr/f_ac))<len(ac) else 0),peaks=peaks[:6])
if __name__=="__main__":
    notes=list(range(16,89))
    res={}
    for body in (1,5,10):
        for n in notes:
            f0=mtof(n)
            x=run(n,body)
            res[f"{body}_{n}"]=analyse(x,f0)
    json.dump(res,open(f"{SP}/results.json","w"))
    print("done",len(res))
