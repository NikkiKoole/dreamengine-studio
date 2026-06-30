# Touch controls — a native-resolution on-screen joystick / d-pad / buttons

STATUS: DESIGN — ready to pick up (rev. 2026-06-30). Research, a recommendation, AND a concrete
implementation plan (see "Implementation plan" below); nothing rebuilt yet. The task is
pulled from **two ends**: the iOS port ([`ios-plan.md`](ios-plan.md) — raw `key()`/`btn()` carts
have no touch equivalent on a phone) and the cart-polish backlog
([`../../field-notes/cart-polish-punchlist.md`](../../field-notes/cart-polish-punchlist.md) — ~18
carts ask for an on-screen joystick/d-pad). Companion: [`action-plan.md`](action-plan.md) (Tier 0
"Touch movement affordance"), `runtime/studio.c` (the current overlay), `runtime/web_shell.html`
(the web canvas), `runtime/pointer.h` / `ui.h` / `gestures.h` (the input primitives).

## The core requirement (the maker's call)

The controls get their **own real estate, placed where there's room** — they are **not** a sticker
floating on top of the game. And they are **engine-drawn at the cart's native pixel resolution**, so
they read as part of the console (chunky, palette-correct), never as a smooth device-res UI layer.

Two halves, both firm:

1. **Placement is responsive — "it depends, do the best for each case."** A cart is ~8:5
   (320×200 = 1.6:1); a phone is ~19.5:9 (≈2.17:1). Where the *spare* space lands flips with
   orientation, so the engine measures the letterbox at runtime and places the controls into it —
   no per-platform `if (iOS)` branch, just a couple of geometry rules:
   - **Portrait** (web or iOS): the game sits in the top third, a tall band is free **below** →
     **deck below** (this is the PICO-8 portrait layout).
   - **Landscape** (the common iOS-native action pose): the game fills the height, spare space is
     on the **sides** → **side-rails** (stick on the left rail, buttons on the right).
   - **Game fills the screen** (matched aspect, no letterbox) → **corner overlay** on top, hugging
     the lower-third thumb arc (the only case where we draw over gameplay, as a fallback).
2. **The cart declares *what*, the engine decides *where*.** The cart picks the *scheme* in code
   (`touch_layout(mode, n_buttons)` — analog vs d-pad, how many buttons); the engine picks the
   *placement* from the available space. Same cart call works on web portrait, iOS landscape, and a
   desktop touchscreen.

**Opt-in, always.** Most carts will never show a joypad — a sound toy, a mouse-driven editor, a
text game, anything already playable by tap don't want one. So the feature is **inert until a cart
explicitly asks** (`touch_layout(...)`/`touch_controls(true)`); no cart's behaviour or look changes
otherwise. This isn't just politeness — it's the blast-radius bound from "De-risking principle" rule
3: a bug can only reach carts that opted in.

(See "the central tension" below for why native-res, and "the placement model" for the geometry.)

## What already exists — and why it's wrong for us

The engine **already ships** an on-screen control overlay (`runtime/studio.c`):

- `void touch_controls(bool on)` toggles it; `TOUCH_CONTROLS_DEFAULT` is the compile default (off).
  No published cart enables it yet.
- It draws a **bottom-left analog stick** (`STICK_RADIUS 60`, `STICK_DEADZONE 0.35`) + **two
  bottom-right A/B buttons** (`BTN_RADIUS 44`).
- `btn(0, …)` already synthesizes from it: directions read `stick_x()/stick_y()` vs the dead zone,
  A/B hit-test the button circles. Falls through to keyboard/gamepad otherwise.

**Two things are wrong: it's an overlay, and it draws at device res.** `draw_touch_overlay()` runs
**after** the canvas render-texture is blitted to the window, using **raw Raylib
`DrawCircle`/`DrawCircleLines`/`DrawTextEx` in window/device pixels and flat white RGB.** So it:

- always sits **on top of gameplay** (no concept of placing into the letterbox);
- renders at **device resolution**, not the native pixel grid — smooth circles on top of chunky
  pixels (the "different overlay");
