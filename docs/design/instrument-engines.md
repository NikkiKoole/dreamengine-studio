# Instrument engines — the navkit port program

The rich-instrument program: porting navkit's self-contained oscillator engines
(organ, electric piano, Karplus-Strong strings, mallets, FM, winds…) behind single
`INSTR_*` ids with the fixed three-macro surface (harmonics / timbre / morph), plus
the effects-layer plan. **Genre: design exploration.**

> ## ⏱ STATE — read this first (2026-06-10)
>
> Orient cheaply: current state + the next bite, so you don't have to read 1,400 lines to
> know where the program stands. (Below this header, much of §8/§8.8 is the *historical*
> port plan — useful for rationale, stale on status. The studio.h `INSTR_*` block is the
> source of truth for what's shipped; the per-engine §8.8.x "step-1 design" labels are
> historical and mostly mean SHIPPED now.)
>
> **Engines: the roster is COMPLETE — all 12 shipped + 1 experimental.** PLUCK · MALLET ·
> FM · ORGAN · EPIANO · PD · MEMBRANE · REED · PIPE · GUITAR · PIANO · BOWED, each behind an
> `INSTR_*` id with the fixed 3-macro surface (harmonics/timbre/morph). **VOICE** (formant,
> `INSTR_VOICE`) is EXPERIMENTAL — its public macro mapping isn't locked (driven raw via
> `voice_param()` in the voxlab prototype). What's left on engines is **tweaking/tuning**,
> not new engines. To add/tune one: its §8.8.x section + the shipping playbook §8.8.2.
>
> **Stereo (the effects-layer prerequisite): DONE.** Shipped 2026-06-09 (linear pan law,
> `instrument_pan`/`note_pan`/`LFO_PAN`); constant-power added as a gated opt-in `pan_law()`
> 2026-06-10. Full record: [`stereo.md`](stereo.md). The §8.10 gate is now open.
>
> **▶ Effects-bus layer (§8.10): STARTED. Reverb + chorus SHIPPED 2026-06-10.** The first bite
> **formalized the per-sample MASTER FX SECTION** (in `sound_callback`): an explicit, ordered,
> send/insert-aware block — SEND RETURNS (echo, reverb — each a shared processor with per-slot
> sends) then the INSERT CHAIN (chorus → … → soft-clip limiter last), with a documented (not
> built) side-chain seam. Rather than `bus[NBUS]` + per-slot aux routing up front.
> **Reverb** = a SEND bus modelled like the echo bus (`reverb(size, damping)` +
> `instrument_reverb`/`note_reverb`, navkit Schroeder port, mono v1). Showcase: **cathedral**.
> **Chorus** = the first use of the **shared modulated-delay buffer** 0015 reserves (the master
> "wow/flutter buffer" — flanger + tape-wow will reuse it). It's a MASTER INSERT, not a third send
> bus, so 0015's two-bus cap holds — `chorus(rate, depth, mix)`, navkit's BBD port with antiphase
> stereo taps (mono → wide), master-wide. Showcase: **juno**. Both dormant-until-called
> (byte-identical for old carts). Per-slot AUX routing (`instrument_bus`) stays the deferred next
> increment; the send-vs-insert reasoning + the placement sanity-check per effect are in
> [`sound-next-steps.md`](sound-next-steps.md). **Next:** **flanger + tape** as further uses of the
> chorus buffer; the §8.10.1 PARKED per-voice stand-ins migrate onto real phase-coherent buses (the
> **great auto-wah** = bandpass on the summed mix + follower, **epiano tremolo**, the **envelope
> follower**); leslie per the build-list.
> **Authoritative roster = [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md)**
> (effects are recipes; ~12 primitives, forever); this doc's §8.10 is the routing sketch 0015
> disciplined — where they differ, 0015 wins. To work on effects, read **0015 + §8.10 +
> §8.10.1** and skip the rest.

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

> **⚠️ HISTORICAL (2026-06-03) — superseded by the STATE header at the top of this file.**
> This is the *original* port plan, kept for rationale; the whole engine roster has since
> shipped. Read it for "why we sequenced it this way," not for current status.
>
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

> **Tempted to add a 4th macro?** Don't — read [decision 0017](../decisions/0017-three-macro-core-plus-engine-aux-channel.md)
> first. The core stays at three; everything *past* three routes into one of four lanes by *what it
> is*: **output-shaping** (mute/wah, vibrato, slide → the universal per-voice controls
> `note_cutoff`/`note_lfo`/`note_pitch`/…, every engine has them), **intrinsic timbre** (the 3
> macros — and if an engine seems to want a 4th continuous axis, suspect it's really *two* engines,
> à la HARP→GUITAR), **excitation/mode** (pizz vs arco → `eng_tune`, read at note-on), or a **timed
> articulation event** (`voice_consonant`/`voice_coda`). `eng_tune` (note-on) and `voice_param`
> (live) are the blessed per-engine **aux channel** — the same mechanism Plaits gives as its model
> selector and Rings as its 4th knob. VOICE is the engine that needs the live multi-param form;
> brass needs none of it (its mute is just `note_cutoff`).

#### Where the taste lives

The richness of the ~30 internal params doesn't vanish — it moves from *API* into a tiny
**per-engine mapping function** the porter writes once, clamped to the good-sounding range so
the instrument stays "impossible to make ugly":

| Engine | harmonics | timbre | morph |
|---|---|---|---|
| **Organ** | drawbar registration (snapped table — **full design §8.8.4**) | brightness / drawbar tilt + click bite | percussion + internal vibrato/chorus scanner depth *(Leslie is a separate per-voice recipe — §8.8.4 / 0015, not a macro)* |
| **Electric piano** | instrument (snapped: Rhodes/Wurli/Clav — **full design §8.8.5**) | brightness (pickup pos + hammer) | bark (pickup-nonlinearity drive) |
| **Acoustic piano** (StifKarp) | string stiffness / inharmonicity (pure → metallic shimmer — **full design §8.8.9**) | hammer (soft felt → hard plectrum) | pedal (dry/damped → open + sympathetic bloom) |
| **Guitar** (bodied pluck) | body (open harp → resonant box → bright banjo — **full design §8.8.9**) | string brightness (warm nylon → bright steel) | mute (long ring → tight pizzicato stop) |
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
   stays a catalog row for when a real lip model is wanted.)* **Design + STEP-0 render
   findings: §8.8.7** (2026-06-08) — the first *self-oscillating* held voice; STEP 0 revised
   the macro mapping (stiffness is a weak timbre axis, the model chokes instead of overblowing,
   harmonica is out of scope — all clamped to the self-oscillation window).
8. **`INSTR_MEMBRANE`** — tabla/conga/djembe, ~100 B mallet-pattern port; hand percussion
   (strike-pos + pitch-bend) for the world-music stations. **SHIPPED 2026-06-08** — `INSTR_MEMBRANE`
   (22), six modal sines, buffer-free; `harmonics` crossfades tuned-harmonic (tabla) ↔ navkit's
   ideal-membrane Bessel ratios (djembe) **and** sets the ring (the one deviation from the navkit
   port, which keeps Bessel for every preset — real tablas *are* harmonic, and the doc wanted the
   macro to span the head); `timbre` is navkit's live circular-membrane strike-position weighting
   (center thump ↔ edge ring + slap click); `morph` is the monotonic pitch-bend chirp (raised →
   settles, navkit's model). Showcase: the **tabla** cart (5 kit presets, the drumhead viz).
9. **Pipe / Bowed** — the rest of the continuous-excitation family reed unblocked.
   **`INSTR_PIPE`** (flute/recorder/pan pipe) — **SHIPPED 2026-06-09** (25): an STK jet-drive
   flute, bore reuses `ks_buf` + a tiny `jetBuf[64]`, reusing reed's whole breath/vibrato/chiff
   surface; harmonics = overblow, timbre = breath air, morph = embouchure. Design + STEP-0: §8.8.8.
   **Bowed** (violin/cello) — **SHIPPED 2026-06-09** as `INSTR_BOWED` (28): a line-for-line port of
   navkit's `processBowedOscillator`/`initBowed` (the two nut+bridge delay lines PACKED into one
   `ks_buf` — nut = `[0..nutLen)`, bridge = `[nutLen..total)` — distinct wave id, never collides with
   the Karplus/reed/pipe paths). Self-oscillating/held like reed/pipe. Macros pinned inside the
   STEP-0 wedge: harmonics = bow position β (note-on split, 0.05–0.25), timbre = bow pressure
   (0.15–0.42, the narrow axis), morph = bow speed/swell. Realism navkit omits, added like reed/pipe:
   humanized pitch-vibrato, light bow-noise (rosin), an attack bite. Verified: the showcase's held A3
   locks at steady-state crest ~3 / pitch-lock corr ~0.8 (with vibrato), and the timbre axis spans
   corr 0.96 (soft, clean Helmholtz) → 0.35 (hard, the scratchy "surface sound" — a real articulation,
   the same corr that got preset 107 rejected). Soundcheck slot 23, tripwire PASS, full 4-place wiring.
   Showcase: the **bowed** cart — played by RUBBING (energy accumulates: rub more → builds & digs in;
   stop → rests), a TAP plucks it (PIZZICATO). **Pizz is the same waveguide, not a guitar preset:**
   a second `INSTR_BOWED` slot flagged via `eng_p[0] >= 0.5` (`eng_tune(slot,0,1)`) seeds the two
   delay lines with a lowpassed pluck burst and bypasses the stick-slip friction per-sample, so the
   identical string + body rings down (~1s, brLoss 0.990 vs the bowed 0.995) instead of
   self-oscillating — arco and pizz differ *only* in excitation, exactly as on a real violin. (Can't
   self-oscillate: friction=0 → it can only decay.) Verified: clean pluck, sharp attack → fast
   brightness rolloff → warm ~1s ring-down, peak ≈ arco level, DC≈0. Below: the STEP-0 sweep that
   green-lit it.
   **Bowed** (violin/cello) — **STEP-0 sweep DONE 2026-06-09: GREEN LIGHT, engine is stable, the
   reject was a bad preset.** The original audition rejected navkit's bowed off one render (erratic
   envelope, crest 12.6 vs a clean voice's ~2–5). The lit-check hunch — that crest-12.6 is the
   signature of bowing *outside the Schelleng wedge* (too little force → double-slip "surface sound";
   too much → raucous crunch), a bow-force/velocity/position **regime** problem, not inherent
   instability — is now **measured and confirmed.** `tools/navkit-bowsweep.c` renders navkit's bowed
   across a pressure × velocity × position grid and classifies each cell on the steady-state tail by
   pitch-period autocorrelation + crest:
   - **The crest-12.6 reject IS preset 107's operating point** (P=0.6, V=0.5, β=0.13): `wav-analyze`
     reports crest **12.61 dB** — the exact number in the original verdict. That cell's steady-state
     pitch-lock is only **corr=0.36** — genuinely *not* locked, real surface sound. Presets 107
     (Cello, P=0.6) and 108 (Fiddle, P=0.8) both sit in the over-pressed `~` band. The original
     STEP-0 measured one preset and rejected the engine — the **Rhodes/Wurli trap exactly.**
   - **A large, range-stable Helmholtz wedge exists.** 100–150 of 600 cells lock cleanly (corr→1.0)
     at *every* pitch G2 (98 Hz) → A4 (440 Hz). Best cell P=0.2/V=0.2/β=0.25: **corr 1.00, crest
     1.67** — textbook leaning-sawtooth. Wedge shape is textbook Schelleng: a stable band bounded by
     collapse below and surface-noise above, **widening with bow velocity**, shrinking slightly with
     pitch (150→101 cells) but never vanishing. **No hysteresis bow table needed** — Smith's
     simplified STK friction table locks fine *inside* the wedge.
   - **Model param note:** in navkit's friction (`pres = pressure*5+0.5`, `f = pres·dv·exp(-pres·dv²)`)
     *higher* `pressure` steepens/narrows the slip region → chaos, so the clean band is **LOW
     pressure (~0.15–0.45)**, inverted from the physical-force intuition. Macro plan that falls out
     (keep all three INSIDE the wedge): **`timbre` → bow pressure mapped 0.15–0.45** (the narrow
     axis; low=collapse, high=deliberate scratch), **`harmonics` → bow position β 0.05–0.25**
     (ponticello→tasto, all have wedges), **`morph` → bow velocity/swell 0.2–1.0** (safe, and louder
     speaks cleaner since velocity widens the wedge). Reuses one delay line in `ks_buf`, held-note
     voice — same port difficulty as the shipped reed/pipe, NOT "the hardest of the family." Next:
     the line-for-line port (per the piano lesson), `INSTR_BOWED` (28).
   Then the genuinely buffered pair on the pluck-validated path: **`INSTR_GUITAR` (26) — SHIPPED
   2026-06-09** (KS string + 4 body-formant biquads; design + status §8.8.9), and **`INSTR_PIANO`
   (StifKarp, 27) — SHIPPED 2026-06-09** (KS string + dispersion allpass chain + soundboard;
   single-string v1, double-string `ks2Buffer` deferred; §8.8.9). The buffered pair is shipped.
   **STEP-0 done 2026-06-09 — green light, two engines not three:**
   - **Harp folds into guitar.** navkit ships harp as `GUITAR_HARP` (preset 216), one of *eight*
     `WAVE_GUITAR` presets (210 Acoustic · 211 Classical · 212 Banjo · 213 Sitar · 214 Oud · 215
     Koto · 216 Harp · 217 Ukulele). So `INSTR_HARP` is **not** its own engine — it's a guitar
     preset, exactly the way Rhodes/Wurli/Clav fold into EPiano. The buffered work is **two**
     engines: `INSTR_GUITAR` (the whole plucked-string family) + `INSTR_PIANO` (`WAVE_STIFKARP`,
     preset 218 Grand Piano). **Pizzicato strings are another short-decay `INSTR_GUITAR` preset** —
     this engine unlocks them for free.
   - **Stable across the range — clean ports, no friction fight** (unlike bowed). Rendered 210/216/218
     at C3/G4/C5/G5 (`/tmp/nk_{210,216,218}_*.wav`): 0 clipped samples, DC ≈ 0 everywhere (piano a
     negligible ~0.001–0.004). Crest is *high and rising with pitch* (gtr 21→27 dB, piano 14.5→19.6
     dB) — **correct for a plucked voice**: a sharp attack over a long decay tail measured across the
     whole window. The metric inverts vs. the sustained engines — for a pluck, high crest = real
     attack + ring-out (good); only for a *held* voice (bowed/reed/pipe) does it mean a lurching
     envelope (bad). So the bowed crest-12.6 alarm does **not** apply here.
   Finally the wind family's last member: **`INSTR_BRASS` (29) — SHIPPED 2026-06-10**, the last
   engine-blocked instrument in the whole library. STEP-0 found the catalog's literal lip
   mass-spring **doesn't self-oscillate** (loop gain < 1 — it decays after the attack); the working
   build is **reed's pressure-valve oscillator** (retuned for loop gain > 1) wearing a brass skin: a
   **dynamics-swept brass-formant bandpass** (the formant rising with brassiness *is* the blatty
   "blaaat") + the pressure-driven `tanh` steepening. harmonics = instrument/bore (trumpet→tuba),
   timbre = brassiness, morph = breath. Bore reuses `ks_buf`; soundcheck slot 22, tripwire PASS,
   full 4-place wiring. Showcase: the **brass** cart, marquee = the trombone SLIDE (drag → live
   `note_pitch` glissando). Mute deferred (only three macros). Design + STEP-0: §8.8.10.
