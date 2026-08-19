# The Side Man's ten voices: what each one measures

> **STATUS: SHIPPED (2026-08-19)** ‑ [`runtime/sideman.h`](../../runtime/sideman.h) is the voice
> bank; [`tools/carts/smprobe.c`](../../tools/carts/smprobe.c) is the bench that produces every
> number below. The header comment is the design brief and the reasoning; this file is the
> measurement log, the method, and the three things the first cut got wrong.

The acceptance criterion for this bank came from the repo owner in one sentence: *"these old drum
machines in organs from the 50s, 60s, 70s have these wooden plocking sounds, the drums sound kinda
full."* The plock and the fullness are the deliverable. Nobody involved in tuning it could listen,
so every claim in the header had to become a number that can go red.

## How to reproduce any number here

```bash
node tools/play.js smprobe script /dev/null --headless --frames 1260 --wav /tmp/sm.wav
```

The bench pins every event to a frame, so the WAV is a map: voice `i` alone at `1.00 + i*0.75`
seconds in panel order, the whole bank at once at 9.50, a wooden roll at 10.50, a four-bar groove at
13.50. That is what lets `wav-envelope --from/--to`, `harmonic-spec`, `inharm-spec` and
`click-check` each read one hit rather than a mixture.

Two method notes that cost real time:

- **`harmonic-spec.js` starts its window 35% into the file.** On a 200 ms cut of a 45 ms voice that
  window is entirely tail, so it measures 16-bit quantisation noise, which is broadband and reads as
  upper harmonics: a synthetic PURE decaying sine measured h3 at **-17.8 dB** that way. Cut a region
  that stays loud across the whole window, or read the ladder over an explicit
  onset-to-(-25 dB) window.
- **A whole-period zero-crossing reading cannot see a pitch tuck on the bass drum.** One cycle at
  92 Hz is 10.8 ms, so a 26 ms tuck is under three cycles and the period average erases it: a tuck
  that is plainly there measured **0 cents**. Use half-periods (both crossing directions), or
  compare the spectral band peak of the first 25 ms against the settled one.

Whatever analyser you point at this, **run its self-test first**. A broken analyser and a broken
voice print the same table, and that trap fired twice during this work.

## The measured bank

Solo hits, dry, no cabinet, no outboard chain. `peak` is a 1 ms-RMS peak (a relative balance
number, ~4 dB under the sample peak); `atk` is time to that peak; `-20`/`-40` are the decay
crossings in ms; `in-band` is the share of energy within ±150 cents of the resonance; `band pk` is
the centre of the loudest 1/6-octave band and `cntrd` the spectral centroid, both in Hz; `h2..h5`
are dB relative to the fundamental.

| voice | note | peak Hz | peak dB | atk | -20 | -40 | in-band | band pk | cntrd | h2 | h3 | h4 | h5 | >6 kHz |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| BASS DRUM | F#2 | 92.5 | -20.6 | 6 | 150 | 177 | 89% | 95 | 113 | -37 | -11 | -45 | -18 | 0% |
| TOM TOM I | F#3 | 185.0 | -21.7 | 3 | 132 | 150 | 91% | 180 | 227 | -38 | -11 | -44 | -18 | 0% |
| TOM TOM II | C#3 | 138.6 | -21.6 | 4 | 147 | 169 | 91% | 135 | 170 | -38 | -11 | -44 | -18 | 0% |
| WOOD BLOCK | F#6 | 1480.0 | -19.7 | 0 | 41 | 45 | 92% | 1438 | 1756 | -39 | -12 | -48 | -19 | 1% |
| TEMPLE BLOCK I | B5 | 987.8 | -19.6 | 1 | 63 | 70 | 91% | 960 | 1198 | -38 | -11 | -46 | -19 | 0% |
| TEMPLE BLOCK II | F#5 | 740.0 | -19.7 | 2 | 82 | 89 | 91% | 719 | 918 | -38 | -11 | -44 | -18 | 0% |
| CLAVES | C#7 | 2217.5 | -19.7 | 0 | 25 | 28 | 97% | 2155 | 2348 | -47 | -16 | -48 | -29 | 3% |
| BRUSH | — | noise | -23.7 | 27 | 137 | 170 | 22% | 1812 | 2927 | — | — | — | — | 11% |
| MARACAS | — | noise | -23.8 | 3 | 22 | 24 | 31% | 4310 | 5154 | — | — | — | — | 22% |
| CYMBAL | — | noise | -23.4 | 1 | 431 | 494 | — | 4838 | 5888 | — | — | — | — | 33% |

