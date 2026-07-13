# Pocketbox — reimagining the Wee Noise Makers PGB-1: the buttons-only pocket groovebox

STATUS: SHIPPED v1 (2026-07-02, design → cart the same day) — `pocketbox` is on the shelf:
phases 1–7 landed in one pass (chassis/OLED/modes, all 8 tracks + engine menus, p-locks/ratchets/
conditions, chord track + CHD modes, finger-drawn wave tracks, patterns + link/polymeter, FX buses
+ live FX, 12-part song mode, autosave + demo groove). The engine verdict held: **zero new engine
features were needed**. Same-day follow-ups: a **legibility pass** (see the verdict below) and
**ROLL-a-groove** (the seeded generator from the open questions — harmony-first: chords, then
vocabulary drums, then chord-degree bass/lead; the 8-hex code shows on the OLED, though typing a
code back in is still open). Deferred to the cart's `de:meta.todo[]`: seed entry, per-track arp
page, WAV-export button, mixer-meters page, step/track copy, `spec()`.

**First-play verdict (maker, 2026-07-02): the naked modal grammar was ILLEGIBLE** — "I don't
understand the paradigm." The faithful port shipped the PGB-1's one-keypad-many-meanings grammar
with no printed faceplate (hardware's labels) and no manual (hardware's onboarding), so the mode
keys read as arbitrary. Fixed same day with a **legibility pass**: per-mode key labels (track
names / P1–16 / part numbers / note names / FX names — the screen doing what hardware printing
does) + an always-on hint line stating what the 16 keys and the strip mean right now. The data
point for the rack program, paired with yachtrack's: **knob-forest = too indirect (yacht-rack),
unlabeled modal grammar = too opaque (this, v1.0)** — the target is modal depth with the meaning
always visible. If the labeled version still doesn't click, the fallback is the §2 hybrid
(always-visible step grid, modes only for depth).

