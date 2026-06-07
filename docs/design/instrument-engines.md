# Instrument engines — the navkit port program

The rich-instrument program: porting navkit's self-contained oscillator engines
(organ, electric piano, Karplus-Strong strings, mallets, FM, winds…) behind single
`INSTR_*` ids with the fixed three-macro surface (harmonics / timbre / morph), plus
the effects-layer plan. **Genre: design exploration.**

> **Provenance + numbering.** Split out of [`audio-notes.md`](audio-notes.md) on
> 2026-06-07 — it was **§8** there (40% of the file). **Section numbers are kept
> verbatim (§8.1–§8.10)** so every existing reference — "audio-notes §8.9" in STATUS,
> tinydaws' cross-index, the cart-library list — still resolves; only the filename
> changed. Bare **§1–§17** references below point at sections of
> [`audio-notes.md`](audio-notes.md), where this doc was born and where the companion
> ledgers still live: **§12** instrument gaps · **§13** SCWs-vs-engines (the
> engines-first decision) · **§14** Roland machine readiness · **§2** the shipped
> surface map.

## 8. Borrowing rich instruments from navkit

> **Status: QUEUED — next sound feature, not started (as of 2026-06-03).** The plan below is
> agreed; the trigger just hasn't been pulled. **First bite when we do:** port **Karplus-Strong
> pluck** as `INSTR_PLUCK` — ~20 lines, sounds unmistakably like a real plucked string the moment
> it runs, needs no companion effect, and drops onto the existing fire-and-forget `note()` path.
> Crucially it exercises the small per-voice delay buffer (§8.2 — *decided*), proving the riskiest
> new infrastructure first so the whole buffered family (piano / guitar / strings) then rides a
> validated path. It ships with the fixed **3-macro surface** (harmonics / timbre / morph —
> §8.1.1) so it's live-tweakable from frame one, not a frozen preset. Then **`INSTR_MALLET`**
> (buffer-free, the celesta / music-box voice), then **`INSTR_ORGAN` + Leslie** as a complete
> package (drawbars → scanner on the proven buffer → shared rotary), then the EP / acoustic-piano /
> guitar family. (Organ *was* the original first bite for being buffer-free/zero-risk; once the
> buffer decision landed that risk argument dissolved, and organ's best self needs the Leslie —
> so it moved to the package bite.) **Coordinate:** this touches `runtime/sound.h`/`studio.c`,
> which the live/libtcc runtime work also lives in — sync before starting. modrack would expose
> these as a PLAITS-style MACRO voice (or just more `wav` options). Full sketch: §8.8.

There's a sibling project, **navkit** (`~/Projects/navkit/soundsystem`), with a large,
mature software synth (~53k LOC of header-only engines). It contains fully-built,
*self-contained* (no malloc, no heap in the hot path) oscillator engines for rich,
non-chiptune instruments: a Hammond tonewheel organ, Rhodes/Wurlitzer electric piano,
Karplus-Strong acoustic piano/guitar, bowed strings, mallets, formant/vowel voices, FM,
additive, and more. Each is a per-sample `processXxxOscillator(voice)` function — exactly
the shape our audio callback already uses.

The point of pulling from it: **"limited games, but with gorgeous sounds."** A silent-movie
platformer scored with a real upright piano + a theatre organ beats any amount of bleepy
breadth. We don't need general synthesis or a tracker for that — we need ~6–8 hand-tuned
instruments that sound like real money.

### 8.1 The key realization: `instr` is already the delivery vehicle

Adding a Hammond organ needs **no new API and no new concept.** It's one more `INSTR_*`
id wired to a richer oscillator in the mix loop:

```c
note(60, INSTR_ORGAN, 6);     // plays a B3 — same call shape as everything else
chord(48, CHORD_MAJ7, INSTR_EPIANO, 5);
```

This is the highest-leverage move in the whole doc against the §1 rule: the timbre ceiling
goes through the roof while the learner-facing surface stays tiny. The §5.4 "instrument
bank + ADSR" path is about letting carts *build* timbres; this path is about *shipping*
finished ones. For our audience, shipping finished ones wins — keep each engine's ~30 internal
params out of the API, but **not** behind a wall of frozen presets: expose a *fixed, tiny,
engine-agnostic* macro set (§8.1.1) so carts can still play the timbre live. Named per-engine
presets (`INSTR_ORGAN_JAZZ`, `INSTR_EPIANO_RHODES`) then become just baked macro positions.

### 8.1.1 The macro refinement — 3 live knobs per engine, not 0

> Amends §8.1/§8.6's original "expose 0 params, ship frozen presets" stance. The fear that
> stance guards against is real but narrower than "0 params": it's the *per-engine named param*
> explosion (`organ_drawbar()`, `epiano_bark()`, `piano_hardness()` …), which grows the API
> `O(engines × params)`. The escape is to keep the count **fixed and engine-agnostic** — not
> to expose nothing.

Borrow the discipline from Mutable Instruments **Plaits**: dozens of wildly different synthesis
engines, and *every one* exposes the **same three normalized controls** — **HARMONICS, TIMBRE,
MORPH** — and nothing else. Each engine internally maps those 0..1 inputs onto whichever 3–5 of
its own internal params matter most. The surface for engine #1 and engine #20 is identical.

**This is not theoretical — it's the MicroFreak.** Arturia's MicroFreak digital oscillators are
built on Mutable's open-source code (the Plaits/Braids lineage; Émilie Gillet's engines are
MIT-licensed), and the front panel surfaces exactly this idea as three continuous knobs
(**Wave / Timbre / Shape**) that mean something different per engine. A hardware synth people
love to *play* settled on three macros per engine — that's the strongest validation we could
ask for. We'll use the Plaits names (harmonics / timbre / morph). Three it is.

So the rule for the navkit port becomes: **every `INSTR_*` engine exposes the same three
normalized macros — `harmonics`, `timbre`, `morph` — and no per-engine params.** The surface
is `O(1)`: it does not grow when the 8th engine lands.

#### The surface (reuses the existing handle + slew + CV path)

We already ship live, slew-smoothed, CV-able setters on held voices (`note_cutoff`, `note_res`,
`note_duty`, `note_pitch`, …) — modrack's VOICE drives them off a knob and a CV cable today. The
macros are three more destinations on that *same* machinery; no new plumbing concept:

```c
// baked into a slot (the instrument template)
void instrument_harmonics(int slot, float x);   // 0..1
void instrument_timbre   (int slot, float x);   // 0..1
void instrument_morph    (int slot, float x);   // 0..1

// live on a held voice — the game / a knob / a CV cable is the modulator
void note_harmonics(int handle, float x);        // 0..1, slewed
void note_timbre   (int handle, float x);        // 0..1, slewed
void note_morph    (int handle, float x);        // 0..1, slewed
```

