# Choochootracker: what to borrow

**STATUS: BUILDING (2026-09-27)** — a ranked borrow list read off one repo. Row 1, the MME voice,
is SHIPPED as `INSTR_MME` (ported cart-first the same day; the `mme` cart keeps the cart-land
reference as an E-toggled A/B). Rows 2 and 3 are SHIPPED as `INSTR_METAL` (2026-09-28, cart-first
in the `bogie` cart the same day, which now cycles engine / prototype / 808 on one set of pads).
Row 5 is SHIPPED as `INSTR_SINTER` (2026-09-28, cart-first in the `sintered` cart the same day).
Row 7 turned out mostly shipped already; its one missing piece, per-track speed, is BUILT as the
`slipstep` cart (2026-09-28). Row 6 is BUILT as the `specimens` cart (twelve algorithmic techniques).
Row 4 is open. Each row names the upstream file, what it
would become here, and why it made or missed the cut. Update the row (not this line) when
something lands.

Upstream: <https://github.com/paiheulevrai/Choochootracker> (MIT, `paiheulevrai`, 2026). A fork of
the ChipNomad tracker for Anbernic handhelds: LSDJ-style tracker workflow, SDL2, C++. Every path
below is relative to `chipnomad_lib/` in that repo.

## The verdict in one paragraph

The engine menu advertises Braids, Plaits, Plaits-Alt, open303 and Clouds. All of that is vendored
third-party C++ (~135k lines) and we have already decided against it: [`instrument-engines.md`](instrument-engines.md) §8
and [ADR-0017](../decisions/0017-three-macro-core-plus-engine-aux-channel.md) borrow Plaits's harmonics/timbre/morph *discipline* and not its code, [`pocketbox.md`](pocketbox.md)
rates our `INSTR_*` family as exceeding Braids's range, and `acid303.h` is our own 303 measured
against `tb303`. The build constraint is hard anyway: carts compile with clang C, emcc and libtcc,
and libtcc cannot compile C++ at all, so a vendored Mutable voice would drop out of the live
editor. What IS worth taking is the ~1.1k lines the author wrote by hand: small, plain-float,
C-portable voices, plus their engine roster as a menu of ideas.

## The list, ranked

