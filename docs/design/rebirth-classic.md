# Rebirth-classic — the RB-338 homage rack: 2×303 + 808 + 909 in one cart

STATUS: SHIPPED (2026-07-02, one day, design → cart) — **`acidrack.c`, all four increments**: the
320×240 accordion rack (2× diode-ladder 303 + full 909 + full 16-voice 808), banks A–D + 64-bar chain,
per-DEVICE FX ([fx] view per machine: dist + delay send into the one shared unit; MASTER keeps
glue + the PCF lane), CPY/CLR/RND scoped to the open strip, and the SEEDED GENERATOR: an 8-hex
song code deterministically fills all banks + the chain (byte-identical trace+WAV acceptance
test), tap-to-nudge digits, [ ] history, WAV export via the engine's live capture. Same-day
engine work: `FILTER_DIODE` + `tools/filter-spec.js` (audio-notes §25), tb303.c upgraded.
Open tails live in the cart's de:meta todo (touch strokes, per-voice knobs, undo, naming) +
Increment G machine buses (effects-bus-architecture.md). This doc proposed taking the pilot
slot from rebirth-house — it did, and the pilot is done: generate → play → export works.

> The rebirth-classic row of the [`tinyjam-racks.md`](tinyjam-racks.md) rack table, promoted to its
> own design. Maker direction captured here (2026-07-02): **build the cart first, self-contained —
> no `rack.h` until a second rack earns it** (the radio.h second-customer rule); **patterns + song
> chain are v1, not v2** ("so you can make a real longer song"); **320×240 with accordion panels**;
> and **sonic fidelity is the bar** — "if we use a wrong filter we need a better one" — a new
> engine filter is pre-approved if the A/B demands it.

Companion reading: [`tinyjam-racks.md`](tinyjam-racks.md) (the rack program this belongs to; lane
format, export tiers, trademark rule), [`tinyjam-racks-followup.md`](tinyjam-racks-followup.md)
(bias knobs, JSON-diff export, the dancer), [`audio-notes.md`](audio-notes.md) §17/§21/§23 (the 303
audit, the ladder, the Steiner), [`../guides/radio-station-howto.md`](../guides/radio-station-howto.md)
+ `runtime/radio.h` (seed rules the generator follows).

## Why this pilot (the swap argument)

`tinyjam-racks.md` gave the pilot slot to rebirth-house because its generator (house.c) exists and
the seed-code handoff to a shipped station is the pipeline-proving constraint. The counter-case for
classic-first, now research-backed:

- **Zero seed-compat burden.** There is no acid station to trace-match — the "one dangerous part"
  of the house pilot simply doesn't exist here. The generator is written seeded-native from day one.
- **Every voice is already shipped and tuned** (tb303, tr808, tr909 carts) — zero new sound work
  beyond the filter fidelity spike, which upgrades the shipped tb303 cart regardless.
- **Acid is the simplest lane format**: two 303 lanes (note/slide/accent per step), drum trigger
  lanes, harmony that barely moves. Minimal machinery to prove lanes, tap-editing, banks, chain,
  save, WAV export — everything the pilot must prove *except* the radio handoff.
- **It's the purest homage** — the rack that markets itself.

Honest framing: **classic proves the rack chassis; house proves the radio handoff.** Whichever goes
first, the other is customer #2 and triggers the `rack.h` extraction. This doc proposes classic
first.

## 1. Ground truth from the research pass (2026-07-02)

### The machines, as shipped

| Cart | Slots | What ports verbatim | What must change |
|---|---|---|---|
| `tb303.c` | 1 (`SLOT 9`) | voice recipe (~17 lines: saw/square + LP + drive + slapback), slide = `note_glide`+`note_pitch` on the held handle with **no re-trigger**, accent = vol 7 vs 5 + scaled env amount, 70% staccato gate, knob→param maps | all state is file-scope globals with `SLOT` hardcoded → wrap in a struct, instantiate ×2 on two slots. `bpm()`/echo are global engine state → owned by the rack transport, not the machine |
| `tr808.c` | 14 (9..22) | all 16 voice recipes, choke (`instrument_choke`), presets, the offbeat-8th swing (admitted anachronism) | ships COMPLETE in the rack now (16 voices, no curation — see §slot math); grid/knob code merges with the 909's (shared skeleton) |
| `tr909.c` | 13 (9..21) | analog kick/snare/toms + FM-clang metal recipes, stroke family (flam 22ms / drag / ratchet), period-correct even-16th shuffle, metal-filter XY pad, choke | same skeleton merge; slots renumbered |