Whole bank: **all ten at once -4.51 dBFS peak, a four-bar groove -6.96 dBFS peak**, 0 clipped
samples, DC 0.0001. Every voice's band peak and centroid sit inside the machine's 60 Hz .. 6 kHz
window, and nothing measures any energy below 60 Hz.

### The plock, as four numbers

Each block's resonant peak lands on its note to within 0.1% (the wave and the `FILTER_BAND` agree
rather than fighting), the family is monotonic in both pitch and damping, and the two temple blocks
are a clean fourth apart:

| | Hz | -40 dB | in-band | front over ring |
|---|---|---|---|---|
| TEMPLE BLOCK II | 740.0 | 89 ms | 91% | +2.1 dB |
| TEMPLE BLOCK I | 987.8 | 70 ms | 91% | +2.3 dB |
| WOOD BLOCK | 1480.0 | 45 ms | 92% | +3.1 dB |
| CLAVES | 2217.5 | 28 ms | 97% | +4.4 dB |

"Front over ring" is the peak of the first 2.5 ms against the peak of 5..18 ms: the hard front the
ear reads as a struck block. With the shared contact click removed the same column reads
+0.4 / +0.6 / +0.9 / +1.6 dB, so the click is worth about 2 dB of front and the resonant band's own
transient supplies the rest. At click gain 1.6 it becomes +2.9 / +3.2 / +4.2 / +5.9, which is a tick
separating from the block.

### The fullness, as a saturation curve

Wood block, sweeping `SIDEMAN_TUBE` with `ab-render.js` (which also proves the value reaches the
DSP — it exits 2 on byte-identical renders):

| SIDEMAN_TUBE | h2 | h3 | h5 | in-band | claves >6 kHz |
|---|---|---|---|---|---|
| 0.00 | -94 | -30 | -44 | 100% | 0% |
| 0.30 | -52 | -17 | -31 | 98% | 0% |
| **0.45** | **-39** | **-12** | **-19** | **92%** | **3%** |
| 0.60 | -39 | -10 | -16 | 88% | 7% |

0.45 is the knee: 0.30 is 20 dB thinner on h5, and past 0.45 the ladder gains 2 dB while the
claves' out-of-band energy doubles. Pushed further (0.75, 1.00) the shaper measures within 1 dB of
0.50 — the pre-gain is `amount² × 24`, so it is fully saturated by then.

Per voice the amount is scaled by how hard that circuit drives the tube (`SmVoice.tube`), because
a high voice's drive harmonics leave the 6 kHz band and a low voice's do not: the claves run at 0.60
of the bank amount and the membranes at 1.00.

## Three things the first cut got wrong

**1. DRIVE_ASYM is not an even-harmonic generator.** The header claimed the fullness comes from even
harmonics, and it does not. `drive_shape`'s asym curve is `tanh(s·g)` with the negative half's
pre-gain scaled by `(1 - 0.4·amount)`: a tanh is an ODD function, so at every amount the odd
partials lead. Measured across five amounts on the wooden family, h3 and h5 sit **25..30 dB above**
h2 and h4 without exception. What the asymmetry genuinely buys is the even partials existing at all:
on the wood block h2 climbs from **-94 dB bypassed to -39 dB** at 0.45, a 55 dB rise. So the claim
survives in a weaker and more useful form — the *ladder* is the fullness, and the even half of it is
what separates `DRIVE_ASYM` from `DRIVE_SOFT`. The header now says that.

