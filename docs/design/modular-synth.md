# Modular Synth Cart — Design Spec

A Eurorack-style control-rate patcher cart. Tiny panels with knobs, buttons, and patch jacks. Curvy cables connect outputs to inputs. The synth engine (studio.h) does the audio; the modular layer is the brain.

> **Engine prerequisite — SHIPPED.** The VOICE module needs *held notes* (`note_on`/`note_off`
> + live `note_pitch`/`note_cutoff`/…), which were the original reason that work got built and
> are now live — see [`held-notes.md`](held-notes.md). With fire-and-forget `note()` the cyan
> filter-CV cable couldn't modulate a *ringing* voice; held notes fix exactly that. So this
> cart is now a "just build it" job — only the open questions at the bottom remain.

## Build plan (in steps)

The order is deliberate: **hardcode the patch in C first, make it editable last.** You get
sound at step 1 and only fight the cable-dragging UI once the engine, drawing, and knobs
already work. Each step compiles, runs, and bakes a screenshot — a real checkpoint.

| Step | Build | You get | Status |
|---|---|---|---|
| **0** | Cart skeleton at 320×200; one `note_on`/`note_off` on the beat | a repeating bleep | ✅ done |
| **1** | The generative chain **hardcoded** (CLOCK→LFO→S&H→QUANT→VOICE), no UI/cables | endless in-key melody + a "scope" readout of each stage | ✅ done — `tools/carts/modrack.c` |
| **2** | Draw the 6 module strips (names, knobs, jacks, LEDs) + proximity-brightness labels | the running engine made visible | ✅ done — `modrack.c` |
| **3** | Make knobs click-drag (BPM, rate, scale, root, cutoff) + hover-to-read | a playable generative instrument (fixed patch) | ✅ done — `modrack.c` |
| **4** | Draw the fixed default patch as bezier cables with a dot pulsing on each gate | picture matches the sound | ✅ done — `modrack.c` |
| **5** | Cables **editable**: drag out→in (type-checked), grab/rewire, right-click clear; engine is now cable-driven (modules read their input jacks) | a real modular you patch yourself | ✅ done — `modrack.c` |
| **6** | SAVE/LOAD (`save_bytes`) + R-reset, EUCLID as a euclidean-drum module (clock in → kick + gate out, live pattern dots), LED/cable juice | a complete, shareable cart — **v1 done** | ✅ done — `modrack.c` |
| **7** | ENV + DRUM modules added; rack went 4×2 (8 bays) | per-note plucks + a real kit | ✅ done |

Steps 1–4 run on a **hardcoded** eval order; step 5 swaps the hardcoded wiring for the
cable-driven `propagate` (`dst.val = src.val`) — that's the conceptual heart, saved for
when everything else already works.

## Patcher rebuild (steps P1–P4) — toward a real node editor

Once the fixed rack felt complete, the next arc turns it into an open patcher you grow.

| Step | Build | Status |
|---|---|---|
| **P1** | **Data-driven modules** — a `ModType` registry (jacks + knobs as data) and `Module` instances (type + position + `param[]`/`state[]`); generic eval/draw via a `switch` per kind. Cables address `(module, jack)`. Same default patch, same look & sound. | ✅ done — `modrack.c` |
| **P2** | Palette sidebar + drag-to-add (spawn an instance) + delete a module (`x` corner; cables reindex) | ✅ done — `modrack.c` |
| **P3** | Zoom/pan endless canvas (`camera_ex` + `mouse_world_*` for all hit-tests) — folded into P2, since the palette needs canvas room. wheel=zoom, drag-empty=pan | ✅ done — `modrack.c` |
| **P4** | Drag-to-move modules around the stage (grab the body → follows cursor, snaps to grid, cables follow) | ✅ done — `modrack.c` |

