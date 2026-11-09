# Piano engine (INSTR_PIANO / StifKarp) — why it sounds like a harp, and the fix list

> **New (2026-11-09): a real Steinway.** The grand matched against a Steinway D (`MODE_PIANO_MODEL`), then a
> HYBRID: the real recording, fading into the model at 2.6 s (`instrument_piano_attack`, baked in by
> `steinway.h`; play it in `grandkeys`). See [the Steinway match and the hybrid](#the-steinway-match-and-the-hybrid-2026-11-09).

STATUS: SHIPPED (2026-06-25) — the full fix roadmap landed (two-rate decay, hammer knock, velocity→timbre, per-voicing decay + dulcimer unison, stretched tuning); A/B crest 21.6→25.9 dB, onset brightness 0.10→0.25. Only optional polish remains (ear-tune the per-voicing baselines + PIANO_STRETCH_K; promote the eng_p indices to public MODE_PIANO_* constants).

> **✅ Outside-in pass DONE (2026-07-28): Synth Secrets Parts 42-45** (SOS Oct 2002 to Jan 2003 — the
> physics chapter plus a three-part Roland JX10 build) read against this engine. Full write-up with a
> measurement and an 8-step order: [`synth-secrets-audit.md`](synth-secrets-audit.md) §I. **This came out
> the best-matched engine in the whole audit**, so the headline is a confirmation rather than a gap:
>
> - **The hammer comb is the INVERSE of the pluck comb, and we get it right.** Part 42: "Whereas the
>   position at which a guitar string is plucked determines its maximum displacement, **the piano hammer
>   remains in contact with the string long enough to ensure that the position at which the string is
>   struck is a node of zero displacement**." So the excluded harmonics are the complementary set. Our
>   `sound_piano_start` uses an **averaging** comb `(tmp[i] + tmp[i+ps])·0.5` where `PLUCK`/`GUITAR` use a
>   **differencing** one — and at `ps = len/2` the averaging comb nulls n = 1, 3, 5, 7…, i.e. the
>   fundamental and every odd harmonic, which is Reid's sentence verbatim. The navkit port got a subtle
>   thing right. **The sign is load-bearing and nothing says so in the code** — worth a comment before
>   someone "unifies" the two combs and silently breaks the physics. That is step 1 of §I's order.
> - **Also confirmed:** stretched tuning matches Reid's own mechanism (track the keyboard at slightly
>   over 1:1, Figure 14) and his reason for it ("a perfectly tuned piano not only sounds out of tune, **it
>   sounds dull**"); register-dependent decay falls out of the delay line for free and was **measured**
>   (A1 still at 0.11 after 1 s, A4 gone); `pn_dd`, `pn_knock`, `pn_symp`, `pn_body[4]`, velocity→timbre
>   all have direct counterparts in Part 42. And the macro split is validated from an odd direction:
>   Part 45 says a piano note's brightness and loudness *cannot* change once sounded ("aftertouch … must
>   be set to zero"), so fixing hammer/voicing at note-on and giving the live axis to the pedal is
>   physically correct.
>
> **Real gaps §I found, in its suggested order** (none queued): hammer position is per-voicing where Reid
> says it varies **1/7 to 1/15 along the string across the keyboard** (§I2, one float); the top octave
> measured **gone in half a second** (A4 at 0.01 of peak by 500 ms), which is steeper than a real grand and
> may share a cause with §H8's guitar high-register loss (§I5); inharmonicity is fixed at note-on so it
> never grows with level (§I4, and see below); a **two-slot layered patch** is free and is Part 45's whole
> conclusion (§I9); peak level doesn't taper with pitch though Figure 9 wants both (§I6); and the
> tricord is capped at two strings, chosen per voicing, with no energy exchange between them (§I3).
>
> **One counter-intuitive finding worth reading in full (§I7): the bottom octave's fundamental should be
> WEAK.** Part 42: "for the lowest notes on a grand piano, **the fundamental pitch has very low
> amplitude**, and the note that you think you hear is to some extent implied by the harmonics. This
> suggests that we require a high-pass filter for the lowest notes." Our `eng_p[0]` sub-oscillator — the
> shipped "thin" cure — reinforces exactly the partial a real grand has least of. Not a call to remove it
> (the thin complaint was real and it worked), but it may be why the bass reads synthetic-round rather
> than piano-huge, and the honest version is register-dependent. A/B before believing either way.
>
> **Correction to this doc's own framing, from §H3:** `pn_dd`'s comment reads "The fast initial drop that
> says 'struck', not 'plucked harp'". Part 28 shows a *plucked guitar* has two-rate decay too, from the
> string's two polarisation planes; Part 42 gives the piano's own different cause, the pairs and tricords
> interacting so "the rate at which energy is transferred to the soundboard diminishes". So two-rate decay
> is common to struck *and* plucked, and the harp-vs-piano difference is in the proportions. The behaviour
> is right; the stated reason isn't. `GUITAR`/`PLUCK` should probably borrow it (§H3).

**Genre: design exploration / handoff.** The diagnosis + roadmap for the one engine in the
roster that never got past "nice but mediocre." Engine impl: `runtime/sound.h`
`sound_piano_start` / `sound_piano_sample` (search `INSTR_PIANO`). Showcase + tuning rig:
`tools/carts/piano.c`. Port lineage: navkit `processStifKarpOscillator`
(`~/Projects/navkit/soundsystem/engines/synth_oscillators.h`), which itself ports STK's StifKarp.

## TL;DR

It sounds like a **harp** because the model **is** a plucked stiff string. STK's StifKarp is a
Karplus-Strong *pluck* in a piano costume; navkit ports it faithfully (and is itself only
so-so here); we port navkit faithfully. So the mediocrity is inherited, not drift. To get
past "nice harp" you have to **add the things STK omits** — none are large.

## Why it reads as a harp, ranked by payoff

1. **No double decay — the single biggest tell.** A real piano string couples two vibration
   planes to the bridge → the signature *two-stage* decay: a fast initial drop (~100–300 ms)
   into a long, much quieter aftersound. Our loop has ONE damping coefficient
   (`effDamp = ksd · (0.992 + 0.008·damper)`, sound.h ~4091) → a *single pure exponential*
   ringdown, which is the textbook plucked-string envelope. Ears read single-exponential
   ringdown as "pluck/harp" almost unconditionally. navkit doesn't model this either.

2. **The onset is a pluck, and the one knock we have is OFF by default.** Excitation = a
   filtered, normalized noise burst through an **averaging pick-position comb**
   (`applyPickPosition`, sound.h ~4035) — verbatim the KS *pluck* excitation. There IS a
   percussive `eng_click` transient (sound.h ~4166) but it's gated behind `eng_p[1]`, which
   **defaults to 0** (sound.h ~6667) and is only set via `instrument_mode(MODE_STRING_CLICK)`.
   The piano cart never sets it → zero broadband hammer "thunk" at onset.

3. **The hammer is modeled as a pick.** A real hammer is a soft mass with finite contact time;
   harder/faster strikes get brighter AND noisier. Here `timb` is a static macro applied once at
   note-on with a fixed brightness "bloom" (sound.h ~4092) regardless of velocity → every note
   has identical spectral character (sampler-like, un-piano). No velocity→timbre coupling.

4. **Most voicings are single-string.** Only grand/bright carry a 2nd string and the detune is
   ~1 cent (`1.000694`). Harpsi/dulcimer/clav/celesta are single (`detune 1.0`) → no unison
   beating / phase double-decay → thinner, more monochrome = more harp.

5. **Near-lossless loop → the cart's gate shapes the note**, and the cart just plays a long
   exponential release (`instrument(I_PNO, INSTR_PIANO, 1,0,7,2000)` + `hit()`, piano.c) — again
   a pluck envelope.

6. **Tuning fights the inharmonicity (the Feynman point).**
   > ⚠ **MEASURED 2026-07-29 — the premise of this whole item is wrong in both halves.** We do **not**
   > make the partials inharmonic: `B = stiff²·0.015` yields a dispersion allpass coefficient of
   > 0.9999948, the identity, and the measured partials are harmonic to within 0.4¢ (fitted B ≈ 2e-6
   > against a real grand's ~1e-4). And the stretch that was later added to fix this (`PIANO_STRETCH_K`)
   > only worked in the **treble**: the bass half was cancelled a frame after note-on (`v->freq` was written
   > back, `v->freq_target` was not), so we shipped **half a Railsback curve** — sharp treble, ET bass. The two
   > defects hid each other. **The stretch half is FIXED (2026-07-30)**, and `tune-check` now asserts the
   > curve rather than tolerating it; the inert dispersion (§I4b) is still open. Evidence:
   > [`synth-secrets-plan.md` §2.3(a)](synth-secrets-plan.md#the-premise-failed-three-defects-found-by-measuring-first-2026-07-29).

   We make the partials inharmonic via
   the dispersion allpass chain (`B = stiff²·0.015`, sound.h ~4042 — this *is* our stiff-string
   inharmonicity) but tune the **fundamental** dead-on equal temperament. A real piano tunes the
   *fundamentals* **stretched** (the Railsback curve: bass flat, treble sharp) so the stretched
   partials of different notes agree. Skipping the stretch is part of why our dispersion reads as
   "metallic shimmer" rather than "piano" — stretch and tuning are fighting instead of reinforcing.
   - **Ref:** Feynman on stiff-string theory & piano tuning —
     https://physicstoday.aip.org/features/stiff-string-theory-richard-feynman-on-piano-tuning
     (canonical derivation of the inharmonicity↔stretched-tuning relationship).

## Fix roadmap — ALL SHIPPED (2026-06-25)

The whole roadmap landed. A/B on the same arp: crest 21.6→25.9 dB, onset brightness 0.10→0.25.
Engine code is in `sound_piano_start` / `piano_stretch_freq` (`runtime/sound.h`); the `piano` cart's
row-2 knobs (`decay`/`knock`/`velo`) tune it by ear.

1. ✅ **Two-rate decay** (`pn_dd`) — extra per-period loss at the strike, relaxes to 0 over ~0.2s →
   fast initial drop into a long tail. Per-voicing base (`PianoVoicing.dd`). *(#1, commit 9b35af30)*
2. ✅ **Hammer knock** (`pn_knock`) — broadband onset thump, ON by default, per-voicing base
   (`PianoVoicing.knock`). *(#2, 9b35af30)*
3. ✅ **Velocity → timbre** — strike velocity (`v->vol`) drives hammer brightness + knock, not just
   loudness (soft = dark/mellow, hard = bright/clangy). *(#3, afe623cf)*
4. ✅ **Per-voicing decay/knock + dulcimer unison** — each voicing its own `dd`/`knock`; dulcimer
   gets a detuned 2nd string (1.0015). *(#4, e486e95c)*
5. ✅ **Stretched tuning** (`piano_stretch_freq`) — Railsback curve (bass flat, treble sharp,
   `cents = K·oct·|oct|`) so the inharmonic partials agree across notes. SEAM: `PIANO_STRETCH_K` →
   `0.0f` disables (back to ET). Pitch-based, decoupled from the stiffness macro. *(#5)*

**Tunable knobs (eng_p) are PUBLIC constants** — `MODE_PIANO_DECAY`=2 and `MODE_PIANO_KNOCK`=3 went
through the four-places ritual and live in `studio.h` + `studioDocs.js` + `shell.js`. Per-voicing
`dd`/`knock` baselines are still reasoned defaults, not yet ear-tuned.

### Remaining polish (optional)
- Ear-tune the per-voicing `dd`/`knock` baselines and `PIANO_STRETCH_K`.
- More voicings could carry a detuned 2nd/3rd string (only grand/bright/dulcimer do now).

## The Steinway match and the hybrid (2026-11-09)

The grand measured against a real Steinway D (13 notes, C1 to C7 every 6 semitones, bitKlavier Grand, Daniel
Trueman, Princeton, CC BY 4.0, in [`assets/samples/steinway/`](../../assets/samples/steinway/README.md)), then
given a real attack. Ends in **`grandkeys`** (play it), **`openstrings`** (with the [resonator](sympathetic-resonator.md)) and the probe
**`pianoref`**. Recipes: [`instrument-recipes.md`](../guides/instrument-recipes.md) (piano/steinway rows).

**1. The model, matched (opt-in: `instrument_mode(slot, MODE_PIANO_MODEL, 1)`; 0 = the classic grand, bit-exact).**
Measured note by note with `pianoref` + `node tools/oneshot-compare.js --render pianoref --spec
tools/oneshot-specs/pianoref.json --frames 2760` and `inharm-spec --decay`: a felt hammer (a lopsided t·e^-t
pulse, contact time shorter up the keyboard and with a harder hammer) instead of a noise burst, decay set in
dB/s per note (the brightness blend was the dominant treble loss, so the loop gain is solved exactly), an HF
loss filter, a few-ms onset ramp, and the knock OFF at the knob centre (it was the onset click: >8 kHz energy
35-50 dB over the Steinway). Side find: the old grand's treble played 4-11¢ flat, the new one is within 0.8¢.
⚠ `tune-check` never tests the grand (it uses the harpsichord voicing and finds no pitch at A5): verify grand
tuning with `inharm-spec --voicing 0`. **By ear: the tail is good, the onset is not** ("farty", "a thump",
"missing the bell"). The onset is where a model loses, so:

**2. The hybrid (`instrument_piano_attack`).** A real recording per zone plays from the strike while the
model runs unheard beside it; at the splice the model is level-matched and an equal-power fade hands over.
- **Splice LATE: 2.6 s, 0.5 s fade.** The first build spliced where each attack's noise settles (measured
  under -20 dB between the partials: ~190 ms F#1, ~100 ms C2-F#2, ~50 ms C3-F#3, 20-40 ms from C4 up), with
  70-300 ms splices. Levels matched within ~1 dB and click-check was clean, and it was **plainly audible**:
  that early the real and modelled strings still differ in tone colour (brightness, beating, partial
  balance), and no level match fixes that. At 2.6 s nobody hears it ("sounds great"). Lesson: a seam is
  hidden by WHEN, not by matching.
- **A note above its recording reads it faster** (3 semitones up = 1.19x) and runs out before a 2.6 s +
  0.5 s hand-over. The first build silently dropped the zone (model only, half the keys); the engine now
  pulls that note's splice earlier to fit.

**3. Shipping it: `runtime/steinway.h`** (generated by `tools/gen-steinway.js`). Trimming was not an option:
the bass and middle are still only ~20 dB under their peak at 3.1 s. So the size is set by the encoding:
4-bit IMA ADPCM, 896 KB for all 13 (35-49 dB SNR, its noise follows the signal); 8-bit mu-law was worse at
twice the size (25-37 dB, the noise sits on the quiet tail). 16-bit would be 3.5 MB.

**4. Voicings without new samples (`grandkeys`).** The recording runs through the voice's own filter and
volume, so per-note `note_cutoff` / `note_vol` shape it. PLEYEL (after the 1840s piano; Flyfish's idea, our
numbers): low-pass at 7x the fundamental, 6 dB/s extra fade, 0.85 touch (C3 chord: centroid 328 → 258 Hz,
16 dB less above 4 kHz). SOFT pedal at the strike: x0.65 and a low-pass at 9x the fundamental. ⚠ An "open"
18 kHz low-pass is NOT transparent (2.4 dB off the Steinway's top): a note needing no darkening plays with
`note_filter(h, FILTER_OFF)`.

**Gotcha for the next person:** click-check flags a smooth but steep zero crossing in a quiet passage at
4-5x; read the samples before believing a splice there (it fooled one check here).

**Open:** velocity layers (soft notes are only quieter, not darker): needs the full library download (behind
a bot check), more than 16 sample slots, and a velocity axis on the zones; ~0.9 MB a layer. F#6 once rendered
to digital silence near its noise floor, cause unknown.

## Verify any change

- A/B render against the current voice before/after (don't trust reasoning — use ears).
- `node tools/tune-check.js --quiet` if touching pitch/dispersion/stretch.
- Re-bake the showcase: `tools/carts/piano.c` (make-cart build + `--run`).
- `node tools/play.js soundcheck script /dev/null --headless --frames 900 | grep "\[sound\]"`
  (silence = PASS) after any `sound.h` edit.
