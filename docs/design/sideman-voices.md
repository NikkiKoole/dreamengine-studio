# The Side Man's ten voices: what each one measures

> **STATUS: SHIPPED (2026-08-19)** ‑ [`runtime/sideman.h`](../../runtime/sideman.h) is the voice
> bank; [`tools/carts/smprobe.c`](../../tools/carts/smprobe.c) is the bench that produces the
> per-voice numbers and [`tools/carts/sideman.c`](../../tools/carts/sideman.c) the ones that decide
> headroom. The header comment is the design brief and the reasoning; this file is the measurement
> log, the method, and the things the first two cuts got wrong.

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
| BASS DRUM | F#2 | 92.5 | -16.3 | 6 | 150 | 177 | 89% | 95 | 113 | -37 | -11 | -45 | -18 | 0% |
| TOM TOM I | F#3 | 185.0 | -17.5 | 3 | 132 | 150 | 91% | 180 | 227 | -38 | -11 | -44 | -18 | 0% |
| TOM TOM II | C#3 | 138.6 | -17.4 | 4 | 147 | 169 | 91% | 135 | 170 | -38 | -11 | -44 | -18 | 0% |
| WOOD BLOCK | F#6 | 1480.0 | -15.4 | 0 | 41 | 45 | 92% | 1438 | 1757 | -39 | -12 | -48 | -19 | 1% |
| TEMPLE BLOCK I | B5 | 987.8 | -15.4 | 1 | 63 | 70 | 91% | 960 | 1198 | -38 | -11 | -46 | -19 | 0% |
| TEMPLE BLOCK II | F#5 | 740.0 | -15.5 | 2 | 82 | 89 | 91% | 719 | 918 | -38 | -11 | -44 | -18 | 0% |
| CLAVES | C#7 | 2217.5 | -15.3 | 0 | 25 | 28 | 97% | 2155 | 2348 | -47 | -16 | -48 | -29 | 3% |
| BRUSH | — | noise | -18.5 | 27 | 134 | 178 | 12% | 906 | 2731 | — | — | — | — | 10% |
| MARACAS | — | noise | -22.1 | 0 | 22 | 24 | 16% | 4068 | 5172 | — | — | — | — | 22% |
| CYMBAL | — | noise | -18.2 | 14 | 259 | 296 | — | 4838 | 5945 | — | — | — | — | 33% |

Every voice's band peak and centroid sit inside the machine's 60 Hz .. 6 kHz window, and nothing
measures any energy below 60 Hz. DC 0.0002, 0 clipped samples. See **Levels** below for the numbers
that actually decide headroom, which come from the cart and not from this bench.

### The plock, as four numbers

Each block's resonant peak lands on its note to within 0.1% (the wave and the `FILTER_BAND` agree
rather than fighting), the family is monotonic in both pitch and damping, and the two temple blocks
are a clean fourth apart:

| | Hz | -40 dB | in-band | front over ring |
|---|---|---|---|---|
| TEMPLE BLOCK II | 740.0 | 89 ms | 91% | +2.4 dB |
| TEMPLE BLOCK I | 987.8 | 70 ms | 91% | +2.7 dB |
| WOOD BLOCK | 1480.0 | 45 ms | 92% | +3.4 dB |
| CLAVES | 2217.5 | 28 ms | 97% | +4.7 dB |

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
difference against the local step-rms over a 10 ms window) on the same full render, skipping 30 ms
after each scheduled hit: **worst 3.8x over 257270 judged samples**, under the 4x threshold, so
there is no mid-note or note-off discontinuity anywhere in the bank. The guard has to cover the
slowest AMP ATTACK in the bank, not just the onset instant: at 15 ms one event reads 4.3x, and it
sits exactly 26 ms after a hit, which is the brush's attack time — its amplitude peak against a
baseline measured while it was still ramping. (Confirmed not to be the new cutoff corner: zeroing
`SIDEMAN_BRUSH_ENGAGE` leaves a 4.1x of the same kind elsewhere.) Cutting the tails to separate
WAVs instead does NOT work and cost a round: the cut edge is itself a discontinuity, and a 1 ms
raised-cosine fade over a loud signal is still a 6-8x step against a silent baseline, so four voices
reported a splice at exactly t=0.000s of their own cut. Mask, don't cut. The negative control is the
same run with the mask removed, which must go red and does (351.6x).

