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
| **Jog / spinner wheel** (browse/data wheel) | relative, unbounded, **weighted** | Pioneer CDJ, DJ browse, tape scrub, steering-wheel spinner | bigger; often a **finger dimple** you spin repeatedly; **momentum + acceleration** (fast spin = big jumps) to fly through a long list |
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

**Lighting — one source, top-left (the Win95 model).** Every control on the panel is lit from the
**same top-left corner**, exactly like Windows did 30 years ago: **highlight on the top + left, shadow
on the bottom + right.** This is the single rule that makes a panel of mixed controls read as one lit
surface instead of a pile of separate widgets — so it is not per-control taste, it's a house constant.
Concretely:
- **Round controls** (knobs, ring-encoders): sheen arc centred at **225°** (up-left), shade arc 180°
  opposite at **45°** (down-right) — *not* pure-top/pure-bottom (270°/90°), which was the first-pass
  mistake that made knobs and buttons disagree. (0°=right, 90°=down.)
- **Rectangular controls** (buttons, pads, step cells): the **Win95 double-bevel chisel** — a two-tone
  edge, highlight (white → light) on top + left, shadow (mid → black) on bottom + right, black on the
  outermost bottom-right. Draw the highlight first so the shadow wraps the shared corners.
- **Pressed / engaged INVERTS the bevel** (dark top-left, light bottom-right) and nudges the content
  1px down-right — the control reads as pushed *into* the same top-left light. A latching toggle stays
  inverted while engaged (the Win95 sunken-toggle look); an LED-style button stays raised and glows instead.