Three macros × {slot, live} = six functions, **forever** — bounded, because the count never
scales with the number of engines. (If even six bites, the `which`-index form already used by
`instrument_lfo` is an option — `note_macro(h, MACRO_TIMBRE, x)` — but named reads better for
beginners and there are only ever three.)

#### Where the taste lives

The richness of the ~30 internal params doesn't vanish — it moves from *API* into a tiny
**per-engine mapping function** the porter writes once, clamped to the good-sounding range so
the instrument stays "impossible to make ugly":

| Engine | harmonics | timbre | morph |
|---|---|---|---|
| **Organ** | drawbar registration (sub / 8′ / 4′ / 2′ mix) | brightness / drawbar tilt | percussion + internal vibrato/chorus scanner depth *(Leslie is a separate shared effect — §8.3, not a macro)* |
| **Electric piano** | partial spread (Rhodes↔Wurli) | pickup growl / bark amount | bell↔tine balance |
| **Acoustic piano** | unison detune / string spread | strike hardness | dispersion ("expensive"-ness) + damping |
| **Strings** | section size (1 player↔ensemble) | bow brightness | bow pressure / attack bite |
| **Mallet** | inharmonicity (marimba↔bell) | strike position | decay length (celesta↔vibe) |

That table is *code, curated by taste* — not API surface. The beginner gets three knobs that
always "make it more interesting"; the porter decides which internal params each one sweeps.
**Infinite internal richness, constant external surface** — the actual middle ground.

#### Composes with the four axes (§10), and with modrack

The macros sit *on the engine/oscillator*; the shipped ADSR / LFO / `instrument_filter` axes
still wrap *around* it. So an organ can have `note_morph` ramp the Leslie up **and** an
`LFO_CUTOFF` wah **and** a live `note_cutoff` — all on one handle, all slewed. In modrack the §8
engines become a Plaits-style **MACRO voice** with three CV inlets (HARMONICS / TIMBRE / MORPH);
patch an LFO or envelope into MORPH and the organ swells on its own.

> **Shipped (2026-06-05): modrack's MACRO module.** `eng` knob picks plk/mlt/fm (slots 23-25),
> har/tmb/mor knobs + h/t/m CV inlets (CV *adds* to the knob, full-range — ATTN sets depth).
> Strike-shaping macros are pushed to the slot just before `note_on` (queue order makes them
> land per-strike even with several MACROs sharing an engine slot); live macros stream via
> `note_harmonics/timbre/morph` while the gate is held. "Macro voice" preset = Turing epiano
> with an LFO swelling MORPH (FM feedback) on its own.

This does not change the first-bite plan: `INSTR_ORGAN` still ships buffer-free — it just ships
*with* its three-macro mapping instead of as a single frozen preset.

### 8.2 The one architectural decision: buffer-free vs. buffered

> **Decided (2026-06-03): yes — a `Voice` may carry a small per-voice delay line (~2 KB).** The
> organ's internal vibrato/chorus scanner wants one anyway, and the *same field* is reused across
> the whole buffered family below (pluck / piano / guitar), so committing once pays for all of
> them. This retires the "decide one thing first" gate — the §8.5 ordering still holds (ship the
> buffer-free drawbar/modal cores first), but the buffered engines are no longer blocked on a
> pending decision.

The only thing that divides "drop in tomorrow" from "decide one thing first" is whether a
voice has to carry a **delay-line buffer**.

**Buffer-free** — just a few phase accumulators; fits today's tiny `Voice` unchanged, no
architecture change, no API change. Band-limitable (cap harmonics at Nyquist → no aliasing):

| Instrument | navkit technique | Per-voice | Character |
|---|---|---|---|
| **Hammond B3 organ** | 9 additive drawbar sines + key-click noise + percussion + tonewheel crosstalk | ~160 B | ★★★★★ the bargain |
| **Electric piano** (Rhodes/Wurli/Clav) | 12 inharmonic sine modes + pickup nonlinearity (the growl) | ~280 B | ★★★★★ warm/vintage |
| **Mallet** (marimba/vibes/celesta) | 4 modal sines + strike weighting | ~60 B | ★★★★ celesta = music-box, very silent-movie |
| **Additive** (bell/strings/choir/brass) | up to 16 sines, per-partial decay | ~120 B | ★★★ versatile pad |
| **FM** (2–3 op) | DX-style bells/chimes | ~50 B | ★★★ cheap but expert to dial |

**Buffered** — needs a per-voice Karplus-Strong/waveguide buffer. Cost is **memory only**
(8 voices × 2 KB = 16 KB — trivial on desktop); CPU is fine (~30% worst case at 8 voices).
Say "a Voice may hold a small buffer" once and the whole physical-modeling family unlocks:

| Instrument | navkit technique | Per-voice | Notes |
|---|---|---|---|
| **Acoustic piano** | Stiff-Karplus + allpass dispersion chain | ~2–4 KB | **the literal "silent-movie pianist."** Dispersion is the secret that makes it sound expensive; buffer shrinkable to 1024 |
| **Guitar / harp / sitar** | Karplus-Strong + body biquads + jawari buzz | ~2.2 KB | one engine, many presets |
| **Bowed violin/cello** | dual waveguide + stick-slip friction | ~8 KB | gorgeous sustain; heaviest; optional |

### 8.3 What specifically sells the theatre-organ / silent-film vibe

- **Hammond organ + a simplified Leslie.** The Hammond sound is ~70% Leslie rotary, ~25%
  drawbars. The organ engine itself is ~160 B/voice. **The Leslie is an *effect*, not part of
  the oscillator** — that's how navkit factors it (`engines/effects.h`) and it's the right call
  here too: physically it's a speaker sim that sits *after* the tonewheels, it's a single
  ~1.5 KB buffer **shared across all voices** (amplitude + slight Doppler mod, slow/fast speed),
  not per-voice, and it'd run just as well on a Rhodes or a guitar. So: implement it as a
  standalone shared DSP block, decoupled from the engine.
  - *The open decision is exposure, not structure.* A general cart-facing **effects bus** is a
    new concept, and the §8 thesis is "no new concept." So for now the organ slot **auto-routes**
    through the Leslie and its speed rides a control we already have (a macro, or a dedicated
    slow/fast flag) — the cart never sees an "effect." When the §8.5-phase-4 character effects
    (chorus / drive / bitcrush) land as a real layer, this block folds straight into it.
  An organ *wants to sustain*, so it pairs naturally with the held-channel model (§6).
