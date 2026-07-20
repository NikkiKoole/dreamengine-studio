# dreamengine — audio design notes

STATUS: LIVING — the sound HUB (surface map, parameter placement, the audio-family index).

A living document for the sound system: what exists in code today, where it sits
relative to the chips it evokes, the ideas on the table, and a guiding rule —
**get as much expressivity as possible from as few primitives as possible.**

Companion to [`../VISION.md`](../VISION.md). Implementation lives in `runtime/sound.h`
(header-only, included once from `runtime/studio.c`); the public surface is the `sound`
section of `runtime/studio.h`. **Genre: design exploration** — rationale + roadmap for the
sound subsystem; status lives in [`../STATUS.md`](../STATUS.md), settled choices in
[`../decisions/`](../decisions/README.md) (e.g. [0003 code-first sound](../decisions/0003-code-first-sound.md)).

## The audio doc family — where does an audio idea go?

This file is the hub; substantial investigations get their own file (an
investigation longer than ~a screen should be spun out, with a one-line pointer
left here — that rule is what keeps this doc from regrowing).

| An idea about… | goes to |
|---|---|
| engine/synthesis wants, parameter placement, gaps | **here** (§12 gaps, §4 placement, §9 open questions) |
| a new modeled instrument (organ, reed, membrane…) | [`instrument-engines.md`](instrument-engines.md) — the navkit port program: §8.9 catalog, §8.8.2 playbook, §8.10 effects layer |
| a musical style / generative recipe | [`../guides/game-music.md`](../guides/game-music.md) (recipes, brain catalog, style cheat-sheet) |
| a new radio station to build | [`future-stations.md`](future-stations.md) — the candidates parking lot + build-order axes |
| a per-station timbre swap / engine retrofit | [`radio-instrument-options.md`](radio-instrument-options.md) |
| genre racks / generate-play-export / song.h | [`tinyjam-racks.md`](tinyjam-racks.md) |
| selling any of this (market, MIDI/AUv3, latency, trademark) | [`product-notes.md`](product-notes.md) — sketch-first decision + the parked builder items |
| timing jitter / drift (esp. web "drunk" playback) | [`audio-timing.md`](audio-timing.md) — frame-quantized triggers vs schedule-ahead; the web unclamped-`dt` hitch (latency is product-notes) |
| debugging a voice that stops (a solo "cut off", stolen/reused voices) | [`audio-voice-debugging.md`](audio-voice-debugging.md) — stem/solo render (`play.js --solo-slot`) + voice-allocation trace (`tools/voice-trace.js`), BUILT |
| the game/audio **thread model**, backends, the AudioWorklet migration | [`audio-threading.md`](audio-threading.md) — the lock-free queue, per-platform audio thread, runtime backend pick, staged build plan |
| holding + driving notes live (the shipped spec) | [`held-notes.md`](held-notes.md) |
| placing sound in the WORLD (pan/distance/Doppler, positional audio) | [`spatial.md`](spatial.md) — v1 per-voice + v2 emitter buses SHIPPED; v3 acoustic zones (inside/outside/occlusion) PROPOSED |
| modrack / patching | [`modular-synth.md`](modular-synth.md) |
| recording the player, looping it back | [`input-recording-looper.md`](input-recording-looper.md) |
| a cart to build (instruments, toys) | [`cart-library-direction.md`](cart-library-direction.md) |
| settled "why we (didn't)" | decisions [0003](../decisions/0003-code-first-sound.md) code-first · [0013](../decisions/0013-cut-music-api.md) music() cut · [0015](../decisions/0015-effects-are-recipes-not-primitives.md) effects-are-recipes |

---

## 1. Guiding principle: maximum range, minimum surface

The engine is for people learning C by making games. Every new function is a new
thing to read, hover, document, and misuse. So the bar for adding to the audio API
is not "is this useful?" but **"what is the leverage?"** — how much new sonic range
does one new concept unlock.

We rank ideas by *leverage*, not by feature-completeness. A single primitive that
turns into engines, sirens, sliders, and theremins beats five one-trick calls.

Three corollaries:

- **Prefer one reusable concept over many special cases.** One "instrument" concept
  unlocks ADSR + vibrato + duty + cart-authored sound at once. That's high leverage.
- **Keep the easy path zero-setup.** Beginners should still be able to `note(60, INSTR_SAW, 5)`
  with no ceremony. Richer features are opt-in, never mandatory.
- **Match the hardware mental model where it's also the simplest model.** Real chip
  audio *is* writing registers to fixed channels every frame. Leaning into that is
  both authentic and beginner-friendly — a rare alignment.

---

## 2. Where we are now (in code)

> **Disabling audio entirely:** `DE_AUDIO=off` (env) skips `InitAudioDevice` + `sound_init` + the
> miniaudio callback thread — cart audio calls become harmless no-ops. The effects bus (reverb tail,
> set-and-hold inserts) processes the master every callback even on silence, so this is a real CPU
> saving on sound-free carts, a low-end lever, and it de-noises CPU profiles. (`--wav` forces audio
> back on — it needs the synth.) Profiling how-to: [`../guides/profiler.md`](../guides/profiler.md).

> **Refreshed 2026-06-07**, after the 2026-06-05 wave landed: the first three modeled
> engines (`INSTR_PLUCK/MALLET/FM` + the six macro functions —
> [`instrument-engines.md`](instrument-engines.md)), `instrument_choke` (§12),
> drive / master soft-clip / the echo bus (§17), and the 8→16 voice flip (§15).
> Prior refresh 2026-06-04 (the §11 mod-envelopes; `schedule_hit`, `wave_set` +
> `INSTR_USER0..3`; `music()` cut — [decision 0013](../decisions/0013-cut-music-api.md)).
> This is the authoritative map of the shipped sound surface: **45 functions +
> 39 constants** (recount with `node tools/api-usage.js` when in doubt). §5–§11 hold the
> rationale for how it got here; STATUS item 5 holds what's next (organ, SFX authoring).

**Engine**
- 16 voices (`SOUND_VOICES` — 8 until 2026-06-05, raised after the §15 starvation
  measurement), 44.1 kHz, **mono**, software-mixed (sum × 0.2 gain each, then the §17
  **master soft-clip**: linear below a ±0.8 knee, tanh-shaped above — quiet mixes
  bit-identical, hot mixes glue instead of slamming a wall). Runs on raylib's
  audio-stream callback (the **audio thread**).
- Voice allocation: first free → else a **non-held** voice → else voice 0. Held notes
  are stolen last — they're meant to last. Ringing engines get a ~3ms steal-declick tail.
- Held-note **handles** pack slot (low `SOUND_HANDLE_BITS` = 4 bits, guarded by a
  `_Static_assert` against `SOUND_VOICES`) + generation; a stale handle (its voice was
  stolen or the slot reused) silently no-ops. 16 handle slots. See `held-notes.md`.

**The surface, in four layers**

1. **Just make sound — zero setup.** `note` `hit` `chord`(+9 `CHORD_*`) `strum`
   `tone`(+6 `SCALE_*`) `schedule` `schedule_hit` `sfx`. Slots 0–4 come pre-filled
   with the raw waves (`INSTR_SQUARE/SAW/TRI/NOISE/SINE`), so the first
   `note(60, INSTR_SAW, 5)` needs nothing else. `schedule_hit` (delay **+** duration) fills
   the note-call quadrant that lets a cart sequence sub-frame sfx steps sample-accurately.
2. **Design an instrument.** A slot (5–31) is the container; one call per axis —
   `instrument` (timbre + **amp ADSR**) · `instrument_duty` (pulse width) ·
   `instrument_lfo` (**×3**, cyclic modulation) · `instrument_env` (**×2**, one-shot AD
   modulation, bipolar amount — §11) · `instrument_filter` (resonant SVF, 5 `FILTER_*`
   modes) · `instrument_drive` (post-filter tanh, §17) · `instrument_echo` (send to the
   one echo bus, §17) · `instrument_choke` (new note on a kills b — hats, §12).
   Timbre itself is extensible two ways: **`wave_set` + `INSTR_USER0..3`** are four DRAWN
   single-cycle waveforms (64 samples, live-morphable — the §8.4 SCW idea; draw them in
   the `wave editor` cart), and **`INSTR_PLUCK/MALLET/FM`** are modeled engines
   ([`instrument-engines.md`](instrument-engines.md)), each shaped by the three macros
   `instrument_harmonics/timbre/morph` — all work anywhere a wave id does.
3. **Play it live — held notes.** `note_on`→handle, `note_off`, `note_off_all`;
   performance gestures `note_pitch` / `note_vol` / `note_glide`; live twins of the
   defines: `note_duty` `note_lfo` `note_env` `note_filter` `note_cutoff` `note_res`
   `note_drive` `note_echo` `note_harmonics` `note_timbre` `note_morph`.
   Live writes slew per-sample (no zipper).
4. **Musical time & theory.** `bpm` `beat` `beat_pos` `every` (the clock, ticked in
   `sound_tick(dt)`) · `euclid` `chance` `degree` (rhythm + scale math).

**The modulation matrix is complete** — every parameter is reachable three ways
(cyclically, per-note, and by hand). This table *is* the synth:

| Destination | LFO (cycles) | mod-env (one-shot) | live (`note_*`) |
|---|---|---|---|
| pitch  | `LFO_PITCH` vibrato   | `ENV_PITCH` punch / zap | `note_pitch` (+ `note_glide`) |
| cutoff | `LFO_CUTOFF` wah      | `ENV_CUTOFF` the "pew"  | `note_cutoff` / `note_res` |
| duty   | `LFO_DUTY` PWM shimmer| `ENV_DUTY` PWM sweep    | `note_duty` |
| volume | `LFO_VOLUME` tremolo  | — *(the amp ADSR — deliberate, no `ENV_VOLUME`)* | `note_vol` |
| harmonics | `LFO_HARMONICS` *(SNAPPED — steps detents)* | `ENV_HARMONICS` | `note_harmonics` |
| timbre | `LFO_TIMBRE` *(PD-reso = filter sweep, no filter)* | `ENV_TIMBRE` | `note_timbre` |
| morph  | `LFO_MORPH` *(PD DCW / FM fb / organ chorus / EP bark)* | `ENV_MORPH` | `note_morph` |

The bottom three rows — the **engine macros** — were added 2026-06-08 (§2.1); they're the
per-engine 3-axis surface (INSTR_PLUCK+), so what each modulation *does* is per-engine.
Two newer parameter families still ride only the define + live halves and are deliberately
**not** LFO/env destinations: `drive` and the per-slot `echo` send.

### 2.1 Macros as LFO/mod-env destinations — SHIPPED 2026-06-08

> **Status: shipped + proven.** Closed the matrix's biggest hole. Born from the PD `morph`
> = DCW-sweep macro: "LFO the DCW sweep" wanted `morph` to be an LFO target. General across
> all six engines, not PD-only. Second customer: modrack's VIBE module now offers
> `har/tmb/mor` destinations into the MACRO voice's `vb` jack — `preset_macro` ships a PD
> patch with the VIBE sweeping morph at audio rate. Proof: a deterministic A/B render
> (VIBE depth 0.5 vs 0) diverges byte-for-byte, so the LFO demonstrably reaches the engine.

**What ships:** the macro family gains its LFO + mod-env columns. Six new destination
*constants* — `LFO_HARMONICS / LFO_TIMBRE / LFO_MORPH` and `ENV_HARMONICS / ENV_TIMBRE /
ENV_MORPH` — reusing the existing `instrument_lfo` / `note_lfo` / `instrument_env` /
`note_env` calls (NO new functions; the live `note_harmonics/…` half already exists).

**The apply (no per-engine edits).** The mix loop already computes per-sample LFO/env
contributions at the bottom of the sample block (`sound.h` ~1450-1473) into `pitch_mul /
cutoff / duty / trem` for the next sample. Add three more accumulators — `harm_mod /
timb_mod / mor_mod` — fed by the new dest cases. Then at the oscillator call, for engine
voices only, **save → set effective macro = `clamp(base + mod, 0, 1)` → call
`sound_engine_sample` → restore**. Localized to the mix loop; every engine reads
`v->harm/timb/mor` unchanged; zero cost when no macro is modulated (mod == 0).

**Units:** macros are `0..1`, so LFO `depth` and env `amount` for a macro dest are `0..1`
(a swing / bipolar offset), not Hz or semitones.

