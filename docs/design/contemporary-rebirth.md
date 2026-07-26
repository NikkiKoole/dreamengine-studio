# Contemporary ReBirth — the post-hardware genre rack (techniques, not machines)

STATUS: BUILDING (2026-07-26) — **Rung A shipped the same day** (`multiband()` /
`instrument_multiband()` / `FX_MULTIBAND`, the OTT box: [`audio-notes.md`](audio-notes.md) §17 #34,
recipes in [`../guides/effects-recipes.md`](../guides/effects-recipes.md)). Next: `hyperbox` v1 as its
showcase, then Rung B. The engine audit below is done and precise: of the eight
boxes across the two candidate racks, **six need zero engine work**. Three gaps remain (multiband
squash · a formant/harmoniser dial · a beat-synced buffer re-reader), each specced as a rung with an
API shape. The pick for the first build is the **hyperpop** rack, because its four boxes need no
external audio at all and it forces the two gaps that are pure signal processing. Origin: a
maker + Claude conversation (2026-07-26) starting from "what would a contemporary ReBirth look like,
not for late-90s acid house but for music popular now."

> **The one idea.** ReBirth RB-338 cloned specific *machines* (TB-303, TR-808, TR-909) because in
> 1997 the genre lived in unobtainable boxes. Modern genres were not made on hardware: their identity
> lives in a **workflow** (the glide, the ratcheted hat, the always-on squash, hard tune as an
> instrument). So a contemporary version clones **techniques**. Nobody covets a box any more; they
> covet a move.

The second idea, which is the same one ReBirth had: the appliance's **constraint is the product**.
In 1997 the pitch was "finally affordable". Today plugins are cheap and cracked, so the pitch is
**"finally constrained"** — four boxes, one screen, no menu diving, no arrangement view, a fixed
master chain, and everything you touch already sounds like the genre.

Companion reading: [`genre-box-rosters.md`](genre-box-rosters.md) is the **hardware-era** catalogue
(which real machines to homage, filtered by what the engine can synthesize); this doc is its
post-hardware branch, where the roster is a list of *moves* rather than gear.
[`tinyjam-racks.md`](tinyjam-racks.md) is the rack program (lane format, generate → play → export,
the trademark rule), [`rebirth-classic.md`](rebirth-classic.md) is the RB-338 pilot itself (shipped
as `acidrack` / `acidcandy` — the chassis both racks below would reuse),
[`audio-input-frontier.md`](audio-input-frontier.md) + [`mic-and-sampling.md`](mic-and-sampling.md)
+ [`transparent-autotune.md`](transparent-autotune.md) are the audio-input veins the vocal boxes ride,
[`distortion-lab.md`](distortion-lab.md) holds the adjacent multiband-distortion gap, and
[`../guides/effects-recipes.md`](../guides/effects-recipes.md) is where a shipped effect's settings land.

## 1 · The two candidate racks (as sketched)

Each is four modules plus a locked master chain, in the ReBirth body plan.

### 1a · The hip-hop / trap rack

| Box | The move it clones |
|---|---|
| **SB-808 · bass engine** | the tuned, gliding, driven 808 sub as the *melodic lead*. Knobs: glide, drive, decay. Its sequencer has **per-step slide flags** — the direct descendant of the 303's slide, which is why this maps so cleanly |
| **Hat machine · drums** | rolls as a **first-class step property**, not hand-programmed. The atomic unit is not "on/off" but "this step is a 1/32 triplet burst": one button per step turns it into a ratchet. This is the equivalent of ReBirth putting accent + slide in the sequencer row |
| **Loop box · melody** | the pitched-down soul chop / pluggnb pluck. Deliberately breaks ReBirth's no-sampling rule (its biggest limitation) but stays minimal: pick a category, pitch it, darken it, reverse it. No waveform editing |
| **Ad-lib pad · vocal** | the vocal tag as an *instrument*: five one-shots, hard tune permanently on, one record slot |
| **master chain** | halftime · stutter gate · multiband squash · tape stop. The halftime/stutter punch-in is this generation's pattern-controlled filter: a tempo-locked effect you ride live |