And the comparison the gate is actually for: the worst onset event went from **19757x on the first
cut to 352x**, a 35x improvement, from softer fronts and bandpassed noise.

## Levels: measure the CART, not the bench

The first pass balanced the ten voices against each other on the bench and set the headroom from a
synthetic ten-voice stack. That was the wrong reference and it shipped a bank **8.1 dB quieter than
its nearest sibling**: rendered through the cart with its organ cabinet, the default rhythm peaked
**-10.97 dBFS** against `cr78`'s **-2.87 dBFS** on the same 7-second headless render. A cart 8 dB
down reads as broken next to the shelf, whatever its spectra say.

The number to tune is the cart with the cabinet IN. Now, all twelve rhythms, `--frames 900`, measured
over 3..15 s (the default row also carries the 7-second boot render, which is the A/B command):

| rhythm | peak dBFS | rms dBFS | crest | clipped |
|---|---|---|---|---|
| BEGUINE | -7.86 | -23.40 | 15.5 | 0 |
| BOLERO | -6.28 | -24.74 | 18.5 | 0 |
| CHA CHA | -5.16 | -23.62 | 18.5 | 0 |
| FOXTROT 2 BEAT | -10.42 | -25.53 | 15.1 | 0 |
| **FOXTROT 4 BEAT** (default) | **-6.55** (-6.75 over 7 s) | -21.92 | 15.4 | 0 |
| MARCH | **-4.27** | -23.63 | 19.4 | 0 |
| RHUMBA | -5.18 | -23.02 | 17.8 | 0 |
| SAMBA | -6.41 | **-19.58** | 13.2 | 0 |
| SHUFFLE | -7.51 | -23.97 | 16.5 | 0 |
| TANGO | -7.13 | -20.41 | 13.3 | 0 |
| WALTZ | -7.88 | -23.86 | 16.0 | 0 |
| WESTERN | -9.95 | -24.08 | 14.1 | 0 |

Default at **-6.75 dBFS**, 3.9 dB under `cr78`; loudest rhythm **-4.27 dBFS**, so the outboard chain
keeps 4.3 dB; nothing clips anywhere.

Three findings from getting there, each of which changes how you would do it again:

**The cabinet is not what costs the headroom.** Switching EQ+IRON out (the cart's `C` key) moves the
RMS by **2.22 dB** and the peak by **0.12 dB**. So the WARM low boost is not eating the top: it adds
body under a peak that does not move, which is the same shape [`analog-outboard-chain.md`](analog-outboard-chain.md) measured for
the comp. Every dB of the 8 dB gap was the bank's.

**SAMBA is the densest rhythm but MARCH is the loudest.** SAMBA carries maracas on all sixteen and
has 1.8 dB more RMS than anything else, and its crest is the lowest in the table at 13.2 dB — a
continuous rattle bed, not a spiky one. MARCH peaks **2.1 dB higher** at crest 19.4 dB, because its
step 0 lands bass + wood block + cymbal together. A peak comes from coincidence, not from density,
so "the densest rhythm" is the wrong thing to headroom-check. Check the highest-crest one too.

**Nothing upstream of the drive can add level.** Worth knowing before anyone sweeps for it: the tube
shaper normalises full-scale to full-scale, so raising the wooden family's `FILTER_BAND` resonance
from 9 to 13 — which multiplies the filter's peak gain several times over — moves the output by
**0.4 dB**, while in-band energy falls from 94% to 87% and out-of-band doubles. Level lives strictly
after the drive: `vol`, `instrument_level`, `SIDEMAN_TRIM`.

Which is why the bank now runs at the **top** of the per-slot gain range: `vol` 7 × `level` 1.0 ×
`SIDEMAN_TRIM` 1.0 is the ceiling, the first cut sat 4.2 dB under it, and the cart needed all 4.2.
`SIDEMAN_TRIM` is a down-only lever now, and so is `boost` (see the next section). The gap between
the default and SAMBA also had to close a little, which is why the maracas took +2.7 dB where
everything else took +4.2: it is the voice that only shows up in the dense rhythms.

## No velocity, and what that costs

Recorded here because it is a limit, not a defect. A spinning disc closes a contact the same way
every revolution: there is no accent, no velocity, and no dynamics anywhere in the Side Man, and the
cart calls `sideman_fire(..., boost = 0)` always. So the bank is voiced at full level and:

- **`boost` is a down-only trim.** A positive boost clamps to 7 and does nothing.
- **Velocity would not change the TONE even if it changed the level.** The engine applies drive
  post-filter but PRE-VCA, so the tube sees a constant-amplitude signal and the harmonic ladder is
  identical at every volume. Measured: the h2/h3/h5 ladder is the same at vol 3 and vol 7.

Both are faithful for this machine (a tube fed by a fixed contact pulse does not sag differently),
but a cart playing this bank from a **keybed** inherits both, and the second one is the real ceiling
on expressiveness. Fixing it means a per-voice drive that tracks velocity, which is an engine change,
not a voicing change.

## The brush failed the ear test: three rounds, and one negative result that matters

The owner listened to the bank. Verbatim: *"sounds pretty good except the brush that doesn't sound
like a brush."* Nine voices passed and are now a thing to protect. What followed is three rounds of
blind iteration against a moving target, and the most useful thing to come out of it is a null.

### Round 1 (shipped): "sounds like a woosh"

He reached for "woosh" independently, and the numbers had already said why: the shipped brush's mid
band sits **11.8 dB ABOVE the quieter of its two ends**, so it lives in the nasal region that reads as
filtered noise, and the carefully measured 1700-cent arc documented further down travels entirely
inside the wrong place. A good reminder that a measurement can be correct and irrelevant at once.

**The fix is confirmed by ear and is kept in every candidate since:** split the spectrum into a low
body and a high fizz with the middle scooped. The challengers measure **+5.7 to +8.1 dB** the other
way.

### Round 2 (three grain settings): "the other 3 sound very similar"

This is the result worth recording. The hypothesis was that a brush is nothing but grain, so three
candidates swept granular amplitude texture: sample-and-hold at 72/120 Hz, the same at 156/260 Hz, and
a sine at the second rate and depth. Measured, they spanned **2.06x / 3.71x / 3.98x** envelope
modulation depth, with a sharp response knee between 90 and 130 Hz and a 30 dB tonal spike separating
the sine from the sample-and-hold. Every one of those numbers is right.

**None of it survived contact with an ear.** All three were indistinguishable to the listener. So the
grain metric, its knee and its shape discriminator are not measuring what a listener hears on this
material, and the reason they collapsed is visible in hindsight in a column nobody was reading: all
three shared a **375 ms drag envelope with a 63 to 89 ms attack**, so they were the same *gesture*, and
gesture dominated everything the modulation was doing. Grain is now **fixed** at round two's C setting
and is not an axis. Do not sweep it again.

The general lesson, which is cheap to state and expensive to learn: when a listener says two things
sound the same, the differing parameter is not the problem, and the shared structure is where to look.

### Round 3 (the current probe): a brushed jazz snare

*"i am more looking like a noise snare thing that fizzles out a bit more a jazzy snare?"* That kills
the "brushing means a sustained drag" inference behind round two and gives three structural
requirements: it is a **hit with a tail**, not a symmetric drag; **"fizzles out" is about the tail**, so
a short head body with the wires sizzling on underneath at a lower level for a good while after; and
because this machine has **no snare voice at all**, the wire rattle is central rather than decorative.
That is the house snare shape (`cr78`'s tonal shell plus bandpassed rattle, `tr808.h`/`tr909.h`'s body
plus snappy), so 2/3/4 also carry a quiet tonal **shell** at two pitches in the bank's own F# (F#3 and
C#4, the toms' tuning), which is what makes an ear hear a drum rather than a burst of noise.

His phrasing is ambiguous between "the tail should sizzle on longer" and "it should trail away more
gradually", so candidates 3 and 4 take one reading each instead of asking.

The axis is the **body-to-tail relationship**, and after round two the steps are structural rather than
parametric:

| | shape | peak | atk | -40 dB | -60 dB | @100 | @200 | @400 | @700 | bend | body cntrd | tail cntrd |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **1 ROUND2 C** | the control he heard | -24.6 | **45** | 375 | 385 | -7 | -19 | — | — | 2.67 | 4987 | 2183 |
| **2 TAP** | tight tail, body-dominant | -24.9 | **5** | 275 | 285 | -13 | -22 | — | — | 1.99 | 2677 | 6697 |
| **3 SIZZLE** | loud sizzle, stops | -23.7 | **5** | 445 | 455 | -7 | -9 | -27 | — | **6.05** | 3963 | 6358 |
| **4 TRAIL** | quiet, goes on | -24.7 | **5** | **915** | **935** | -12 | -11 | -14 | **-20** | **0.10** | 3286 | 6540 |

`@N` is the level in dB below the peak at N ms. `bend` is the late decay slope (200-500 ms) divided by
the early one (10-90 ms), which is the number that separates a snare from a drag: **a single
exponential decay is a straight line in dB, so one decay reads 1.0, a tail taking over reads far
below, and a plateau-then-cliff reads far above.** Candidate 1 is 2.67 because a drag holds and then
falls off a cliff; candidate 3 is 6.05 (a loud sizzle that stops hard); candidate 4 is **0.10**, the
tail completely taking over, which is "trails away gradually" as a number.

The four are within **1.2 dB** on peak and **0.17 dB** on groove RMS, so the listen is about structure.
Attack is the gross difference this round turns on: **45 ms for the drag against 5 ms for all three
snares.**

**The honest cost, measured.** The sample-and-hold grain steps gain instantaneously, and the splice
oracle sees it. With onsets masked at a 60 ms guard, grain off is a clean **PASS at 3.8x with zero
samples over 4x**; grain on reads **6.4x worst, with 186 samples over 4x, 16 over 5x, 3 over 6x and 0
over 8x** across 46 seconds of render. So it is three isolated single-sample coincidences, at the very
bottom of the 6-20x band `click-check.js` documents as audible, on a noise bed whose own steps are
comparable. The owner heard sample-and-hold grain in round two and reported no crackle, so the texture
is left exactly as he heard it. Candidate 4's long quiet trail does use `LFO_SHAPE_RANDOM` (a smooth
filtered random walk: just as irregular, but continuous) both because it halved that layer's
contribution and because it is the right physics for many wires settling. If a grainy candidate wins
and crackles, that is the one-line fix for the others.