- **ignores the palette** (raw white, not a `CLR_*` index);
- ignores `camera()`/`clip()` and any cart styling;
- is player-0-only, exactly two buttons, analog-stick-only (no d-pad mode).

So we're not building from zero — we're **moving the existing controls onto native-res engine
primitives, giving them a responsive home (deck / rails / overlay), and generalizing
stick→(stick | d-pad) + N buttons.**

## The placement model (the new heart of the design)

The engine knows the window/canvas size and the cart's `SCREEN_W×SCREEN_H`. From those it computes
the letterbox once per frame (cheap) and chooses a home:

```
fit the SCREEN_W×SCREEN_H game into the window preserving aspect → game rect + leftover bands
  band BELOW the game ≥ min deck height   → DECK   (game shrinks up, controls fill the band below)
  bands on the SIDES ≥ min rail width      → RAILS  (stick → left band, buttons → right band)
  neither (game fills the window)          → OVERLAY (corner thumb-arc, on top — the fallback)
```

Key consequences:

- **Deck and rails are NOT overlays** — the game is scaled to leave them room, so nothing is ever
  covered. Only the no-room fallback draws on top.
- **The deck/rail is engine-drawn**, native-res, palette-correct — same `circ`/`circfill`/`print`
  primitives as the game, just into a region outside the cart's drawable area. On web this means the
  framebuffer/canvas is *taller (or wider) than the game* and the band is ours; it does **not** mean
  DOM/HTML buttons (web-only, non-pixel — explicitly rejected, see forks).
- **One code path, all platforms.** Web portrait → deck; iOS landscape → rails; desktop with no
  touch → no controls at all (full window is game). The platform never appears in the logic; only
  the measured geometry does.
- **Desktop/no-touch shows nothing** — `touch_ceiling() == 0` ⇒ skip the whole pass, game uses the
  full window as today.

## What the successful games do (external study)

- **Two archetypes, by game type.** *Fixed* (base always in one spot, always visible) suits
  precision/action and grid games — you return to a known home. *Floating / dynamic* (base spawns
  where the thumb lands, re-centers on drift) suits free movement — you never look down. **Vampire
  Survivors mobile is a single floating stick + auto-attack, zero buttons.**
- **D-pad vs analog.** D-pad / 4-way wins for **grid & precise** games (columns, puyo, dr-mario,
  sokoban, rogue, platformers). Floating analog wins for **free 8-way** (vampire-survivors,
  twin-stick). → the widget needs **both modes, chosen per cart** — which is exactly what the
  polish list asked for ("joystick for free movement, d-pad for grid games").
- **PICO-8's web/mobile layout** is the deck model: the cart sits in a fixed pixel rect and the
  controls live in the surrounding chrome, *below* in portrait — not floating over the game. That's
  the layout we're adopting (generalized to rails in landscape).
- **Feel.** Clamp + normalize output to −1..1; a **dead zone** so a resting thumb doesn't drift; let
  the thumb **drag past max radius**; ~1 cm travel = full tilt; **smooth knob lerp** to kill jitter.
- **Ergonomics (hard numbers).** Min tap target **44 px (Apple) / 48 dp (Material)**, never below
  ~9 mm; ≥8 px spacing; controls live in the **lower-third reachable arc** (left = move, right =
  act).
- **The emulator "standard" is the anti-pattern for us.** RetroArch / Delta overlays are a
  semi-transparent PNG at **device resolution**, positioned by **normalized coordinates**, default
  **0.75 opacity** — deliberately smooth, high-res, and always a layer on top. We reject *both* the
  device-res look *and* the always-on-top placement.

## The central tension

The most common, most "successful" approach (emulator-style device-res overlay-on-top) is the exact
opposite of the maker's requirement on **both** axes. The honest resolution for *this* engine:

1. **Placement** — give the controls the letterbox (deck/rails) instead of stealing gameplay space;
   only overlay as a no-room fallback.
2. **Look** — draw with the normal `circ`/`circfill`/`rectfill`/`spr` primitives at native
   resolution so the control rasterizes into the pixel grid and obeys the palette.