All P1–P4 shipped. modrack is now a full patcher: data-driven modules, palette + drag-to-add,
delete, pan/zoom + 1:1, 12px grid + snap, variable module sizes, three tiny utilities, per-module
`?` help, and drag-to-move. Plus a `+` cell-grid foundation for future 1×1 modules.

P1 is the gate: modules became data, so adding one is a registry entry and you can have
several of the same kind. The catalog to draw from (SLEW, ATTENUVERTER, LOGIC, SEQ, SCOPE,
KEYBOARD, MIXER, CHORD, …) is in the §"add more and more" notes.

## Screen

**320×200 at 4× scale** (1280×800 window — the engine default, so no `de:settings`
needed). Modules are laid out as a **4×2 grid of bays** (each ~72px wide × 84px tall), using
the **`FONT_SMALL` 4×6 font** for labels so the panels stay compact. The grid is fully
parametrized (`bayx`/`bayy`/`baycx` from `NCOL`/`GW`/`GSP`/`GH`/`GRP`), so reshaping the rack
is a few constant edits. It pairs with the proximity-reveal UI (below): small labels brighten
as the cursor nears them.

> _History: started as a single row of 6 tall vertical strips (the "authentic Eurorack"
> look), then went FONT_SMALL + narrower, then to the 4×2 grid — which fits more bays and
> gives each module a roomier landscape panel for side-by-side knobs._

### Legibility: proximity reveal

At this size you can't print every knob/jack label all the time. Instead, **brightness
tracks cursor distance** — a label is dim/tiny at rest and lights up (and the nearest one
shows a floating name+value readout at full font) as the mouse approaches. One distance
calc per knob; cheap, alive, and the thing that makes 320px strips readable.

```c
int near_col(int kx, int ky) {       // label brightness by cursor distance
    float d = distance(mouse_x(), mouse_y(), kx, ky);
    return d < 12 ? CLR_WHITE : d < 28 ? CLR_LIGHT_GREY
         : d < 50 ? CLR_MEDIUM_GREY : CLR_DARKER_GREY;
}
```

## The rack — modules

**Rack capacity — 8 bays (4×2), ~15–20 in the catalog.** The 4×2 grid holds 8 modules (v1
fills 6, leaving 2 empty "+ add" bays). More rows or columns are a constant edit away; beyond
what fits, the escape hatch is swap-per-slot then scroll. The original single-row sizing note,
kept below for reference: at 320px ~6 vertical strips fit
comfortably (`6 × 48px + gutter ≈ 292`), 7 is tight, 8 is the ugly max. So the rack is a
**fixed ~6 slots on screen**, filled from a **growing catalog of ~15–20 module types**.
When the catalog outgrows the screen the escape hatch is *swap-per-slot* (pick which module
lives in each slot) first, *horizontal scroll* (pan a wider rack) later — both additive, no
rework. For a teaching toy, fewer-but-understood beats a wall of cramped strips.

**Shipped catalog (13 types, drag from the palette):** CLOCK · LFO · S&H · QUANT · VOICE ·
EUCLID · **ENV** · **DRUM** (big 6×7) + tiny utilities **SLEW** (3×5, smooth a CV → glide),
**ATTN** (3×5, scale a CV → depth), **LOGIC** (4×5, AND/OR/XOR two gates → emergent rhythm)
+ **SCOPE** (6×4, draws a cv as a moving trace) and **KEYS** (6×5, playable gate+pitch via
on-screen or computer keys). Each **VOICE** instance now uses its own timbre slot (saw /
square / tri-pad / sine-pad, by order) so several voices layer as lead + pads. The palette
scrolls (wheel over the sidebar); the wheel zooms only over the canvas, toward the cursor.
A **PRESETS dropdown** (top bar) loads ready-made patches — Generative, Acid bass, Beats,
Keys synth, PWM pad, Turing, Grids beat, Marbles, Maths sweep — each a small builder function
that spawns its modules and wires their cables. (e.g. PWM pad = a square VOICE with a slow LFO
→ its `w` input; Maths sweep = MATHS cycling as a slow asymmetric filter sweep over the line.) Each `ModType` carries its
size in 12px cells (`cw×ch`); the smallest are 3×5, and a 1×1 (12×12) is possible for a
bare jack/button. The generative-melody chain (CLOCK→S&H←LFO→QUANT→VOICE) plus a beat:
EUCLID is now a *pure gate source* (no built-in sound) that drives the **DRUM** module (kick/snare/hat trigger
inputs), and **ENV** is an AD envelope (gate in → 0→1→0 CV out) you patch to a filter for
per-note plucks. The rest of the catalog (SLEW, MULT, ATTENUVERTER, SCOPE, DELAY, VOICE B…)
are later additions; each is a ~20-line pure function over the jack graph (see the build plan).

