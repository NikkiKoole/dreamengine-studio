# Morphdrum — the 808 and the 909 are two points in one knob

STATUS: SHIPPED (2026-07-19, design → cart same session; 2026-09-28 the hat moved onto the dedicated `INSTR_METAL` engine, see "Seam — the hat") — **`runtime/morphdrum.h` + `morphbox.c`**:
a three-voice morphing drum synth (KICK/SNARE/HAT) where a per-voice CHARACTER knob slides the
808↔909 *structure* and a deep panel of absolute knobs carves the sound past both machines; a MORPH
PAD (CHAR × TUNE, with the 808/909 landmarks drawn on it) makes the parameter space visible and
playable; closed/open hats with choke.

## The honest core

Read `tr808.h` and `tr909.h` side by side and the kick is the *same synthesis structure* in both —
`INSTR_SINE` body → lowpass → pitch-env punch → drive — only the numbers differ (808: cut 250, sweep
+26st/50ms, dec 500, drive 0.28; 909: cut 380, sweep +30st/35ms, dec 320, drive 0.35, plus a noise
click). The snare is the same two-sine-modes-plus-highpassed-noise in both; ditto toms. **So an 808
and a 909 aren't two engines — they are two coordinates in one parameter space.**

`morphdrum.h` expresses that space directly: each voice is ONE parametric model. `CHARACTER` (0=808 …
1=909) morphs the *structural* voicing — the parts that don't get their own knob (kick click
brightness, snare noise-mode shape, hat FM-metal floor) — while `TUNE / DECAY / PUNCH / DRIVE / CUT /
SUB / …` are **absolute** controls with ranges that reach *well past* both machines (sub-deep tune,
endless boom, distortion). 808 and 909 become two spots you can dial to, and you can drive beyond
either. This is the [`acid303.h`](../../runtime/acid303.h) move — "both voicings, one header, chosen
by data" — applied to drums, with the whole synth exposed as knobs.

## The melodic + pad voices (PLUCK · UPRIGHT · PAD)

The bank isn't only drums: two melodic voices ride the same parametric-voice + p-lock machinery.
**PLUCK** (`INSTR_PLUCK`) is a bright synth bass/lead; **UPRIGHT** (`INSTR_BOWED` in `MODE_BOW_PIZZ`,
the double-bass pizz lifted from `walkroll`/`walkbox`) is the woody acoustic bass. For both, CHAR
morphs **material** (dark round → bright metallic / bow position) rather than 808↔909 — the general
thesis holds (a voice is a parameter space; CHAR travels its character axis; 808↔909 is just the drum
instance). Their TUNE snaps to a **minor pentatonic** over ~3 octaves (+ an OCT knob) so per-step TUNE
*p-locks* always land musical — you draw a bassline by selecting steps in LOCK mode and pitching them.
This is where the step-first p-locks pay off most: the same feature that morphs a kick per-hit writes
a melody on the bass. (The upright is why the standalone `walkroll` piano-roll experiment can stay an
experiment — the instrument in this box, played with p-locks, covers the same ground.)

A 6th voice is a **PAD** — the lush detuned-saw from `groovebox` (`INSTR_SAW`, slow SWEL attack, long
REL fade, unison WIDE detune). Unlike the one-shot voices it fires a **CHORD**: a pentatonic triad
(scale-thirds *i, i+2, i+4*, so always in-key) that swells and overlaps into a wash. It's the one pad
that earns its place here because the box already has the **PUMP** keyed off the kick — so the pad
breathes against the four-floor exactly like groovebox's sidechained pad. CHAR morphs warm↔bright
(filter). A drone/held instrument like the grenadier filterbank would NOT fit (it wants continuous XY
sweep, not per-step triggers — that lives in `dubjam`); a gated chord-pad is the step-friendly way to
get "something long underneath."

## Where it sits on the shelf

- **NOT [`drumkit.h`](../../runtime/drumkit.h)** — that's a pad *map* over the engine's built-in roles.
- **NOT `tr808.h` / `tr909.h`** — those are two *fixed, faithful* recipes (and stay that way; the
  [`acidrack`](rebirth-classic.md) / `acidcandy` carts want the real machines, byte-honest).
- **`morphdrum.h` is the CONTINUUM between and beyond them**, turned into knobs. It complements
  `acidcandy` (which keeps 808 and 909 as separate *faces* you switch between) — this is the one
  voice you morph.

## The cart — `morphbox`

Reflows with the canvas: a **roomy 320×200** by default (FONT_NORMAL, a title, breathing room) and a
**compact 160×100** pocket face when run smaller — one source, `layout()` reads `screen_w()` and picks
(same structure both ways; no camera scale, so touch stays 1:1). Tap a voice name (left of the grid,
doubling as the voice selector) to focus it;
the two knob rows up top edit that voice's panel. The **MORPH PAD** on the left is the headline: X =
CHAR with `808`/`909` marked, Y = TUNE, a puck you drag to travel the lineage (and past it) live. A
self-playing 3-row step grid; SWG shuffles the whole grid; PUMP is a real summed-bus sidechain keyed
off the kick. Hat cells cycle off → closed → open (ringed) → off, with the open hat choked by the
next closed one.