The tradeoff of (2): at low-res the control is **chunky**, so it must be **sized so that
`native_px × SCALE` still clears the ~44 px physical thumb target** — the design reasons in *both*
native pixels and final on-screen size. (At 320×200·4× a 44 px physical target ≈ 11 native px
radius; bigger on smaller scales.) The deck/rails make this *easier* than an overlay: the controls
have their own space, so they can be as large as the band allows without covering the game.

## De-risking principle — separate the dangerous plumbing from the visible feature

The one genuinely *hard-to-fix* risk here is **silent, library-wide coordinate breakage**: today
`touch_x/y()` is a bare `window_px / SCALE`, and the moment placement shrinks/offsets the game rect
that math is wrong for **every cart that reads touch or mouse** — it compiles fine, looks fine, and
just puts taps in the wrong place. The plan is structured so this risk is *retired before the
feature is built*, not discovered on a phone afterward. Three rules make that hold:

1. **Land the coordinate transform first, as a provable no-op (Phase 1.5).** Introduce one
   `game_rect` (origin + scale) chokepoint that *all* window↔canvas conversion goes through, with
   `game_rect = full window` (an identity transform). Ship it alone; it must change **nothing**
   (`canvas-diff` byte-identical, every cart's coords unchanged). Then Phase 2 placement is merely
   *setting `game_rect` to a different value* and every coordinate consumer follows automatically —
   the scary rewrite is over before the feature starts.
2. **The placement decision is a pure, headlessly-tested function.** `(win_w, win_h, screen_w,
   screen_h) → {mode, game_rect, control_rect}` renders nothing, so it gets a deterministic test
   over a matrix of window/screen sizes (the `det-probes/` pattern), including a **round-trip
   property**: `canvas→window→canvas == identity` for sampled points. That *auto-catches* the silent
   mapping bug off-device instead of relying on eyeballing a phone.
3. **The new path is 100% inert until a cart opts in.** No existing cart changes behavior until it
   explicitly calls `touch_layout()` (or `touch_controls(true)`). This bounds the blast radius: a
   bug is confined to the handful of opt-in carts, never library-wide. (And keep `btn()` synthesis
   untouched — change *where things draw* and *how coords map*, not the proven input layer.)

Plus two moves that shrink the fragile surface and the no-oracle surface:

- **Web framebuffer change is deferred out of the first risky release.** The `web_shell.html`
  touch-mirror is the piece most likely to pass on desktop and break on a real phone, so prove
  deck/rails on **native + iOS first** (where we own the surface) and keep web on the Phase-1 corner
  overlay until the model is proven.
- **Feel has no oracle, so make iterating it cheap.** Pull the tunables (dead zone, radius, knob
  lerp, drag-past) into named constants and tune via the live-inspection harness (state snapshot +
  screenshot without restart) — 20 tweaks in the time one rebuild takes.

## Recommendation

**Phase 1 — fix the look (small, high-payoff, no placement change yet).** Rewrite the existing
overlay to draw with engine primitives into the **canvas at native resolution** using `CLR_*`
palette indices instead of raw white. Same stick + A/B, same `btn()` wiring, same API — just
*native-res and palette-correct*, still a corner overlay for now. This alone delivers the "looks
like part of the game" half and instantly improves the ~18 carts that would enable it. Verify with
`canvas-diff`.

**Phase 1.5 — the coordinate chokepoint, as a no-op (do before any placement).** Introduce the
`game_rect` (origin + scale) abstraction and route all window↔canvas conversion (`touch_x/y()`,
mouse, hit-tests) through it, with `game_rect = full window`. Add the pure placement-decision
function and its headless round-trip test (per "De-risking principle"). This ships *changing
nothing* — `canvas-diff` byte-identical, all cart coords unchanged — and turns Phase 2 into "set
`game_rect`," not "rewrite coordinate math on a phone."

**Phase 2 — responsive placement (the deck/rails model).** Now that `game_rect` is the chokepoint,
add the letterbox measurement and the three homes (deck / rails / overlay fallback) from "the
placement model" above by *setting `game_rect`*; the controls draw into the freed band. This is the
change that fulfils the maker's core requirement. **Native + iOS first; keep web on the Phase-1
corner overlay** (defer the `web_shell.html` framebuffer change until the model is proven).

**Phase 3 — generalize to a real widget.** Add what the study says is needed:

- a **mode**: `floating analog` | `fixed analog` | `4-way d-pad` | `8-way d-pad`, chosen **per cart
  in code** (not settings — the polish list is explicit);
- **N action buttons** (not hard-wired to 2), with optional labels/icons (code-drawn glyphs via
  `sprite-draw.js` keep them pixel-native — see `flank`);
- **feel**: normalized −1..1 + dead zone (keep ~0.35-of-radius), thumb-drag-past-radius, knob lerp;
- **auto-size** the control from `SCALE` + `touch_ceiling()` so it clears the physical target on
  phone vs tablet without per-cart fiddling.

**Phase 4 — wire into the iOS shell.** Per `ios-plan.md`, route the synthesized inputs through the
existing `de_key_event` seam so **raw `key()` carts** (WASD movement) also become playable, not just
`btn()` carts. iOS landscape will naturally resolve to the **rails** placement.

## Synergy with the gamepad item (STATUS #7)

This work and the open **gamepad** item (`gp_axis`/`gp_present` + a `btn()` augment — STATUS.md #7,
`api-notes.md` §15) are the **same seam from two directions**, and `touch_layout()` is the shared
foundation:

- **One control declaration drives both.** `touch_layout(mode, n_buttons)` says "this cart's logical
  controls are a 4-way + 2 buttons" (or analog + 1). That's exactly what a gamepad mapping needs too
  — it answers "what are this cart's buttons" once, for *every* alternate input source. Touch and
  gamepad stop being two features and become two front-ends to one declaration.
- **The synthesis paths line up 1:1.** Touch analog → −1..1 + dead-zone path; a gamepad **left
  stick** feeds the *same* path. Touch d-pad → boolean directions; a gamepad **d-pad** feeds the
  same. Touch N-buttons → the gamepad **face buttons**. So the `btn()` augment the gamepad item
  wants is the same code the touch widget already feeds `btn()` through.
- **It closes a polish-list request directly.** "Sometimes we only want 2 on-screen buttons but
  still have A/B trigger them too" (cart-polish-punchlist) is literally "the on-screen buttons and
  the gamepad buttons are the same logical buttons" — true for free once both read one declaration.
- **Shared focus model for knob carts.** `ui-widgets-notes.md` wants gamepad focus-ring navigation
  for the knob/slider carts; the same d-pad-moves-focus / A-activates logic the touch d-pad emits is
  what that needs.

**Recommendation:** don't build gamepad as a parallel track. Land touch first (it forces the
`touch_layout()` declaration + the `btn()`-augment synthesis layer into existence), then the gamepad
item is mostly "read the physical pad and feed the *existing* synthesis layer" — a much smaller job
than #7 looks in isolation. Worth a one-line note on STATUS #7 pointing here once touch ships.