### Famous-module-inspired ideas

The Eurorack cult favorites — mostly **Mutable Instruments** + **Make Noise** — and how they
map onto our control-rate engine (cables carry gate/pitch/cv; sound is made by `note_on`/`hit`):

**Great fits (generative / CV — doable as ~20-line modules):**
- **TURING** (Turing Machine) — a looping shift-register: a *rnd* knob sweeps from a locked
  repeating melody → evolving variations → full chaos. Stepped CV out. *The* cult generative
  module; perfect control-rate fit. **(shipping now)**
- **GRIDS** (Mutable Grids) — drum-pattern generator: *map* morphs the pattern, *fill* sets
  density; outputs kick/snare/hat gates into DRUM. Instant evolving beats. **(shipping now)**
- **MARBLES** (Mutable Marbles) — shaped randomness: *dens* sets how often a random gate fires,
  *sprd* how wide the random CV swings; held & quantizable. **(shipping now)**
- **BRANCHES** (Bernoulli gate) — a gate in, routed to A *or* B by a probability knob (coin flip).
- **MATHS / FUNC** (Make Noise Maths) — the beloved all-in-one rise/fall function gen = envelope
  + LFO + slew + end-of-cycle trigger. We have those split (ENV/LFO/SLEW); Maths bundles them.

**Doable once the timbre work lands (needs the navkit instrument port, [`instrument-engines.md`](instrument-engines.md)):**
- **PLAITS** — a macro-oscillator: one voice, many synthesis *models* (analog/FM/wavetable/bell/
  chord) with timbre + morph knobs. We'd expose navkit engines (organ/Rhodes/Karplus) as a MACRO voice.
- **RINGS** — physical-modeling strings/bells (Karplus/modal) as a voice timbre.