- **Acoustic piano** (KS + dispersion) for the pianist themselves.
- **Formant filter** for "singing"/choir/vocal-organ color — 4 bandpass peaks, ~512 B of
  vowel tables + ~8 floats/voice, reusing a state-variable filter. Bonus: that **same SVF is
  the resonant `filter()` from §5.5** — build one filter primitive, get the SID sweep *and*
  vowel formants from it.

### 8.4 SCW tables — a complementary cheap lever (not a replacement)

> **Partially SHIPPED (2026-06-04) as `wave_set` + `INSTR_USER0..3`:** four 64-sample
> single-cycle tables a cart can fill (and a `wave editor` cart that lets you DRAW them —
> with a live-morph drone, since the table is read per-sample). That's the cart-authorable
> half of this idea. The other half — a curated bank of ~600-sample SCWs *sampled from real
> instruments* (richer, less aliased) — remains open and would slot in the same way.

navkit also embeds **single-cycle waveforms** (one cycle of a real instrument as a small
float table; ~600 samples ≈ 2.4 KB each). A hand-picked bank of ~6 (≈8–10 KB) plus ~100
lines of phase-accumulator + cubic-interpolation playback gives instantly richer *static*
timbres. But SCW gives a rich *snapshot*, not the *living behavior* — the EP pickup growl,
the organ key-click, the piano's inharmonic dispersion — that makes the modeled engines feel
alive. Treat SCW as a cheap timbre-source lever (it could even feed an organ drawbar), not as
a substitute for the modeled instruments. Aliasing at high notes is acceptable/retro here.
*Build-order decision — when to invest in this vs. the engines: §13.*

### 8.5 How this reframes the roadmap (§7)

A curated instrument set is a *better* first move than the instrument bank, because it needs
**no new concept** — beginners keep calling `note`/`chord`/`tone`. The per-voice buffer is now
decided (§8.2), so the old buffer-free/buffered gate no longer drives the sequence; instead, lead
with the engine that proves the most for the least code. Suggested ordering, each phase
independently shippable:

1. ~~**`INSTR_PLUCK`** (Karplus-Strong) + the fixed macro surface (§8.1.1 — 6 fns total,
   independent of engine count).~~ **→ SHIPPED 2026-06-05** — per-voice KS buffer (§8.2) made
   concrete, full macro surface (`instrument_harmonics/timbre/morph` + `note_*` twins), `pluck`
   showcase cart. Station retrofit (jangle/bossa) is the open follow-up.
2. ~~**`INSTR_MALLET`/`INSTR_CELESTA`** — buffer-free modal; the music-box / silent-movie voice.~~
   **→ SHIPPED 2026-06-05** — one id (`INSTR_MALLET`), buffer-free (4 modal sines + strike
   click, ~10 floats on `Voice`, no delay line), full macro surface (harmonics = bar material
   wood→bell, timbre = mallet hardness, morph = ring length + the motor tremolo in its top
   third), `mallet` showcase cart with the 5 hardware presets as baked macro positions.
   Built by the §8.8.2 playbook (its first full walk). Open tail: macro taste-tuning by ear
   + the lowend/ambient/ymo retrofits (`radio-instrument-options.md`).
