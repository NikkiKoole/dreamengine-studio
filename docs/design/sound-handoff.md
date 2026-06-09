# Sound work — session entrypoint (instrument engines)

> **Genre: living entrypoint.** The one place to start a sound session: where the engine
> roadmap stands, what's next, and where the durable knowledge lives. NOT a design doc —
> it points at the canonical docs and stays thin. Fold anything that proves out into
> §8.8.x and trim this. Last updated 2026-06-09.

## Where we are — the §8.5 engine roadmap

Nine modeled engines shipped + fully wired (studio.h id · sound.h · studioDocs.js · shell.js ·
soundcheck slot · showcase cart). In roadmap order:

| # | Engine | Status |
|---|---|---|
| 2 | **`INSTR_MALLET`** | ✅ shipped |
| 3 | **`INSTR_FM`** | ✅ shipped |
| 4 | **`INSTR_ORGAN`** | ✅ shipped + published (Leslie deferred to §8.10) |
| 5 | **`INSTR_EPIANO`** | ✅ shipped + published; Rhodes rebuilt from measured spectra (§8.8.5) |
| 6 | **`INSTR_PD`** (Casio CZ) | ✅ shipped |
| 7 | **`INSTR_REED`** (clarinet↔sax) | ✅ shipped — first self-oscillating held wind (§8.8.7) |
| 8 | **`INSTR_MEMBRANE`** (tabla/conga/djembe) | ✅ shipped |
| 9 | **`INSTR_PIPE`** (flute/recorder/pan pipe) | ✅ shipped (§8.8.8) |

Canonical state lives in the docs below — don't duplicate it here, update those:
[STATUS §5](../STATUS.md) · engine specs + the **§8.5 roadmap** + the **§8.8.2 playbook**
[`instrument-engines.md`](instrument-engines.md) · PARKED interim items [§8.10.1](instrument-engines.md) ·
[ADR-0015](../decisions/0015-effects-are-recipes-not-primitives.md) ·
[ADR-0016](../decisions/0016-combo-organ-recipe-then-macro-or-engine.md).

## Start here next session

The continuous-excitation family (reed, pipe), the modal families (mallet, membrane, epiano), AND
the buffered string pair are now done. What remains, in order:

1. **The buffered acoustic pair — `INSTR_GUITAR` (26) + `INSTR_PIANO` (27) — SHIPPED 2026-06-09.**
   Guitar = Karplus-Strong + body resonator; piano = a **verbatim** StifKarp port (near-lossless
   string, 6 voicings grand→celesta, dispersion, soundboard, detuned 2nd string, brightness bloom).
   The verbatim port was the key lesson: param-matching navkit wasn't enough, the DSP had to be
   line-for-line (the harpsichord then matched navkit's render). Pizzicato = a short-mute guitar
   preset; clavinet stays an EPiano position (`WAVE_EPIANO`, not a string engine). Design + journey:
   [`instrument-engines.md`](instrument-engines.md) §8.8.9. **`HARP` folded into `GUITAR`** (a preset,
   like Rhodes/Wurli/Clav fold into EPiano). Tuning scaffolding `eng_tune()` (weight/attack) is live
   on the showcase carts — EXPERIMENTAL, bake-to-constants-and-retire when the values lock.
   - **Bowed (violin/cello) — the remaining string engine; STEP-0 DONE 2026-06-09, GREEN LIGHT.**
     The "unstable" verdict was over-read: `tools/navkit-bowsweep.c` swept navkit's bowed across
     pressure × velocity × position and the crest-12.6 reject turned out to be **preset 107's
     operating point** (P=0.6/V=0.5/β=0.13 → corr 0.36, real surface sound) — one bad preset, not a
     bad engine (the Rhodes/Wurli trap). A large Helmholtz wedge exists (100–150/600 cells lock
     corr→1.0, crest 1.6–1.8) and holds across G2→A4. **No hysteresis bow table needed** — Smith's
     simplified friction locks inside the wedge. The clean band is **LOW** pressure (~0.15–0.45;
     navkit's friction inverts the force intuition). Macro plan: `timbre`→pressure 0.15–0.45,
     `harmonics`→bow position β 0.05–0.25, `morph`→velocity/swell 0.2–1.0. Reuses one `ks_buf` delay
     line, held-note — same difficulty as the shipped reed/pipe. Next: line-for-line port (the piano
     lesson), `INSTR_BOWED` (28). Detail + the wedge map: §8.5 step 9.
2. **Formant + the effects-bus layer (§8.10).** `INSTR_VOICE` (24) is **EXPERIMENTAL** — the voxlab
   prototype is live but the public 3-macro mapping isn't locked (which is why it's not in the help
   tab yet). The effects-bus layer is sequenced LAST, after the engines: master reverb + the bus
   concept, then delay/tape/leslie/wah. **Resolve stereo (§9) before it.** The PARKED interim items
   (per-voice wah, envelope follower, epiano tremolo, suitcase auto-pan) all fold in here — see
   [§8.10.1](instrument-engines.md).

**Run playbook STEP 0 before any new-engine code** (§8.8.2): render the navkit reference and locate
its LAYER (per-voice vs bus) before building. STEP 0 exists because of the wah detour (a per-voice
build chasing a bus-layer target) — don't skip it.

## Comparing against navkit — the render A/B (a reusable process)

When ours sounds different from navkit, **render navkit's actual output and A/B it** rather than
reading its source and guessing. Tools (committed):
- `tools/navkit-render.c` — renders any navkit preset to a WAV (needs the `~/Projects/navkit`
  sibling; build cmd in its header). e.g. preset 180 = "Clav Funky".
- `tools/wav-envelope.js` — plots a 100ms amplitude + brightness(HF) envelope; a real wah/tine
  shows **brightness dropping faster than amplitude**.

> **Watch:** `wav-envelope.js`'s brightness proxy conflates a resonant-filter PEAK with perceptual
> brightness, so it under-shows a resonant snap — trust ears for resonant timbre.

**Lesson the wah detour taught (now baked into playbook STEP 0):** confident claims — ours *or the
docs'* — about a reference sound are hypotheses until rendered. The funky-clav "wah" turned out to
be a fast per-note filter-env quack (per-voice, shipped), while the realistic "woah woah" auto-wah
is a **bus effect** ([`instrument-engines.md`](instrument-engines.md) §8.10) — the docs had
contradicted themselves until the render settled it (corrected in §8.10 + the
[ADR-0015](../decisions/0015-effects-are-recipes-not-primitives.md) Correction note).
