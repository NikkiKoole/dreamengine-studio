# Choochootracker: what to borrow

**STATUS: PROPOSED (2026-09-27)** — a ranked borrow list read off one repo, nothing ported yet.
Top of the list is the MME voice. Each row names the upstream file, what it would become here,
and why it made or missed the cut. Update the row (not this line) when something lands.

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

## How to port #1 (when it happens)

1. Read `mme_voice.cpp` once, then write it fresh in C inside `sound.h` as an engine with the
   3-macro surface (ADR-0017); do not transliterate the class.
2. `ab-render.js` on a probe cart to prove each of the seven models reaches the DSP (a model
   switch that renders byte-identical audio is the bug that tool exists for).
3. `tune-check.js --quiet` (it is pitched), `click-check.js` on a model sweep, `level-check.js`.
4. Register the aux params through `lint-aux-params.js` (five places must agree).
5. A cart that plays it, per the "ship a cart that exercises it" rule; recipe into
   [`instrument-recipes.md`](../guides/instrument-recipes.md).
