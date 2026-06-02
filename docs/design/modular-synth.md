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
| **5** | Cables **editable**: drag out→in (type-checked), right-click delete; swap to the cable-driven propagate step | a real modular you patch yourself | — |
| **6** | SAVE/LOAD (`save_bytes`), wire EUCLID→DRUMS, polish LED/cable juice | a complete, shareable cart | — |
| **7+** | Add modules (SLEW=`note_glide`, ENV, MULT, ATTENUVERTER, SCOPE…) — each a ~20-line pure function over the jack graph | the "more and more" growth | — |

Steps 1–4 run on a **hardcoded** eval order; step 5 swaps the hardcoded wiring for the
cable-driven `propagate` (`dst.val = src.val`) — that's the conceptual heart, saved for
when everything else already works.

## Screen

**320×200 at 4× scale** (1280×800 window — the engine default, so no `de:settings`
needed). Modules are **tall, narrow vertical strips** (authentic Eurorack), ~46px wide ×
~150px tall, ~6 across. The cramped width is intentional and pairs with the
proximity-reveal UI (below): permanent labels don't fit, so detail blooms under the cursor.

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

**Rack capacity — 6 visible, ~15–20 in the catalog.** At 320px, ~6 vertical strips fit
comfortably (`6 × 48px + gutter ≈ 292`), 7 is tight, 8 is the ugly max. So the rack is a
**fixed ~6 slots on screen**, filled from a **growing catalog of ~15–20 module types**.
When the catalog outgrows the screen the escape hatch is *swap-per-slot* (pick which module
lives in each slot) first, *horizontal scroll* (pan a wider rack) later — both additive, no
rework. For a teaching toy, fewer-but-understood beats a wall of cramped strips.

**v1 starter set (the 6 slots):** CLOCK · LFO · S&H · QUANT · VOICE · EUCLID — the full
generative-melody chain plus a generative beat. VOICE B, DRUMS, and everything in the
expansion clusters (SLEW, ENV, MULT, ATTENUVERTER, SCOPE, DELAY…) come as later catalog
modules; each is a ~20-line pure function over the jack graph (see the build plan above).

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