## The brush's first fix: a band that travels

The first pass gave the brush the right envelope and the wrong motion. It is the only soft-front
voice in the box and five of the twelve rhythms lean on it for the backbeat (FOXTROT, MARCH, SHUFFLE,
WALTZ, WESTERN), so it earns more than one contour.

A swirl is a brush **travelling** across a head, and a single `ENV_CUTOFF` cannot travel: it rises
and returns to where it started. Two things fixed it, both cheap:

- **Two cutoff envelopes on the body slot**, which SUM (`sound.h` adds each env's contribution into
  the same `cutoff`). Env 0 opens the band as the wires engage; env 1 arrives later, is NEGATIVE, and
  pulls the band down **past** its resting point as the brush leaves.
- **A second slot for the wire tips**: a narrower band at 3.1 kHz, fired 12 ms behind the body, with
  its own contour running the other way (bright, then losing speed). The same two-layer trick as
  `cr78`'s snare.

Measured on the band peak per 12 ms window across one stroke:

| | 0 | 24 | 48 | 72 | 84 | 120 | 144 ms |
|---|---|---|---|---|---|---|---|
| single contour (before) | 1812 | 2283 | 1812 | 1711 | 1711 | 2034 | 1920 |
| two layers (after) | 1812 | **2283** | 1812 | 1078 | **855** | 960 | 1210 |

