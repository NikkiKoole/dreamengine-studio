# The control vocabulary — a named taxonomy of hardware controls (beveled lo-fi)

REFERENCE (2026-07-14) — the shared NAMING + look for the tactile UI elements the
[device-face paradigm](device-face-paradigm.md) reuses across every instrument cart. Settled with the
maker *before* building the study carts, so the names are fixed once. Target register: **beveled
lo-fi** (§6). The study carts that prove each family (`rotaries` first) are the next build; this doc
is the vocabulary they're written against.

> **Why this doc exists.** "Knob" is doing far too much work. Real hardware has a precise vocabulary,
> and the names aren't cosmetic: **each control type implies a value model AND an indicator**, which is
> what both our drawing code and our naming must honour. A bounded pot has a pointer because its angle
> is real; an endless encoder has no pointer because its angle is a lie — all its truth is in a ring.
> Getting the vocabulary right is what lets a stranger read a control correctly with no manual (the
> [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md) legible-to-a-stranger bar), and it
> lifts every device-face cart at once — one gorgeous rotary, and every zone-2 gets it for free.
>
> This is the *vocabulary + look*; [`device-face-paradigm.md`](device-face-paradigm.md) is the *layout*
> (which control goes in which zone) and [`runtime/ui.h`](../../runtime/ui.h) is the *implementation*
> (input capture, focus, hit-pads). Graduation of these looks INTO `ui.h` is deferred — first get the
> carts great (maker, 2026-07-14).

## 1 · Two axes — and the value model is the primary one

Every control has a **form** (what it looks and feels like) and a **value model** (what it *means*).
We reach for form first ("a knob"), but the value model should drive naming and code, because **it
dictates the indicator** — the single fact that most often gets a control drawn wrong.

- **Form** → §3 (the families: turn / slide / press / flip / show / 2D).
- **Value model** → §2. This is the primary sort.

## 2 · The value models (the primary taxonomy)

| value model | position means | needs an indicator | wraps? | examples |
|---|---|---|---|---|
| **Absolute + bounded** | *is* the value; visible min & max | the pointer/notch itself | no (hard stops) | potentiometer, fader |
| **Relative + unbounded** | nothing — spins forever | **yes, external** (ring / number) | yes | endless encoder, jog wheel |
| **Discrete positional** | one of N labelled detents | the lit/leaning position | no | rotary selector, toggle, slide switch |
| **Momentary** | — (an event, not a state) | flash on the frame it fires | — | pushbutton, trigger pad |
| **Latching** | a boolean state | the light (off/on) | — | mute/arm button, loop toggle |

Two orthogonal flavours cut across the continuous ones:

- **Unipolar vs bipolar** — where "zero" lives. A volume pot fills **from the bottom/min**; a pan or
  detune pot **detents at centre and fills outward** (a centre-out ring, a fader that rests mid-track).
  Same form, different indicator — this is a per-control call, not a per-type one.
- **Detented vs smooth** — clicky steps vs continuous glide. A feel, and (for us) an animation: the
  detent is a tiny snap; smooth is free.

## 3 · The families (form) — with real hardware to steal from

### ↻ TURN — the rotary family

| control | value model | real hardware | the visual tell |
|---|---|---|---|
| **Potentiometer** ("pot") | absolute, bounded (~270°) | Moog knobs, guitar amps | **has a pointer/notch** + optional printed dial arc |
| ↳ **pointer / chicken-head knob** | " | vintage synths, amps | a beak that *points* — best "which way am I set" |
| ↳ **skirted knob** | " | Fender amps | wide base with the scale printed *around* it |
| ↳ **concentric (dual)** | two pots, one shaft | amp bass/treble, MS-20 | big + small stacked — two params, one footprint |
| **Endless encoder** | relative, unbounded | Elektron data knob, car stereo | **NO pointer** — bare knurled cap; value lives elsewhere |
| **LED-ring encoder** | relative + a ring readout | MIDI Fighter Twister, NI Maschine, Novation | a **halo of light IS the value** (§4) — the premium one |
| **Push-encoder** | turn **and** click | synth data wheels, car dials | turn = value, push = enter/toggle — "a knob that's also a button" |
| **Rotary selector / switch** | discrete, N labelled | guitar pickup selector, osc-range switch | detented, labels around it, snaps between positions |
| **Jog wheel / data wheel** | relative, free-spin, weighted | Pioneer CDJ, tape scrub | a big thumb wheel — scrub / nudge |
| **Motorized** (behaviour, not a type) | recalls on preset change | SSL faders | *animates to* the value on load — a gift we get for free |