- **Verify sheen/shade against palette LUMA, not the colour name.** The bevel assumes
  `sheen > face > shade` in brightness. `CLR_MAUVE` (#754665, luma ~96) *sounds* like a light purple
  but is darker than `CLR_INDIGO` (#83769c, ~127) — using it as a highlight drew dark arcs top *and*
  bottom. pico32 has no lighter purple, so the indigo caps highlight with `CLR_PINK` (~180). When these
  graduate into `ui.h`, derive hi/lo from the face luma (or keep a vetted per-colour {face,hi,lo} table)
  rather than hand-picking names.

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

**Pixel-alignment discipline (the recurring bevel trap — learned building `rotaries`).** `circ`/
`circfill` and `arc`/`arcfill`/`ring` all rasterize off the *identical* pixel-centre metric
(`x + 0.5 − cx`, `d² ≤ r²` — see `studio.c` `disc_inside` / `sector_inside`), so a fill and a stroke
line up **pixel-exact** — but only under two rules, and breaking either makes the rim, the fill and the
sheen "drift" by a pixel:

1. **Stay concentric.** Every piece that touches an edge shares one centre `(cx,cy)`. Do NOT offset a
   face circle to fake a dome — the offset piece no longer sits under the rim. (Want a dome? use a soft
   highlight blob kept fully *inside* `r−3`, so it never reaches the rim and its offset can't mismatch.)
2. **One stroke family.** Don't rim with `circ` (its rim = "an inside pixel with an outside neighbour")
   and then sheen with `arc` (an annulus band) — two different definitions of "the 1px ring." Pick the
   arc/ring family for every stroke: flat `circfill` face → `ring` sheen one pixel inside the rim →
   `arc` rim on top. The engine documents the blessed pattern at `studio.c:4508` — *"draw the fill,
   then arc() on top, no gap."* The result is edge-lit (bright top rim, dark bottom rim), which reads
   as a beveled cap without any offset.

**Moving parts move.** A control's *texture* rotates/slides with the control, not just its indicator:
an endless encoder's knurl grip must spin with the turn (it's the ONLY turn feedback — there's no
pointer), a fader cap's grip line rides with the cap. Cheap, and it's half of what makes a control feel
physical.

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

**Labeling is part of the vocabulary** (how you NAME a control when there's no room), studied in
[`labels`](../../tools/carts/labels.c): the five fonts + `print_rot_scaled`, text effects (drop-shadow,
4-way outline, an inverted chip/pill, a handwritten `FONT_COMIC` boil), **rotated labels** for tight
vertical columns (`print_rot` — a 90° label beside a fader), and the **Dymo labelprinter** look —
embossed rounded tape + a punched hole, in both classic colorways (white-on-dark tape, dark-on-white
paper). Ships reusable `label_*` helpers (`label_shadow`/`label_outline`/`label_chip`/`label_hand`/
`label_tape`) the control carts pull from. A tiny code-drawn glyph still beats a word where one fits
([`design-system.md`](design-system.md) §"minimal and beautiful"); labels are for when it doesn't.

## 8 · The study carts (a few, grouped by family)

Grouped by family so each cart is a rich study, not a single widget — and each family maps to the
device-face zones that pull from it. **Five built (2026-07-14), each cycling all three skins** (§10);
`keys` is the one still open:

1. **`rotaries`** ✅ — the whole turning family: pot (pointer) · endless encoder (no pointer) ·
   **LED-ring encoder in all 5 ring modes** · push-encoder · concentric · rotary selector · a SIZE row
   (r 6→22) · a STYLES row (ridged/scalloped/coloured-cap collet knobs) · a LIVE row (mod halo / glow /
   ghost / jog wheel). Exercises the value-model spine + the ring/light system. → zone 2.
2. **`switches`** ✅ — momentary · latching · illuminated states (off/dim/lit/blink) · radio group ·
   bat toggle (2 & 3 pos) · slide · rocker · a COMPACT `finger_px()` row. → zones 1 & 4.
3. **`pads`** ✅ — candy velocity pads (flash + fade envelope) · drum pads with the EXTERNAL-trigger
   corner LED (clock-fired vs finger-tapped) · a playhead clock. → zones 4 & 5.
4. **`faders`** ✅ — a vertical mixer bank (LED-per-fader) · horizontal · centre-detented crossfader ·
   spring-return ribbon · XY pad. → zones 2 & 3.
5. **`indicators`** ✅ — status LEDs (states/colours/chase) · stereo VU + peak-hold · 7-seg readouts ·
   an LCD scope with phosphor + scanlines. → zones 1 & 3.
6. **`keys`** (open) — the keybed. → zone 5.

## 9 · Beyond hardware — the live/software layer (parked)

The whole [device-face thesis](device-face-paradigm.md) is *"the aesthetic is a gift, the limitations
are baggage — take only the gift."* §1–8 above take the **look** (bevel, socket, knurl). This section
parks the other half: the **behaviours** a software rotary can have that a physical one can't. They're
parked because [`rotaries`](../../tools/carts/rotaries.c) is a *look/anatomy* study (static forms), and
these are about **behaviour over time** — they want a live modulation source + interaction to show off.
Home = a future **`liveknobs`** behaviour study (not yet built).

**The unlock:** a physical knob *fuses* two things — where your hand set it, and what the value actually
is (the cap position IS the value; it can be in only one place). Almost every superpower comes from
**separating those two.**

- **① See what hardware must hide (the value has its own life)**
  - **Modulation halo** — the killer one. The cap shows your *base* setting; a second live mark sweeps
    the ring showing the *actual modulated* value as an LFO/envelope moves it. You watch the filter
    breathe while the knob sits still — impossible for a physical cap, and pure emergent-behaviour grain.
  - **Ghost default** — a faint tick at the default/preset, always visible, so "home" is never lost.
  - **Automation trail** — the ring remembers where it's been; recorded automation drawn on it.
- **② Move in ways a shaft can't** — **teleport** (tap the ring → jump; a pot must sweep) · **double-tap →
  snap to default** · **momentum** (flick an endless spinner, it coasts and settles — see the jog-wheel
  entry in §3) · **spring-return OR hold, same widget** (pitch-bend springs, cutoff holds) · **remappable
  travel** (log/coarse/wrapping, changeable live).
- **③ One knob, many jobs** — **contextual retarget** (the same knob edits the patch / a held step / a mod
  depth, the ring re-skinning to say which — the device-face p-lock, generalized; hardware needs a shift +
  memory, ours *shows* the mode) · **gang/macro** (one knob drives several with locked offsets).
- **④ Show what it DOES, not a number** — **value-reactive skin** (colour cold→hot, glow with the audio it
  controls, pulse at an extreme) · **the ring IS the parameter's curve** (a cutoff knob's halo previews the
  filter response; an attack knob's ring is the envelope) · **self-labeling** (name always; exact value +
  unit only while touched — no silkscreen budget).
- **⑤ Safety hardware can't give** — grab to preview, commit on release, drag off to abandon; per-control
  undo. `ui.h` already surfaces `ui_grabbed`/`ui_released` — the hook points exist.

**Shortlist to build first** (most "impossible in hardware" × most useful for our instruments): the
**modulation halo**, **ghost-default + tap-to-teleport + double-tap-reset**, and **value-reactive glow**.
The halo alone justifies the idea.

## 10 · The skin system (and the external-stylesheet horizon)

A control has a **skin** — the visual language it's drawn in — separate from its *identity* (what value
model it is) and its *behaviour* (how it reads input). The study carts prototype three:

- **TACTILE** — beveled chrome: sockets, top-left lighting, the Win95 chisel. Reads as hardware. This
  is really authored for a big screen and shrunk; lush but heavy on a 320×200 panel.
- **FLAT** — PICO-flat: the same geometry with the lighting removed (flat faces, 1px borders). The
  humble-lo-fi middle.
- **PURE** — the **native 320×200 register**: hairline (1px) frames, thin dials, `fillp` dither fills,
  compact, dense, high-info — the C64-OS / tracker / VSTSID look, authored *for* the low resolution
  rather than scaled down to it. Often the most honest fit for a dream­engine cart.

Prototyped in [`rotaries`](../../tools/carts/rotaries.c) as an enum (`SK_TACTILE/FLAT/PURE`) cycled by
the SKIN button (or `B`); rolling to `switches` + `indicators` next. Skin gates *only* the drawing —
identity, value rings, textures and LED glows are constant across skins, so no information is lost, only
the register changes.

**The external-stylesheet horizon (design-for, don't-build-yet).** Scattered `if (skin==…)` branches are
the prototype, not the destination. The target is a **`Skin` as a data record** — a stylesheet-in-a-struct
the widgets *read* instead of branch on:

```c
typedef struct {
    int  border;     // BORDER_NONE / FLAT / BEVEL / CHISEL
    int  fill;       // FILL_SOLID / DITHER / GRADIENT
    int  hairline;   // 1 = 1px frames, tight metrics
    int  knob_r, corner_r;                 // metrics
    int  role[UI_ROLE_N];                  // palette by semantic role (design-system.md §7)
} Skin;
```

Then the three skins are three `Skin` literals, and — the horizon — that record could later be **loaded
from a file** (a real stylesheet), the same way [`lay.h`](../../runtime/lay.h) / `disclose.h` made
*layout* data-driven and reflowable. This is the seam to design toward: keep style in the `Skin` record,
keep the widgets reading roles/metrics from it, and an external sheet becomes a parser + a struct-fill,
not a rewrite. Related: [`design-system.md`](design-system.md) §7 (the semantic colour roles a `Skin`
would key on) and the device-adaptive layout work (`lay.h`/`disclose.h`) — style is to those widgets
what layout is to the box tree. Not building the loader now; building the *shape* that admits one.

A worked example of a theme *on top of* the skins: [`candy-style.md`](candy-style.md) — the warm candy-toy
device-face look (a `Skin` instance's warm palette **plus** the two things a skin doesn't cover: an on-screen
mascot and juice). It's what a stylesheet-plus-character looks like in practice.

## Related

[`device-face-paradigm.md`](device-face-paradigm.md) (the layout — which control goes in which zone;
this is the vocabulary those zones are filled from) · [`ui-widgets-notes.md`](ui-widgets-notes.md) (the
`ui.h` widget-kit proposal — the eventual graduation target) · [`design-system.md`](design-system.md)
§7 (semantic colour roles) · [`runtime/ui.h`](../../runtime/ui.h) (current flat widgets) ·
[`runtime/studio.h`](../../runtime/studio.h) (the drawing primitives §6 builds on) ·
[`arcs`](../../tools/carts/arcs.c) + [`uikit`](../../tools/carts/uikit.c) (the carts we start from).
