# Instrument bank + orchestra tuner — plan (PARKED, pick up in a long stretch)

*Filed 2026-06-11. Status: **parked behind a prerequisite** (see §2). The idea is good and
the groundwork audit is done — this doc is the actionable spec so a future session can pick
it up cold.* Sibling of [`tuning-handoff.md`](tuning-handoff.md); tracked as
[STATUS #33](STATUS.md).

---

## 1. The idea

Two complementary things, one shared source of truth:

- **An instrument bank** — a machine-readable registry of the named, dependable instrument
  voices (the "vanilla violin", "piccolo", "Rhodes"…), each with its engine + macro settings +
  intended register + a **tuning verdict**. Today these voices exist only as floats copy-pasted
  out of [`guides/instrument-recipes.md`](guides/instrument-recipes.md) into individual carts —
  there is **no shared header**, so a recipe lives in 3–4 places and can rot independently.
- **An "orchestra tuner" cart** — the human/agent **audition** counterpart to the headless
  `tune-check.js` CI gate. Pick an instrument → pick one of its named voices → it plays the note
  **and a sine reference at the same MIDI simultaneously**, so you *hear* the beating if it's
  detuned (the classic tune-against-A440 trick), while the screen shows the **baked cents needle**
  for that voice (the *answer key*, sourced from `tune-check.js`). Ears get the phenomenon, eyes
  get the number, and they agree because both trace to the same measurement.

**Why both:** `tune-check.js` (have it) is the *CI gate* — headless, trust-the-number. The tuner
cart (want it) is the *audition* — interactive, trust-your-ears-and-eyes. The connective tissue
that makes both honest is the **single registry**: the tuner is only honest if it plays the same
values the radio carts run, and the gate is only useful if it measures the *named recipes* people
actually pick (it currently measures bare engine defaults — see §4).

> **▶ A working prototype already ships: the `pipetune` cart** (`tools/carts/pipetune.c`, in the
> gallery as **"pipe tuner"**, tag `probe`). It's the PIPE-focused, single-engine version of the
> orchestra tuner above: a chromatic sweep low→high sounded against a pure SINE so you *hear* drift
> as beating. The five flute presets are on keys **1–5** (flute/piccolo lock in tune;
> recorder/breathy/pan-pipe go audibly flat — the hollow-embouchure `morph ≤ 0.5` failure), and the
> embouchure macro is live on **UP/DOWN** so you can hear a macro retune the model. Every on-screen
> tuning verdict was validated against `tune-check.js`. **This is the audible reference for "is a
> voice in tune"** — and it proves out the per-voice `tune:` verdict the registry needs (§3 schema):
> the cart already carries it as a per-preset `bad` flag.

### Hard design principle — the bank is an opt-in palette, NEVER a gate
A cart that wants a weird voice just sets `instrument_harmonics/timbre/morph(slot, x)` raw, same
as today. The bank is a *convenience + a guarantee* for the dependable voices, not a checkpoint.
`tune-check.js` only gates what's *registered*; off-book/experimental/live-swept voices keep
working untouched. We're paving a road, not fencing the field.

---

## 2. Why it's parked — the prerequisite that must land first

**The per-voice control surface beyond `h/t/m` is not unified yet.** A preset schema with only
the three macros would *silently fail to capture* the voices that need it most:

- `eng_tune(slot, idx, value)` — **EXPERIMENTAL**, per-engine idx-keyed: BOWED pizzicato
  (`idx 0 ≥ 0.5`), GUITAR/PIANO fundamental-weight + attack-click. The decl itself says
  *"values get baked to constants, not a final API"* (`runtime/studio.h:349`).
- `voice_nasal()` — INSTR_VOICE's "4th axis", but a bespoke function; plus `voice_consonant`/
  `voice_coda`/`voice_param` (`studio.h:350-353`). VOICE doesn't use the `h/t/m` model the same way.
- `instrument_tune(slot, semitones)` — octave/detune trim. This one **is** clean (`studio.h:356`).
- (Note: the `aux` references in `sound.h` are **FX buses** — chorus/flanger routing — *not* a
  control axis. Different "aux". Don't conflate.)

So **before building the registry, land a clean "4th axis" / per-engine aux-param API** that
replaces the experimental `eng_tune` idx-poke and gives bowed-pizz / guitar-fundamental / voice-
nasal a stable, named home. Then the preset schema can faithfully represent a *whole* voice.
Building the bank on the 3-macro model now = baking on shifting ground (the same hazard the
`studio.h` "re-read the current declaration" rule guards against).

> **Resume trigger:** when the clean 4th-axis/aux-param API has landed and `eng_tune` is either
> promoted or retired. Until then, parked.

---

## 3. Decisions already locked (don't relitigate)

- **Seed from the doc, as-is.** The audit (§4) found **zero drift** between
  `instrument-recipes.md` and the showcase carts' `PRESET[]` arrays — the doc is a trustworthy
  seed. The single-source work is about *preventing future* drift, not repairing existing rot.
- **Opt-in palette, not a gate** (see §1 principle).
- **Full tables, not just anchors.** Register all ~60 documented voices; a `canonical: true` flag
  marks the one vanilla anchor per family. Same effort (data's extracted), and the tuner cart
  becomes a real gallery instead of 13 lonely notes.
- **Architecture: JSON source → codegen header**, mirroring `gen-tcc-symbols.js → studio_tcc_symbols.h`:
  - `tools/presets.json` — **the single source of truth**.
  - `tools/gen-presets.js → runtime/presets.h` — emits a table + `preset_apply(slot, PRESET_*)`
    so carts `#include` it instead of hardcoding floats. JS source is read directly by `tune-check.js`.
  - A `lint-docs.js`-style check (or a generated column) keeps `instrument-recipes.md` honest
    against the JSON.

### Proposed schema (`presets.json` entry)
```json
{ "name": "bowed/violin", "family": "BOWED", "engine": "INSTR_BOWED",
  "h": 0.45, "t": 0.30, "m": 0.70,
  "aux": { ... },                 // <-- the §2 prerequisite defines this shape (pizz, fundamental, nasal…)
  "lo": 55, "hi": 91, "canonical": true,
  "tune": "verified",             // verified | risky | not-pitch  → drives CI gate + needle color
  "tune_note": "" }
```

---

## 4. Insights from the reconciliation audit (2026-06-11)

Full audit ran doc ↔ cart for all 13 modeled engines. Keep these so we don't re-run it:

- **API surface:** macros are `instrument_harmonics/timbre/morph(slot, x)` (`studio.h:342-344`).
  41 carts set macros; the **13 showcase carts** carry the canonical `PRESET[]` arrays and are
  the authoritative seed (clean):
  `guitar.c:37`, `mallet.c:41`, `fm.c:47`, `epiano.c:57`, `organ.c:60`, `pd.c:45`, `tabla.c:44`,
  `reed.c:50`, `pipe.c:45`, `bowed.c:43`, `brass.c:47`, `piano.c:42`.
- **Zero float drift.** Every showcase `PRESET[]` matches the doc value-for-value. Radio carts
  that cite specific values match too (`air.c:213`, `polopan.c:212`, `mariachi.c:459/477`,
  `afrobeat.c:434`).
- **PIPE is the lone tuning hazard** (per [STATUS #31](STATUS.md) / [audio-notes §18](design/audio-notes.md)):
  only `pipe/flute` (m0.70) is verified in tune. `recorder` (m0.30) + `breathy` (m0.42) go flat
  up high → `tune: risky`. `piccolo` is a deliberate overblown flageolet → `tune: not-pitch`.
  Any new flute voice must use morph ≳ 0.5 and pass `node tools/tune-check.js --engine PIPE`.
- **Vanilla anchor per family** (`canonical: true` candidates):

  | family | anchor | h / t / m |
  |---|---|---|
  | PLUCK | pluck/default | 0.5 / 0.55 / 0.35 |
  | GUITAR | guitar/nylon | 0.45 / 0.20 / 0.22 |
  | MALLET | mallet/marimba | 0.00 / 0.45 / 0.35 |
  | FM | fm/epiano | 0.15 / 0.45 / 0.10 |
  | EPIANO | epiano/rhodes | 0.15 / 0.30 / 0.25 |
  | ORGAN | organ/jimmy | 0.44 / 0.55 / 0.75 |
  | PD | pd/cz-bass | 0.06 / 0.62 / 0.40 |
  | MEMBRANE | membrane/tabla | 0.10 / 0.45 / 0.45 |
  | REED | reed/clarinet | 0.00 / 0.30 / 0.40 |
  | BOWED | bowed/violin | 0.45 / 0.30 / 0.70 |
  | BRASS | brass/trumpet | 0.15 / 0.60 / 0.42 |
  | PIANO | piano/grand | 0.08 / 0.50 / 0.62 |
  | **PIPE** | **pipe/flute (ONLY safe one)** | 0.00 / 0.38 / 0.70 |

  (VOICE uses formant params, not `h/t/m` — out of scope for the macro bank; handle via the §2 API.)

---

## 5. Build order (when un-parked)

1. **Land the §2 prerequisite** (clean 4th-axis/aux-param API). *Blocks everything below.*
2. **Write `tools/presets.json`** — full tables, seeded from `instrument-recipes.md` + the
   showcase `PRESET[]` arrays, `canonical` + `tune` + `aux` fields populated.
3. **`tools/gen-presets.js → runtime/presets.h`** — table + `preset_apply()`. Re-run by the
   live-host build like `gen-tcc-symbols.js`; run manually after editing `presets.json`.
4. **Wire `tune-check.js` to the registry** — measure the *named voices over their intended
   register* (not just bare defaults), gate on the `tune` field. This is the real "the vanilla
   violin is guaranteed in tune" CI promise.
5. **Build `orchestra.c`** — instrument → named-voice browser; live note + sine reference (HEAR);
   baked cents needle from `tune-check.js` output (SEE). Doubles as the canonical-voice gallery.
   Register in `index.json` + bake a thumbnail.
6. **Findability** — generate/lint a per-voice tuning-verdict column into
   `instrument-recipes.md` so picking a voice from the docs shows whether it's verified.
7. **(Later, separate) migrate radio carts to `#include "presets.h"`** — retires the copy-paste at
   every call site. Mechanical once the registry is correct; deferred like the sound.h split.

---

## 6. Doc-debt subtask — RESOLVED ON INSPECTION 2026-06-11 (no work needed)

The reconciliation audit flagged these radio voices as "absent from `instrument-recipes.md`":
polopan balafon, mariachi vihuela/guitarra/guitarrón, the afrobeat horns + interlocking guitars,
jangle/bossa guitars. **This was a false positive.** Radio-station voices deliberately do **not**
live in `instrument-recipes.md` (that doc is the supply palette for *non-radio* instrument/machine
carts, per CLAUDE.md) — they belong in [`guides/radio-voices.md`](guides/radio-voices.md)
(per-station slot→role→preset) + [`guides/instrument-presets.md`](guides/instrument-presets.md)
(named patch + origin/used-by). The audit compared against the wrong doc.

On inspection, **every one is already documented there, and value-accurate** (all `h/t/m` match the
cart, including chair-variant voicings): `mallet/balafon`, `guitar/vihuela`, `guitar/mariachi-rhythm`
(+ nylon swap), `guitar/guitarron`, `guitar/afro-tenor` (`I_GTR1`), `guitar/afro-rhythm` (`I_GTR2`),
`reed/afro-tenor-sax` (+ alto chair), `brass/afro-trumpet`, `pluck/jangle-guitar`,
`tri/nylon-fake` + `pluck/nylon-guitar` (bossa). Zero drift. Nothing to write back.

*(Lesson for the §1–5 work: when wiring `tune-check.js`/the registry, the radio voices in
`instrument-presets.md` are a clean, value-accurate seed too — not just the showcase carts.)*