| # | Borrow | Upstream file | Becomes | Why this rank |
|---|---|---|---|---|
| 1 | **MME voice** (Multi Modulation Engine) | [`synth/mme_voice.cpp`](https://github.com/paiheulevrai/Choochootracker/blob/main/chipnomad_lib/synth/mme_voice.cpp) (151 lines) | a new engine, macro-mapped: harmonics = model, timbre = amount, morph = flow; a `MODE_MME_FEEDBACK` aux for the feedback knob | the ONE sound on that shelf we do not have: two oscillators through seven cross-modulation models (diode ring, fold, cross, VPM, sync, XOR logic, 20-band vocoder). We ship ring mod and fold as EFFECTS; nothing here is an aggressive two-osc VOICE. Two details worth keeping verbatim: the Warps diode ring-mod formula, and the feedback path that reinjects only the AC component (a DC-blocked feedback state) so a wild patch never collapses to silence or a flat line. Inspired by Noise Engineering's Loquelic Iteritas |
| 2 | **Bogie hat + cymbal metal bank** | [`synth/drum_synth_voice.cpp`](https://github.com/paiheulevrai/Choochootracker/blob/main/chipnomad_lib/synth/drum_synth_voice.cpp) (`hat` / `cymbal` cases) | a tuning pass on `morphdrum.h`'s hat seam | six squares at inharmonic ratios, each with a DIFFERENT decay time, so the bank never settles into a pitched square-wave chord. Our 808 hat is the same six-square idea; whether ours already staggers the mode lifetimes is the first thing to check, measured with `harmonic-spec` before and after |
| 3 | **Bogie cowbell cross-mod** | same file, `cowbell` case | one `fm` knob on the cowbell voice in `tr808.h` or `morphdrum.h` | a third oscillator modulates the two fixed squares; at zero it is the classic pair, turned up it goes controllably metallic. Cheap, and the "stays classic at zero" shape is our detent rule |
| 4 | **Tilt EQ with drive below centre** | [`synth/track_tilt.cpp`](https://github.com/paiheulevrai/Choochootracker/blob/main/chipnomad_lib/synth/track_tilt.cpp) (52 lines) | a per-slot one-knob tilt (candidate for `outboard.h` or a rack channel strip) | one knob: low shelf up + high shelf down around a pivot (250..4000 Hz), and the bottom quarter of the knob adds tanh drive. Our `eq()` is three bands on the master; a channel-strip cart wants ONE knob per track. Includes 10 ms parameter smoothing |
| 5 | **Sintered** (six percussive models: knot, shard, burst, comb, logic, melt) | [`synth/sintered_voice.cpp`](https://github.com/paiheulevrai/Choochootracker/blob/main/chipnomad_lib/synth/sintered_voice.cpp) (110 lines) | recipes for a "wild percussion" kit, or MME's drum face once #1 exists | same family as MME. The reusable recipe is the excitation: a 1.5..7 ms noise impact into a smoothed, DC-blocked feedback tail, with a per-model `motion` envelope that pushes the knobs for the first few ms. Idea-level; the models themselves are one-liners |
| 6 | **Plaits-Alt engine roster as a MENU** | [`external/mutable/plaits_alt/dsp/engine2/`](https://github.com/paiheulevrai/Choochootracker/tree/main/chipnomad_lib/external/mutable/plaits_alt/dsp/engine2) (Lyle Mills, 2026, MIT) | nothing to port; a shortlist for future `INSTR_*` work | names things we do NOT have: gendy (dynamic stochastic), bytebeat, wave terrain, VOSIM, pulsar, FOF vowel, LPC speech, phase flock, tapfield, glisson, rulefield. Each is algorithmically small and hand-portable if a cart ever wants one. Read the header comments (they say what OUT and AUX are per engine), not the code |
| 7 | **Sequencer ideas** (not engines) | `playback_fx_*.cpp`, the manual | acidcandy / groovebox carts | P-locks via track FX columns, probability + modulo trig conditions, per-track playback speed, mod sources that target other modulations, decoupled (free-running) tables. These are the Elektron/Nerdseq features our racks keep reaching for |

## Rows 2 and 3, as built: the `bogie` cart (2026-09-28)

Cart-only, from stock engine pieces, no engine work: each hat and the cymbal is six
`INSTR_SQUARE` slots at Bogie's inharmonic ratios, each with its OWN decay (per-slot ADSR),
plus one highpassed `INSTR_NOISE`; the cowbell is one `INSTR_MME` slot, because Bogie's cowbell
cross-modulation (a third oscillator modulating two fixed squares) IS the MME cross model on a
square pair, through the 808's 2.6 kHz bandpass. `tr808.h` is built alongside and key 8 routes
the same four pads to its hat / open hat / cymbal / cowbell, so both kits answer one gesture.
Per-pad trims put each pad within 0.5 dB of the 808's peak. `spec()` = 33 assertions on the bank
maths (the stagger holds on both banks at every tone, FM spreads the top mode more than the
bottom, the cowbell's ratio range sits inside MME's interval knob, the panel).

What `wav-envelope` MEASURED on single hits, the reason this row exists:

- **The 808 open hat is a chord.** Its brightness is flat for the whole decay (3.97 in every
  60 ms window, centroid pinned at 19 kHz): one spectrum, fading. Bogie's open hat and cymbal
  MOVE, brightness 1.1 → 2.3 across the tail, because the six modes die at different rates.
- **Bogie staggers upward.** Its top modes get the longest lifetimes (`.025 + o*.020`), so the
  lows die first and the tail BRIGHTENS. Synth Secrets' 808 cymbal does the opposite (the low band
  carries the long decay, the highs die first). A stagger-direction knob is the obvious next
  experiment; the cart ports Bogie's direction as-is.
- **The closed hat is a chord in Bogie too**, a 60 ms one: at its short DECAY the global
  `exp(-5.5 t/dur)` envelope dominates and the six lifetimes collapse to 25..30 ms. The stagger is
  an open-hat / cymbal property.

Two approximations the prototype carries: the engine amp decay is a LINEAR ramp
(`sound_adsr_gated`), so each exponential time constant is a 3.5× ramp there (2.5× cut the cymbal
short, measured); and a hat hit costs seven voices. Both are why it became an engine the same day.

## Rows 2 and 3, the engine: `INSTR_METAL` (2026-09-28)

One voice per hit in `sound.h`: six naive squares at Bogie's ratios (hat table → cymbal table on
harmonics), each with its own exponential envelope, plus Bogie's bright noise on the base
lifetime, through a DC blocker. Initial phases sit on the golden ratio (free-running oscillators
caught at unrelated phases; six squares starting at 0 sum to a spike). Macros: harmonics = tone,
timbre = noise mix, morph = STAGGER DIRECTION, bipolar with the chord at 0.5: 0 = the lows live
longest (Synth Secrets' 808 cymbal), 1 = the highs do (Bogie). `MODE_METAL_DECAY` (20 ms..2 s,
log) and `MODE_METAL_SPREAD` on the aux channel. Trimmed to the 808 hat's PEAK at the same vol,
which is hotter than level-check's sustained-tone baseline; a drum's peak is not a tone's, and
`instrument_level()` can only cut.

Measured on the engine with the noise at zero, the knob does what it says: brightness rises
across the cymbal's decay at morph 0.85 (0.33 → 0.44, centroid 8.2 → 8.8 kHz) and falls at 0.15
(0.30 → 0.20). With the noise on, the noise shapes the first ~100 ms and the bank the rest.
Deliberately NOT in tune-check's sweep (mode 0 is at f0 × 1.18, the bank has no fundamental), so
its level was set by hand in the cart. The `bogie` cart cycles engine / slot-bank prototype / 808.
Its first real home, the same day: `morphdrum.h`'s `MD_HAT` (its seam comment had described exactly
this engine since it was written). CHAR became the stagger direction there, 808 chord → 909
lows-longest; the FM hat stays behind `MD_HAT_ENGINE` for the A/B. [`morphdrum.md`](morphdrum.md).

## Row 5, as built: the `sintered` cart (2026-09-28)

Six pads, each its own patch (model + pitch + mod / a / b / c / motion / decay), rendered in
cart-land C into a PCM slot when a knob changes and played through `INSTR_SAMPLE` at root. For a
one-shot drum that path is EXACT, not a prototype's shortcut: upstream reseeds Sintered's noise on
every note-on, so every hit of a patch was always the same sound, and a pattern costs one voice per
hit. Ported from `synth/sintered_voice.cpp` in `de_*` math with a symmetric floor-modulo fold (same
fmodf issue as MME's, but only below -1: for small inputs upstream's fold is an inverted triangle and
that shape is kept), a 10 Hz output DC blocker, a 5 ms end fade (upstream stopped at -48 dB, which
is a step in a buffer) and per-hit peak normalisation. `spec()` = 60. A hit reads about -16.5 dBFS
at vol 6, several dB under the 808 kit, because the sample path's level can only cut; the engine
version would carry its own trim.

**The engine, same day: `INSTR_SINTER`.** Its own voice rather than a mode on MME: they share the
idea but not the code (Sintered's six models, impact and motion envelope have no MME counterpart, so
a mode would have been a second voice behind one id). Macros: harmonics = model (6 detents), timbre
= MOD, morph = C, live on a ringing hit; A / B / MOTION / DECAY on the aux channel. It keeps
upstream's own LCG, reseeded per note-on, so a hit is as repeatable as the render was. Per-model
trims (`SN_TRIM`) put every default pad at -12.0 dBFS; unlike the render, the engine does NOT
normalise per hit, so MOD and C driving into tanh move the level, as they should on an instrument.
One trap met on the way: a local named `delayed` in the comb model was silently rewritten by
`sound_ctx.h`, which `#define`s every per-instance engine static by name (`delayed` is one). The
local is `tap` now; any new engine local wants a name that is not an engine static.
The `sintered` cart plays the engine and keeps the render behind E.

**Row 7, checked before building it:** acidcandy already carries probability, p-locks and trig
conditions throughout, and morphbox has p-locks and probability. The one Choochootracker sequencer
idea none of the racks has is PER-TRACK PLAYBACK SPEED. Row 7 is that feature now, nothing more.

**Built as `slipstep` (2026-09-28):** five loops, each with its own LENGTH (3..16, polymeter) and
SPEED ratio (1/2 2/3 3/4 1 5/4 4/3 3/2 2, polyrhythm), voiced on the three new engines (SINTER kick +
snare, METAL hat, MME bass) plus a pluck lead. Timing is the point: each track keeps an anchor and
its k-th step lands at `t0 + k × step × den/num` exactly, read against the audio beat clock and
queued 40 ms ahead with `schedule_hit`, so an odd ratio never snaps to a frame. A speed or length
change re-anchors at the next pending step (no skip, no double). Measured on soloed renders with an
energy-rise onset detector: every gap lands on its own track's grid within 1 ms (the detector's
window), the 3/2 hat included. The footer counts the bars until every track is back on step 1
together, the LCM of the rational periods (`lcm(numerators) / gcd(denominators)`); the default five
meet every 105 bars. `spec()` = 26. The same chip on acidcandy is its own design pass (its steps are
p-locked, so a slowed track must still read its locks per step), noted in the cart's todo.

## Row 6, as built: the `specimens` cart (2026-09-28)

Reading the Plaits-Alt headers split the ~60 engines in two: about half are Braids ports (CSAW,
FOLD, VOWL, the Braids kick/snare/cymbal/bell/pluck…), which our own engines mostly cover, and half
are ALGORITHMIC originals by Lyle Mills, sounds made by a process you could watch. `specimens` ports
TWELVE techniques the repo had no version of (checked by grep, and every hit read) to cart-land C
from `plaits_alt/dsp/engine2/*_engine.cc` (MIT), on two rows of tabs:

- **first six:** phase_flock (seven Kuramoto-coupled oscillators), rulefield (a 1-D cellular
  automaton whose row is the wavecycle), scanned (a 32-mass spring ring read as a wavetable), gendy
  (Xenakis' dynamic stochastic synthesis), attractor (a Thomas cyclically symmetric flow), bytebeat.
- **second six:** pulsar (Roads' pulsar synthesis), wave_terrain (an orbit over a 2-D surface, the
  five analytic terrains), spectral_spiral (frequency-shift feedback round a 32-sample complex loop),
  lockstep (a phase-locked loop chasing the note at a ratio), vosim (Kaegi/Tempelaars via Braids,
  with Braids' bell window table) and glisson (chirping grains).

Each note renders into a PCM slot (the mme / sintered route) and also writes a snapshot of the
algorithm's STATE sixty times a second; the picture draws the snapshot at the playback moment, so
what you see is the process making what you hear. Waveform pictures (pulsar, vosim) start their
capture on a cycle so they hold still.

`spec()` = 175, and it asserts the physics each picture claims, not only that sound comes out:
full coupling syncs the flock (r = 1.00), rule 90 is XOR and 204 the identity, a damped scanned ring
keeps 4% of its energy vs 225% (measured on the masses: that knob is damping AND a wavefolder
upstream, and the fold raises the output), chaos widens the attractor's orbit (0.01 → 0.66), a short
pulsar duty is 94% silence vs 5%, a stationary spiral loop is periodic and a shifting one is not,
the PLL locks to zero phase error with its follower at 1.0000× while a narrow loop is still at
0.59×, VOSIM's pedestal comes back out (mean -0.004), glisson's knob picks the chirp direction.
Every knob of every specimen is asserted to reach the sound.

Changes from upstream: per-note LCGs for `Random::GetFloat()` (gendy, glisson: repeatable per
pitch), scanned always triggered, terrain at 1× instead of 2× oversampling (22.5 ms per 6 s render
at 2×, over a frame; now 11.6), and every render ends in a DC blocker, 3/40 ms fades and peak
normalisation. **Plaits-Alt's `vowel_fof` is left out on purpose:** despite the name it is five
resonant filters on a saw, the same idea as `INSTR_VOICE`, so glisson took its tab. A full 6 s
render costs 2..13.5 ms here (lockstep the dearest), inside one frame on a Mac. Still unported and
absent: undertow, tapfield, phase_weave, loopback, sideband (DSF), question_mark (Morse), Braids'
digital filters, twin-peaks / clocked / particle noise.

## Kept out, and why

- **Braids / Plaits / Plaits-Alt / stmlib code** (~127k lines C++): the discipline is already ours
  (ADR-0017); the code cannot enter the libtcc live backend; most engines have a counterpart in
  `INSTR_*` (modal, string, speech, bass drum, snare, hi-hat, FM, wavetable, chord, additive).
- **open303** (Robin Schmidt, MIT, ~7.6k lines, double precision on libm): duplicates `acid303.h`,
  and doubles + libm transcendentals fail [`determinism.md`](determinism.md). Keep as an A/B REFERENCE the way
  `tools/ref-render/` uses STK: its `rosic_TeeBeeFilter` is a second opinion on our
  `FILTER_DIODE`, not a replacement.
- **Clouds reverb** (`external/mutable/clouds/dsp/fx/reverb.h`, 183 lines): ours ships; at most a
  reference render to A/B `reverb()` against.
- **Multimode filter** (`synth/multimode_filter.cpp`): the standard trapezoidal SVF we already run;
  its "character" modes are tanh drive with output feedback.
- **SCWF dual wavetable + granular sample voice**: covered by `wave_set()`, `FX_GRAINS` and the
  PSOLA work.
- **ayumi** (AY-3-8910 emulator, 540 lines, MIT): a chip we have never wanted; noted only so nobody
  re-evaluates it.

## Licence

Everything here is MIT (ChipNomad, paiheulevrai, Émilie Gillet, Lyle Mills, Robin Schmidt). One
file under `plaits_alt/test/` carries a GPL header; it is a test, not shipped, and not on this
list. Borrow WITH attribution in the header of whatever we write, the way `BOW_BODY_HZ` credits
STK.

## Row 1, as built: the `mme` cart (2026-09-27)

Cart first, engine later. The engine has no per-sample hook for cart code, but a cart can render
its own audio and hand it to a PCM slot: every key press renders 2 s of the voice at the pressed
pitch in cart-land C (`tools/carts/mme.c`), `sample_load()`s it into one of six slots
(round-robin) and binds the matching `INSTR_SAMPLE` instrument at root = that note, so nothing is
resampled. The engine's ADSR gates the release. Seven models, five wave pairs, five knobs, a
keybed, an autoplay walk that steps the models, and a `spec()` of 55 assertions (every model
finite + audible + a different sound from every other, every knob reaches the DSP, a patch renders
byte-identical twice, max feedback stays turbulent and DC-free, the panel keys). Limits, by
construction: a held note cannot ride a knob (the next note re-renders) and the per-note peak
normalisation hides how much louder ring is than vocode.

Three things the port MEASURED that a read of the upstream file does not show:

- **Upstream's fold is asymmetric.** `fabsf(fmodf(x + 1, 4) - 2) - 1` keeps the sign of a
  negative input, so negative lobes fold to large positive values and the shaper then pins both
  halves of a square pair to the same rail: the fold model at the sq/sq pair rendered a FLAT LINE
  at every feedback setting (ac rms 0.003). The cart uses the symmetric floor-modulo fold, which
  is what a triangle folder means. Worth sending upstream.
- **The feedback state is DC-blocked, the output is not.** A folder at full feedback carries about
  0.2 of DC; the cart adds a 10 Hz output blocker so the sample engine does not thump it.
- **click-check flags the cross/vpm models and is wrong to.** 64 events on a 4 s take, but the
  largest sample step is 0.057 inside a smooth slope and the ring model has no step above 0.02: a
  steep phase-modulated slope against a quiet local step-rms, the tool's documented false-positive
  shape. Read the step dump before believing the count on this voice.

## The engine port, as built (2026-09-27, same day)

`INSTR_MME` in `sound.h`, written from the cart's `mme_render()` with the 3-macro surface
(ADR-0017): harmonics = model (7 detents), timbre = amount, morph = flow, all live on a held
note; `MODE_MME_FEEDBACK / SHAPER / PAIR / INTERVAL` on the aux channel, read at note-on. Osc A
rides the engine's own phase accumulator, so glide, LFO and pitch bend work by construction. The
`mme` cart now plays the engine and keeps the sample-slot prototype behind E as the reference
A/B, the way `modal` keeps both macro mappings live. Gates run: `lint-aux-params`, the 900-frame
soundcheck, `spec` (59), `tune-check` (A2..A5 within 3.4¢), a per-model level sweep (the
`MME_TRIM` table lands all seven at -14 dBFS peak, the engine's single-voice baseline).

Three things the port found in the TOOLS, not the voice, all fixed the next day: tune-check,
dc-check and level-check each carried a hardcoded frame budget sized for 14 sweep entries, so
everything past MODAL had been silently truncated (the PIANO differential pass, FM4, MME; now
4500 frames each, and the comments say to count `ENGINES[]`). The first full dc-check sweep found
`INSTR_FM4` at -31 dBFS of DC at A5, which a 10 Hz output blocker (the one MODAL/MME/EPIANO carry)
took to -62. The level baseline is re-blessed at 68 notes: the one real drift it absorbed, BOWED
A5 +2.1 dB peak, is the 2026-08-25 bow-friction fix landing after the 2026-08-15 baseline.

Still open on row 1: the ear pass on `MME_TRIM` against the reference, and a few named presets
(the recipe table in [`instrument-recipes.md`](../guides/instrument-recipes.md) is the start).
