# Piano engine (INSTR_PIANO / StifKarp) — why it sounds like a harp, and the fix list

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

6. **Tuning fights the inharmonicity (the Feynman point).** We make the partials inharmonic via
   the dispersion allpass chain (`B = stiff²·0.015`, sound.h ~4042 — this *is* our stiff-string
   inharmonicity) but tune the **fundamental** dead-on equal temperament. A real piano tunes the
   *fundamentals* **stretched** (the Railsback curve: bass flat, treble sharp) so the stretched
   partials of different notes agree. Skipping the stretch is part of why our dispersion reads as
   "metallic shimmer" rather than "piano" — stretch and tuning are fighting instead of reinforcing.
   - **Ref:** Feynman on stiff-string theory & piano tuning —
     https://physicstoday.aip.org/features/stiff-string-theory-richard-feynman-on-piano-tuning
     (canonical derivation of the inharmonicity↔stretched-tuning relationship).

## Fix roadmap (rough order of payoff)

1. **Two-rate decay envelope** — fast initial + long aftersound (the #1 perceptual flip).
2. **Hammer-pulse excitation on by default** — replace/augment the pluck comb with a real knock
   (turn `eng_click` on for the piano voicings, or model a force pulse).
3. **Velocity → brightness + attack-noise coupling** — drive `timb`/click from note velocity.
4. **Multi-string unison on more voicings** — beating/phase double-decay for body.
5. **Stretched tuning** — match the fundamental tuning to the modeled inharmonicity (Feynman).

#1 + #2 together would likely flip the perception on their own; the rest are polish.

## Verify any change

- A/B render against the current voice before/after (don't trust reasoning — use ears).
- `node tools/tune-check.js --quiet` if touching pitch/dispersion/stretch.
- Re-bake the showcase: `tools/carts/piano.c` (make-cart build + `--run`).
- `node tools/play.js soundcheck script /dev/null --headless --frames 900 | grep "\[sound\]"`
  (silence = PASS) after any `sound.h` edit.
