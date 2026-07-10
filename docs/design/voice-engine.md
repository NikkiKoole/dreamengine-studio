# The voice engine (INSTR_VOICE) — design notes

> **Status: SHIPPED (2026-06-10).** Public API locked + wired: the 3 macros are VOWEL/SIZE/EFFORT,
> `voice_nasal()` is the earned 4th axis, `voice_consonant()`/`voice_coda()` articulate — all in
> `studioDocs.js`/`shell.js`. `voice_param()` stays as the probe carts' low-level poke (not public).
> See "Status — SHIPPED" + the RESOLVED section below. Carts: `vox4` (the locked-API demo), `vox`
> (jam), `voxpad` (phoneme keyboard), `voxlab`/`voxab`/`say` (probes). Ledger: [`probe-carts.md`](probe-carts.md).

## What it is

A port of navkit's **VoicForm** (`navkit/soundsystem/engines/`): a glottal pulse (Rosenberg
polynomial) through **4 parallel SVF formant filters** with a continuous vowel morph — the
same Chamberlin SVF dreamengine already uses for `filter()`. Vowels-only port: the 10 vowel
rows of navkit's phoneme table, plus a trimmed consonant set used for **articulation onsets/
codas** (not the full ARPABET text-to-speech path — that stays a separate future project).

This is the **formant SOURCE engine**, distinct from the formant *filter effect* (a vowel
filter you can put on any instrument). This one generates the voice from scratch.

## The control decomposition — the key idea

The voice's expressivity splits into **three KINDS of control**, and only one kind competes
for the engine's three macro slots:

| Kind | Examples | Lives where |
|---|---|---|
| **Continuous timbre** (held knobs) | vowel, size, effort | the 3 macros (`harmonics`/`timbre`/`morph`) |
| **Per-note articulation** (timed events) | consonant onset, consonant coda | `voice_consonant()` / `voice_coda()` at note-on/off — like velocity, NOT a macro |
| **Modulators** (shared infra) | vibrato | `LFO_PITCH` via `note_lfo()` — not a voice param at all |

The consonants are deliberately **not** axes: they're transient, timed events, so they belong
in the note-trigger API, not a held slider. Vibrato is an orthogonal modulator every engine
already has. So "how many macros does the voice need?" is really only about the *continuous*
dimensions.

## The 3 continuous axes (current proposal)

| # | Axis | Engine macro | What it does |
|---|---|---|---|
| 1 | **VOWEL** | `harmonics` | U→O→A→E→I morph — *what's said* |
| 2 | **SIZE** | `timbre` | formant shift = vocal-tract length, giant→child — *who says it* |
| 3 | **EFFORT** | `morph` | one macro of breath + glottal open-quotient + spectral tilt — soft/breathy/dark → hard/pressed/bright — *how it's said* |

**Why effort is a macro-of-three:** breath, open-quotient and tilt all point the same
perceptual direction (soft↔hard); the `voxlab` probe confirmed they read as one gesture.
(That probe also caught a bug — the original tilt mapping went transparent at both extremes
and did nothing; fixed to a monotonic bright→dark LP. Open-quotient is the subtlest of the
three solo, which is itself evidence it belongs folded in.)

## Per-note articulation (the consonant layer)

- `voice_consonant(handle, id)` — **onset**: the note STARTS on a consonant that morphs into
  the vowel ("bah", "mah", "sss-ah"). Fired right after `note_on`.
- `voice_coda(handle, id)` — **coda**: the note CLOSES on a consonant as it releases
  ("ahh-m", "oo-d"). The mirror; fired right before `note_off`. Voiced ids keep it sung.
- 8 consonants (`b d g m n l s sh`): plosives (noise burst), nasals/liquid (low-F1 voiced),
  fricatives (noise that voices in). `vox`'s jam uses a mellow no-`s`/`sh` pool.

## RESOLVED (2026-06-10) — 3 macros + ONE earned extra (nasality)

The by-ear pass (`vox4`, the swappable-4th-axis cart, then the lean cart) settled it. **The voice
is the one engine that earns a small break from the uniform 3-macro mould — but only by ONE knob.**

**The locked API:**