**Per-step automation — two idioms** (a MODE button cycles STEP / PROB / VEL / LOCK). The design
language for this, from hardware grooveboxes, is two *orthogonal* models: **parameter-first**
(automation lanes — pick a param, draw it across all steps) and **step-first** (Elektron *parameter
locks* — select a STEP, edit many of its params on the controls you already have). morphbox uses
both:

- **PROB / VEL = contour lanes** (parameter-first): the two rhythmic step-attributes with no single
  voice knob, as pull-down bars you sweep across the row. PROB gates the hit (`rnd`), VEL sets its
  velocity.
- **LOCK = step-first p-locks**: tap a step to select it and the **knob panel + MORPH pad become
  that step's editor** — turning a knob pins that param to an absolute value for the step (a dot
  marks locked knobs + locked steps). At fire time the locked params are swapped in + re-`morph_apply`
  (they reshape the instrument), fired, then restored. So one hit can sit anywhere in the 808↔909
  space with its own cutoff/decay/drive/sub. This replaced an earlier page-per-param design — the key
  insight (from [Elektron p-locks](https://forum.loopypro.com/discussion/26340/parameter-locking-what-is-it-exactly))
  is that select-a-step reuses the existing controls, so it scales to *every* param with no new pages.

The grid's finger pool uses `pointer.h` (never hand-roll it — the negative synthetic-mouse id
collides with a `-1` free marker; `lint-carts` guards this now).

Recipe entry (the synthesis params + ranges): [`instrument-recipes.md`](../guides/instrument-recipes.md).

## Seam — the hat

An 808 hat is a six-square metal bank; a 909 hat is FM-clang. Both are one bright, highpassed,
ringing metal source, so `morphdrum.h` models the hat as ONE `INSTR_FM` voice and gives it timbre /
morph / cutoff knobs — the honest continuum, *not* byte-equal to either machine's hat. The other two
voices share their oscillator structure across the pair outright.

**2026-09-28: the hat sits on `INSTR_METAL` now, and that settles the header's story.** Morphdrum
built every voice in cart-land out of generic primitives (`INSTR_SINE` bodies, `INSTR_FM` for the
hat, noise) because that was the only way to make one parametric model per voice when it was
written. `INSTR_METAL` ([`choochootracker-borrow-list.md`](choochootracker-borrow-list.md) rows 2+3,
the `bogie` cart) is the first dedicated engine one of these voices can sit on, and the hat is the
voice whose old model was weakest: an 808 hat IS a six-square bank through a highpass with one decay
(measured on `tr808.h`: its brightness never changes across the tail, a chord fading), and a 909 hat
was a SAMPLED real cymbal, a bank of modes whose highs die first. Same structure, different
lifetimes. So the hat is one `INSTR_METAL` voice and **CHAR is the stagger direction**: 808 = one
lifetime for all six (the chord) → 909 = the lows live longest (the tail darkens like a real
cymbal's). TONE runs the ratio table hat → cymbal, METL is bank-vs-noise, TUNE moves the bank's base
(52..76, the 808's members sit at 205..800 Hz). Same ten knobs, and the continuum between the
machines is a real parameter path rather than a crossfade. The thesis (one parametric model per
voice) holds; what changed is that the model rides a dedicated engine where one exists and generic
primitives where none does yet (kick and snare still are).

The FM-clang hat stays reachable behind `MD_HAT_ENGINE` for the ear pass:
`node tools/ab-render.js morphbox --file runtime/morphdrum.h --set MD_HAT_ENGINE=INSTR_METAL,INSTR_FM`
(add `--play-arg --solo-slot --play-arg 14,15` for the hat slots alone). Measured on morphbox's own
pattern, solo hats: FM -7.2 dBFS peak, METAL -8.2 at unity level (`instrument_level` only cuts, so
that last dB is `MD_HAT_METAL_LEVEL`'s ceiling). On the FULL mix the metal hat reads darker
(brightness 0.17 vs 0.33, centroid 7.5 vs 8.0 kHz): the FM clang carried more energy above 4 kHz than
a square bank through the same highpass does. That is character, not a bug, and TONE / CUT reach it;
it is the first thing the ear pass should decide. Kept: the FM path is small and the A/B is the point.

## Open / next

- **Ear pass on the new hat** (2026-09-28): the hat is on `INSTR_METAL`, FM behind `MD_HAT_ENGINE`.
  Play morphbox's pattern with CHAR swept; if the 909 end wants more sizzle, its stagger endpoint
  (`0.22` in `md__resolve`) and the METL → noise map are the two numbers to move. Then drop the FM
  path or keep it as the A/B, either is fine.
- **Kick and snare** stay on generic primitives; a dedicated kick engine (the bridged-T + boom) would
  be the next seam if one ever lands, same move.
- Widen any knob ranges that feel cramped once it's been played hard (ear-tuned, not measured).
- A 4th/5th voice (CLAP, TOM) is a straightforward addition — the earlier 5-voice cut had them; they
  were dropped to give KICK/SNARE/HAT deep panels. Add back as their own parametric models if wanted.
- Reflow shipped as **two discrete modes** (compact 160×100 ↔ roomy 320×200, chosen by `screen_w()`);
  fully-fluid interpolation between arbitrary sizes is still possible if wanted.
- A/B the CHAR endpoints against the real `tr808`/`tr909` voices (`wav-correlate`) if byte-closeness
  to the machines is ever wanted at the extremes — the current voicing is tuned by ear for feel, not
  measured against them.