## Open questions / forks

- **Draw hook.** Cleanest is a studio-owned pass: the cart draws into its `SCREEN_W×SCREEN_H`
  texture as always; the engine blits that into the computed game rect, then draws the controls into
  the letterbox band — all native-res, the cart never has to call anything. (Alternative: expose a
  `draw_touch_ui()` the cart calls itself — more control, more boilerplate.) Recommend studio-owned.
- **Deck vs DOM on web (decided).** The deck/rails are **engine-drawn into a larger framebuffer**,
  not HTML/CSS buttons in the page. DOM controls would be web-only and non-pixel; the engine-drawn
  band is one code path for web + iOS + desktop and stays palette-correct. (`web_shell.html` today is
  a single full-bleed canvas Raylib sizes to `SCREEN_W*SCALE × SCREEN_H*SCALE` — Phase 2 lets the
  framebuffer exceed the game so the band exists.)
- **Cart override of placement.** Default is space-driven. A game that wants the controls hugging the
  action could request `prefer overlay` even when there's room for a deck — a later hint, not v1.
- **Opt-in vs auto.** Explicit `touch_controls(true)` / `touch_layout(...)` (per the polish list),
  or auto-show when `touch_ceiling() > 0` and the cart reads `btn()`? Recommend explicit opt-in in
  code first; auto-detect is a later convenience.