### ⇅ SLIDE — the linear family

- **Fader** — a linear pot, **vertical** (mixer) or **horizontal**. Cap variants: T-bar, knurled,
  colour-coded. The TinyJam icon's fader (metal cap + grip line + an amber LED reading its value) is this.
- **Crossfader** — a centre-detented horizontal fader (a DJ A/B blend; bipolar).
- **Ribbon / touch strip** — no moving part; absolute finger position. May spring back to centre
  (pitch ribbon) or stay put (`pocketbox`'s strip). The "fader with no cap."

### ⊙ PRESS — the button family

- **Momentary button** — on while held, off on release (transport, one-shot triggers).
- **Latching / toggle button** — a press flips a sticky boolean (mute, arm, loop).
- **Illuminated button** — the cap lights; states are a colour/brightness language (§4): **off /
  dim-armed / bright-active / blinking-pending**. Ableton Push, Novation Launchpad.
- **Velocity / RGB pad** — pressure-lit; harder = brighter (MPC, Maschine, Push). The candy pads in the icon.
- **Radio group** — mutually exclusive; exactly one lit (waveform / mode select).
- Physical flavours worth studying: **rubber drum pad** (springy travel), **arcade button** (convex,
  deep throw), **capacitive** (no travel, glows on touch).

### ⇄ FLIP / SELECT — the switch family

- **Toggle switch (bat handle)** — 2–3 positions; a lever that *leans*. Classic Moog/synth osc-range,
  LFO-shape. A tactile "it is set *here*" we don't have yet.
- **Rocker** (a see-saw; power) · **slide switch** (a nub between labelled stops) · **DIP bank** (a tiny
  on/off row; config).

### ▦ SHOW — the indicator family (takes no input, sells the "device")

- **LED** — a single light; colour = state, blink = attention. **Bi-colour / RGB** variants.
- **LED bargraph / VU meter** — a stack of segments reading a level.
- **7-segment / segmented readout** — the numeric display (BPM, pattern #); crisp, glowy.
- **LCD / OLED** — the zone-3 "soul" screen (the device-face context display); its own study.

### ⊹ 2D / exotic

- **XY pad / Kaoss pad / joystick** — 2D continuous (the icon's SOUND pads).
- **Keybed** — its own family; see `epiano`/`moog`/`touchpiano` and `keybed.h`.

## 4 · The light & ring sub-system (cross-cutting)

The single biggest jump to "premium," and it reuses across encoders, buttons, pads and meters — so
it's one small language, not per-widget code. It's built on the existing `arc()` / `ring()` primitives
(the [`arcs`](../../tools/carts/arcs.c) cart's gauge is already a fill-mode ring).

- **Ring modes** (a lit halo around an encoder or a pad):
  - **dot** — a single lit position (an endless encoder's only honest readout)
  - **fill** — a `0 → value` arc (unipolar)
  - **bipolar** — centre-out, growing left/right from 12 o'clock (pan, detune)
  - **spread / width** — a lit *band* between two angles (a range)
  - **pulse** — a breathing glow (armed / tempo / pending)
- **Button-light states** — one vocabulary every illuminated button/pad shares:
  **off** → **dim** (present but inactive) → **lit** (active) → **blink** (recording / pending).

## 5 · "The look tells you the type" — the drawing discipline

The rules that keep a control honest (a stranger reads it right without a manual):

1. **A bounded control shows its bounds; an unbounded one hides its position.** A pot/fader gets a
   pointer, a printed min→max arc, hard end-stops. An endless encoder gets a **bare cap and no
   pointer** — because a pointer would claim an absolute angle it doesn't have. Its value goes in a
   **ring or a number**, never in the cap.
2. **Discrete controls snap; continuous controls glide.** A rotary selector / toggle jumps between
   labelled detents (and the label under it is lit); a pot sweeps smoothly.
3. **Zero lives where the fill starts.** Unipolar fills from min; bipolar fills from centre. Draw the
   rest track so the resting position reads (a centre-detent tick for bipolar).
4. **Momentary flashes, latching holds.** A trigger lights only the frame it fires; a toggle stays lit
   until pressed again. Never draw a momentary as if it were stateful.
5. **Feeling blocked vs free.** Turning a bounded pot past its stop should read as *blocked* (the
   pointer stops, maybe a nudge); an endless encoder should feel free and wrap.

## 6 · Beveled lo-fi — the rendering register

The [TinyJam icon](../marketing/tinyjam/icons/tinyjam-icon-1024.png) is the **mood board**, not the
build target: it's painterly (anti-aliased, airbrushed shadows) which breaks the "humble lo-fi
surface" north star and won't animate. We translate its *vibe* — tactility, candy colour, a little
soul-glyph on things — into **pixel-honest 1–2px bevels** (the DIV / BlitzMax / Win-95 register):

- **Bevel** — a 1–2px light edge (top-left) + dark edge (bottom-right); press *flips* it and shifts
  the face down 1px.
- **Socket** — a control sits in a recessed dish: a `fillp`-dithered inner shadow + a `BLEND_AVG`
  drop shadow underneath (`BLEND_AVG` with black = an instant soft shadow).
- **Face lighting** — a subtle `vgradient` top-light on the cap; a 1px highlight crescent top-left.
- **Glow** — LEDs/rings brighten toward white at full; a dithered halo for "lit."

Primitives we build from (all already in [`studio.h`](../../runtime/studio.h)): `rrect`/`rrectfill`
(rounded panels & pads), `arc`/`ring` (the value halos), `gradient`/`vgradient`/`hgradient` (dithered
lighting), `ovalfill`, `fillp` (dither shadows), `BLEND_AVG`/`BLEND_SUB` (soft/cold shadows). No new
engine primitive is needed — this is composition and taste. Palette is the pico32 32-colour set
(British greys: `CLR_*_GREY`, never `GRAY`; lavender = `CLR_INDIGO`).

## 7 · Naming (what we call things in code)

Lead with the value model, not the shape, so the name says what it *does*:

- `pot` (bounded) vs `encoder` (endless) vs `selector` (discrete rotary) — **not** all "knob."
- `fader` (linear) · `ribbon` (touch strip) · `crossfader` (bipolar linear).
- `button` (momentary) · `toggle` (latching) · `radio` (mutually-exclusive group).
- `switch` (bat/slide, discrete positional).
- `pad` (velocity/lit) · `stepcell` (sequencer LED).
- Indicators: `led` · `vu` / `bargraph` · `readout` (7-seg) · `screen` (LCD).
- Ring modes: `RING_DOT` / `RING_FILL` / `RING_BIPOLAR` / `RING_SPREAD` / `RING_PULSE`.
- Light states: `LIT_OFF` / `LIT_DIM` / `LIT_ON` / `LIT_BLINK`.

(These are the *intended* code names; nothing is in `ui.h` yet — graduation is deferred per §STATUS.)

## 8 · The study carts (a few, grouped by family)

Grouped by family so each cart is a rich study, not a single widget — and each family maps to the
device-face zones that pull from it:

1. **`rotaries`** (build first) — the whole turning family: pot (pointer) · endless encoder (no
   pointer) · **LED-ring encoder in all 5 ring modes** · push-encoder · concentric · rotary selector.
   Exercises the entire value-model spine and debuts the ring/light system. → zone 2.
2. **`switches`** — momentary · toggle · illuminated states · radio group · bat toggle · slide switch. → zones 1 & 4.
3. **`pads`** — velocity/RGB pads (candy, icon-style) · step-cell LEDs (off/on/playing/accent). → zones 4 & 5.
4. **`faders`** — vertical + horizontal + crossfader + ribbon (+ maybe XY). → zones 2 & 3.
5. **`indicators`** — LED · VU bargraph · 7-seg readout · the LCD/screen bezel. → zones 1 & 3.
6. **`keys`** (later) — the keybed. → zone 5.

## Related

[`device-face-paradigm.md`](device-face-paradigm.md) (the layout — which control goes in which zone;
this is the vocabulary those zones are filled from) · [`ui-widgets-notes.md`](ui-widgets-notes.md) (the
`ui.h` widget-kit proposal — the eventual graduation target) · [`design-system.md`](design-system.md)
§7 (semantic colour roles) · [`runtime/ui.h`](../../runtime/ui.h) (current flat widgets) ·
[`runtime/studio.h`](../../runtime/studio.h) (the drawing primitives §6 builds on) ·
[`arcs`](../../tools/carts/arcs.c) + [`uikit`](../../tools/carts/uikit.c) (the carts we start from).