10. **Formant filter** + the **effects layer** (§8.10 — buses vs. master; reverb / delay /
    tape / leslie / wah, starting with one master reverb + the formalized bus concept).
    **Additive stays deferred** — `INSTR_SINE` + FM + MALLET cover its near corners today —
    but it now has a first named customer: the MT70 family is 2–3 partials with per-partial
    decay, i.e. small additive (§8.9 corrected note). SCW bank (§8.4) remains optional.

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

0. **Render the navkit reference FIRST, and characterize it — before building anything.**
   `tools/navkit-render.c` renders the actual navkit preset to a WAV; `tools/wav-envelope.js`
   plots its amplitude+brightness. Ask: *what is it, really* — which engine, what modulation,
   and critically **what LAYER** (per-voice vs a bus effect)? This step is cheap (~2 min) and it
   is the one that stops rabbit holes. Added 2026-06-08 after the wah detour: we spent a session
   building a per-voice follower to chase navkit's "woah woah" wah — then rendered it and found
   it's a *bus* effect (and that the docs' confident "wah is per-voice" claim was wrong, 0015
   Correction). **A confident claim — yours, mine, or a doc's — about a reference sound is a
   hypothesis until you've rendered the reference.** Don't write engine/primitive code to match a
   target you haven't first reproduced and located on the right layer.
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

### 8.8.4 Engine #4: ORGAN — SHIPPED + published 2026-06-07 (was the step-1 design)
> **Post-ship (2026-06-07):** shipped per the design below + the `organ` cart. Two fixes landed:
> a **drive-fizz** rolloff (saturating a sparse bright registration made harsh intermodulation —
> the engine now rolls its top off before the voice drive) and the shared per-voice **DC blocker**
> on the drive path (driven organ injected DC → an env-ramp click). Leslie stays a per-voice
> recipe / a future bus item (§8.10, 0015).

The playbook's paper round for `INSTR_ORGAN`, the §8.5-phase-4 / [STATUS §5](../STATUS.md)
**NEXT** engine. navkit crib (all paths in `~/Projects/navkit/soundsystem/engines/`):
`processOrganOscillator` — `synth_oscillators.h:4064`; `OrganSettings` — `synth.h:912`;
**9 named registration presets** — `instrument_presets.h:3427` (reproduced in the appendix
below). **Scope guard:** this is the **dry tonewheel core only**. The **Leslie is NOT in the
engine** — [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md) rules it a
*per-voice recipe* (`LFO_VOLUME` + `LFO_PITCH` + `LFO_CUTOFF`, with a cart-side rate `lerp` for
the chorale↔tremolo spin-up). The engine's own motion is the **internal scanner vibrato/chorus**
(the Hammond V/C system) — a different, complementary thing that lives *under* a Leslie, not
instead of it.

**The whole oscillator is 9 additive sine partials** at fixed tonewheel ratios (navkit
`organDrawbarRatios`, `synth_oscillators.h:4032` — these ARE the standard Hammond drawbar
footages, in order):

```c
//                16'   5⅓'   8'   4'   2⅔'   2'   1⅗'   1⅓'   1'
static const float organRatios[9] = {
    0.5f, 1.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f };
//  the 5th partial (1⅗', ratio 5.0) is the major-3rd — the "nonobvious" Hammond color
```

**The mapping — 9 drawbars + percussion + scanner → 3 macros.** The hard part of this engine:
a B3 is nine independent faders and we get three knobs. Resolution (this **refines the §8.1.1
organ row**, which now points here; the playbook step-1 "resolve disagreements in the doc" call):