### 1b · The hyperpop rack

| Box | The move it clones |
|---|---|
| **Voice engine** | the vocal chain *is* the lead instrument, sitting where ReBirth put the 303. The tune-speed knob only goes from "hard" to "harder": no natural setting exists. Presets: chipmunk / robot / choir stack |
| **Supersaw box** | seven detuned saws; knobs detune / bright / **toy** (degradation toward crushed 8-bit ringtone, because the genre flirts with cheap MIDI on purpose — a musical parameter, not a mistake) |
| **Blown-out drums** | clip always on; light steps are glitch stutters; the pattern is intentionally too fast at the end |
| **master destruction** | OTT at 100% with **no bypass** · clipper · pitch riser. Everything too loud by design |

The two mirror each other, which is the interesting part: the hip-hop rack's knobs are all about
**groove precision** (glide, ratchets, timing), the hyperpop rack's are all about **signal abuse**
(pitch, saturation, compression). Same four-box format, opposite philosophy of what a knob is for.

## 2 · Engine audit — what exists, exactly

Verdict first: **six of the eight boxes are pure assembly of shipped API.** Every claim below was
checked against `runtime/studio.h` and a proof cart, not from memory.

### 2a · Hip-hop rack

| Box | Shipped API it is built from | Proof cart | Gap |
|---|---|---|---|
| SB-808 bass | `note_on` + `note_glide(handle, ms)` + `note_pitch(handle, float)` (non-retriggering slide, `acid303.h`'s exact model) · `INSTR_SINE` · `instrument_env(ENV_PITCH)` for the click · `instrument_drive` + `instrument_drive_mode(DRIVE_*)` + `note_drive` to ride it · `instrument_filter` · `tr808.h` `TR_BD` for the transient layer · `sidechain()` / `sidechain_key()` for the duck | `tb303`, `acidcandy`, `braindance` (`I_SUB`); 55 carts use `note_glide` | none |
| Hat machine | `schedule_hit(delay_ms, midi, instr, vol, dur_ms)` is **sample-accurate**, so sub-step bursts do not inherit frame jitter · `tr909.h` already ships the stroke family `tr909_fire_stroke(base, v, stroke, …)` with `TR9_ST_FLAM / DRAG / RATCHET` · `tr808.h` hats · `instrument_choke` for open/closed | `tr909`, `acidcandy` | none. "One button makes this step a triplet burst" is cart-side pattern data |
| Loop box | 8 PCM slots · `sample_load()` · `instrument_sample(slot, sample_slot, root_midi)` · `instrument_sample_region(start, end)` (the chop) · `instrument_sample_mode(SAMPLE_REVERSE / LOOP / PINGPONG)` · `instrument_grains_pitch()` (repitch that keeps slice length) · `instrument_eq` / `instrument_filter` for "dark" · `sample_peaks()` to draw it | `breakchop` (tempo-locked chopping, per-pad reverse/speed/tone, runtime loop via `de_data_path()`), `sampler`, `grainchop` | none in the engine. See §5 for the real blocker (where the audio comes from) |
| Ad-lib pad | `mic_start` → `mic_record(seconds)` → `mic_record_read` → `sample_load` → `sample_autotune(slot, root, SCALE_*, amount)` (formant-preserving snap, shipped 2026-07-17), then pads are `note_on` on the sample slots | `mictune`, `voxbox`, `hardtune`, `singsynth` | none. Ceilings: 8 sample slots, 8 s per mic take |
| master · tape stop | `varispeed(0.25..4)`, documented for exactly this dive | `kaoss` | none |
| master · stutter gate | `tremolo(rate_hz, depth, LFO_SHAPE_SQUARE)` with the rate derived from `bpm()`. (`gate()` is a *threshold* gate, a different thing) | `kaoss` GATE program, `breakchop` STUT | none, but see §5 on set-and-hold |
| master · halftime | nothing time-stretches the mix. `varispeed(0.5)` couples pitch and time (it is a tape). `grains()` + `grains_pitch()` can pitch-compensate, which is grainy and not beat-locked | `breakchop`'s TONE proves granular repitch holds duration, but on a slot bus | **GAP 1** |
| master · multiband squash | **`multiband(low, mid, high, up, mix)` — SHIPPED 2026-07-26** (Rung A). Was: `glue()` one band downward-only · `eq()` before/after · `drive_insert(…, DRIVE_HARD, …)` as the clipper | `fxcheck`; `hyperbox` next | closed |

### 2b · Hyperpop rack

| Box | Shipped API it is built from | Proof cart | Gap |
|---|---|---|---|
| Voice engine | `autotune_mic(root, scale, amount)` is **live and formant-preserving**; the tune-speed knob is its `amount` plus a retune slew, which `hardtune` already implements against `mic_pitch()` · `sample_autotune` for takes · chipmunk is free (play a sample above its `root_midi`: pitch and formants rise together) · robot = the vocoder carrier (`vocoder_mic` + `vocoder_unvoiced`) · choir stack = one sample slot triggered at three intervals | `hardtune`, `livetune`, `mictune`, `voxbox` | the **dial between** chipmunk and transparent is missing, as is a fixed-interval harmoniser. **GAP 3** |
| Supersaw box | `instrument_unison(slot, 1..7, detune)` (`SOUND_UNISON_MAX 7`) · `instrument_unison_detune` rides live · `LFO_DETUNE` / `ENV_DETUNE` for the bloom · `instrument_bandlimit` (PolyBLEP) as the honest "bright" · `instrument_crush` as "toy" | `supersaw`, `motionbox` | none. Unison sums **inside one voice**, so a 7-saw wall costs 1 of 32 voices |
| Blown-out drums | `tr808.h` / `tr909.h` / `drumkit.h` · `instrument_crush` · `instrument_drive` · `drive_insert(…, DRIVE_HARD, …)` · `glue()` · ratchets via `schedule_hit` | `tr909`, `morphbox`, `acidcandy` | none |
| master destruction | OTT = **`multiband()`, SHIPPED** (Rung A) · clipper = `drive_insert` + `DRIVE_HARD` (plus `drive_voice(DRIVE_VOICE_TS)` for a pedal character) · pitch riser = `ENV_PITCH` / `note_pitch` ramp or `varispeed` up · "no bypass" is a UI decision, not an engine feature | `distlab`, `pedalboard`, `fxcheck` | closed |

## 3 · The three gaps, as build rungs

### Rung A · GAP 2 — multiband squash (the OTT box) ★ SHIPPED 2026-07-26
Both racks need it, so it has the highest leverage of the three.

- **What is missing:** `glue()` is a single-band, downward-only bus compressor. OTT's signature is
  **three bands** and **upward** compression (the quiet detail gets pushed up, which is what makes a
  hyperpop master sound permanently "on").
- **API as shipped:** `void multiband(float low, float mid, float high, float up, float mix);` plus
  `instrument_multiband(int slot, …)`. Down-amount per band 0..1, one shared `up` amount, `mix` 0..1
  with **0 = bypass, byte-identical**. **Named `multiband`, not `squash`** — `squash` is unusable in
  the cart-land namespace because squash-and-stretch is animation vocabulary, and `build-all` caught
  five carts already declaring a local `squash` (the `map` trap from CLAUDE.md, second instance).
- **Where it landed:** insert kind `FX_MULTIBAND` (18, the next free past `FX_GATE` 17), auto-placed
  on first call, so `fx_order()` can put it before or after the drive stage. It reuses the house
  crossover idiom (`eq_process()`'s one-pole split, at 120 Hz / 2.5 kHz here) and a peak follower per
  band. The bands sum back to the input, which is what makes the bypass claim exact.
- **Two things the first render taught us:** with no output makeup the full wall measured **1.8 dB
  quieter** than dry (backwards for an effect whose entire point is "louder and always on"), so
  makeup scales with the mean down amount (now +5.7 dB, 2% clip); and the upward half has to taper
  out near silence or it amplifies the noise floor.
- **Also unlocks:** the multiband distortion gap already parked in
  [`distortion-lab.md`](distortion-lab.md) §Multiband — same crossover, different per-band stage.
- **Gates run (all green):** `soundcheck` silent · `fx-check` shows every *other* effect at Δpk/Δrms
  +0.0 (the byte-identical proof) and the new case finite/bounded/off-dry · `level-check` within
  tolerance · `soak-check` stable · `web-audio-check` wasm parity · `build-all` 566/566 ·
  `lint-fx-frame` clean (it is ride-safe, so it stays out of the footgun set). Recipes +
  §17 #34 ledger entry written.

### Rung B · GAP 3 — the formant / harmoniser dial
- **What is missing:** the two *ends* ship (chipmunk = formants follow the pitch, because a sample
  played off-root moves both; transparent = `sample_autotune`, which holds formants), but there is no
  dial between them, and no way to ask for a **fixed interval** rather than a scale snap.
- **API shape:** `void sample_formant(int slot, float semitones);` for a take (offline, in place,
  next to `sample_autotune`), and `void harmonize_mic(float semitones, int voices, float formant);`
  for the live chain (`voices` 1..3 = the choir stack).
- **Why it is cheap:** this is a **re-facing of DSP that already shipped** with
  [`transparent-autotune.md`](transparent-autotune.md), not new DSP. The formant-preserving shifter
  exists in both offline and streaming form; today it is only reachable through a scale-snap face.
- **Gates:** `formant-check.js` is exactly the oracle for this (f0 moved AND F1/F2/F3 held), plus
  `soundcheck`.

### Rung C · GAP 1 — a beat-synced buffer re-reader (halftime / beat repeat)
- **What is missing:** any way to manipulate *time* on the master without dragging pitch along.
  `varispeed` is a tape; `grains` is a texture.
- **API shape:** `void beatfx(int mode, float bars, float mix);` where mode is
  HALFTIME / REPEAT / REVERSE / SCRATCH, over a captured trailing buffer of `bars`. The Gross Beat
  model is a *curve over a rolling buffer*, so a later `beatfx_curve()` is the natural extension.
- **Hard requirement:** it must be **ride-safe**. `kaoss` found that the buffer-rebuilding effects
  glitch when swept, so the parameters have to be read per sample, never re-allocated per call, or it
  lands on the wrong side of the set-and-hold rule (`lint-fx-frame.js`).
- **Cost:** the biggest of the three (a new buffered insert). Do it when the hip-hop rack is next.

## 4 · Ceilings to design a four-box rack against

Checked, not assumed: 32 voices (`SOUND_VOICES`) · instrument slots 5..47 · **8** PCM sample slots ·
8 FX buses (master + 7 per-instrument) · **2** granular tanks (master + one instrument bus) · reverb
tanks 0..2 · two instances per master FX kind (`FX_INST`) · mic takes up to 8 s · unison is summed
inside one voice (a 7-saw stack costs one voice, not seven).

A four-box rack with ratcheted hats, a unison wall and two sample voices fits comfortably.

## 5 · The non-engine blockers (the ones that actually bite)

- **Where the loop-box audio comes from is the hard part of the hip-hop rack, not the DSP.** Three
  legal sources exist: engine-synthesised (`record_arm` + `record_grab`, which is also the only
  replay-deterministic one), a runtime `--data` file (`de_data_path()`, `breakchop`'s path), or the
  mic. There is no baked-into-the-cart audio, deliberately. `breakchop` already carries a RELEASE
  GATE todo about its copyrighted dev fixture; a "soul chop" module inherits that gate exactly.
- **The sequencer is cart-owned by design** ("cart owns the PATTERN, header owns the SOUND"), so
  per-step slide flags, ratchet zones and the pattern bank are hand-rolled in the cart, as
  `acidcandy` does. No engine work, but it is where the cart's line count goes.
- **Set-and-hold.** Every master effect here (`tremolo`, `crush`, `eq`, `tape`, and the new `squash`)
  must be re-applied **only on change**. Wiring a knob straight into `draw()` rebuilds the bus DSP
  60×/s and stutters silently. Copy `groovebox`'s `apply_fx()`; `lint-fx-frame.js` enforces it.
- **`autotune_mic` / `vocoder_mic` / `input_monitor` are LIVE** and break `.rec` replay (ADR-0032),
  so a vocal box cannot be demoed by a committed input track. Its clip has to be captured live.

## 6 · The pick: build the hyperpop rack first

Both racks need two gaps and they share Rung A, so the tiebreakers are these:

- The hyperpop rack's other three boxes are **100% engine-complete** and need **no external audio at
  all**: supersaw, drums and master are pure synthesis. So the cart is small and self-contained.
- It forces the two gaps that are **pure signal processing** (Rung A, Rung B). The hip-hop rack's
  distinctive gap (Rung C, the buffer re-reader) is the biggest build of the three and its loop box
  drags in the sample-source question above.
- Its aesthetic *is* the master chain, which means the rack doubles as the showcase cart for the new
  `squash()` — the "prove the voice as its own cart first" rule from
  [`radiophonic-workshop.md`](radiophonic-workshop.md), satisfied for free.

**Tiny cart: `hyperbox`** (name free; the `*box` family — `voxbox` / `morphbox` / `fmbox` /
`motionbox`). Scope for v1, deliberately small:

- **Supersaw box** — one `INSTR_SAW` slot, `instrument_unison(slot, 7, detune)`, three knobs
  (detune / bright / toy) mapped to `instrument_unison_detune` / `instrument_filter` cutoff +
  `instrument_bandlimit` / `instrument_crush`. Playable from a short keybed or a held chord.
- **Blown-out drums** — one 16-step kick lane on `tr909.h` with light steps as `schedule_hit`
  ratchets, `instrument_drive` + `instrument_crush` fixed hot.
- **Voice engine** — deferred to v2 (it is the one part needing Rung B and the mic). v1 puts
  `INSTR_VOICE` in the slot instead: deterministic, no permission prompt, and its SIZE macro is
  already a vocal-tract/formant axis, so the box reads correctly while Rung B is built.
- **master destruction** — `multiband()` at 100% with **no bypass control drawn**, `drive_insert` +
  `DRIVE_HARD` always on, and a pitch riser on a held note. One `apply_fx()`-style change-detector,
  per §5.

Build order: **Rung A → `hyperbox` v1** (which is Rung A's showcase) **→ Rung B → `hyperbox` v2's
voice engine → Rung C → the hip-hop rack.**

## 7 · Open questions

- **Does a post-hardware rack still get a seeded generator?** The tinyjam magic is generate → play →
  export ([`tinyjam-racks.md`](tinyjam-racks.md)). A hyperpop generator is a real design question:
  the genre's "correctness" is partly *mistakes*, so the generator would need to deliberately
  overshoot (too fast, too loud, too pitched).
- **Faceplate + naming.** Homage names are free, original faceplate for anything paid
  ([`tinyjam-racks.md`](tinyjam-racks.md) §trademark). "SB-808" in the sketch is close to a live
  trademark; the hip-hop rack needs its own name before it ships.
- **Is "no bypass" honest or hostile?** The sketch locks OTT and the clipper on. That is the
  constraint-as-feature thesis, but it also means a cart where a knob does nothing. Precedent to
  follow: `acidcandy`'s always-on machine FX.
- **Where does the AI-prompt answer sit?** The conversation raised a darker candidate: if the
  defining instrument of current pop is a generative model, the contemporary ReBirth is a text box.
  This repo's answer is the opposite bet (an appliance you *play*), which is worth stating out loud
  in the cart's own `de:meta.lineage` rather than leaving implicit.
