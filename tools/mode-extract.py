#!/usr/bin/env python3
"""mode-extract.py: a struck object's MODES from a recording, ready for instrument_modes().

    python3 -I tools/mode-extract.py <one-hit.wav> [--top 50] [--out <dir>] [--tlen <s>] [--wavs] [--fast] [--from <modes.json>]

WHAT IT GIVES YOU
  The decaying sinusoids a recording is made of (Hz, amplitude at the strike, decay dB/s, starting
  phase), found by SUB-BAND ESPRIT, plus the NOISE the strongest --top modes leave out: a decaying wash
  BED per half octave for the modes too weak to list, and a 6 ms TICK where the modes do not already
  supply the first 20 ms. It prints both as C tables in exactly the shape instrument_modes() and
  instrument_modes_noise() take, and how much of the recording the modes explain at several counts.
  --out writes modes.json (every mode, strongest first); --wavs also writes a resynthesis per count
  (and the residual) so the right count can be picked BY EAR, which is how drumhand's ride got 400
  (50 = the first count that sounds like a cymbal, 400 = the first as rich as the recording).

WHY ESPRIT, AND WHY SUB-BANDS
  A cymbal is ~2000 modes and full of near-identical PAIRS (a ride: 305 of its 708 strong modes sit
  within 3 Hz of a neighbour, beating slower than 3/s). Picking FFT peaks counts one smeared pair two
  or three times (the first attempt did, and its resynthesis came out 2.2x too loud). ESPRIT fits
  frequency, damping and complex amplitude JOINTLY, so a pair comes out as two poles. Run over the
  whole band it would need thousands of poles; instead each 200 Hz slice is heterodyned to baseband,
  low-passed and decimated to 735 Hz, and fitted with at most 40 poles, over a Hankel window of half the
  fitted span (~2100 rows for a 6 s hit). That is the slow part and it is NOT free to shrink: `--fast`
  caps it at 512 rows (~3 min instead of ~24) and on the ride resolved the close pairs less well
  (all-modes band error 2.9 dB vs 2.2; the 271 Hz mode missing from the top 50). Explore with --fast,
  make the table you ship without it.

  FIVE things it does that the obvious build gets wrong, each of which cost a wrong result once:
  - every mode keeps its measured STARTING PHASE (the 4th column). A cymbal's close pairs start out of
    phase and swell into phase after the hit; started all together, the ride's 2 kHz band came out 10 dB
    hot for the first 0.3 s, a swell turned into a spike.
  - poles decaying faster than 150 dB/s are DROPPED and the amplitudes re-solved without them. They are
    the attack, not the plate, and projected back to t=0 they explode (one came out +44 dB at 14 kHz).
  - modes are anchored at T0 (20 ms, after the stick contact) and the phase is referenced with the
    BASEBAND offset, not the absolute Hz (that bug left the waveform residual at 117%).
  - the noise rows are measured as band ENERGY of (recording minus the top-N modes); noise is
    incoherent, so subtracting waveforms would be meaningless for it.
  - and UNMIXED through the engine's own band shapes (non-negative least squares per frame): fitted one
    octave at a time, the 4 kHz wash's skirt was taken for the 8 kHz wash and the top rang 7-20 dB long.

WHAT IT CANNOT SEE
  Anything nonlinear (a hard crash BLOOMING brighter after the hit: on drumhand's soft ride stroke the
  linear modes reproduced the 2 kHz swell, it was beating), and anything the source lacks (freesound
  MP3 previews: the last seconds sit near the codec's floor, so the late tail is not trustworthy).

  Measured on build/refs/drums-cc0/ride.wav (2026-11-06, this tool, 24 min): 2005 modes; all of them
  explain 79% of the waveform from 20 ms on and match the octave bands within 2.2 dB over 0-3 s;
  50/90/99% of the modal energy sits in 8/128/469 modes. drumhand's table = `--top 400 --from` that run's modes.json.
  Needs numpy + scipy. Design: docs/guides/tuning-drum-voices.md "Taking a cymbal's modes".
"""
import sys, os, json, argparse
# ONE BLAS thread, set BEFORE numpy loads: thousands of small complex SVDs make OpenBLAS's worker threads
# spin-wait (sampled: most of the time in inner_thread/gettimeofday, not maths). Threaded, --fast ran >1 hour.
for _v in ('OPENBLAS_NUM_THREADS', 'OMP_NUM_THREADS', 'MKL_NUM_THREADS', 'VECLIB_MAXIMUM_THREADS'):
    os.environ.setdefault(_v, '1')
