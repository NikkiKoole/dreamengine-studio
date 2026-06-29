# Touch controls — a native-resolution on-screen joystick / d-pad / buttons

STATUS: DESIGN — ready to pick up (2026-06-29). Research, a recommendation, AND a concrete
implementation plan (see "Implementation plan" below); nothing rebuilt yet. The task is
pulled from **two ends**: the iOS port ([`ios-plan.md`](ios-plan.md) — raw `key()`/`btn()` carts
have no touch equivalent on a phone) and the cart-polish backlog
([`../../field-notes/cart-polish-punchlist.md`](../../field-notes/cart-polish-punchlist.md) — ~18
carts ask for an on-screen joystick/d-pad). Companion: [`action-plan.md`](action-plan.md) (Tier 0
"Touch movement affordance"), `runtime/studio.c` (the current overlay), `runtime/pointer.h` /
`ui.h` / `gestures.h` (the input primitives).

## The core requirement (the maker's call)

The controls must render at the **cart's native pixel resolution, drawn inside the engine** — so
they look like part of the game, not a UI sticker on top. This is the one firm constraint, and it's
the right one (see "the central tension" below).

## What already exists — and why it's wrong for us

The engine **already ships** an on-screen control overlay (`runtime/studio.c`):

- `void touch_controls(bool on)` toggles it; `TOUCH_CONTROLS_DEFAULT` is the compile default (off).
  No published cart enables it yet.
- It draws a **bottom-left analog stick** (`STICK_RADIUS 60`, `STICK_DEADZONE 0.35`) + **two
  bottom-right A/B buttons** (`BTN_RADIUS 44`).
- `btn(0, …)` already synthesizes from it: directions read `stick_x()/stick_y()` vs the dead zone,
  A/B hit-test the button circles. Falls through to keyboard/gamepad otherwise.

**The problem is *how* it draws.** `draw_touch_overlay()` runs **after** the canvas render-texture is
blitted to the window, using **raw Raylib `DrawCircle`/`DrawCircleLines`/`DrawTextEx` in window/device
pixels and flat white RGB.** So it:

- renders at **device resolution**, not the native pixel grid — smooth circles on top of chunky
  pixels (the "different overlay");
- **ignores the palette** (raw white, not a `CLR_*` index);
- ignores `camera()`/`clip()` and any cart styling;
- is player-0-only, exactly two buttons, analog-stick-only (no d-pad mode).

So we're not building from zero — we're **moving the existing controls from a device-res Raylib
overlay onto native-res engine primitives**, and generalizing stick→(stick | d-pad) + N buttons.

## What the successful games do (external study)

- **Two archetypes, by game type.** *Fixed* (base always in one spot, always visible) suits
  precision/action and grid games — you return to a known home. *Floating / dynamic* (base spawns
  where the thumb lands, re-centers on drift) suits free movement — you never look down. **Vampire
  Survivors mobile is a single floating stick + auto-attack, zero buttons.**
- **D-pad vs analog.** D-pad / 4-way wins for **grid & precise** games (columns, puyo, dr-mario,
  sokoban, rogue, platformers). Floating analog wins for **free 8-way** (vampire-survivors,
  twin-stick). → the widget needs **both modes, chosen per cart** — which is exactly what the
  polish list asked for ("joystick for free movement, d-pad for grid games").
- **Feel.** Clamp + normalize output to −1..1; a **dead zone** so a resting thumb doesn't drift; let
  the thumb **drag past max radius**; ~1 cm travel = full tilt; **smooth knob lerp** to kill jitter.
- **Ergonomics (hard numbers).** Min tap target **44 px (Apple) / 48 dp (Material)**, never below
  ~9 mm; ≥8 px spacing; controls live in the **lower-third reachable arc** (left = move, right =
  act).
- **The mainstream "standard" is the anti-pattern for us.** RetroArch / Delta overlays are a
  semi-transparent PNG at **device resolution**, positioned by **normalized coordinates**, default
  **0.75 opacity** — deliberately smooth and high-res, always a layer on top. That's the look we are
  *rejecting*.

## The central tension

The most common, most "successful" approach (emulator-style device-res overlay) is the exact
opposite of the maker's requirement. The honest resolution for *this* engine is to draw with the
**normal `circ`/`circfill`/`rectfill`/`spr` primitives at native resolution** so the control is
rasterized into the pixel grid and obeys the palette. The tradeoff: at low-res the control is
**chunky**, so it must be **sized so that `native_px × SCALE` still clears the ~44 px physical
thumb target** — the design has to reason in *both* native pixels and final on-screen size. (At
320×200·4× a 44 px physical target ≈ 11 native px radius; bigger on smaller scales.)

## Recommendation

**Phase 1 — fix the render path (small, high-payoff).** Rewrite `draw_touch_overlay()` to draw with
engine primitives into the **canvas render-texture at native resolution** (reset `camera()`/`clip()`
first so it sits in screen space), using `CLR_*` palette indices instead of raw white. Same stick +
A/B, same `btn()` wiring, same API — just *native-res and palette-correct*. This alone delivers the
maker's requirement and instantly improves the ~18 carts that would enable it. Verify with
`canvas-diff` (it now goes through the software canvas like everything else).