**2. Two of the ten voices were digital hiss.** Maracas and cymbal were highpassed noise
(`FILTER_HIGH` at 5.2 and 5.6 kHz), which leaves everything up to Nyquist: measured **96% and 97% of
their energy above 6 kHz, peaking at 19 and 21 kHz**. A 1959 tube channel physically cannot make
that. Both are bandpasses now. This is also why the band limit belongs in this header and not in the
cart's cabinet: it is the coupling capacitor and the tube's own bandwidth, not the box.

Be honest about how far that goes. The engine's `FILTER_BAND` is 2-pole, so a 6 dB/oct skirt against
a 16 kHz-wide upper band still leaves the three noise voices spilling 11 / 22 / 33% over 6 kHz. The
only lever that pushes those to zero is a resonance so narrow the noise stops being noise: at
resonance 15 the maracas measures **72%** of its energy within ±150 cents, which is a pitched ping
and not a rattle. Resonance 13 is that trade.

**3. The bass drum chirped.** +11 semitones over 40 ms at 65 Hz measured a **624-cent** drop over
4 cycles, which is pitch movement you can hum, and a bridged-T ringing network does not sweep. It is
119 cents now (99 → 92 Hz, settled inside 20 ms), and the drum was retuned up to F#2 / 92 Hz, out of
the sub and into the band. Sweeping the tuck: 0 → 0 cents, 3 semitones → 0, 5 → 119, 7 → 254.

## Clicks and splices

`click-check.js` reports 64 events at/above 4x on the bench render and always will: the bench is
sparse percussion, and this gate's own header documents that an onset landing on a near-silent tail
divides by almost nothing. Two things make that verdict useful anyway.

**All 70 listed events fall within 12 ms of a scheduled hit**, checked against the bench's own
schedule. So every one is an onset, and the gate is reporting the false positive it warns about.

**Re-running its metric with the onsets masked out gives an edge-free PASS.** Same measure (first
difference against the local step-rms over a 10 ms window) on the same full render, skipping 15 ms
after each scheduled hit: **worst 3.8x over 342478 judged samples**, under the 4x threshold, so
there is no mid-note or note-off discontinuity anywhere in the bank. Cutting the tails to separate
WAVs instead does NOT work and cost a round: the cut edge is itself a discontinuity, and a 1 ms
raised-cosine fade over a loud signal is still a 6-8x step against a silent baseline, so four voices
reported a splice at exactly t=0.000s of their own cut. Mask, don't cut. The negative control is the
same run with the mask removed, which must go red and does (351.6x).

And the comparison the gate is actually for: the worst onset event went from **19757x on the first
cut to 352x**, a 35x improvement, from softer fronts and bandpassed noise.

## Levels

`SmVoice.level` is the relative balance and `SIDEMAN_TRIM` is one bank-wide gain, so headroom is a
single number to A/B and the balance stays readable. The wooden family sits ~2 dB forward of the
membranes (it is the melody of this machine) and the bass drum is a dB under the blocks rather than
over them, which is what "soft and round, not a punch" means in a mix. Gain is linear in both knobs,
so a target peak is one multiplication away: every value in the table above was hit on the first
attempt from the previous render's numbers.

## What is still open

- **The tube is a static shaper, not a tube.** A real single-ended stage sags: the harmonic content
  should depend on how hard it is hit. The engine applies drive post-filter and PRE-VCA, so the
  shaper sees a constant-amplitude signal and the ladder is identical at every velocity. That is
  why `boost` changes only loudness here. Fixing it needs a per-voice drive that tracks velocity.
- **The brush is the least convincing voice.** It has the right envelope (27 ms to peak, the only
  soft front in the bank) but a swirl is a moving band, and one `ENV_CUTOFF` contour is a crude
  stand-in for a brush travelling across a head.
- **The cymbal's decay is 494 ms to -40 dB, which is long for 1959.** It measures as intended but
  the era is worth a second opinion from an ear.
- **Nothing here has been heard.** Every number is a proxy. The two-part bar
  ([ADR-0022](../decisions/0022-collaboration-is-the-north-star.md)) needs the second half.