import numpy as np
import scipy.io.wavfile as wavfile
from scipy.signal import firwin, lfilter

ap = argparse.ArgumentParser()
ap.add_argument('wav')
ap.add_argument('--top', type=int, default=50, help='modes in the emitted table (pick by ear, see --wavs)')
ap.add_argument('--out', default=None, help='directory for modes.json (+ WAVs with --wavs)')
ap.add_argument('--tlen', type=float, default=0, help='seconds ESPRIT sees (default: the whole file after 20 ms)')
ap.add_argument('--wavs', action='store_true', help='write resynthesis WAVs per mode count + the residual')
ap.add_argument('--from', dest='from_json', default=None, help="reuse a modes.json (--out of an earlier run): skip ESPRIT, refit the noise for a new --top in ~a minute")
ap.add_argument('--fast', action='store_true', help='cap the ESPRIT window at 512 rows: ~3 min, close pairs less resolved')
A = ap.parse_args()

sr, x = wavfile.read(A.wav)
x = x.astype(np.float64)
if x.ndim > 1: x = x.mean(1)
if np.abs(x).max() > 2: x /= 32768.0
N = len(x); t = np.arange(N) / sr

W, DEC, ORDER = 200.0, 60, 40
LMAX = 512 if A.fast else 10**9    # Hankel rows (see the header: the cap costs accuracy)
R = sr / DEC
T0 = 15 * DEC / sr                  # ~20 ms, an exact decimated sample: past the stick contact
MAXDEC = 150.0                      # dB/s: faster is a transient, not a mode
TLEN = A.tlen if A.tlen > 0 else (N / sr - T0 - 0.05)
h = firwin(801, 140, fs=sr); gd = (len(h) - 1) // 2

