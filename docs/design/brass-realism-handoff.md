# BRASS realism — handoff (2026-06-12; brightness fix landed 2026-06-16)

STATUS: SHIPPED — brass-engine tuning handoff; asymmetric shaper + level-coupled brightness + output high-shelf fixes landed.

> Session map for the whole waveguide-engine arc (down-bend + brass + the open tuning thread):
> [`waveguide-bend-handoff.md`](waveguide-bend-handoff.md).

> **Outside-in audit (2026-07-28): this engine has now been read against Synth Secrets Parts 24-27**
> (the two brass theory chapters plus the Minimoog and SH-101/Axxe patch walkthroughs) —
> [`synth-secrets-audit.md`](synth-secrets-audit.md) §E, with measurements and a 10-step order.
> Nothing there contradicts this doc; it is additive. The headlines, in the order §E ranks them:
>
> - **§E9 read this first: our brass measurements are not trustworthy yet.** `harmonic-spec.js`
>   analyses a 372 ms window, which spans ~2 cycles of the engine's always-on 5.4 Hz lip vibrato;
>   FM smears high harmonics more than low ones, so every harmonic figure (including this doc's) is a
>   lower bound. It can't be controlled for, because the vibrato has a floor of 0.08 and no defeat.
>   Separately, **the h9→h17 headline below does not reproduce** from `brasspec`'s committed defaults:
>   I measure h9 / 1.7% at timbre 0.80 and h23 / 2.2% at timbre 1.00. Checked for a regression and
>   found none (no brass commit since `8dfd12a`; the only later change is a uniform gain trim). So it
>   was measured at an unrecorded macro position and needs pinning.
> - **§E4, a structural miss this doc has not considered.** Reid's patch sets the amp attack to 100 ms
>   and the *brightness* attack to 600 ms, and cites researchers who hold that the differing
>   development rates of the harmonics are "the most important audible clue" to an instrument's
>   identity. Our `br_env` is a level *follower* with a 14 ms time constant, so brightness arrives with
>   the amplitude: measured at 100 ms resolution the note is fully bright in the first window. This is
>   a credible answer to this doc's standing "very obviously not real brass."
> - **§E5 gives fix #3 a number.** Part 24 Figure 8: on an overblown note the **8th harmonic is
>   dominant**. Measured at timbre 1.00, h1 is still loudest and h7 (-3.3 dB) is the strongest
>   overtone, with evens still suppressed (h2 -31.3, h4 -21.9). Nothing in the engine trades the
>   fundamental away as blow rises. Pass/fail for "model the bell natively": h8 ≥ h1 at forte.
> - **§E2/E3/E6/E7/E8**: vibrato never delayed and never defeatable (Reid: it "does not occur during
>   the transient stage"); no onset rasp (his is an ~80 Hz triangle into the *filter*, AD-gated to
>   ~50 ms, explicitly **not** into oscillator pitch — that would be FM and "would destroy the
>   timbre"); the brassiness macro does most of its work in the last fifth of its travel; `eng_p`'s
>   sub-oscillator "weight" is wired for GUITAR/PIANO but not brass, which the SH-101 Tuba patch says
>   low brass needs; partials aren't stretched sharp.
> - **§E10, and it's cart-side and free:** `brass.c` uses amp attack **1 ms** and release **1200 ms**
>   where Reid uses 100 ms and effectively instantaneous ("a real brass sound ends very rapidly once
>   you stop blowing"). Caveat: our release also governs the bore ring-down, so A/B rather than edit.
>
> Also relevant from the architecture pass: **§C6**, Part 8's argument that ADSR structurally cannot
> make a brass contour — the swelled/spit shape needs an attack *level* below maximum (Alpha Juno
> `L1`), and our mod envelopes are AD with a single amount, hitting Reid's limitations 3 and 5 exactly.
> Nothing above is queued.

Working note for whoever picks up the BRASS engine next. The complaint that started this:
*"my brass synth isn't really sounding very brassy"* — and, after a first fix, *"it's very
obviously not real brass, whereas the reed section is a lot closer."* That judgment is correct.
This documents what was measured, what was changed, and what the **real** fix still needs.

## ✅ STATUS UPDATE (2026-06-16): fixes #1+#2 SHIPPED

The asymmetric shaper + DC blocker (below) **and** the level-coupled brightness work
("What the REAL fix needs" #1 + #2) are now committed — see commit **`8dfd12a`**. On the OUTPUT
stage (the bore loop destabilizes if touched): a bore-amplitude follower → a 0..1 level, the
asymmetric shaper steepening with level, and a level+brassiness high-shelf lifting energy past
~4kHz. Measured (forte trumpet A3): highest harmonic within 20dB **h9→h17** (~2.0→3.7kHz), energy
>4kHz **0.2%→2.3%**, crest **6.3→14.6dB**. DC-check + tune-check clean (tuning untouched). **Still
open:** fix #3 (model the bell to fill the harmonic series natively) and the deferred **mute/plunger
axis** — the most aggressive bite. Tracked in [audio-notes.md](audio-notes.md) §19 + STATUS. The
diagnosis + remaining-work sections below are kept verbatim as the as-built record.

## ⚠️ ORIGINAL STATE NOTE (2026-06-12): there was UNCOMMITTED work in the tree

(Historical — the items below were committed in `8dfd12a`.) If you're starting fresh, `git status` / `git diff` first.
Changes made this session, all isolated to these files (do NOT sweep in the other pre-existing
dirty files — docs/groovebox/etc. belong to other work):

1. **`runtime/sound.h`** — `sound_brass_sample()` + the brass Voice fields. Two edits:
   - The output brassiness shaper was made **asymmetric** (was `tanhf(voiced*driveOut)`, a pure
     odd nonlinearity). Now: `shaped = voiced*driveOut; asym = bright*0.7f;
     blaat = tanhf(shaped + asym) - tanhf(asym);` — the bias fills in EVEN harmonics, scaled by
     brassiness; subtracting `tanh(asym)` keeps it from adding standing DC.
   - Added an **output DC blocker** (the asymmetric shaper injects DC). New Voice fields
     `br_out_prev, br_out_state` (declared next to `br_dc_prev/state`, zeroed in
     `sound_brass_start`), one-pole `bdc = blaat - br_out_prev + 0.995f*br_out_state` applied to
     `blaat` just before the makeup-gain return.
2. **`tools/dc-check.js`** (NEW) — the DC tripwire (see below). Standalone, mirrors tune-check.js.
3. **`.githooks/pre-commit`** — added a reminder line pointing at `dc-check.js` for shaper/
   nonlinearity edits (next to the existing tune-check reminder).

If committing: explicit pathspec only —
`git add runtime/sound.h tools/dc-check.js .githooks/pre-commit && git commit -m "…" -- runtime/sound.h tools/dc-check.js .githooks/pre-commit`
After committing sound.h, confirm it survived a parallel clobber:
`git show HEAD:runtime/sound.h | grep br_out_state` (must print).

## The diagnosis (measured, not guessed)

Rendered the `brass` cart fanfare (trombone preset, ~329 Hz) to WAV and pulled the harmonic
series (a throwaway DFT script; reproduce with any FFT over a 16k Hann window in the sustained
middle). Key numbers, dB relative to the fundamental:

```
                  h2    h3    h4    h5    h6    h7    h9   h11   ...rolloff
BRASS (original) -45   -5   -39  -14   -51  -24   -26  -34    dead by h12 (~3.5kHz)
BRASS (asym fix) -25   -5   -17  -13   -30  -24   -26  -34    still dead by h12
REED (clarinet)  -60   -4   -70   -2   -64   -8   -15  -12    STRONG out past 5kHz
```

Two findings:

1. **Original brass was odd-harmonics-only** = a stopped-cylinder / clarinet spectrum (hollow,
   "where's the brass"). Cause: a symmetric `tanh` shaper can only make odd harmonics. The asym
   fix above brought the even harmonics up +13..+22 dB — necessary, and it does sound more brass,
   but NOT sufficient.

2. **The deeper reason it still doesn't convince** — two structural problems:
   - **It's a clarinet core wearing a brass filter.** BRASS borrows REED's McIntyre/Schumacher
     pressure-valve + cylindrical bore (the code comment even says "brass borrows it"), then fakes
     the trumpet bell with a fixed output bandpass (`fmtHz` ~900–1600 Hz) + the shaper. REED sounds
     real *because that core IS correct clarinet physics* (a clarinet genuinely is odd-dominant).
     Brass's real acoustics — the flaring bell that reshapes the bore modes into a near-complete
     integer series and radiates highs, plus nonlinear shock-wave brightening in the bore — are not
     modeled; they're approximated downstream.
   - **It rolls off ~an octave too early and has no loud→bright coupling.** Brass is the BRIGHTEST
     of the lot at forte — the "blat" is a shock wave with energy screaming to 8–10 kHz, and that
     brightness is *dynamically coupled* (pp = nearly a sine, ff = blazing). Our brass dies by
     ~3.5 kHz and sits at a perpetual dull mezzo-piano. The bell-radiation lowpass
     (`lpCoeff = 0.85 - dark*0.35`) is actively killing the highs the shaper makes.

## What the REAL fix needs (next session's actual work)

> **Update 2026-06-16: #1 and #2 below are SHIPPED (`8dfd12a`); #3 remains.** See the status
> banner at the top for the as-built approach and measured numbers.

Priority order, biggest perceptual payoff first:

1. ✅ **Dynamically-coupled brightness (the shock wave). — DONE 2026-06-16.** The defining brass cue. Brightness must
   RISE with playing level / breath pressure, and extend much higher (to 5–10 kHz at forte). Today
   `timbre` is a mostly-static output drive. Real brass steepens the wave *in the bore* as pressure
   climbs. Cheapest credible approximation: drive the harmonic-generation (and back off the bell LP)
   as a function of instantaneous bore amplitude / `Pm`, not just the static `timbre` macro. Make
   `lpCoeff` open up with level instead of being fixed.
2. ✅ **Stop killing the highs. — DONE 2026-06-16.** Re-examine the bell-radiation LP and the makeup chain — the spectrum
   shouldn't be ~gone by h12. Reed keeps strong partials past h15; brass should too (more so at ff).
   (Shipped as a level-coupled output high-shelf rather than touching the in-loop bell LP, which
   destabilizes the oscillator — see the status banner.)
3. **Bell that fills the series, not a bandpass painted on top. — STILL OPEN.** Longer shot: model the bell as a
   frequency-dependent reflection/radiation at the open end so the bore modes land on a complete
   integer series naturally, rather than synthesizing even harmonics with a downstream shaper. This
   is the "do it right" path; (1)+(2) are the high-ROI path to try first.

### How to work it / verify (tooling is all here)

- **Render + listen:** `node tools/play.js brass script /dev/null --headless --frames 360 --wav /tmp/b.wav --det`
  then `node tools/wav-analyze.js /tmp/b.wav` (peak/rms/**crest** — brass wants a HIGH crest, peaky).
- **Harmonic spectrum:** reuse the DFT approach above; compare against REED (`node tools/play.js reed …`)
  as the "this is what a convincing modeled wind voice looks like" reference. Watch the high-harmonic
  rolloff and how it changes when you push `timbre`/breath.
- **A/B a real reference:** BRASS is NOT a navkit port (it's from STK's BrassInstrument), so there's
  no golden WAV. But navkit's `tools/preset_audition.c` can render *its* brass-ish voices if you want
  an external ear — see CLAUDE.md "A/B against navkit". Or grab any real trumpet sample and FFT it to
  see the true forte rolloff (energy to ~8kHz).
- **Tuning:** after any bore/delay change run `node tools/tune-check.js --quiet`. KNOWN pre-existing
  residual: BRASS A5 reads ≈ −25¢ (integer-delay quantization, audio-notes §18) — not introduced by
  this work, leave it unless you're explicitly fixing tuning.
- **DC:** after any shaper/nonlinearity change run `node tools/dc-check.js` (see below).
- **Compile gate (sound.h is a hot shared file):** `node tools/play.js soundcheck script /dev/null
  --headless --frames 2` must print `compiling… ok` with no `[sound] WARNING`. Re-Read the brass
  region immediately before editing (line numbers drift — other agents edit this file).

## Side-deliverable shipped this session: `tools/dc-check.js`

A DC tripwire, sibling to tune-check.js. Renders the `tunecheck` sweep, segments per-note via the
trace, measures each engine's output **mean (DC)**, flags any past tolerance (warn −48 dBFS, bad
−36 dBFS). `--quiet` = CI gate (exit 1). Validated: all 13 engines pass today (worst BRASS −59 dBFS);
and it genuinely trips (disabling the brass blocker → `✗ BRASS −24 dBFS`, exit 1).

WHY it exists: dreamengine removes DC **at the source** (each engine blocks its own — blown models
carry large pressure DC; asymmetric shapers inject it). There is deliberately **no master DC blocker**
(it would eat headroom vs the master soft-clip, click on note-on under the amp envelope, and break
byte-reproducibility for old carts). That scheme rests on author discipline — every new engine/shaper
must add its own blocker. dc-check is the backstop for a forgotten one. The brass asym-shaper above is
exactly the failure mode it guards (it injected −0.02 DC until the output blocker was added).

## One-line summary for the impatient

Asymmetric shaper fixed the missing even harmonics, then level-coupled brightness + an output
high-shelf extended the highs (both shipped, `8dfd12a`) — much brassier. **But the owner still hears
room for more (2026-06-16): keep pushing.** The brightness was tuned conservatively (stops ~2.3%
energy >4kHz to dodge fizz; realistic forte is ~8–10kHz), so the coefficients can go harder; and the
**deferred mute axis** + modelling the bell to fill the series natively (fix #3) are the bigger,
unshipped levers. Reed sounds right because its simple core is genuinely correct clarinet physics;
brass is still faking the bell downstream.