3. ~~**`INSTR_FM`** (2-op + feedback, buffer-free) — promoted ahead of the organ
   2026-06-05: the demand is on the dial today (citypop's Rhodes overtone, the epiano gap
   blocking the whole Italo/AOR batch, gamelan's bronze, exotica's bells), while nothing
   ships blocked on drawbars — and the organ's best self wants the Leslie (the effects-layer
   bite) anyway. Crucially this is §12 gap **2a** only — a self-contained engine behind one
   wave id, NOT the deferred second-oscillator plumbing (gap 2b). Design: §8.8.3.~~
   **→ SHIPPED 2026-06-05** (same-day as the §8.8.3 design — the playbook's second walk):
   `INSTR_FM` with the snapped ratio table, in-note brightness decay, feedback morph;
   ~2 floats on `Voice` (the carrier rides `v->phase`, so the pitch machinery is satisfied
   by construction). `fm` showcase cart: epiano/bell/bass/brass/clang presets + a live
   scope drawing the engine's actual formula. Open tail: taste-tuning by ear — **brass is
   the named stress test** (it wants the index to *rise* on attack; the §8.8.3 follow-amp-env
   alternative is the prepared answer if it fails) — and the DX-tine question for epiano.
   **Ear verdict (owner, 2026-06-05): epiano is close but "not exactly DX Rhodes" — the
   §8.8.3 tine prediction confirmed.** → **Tine tried the same day**: ~10 lines inside
   `sound_fm_sample`, a quiet 14× ping triple-contained so it can't leak into other
   presets (gated to the 1:1 ratio detent · dies in ~75ms · scaled by timbre and by
   1−morph, so soft strikes and feedback-heavy patches never hear it) — the §8.1.1
   pattern of taste living inside the mapping fn, like the mallet's motor only waking at
   morph's top. → **Second ear pass, same day: "now it really sounds like the FM
   epiano" — nameplate test PASSED.** The tine ships as-is; the fm cart's scope draws it
   (the 14× flicker on strike). This closes the epiano taste loop and greenlights the
   citypop/lowend Rhodes retrofits with confidence. → **The brass stress test also
   resolved the same day** (§8.8.3 post-ship findings): the in-note decay was backwards
   for horns, so brightness now **follows the amp attack** — a 70ms-attack slot speaks
   like a horn, instant-attack patches byte-identical. Both named risks closed; the FM
   tail is plain taste-tuning. (Already exercised in anger: yacht.c's epiano comps,
   tr909's clang hats, game-music's hard-bop combo recipe.)
4. **`INSTR_ORGAN`** — drawbars → scanner, buffer-free core. *(2026-06-05: the Leslie is
   deferred — ships later as the effects/bus layer's first instance, §8.10; the morph macro's
   scanner vibrato carries the motion until then.)* Pilots the held-notes + macros-as-CV
   surface the whole wind family (steps 7/9) will reuse. The resonant SVF still rides along —
   it's the reusable primitive that also gives §5.5 and §8.3's formant.
5. **`INSTR_EPIANO`** — promoted to its own step by the navkit verification (2026-06-05):
   **buffer-free** — a pure 12-mode modal bank + pickup nonlinearity (~296 B/voice, no delay
   line; the cost table had this right all along), so it's a *mallet-sized* job, not a
   pluck-sized one — and one engine covers Rhodes/Wurli/Clav via `pickupType` (see §8.7).
   Its case is the *electromechanical* corner FM can't reach: Wurli reed bark, Clav pickup
   pluck, mark-I Rhodes growl; the FM tine keeps the DX corner. (Clav + the §8.10 wah is the
   named endgame combo.)
6. **`INSTR_PD`** (Casio CZ) — the snack between bigger ports: 2 floats, buffer-free,
   resonant sweeps with zero filter. Slot it in whenever a small win is wanted.
7. **`INSTR_REED`** (clarinet↔sax) — first true wind; one bore line fits `ks_buf` as-is,
   zero architecture work. *(The swap clause is moot: FM's brass stress test **passed**
   2026-06-05 — §8.8.3 post-ship findings — so reed keeps this slot; waveguide Brass
   stays a catalog row for when a real lip model is wanted.)*
8. **`INSTR_MEMBRANE`** — tabla/conga/djembe, ~100 B mallet-pattern port; hand percussion
   (strike-pos + pitch-bend) for the world-music stations.
9. **Bowed / Pipe** — after the organ proves the held-note surface, and pending the
   one-buffer-pack verification (§8.9 rows). Then **`INSTR_PIANO` (StifKarp) /
   `INSTR_GUITAR`/`INSTR_HARP`** — the genuinely buffered pair (piano's `ks2Buffer[2048]`
   second string, guitar's KS string + body resonator) on the pluck-validated path.
10. **Formant filter** + the **effects layer** (§8.10 — buses vs. master; reverb / delay /
    tape / leslie / wah, starting with one master reverb + the formalized bus concept).
    **Additive stays deferred** — `INSTR_SINE` + FM cover its near corners today, and it
    subsumes the MT70/sine family when it lands (§8.9 note). SCW bank (§8.4) remains optional.

### 8.6 Cons / watch-outs

- **Don't expose the *named* internal params** (no `organ_drawbar()`), but **do** expose the
  fixed three-macro set (§8.1.1) — harmonics / timbre / morph, identical across every engine.
  The moment a beginner sees "drawbar 4 = 6" the simplicity is gone; three normalized "make it
  more interesting" knobs keep it. Frozen `INSTR_*_JAZZ` presets are just baked macro positions.
- **Porting = lift the oscillator fn + its tuning constants.** The engines are self-contained
  (no malloc, no dependency on navkit's sequencer/effects bus), so each is a clean
  copy → shrink → rename into `sound.h`. Buffered ones bring their buffer — the only
  structural change.
- **Pairs with held channels (§6).** Sustained organ chord + Leslie underneath a fire-and-forget
  piano melody = a whole silent-film score using both the event and state models.

### 8.7 navkit source pointers (for when we port)

> Paths verified 2026-06-05 — navkit lives at `../navkit`, and everything below sits under
> `navkit/soundsystem/` (the bare `engines/…` paths in earlier drafts were missing that prefix).

- Organ: `soundsystem/engines/synth.h` (`OrganSettings`, `:925`) +
  `soundsystem/engines/synth_oscillators.h` (`initOrganSettings` `:4036`,
  `processOrganOscillator` `:4064`); plan in `soundsystem/docs/done/organ-engine-plan.md`
- Electric piano: `processEPianoOscillator` (`synth_oscillators.h:3937`, settings init `:3663`).
  **Buffer-free (verified 2026-06-05):** `EPianoSettings` (`synth.h:909`) is a pure 12-mode
  modal bank (`EPIANO_MODES` ratios/amps/decays/phases) + pickup nonlinearity + DC blocker —
  no delay line anywhere in struct or process fn. ~296 B/voice; mallet-family port cost.
  **Wurlitzer (`#111`) and Clavinet (`#112`) in `instrument_presets.h` are parameterizations of
  this same EP engine** — three pickup models built in: `EP_PICKUP_ELECTROMAGNETIC` (Rhodes),
  `EP_PICKUP_ELECTROSTATIC` (Wurli), `EP_PICKUP_CONTACT` (Clav) — so porting the EP engine
  brings Rhodes/Wurli/Clav as baked macro positions, the §8.8.2 mallet-preset pattern.
  (`FM Clav` `#134` is an *FM* preset — try recreating it on the shipped `INSTR_FM` first.)
- Acoustic piano: `processStifKarpOscillator`
- Guitar: `processGuitarOscillator`; Bowed: `processBowedOscillator`; Mallet: `processMalletOscillator`
- Winds (all in `synth_oscillators.h`): Reed `processReedOscillator` (`:587`, `ReedSettings`
  `synth.h:860` — one `boreBuf[1024]`); Brass `processBrassOscillator` (`:725`, `BrassSettings`
  `synth.h:883` — one `boreBuf[1024]`); Pipe/flute `processPipeOscillator` (`:496`,
  `PipeSettings` `synth.h:838`); Bowed `BowedSettings` `synth.h:818` (nut+bridge lines)
- Phase distortion (CZ): `processPDOscillator` (`:1508`, `PDSettings` `synth.h:425` — 2 floats)
- Leslie: `soundsystem/engines/effects.h` (`:77` constants, params `:271`); **Wah / auto-wah:
  same file (`:68` — swept bandpass, LFO + envelope-follow modes, per-bus in navkit's DAW)** —
  maps straight onto the §8.10 wah row (the per-voice SVF, 4th use of the one filter).
  Formant spec: `soundsystem/docs/vocoder-formant-effect.md`
- SCW data + embed tool: `soundsystem/engines/scw_data.h`, `tools/scw_embed.c`
- Calling convention: set `voice.wave`/`frequency`/envelope, then per sample
  `sample = processXxxOscillator(&voice, sampleRate)` — no heap, all state in-struct.

### 8.8 Port sketch — macro plumbing + three engines + the Leslie effect

Concrete shape for the first bite, grounded in today's `sound.h` (mix loop ~`:467–550`, the
stateless `sound_osc` `:176`, ctrl-kind dispatch `:360–437`). **Three touch points; everything
else is unchanged.**

**(a) Macros** — 3 floats on `Instrument`, current+target on `Voice` (mirrors
`flt_cutoff`/`cutoff_target`), two new ctrl kinds, six named functions, three lines in the
existing slew block (`:478`):

```c
// Instrument:  float harmonics, timbre, morph;            // 0..1, meaning is per-engine
// Voice:       float harm,timb,mor,  harm_target,timb_target,mor_target;
// kind 18 = instrument_macro(slot, which 0/1/2, val*1000);  kind 19 = note_macro(handle, which, val*1000)
void instrument_timbre(int slot, float x);   void note_timbre(int handle, float x);   // + harmonics / + morph
// slew:  v->harm += (v->harm_target - v->harm) * 0.002f;   // and timb, mor
```

Named API (only ever six functions), one `which`-indexed code path inside — rides the handle +
slew + CV machinery `note_cutoff` already uses; modrack gets three CV inlets for free.

**(b) Engine fork** — `sound_osc` keeps the 5 raw waves; engine ids dispatch to stateful
generators that read the macros. One line at `:534`:

```c
float s = (v->wave >= INSTR_ENGINE_BASE) ? sound_engine(v, v->harm, v->timb, v->mor)
                                         : sound_osc(v->wave, v->phase, duty, &v->noise_state);
```

**Buffer-free engines** (drop in now, no `Voice` growth — phase 1):

```c
// ORGAN — 9 drawbars derived from v->phase (fixed Hammond ratios). ~25 lines.
//   harmonics = registration  (mellow 8′  ↔  bright full spread)  — REG_MELLOW→REG_BRIGHT lerp
//   timbre    = brightness     (per-partial expf rolloff)
//   morph     = percussion strike + internal vibrato/chorus scanner depth
//               (drawbar core is buffer-free → ships first; the scanner taps the small per-voice
//                delay from §8.2 — the same buffer field the pluck/piano engines reuse)
float sound_organ(Voice *v, float harm, float timb, float mor);

// MALLET (marimba / vibes / celesta) — 4 inharmonic modal sines + a strike-noise transient. ~15 lines.
//   harmonics = inharmonicity  (wood ratios 1·3.9·9.2  ↔  metal/bell 1·4.1·10.7)
//   timbre    = strike hardness (weights upper partials + the noise-click amount)
//   morph     = ring length     (celesta short  ↔  vibe long; the long end adds the vibe motor tremolo)
float sound_mallet(Voice *v, float harm, float timb, float mor);   // decay from step_samples; reuses v->phase + noise_state
```

**Buffered engine** — `PLUCK` (the guitar/harp family) is the *first* engine that needs a
per-voice delay line: **this is the one §8.2 architectural decision, made concrete.**
Karplus-Strong = excite a delay line of length `SR/freq` with noise, run it through a feedback
lowpass:

```c
// Voice gains:  float ks_buf[KS_MAX]; int ks_len, ks_idx; bool ks_init;  // ~2KB/voice (KS_MAX≈1024 caps the low end)
// PLUCK — pitch comes from the buffer LENGTH, not v->phase.
//   harmonics = damping / sustain (short pluck  ↔  long ring)  — feedback coefficient
//   timbre    = pick brightness    (lowpass on the initial noise burst)
//   morph     = pick position      (comb/allpass tap into the line)
float sound_pluck(Voice *v, float harm, float timb, float mor);
```

Modeled engines treat the slot's ADSR as an **optional override, not a replacement** — they bake
their own decay (mallet ring, pluck damping), and a user ADSR layered on top flattens the very
motion that sells them (§10).

**(c) Leslie** — a shared *effect*, not per-voice (matches navkit's `engines/effects.h`, per
§8.3). Split organ voices into a sub-bus, run it through one shared rotary, add back:

```c
// #define LESLIE_LEN 512   // one shared ~2KB delay line — NOT per voice
typedef struct { float buf[LESLIE_LEN]; int widx; float horn_ph, drum_ph, speed, speed_target; } Leslie;
float leslie_process(Leslie *L, float in);   // 2 rotors → amplitude tremolo + doppler delay-mod;
                                              // speed slews slowly (the ~1s spin-up/down IS the magic)
// in the voice loop (:540):   if (v->wave == INSTR_ORGAN) organ_bus += out_s; else mix += out_s;
// after the loop   (:547):    mix += leslie_process(&leslie, organ_bus);
```

`leslie.speed_target` stays internal (auto, defaults slow) — no cart-facing effects API until the
§8.5-phase-4 effects layer, into which `leslie_process` folds cleanly (and `organ_bus`
generalizes to a per-instrument send). Mono today; stereo horn/drum panning is the upgrade when
§9's stereo question resolves.

**Cost:** macros ≈ 6 floats + 2 ctrl kinds + 6 one-liners; organ drawbars + mallet are
buffer-free; the organ scanner and the `PLUCK`/piano/guitar family share one small per-voice
delay line (§8.2 — decided). Leslie is a separate *shared* ~2KB buffer.

### 8.8.1 What shipping engine #1 taught (PLUCK, 2026-06-05) — rules for engine #2

Empirical, from one day of building and listening. Each bit of taste below cost a
"the knob doesn't do anything" or a "what's that pop?" to find:

1. **Macro mappings must be perceptual, not parametric.** The first harmonics
   mapping was linear in the feedback coefficient (`0.985→0.9999`) — physically
   reasonable, *audibly dead*: ring time is exponential in feedback, so 3/4 of the
   knob did nothing. The fix: the knob sets an exponential **target quantity**
   (T60 ≈ 0.04s → ~2min) and the coefficient is derived per note, frequency-
   compensated. The test for every future macro: **every quarter-turn must be an
   audible step**, on every note of the keyboard. (Mallet's "ring length" and
   "inharmonicity" macros will hit the identical trap — map the *percept*.)
2. **Pitch is never a macro — engines must consume the pitch machinery.** The
   pressure to put bends/vibrato on a knob appeared within hours ("can harmonics
   do the pitch stuff?"). The right move was making the engine honor the
   per-sample `pitch_mul` (LFOs/envs) and the slewed `v->freq` (`note_pitch`/
   `note_glide`) — for KS, a fractional-delay read tap. Payoff: jangle's chorus-
   warble recipe worked on the real string *verbatim*, zero new API. Engine #2's
   oscillator must take `pitch_mul` from day one.
3. **Engines that ring expose every hard cut.** Voice-steal pops existed all
   along; short quiet envelopes hid them. A full-amplitude 30s pluck doesn't. The
   `steal_tail` declick (~3ms fade paid at steal/`sfx(-1)` time) is now in the mix
   loop and covers all future ringing engines.
4. **Self-decaying engines need generous gates.** The engine bakes its own decay
   (§8.8's "ADSR is an override"), so a short gate/release *chops* it — the pluck
   cart ships `release 1200ms` and scales `hit()` durations with ring time. Open
   refinement when polyphony pressure appears: voice self-silence detection (free
   the voice when buffer energy ≈ 0, instead of holding the full gate).
5. **Grow `soundcheck.c` in the same commit as the surface.** The walk now plays
   the engine audibly and pushes the macro kinds (21/22) through the queue at
   worst-case load — a request kind the self-test never fires is a kind the
   tripwire never guards.

### 8.8.2 The engine-shipping playbook (validated by pluck + mallet)

The §8.8.1 lessons, turned into the **standard procedure for every new engine**.
Walked twice (PLUCK 2026-06-05, MALLET 2026-06-05); organ/EP/piano follow this
path. The steps in order — none optional, most are one sitting:

1. **Design the macros on paper first.** The engine's row in the §8.9 catalog
   (or the §8.8 sketch) must say what `harmonics` / `timbre` / `morph` mean
   *before* any code — and per §8.1.1 they are the ONLY knobs; no per-engine
   named params, ever. If two doc passes disagree on a mapping (it happened:
   mallet's timbre was "strike position" in §8.1.1 but "hardness" in §8.8),
   resolve it now, in the doc, not in the code.
2. **Port the oscillator** (navkit pointers: §8.7). Two functions: a note-on
   excitation (`sound_<engine>_start`) and a per-sample generator
   (`sound_<engine>_sample`), dispatched from `sound_setup_note` and
   `sound_engine_sample`. State lives on `Voice` — buffer-free if possible
   (mallet: ~10 floats); the shared per-voice delay (§8.2) if waveguide.
   Non-negotiables, each one a §8.8.1 scar:
   - **consume the pitch machinery from day one** — derive frequency from
     `v->freq * pitch_mul` *per sample*, so vibrato / pitch-env / `note_pitch`
     / `note_glide` work verbatim. Pitch is never a macro.
   - **map percepts, not parameters** — ring/decay knobs set an exponential
     target quantity (T60), brightness knobs a pow-shaped weight; the test is
     "every quarter-turn audibly steps, on every key."
   - **normalize the excitation** — every macro position strikes at the same
     loudness, or the knobs read as volume controls.
   - **self-decay means the ADSR is an override** (§10.4) and gates must be
     generous (the showcase cart scales its `hit()` duration with the ring
     knob); guard the engine-id-without-note-on case (an sfx step naming the
     id) so it stays silent instead of reading stale state.
3. **Wire the id in all four places** — `studio.h` (`INSTR_*` + house-style
   one-liner, plus a clause in each of the six macro one-liners),
   `studioDocs.js` (the constant's entry + the three `instrument_*` macro
   entries), `shell.js` (the keys array). Run `gen-tcc-symbols.js` in the same
   commit. An engine adds **zero functions** — if you're adding one, stop and
   re-read §8.1.1.
4. **Grow `soundcheck.c` in the same commit** — the engine gets its own slot
   and an audible step in the wave walk; then the tripwire run
   (`node tools/play.js soundcheck script /dev/null --headless --frames 900`,
   silence = PASS) is also the engine's compile + queue test.
5. **Build the showcase cart = the tuning rig** (pluck.c / mallet.c are the
   template): keys play it, the three macros are draggable sliders that
   *audition while dragging*, autoplay keeps it sounding, `watch()` the knobs
   under `DE_TRACE`. **Give it preset keys named after hardware** — presets
   are nothing but baked macro positions (§8.1), which makes them the
   acceptance test: *if pressing "vibraphone" doesn't sound like a vibraphone,
   the mapping is wrong, not the preset.* Bake the screenshot, register in
   index.json, `lint-carts`.
6. **Tune by ear in that cart.** Iterate the mapping function (the per-engine
   taste table lives in `sound.h`, never in API), rebuild, listen. This step
   is the human's; everything before it just makes the loop fast.
7. **Retrofit a shipped station** — `radio-instrument-options.md` keeps the
   ranked target list per engine (mallet: lowend's vibraphone lead is "the
   single highest-value engine swap"). Wire a live A/B toggle against the old
   fake (the G-key pattern from jangle/bossa) so the verdict is one keypress.
8. **Update the ledgers** — STATUS item 5, the §8.5 phase list, §13's order of
   work, and a recipe in `game-music.md` once the macros have settled.

### 8.8.3 Engine #3: FM — the step-1 design (2026-06-05)

The playbook's paper round for `INSTR_FM`, resolving the §8.9 row into buildable taste.
**Scope guard first:** this is §12 gap **2a** — a self-contained engine whose second
oscillator is an *inaudible phase-wiggler* sealed inside the sample function (you only
ever hear the carrier). It is NOT gap **2b** (second *audible* oscillator on the generic
voice path — Juno saw+square mix, unison detune), which stays deferred until the Juno-60
cart defines it. Architecture: **2-op + feedback** — the Plaits/MicroFreak FM engine's
shape, not the DX7's 6-op/32-algorithm panel (which is exactly the per-engine-param
explosion §8.1.1 exists to forbid). The DX *sound family* — epiano, glass bells, solid
bass, brassy growl, clang — is the target; the architecture is not.

| macro | maps to | the taste decision (made here, on paper) |
|---|---|---|
| **harmonics** | carrier:modulator **ratio** | **SNAPPED to a curated table** (~10 entries: 0.5 · 1 · 1.5 · 2 · 3 · 3.5 · 4 · 5 · 7 · 14), never continuous — a continuous ratio is out-of-tune clang everywhere except the integers. Integer entries = harmonic family (bass/epiano/brass); non-integer = bells/clang; 14 is the DX tine. Each detent is a *different instrument* — the strongest possible quarter-turn audibility |
| **timbre** | **mod index** (peak FM amount) | the index must **decay within the note** (bright strike → mellow tail — the DX epiano/bell signature; a static index reads as cheap organ). Bake an exponential settle toward a ~25% floor over ~0.9s; engines own their timbral motion (the pluck/mallet precedent). Open alternative if brass suffers: scale the index with the voice's own amp envelope instead (sustained ADSR = sustained brightness) — decide in the cart |
| **morph** | modulator **feedback** | 0 = pure two-sine FM; up = the modulator self-saturates toward saw → growl → at the top, noisy clang (useful percussion territory). One knob, huge range |

Known risk, named up front: **DX7 E.PIANO 1 is two op-pairs** (1:1 body + a faint 14:1
tine ping). If the cart's epiano preset fails the nameplate test on pure 2-op, the fix is
a small *internal* third op (navkit's `FM_ALG_PAIR` is the crib — mod1→carrier plus an
additive ping), still zero API. Don't build it speculatively.

**Post-ship ear findings (2026-06-05, both fixed same day):**
- **Brass:** the in-note decay is *backwards* for horns — FM brass SWELLS into brightness.
  Shipped answer (a variant of the follow-amp-env alternative above): **brightness follows
  the amp ATTACK** — `beta` ramps over the slot's attack samples, so a 70ms-attack slot
  speaks like a horn while instant-attack patches are byte-identical. Corollary: an FM
  *patch* is macros **+ an ADSR** (this engine deliberately doesn't bake amplitude), so the
  fm cart's presets carry both — the nameplate contract is unchanged.
- **Clang:** pointing it at ratio **14 was a theory error — integer ratios are HARMONIC**
  (sidebands at f0(1±14k) are all multiples of f0 → periodic → buzzy organ-bright, never
  metal). Metal lives on the NON-integer detents (3.5 in the table). Clang now sits on
  3.5 with feedback cranked; the 14 detent remains useful as sparse harmonic shimmer.
  Rule for future engines: **inharmonicity, not ratio height, is what reads as metal.**

Mechanics: buffer-free (~3 floats on `Voice`: mod phase, feedback sample, plus nothing —
the index decay derives from `step_samples`). No note-on excitation buffer → no stale-state
guard needed (sines from any phase are safe). Pitch: carrier = `freq × pitch_mul` per
sample, modulator = carrier × ratio — the §8.8.1 rule satisfied by construction. Cart
presets (= the acceptance tests): **epiano · bell · bass · brass · clang**.

### 8.9 Candidate engine catalog (running wishlist)

The set we'd *like*, beyond the first-bite engines (§8.5). Adding one is mostly: port the
oscillator (§8.7), mark buffer-free vs. buffered (the per-voice delay from §8.2 is only for
Karplus/waveguide), and define its **3-macro mapping** — because the §8.1.1 discipline holds no
matter how the engine synthesizes, every row exposes the same `harmonics` / `timbre` / `morph`;
the table's only job is to say what those three mean for each. Grow it freely.

| Engine | navkit src (§8.7) | buffer | harmonics | timbre | morph | character |
|---|---|---|---|---|---|---|
| **Additive / sine** (bell, choir, brass, strings) | additive osc | free | # / spread of partials | spectral tilt (brightness) | per-partial decay + inharmonicity | rich multi-partial pads. (A *bare* sine is already `INSTR_SINE` — see the MT70 note below) |
| **FM** (2-op + feedback, DX) | `processFMOscillator` | free | carrier:modulator ratio (snapped table) | mod index (decays in-note) | feedback | DX bells, chimes, e-pianos, clang. Macros *are* the cure for "expert to dial". **Full design + post-ship findings: §8.8.3** — SHIPPED 2026-06-05 |
| **AM / ring mod** | trivial (≈10 lines native) | free | modulator ratio | AM ↔ ring depth | modulator detune / wave | metallic, robotic, clangorous bells |
| **Voice / formant** | formant SVF + buzz (§8.3) | free (reuses SVF) | vowel (a→e→i→o→u) | breathiness / brightness | formant shift (size/gender) | choir "aah", vocal-organ, talkbox. Comes near-free with the §8.3 filter |
| **Bowed string** (violin/cello) | `processBowedOscillator` (Smith/McIntyre waveguide) | nut+bridge lines, **sum = one period → likely packs into the one `ks_buf`** (split at the bow point; verify at port) | bow position (sul tasto ↔ ponticello) | bow pressure (smooth ↔ scratchy stick-slip) | bow velocity / swell | sustained strings that *speak* — attack scratch, swells. Wants held notes (§6); macros-as-CV is its natural surface |
| **Reed** (clarinet ↔ sax) | `processReedOscillator` (pressure-driven reed valve) | one `boreBuf[1024]` — **fits today's `ks_buf` as-is** | bore conicity (clarinet hollow-odd ↔ sax full) — literally navkit's `bore` param | reed stiffness (dark ↔ bright) | breath / aperture (soft ↔ overblown squawk) | the *blown* family's workhorse; klezmer to smoky jazz on one knob |
| **Pipe / flute** (Fletcher/Verge jet-drive) | `processPipeOscillator` | upper+lower bore halves (sum ≈ bore; same one-buffer pack as bowed) + tiny `jetBuf[64]` | overblow (fundamental ↔ octave flageolet) | breath noise (pure ↔ airy) | embouchure | airy flutes, pan pipes, organ-flue color; breathy attacks for free |
| **Brass** (lip-valve waveguide) | `processBrassOscillator` (2nd-order lip mass-spring + bore) | one `boreBuf[1024]` — **fits `ks_buf` as-is** | bore conicity (trumpet ↔ horn) | blow pressure (soft ↔ brassy blare — the rip/blare *is* the model) | mute (open ↔ harmon) | a real lip model, not an approximation. *(Was the prepared answer if FM brass failed its §8.8.3 stress test — FM passed, so this is no longer queued; port it when a station wants the genuine rip/blare)* |
| **PD / phase distortion** (Casio CZ) | `processPDOscillator` — **2 floats, 8 wavetypes incl. 3 resonant** | free (cheapest in the catalog) | wavetype (snapped detents, like FM's ratio table) | distortion amount (the CZ "DCW" sweep — filter-like brightness with zero filter) | saw ↔ reso window blend | CZ basses, synth-brass, the famous resonant sweeps; deeply chiptune-adjacent — strong identity fit, near-zero cost |
| **Membrane** (tabla/conga/bongo/djembe/tom) | `processMembraneOscillator` (`:1754`, `MembraneSettings` `synth.h:437` — 6 modal sines at circular-membrane Bessel ratios) | free (~100 B — mallet-family cost) | head character (tabla ↔ djembe mode spread / tension) | **strike position** (center thump ↔ edge ring — the model reweights modes physically; conga open/slap/mute in one knob) | **pitch-bend depth/decay** (the tabla bayan *glissando* — baked into the model) | hand percussion the analog 808/909 recipes can't reach — bend + strike-pos are exactly what sine+pitch-env approximations lack. World-music radio fuel (promoted from the census NO list 2026-06-05) |

> **Full navkit census (2026-06-05) — 23 engines in `soundsystem/engines/synth_oscillators.h`.**
> Shipped here: pluck, mallet, FM. Roadmapped: organ (next), EP + StifKarp-piano + guitar (§8.5
> step 5). Catalog above: additive, AM, voice/formant, bowed, reed, pipe, brass, PD, membrane
> (promoted from this NO list 2026-06-05 — the bend + strike-pos physics earn it). **Not
> taken** (revisit freely): `Granular`,
> `Metallic`, `Mandolin` (paired-course guitar variant — likely a guitar preset, not an engine),
> `Whistle`, `Bird` (chirp/trill/warble — charming as game-SFX, not music), `Shaker`
> (maraca/tambourine physical model), `BandedWG` (bowed bars / glass harmonica — exotic),
> `VoiceOscillator`/`VoicForm` (superseded by the §8.3 formant-SVF plan). The wind/bowed group
> (reed, brass, pipe, bowed) are continuous-excitation instruments — they pair with held notes
> (§6) and live macros the way the organ does, and reed + brass need **zero** new buffer
> architecture (one bore line ≤ `SOUND_KS_MAX`).

> **MT70 — resolved (verified in navkit `demo/songs/*.song`, 2026-06-03).** The "MT70" presets
> (Flute, Bells, Organ, Vibes, JzOrg2, …) are **all `waveType = sine`** — *not* a synthesis engine.
> Each is one pure sine wave shaped by ADSR + lowpass filter + (optional) vibrato; the names differ
> only in those settings. **So this needs no engine port — dreamengine already makes it** with
> shipped API. e.g. MT70 Flute ≈ `instrument(slot, INSTR_SINE, 50,100,5,120)` +
> `instrument_filter(slot, FILTER_LOW, 3500, 0)`. → Belongs as a **shipped preset / demo cart**
> (zero-setup `INSTR_*` presets, §5.4 / §10.1), not in this engine catalog.

> **Sine = Additive at `harmonics = 0`.** `INSTR_SINE` and the Additive engine aren't two things —
> additive *is* a sum of N sine partials, so one partial is a sine. That means the MT70/sine family
> are the harmonics-≈0 corner of the Additive row above, and the whole bank is a path through its
> three macros: **harmonics** (0 = pure sine → up = organ/string body), **timbre** (spectral tilt /
> brightness — the lowpass), **morph** (inharmonicity: integer-ratio = organ/flute ↔ stretched =
> bell/metallic, optionally + per-partial decay). So: Flute ≈ (harm 0, mellow, harmonic);
> Organ ≈ (harm up, harmonic); Bells ≈ (mid, bright, **inharmonic**); Vibes ≈ (inharmonic + fast
> decay). Ship the bare sine as `INSTR_SINE` now (no port); when the Additive engine lands it
> subsumes the family with live macros — same engine, just `harmonics` turned up off zero.

*Format for adding more:* just name the sound + a reference patch/track if you have one; the
buffer flag, navkit source, and macro mapping get filled in here.

> **Which engine next, scored by what it opens (2026-06-07, the tinydaws angle —**
> **see [`tinydaws.md`](tinydaws.md) "wider map" for the full cross-index):**
> rank by *kind of door*, not just cost. **Formant = the biggest single door**
> (the human voice — a whole category nothing on the dial touches: Toy-Town lead,
> choirs, doo-wop; near-free on the §8.3 SVF). **Reed = the biggest cart family**
> (the bellows trio in [`cart-library-direction.md`](cart-library-direction.md)
> §2b — melodeon/accordion/harmonica — plus fairground clarinet, klezmer, ska,
> hard-bop sax; one bore buffer, fits `ks_buf`). **Membrane = the rhythm-section
> unlock** (what the 808/909 analog recipes structurally can't do). **AM/ring =
> best leverage-per-line** (~10 lines = the Radiophonic Workshop's identity —
> should probably just happen). Two natural "next sessions": formant + AM/ring
> completes the Radiophonic rack end-to-end; reed alone completes the bellows trio.

### 8.10 Effects layer — buses vs. master (the §8.5-phase-4 plan)

> The effects wishlist + the routing model. Still **deferred** to §8.5 phase 4 (after the first
> engines ship) — begin small. The Leslie (§8.3/§8.8) is the first instance and sets the pattern.
> **Juno-60 raises the cost of this deferral:** the Juno's BBD chorus is *the* defining sound of
> the instrument — without it a Juno-60 cart is a dry generic pad, not a Juno. The BBD chorus is a
> short pitch-modulated delay (same building block as the Leslie's doppler component), so it would
> fall out of the Leslie/effects layer work rather than requiring its own separate engine. See §14.

**The routing model: unify on "bus"; master is just the default bus.** Rather than two concepts
(master FX vs. bus FX), there's *one* — a **bus** carrying a small fixed FX chain — and "master"
is bus 0:

- An instrument slot names a **bus** (default = master).
- **Aux buses (1..N):** voices route in → the bus's FX chain → summed into master.
- **Master bus (0):** the full sum → master FX → output.

This gives both behaviours from one mechanism: Leslie/wah on a per-instrument **aux** bus (only the
organ gets the rotary, only the lead gets wah); tape/reverb/limiter on **master** (everything
shares them). The `organ_bus` already in the §8.8 sketch *is* aux bus #1 — the general version just
lets any instrument pick a bus and any bus carry a chain. Generalizing §8.8's organ sub-bus:

```c
float bus[NBUS];                          // small fixed N; accumulators zeroed per sample
// voice loop:   bus[v->bus] += out_s;             // was: organ_bus += … / mix += …
// post-loop:    for (aux b) master += fx_chain(b, bus[b]);
//               out[i] = fx_chain(MASTER, master + bus[MASTER]);
```

**Surface discipline** (same as macros/Leslie — tiny, global-first): the smallest cart-facing step
is global **master** effects — `reverb(amount)`, `delay(ms, fb)`, `drive(amount)` — i.e. "effects
on master," matching the global `filter()` precedent (§5.5). Per-bus routing
(`instrument_bus(slot, b)` + per-bus setters) is the *next* increment, when a cart wants the lead
on its own wah without washing the whole mix.

**The wishlist.** "Shared" below means *one* processor instance (not 8 private ones), **not**
"applied to everything" — you pick *which* sounds feed it by routing them to a bus. So a delay on
just the lead = route the lead to an aux bus carrying delay (`instrument_bus(LEAD,1)`); everything
else stays dry. That's per-*instrument*, achieved with one shared delay line + bus routing — the
buffer-hungry effects (delay/reverb/tape) are shared-and-routed rather than per-voice purely on
memory cost (a per-voice echo line would be ~384 KB × 8 voices for an effect nobody wants 8 of).
The cheap exception is **wah** — it's just the per-voice SVF, so it genuinely *is* per-note.

| Effect | granularity | DSP | buffer | home | insert/send | notes |
|---|---|---|---|---|---|---|
| **Reverb** | bus / master | Freeverb (8 comb + 4 allpass) | shared ~20–40 KB | master / aux | send (dry+wet) | the big "space" |
| **Delay / echo** | bus / master | delay line + feedback + tone | shared (~SR·2) | aux / master | send | route one instrument to it; tempo-syncable to the beat clock |
| **Tape** | master | wow/flutter (modulated delay) + soft-clip drive + HF rolloff | shared small | master | insert | lo-fi character over the whole mix |
| **Leslie** | bus | 2 rotors: tremolo + doppler delay-mod (§8.3) | shared ~2 KB | aux (organ) | insert | already specced |
| **Wah** | **per-voice** | resonant SVF bandpass swept by LFO / envelope / expression | none — reuses SVF | per voice | insert | **4th use of the one SVF** — filter (§5.5) + formant (§8.3) + wah, build it once |

**Begin small:** when this layer opens, the minimal first bite is **one master reverb + formalizing
the bus concept** (so the Leslie stops being a hardcoded organ special-case). Delay / tape / wah
follow. Stereo (a §9 open question) matters more here than for engines — reverb/delay/Leslie are
where width lives — so resolve stereo before or alongside this layer.
