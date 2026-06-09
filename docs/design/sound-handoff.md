# Sound work — session entrypoint (instrument engines)

> **Genre: living entrypoint.** The one place to start a sound session: where the engine
> roadmap stands, what's next, and where the durable knowledge lives. NOT a design doc —
> it points at the canonical docs and stays thin. Fold anything that proves out into
> §8.8.x and trim this. Last updated 2026-06-09.

## Where we are — the §8.5 engine roadmap

Twelve modeled engines shipped + fully wired (studio.h id · sound.h · studioDocs.js · shell.js ·
soundcheck slot · showcase cart). Roadmap-step order, then the string family (ids 26–28):

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
| 26 | **`INSTR_GUITAR`** (acoustic/nylon/banjo/koto/harp/uke/pizz) | ✅ shipped (§8.8.9) |
| 27 | **`INSTR_PIANO`** (StifKarp — grand→celesta) | ✅ shipped (§8.8.9) |
| 28 | **`INSTR_BOWED`** (violin/cello — arco + pizz from one waveguide) | ✅ shipped (§8.5 step 9) |

**The modeled string family (pluck → guitar → piano → bowed) is complete.**

Canonical state lives in the docs below — don't duplicate it here, update those:
[STATUS §5](../STATUS.md) · engine specs + the **§8.5 roadmap** + the **§8.8.2 playbook**
[`instrument-engines.md`](instrument-engines.md) · PARKED interim items [§8.10.1](instrument-engines.md) ·
[ADR-0015](../decisions/0015-effects-are-recipes-not-primitives.md) ·
[ADR-0016](../decisions/0016-combo-organ-recipe-then-macro-or-engine.md).

## Start here next session

The continuous-excitation family (reed, pipe), the modal families (mallet, membrane, epiano), AND
the **full string family** (pluck / guitar / piano / bowed — arco *and* pizz) are all done. With
the string family closed, the only modeled-engine work left is the formant voice; after that the
roadmap turns to the effects layer. What remains, in order:

1. **Formant + the effects-bus layer (§8.10).** `INSTR_VOICE` (24) is **EXPERIMENTAL** — the voxlab
   prototype is live but the public 3-macro mapping isn't locked (which is why it's not in the help
   tab yet). The effects-bus layer is sequenced LAST, after the engines: master reverb + the bus
   concept, then delay/tape/leslie/wah. (Stereo §9 — now resolved.) The PARKED interim items
   (per-voice wah, envelope follower, epiano tremolo, suitcase auto-pan) all fold in here — see
   [§8.10.1](instrument-engines.md).
2. **Cleanup, when the values lock:** the `eng_tune()` scaffolding is EXPERIMENTAL on three slots —
   guitar/piano weight+attack-click, and the **bowed pizz flag** (`eng_tune(slot,0,1)` → `eng_p[0]`).
   Bake the dialed-in values to constants and retire the live knobs.

### ✅ Done this session — the string family (reference, not remaining work)

- **Buffered acoustic pair — `INSTR_GUITAR` (26) + `INSTR_PIANO` (27), SHIPPED 2026-06-09.** Guitar =
  Karplus-Strong + body resonator; piano = a **verbatim** StifKarp port (near-lossless string, 6
  voicings grand→celesta, dispersion, soundboard, detuned 2nd string, brightness bloom). The verbatim
  port was the key lesson: param-matching navkit wasn't enough, the DSP had to be line-for-line.
  `HARP` folds into `GUITAR` (a preset); clavinet stays an EPiano position. §8.8.9.
- **Bowed — `INSTR_BOWED` (28), SHIPPED 2026-06-09.** The "unstable" verdict was over-read:
  `tools/navkit-bowsweep.c` swept navkit's bowed and the crest-12.6 reject turned out to be **preset
  107's operating point** (corr 0.36, real surface sound) — one bad preset, not a bad engine (the
  Rhodes/Wurli trap). Line-for-line port: navkit's two nut+bridge delay lines PACKED into one
  `ks_buf`, self-oscillating/held, macros pinned inside the wedge (timbre=pressure 0.15–0.42 the
  narrow axis, harmonics=position β 0.05–0.25, morph=speed/swell); timbre confirmed corr 0.96 clean →
  0.35 deliberate scratch. **PIZZICATO is the same waveguide** — a slot flagged `eng_p[0]>=0.5`
  (`eng_tune(slot,0,1)`) seeds the string with a pluck and bypasses the friction, so the identical
  string+body rings down (arco/pizz differ only in excitation). Showcase: the **bowed** cart — RUB to
  bow (energy accumulates), TAP to pizz. Soundcheck slot 23, tripwire PASS. Detail: §8.5 step 9.

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
