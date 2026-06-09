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

The continuous-excitation family (reed, pipe) and the modal families (mallet, membrane, epiano)
are done. Two things remain on the roadmap, in order:

1. **The buffered acoustic pair — `INSTR_PIANO` (StifKarp) / `INSTR_GUITAR` / `INSTR_HARP`**
   (§8.5 step 9 tail). The *genuinely buffered* engines — piano's `ks2Buffer[2048]` second string,
   guitar's KS string + body resonator — on the pluck-validated path. **This is the next engine.**
   navkit cribs: `processStifKarpOscillator` / `processGuitarOscillator` (§8.7).
   - **Bowed (violin/cello) — re-examine; the "unstable" verdict was over-read** (2026-06-09 lit
     check). STEP-0 rejected navkit's bowed off one render (erratic envelope, crest 12.6 vs a clean
     voice's ~2–5), but the DSP literature says that's the signature of bowing *outside the Schelleng
     wedge* (too little force → double-slip "surface sound"; too much → raucous crunch) — a
     bow-force/velocity/position **regime** problem, not inherent instability. A working bowed string
     is a low-crest Helmholtz "leaning sawtooth." Same trap as the Rhodes/Wurli mis-tuning: a bad
     preset, not a bad engine. Next: a proper bow-force × velocity × position sweep of navkit's
     bowed to find the Helmholtz wedge; fallback is the standard hysteresis bow table (McIntyre-
     Schumacher-Woodhouse 1983, which Smith's simplified STK table omits). Detail: §8.5 step 9.
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
