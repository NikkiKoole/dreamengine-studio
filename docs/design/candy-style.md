# The candy device-face style

**STATUS: EXPLORING (2026-07-14)** — the warm "candy-toy" visual style for device-face instruments, prototyped in
the [`facemock`](../../tools/carts/facemock.c) / [`tinyface`](../../tools/carts/tinyface.c) /
[`facefull`](../../tools/carts/facefull.c) vibe mockups and made functional in
[`acidcandy`](../../tools/carts/acidcandy.c). This is the STYLE GUIDE those carts were reverse-engineered
from — a coherent recipe so the candy acid rack (and future candy faces) build *to a spec*.

> **Why it exists.** The maker's north-star reference is the **TinyJam app icon**
> ([`docs/marketing/tinyjam/icons/tinyjam-icon-1024.png`](../marketing/tinyjam/icons/tinyjam-icon-1024.png)):
> a warm cream-and-candy handheld toy with a little creature on its screen. The first functional face
> ([`grooveface`](../../tools/carts/grooveface.c)) was *competent but soulless* — it cleared the
> [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md) "verifiable" bar but not the
> "delightful-to-a-stranger" one. Chasing that "YES" produced this style. It's the **delight half of the
> bar**, written down.

## 0 · What candy IS (and isn't)

Candy is a **palette + character + juice** direction, *not* a fourth skin. The three
[control-vocabulary skins](control-vocabulary.md) (§10: TACTILE / FLAT / PURE) are the *chrome register*;
candy is a **theme on top** — mostly the TACTILE bevel + a warm candy palette + (crucially) a **mascot
with soul** + **juice**. In the eventual `Skin`-struct-as-stylesheet (control-vocabulary §10), candy is
one `Skin` instance (warm role-palette) plus the two things a skin doesn't cover: the on-screen character
and the feedback. Take those two away and it's just "TACTILE in warm colours" — they're what make it sing.

## 1 · The resolution — design FOR 160×100 ×4

Candy faces target **160×100 at scale 4** (a 640×400 window), NOT 320×200. The harsh half-resolution is a
*feature*: it forces the iconic, chunky essence and kills fuss — the maker's instinct, borne out by
`tinyface` reading far more "toy" than the roomier `facemock` (320×400). Design *for* the small canvas,
don't shrink a big one into it (the same lesson as the PURE skin). The full five-zone paradigm fits (see
`facefull`), though it's dense — favour letting the mascot breathe over cramming.

## 2 · The palette (pico32 indices)

| role | colour | notes |
|---|---|---|
| **shell** (outer case) | `CLR_INDIGO` + `CLR_DARK_PURPLE` edge | the purple rounded shell |
| **chassis** (faceplate) | `CLR_LIGHT_PEACH` | warm cream; a 1px `BLEND_AVG` white top-sheen line |
| **screen bezel** | `CLR_BROWNISH_BLACK` | dark frame around the LCD |
| **screen field** | `CLR_DARK_GREEN` | phosphor; `BLEND_AVG` black scanlines every 2px |
| **mascot** | `CLR_GREEN` body / `CLR_LIME_GREEN` lit belly | + `CLR_PINK` cheeks |
| **knobs** | `CLR_INDIGO` body, `CLR_PINK` top-left glint, `CLR_DARKER_PURPLE` shade | pink glint because MAUVE is darker than INDIGO (control-vocabulary §6 luma trap) |
| **candy pads** | `CLR_PINK` / `CLR_ORANGE` / `CLR_YELLOW` / `CLR_GREEN` / `CLR_TRUE_BLUE` | highlights: LIGHT_PEACH / LIGHT_YELLOW / LIME_GREEN; shades: the DARK_* of each |
| **title** | `CLR_RED` + `CLR_YELLOW`, black outline | see §4 |
| **accents / LEDs** | `CLR_ORANGE` (amber), `CLR_RED` (heart), `CLR_LIGHT_YELLOW` (sparkle) | |

Everything is lit **top-left** (the pinned lighting rule, control-vocabulary §5–6).

## 3 · The soul — the mascot (the single most important element)

A device you love has something *alive looking back at you*. Candy's is a **slime creature** living on the
LCD (the paradigm's "the screen carries character," [device-face-paradigm](device-face-paradigm.md) §1f):
- a rounded green blob (`ovalfill` body + a lighter lit belly), **white eyes with black pupils**, a small
  **smile arc**, **pink cheek blush**, and a **music note** it hums;
- **bops to the beat** — `cy += sin_deg(now()*rate)` (or the real playhead), squashing slightly at the bottom;
- **blinks** occasionally; ideally **reacts** to what you do (which pad you hit, an accent).

If a candy face's screen is a bare data readout, it has no soul — that's the `grooveface`/`acidcandy` gap.
The data (piano-roll, pattern) should be a *flow you tap to*, with the mascot as the default face.

## 4 · Title & labels

Handwritten, candy, outlined: `FONT_COMIC` (rounded handwriting), drawn in `CLR_RED`/`CLR_YELLOW` with a
1px black 4-way outline (see `label_outline` in [`labels`](../../tools/carts/labels.c)). At 160×100 the
comic font is too big for a title band — use `FONT_SMALL` there and save comic for the "big toy" (320×400).
Use the `labels` cart's helpers (`label_shadow`/`label_outline`/`label_chip`/`label_tape`) for the rest.

## 5 · Juice (every action noticeable — [game-feel](../guides/game-feel.md))

- **pads squish** on press (shift down 1–2px + shrink) and flash a `BLEND_AVG` white pop
- **sparkle burst** (a twinkling plus-shape) on a hit
- **heart pulse**; **LED glow** (colour halo via `BLEND_AVG`)
- the **mascot reacts** — bops harder / turns to the hit
Each effect tied to a specific event; if removing it wouldn't make the action less clear, cut it.

## 6 · Where it lives (the reference carts)

- [`tinyface`](../../tools/carts/tinyface.c) — the distilled 160×100 toy (mascot as hero). **The look to match.**
- [`facefull`](../../tools/carts/facefull.c) — the whole five-zone paradigm at 160×100, candy.
- [`facemock`](../../tools/carts/facemock.c) — the 320×400 "big toy" (fuller, less iconic — kept for contrast).
- [`acidcandy`](../../tools/carts/acidcandy.c) — the FUNCTIONAL candy RACK: five focusable machine faces
  (2×303 on `acid303.h`, 808 on `tr808.h`, 909 on `tr909.h`, a MST master) on one transport, with note-bar
  drawing, a flag palette, and a drawable PCF — packaging `acidrack`'s guts as the device-face paradigm.
  Still **no mascot** (the soul gap, deferred); the rest of the open work is in its `de:meta.todo[]`.

## Related

[`device-face-paradigm.md`](device-face-paradigm.md) (the five-zone shape candy dresses) ·
[`control-vocabulary.md`](control-vocabulary.md) (the widgets + the three skins candy themes over; §6
lighting, §10 skin system) · [`labels`](../../tools/carts/labels.c) (the label helpers) ·
[`game-feel.md`](../guides/game-feel.md) (juice) · the TinyJam icon (the north-star reference).