| | Control | Mapping |
|---|---|---|
| macro `harmonics` | **VOWEL** | U→O→A→E→I |
| macro `timbre` | **SIZE** | formant shift / vocal-tract length, giant→child |
| macro `morph` | **EFFORT** | one clean soft/dark/relaxed → bright/pressed knob (tilt + glottal open-q + *light* breath) |
| `voice_nasal(h, x)` | **NASAL** | the earned 4th — open → hummed/nasal (the honk, the Tibetan-monk chant) |
| `voice_consonant(h,id)` / `voice_coda(h,id)` | articulation | timed onset/coda events over the full 10-vowel / 22-consonant table |

**How the verdict was reached (each candidate had to earn it by ear):**
- **Roughness** (jitter/shimmer/creak) — *cut.* Strengthened and re-auditioned; the owner "just
  doesn't like what it does." It's a character effect (robot/rasp/fry), not an always-on axis —
  belongs in a preset/per-note path later, if ever, not a macro.
- **Buzz** (glottal↔sine) — *cut.* Read as "duller," overlapping the vowel/brightness territory;
  not distinct enough to earn a knob.
- **2D vowel** (openness/frontness) — *cut.* 1D vowel + diphthong cover it at far less budget.
- **EFFORT was over-bundled** — the original breath=0.55 made it whispery/"funky"; retuned to a
  *light* breath led by tilt+open-q so it reads as one clean gesture.
- **Nasality** — *kept.* The only color that reaches somewhere VOWEL/SIZE/EFFORT can't (hummed/
  honk/chant). The owner missed it when it was gone → that's the test for earning a dedicated knob.

**Stays cart-land / reused (not API):** vibrato = `note_lfo(LFO_PITCH)`; diphthong, schwa,
prosody, phoneme-pool selection = cart recipes; measured-vs-derived bandwidth = internal default.
`voice_param()` (the raw by-index poke) stays as the **experimental low-level path the probe carts
ride** — it is not the public surface and is not advertised.

<details><summary>Original open question (for the record)</summary>

Three generic macros is the engine convention, and the voice already exceeds a pure 3-macro
engine by adding the consonant calls + the LFO. The question was whether the continuous
dimensions fit in three, or the voice deserves a richer API. Candidates auditioned for a 4th+
axis: breath-split-from-effort, nasality, 2D vowel, diphthong target. **Outcome: nasality earned
the one extra knob; the rest folded or were cut.**
</details>

## Enrichment ideas (same style — playable, cart-land first)

Things that would deepen the instrument without necessarily adding macros:

- **Glide/portamento between syllables** — legato scat instead of re-triggered steps on the
  jam pad (within-drag pitch glide, re-randomise only on a fresh press).
- **Choir / unison-detune** (since promoted to an engine primitive: [`unison-primitive.md`](unison-primitive.md)) — 2–3 voices a few cents apart for the "aah" pad / gospel stack.
- **Diphthongs** — vowel→vowel morph within a held note (needs a target-vowel set or the
  diphthong axis above).
- **Mid-note consonants / clusters** — stepping toward real words ("la-la" → "hello"); the
  bridge to the TTS project.
- **Scat-rate / auto-rhythm** on the jam pad — tempo-locked syllable retrigger.
- **Vowel on the pad's Y axis** (instead of effort) — sing words by gesture.
- **Per-note random size** — a ragged kids'-choir / crowd texture.

## Prosody — the fourth kind of control (toward SPEAKING)

The three kinds above (continuous timbre / per-note articulation / modulators) cover *singing
one note*. **Speaking** needs a fourth: **prosody** — pitch, timing and stress shaped over a
whole *phrase*. It's where "is this a question?", "which word is emphasized?", and the melody of
a spoken sentence live. Crucially it is mostly **not** an engine concern — it's *sequencing*, so
per the recipe-first instinct ([ADR-0006](../decisions/0006-library-carts-not-engine.md),
[decision-0015](../decisions/0015-effects-are-recipes-not-primitives.md)) it should prove out in
cart-land before any new API. The `say` probe does exactly that.

**What `say` established by ear (2026-06-09), using ONLY existing primitives:**