The before row spans 900 cents with **no time order** — that is noise-bin jitter, not travel. The
after row is an arc: up to 2283 Hz at 24 ms, down to 855 Hz at 84 ms, creeping back to 1210 by
144 ms. **1700 cents of travel**, monotone in each half. The centroid follows (2720 → 4173 → 1878 →
2225 Hz), and `wav-envelope`'s brightness ratio over the stroke went from 1.5x with no trend to 2.8x
ordered in time.

Two knobs and why they sit where they do. `SIDEMAN_BRUSH_LEAVE` at -450 Hz bottoms the band at
855 Hz; -800 measures more travel (2600 cents) but bottoms at 509 Hz, which reads as a soft tom
rather than a brush tail. `SIDEMAN_BRUSH_TOP_VOL` at 4 lifts the mid-stroke centroid by ~950 Hz over
having no tips at all (1895 → 2840 Hz at 72 ms) without flattening the arc, which volume 5 starts to
do. The attack stays soft: 27 ms to peak, still the only voice in the bank that does not start at
once.

## The cymbal: 494 ms was a modern crash

Shortened to **296 ms** to -40 dB (amp decay 500 → 300 ms, release 150 → 95, metal layer 320 → 210).
The Side Man's cymbal is described everywhere as thin, splashy and the weakest voice on the machine,
and half a second of decay is a 1970s ride. Nothing in the measurements argued for keeping it long.

The metal layer's balance was re-trimmed to hold the place it was in before the decay changed, since
shortening the noise more than the squares would have promoted the metal by proportion alone: it now
adds **0.27 dB to the cymbal's RMS** (about 6% of its energy) while its partials stand **9..21 dB**
above the local noise floor. Same two numbers as before the change, one for "not dominant" and one
for "plainly audible".

## What is still open

- **The bank has no upward headroom left.** It sits at the top of the per-slot gain range to reach
  the cart's target, so `SIDEMAN_TRIM` and `boost` are both down-only. A cart that wants it louder
  needs its own gain, and a cart that stacks more than four voices on a step should trim down: ten
  voices fired together measure -0.6 dBFS.
- **The velocity ceiling above.** The ladder is level-independent because drive runs pre-VCA. Right
  for this machine, wrong for any keybed cart that borrows the bank.
- **The maracas is the least in-band voice** at 22% of its energy over 6 kHz, and the cymbal at 33%.
  A 2-pole bandpass cannot do better without turning noise into a whistle; a steeper per-slot filter
  or a second cascaded band would.
- **The brush is waiting on a third listen.** Four candidates are baked into `smprobe`, spanning the
  body-to-tail axis; the shipped recipe stays in place until one is picked. `LFO_SHAPE_RANDOM` on the
  wire layers is the one-line fix if a grainy one wins and crackles.
- **We have no measurement that predicts this voice.** Round two's grain numbers were correct and
  perceptually dead, so for the brush specifically the numbers are now used to make the candidates
  DIFFERENT and level-matched, and the choice between them is entirely the ear's. Weight bets toward
  what the structure should sound like, not toward which number is largest.
- **The rest of the bank has been heard and passed.** Which makes it a regression surface: any future
  edit to `sideman.h` should re-render `smprobe` and diff the first 21 seconds against the committed
  bench, the way this round did.