- **Sprites vs primitives for the art.** Primitives first (no asset dependency, trivially
  palette-correct). A code-drawn pixel sprite set (`sprite-draw.js`) is a nice later upgrade for a
  more characterful d-pad/stick.
- **Synthetic input.** `btn()` is read-only today (touch→stick→btn); nothing can *inject* button
  state. Not needed for touch, but worth noting for any future headless/AI/`spec` input path.
- **Per-cart config home.** Where the mode + button set live — a `touch_layout(...)` call in the
  cart, or fields in its `.cart.js` metadata. Decide alongside Phase 3. Recommend the code call
  (carts already opt in via code, per the polish list — *not* settings).

## Implementation plan

Read this before starting; line numbers drift, so **grep for the function names**, don't trust the
~approx hints. Edits are in `runtime/studio.c` (+ `studio.h` for any new API), and Phase 2 touches
`runtime/web_shell.html` — `studio.c` is a **hot shared file**: targeted `Edit`s only, never a full
`Write`, re-Read the region first, and after committing confirm your change survived
(`git show HEAD:runtime/studio.c | grep '<sentinel>'`). See CLAUDE.md "Hot shared source files".

### The seam (where things live today)

- `draw_touch_overlay()` (~`studio.c:912`) — draws stick + A/B with **raw Raylib** (`DrawCircle`/
  `DrawCircleLines`/`DrawTextEx`) in **window pixels**. Called at ~`:1692`, **after** the canvas
  render-texture is blitted to the window. ← this placement is half the problem.
- The canvas is drawn into a `SCREEN_W×SCREEN_H` `RenderTexture2D` between `BeginTextureMode(canvas)`
  / `EndTextureMode()` (~`:1648`–`:1689`), then blitted ×`SCALE` to the window. ← the blit rect is
  what Phase 2 shrinks to open a letterbox band.
- `update_stick()` (~`:876`) — per-frame stick math; called ~`:1500`.
- `btn(0,…)` (~`:2210`) — when `show_touch_ui`, directions read `stick_x()/stick_y()` (~`:2335`) vs
  `STICK_DEADZONE`; A/B `any_touch_in_circle()`. Else keyboard/gamepad.
- Constants (~`:484`): `STICK_RADIUS 60`, `STICK_DEADZONE 0.35`, `BTN_RADIUS 44`.
- `touch_x()/touch_y()` divide window pixels by `SCALE` → canvas space (~`:2257`). ← once the game
  rect is offset/resized for a band, this mapping must account for the game-rect origin/scale, not a
  bare `/SCALE`.

### Phase 1 — native-res look (no API change, no placement change, do first)

Redraw the existing corner overlay with **engine primitives + palette indices**, into the canvas at
native res:

1. Add a studio-internal `draw_touch_overlay_native()` that runs **after the cart's `draw()` returns
   but before `EndTextureMode()`**. Reset transform first — `camera(0,0)` (or the internal
   equivalent) and clear any `clip()` — so the controls sit in fixed screen space regardless of what
   the cart left set.
2. Replace every raw Raylib call with the engine equivalent in **canvas coordinates**:
   `DrawCircle`→`circfill`, `DrawCircleLines`→`circ`, `DrawTextEx`→`print` (or a code-drawn glyph),
   colours as `CLR_*` indices (e.g. `CLR_WHITE` ring on a `CLR_DARK_GREY` pad — see blend-tables for
   index-only translucency if wanted).
3. Convert geometry from window px to **native px**: divide `STICK_RADIUS`/`BTN_RADIUS` by `SCALE`
   (see sizing math). Hit-testing already works in canvas space via `touch_x/y()`; keep
   `btn()`/`stick_x/y()` math unchanged.
4. Delete the old post-blit `draw_touch_overlay()` call (~`:1692`).
5. Verify (below), prototype-bake on `vampire`, confirm controls read as chunky/palette-correct,
   commit.

### Phase 1.5 — coordinate chokepoint as a no-op (de-risk before placement)

The single hard-to-fix risk (silent, library-wide coord breakage) is retired here, *before* the
game rect ever moves. This step must change **nothing** observable.