The 808 and 909 carts are **near-identical skeletons** (same `Pat` struct, `grid[NV][STEPS]`,
three-knob-per-voice system, preset format — every top-level symbol collides). The rack therefore
builds **one drum-panel implementation with two voice tables**, not two copied panels. The 909's
stroke chars (`x`/`f`/`d`/`r`) and `gstroke[][]` are the superset format; the 808 just doesn't use
strokes.

### Slot math (a constraint that dissolved)

**UPDATE (2026-07-09): there is no slot squeeze — the 808 is now the COMPLETE 16-voice kit.** This
section was written against `SOUND_INSTR_SLOTS = 32` (definable 5..31 = 27), but the engine had
*already* been bumped to **48** (definable 5..47 = 43; commit `5db23270`). The "growing the engine
past 32 slots" that this section rejected as unearned had actually shipped for other reasons — we
just didn't know, because `studio.h`'s doc line still said "5..31". So the whole rack fits with room
to spare: 2×303 (2) + 909 (13) + full 808 (16 voices across slots 20..33, sharing the tom/conga and
rim/clave circuits like the hardware) = **~29 of 43**. The 808 now ships every voice, ordered for
acid (always-on kick/hats/clap/snare on top, cowbell + accents next, toms/congas at the bottom).

Original (now-obsolete) plan, kept for the reasoning: *curate the 808 down to ≈9 (BD, SD, CP, CB,
MA, OH, CH + 2 toms), cutting congas ×3 / clave / rimshot / cymbal — the voices acid house least
missed.* Rejected alternative that still holds: re-`instrument()`-ing shared analog slots *per
trigger* (breaks set-and-hold, kills 808+909 kick layering) — which is why the extra voices got
dedicated slots, not a reconfigured pool.

### Effects: ReBirth's rack maps one-to-one onto existing master inserts

| RB-338 effect | Engine call | Note |
|---|---|---|
| Distortion | `drive_insert(amount, mode, mix)` | reorderable `FX_DRIVE` |
| Delay | `echo_insert(time, fb, tone, mix)` | reorderable `FX_ECHO`; tempo-synced times from the transport |
| Compressor | `glue(0, amount, atk, rel)` | groovebox's `apply_fx()` is the reference idiom (PUMP/GLUE share the one master comp) |
| PCF (pattern-controlled filter) | master `filter(mode, hz, res)` | **explicitly ride-safe every frame** — a sequencer lane driving it IS the PCF |