- **Connected speech works on ONE held note.** Re-firing `voice_consonant()` on an
  already-held note resets `vox_cons_t`, so it re-articulates the consonant *mid-note* — turning
  the "one syllable per note" voice into connected syllable chains on a single `note_on`, no
  retrigger. `say`'s RANDOM now builds language-like simlish from a mix of **V / CV / VC / CVC**
  shapes (codas wired through `voice_coda()` fired in each syllable's back third), not a
  monotonous CV string. **So a dedicated mid-note-consonant API is not *required* to prototype
  speech** — the open question is whether the re-fired onset/coda morph *reads* as a true
  inter-vocalic / syllable-closing stop or needs a purpose-built one.
- **Pitch contour = per-frame `note_pitch()`** along a per-syllable shape (flat/rise/fall/peak)
  plus phrase-level declination. Intonation falls out as data: statement = final fall, question =
  final rise, exclaim = hard fall; **emphasis** = a syllable gets a pitch *accent* (peak) + longer
  duration. This is the "pitch in a word / question / emphasize a word" the design was reaching for.
- **Character** (the old terminal `say()` "vary character" lever) = a preset bundle of
  size/breath/open-q/tilt + pitch-offset + **pitch-RANGE** + rate. robot = zero range + no glide →
  monotone; kid = small+high+wide; giant = long+low+slow; whisper = breath maxed.

**The engine gaps the probe surfaces** (candidate real API, each must earn it by ear):

1. **Creak / jitter in the glottal source** — without it robot can't sound truly sterile and
   there's no old-man rasp / vocal fry. The lever that most "varies character"; cheap to add to the
   Rosenberg pulse. *Most likely first engine addition.*
2. **A fuller consonant set** — only `b d g m n l s sh` today; speech wants `h w r f v` and the
   unvoiced stops `p t k`. Each is a formant/noise table row.
3. **Schwa (reduced vowel)** — unstressed syllables collapse to it; without it everything
   over-enunciates. A trivial vowel row.
4. **Diphthong / vowel-glide target within a note** — "I", "oh", "now"; also makes *singing* far
   more natural. **Now cart-prototyped in `say`** (a per-frame `note_pitch`-style lerp of the
   VOWEL param toward a second vowel across the syllable) — so, like breath in `voxlab`, it's
   auditionable by ear; the open question is whether the cart-land lerp is enough or the engine
   wants a first-class diphthong target.

The contour/timing/stress layer itself stays a **cart recipe**; only if it proves universal does a
`voice_phrase()` convenience earn a place.

## Completeness audit — what navkit still has that we trimmed (2026-06-10)

`INSTR_VOICE` is a **faithful but trimmed** port of navkit's VoicForm
(`navkit/soundsystem/engines/synth_oscillators.h`). Auditing the source against our
kernel (`runtime/sound.h` ~§1462–1655) turned up levers worth pulling — most **"free"**
(the code/data already exists in navkit, so it's a port not a research problem), one
genuinely **net-new**. This is the reference list for the experiment.

### Trimmed out of VoicForm itself

| Gap | navkit | us | Why it matters |
|---|---|---|---|
| **Phoneme table** | 31 rows (`vfPhonemeTable`, `synth_oscillators.h:215`) | 13 (5 vowels + 8 consonants) | Missing vowels **ER/schwa, AE, AH, AW, UH** and consonants **R W Y NG F V Z TH P T K CH**. The `say` probe's "schwa" + "fuller consonant set" gaps are *already sitting in navkit as measured rows* — a copy-paste, not new design. |
| **Measured bandwidths** | per-phoneme BW1–4, Klatt 1980 / Peterson-Barney 1952 | we *derive* `bw = 60 + fc*0.08` (`sound.h` formant loop) | Our vowels read slightly "synthier" because the resonance widths are approximated. Porting the real BW table is a free authenticity bump and a clean A/B. |
| **Phoneme morph engine** | `morphRate`/`morphBlend` general phoneme→phoneme glide (`processVoicFormOscillator`) | hand-rolled onset/coda timing in cart-land (`say`) | Kernel-level diphthong/coarticulation machinery. Worth comparing whether the engine glide reads better than `say`'s per-frame vowel lerp. |

> **PORTED 2026-06-10** (the first two rows): the vowel table is now navkit's full **10 rows**
> (rows 0–4 = the unchanged U→I slider path; 5–9 = AE AH AW UH ER) and the consonant table is the
> full **22** (ids 0–7 frozen; 8–21 = ng r w y dh f v z zh th p t k ch) — all with measured
> bandwidths. The extra vowels are reachable via the experimental **diphthong glide** (`voice_param`
> 17 vow2 / 18 glide), which also covers the 4th `say` gap and partially the morph-engine row
> (a within-note vowel glide, not yet navkit's general timed `morphRate`). Still missing vs navkit:
> **`h`** (navkit's table has no H row — that one *is* research, not a copy-paste).

### Levers in navkit's *other* voice (WAVE_VOICE) that VoicForm never got

These live in `processVoiceOscillator` (`synth_oscillators.h:75–207`), not VoicForm — so
they're absent from our port but the code exists to crib:

| Lever | navkit code | What it adds |
|---|---|---|
| **Anti-formant nasality** | `:179` — a ~350 Hz *notch* + nasal resonance | A *physically correct* nasality vs voxlab's current "morph toward a nasal formant config." The better model to settle "does nasality earn an axis?". |
| **Pitch envelope** | `:118–135` — `pitchEnvAmount` + `pitchEnvCurve` | Per-note pitch scoop/dip — sells voiced-plosive attacks, gives prosody a kernel-level accent shape. |
| **Consonant pitch-mod** | `:139` `consonantPitchMod` | Voiced stops bend pitch microscopically — realism the `say` codas lack. |
| **Buzziness** | `:147` — triangle↔glottal source blend | A *source-character* knob orthogonal to tilt/breath. Cheap candidate axis. |

### The one gap navkit can't fill: creak / jitter / shimmer

Period-to-period pitch + amplitude wobble in the glottal source. **Not present in either
navkit voice** — so it's genuinely net-new code on the Rosenberg pulse, and therefore the
**highest-value engine lever**: the only one on this list you cannot prototype in cart-land.
Three flavors worth separate audition sliders: *jitter* (pitch noise → roughness),
*shimmer* (amplitude noise), *creak/diplophonia* (occasional dropped/doubled pulse → vocal
fry). This is the lever the `say` probe flagged as most "varies character" (robot sterility,
old-man rasp).

### Audition priority (by cost)

1. **Free, port now** — measured per-phoneme bandwidths (A/B vs derived); schwa + ER vowel
   rows (unblocks `say` over-enunciation); anti-formant nasality (the *good* nasality model,
   alongside the config-morph one to compare); buzziness (a 5-minute source knob).
2. **Net-new, highest value** — creak/jitter/shimmer trio; pitch envelope.
3. **Cart-land recipe, prove before API** — unison/choir detune; glide/portamento between
   jam-pad steps; per-note random size (see Enrichment ideas above).

## Decision log

- **2026-06-10** — **API LOCKED + wired (shipped).** Verdict by ear: 3 macros (VOWEL/SIZE/EFFORT)
  + one earned 4th knob (`voice_nasal`) + the two articulation calls. Roughness and buzz cut
  (re-auditioned at full strength; roughness disliked, buzz too redundant with vowel/brightness),
  2D-vowel cut, EFFORT retuned (light breath). Wired into the kernel macros + the four editor/host
  places; `voice_param` kept as the probe carts' low-level path. See "Status — SHIPPED" below.
- **2026-06-10** — **Phoneme table completed to navkit (the copy-paste the audit promised).** Vowel
  table 5→**10** rows (rows 0–4 = the unchanged U→I slider path; 5–9 = AE AH AW UH ER, with measured
  BW); consonant table 8→**22** (ids 0–7 frozen; appended ng r w y dh f v z zh th p t k ch). Pure
  data + non-breaking: existing carts never reference vowel rows 5–9 or consonant ids 8–21, so
  `vox`/`voxlab`/`say` render byte-identically (tripwire PASS). The extra vowels are reachable via a
  new experimental **diphthong** lever (`voice_param` 17 vow2 / 18 glide; glide=1 sits fully on vow2,
  so AE etc. are usable as steady vowels too) — which also prototypes the 4th `say` gap (diphthong)
  and a within-note slice of the morph-engine row. `voxab` gains a VOWEL2 pair + the two rows.
  **Only `h` remains unported** (navkit has no H row — genuinely research, not copy-paste).
- **2026-06-10** — **Audit levers wired + `voxab` comparator built.** Acting on the audit, the
  "free" navkit levers + the net-new roughness trio went into the kernel as EXPERIMENTAL
  `voice_param()` indices 10–16 (`runtime/sound.h`): **10** buzz (glottal↔sine source), **11**
  jitter, **12** shimmer, **13** creak (the net-new trio), **14** anti-formant nasality (navkit
  WAVE_VOICE's notch — alongside the existing config-morph nasality on idx 7, to A/B), **15**
  schwa-reduce, **16** measured-bandwidth toggle (navkit's BW table vs derived). All default to
  current behavior, so existing carts (`vox`/`voxlab`/`say`, which only set 0–9) are unchanged;
  sound tripwire PASS. Built **`voxab`** — the A/B + solo-sweep comparator — as the rig to settle
  the verdicts by ear (probe ledger row added). **Open: the by-ear verdicts themselves** (which
  levers ship, which nasality model wins, is measured BW worth it). ER-as-distinct-vowel and the
  fuller consonant set (`h w r f v`, `p t k`) deferred — they need the phoneme-table expansion,
  not just a param.
- **2026-06-10** — **Completeness audit vs navkit** (section above). Our `INSTR_VOICE` is a
  trimmed VoicForm: 13 of navkit's 31 phoneme rows, derived (not measured) bandwidths, no
  built-in phoneme-morph engine. navkit's *other* voice (WAVE_VOICE) additionally has
  anti-formant nasality, a pitch envelope, consonant pitch-mod, and a buzziness source knob —
  none ported. **creak/jitter/shimmer is in neither navkit voice** → confirmed net-new and the
  top engine lever (only one not fakeable in cart-land). Next: audition the "free" levers by
  ear before any axis decision.

- **2026-06-09** — Built the `say` prosody probe (see §"Prosody" above). Confirmed by render
  that **connected speech runs on a single held note** via re-fired `voice_consonant()`, that a
  per-frame `note_pitch()` contour gives statement/question/emphasis intonation, and that
  character presets cover the terminal-`say()` "vary character" lever — all with **zero new
  engine API**. Prosody stays a cart recipe. Next engine lever to audition: **creak/jitter** in
  the glottal source (the one gap that's clearly kernel-only and most changes perceived character).
- **2026-06-09** — On "is 3 macros enough?": **defer the API shape; audition first.** Decide
  whether breath / nasality / 2D-vowel each earn their own continuous knob *by ear* in
  `voxlab`/`vox` before committing to either the lean (3-macro + events) or the richer
  (dedicated `voice_*`) API. Evidence before structure.
  - Already auditionable: **breath** is its own slider in `voxlab` (param 2) — move it
    independently of tilt/open-q to feel whether it deserves to leave the effort bundle.
  - Not yet auditionable (would need an experimental slider added to `voxlab`):
    **nasality** (no param yet) and **2D vowel** (vowel is currently a 1D U→I path).

## Status — SHIPPED (2026-06-10)

The listen-by-ear pass landed the axes (see the RESOLVED section above) and the public API is
**wired and live**:

- **Macros mapped** — `INSTR_VOICE` reads `harmonics`/`timbre`/`morph` as VOWEL/SIZE/EFFORT
  (`vox_apply_macros()` in `runtime/sound.h`, hooked at note-on + the live `note_*` macro setter).
  EFFORT retuned to a clean light-breath curve (the old 0.55 breath was the "funky" whisper).
- **`voice_nasal()`** — the one earned 4th axis, wired in all four places (`studio.h` / `sound.h` /
  `studioDocs.js` / `shell.js`) + `studio_tcc_symbols.h` regenerated.
- **`voice_consonant()` / `voice_coda()`** — finalized (dropped EXPERIMENTAL, full 0..21 id range)
  and documented in the editor.
- **Demo** — `vox4` rewritten to drive the locked API (`note_harmonics/timbre/morph` + `voice_nasal`,
  no `voice_param`); verified non-silent + 0-clip via a headless WAV render. `vox`'s EFFORT retuned
  to match.
- **`voice_param()` kept** as the low-level/experimental poke for the probe carts
  (`voxlab`/`voxab`/`voxpad`/`say`) — not retired (it would break six working carts), not advertised.

Possible follow-ups (not blocking): move `vox`/`say` onto the macro API too; decide measured-vs-
derived bandwidth and hardcode the winner; a per-note creak/character preset if a cart ever wants it.