> The maker's framing: "between an instrument and a rack." Exactly right — this is the missing
> middle of the shelf: the **player-authored** groovebox (tracker's authorship) with the
> **immediacy** of a hardware box (the racks' directness), and no generator required to be fun.

Companion reading: [`tinyjam-racks.md`](tinyjam-racks.md) (the rack program this sits beside —
this cart is rack-*adjacent*, not a rack: no radio generator upstream),
[`rebirth-classic.md`](rebirth-classic.md) + [`yacht-rack.md`](yacht-rack.md) (the two shipped
racks whose chassis lessons apply), [`tracker-cart.md`](tracker-cart.md) (the other
player-authored sequencer; this cart is its performance-first sibling),
[`../guides/instrument-carts.md`](../guides/instrument-carts.md) (the shelf),
[`../guides/instrument-recipes.md`](../guides/instrument-recipes.md) (the 107-recipe palette the
engine menus shop from), [`audio-notes.md`](audio-notes.md) (engine surface),
[`touch-notes.md`](touch-notes.md) (touch-first sizing).

## 1. The source device (research pass, 2026-07-02)

The **PGB-1 "Pocket Groove Box"** by **Wee Noise Makers** (Fabien Chouteau, one-person company) —
a $299, 10×6.6 cm, fully **open-source** groovebox (firmware in **Ada**, GPL-3.0; synthesis
library `tresses` is an Ada translation of Mutable Instruments **Braids** — macro-oscillator
lineage, *not* Plaits). RP2040: core 0 runs sequencer + UI, core 1 is all synthesis; the two
halves deliberately talk **only via MIDI messages** internally.

The surface, and the thing to steal, is its interaction grammar:

- **30 buttons, NO knobs** — "program it like a retro gaming console": 16 step keys, 4 mode keys
  (**Track / Step / Pattern / Song**), 4 control keys (**Menu / Play / Edit / Cpy-FX**),
  4 arrows + **A** (yes) + **B** (no/back).
- **One capacitive touch strip** along the bottom — the only continuous controller: value slides
  + live performance rides.
- **128×64 mono OLED** with rock-solid conventions: persistent header (track name left, pattern +
  step top-right), a **dot page-indicator** (←/→ pages, ↑/↓ values), one parameter at a time,
  graphical pages (LFO shape viz, mixer with peak meters, a **waveform-drawing screen**).
- **Everything else is hold-combos**: hold Edit = chromatic keyboard on the 16 keys; hold Play +
  key = mute; hold Cpy/FX + key = 16 live-FX toggles; hold Cpy/FX + strip = ride a parameter; one
  copy idiom (hold Cpy + mode + source + destination) copies steps/patterns/tracks/parts.

Sound: **8 mono voices** = 6 synth tracks with fixed roles (**Kick, Snare, Hihat, Bass, Lead,
Chords**) + 2 sampler tracks (62 s shared memory; mic/line-in — **no FM radio**, that's a PO/OP-1
memory). ~19+ engines grown per firmware release (808 bass, **Acid** ladder 303, phase-distortion
"PDR", chip pluck, glide, custom-waveform everything; the v1.3 hi-hat alone has **24 variations**
— 909/707/808/505/LM2/CR78/MRK2/acoustic × LP/BP/HP). Per track: 4 engine params + **1 LFO**
(rate/amp/shape/target) + volume/pan + FX-bus pick. Effects: **4 global parallel buses — Bypass,
Overdrive, Reverb, Bitcrusher** — each track routes to exactly one. Post-master **Live FX**:
sweepable LP/HP filter, stutter, rolls, fills on the 16 keys.

Sequencer: **16 tracks × 16 patterns × 16 steps**; per-pattern length; patterns **link** into up
to 256-step sequences; pattern choice is **per-track independent** (polymeter for free).
Per-step: note (fixed / **chord degree 1–4** / arp / full chord), octave, velocity, duration,
**ratchets** (count + rate), **Elektron-style trigger conditions** (never/25/50/75%/always/fill/
X:Y loop-ratio), and **parameter locks** (any engine param per step; on sampler tracks a p-lock
picks a different *sample* per step). A chord-progression sequencer feeds the Chords track, and
melodic steps reference "chord note 1–4" so bass/lead follow the progression automatically.
16 arpeggiators (per track: up/down/random/order). **No micro-timing** — feel comes from
per-track shuffle. Song mode: **12 parts**.

Sources: [product page](https://weenoisemakers.com/pgb-1/) · [user
manual](https://weenoisemakers.com/pgb-1-user-manual/) · [firmware
(Ada)](https://github.com/wee-noise-makers/WNM-PGB1-firmware) ·
[tresses](https://github.com/wee-noise-makers/tresses) · gearnews / Synth Anatomy / Synthtopia
coverage. Unconfirmed bits flagged in the research: the full canonical engine list (manual page
is WIP), LED count 24 vs 25.

## 2. The reimagining thesis

**Keep the grammar, translate the guts.** What makes the PGB-1 lovable is not its DSP (we have
more engines than it does) — it's the *discipline*: a whole groovebox operated through 16 step
keys, 4 modes, hold-combos, one strip, and a tiny honest screen. That discipline is deeply
fantasy-console — it's PICO-8-as-hardware — and it maps 1:1 onto a 320×200 touch cart.

It is also a live answer to an open problem: the maker's first-play verdict on `yachtrack` was
that the knob-heavy rack surface is **"a tad too indirect"** ([tinyjam-racks](tinyjam-racks.md)
STATUS). The PGB-1 grammar is the opposite pole — no knob forest, everything two presses deep,
muscle memory over mouse hunting. If this cart's grammar feels right, it's a candidate direction
for rack #3+.

What we keep (the soul):

1. **Buttons-only + one strip.** The faceplate IS the instrument: 16 step keys, Track/Step/
   Pattern/Song modes, Menu/Play/Edit/FX, arrows + A/B, a full-width touch ribbon. No `ui_knob`
   anywhere on the panel.
2. **The OLED, drawn honestly.** A real 128×64 window on the faceplate, rendered 1:1, white-ish
   on near-black, `FONT_TINY`/`FONT_SMALL`. Persistent header, dot page-indicator, ←/→ pages,
   ↑/↓ or strip for values, LFO viz, mixer meters, the waveform-draw page.
3. **Fixed track roles** — Kick/Snare/Hat/Bass/Lead/Chords + 2 wave tracks — each with a curated
   engine *menu* (shopped from [`instrument-recipes.md`](../guides/instrument-recipes.md)), not a
   free-for-all. The PGB-1's "24 hi-hat variations" becomes our 808/909/CR78 hat recipe shelf
   behind one ENGINE param.
4. **Per-step depth**: p-locks, ratchets, trigger conditions (incl. X:Y), velocity, duration,
   note-as-chord-degree. This is what makes it a *groovebox* and not a drum machine.
5. **The chord track feeding melodic tracks** — the progression sequencer + "chord note 1–4"
   note mode. (Our shelf has nothing like it outside the auto-radios; this is the feature that
   makes basslines follow the song by construction.)
6. **The 4-parallel-FX-bus model** — Bypass / Overdrive / Reverb / Bitcrush, one pick per track.
7. **Custom waveform drawing** — the PGB-1's beloved v1.2 feature is *literally* our
   `wave_set()` + `INSTR_USER0..3`: an OLED page where you draw a single cycle with the strip.
8. **Hold-combo performance**: mute/solo on Play+key, live FX on FX+key, the one copy idiom.

What we translate:

- **The 2 sampler tracks → 2 WAVE tracks.** The engine deliberately does no sample playback
  ([`recorded-timbres.md`](recorded-timbres.md); the braindance brief names sampler/granular as
  the known gap — a separate decision we do NOT reopen here). The honest reimagining: wave
  tracks play the four *drawn* single-cycle waves, and the PGB-1's "p-lock a different sample
  per step" becomes **p-lock a different USER wave per step**. Sampling-from-the-mic becomes
  drawing-with-your-finger — more fantasy-console, not less.
- **MIDI tracks (12–16) → cut.** Nothing external to drive. (MIDI *in* already plays keybeds;
  out is the product parking lot, [`product-notes.md`](product-notes.md).)
- **3 sequenceable FX tracks → one FX lane** (v1.5): a per-step Live-FX automation row, exactly
  `acidrack`'s pattern-controlled-filter lane generalized.
- **Battery/mic/speaker/USB** → not applicable; the *aesthetic* (retro handheld, LED language)
  stays.

## 3. Engine verdict: zero new features needed

Every PGB-1 capability maps onto shipped engine surface (all signatures verified via
`tools/api.js`, 2026-07-02):

| PGB-1 feature | Engine surface | Notes |
|---|---|---|
| 6 synth voices, fixed roles | `instrument()` slots + `schedule_hit()` | 8 slots is nothing (acidrack runs ~24) |
| Braids-style engine menu | `INSTR_*` family (13 modeled + chip waves + PD + FM) | we exceed its range; "Acid" = `FILTER_DIODE` per `tb303` |
| Custom-waveform engines | `wave_set()` + `INSTR_USER0..3` | LIVE — a ringing note morphs as you redraw |
| 1 LFO/track (rate/amp/shape/target) | `instrument_lfo()` — **3 per slot**, 8 dests, `lfo_shape()` incl. S&H | we exceed it |
| 4 FX buses: bypass/drive/reverb/crush | `instrument_drive()` / `instrument_reverb()` / `instrument_crush()` per slot | exact 1:1; "one bus per track" is just UI discipline |
| Live FX: LP/HP sweep, stutter | master `filter()` (ride-safe by design) + cart-side retrigger; `varispeed()` for the tape-dive | `kaoss` is the precedent for the ride-safe set |
| Ratchets | `schedule_hit()` sub-hits at step subdivisions | `braindance` precedent |
| Trigger conditions, p-locks, chord degrees | pure cart-side sequencer data | no engine involvement |
| Per-track shuffle | `schedule_hit()` millisecond offsets | `acidrack` swing precedent |
| Velocity / duration per step | `schedule_hit(delay, midi, instr, vol, dur_ms)` | already in the one call |
| Mixer meters page | cart-side level proxy (`combo`'s VU trick) | honest-enough; no engine tap needed |
| Song parts, pattern link, save | cart-side data + `save_bytes()` | `acidrack`/`tracker` precedents |
| WAV export | `.bake/wav_request` | shipped harness path |

The **only** genuinely missing thing is real sample playback (mic/line-in), and that is a
standing deliberate non-goal we're not reopening — the wave-track translation above is the
design answer. If a future decision ever adds a one-shot sample buffer, this cart's sampler
tracks become its first customer, but **nothing here waits on it**.

## 4. The cart spec

**Working name: `pocketbox`** (title "pocket groove box"). `de:meta` carries
`homage: "Wee Noise Makers PGB-1"` — extra in-spirit since the PGB-1 itself is open source
(GPL firmware / MIT tresses). Trademark rule as always
([`product-notes.md`](product-notes.md)): a free homage with credit is fine; if it ever crosses
a paywall it gets an original identity. Final name is a maker call (open question §7).

### Layout (320×200, touch-first)

```
┌──────────────────────────────────────────────────┐
│  MENU  ┌────────128×64 OLED────────┐   ▲          │
│  TRACK │ KICK          P03  ST 05 │ ◀ A ▶   B    │  mode keys left,
│  STEP  │  · · ● · ·   (page dots) │   ▼          │  arrows+A/B right
│  PATT  │  ENGINE: 909 KICK        │              │
│  SONG  └──────────────────────────┘  PLAY  EDIT FX│
│  ┌──┬──┬──┬──┬──┬──┬──┬──┐  steps 1–8  (row 1)   │
│  ├──┼──┼──┼──┼──┼──┼──┼──┤  steps 9–16 (row 2)   │  40×36px keys —
│  └──┴──┴──┴──┴──┴──┴──┴──┘  4-step group shading │  fat-finger clean
│ ═══════════ touch strip (full width) ═══════════ │  ~26px ribbon
└──────────────────────────────────────────────────┘
```

Step keys light like the hardware LEDs: dim = off, lit = step on, bright sweep = playhead,
color = the LED language (selected pattern blue, linked cyan, playing green-blink). Every button
is `ui.h`; the strip and hold-combos ride `pointer.h` per-finger capture (hold FX with one
finger, slide the strip with another — the two-hand grammar must work on touch).

Desktop mapping: number row `1–8` = steps 1–8, `Q–I` = steps 9–16 (mirrors the two on-screen
rows); Space = Play; arrows = arrows; Enter = A, Backspace = B; hold-combos via held keys. In
**keyboard mode** (hold Edit) the note keys are the **GarageBand map** as always (`A W S E D…`,
Z/X octave) — the 16-step-keys-as-chromatic-keyboard idea stays for touch, where it shines.

### Data model (the load-bearing struct)

```c
enum { T_KICK, T_SNARE, T_HAT, T_BASS, T_LEAD, T_CHORD, T_WAVE1, T_WAVE2, NTRACK };
enum { NSTEP = 16, NPAT = 16, NPART = 12 };

typedef struct {                    // one step
    unsigned char on;
    unsigned char nmode;            // NM_FIXED / NM_CHD1..4 / NM_ARP / NM_CHORD
    unsigned char note, oct;        // meaning depends on nmode
    unsigned char vel, dur;         // strip-set; dur in step ticks
    unsigned char rep, reprate;     // ratchet count + rate
    unsigned char cond;             // ALWAYS/25/50/75/NEVER/FILL/ X:Y table index
    signed char   lock[4];          // p-locks on the 4 engine params (-1 = none)
    signed char   lockwave;         // wave tracks: USER wave per step (-1 = track default)
} Step;

typedef struct { Step st[NSTEP]; unsigned char len, link; } Pattern;   // per-len + link flag

typedef struct {
    unsigned char engine;           // index into this ROLE's curated menu
    float         p[4];             // the 4 macro params (enum-named per role — house rule)
    unsigned char fxbus;            // BUS_DRY / BUS_DRIVE / BUS_VERB / BUS_CRUSH
    unsigned char lfo_dest, lfo_shape; float lfo_rate, lfo_depth;
    float         vol, pan;
    unsigned char oct, shuffle, arp;
    unsigned char pat;              // CURRENT pattern slot — per-track ⇒ polymeter
    Pattern       bank[NPAT];
} Track;

typedef struct { unsigned char pat[NTRACK], reps; } Part;   // song mode: 12 parts
```

The chord track's patterns store chord IDs (root pc + quality from a small vocabulary) instead
of notes; at play time melodic steps in `NM_CHD1..4` resolve against the sounding chord.
Playback is the dumb loop the racks proved: read the lanes, `schedule_hit()` a few ms ahead
(sample-tight on web via the automatic worklet build), apply p-locks by setting the slot macros
just before the hit (per-step-rate setter calls are the `acidrack` mini-knob pattern — the
fx-lint rule is about *bus* reconfig, which stays out of the step path; `filter()`/`varispeed()`
are ride-safe by design).

### The engine menus (shop the palette, don't invent)

Per role, curated from [`instrument-recipes.md`](../guides/instrument-recipes.md) — each entry
is one recipe + 4 macro params:

- **KICK**: 909 / 808 / sine-click / `INSTR_MEMBRANE` thump.
- **SNARE**: 909 / 808 / clap / noise-snare.
- **HAT**: the recipe shelf's 808/909/CR78 hats + `INSTR_FM` clang (the PGB-1's 24-variation
  move: variations behind param 1).
- **BASS**: acid `INSTR_SAW`+`FILTER_DIODE` (the `tb303` recipe) / `INSTR_PD` CZ / `INSTR_FM`
  DX bass / 808-bass sine.
- **LEAD**: `INSTR_PD` (its "PDR" engines are phase distortion — direct hit) / chip pluck
  (`INSTR_PLUCK` / square 25%) / glide lead (`note_glide`) / `INSTR_FM` bell.
- **CHORD**: stacked-hit `INSTR_SAW` pad / `INSTR_EPIANO` / `INSTR_ORGAN` / drawn-SCW pad.
- **WAVE1/2**: `INSTR_USER0..3` + the draw page; per-step `lockwave`.

### Live FX (hold FX + steps) — v1 set of 8, not 16

Master `filter()` LP sweep · HP sweep · `varispeed()` tape-dive · stutter (retrigger the current
step's hits at 32nd rate, cart-side) · FILL (force fill-conditions true while held) · `crush()`
master toggle · `tremolo` · roll (ratchet-all while held). Hold FX + strip rides the last-touched
FX's depth. All from `kaoss`'s ride-safe set + cart-side tricks — nothing set-and-hold gets
called per frame.

## 5. Build order (each phase ends playable)

1. **Chassis + drums.** Faceplate, step keys, strip, OLED framework (header / page dots / param
   pages), Track+Step modes, KICK/SNARE/HAT with 2 engines each, play/stop, `save_bytes`
   autosave. *The grammar test: does hold-Edit / hold-Play feel right on touch?*
2. **Melodic + depth.** BASS/LEAD, hold-Edit keyboard, velocity/duration via strip, p-locks,
   ratchets, trigger conditions.
3. **Chords.** The chord track + progression page, `NM_CHD1..4` note modes, per-track arps.
4. **Waves.** WAVE1/2 tracks + the OLED waveform-draw page (`wave_set` live), `lockwave`
   p-locks.
5. **Patterns.** 16 slots, per-pattern length, link, per-track independent slots (polymeter),
   the copy idiom, Pattern mode UI.
6. **FX + performance.** Bus picks, Live FX layer, mute/solo, mixer page with meters.
7. **Song + export.** 12 parts, Song mode, `.bake/wav_request` WAV export button.
8. **Ship**: `de:meta` (kind instrument, homage line), bake, shelf row in
   [`instrument-carts.md`](../guides/instrument-carts.md), recipes into the palette doc, a
   committed demo clip (`tools/clips/pocketbox/`), and a **`spec()`** — the sequencer brain
   (condition table, chord resolution, pattern advance, part chaining) is exactly the
   deterministic logic the spec harness exists for ([`spec-harness.md`](spec-harness.md)).

## 6. What this cart is NOT (scope fences)

- **Not a rack** — no radio generator upstream, no seed-code compatibility burden. (A "roll me a
  starter groove" button is a cheap v2 *idea*, but the identity is player-authored; see open
  questions.)
- **Not a sampler** — no reopening of the sample-playback non-goal.
- **Not a tracker replacement** — `tracker` is document-first (phrases/chains, deep arrangement);
  pocketbox is performance-first (16 steps under your fingers, mute/FX in the other hand). They
  bracket the authorship space from opposite ends.
- **Not `rack.h`'s trigger** — its modal grammar shares idiom, not code, with the accordion
  racks; the third *rack* still decides that extraction.

## 7. Open questions

- **The name** — `pocketbox`? Something wee-er (`weebox`)? Maker call; the homage credit line is
  non-negotiable either way.
- **Screen dims** — 320×200 sketched above; `acidrack` chose 320×240. Decide at phase 1 when the
  OLED + two key rows are real.
- **Generator or blank?** Ship blank + a good demo save (the PGB-1 way), or add a seeded "roll"
  from day one (the tinyjam way)? Leaning blank-first — it keeps the identity clean and the
  demo save teaches better than a random roll.
- **The FX lane** (PGB-1 tracks 9–11) — v1.5 or cut? `acidrack`'s PCF lane says it's cheap and
  fun; parked until phase 6 shows whether Live FX alone covers it.
- **Pattern count** — 16 patterns × 8 tracks × 16 steps ≈ a ~20 KB save blob; fine for
  `save_bytes`, but confirm against the web save budget at phase 5.
- **Co-op jam** — the two-hand grammar (one player on steps, one riding FX/strip) is the
  lockstep-netplay spark from [`tinyjam-racks.md`](tinyjam-racks.md) §Spark wearing hardware
  clothes. Parked, but the input design shouldn't preclude it.

## Related

- [`device-face-paradigm.md`](device-face-paradigm.md) §1e works pocketbox as its **hardware-faithful**
  case: the completest device-face in the repo, and the worked example of the paradigm *fixing* a hardware
  clone's UX pain (the 14-page paramscroll, no knobs, the invisible hold-combos — exactly the "illegible
  modal grammar" the first-play verdict above named) **without touching the sequencer brain**. The rule it
  taught: *hardware charm is a look you can keep; its limits are not one you have to inherit.* pocketbox also
  ships the paradigm's true hold-a-step parameter-lock gesture (hold a step key + strip), which that doc's §5
  had listed as open.