Two rules to honor (both already bit other carts): `fx_order()` **replaces** the chain — every
pedal used must be listed or it silently bypasses; and buffer effects are **set-on-change only**
(copy groovebox's knob-moved gating). Ride-safe set: `filter`/`varispeed`/`note_*`/`fx_mod`.

## 2. The design

### One self-contained cart

No `rack.h`. Machines are structs, not headers: a `Machine303` (voice + 16-step
note/slide/accent/gate data + knobs) instantiated twice, and a `DrumPanel` (grid + stroke data +
voice table + per-voice knobs) instantiated twice with the 808/909 tables. Copy recipes verbatim
from the source carts — the rack embeds *recipes*, never carts.

### 320×240, accordion panels (maker-confirmed)

320×240 is precedented (`moog`/`fingerdrums` ship at it; `sh101` goes 460×300 if we ever need
taller). Layout:

- **Transport bar** (~24px, always visible): play/stop, BPM, shuffle, pattern bank A–D, chain
  toggle, seed/song-code display.
- **Five accordion strips**: 303-A, 303-B, 808, 909, FX/mixer. Folded strip ≈ 22px: name LED,
  **live mini 16-tick playhead**, mute, tiny level meter — folded machines stay visibly *breathing*
  (most of ReBirth's charm). Tap a strip to expand it; **one expanded at a time**.
- **Expanded panel** gets ≈ 240 − 24 − 4×22 ≈ **~128px** (~150px if the FX strip folds into the
  transport bar). The 909's grid needs ~100px at the shipped 9px row pitch — fits. The 303's
  panel needs compacting from its full-screen layout (drop the help overlay, tighter roll).
- **Two invariants**: sound never depends on what's expanded (the sequencer is global; the
  accordion is pure view), and folded ≠ blind (the mini playheads).

### Patterns + song mode: v1 (maker-confirmed)

- **Global pattern snapshots**: one pattern = all machines' 16 steps together. Banks **A–D**
  (format leaves room for A–H free — the bank index is a byte).
- **One chain row** = song mode: up to 64 entries, one pattern per bar ≈ 2 minutes at 130 BPM —
  a real track, and enough for WAV export to mean something.
- The generator fills the banks as the arrangement: A sparse intro, B groove, C build (kick roll),
  D drop — `tinyjam-racks.md`'s "sections become pattern banks," landing in v1.
- Per-machine pattern locks (mix 303-A's pattern 3 with the 909's 1, real-RB-338 style) are a
  compatible later extension: a chain entry grows from 1 index to 4.
- Persistence: the whole song (banks + chain + knobs + fx) via `save_bytes()` — the same blob the
  future `song.h` export tier reads (`tinyjam-racks.md` §Export).

### The seeded acid generator (the genuinely new code)

No station to stay compatible with, so it's written radio-idiom from scratch:
`rad_seed_begin(&rs, seed)` + every compositional draw from the `srnd()` stream in fixed order
(the radio.h seed rules; performance humanize stays on `rnd()`). Ingredients already proven:

- **Musicality** from `tb303.c`'s `gen_random()`: root-heavy minor-pentatonic pool
  `{0,0,0,3,5,7,10,12}`, ~72% gate, ~35% note-repeat, ~30% accent, ~25% slide, lands on the root.
- **Seeded discipline** from `braindance.c`/`squarepusher.c` (both already roll srnd-seeded 16-step
  acid lines cart-side).
- **Extended to the rack**: fill both 303s (call-and-response: B answers A's line thinned/octaved),
  pick drum presets/variations per bank, place the C-bank kick roll and D-bank open-hat lift, and
  write the chain (e.g. `A A B B A B C D · B B …`).

The seed displays as the 8-hex **song code** on the transport bar — shareable, typeable,
reproducible. Acceptance test: same seed → identical trace, the standard radio trace-diff.

## 3. Sonic fidelity — the filter is the product (maker-set bar)

The research verdict on the acid path as shipped: `tb303.c` runs **`FILTER_LOW`, a clean linear
12dB/oct 2-pole Chamberlin SVF** — no in-loop nonlinearity, no bass-thinning, no resonance
compensation (audio-notes §17: the 303 squelch is "the filter driven into saturation, not the
filter" — and our saturation is post-filter only). The real TB-303 is an ~18dB/oct **diode ladder**.
What the engine has today:

- **`FILTER_LADDER`** (`sound_ladder`, sound.h ~4309) — ZDF 4-pole transistor ladder: passband-bass
  drain + self-oscillation near max res (the two signature behaviors `FILTER_LOW` can't do). But
  it's the stripped port: single-rate, **linear** feedback, no calibrated resonance curve.
- **`FILTER_STEINER`** — 2-pole with a diode-style tanh **in the feedback path**; docs already call
  it "great for acid."
- **navkit's ladder was deliberately not ported** (`~/Projects/navkit/soundsystem`,
  `engines/synth.h` ~1323–1470): 2× polyphase-IIR oversampling, OTA Padé-tanh saturation on the
  feedback loop, Juno-calibrated exponential resonance curve, thermal-noise self-oscillation
  seeding, Q/frequency compensation. A proper 303 filter is a **port + diode-topology adaptation**,
  not greenfield DSP. (navkit has no diode ladder either — its model is the Juno IR3109 lineage.)

**The fidelity ladder — RESOLVED (2026-07-02, the spike ran same day as the design):**

1. **A/B ran** via the new **`tools/filter-spec.js`** (fixed-pitch saw probe, Goertzel per
   harmonic vs a `FILTER_OFF` reference — the tool this section asked for, now shipped). Verdict:
   **no pre-existing filter had all three signatures** — the SVF has none (−10 dB/oct, no drain,
   linear), the transistor ladder has drain but is too steep (−22) and linear, the Steiner has the
   slope + loop-tanh but no drain.
2. **`FILTER_DIODE` was built** (`sound_diode` in sound.h, mode 10): ZDF ladder skeleton with a
   stage-3 tap (−16..−17.5 dB/oct measured), tanh in the feedback path, and gentle res makeup.
   Measured: bass drain −4→−8 dB across the res sweep, resonance peak +3.3 dB at res 8 / +10.6 at
   res 12 (rings at usable knob positions where all the others stay flat until ~15). Full table +
   implementation notes: [`audio-notes.md`](audio-notes.md) §25. Deliberately NOT ported yet:
   navkit's 2× oversampling (single-rate is clean at these ranges; the donor is on file if the top
   of the range ever aliases).
3. **`tb303.c` upgraded**: ships on `FILTER_DIODE` via its `ACID_FILTER` define (one line back to
   `FILTER_LOW` for an ear A/B). Musical renders (ACID 1 pattern): SVF −20.5 dB RMS / crest 7.2 ·
   transistor ladder −26.2 / 12.0 (starved) · diode −23.7 / 10.1 (drained but punchy).

`filter-spec.js` is now the standing oracle for any sound.h filter change
([`checks-and-oracles.md`](../guides/checks-and-oracles.md) row added); its §25 table is the
blessed baseline the rack builds on.

Same bar applies down the rack, in order of audibility: the 303 filter (above) ≫ 909 kick attack
click ≫ hat FM stand-ins (already the honest documented compromise for 6-bit ROM samples) ≫ the
rest — already vouched for by their shipped carts.

## 4. Identity & trademark

Roland-flavored homage stays free (gallery/web); nothing Roland-skinned crosses a paywall — the
paid identity gets original naming ([`tinyjam-marketing.md`](../marketing/tinyjam/tinyjam-marketing.md) §2, the `tj-acid3`/`tj-drums9`
convention). Precedent: `moog.c` ships titled **"dream synth."** The rack should follow suit from
day one — original faceplate name, RB-338 in `lineage`/`homage` metadata where it belongs.

## 5. Build order

1. ~~**Filter fidelity spike** (§3)~~ **SHIPPED 2026-07-02**: `filter-spec.js` + `FILTER_DIODE` +
   tb303.c upgraded (audio-notes §25).
2. **The cart, playable**: 320×240 accordion shell; Machine303 ×2 + DrumPanel ×2 with curated
   slots; shared transport; banks A–D + chain row; `save_bytes` persistence. Hand-authored default
   patterns (one per bank) so it's an instrument before it's a generator.
3. **The seeded generator**: banks + chain from a song code; seed on the transport bar; trace-diff
   acceptance test.
4. **Export**: WAV button (arms the `.bake/wav_request` trigger); song-code display/entry. Stems =
   solo-a-strip + re-render, a loop not a feature.
5. Later, with the rack program: per-machine pattern locks, the bias knobs / JSON-diff export /
   dancer from [`tinyjam-racks-followup.md`](tinyjam-racks-followup.md), `rack.h` when customer #2
   arrives.

## Modern-era feature backlog (real-hardware precedents)

Recorded 2026-07-09 from a research pass on what the 303/808/909 *reissues and clones* added over
the decades — the features acid/house/techno players missed on the originals. These live as items in
`acidrack.c`'s `de:meta.todo[]` (surfaced by `cart-todos.js`); this section is the **why + sources**
so the next builder doesn't re-research. Priority order overall: **step probability → per-track
length/polymeter → sidechain pump → the Devilfish 303 knobs → motion/p-lock lanes** (cheapest-and-most-
alive first; the deep one last).

**Already in acidrack (don't re-add):** per-machine DIST (≈ the TD-3's Boss-DS-1 distortion), the
seeded GEN/song-code (≈ the TD-3's random-pattern generator), per-voice TUNE/DEC/CHAR, flam/drag/
ratchet strokes, the METAL hat pad, accent rows, period-correct shuffle, the master PCF lane + GLU
glue-comp, banks + song chain. **Deliberately skipped (moot in one cart):** the clones' MIDI/USB/
CV-gate/DIN-sync/patch-point/PC-editor additions — all about wiring to *other* hardware.

### The 303 (Devil Fish → Behringer TD-3 / TD-3-MO)
The stock 303 is thin; the famous Robin Whittle "Devil Fish" mod (and Behringer's TD-3-MO recreation
of it) added the knobs acid producers actually wanted:
- **Separate accent decay** — the signature mod: a normal-decay knob *plus* an independent accent
  decay, so accented steps ring/breathe differently. acidrack's `DEC` is shared across all steps today.
- **Variable slide time** — a knob, instead of the fixed non-retriggering glide.
- **Soft attack** — rounds the note onset for subtler, less-clicky lines.
- **Sub-oscillator** — a deep sub beneath the saw/square for fatter bass.
- **Accent-sweep modes** — acidrack already routes accent into the filter env; the TD-3-MO makes the
  *sweep speed* selectable (3 modes).

**Song-level transpose** (the design conclusion, not a clone knob): the arrangement feature the stock
303 lacked — every clone workflow gets it via incoming MIDI-note transpose. In acidrack it belongs at
the **song/arrangement layer, not as a 303 knob**: a per-song-cell semitone offset (`int8_t
songTranspose[64]`) added into *both* 303s' `pitches[]` at the **bar boundary** (drums ignore it). It's
the pitch twin of the existing per-bank PCF lane, and applying it at the bar boundary sidesteps the
open slide/tie/shuffle scheduling caveat (no mid-pattern pitch mutation). A continuous *knob* is the
wrong control — musical transposition is discrete and rhythmic. Per-303 override (one line a third/
fifth above) is a later nicety; default shared.

### The 808/909 (Behringer RD-8/RD-9 → Roland TR-8S)
The drum-machine additions are mostly **sequencer/performance**, which is exactly the house/techno
workflow — and three of the four are **not greenfield**, we've shipped the mechanism already:
- **Step probability** — each active step gets a % chance to fire; the modern move for a living,
  never-quite-repeating loop. *Reference:* `pocketbox.c`'s Elektron trigger conditions (`CONDS[]`
  table + `cond_ok()` gate, incl. a 50% + `X:Y` loop conditions); `turing.c` is the pure per-step
  flip-probability engine (`flip_prob()`).
- **Per-track step length / polymeter** (RD-9 "Poly") — a 15-step hat against a 16-step kick drifts
  in and out of phase; long-form techno evolution from one loop. acidrack is locked to `STEPS=16`.
- **Sidechain "pump"** — duck the non-drum buses to the kick for the house/techno breathing. acidrack
  has the GLU glue-comp but no kick-triggered duck; a single PUMP-amount knob keyed off the kick step.
- **Motion / parameter-lock lanes** (TR-8S motion · Elektron p-locks) — automate any per-voice knob
  per-step or by recording knob moves, played as a delta over the base. acidrack already has the
  *concept* via the per-bank PCF lane; generalize to per-voice lanes. *Reference:* `pocketbox.c` has
  literal per-step LOCK 1–4 pages; `motionbox.c` is the continuous motion-record flavor. (This is
  build-order item 5's "per-machine pattern locks", now with sources.) The deepest build — do last.
- **Smaller flavor:** Wave Designer / transient shaping (attack+sustain punch per voice), a dual-mode
  LP/HP recordable filter per machine (acidrack's PCF is a master lowpass), and Scatter/beat-repeat
  glitch fills (acidrack has per-step ratchet but no global performance-glitch).

Sources: [MusicTech — TD-3](https://musictech.com/news/behringer-td-3-release/) ·
[Sound On Sound — TD-3-MO](https://www.soundonsound.com/reviews/behringer-td-3-mo) ·
[MusicTech — TD-3-MO / Devil Fish mods](https://musictech.com/news/gear/behringer-td-3-mo-bass-synth-recreation-roland-tb-303-devil-fish-mod-available/) ·
[Sound On Sound — Mode Machines Xoxbox](https://www.soundonsound.com/reviews/mode-machines-xoxbox) ·
[Behringer RD-9](https://www.behringer.com/en/products/0704-AAB) ·
[Sound On Sound — Behringer RD-8](https://www.soundonsound.com/reviews/behringer-rd-8-rhythm-designer?page=2) ·
[Sound On Sound — Roland TR-8S](https://www.soundonsound.com/reviews/roland-tr-8s).

## Open questions

- **Chain length / structure** — 64 flat entries, or entries with a repeat count (`B×4`)? Flat is
  simpler to edit on a strip; counts read better. Decide at the chain-row UI.
- **808+909 both, always?** ReBirth v2 ran both kits simultaneously; the slot budget allows it.
  But does the *generator* use both per song, or pick one per seed with the other muted?
- **Shuffle scope** — one master shuffle knob (period-correct even-16ths), or per-machine like the
  standalone carts (the 808's offbeat-8th anachronism preserved)? Lean: one master knob.
- **PCF routing → the MACHINE-BUS engine gap (recorded 2026-07-02, maker thinking on it).**
  ReBirth's effects were per-DEVICE (assign buttons per machine); the rack's v2 FX gets
  per-device dist + delay the engine-free way (per-voice `instrument_drive` + the shared echo
  send with per-machine send knobs), but per-device **PCF and comp** genuinely need slots
  grouped onto one bus — every voice's own filter is already taken by its machine recipe, and
  `glue()` targets one bus. The engine sketch (an `instrument_bus(slot, group)` assignment) is
  banked as **Increment G in [`effects-bus-architecture.md`](effects-bus-architecture.md)** —
  it also unlocks per-lane fx for every future rack.
- **303 panel compaction** — does the piano roll survive at ~128px height with touch targets, or
  does the expanded 303 need a reduced flag-row layout (OCT/ACC/SLD as cell states instead of
  separate rows)?
