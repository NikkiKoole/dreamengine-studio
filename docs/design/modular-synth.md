# Modular Synth Cart — Design Spec

A Eurorack-style control-rate patcher cart. Tiny panels with knobs, buttons, and patch jacks. Curvy cables connect outputs to inputs. The synth engine (studio.h) does the audio; the modular layer is the brain.

## Screen

**480×270 at 2× scale** (960×540 window). The `de:settings` chunk enforces this on load.

## The rack — 7 modules

```
480px total, ~10px gutters, modules left-to-right in dependency order

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
```

### Per-frame evaluation order (left-to-right = dependency order)

1. **Propagate** — for each cable: `dst.val = src.val`
2. **CLOCK** — calls `bpm(knob)`, sets /1 /2 /4 gate outputs on beat edges
3. **LFO** — `phase += rate * dt()`, computes sine/tri/sq/ramp → out 0..1
4. **S&H** — on gate rising edge, latches CV input into held value
5. **QUANT** — `degree(scale, oct, (int)(cv * 7)) + root_offset` → MIDI pitch
6. **VOICE** — on gate rising edge: `note(pitch, slot, vol)`; every frame: `instrument_filter(slot, FILTER_LOW, base_cutoff + flt_cv * 2000, res)`
7. **DRUMS** — on each trigger rising edge: fire the hardcoded drum hit

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
