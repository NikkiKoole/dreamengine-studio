# Piano engine (INSTR_PIANO / StifKarp) — why it sounds like a harp, and the fix list

STATUS: DESIGNED — diagnosis done (a faithful harp port); the fix roadmap (double decay, hammer knock, velocity coupling, stretched tuning) is the build.

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

**Tunable knobs (eng_p) are still RAW indices** (`MODE_PIANO_DECAY`=2 / `MODE_PIANO_KNOCK`=3 as
cart-local defines), deliberately not yet promoted to public `MODE_PIANO_*` constants via the
four-places API ritual. Per-voicing `dd`/`knock` baselines are reasoned defaults, not yet ear-tuned.

### Remaining polish (optional)
- Ear-tune the per-voicing `dd`/`knock` baselines and `PIANO_STRETCH_K`.
- Promote the eng_p indices to proper `MODE_PIANO_*` constants (studio.h + studioDocs + shell.js).
- More voicings could carry a detuned 2nd/3rd string (only grand/bright/dulcimer do now).

## Verify any change

- A/B render against the current voice before/after (don't trust reasoning — use ears).
- `node tools/tune-check.js --quiet` if touching pitch/dispersion/stretch.
- Re-bake the showcase: `tools/carts/piano.c` (make-cart build + `--run`).
- `node tools/play.js soundcheck script /dev/null --headless --frames 900 | grep "\[sound\]"`
  (silence = PASS) after any `sound.h` edit.