1. **Introduce `game_rect` (origin + scale)** as the one source of truth for where the game sits in
   the window. Initialize it to the full window (identity).
2. **Route every window↔canvas conversion through it** — `touch_x()/touch_y()` (~`:2257`), mouse
   coords, and any control hit-test — replacing the bare `/SCALE` with the `game_rect` transform.
   With `game_rect = full window` the transform reduces to `/SCALE`, so output is identical.
3. **Extract the placement decision as a pure function** (no rendering, no globals):
   `place(win_w, win_h, screen_w, screen_h) → {mode, game_rect, control_rect}`. In Phase 1.5 it
   always returns `{OVERLAY, full-window, corner}` — the wiring exists, the behaviour doesn't change
   yet.
4. **Headless test** (a `det-probes/` entry or a small harness): over a matrix of window/screen
   sizes assert (a) the placement decision matches expectation and (b) the **round-trip property**
   `canvas→window→canvas == identity` (within ±1px) for sampled points. This is the automated guard
   for the coord bug.
5. Verify the no-op: `canvas-diff` byte-identical on a few carts (incl. a touch/mouse one), commit.

### Phase 2 — responsive placement (deck / rails / overlay)

This is the maker's core requirement. With `game_rect` already the chokepoint (Phase 1.5), placement
is *setting `game_rect`* — the coordinate plumbing already follows. Add the letterbox measure and
three homes:

1. **Fill in the pure `place()` function** (the stub from Phase 1.5): from window size and
   `SCREEN_W×SCREEN_H` compute the aspect-fit game rect and leftover bands, and pick band-below ≥
   min deck height → **DECK**; side bands ≥ min rail width → **RAILS**; else → **OVERLAY** (the
   fallback). Its headless test already exists — extend the matrix to cover the new branches.
2. **Set `game_rect` from `place()`** each frame and shrink the blit so the band is free; draw the
   band background + controls there with engine primitives. The coord plumbing follows for free
   (Phase 1.5). Prefer growing the canvas texture to game+band so `canvas-diff` still covers the
   band, over a second straight-to-window pass.
3. **No input rewrite needed** — `touch_x/y()` already routes through `game_rect`, so taps land
   correctly the moment placement sets it. Just hit-test the controls in band space, and re-run the
   round-trip test in each placement.
4. **Web is deferred to a later step** — keep the Phase-1 corner overlay on web for now; the
   `web_shell.html` framebuffer change (let the canvas exceed the game so a band fits; no DOM
   controls) lands only after native + iOS prove the model.
5. Verify: portrait device (deck), landscape device (rails), matched-aspect window (overlay),
   desktop no-touch (nothing). `canvas-diff` on each.

### Sizing math (the one subtlety)

A physical thumb target should be ≥ ~44 device px. Native size = `ceil(44 / SCALE)`. At 320×200·4×
that's an **11 native-px radius**; at 2× it's 22. So derive control sizes from `SCALE` (and bump on
phones via `touch_ceiling()`), don't hard-code native px. Keep ≥ a few native px of spacing between
buttons so chunky pixels don't merge. In a deck/rail the band height/width sets the ceiling — size
the control to the band, clamped to ≥ the physical target.

### Phase 3 — generalize to a real widget (after placement ships)

Proposed API (new — needs the 4-place dance in CLAUDE.md "Adding a new API function": declare in
`studio.h` w/ one-liner, implement in `studio.c`, document in `studioDocs.js`, list in `shell.js`):

```c
// movement modes
#define TOUCH_NONE       0
#define TOUCH_ANALOG     1   // floating analog stick (free 8-way) — default
#define TOUCH_ANALOG_FIX 2   // fixed analog (base always shown)
#define TOUCH_DPAD4      3   // 4-way d-pad (grid games)
#define TOUCH_DPAD8      4   // 8-way d-pad

void touch_layout(int move_mode, int n_buttons);   // configure once (cart opt-in, in code)
void touch_button_label(int i, const char *txt);    // optional per-button glyph/label
```

- `move_mode` picks stick vs d-pad; `TOUCH_DPAD*` feeds `btn()` directions as booleans (no dead
  zone), analog modes keep the −1..1 + dead-zone path.