| macro | maps to | the taste decision (made here, on paper) |
|---|---|---|
| **harmonics** | drawbar **registration** | **SNAPPED to a curated table** of the iconic registrations (the appendix's nine + Vox/Farfisa combo), exactly like FM's ratio table — *not* a continuous 9→1 blend, which collapses to mush and reads as volume. Each detent is a different drawbar recipe = a different instrument: the strongest quarter-turn audibility, and presets become trivial. (Registrations *do* stay in tune if crossfaded — they're all harmonics of one fundamental — so continuous is *possible*; snapping is the taste call, not a necessity, because nobody thinks in "halfway between 888000000 and 006876540".) |
| **timbre** | drawbar **brightness tilt** + key-click bite | a continuous spectral tilt applied *over* the chosen registration: dark = weight 16'/8', bright = weight 2'/1⅗'/1⅓'/1' (a pow-curve gain per partial keyed by ratio). Folds in **key-click** amount (navkit `orgClick`: brighter ⇒ more attack click). The "open it up" knob; works on every registration. |
| **morph** | **animation**: scanner V/C depth + percussion | one "liveliness" axis from dead-still combo organ → fully animated jazz/gospel B3. 0 = no vibrato, no perc (Vox/Farfisa/reggae skank); rising sweeps the navkit scanner **C1→C2→C3** chorus depth (the appendix's depth table) and fades in the **percussion chiff** (2nd-harmonic ping) near the top. Keeps §8.1.1's stated "percussion + scanner depth" pairing — they co-vary on a real lively B3. |

**The buffer question (decided here).** STATUS/§8.5 call organ "buffer-free." True of the
drawbar core — 9 phase accumulators, zero buffer. But the **authentic scanner is not**: navkit's
**C (chorus) modes mix dry+wet from a 64-sample modulated delay** (`ORGAN_VIBRATO_BUFSIZE 64`,
~1.5 ms, `synth.h`) — the comb shimmer that *is* the gospel/jazz B3, unreproducible by a pitch-LFO
alone (that only gives the V/vibrato modes). **Decision: reuse the per-voice `ks_buf` (§8.2)** —
borrow 64 of its 1024 samples for the scanner tap. The engine then adds **no new buffer
infrastructure** (which is the sense in which "buffer-free" was meant), and the C-modes ship
authentic. Buffer-free fallback (V-modes only, scanner as a pitch-LFO) is the degraded option if
`ks_buf` reuse proves awkward — note it, but prefer the buffer. *(This sharpens the §8.5/STATUS
"buffer-free" wording: the core is, the scanner borrows.)*

**Known risks, named up front (the §8.8.3 discipline):**
- **Single-trigger percussion is monophonic + stateful.** A real Hammond fires the perc ping only
  on the *first* key of a legato phrase, re-arming on full release; navkit gates it on a global
  `orgPercKeysHeld` count. Our voices are independent — faithful single-trigger needs a
  per-instrument held-key tally on the audio thread. **Shippable default: per-note chiff** (every
  note-on pings) — polyphonically correct, slightly less authentic on fast legato runs.
  Single-trigger is a *deferred nicety, not a phase-4 blocker* — organ's "DX epiano is two
  op-pairs": don't build the faithful version speculatively.
- **Additive normalization = a hidden volume knob.** Gospel (9 bars) vs Reggae (2 bars) differ ~5×
  in raw sum. The §8.8.1 "normalize the excitation" rule is non-negotiable here: each harmonics
  detent must be **loudness-matched** (navkit's `sum /= ampSum` when `ampSum>1` is the start; then
  tune by ear so registration reads as *timbre*, never level).
- **CPU — heaviest engine yet: 9 `sinf()` × 16 voices = 144 sines/sample** (+ the scanner read),
  vs pluck=1 / mallet=4 / FM=2. Probably fine, but PROF-check the showcase cart. Cheap mitigations
  if it bites: a shared sine LUT (the raw waves already want one), and skip sub-0.001 drawbars
  (navkit's `if (level < 0.001f) continue;`).

**Mechanics:** state on `Voice` (an `OrganState`: 9 `drawbarPhases`, `clickEnv`, `percEnv`,
`percPhase`, `scannerPhase`, + a write-index into the borrowed `ks_buf` scanner tap). The nine
effective levels are **recomputed per-block from the live slewed macros** (registration × timbre
tilt) — *not* cached at note-on like navkit — so drag-to-audition works (the playbook's live-macro
rule; the macros are slewed already). Pitch: each partial = `v->freq × pitch_mul × organRatios[i]`
*per sample*, so vibrato / pitch-env / `note_pitch` / `note_glide` bend the whole stack together
(§8.8.1 satisfied by construction). The only excitation state is the scanner buffer → guard the
id-named-by-an-sfx-step-without-note-on case so it stays silent, not stale.

**Cart presets (= the acceptance tests), straight from navkit's nameplates:** **Jimmy Smith ·
Gospel Full · Jon Lord · Booker T · Larry Young · Reggae Bubble · Soft Combo · Ballad** (drop
Emerson or keep as a deep cut). Each is a baked (harmonics-detent, timbre, morph) triple — *if
pressing "Jimmy Smith" doesn't sound like a jazz B3, the mapping is wrong, not the preset.* The
showcase cart `organ.c` (pluck.c / mallet.c template: keys + three audition-while-dragging macro
sliders + autoplay) also puts the **Leslie recipe on a button** (chorale↔tremolo spin-up via a
`note_lfo()` rate lerp) — so the engine *and* its companion recipe are demonstrated end-to-end,
the whole §8.10/0015 story in one cart.

**Retrofit target:** **roadhouse** (the Doors) already auditions VOX/Gibson drawbar tables in its
THE BAND panel — the natural first station swap. A dedicated **gospel / soul / ska** station is
the bigger prize. `radio-instrument-options.md` owns the ranked list.

**Combo organ (Vox/Farfisa) is deliberately a *recipe* here, not a detent.** This is tonewheel
Hammond; a transistor combo organ's character is a non-sine divider waveform a pure-sine engine
can't model, only approximate. Ship it as a cart preset (bright registration + baked `drive`); it
graduates to its own `morph` axis or a second `INSTR_COMBO` engine only when a built station proves
the recipe insufficient — the path + named triggers are [decision 0016](../decisions/0016-combo-organ-recipe-then-macro-or-engine.md).
**Morph-fill verdict (2026-06-07): FULL** — playing `organ.c`, the scanner+perc `morph` is a full
distinct axis, *not* spare. So 0016 resolves to **branch B**: combo organ, when it earns promotion,
gets its own `INSTR_COMBO` engine rather than folding onto morph. Timbre confirmed by ear (presets
sound right driven + clean).

**Post-ship ear finding (2026-06-07): drive on sparse bright registrations fizzed.** Saturating
ballad (sub + fundamental + a lone 1′ over a big gap) through the voice's `tanh` drive made harsh
*intermodulation* in the empty gap — the sum/difference tones of the fundamental and the isolated
8× partial. Fix (shipped): the engine rolls off its top **before** the drive (`v->org_lp`, a
drive-gated one-pole, ~6 kHz light → ~1.8 kHz cranked) — exactly what a real cranked amp/Leslie
does. Clean (drv=0) is bit-identical (filter bypassed). Rule echoing FM's §8.8.3 clang finding:
**a hard nonlinearity punishes spectral *gaps* — roll the top off before you saturate.**

#### Appendix — navkit's 9 organ registrations (verbatim, `instrument_presets.h:3427`)

Drawbar columns in standard footage order (`8`=full/1.0, `6`=0.75, `4`=0.5, `0`=off):

| preset | 16′ 5⅓′ 8′ 4′ 2⅔′ 2′ 1⅗′ 1⅓′ 1′ | click | percussion | scanner | character |
|---|---|---|---|---|---|
| **Jimmy Smith** | `8 8 8 0 0 0 0 0 0` | .20 | 2nd, fast | C3 | fat sub+fund jazz B3 |
| **Gospel Full** | `8 8 8 8 8 8 8 8 8` | .40 | 2nd, fast | C3 | all bars out, choir wall (+crosstalk .3) |
| **Jon Lord** | `8 8 8 6 0 0 0 0 0` | .50 | off | V3 | Deep Purple growl (+drive .15, crosstalk .4) |
| **Booker T** | `8 8 6 0 0 0 0 0 0` | .15 | off | C1 | Green Onions, 60s clean |
| **Ballad** | `8 0 8 0 0 0 0 0 4` | 0 | 3rd, soft, slow | C2 | sub+fund+sparkle |
| **Reggae Bubble** | `0 0 6 0 6 0 0 0 0` | 0 | off | off | skanky upstroke (no motion) |
| **Larry Young** | `8 8 8 8 0 0 0 0 0` | .30 | 3rd, fast | C2 | Unity-era modern jazz |
| **Emerson** | `8 8 8 8 0 8 0 0 8` | .50 | 2nd, slow | V3 | prog bombast (+drive .1, crosstalk .3) |
| **Soft Combo** | `0 0 6 6 0 0 4 0 0` | 0 | off | C1 | cocktail lounge |

Scanner depths (`processOrganOscillator`, the LFO is **6.9 Hz** gear-locked to the motor):
**V1/C1** = ±0.15 ms · **V2/C2** = ±0.40 ms · **V3/C3** = ±0.70 ms. **V** modes are wet-only
(pitch vibrato); **C** modes are 50/50 dry+wet (the chorus shimmer — needs the buffer). Other
modeled extras available to borrow if wanted: **tonewheel crosstalk** (`orgCrosstalk`, ~−36 dB
adjacent-wheel leakage — cheap dirt, several presets use it) and the **3 ms key-click** noise
burst (already folded into `timbre` above). Percussion: 2nd or 3rd harmonic, soft = −3 dB,
fast ≈ 200 ms / slow ≈ 500 ms decay.

### 8.8.5 Engine #5: ELECTRIC PIANO — SHIPPED + published 2026-06-08 (was the step-1 design)
> **Post-ship findings (2026-06-08), tuned by ear + the navkit-render A/B (`tools/navkit-render.c`
> + `tools/wav-envelope.js`):**
> - **Rhodes rebuilt from MEASURED spectra (2026-06-08, second pass — supersedes the body/bell
>   split below).** The bigger problem was structural: the old `RAT[0]` (`1, 4.2, 9.5, 16.3…`) made
>   the *loudest, longest* partial an INHARMONIC 4.2× — an "untuned bell." FFT of our own render
>   confirmed it (4.2× louder than the fundamental). Real Rhodes is the opposite (Shear 2011 UCSB
>   thesis §2.1.2/Fig 2.2-2.3; Münster & Pfeifle JASA 148(5) 2020): the tine settles into near
>   **simple harmonic** motion — fundamental + INTEGER harmonics 2,3,4,5 (2nd often loudest, *made
>   by the nonlinear pickup*, Faraday + non-uniform field); the genuine inharmonic cantilever modes
>   (clamped-free bar series **1 : 6.27 : 17.55 : 34.4**) are SHORT-LIVED — a fast attack ding only.
>   Fix: `RAT[0]` = harmonics 1-6 + 6.27× bell + harmonics 8/10/12 + 17.55×/34.4× bells; per-mode
>   `DEC_R[]` (harmonics sustain, bells die in ~0.1s); `AC/AO[0]` give the **"voicing"** crossfade
>   (mellow = fundamental-dominant; bright = 2nd-harmonic dominant with even>odd partials, Shear
>   §2.2.2); pickup nonlinearity gets an **always-on grit floor** + gentler register gate (it's the
>   harmonic source, not just a dig-in effect). Verified by FFT vs the paper's figures: mellow →
>   fundamental-dominant, bright → 2nd-dominant + both tine bells in the attack. Removed
>   `RHO_BODY/BELL/BLVL`. (Tremolo is separate — see §8.10.1 PARKED.)
> - ~~**Rhodes was too "ringy."**~~ *(SUPERSEDED by the measured rebuild above.)* The inharmonic
>   tine/bell modes decayed in lockstep with the fundamental (a drone, not a *ding*) — which also
>   made the 3 Rhodes presets sound alike. First fix (now replaced): split the decay into a long
>   BODY (mode 0) + a short BELL (modes ≥1) + a bell-level boost (the old `RHO_BODY/BELL/BLVL`).
> - **`timbre` did nothing on Wurli/Clav** — it was only the pickup-position crossfade, whose
>   profiles are near-identical there. Added the **hammer-hardness** half (timbre scales the upper
>   modes), so timbre now brightens every instrument (clav 3× / wurli 4× / rhodes 6.5× swing).
> - **`bark` (morph) folds in drive** above its halfway point — clean → pickup-bark → tanh-growl
>   on one knob (the cart maps the bark slider to `instrument_morph` + `instrument_drive`).
> - **The funky-clav "wah" is a fast per-note filter-env quack** (resonant lowpass, ~100ms snap),
>   baked into the clav. The realistic **"woah woah" auto-wah is a BUS effect** (one filter on the
>   summed mix, exp sweep, follower on the whole performance) — deferred to §8.10; the per-voice
>   wah + the envelope follower are PARKED (0015 Correction, §8.10.1). 12 modes/voice ≈
>   the heaviest engine — PROF-check stands.

The playbook's paper round for `INSTR_EPIANO`, §8.5 step 5. navkit crib (all in
`~/Projects/navkit/soundsystem/engines/`): `processEPianoOscillator` (`synth_oscillators.h:3937`),
the ratio/amp tables (`:3675`–`:3766`), `initEPianoSettings` (`:3663`), `EPianoSettings`
(`synth.h:892`), 9 presets (`instrument_presets.h:3168`–`3398`). **Scope:** Rhodes / Wurlitzer /
Clavinet in ONE engine (the roadmap's "one engine = Rhodes/Wurli/Clav via pickup type").
**Buffer-free, confirmed from source:** a **12-mode modal bank + a pickup nonlinearity + a DC
blocker**, no delay line — the mallet family with 12 modes instead of 4. **Struck and
self-decaying** (NOT held like organ): hit it and the modes ring down, so it plays like
pluck/mallet with the ADSR as an override (§10.4), and it fits melodic comping (the citypop/yacht
EP retrofits).

**The whole engine in one line:** sum 12 decaying inharmonic sine modes → push the sum through a
pickup nonlinearity (the polynomial that makes it an EP, not a dull bell) → DC-block. The ratios,
amp profiles, decays, and the polynomial are all per-instrument data (appendix) — taste curated in
`sound.h`, never API.

**The mapping — navkit's ~9 params → 3 macros.** navkit exposes
hardness/toneBar/pickupPos/pickupDist/decay/bell/bellTone/ratioSet/pickupType. Collapse (this
refines the §8.1.1 EP row, which now points here):

| macro | maps to | the taste decision (on paper) |
|---|---|---|
| **harmonics** | the **instrument**, SNAPPED | three detents — **Rhodes (tine) · Wurlitzer (reed) · Clavinet (string)** — each selects a *different physical recipe*: ratio table, amp profile, nonlinearity polynomial (Rhodes = asymmetric even-harmonic **bark**; Wurli = symmetric odd-harmonic **buzz**; Clav = mixed **honk**), and the baked decay + bell character. SNAPPED, not continuous — the three have *different harmonic structures* (even vs odd), uninterpolatable: FM's ratio-table lesson again. |
| **timbre** | **brightness** = pickup position + hammer hardness | mellow (centered pickup → fundamental-heavy, soft hammer) → bright/snappy (offset pickup → strong 2nd, hard hammer → fast attack). The "tone" knob; works on every instrument. |
| **morph** | **bark** = the pickup-nonlinearity drive (navkit `pickupDist`) | the expressive dig-in: 0 = crystal-clean fundamental → up = the Rhodes bark / Wurli buzz / Clav honk grows. *The* electric-piano gesture (digging in distorts the pickup); stands in for velocity-driven drive. |

**Macro budget — the 0016 lesson applied up front: all three are full.** harmonics (instrument),
timbre (brightness), morph (bark) each do real, distinct work — no spare knob — so **decay/sustain
and bell-emphasis are baked per detent** (Rhodes = long tone-bar sustain + shimmer; Wurli = short,
reedy; Clav = very short, percussive), not live macros. An EP *patch* = macros + baked decay + the
voice ADSR (the FM precedent: a patch is macros + an envelope). A live decay knob would be the
moment to interrogate a 4th surface — not now.

**Known risks, named up front:**
- **Register scaling is not optional — it's the line between "EP" and "thin junk."** navkit's
  biggest correctness machinery is the per-mode rolloff with register (`freqNorm`): high notes
  shed their upper modes and most of the nonlinearity toward a near-pure sine (a real Rhodes top
  octave nearly *is* a sine), low notes stay rich and barky. Skip it → glassy-thin top, muddy
  bottom. Port the `(1−freqNorm)²`/cubic rolloff **and** the per-mode velocity floors (so soft
  hits still feed the nonlinearity). This is EP's "normalize the excitation" scar.
- **The pickup nonlinearity IS the instrument.** A bare 12-sine sum is a dull marimba-bell, not a
  Rhodes. The polynomial (asymmetric sum²+sum³ bark / symmetric sum³+sum⁵ buzz / sum²+sum³ honk)
  generates the harmonics the ear reads as "electric piano." Ships with the engine, not as a macro.
- **A DC blocker is mandatory** (navkit: one-pole HP, R=0.995, ~7 Hz) — the even-harmonic (sum²)
  nonlinearity injects DC. **This is the same DC the organ's drive produced** (§8.8.4 post-ship
  finding) — here it's designed in from frame one, and it's the **second customer** for a DC
  blocker: if reed/brass/the voice drive-path also want one, it graduates to a shared helper.
- **12 modes × 16 voices = up to 192 `sinf`/sample** + the polynomial — heaviest engine yet (organ
  9, mallet 4). PROF-check; the register rolloff helps (skip `mode_amp < 0.0001` modes), a shared
  sine LUT is the mitigation if it bites.

**Mechanics:** state on `Voice` (an `EPianoState`: 12× `mode_ph`/`mode_amp`/`mode_decay`/
`mode_ratio`, the 2 DC-blocker taps, `freq_norm`, captured strike level/bark). At note-on, build
the 12 ratios/amps/decays from the harmonics-detent tables × timbre/morph × register/velocity
scaling (navkit `initEPianoSettings` is the crib). Per sample: advance + decay each mode, sum,
apply the detent's polynomial (reading morph live for bark), DC-block. Pitch: `mode_freq =
v->freq × pitch_mul × ratio[i]` per sample → vibrato/glide bend the whole stack (§8.8.1).
Self-decaying → guard the id-without-note-on case (silent) and give the showcase a long gate.

**Cart presets (= acceptance tests), navkit's nameplates:** **Rhodes Warm · Rhodes Bright ·
Rhodes Suitcase · Wurli Soul · Wurli Buzz · Clav Funky** — each a baked (harmonics-detent, timbre,
morph) + decay recipe. *If "Wurli Soul" doesn't sound like Ray Charles, the mapping is wrong, not
the preset.* Showcase `epiano.c` is the tuning rig (pluck/mallet/organ template; struck keys, the
3 macro sliders, presets).

**Retrofit target:** **citypop** already fakes its Rhodes with `INSTR_FM` (the FM-Rhodes
retrofit) — the natural first swap to the real modal EP; **yacht** (DX-tine EP) and **lowend**
next. `radio-instrument-options.md` owns the ranked list. (FM stays the *DX/digital* electric
piano; `INSTR_EPIANO` is the *electromechanical* one — two different sounds, both wanted.)

#### Appendix — navkit's EP data (verbatim)

**Mode ratios** (`synth_oscillators.h:3675`):
- **Rhodes** tine+spring: `1, 4.2, 9.5, 16.3, 24.8, 35, 47, 61, 77, 95, 115, 137` (wildly inharmonic)
  — ⚠ **REPLACED 2026-06-08.** This navkit row sounded like an "untuned bell" (loud sustained 4.2×);
  our shipping `RAT[0]` is now the measured `1,2,3,4,5,6, 6.27, 8,10,12, 17.55, 34.4` (harmonic body
  + sparse fast inharmonic bells). See §8.8.5 post-ship findings + the `DEC_R` note in `sound.h`.
- **Wurli** reed (odd-ish): `1, 2.02, 3.01, 5.04, 7.05, 9.08, 11.1, 13.1, 15.2, 17.2, 19.3, 21.3`
  — ⚠ **REPLACED 2026-06-09.** Too odd-dominant (weak 2nd, no 4th) — missing the 200A's octave
  warmth. Shipping `RAT[1]` is now `1,2,3,4,5,6,7,8,9,11,13,16` with the OCTAVE partials (2,4,8,16)
  amped up near the reedy 3rd (Reed200 spectral-model crib; A/B + FFT confirmed). See §8.8.5.
- **Clav** string (near-harmonic): `1, 2.003, 3.012, 4.028, 5.15, 6.35, 7.6, 8.9, 10.2, 11.6, 13.0, 14.5` *(still shipping — near-harmonic struck string, plausible)*

**Amp profiles** — centered (mellow) / offset (bright), `:3747`; timbre crossfades them:
- Rhodes ctr `1, .04, .03, .06, .03, .02, …` · off `.6, .35, .08, .20, .08, .05, …`
- Wurli  ctr `1, .08, .45, .12, .10, .04, 0…` · off `.6, .15, .60, .20, .20, .08, 0…`
- Clav   ctr `1, .30, .20, .35, .15, .06, 0…` · off `.6, .55, .50, .20, .30, .10, 0…`

**Nonlinearity polynomials** (the soul; `:3960`+), all register- & velocity-scaled:
- **Rhodes** (electromagnetic): `out = sum + k·sum² + k2·sum³ + …`, **asymmetric** soft-clip (negative path ×0.85) → even harmonics = bark.
- **Wurli** (electrostatic): `out = sum + k3·sum³ + k5·sum⁵`, **symmetric** `tanh` → odd harmonics = reedy buzz.
- **Clav** (contact): `out = sum + k2·sum² + k3·sum³`, symmetric `tanh(·1.2)` (harder clip) → mixed = funky honk.
- All three → DC blocker `y = x − x₋₁ + 0.995·y₋₁`.

**Tone bar (Rhodes only):** extends fundamental decay ×(1 + 1.5·toneBar), 2nd ×(1 + 0.6·toneBar) — the suitcase's singing sustain. **Register rolloff:** upper modes ×`(1−freqNorm)³` (Rhodes bell modes ×6th power), fundamental ×`(1−0.15·freqNorm)`. **Key presets:** Rhodes Warm (soft hammer, long decay, centered) · Rhodes Bright (hard, offset, short) · Rhodes Suitcase (very soft, max tone bar, +tremolo) · Wurli Soul (soft, clean) · Wurli Buzz (hard, cranked `pickupDist`) · Clav Funky (hard, bridge pickup, filter-wah) — full dumps at `instrument_presets.h:3168`+.

### 8.8.6 Engine #6: PD (Casio CZ) — the step-1 design (2026-06-08)

The playbook's paper round for `INSTR_PD` — roadmap §8.5 step 6, the cheap snack between
bigger ports. Buffer-free, ~2 floats on `Voice`, no filter needed (the resonance is *in*
the oscillator). Target sound family: CZ basses, synth-brass, and the famous resonant
sweeps — deeply chiptune-adjacent, a strong identity fit at near-zero cost.

**Scope finding first — the engine is thin, and that's the design problem.** navkit's
`PDSettings` (`synth.h:422`) is exactly two fields: `waveType` (8 discrete) + `distortion`
(0..1, the DCW amount, driven live per sample as `patch_value + pdLfoMod`). The sample
function `processPDOscillator` (`synth_oscillators.h:1508`) is one `switch` over 8 wavetypes,
each a `cosf()` through a warped phase. navkit ships **two** presets (PD Bass SAW@0.7, PD
Lead SQUARE@0.5) and leans on the *shared* ADSR + filter-env + LFO for all movement — there
is **no dedicated DCW envelope**, the one thing that actually makes a CZ sound like a CZ.
So natively this is ~1.5 continuous axes, not enough for three macros. The third macro is
**built here**, from the feature navkit omitted (we already ship the second EG, so it's
nearly free — and makes our PD *more* authentic than the reference).

The 8 wavetypes split into two families: **phase-warp** (SAW · SQUARE · PULSE · DOUBLEPULSE
· SAWPULSE — `distortion` warps the phase ramp → harmonic brightening; DOUBLEPULSE is
sync/octave-up, SAWPULSE a saw+pulse blend) and **window×reso-cosine** (RESO1/2/3 — the
classic CZ resonance: a window gates a cosine whose freq = `1 + d·7` (1–8× harmonic), so `d`
sweeps a formant peak up). Note `distortion` *means a different thing* per family — fine,
because the wavetype is itself a macro detent.

| macro | maps to | the taste decision (made here, on paper) |
|---|---|---|
| **harmonics** | **wavetype**, SNAPPED to the 8 detents | exactly the FM §8.8.3 pattern — each detent is a different instrument (saw/square/pulse/doublepulse/sawpulse/reso1/reso2/reso3), max quarter-turn audibility. Never crossfaded — the wavetype formulas are discontinuous (blending two warped cosines = mud) |
| **timbre** | **static `distortion` `d`** | the steady-state brightness / resonant-peak position — the one knob navkit has, and the strongest axis. Phase-warp family: brightening; reso family: formant sweep |
| **morph** | **DCW-envelope depth** (internal attack→settle sweep of `d`) | **the engine's signature, built here.** 0 = static (navkit-flat); up = the CZ "wowww" — `d` snaps to a peak on the strike and decays toward the `timbre` setting over the note. An exponential settle (crib the FM index-decay mechanics — derives from `step_samples`, no extra EG plumbing). This is what reads as "phase distortion" rather than "a buzzy oscillator" |

Known risk, named up front: the **reso family can sweep into the harsh top** (`d·7` → 8×
harmonic with a hard window edge) → aliasing/icepick. Lever if it bites: cap the reso
multiplier below 8× in the played range, or soften the saw window (RESO3) edge. Decide by
ear, don't pre-clamp speculatively. (Phase-warp family is bandlimited-ish by the cosine
read-out and should be safe.)

Mechanics: buffer-free. Per `Voice`: the phase accumulator (already there) + the DCW
envelope's current value + its target — call it ~2 floats. No note-on excitation buffer →
no stale-state guard needed (cosine from any phase is safe). Pitch: phase advances at
`freq × pitch_mul / sampleRate` per sample — the §8.8.1 rule satisfied by construction.

**Build order — STEP 0 is non-negotiable (this is the wah-detour scar, §8.8.2 / sound-handoff
PARKED):** before freezing any numbers, **render navkit's presets 52/53** with
`tools/navkit-render.c` and A/B them against ours — the macro *architecture* above is
source-derived and solid, but the `d` ranges, the DCW decay time, and the reso cap are
ear-tuned, not read off the source. Then the cart presets (= the acceptance tests):
**CZ bass · resonant lead · synth-brass · the sweep-pad (full DCW "wowww") · a pluck/clav**.

**STEP 0 render findings (2026-06-08) — done before building, all confirm the design:**
- **The warp family has zero in-note spectral motion.** navkit's two presets (52 SAW@0.7,
  53 SQUARE@0.5) render with brightness **dead flat at 0.001** the whole note — only the amp
  ADSR moves. Empirical proof there's no DCW envelope → the `morph`=DCW-sweep macro is genuinely
  additive, not redundant. (Reference WAVs: `/tmp/navkit_pd_bass.wav`, `/tmp/navkit_pd_lead.wav`.)
- **The reso family's peak climbs with distortion, as modeled.** navkit ships no reso preset, so
  this was rendered by overriding a PD patch's `p_pdWaveType`→RESO3 + sweeping `p_pdDistortion`
  (the throwaway `/tmp/nkreso.c` — note the irony: characterizing the reso engine meant *writing*
  it in navkit-land). Brightness rose monotonically `0.007 → 0.011 → 0.017 → 0.022` for
  `d = 0.25/0.5/0.75/0.9` at C3 — the `1+d·7` resonant peak marching up. So `timbre` (static `d`)
  and `morph` (DCW sweep of `d`) have real, audible work.
- **The reso cap-risk is REAL and register-dependent.** The *same* RESO3 @ d=0.90 goes brightness
  **0.022 at C3 → 0.938 at C6** (near-total HF energy — a whistle/icepick, though no hard
  clipping: 0 clipped samples either way). So the fix lever named above is required, and it's
  **register-scaled**: cap the reso multiplier *lower as the note rises* (cheap: scale the `·7`
  by `(1 − freqNorm)` like EP's register rolloff, §8.8.5). Tune the curve by ear during the build.

### 8.8.7 Engine #7: REED (clarinet↔sax) — the step-1 design (2026-06-08)

The playbook's paper round for `INSTR_REED` — roadmap §8.5 step 7, and a milestone: the **first
true continuous-excitation (blown) voice.** Unlike everything shipped so far — pluck/mallet/EP/
membrane *strike* and ring down, organ/PD are oscillators *gated* by the amp envelope — the reed
**self-oscillates**: a pressure-driven feedback loop that sings for as long as it's blown. So
it's the first engine that genuinely *needs* the held-note + macros-as-CV surface (§6); the organ
(§8.8.4) proved that plumbing, but the organ would survive as a struck voice — the reed would not.
This is the engine the whole wind family (bowed/pipe, §8.5 step 9) is waiting behind.

The model (`processReedOscillator`, `synth_oscillators.h:587`; `ReedSettings` `synth.h:840`) is the
canonical McIntyre/Schumacher/Woodhouse (1983) reed valve: a **bore delay line** + a **nonlinear
reed reflection** at the mouthpiece. Mouth pressure drives a reed gap whose flow recirculates
through the bore; the bore's reflected pressure modulates the reed. Buffer: the bore is navkit's
`boreBuf[1024]` — it **reuses our `ks_buf[SOUND_KS_MAX]` (1024) as-is** (reed is a distinct wave
id; it never shares a voice with the Karplus pluck path), so **zero new buffer** — only a handful
of scalar state floats, exactly like the organ added its `org_*` fields. Cost: one fractional
delay read + ~10 mults/sample — pluck-class.

**Scope finding — STEP 0 first, and it *reshaped* the design (this is the wah-detour scar, §8.8.2,
earning its keep).** Before freezing the macro mapping I rendered navkit's six reed presets (192
Clarinet, 193–195 Soprano/Alto/Tenor Sax, 196 Oboe, 197 Harmonica) with `tools/navkit-render.c`,
plus bore / blow-pressure / stiffness sweeps via a throwaway `/tmp/nkreedsweep.c` (the PD-spec
trick: characterizing an axis means *driving* it in navkit-land). The renders **contradict the §8.9
catalog row in two of its three macros** — proof that reading the source and guessing would have
shipped the wrong knobs:

- **It self-oscillates and HOLDS dead-flat** for the full 2s on clarinet/oboe/all three saxes
  (rms steady, 0 clipped, DC ≈ 0 — the model's DC blocker is essential and works). Confirms the
  held-voice premise, and — like PD's warp family — confirms there is **no in-note spectral motion
  from the model itself**, so any `morph` movement is genuinely *additive*, not redundant.
- **Bore conicity is the dominant, most musical axis.** Sweeping `bore` 0→1 (clarinet→sax) raised
  brightness monotonically `0.006 → 0.010` (odd-only → all-harmonics) **and dropped loudness ~10 dB**
  (rms `0.36 → 0.12` — the flared conical bell radiates highs but carries less total energy). So
  `harmonics = bore` is the strongest knob *and* literally "which instrument" — but it needs
  **per-position makeup gain** (~×3 at the conical end) or the sax presets bury, a register-rolloff-
  style compensation (cf. EP §8.8.5).
- **Reed stiffness is a WEAK brightness axis** — the catalog's premise fails. At the cylindrical
  bore, stiffness `0.25 → 0.75` barely moved brightness (`0.006 → 0.005`). So `timbre = stiffness`
  *alone* is not a real knob. It gets **built here** as a compound *edge* axis (see table).
- **The model does NOT overblow musically — it CHOKES.** Pushing blow pressure past ~0.8 collapses
  the oscillation (the reed beats shut: rms → 0.0001), and below ~0.42 it never starts. There is a
  narrow **viability window ≈ [0.42, 0.78]**, and *within* it pressure is mostly a loudness axis.
  So the catalog's "morph = overblown squawk" is wrong for this model; `morph` is rebuilt as breath
  expression *inside* the safe window (table).
- **Self-oscillation has a hard viability floor/ceiling the macros MUST clamp to.** Stiffness 1.0
  chokes; pressure outside [0.42, 0.78] dies. And **navkit's own Harmonica (197) sits below the
  floor → it rendered essentially silent** (rms 0.013 → 0.000). Free reeds aren't air-column-coupled
  instruments, so **harmonica is out of scope** for this waveguide; the 5 reachable presets are
  clarinet + soprano/alto/tenor sax + oboe. The #1 implementation rule: **every macro maps onto a
  clamped, pre-validated physical sub-range, never the raw 0..1.**
- **Register is stable C3–C6** (rms flat ~0.16, no choke or blow-up; boreLen `169 → 21`, the
  fractional read holds tuning). Unlike PD's reso icepick, reed has **no high-register risk** in its
  playing range. (Reference WAVs: `/tmp/reed_clarinet.wav`, `/tmp/reed_altosax.wav`, `/tmp/reed_oboe.wav`.)

| macro | maps to | the taste decision (made here, *post-render*) |
|---|---|---|
| **harmonics** | **bore conicity** (continuous, clamped ~`[0, 0.95]`) — and with it the bell-LP cutoff, the open-end reflection, and the conical even-harmonic drive (all derive from `bore` in the model) + **per-position makeup gain** | the dominant axis *and* "which instrument": clarinet (cyl, odd-only, hollow chalumeau) → oboe (narrow conical, nasal) → sax (full conical, all harmonics). **Continuous, not snapped** — `bore` is a genuinely continuous physical axis (cf. membrane's continuous ratio crossfade, unlike FM/PD/organ's discrete tables), and every quarter-turn is audible (odd→all + brightness, STEP-0). The ~10 dB conical loudness drop is compensated per-position. |
| **timbre** | **reed edge** — a *compound* knob: stiffness↑ **+ aperture↓ together**, plus a one-pole brightness tilt on the output | stiffness alone is too weak (STEP-0) — paired with aperture-narrowing (exactly how real bright reeds are built: oboe = stiff 0.9 + aperture 0.2) and a small output tilt, it becomes a strong dark↔nasal-edge axis with *guaranteed* audible travel regardless of what the waveguide does. The PD-DCW move: **build the axis the source is too weak to provide.** Clamped to the oscillating window (stiffness ≤ ~0.85). |
| **morph** | **breath expression CV**, *strictly inside* the viable pressure window `[~0.5, ~0.75]` | the held voice's live axis (the point of §6): 0 = steady soft tone; up = breath swell + lip-vibrato deepening + a controlled lean *toward* (never across) the choke threshold → a straining growl. The model can't overblow (STEP-0), so this is the musical substitute, **built here**, and it's live-modulatable as CV — the whole reason the wind family wanted held notes. |

**Mechanics:** buffer-free beyond the `ks_buf` reuse. Per-`Voice` scalar adds (~8 floats + 2 ints,
the organ-sized footprint): bore read/write indices, the bore-loss LP state, the DC blocker taps
(`dc_prev`/`dc_state` — **non-negotiable**, the reed carries large DC from steady blow pressure),
the lip-vibrato phase, `initFreq` (glide/arp pitch tracking), and a `reed_on` guard (an engine id
hit without a note-on init, like `org_on`/`mb_on`). Pitch (§8.8.1): the bore read length =
`boreLen / pitch_mul` (fractional), so vibrato/glide/pitch-env bend it — satisfied by construction
(navkit already does exactly this). Gate: reed is a **held** (`note_on`, infinite-gate) voice like
organ; note_off ramps blow pressure toward 0 so the bore energy physically dissipates (a natural
breath release), backed by the standard amp release. Seed the bore with tiny noise on note-on
(navkit's faster-startup trick). It's the **first held voice with a self-oscillating excitation
buffer** — so the §8.2 / stale-state guard matters here in a way it didn't for organ (which only
borrows `ks_buf`'s head as a clean scanner delay): re-seed the bore on every note-on.

**Build order — STEP 0 is done (above); the cart presets are the acceptance tests:** clarinet
(dark hollow chalumeau — the cylindrical anchor) · alto + tenor sax (the jazz workhorse, the
headline sound) · oboe (nasal double-reed) · a **breath-swell demo** that sweeps `morph` live to
show the held-note CV — the gesture *no struck engine can make*. Showcase: a **reed** cart (the 5
reachable presets + a breath/vibrato viz), and since reed is "klezmer to smoky jazz on one knob"
it's a natural jazz/lounge or klezmer radio-station voice. Id is `INSTR_REED` = **23** (next after
membrane's 22); wire it in all four places (`studio.h` `INSTR_*` + house-style comment,
`studioDocs.js`, `shell.js` — the synth lives in `sound.h`, not `studio.c`), and run the soundcheck
tripwire after touching `sound.h` (CLAUDE.md → "After touching `runtime/sound.h`").

**SHIPPED 2026-06-08** as `INSTR_REED` (23) — engine + showcase cart (poly + a mono/slide mode,
key `V`, legato portamento via `note_pitch`/`note_glide`). **Realism pass (same day):** the bare
waveguide port read as a "synth tooter" (ear-test verdict from a former clarinettist — the model
is a minimal 1983 single-delay waveguide, and like navkit it shipped with *no* breath noise). Added
the three cues it was missing: **breath turbulence** injected into the driving pressure so the bore
resonates it (the #1 cue; scales with breath + the `morph` macro), an **attack chiff** (~28ms
breathy onset, suppressed on mono slides), and a **humanized vibrato** (wandering rate/depth, moved
mostly into pitch, + a slow breath random-walk so the steady state is never dead-flat). Verdict
after: "much better."

> **OPEN — still needs ear-tuning (not blocking, revisit when convenient):**
> - **The output brightness `tilt` is UNVERIFIED — the one likely-undone thing.** It was added
>   pre-realism-pass to force audible `timbre` travel (STEP-0 found reed stiffness too weak on its
>   own). It's a synthetic EQ boost and the prime suspect for any remaining *electronic* edge on the
>   bright presets. Now that breath + drift carry the life, **A/B removing or softening it** — it may
>   no longer be earning its keep. First knob to revisit.
> - **Per-preset breath amounts want tuning by ear** — a tenor sax wants more air than an oboe; the
>   current `air = 0.10 + morph·0.12` is one global curve. Consider a per-voice trim.
> - General: the macro→physical ranges + the chiff/vibrato depths are ear-set starting points, not
>   final. Tune against the navkit reference WAVs (`/tmp/reed_*.wav`) and real recordings.

### 8.8.8 Engine #8: PIPE (flute/recorder/pan pipe) — the step-1 design (2026-06-09)

The playbook's paper round for `INSTR_PIPE` — roadmap §8.5 step 9, the **second blown voice** and
the reed's close cousin. An STK jet-drive flute (Cook/Scavone): a single **bore delay line** + a
short **jet delay** + a nonlinear **jet deflection** (`tanh`) that self-oscillates. Where the reed
is a *pressure-driven valve*, the pipe is an *air jet* striking the labium edge — but both are
self-oscillating held voices, so `INSTR_PIPE` **reuses the reed's whole surface**: the held-note
path, the breath-noise turbulence, the humanized vibrato, the attack chiff. This is the
lowest-risk port on the board *because* reed paved it.

**Scope finding — STEP-0 audition + source dig (2026-06-09), and it's the cleanest yet.** Rendered
navkit's pipe preset (109 Pipe Flute) across the range with `tools/navkit-render.c`:

- **Rock-stable C3→G6** — crest 5.4–5.8, RMS dead-even (0.36–0.39), 0 clipped, DC ≈ 0 at *every*
  pitch. The opposite of navkit's bowed render (which lurched, crest 12.6, erratic envelope — though
  that was later traced to a Schelleng-wedge tuning issue, not inherent instability; see §8.5 step 9). No register icepick like PD's reso. So no stability fight: the model just works.
  (Reference WAVs: `/tmp/fl_pipeflute_{c3,g4,g5,g6}.wav`.)
- **Cheap buffer tier — fits `ks_buf` like reed.** `initPipe` sizes the bore at `SR/(freq·boreScale)`
  capped at **1023**, and the oscillator reads only `lowerBuf` (the struct's `upperBuf` is vestigial —
  seeded but never read). So the bore **reuses `ks_buf` as-is**; the only new state is a tiny
  `jetBuf[64]` (the lip→labium travel delay) + a few scalars. *Not* the heavy banded/StifKarp
  buffered tier.
- **The breath excitation already exists** — `excitation = tanh(jetOut · gain) · breath`. navkit's
  `breath` scalar is exactly where our reed breath-noise injects, so the turbulence/air work
  transfers wholesale. A flute is *more* air than a reed, so it leans on this even harder.
- **navkit ships `overblow = 0` on both presets** (109 Flute, 110 Recorder) — so the octave
  register-jump (the flute's signature, sitting right in the jet gain `2 + overblow·8`) is **unused
  by the reference**, exactly like reed's morph was ours to exercise. We dial it in.
- **Embouchure is the live tone axis** — it sets both the mouth-end `feedbackGain` (`0.5 + embou·0.4`)
  *and* the jet delay length (`jetLen = 3 + (1−embou)·8`, longer jet = easier to overblow). The two
  presets differ almost entirely by it: flute `embou 0.6`, recorder `0.35` (breathier, medieval).

| macro | maps to | the taste decision (made here, on paper) |
|---|---|---|
| **harmonics** | **overblow** (jet gain `2 → 10`) | register + brightness: pure fundamental → overblown octave flageolet → bright harmonics. The flute identity; navkit leaves it at 0 so it's ours to exercise (cf. reed's morph). Continuous, not snapped — it's a smooth gain, every quarter-turn audible. **Watch:** high gain may screech at the top; `tanh` bounds it but verify the sweep doesn't destabilize (the named risk, cf. reed's choke / PD's icepick). |
| **timbre** | **breath air** — excitation level + breath-noise amount (reusing reed's turbulence) | pure ↔ airy/breathy, the *defining* flute texture. A flute is mostly air; this is the strongest perceptual knob, and we already own the machinery. |
| **morph** | **embouchure** — `feedbackGain` + live `jetLen` | the lip over the hole: hollow/dark ↔ focused/bright, and it eases the overblow. Live-modulatable (the held-voice CV gesture); recomputing `jetLen` per-sample from morph makes it a true live macro, not an init-only param. |

`bore` (navkit's 4th param) only ±10%s the length here, so it folds into a per-preset constant, not
a macro. **Realism from day one** (the reed lesson — don't ship the bare port): port `breath`
turbulence, the humanized pitch-vibrato, the attack chiff (the flute "tu" tongued onset is *very*
characterful), and the slow breath drift, all from the reed build — a flute that's never heard
a synth-tooter phase.

**Mechanics:** buffer-free beyond `ks_buf` reuse. Per-`Voice`: bore read/write indices (or reuse
reed's if the two never coexist on a voice — they're distinct wave ids, so safe), `jetBuf[64]` +
its index, the open-end LP state, the DC blocker, jet length, `initFreq`, a `pp_on` guard, and the
shared breath/vibrato/drift/chiff state. Pitch (§8.8.1): bore read length = `boreLen / pitch_mul`
(fractional) — satisfied by construction. Held voice (`note_on`, infinite gate) like reed/organ;
amp ADSR gates the fadeout.

**Build order — STEP-0 done; cart presets = acceptance tests:** concert flute (the clean anchor) ·
recorder (breathy, low embouchure) · pan pipe (airy, more breath noise) · an **overblown** voice
(harmonics up — the octave flageolet) · a **breath-swell demo** (morph/breath as live CV). Showcase:
a **pipe** cart (the reed cart's sibling — bore viz → a fipple/jet mouth, the overblow register
shown). Id `INSTR_PIPE` = **25** (24 is the in-flight `INSTR_VOICE`); wire all four places + the
soundcheck tripwire after `sound.h`.

### 8.8.9 Engines #9–10: GUITAR + PIANO — the buffered pluck pair, step-1 design (2026-06-09)

The playbook's paper round for the two **genuinely buffered** engines — roadmap §8.5 step 9 tail.
**`INSTR_GUITAR` (26) SHIPPED 2026-06-09** — ported onto our KS path (PLUCK's string + 4 parallel
body-formant biquads, lerped open→boxy by harmonics, + a DC blocker; morph drives the mute/decay,
timbre the excitation brightness). Wired all 4 places, soundcheck slot 25 (tripwire PASS), showcase
`guitar` cart with 8 hardware presets incl. pizzicato. Output verified clean (DC≈0, 0 clipping,
crest 19 dB — navkit's reference range). **Open tail: STEP-6 macro tuning by ear** (the body-formant
anchors + the mute→T60 curve are first-pass). Buzz/jawari (sitar) deferred.
**`INSTR_PIANO` (27) SHIPPED 2026-06-09 — VERBATIM StifKarp port.** First attempt (adapted onto our
KS loop) measured *thin* — A/B vs navkit showed grand 1–3k presence 3% vs navkit's 44%. The lesson:
param-matching isn't enough, the DSP must be **verbatim**. Re-ported `processStifKarpOscillator`
line-for-line: **near-lossless loop** (decay comes from the amp envelope/gate, NOT the loop — that's
what keeps the upper harmonics alive), one-period buffer + fractional-delay allpass tuning, averaging
strike comb, per-voicing brightness/damping, dispersion, per-voicing soundboard + tone-filter,
detuned 2nd string (grand/bright), sympathetic tap. **Result: harpsichord now matches navkit**
(1–3k 40% vs 43%), grand has real presence + a brightness **bloom** (bright strike → mellow) + hammer
thump for character — user-approved. Macros: **harmonics = voicing** (snaps grand/bright/harpsi/
dulcimer/clavichord/celesta — each a full navkit voicing table; the EPiano-style selector), timbre =
hammer, morph = pedal. The 6 presets = the 6 voicings. Also shipped this pass: **`eng_tune()`** (the
throwaway weight/attack tuning poke), the **brightness bloom**, and a core fix — **quietest-voice
stealing** in `sound_find_voice` (steal the most-faded tail, not a loud ring-out; needed now that the
string engines sustain for seconds — helps pluck/guitar too). Wired all 4 places, soundcheck slot 24
(tripwire PASS), showcase `piano` cart (6 voicings, 2-row tuning knobs). Verified stable (DC≈0, 0
clipping). **STEP 0 is done** (rendered navkit 210 Acoustic / 216 Harp / 218 Grand Piano across C3→G5: clean,
stable, 0 clipping, DC≈0 — high crest is correct decaying-pluck behavior, *not* bowed-style
instability; see §8.5 step 9 tail). Two structural findings from STEP 0 set the scope:

- **Two engines, not three.** navkit ships harp as `GUITAR_HARP`, one of eight `WAVE_GUITAR`
  presets — so `INSTR_HARP` folds into `INSTR_GUITAR` the way Rhodes/Wurli/Clav fold into EPiano.
  The work is `INSTR_GUITAR` (the whole bodied plucked-string family) + `INSTR_PIANO`
  (`WAVE_STIFKARP`, the stiff-string keyboards).
- **Both must earn their slot vs. the existing `INSTR_PLUCK` (16)**, which is the *bare* KS string
  (harmonics=ring, timbre=pick brightness, morph=pick position). The new engines foreground what
  PLUCK lacks: GUITAR = **+ body resonator + buzz**; PIANO = **+ string stiffness/inharmonicity +
  hammer + pedal**. The macros spend their three knobs on those new percepts, not on what PLUCK
  already does. **PLUCK coexists** — it stays the cheap bare string (≈45 carts may use it); GUITAR
  is the richer bodied tier (a clean good/better ladder, no breakage).

Macro design (§8.1.1: three knobs only; map percepts, not parameters; every quarter-turn audible
on every key). navkit param ranges are the menu each macro sweeps.

**`INSTR_GUITAR` — bodied plucked-string family** (acoustic / classical / banjo / koto / oud /
sitar / harp / ukulele — **and pizzicato**). KS string through a resonant body; decays on its own.

| Macro | Percept (0 → 1) | navkit params | Why |
|---|---|---|---|
| **harmonics** | **body** — open & bodyless (harp) → resonant box (acoustic/oud) → bright snappy box (banjo/koto) | `guitarBodyMix` 0.15→0.7 (+ `bodyBrightness`) | The body *is* why GUITAR exists vs. PLUCK; the "which instrument" family axis. Continuous (cf. reed's bore), not snapped. |
| **timbre** | **string brightness** — warm nylon → bright steel | `pluckBrightness` 0.35→0.85 (+ `bodyBrightness`) | Universal dark↔bright; nylon-vs-steel. Same slot as every engine's timbre. |
| **morph** | **mute/damping** — long open ring → tight muted stop (palm-mute → **pizzicato**) | `pluckDamping` 0.9995→0.993 (+ `release`) | The live gesture: drag to choke a ringing note. Hands the player **pizzicato on a knob**; staccato/palm-mute for free. |

Dropped to fit three knobs (baked per preset): `guitarPickPosition` (PLUCK already owns pick
position as *its* morph) and `guitarBuzz` (sitar jawari — niche; baked into the Sitar preset).

```c
#define INSTR_GUITAR  26  // plucked string + body — acoustic/nylon/banjo/koto/harp/uke/pizzicato. KS string through a resonant body; decays on its own (long hit()/release). macros: harmonics = body (open harp → resonant box → bright banjo), timbre = string brightness (warm nylon → bright steel), morph = mute (long ring → tight pizzicato stop)
```

**`INSTR_PIANO` — struck stiff-string keyboards** (grand / bright / harpsichord / dulcimer /
clavichord). Inharmonic string + hammer + soundboard + sympathetic strings; rings down on its own.

| Macro | Percept (0 → 1) | navkit params | Why |
|---|---|---|---|
| **harmonics** | **stiffness** — pure harmonic → stretched/metallic inharmonic shimmer | `stifkarpStiffness` 0.1→0.4 | Literally the partial series — the cleanest "harmonics" mapping. The stretched-octave shimmer *is* what reads as real piano/dulcimer. |
| **timbre** | **hammer** — soft felt & mellow → hard & bright (grand → harpsichord plectrum) | `stifkarpHardness` 0.2→0.9 (+ `pluckBrightness`) | Exact precedent: MALLET's timbre is "mallet hardness." Felt-vs-plectrum brightness. |
| **morph** | **pedal** — dry/damped → sustain-pedal open + sympathetic bloom | `stifkarpDamper` 0.9→0.0 + `stifkarpSympathetic` 0→0.25 (+ `pluckDamping`) | The piano's signature *live* gesture — drag = press the pedal: the note opens and other strings ring sympathetically. |

Dropped to fit three knobs (baked per preset): `stifkarpStrikePos`, soundboard
`bodyMix`/`bodyBrightness` (clavichord low → dulcimer high), `stifkarpDetune` (the 3-string chorus).

```c
#define INSTR_PIANO   27  // struck stiff string (StifKarp, verbatim) — grand/bright/harpsichord/dulcimer/clavichord/celesta. Near-lossless KS string + dispersion + soundboard; rings down on its own (long hit()). macros: harmonics = voicing (snaps the six), timbre = hammer (soft felt → hard plectrum), morph = pedal (dry/damped → long open sustain)
```

**Acceptance-test presets (STEP 5 — "if 'Harpsichord' doesn't sound like one, the mapping's wrong"):**
GUITAR — Harp · Classical (nylon) · Acoustic (steel) · Banjo · Koto · Ukulele · Sitar ·
**Pizzicato** (harmonics≈0 open, morph≈high mute, short ADSR). PIANO — Grand · Bright ·
Harpsichord · Dulcimer · Clavichord.

> Ids `26`/`27` assume `INSTR_VOICE` (24) and `INSTR_PIPE` (25) hold — confirm the next free id at
> port time. Open mapping question carried into STEP 6 (tune-by-ear): guitar morph = mute may want
> to fight PLUCK's morph = pick-position for the slot; mute won on giving pizzicato directly, revisit
> if the body axis already implies enough decay variation.

### 8.8.10 Engine #11: BRASS — the lip-reed waveguide, design + STEP-0 (2026-06-10)

The last engine-blocked instrument (§2b of `cart-library-direction.md` — every other modeled
timbre had shipped). **SHIPPED 2026-06-10 as `INSTR_BRASS` (29).** One id covers the brass
family: trumpet / cornet / flugelhorn / trombone / french horn / tuba.

**Macros (on paper first, §8.8.1):** harmonics = **instrument/bore** (bright tight trumpet → dark
wide tuba — drives the bell radiation LP and the brass-formant centre, never the delay length so
it can't detune); timbre = **brassiness** (the pressure-driven wave-steepening shockwave — round
& mellow → loud & blatty); morph = **breath/lip lean-in** (soft steady → growling breath + deeper
vibrato, the reed surface reused).

**STEP-0 finding — the lip biquad doesn't self-oscillate on its own; the reed valve does.** The
catalog (§8.9) prescribed a "2nd-order lip mass-spring." The first build modeled the lip as a
literal resonant biquad in the loop (STK BrassInstrument's normalized bandpass + squared valve);
both attempts **decayed to silence after the attack** — measured loop gain ≈ 0.92 < 1 at the
fundamental (the normalized bandpass's broadband gain `(1−r)` ≈ 0.001 makes the lip displacement
vanish, and the squared-valve injection couldn't make up for it). The fix that *reliably*
self-oscillates: **reuse REED's proven pressure-controlled valve core** (the negative-resistance
`lipRefl = offset + slope·pdiff` reflection), retuned for loop gain > 1 (`endRefl ≈ −0.96`,
`slope = −0.70`) — the `tanh` brassiness shaper bounds the amplitude even at `drive = 1`. The
brass *identity* then comes from the TIMBRE stages layered on that oscillator, not from a literal
lip mass-spring:
- a **brass-formant resonant bandpass** (the lip+bell resonance, reusing the lip-biquad state) on
  the output, whose centre **sweeps UP with brassiness** — that rising formant *is* the blatty
  "blaaat", the one thing no other engine does;
- the **pressure-driven steepening** `tanh(x·drive)/drive`, `drive` scaling on timbre AND blow
  pressure (blow harder → blattier, the real physics) with the `1/drive` loudness-normalize.

So the lesson mirrors §8.8.1's wah detour and the membrane "harmonics also sets the ratios"
deviation: **the navkit/STK structure was a hypothesis; the rendered reference (here: the loudness
trace showing decay) settled it** — a working brass is reed's oscillator + a dynamics-swept
formant, not a from-scratch lip model. Verified headless: self-oscillates and HOLDS, RMS ≈ −21
dBFS (matches reed's −23), crest ~7 dB (steady tone), DC ≈ 0, **0 clipped** at the loud extreme
(tuba bore + brassiness 1.0, peak −12.9 dBFS). Showcase: the **brass** cart, whose marquee is the
trombone **SLIDE** (drag it → continuous `note_pitch` glissando — pitch tracking per §8.8.1 made
it free). Six acceptance presets (trumpet/cornet/flugelhorn/trombone/french horn/tuba). Open tail
(STEP 6, the owner's): preset taste-tuning by ear; a **mute** axis (harmon/cup → a second
bandpass) is deferred — there are only three macros and bore/brassiness/breath earned them.

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
| **Reed** (clarinet ↔ sax) | `processReedOscillator` (pressure-driven reed valve) | one `boreBuf[1024]` — **fits today's `ks_buf` as-is** | **bore conicity** (clarinet hollow-odd ↔ sax full) — literally navkit's `bore`, the dominant axis | **reed edge** — stiffness+aperture compound (stiffness *alone* is too weak — STEP-0) | **breath expression CV** *inside* the viable window (the model chokes, doesn't overblow — STEP-0) | the *blown* family's workhorse + the first **self-oscillating held** voice; klezmer to smoky jazz on one knob. **Design + STEP-0: §8.8.7** (2026-06-08) |
| **Pipe / flute** (STK jet-drive) | `processPipeOscillator` | **one bore line fits `ks_buf` as-is** (only `lowerBuf` is read; `upperBuf` vestigial) + tiny `jetBuf[64]` | **overblow** (fundamental ↔ octave flageolet) — jet gain | **breath air** (pure ↔ airy, reuses reed turbulence) | **embouchure** (feedback + jet length) | airy flutes, pan pipes, organ-flue color; breathy attacks for free. **SHIPPED 2026-06-09** as `INSTR_PIPE` (25) — design + STEP-0: §8.8.8 |
| **Brass** (lip-valve waveguide) | reed's pressure-valve core + a dynamics-swept brass formant (STEP-0 found the literal lip mass-spring doesn't self-oscillate — §8.8.10) | one bore line — **fits `ks_buf` as-is** | instrument/bore (trumpet ↔ tuba) | **brassiness** (round ↔ loud/blatty shockwave — blow harder, get blattier) | breath/lip lean-in | **SHIPPED 2026-06-10** as `INSTR_BRASS` (29) — the last engine-blocked instrument; showcase = the **brass** cart (trombone slide). Mute deferred. Design + STEP-0: §8.8.10 |
| **PD / phase distortion** (Casio CZ) | `processPDOscillator` — **2 floats, 8 wavetypes incl. 3 resonant** | free (cheapest in the catalog) | wavetype (snapped detents, like FM's ratio table) | static distortion amount (filter-like brightness / reso-peak position, zero filter) | **DCW-envelope depth** — an attack→settle sweep of distortion (the CZ "wowww"; navkit omits this, we build it from the second EG) | CZ basses, synth-brass, the famous resonant sweeps; deeply chiptune-adjacent — strong identity fit, near-zero cost. **Full design: §8.8.6** (2026-06-08) |
| **Membrane** (tabla/conga/bongo/djembe/tom) | `processMembraneOscillator` (`:1754`, `MembraneSettings` `synth.h:437` — 6 modal sines at circular-membrane Bessel ratios) | free (~100 B — mallet-family cost) | head character (tabla ↔ djembe mode spread / tension) | **strike position** (center thump ↔ edge ring — the model reweights modes physically; conga open/slap/mute in one knob) | **pitch-bend depth/decay** (the tabla bayan *glissando* — baked into the model) | hand percussion the analog 808/909 recipes can't reach — bend + strike-pos are exactly what sine+pitch-env approximations lack. World-music radio fuel (promoted from the census NO list 2026-06-05). **SHIPPED 2026-06-08** as `INSTR_MEMBRANE` (22) — see §8.5 step 8 for the macro mapping + the one deviation from the port (harmonics also crossfades the *ratios*, tuned↔Bessel) |

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

> **MT70 — CORRECTED 2026-06-07 (the 2026-06-03 "all one pure sine" verdict was a
> verification artifact).** The original check read `waveType = sine` at the top of each
> instrument block in `demo/songs/*.song` and stopped — the `osc2*` fields sit ~50 lines
> down the serialization, and they are **non-zero in those very songs** (Organ 0.4, Flute
> 0.15, Bells 0.5 *plus an osc3*, Vibes 0.3, JzOrg2 0.35). The authoritative source is
> `engines/instrument_presets.h:2813` ("Casio MT-70 (1982): **2 mixed sine waves** with
> different digital envelopes", after weltenschule.de): 9 of 10 presets carry a second
> sine at a harmonic ratio (2·/3·/4·, the Chime's 1.335 ≈ a fourth) with its **own faster
> decay**; Bells/Banjo add a third; Cosmic/JzOrg2 add a click transient. So the family is
> **small additive with per-partial decay** — bright strike fading to a pure fundamental.
> *(Verification lesson: read the whole serialized block, not its first field.)*
>
> **The conclusion survives, with better reasons: still no engine port needed.** Three
> shipped routes cover it: (a) the **struck half (Vibes/Celesta/Chime/Bells/Banjo) is
> `INSTR_MALLET`'s native territory** — decaying sine modes + strike click *is* the mallet
> engine; vibraphone is already a shipped mallet preset; (b) **exact reproduction = 2-slot
> voice stacking** (the sh101 SOURCE-MIXER lesson, audio-notes §14): fundamental slot +
> harmonic slot (+12/+19/+24/+5 st) with a faster ADSR — 2 voices/note, cheap at 16
> voices; (c) single-voice approximation: a drawn SCW (fund + harmonic) through a
> **closing `ENV_CUTOFF`** — the filter env kills the harmonic over time, which is most
> of the per-partial-decay percept. → Still belongs as a **shipped preset / demo cart**
> (§5.4 / §10.1), not as an engine — but note it is now the **first named concrete
> customer for the Additive engine** (2–3 partials with per-partial decay is literally
> that row's morph column), strengthening additive's case rather than the reverse.
>
> **Build order (decided 2026-06-07): cart-land first — the second-customer rule.**
> The Plaits-style "hide it in one engine" instinct is right *eventually* (one voice per
> key, live macros, scales to the 8–16-partial choir/bell territory), but MT70 alone is
> a recipe, and [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md)'s
> bar applies: a primitive must prove it can't be one. So: ship the **mt70 cart** on
> building blocks (mallet presets + 2-slot stacking — the entry on
> [`cart-library-direction.md`](cart-library-direction.md)'s instrument shelf has the
> technique split); it doubles as the **Additive probe** — its tuned ratio/decay table is
> half of the playbook's step-1 macro design. The engine graduates when the **second
> customer** appears (the rule that produced `radio.h`/`improv.h`): a choir/strings/bell
> pad station, or MT70-style tones wanted as *polyphonic pads* where stacking's
> 2–3-voices-per-key bill stops being affordable. Building additive sooner *because the
> MicroFreak aesthetic is attractive* is exactly the §13 cheap-lever trap.
>
> **mt70 SHIPPED 2026-06-07 + ear verdict (probe-carts.md).** The cart's A/B (`V`)
> pits 2-osc stacking against `INSTR_MALLET` on the struck presets. Owner verdict:
> *"they sound very different, I like both."* Reading: stacking and the engine are
> **complementary, not rival** — so this is a **soft, non-blocking** pull toward
> Additive (wanted for *range*, the choir/bell territory stacking can't reach), not
> evidence that stacking failed. The graduation trigger is unchanged: a second real
> customer, not "mt70 wanted it." §12 gap 2b stays deferred — mt70 never needed it.

> **Sine = Additive at `harmonics = 0`.** `INSTR_SINE` and the Additive engine aren't two things —
> additive *is* a sum of N sine partials, so one partial is a sine. The MT70 family (per the
> corrected note above: 2–3 partials, per-partial decay) sits at the **harmonics-low** corner of
> the Additive row — not at zero, which is exactly why it's that engine's first named customer.
> The whole bank is a path through its three macros: **harmonics** (0 = pure sine → up = more
> partials/body), **timbre** (spectral tilt / brightness — the lowpass), **morph** (inharmonicity:
> integer-ratio = organ/flute ↔ stretched = bell/metallic, + per-partial decay). So: Flute ≈
> (harm low, mellow, harmonic); Organ ≈ (harm up, harmonic, sustained osc2); Bells ≈ (mid,
> bright, **fast-decaying upper partials**); Vibes ≈ (low, bright strike, 4:1 partial fading
> first). When the Additive engine lands it subsumes the family with live macros — same engine,
> `harmonics` turned up off zero.

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

> **⚠️ Authoritative roster = [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md).**
> This §8.10 is the **routing-model sketch + wishlist**; 0015 (which carries the full pedalboard
> audit + the closed roster) disciplined it. Keep this for the bus-routing mechanics, but **where
> the two differ, 0015 wins.** Two corrections 0015 makes to the framing/table below:
>
> - **Leslie is NOT a bus insert (the table below is the old framing).** 0015 rules it a
>   **per-voice recipe**: tremolo (`LFO_VOLUME`) + Doppler (`LFO_PITCH`) + swirl (`LFO_CUTOFF`) +
>   tube growl (`drive`) — the three per-slot LFOs spent on one effect, the chorale↔tremolo spin-up
>   a cart-side `note_lfo()` rate `lerp`. *No `instrument_leslie`, ever.*
> - **Chorus is NOT a queued bus/delay effect — it's `detune`** (oscillator param): "two notes a
>   few cents apart *is* chorus." So the **Juno-60's chorus need is mostly the second-oscillator
>   detune plumbing (§12 gap 2b)**, not the delay-line "BBD chorus" sketched below. A *true*
>   modulated-delay chorus sits with **flanger** in 0015's deferral pile — it only lands free if
>   the master tape wow/flutter buffer ever ships. (The "BBD falls out of Leslie work" line below
>   is the pre-0015 reasoning; treat it as superseded.)
>
> **Build state (2026-06-10):** both shared buses are now SHIPPED — **`echo`** (2026-06-05) and
> **`reverb`** (2026-06-10, the §8.10 "begin small" first bite), the two-and-last admitted buses.
> Reverb landed as a SEND bus mirroring echo (`reverb(size,damping)` + `instrument_reverb`/
> `note_reverb`, navkit Schroeder port, mono v1) and the per-sample master section was formalized
> into the ordered send/insert-aware **MASTER FX SECTION** (see the banner at the top of this doc).
> The bite deliberately did NOT build `bus[NBUS]` + per-slot aux routing — that's the deferred
> next increment; v1 is master-only. Stereo (§9) was the real prerequisite and shipped
> 2026-06-09 — **specced in [`stereo.md`](stereo.md)**. Per-effect placement calls + the
> send-vs-insert reasoning: [`sound-next-steps.md`](../design/sound-next-steps.md).
>
> **chorus SHIPPED 2026-06-10** (the second §8.10 effect) — but NOT as a third send bus (the cap
> holds). It's the **first use of the shared master modulated-delay buffer** 0015 reserves (the
> "wow/flutter buffer"): a MASTER INSERT, `chorus(rate, depth, mix)`, navkit's BBD port with
> antiphase stereo taps (a centered mono mix → wide stereo — the Juno's two amps). The buffer
> landed early via chorus rather than tape; **flanger** (= same buffer + feedback + short delay)
> and **tape wow/flutter** (= same buffer + slow LFO) are now queued uses of it — the "write the
> mod-delay once, place it many ways" discipline (cf. the SVF serving filter/formant/wah). Master-
> wide (no `instrument_chorus` — 0015-faithful; per-part waits for aux routing). Showcase: **juno**.
> See [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md)'s 2026-06-10 update.

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
The cheap exception is **simple wah** — pedal/LFO/per-note-quack wah is just the per-voice SVF
swept, so it genuinely *is* per-note (the `epiano.c` clav quack is this). **But the realistic
"woah woah" auto-wah is a true bus effect** and belongs here on an aux bus: a bandpass on the
*summed* mix with an exponential sweep + an envelope follower tracking the *whole performance*
— so a chord sweeps coherently and the wah pumps with the groove (a per-voice filter can do
neither). Confirmed 2026-06-08 by rendering navkit (`tools/navkit-render.c`); corrects
[decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md)'s "wah is per-voice,
already covered" claim. So wah lands in TWO places: simple = per-voice recipe (done), great
auto-wah = this bus layer (deferred). The envelope follower's real home is here too (bus-level).

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

#### 8.10.1 PARKED — interim per-voice items that fold in here when this layer opens

These shipped per-voice as stand-ins because the bus layer doesn't exist yet. Each is explicitly
**interim, not the end goal** — kept because it's handy now, flagged so we don't build more on top
of it or mistake it for finished intent. When §8.10 opens, fold each into a shared, phase-coherent
bus processor (or remove it). *(Migrated 2026-06-09 from the retired `sound-handoff.md`; this is now
the canonical home — the §8.8.5 post-ship notes point here.)*

- **Per-voice wah (epiano AUTO / TOUCH flavours)** — a *simple* swept SVF. The real "woah woah" is
  the **bus auto-wah** specced above (bandpass on the summed mix). The clav **ENV quack** stays
  per-voice (a fast per-note filter-env snap, ~100ms — genuinely per-note and shippable); AUTO/TOUCH
  will likely be replaced by the bus wah.
- **The envelope follower (`instrument_follow` / `note_follow`)** — the 3rd modulation source
  (tracks a voice's own amplitude → cutoff/vol/pitch). Shipped + wired + tripwired, but its real
  home is **bus-level** (it should track the *whole performance*, not one voice), and it has no
  shipped per-voice customer. Re-evaluate when §8.10 lands; don't build on it meanwhile.
- **Per-voice epiano tremolo** (`epiano.c` `SL_TREM` → `LFO_VOLUME` on LFO slot 1) — the
  suitcase/Wurli amp wobble, the "electric" signature the dry tine+pickup model lacked. On real
  hardware tremolo is a **bus effect**: one LFO downstream of where the tines sum, so the whole
  keyboard pulses in lockstep. Our per-voice version resets each voice's LFO phase to 0 at note-on
  (`sound.h` `sound_*_start`), so **block chords struck on one frame wobble coherently** (the common
  case — sounds right) but **staggered/rolled notes drift out of phase** (hardware stays locked).
  Per-variant target when it graduates:
  - **Rhodes SUITCASE wants a stereo AUTO-PAN** — its "tremolo" is really the signal panning between
    two amp pairs (L↔R movement, not just a level dip). We're MONO today, so we only do amplitude
    tremolo; the suitcase's defining wobble needs a stereo field — itself a reason its home is the
    output/bus layer. The mono amp-trem is a stand-in until then.
  - **Wurlitzer wants mono amplitude tremolo** (the 200A's built-in trem genuinely is level
    modulation, deeper/faster than the Rhodes) — our per-voice amp-trem already matches it closely.
  - **Clav**: none (the preset zeroes it; a real clav has no tremolo).

All three want **one shared phase-coherent LFO on the summed mix**, alongside the bus auto-wah — and
the bus is where the suitcase stereo auto-pan can finally exist. See
[decision 0015 Correction](../decisions/0015-effects-are-recipes-not-primitives.md).