modes = []
for fc in ([] if A.from_json else np.arange(W / 2 + 40, min(17000, 0.45 * sr), W)):
    zf = lfilter(h, 1, np.concatenate([x * np.exp(-2j * np.pi * fc * t), np.zeros(gd)]))[gd:]
    zd = zf[::DEC]
    s0 = int(round(T0 * R)); y = zd[s0:s0 + int(TLEN * R)]
    M = len(y); L = min(M // 2, LMAX)
    if L < ORDER + 2: continue
    H = np.lib.stride_tricks.sliding_window_view(y, L).T
    U, sv, _ = np.linalg.svd(H, full_matrices=False)
    k = min(ORDER, int(np.sum(sv > sv[0] * 10 ** (-50 / 20))))
    if k == 0: continue
    Us = U[:, :k]
    poles = np.linalg.eigvals(np.linalg.lstsq(Us[:-1], Us[1:], rcond=None)[0])
    fb = np.angle(poles) * R / (2 * np.pi)
    dd = -20 * np.log10(np.clip(np.abs(poles), 1e-9, 0.99999)) * R
    keep = (dd < MAXDEC) & (np.abs(poles) < 1.0005)
    poles, fb, dd = poles[keep], fb[keep], dd[keep]
    if len(poles) == 0: continue
    amps = np.linalg.lstsq(poles[None, :] ** np.arange(M)[:, None], y, rcond=None)[0]
    for f_, d_, a_ in zip(fb, dd, amps):
        if abs(f_) <= W / 2 and 2 * abs(a_) > 1e-5:
            modes.append(dict(f=float(fc + f_), amp=float(2 * abs(a_)), decay_db_s=float(d_),
                              phase=float(np.angle(a_) - 2 * np.pi * f_ * T0)))
if A.from_json: modes = json.load(open(A.from_json))
for m in modes: m['energy'] = m['amp'] ** 2 / max(m['decay_db_s'], 0.5)
modes.sort(key=lambda m: -m['energy'])

def resynth(ms):
    y = np.zeros(N); on = t >= T0
    for m in ms:
        y += on * m['amp'] * 10 ** (-m['decay_db_s'] * (t - T0) / 20) * np.cos(2 * np.pi * m['f'] * t + m['phase'])
    return y

OCT = [250, 500, 1000, 2000, 4000, 8000, 16000]
def band_env(s, lo, frame=441, oct=1.0):
    e = 2 ** (oct / 2)
    Y = np.fft.rfft(s); f = np.fft.rfftfreq(len(s), 1 / sr); Y[(f < lo / e) | (f >= lo * e)] = 0
    b = np.fft.irfft(Y, len(s)); return (b[:len(b) // frame * frame].reshape(-1, frame) ** 2).mean(1)

seg = slice(int(T0 * sr), N)
E = np.array([m['energy'] for m in modes]); cs = np.cumsum(E) / E.sum()
print(f"{os.path.basename(A.wav)}: {len(modes)} modes; 50/90/99% of modal energy in "
      f"{np.searchsorted(cs, .5) + 1} / {np.searchsorted(cs, .9) + 1} / {np.searchsorted(cs, .99) + 1} modes")
B0 = np.array([band_env(x, lo) for lo in OCT])
print("  modes   waveform unexplained   octave-band error 0-3 s")
for n in sorted(set([10, 25, 50, 100, 200, 400, A.top, len(modes)])):
    if n > len(modes): continue
    y = resynth(modes[:n])
    res = np.sum((x[seg] - y[seg]) ** 2) / np.sum(x[seg] ** 2)
    B = np.array([band_env(y, lo) for lo in OCT])
    err = np.mean(np.abs(10 * np.log10(B0[:, 2:300] + 1e-14) - 10 * np.log10(B[:, 2:300] + 1e-14)))
    print(f"  {n:5d}   {res * 100:6.1f}%               {err:5.1f} dB")
    if A.wavs and A.out:
        os.makedirs(A.out, exist_ok=True)
        wavfile.write(os.path.join(A.out, f'{n:04d}-modes.wav'), sr, (y / np.abs(x).max() * 0.9 * 32767).clip(-32767, 32767).astype(np.int16))

# ---- the noise the top-N modes leave: per octave a TICK (0..T0) and a BED (T0 on), as band RMS
top = modes[:A.top]
yt = resynth(top)
# the modes as they will PLAY (back-projected to t=0) for the tick window
y0 = np.zeros(N)
for m in top:
    a0 = m['amp'] * 10 ** (m['decay_db_s'] * T0 / 20)
    y0 += a0 * 10 ** (-m['decay_db_s'] * t / 20) * np.cos(2 * np.pi * m['f'] * t + m['phase'])   # as the engine plays them
# The engine plays each noise row through TWO band-pass stages, so a band LEAKS into its neighbours. Two
# lessons, both measured on the ride: (1) fit one octave at a time and the 4 kHz wash's skirt is taken for
# the 8 kHz wash, so the bands are UNMIXED per frame by non-negative least squares through the engine's own
# filter shapes (G[i][k] = the share of band k's power landing in band i); (2) even unmixed, OCTAVE bands
# cannot follow a decay rate that changes steeply with frequency (this ride: ~7 dB/s at 4 kHz, ~14 at 8):
# late in the tail the slow band's spill alone out-rings the fast one, which no non-negative fit can
# remove. So the wash is fitted in HALF-OCTAVE bands. The tick rows go only where the fit finds a tick.
from scipy.optimize import nnls
WID = 0.5
GRID = [250 * 2 ** (k * WID) for k in range(int(round(np.log2(16000 / 250) / WID)) + 1)]
def stage_q(oct):
    x = 2 ** (0.5 * oct); return 0.6428 / (x - 1 / x)     # = the engine's modes_band_q
Qs = stage_q(WID)
ff = np.linspace(1.0, sr / 2, 200000)
G = np.zeros((len(GRID), len(GRID)))
for k, fc in enumerate(GRID):
    fc_ = min(fc, 0.45 * sr)
    H4 = (1.0 / (1.0 + Qs * Qs * (ff / fc_ - fc_ / ff) ** 2)) ** 2
    for i, c in enumerate(GRID):
        e = 2 ** (WID / 2); G[i, k] = H4[(ff >= c / e) & (ff < c * e)].sum() / H4.sum()
EO = np.array([band_env(x, c, oct=WID) for c in GRID]); EY = np.array([band_env(yt, c, oct=WID) for c in GRID])
E0 = np.array([band_env(y0, c, oct=WID) for c in GRID])
nt = int(T0 * sr / 441)
tick_p = nnls(G, np.clip(EO[:, :nt].mean(1) - E0[:, :nt].mean(1), 0, None))[0]
D = np.clip(EO - EY, 0, None)
Lk = np.array([nnls(G, D[:, f])[0] for f in range(D.shape[1])]).T    # per band, per 10 ms frame
tt = np.arange(Lk.shape[1]) * 0.01
rows = []
tau = 0.006
for k, hz in enumerate(GRID):
    L = Lk[k]; s_ = (tt >= T0) & (tt < 3.0) & (L > 0)
    if s_.sum() < 10: continue                            # neighbours explain this band: no row
    db = 10 * np.log10(L[s_]); t_ = tt[s_]
    keep = db >= db[:5].mean() - 40                       # stop at the floor (MP3 previews have one)
    sl, ic = np.polyfit(t_[keep], db[keep], 1)
    rows.append((round(hz), 10 ** ((ic + sl * T0) / 20), max(-sl, 0.5), 'bed'))
for k, hz in enumerate(GRID):
    if tick_p[k] > 0.01 * tick_p.max():
        rows.append((round(hz), np.sqrt(tick_p[k] * 2 * T0 / (tau * (1 - np.exp(-2 * T0 / tau)))), 20 * np.log10(np.e) / tau, 'tick'))

tm = sorted(top, key=lambda m: m['f'])
print(f"\n// {A.top} strongest modes of {os.path.basename(A.wav)}: Hz, amplitude at the strike, decay dB/s, starting phase (turns)")
print(f"static const float MODES[{len(tm)}][4] = {{")
cells = ['{%.2ff,%.6ff,%.1ff,%.3ff}' % (m['f'], m['amp'] * 10 ** (m['decay_db_s'] * T0 / 20), m['decay_db_s'],
                                      (m['phase'] / (2 * np.pi)) % 1.0) for m in tm]
for i in range(0, len(cells), 4): print('    ' + ', '.join(cells[i:i + 4]) + ',')
print("};")
print(f"// the noise those {A.top} leave, per half octave: a decaying wash BED, and a 6 ms TICK where one is left (band RMS)")
print(f"static const float BANDS[{len(rows)}][4] = {{")
for hz, a, d, kind in rows: print(f"    {{ {hz:5d}, {a:.6f}f, {d:7.1f}f, {WID}f }},   // {kind}")
print("};")
print("// instrument_modes(slot, &MODES[0][0], N); instrument_modes_noise(slot, &BANDS[0][0], N);")

if A.out:
    os.makedirs(A.out, exist_ok=True)
    json.dump(modes, open(os.path.join(A.out, 'modes.json'), 'w'), indent=0)
    if A.wavs:
        wavfile.write(os.path.join(A.out, 'zz-residual.wav'), sr, ((x - resynth(modes)) / np.abs(x).max() * 0.9 * 32767).clip(-32767, 32767).astype(np.int16))
