# dreamengine — Design System

> **STATUS: LIVING (reference).** The single visual contract for the machine: resolution,
> the pico32 palette, the type family, the input model, and the house UI conventions. This
> doc is a **mirror** — the authoritative sources are the code files named in each section
> (and in [Sources of truth](#sources-of-truth) at the bottom). On any conflict, defer to
> the code. A click-to-copy **visual companion** is published as an Artifact (see
> [Companion page](#companion-page)).
>
> Written to be handed to a designer (human or Claude) so anything drawn for dreamengine —
> a cart, a mockup, a store screenshot, a HUD — lands inside these constraints from the start.

---

## The one-line spec

**320 × 200** native canvas · **4×** default scale (→ 1280 × 800 window) · **32** fixed colours
(pico32, indexed 0–31) · **8×8** bitmap type · one input model across keyboard / gamepad / mouse / touch
· responsive & touch rules in [§6](#6--responsive--touch).

The north star: *deep, honest simulations behind a humble lo-fi surface.* The constraints below
are the humble surface — design **for the pixel grid**, never for the scaled-up window.

---

## 1 · Display

The cart draws into a fixed native render texture, which the engine then integer-scales
(nearest-neighbour, no blur) to the window.

| Property | Value | Notes |
|---|---|---|
| Native resolution | **320 × 200 px** | default; per-cart via `-DSCREEN_W` / `-DSCREEN_H` |
| Default scale | **4×** | → 1280 × 800 window; `-DSCALE` |
| Aspect ratio | **8 : 5** | |
| Scaling | integer, nearest | crisp pixels, no filtering |
| Origin | top-left `0,0` | +x right, +y down |
| Sprite cell | **16 × 16** default | `cellW` / `cellH` |
| Sprite sheet | **64 slots** | 8×8 grid → 128 × 128 px sheet |
| Tilemap | **128 × 64** cells | `mapW` / `mapH` |

Resolution, scale, and cell size are compile-time `-D` flags set per cart in the editor's
settings tab. **320 × 200 @ 4× is the house default** and the target unless a cart says otherwise.

*Source: `runtime/studio.h` (`SCREEN_W`/`SCREEN_H`), `editor/src/settings.js` (`DEFAULTS`).*

---

## 2 · Palette — pico32

Exactly **32 fixed colours**, indexed **0–31**. Every draw call takes a palette **index**, never
an RGB value — and in cart code always the **named `CLR_*` constant**, never the bare number.

### Base · 0–15

| # | Constant | Hex | | # | Constant | Hex |
|--:|---|---|---|--:|---|---|
| 0 | `CLR_BLACK` | `#000000` | | 8 | `CLR_RED` | `#ff004d` |
| 1 | `CLR_DARK_BLUE` | `#1d2b53` | | 9 | `CLR_ORANGE` | `#ffa300` |
| 2 | `CLR_DARK_PURPLE` | `#7e2553` | | 10 | `CLR_YELLOW` | `#ffec27` |
| 3 | `CLR_DARK_GREEN` | `#008751` | | 11 | `CLR_GREEN` | `#00e436` |
| 4 | `CLR_BROWN` | `#ab5236` | | 12 | `CLR_BLUE` | `#29adff` |
| 5 | `CLR_DARK_GREY` | `#5f574f` | | 13 | `CLR_INDIGO` | `#83769c` |
| 6 | `CLR_LIGHT_GREY` | `#c2c3c7` | | 14 | `CLR_PINK` | `#ff77a8` |
| 7 | `CLR_WHITE` | `#fff1e8` | | 15 | `CLR_LIGHT_PEACH` | `#ffccaa` |

### Extended · 16–31

Darker / desaturated companions to the base row — shadows, muted fills, night palettes.
Reach for base colours first; use extended to build depth.

| # | Constant | Hex | | # | Constant | Hex |
|--:|---|---|---|--:|---|---|
| 16 | `CLR_BROWNISH_BLACK` | `#291814` | | 24 | `CLR_DARK_RED` | `#be1250` |
| 17 | `CLR_DARKER_BLUE` | `#111d35` | | 25 | `CLR_DARK_ORANGE` | `#ff6c24` |
| 18 | `CLR_DARKER_PURPLE` | `#422136` | | 26 | `CLR_LIME_GREEN` | `#a8e72e` |
| 19 | `CLR_BLUE_GREEN` | `#125359` | | 27 | `CLR_MEDIUM_GREEN` | `#00b543` |
| 20 | `CLR_DARK_BROWN` | `#742f29` | | 28 | `CLR_TRUE_BLUE` | `#065ab5` |
| 21 | `CLR_DARKER_GREY` | `#49333b` | | 29 | `CLR_MAUVE` | `#754665` |
| 22 | `CLR_MEDIUM_GREY` | `#a28879` | | 30 | `CLR_DARK_PEACH` | `#ff6e59` |
| 23 | `CLR_LIGHT_YELLOW` | `#f3ef7d` | | 31 | `CLR_PEACH` | `#ff9d81` |

**Naming gotchas** (recurring compile errors):
- Greys are **British** — `CLR_DARK_GREY` / `CLR_LIGHT_GREY` / `CLR_MEDIUM_GREY` / `CLR_DARKER_GREY`.
  `GRAY` will not compile.
- There is **no `CLR_LAVENDER`** — the lavender colour is `CLR_INDIGO` (13).

*Source: `runtime/studio.h` (the `CLR_*` defines), `editor/public/palettes/pico32.json`.*

---

## 3 · Type

In-game text is **bitmap**. Set the active face with `font(FONT_*)`; every following `print`
call uses it until you reset. The default 8×8 DOS font is home base.

| Constant | Face | Cell | Fits across 320px | Use |
|---|---|---|---|---|
| `FONT_NORMAL` | DOS 8×8 | 8 × 8 | ~40 cols | default, everything |
| `FONT_SMALL` | small | 4 × 6 | ~64 cols | dense HUDs, labels |
| `FONT_TINY` | tiny | 3 × 5 | ~80 cols | tightest fits |
| `FONT_COMIC` | Comic Mono Bold, baked | 10 × 20 | — | titles, dialogue (friendly, rounded) |
| `FONT_THIN` | IBM CGA "thin" 8×8 | 8 × 8 | ~40 cols | lighter-stroke alternate |

- `font(FONT_NORMAL)` resets to the default.
- The **editor UI itself** (not in-game) is set in **Comic Mono**.
- Glyphs are pixel-exact bitmaps rendered from `dos_8x8.png` + ROM dumps — adding a font is its
  own pipeline (see [`font-rendering.md`](font-rendering.md)).

*Source: `runtime/studio.h` (`FONT_*`, `font()`).*

---

## 4 · Input

One model spans keyboard, gamepad, mouse, and touch. Buttons are per-player and edge-aware;
pointers are always positioned.

### Buttons — `btn(player, BTN_*)`

D-pad `BTN_UP` `BTN_DOWN` `BTN_LEFT` `BTN_RIGHT` (0–3) and face buttons `BTN_A` `BTN_B` `BTN_X`
`BTN_Y` (4–7). Keyboard maps the face buttons to **Z X C V**.

| Call | Meaning |
|---|---|
| `btn(p, b)` | held this frame |
| `btnp(p, b)` | pressed **this** frame (edge-triggered) |
| `btnr(p, b)` | released this frame (edge-triggered) |

> A key the cart reads is the cart's key — reading a keycode claims it (the pause hotkey skips
> claimed keys).

### Mouse — desktop (position always known, even with no button down)

| Call | Meaning |
|---|---|
| `mouse_x()` / `mouse_y()` | canvas-space pointer |
| `mouse_world_x()` / `mouse_world_y()` | undoes the active `camera()` shift (click-on-world) |
| `mouse_down(b)` | held — `MOUSE_LEFT` / `MOUSE_RIGHT` / `MOUSE_MIDDLE` |
| `mouse_pressed(b)` / `mouse_released(b)` | edge — this frame only |
| `mouse_wheel()` | + up / − down |
| `mouse_cursor(kind)` | set the pointer shape — **on change, not per frame** |

### Touch — mobile

| Call | Meaning |
|---|---|
| `touch_count()` | active fingers, **0–10** |
| `touch_x(i)` / `touch_y(i)` | canvas-space per finger |
| `touch_id(i)` | stable id — follows the finger as indices shuffle |
| `tap(x,y,w,h)` | any touch inside this rect |
| `tapp` / `tapr` | began / ended inside rect this frame (edge) |
| `touch_layout(mode, n)` | on-screen stick + buttons: `TOUCH_ANALOG` · `TOUCH_ANALOG_FIX` · `TOUCH_DPAD4` · `TOUCH_DPAD8` |

Finger ceiling: **5** iPhone · **10** iPad · **0** desktop.

*Source: `runtime/studio.h` (input section); full signatures in `editor/src/studioDocs.js`.*

---

## 5 · House conventions

The non-obvious rules that keep carts consistent and readable — these bite when ignored.

- **Short verbs, PICO-8 lineage.** The C API is terse: `pset` `circ` `circfill` `rect` `rectfill`
  `line` `spr` `print` `cls`. Never `pixel` / `circle` / `circlefill`. (The `sprite-draw.js` *JS*
  library uses long names — the C API does not.)
- **Colour is an index.** Every draw call takes a palette index 0–31, passed as a `CLR_*`
  constant. No RGB, no hex literals in cart code.
- **British greys / no lavender.** `CLR_*_GREY`, and `CLR_INDIGO` for lavender (see §2).
- **Feedback is tied to events.** Make every action *noticeable* — a hit lands, a jump takes off —
  without reading a number. The one rule: every effect is tied to a specific event; if removing it
  wouldn't make the action less clear, cut it. See [`../guides/game-feel.md`](../guides/game-feel.md).
- **Effects are set-and-hold.** Configure audio DSP (`crush`/`tape`/`reverb`/`eq`/`chorus`) only
  when a value **changes** — never every frame. Wiring a knob straight into `update()`/`draw()`
  rebuilds the bus 60×/s → silent stutter.
- **Layout & widgets are libraries — don't hand-roll.** `lay.h` gives responsive flex-style
  layout (`split`/`cell`/`grid`/`wrap`); `ui.h` gives per-finger widgets (`ui_button`/`ui_slider`/
  `ui_knob`) with fat-finger hit pads and a focus ring. Check these before writing rect math.
- **A small glyph reads where a word can't.** A cramped text HUD (`HP 80  ammo 12|36`) becomes
  minimal and beautiful by swapping labels for code-drawn glyphs (♥ / clip / skull). `flank.c` is
  the worked example.

### Draw-name do / don't

```c
circfill(x, y, r, CLR_RED);            // ✓
circlefill(x, y, r, 0xff004d);         // ✗ wrong name AND raw hex

rectfill(x, y, w, h, CLR_DARK_GREY);   // ✓
rectfill(x, y, w, h, CLR_DARK_GRAY);   // ✗ won't compile (American spelling)
```

### The two-part bar

Every cart must be **verifiable** (focused enough that an oracle can judge it) **and**
**legible-and-delightful to a stranger** (the soul — no oracle checks it). Passing the gates is
not the whole bar. See [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md).

---

## 6 · Responsive & touch

> **STATUS: LIVING — method + engine both shipped; applying to the real racks is Phase 3 (in
> progress).** The layout primitives (`lay.h`) and widgets (`ui.h`, §9) are **shipped**, the
> finger-sized approach is proven in `respond.c` / `rackfit.c` / `acidfit.c`, and — as of
> 2026-07-04 — so is the **engine foundation**: a `-DDE_RESIZABLE` cart fills any device, dodges
> the notch (`safe_rect()`), and reflows on rotate (device-adaptive-layout.md **Phases 0–2 DONE**,
> verified on iPhone SE / 15 + iPad Pro 12.9). Read the live dims with `screen_w()` / `screen_h()`.
> What remains is **Phase 3** — actually reflowing the three Tinyjam product racks
> (`acidrack` · `yachtrack` · `epiano`), all still locked at fixed 320 × 240; **`acidrack` first.**
> Plan: [`device-adaptive-layout.md`](device-adaptive-layout.md). Extends §1, §4, §5.

The one idea this section turns on — proven in `rackfit.c`: **size controls to the finger, not to
the screen.** A fingertip is a fixed physical size (~9 mm ≈ 44 pt) that covers a *different
fraction* of each device — ~11 % of an iPhone's width, ~4 % of an iPad's. So the real driver of a
mobile layout is **points-per-finger, not aspect ratio.** Size every control to a finger target,
then let *how many fit* be **emergent** (feed `lay_wrap` a finger-sized `minItem` — §9.3). Never
branch on device name.

---

### 6.1 · The finger unit (fu)

Work in **logical points** (density-independent), and measure controls in **finger units** —
`fu = 44 pt`, the Apple-HIG comfortable target. The carts carry these constants:

| Constant | Value | Meaning |
|---|---|---|
| `FINGER_PT` (`fu`) | **44 pt** | one comfortable touch target — the sizing unit |
| `TABLET_PT` | **700 pt** | if the device's *min* dimension ≥ this → tablet-class density |
| iPhone (logical pt) | **390 × 844** | swap for landscape |
| iPad (logical pt) | **834 × 1194** | swap for landscape |

**Per-control footprints** (size the control, let count emerge):

| Control | Footprint | Note |
|---|---|---|
| Knob | **≈ 1.4 fu** (~60 pt) | comfortable knobs want more than the 44 floor |
| Sequencer step | **≈ 0.5 fu** | a precision target, deliberately sub-pad — pair with the hit-pad (§9.1) / loupe (§9.2) |
| Keybed white key | **≈ 0.62 fu** → round to whole octaves | phone gets 1 octave, iPad 3, from one loop |

Emergent, not branched: 8 finger-knobs land in one row on iPad and **wrap to two rows on iPhone**
with no `if` — the column count *is* the density (`lay_wrap_cols`, §9.3).

---

### 6.2 · The reflow ladder (reference points)

The four below **fall out** of §6.1 on real devices — they're what you *test against*, not four
builds or hard breakpoints. Ship **one cart** that reads `screen_w()`/`screen_h()` and reflows via
`lay.h`; the same code lands anywhere. They're distinct from the 320 × 200 (8:5) desktop default —
don't ship the house default to a phone. Native dims stay on the 8-px grid so the DOS font (§3)
keeps whole cells.

| Target | Native (`-DSCREEN_W×H`) | Aspect | Nav model | Integer scale to glass |
|---|---|---|---|---|
| iPad landscape | **320 × 240** | 4:3 | Full desk — everything visible at once | **3×** |
| iPad portrait | **240 × 320** | 3:4 | Focused editor **+** track list, stacked | **3×** (slight letterbox) |
| iPhone landscape | **416 × 192** | ~2:1 | Side rail (transport + tabs) + main grid | **2×** |
| iPhone portrait | **192 × 416** | ~1:2 | One machine; **tabs** switch, floor bar switches view | **2×** |

Because scaling is integer (§1), the rendered texture rarely fills a phone/tablet edge-to-edge —
the remainder **letterboxes**, which is on-brand for the console. Choose native sizes for a good
integer fit first, exact aspect second.

**The rule of thumb for nav:** if the short edge is ≥ 240 native px you can afford the
desktop *track-list* model (rows you tap to focus). Below that, switch to **tabs + a single
focused editor** — there isn't room to show every machine and edit one well.

- **Reflow on shape, not device.** Branch on `w/h` and short-edge size, and on the `TABLET_PT`
  threshold — never a hardcoded "is iPhone" check. `short_edge < 240 → tabs`, `w > h → side rail`,
  `else → stacked`. An untested resolution (foldable, dragged desktop window) still lands sane.
- **Snap to the 8-px grid after the math.** `lay.h` gives fractional regions; round region edges
  to multiples of 8 so the DOS font (§3) keeps whole cells and borders stay crisp under the scaler.
- **Test the seams, not just the four.** Resize continuously and confirm nothing overlaps or
  collapses to zero at the in-between widths — that's where float layouts break (`respond.c`).

---

### 6.3 · Touch target floors

Platform guidance is physical: Apple **44 × 44 pt**, Material **48 × 48 dp** (~9 mm), with a
WCAG hard floor of **24 × 24 px** *or* 24 px of clear spacing. Divided by each target's **integer**
scale (§1), in **native pixels**:

| | Comfortable (≈44 pt) | Hard floor (≈24 pt) |
|---|---|---|
| iPhone — 2× | **≥ 22 native px** | **≥ 12 native px** |
| iPad — 3× | **≥ 15 native px** | **≥ 8 native px** |

> **Where these numbers come from (check before you trust them).** The engine integer-scales
> (§1), so the on-glass factor is the largest whole number that fits: **iPhone 2×** (192-native
> short edge → 384 on a ~393 pt iPhone-15-class screen) and **iPad 3×** (240-native → 720 on an
> ~820 pt 10.9″ iPad; the remainder letterboxes). Native px = physical size ÷ that integer scale.
> **To re-derive:** `scale = floor(device_logical_short_edge_pt ÷ native_short_edge_px)`, then
> `min_native_px = ceil(44 ÷ scale)` comfortable, `ceil(24 ÷ scale)` floor. Worked (iPad):
> `floor(820 ÷ 240) = 3` → `44 ÷ 3 = 14.7` → **15**. Change device, `-DSCALE`, or native res and
> these move — a bigger integer scale means a *smaller* native-px minimum (the scale does the
> finger-sizing for you).

- **Chrome buttons, knobs, tabs, mute/fx** — size to the *comfortable* column. On iPhone that
  means transport buttons ~22–30 px tall, knobs ~30–40 px; on iPad they can be denser.
- **Step cells are the exception, and that's fine.** A 16-step row across a phone lands each
  cell at ~20 native px — under the comfort line. Don't shrink the row to fit a bigger cell;
  instead use the **spacing / hit-pad exception**: keep the *visual* cell small but extend the
  *hit area* past it with `ui.h`'s fat-finger pad (this is exactly what `ui_button`/`ui_slider`
  give you — see §5). Never wire a raw `tap(x,y,w,h)` to the visual rect of a sub-comfort cell.
- **8-px minimum gap** between independent controls, so a fat touch can't fire two at once.

---

### 6.4 · Thumb zones & placement

Most one-handed use drives the bottom of the screen; the top and far corners are the hardest
reach. Place controls by frequency and danger, not by desktop habit.

- **Bottom = prime zone.** Put the view-switcher and the most-used performance controls low.
  In the acid-rack phone-portrait layout the `SONG / MIX / FX` bar is pinned to the floor for
  this reason.
- **Landscape → side rails.** With the device held in two hands, the reachable zones are the
  left/right edges, not the top. That's why iPhone-landscape puts transport + machine tabs in
  a **left rail** and gives the whole middle to the step grid.
- **Top strip = status, not action.** Pattern name, BPM readout, PLAYING indicator — glanceable,
  rarely tapped. Fine up top.
- **Guard destructive actions.** `CLR`, pattern-delete, `RND` — keep them out of the prime
  zone or require a confirm; a mis-tap mid-jam is expensive.

---

### 6.5 · Degradation order

When space tightens, drop detail in this order — the goal is that the *performance surface*
(step lanes) survives longest:

1. **Piano roll height** shrinks first (8 → 6 pitch rows on phone portrait). It's context, not
   the edit surface.
2. **Knob row → knob grid.** Seven-across becomes a 4-wide grid so each knob can grow to a
   comfortable target.
3. **All-machines-visible → tabbed.** The track list collapses to a tab strip; only the focused
   machine renders its editor.
4. **Never drop:** the OCT / ACC / SLD step lanes stay full-width on every target. They're what
   you actually perform on.

---

### 6.6 · Legibility floor

At phone native resolutions, small faces stop being readable. Pair the §3 fonts to target size:

| Face | OK down to | Use on phone |
|---|---|---|
| `FONT_NORMAL` (8×8) | all targets | default, labels, values |
| `FONT_SMALL` (4×6) | iPad; iPhone landscape | dense HUD rows |
| `FONT_TINY` (3×5) | **iPad only** | avoid on iPhone — sub-legible after scale |

Rule: if a label would need `FONT_TINY` to fit on a phone, the layout is too dense — reflow
(6.5) instead of shrinking the type. Degrade *detail before control*: draw a label only when the
cell is big enough for it (`if (r >= 5)`, `if (pad.h >= 8)`), so a shrinking control loses its
text before it loses itself (`acidfit.c`).

---

### 6.7 · The geometry layer (`lay.h` patterns)

The reusable moves from `respond.c` / `rackfit.c` — express layout as proportional regions, never
absolute rects (fixed coords are only for genuinely fixed chrome: a bar height, an 8-px gutter).

- **Dock chrome, keep the remainder.** Chain `lay_split` — peel a title off the top, a keybed /
  sequencer off the bottom; `*rest` is the body. Replaces hand arithmetic.
- **Row↔column by flipping `dir`.** `lay_cell(box, narrow ? 1 : 0, n, i, gap)` — same children,
  a row when wide, a stacked column when narrow.
- **Auto-fit vs hand-picked.** `lay_wrap` for "as many finger-wide items as fit" (a swatch strip
  falls 4→3→2→1 columns itself); `lay_grid` when you want a fixed topology (a drum machine wants a
  clean 4-wide pad, not "whatever fits").
- **Fluid, not snapped.** `lay_fluid(pct, container, lo, hi)` for padding / bar heights / gaps
  that should scale smoothly. Pick the font by a width chain too: `w < 110 ? FONT_TINY : w < 200 ?
  FONT_SMALL : FONT_NORMAL`.
- **Anchor floaters** with `lay_at` (9 spots + inset) — a badge in a corner. **Letterbox** a fixed
  ratio with `lay_aspect` (§6.2). `clip()` to the layout rect so nothing spills.
- **Safe-area is asymmetric** — a notch tops one edge, the home-bar strips the opposite, and both
  move in landscape, so use *per-side* `lay_pad` (from `safe_rect()`), never a uniform `lay_inset`.
- **Reserve a corner, not a whole edge**, for system chrome (the shell's "back to root" chip):
  `lay_at(title, L_TR, …)` and pad the title off that side. Giving up a full edge wastes precious
  phone height.

---

### Do / don't

```c
// ✓ small visual cell, but ui.h inflates the HIT rect to UI_MIN_TARGET (24px)
if (ui_button(sx, sy, 12, 12, "")) toggle_step(s);   // fat-finger pad is automatic

// ✗ raw tap bound to the 12px visual cell — misfires constantly
if (tap(sx, sy, 12, 12)) toggle_step(s);
```

- ✓ Size to the finger (`fu` = 44 pt); let count be emergent via `lay_wrap` (§6.1).
- ✓ One fluid cart via `lay.h`; branch on shape / `TABLET_PT`, not device identity.
- ✓ Dock chrome with `lay_split`; flip `lay_cell` `dir` for row↔column; per-side `lay_pad` for safe-area.
- ✓ Size chrome to the *comfortable* column; use the hit-pad exception for grids.
- ✓ Primary controls low (portrait) or on the side rails (landscape).
- ✗ Four separate builds, or absolute rects where a `lay.h` region belongs.
- ✗ Branching on "is iPhone"; sizing to "does the box fit" instead of to the finger.
- ✗ `FONT_TINY` on iPhone; destructive actions in the thumb prime zone.

*Source: `respond.c` (geometry), `rackfit.c` (size-to-finger), `acidfit.c` (disclosure);
primitives shipped in `runtime/lay.h`; Apple HIG / Material / WCAG for the 44 pt & 24 px figures.
Engine viewport/backing-scale status: **verify in `docs/design/device-adaptive-layout.md`**.
Mirrors §1, §3, §4, §9.*

---

## 7 · Semantic colour roles

> **STATUS: PROPOSED — being validated by the acidrack Phase-3 reflow (in progress).** Extends §2 (Palette — pico32). This is deliberately a **floor, not a
> grammar.** §2 gives 32 named colours and full freedom to use them; this section reserves only
> the *few* that must mean the same thing in every cart, so a player never has to relearn "what's
> selected / what's an error" per cart. **Everything else stays wild** — machine hues, step
> colours, backgrounds, themes, night palettes are the cart's own expression. This is a fantasy
> console, not a corporate style guide; loud, colourful, per-cart identity is the point.

### The reserved few

Only these carry fixed meaning across carts. Keep the set this small on purpose — every colour
you promote to "always means X" is one you've taken away from expression.

```c
#define UI_SEL     CLR_ORANGE   // 9 · selection / focus ring — "you are here"
#define UI_DANGER  CLR_RED      // 8 · destructive + error (CLR, delete, failed action)
#define UI_OK      CLR_GREEN    // 11 · activity / PLAYING / pass  (nice-to-have, not load-bearing)
```

That's it. `UI_SEL` and `UI_DANGER` are the two that actually matter — they're the ones where a
misread costs you mid-jam. `UI_OK` is a soft convention; break it if a cart's palette wants to.

Still an **index, never RGB** (§2) — the alias just pins intent. Redefine per cart only if you
have a real reason, and know you're trading away cross-cart muscle memory when you do.

### Everything else is yours

The other ~29 colours have **no reserved meaning**. Use them freely, and re-theme per cart:

- **Machine / voice identity** — give each its own hue (acid rack happens to use pink 303,
  orange 909, yellow 808, but that's *that cart's* choice, not a rule).
- **Step / note colours, lanes, backgrounds, grids** — whatever reads best for the instrument.
- **Themes & night palettes** — lean on the extended row (16–31) for moody variants.

The only ask: don't let expression collide with the reserved few. If your cart's identity hue
*is* orange, make sure selection still reads as selection — shift the identity, add a border,
or move the state cue, so `UI_SEL` doesn't get lost.

### One real gotcha

If a colour is doing double duty — e.g. a step lights up in the same hue as its machine's
identity — **state wins on the control, identity retreats to the label/tab.** A lit step already
means "on"; don't also make it carry "which machine." That's a clash to resolve, not a rule to
memorise.

### Why bother at all

Ties to §5's "a hit lands / state should out-glow its surroundings": on a deliberately loud
screen, the *one thing you must notice* — what's selected, what just errored — has to cut
through. Reserving two colours isn't about taming the palette; it's insurance that the signal
survives the noise. Everything else, go bright.

### Do / don't

- ✓ Treat `UI_SEL` / `UI_DANGER` as fixed; theme the other 29 per cart, freely.
- ✓ Make selection legible *even when* the cart's palette is maximal.
- ✗ Aliasing the whole palette into roles — that's the corporate flattening this section avoids.
- ✗ British-grey / lavender slips — `CLR_*_GREY`, `CLR_INDIGO`, never `GRAY`/`LAVENDER` (§2).

*Source: acid-rack conventions (`carts/acidrack/`); mirrors and defers to §2, which is the
authority on the palette. These roles are advisory and intentionally minimal.*

---

## 8 · Interaction density & depth

> **STATUS: PROPOSED (design notes) — being validated by the acidrack Phase-3 reflow (in progress).** Extends §6 (Responsive & touch). §6 answers *where things
> go and how big*; this section answers a different question — *what the interaction should even
> be* at a given form factor. Captured as a line of thinking to reuse, not a spec to obey. The
> insight: **the finger-to-screen ratio doesn't just resize the layout, it changes the interaction
> model** — menu depth, widget choice, and precision tooling all shift with it.

### 8.1 · The ratio is the driver

§6.3 already computes one number per device: *how many native pixels a fingertip covers.* Read
that as an **interaction budget**, not just a hit-size floor. When a finger is small relative to
the screen (iPad), you can afford to show everything and let the user touch it directly. When a
finger is large relative to the screen (iPhone portrait), every control competes for the same few
"finger-widths" of space — so you must **spend depth instead of area**: hide, stage, disclose.

| Finger : screen | Mental model | Menu depth | Default widget | Precision aid |
|---|---|---|---|---|
| Small (iPad / desktop) | **See-and-touch** — direct manipulation, everything on one plane | **Flat** (0–1 taps to anything) | live **knob** | rarely needed |
| Medium (iPhone landscape) | **Focus-and-perform** — one machine, full grid | 1–2 levels | knob **or** slider | tap-drag fine mode |
| Large (iPhone portrait) | **Select-then-adjust** — one thing at a time | **Deep** (accordion / drill-in) | **stepper / value-drag**, not knob | **loupe** |

The same cart should *feel* different across this range — not just look smaller. That's expected,
not a failure of consistency.

### 8.2 · Menu depth: flat on big, deep on small

Worked from acid rack, the two ends of the range:

- **Large iPad → a 2×2 grid of all four machines, each with a few live knobs on its face.** The
  point of a big screen is to *remove* the dive: you see 303-A, 303-B, 909, 808 at once and tweak
  the two or three knobs that matter (cutoff, res, decay) without entering anything. Flatten the
  hierarchy to match the extra area — surface the shallow, high-frequency controls; keep only the
  rare stuff (routing, presets) behind a tap.
- **iPhone portrait → an accordion, not a grid.** Four machines each showing live knobs is not
  feasible when a finger is ~⅑ of the width. So collapse to one expanded machine over a stack of
  collapsed rows; tapping a row expands it and collapses the rest. Depth buys back the area the
  finger stole. (This is the "tabs + single focused editor" rule of §6.1, seen from the
  interaction side — an accordion *is* progressive disclosure.)

Rule of thumb: **area you have → flatten; area you lack → deepen.** Never port the deep phone
menu up to the iPad (wasted space, needless taps) or the flat iPad grid down to the phone
(unhittable).

**How `acidfit.c` decides, mechanically.** Each section carries a **priority** (array order) and a
**minimum finger-footprint** (`minW`/`minH` in `fu`) — sized so the controls *inside* stay
finger-sized (a 303 panel ≈ "7 finger-knobs + labels" ≈ 4.2 × 3.4 fu; a drum grid ≈ "4-wide finger
pads"). A single **finger-budget pass** asks: does the shape have room to open everything at once?

- **Roomy (tablet) → INLINE-ALL:** every section open, gridded (the 2×2).
- **Tight (phone) → ACCORDION:** list every section as a collapsible row; expand a few by priority
  until the vertical finger-budget runs out; the rest wait as tappable headers.
- **Pin the primary surface regardless:** acidfit's sequencer lane is always inline in *both*
  modes — disclosure hides secondary panels, never the thing you're actually performing on.

The mental model to keep separate: **GEOMETRY (§6.7) decides where a shown panel goes; BEHAVIOUR
(this) decides whether it's shown inline or behind a toggle.** Two layers, one on top of the
other — most "responsive" work only does the first. And size footprints to *"do the controls stay
finger-sized"*, never to *"does the box fit"* — the latter is the trap that crams five sections
onto a phone at sub-finger size.

### 8.3 · A knob isn't sacred — swap the widget by size

A knob is a great *continuous, space-hungry, precision* control. Below the comfortable target
(§6.3) it stops being one — a sub-comfort knob is fiddly and occluded. Don't shrink it; **change
the control to fit the budget:**

| Have | Prefer | Because |
|---|---|---|
| Room for a comfortable knob | **`ui_knob`** | coarse drag **and** wheel-fine in one control |
| Half that | **`ui_slider`** | reads value at a glance; absolute press-to-set |
| Barely any | **`ui_button` / `ui_spr_button`** (make the param discrete) | hittable; a stepped value beats an unhittable knob |
| A dense grid cell | **`ui_spr_button` toggle + the loupe** for the value | cell stays tiny; precision moves into the lens (§9.2) |

Keep the *parameter* identical across form factors; let its *widget* change. A cart that shows a
`ui_knob` on iPad and a `ui_slider` (plus the loupe for fine values) on the phone for the same
cutoff is doing this right. There's no dedicated stepper widget — continuous fine-tuning lives in
the knob's wheel-drag or the loupe.

### 8.4 · The loupe — precision under an occluding finger

On dense surfaces (step grid, piano roll) the finger hides exactly what it's editing. The loupe
(`ui_loupe`, §9.2) is the console's answer: an **opt-in, sticky 3× lens** you summon by dragging
a corner handle over the tiny widgets — then a **second finger edits the magnified copy** while
the first holds the lens. Notes for reuse:

- **The lens is a panel you park, not a press-hold popup.** It sits where you drag it (or
  `ui_loupe_at`), inherently offset from the editing finger — no separate "offset the magnifier"
  work needed.
- **Loupe = precision, not navigation.** Fine-place a note/step/value with it; use
  `camera_ex` / `pinch_scale` for scrolling and whole-view zoom (§9.4).
- **It's a phone tool first.** iPad rarely needs it (finger already small vs. content); on iPhone
  portrait it's often the *only* way to hit a single 16th cleanly. It complements the built-in
  hit-pad (§6.3 / §9.1), it doesn't replace it.
- **Tie it to an event (§5):** the lens appearing *is* the feedback that precision mode is live.

### 8.5 · The through-line

One cart, one set of parameters, but the *verbs* change with the ratio: **touch** on iPad,
**focus then adjust** on phone, with the loupe as the phone's precision escape hatch. Design the
interaction for the finger you'll actually have there — don't assume the desktop gesture scales
down.

*Source: `acidfit.c` (progressive disclosure — priority + finger-footprints), `rackfit.c`
(size-to-finger); extends §6, concretised in §9. The disclosure decision is shipped as cart
logic; a `ui.h` helper for it is not yet standardised.*

---

## 9 · Widget & layout catalog

> **STATUS: LIVING (reference).** The concrete controls behind §6 / §8, mirroring the shipped
> headers `runtime/ui.h` and `runtime/lay.h`. These are **cart-land headers** — their doc strings live in the header
> comments, *not* in `studioDocs.js`, so the editor gives no hover/autocomplete for them (unlike
> the `studio.h` API). Everything is `static`, called every frame from `draw()`.

### 9.1 · Widgets (`ui.h`)

Wrap every frame: **`ui_begin()` first** in `draw()`, **`ui_end()` last**. Skip `ui_end()` and
clicks never resolve — only hover shows.

| Widget | Value | Behaviour |
|---|---|---|
| `ui_slider(*v,x,y,w,label)` | `*v` 0–1 | **absolute** — press sets value at the point; height is fixed at `UI_SLIDER_H` 9 (no `h` arg) |
| `ui_knob(*v,x,y,label)` | `*v` 0–1 | `x,y` = centre; **relative** vertical drag (`UI_KNOB_DRAG_PX` 60 = full sweep), wheel fine-tunes; `UI_KNOB_R` 10 |
| `ui_button(x,y,w,h,label)` | — | momentary; returns 1 on activate. **Identity is the rect, not the label** |
| `ui_spr_button(slot,x,y,w,h,selected)` | toggle | sprite-faced; `selected` = lit "hot" fill |
| `ui_spr_button_styled(…,UiSprStyle)` | toggle | same + per-state colours (`bg`/`frame`/`halo`…; `-1` = skip) so a cart keeps its own look |

- **Events** (query around the call): `ui_grabbed(v)` — frame grabbed (snapshot undo *before* the
  widget); `ui_released(v)` — frame let go (commit *after*); `ui_captured(id)` — contact owned by
  a widget, so your own pad/ribbon can ignore that finger.
- **Focus mode:** `ui_focus(on)` — sticky d-pad/gamepad focus ring (off by default so gameplay
  keeps the d-pad). `ui_get_widgets(&out)` → live rect array.
- **Hit sizing → §6.3:** `UI_MIN_TARGET` 24 (min hit square), `UI_HIT_PAD` 2. The fat-finger pad
  is built in — *this is the "hit-pad exception."* Visual size can be smaller; the hittable rect isn't.

### 9.2 · The loupe (`ui.h`)

`ui_loupe(on)` — opt-in, sticky 3× lens: drag a corner handle to summon it over the tiny widgets,
then a **second finger edits the magnified copy**. The one-line fix for "my knobs are too small on
mobile." Built on `zoom_rect`. `ui_loupe_at(sx,sy)` summons + parks it programmatically;
`ui_loupe_has(id)` = is contact over the lens. Tunables: `UI_LOUPE_ZOOM` 3.0, `_W` 120, `_H` 84,
`_HANDLE` 20.

### 9.3 · Layout (`lay.h`)

`Box {x,y,w,h}` in, `Box` out; **re-solved every frame** (no retained tree — that's what makes it
responsive). All coords relative to the container `Box`.

| Primitive | Does | Reflow use |
|---|---|---|
| `lay_split(c,edge,size,*rest)` | dock a fixed band off an edge; remainder → `*rest` | rails, status bars; chainable |
| `lay_cell(c,dir,n,i,gap)` | i-th of n equal flex kids along `dir` (0 row / 1 col) | **flip `dir` at a breakpoint = row↔column** |
| `lay_grid(c,cols,n,i,gap)` | i-th of n in a fixed `cols` grid | the clean 4-wide pad |
| `lay_wrap(c,n,i,minItem,gap)` | auto-fit: as many `minItem`-wide cols as fit, wraps | column count follows width itself |
| `lay_aspect(c,ratio)` | centred `w/h` box that **letterboxes** itself | the §6.1 letterbox, for free |

Sizing/spacing: `lay_clamp`, `lay_fluid(pct,container,lo,hi)` (scales with the container),
`lay_pad`/`lay_inset`, `lay_at(c,anchor,w,h,inset)` (9 anchors, `L_TL`…`L_BR`),
`lay_wrap_cols(…)` (query density). Drawing sugar (needs `studio.h`): `boxfill` / `boxrect` /
`binside`.

### 9.4 · Whole-view zoom — *not* the loupe

Keep these for **navigation**; the loupe is for **precision** (§8.4).

- `zoom_rect(sx,sy,sw,sh, dx,dy,dw,dh)` — nearest-neighbour region copy; the loupe / PiP / minimap
  primitive. Samples the frame *so far* — call it **after** the content you're copying.
- `camera_ex(x,y,zoom,angle)` — sticky view zoom + rotate (studio.h).
- `pinch_scale()` (`gestures.h`) — this frame's two-finger factor; multiply your zoom by it.

*Reference carts: `respond.c` (drag a fake screen, watch it reflow), `rackfit.c`, `acidfit.c`.*
*Source: `runtime/ui.h`, `runtime/lay.h` (header comments are the authority — not in `studioDocs.js`);
zoom primitives in `editor/src/studioDocs.js`. Mirrors §6 / §8.*

---

## Companion pages

Visual, standalone companions to this doc live in
[`design-system-pages/`](design-system-pages/) — self-contained HTML, committed so they survive
without the Artifact host. They're numbered in **the order they were made, and read as the design
system *maturing***: an early device-layout take, then a fuller visual companion, then the reflow
sketch that folds in §6–§9. Open them in a browser (or preview on GitHub) side by side to see the
thinking sharpen.

| # | Page | What it is |
|---|---|---|
| 1 | [`1-acid-rack-device-layouts.html`](design-system-pages/1-acid-rack-device-layouts.html) | **Oldest** — the first pass at acid rack across devices, before the responsive rules were written down. |
| 2 | [`2-dreamengine-companion.html`](design-system-pages/2-dreamengine-companion.html) | The visual companion to §1–§5 — click-to-copy palette swatches, screen diagram, font specimens, button-map. |
| 3 | [`3-acid-rack-device-adaptive-sketch.html`](design-system-pages/3-acid-rack-device-adaptive-sketch.html) | Acid rack reflowed for iPad/iPhone × portrait/landscape (INLINE-ALL / STACKED / TABS / ACCORDION), applying §6–§9. |
| 4 | [`4-acid-rack-four-modes.html`](design-system-pages/4-acid-rack-four-modes.html) | **Latest** — the four disclosure modes (INLINE-ALL / STACKED / TABS / ACCORDION) worked out furthest. |

The markdown here is the **durable record**; the pages are convenience + evolution trail. The
live Artifact of page 2 is at
`https://claude.ai/code/artifact/268e6bfa-1cee-4f53-b26d-0ad5008be248` (redeploy it to cover
§6–§9 — reflow diagram, role swatches, interaction map, widget specimens — when those sections
move from PROPOSED to LIVING).

---

## Sources of truth

This doc mirrors, and defers to, the code:

| What | Where |
|---|---|
| Palette + `CLR_*` constants | `runtime/studio.h` · `editor/public/palettes/pico32.json` |
| Screen defaults | `editor/src/settings.js` (`DEFAULTS`) |
| Fonts, input, all API | `runtime/studio.h` |
| Authoritative API signatures + docs | `editor/src/studioDocs.js` |
| Responsive / touch (§6) | `carts/respond.c` · `rackfit.c`; primitives in `runtime/lay.h`; engine viewport → `docs/design/device-adaptive-layout.md` |
| Semantic colour roles (§7) | acid-rack conventions `carts/acidrack/`; defers to §2 |
| Interaction density & depth (§8) | `carts/acidfit.c` · `rackfit.c`; extends §6, concretised in §9 |
| Widgets & layout (§9) | `runtime/ui.h` · `runtime/lay.h` (header comments); zoom in `editor/src/studioDocs.js` |
| Rules + pointers | [`../../CLAUDE.md`](../../CLAUDE.md) · [`../README.md`](../README.md) |