**Phase 2 — generalize to a real widget.** Add what the study says is needed:

- a **mode**: `floating analog` | `fixed analog` | `4-way d-pad` | `8-way d-pad`, chosen **per cart
  in code** (not settings — the polish list is explicit);
- **N action buttons** (not hard-wired to 2), with optional labels/icons (code-drawn glyphs via
  `sprite-draw.js` keep them pixel-native — see `flank`);
- **feel**: normalized −1..1 + dead zone (keep ~0.35-of-radius), thumb-drag-past-radius, knob lerp;
- **placement**: lower-third arc; floating zone on the left half, buttons bottom-right;
- **auto-size** the control from `SCALE` + `touch_ceiling()` so it clears the physical target on
  phone vs tablet without per-cart fiddling.

**Phase 3 — wire into the iOS shell.** Per `ios-plan.md`, route the synthesized inputs through the
existing `de_key_event` seam so **raw `key()` carts** (WASD movement) also become playable, not just
`btn()` carts.

## Open questions / forks

- **Draw hook.** Cleanest is a studio-owned pass that runs *inside* `BeginTextureMode(canvas)` after
  the cart's `draw()`, with camera/clip reset — so it's native-res but the cart never has to call it.
  (Alternative: expose a `draw_touch_ui()` the cart calls itself — more control, more boilerplate.)
  Recommend studio-owned.
- **Opt-in vs auto.** Explicit `touch_controls(true)` / a `.cart.js` flag (per the polish list), or
  auto-show when `touch_ceiling() > 0` and the cart reads `btn()`? Recommend explicit opt-in in code
  first; auto-detect is a later convenience.
- **Sprites vs primitives for the art.** Primitives first (no asset dependency, trivially
  palette-correct). A code-drawn pixel sprite set (`sprite-draw.js`) is a nice Phase-2 upgrade for a
  more characterful d-pad/stick.
- **Synthetic input.** `btn()` is read-only today (touch→stick→btn); nothing can *inject* button
  state. Not needed for touch, but worth noting for any future headless/AI/`spec` input path.
- **Per-cart config home.** Where the mode + button set live — a `touch_layout(...)` call in the
  cart, or fields in its `.cart.js` metadata. Decide alongside Phase 2.

## Implementation plan

Read this before starting; line numbers drift, so **grep for the function names**, don't trust the
~approx hints. All edits are in `runtime/studio.c` (+ `studio.h` for any new API) — a **hot shared
file**: targeted `Edit`s only, never a full `Write`, re-Read the region first, and after committing
confirm your change survived (`git show HEAD:runtime/studio.c | grep '<sentinel>'`). See CLAUDE.md
"Hot shared source files".

### The seam (where things live today)

- `draw_touch_overlay()` (~`studio.c:912`) — draws stick + A/B with **raw Raylib** (`DrawCircle`/
  `DrawCircleLines`/`DrawTextEx`) in **window pixels**. Called at ~`:1692`, **after** the canvas
  render-texture is blitted to the window. ← this placement is the whole problem.
- The canvas is drawn into a `SCREEN_W×SCREEN_H` `RenderTexture2D` between `BeginTextureMode(canvas)`
  / `EndTextureMode()` (~`:1648`–`:1689`), then blitted ×`SCALE`.
- `update_stick()` (~`:876`) — per-frame stick math; called ~`:1500`.
- `btn(0,…)` (~`:2210`) — when `show_touch_ui`, directions read `stick_x()/stick_y()` (~`:2335`) vs
  `STICK_DEADZONE`; A/B `any_touch_in_circle()`. Else keyboard/gamepad.
- Constants (~`:484`): `STICK_RADIUS 60`, `STICK_DEADZONE 0.35`, `BTN_RADIUS 44`.
- `touch_x()/touch_y()` divide window pixels by `SCALE` → canvas space (~`:2257`).

### Phase 1 — native-res render path (no API change, do first)

Move the overlay drawing **inside** `BeginTextureMode(canvas)` (so it rasterizes at native res and
scales up with the game), and redraw it with **engine primitives + palette indices**:

1. Add a studio-internal `draw_touch_overlay_native()` that runs **after the cart's `draw()` returns
   but before `EndTextureMode()`**. Reset transform first — `camera(0,0)` (or the internal
   equivalent) and clear any `clip()` — so the controls sit in fixed screen space regardless of what
   the cart left set.
2. Replace every raw Raylib call with the engine equivalent drawing in **canvas coordinates**:
   `DrawCircle`→`circfill`, `DrawCircleLines`→`circ`, `DrawTextEx`→`print` (or a code-drawn glyph),
   colours as `CLR_*` indices (pick a legible pair, e.g. `CLR_WHITE` ring on a `CLR_DARK_GREY`
   translucent pad — see blend-tables for index-only translucency if wanted).
3. Convert the geometry from window px to **native px**: divide the current `STICK_RADIUS`/
   `BTN_RADIUS` constants by `SCALE` for the canvas-space sizes (see sizing math below). Hit-testing
   already works in canvas space via `touch_x/y()`; keep `btn()`/`stick_x/y()` math unchanged.