- `n_buttons` generalizes the hard-wired 2; lay them out in the reachable arc (deck: bottom; rails:
  right rail; overlay: bottom-right).
- **Floating analog**: on first touch in the move zone, spawn the base at the touch point; let the
  knob drag past the radius (clamp output, optionally re-center base on big drift); smooth-lerp the
  drawn knob. Use `pointer.h` (`PTR_ACQUIRE`) to own the moving finger so a second finger on a
  button doesn't steal it (also why raylib gestures was rejected — see `gestures.h`).
- Config home: a `touch_layout()` call in the cart's `update()`/init is most in-keeping (carts opt
  in via code, per the polish list — *not* settings). A `.cart.js` field is a possible later
  convenience.

### Phase 4 — iOS shell

Route the synthesized inputs through the existing `de_key_event` seam (`ios-plan.md` Phase 2) so raw
`key()` carts (WASD movement) become playable, not just `btn()` carts. Landscape resolves to rails
placement automatically. Mostly Swift-side wiring; the engine side is the same controls reading
touches.

### Verification

- `node tools/canvas-diff.js <cart>` — the controls now go through the software canvas like every
  other primitive; A/B GPU-vs-CPU must match (guards the native-res move).
- Bake + eyeball: `node tools/make-cart.js <c> <png>` then `--run`; read the thumbnail — controls
  should be chunky/palette-correct, not smooth white.
- `node tools/mobile-lint.js <cart>` — sanity on touch-playability.
- Placement matrix: portrait (deck), landscape (rails), matched-aspect (overlay), desktop no-touch
  (nothing). Smoke a `btn()` cart in each to confirm directions/buttons still fire and the input
  mapping survives the game-rect offset.

### Checklist

- [ ] **P1** redraw overlay with `circ`/`circfill`/`print` + `CLR_*`, native-res in the canvas
- [ ] **P1** size from `SCALE` (`ceil(44/SCALE)`), remove post-blit raw-Raylib call
- [ ] **P1** `canvas-diff` + bake on `vampire`, commit
- [ ] **P1.5** `game_rect` chokepoint; route `touch_x/y()`/mouse/hit-tests through it (identity)
- [ ] **P1.5** pure `place()` stub + headless round-trip test; `canvas-diff` byte-identical, commit
- [ ] **P2** fill in `place()`: letterbox measure → deck / rails / overlay decision
- [ ] **P2** set `game_rect` from `place()`, shrink blit, draw band (coords follow for free)
- [ ] **P2** placement matrix verified (portrait/landscape/matched/desktop)
- [ ] **P2+** web framebuffer may exceed game so the band exists (no DOM controls) — after native
- [ ] **P3** `touch_layout(mode, n_buttons)` + d-pad modes (4-place API dance)
- [ ] **P3** floating-analog base-spawn + `pointer.h` capture + knob lerp
- [ ] **P3** N buttons + optional labels; per-cart opt-in in code
- [ ] **P3** prototype `columns`/`puyo` (d-pad) and a free-move cart (analog)
- [ ] **P4** wire `de_key_event` in the iOS shell for `key()` carts

### First move

Phase 1, step 1 — redraw the existing overlay native-res in the canvas. It's the smallest change
that delivers the "looks like part of the game" half, and Phase 2's placement work builds on the
same native-res draw.

---

Sources (external study): [Cursa — touch control patterns](https://cursa.app/en/page/touch-controls-for-mobile-games-input-patterns-and-feedback),
[Scripts for Unity — touch input](https://scriptsforunity.com/blog/touch-input-best-practices-unity/),
[Libretro overlay docs](https://docs.libretro.com/development/retroarch/input/overlay/) (the device-res-overlay standard we reject),
[Parachute — thumb zone](https://parachutedesign.ca/blog/thumb-zone-ux/),
[DesignMonks — button size](https://www.designmonks.co/blog/perfect-mobile-button-size),
[HowToGeek — d-pad vs analog](https://www.howtogeek.com/types-of-games-you-should-play-with-your-d-pad-instead-of-an-analog-stick/),
[Gamereactor — Vampire Survivors touch](https://www.gamereactor.eu/vampire-survivors-now-has-touch-controls-1221203/).