**Per-engine meaning (each macro means something different — that's the point):**
- `timbre` / `morph` are **continuous** → smooth modulation. PD: `timbre` = a resonant
  filter sweep *with no filter* (the cheap auto-wah — note this is the per-voice cousin of
  the PARKED §8.10 bus wah), `morph` = wobbling the DCW depth. FM: `timbre` = brightness
  LFO, `morph` = feedback sweep. Organ: `morph` = chorus/animation depth. EP: `morph` = bark.
- `harmonics` is **SNAPPED** to detents inside each engine → modulation **steps** through
  them (wavetype / ratio / instrument). A feature on PD (a wavetype sequencer off an LFO)
  but jarring on EP (Rhodes→Wurli→Clav mid-note). Ship it; document the spice.

**Wiring:** 4 places for the 6 constants (`studio.h` + `studioDocs.js` + `shell.js`; no
tcc symbols — constants), a `soundcheck` line exercising a macro-LFO, the sound tripwire.

**Resolved at build:** shipped **all six** (incl. the snapped `harmonics` — its detent-stepping
is a useful spice, e.g. a wavetype sequencer on PD, and symmetry keeps the matrix honest). The
§2 matrix table now carries the three macro rows; the "deliberately not destinations" note now
covers only `drive` + `echo`. Constants: `LFO_HARMONICS/TIMBRE/MORPH` = 4/5/6,
`ENV_HARMONICS/TIMBRE/MORPH` = 3/4/5. Apply = a save→`clamp(base+mod)`→restore around
`sound_engine_sample` (no per-engine edits; zero-cost when unmodulated).

**Data model**
- **SFX**: 32 slots × up to 32 steps `{pitch (MIDI), instr, vol 0..7}`; `step_dur` in
  10ms units; per-SFX loop flag. (**Music patterns are gone** — `music()` cut 2026-06-04,
  [decision 0013](../decisions/0013-cut-music-api.md): zero cart users, the generative
  beat-clock path won.)
- ⚠️ **The SFX bank is still hardcoded** in `sound_load_demo_data()` (6 demo slots) — no
  cart-side authoring. Settled direction: carts author sound *as code* (the sfx-editor /
  sfx-generator carts export paste-ready C); `sfx(n)` exists for first-contact demos and
  `sfx(-1)` as the "silence ringing sounds" verb. A live-performance answer (play music
  in, capture it as control events) is explored in
  [`input-recording-looper.md`](input-recording-looper.md).

**Thread-safe control (the important architectural fact)**
- Main thread → audio thread via a 512-entry **request ring buffer**; kinds 0–28 cover
  play (0–2), define (3–6, 18, 20 = `wave_set` packed 4 samples/request, 21 macros,
  23 choke, 24 drive, 26–27 echo), and held-note live control (7–17, 19, 22, 25, 28).
  Delayed requests (`strum`/`schedule`/`schedule_hit`) sit in a 64-entry holding pen on
  the audio thread.
- **The ring buffer is the one correct place to mutate sound state.** Every new control
  surface rides it rather than poking shared structs (the §11 mod-envelopes are kinds 18/19).
- **Overflow is a tripwire, not silence**: dropped requests are counted and `sound_tick`
  printh-screams `[sound] WARNING … DROPPED` (editor log / bake / play.js output). The
  `soundcheck` cart is the worst-case self-test — run it after touching `sound.h`.
- ⚠ **Ordering subtlety**: define kinds apply when *drained* (next callback), but a
  *delayed* note snapshots its instrument slot **at fire time** — per-step instrument
  changes for scheduled notes therefore need a **rotating slot per pending step** (see the
  sfx editor's CUT lane), or the last define wins for every queued note.

**Rules that shape everything**
1. **Wave + amp-ADSR snapshot at note-on** — you can't re-attack or re-timbre a ringing
   voice. Everything else (filter, duty, LFOs, mod-envs, pitch, volume) is live on a held note.
2. **One-shots stay fire-and-forget**; continuous control is the held-note layer (§6 → §10.3,
   shipped). The two models coexist; handles make the live layer safe.

**Known warts (accepted, documented)**
- LFO `depth` / env `amount` **units depend on the destination** (semitones / Hz / 0..1) —
  the §10.2 wart; named sugar wrappers are the known escape if it ever bites.
- Mixed ranges (vol 0–7, res 0–15, sustain 0–7) — each fits its domain; small cognitive tax.
- The mod-env is **AD-only** (no sustain/release stage) — widening noted in §11.2.
- No master volume / mute / looping ambience yet (STATUS "smaller open items").

---

## 3. Where that sits vs. the chips

> **Status: HISTORICAL — the 2026-05 gap analysis; every ❌ below has since shipped.**
> Kept because this table is what motivated §5–§11. The ❌s became: variable duty + PWM
> (`instrument_duty`/`LFO_DUTY`), full ADSR (`instrument`), resonant multimode filter
> (`instrument_filter`, per-slot), vibrato + pitch sweep (`instrument_lfo`/`instrument_env`)
> — and voices are 16 now (§15), with three modeled engines on top
> ([`instrument-engines.md`](instrument-engines.md)). Still true today: no ring mod / hard
> sync (AM/ring sits in the engine catalog, instrument-engines §8.9) and no PCM samples
> (the FM-clang hat workaround covers the 909 case). The honest 2026 comparison is no
> longer SID/NES — §15 calls it SNES/Amiga-class. Current surface: §2.

Short version: it's chiptune-family, but **cleaner and more generic** than either the
C64 SID or the NES APU, because the *expressive* tools that define those chips are the
ones we're missing.

| Feature | C64 SID | NES APU | dreamengine now |
|---|---|---|---|
| Voices | 3 | ~5 | **8** (more than either) |
| Waveforms | tri/saw/pulse/noise | pulse/tri/noise/DPCM | square/saw/tri/noise/sine |
| Duty / PWM | ✅ variable (signature) | ✅ 4 duties | ❌ square locked at 50% |
| Envelope | ✅ full ADSR | ✅ volume decay | ❌ fixed declick only |
| Filter | ✅ resonant multimode (its identity) | ~fixed | ❌ none |
| Pitch sweep / vibrato | via envelope | ✅ sweep unit | ❌ hand-stepped in SFX |
| Ring mod / hard sync | ✅ | ❌ | ❌ |
| PCM samples | — | ✅ DPCM | ❌ |

Takeaways:
- **C64 is the least accurate comparison** — the SID's identity is filter + PWM + ring
  mod, none present.
- **Closest real chip is the NES APU minus duty cycles, sweep, and DPCM.**
- **Polyphony exceeds the real hardware** (8 vs 3/5), so we don't impose authentic
  voice-stealing scarcity.
- Timbre is "clean chiptune"; the sine is something neither chip truly had.

---

## 4. The decision that drives all additions: *where does a parameter live?*

Every new knob (duty, attack, cutoff, vibrato…) must attach somewhere. Three homes:

| Home | Shape | Right for | Wrong for |
|---|---|---|---|
| **On the note** | `note(midi,instr,vol,duty,attack…)` | one-off tweaks | bloats fast — unusable past ~2 extra args |
| **On an instrument** | `instrument(slot,…); note(midi,slot,vol)` | timbre: duty, ADSR, vibrato — set once, reuse | adds a setup step + a concept |
| **Global / master bus** | `filter(mode,cutoff,res)` | one shared effect (the SID filter) | can't differ per voice |

**The extension point already exists:** `instr` is currently a *waveform id* (0–4).
Reinterpret it as an **instrument slot**: pre-fill 0–4 with today's waveforms + the
default declick envelope, let carts define 5+. Every existing cart keeps working
unchanged, and duty/ADSR/vibrato get a natural home without touching `note()`'s
signature. **Bonus:** the same bank mechanism is what carts need to author their own
sounds at all (see the §2 gap) — one concept closes two holes.

---

## 5. Ideas, ranked by leverage

### 5.1 Pulse duty — *highest leverage, near-zero cost*
The oscillator already branches on wave; square is `phase < 0.5f`. Make it `phase < duty`.

```c
// Option A — just more waveform ids (zero signature change, most NES/PICO-8-like)
#define INSTR_PULSE_12  5   // thin / nasal
#define INSTR_PULSE_25  6
// INSTR_SQUARE (50%) already exists
```
- **Leverage:** ~3 lines + 2 constants → a big jump in timbral range. Covers ~90% of
  "chip" character.
- **Limit:** fixed duties can't *animate* (no PWM shimmer) — that needs duty as an
  instrument field or a live channel (§6).
- **Verdict:** ship first.

### 5.2 Slide / portamento — *easy, self-contained*
A one-shot glide. Fits the existing `note`/`hit` family; no new concept.
```c
void slide(int midi_from, int midi_to, int instr, int vol, int dur_ms);
```
- Implementation: lerp `freq` across the voice's life. Small.

### 5.3 Preset instruments — *easy, beginner-facing*
Before the full bank, ship 2–3 baked envelopes as named ids:
```c
#define INSTR_PLUCK  …   // fast attack, short decay
#define INSTR_PAD    …   // slow attack, long release
```
- **Leverage:** real envelopes with *zero* params for the learner. Buys time before the
  instrument concept lands.

### 5.4 Instrument bank (ADSR + vibrato + custom duty) — *the keystone*
The one real concept jump; pays for itself many times.
```c
// attack/decay/release in ms, sustain 0..7
void instrument(int slot, int wave, int attack_ms, int decay_ms, int sustain, int release_ms);
void instrument_vibrato(int slot, float depth_semitones, float rate_hz);
// (duty becomes a field here too)
instrument(5, INSTR_SAW, 5, 120, 3, 200);   // plucky
note(60, 5, 7);
```
- **Leverage:** one concept unlocks ADSR **+** vibrato **+** animated duty **+**
  cart-authored timbres simultaneously. Highest feature-per-concept ratio on the list.
- **Costs:** "define then play" is a mental step; release means voices outlive their
  nominal duration → voice-stealing gets slightly smarter. Bank is read by the audio
  thread, written via the ring buffer (same pattern as `sfx_bank`).

### 5.5 Master filter — *biggest "SID" payoff, do last*
DSP is trivial (a state-variable filter is ~6 lines). Do **one global** filter, not
per-voice.
```c
#define FILTER_OFF 0
#define FILTER_LOW 1
#define FILTER_HIGH 2
#define FILTER_BAND 3
void filter(int mode, int cutoff_hz, int resonance);   // res 0..15

// sweep it per frame for the classic SID "wow"
filter(FILTER_LOW, 400 + (int)(noise(now()) * 3000), 10);
```
- **Leverage:** one call → the single most C64-sounding result available; cheap (one
  filter, not eight); sweepable for free by calling it each frame.
- **Risks:** hits everything (drums/noise too); resonance self-oscillation must be
  clamped; the one feature that can "sound broken" for beginners.

### 5.6 Expose the SFX bank to carts — *fills a current hole*
Independently of the above, carts can't author `sfx` data today. A small
`sfx_def(slot, steps…)` API would make the existing bank machinery actually usable
from a cart. Low-glamour, decent leverage (unlocks data-driven sound + a future
tracker UI to match the sprite editor — already a stated VISION goal).
(The *pattern* half of this idea is dead: `music()` was cut 2026-06-04,
[decision 0013](../decisions/0013-cut-music-api.md) — any future pattern playback is
an export-as-code editor cart, not engine banks.)

> **Direction (2026-06-04, leaning — not locked):** prototype this as a **PICO-8-style
> editor CART first, with zero new engine API.** Draw the pitch contour over steps with
> the mouse, toggle wave/vol per step (instrument slots 5–15 give each "column" a full
> voice — richer than PICO-8's fixed waves). Everything needed already ships:
> **playback** = the cart sequences its own steps with `hit()`, `schedule()`-ed ahead for
> sample-accurate timing off the beat clock; **persistence** = `save_bytes`/`load_bytes`
> (the modrack pattern); **into a game** = export-as-C-code (a step array + a ~10-line
> player), which is the decision-0003 "code-first" answer anyway. Only if the prototype
> proves the *engine* should own banks (determinism for play.js, a real tracker UI) does
> `sfx_def()` get built — and the prototype then informs its shape.

### 5.7 Quick effort/payoff table
| Idea | Effort | Payoff | New concept? |
|---|---|---|---|
| Pulse duties (constants) | tiny | high | no |
| `slide()` | small | medium | no (1 flat call) |
| Preset instruments | small | medium | no |
| Instrument bank (ADSR/vib/duty) | medium | **high** | yes (the keystone) |
| Master `filter()` | medium | high (most "SID") | no (1 global call) |
| Cart SFX authoring | medium | medium | yes (data model) |

---

## 6. Real-time / continuously-adjustable sound

> **Status: SHIPPED — but as handles, not channels.** This section sketched fixed *held
> channels* (`ch_play`/`ch_pitch`/…). We instead shipped a **handle** model —
> `note_on()→handle` / `note_off()` + `note_pitch`/`note_vol`/`note_cutoff`/`note_duty` —
> because handles give polyphony for free and match the MIDI mental model. The §6.2 slew
> insight below is exactly what we built. Full rationale + the shipped API:
> [`held-notes.md`](held-notes.md). The channel sketch below is kept as the original
> exploration.

Goal: sounds the *game* drives frame-by-frame — an engine that revs with speed, a UI
slider that pitches up, a charging laser, wind that swells, a siren, a theremin.

**The trap: "just re-set the instrument every frame" does not work** — for three
reasons rooted in §2:
1. Voices **snapshot** at note-on, so mutating a bank entry won't move the sound you
   currently hear.
2. One-shots **end**; re-triggering `note()` every frame resets phase → machine-gun
   clicking, not a morph.
3. An instrument is a **shared template** — two emitters on one instrument can't differ.

The right target is a **live voice you write to continuously**, not the template. This
is also exactly how a SID/NES program works: keep poking a channel's registers and it
sings until you change them.

### 6.1 Models
| Model | Shape | Independent emitters? | Beginner cost | Authenticity |
|---|---|---|---|---|
| Mutate instrument | `instrument(slot,…)`/frame | ❌ shared | low | — (fights it) |
| **Held channels (fixed N)** | `ch_pitch(0,…)`/frame | ✅ per channel | low–med | **high (= real chip)** |
| Voice handles | `int v=sound_play(); set(v,…)` | ✅ per handle | med (lifetime mgmt) | high |

**Recommended: held channels.** A "channel" is just one of the 8 voices, reserved,
sustaining, and marked non-stealable. Fixed channel numbers mean no allocation and no
handle-lifetime problem — finite and predictable, like the hardware.

```c
void ch_play (int ch, int midi, int instr, int vol);  // start a held tone (vol 0 = silent)
void ch_pitch(int ch, float midi);   // float → glides
void ch_vol  (int ch, int vol);      // 0..7; 0 gates off without freeing the channel
void ch_duty (int ch, float duty);   // real-time PWM
void ch_cutoff(int ch, int hz);      // when there's a filter
void ch_stop (int ch);
```
```c
void init(void)  { ch_play(0, 40, INSTR_SAW, 0); }      // start silent
void update(void){
    ch_pitch(0, 36 + speed * 0.6f);                      // pitch tracks speed
    ch_vol  (0, speed > 0.1f ? 5 : 0);                   // fade at a stop
}
```

The `sound_play() → handle` variant is the alternative when you want *many* transient
controllable sounds without assigning channel numbers; its cost is handle invalidation
on voice-steal (needs a generation counter). For a learning engine, fixed channels win.

### 6.2 The gotcha that bites *every* continuous approach: smoothing
Parameters change at 60 fps; audio runs at 44,100. If a voice reads the latest value
raw, that's **60 discrete jumps/sec** → stairstepping on pitch and outright **zipper
noise** on volume/cutoff. Naive "change it continuously" sounds *bad*.

Fix: **per-parameter slew.** Store `target` and `current` per knob; in the audio
callback glide `current += (target - current) * k` each sample. (Generalizes the
existing per-step declick.) Pitch wants light smoothing or it feels laggy; volume and
cutoff want more.

**So the real work in real-time audio is not the API — it's (a) a persistent voice that
doesn't auto-free, and (b) the per-param slew.** Both are small; (b) is the line between
"sounds like a synth" and "sounds broken," and is the only genuinely new DSP.

### 6.3 Coexistence
Keep both worlds; they share the 8 voices:
- **Events** (fire-and-forget): `note/hit/chord/sfx` + the instrument bank → blips, hits, music.
- **States** (held/continuous): the channel model → engines, sliders, sirens, ambience.

---

## 7. Suggested roadmap

A path that front-loads zero-setup wins, then introduces the two real concepts:

1. **Pulse duties** (`INSTR_PULSE_*`) — tiny, high payoff, no new concept.
2. **`slide()`** — easy, self-contained.
3. **Preset instruments** (`INSTR_PLUCK/PAD/…`) — easy, real envelopes for free.
4. **Held channels + per-param slew** (§6) — the real-time foundation; also proves the
   smoothing that everything dynamic depends on.
5. **Instrument bank** (ADSR + vibrato + custom duty) — the keystone; also unlocks
   cart-authored timbres.
6. **Master `filter()`** — the cherry; biggest "SID" payoff, riskiest, benefits from
   4–5 existing.
7. **Cart SFX/pattern authoring** — fills the §2 gap; groundwork for a tracker UI.

Steps 1–3 are flat additions that fit the current "just call it" vibe with no new mental
model. Step 4 and step 5 are the two concept jumps; everything dynamic and everything
rich, respectively, hangs off them.

---

## 8. Borrowing rich instruments from navkit → moved

**Moved to [`instrument-engines.md`](instrument-engines.md)** (2026-06-07 — it had grown
to 40% of this file). **Section numbers are preserved there verbatim**, so every `§8.x`
reference in this file and elsewhere (the §8.1.1 macro discipline, the §8.2 buffer
decision, the §8.5 phase ordering, the §8.8.2 engine-shipping playbook, the §8.9
candidate catalog, the §8.10 effects layer) resolves in that doc, same numbers.

## 9. Open questions

- ~~**Channels vs. handles** for real-time~~ — **Resolved (shipped): handles won.**
  `note_on()` returns a slot+generation handle; stale handles silently no-op. See
  [`held-notes.md`](held-notes.md).
- ~~**Filter scope:** global-only or per-channel?~~ — **Resolved (shipped): per-instrument.**
  Each slot carries its own SVF (`instrument_filter`); 8 SVFs are negligible on desktop.
- ~~**Where does duty live?**~~ — **Resolved (shipped): on the instrument** (`instrument_duty`)
  **+ live** (`note_duty`). The `INSTR_PULSE_*` named shortcuts were not added — folded into
  the zero-setup preset-bank idea (§5.3) if it ever ships.
- ~~**Stereo?**~~ — **Resolved (SHIPPED 2026-06-09): yes.** The path is 2-channel;
  `instrument_pan`/`note_pan` + `LFO_PAN` place a voice in the field. **Linear pan law,
  center unchanged** — a centered voice is byte-identical to the old mono mix, so it doubles
  only the final accumulate (not the per-voice synthesis) and existing carts are untouched. The
  "low leverage for this audience" worry is answered by keeping the surface tiny: one pan knob,
  default center, never required. Echo stays a mono bus in v1; stereo width (ping-pong, reverb
  spread) belongs to the §8.10 effects layer this unblocks. Full spec + the bite checklist:
  [`stereo.md`](stereo.md).
- **Tracker UI**: VISION mentions a sound tracker to match the sprite editor.
  *Direction (2026-06-04):* leaning **PICO-8-style and prototyped as a CART** — draw the
  pitch contour over steps, toggle wave/vol per step — which needs **no new engine API**
  (see §5.6). The cart prototype is the cheap way to find out if the editor earns a place
  before any engine-side bank API is built.
- ~~**Voice budget:** with held channels reserving voices, do we raise `SOUND_VOICES`, or
  is 8 still plenty once some are long-lived?~~ — **Resolved (2026-06-05, measured): 16.**
  8 was hitting voice starvation *and* handle-slot exhaustion on the dense carts; the flip
  is measured-safe with the 0.2 scale kept. Full experiment + the handle-bits landmine: §15.
- ~~**Per-voice buffers (§8.2):**~~ **Resolved (2026-06-03): yes** — a `Voice` may carry a small
  ~2 KB delay-line buffer. Wanted by the organ scanner and reused across the whole Karplus-Strong /
  physical-modeling family (pluck / piano / guitar). See §8.2.
- **Curated instruments vs. instrument bank:** ship finished `INSTR_*` presets from navkit
  (§8) *and/or* build the cart-authorable bank (§5.4)? They're complementary, but which is
  the headline for the audience — sounding great out of the box, or authoring your own?
- **One filter primitive, two uses:** commit to a single state-variable filter that serves
  both the SID-style `filter()` sweep (§5.5) and the vowel formant bank (§8.3)?

---

## 10. The four axes, one container — SHIPPED

> **Status: shipped** (originally HEAD `141a0a7`; updated since). All four axes below are
> live on the raw waveforms, bundled per instrument slot: `instrument()` (ADSR),
> `instrument_duty()`, `instrument_lfo()`, `instrument_filter()` — **plus a fifth axis
> since: `instrument_env()` (×2 mod-envelopes, §11)**. Deltas from the original sketch:
> **3 LFOs per slot**, so `instrument_lfo()` takes a `which` (0–2) index; **held voices
> shipped as `note_on()` + handle-based `note_*` setters** (see `held-notes.md`), not the
> `ch_*` channel API sketched in §10.3 — same capability, handle-safe. The `dream synth`
> cart now plays real held notes (hold-to-sustain) with live filter/LFO control and the
> §11 filter-contour/pitch-env editors. Carts: `instruments`, `lfo`, `filter`,
> `dream synth`, `filterenv`, `pitchenv`. §2 ("where we are now") was refreshed 2026-06-04
> and is the current surface map. (The original proposal text is kept below as rationale.)

A concrete API sketch for the whole expressive space. The realization that drives it: the
range we want isn't a *list* of features — it's **four orthogonal axes that multiply**, and
an **instrument is just the container that bundles them**. It is not a fifth axis.

```
instrument (slot) = timbre × envelope × LFO × filter
       └─ played by → note() / chord() / ch_play()
```

| Axis | What it controls | Lives on |
|---|---|---|
| **Timbre** | which oscillator/engine (raw wave → pulse → §8 modeled) | instrument `wave` field |
| **Envelope** | amplitude shape over time (ADSR, or the engine's baked default) | instrument |
| **LFO / modulation** | motion over time — *one routable LFO* | instrument |
| **Filter** | resonant SVF (subtractive **+** formant) | instrument (or global — see open Q) |

8 timbres × a few envelopes × a few LFO routings × a few filter modes is enormous range from
~4 small concepts — exactly the §1 thesis. Crucially, the four axes don't all *compose*: duty
is pulse-only (a no-op on organ/piano), and a user ADSR layered on a §8 modeled instrument
tends to flatten the very motion that makes it sound expensive. So the modeled engines treat
ADSR as optional override, never replacement (see below).

### 10.1 The easy path stays zero-setup

`instr` becomes a **slot id**, not a waveform id. Slots 0–4 are pre-filled with today's waves
+ the declick envelope; presets we ship (`INSTR_PLUCK`, `INSTR_PAD`, `INSTR_ORGAN`, …) are just
pre-baked slots. Every existing cart keeps working unchanged, and most carts never define an
instrument at all.

```c
note(60, INSTR_SAW, 5);                      // raw wave — unchanged
note(60, INSTR_PLUCK, 5);                    // a baked preset, zero setup
chord(48, CHORD_MAJ7, INSTR_ORGAN, 5);       // a §8 modeled engine, same call shape
```

### 10.2 Defining an instrument — base call + one modifier per axis

```c
// axis 1+2: timbre (which engine) + ADSR envelope.  a/d/r in ms, sustain 0..7
void instrument(int slot, int wave, int attack_ms, int decay_ms, int sustain, int release_ms);

// axis 1 detail: pulse width (only meaningful for pulse/square timbres; no-op otherwise)
void instrument_duty(int slot, float duty);                 // 0..1

// axis 3: ONE routable LFO — this single call IS vibrato / tremolo / PWM / wah
void instrument_lfo(int slot, int dest, float rate_hz, float depth);
//   LFO_PITCH    depth = semitones   → vibrato
//   LFO_DUTY     depth = 0..1        → PWM / duty sweep
//   LFO_VOLUME   depth = 0..1        → tremolo
//   LFO_CUTOFF   depth = Hz          → filter wah

// axis 4: resonant state-variable filter (the §5.5 SID filter AND the §8.3 formant)
void instrument_filter(int slot, int mode, int cutoff_hz, int resonance);   // res 0..15
//   FILTER_OFF / LOW / HIGH / BAND / NOTCH / FORMANT
```

Four functions define *any* instrument. Each call is short, maps to exactly one axis, and is
optional. The LFO destination-dependent `depth` units are a small wart — named sugar wrappers
(`instrument_vibrato`, `instrument_tremolo`, …) are an option if it bites, at the cost of more
surface.

```c
void init(void) {
    instrument(5, INSTR_PULSE_25, 2, 80, 4, 120);   // timbre + ADSR
    instrument_duty(5, 0.25f);                       // thin pulse
    instrument_lfo (5, LFO_DUTY, 4.0f, 0.15f);       // ...that shimmers (PWM)
    instrument_lfo (5, LFO_PITCH, 6.0f, 0.2f);       // ...with a touch of vibrato
    instrument_filter(5, FILTER_LOW, 1800, 9);       // ...rounded off
}
void draw(void) { if (every(60)) note(60, 5, 6); }
```

One timbre × ADSR × two LFO routings × a filter, from four lines — the multiply in action.

### 10.3 Real-time control — held channels play an instrument live (§6)

The instrument is the *template*; a channel is a held voice you write to per-frame. Same four
axes — the difference is the *game* is the modulator instead of the LFO.

```c
void ch_play  (int ch, int midi, int instr, int vol);   // start held (vol 0 = silent)
void ch_pitch (int ch, float midi);                      // float → glides (slewed, §6.2)
void ch_vol   (int ch, int vol);                         // 0 gates off, keeps the channel
void ch_duty  (int ch, float duty);
void ch_cutoff(int ch, int hz);
void ch_stop  (int ch);
```

```c
void init(void)  { ch_play(0, 40, INSTR_SAW, 0); }                  // start silent
void update(void){ ch_pitch(0, 36 + speed*0.6f);                    // engine rev
                   ch_vol  (0, speed > 0.1f ? 5 : 0); }             // fade at a stop
```

### 10.4 How the §8 engines slot in

`INSTR_ORGAN`/`INSTR_EPIANO`/… are just more `wave` values — no new API, no new concept:

```c
instrument(6, INSTR_ORGAN, 0, 0, 7, 80);    // instant attack, sustain, fast release
```

Rule that resolves the ADSR-vs-baked-envelope conflict: **all-zero ADSR = use the engine's
baked default.** A non-zero user ADSR overrides *amplitude only*, never the internal timbral
motion (key-click, pickup growl, dispersion). `instrument_duty` is a no-op on non-pulse engines.

### 10.5 Surface count and the one open decision

The entire grid costs: **4 define-calls + 6 channel-calls + a handful of `INSTR_*` / `LFO_*` /
`FILTER_*` constants.** Tiny, for the whole space.

The one real decision baked into this sketch: **filter scope — per-instrument (as above) or one
global master bus?** Per-instrument composes cleanly and gives formant vowels for free, but costs
8 SVFs instead of 1; global is cheaper and more SID-authentic but can't differ per voice. On
desktop 8 SVFs is negligible, so this sketch leans per-instrument — but it's the live question
from §9 ("filter scope", "one filter primitive, two uses") and wants a deliberate call before
building.

## 11. Modulation envelopes — the second EG the synth is missing

> **Status: AGREED, next sound feature — built BEFORE the §8 instrument engines.** Surfaced while
> ear-testing navkit's pluck (2026-06-03): the **filter envelope** and **pitch envelope** are
> sonically huge, and they are not two features — they are **one primitive** we don't have yet.
> Decided to ship this first; it then makes every §8 engine (and every raw wave) better for free,
> and keeps the engine macros (§8.1.1) clean.

### 11.1 The realization

We ship exactly one envelope today — the **amplitude ADSR** (`instrument()`, driving volume via
`sound_adsr_gated`). navkit's "filter env" and "pitch env" are nothing exotic: they are **second
and third envelope generators routed to a destination other than volume.** That's the textbook
subtractive layout — every Minimoog/MS-20 has a VCA envelope *and* a VCF envelope.

And we **already have the routing pattern**: `instrument_lfo(slot, which, dest, rate, depth)` is a
*cyclic* modulator pointed at a destination (`LFO_PITCH/DUTY/VOLUME/CUTOFF`). An envelope is its
**one-shot twin** — same routing idea, fires once per note instead of looping. So we don't invent a
concept; we add the missing half of one we have.

| Envelope | Routed to | navkit shape | amount units | what it buys |
|---|---|---|---|---|
| **amp** *(shipped)* | volume | ADSR | 0..1 | loudness contour |
| **filter** *(missing)* | cutoff | **AD** | Hz, **bipolar** | the pluck "pew / dwow" sweep |
| **pitch** *(missing)* | pitch | **D** (+curve) | semitones, **bipolar** | drum punch, attack snap, zaps |

navkit grounding: filter env is `cutoff = base + amount·level`, `level` a bare AD (attack→1, linear
decay→0; `synth.h:3516–3542,3603`). Pitch env is decay-only — pitch starts `amount` semitones off
and settles back over `decay`, with a linear/exponential `curve` (`synth.h:3023–3051`); the
exponential curve is what makes a kick *punch*.

### 11.2 The API — one routable call, mirroring `instrument_lfo`

```c
// the one-shot twin of instrument_lfo(). 2 env slots per instrument (which 0..1) so a
// filter env and a pitch env can run at once. amount units depend on dest (like LFO depth).
void instrument_env(int slot, int which, int dest, int attack_ms, int decay_ms, float amount);
//   ENV_CUTOFF   amount = Hz         (bipolar; + opens then closes — the pluck sweep)
//   ENV_PITCH    amount = semitones  (bipolar; + starts sharp and settles — punch/snap/zap)
//   ENV_DUTY     amount = 0..1       (PWM sweep — near-free; ship or defer)
// NB: there is deliberately NO ENV_VOLUME — that is the amp ADSR (instrument()). Don't duplicate it.
```

- **Count:** 1 function · 2 destinations to ship (`ENV_CUTOFF`, `ENV_PITCH`; `ENV_DUTY` optional) ·
  2 slots per instrument. ("2 envelopes — filter and pitch" = the two destinations; the slots are
  what let them coexist.)
- **Shape: AD now** (attack + decay, no sustain/release). AD covers every killer use (pluck filter,
  pitch blip, drum punch). Full ADSR only matters for routing a *held pad's* filter to stay open
  while the key is down — a later widening, not now.
- **Decay curve: exponential by default** — it punches harder than linear and is what sells the
  kick / the pluck attack. (navkit's pitch env exposes a curve knob; we hardcode exp to start.)
- **Held-note twin:** `note_env(handle, which, dest, attack_ms, decay_ms, amount)` mirrors
  `note_lfo` — gives modrack a per-voice env modulator and live control for free.

### 11.3 Why this is the right order (engines wait)

It **decouples two layers** that were getting tangled in the pluck-macro discussion:

- **Engine macros** (`harmonics`/`timbre`/`morph`, §8.1.1) → the *oscillator's own character*.
- **Modulation layer** (amp env + these mod-envelopes + LFOs + filter) → the standard subtractive
  wrapper that works on *any* engine or raw wave.

Concretely it **resolves the pluck `morph` fight**: with the filter sweep living in its own envelope
layer, `morph` no longer has to be "filter-env decay" — it can be **inharmonicity** (harp↔metallic),
the engine-character knob that also pre-builds the acoustic-piano engine. You get the pluck-bass
"pew" *and* the character knob, in different layers. Hence: build mod-envelopes first, then the
pluck engine sits cleanly on top.

### 11.4 Implementation shape (grounds in today's `sound.h`)

Reuses machinery that already exists — no new subsystem:
- `Instrument`/`Voice` gain a small `env[2]` (dest, a_samp, d_samp, amount, + per-voice
  stage/timer/level) — mirrors the existing `lfo_*[3]` arrays.
- New ctrl kinds for `instrument_env` (define) + `note_env` (live), next free after the held-note
  set (kinds ≥18). The §8.8 macro ctrl-kinds renumber to sit after these.
- The cutoff destination feeds the **same per-sample cutoff** the SVF + `LFO_CUTOFF` already drive
  (`sound.h:518,528,535`): `cutoff += env_level·amount`, summed alongside the LFO term — one extra
  addend, not a new code path. The pitch destination multiplies `freq` next to `pitch_mul`
  (`sound.h:525,542`).
- Per-note retrigger: stamp env stage/timer at note-on in `sound_setup_note` (`sound.h:262`).

**Demo carts (one each, the "tweak rig" pattern):** a `pitchenv` cart (drum punch + zap) and a
`filterenv` cart (pluck-bass "pew" + sweep), each with attack/decay/amount sliders and a keyboard —
mirroring `heldnotes`/`instruments` cart style. These double as the by-ear tuning rig.

---

## 12. Instrument gaps — found by building the radio family (2026-06-05)

**Empirical, not speculative:** this list is what was actually missing while
building and planning the ten radio stations (bossa → house) plus the next
batch's designs (gamelan, Italo, Steely Dan, the Doors). Ranked by how often
the wall was hit, and scored against §1's leverage rule.

1. **Voice choke groups.**
   **→ SHIPPED 2026-06-05** as `instrument_choke(slot_a, slot_b)` (SR_INSTR_CHOKE,
   kind 23): a new note on slot `a` instantly kills any sounding voice on slot `b`
   (with steal-declick). `tr808.c` and `house.c` both wire
   `instrument_choke(SL_HATC, SL_HATO)` — the documented infidelity is closed.
   The bitmask design (`choke_mask` on `Instrument`) lets one slot choke several
   others, covering future cymbal-choke and hi-hat-pedal scenarios.

2. **A second oscillator per voice** (detune pair, or 2-op FM). The big
   timbre unlock, felt four distinct ways.
   **→ PARTIAL 2026-06-07: `instrument_tune(slot, float semitones)` SHIPPED** — a
   slot-level detune (±24, fractions the point) read LIVE by every sounding voice,
   scheduled arp/seq hits included. Born from the SH-101's TUNE trimmer not
   reaching the arp. It's the cheap 80% of the *detune-pair* want: two slots, one
   tuned +0.06 = unison shimmer (still costs two voices — the polyphony-halving
   unison *flag* of 2b stays deferred); also gamelan ombak in one call. Wired all
   four places + soundcheck PASS; worked example = sh101.c `define_slots()`.
   **→ SPLIT 2026-06-05 — this gap conflated two architecturally opposite things:**
   - **2a — an FM *engine*** (`INSTR_FM`): the second oscillator is an *inaudible*
     phase-wiggler sealed inside one engine's sample function — you only ever hear
     the carrier. Self-contained behind one wave id, buffer-free, zero new API,
     zero shared plumbing. Covers the epiano/bell/brass/clang needs below.
     Design: §8.8.3. **Promoted to next engine.**
   - **2b — second-oscillator *infrastructure***: a second *audible* source on the
     generic voice path (Juno saw+square mix, the unison-detune flag) — touches
     `Voice`/mix-loop semantics for every wave and needs new API surface. **Stays
     deferred** until the Juno-60 cart defines the requirements (§14 gap A).
   The original four motivations, now tagged by which half they need:
   - **[2a] Electric piano doesn't exist.** The guide's recipe ("SINE + tremolo")
     is a placeholder, not a Rhodes. Italo disco, Steely Dan/AOR and any
     deeper citypop all *center* on epiano/DX keys — the planned batch runs
     straight into this wall.
   - **[2a] Bells and metal are out of reach.** FM's inharmonic partials are
     exactly gamelan's bronze (ombak beating *inside* one voice) and
     exotica's vibraphone.
     **→ MOSTLY CLOSED 2026-06-08** by `INSTR_MALLET` (struck bronze/vibes) +
     `INSTR_FM`'s 3.5 clang detent (inharmonic bell) — exotica/lowend/addis all
     play bronze today. **The one remaining sliver — the sustained GONG.**
     (Noted 2026-06-09 while speccing [`gamelan.md`](gamelan.md).) A big hanging
     gong (gong ageng/kempul) needs a *10–30 s shimmering metallic tail* — and no
     engine sustains inharmonic metal that long: MALLET and FM are both **struck**
     (self-decaying within the note), so a held gong fights its own decay envelope.
     The shippable **workaround** (good enough for the gamelan station, documented
     in its spec): `instrument(I_GONG, INSTR_MALLET, 1, 0, 7, 5000)` — full bell
     ratios, soft strike, max ring, **sustain 7 + a long ~5 s release** — plus
     `instrument_tune` shimmer for the ombak *inside* the gong and a little
     `instrument_echo()` (the dub-throw bus send) for pavilion air. The **true
     fix** is the deferred room/reverb layer (gap 5 below): a gong's defining
     shimmer is the bronze *plus* the pavilion acoustics, and a diffuse tail can't
     be a struck envelope. Until that ships, the honest fallback is a shorter,
     more *present* gong rather than one pretending to ring for 20 s.
   - **[2b] Pads cost double.** ambient.c burns 4 of `SOUND_VOICES 8` on one
     chord; house.c cut its string machine to 2 voices for budget. An
     instrument-level **unison-detune flag** would halve every pad's
     polyphony cost — arguably the cheapest 80% of this item.
   - **[2b] Juno-60 DCO mixer.** The Juno mixes SAW + SQUARE simultaneously per
     voice — that's a second oscillator with a different waveform, not an
     FM pair. The unison-detune flag alone doesn't cover it (detune = same
     wave, slightly offset pitch). A static approximation is possible via a
     drawn SCW (`INSTR_USER`) blending saw and pulse harmonics into one
     table; a live-mixable version needs the full second-oscillator path.
     The Juno is a concrete build target that makes both the unison-detune
     flag and the second-osc path more urgent (see §14).

3. **A plucked-string wave (Karplus-Strong).** Every guitar on the dial is a
   filtered TRI envelope: bossa's nylon, jangle's chorus-wobble, the planned
   Krieger drone guitar. One wave type covers guitar / harp / koto — and KS
   variants do metallic percussion too. Famously ~20 lines.
   **→ SHIPPED 2026-06-05** as `INSTR_PLUCK` + the §8.1.1 three-macro surface
   (six functions, ctrl kinds 21/22), showcased by the `pluck` cart. The
   jangle/bossa retrofit (the ear test against shipped stations) is the
   open follow-up.

4. **Finer dynamics.** `vol 0..7` is coarse: ghost notes jump audibly
   between 1 and 2, and satie.c — solo piano, where *touch is the whole
   arrangement* — strains hardest against 8 steps. Even 0..15 would let
   grooves breathe. (Mind the API-churn cost: every existing call site.)

5. **The contentious one: a tiny shared room.** The family is bone dry.
   dub.c proved echo can be *scheduled notes* — but diffuse tails can't be
   scheduled. satie/ambient/exotica would transform with one fixed mono
   plate + per-slot send level. Counter-argument: dryness IS the chip
   identity. A taste decision, not a clear gap — flagging, not advocating.

**SFX editor as percussion modeler — a partial bridge for gap 2/sample sounds.**
The SFX editor (`sfxed.c`, §5.6) exports per-step pitch + volume + filter-CUT
arrays as C code. That's more sculpting power than a single `instrument()` call —
you can draw the exact filter-close curve and volume shape of a hit (NOISE wave,
steep CUT lane, short vol decay → a sampled-hat-style burst, fired with `sfx(n)`).
A useful technique for any drum machine needing sounds more sculpted than a fixed
ADSR — though **still unused in anger**: its motivating case, the 909's ROM-sample
hihat, ended up solved *better* by `INSTR_FM`'s clang detent + a negative
`ENV_CUTOFF` when the cart actually shipped (§14). It stays on the shelf for the
next sound that genuinely needs a hand-drawn contour (tinyjam racks names the
synthwave gated-reverb snare).

**What was NOT missing**, for the record: the per-note modulation system is
complete in practice — house.c's entire filter ride was `note_cutoff` +
`note_res` with zero friction, and the env/LFO routing covered every recipe
in game-music.md so far. Also `wave_set`/`INSTR_USER0..3` went **unused by
all ten stations** — a drawn single-cycle wave could fake organ drawbars or
a nasal clav today; worth a reminder in the guide before adding anything.
(Done — the guide now has a "Drawn waves" recipe section. But don't read
that observation as "the SCW bank is the next move" — it isn't; the
investment call is laid out in §13.)

## 13. The lay of the land — SCWs vs. engines (decided 2026-06-05)

§8.4 (single-cycle waves) and §8.5–8.9 (modeled engines) keep getting
discussed in the same breath, and §12's closing note about `wave_set` going
unused makes it worse: it *reads* like the cheap, shipped, underused lever
should be picked up first. This section fixes the map so a fresh session
doesn't re-derive (or re-confuse) the decision.

**Two different levers for two different problems:**

| | `wave_set` / SCW bank | modeled engines (§8) |
|---|---|---|
| what it is | a static single-cycle table — a *snapshot* of a timbre | per-voice DSP — *behavior over time* |
| what it buys | fixed harmonic recipes: organ drawbars, nasal clav, brass blend | epiano pickup growl, FM/bell inharmonic beating, plucked-string decay |
| §12 gaps it solves | **none** (the gaps are all temporal/relational) | gaps 2 and 3 — the blocking ones |
| stations it unblocks | none — it *colors* stations that already work | the whole parked batch: Italo, Steely Dan/AOR, gamelan, the Doors, ethio-jazz |
| cost | an afternoon | real work — but pluck is pre-designed (§8.2 buffer, §8.5 ordering, §8.8 sketch, §11 decoupled mod-env) |

**The decision: engines first, pluck specifically.** Three separate design
passes (§8.5, §8.8, §11) already converge on KS pluck as the first engine,
and §12 added the empirical evidence: every guitar on the dial is a faked
filtered TRI. Pluck also validates against *shipped* carts (retrofit
jangle/bossa) before any new station needs it — the cart-ships-with-the-API
rule is satisfied from day one.

**The trap this section exists to prevent:** building the SCW bank first
*because it's easy* and counting it as progress on §12. It wouldn't be —
the parked stations would still be parked. Cheap-and-shippable is exactly
how an afternoon lever displaces the week that moves the project.

**Order of work:**

1. **KS pluck** (§8.5 phase 1) — retrofit jangle or bossa as the proof.
   **→ SHIPPED 2026-06-05**: `INSTR_PLUCK` (per-voice KS buffer, §8.2 made
   concrete) + the full §8.1.1 macro surface (`instrument_harmonics/timbre/
   morph` + `note_*` twins — landed as ctrl kinds 21/22; the sketch's 18/19
   were taken by §11's mod-envelopes by ship time) + the `pluck` showcase
   cart. Same-day follow-ups: a **fractional read tap** (the whole pitch
   system — LFO_PITCH warble, ENV_PITCH, `note_pitch`/`note_glide` — now
   bends the string live, and pitch is exact instead of length-quantized)
   and the **steal-declick** tail. jangle.c and bossa.c carry live A/B
   toggles (G); the listening verdict + macro taste-tuning are the open
   tail. Porting lessons for engine #2: §8.8.1.
2. **Choke groups** (§12 gap 1) — **→ SHIPPED 2026-06-05**. See §12 above.
2½. **Mallet engine** (§8.5 phase 2) — **→ SHIPPED 2026-06-05** by the §8.8.2
   playbook; `mallet` cart. Taste-tuning + lowend retrofit are the open tail.
3. **FM engine — §12 gap 2a only** (split 2026-06-05; design §8.8.3) —
   **→ SHIPPED 2026-06-05** as `INSTR_FM` + the `fm` cart (§8.5 phase 3 has the
   ship note). Gap **2b** (the audible second osc: Juno saw+square mix,
   unison-detune flag) stays deferred until the Juno-60 cart defines it; the
   detune flag remains the cheapest 80% of 2b and may ship on its own as pure
   polyphony relief.
4. **The SCW bank — opportunistically**, the first time a station wants
   organ or clav color (the Doors' Vox Continental is the likely trigger).
   Shape is decided, see below.
5. **Reverb** (§8.10 / §12 gap 5) — stays deferred, as both docs agree.

**The SCW bank's decided shape** (so the debate doesn't rerun): a
hand-written **`runtime/waves.h`** as single source of truth — `wave_gen(kind,
float out[64])` fills a buffer, convenience `wave_seed(slot, kind)` calls
`wave_set()`; carts get one `#include` + one call. **`waveed.c` includes it**
and drives its seed buttons from the same table, becoming the bank's
interactive auditioner — one definition, no drift. Keep it small and
researched: 6–8 named shapes beats 20 speculative ones. Explicitly
rejected: a JS source of truth + codegen (the `gen-tcc-symbols` pattern
doesn't apply — waves aren't baked into `.cart.png`, they're set at runtime
from C, so the JS side would have **no consumer**), and dual hand-written
JS+C files (sync discipline purchasing nothing).

## 14. Classic Roland machine build readiness (2026-06-05)

Assessed against the current API and the §12/§13 gap map. Three machines that
fit the existing style of the classic-machine family (cr-78, tr-808, tb-303):

### SH-101 (1982) — monophonic lead/bass synth

**SHIPPED 2026-06-05** — `sh101` cart, panel-faithful (grey fader panel:
TUNE | MODULATOR | VCO | SOURCE MIXER | VCF | VCA | ENV + sequencer/arpeggio
LED row + bender). The original assessment ("build it now, no engine work")
held, with four build lessons worth recording:

- **The SOURCE MIXER is voice-stacking.** Pulse + saw + sub + noise each get
  their own slot and `note_on`; one held note = 4 of 8 voices, faders ride
  live via `note_vol()`. This answers §12 gap 2 ("simultaneous waveform mix")
  **for monosynths only** — a poly like the Juno-60 can't afford 4 voices per
  key, so gap 2 proper (a real second-oscillator path) stays open.
- **Engine LFOs are sine-only; the rest is a software LFO.** The 101's
  tri/square/random(S&H)/noise modulator: TRI rides the engine LFO, the other
  three are computed per-frame in `update()` and pushed through the live
  setters (`note_pitch`/`note_duty`/`note_cutoff`). Fine for held notes at
  60fps; scheduled arp hits (`schedule_hit`) only get the engine path — a
  per-hit LFO-shape param would close that if it ever matters.
- **KYBD tracking = per-note `note_cutoff` scaling** (cutoff × 2^((midi−60)/12·k)
  right after `note_on`). Same limitation: `schedule_hit` carries no per-note
  cutoff, so arp steps don't track. Audible only at high KYBD + wide arps.
- **LFO RATE doubles as the seq/arp clock** (the real panel's LFO/CLK slider).
  Self-scheduled with a ~100 ms `schedule_hit` lookahead off `now()` — no bpm
  grid needed, and tempo follows the slider live.

Runtime side-effect of the build: **key claiming** — a cart that reads a key
via `key()/keyp()/keyr()` claims it from the pause hotkey (the two-manual
keyboard needs P as a note; ENTER still pauses). Plus two pause bugfixes:
`-DPAUSE_KEY` is now honored (was hardcoded `KEY_P`) and ENTER can actually
open the overlay (the menu used to consume the same press as Continue).

### TR-909 (1983) — hybrid drum machine

**SHIPPED 2026-06-05** — `tr909` cart, completing the ReBirth RB-338 rack
(303 + 808 + 909). The pre-build call ("kick/snare/toms now; hihat/cymbal need
a workaround") held for the analog half and was *beaten* on the sampled half:

| 909 section | How it landed |
|---|---|
| Kick (analog, ENV_PITCH drop) | ✅ as assessed — 808 kick retuned, + a click layer on the ATTACK knob |
| Snare / toms / clap (analog) | ✅ as assessed — 808 architectures, 909 component values |
| Hihat / cymbal (ROM samples in hardware) | ✅ **better than the predicted workaround**: `INSTR_FM` on the 3.5 inharmonic clang detent (harmonics 0.55, feedback cranked) through a highpass whose cutoff rises via a **negative `ENV_CUTOFF` amount** — the fast-closing sizzle of a sampled hat, synthesized. Closed hat chokes open via `instrument_choke` |
| Shuffle (909 had it; 808 didn't) | ✅ period-correct at last (even 16ths drag) |

The *predicted* hihat answer — SFX-editor-sculpted CUT-lane bursts — wasn't
needed; that technique's canonical write-up lives in §12 (still unused in anger).
Lesson banked in instrument-engines §8.8.3: inharmonicity, not ratio height, is
what reads as metal — the clang detent is a drum machine's sample shelf.

### Juno-60 (1982) — 6-voice polyphonic pad/chord synth

**Architecture works; two gaps block authenticity. Build after §12 gap 2 + §8.10.**

The Juno-60's voice architecture (polyphonic `note_on`, LP filter, LFO, ADSR)
maps fine. Two things stand between a Juno-60 cart and the real instrument:

**Gap A — DCO SAW+PULSE simultaneous mix (§12 gap 2b).**
The Juno mixes saw and square waveforms per voice for thickness — that's a
second oscillator with a *different* waveform, distinct from both FM and
unison-detune. ⚠️ Note (2026-06-05): the **FM engine (§12 gap 2a / §8.8.3) does
NOT close this gap** — its second oscillator is an inaudible phase modulator,
not a second audible source. Current workaround: draw a blended SCW
(`INSTR_USER`) with partial saw + partial pulse harmonics — static mix, can't
sweep the ratio, but costs nothing. The full dynamic mix requires the
second-oscillator path in §12 gap 2b. The unison-detune flag (cheapest 80% of
gap 2b) covers the *thickness* aspect but not the waveform blend.

**Gap B — BBD chorus (§8.10).** **✓ RESOLVED — `chorus()` IS this BBD chorus** (the
shared modulated-delay buffer; §8.10 shipped). **Showcase: `juno`** (juno-6 — the famous
OFF/I/II switch + the mono chord fanning out into a stereo wash). The Juno-60 is no longer
blocked; the rest of this note is the original (now-closed) planning rationale.
The Juno's chorus is *the* defining sound. Without it the cart is a dry generic
pad, not a Juno. The BBD chorus is a short pitch-modulated delay — the same
building block as the Leslie's doppler component (§8.3/§8.10). It falls out of
the effects-layer work rather than needing its own separate engine. Since §8.10
is deferred to phase 4 (after the first engine batch), the Juno-60 is blocked
until then.

**Build order implication:** the Juno-60 is the concrete hardware target that
makes the unison-detune flag, the second-oscillator path (§12 gap 2), and the
§8.10 effects layer all more urgent. If and when the Italo/gamelan/AOR batch
starts and §12 gap 2 opens, design the second-oscillator surface with the
Juno-60 in mind alongside the Rhodes and the bell engines.

## 15. Voice budget — what `SOUND_VOICES 8` actually costs (2026-06-05)

Analysis only — **no change made yet**; the radio family pressing the 8-voice
ceiling (octave doublings, two-voice brass spreads, ambient's 4-voice pad)
prompted the question "why 8, why not 16?". Findings from reading `sound.h`:

**Hardware is not the constraint.** Per voice per sample: one oscillator (or
modeled engine), an SVF, 3 LFOs, 2 mod-envelopes — order of ~100 flops. At
44.1 kHz that's ~35 MFLOPs for 8 voices, ~70 for 16; trivial on every target
(Apple Silicon native, Electron, the WASM web build). Memory is ~4.5 KB/voice
(dominated by `ks_buf[1024]`), so 16 voices ≈ 72 KB. Inactive voices skip the
mix loop — you only pay for what rings.

**The real coupling is loudness.** `sound.h` mixes each voice at a fixed
scale, then hard-clips the sum:

```c
float contrib = s * v->vol * env * trem * 0.2f;   // per-voice scale
...
if (mix >  1.0f) mix =  1.0f;                     // hard clip ±1
```

5 × 0.2 = 1.0 — six full-volume voices can already clip today (rare in
practice because envelopes/filters keep levels lower). The 0.2 constant is
where "how many simultaneous voices we expect" is really encoded:

- bump `SOUND_VOICES` to 16 and keep 0.2 → busy carts clip harshly;
- lower the scale to compensate → **every existing cart gets quieter and its
  mix balance shifts** (the real migration cost — not performance).

Everything else scales mechanically: handle slots, choke loops, and the
steal path all derive from `SOUND_VOICES`; stealing already degrades
gracefully (free → non-held → steal + declick tail).

**Recommendation (parked until measured):** if the radio carts genuinely need
headroom, go to 12–16 **paired with a soft-clip** (tanh-shaped) replacing the
hard clamp — graceful overload instead of digital clipping, without touching
the 0.2 scale or existing carts' loudness. Gate the change on: the soundcheck
tripwire, an ear test on the densest carts (radio stations, modrack), and a
measured before/after — which needs the WAV tooling below. The aesthetic
argument for keeping 8 ("fantasy consoles are scarce") is weak here: with 32
instrument slots, per-voice filters, and three modeled engines, this is
SNES/Amiga-class, not NES-class.

**MEASURED 2026-06-05** (via the §16 tooling, same day): deterministic 60 s
renders of the `house` cart (densest radio station, seed 7) at 8 vs 16 voices:

- **Starvation at 8 voices is real.** The renders diverge from t = 21 s (when
  the arrangement fills out); 25 of 60 seconds differ audibly, peaking at
  **+11% RMS restored** at 25 s — at 8 voices that part of the band was being
  silently voice-stolen. Peak rose only −5.49 → −4.82 dBFS.
- **No clipping at 16 with the 0.2 scale kept**: 0 clipped samples in the
  house render *and* in soundcheck's worst-case burst. Envelopes/filters keep
  real levels far below the theoretical 5-voice rail — the soft-clip is
  optional insurance, not a prerequisite.
- Soundcheck tripwire: PASS at both voice counts.

Conclusion: **16 voices is measured-safe and audibly restores starved parts.**
**FLIPPED TO 16 same day.** The flip surfaced a landmine the experiment alone
couldn't: note_on handles packed their slot in 3 hardcoded bits (`& 7` /
`>> 3` at 13 call sites) — handle slots 8–15 aliased onto 0–7, live setters
steering the wrong notes. Now `SOUND_HANDLE_BITS`/`SOUND_HANDLE_MASK` + a
`_Static_assert` that fails the build if `SOUND_VOICES` ever outgrows the
field.

The refactor's byte-diff was itself a measurement: the corrected 16-voice
house render differs from the pre-fix one **from t≈1s**, proving house holds
**more than 8 simultaneous note_on handles** — at 8 voices it was hitting
*two* ceilings at once: voice starvation (tails stolen) *and* handle-slot
exhaustion (`note_on` with all 8 slots live reuses slot 0, releasing the
oldest held note and disowning its handle — so live cutoff rides steered
nothing or the wrong voice). The corrected 8-vs-16 comparison is starker than
the first pass: **57 of 60 seconds differ**, divergence from t=0, and at 51 s
the 16-voice render is 26.5% *quieter* — correct note_off/note_cutoff control
means fewer wrongly-ringing notes, not just more restored tails. The filter
ride being the genre's whole form, house is the cart that suffered most.
Soundcheck tripwire: PASS.

## 16. WAV capture + analysis tooling — SHIPPED (2026-06-05)

> **Scope tap (added 2026-06-30):** the public `scope_read(float *dst, int n)` reads the
> latest `n` mono output samples from the SAME final-mix tap as the WAV capture, into a
> 2048-sample ring buffer in `sound.h`. Gated by `scope_ever` → zero-cost / byte-identical
> until a cart first calls it. For drawing a live oscilloscope; lock-free best-effort (audio
> thread writes, draw thread reads — a torn sample is invisible on a scope). NB: only fills
> with a live audio thread running; the headless `--wav` batch render leaves it flat (the tap
> arms after the batch). For a STILL waveform image, predict the shape in cart-land instead.
>
> **Stereo twin (added 2026-07-10):** `scope_read2(float *l, float *r, int n)` reads the same
> point in the chain but UNDOWNMIXED — the raw L/R pair, same-index-matched, in its own
> separately-gated rings (`scope2_ever`; arming the mono tap doesn't arm this one). Built for
> the vectorscope/goniometer view in the `wavecandy` cart (plot mid=L+R vertical vs side=L−R
> horizontal: mono = a vertical line, autopan tilts it, chorus opens it into a cloud). Same
> caveats as the mono tap.

Ear tests don't scale and don't diff. The §15 experiment (and mallet/fm macro
taste-tuning, and any future DSP change) wants: render a cart's audio to a
WAV, compute metrics, compare two runs. The sibling project already has the
working patterns — steal the shape, not the code:

- `~/Projects/navkit/soundsystem/tools/preset_audition.c` — headless preset
  renderer → WAV + audio metrics, `--ref` mode compares against a reference
  WAV and suggests parameter changes (directly relevant to mallet/fm tuning).
- `~/Projects/navkit/soundsystem/tools/golden_wav_gen.sh` — golden-WAV
  regression: render representative songs, checksum, re-render + verify after
  engine changes.
- `~/Projects/navkit/soundsystem/tools/audio_analyze.py` — musical analysis
  (tempo/key/chords via librosa), for transcription-level checks.

**Capture design (two modes, one tap point).** The whole engine mixes into a
single float in `sound_render()` — tap `mix` right before `out[i] = mix`:

1. **Live capture — `wav_request` trigger file**, same handshake as
   `screenshot_request`/`state_request` (write a file containing the output
   path + a duration; the engine records the next N seconds of callback
   output, writes a 44.1 kHz mono WAV, deletes the request). Works on any
   running cart, editor or `play.js`, no flags.
2. **Deterministic render — `--wav <file>` harness flag** (with `--det`):
   render the mixer in lockstep with frames (44100/60 samples per tick),
   no audio device needed — headless-safe, byte-reproducible. This is what
   makes **golden-WAV regression** possible: same cart + same script + same
   seed → identical WAV → checksum diff catches silent DSP regressions the
   soundcheck tripwire can't (it only catches dropped requests).

**Analysis tiers:**

- **Tier 1 (no deps, build first):** `tools/wav-analyze.js` — peak, RMS,
  crest factor, clipped-sample count, per-second envelope. Plain node reading
  the WAV; enough to run the §15 experiment (8 vs 16+softclip on a busy
  station: did clip count drop? did RMS hold?).
- **Tier 2 (borrow, don't port):** shell out to navkit's `audio_analyze.py`
  for tempo/key/chord checks, and crib `preset_audition.c`'s `--ref` metric
  comparison for engine taste-tuning against real-instrument reference WAVs.

Order of work: capture (1+2) → tier-1 analyzer → run §15 experiment → golden
WAVs as a by-product once `--wav` exists.

**Shipped same day, as proposed**: the `wav_request` trigger file, the `--wav`
flag (verified byte-reproducible: two identical runs → `cmp`-identical WAVs),
and `tools/wav-analyze.js` (single-file metrics + two-file compare with a
bytes-identical check — the golden-WAV primitive). The §15 experiment ran on
this tooling the same day. How-to lives in
[`guides/debug-harness.md`](../guides/debug-harness.md) → "WAV capture".
Tier 2 (navkit `audio_analyze.py` / `preset_audition --ref`) remains open.

## 17. Grit, darkness, weight — why everything still sounds clean (2026-06-05)

Modrack grew six modules in a day (MACRO/XPOSE/MIX/CMP/DIV/ADSR) and every patch
still comes out *polite*. That's not taste, it's an audit-able property of the
engine. Five findings, all verified against `sound.h`:

1. **Zero nonlinearity in the signal path.** No drive, no waveshaper, no
   clipper, no bitcrush — the only saturation anywhere is the FM modulator's
   self-feedback (`morph`). Grit *is* nonlinearity; a 303's squelch is the
   filter driven into saturation, not the filter. Ours is a clean SVF into a
   linear sum.
2. **Bone dry.** No delay, no reverb, no echo. "Dark" is mostly a space
   property — repeats that get darker each pass. Every patch plays into an
   anechoic void.
3. **One oscillator per voice, perfectly in tune.** No detune/unison/chorus.
   Two VOICEs fed the same pitch sum *louder*, not *fatter* — analog thickness
   is the beating between near-identical frequencies, and nothing in the
   system can be "a few cents off" (XPOSE deliberately moves whole octaves).
4. **The master section is a hard clip.** Voices sum into one float, then
   `if (mix > 1) mix = 1` (`sound_callback`, end of the mix loop). With
   SOUND_VOICES now 16, hot mixes will hit that wall harshly. There is no
   headroom stage, no glue.
5. **Cart-side: drums are bright ticks** (18ms noise + 90ms tri kick — no 909
   boom) **and the grid is dead straight** (no swing knob — though the engine
   already fires delayed requests sample-accurately *specifically* to keep
   "swing pushes" intact, so this is cart work waiting to happen).

### "So is this effect-bus stuff?" — the taxonomy (the actual design decision)

It looks like one feature ("add FX") but it's **four different layers**, and
the §4 question — *where does the parameter live?* — answers each differently.
Only one of the four is genuinely a bus.

| layer | what goes there | why there | API shape |
|---|---|---|---|
| **voice insert** | drive (tanh), bitcrush | stateless per-sample math, cheap per-voice; *musically per-part* (drive the bass, not the pad) | the §10 four-axes container grows: `instrument_drive(slot, x)` + live `note_drive(handle, x)`, slewed — exactly the filter/duty/macro pattern |
| **per-slot output gain** | LEVEL (VCA trim) ✓ (2026-07-10) | balance parts in a multi-voice mix (a groovebox kit) — *not an effect*, a plain per-voice gain at the VCA; the leg the per-slot mixer family (drive/echo/reverb/pan) was missing | `instrument_level(slot, gain)` — read LIVE at the per-voice sum like `instrument_tune` (fire-and-forget hits obey it, and it's automatable), `1.0` = unity/byte-identical bypass. Born from motionbox's MIX page (per-part LEVEL/PAN/DELAY/VERB) |
| **shared bus + per-slot send** | echo/delay · **reverb ✓ (2026-06-10)** | a delay/room line is KBs and wants musical coherence (one tempo'd echo / one room per song) — this is why hardware mixers have sends | `echo(time_ms, feedback, tone)` / `reverb(size, damping)` configure the buses; `instrument_echo`/`instrument_reverb`(slot, send) + the `note_*` twins set how much each part feeds each. Both returns + the soft-clip insert now live in the explicit **master FX section** (§8.10) |
| **oscillator param** | detune (cents) | not an effect at all — belongs beside duty/glide in `Voice` | `instrument_detune(slot, cents)` + `note_detune`; unison = two notes a few cents apart, suddenly meaningful |
| **master stage** | soft-clip replacing the hard clip; maybe master drive/crush later | one place, whole-mix glue; also the §15 16-voice headroom answer | internal first (tanh at the sum); a `master_*` API only if carts ask |

Chain order inside a voice, decided by the 303 test: **osc → SVF → drive →
VCA(env)** — drive *after* the filter so resonance screams into it, *before*
the envelope so quiet tails don't pump the saturation.

**Modrack mapping** (the reason this section exists): drive/crush/detune become
VOICE/MACRO knobs or CV inlets — per-module character, no new module. The echo
bus surfaces as a **DELAY module** that is secretly a *front-end*: its knobs
write the global bus params, its input-side "send" is per-source. One DELAY in
the rack is honest (there *is* one bus); a second one fights over it — fine for
v1, document it on the panel.

### Order of work

1. **Drive** — biggest lever, smallest diff (one tanh + four-place API wiring +
   the §16 tooling regression-tests it: golden-WAV a driven acid line, crest
   factor should *drop* while RMS rises).
   **✓ SHIPPED 2026-06-05** — `instrument_drive(slot, x)` / `note_drive(handle, x)`,
   tanh after the SVF, pre-gain `g = x²·24` (so `tanh(s·g)/tanh(g) → s` as x → 0:
   drive 0 is a true bypass, verified bytes-identical on soundcheck + bossa WAVs),
   normalized so full-scale stays put. The §16 prediction held exactly: driven
   acid line crest 13.6 → 6.9 dB, RMS −20.6 → −17.2 dBFS, zero clipping.
   tb303 grew the DRV knob (live `note_drive` on the ringing voice — RES + DRV
   is the squelch §17.1 said we were missing).
   **✓ MODES 2026-06-11** — `instrument_drive_mode(slot, mode)` / `note_drive_mode()` pick the
   waveshaper while `instrument_drive()` stays the amount: `DRIVE_SOFT` (the original tanh, the
   default — bit-identical), `DRIVE_HARD` (hard clip `clamp(s·g)/min(g,1)`), `DRIVE_FOLD` (sine
   wavefolder, dry/wet by amount, no divide), `DRIVE_ASYM` (asymmetric tube — softer negative
   half scaled by drive, the even-harmonic warmth). All four keep the bypass-at-0 + full-scale
   contract; the existing DC blocker cleans ASYM/FOLD. Showcase: `drivemodes.c`. Verified: WAV
   cycle through all four — 0 clipped, steady RMS, DC ~0.0006. (Rectify deliberately dropped: an
   even function can't bypass-at-zero, and a `/fold(g)`-normalized folder divides by zero at high
   drive.)
2. **Master soft-clip** — three lines, pays for §15's 16 voices immediately.
   **✓ SHIPPED 2026-06-05** — linear below a ±0.8 knee (quiet mixes bit-identical),
   tanh-shaped above, slope-continuous, asymptote ±1.0.
3. **Echo bus** — the real architecture step (audio-thread-owned buffer, send
   field in `Instrument`, bus params via the request ring like everything else).
   **✓ SHIPPED 2026-06-05** — `echo(time_ms, feedback, tone)` / `instrument_echo(slot, send)`
   / `note_echo(handle, x)`, exactly the §17-table shape. One 2s delay line
   (~345 KB, audio-thread-owned), dormant until the first echo call ever arrives —
   old carts verified bytes-identical (soundcheck + bossa golden WAVs). Three
   design wins beyond the sketch: (a) **tone is a one-pole LP *inside* the
   feedback loop**, so repeats genuinely darken each pass; (b) **a tanh inside
   the loop lets feedback go to 1.1** — past 1.0 it self-oscillates into a
   tape-style saturation plateau (verified: 10s runaway grows 0.018→0.042 RMS,
   peak −12 dBFS, zero clipping) instead of exploding; (c) **the read tap is
   fractional and slews toward its target with a clamped per-sample step**, so
   sweeping `echo()` time live pitch-bends the ringing tail like varying tape
   speed. Showcase: `spacecho.c` (RE-201 — the effect as the instrument).
   Adoption: dub.c layers the bus's darkening wash under its scheduled-note
   taps and rides the skank's send hot on throws (the §12 "diffuse tails"
   gap, closed); tb303/sh101 get subtle slapback sends.
4. **Detune** — small, after drive (driven unison is the payoff).
5. **Bitcrush** — on-brand dessert; decide insert vs master when it lands.
   **✓ SHIPPED 2026-06-11** — `crush(bits, rate, mix)` (master) **and** `instrument_crush(slot, …)`
   (per-instrument aux bus, the tape/wah pattern), so it's BOTH whole-mix lo-fi and per-part
   (crush the drums, leave the lead hi-fi). Bit-depth = floor to 2^bits levels; rate = sample-
   and-hold every Nth sample. Applied last in both the per-bus insert loop and the master chain,
   before the soft-clip; dormant until first call (mix 0 = off), so non-users stay byte-identical.
   Quantization is **round-to-nearest** (`floorf(x*levels + 0.5f)`), symmetric about zero.
   *(2026-06-14: was `floorf(x*levels)` = truncate-toward−∞, which added a ~−0.014 @ 5-bit negative
   DC bias AND held a decaying note's negative half at a constant −1-LSB level — so a crushed tail
   buzzed at fixed amplitude instead of fading, then snapped off when the voice gated. Round-to-nearest
   fixes both: sub-½-LSB samples → true 0, so the tail fades to silence; DC measured −0.0148 → −0.0002
   on the `bitcrush` render.)* Showcase: `bitcrush.c` (master vs per-instrument on a clean lead over a
   crushed bass). Verified: OFF/MASTER/BASS-ONLY renders all distinct + clean (0 clipped, no NaN).
6. **Cart-side, no engine change: swing knob on CLOCK** (`schedule_hit` already
   keeps the timing), **darker DRUM voices** (909 kick = longer sine + pitch
   env — the "Punch (VCA)" preset recipe, baked in).
   **✓ SHIPPED 2026-06-07** — both in modrack: CLOCK's `swg` knob (0–60%, delays
   every odd 8th-step at eval time; /2 and /4 fire on even steps so the on-beats
   stay straight, and swing 0 is bit-identical to the old clock), and DRUM slots
   26–28 (kick = 280ms sine + `ENV_PITCH` +30 st/55ms donk + a noise click layer;
   snare = band-passed noise over a tri body; hat = high-passed noise). Measured
   on the default patch: RMS −21.8 → −19.9 dBFS, crest 17.7 → 19.3 dB (more
   transient relative to body), zero clipping — kick peaks lean into the master
   soft-clip knee as designed. **Second customer shipped same day:**
   `drummachine.c` carries the same kit (slots 5–10, the punch-recipe kick +
   per-row hat/snare filters, clap = three `schedule_hit` bursts) and a swing
   knob on its 16-step transport (↑/↓, 0–60%) — RMS −24.0 → −21.4 dBFS.
7. **EQ** — the one tone tool that can BOOST (the SVF filters only cut, so until now no band
   could be lifted — the gap the distortion/EQ discussion flagged).
   **✓ SHIPPED 2026-06-12** — `eq(low_gain, mid_gain, high_gain)` (master) **and** `instrument_eq(slot, …)`
   (per-instrument aux bus, the tape/wah/bitcrush pattern). **3-band**, ported from navkit
   `processMasterEQ`, made **stereo, per-bus, and 3-band**: two one-pole crossovers (~80 Hz / ~6 kHz)
   split the signal into low / mid / top, then EACH band is scaled by its own dB→linear gain
   (`10^(dB/20)`). navkit left the mid at unity (a 2-band shelf); the split already computes the mid
   band, so the third knob was free — shipped 2-band first, then extended to 3 on the same-day
   "no middle?" feedback. Gains in dB, **±12**; at 0/0/0 dB the three bands sum back to the input
   exactly (gain 1.0 = identity), and it's dormant until the first `eq()` call → non-users
   byte-identical. Applied after tape, before bitcrush, in both the per-bus insert loop and the
   master chain (so it's pre-soft-clip). `SR_EQ`=55 / `SR_INSTR_EQ`=56, gains via the `*1000`
   control convention. The point-of-having-it: paired with `DRIVE_ASYM` (EQ around a clipper) it's
   a real **guitar-amp tone** — the `eq.c` showcase (a draggable LOW/MID/HIGH node grid) stacks
   exactly that on its AMP toggle. Verified per band: a 700 Hz saw in the mid band `eq(0,+12,0)`
   RMS −30.97 → −19.19 dBFS (**+11.8 dB** — nearly the full boost, the tone sits squarely in-band);
   a 55 Hz drone `eq(+12,0,0)` −30.95 → −21.76 (**+9.2 dB**, under +12 since it straddles the 80 Hz
   pivot), `eq(−12,0,0)` drops it to −33.66; per-instrument path clean (DC ~−0.00003) — 0 clipped,
   no NaN throughout.

8. **Tremolo** — the amp volume-LFO (Fender/Wurlitzer throb), the "electric piano" signature.
   **✓ SHIPPED 2026-06-12** — `tremolo(rate, depth, shape)` (master) **and** `instrument_tremolo(slot, …)`
   (per-instrument aux bus, the tape/wah/eq pattern). A **VERBATIM port of navkit's `processTremolo`**:
   one LFO ducks the bus level, `out = in·(1 − depth·(1 − mod))` where `mod` is the shape in 0..1, so
   it only attenuates below unity (never boosts — the amp character). `shape` = `TREM_SINE` (default) /
   `TREM_SQUARE` / `TREM_TRI`; `rate` 0.1–20 Hz, `depth` 0–1 (0 = bypass → non-users byte-identical).
   **Per-bus phase** is the whole point: a per-instrument tremolo shares ONE LFO phase across all the
   slot's voices, so the instrument throbs in unison like a real amp — the coherent wobble a per-voice
   `LFO_VOLUME` can't give (there each note runs its own phase = a shimmer, not a throb). This replaced
   the `epiano`'s old per-voice `LFO_VOLUME` tremolo — the "Wurli dry is good, tremolo still TODO" gap:
   the Wurli 200A's amp tremolo IS a bus effect. Applied **first** in both the per-bus insert loop and
   the master chain (before wah — navkit's order), `SR_TREMOLO`=59 / `SR_INSTR_TREMOLO`=60 (`*1000` for
   rate/depth, shape passed as a raw int). **A/B-verified against navkit's real DSP** (a harness driving
   navkit's genuine `setBusTremolo`+`processBusEffects`, NOT a formula copy — see
   [debug-harness.md](../guides/debug-harness.md) → "A/B a bus EFFECT"): both read **5.513 Hz / depth
   0.700** on a sine, and **8.018 Hz / 0.400 / square** at a second setting — rate and depth dead-on, on
   both the master and per-instrument paths. Showcase: `epiano` (Rhodes/Wurli throb; clav preset depth 0).

9. **Phaser** — the swept allpass NOTCHES: the 70s phased-Rhodes / Small Stone swirl (the one EP
   effect the library was missing — we had a flanger's swept *comb*, but not a phaser's *notches*).
   **✓ SHIPPED 2026-06-12** — `phaser(rate, depth, feedback, mix, stages)` (master) **and**
   `instrument_phaser(slot, …)` (per-instrument aux bus, the tape/wah/eq pattern). A **VERBATIM port
   of navkit's bus `processPhaser`**: N first-order allpass stages in series sharing one LFO-swept
   coefficient (`coeff = 0.5 + lfo·depth·0.4`, navkit's 0.1..0.9 range) with feedback from the last
   stage. navkit's is mono; we run it **per channel** (own allpass memory L/R) sharing one LFO phase,
   so a centered source matches navkit's mono exactly and a stereo source keeps its width. `stages`
   2..8 (**4 = the classic Phase-90**); `mix` 0 = bypass → non-users byte-identical, and **0.5 is the
   deepest** (an all-wet allpass is magnitude-flat — the notches live in the dry+wet sum). Applied
   after chorus in both the per-bus insert loop and the master chain (navkit's order). `SR_PHASER`=61
   / `SR_INSTR_PHASER`=62 (`*1000` for rate/depth/fb/mix, stages a raw int). **A/B-verified against
   navkit's REAL DSP** (the saved `tools/navkit-fx-render.c` harness driving navkit's genuine
   `setBusPhaser`+`processBusEffects`): **0.99999 sample-level correlation** (`tools/wav-correlate.js`)
   at two settings (4-stage/fb0.3 and 6-stage/fb0.6/depth1.0). Showcase: `epiano` (the `P` toggle; the
   `suitcase` preset boots it — the classic phased suitcase Rhodes).

10. **Insert ORDER** — not a new effect, but the routing that makes the chain above a *chain*:
    the order inserts run in was hardcoded source order, so "bitcrush before vs after eq" (audibly
    different) wasn't reachable from a cart.
    **✓ SHIPPED 2026-06-12** — `fx_order(bus, kinds, n)` + `FX_*` kind constants. Each bus walks a
    per-bus visit list `insert_order[bus][]` instead of the fixed `if (x_used[b]) x_process(b,…)`
    ladder; `fx_order(0, …)` reorders the master chain, `fx_order(busN, …)` an instrument's aux bus.
    Default order = the old ladder (`trem→wah→cho→phaser→flg→tape→eq→crush`, seeded in `sound_init`),
    and each step still gates on its `_used[b]` flag, so a cart that never calls `fx_order` is
    **byte-identical** (soundcheck-verified, no `[sound] WARNING`; it's post-mix insert routing, so
    tuning is untouched). Soft-clip stays pinned last (a safety limiter, not a reorderable pedal).
    `SR_FX_ORDER`=63 packs the kinds 3-bits-each into one int. Sends (echo/reverb) are parallel, NOT
    in the chain — reverb's chain position only becomes real with the multi-reverb-tank step
    (effects-bus increment C). Showcase: the **pedalboard** cart, rebuilt as a drag-and-drop chain
    BUILDER (palette → drag to add / reorder / remove, with a live drop caret). This is increment A
    of [`effects-bus-architecture.md`](effects-bus-architecture.md).

11. **Leslie** — the rotary-speaker cabinet (the organ's voice), and the decision-0015 refusal
    reversed. **✓ SHIPPED 2026-06-12** — `leslie(speed, drive, balance, doppler, mix)` (master) **and**
    `instrument_leslie(slot, …)` (per-instrument aux bus). A **VERBATIM port of navkit's processLeslie**
    (Leslie 122 model): a 1-pole 800 Hz crossover splits DRUM (bass, gentle sine AM) from HORN
    (treble, shaped AM + a physical **Doppler** via a Hermite-tapped modulated delay line — reusing
    `moddel_hermite`), with two rotors spinning at INDEPENDENT rates and asymmetric spin-up/down
    inertia (horn light/fast, drum heavy/slow) — so `LESLIE_STOP/SLOW/FAST` ramps the chorale↔tremolo
    swell on its own. navkit is mono; we run it per channel (own crossover + Doppler buffer L/R)
    sharing one rotor → centered source matches navkit's mono exactly. **PINNED last** in both the
    aux-bus loop and the master chain (the cabinet output stage, like the soft-clip — NOT a reorderable
    `FX_*` kind, so it never touches the `fx_order` 3-bit packing); `leslie_used[]`-gated → dormant
    carts byte-identical (soundcheck silent). `SR_LESLIE`=64 / `SR_INSTR_LESLIE`=65. **A/B vs navkit's
    real `processLeslie`: 0.99999** sample-level correlation (FAST & SLOW); a driven setting reads
    0.992 only because *our* master soft-clip catches the hotter signal navkit's bare harness lacks —
    not a Leslie discrepancy. **The 0015 angle:** this was the ADR's flagship refusal ("Leslie = 3 LFOs
    + drive, a recipe; no `instrument_leslie`, ever") — building the real one proved the recipe can't
    band-split, can't delay-line-Doppler, can't run two inertial rotors, so it clears 0015's own gate.
    Reversal logged in [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md). Showcase:
    `organ` (the `L` footswitch — replaced its old per-voice tremolo+doppler recipe with the real bus rotary).

12. **Multi-reverb / reverb-as-bus** — effects-bus increment C (engine). **✓ SHIPPED 2026-06-12** —
    the single shared reverberator became a **`ReverbTank` pool** (`SOUND_REVERB_TANKS = 3`): the loose
    `rvb_*` globals fold into a struct, `reverb_process` takes a tank pointer. Tank 0 = the legacy
    `reverb()` master parallel send (routing untouched → **bytes-identical**, verified by a before/after
    `cathedral --wav --det` byte-match); tanks 1–2 = independent **reverb send-buses**. `reverb_bus(tank,
    size, damp)` lazily claims an aux bus and sets its chain to `[FX_REVERB]` — a new 9th `FX_*` insert
    kind that **wet-replaces** the bus it runs on (so any inserts after it chew the wet tail), and a
    safe no-op on any bus whose `bus_tank[b] < 0` (master / a pedalboard bus — never zeroes a real mix).
    `instrument_reverb_bus(slot, tank, mix)` routes a slot's send into a tank, resolved tank→bus at
    note-on. The `fx_order` packing **widened 3→4 bits** (across `b` + `e0`) to fit the 9th kind — the
    gotcha increment A flagged. `SR_REVERB_BUS`=66 / `SR_INSTR_REVERB_BUS`=67; bus-pool exhaustion bumps
    `sound_dropped` (no silent caps). Two instruments now ring in two different spaces at once (drums in a
    room + keys in a plate) — the §4 increment-B win, subsumed. Showcase: the **reverb spaces** cart.
    **Effects AFTER the reverb also shipped** (the same-day fast-follow): `reverb_bus_fx(tank, fx, a, b, c)`
    is tank-keyed (resolves tank→bus on the audio thread, so a cart never sees the auto-allocated bus
    index), configures the insert (crush/eq/tape/chorus) on the reverb-bus, and appends it after FX_REVERB
    so it chews the wet tail. reverb→crush = the **reverb spaces** CRUSH toggle. `SR_REVERB_BUS_FX`=68.
    This is increment C of [`effects-bus-architecture.md`](effects-bus-architecture.md), now fully exposed.
    **In-line reverb (`reverb_insert`)** completed the picture: the wet-replace insert gained a dry/wet
    **mix** (`out = dry·(1−mix) + wet·mix`; `mix=1` = the old wet-replace, so reverb-buses are byte-identical),
    and `reverb_insert(size, damp, mix)` puts a mix-reverb on the master bus (tank 1) as a real reorderable
    `FX_REVERB` pedal. The **pedalboard** cart switched its reverb from a parallel send to this, so its chain
    position is finally audible — closing the increment-A "reverb position is cosmetic" caveat. The reverb
    doctrine ladder: **send** (`reverb()`, parallel) → **in-line insert** (`reverb_insert`, mix) → **dedicated
    space** (`reverb_bus`, wet-replace + downstream `reverb_bus_fx`).

13. **Formant (vowel) filter** — the talkbox-family vowel colour, on a bus. **✓ SHIPPED 2026-06-12** —
    `formant(vowel, q, mix)` (master) **and** `instrument_formant(slot, …)` (per-instrument aux bus). Four
    parallel TPT bandpasses parked at the human formant frequencies (F1–F4), summed by the vowel's
    relative amps, so any source takes on an "ooh/aah/eee" vocal colour; `vowel` 0–1 sweeps OO→OH→AH→EH→EE,
    `q` narrows the peaks, `mix` 0–1 (0 = off). **Reuses navkit's measured vowel table** (`vox_vowel_f/a/bw`,
    already in `sound.h` for `INSTR_VOICE`) — so it was routing + a control surface, not a new DSP port. A
    per-bus insert: `FX_FORMANT`=9, a reorderable pedal in every bus's default chain, `fmt_used[]`-gated →
    dormant carts byte-identical (soundcheck silent). `SR_FORMANT`=70 / `SR_INSTR_FORMANT`=71. Showcase: the
    **vowel** cart (a saw chord that talks). **It's the BUS half of the vowel story** — color any sound;
    `INSTR_VOICE` is the instrument half (a synth that sings, with polyphonic per-note articulation a bus
    can't do). **NOT a vocoder** — that's carrier×modulator (two inputs), waiting on the sidechain path.
    The 0015 angle: a formant filter is "the SVF reused four times" (like wah was its 4th use), so it clears
    the gate as a filter-reuse, not a new primitive shape — see the 0015 correction.

14. **Sidechain & bus compression** — summed-signal DYNAMICS (the pump + the glue). **✓ SHIPPED 2026-06-12**
    (effects-bus Increment D). `sidechain(victim_bus, key, amount, atk, rel)` ducks a victim bus on a TRIGGER;
    `sidechain_key(slot, key, send)` routes a slot (the kick) into a trigger key; `glue(victim_bus, amount,
    atk, rel)` is the same envelope→gain stage self-keyed (reads the bus's own level), a trigger-less bus
    compressor. The new mechanism is a **second input path** — a per-sample `sc_key[]` accumulator (same shape
    as `reverb_in`) the trigger slot feeds, read by `sc_apply` (one-pole atk/rel follower → `1 − amount·env`
    gain), applied per-bus + at the master like leslie. **NOT an `fx_order` insert** (a gain stage, no `FX_*`).
    One `SideChain` per victim bus (sidechain OR glue, not both). `SR_SIDECHAIN`=72 / `_KEY`=73 / `SR_GLUE`=74;
    amount 0 = dormant → byte-identical. **The `sc_key` path is exactly what the vocoder needs** (the "waiting
    on the sidechain path" the formant note above referenced — now built, ready to reuse). Showcase: the
    **groovebox** (PUMP kick-keyed + GLUE self-keyed, sharing the master comp). Detail: effects-bus-architecture
    Increment D.

15. **Resonant filter (the DJ filter)** — a sweepable master filter, the build-up/breakdown gesture.
    **✓ SHIPPED 2026-06-12.** `filter(mode, cutoff_hz, resonance)` — a TPT state-variable filter (the SAME
    core as wah/formant, just a plain swept filter in a selectable mode `FILTER_LOW`/`HIGH`/`BAND`/`NOTCH`),
    per-channel (preserves stereo). A reorderable insert (`FX_FILTER`=10, in every bus's default chain,
    `filt_used`-gated → dormant carts byte-identical). `SR_FILTER`=75; `filter()` configures the master.
    Cheap to re-call every frame, so the cart RIDES the cutoff live (the house "filter ride"). 0015 angle:
    filter-reuse (like wah was the SVF's 4th use), not a new primitive shape. Replaces the per-voice fake
    `house` carries. Verified: compile-gate + tripwire clean, byte-identical when off, LP-200-on-noise drops
    RMS −20→−40 dBFS. **Showcase shipped: `djfilter`** (the bipolar one-knob mixer filter — XONE:92/DJM
    homage — over a self-playing house loop, with a live response curve + a breakdown→riser→DROP BUILD;
    verified open-vs-closed: closed-LP peak −0.06→−5.58 dBFS, crest 11.4→7.2 dB). House-radio retrofit still pending.

16. **Auto-pan (stereo)** — the tremolo LFO applied **antiphase** to L/R: the stereo sweep (Rhodes
    Suitcase vibrato, the auto-pan stompbox). **✓ SHIPPED 2026-06-14.** `autopan(rate, depth, shape)`
    (master) **and** `instrument_autopan(slot, …)` (per-instrument aux bus). gL = the plain tremolo gain
    `1 − depth·(1 − mod)`, gR = its complement `1 − depth·mod` → the level shifts L on the LFO peak, R on
    the trough; reuses the `TREM_SHAPE_*` shapes. **Its OWN insert (`FX_PAN`=11), NOT a mode of tremolo** —
    own LFO state, so it stacks with `tremolo` on one bus (a throb AND a stereo drift at once) and is a
    distinct reorderable pedal. The design call **reverses** effects-bus-architecture §0's earlier "make
    it a mode of tremolo": a shared-state mode can't run alongside tremolo, and a "separate function /
    shared state" API would falsely *look* combinable — a distinct insert is the honest, composable form.
    `SR_AUTOPAN`=76 / `SR_INSTR_AUTOPAN`=77; `pan_used`-gated → dormant carts byte-identical (`N_INSERTS`
    grew to FX_PAN+1, the 4-bit `fx_order` packing already had room). 0015 angle: it cleared the gate as a
    real bus effect (passed the pedalboard test) the same way tremolo did; the only question was mode-vs-
    own-kind, decided own-kind by the can't-have-both cost. Verified: a centered mono source reads
    correlation +1.0; with auto-pan, 0.33 / −1.78 dB mono-fold = real width, and a 0.5 Hz sweep swings the
    balance ±0.56 dB symmetrically. **Showcases: `epiano`** (the RHODES VIBE pedal — replaced its per-voice
    `LFO_PAN` stand-in) **and `pedalboard`** (the AUTOPAN pedal, beside TREMOLO in the palette).

17. **Ring modulator** — multiply the bus by a sine CARRIER → inharmonic sum/difference sidebands (the
    metallic/robot/bell clang). **✓ SHIPPED 2026-06-14.** `ringmod(freq_hz, mix)` (master) **and**
    `instrument_ringmod(slot, …)` (per-instrument aux bus). `out = in·((1−mix)+mix·sin(2π·f·t))`. A real
    new bus effect, `FX_RINGMOD`=12 (a reorderable pedal in every bus's default chain), `rm_used`-gated
    → dormant carts byte-identical (`N_INSERTS` grew to FX_RINGMOD+1; 4-bit `fx_order` packing still has
    room — 12/16). `SR_RINGMOD`=78 / `SR_INSTR_RINGMOD`=79, full 4-place wiring + tcc + `fxicons.h` (the
    ⊗ multiply glyph). **0015 angle — a genuinely NEW primitive, not a recipe:** the carrier is BIPOLAR
    (−1..1), so it generates frequencies the input didn't contain (sum/difference) — something no
    combination of the existing roster (drive/filter/the unipolar tremolo LFO) can produce. It cleared
    the gate the way leslie/sidechain did: it passed the pedalboard test AND proved it can't be a recipe.
    This was the last unbuilt entry on the [`sound-next-steps.md`](sound-next-steps.md) build-list. Verified:
    soundcheck compile-gate `ok` + 900-frame tripwire silent + tune-check exit 0 (the carrier is post-mix,
    pitch of an un-modulated voice untouched); a 440 Hz sine renders dry at conf 1.0, and wet (mix 1,
    carrier 150 Hz) the 440 fundamental vanishes — pitch conf collapses to 0.4 as the 290/590 Hz sidebands
    appear, RMS −20→−23 dB (the sin×sin power halving), 0 clipped. **Showcase: `pedalboard`** (the RINGMOD
    pedal — FRQ 20..3000 Hz exp + MIX). A dedicated robot-voice/bells cart is the optional fast-follow.

18. **Delay INSERT (`echo_insert`)** — the parallel echo SEND made into an in-line, reorderable DELAY
    pedal. **✓ SHIPPED 2026-06-14.** `echo_insert(time_ms, feedback, tone, mix)` — the same tape-delay
    DSP as the `echo()` send (fractional read tap + tape-speed time slew + feedback-through-`tanh` +
    one-pole tone LP) but on its OWN buffer, placed IN the master `fx_order` chain as `FX_ECHO`=13, so
    its chain position is audible (delay→drive distorts the repeats; drive→delay = clean echoes of a
    dirty signal). The same send-vs-insert split `reverb_insert` made for reverb — `echo()` (send) returns
    clean to master (position cosmetic); `echo_insert()` (insert) is reorderable. **Master-only** (one
    buffer, gated on `b==0` in `apply_insert`), wet ADDS over full dry at `mix` (a delay pedal's blend, not
    a crossfade); NOT in the default chain (the cart places `FX_ECHO` via `fx_order(0,…)`, like FX_REVERB).
    `mix 0` → `echo_ins_used` false → dormant/byte-identical. `SR_ECHO_INSERT`=80, full 4-place wiring +
    tcc + `fxicons.h` (hit + fading-repeat-dots). Cost: one extra `SOUND_ECHO_MAX` (2s) buffer ≈ 352 KB
    static `.bss` (0 download; doubles echo memory) — the price of a real in-line delay vs the cosmetic
    send. Verified: soundcheck compile-gate `ok` + 900-frame tripwire silent + tune-check exit 0; a 120 ms
    blip leaves a decaying echo train (RMS 0.007 in the 1 s window after the note) where dry is 0.000.
    **Showcase: `pedalboard`** (the DELAY pedal — TIM 20..1500 ms / FB / TON / MIX). With it the board now
    carries every rostered `FX_*` insert.
19. **Granular delay (`grains`)** — navkit's `processGranularDelay` (`engines/granular_delay.h`) ported
    VERBATIM. **✓ SHIPPED 2026-06-14.** `grains(grain_ms, density, position, scatter, feedback, mix)` +
    `instrument_grains(slot,…)` + `grains_freeze(on)`/`instrument_grains_freeze`. Captures the live signal
    into a ring buffer; on a `density` schedule spawns Hanning-windowed grains reading scattered slices of
    the recent past (seeded-LCG scatter), summed + overlap-normalized; FREEZE stops the write so the buffer
    loops forever. Reads through the existing `moddel_hermite` — which already IS navkit's `hermiteInterpolate`
    (ported for chorus/flanger/tape), so no second copy. `FX_GRAINS`=14, auto-placed in the bus chain on
    first call (like FX_ECHO but auto-added). `SR_GRAINS`=81/`SR_INSTR_GRAINS`=82/`SR_GRAINS_FREEZE`=83/
    `SR_INSTR_GRAINS_FREEZE`=84; the instrument variant PACKs feedback+mix into one int (`e2 = fb*100·*1001 +
    mix*1000`) to fit the 6-int request budget. Big buffer (3 s, NOT navkit's 5 s — half the RAM, plenty of
    lookback) → a 2-tank POOL mapped per-bus on demand (mirrors the reverb tanks), NOT one buffer per bus:
    cost ≈ 2 × 3 s = ~1 MB static `.bss` (master + one instrument bus at once; pool-exhaust logs to
    `grain_overflow`). `mix 0` → dormant/byte-identical. Full 4-place wiring + tcc. Verified: soundcheck
    compile-gate `ok` + 900-frame tripwire silent; A/B vs navkit's genuine `processGranularDelay`
    (`tools/navkit-fx-render.c grains` + `grainstest.c`) — crest 7.29 (navkit) / 7.59 (ours) dB, the
    level-independent texture fingerprint matching despite the intentional buffer-size divergence (stochastic
    effect → character match, not sample-identity). **Showcases: `grains`** (the freeze/cloud toy) +
    the **`pedalboard`** GRAINS pedal (`FX_GRAINS` insert: SIZE/DENS/MIX + a discrete FRZ freeze knob;
    position/scatter/feedback fixed to a shimmer-cloud voicing since the pedal has only 4 knobs).
    **Addendum 2026-06-14 — pitched + reversed grains (building-blocks Block A).** `grains_pitch(semitones,
    spread, reverse)` + `instrument_grains_pitch` (`SR_GRAINS_PITCH`=91/92): per-grain `posInc =
    2^(semitones/12)` set at spawn (the Hanning window still lasts `grainSamples` *output* samples, so it's
    a true transpose, not a speed change), plus a random per-grain detune (`spread`) and a `reverse` flag
    (negative `posInc`, read from the grain's far end; the read loop gained a `<0` wrap). Turns granular
    delay into a shimmer/glitch machine. Verified exact: a +12 cloud on a 593 Hz source measured 1186.6 Hz
    (= 2×, octave perfect; the source's own +17.5¢ shows identically at both octaves). The cheapest item on
    the boutique-pedals roadmap — headroom on an engine we already owned, no new `FX_*` slot.

20. **Mix-bus saturation (`drive_insert`)** — the per-voice `instrument_drive` (post-filter, §17)
    given a whole-BUS sibling: `drive_insert(amount, mode, mix)` drives the SUMMED master mix as a
    reorderable insert, so the drums + the modeled MACRO engines grit up too (per-voice drive only
    touches the one voice it sits inside). The trigger was **`modrack` asking** — exactly the bar §17
    set ("a `master_*` API only if carts ask"). Stateless waveshaping (no buffer), so it's the cheapest
    insert: `drive_shape()` is a **verbatim copy** of the per-voice `DRIVE_*` switch (HARD/FOLD/ASYM/SOFT)
    so the bus version A/Bs against per-voice character, + a one-pole DC blocker on the wet path (ASYM is
    one-sided, like the voice path). `FX_DRIVE`=15 — the **last** kind that fits `fx_order`'s 4-bit-per-slot
    packing (a 16th `FX_*` needs a packing change). `SR_DRIVE_INSERT`=89; master-only, NOT in the default
    chain (the cart places `FX_DRIVE` via `fx_order(0,…)`, like FX_ECHO/REVERB). `mix 0` (or `amount 0`) →
    dormant/byte-identical.
    **TRAP (cost an hour, motionbox 2026-07-09): `drive_insert()` does NOT self-place `FX_DRIVE` in the
    chain — but its cousins `FX_GRAINS`/`FX_GATE`/`FX_SHALLOW` DO** (their `SR_*` handlers auto-append to
    `insert_order[0]`; `SR_DRIVE_INSERT` only calls `fx_set_drive`). So `drive_insert()` alone SETS the
    saturator's params but it's never visited → the knob is configured yet **silent**, no warning. Fix is
    one line in `init()`: `int c[]={FX_DRIVE}; fx_order(0,c,1);`. If a future refactor unifies this, make
    drive self-place like the others and delete this trap. Full 4-place wiring + tcc. Verified: soundcheck compile-gate `ok` + 900-frame
    tripwire silent; a clean→hard-drive switch mid-render jumps RMS 0.18→0.89 (the wall-of-fuzz fills the
    waveform). **Showcase: `modrack`'s SAT module** (whole-mix saturator; + the "Sat bus" preset that grits
    a whole beat). Per-voice drive stays the separate tool for the 303/acid post-filter scream.
21. **Modulation kit + Univibe (building-blocks Block B).** **✓ SHIPPED 2026-06-15.** The boutique-pedals
    roadmap's Primitive 1: a small set of reusable, deterministically-seeded modulation sources in
    `sound.h` — `mod_randwalk` (filtered random walk), `mod_sh` (sample-&-hold), `mod_optical` (the
    asymmetric "incandescent bulb" LFO shape: slow bright, fast dim). `static inline` so unused sources
    don't warn (mod_randwalk/mod_sh await Shallow Water / the VHS dropout). First consumer: **`univibe`**
    (`SR_UNIVIBE`=93/94) — the SAME `phaser` allpass chain + a `phaser_optical[]` flag that swaps the
    sine LFO for `mod_optical`, classic 4-stage, no feedback. Rides the existing `FX_PHASER` insert
    (shares it with `phaser()` — don't run both on one bus), so zero chain wiring. `fx_set_phaser` resets
    the flag false → existing phaser byte-identical. Verified: soundcheck `ok` + 900-frame tripwire
    silent; A/B sine-probe through `univibe` vs `phaser` at matched rate/depth differs (metrics + sweep
    depth), confirming the optical LFO is doing distinct work. **Showcase: `univibe`** (a `P` A/B toggle +
    a throbbing lamp + live LFO-curve that draws the asymmetry). The kit is the multiplier: Shallow Water,
    the Generation Loss dropout, and germanium bias-drift all ride these same three sources next.

22. **Dropout — the VHS "Failure" knob (`dropout`).** **✓ SHIPPED 2026-06-15.** Boutique-pedals
    roadmap, 2nd consumer of the modulation kit (`mod_sh`). `dropout(amount, depth)` (`SR_DROPOUT`=95):
    a sample-&-hold clock rolls the dice each step (`P(catch) = amount`, detected by the held value
    changing — rides `mod_sh` without needing an edge signal), and a catch kicks `drop_env=1` which
    decays fast (~25 ms) → a momentary stumble that **ducks level + rolls off highs** (lerp toward a
    one-pole-LP copy). A **master-stage** effect (called at the sum before the soft-clip, like
    leslie/sidechain) — NOT an `FX_*` insert (packing full at 15; a whole-mix tape-failure belongs at
    the master). `amount 0` → not called → byte-identical; seeded LCG → `--det` reproducible.
    **Gotcha caught in verify:** the first trigger (rising-edge across a threshold) fired *rarely* at
    high amount (the S&H sat above a low threshold, so few crossings) — backwards. Fixed to per-step
    dice; A/B then showed RMS 0.101→0.087 on a sine at `dropout(0.9,0.95)` (energy actually removed),
    where the buggy version barely moved it. Verified: soundcheck `ok` + 900-frame tripwire + `--det`
    byte-identical. **Showcase: `genloss`** (crush→tape→dropout; VHS transport tears its tracking on a
    catch). Pitch-dip on a catch deferred to the bus pitch-shifter (Primitive 2).

23. **`fx_order` packing widened (16 → 32 insert kinds).** **✓ 2026-06-15.** `FX_DRIVE=15` had filled
    the old 4-bit-per-slot `SR_FX_ORDER` packing, and the FX_INST work spent the spare request ints.
    Repacked each chain slot as one BYTE — **5 bits kind | 3 bits instance** — 4 slots per int across
    the same 4 payload ints (b/e0/e1/e2): **kinds 0..31** (16 new), **8 instances/kind**, **16 chain
    slots** (`FX_ORDER_SLOTS`). Pure repack of `fx_order()` + the `SR_FX_ORDER` handler + the `FX_INST`
    macro (`inst << 5`); no `SoundReq` growth. Verified TRANSPARENT — `pedalboard`/`groovebox`/`epiano`
    render byte-identical (kinds 0..15 + existing instances unchanged) — and an `FX_INST(FX_DRIVE,1)`
    sine A/B confirms instances still route (driven crest 0.65 dB vs clean 3.01 dB). Unblocks new
    reorderable inserts (Shallow Water, noise gate, …) that the full table had blocked.

24. **Shallow Water (`shallow`) — first new insert past the widened ceiling.** **✓ SHIPPED 2026-06-15.**
    Boutique-pedals roadmap; 3rd consumer of the modulation kit (`mod_randwalk`). `shallow(rate, depth,
    mix)` + `instrument_shallow` (`SR_SHALLOW`=96/97), `FX_SHALLOW`=16 — the first kind minted past the
    16→32 packing widen (#23). A filtered-random ("K-field") short delay (`mod_randwalk` drifts the tap
    via the shared `moddel_hermite` read) for the warped-water warble, then a Buchla **Low Pass Gate**:
    an envelope follower (`|wet|`×5 sensitivity) opens cutoff + level together — steady signal stays
    open, quiet/decaying tails go dark+soft and bloom back. Per-bus insert, auto-added to the chain on
    first call (like grains). **Calibration gotcha caught in verify:** the LPG first followed raw
    `|wet|`, so normal levels (~0.1–0.2) never opened it → everything came out dark + 13 dB quiet; added
    the ×5 sensitivity + a 0.5 gain floor → a sustained sine now passes near-unity (RMS 0.107 vs dry
    0.121) while a pluck tail correctly blooms closed. Verified: soundcheck `ok` + 900-frame tripwire +
    `--det` byte-identical; `pedalboard`/`groovebox`/`epiano` byte-identical (new kind doesn't perturb
    default chains). **Showcase: `pedalboard`** (the SHALLOW pedal — a real reorderable insert, which the
    full 16-kind table had blocked until the widen).

25. **Amp noise — the optional rig-noise floor (`amp_noise`).** **✓ SHIPPED 2026-06-15.** Boutique-pedals
    roadmap (Block C, part 1). `amp_noise(hiss, hum, mains_hz)` (`SR_AMP_NOISE`=98): a constant
    master-output floor — broadband hiss (two decorrelated white→one-pole-LP draws for stereo width) +
    a centered 50/60 Hz mains hum (fundamental + 2nd harmonic). Added AFTER the soft-clip so it's a true
    constant floor (never ducked/clipped by the mix), present even in dead silence. **Opt-in by design:**
    `hiss 0 && hum 0` → not called → byte-identical (verified: a no-note render is RMS 0 exactly when
    off, RMS 0.0092 when on; deterministic re-render). Master-only, seeded LCG. NOT an `FX_*` insert (a
    whole-rig floor belongs at the master, like dropout). The user's framing drove the design — "might be
    great, might not always be desirable" → a choice, not a default. **Showcase: `ampnoise`** (an `N`
    toggle A/Bs floor-on vs dead-silent; a glowing 12AX7 + hiss speckle on the amp grille). Companion
    noise GATE (clamps the floor between notes) is the next pedal — now mintable as a real `FX_*` kind.

26. **Noise gate (`gate`) — a reorderable insert; the rig gate + gated reverb.** **✓ SHIPPED 2026-06-15.**
    Boutique-pedals roadmap (Block C, part 2). `gate(threshold, attack_ms, release_ms)` + `instrument_gate`
    (`SR_GATE`=99/100), `FX_GATE`=17 (2nd kind past the widen). Per-bus envelope follower + threshold:
    above → gain slews to 1 (attack), below → to 0 (release); applied to L/R. `threshold` 0 → always open
    → not called → byte-identical. attack/release via an inlined one-pole coef (`sound_follow_coef` is
    defined later in the file — a forward-ref compile error caught immediately by the gate; inlined
    `gate_coef`). Per-bus insert, auto-added to the chain on first call. **Showcase: `pedalboard`** (the
    GATE pedal — put it after the REVERB pedal for gated reverb). Verified: soundcheck + 900-frame
    tripwire + `--det` byte-identical; `pedalboard`/`groovebox`/`epiano` byte-identical (new kind inert
    in default chains); gated-reverb A/B chops the tail (~5× less back-half energy vs ungated). Honest
    note: the gate is a pre-output insert, so it clamps the *signal path* (a noisy part, or a reverb tail
    for gated verb) — NOT the post-limiter `amp_noise` floor (#25), which sits after everything. That
    completes Block C (amp realism = the optional floor + the gate that tames signal noise).

27. **Shimmer reverb (`shimmer`) — the trophy: a bus pitch-shifter in a reverb loop.** **✓ SHIPPED
    2026-06-15.** Boutique-pedals roadmap Primitive 2 — the one genuinely-new engine left, and the
    hardest DSP of the set. `shimmer(size, damp, shimmer_amt, mix)` (`SR_SHIMMER`=101): a private
    `ReverbTank` whose wet output is tapped each sample, pitched **up an octave**, and recirculated into
    the input → a held chord climbs into an ascending crystalline pad. The pitch-shifter (`octaveup_process`)
    is the textbook granular delay-line octave-up: a read tap sweeps at 2× toward the write head, two
    grains a half-window apart with a constant-power sine crossfade hiding the restart (the same
    `moddel_hermite` resample that `grains_pitch` proved is an exact octave). Master-stage; `mix 0` →
    byte-identical. **The hard part was stability** — a pitch-shifter in a feedback loop wants to die or
    explode, and the first build did both (RMS pinned to 1.0, 72% clipped, DC −0.87). Three fixes:
    octave-up output normalized ×0.707 (the two constant-power grains can sum >1 at the crossfade); the
    recirculated feedback **soft-clipped through `tanh`** (a governor — `shimmer_amt` high plateaus into
    the infinite climb instead of exploding, the echo-bus trick); and a **DC blocker** in the loop (the
    combs pump DC under recirculation). `shim_fb = shimmer_amt × 0.95`, reverb fb 0.70..0.95 — small-signal
    loop gain crosses 1 near the top, so low/mid settings bloom-and-fade and high settings self-sustain.
    Verified: octave-up confirmed (a 110 Hz stab's tail climbs to ~500 Hz, ~2 octaves); max settings stay
    bounded (peak ceiling, ~0.04% soft-clip); soundcheck + 900-frame tripwire + `--det` byte-identical;
    no-shimmer carts byte-identical. **Showcase: `shimmer`** (held chord + rising sparks for the ascent).
    Took the predicted few tuning iterations (die → explode → tame → tune), not first-try like the others.
    **Addendum 2026-06-15 — `instrument_shimmer` (per-instrument).** The singleton was generalized into a
    `SOUND_SHIM_TANKS`=2 pool (`ShimVoice` = tank + `OctaveUp` + per-tank scalars incl. the DC-blocker, so
    two shimmers don't cross-talk) — the spatial showcase exposed that master-only shimmer washed the whole
    mix. `instrument_shimmer(slot, size, damp, shimmer, mix)` (`SR_INSTR_SHIMMER`=109) claims an aux tank
    per slot (`shim_tank_for_bus`, copy `instrument_grains`); ~92 KB total; pool-exhaust → `shim_overflow`,
    bus stays dry. **The trap avoided:** shimmer is master-stage, *not* an `FX_*` insert, so it was NOT
    converted to `FX_SHIMMER`/`apply_insert` (that would move the master shimmer + break bit-exact). Instead
    **tank 0 stays the existing master-stage call (math/order untouched → master is bit-exact, verified:
    `shimmertest` `--det` md5 `9587acf…` identical before/after)** and tanks 1+ run per-aux-bus (after that
    bus's inserts, before it folds to master). Isolation A/B: a stab on the shimmered slot blooms (tail
    0.004→0.002→0.001) while the same stab on a dry slot is silent after the transient. Isolation, not
    spatialization — the wet tail moving with the source is spatial v2 (emitter buses).
28. **Varispeed (`varispeed`) — tape playback speed (the MOOD clock).** **✓ SHIPPED 2026-06-15.** The
    last boutique-list item — navkit's `processHalfSpeed` ported (`SR_VARISPEED`=110). Writes the final
    mix to a 2 s stereo ring buffer, reads it back at `speed` with linear interp: 1.0 = bypass, <1 =
    slower (pitch down + time-stretch, the tape-stop dive), >1 = faster. The applied speed is **slewed**
    (tape inertia → buttery dives, no zipper); **engages at the live edge** (`vari_rpos = vari_wpos` on
    the off→on transition) so the read only ever touches fresh audio — no stale jump. Master output
    stage, after `amp_noise`. **Built for sweeps, not dwelling**: at a held off-speed the read eventually
    laps the write (~2 s click) — for a sustained octave use `grains_pitch`/shimmer instead. Dormant at
    1.0 → not called → byte-identical; keeps running through a release until the slew settles to 1, then
    clears itself. Verified: A4=440 at `varispeed(0.5)` measures **220 Hz / 0¢** (exact octave down),
    `--det` byte-reproducible, soundcheck + tripwire, no-varispeed carts byte-identical. **Showcase:
    `varispeed`** (riff + SPACE tape-stop dive + a SPEED bend slider, reels spinning at the live speed).
    That closes the original boutique-pedal lists.

29. **`fx_mod` / `fx_lfo` — the modulation layer over effects (ADR 0018).** **✓ SHIPPED 2026-06-15.**
    Effects keep their bespoke set-and-hold knobs; this RIDES a *curated, sweep-safe* one under CV
    (`fx_mod`, the per-frame sink modrack patches into) or a fire-and-forget engine sine (`fx_lfo`).
    A per-bus×target slew loop (`fxmod_tick`, top of the sample loop, gated by `fxmod_any` → byte-
    identical until first use) reads the LFO/CV value, slews it (the `note_cutoff` one-pole, CV path
    only — the sine is already smooth), and writes the natural-unit param into the existing array.
    `SR_FX_MOD`=113, `SR_FX_LFO`=114. **Modulation rides, it does not enable** — a target writes an
    *already-configured* effect's param (never its `used` flag), so a stray mod on an un-configured
    bus is silent (the static/modulated split). **7 targets** (`FXMOD_FILTER_CUT` exp-mapped 40Hz–
    18kHz, `_FILTER_RES`, `_DRIVE`, `_TREM_DEPTH`, `_PAN_DEPTH`, `_GRAINS_MIX`, `_SHIMMER_MIX`) — each
    one cheap slewed write. **Deferred (the enum leaves room): reverb/delay *sends* are per-*voice*,
    not per-bus** (no bus-level return-gain to ride — would need a new knob), and **wah has no manual
    position** (envelope/LFO-driven — would need a manual-sweep mode). Both need a *new param* before
    they can be a *target*; see ADR 0018 "Shipped" + STATUS #39. Verified: `fx_lfo` on `SHIMMER_MIX`
    swings RMS 0.10↔0.15 (the wash blooms in/out), bounded (peak −3.3 dB, 0 clip), soundcheck +
    tripwire clean, tune-check unaffected. **Showcase: `fxmod`.** **Bug fixed in passing:** `SR_INSTR_POS`
    collided with `SR_VARISPEED` at request-kind 110 (two parallel agents) — varispeed's handler ran
    first and shadowed `instrument_pos` (silently broken on master); renumbered `SR_INSTR_POS`→112.

30. **Unified `LFO_SHAPE_*` — one modulator-waveform vocabulary (STATUS #39).** **✓ SHIPPED 2026-06-15.**
    Folded three disconnected shape systems (voice LFOs = sine-only; `tremolo`/`autopan` = ad-hoc
    `TREM_*`; the modulation kit's `mod_sh`/`mod_randwalk`/`mod_optical` baked into single effects) into
    ONE enum (SINE/SQUARE/TRI/SAW/RAMP/OPTICAL/SH/RANDOM) + ONE dispatcher: `lfo_wave(shape,phase)` for
    the stateless shapes (−1..1, SINE = the historical `sinf` so it's byte-identical), `lfo_eval(…,
    ModState*,rate,dt)` adding the stateful S&H/random (reusing the kit helpers). Wired into all four LFO
    sites: voice LFOs (`lfo_shape`/`note_lfo_shape` — NON-breaking setters, since 72 carts call the
    frozen `instrument_lfo`), `tremolo`/`autopan` (any shape now; `TREM_*` aliased to `LFO_SHAPE_*`), and
    `fx_lfo` (gained a `shape` arg; the lone caller fixed). Stateful shapes embed a deterministically-
    seeded `ModState` per LFO instance → `--det` byte-reproducible (verified: S&H render md5-identical
    across two runs, bounded, no clip). `SR_LFO_SHAPE`=115; `SR_FX_LFO` extended (e2=shape). `ModState`
    typedef moved above the Voice struct (the Voice embeds `lfo_mod[3]`). Byte-identical for every
    existing cart: SINE → same `sinf`, `depth==0` skips the eval, trem/pan sine/square/tri reproduce the
    old `mod` exactly. **Showcase: `lfoshapes`** (8 shapes live, on pitch or cutoff — S&H-on-pitch = the
    random-step arp). **Forward-compat:** promoting `shape` into `instrument_lfo`'s signature later is
    additive only (the storage/dispatcher/request are already shape-aware).
31. **BBD analog voicing (`echo_insert_bbd`)** — the `echo_insert` delay given a real bucket-brigade
    (MN3005) character. **✓ SHIPPED 2026-07-19.** `echo_insert_bbd(amount)` — `amount` 0..1, `0` = clean
    digital delay (default, dormant → byte-identical). Two things a clean delay line lacks, both applied
    INSIDE the loop so ONLY the repeats are coloured (the dry passes untouched): **(a) clock jitter** — a
    slow wow + faster flutter LFO added to the fractional read tap (`ECHO_BBD_WOW/FLT_RATE/DEPTH`), so the
    repeats shimmer; **(b) delay-time→darker coupling** — a real BBD's clock slows as delay lengthens, so
    bandwidth drops. `echo_ins_set_coef()` scales the loop-LP coef by ~`80ms/time` when BBD is on, so short
    slapback stays bright and a long delay darkens the tail. This is the faithful version of the earlier
    master-`tape()` spike, which got the wobble+darkening but on a master insert wobbled the DRY too; here
    the modulation lives on the read tap / loop filter, dry-exact. `SR_ECHO_INS_BBD`=131, full 4-place
    wiring; `echo_ins_bbd==0` skips the LFO block and `set_coef` returns exactly `sound_echo_coef(tone)` →
    byte-identical to the shipped `echo_insert`. Verified: soundcheck compile-gate `ok`; level-check /
    fx-check Δpk Δrms `+0.0` (pedalboard/modrack unchanged); soak-check stable (feedback loop); wasm/native
    parity holds; and the load-bearing proof — the pre-first-repeat render window (pure dry) is byte-hash
    IDENTICAL with BBD on vs off, while the repeats window DIFFERS and is measurably darker (centroid 3538
    vs 3741 Hz @160ms). **Showcase: `aquapuss`** (the Way Huge Aqua-Puss — `B` toggles analog↔clean for the A/B).
    **Regeneration tuning (2026-07-20):** first cut damped self-oscillation — pull feedback back to tame a
    howl, push it past 1.0 again, and it stayed dead instead of re-igniting (the "didn't seem emptied on real
    hardware" report). Measured two causes: (1) the flutter LFO's read-tap swing was DECORRELATING the
    recirculation each pass (a fast mod > the loop period smears the comb → no coherent buildup); (2) the
    in-loop tone LP eats loop gain, so a bare `fb`=1.05 is only marginal sustain. Fix, both gated on `bbd>0`
    (base echo byte-identical): flutter depth 6→2 samples (the slow wow still carries the audible wobble, but
    the loop stays coherent enough to oscillate), and a self-osc HEADROOM remap in the write —
    `fb>1 → 1 + (fb−1)·(1+4·bbd)` (so `fb`=1.1 drives the loop at ~1.5, a real runaway). Verified: with BBD on,
    pull to 0.5 then push back to just 1.05 now re-ignites and blooms to full (amp 0.06→1.00), peak −7.7 dBFS /
    0 clipped, soak stable, level-check ✓ (base echo unchanged).

32. **Spring reverb voicing (`reverb_spring` + `reverb_spring_tone`)** — the Schroeder reverb given a
    spring-tank voice. **✓ SHIPPED 2026-07-20.** `reverb_spring(amount)`, amount 0..1, `0` = clean Schroeder
    (dormant → byte-identical). Two ingredients, both inside the reverb tank: (a) DISPERSION — a cascade of
    **8 STRETCHED (delay-line) allpasses** (`SPRING_AP_LEN`, mutually-prime ~67..127-sample delays, coef
    `rvb_spring_disp`) reusing `rvb_allpass()`; this is the standard efficient "dispersive delay line" — its
    group delay spans ~tens of ms so a transient smears into a long, audible metallic chirp/"boing" (the
    first-order-allpass scaffold gave too short a window; the ear pass confirmed the stretched form is the
    real thing). (b) a mid BAND-LIMIT (~75 Hz HP + ~4 kHz LP) so the tone narrows to the metallic spring band.
    Applied to the reverb INPUT (`pre`), blended by amount, for ALL tanks (global `rvb_spring`).
    **`reverb_spring_tone(x)`** rides the dispersion coefficient live (0.20..0.90) — the "BOING" character
    knob (looser↔tighter/twangier). `SR_REVERB_SPRING`=132 / `SR_REVERB_SPRING_TONE`=133, both full 4-place;
    `rvb_spring==0` skips the block → byte-identical (level/fx Δpk Δrms +0.0, soak stable, dc clean, no clip).
    **Showcase: `springtank`** (three scenes — KICK a membrane thunk / N a noise burst into the tank, SURF a
    drenched twang line, DUB long-spring skanks; B = A/B vs clean, BOING dials the character). Tiers of spring
    modeling + why this (allpass cascade) is the standard efficient one: Välimäki *Spring reverberation: a
    physical perspective*; arXiv 1910.10105. Optional future polish: a feedback bounce for repeated boings.
33. **Drive voice — Tube Screamer (`drive_voice`)** — the drive INSERT given famous-pedal tone-shaping.
    **✓ SHIPPED 2026-07-20.** `drive_voice(voice, tone)`: `DRIVE_VOICE_NONE`(0, default) = the plain `DRIVE_*`
    clip (byte-identical); `DRIVE_VOICE_TS`(1) = the Ibanez Tube Screamer. The TS character is the FILTERING,
    not the clip (which is our existing `DRIVE_ASYM`): a one-pole split keeps the BASS clean (only mids/highs
    reach the clipper → the tight, no-flub low end), a post-LP is the TONE knob, and clean-lows + clipped-mids
    + rolled-highs = the famous MID HUMP that cuts through a mix. Per-bus/instance filter state
    (`drvins_lp1/lp2`) inside `drive_process`; `drvins_voice==0` skips it → byte-identical (level/fx Δpk Δrms
    +0.0, dc clean, soak stable, wasm parity). `tone` 0..1 rides the post-LP live. `SR_DRIVE_VOICE`=134, full
    4-place wiring + the `DRIVE_VOICE_*` constants. Measured: TS spectral centroid ~2850 Hz vs plain `DRIVE_ASYM`
    ~3500 Hz (the hump = rolled highs). Refs: [ElectroSmash Tube Screamer analysis](https://www.electrosmash.com/tube-screamer-analysis);
    GeoFex TS tech. **Showcase: `tubescreamer`** (OVERDRIVE/TONE/LEVEL + stomp + `B` A/Bs the hump).
    **`DRIVE_VOICE_RAT`(2) + `DRIVE_VOICE_MUFF`(3) added same day** — the dirt spectrum in one enum:
    RAT = full-range HARD clip (hotter) + a low-pass FILTER (aggressive rock/grunge, ProCo RAT); MUFF =
    cascaded soft-clip stages (fuzz sustain) + a mid SCOOP (the exact inverse of the TS hump — EHX Big Muff).
    Each is a clip curve + tone-shaping in the one `drive_voice_shape()` switch, reusing `drvins_lp1/lp2`;
    all gated (voice 0 → plain clip). Measured distinct + 0-clip: TS centroid ~2880 Hz, RAT ~2670 (darker,
    mid-forward), MUFF ~4350 (bright fizz). Next: the three as switchable pedals in `pedalboard`.

One-line version: **we built a very good modular synth and forgot to build the
broken speaker it should play through.**

> **Update 2026-06-05:** the four-layer table above is now a frozen decision —
> [decisions/0015](../decisions/0015-effects-are-recipes-not-primitives.md) closes the
> primitive roster at ~12 functions forever. Everything else (wah, chorus, tape, the
> Leslie, sidechain…) is a recipe of these primitives or a refusal with the musical
> need covered elsewhere; the pedalboard audit lives in that ADR. A new primitive must
> prove it can't be a recipe.

## 18. Tuning measurement — `tune-check.js` + first-engine audit (2026-06-10)

The §16 WAV tooling measures *level*, not *pitch*: `wav-analyze.js` is blind to whether
an engine plays in tune. Adding the modeled engines (bowed, piped, brass, …) made that a
gap worth closing — a waveguide whose delay length is quantized to whole samples drifts
sharp/flat in ways you can't see in an RMS plot. So: **`tools/tune-check.js`** (no deps),
driven by the **`tunecheck`** harness cart. The cart plays a sweep of every non-standard
engine across four octaves of A and `watch()`es the expected MIDI per note; the analyzer
reads that ground truth from the trace, measures each note's actual fundamental, and
reports the error in cents. How-to + the pitch-detection gotchas it sidesteps (frame- not
time-indexing; ±18% candidate-constrained autocorrelation to dodge subharmonic octave
errors) live in [`debug-harness.md` → "Tuning"](../guides/debug-harness.md). `SINE` is the
control — it reads 0.0¢, which is what proves the measurement itself is sound.

**First-run audit (2026-06-10, default voices, A2–A5).** Three buckets:

- **In tune (≤ ±6¢):** SINE (control, 0.0¢), MALLET, EPIANO, PD, PIANO, GUITAR, FM, and —
  notably — **BOWED** (≤ +3¢). Whatever's off about the bowed voice, it isn't pitch.
- **Flatten toward the top (the waveguide signature):** PLUCK, REED, BRASS read ~0¢ low,
  then progressively flat as pitch rises — BRASS A5 −24.8¢, REED A5 −18.3¢, PLUCK A5
  −17.2¢. Consistent with integer-sample delay-length quantization (the period can only
  be a whole number of samples, so high notes round to the nearest available pitch). The
  open fix: a fractional-delay (interpolated) read tap, or a tuning correction baked per
  note. Tracked here until someone takes it.
- **PIPE flute — was genuinely out of tune; FIXED 2026-06-11.** Spoke an **octave low** AND went
  badly flat as it climbed: A2 −13¢, A3 −35¢, A4 −78¢, A5 −159¢. Cause: the bore was sized a full
  wavelength, but the single open-end reflection INVERTS, so a waveguide resonates at SR/(2·delay)
  — an octave down — and the uncompensated jet+filter loop delay added the flat. Fix
  (`sound_pipe_start`): half-wavelength bore minus a loop-delay term **derived from the note-on jet
  length** (`1.69 + 0.308·jetLen`), sized with the bowed-string fractional-read trick to remove
  integer quantization. The jet term matters because the embouchure macro (morph) sets the jet
  length — a constant left morph≠0 sharp by up to a semitone (measured: morph 0.70 was +88…+147¢
  before deriving it). **Now in tune within ~±3¢ from C4 to ~E6 at typical embouchure** (verified
  morph 0.70, robust across seeds). Caveat: the extreme morph≈0 default (longest jet ≈ bore at the
  top) is seed-unstable in its top octave — the `tune-check.js` default sweep tests overblow/morph 0
  and still flags PIPE A5; any real recipe uses morph ≳ 0.3 where it's stable. Tracked in
  [STATUS](../STATUS.md) Open #31; first customer: `air.c` reopened its flute register.

Not a fault, correctly separated by the tool into a "transposed, not detuned" list:
**ORGAN** reads ~an octave low because the default registration leans on the 16′
sub-octave drawbar — it's in tune (+3 to +7¢), it just sounds an octave down.

### Conclusions (2026-06-11)

1. **The library is in tune; the modeled-waveguide engines were the exceptions.** SINE
   (control, 0.0¢), MALLET, EPIANO, PD, PIANO, GUITAR, FM, and **BOWED** are all within a
   few cents. ORGAN's "octave low" is its 16′ drawbar, not detuning.
2. **PIPE was the real bug, and it's fixed.** Octave-low (full- vs half-wavelength bore +
   an inverting reflection) and flat (uncompensated loop delay). Now in tune ~±3¢ from C4 to
   ~E6 at any sane embouchure. This was the station owner's *"that pipe is hella out of tune"*
   (air's Cherry flute) — the cart-side workaround (register drop, overblow 0) was masking an
   engine bug; with the engine fixed, air's register reopened.
3. **The decisive lesson: tuning is per-RECIPE, not per-engine — measure in the regime the
   cart actually uses.** PIPE's loop delay scales with the jet length, which the *morph*
   (embouchure) macro sets, so a constant calibrated at the default voice left real recipes a
   semitone sharp. And the A-only sweep hid a flat ramp the dense sweep exposed. Two takeaways
   for the next modeled engine: (a) derive any tuning compensation from the physical parameter
   that varies (here jetLen), don't hard-code a constant; (b) sweep densely and at the target
   macro settings, not just octaves of A at defaults.
4. **Known residuals (tracked, [STATUS](../STATUS.md) #31):** PLUCK/REED/BRASS go flat at the
   very top (A5 −17…−25¢) — integer-sample delay quantization, fix is a fractional read tap.
   PIPE's extreme morph≈0 voice (longest jet ≈ a short top-octave bore) is seed-unstable on
   the overblow edge — harmless in practice (recipes use morph ≳ 0.3) but only fully closed by
   a jet∝bore re-voicing.
5. **The measurement is now reusable.** `tune-check.js` is the regression gate for any future
   pitched-engine work (`--quiet` in CI); SINE stays the 0.0¢ proof the rig itself is honest.
6. **Pitch bend DOWN — fixed across the waveguides (2026-06-16, [STATUS](../STATUS.md) #41).** All four
   delay-line engines (BRASS/REED/PIPE/BOWED) sized the bore exactly at the note-on pitch and clamped the
   read length to it, so a held note could only bend UP (downward glide/pitch-env/vibrato + the trombone
   slide all stuck at the note-on pitch). Fix: oversize the buffer ×2.5 at note-on and reference an
   init-freq so the note-on read length — hence tuning — is unchanged; only the clamp ceiling rises.
   **BOWED is full-wavelength so the `ks_buf` cap bites at the bottom: down-bend works from ~E2 up, the
   open low-E (E1) can't bend (buffer already maxed).** Carts that worked around the old limit (`upright.c`
   fret-walk, `pdbass.c`'s INSTR_PD swap) can be revisited — see STATUS #41.

7. **Top-octave flatness — FIXED for PLUCK/REED/BRASS (2026-06-16, commit e458af1); the old §18.4
   diagnosis was wrong.** §18.4/STATUS #31 blamed "integer-sample delay quantization, fix = a
   fractional read tap." But the reads already interpolate (the down-bend session, #6, added that) —
   so re-applying it was a no-op. The dense per-note sweep (not the A-only one) showed the truth: an
   **erratic, mostly-SHARP** error (BRASS C#6 +64.5¢), not the smooth flat ramp the A-sweep implied.
   Two real causes: (a) REED/BRASS sized the note-on bore from an **integer-truncated** delay `d0i`
   (`(int)(SR/2f)`) — truncation always shortens the bore → sharp, worst where the delay is small
   (high notes). Fix: use the TRUE fractional `d0` as the init-freq reference; the interpolated read
   honours it. (b) The remaining smooth flat ramp is the **bell-LP loop group delay** (a one-pole,
   once per round trip): subtract `(1−lpCoeff)/lpCoeff` from `effLen`, scaled per engine (BRASS ×0.5,
   REED ×1.0 — REED carries ~2× the one-pole's DC estimate, tuned by meter). **PLUCK** was already
   fractional; its flatness was the Karplus damping average's **exact** half-sample delay → −0.5 on
   the tap (exact at every frequency, predicted the −17¢→0¢ result precisely). Verified by
   `tune-check.js`: all three in tune at representative macros, macro-0 worst cases improved, SINE
   control 0.0¢, dc-check 0. **Lesson (reinforces #3): sweep DENSELY — the A-only sweep's "smooth
   flat ramp" hid an erratic sharp quantization that pointed at the wrong fix.**

8. **PIPE hollow presets + BOWED pressure — both addressed (2026-06-16).** *PIPE* (commit 97a794e):
   the jet loop-delay `1.69 + 0.308·jetLen` under-compensated at long jets (hollow embouchure,
   morph ≲ 0.5), so recorder/breathy/pan-pipe ran flat (ramp to ~−56¢ by G5). Measured the needed
   extra and it **SATURATES** (~+0.8 sample at jetLen 7 *and* 8 — not a growing quadratic, which
   overshot jetLen 8 sharp on the first try) → a clamped-linear correction past jetLen 5, zero at
   jetLen ≤ 5 (flute/piccolo byte-identical). All 5 presets in tune through A5; morph-0 extreme
   improved (A5 −84¢→−32¢). *Still open:* the morph≈0 / hollow **top octave** (above ~A5) mode-flips
   — the jet∝bore re-voicing (residual #4), a separate fix. *BOWED* (commit d90f2a3): bow-pressure
   range recompressed [0.12,0.32]→[0.10,0.26] — the bright end bowed scratchy (>4kHz noise 3.6% vs
   0.5% at the sweet spot); pitch unchanged.

## 19. BRASS character — partly addressed (2026-06-14; brightness shipped 2026-06-16)

A recurring ear note: the brass doesn't yet sound *very brassy* — it speaks and holds, but the
aggressive blat/bite of a real horn section isn't fully there. This was scattered across three
places; consolidated here so it's tracked as one thing (distinct from the **tuning** residual —
BRASS −25¢ at A5 — which is §18 item 4, a separate fix, **still open**). None of this is a
regression; it's "taste/character not finished." Threads, in priority order:

1. **`INSTR_BRASS` engine — ✓ brightness SHIPPED 2026-06-16 (`8dfd12a`); mute axis still open
   ([instrument-engines.md](instrument-engines.md) §8.8.10).** The handoff
   ([brass-realism-handoff.md](brass-realism-handoff.md)) diagnosed the engine as "a clarinet core
   wearing a brass filter": highs died ~an octave early (dead by ~h12) with no loud→bright coupling.
   **Done (fixes #1+#2 from the handoff), all on the OUTPUT stage** (the bore loop destabilizes if
   touched): a bore-amplitude follower → a 0..1 level, the asymmetric shaper steepening with level,
   and a level+brassiness high-shelf lifting energy past ~4kHz (the shock-wave blat). Measured (forte
   trumpet A3): highest harmonic within 20dB **h9→h17** (~2.0→3.7kHz), energy >4kHz **0.2%→2.3%**,
   crest **6.3→14.6dB**. **Still open:** the deferred **mute/plunger axis (harmon/cup → a second
   bandpass)** — a horn's most aggressive bite, and handoff fix #3 (model the bell to fill the
   harmonic series natively rather than synthesize evens downstream). The mute can't be a 4th macro
   (bore/brassiness/breath earned the three); it'd be a `note_cutoff`+`note_res` cart recipe per
   [decision 0017](../decisions/0017-three-macro-core-plus-engine-aux-channel.md)'s "mute = output
   filter" lane, or a per-engine `instrument_mode` index. Preset taste-tuning by ear (STEP 6) also
   still stands now the engine is brighter.

2. **FM-engine brass *preset* — backwards in-note envelope ([instrument-engines.md](instrument-engines.md) §8.8.3).**
   A different brass (the `fm` cart's brass preset, not `INSTR_BRASS`). Noted there: brass is "the
   named stress test" because the mod **index must RISE on attack** — real horns *swell* into
   brightness, while the DX in-note decay goes the other way. The prepared fix is the follow-amp-env
   alternative; until then the FM brass preset attacks too bright and dulls, the inverse of a horn.

3. **The radio stations fake brass — a routing/coverage gap, not engine weakness ([radio-genre-fidelity.md](radio-genre-fidelity.md)).**
   Multiple stations stand in SAW-brass / PD-synth-brass / two-saws for real horn sections and sax
   (italo, the disco band loop, yacht, etc.). So a lot of "brassless" listening is stations **not
   routing to `INSTR_BRASS`/`INSTR_REED`**, independent of how good the engine is — re-voicing those
   horn chairs onto the real engines is its own easy win.

**Verdict / where to start:** #1's level-coupled brightness shipped (the big audible win); the
**deferred mute** is now the highest-leverage *remaining* engine lever, and **#3 (route the radio
horn chairs onto the real `INSTR_BRASS`/`INSTR_REED`)** is the highest-leverage for "make the *radio*
brassier" and needs no engine work.

> **Owner ear note (2026-06-16): there's still more "brassy" to get — keep going.** Even after the
> brightness fix, it can be pushed further. Levers, easiest first: (a) the brightness was tuned
> conservatively — it stops at ~2.3% energy >4kHz to avoid tipping into fizz, but the realistic-forte
> target is ~8–10kHz, so the high-shelf/drive coefficients (`br_hp` amount, `brite`, the level-coupled
> `driveOut`) have room to go harder, A/B by ear with `tools/harmonic-spec.js`; (b) the **deferred
> mute/plunger axis** (the harmon-cup bite, thread #1); (c) handoff fix #3 — model the bell so the
> harmonic series fills natively instead of being synthesized downstream. This is a standing TODO, not
> "done."


## 20. Test coverage — the audio blind spots (2026-06-16)

A "what is NOT tested?" audit. The synth ENGINES are well-covered — `tune-check.js` (pitch),
`dc-check.js` (DC offset), the soundcheck queue tripwire (dropped requests), and `build-all.js`
(cart-vs-API rot) each gate a real failure mode. What surfaced is that whole *categories* of audio
behaviour have no automated gate at all. Tracked in [STATUS](../STATUS.md) Open #42; ranked by leverage.

### 20.1 Loudness regression — SHIPPED `tools/level-check.js`

The highest-value gap and the one we closed. `tune-check.js` proves an engine is in TUNE; nothing
proved it plays at the right LEVEL. That is exactly the regression a week of `sound.h` edits hides:
an engine that gets 6 dB louder still compiles, reads 0¢ on tune-check, and passes the DC check — it
just lives inside the master soft-clip now (`sound.h:5503`), dynamics squashed, "sounds crushed."

`level-check.js` is the twin of tune-check: it renders the SAME `tunecheck` sweep (every pitched
engine × four octaves of A, gated, `watch()`-driven ground truth), `--det` for byte-reproducibility,
and measures each note window peak / RMS / crest in dBFS. **Why a baseline (tune-check needs none):**
pitch has a mathematically exact target (A440); level has no absolute truth, so the gate compares
against a committed golden render (`tools/level-baseline.json`, re-bless with `--save`). `--quiet`
exits 1 on > 4 dB drift (or new silence/clip) — a CI gate. Three absolute checks need no baseline
(a brand-new engine still gets them): SILENT (broken voice), HOT (a single note near full-scale —
two would clip), and loudness-outlier-vs-library-median. The squash check is gated on a HOT peak: a
low crest on a *quiet* tone is just a dense waveform (REED reads crest ~2.5 dB, below a sine's 3.0 dB
— its harmonics, not clipping), so it only flags when the peak is also near the limiter knee.

**First-run finding (now FIXED 2026-06-17):** the library was *uneven in level*. Most engines sit at
−14 to −18 dBFS peak; **BOWED was ~10–13 dB hotter** (A2 −1.2 dBFS — two sustained notes clipped the
master soft-clip) and **BRASS A2 +9 dB** — the level analogue of the §18 tuning audit (the modeled
engines are the outliers; "whatever's off about BOWED, it's not pitch" — partly it was level). Fix: a
per-engine **output-makeup trim** in `sound.h` — BOWED `dc*3.0f → 0.7f` (−12.6 dB), BRASS
`(2.7f+dark*0.7f) → (0.93f+dark*0.24f)` (−9.3 dB, uniform scale so the trumpet↔tuba balance is kept).
Now BOWED −13.8…−20, BRASS −14…−15.7 — both at the ~−15.5 median, no HOT/outlier flags. Amplitude-only,
so pitch is unchanged (tune/dc-check stayed clean); `level-baseline.json` re-blessed. This is exactly
the regression `level-check` exists to catch — and to verify the fix.

### 20.2 Still open (no gate yet)

1. **Effect stability — SHIPPED `tools/fx-check.js`** (harness `fxcheck.c`, baseline
   `fx-baseline.json`). Drives a loud sustained chord into the master bus and sets one effect at a
   time to its documented EXTREME (echo fb 1.1, flanger/phaser ±0.95, filter res 0.99, …), then
   asserts finite/bounded: no collapse-to-silence (a NaN through a feedback loop reads as silence in
   the 16-bit render), no DC runaway, no permanent limiter-pinning, and that it moves the signal off
   DRY. **The DC test is the subtle one:** a finite-window *mean* mistakes a sub-sonic resonant
   oscillation (which max-feedback combs/allpass produce) for DC, so it integrates over the full
   window AND each half and requires both halves to agree in sign — true DC is a persistent bias, a
   wobble averages out. Baseline records the intrinsic state; `--quiet` flags only regressions
   (got-worse / >4 dB drift). It's a STABILITY gate, not a character gate — beauty is still by ear.

   **First-run findings — two real latent bugs at the feedback extremes, both now FIXED:** the
   **phaser** (fb 0.95, 8 stages) carried **−0.13 persistent DC**, the **echo** (fb 1.1) **−0.04** —
   both far past the ~1e-4 `dc-check` clean tolerance, both confirmed persistent (not wobble). Exactly
   the failure mode `dc-check.js`'s header warns about: there is deliberately no master DC blocker, so
   "every asymmetric / feedback stage must block its own," and these two forgot. The phaser allpass
   cascade passes DC at unity (loop DC gain = fb), and the echo `tanh` injects DC; high feedback
   accumulates it ~1/(1-fb)×. **Fix:** a one-pole DC blocker (R=0.999, ~7 Hz corner — sub-sonic, audio
   untouched) on each feedback tap, the same idiom the `drive` effect already uses (`drv_dc_*`).
   Result: phaser −0.13→−0.007, echo −0.04→+0.002; the phaser now reads only "limiter pinned" at the
   extreme (the bounded self-oscillation, expected). Verified: compile-gate + 900-frame tripwire +
   dc/tune/level/fx-check all green, build-all 390/390.

   **Effect STACKING — covered (fxcheck.c tests 13–18).** Six master chains via `fx_order()`: lo-fi
   master (drive→eq→crush→tape), two combs in series (flanger→phaser), two feedback delays
   (echo+reverb), an A/B order swap (drive→reverb vs reverb→drive — ordering is audible *and* both
   bounded), and an 8-deep kitchen sink. All bounded (worst = "limiter pinned" at the extreme, as
   expected). **The stacking caught a third DC bug the single-effect tests had masked:** the
   **flanger** also accumulated DC at high feedback (the two-combs stack read −0.03 once the phaser was
   clean). At fb 0.95 alone its DC *oscillated* (classed wobble); in series it settled into a steady
   offset. Same feedback-comb-without-a-DC-blocker bug; fixed with the same one-pole idiom (single
   flanger −0.046→−0.002). **All three feedback combs — phaser, echo, flanger — are now DC-blocked.**
   This is the lesson of stacking: a bug that hides as a wobble in isolation can settle into DC when an
   effect runs into another's input. Untested still: stacking on the per-instrument aux buses (only
   master bus 0 is swept).
2. **The web/wasm audio path is verified only by ear — SCOPED (not built).** Full scoping + phasing:
   [`web-audio-parity.md`](web-audio-parity.md). Three axes (codegen/float determinism; sample-rate &
   block-size; real browser runtime). **A 2026-06-17 source dig CONFIRMED a pitch bug on paper: the
   WORKLET backend (desktop default) plays ~+147¢ sharp on non-44.1k devices.** `sound_worklet_init()`
   uses `emscripten_create_audio_context(0)` (NULL opts → device-default SR, usually 48000) and fills the
   128-sample output with NO resampler from 44100-synthesized audio. The **plain** backend resamples via
   miniaudio (`LoadAudioStream(44100)`) and is correct. Device-dependent (macOS built-in is often 44.1k →
   fine on the owner's Mac; 48k devices are sharp), so it shipped unnoticed. **Fix APPLIED** (`sound.h`
   `sound_worklet_init` forces the worklet context to `SOUND_SAMPLE_RATE` via
   `EmscriptenWebAudioCreateAttributes`; compiles native + emcc-worklet). Follow-ups: on-device confirm +
   republish (shipped `site/` carts keep the old context until rebuilt). **Phase 1 codegen gate SHIPPED
   (`tools/web-audio-check.js`, clang-native vs emcc-wasm per engine): 15/16 engines sample-identical (diff
   75–120 dB below signal — inaudible libm/FMA ULP noise; TRI byte-exact). Only BOWED sample-diverges —
   chaotic stick-slip friction, sensitive to 1-ULP differences — yet its two renders match in RMS level to
   0.06 dB (same note, micro-phase only).** So the wasm math is faithful; the only real web bug was the SR.
   Lesson baked into the gate: sample-diff is the wrong metric for a chaotic engine → two-tier (sample parity,
   else perceptual-level fallback). `audio-timing.md` covers timing, not samples.
3. **Set-and-hold footgun — SHIPPED `tools/lint-fx-frame.js`.** The set-and-hold rule
   ([effects-recipes.md](../guides/effects-recipes.md) intro) was documented but unenforced. Now a
   static lint flags an unconditional per-frame call to a buffer-rebuilding effect in
   `update()`/`draw()` (guarded calls / `filter()`/`varispeed()`/`note_*` excluded; waivable;
   `--quiet` CI gate). **The one-time audit came back clean across 390 carts** — the codebase already
   follows the rule — so it's a forward regression guard. Heuristic limit: it doesn't follow
   helper-routed calls (the `apply_fx()` pattern), which is the correct structure regardless.
4. **Soak gate + denormal guard — SHIPPED.** `tools/soak-check.js` (harness `soak.c`) renders ~64s of
   stress/idle cycles — dense notes through a big reverb+echo tail, then silence — and asserts the
   long-run failures a few-second test can't see: stress level STABLE across all cycles (no gain drift,
   no progressive voice-pool starvation from a leak), idle tails DECAY ≥12 dB below stress (no stuck /
   leaked voice ringing — healthy decays 15–18 dB, a leak ~0–5), the idle floor doesn't CLIMB run-long
   (no energy/DC accumulation), and nothing blows up. Decay-RELATIVE thresholds (not an absolute silence
   floor, so it doesn't depend on exactly how long an aggressive tail takes to die). First run clean: 24
   cycles within ~1.5 dB, every tail decaying 15–18 dB. **Denormal flush-to-zero** added in `sound.h`
   (`sound_set_denormal_ftz()`, called at the top of `sound_callback` — the one point every audio path
   funnels through): a long reverb/echo feedback tail decays below ~1e-38 into the denormal range, where
   FP ops run 10–100× slower → audio-thread CPU spikes (stutter) on some CPUs, invisible in the output
   (denormals are far below 16-bit). FTZ+DAZ on x86 (`_mm_setcsr 0x8040`), FPCR FZ bit on arm64, no-op on
   wasm (no denormal control). Output byte-unchanged → level/fx/dc baselines untouched. The soak proves
   the tails decay (correctness); FTZ handles the CPU side — quantifying the latter needs audio-thread
   timing (the profiler only sees the main thread), a follow-up.

### 20.3 Micro-bug spotted in the same pass

`amp_noise_process` + `varispeed_process` run AFTER the master soft-clip (`sound.h:5509-5510`), and
the device output path has no final clamp (only the WAV writer clamps, `sound.h:372`). So varispeed's
interpolation can push the device signal slightly past ±1.0 → a hard driver clip. Tiny, but a real
seam — a final clamp on the device write (or running those two before the soft-clip) closes it.

## 21. The Moog ladder filter — `FILTER_LADDER` (2026-06-21)

The per-voice filter was a Chamberlin SVF (2-pole, 12 dB/oct, the one structure that
gives LP/HP/BP/notch — §5.5, §17). Authentic *enough* for most carts, but on a slow
resonant sweep it reads brighter and buzzier than the real thing: a Minimoog's voice is
the **4-pole transistor ladder** (24 dB/oct, lowpass-only) Bob Moog patented in 1965.
`moog.c` (the Model D rebuild) wanted the genuine article, so `FILTER_LADDER` (mode 5)
joins the `FILTER_*` family.

Implementation (`sound_ladder`, beside `sound_svf`): a **Zavalishin TPT / zero-delay-
feedback** ladder — four one-pole TPT stages with a global feedback `k` (0..4). Chosen
over the musicdsp "naïve Moog" (Stilson/Smith) because the ZDF form is unconditionally
stable and tunes correctly without oversampling — the existing engine is strictly
per-sample, so an oversampled model didn't fit. The closed-form ZDF solve is
`u = (in − k·B)/(1 + k·G⁴)`; the denominator is always > 1, so no division hazard, and a
±8 clamp on the 4 integrator states (`lad_s[4]`) is the NaN/runaway net. Resonance reuses
`flt_q` (recovered back to 0..15) so the knob maps identically to the SVF; `k = res·4/15`,
so res 15 ≈ self-oscillation. The 24 dB slope and the **passband-bass drain as resonance
climbs** (the ladder's signature) both fall out of the topology for free.

Validated: 30 s at max resonance with a sweeping cutoff (self-oscillating) → 0 clipped
samples, DC ≈ 6e-5 (no runaway), no NaN. `level-check`/`fx-check` unchanged — the SVF
path and all existing baselines are byte-for-byte untouched; the ladder only runs when a
voice explicitly selects mode 5. **Open / deferred:** no per-stage `tanh` saturation
(the linear ZDF is what's shipped — clean, the drive stage after the filter supplies the
grit per §17); and oscillator **hard sync** is still the remaining un-modeled Minimoog
trait (see `moog.c` header).

## 22. Oscillator hard sync — `instrument_sync` / `note_sync` (2026-06-22)

The marquee Minimoog/Behringer-Model-D voice we couldn't make: **hard sync** — the
screaming, tearing lead where one oscillator force-resets another. It had to live
INSIDE a voice (sample-accurate phase reset), which the engine had no mechanism for —
our three "oscillators" in `moog.c` are independent voices that only meet at the mixer.

**The enabling find:** the wavetable oscillators are NAIVE (`sound_osc`: a saw is just
`phase*2−1`, etc.). So naive hard sync is *consistent* with the engine — no PolyBLEP
rabbit hole. (If they'd been band-limited we'd have needed a BLEP at the reset point,
scaled by the sweeping discontinuity height — a much bigger job, and even then sync is
the hardest thing to anti-alias. We chose naive deliberately, A/B by ear. The aliased grit
reads as analog-aggressive, fitting.) **The full naive-vs-band-limited rationale — when it's
audible, why we keep it, and the revisit triggers — is frozen in
[decision 0019](../decisions/0019-oscillators-are-naive-antialiasing-deferred.md).**

Implementation: each voice gets a second **slave** phase (`sync_ph`) and a slewed
`sync_ratio` (riding the same machinery as cutoff/drive). The master (`v->phase`) runs
at the note pitch as before; the slave advances at `ratio×` and is **reset to 0 every
time the master wraps**; the audible sample reads the slave when sync is on. Ratio 1 =
transparent, higher = the bright tearing; sweeping it is the sound. Gated entirely on
`sync_ratio > 0` in the wavetable-only branch, so engines and every existing cart are
**byte-identical** (verified: tune/level/fx all unchanged). API mirrors `drive` exactly
(`instrument_sync` note-on snapshot + slewed live `note_sync`), four-place wired.

In `moog.c`: OSC1 stays the clean master pitch, **OSC2 is the synced oscillator** (a SYNC
slider sets the ratio, a SYNC-targeted LFO sweeps it cart-side like DRIVE), and a factory
**SYNC** preset showcases it. Validated: 30 s at swept max ratio → 0 clipped, no NaN, DC
bounded ≈ −46 dBFS (the small offset a chopped asymmetric wave inherently carries — not a
runaway). **Open/deferred:** no per-voice DC blocker on the raw sync output (inaudible at
−46 dB, left raw for character); PolyBLEP/oversampling if the aliasing ever bothers anyone
(it's the naive-on-purpose call). The Steiner-Parker filter (the Neutron's voice) and a
patchable modular UI remain the only un-modeled Behringer-clone ideas.

## 23. Steiner-Parker filter — `FILTER_STEINER` (2026-06-22)

Third filter character, after the clean SVF and the smooth ladder (§21): a
Steiner-Parker-FLAVOURED nonlinear 2-pole lowpass — the **Behringer Neutron's** voice.
Where `FILTER_LOW` stays clean and `FILTER_LADDER` stays creamy, the Steiner is RAW: a
diode-style `tanh` in the resonance feedback path + an output drive make it get dirty and
SCREAM as resonance climbs (great for acid and biting leads).

Implementation (`sound_steiner`, beside `sound_ladder`): the existing Chamberlin SVF math
with `tanh(flt_band)` in the feedback term and a `tanh(flt_low*1.3)` output. **Reuses the
SVF's `flt_low`/`flt_band` state** (only one filter runs per voice) — so unlike the ladder
it needed NO new voice fields, just the function + a dispatch arm. Honest scope: this is a
*flavour*, not a circuit-accurate Steiner model (the real topology is obscure); the goal was
a third, clearly-distinct aggressive character, judged by ear. **Multimode** like the real
Steiner-Parker — `FILTER_STEINER` (6) is the lowpass; `FILTER_STEINER_HP/BP/NF` (7/8/9) are
the other taps (`sound_steiner` switches on `flt_mode`, same as the SVF). Four-place wired, gated so every
existing voice is byte-identical (level/fx unchanged).

Stability: 25 s at max resonance, swept cutoff → 0 clipped, no NaN, bounded by the tanhs
(HP/BP carry even less DC than LP). DC ≈ 0.013 (−37 dBFS) on the LP from the asymmetric
nonlinearity — inaudible, doesn't grow, and in `moog.c` the default DRIVE stage's DC-blocker
removes it; left raw otherwise (in character).

The multimode prompted restructuring `moog.c`'s filter panel from one flat 7-button row
(which conflated *circuit* and *response*) into a **TYPE** selector (OFF/SVF/LAD/STE) × a
**MODE** selector (LP/HP/BP/NF), with MODE greyed when the type is OFF or LADDER (LP-only).
The cart maps the pair to the engine constant via `filter_mode()`; the preset `Patch` now
stores `ftype`+`fresp` instead of a raw mode. The clearer model is the right surface for any
future filter type, too.

## 24. ✅ FIXED — `varispeed` moderate-speed lock + speed-up silence (2026-06-22)

**Status: FIXED + measured.** Two distinct bugs, both in `varispeed_process` (`sound.h`).
Reported by the owner: (a) sweeping the SPEED slider did **nothing audible in the moderate
range (~0.3×–2×)** — pitch only changed at the extremes; (b) **increasing speed went silent
for a while.** `kaoss`'s TAPE program rides the same `varispeed()` and was affected too.

### Bug 1 — moderate speeds didn't pitch-shift (the deadband-reset lock)
The applied speed is slewed toward the target: `vari_speed += (target − vari_speed) * 0.0015`.
The old code took a dry/passthrough branch whenever the *slewed speed* sat inside the deadband
`[0.999, 1.001]`, and in that branch **hard-reset `vari_speed = 1.0` every sample**. But the
per-sample slew step is `(target − 1)·0.0015`. For it to clear the deadband in a *single* step
(before the reset slams it back) you need `(target − 1)·0.0015 > 0.001` → **`target > 1.667`**
(or `< 0.333`). For any moderate target the step is smaller than the band, so the reset put it
back to 1.0 every sample and it never escaped → bypass forever. Only the extremes jumped clear
in one step, exactly matching the report.
**Fix:** gate the dry/reset branch on the **target** being ~1 too, not just the slewed speed
(`bool settled = speed≈1 && target≈1`). Now the slew accumulates out of the band for any target.

### Bug 2 — speeding up was silent for ~1 s (read ran into un-recorded buffer)
The ring buffer only held the *past*, and only recorded *while engaged*. Slowing down reads
*behind* the write head (into recorded past) — fine. Speeding up reads *ahead* of the write
head into the not-yet-recorded region (zeros) and stays there until it laps the 2 s buffer
(~1 s at 1.5×) → silence.
**Fix:** the ring now **rolls continuously** once the cart has ever called `varispeed()`
(`vari_ever` flag; gated so carts that never use it pay zero cost), even while settled at 1.0.
So a later speed-up finds real recent audio instead of silence. Output stays byte-identical
when not engaged (the record writes a side buffer; it doesn't touch the output). Residual: a
*cold-start* speed-up (the cart's very first `varispeed()` call is itself a speed-up, with no
prior unity pre-roll) can still gap briefly — inherent (you can't pitch-up audio that hasn't
played yet). The `varispeed` cart calls `varispeed(1.0)` on frame 0, so it's covered.

### Validation (the earlier "harness invalid" was a non-sustaining test note)
- **Standalone sim** of the exact `varispeed_process` math: OLD locks at ratio 1.005 for
  0.5/0.7/0.9/1.1/1.3/1.5×; NEW gives 0.505/0.700/0.900/1.105/1.305/1.500 — every speed correct.
  A second sim showed ~1 s of `-120 dB` silence on speed-up under the old record-only-when-engaged
  path, gone with the rolling record.
- **End-to-end in the real engine** (a sustained A4 sine via `note_on`, `play.js --wav`, pitch
  measured per window): control 1.0× → 440.0 Hz; after `varispeed(1.3)` → **571.8 Hz** (= 440 ×
  1.30) at the **same −18.3 dB** level (no dropout). The key to a valid measurement was a properly
  *sustained* held note (not a short `hit`) plus a unity pre-roll before the speed step — the
  earlier "crest-0 dB / wrong pitch" was the test note not sustaining, not an engine fault.
- Gates re-run after the edit: compile-gate clean, `tune-check` / `level-check` / `fx-check` all
  pass with zero deltas (byte-identical at rest — varispeed isn't engaged in those sweeps).

## 25. TB-303 diode-ladder filter — `FILTER_DIODE` (2026-07-02)

Fourth filter character, and the first one built to a **measured** target: the acid filter
for the rebirth-classic rack ([`rebirth-classic.md`](rebirth-classic.md) §3). The §17 audit
already named the gap — the 303 squelch is "the filter driven into saturation, not the
filter" — and a fidelity pass with the new **`tools/filter-spec.js`** (see below) made it
quantitative. The three 303 signatures, and which engine filter had them (saw @ A1, response
vs `FILTER_OFF`):

| signature (303 target)        | `FILTER_LOW`   | `FILTER_LADDER` | `FILTER_STEINER` | **`FILTER_DIODE`** |
|-------------------------------|----------------|-----------------|------------------|--------------------|
| slope ≈ 18 dB/oct             | −10            | −21..−23        | −17..−20         | **−16..−17.5**     |
| bass drain as res climbs      | none           | −6→−14 dB       | ~none            | **−4→−8 dB**       |
| saturation inside the loop    | none           | none (linear)   | tanh             | **tanh**           |
| res peak @fc=600, res 8/12    | +1.4 / +7.1    | −1.3 / +4.4     | −0.4 / +4.6      | **+3.3 / +10.6**   |

No pre-existing filter had all three; the diode has all three, and its resonance *wakes up
earlier* (the ring lives at usable knob positions, not just near self-osc).

Implementation (`sound_diode`, beside `sound_ladder`; constant `FILTER_DIODE 10`): the ZDF
transistor-ladder skeleton (reuses `lad_s[]`, no new voice fields) with three circuit
differences — the output taps **stage 3** (≈18 dB/oct, the 303's measured rolloff; the Moog
taps 4), the **feedback path saturates** (`tanh(B·1.5)/1.5` — unity small-signal so low-level
response stays honest, clips the scream: the diodes), and a gentle resonance makeup
(`y3·(1+0.2k)`) so the squelch stays usable while the passband drains (the drain itself is
topology and preserved). Feedback still off stage 4 → ladder bass-drain + self-oscillation
kept; `k` runs to 4.3 (the tanh bounds the loop — self-osc settles where loop gain hits 1).
Honest scope: like the Steiner it's a *flavour* with a measured target, not a component-level
303 model (no oversampling; navkit's oversampled ladder remains the donor if aliasing ever
shows at the top of the range). Four-place wired; purely additive dispatch arm (mode 10 checked
before the `>= FILTER_STEINER` range), every existing voice byte-identical.

Musical A/B (tb303 cart, ACID 1 pattern, default knobs, `play.js --wav`): SVF −20.5 dB RMS /
crest 7.2 (flat, polite) · transistor ladder −26.2 / 12.0 (drained, starved) · **diode −23.7 /
10.1** (drained but compensated — punchy). `tools/carts/tb303.c` now ships on `FILTER_DIODE`
via its `ACID_FILTER` define (one line back to `FILTER_LOW` to compare by ear).

**`tools/filter-spec.js`** is the reusable half: renders a generated probe cart (saw @ A1
through each filter config per 1s segment, tune-check RECIPE pattern), Goertzel-measures every
harmonic against a `FILTER_OFF` reference, and reports slope (fit 2–4×fc), resonance peak, and
fundamental drain per res step. Deterministic; cite its table as acceptance evidence for any
sound.h filter change (`--modes low,ladder,steiner,diode`, `--json`, `--keep`).

Environmental gotcha discovered mid-spike, for the record: the play.js harness (any Raylib
cart, even `--headless`) **segfaults in `rlglInit`/`InitWindow` when the Mac's display is
asleep** — a 3 A.M. render suddenly crashing signal-11 with an empty trace is the screen lock,
not your engine edit. Fix: `caffeinate -du node tools/play.js …` (wakes + holds the display).

**NaN-guard on the ladder cutoff (2026-07-11).** `ladder_core` (both `FILTER_DIODE` and the
Moog `FILTER_LOW`) now clamps `cutoff_hz` to `[10, 0.49·SR]` before the `tanf`. A cutoff **at/
above Nyquist** makes `tanf(π·fc/SR)` blow up → `g` Inf/NaN → `G` NaN, and a cutoff **≤ 0** is
just as invalid; the resulting NaN lands in `lad_s[]` and the existing `±8` state clamp does
**not** catch it (`NaN > 8` and `NaN < −8` are both false), so the voice — and everything it
feeds through the master bus — goes **permanently silent** until reload. The clamp is a strict
no-op for every valid cutoff (all cart cutoffs sit in ~10–10800 Hz), so it's a pure safety net,
not a tone change. Found via acidrack playtest: filter-*tracking* on an **octave-down** note
computed a negative cutoff (`cut_hz + trk·(midi−BASE)·k`, `midi < BASE`) → dead silence. Rule:
any new per-voice cutoff math must stay positive and sub-Nyquist; the engine now backstops it.

> **Publish gotcha — the clamp is a byte-level no-op, so DON'T read "no change" as "it didn't
> work."** Because the guard only bites at out-of-range cutoffs (which were already broken), a
> `-Os` recompile of any cart whose cutoffs are all valid emits **byte-identical wasm** to the
> pre-change build. So after this engine edit, force-rebuilding the gallery leaves *most* carts
> unchanged in `site/` git — that's the guard doing nothing on healthy audio, exactly as intended,
> **not** a stale/failed build. (Verified this way: the rebuilt wasm matching the committed wasm
> IS the no-op proof.) Corollary for any future `sound.h` safety net: expect the same — assert
> correctness by *reproducing the crash it prevents*, not by a diff of the published output.
> Also seen during that deploy: `build-site` parallelizes emcc across a full `publish-all`, and
> ~6% (29/482) transient-**OOM'd** under the memory pressure; each rebuilds fine in isolation.
> If a batch shows scattered `emcc… FAILED` with no error text, it's contention — re-`--force`
> the failed set individually (or in a small serial group), don't hunt for a per-cart bug.

## 26. 303 realism — "it sounds kinda digital" (2026-07-19)

§25 gave the filter its measured 303 character; this pass chased the leftover **digital** feel
(maker's ear on `acidcandy`). Diagnosis, by rendering the `303a`/`303b` solo stems
(`play.js --solo-slot 6|7 --wav`) + reading the voice: the *filter* and its envelope were fine
(the diode ladder is a proper saturating ZDF; the mod-env decays exponentially `e^-4t/d`). Three
tells, all **outside** the filter, ranked by audibility on the default (filter-closed) patches:

1. **The oscillator is frozen-perfect.** `harmonic-spec` on both stems came back razor-clean
   integer harmonics — no pitch drift, no cutoff jitter, no per-cycle variation. The engine
   *models* analog wander for its physical voices (reed/brass/vox humanize; tape/BBD wow) but the
   acid saw got none. **This was the #1 tell** — the sound sits perfectly still.
2. **Resonance quantized to 16 integer Q steps.** `acid_res_q()` cast `p·15` to `int`; the DSP
   `flt_q` (and `note_res`, which encodes ×1000) are continuous — the stepping was purely that cast.
3. **Naive non-band-limited saw** (`sound.h:2578`, `phase·2−1`, no PolyBLEP). Aliases — but a
   *latent* tell: both default patches render **0% energy >4 kHz**, so it only bites when the cutoff
   opens hard on the lead. Engine-wide blast radius (every saw voice), so **deferred** pending a
   real need; see the open item below.

**Fixed 1+2, both isolated to `acid303.h`:**
- **Continuous res** — new `acid_res_f()` (float `p·15`) feeds the live `note_res` ride; the int
  `acid_res_q` stays for `instrument_filter`'s baseline (engine API is int) + the change-guard.
  Only matters ~1 frame at note-on before the ride takes over.
- **Analog drift** — new `Acid.drift` field (0..1, default **0.5**). `acid_define` attaches two
  always-on **`LFO_SHAPE_RANDOM`** wanders (the engine's slow filtered-random source, `mod_randwalk`):
  LFO 0 → `LFO_PITCH` ~5 cents @ 0.13 Hz, LFO 1 → `LFO_CUTOFF` ~20 Hz @ 0.19 Hz (sub-osc gets its own
  pitch wander @ 0.11 Hz). Different rates so they never lock; each note-on re-seeds independently
  (`lfo_seed_ctr`, `sound.h:5014`) so 303a/303b drift apart. `drift=0` → depth 0 → skipped →
  **byte-identical** to the old dead-flat voice (opt-out + clean A/B). Emergent, no per-frame cart
  code. All three acid carts (`tb303`/`acidrack`/`acidcandy`) inherit it; `build-all` clean.

Verification caveat worth recording: **`wav-correlate` is the wrong oracle for a near-self-osc acid
voice.** Same code rendered twice correlated at **0.03** — the diode filter at res ≈10.5/15 is a
nonlinear system that amplifies sub-sample timing differences into fully decorrelated ringing that
sounds identical. The LFO drift itself is deterministic (counter-seeded); the chaos is the filter.
Verify resonant voices with **statistical** measures (spectrum / f0-wobble / levels), not waveform
correlation. Maker confirmed the drift amount "subtle enough" by ear.

**SHIPPED — a drift "tweak" knob (2026-07-20, `5db24ab9`).** `Acid.drift` is now a per-303 **DRIFT knob**
on acidcandy's FX panel (reachable in both voicings via the FX soft-key). It **rides live** — `acid_ride`
re-applies the wander LFOs only when `drift` changes (`acid303.h` `acid_set_drift` + `_ld` guard,
`6c86f3b3`), so turning it is smooth and cheap; constant drift is a no-op (tb303/acidrack byte-identical).
No `ACID_*` enum churn (it's the struct field).

**#3 (band-limit the saw) — REASSESSED 2026-07-20: KEEP THE RAW SAW. Do not "fix" this.** The maker
A/B'd a wide-open high saw sweep (raw / no-filter vs the same through a 1500 Hz filter — the raw one
dumps **67.9%** of its energy >4 kHz vs the filtered **0.4%**, which is why the aliasing is normally
inaudible) and **prefers the RAW, aliased character** — brighter + grittier, dead-on for the lo-fi
surface (VISION). Decision: the naive saw's aliasing is a **feature, not a bug**; do NOT band-limit
`INSTR_SAW` by default. If a *clean* saw is ever wanted for a specific patch, add band-limiting as an
**opt-in per-voice flag** (the `classic`/DF pattern — data-driven, default off), never a silent global
replace. NB for a future agent: "the saw aliases" is a KNOWN, WANTED property here — don't PolyBLEP it
on sight. (Torture-probe that shows/hears it: a raw `INSTR_SAW` glided C3→C9 with no filter.)

> **UPDATE 2026-07-20 — the opt-in clean saw was built (raw is STILL the default).** The maker wanted a
> live toggle to keep A/B-ing, so band-limiting shipped exactly as the reassess prescribed: **opt-in, never
> a default swap.** Engine: **`sound_polyblep()`** subtracted from the naive saw at its wrap, gated by a
> per-voice flag `v->aa` (from `instr_bank.bandlimit`); applied only on the non-unison osc path. API:
> **`instrument_bandlimit(slot, on)`** (SR_INSTR_BANDLIMIT=135; 4-place). `acid303.h` carries **`Acid.clean`**
> (0=raw default) → `instrument_bandlimit` in `acid_define`, non-destructive like `classic`/`drift`. `aa=0`
> ⇒ byte-identical raw saw (verified: h1–h4 identical raw vs clean = brightness kept; high-freq junk lower on
> clean = aliasing removed; soundcheck/tune-check/build-all all clean). **Live A/B: `tb303`, press `C`** (open
> CUT + play high to hear it). Unison saws stay raw by design (acid/tb303 are non-unison). acidcandy per-303
> clean toggle = the pending UI handoff (below).
>
> **SHIPPED — acidcandy per-303 CLEAN toggle (2026-07-20, `5db24ab9`).** A full-width `RAW SAW`/`CLEAN SAW`
> toggle strip on the FX panel (below the DRV/SEND/VERB/DRIFT knobs) — chosen over the DF-extras page so it's
> reachable in BOTH voicings (a vanilla 303 can be clean or raw too). Flips `ac[i].clean` + re-`acid_define`;
> only affects the SAW wave. Verified via scripted taps (RAW SAW → CLEAN SAW).

**SHIPPED — both halves, a per-303 classic⟷Devil-Fish voicing switch (2026-07-20).** UI wired into
`acidcandy.c` 2026-07-20 (commit 235c78b2) exactly per the READY-TO-APPLY snippet below, with one
change: the VIEW-tab hash seed is **`0x05u`**, not the snippet's `0x0Bu` — `0x0Bu` = `0x0Au + 1`
collides with the FLAG palette's `0x0Au + f` (FL_N=6, so 0x0A–0x0F are taken). Rects differ so it
would have worked, but `0x05u` is genuinely free. Both states render clean (CL/DF + greyed VIEW tab);
non-destructive flip holds by construction (toggles `ac[i].classic` + `kpage[i]`, re-`acid_define`,
never touches `a->p[]`). The snippet is kept below as the record. The maker
noticed acidcandy's "DF" button is only a *knob-page* flip (`kpage`, shows the DF-extra knobs) — the
303 is *always* Devil-Fish-voiced; there's no classic-vs-DF *sound* toggle. The engine half is now in
`acid303.h`: a new **`Acid.classic`** flag (0 = DF default, 1 = vanilla), read **live** by the mapping
helpers + `acid_note` so `a->p[]` is never touched — the DF knob values (SUB/TRK/ADEC/SLDT) **survive a
flip**. `classic=1` pins vanilla: 6.0 cutoff range · fixed 60 ms slide · 2 ms attack · two-decay OFF ·
no filter-track/sub-osc/accent-sweep. It's the data-driven twin of `acid_stock()` (a runtime FLIP, not
a one-shot destructive re-voice). Every `classic=0` branch is byte-identical to the old expression, so
`tb303`/`acidrack`/`acidcandy` are unchanged (`build-all` clean); verified with a throwaway flip-probe
(4 non-destructive flips, no crash/NaN, classic reads ~10% quieter + a different harmonic balance).

The UI wiring is deliberately **not** done — `acidcandy.c` was under active parallel edit. The design
(maker-agreed): two *orthogonal* controls, **per-303**. (a) The **DF light becomes the VOICING switch**
(tap = classic⟷DF; write `ac[i].classic` + re-`acid_define(&ac[i])`; the light means "DF sound on", the
honest meaning). (b) A **separate small page-tab** flips the VIEW (the existing `kpage` role — core-5 ⟷
DF-extras), and since the classic-5 knobs (CUT/RES/ENV/DEC/ACC) drive **both** voicings, the tab only
matters in DF mode: **grey/hide the DF-extras page when `classic`** (nothing to edit there). So core-5 is
always tweakable — you can sit on the classic knobs with DF blazing, which was the maker's explicit ask.
No `ACID_*` enum churn (it's a struct field). Small once the cart frees up.

**READY-TO-APPLY acidcandy wiring** (against `tools/carts/acidcandy.c` as of 2026-07-20 — re-locate the
`krDF` block by name, not line number; it may have moved). The 303 face-draw already has in scope: the
per-line `Acid *a` (== `&ac[i]`), the index `i`, `kpage[i]` (the view page), and `mac[i].col` (machine
tint). **No new global** — voicing is `a->classic` (persistent), view is the existing `kpage[i]`.

*Step 1 — split the right-end `krDF` column into two stacked switches* (replace the single `krDF` split):
```c
Box krDF   = lay_split(krow, EDGE_RIGHT, lay_clamp(FU * 0.8f, 14, 26), &krow);
Box krPAGE = lay_split(krDF, EDGE_BOTTOM, krDF.h * 0.44f, &krDF);   // krDF = top (VOICING), krPAGE = bottom (VIEW tab)
```
*Step 2 — the knob if/else (`if (!kpage[i]) … else …`) is UNCHANGED* (still core-5 vs DF-extras+WAVE).

*Step 3 — replace the single `{ // the DF page switch … }` block with these two* (chassis-button idiom,
same as the old DF/WAVE buttons; pixel margins are eyeball-tune, the row is tight):
```c
{   // TOP = VOICING switch: flip classic <-> Devil Fish (non-destructive — a->p[] preserved).
    int bx=(int)krDF.x, by=(int)krDF.y+1, bw=(int)krDF.w-1, bh=(int)krDF.h-2, dp=0,dhot=0,df=0;
    void *wd = ui_wid_hash(0x07u, bx, by, bw, bh);                 // keep the old DF hash id
    if (ui_button_core(wd, bx, by, bw, bh, &df, &dp, &dhot)) {
        a->classic = !a->classic;                                 // FLIP the sound
        if (a->classic) kpage[i] = 0;                             // DF-extras page is meaningless in classic -> snap to core-5
        acid_define(a);                                           // re-apply: attack + cutoff-range change at define time
    }
    int dfon = !a->classic;                                       // LED lit = Devil Fish voicing ON
    rrectfill(bx, by, bw, bh, 2, dfon ? mac[i].col : CLR_DARK_PURPLE);
    rrect(bx, by, bw, bh, 2, (dhot || dfon) ? CLR_WHITE : CLR_BROWNISH_BLACK);
    font(FONT_TINY); plabel(dfon ? "DF" : "CL", bx+bw/2, by+2, dfon ? CLR_BROWNISH_BLACK : CLR_LIGHT_PEACH);
    circfill(bx+bw/2, by+bh-3, 1, dfon ? CLR_LIME_GREEN : CLR_DARKER_PURPLE);
}
{   // BOTTOM = VIEW page-tab: core-5 <-> DF-extras. Only meaningful in DF; greyed + inert in classic.
    int bx=(int)krPAGE.x, by=(int)krPAGE.y, bw=(int)krPAGE.w-1, bh=(int)krPAGE.h-1, pp=0,phot=0,pf=0;
    int avail = !a->classic;                                      // nothing to view when classic
    void *wp2 = ui_wid_hash(0x0Bu, bx, by, bw, bh);               // 0x0B = an unused hash id (verify no clash)
    if (avail && ui_button_core(wp2, bx, by, bw, bh, &pf, &pp, &phot)) kpage[i] = !kpage[i];
    int lit = avail && kpage[i];
    rrectfill(bx, by, bw, bh, 2, lit ? mac[i].col : CLR_DARK_PURPLE);
    rrect(bx, by, bw, bh, 2, (avail && (phot || kpage[i])) ? CLR_WHITE : CLR_BROWNISH_BLACK);
    font(FONT_TINY); plabel(kpage[i] ? "2" : "1", bx+bw/2, by+1,
                            avail ? (lit ? CLR_BROWNISH_BLACK : CLR_LIGHT_PEACH) : CLR_DARKER_PURPLE);
}
```
*Do it for BOTH 303s* (the face-draw runs per `i`, so this already applies to 303a and 303b independently
— that's the per-303 requirement, for free).

**Decided + FIXED (2026-07-20, `ca66009b`):** the classic 303's saw/square switch IS core, and it was
unreachable in classic (WAVE lived on the DF-extras page; classic locks `kpage=0`). Fix: the bottom stacked
slot is now dual-purpose — the VIEW page-tab in DF, the **SAW/SQR WAVE switch in classic** (classic has no
page to switch, so the slot is free for it). WAVE reachable in both voicings; engine was always fine (classic
never gated `a->wave`). Verified via a scripted DF→CL flip (the SAW toggle appears bottom-right). **Test:** ▶ in the editor, flip each 303's DF light independently; set SUB high
on 303a, flip to classic and back — SUB must still be there (proves the non-destructive flip). **Then:** fold
a one-line note into acidcandy's `de:meta.todo[]` (next to the DRIFT-knob entry) and, if the chassis grew a
new control vocabulary, `candy-style.md`.