4. Delete the old post-blit `draw_touch_overlay()` call (~`:1692`).
5. Verify (see below), prototype-bake on `vampire-survivors`, confirm the controls read as pixel art
   (chunky, palette-correct), commit.

### Sizing math (the one subtlety)

A physical thumb target should be ≥ ~44 device px. Native size = `ceil(44 / SCALE)`. At 320×200·4×
that's an **11 native-px radius**; at 2× it's 22. So derive control sizes from `SCALE` (and bump on
phones via `touch_ceiling()`), don't hard-code native px. Keep ≥ a few native px of spacing between
buttons so chunky pixels don't merge.

### Phase 2 — generalize to a real widget (after Phase 1 ships)

Proposed API (new — needs the 4-place dance in CLAUDE.md "Adding a new API function": declare in
`studio.h` w/ one-liner, implement in `studio.c`, document in `studioDocs.js`, list in `shell.js`):

```c
// modes
#define TOUCH_NONE      0
#define TOUCH_ANALOG    1   // floating analog stick (free 8-way) — default
#define TOUCH_ANALOG_FIX 2  // fixed analog (base always shown)
#define TOUCH_DPAD4     3   // 4-way d-pad (grid games)
#define TOUCH_DPAD8     4   // 8-way d-pad

void touch_layout(int move_mode, int n_buttons);   // configure once (cart opt-in, in code)
void touch_button_label(int i, const char *txt);    // optional per-button glyph/label
```

- `move_mode` picks stick vs d-pad; `TOUCH_DPAD*` feeds `btn()` directions as booleans (no dead
  zone), analog modes keep the −1..1 + dead-zone path.
- `n_buttons` generalizes the hard-wired 2; lay them out bottom-right in the reachable arc.
- **Floating analog**: on first touch in the left zone, spawn the base at the touch point; let the
  knob drag past `STICK_RADIUS` (clamp output, optionally re-center base on big drift); smooth-lerp
  the drawn knob. Use `pointer.h` (`PTR_ACQUIRE`) to own the moving finger so a second finger on a
  button doesn't steal it (this is also why raylib rgestures was rejected — see `gestures.h`).
- Config home: a `touch_layout()` call in the cart's `update()`/init is most in-keeping (carts
  already opt in via code, per the polish list — *not* settings). A `.cart.js` field is a possible
  later convenience; decide then.

### Phase 3 — iOS shell

Route the synthesized inputs through the existing `de_key_event` seam (`ios-plan.md` Phase 2) so raw
`key()` carts (WASD movement) become playable, not just `btn()` carts. Mostly Swift-side wiring; the
engine side is the same overlay reading touches.

### Verification

- `node tools/canvas-diff.js <cart>` — the overlay now goes through the software canvas like every
  other primitive; A/B GPU-vs-CPU must match (guards the native-res move).
- Bake + eyeball: `node tools/make-cart.js <c> <png>` then `--run`; read the thumbnail — controls
  should be chunky/palette-correct, not smooth white.
- `node tools/mobile-lint.js <cart>` — sanity on touch-playability.
- Smoke a `btn()` cart under the harness to confirm directions/buttons still fire.

### Checklist

- [ ] **P1** move overlay draw inside `BeginTextureMode`, reset camera/clip
- [ ] **P1** swap raw Raylib calls → `circ`/`circfill`/`print` with `CLR_*`
- [ ] **P1** size from `SCALE` (`ceil(44/SCALE)`), remove post-blit call
- [ ] **P1** `canvas-diff` + bake on `vampire-survivors`, commit
- [ ] **P2** `touch_layout(mode, n_buttons)` + d-pad modes (4-place API dance)
- [ ] **P2** floating-analog base-spawn + `pointer.h` capture + knob lerp
- [ ] **P2** N buttons + optional labels; per-cart opt-in in code
- [ ] **P2** prototype `columns`/`puyo` (d-pad) and a free-move cart (analog)
- [ ] **P3** wire `de_key_event` in the iOS shell for `key()` carts

### First move

Phase 1, step 1 — relocate the overlay draw inside the render-texture pass and reset the transform.
It's the smallest change that delivers the "same resolution" requirement, and everything else builds
on it.

---

Sources (external study): [Cursa — touch control patterns](https://cursa.app/en/page/touch-controls-for-mobile-games-input-patterns-and-feedback),
[Scripts for Unity — touch input](https://scriptsforunity.com/blog/touch-input-best-practices-unity/),
[Libretro overlay docs](https://docs.libretro.com/development/retroarch/input/overlay/) (the device-res-overlay standard we reject),
[Parachute — thumb zone](https://parachutedesign.ca/blog/thumb-zone-ux/),
[DesignMonks — button size](https://www.designmonks.co/blog/perfect-mobile-button-size),
[HowToGeek — d-pad vs analog](https://www.howtogeek.com/types-of-games-you-should-play-with-your-d-pad-instead-of-an-analog-stick/),
[Gamereactor — Vampire Survivors touch](https://www.gamereactor.eu/vampire-survivors-now-has-touch-controls-1221203/).