**Out of scope (need audio-rate buffers between modules, which our cables don't carry):**
- **CLOUDS** (granular), reverb / delay / audio FX.

> The widths below are the **old 480px sketch** — kept for the per-module knob/jack
> inventory, but the v1 layout is 320px vertical strips (relayout pending step 2).

```
(old 480px sketch — inventory reference only)
 CLOCK   LFO    S&H   QUANT  VOICE A  VOICE B  DRUMS
  60px   60px   55px   65px    70px     70px    90px
```

| Module | Knobs / buttons | Outputs | Inputs |
|---|---|---|---|
| **CLOCK** | BPM (60–240) | /1 gate, /2 gate, /4 gate | — |
| **LFO** | rate (0.1–8 Hz), wave (sine / tri / sq / ramp) | CV 0..1 | — |
| **S&H** | — | held CV | CV in, clock trigger |
| **QUANT** | scale (maj / min / penta / blues), root (C..B) | MIDI pitch | CV in |
| **VOICE A** | wave, octave, attack, base cutoff | — | gate, pitch, filter CV |
| **VOICE B** | same | — | gate, pitch, filter CV |
| **DRUMS** | — | — | kick, snare, hat, clap triggers |

## Signal types + cable colors

- **green** — gate (0 or 1, acts on rising edge)
- **yellow** — pitch (MIDI note as float)
- **cyan** — CV (0..1, modulates parameters)

Cables draw as a drooping bezier curve. A small bright dot pulses down the cable on each gate or beat. Jacks are 5px circles — hollow = output, filled = input. Type-checked on connect: gate→gate, pitch→pitch, cv→cv only.

## Default patch (wired on first open — instant sound)

```
CLOCK /1 ──────────────────────────────► VOICE A gate
CLOCK /1 ──► S&H clock    S&H out ─────► QUANT in
LFO out ───► S&H in       QUANT out ───► VOICE A pitch
LFO out ────────────────────────────────► VOICE A filter CV
CLOCK /1 ──► DRUMS kick
CLOCK /2 ──► DRUMS snare
CLOCK /4 ──► DRUMS hat
```

Hit play → generative pentatonic melody over a beat, zero patching required.

## Data model

```c
// each jack: screen position, signal type, current value this frame
typedef struct { int x, y, type; float val; } Jack;

// a cable is just two jack addresses
typedef struct { int src_mod, src_jack, dst_mod, dst_jack, color; } Cable;

// each VOICE module owns one held-note handle (its sustaining voice); -1 = silent.
// stash the previous gate level per VOICE too, to detect the rising/falling edges.
int   voice_handle[2] = { -1, -1 };
float voice_gate_prev[2];
```

### Per-frame evaluation order (left-to-right = dependency order)

1. **Propagate** — for each cable: `dst.val = src.val`
2. **CLOCK** — calls `bpm(knob)`, sets /1 /2 /4 gate outputs on beat edges
3. **LFO** — `phase += rate * dt()`, computes sine/tri/sq/ramp → out 0..1
4. **S&H** — on gate rising edge, latches CV input into held value
5. **QUANT** — `degree(scale, oct, (int)(cv * 7)) + root_offset` → MIDI pitch
6. **VOICE** — holds one sustained note per voice (a `note_on` handle, `-1` = silent):
   - gate **rising** edge → `h = note_on(pitch, slot, vol)` (instrument on `slot` carries the wave + ADSR + base filter, set once)
   - **every frame** → `note_pitch(h, pitch)` and `note_cutoff(h, base_cutoff + flt_cv * 2000)` — these sweep the *ringing* voice, which plain `note()` can't do
   - gate **falling** edge → `note_off(h)`; clear the handle

   (`note_res` / `note_filter` / `note_lfo` / `note_glide` are available the same way if a module ever drives them — e.g. a future RES cable or a glide knob.)
7. **DRUMS** — on each trigger rising edge: fire the hardcoded drum hit (`hit()` — drums are one-shot, no handle needed)

## Interaction

| Action | Effect |
|---|---|
| Click output jack | Start dragging a cable |
| Click input jack | Complete connection (type-checked) |
| Click occupied input jack | Disconnect existing cable |
| Knob — click + drag up/down | Adjust value |
| Button (wave, scale, root) | Click to cycle |
| Right-click cable | Delete it |
| SAVE / LOAD buttons | `save_bytes()` / `load_bytes()` — persists patch + all knob values |

## Open questions (decide before coding)

1. **VOICE A vs B instrument slots** — independent ADSR per voice (lead + pad) or shared? Independent is more useful.
2. **Drum sounds** — hardcoded (kick = tuned noise burst, snare = noise, hat = short noise, clap = layered noise)? Or wave knob per row? Hardcoded is faster.
3. **Extra modules** — worth considering:
   - **DELAY** — echoes the gate/pitch signal N beats later (generative echo, very cheap)
   - **CHORD** — takes one pitch input, outputs a chord via `strum()` on gate
   - **ENV** — AD envelope that outputs a 0..1 CV shape on each trigger (for filter sweeps without using the LFO)

## Module ideas — parking lot

Modules discussed but not yet built. Rough priority order.

### CHORD — chord voice
Takes a root pitch + gate, plays a full chord via the `chord()` API. One knob for chord type
(maj/min/dim/aug/maj7/dom7/sus4/power). Strum variant: `strum()` with a delay knob.
Different character from VOICE — instant harmonic texture on a single gate.

### DELAY (signal) — gate/pitch echo
Echoes a **gate or CV signal** N beats later — generative echo, very cheap (a ring buffer of
frames). Distinct from the *audio* echo bus designed in audio-notes §17 (a DELAY module there
is the front-end to the one shared audio bus); this one delays *control* signals, so an echoed
gate can fire a *different* voice. Both can exist; name them apart (DELAY vs ECHO).

### Shipped ledger
**2026-06-04** — SEQ (8-step CV sequencer) · VIBE (audio-rate vibrato via `note_lfo()`) ·
CHANCE (probabilistic gate filter).

**2026-06-05 — six modules, each with a showcase preset:**
- **MACRO** — the Plaits-style modeled voice (§"borrowing" above, instrument-engines §8): eng knob
  picks plk/mlt/fm (engine slots 23–25), har/tmb/mor knobs + h/t/m CV inlets that *add* to
  the knobs. Preset: *Macro voice* (LFO swells FM feedback).
- **XPOSE** — octave shifter for pitch lines (−2..+2 snapped); the only way to the bass
  register, since QUANT's root walks semitones-up within ROOT_OCT. Retune: *Acid bass* now
  runs through XPOSE(−1). Subsumes the parked OFFSET's "transpose a pitch" half.
- **MIX** — was parked here, now shipped *better* than specced: the knobs are
  **attenuverters** (−1..+1) + an `off` knob, so it's also the inverter and the full
  attenuverter the OFFSET entry wanted. Preset: *Mix mod* (LFO wah + ENV pluck → one cutoff).
- **CMP** — the parked COMPARE: cv→gate while in > thr; threshold = pulse width. Preset:
  *Clockless* (two free-running LFOs through two CMPs — a groove with no CLOCK module).
- **DIV** — the parked DIVIDER: every Nth gate (2–16), flashing /N readout. Preset:
  *Polymeter* (/4 kick against /3 snare+bass off one clock).
- **ADSR** — envelope with a real SUSTAIN stage (gate-length-aware, legato retrigger);
  ENV stays as the simple AD. Preset: *ADSR pad* (CMP gates + ADSR into VOICE 'a' — the
  rack's first ambient patch).

With CMP shipped, the signal-conversion matrix is complete (gate→cv ENV/MATHS, cv→pitch
QUANT, cv→gate CMP, pitch→pitch XPOSE, cv→cv SLEW/ATTN/S&H/MIX, gate→gate
LOGIC/CHANCE/EUCLID/DIV). What the rack still can't do is **sound dirty** — that's an
*engine* gap, not a module gap: see audio-notes §17 (drive → soft-clip → echo bus →
detune → crush, plus cart-side swing + darker DRUM voices).

## Existing carts to not duplicate

- `drummachine.c` — 16-step grid already exists; DRUMS module here is just 4 trigger inputs, no internal sequencer
- `omnichord.c` — already has its own drum sequencer; this cart's CLOCK could drive it eventually
- `mariopaint.c` — melodic grid; this cart's QUANT + VOICE replaces that with generative rather than composed melody

## Visual reference — module anatomy

```
┌──────────┐
│  CLOCK   │  ← module name, white on dark panel
│──────────│
│  ┌────┐  │  ← BPM display (3 digits)
│  │120 │  │
│  └────┘  │
│   (O)    │  ← BPM knob
│          │
│──────────│
│ /1 /2 /4 │  ← jack labels
│ ○  ○  ○  │  ← output jacks (hollow circles, colored by type)
└──────────┘
```

Panels: dark grey background, lighter border, white labels. Knobs: small circle with a tick mark showing value. LEDs: 3px dot, lit = bright green/yellow, dark = dim version of same color.
