# Touch controls — a native-resolution on-screen joystick / d-pad / buttons

STATUS: BUILDING (rev. 2026-07-01). Shipped: P1 (native-res look — later superseded by the
pivot's window-space skin, see below), P1.5 (`game_rect` coordinate chokepoint), P2 brain
(`gr_place()` deck/rails/overlay decision + the letterbox foundation — `game_rect =
gr_place(...)` each frame, opt-in `DE_WINDOW=WxH` desktop preview), `touch_layout(mode,
n_buttons)` with **all four move modes** (`TOUCH_ANALOG` / `TOUCH_ANALOG_FIX` fixed vs
floating stick, `TOUCH_DPAD4` / `TOUCH_DPAD8` 4-/8-way d-pad), the **band-aware layout + the
desktop smooth-circle draw skin** (live stick knob, compass-node d-pad — the item this file
used to list as NEXT), and **band-relative control sizing** (`ctrl_scale` shrinks the controls
to fit a tight deck/rail band, floored so they stay tappable, hit-test and draw always in
sync). Oracle: `tools/det-probes/placetest.c` (placement) + headless mouse-as-touch traces for
d-pad/sizing correctness (the window-space draw itself has no automated oracle — see PIVOT).
(Also fixed en route: the stick was dead for the desktop mouse — `stick_x/y` rejected the
mouse-as-touch id.)

**Bugs found live-testing the d-pad (all fixed, worth knowing if you touch this code again):**
`TOUCH_DPAD8` thresholded x/y independently, so almost any off-axis touch fired two unrelated
directions at once — replaced with a single 45°-wide angle sector per touch (a genuine diagonal
still sets two `btn()` booleans, correctly, just never a spurious third). The d-pad's finger
acquisition reused the analog stick's whole-zone grab area (sized for a *floating* stick that
recenters under your finger) — a fixed d-pad shouldn't fire from a tap on the far side of the
screen, so it now has its own small local grab radius. The 8-way compass dots were drawn at a
radius too small to fit 8 without touching — shrunk the dot size so they clear.

Also shipped: **N action buttons (0-4)** — the draw skin and `btn()` hit-testing only ever wired
up 2, so this first required growing the core input vocabulary itself with `BTN_X`/`BTN_Y`
(numbered to append, not insert — room for shoulder/trigger/stick-click buttons later, see
STATUS #7). Named to match the real Xbox/PlayStation/Switch A/B/X/Y face-button vocabulary, not
an arbitrary C/D — the direction going forward is real-controller support, not just PICO-8's
2-button model (the maker's call, prompted by generalizing this exact touch work). Up to 4
buttons diamond-lay around one anchor per placement (A bottom, B right, X left, Y top); a
`touch_debug()` introspection API (`touch_layout_mode()`/`touch_ctrl_scale()`/`touch_debug(bool)`)
also shipped alongside, for eyeballing band/grab-zone/placement without guessing.

> **ARCHITECTURE PIVOT (2026-06-30) — supersedes the "grow the framebuffer" plan below.** The
> band rendering hit two walls: `sw_cbuf` is a fixed `SCREEN_W*SCREEN_H` array (growing it =
> surgery on the fragile software rasterizer), and the controls can't be eyeballed by the
> canvas tooling. **New approach (maker's call): one shared BRAIN in the engine** — geometry
> (`gr_place`), hit-testing, and the `btn()`/stick synthesis, all platform-agnostic and already
> built — **plus a thin per-platform DRAW SKIN** that just paints the controls at the geometry
> the brain exposes (desktop = raw Raylib, web = canvas/DOM, iOS = native view). Controls are
> drawn with **smooth circles at device resolution**, NOT chunky engine primitives — they read
> as standard thumb-chrome, not part of the console (a deliberate reversal of the original
> native-res requirement; controls are chrome, and smooth reads better on a phone; revisit
> pixelating later if wanted). This **reverses the "Deck vs DOM on web (decided)" fork below**.
> The band only exists where the window ≠ game aspect (iOS/web); desktop uses `DE_WINDOW` to
> preview. Trade-off accepted: window-space controls are invisible to canvas-diff/dumps, so the
> *draw* is eyeballed (maker) while the *brain* stays oracle-tested (`placetest` + the trace).

Next: N action buttons beyond the hard-wired 2 (the draw skin still caps there), the
vocabulary-row prototype cart (`columns`/`puyo` for d-pad, a free-move cart for analog — also
the reference-image source for "Layout vocabulary" below), the editor's cart-reflective
settings readout + force-show override, then iOS shell wiring and web deck/rails (both
deferred by design — see "Platform targeting").
Research, a recommendation, AND a concrete implementation plan follow (see "Implementation
plan"). The task is
pulled from **two ends**: the iOS port ([`ios-plan.md`](ios-plan.md) — raw `key()`/`btn()` carts
have no touch equivalent on a phone) and the cart-polish backlog
([`../cart-polish-punchlist.md`](../cart-polish-punchlist.md) — ~18
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

## External study, round 2 (2026-07-01) — control-scheme gaps found researching successful games

A second research pass (prompted by "are we missing other types of joysticks/pads?"), specifically
hunting for schemes beyond stick/d-pad that big, successful games actually ship. Each item below
survived 3-vote adversarial fact-checking (independent verifiers trying to refute it, ≥2/3 agreement
required) unless marked otherwise — the source is linked so you can go look yourself, not just take
the summary on faith:

- **Move-only stick + fully automatic fire — [Archero](https://www.deconstructoroffun.com/blog/2019/8/9/why-archero-banked-25m-but-leaves-25m-hanging-hlx9n)
  ([2](http://scottfinegamedesign.com/design-blog/2019/7/2/archero-part-1-gameplay)).** A floating
  stick with **zero action buttons**: dragging moves (and disables firing); releasing/standing still
  auto-fires at the nearest enemy. Tactical depth comes purely from movement — dodge into position,
  stop to shoot. **This is already our "single floating stick / 0 buttons" row** (the one cited to
  Vampire Survivors, above) — Archero is a second, independently-confirmed real game running the
  exact same pattern. Nothing to build; `touch_n_buttons = 0` already covers it.
- **Stick style as a PLAYER setting, not just a design-time pick —
  [Mobile Legends](https://bittopup.com/article/Mobile-Legends-Controls-Guide-2025-Pro-Settings-Setup).**
  MLBB lets the player choose fixed-position vs. floating/dynamic joystick from Settings, not just the
  cart author at build time. Cuts against this doc's current stance ("cart declares in code, not
  settings" — see "Open questions" below) — at minimum, the editor's planned force-show/readout panel
  (Phase 4b) is a natural place for a player-facing override, even if the cart still sets the default.
- **Targeting-resolution modes layered on top of a move+attack split — Mobile Legends (same source).**
  Movement stick fully decoupled from a separate attack button, and *how* a tap-attack picks its
  target is itself configurable: "Standard" (nearest-enemy auto-target), "Advanced Targeting"
  (precise manual control over turrets/minions vs. auto), "Hero Lock Mode" (locks all attacks/skills
  onto one selected hero). Not a stick shape — a targeting-resolution axis on top of an attack button.
  Relevant if a cart ever wants auto-aim/lock-on assist.
- **Reduced-input racing: auto-drive + directional swipe only at decision points —
  [Asphalt 9 "TouchDrive"](https://www.touchtapplay.com/asphalt-9-controls-settings-guide/)
  ([2](https://www.pocketgamer.com/articles/076788/asphalt-9s-stripped-back-controls-are-the-best-way-to-play-the-game-heres-five-reasons-why/)).**
  No stick or d-pad at all. Steering + acceleration are fully automatic; the *only* directional input
  is a left/right **swipe**, and it only does anything at route forks/ramps — elsewhere it's inert.
  Manual input otherwise is hold-timing (drift) and tap (nitro). **Gap:** the planned-but-unimplemented
  `TOUCH_NONE` ("buttons-only") row currently implies static tap zones; this needs a directional
  *swipe* treated as a discrete button-like input, not a continuous stick.
- **Gesture-discrimination on a single zone —
  [Marvel Contest of Champions](https://www.bluestacks.com/blog/game-guides/marvel-contest-of-champions/battle-system-marvel-contest-of-champions-en.html).**
  One screen zone (not three buttons) resolves *tap* → light attack, *tap-and-hold* → heavy attack,
  *swipe* → medium attack — three different outputs from one physical region, discriminated by
  gesture shape, not by which of several fixed zones was touched. Plus a separate charged-special
  icon tied to a resource meter. **Gap:** today a "button" is one fixed zone → one boolean; nothing
  lets one zone yield *different* actions for tap vs. hold vs. swipe.
- **Apple's own on-screen controller is *more* constrained than ours —
  [`GCVirtualController`, WWDC21 session 10081](https://developer.apple.com/videos/play/wwdc2021/10081/).**
  Apple's official virtual-controller API explicitly does **not** let apps freely place individual
  control elements — layout comes from a small fixed set of Apple-chosen configurations (buttons +
  exactly one of thumbstick/d-pad/touchpad per side). Our band-aware, per-cart-declared placement is
  already strictly more flexible than the platform's own baseline. Practically relevant for Phase 4
  (iOS shell): `GCVirtualController` is how iOS surfaces on-screen touch as a real system-level
  "gamepad" any app can read via GameController framework — worth evaluating whether to bridge to it
  (Control Center integration, consistency with real MFi controllers) vs. staying fully hand-rolled,
  when that phase starts.
- **Not confirmed, but well-known — gyro-tilt aim assist and multi-finger "claw" layouts** (PUBG
  Mobile / Call of Duty Mobile). Sources repeatedly described toggling the gyroscope while scoped in
  for fine flick-shots, and splitting move/aim/fire across 3–4 simultaneous touches — but the
  *specific* finger-mapping claims got refuted on inconsistent details between guides (unsurprising:
  claw configs are player-customizable, not a fixed spec). The general pattern is real and
  industry-standard, but it's a different *kind* of feature — device tilt/rotation as a continuous
  input axis, layered independently of touch — not a joystick/d-pad shape. Bigger scope than the
  `touch_layout()` vocabulary; a separate future engine capability (no `touch_ceiling()`-style
  detection or API exists for it today), not a vocabulary-table gap.

**Recommendation, ranked by fit with the existing architecture:** (1) gesture-per-zone buttons
(tap/hold/swipe → different outputs from one zone) — the biggest genuine gap, fits naturally once
N-buttons work lands (see Checklist); (2) directional-swipe as a discrete input for the still-unbuilt
`TOUCH_NONE` row — needed to actually cover the reduced-input racing pattern the vocabulary table
already gestures at. The MLBB targeting-mode and player-facing stick-toggle findings are "note for
later" (folded into "Open questions" below), not new engine work right now.

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
[`api-notes.md`](api-notes.md) §15) are the **same seam from two directions**, and `touch_layout()`
is the shared foundation:

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
- **Shared focus model for knob carts.** [`ui-widgets-notes.md`](ui-widgets-notes.md) wants gamepad focus-ring navigation
  for the knob/slider carts; the same d-pad-moves-focus / A-activates logic the touch d-pad emits is
  what that needs.

**Recommendation:** don't build gamepad as a parallel track. Land touch first (it forces the
`touch_layout()` declaration + the `btn()`-augment synthesis layer into existence), then the gamepad
item is mostly "read the physical pad and feed the *existing* synthesis layer" — a much smaller job
than #7 looks in isolation. Worth a one-line note on STATUS #7 pointing here once touch ships.

## Platform targeting — web vs native iOS

**There is only one virtual gamepad.** The engine-drawn controls, the placement logic, and the
`btn()`/`de_key_event` synthesis all live in `studio.c` and compile once — *identical on both
platforms*. The only platform-specific surface is two thin boundaries: where touch contacts come
from, and whether the render surface has been reshaped for a band yet.

| | **Web (wasm)** | **Native iOS** |
|---|---|---|
| Touch source → engine | Already wired: `web_shell.html` mirrors contacts into `Module.deTouches`; `touch_ceiling()` via EM_JS. Engine reads from there — no new plumbing. | Swift shell delivers `UITouch`; raw-`key()` carts get input via the existing `de_key_event` seam (Phase 4). |
| Render surface | One full-bleed `<canvas>` Raylib sizes to `SCREEN_W*SCALE × SCREEN_H*SCALE`. Reshaping it to fit a band is the **fragile** part (touch-mirror + GLFW bounding-rect mapping). | Software canvas → on-screen texture (`ios-plan.md`); we own the surface, so growing it for a band is straightforward. |
| Typical placement | Portrait browser → **deck below** (PICO-8 layout). | Landscape action pose → **side-rails**. |
| Controls drawn as | Engine primitives in the framebuffer — **not** DOM/HTML buttons (decided). | Same engine primitives, identical code. |
| Band reshape order | **Deferred** — stays on the Phase-1 corner overlay until native proves the model (the canvas touch-mirror is what breaks on a real phone, not desktop). | **First** — deck/rails land here where the surface is ours. |

So: same gamepad, two ~50-line input adapters, and web's deck deferred *behind* iOS's — not two
parallel implementations. The one genuinely web-specific risk (canvas reshape vs the touch-mirror)
is pushed last and de-risked by iOS first.

## The editor setting (`touchControls`) — becomes cart-reflective + a force-show override

Today the editor has a global checkbox *"show on-screen stick + A/B buttons"*
(`editor/src/settings.js:260`, persisted to `localStorage.touchControls`, compiled in as
`-DTOUCH_CONTROLS_DEFAULT`). As a **global force-on for every cart** it directly contradicts the
opt-in model (it would slap a generic stick on carts that declared a d-pad, or none). So it changes
from *a behavioural default* into **two clearer things**:

1. **A cart-reflective readout (read-only).** When you load a cart, the panel **reads the cart's own
   declared layout** and shows it — *"this cart: D-pad + 2 buttons"* (or *"twin-stick"*, or *"none"*).
   It mirrors what the cart set; you don't edit the cart's controls from settings. This is the
   maker's call: the setting reads *from* the cart, so it always agrees with the cart.
2. **A force-show override (the one editable toggle).** *"force-show on desktop (preview)"* — on a
   no-touch desktop the controls normally don't draw (`touch_ceiling() == 0`); this forces the cart's
   declared layout visible so you can eyeball placement/feel without a phone. It previews the cart's
   *declared* scheme; if the cart declared none, it falls back to analog+2 just so placement is
   visible. It changes nothing about the cart or other platforms — purely a desktop preview aid.

**Implication for where the layout is declared.** For the panel to "read from the cart" at load
*without running it*, the declaration must be **statically readable** — which favours putting the
layout in the cart's `de:meta` block (e.g. `touch: { move: "dpad4", buttons: 2 }`) over a
runtime-only `touch_layout()` call. (This nudges the "Per-cart config home" fork below toward
metadata; the runtime can still call `touch_layout()` for carts that change scheme mid-game, but the
*declared default* lives in `de:meta` so the editor + compendium can show it.)

## Open questions / forks

- **Twin-stick (e.g. `robotron`) — needs a second move zone.** Today's model assumes ONE move
  control (stick or d-pad) + a button cluster; a twin-stick game needs TWO simultaneous sticks
  (move + aim) both live-drawn and hit-tested at once. Already flagged in "Layout vocabulary" as
  unimplemented; noted here explicitly with a concrete motivating cart so it isn't lost. Real
  layout-thought needed: where does a second stick go in DECK (both hands need reachable space
  below?) vs RAILS (one stick per rail, naturally) vs OVERLAY (two corners, tighter fit)? RAILS
  is probably the easy case; DECK and OVERLAY need actual design work, not just "add a slot."
  **Deliberately not building this now** — leaving the seam (this note + the vocabulary table
  row) for whoever picks it up.
- **Two-player touch layout — currently doesn't exist at all.** `show_touch_ui`/`touch_layout()`
  are wired to player 0 only; player 1 has no touch path whatsoever (keyboard/gamepad only, see
  `btn()`'s `player == 1` branch). Supporting local 2-player on one touchscreen means splitting
  DECK/RAILS/OVERLAY real estate between two players' controls — genuinely tight on a phone,
  especially combined with the twin-stick case above (a 2-player twin-stick game would want 4
  sticks + up to 8 buttons live on one screen at once). This needs dedicated layout thoughtwork
  (halves? corners? a fundamentally different mode for 2P?), not a quick extension of the current
  one-anchor-per-control-type model. **Deliberately not building this now** — flagging the gap so
  it's a known, intentional scope boundary rather than a silent omission discovered later.
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
- **Per-cart config home (leaning metadata).** Where the declared mode + button set live. Earlier
  lean was a `touch_layout(...)` code call; the cart-reflective editor readout (above) needs it
  **statically readable**, which favours a `de:meta` field (`touch: { move, buttons }`) as the
  *declared default*, with the code call kept for runtime scheme changes. Decide alongside Phase 4.
- **Player-facing stick-style override (from "External study, round 2").** Mobile Legends lets the
  *player* pick fixed-vs-floating from settings, not just the cart author. Cuts against "cart
  declares in code, not settings" above — the editor's Phase 4b force-show panel is a natural home
  for a player override if this is ever wanted, without changing the cart-declares-default model.
- **Targeting-resolution modes (from round 2).** Mobile Legends layers a configurable *targeting
  mode* (nearest-auto / manual-precise / locked-on-one-target) on top of a plain attack button —
  orthogonal to move-mode/button-count. Not designed yet; note it here so it's not forgotten if a
  cart wants auto-aim/lock-on assist.
- **Gesture-per-zone buttons (from round 2, real gap).** Today one button = one fixed zone = one
  boolean. Marvel Contest of Champions gets 3 outputs (light/heavy/medium attack) from ONE zone by
  discriminating tap / hold / swipe. Would need `btn()` (or a new call) to expose which gesture fired,
  not just whether the zone is down — bigger than a sizing tweak, a real API shape question.
- **Directional swipe as a button (from round 2, real gap).** Asphalt 9's "TouchDrive" reduced-input
  scheme (auto-drive everywhere, swipe left/right only matters at route forks) needs a discrete
  swipe-direction input for the still-unbuilt `TOUCH_NONE` row — today's buttons are static tap
  zones, not gesture-direction detectors.

## Layout vocabulary (the named schemes)

The schemes a cart can declare — these are what the reference images (below) should depict, one per
row, shown in deck / rails / overlay placements:

| Name | `move_mode` | buttons | Fits |
|------|-------------|---------|------|
| **single floating stick** | `TOUCH_ANALOG` | 0 | vampire-survivors / auto-fire (move only); also archero (round 2) |
| **twin-stick** | `TOUCH_ANALOG` ×2 | 0 | robotron/twin-stick shooters (move + aim) |
| **analog + A/B** | `TOUCH_ANALOG` | 2 | platformers, free-move action |
| **d-pad + A/B** | `TOUCH_DPAD4` | 2 | grid/precise (sokoban, rogue, puzzle) |
| **d-pad8 + N** | `TOUCH_DPAD8` | 3–4 | fighters, 8-way action |
| **buttons-only** | `TOUCH_NONE` | 1–4 | tetris/columns (rotate/drop), tap games; reduced-input racing (asphalt 9 "TouchDrive", round 2) needs a swipe-direction button variant, see "Open questions" |

**Shipped: `TOUCH_ANALOG`, `TOUCH_ANALOG_FIX`, `TOUCH_DPAD4`, `TOUCH_DPAD8`** — so "analog + A/B"
and both d-pad rows are real today via `touch_layout()`. **Not yet implemented:** `TOUCH_NONE`
(buttons-only row — there's no move-mode constant for "no stick/d-pad at all" yet) and
twin-stick (needs the second analog — a Phase-3+ extension of `n_buttons`/a second move slot;
noted here so the vocabulary is complete). **Reference images**: best produced as bakes from a
small prototype cart that renders each row (the still-open `columns`/`puyo` + free-move
prototype from the plan below doubles as the image source); a quick mockup can lock the
vocabulary earlier.

## Plan of attack (ordered)

Each step is **independently shippable + verifiable + reversible** — commit between steps. Ordered
so the one hard-to-fix risk (silent coord breakage) is retired *before* the feature it threatens,
and the fragile web reshape lands *last*, after native proves the model.

| # | Step | Size | What it de-risks / delivers | Gate before commit |
|---|------|------|------------------------------|--------------------|
| 1 | **Native-res look** — redraw the existing corner overlay with `circ`/`circfill`/`print` + `CLR_*`, sized from `SCALE`; delete the post-blit raw-Raylib call. No API/placement change. | S | "Looks like the console" half; improves the ~18 opt-in carts. No new risk. | `canvas-diff` on `vampire`; bake + eyeball (chunky, palette-correct). |
| 2 | **Coordinate chokepoint (no-op)** — `game_rect` (origin+scale) as the one window↔canvas transform; route `touch_x/y()`/mouse/hit-tests through it at `game_rect = full window`. Add pure `place()` stub (always OVERLAY) + headless round-trip test. | S–M | **The big one** — converts the silent, library-wide coord bug into a tested, provable no-op. | `canvas-diff` **byte-identical** on a touch/mouse cart; round-trip test passes. |
| 3 | **Responsive placement (native + iOS)** — fill in `place()` (deck/rails/overlay by letterbox); set `game_rect` from it, shrink the blit, draw the band. Coords follow for free. Web stays on the step-1 overlay. | M | The maker's core requirement. Blast radius bounded to opt-in carts. | Placement matrix (portrait/landscape/matched/desktop); `canvas-diff` each. |
| 4 | **Generalize the widget** — `touch_layout(mode, n_buttons)` + `de:meta touch:{}` declared default (analog / fixed / dpad4 / dpad8), N buttons + labels, feel tuning. 4-place API dance. Build a **prototype cart that renders every layout-vocabulary row** — it doubles as the reference-image source. | M | The per-cart scheme choice; the `btn()`-augment layer gamepad reuses; the reference images. | Prototype `columns`/`puyo` (d-pad) + a free-move cart (analog); bake the vocabulary rows. |
| 4b | **Editor settings rework** — turn `touchControls` into the cart-reflective readout (reads the cart's `de:meta touch:`) + the *force-show on desktop (preview)* override; relabel + note. | S | Setting agrees with the cart; desktop preview without a phone. | Load carts with/without a declared layout; toggle force-show on desktop. |
| 5 | **iOS shell wiring** — route synthesized input through `de_key_event` so raw `key()` carts work; landscape → rails automatically. | M | Phones can play `key()` carts, not just `btn()`. | Smoke `key()` + `btn()` carts on device. |
| 6 | **Web deck/rails** — let the `web_shell.html` framebuffer exceed the game so a band fits (no DOM controls). | M | The deferred fragile bit, now de-risked by step 3. | On-device web: portrait deck, touch-mirror intact, `canvas-diff`. |
| 7 | **Gamepad (STATUS #7), follow-on** — read the physical pad, feed the *existing* synthesis layer; add the focus ring for knob carts. | S–M | Closes #7 cheaply by reusing step 4. | Test one cart on a real controller. |

Steps 1–2 are the safe, high-value foundation and can land now. Step 3 is the first change with real
blast radius — but bounded to opt-in carts and resting on step 2's tested transform. Everything from
4 on is additive.

## Implementation plan

Read this before starting; line numbers drift, so **grep for the function names**, don't trust the
~approx hints. Edits are in `runtime/studio.c` (+ `studio.h` for any new API), and the web-deck step
touches `runtime/web_shell.html` — `studio.c` is a **hot shared file**: targeted `Edit`s only, never
a full `Write`, re-Read the region first, and after committing confirm your change survived
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
   colours as `CLR_*` indices (e.g. `CLR_WHITE` ring on a `CLR_DARK_GREY` pad — see
   [`blend-tables.md`](blend-tables.md) for index-only translucency if wanted).
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

### Sizing math (the one subtlety) — SHIPPED as `ctrl_scale`

This section originally reasoned about *native-pixel* sizing (before the pivot moved the draw
skin to window space). What actually shipped: `ctrl_scale` (0..1), recomputed every frame in
`layout_touch_controls()` from the tight dimension of the current band (DECK: band height,
RAILS: rail width; OVERLAY: always 1.0, there's no band to shrink to), floored at
`CTRL_SCALE_MIN = 0.6` so a control never shrinks below a tappable size, ceilinged at 1.0 so a
roomy band doesn't blow controls up past their normal size. Every hit-test and draw call reads
`eff_stick_r()`/`eff_btn_r()` (`STICK_RADIUS`/`BTN_RADIUS * ctrl_scale`) — never the bare
constants — so the drawn size and the tappable size can't drift apart. One current wrinkle:
`GR_MIN_BAND = 90` (the smallest band `gr_place()` will choose DECK/RAILS for at all) only ever
drives `ctrl_scale` down to 0.75 in practice — the 0.6 floor is a safety margin, not something
you'll see hit today; it'd only bite if `GR_MIN_BAND` were lowered later.

### Phase 3 — generalize to a real widget (after placement ships)

**Shipped API** (the 4-place dance from CLAUDE.md "Adding a new API function" — declared in
`studio.h`, implemented in `studio.c`, documented in `studioDocs.js`, listed in `shell.js`):

```c
// movement modes (all four shipped)
#define TOUCH_ANALOG      0   // floating analog stick (free 8-way) — default
#define TOUCH_ANALOG_FIX  1   // fixed analog (base always shown)
#define TOUCH_DPAD4       2   // 4-way d-pad (grid games) — exclusive, dominant-axis-wins sector
#define TOUCH_DPAD8       3   // 8-way d-pad (diagonals) — single 45°-sector resolution

void touch_layout(int move_mode, int n_buttons);   // shipped — configure once (cart opt-in, in code)
void touch_button_label(int i, const char *txt);    // NOT YET shipped — optional per-button glyph/label
```

Also shipped, in the CORE input vocabulary (`studio.h`, not just touch — every backend reads
these): `BTN_X` (6) / `BTN_Y` (7), alongside the existing `BTN_A`/`BTN_B`. Named to match the real
Xbox/PlayStation/Switch A/B/X/Y face-button set, not an arbitrary C/D — the direction going
forward is real-controller support (STATUS #7's gamepad item), not just PICO-8's 2-button model.
Numbered to *append*, not insert, so shoulder/trigger/stick-click buttons can be added later
without renumbering anything. Default keyboard: P0 continues Z/X into C/V, P1 continues J/K into
L/`;` — both remappable in the editor's settings → controls panel like A/B always were.

(No `TOUCH_NONE` — there's no "buttons-only, no stick/d-pad" mode yet, see "Layout vocabulary.")

- `move_mode` picks stick vs d-pad; both d-pad modes feed `btn()` directions as booleans by
  resolving the touch to one compass sector (angle from the move-zone home) — DPAD4 snaps to 4
  sectors (dominant axis, no diagonals), DPAD8 to 8 (adjacent cardinals combine into one diagonal
  sector, never three directions at once). Analog modes keep the −1..1 + dead-zone path via
  `stick_x()/stick_y()`.
- `n_buttons` (0-4: A/B/X/Y) is shipped — `layout_action_buttons()` diamond-lays them around one
  anchor point per placement (A bottom, B right, X left, Y top — the classic face-button
  arrangement), radius scaled by `eff_btn_r()` so the cluster can't overlap itself at any
  `ctrl_scale`. The stick/d-pad grab-zone exclusion and the button hit-test both loop over
  `touch_n_buttons` via one `touching_action_button()` helper, so drawn size, hit-test size, and
  active-button count can't drift apart from each other.
- **Floating analog** (shipped): on first touch in the move zone, spawn the base at the touch
  point; the knob clamps to `eff_stick_r()` (not the raw constant — see "Sizing math"). Knob
  smooth-lerp and `pointer.h` (`PTR_ACQUIRE`) finger-ownership are still open (a second finger on
  a button can't currently steal the stick's finger, but there's no explicit test for it either).
- **D-pad** (shipped): same finger-tracking shape as the stick, but a **small local grab radius**
  around the d-pad's own home point (`DPAD_GRAB_RADIUS`) — not the stick's whole-zone `sgz` —
  since a d-pad is a fixed widget, not a floating one; a tap far from the graphic correctly fires
  nothing.
- Config home: a `touch_layout()` call in the cart's `update()`/init is most in-keeping (carts opt
  in via code, per the polish list — *not* settings). A `.cart.js` field is a possible later
  convenience (see "Per-cart config home" fork below).

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

- [x] **P1** redraw overlay with `circ`/`circfill`/`print` + `CLR_*`, native-res in the canvas
- [x] **P1** size from `SCALE` (`ceil(44/SCALE)`), remove post-blit raw-Raylib call
- [x] **P1** `canvas-diff` 0px on `flyover` (opt-in); `vampire` PASS; committed
- [x] **P1.5** `game_rect` chokepoint; routed `touch_x/y`/`tap*`/`touch_ended*`/`inp_mouse_x/y` + blit through it (identity). (On-screen control hit-tests read window px directly — unchanged.)
- [x] **P1.5** pure `gr_place()` stub + headless round-trip test (`det-probes/placetest.c`, covers offset+scaled rects too); `canvas-diff` byte-identical (flyover/vampire), committed
- [x] **P2** fill in `gr_place()`: letterbox measure → deck / rails / overlay decision (pure, in `game_rect.h`)
- [x] **P2** set `game_rect` from `gr_place()` each frame + shrink blit (letterbox foundation; `DE_WINDOW` preview, `ClearBackground` bars). Game letterboxes; **band DRAW is now the per-platform skin** (see PIVOT) — shipped, see below.
- [x] **P2** placement matrix verified — `det-probes/placetest.c`: matched→overlay (exact identity), portrait→deck, landscape→rails, tiny-band→overlay; computed rects round-trip
- [x] **P3** `touch_layout(mode, n_buttons)` shipped (4-place dance) with `TOUCH_ANALOG` (floating) / `TOUCH_ANALOG_FIX` (fixed); verified fixed-vs-floating via trace.
- [x] **P3** floating + fixed analog base behaviour (fixed pins base to home, floating spawns under finger). `pointer.h` capture + knob lerp still TODO.
- [x] **band-aware layout + desktop smooth-circle draw skin** (was NEXT above) — `layout_touch_controls()` places the stick/d-pad + buttons into the actual DECK/RAILS/OVERLAY geometry each frame; the desktop skin paints smooth window-space circles after the blit (live stick knob, or compass-node d-pad).
- [x] **P3** `TOUCH_DPAD4` / `TOUCH_DPAD8` d-pad move modes — resolve to one 45°-wide angle sector per touch (DPAD4: 4 sectors, dominant axis; DPAD8: 8 sectors, diagonals combine two adjacent cardinals, never three at once); a small local grab radius around the d-pad's own home (not the stick's whole-zone `sgz`); 8-way node draw sized to clear (no visual overlap). Verified headless via mouse-as-touch traces (exclusive bucketing, a clean diagonal, out-of-zone rejection).
- [x] **P3** control sizing scales to fit a tight deck/rail band — `ctrl_scale` (floored/ceilinged), `eff_stick_r()`/`eff_btn_r()` used everywhere so hit-test and draw stay in sync. Verified via `DE_WINDOW` (forces a phone-shaped window so DECK/RAILS trigger) that a shrunk button's tap radius shrinks with its visual, and a huge band clamps to 1.0 rather than growing unbounded.
- [x] **debug introspection** — `touch_layout_mode()` / `touch_ctrl_scale()` / `touch_debug(bool)` (band + grab-zone outlines, placement/scale text), for eyeballing placement work without guessing. Backs the permanent `dpaddemo` test-bench cart.
- [x] **P3** N action buttons (0-4: A/B/X/Y, diamond-laid) — required first adding `BTN_X`/`BTN_Y` to the CORE input vocabulary (not just touch — `btn()`/`btnp()`/`btnr()`, keyboard defaults, the editor's remap panel), since only A/B existed before. Verified headless: all 4 diamond positions hit exclusively; unused slots (e.g. `n_buttons=2`) correctly fire nothing even when tapped.
- [x] **P3** real-cart d-pad + analog validation, then the FULL affects-list sweep — rather than new throwaway prototype carts, wired `touch_layout()` into every existing cart that had a `de:meta.todo` request for exactly this (16 total). Started with 3 (`columns`, `puyo` — `TOUCH_DPAD4`, 1 button; `08-catchstar` — `TOUCH_ANALOG`, 0 buttons), then swept the rest of `action-plan.md`'s affects-list once the pattern was proven: `boids`, `doomfire`, `10-world` (analog, 0 — pure steering, no action anywhere in the cart); `defender`, `galaga`, `vampire`, `09-enemies`, `rollswarm`, `jumpstar` (analog, 1 — free move + a lone restart/action button); `drmario`, `sandburrow` (d-pad, 1 — grid + rotate/restart); `rogue`, `towerdefense` (d-pad, 2 — grid cursor + two distinct actions). Every mode/button-count choice was read off the cart's actual `btn()` usage, not guessed from genre (`jumpstar` looks like a 2-button platformer but jumping is fully automatic, so it only needed 1 button for "jump again after a fall"; `vampire` is auto-fire in play but still needs 1 button for its level-up popup). Each fulfilled todo removed (one, `09-enemies`, kept a *different*, still-open half of its todo — an editor/tutorial-launcher intro-screen issue unrelated to the joystick). Verified: all 16 build and run headless with no error; the underlying dpad/button mechanics were already exhaustively proven in isolation (see the entries above), so this validated the *integration* (does `touch_layout()` wired into a real cart's `init()` actually work end-to-end), not the primitives again.
- [ ] **P3** a dedicated vocabulary-row REFERENCE cart (bakes one image per named scheme, deck/rails/overlay) — still open; `columns`/`puyo`/`08-catchstar` above are real gameplay validation, not the reference-image source itself. `touchpad` (the pixel-art mockup) is the closest existing thing but predates the real API.
- [ ] **P4b** editor settings rework — cart-reflective readout (reads the cart's declared layout) + force-show-on-desktop override (see "The editor setting" section)
- [ ] **P4** wire `de_key_event` in the iOS shell for `key()` carts

### First move (updated 2026-07-01 — everything below "Checklist" is now open, not this)

Two independent options, pick either: (a) the vocabulary-row reference cart (bakes an image per
named scheme for "Layout vocabulary" below), or (b) start on the editor settings rework (Phase
4b) now that there's real per-cart touch data (`columns`/`puyo`/`08-catchstar`) to reflect in the
readout.

---

Sources (external study): [Cursa — touch control patterns](https://cursa.app/en/page/touch-controls-for-mobile-games-input-patterns-and-feedback),
[Scripts for Unity — touch input](https://scriptsforunity.com/blog/touch-input-best-practices-unity/),
[Libretro overlay docs](https://docs.libretro.com/development/retroarch/input/overlay/) (the device-res-overlay standard we reject),
[Parachute — thumb zone](https://parachutedesign.ca/blog/thumb-zone-ux/),
[DesignMonks — button size](https://www.designmonks.co/blog/perfect-mobile-button-size),
[HowToGeek — d-pad vs analog](https://www.howtogeek.com/types-of-games-you-should-play-with-your-d-pad-instead-of-an-analog-stick/),
[Gamereactor — Vampire Survivors touch](https://www.gamereactor.eu/vampire-survivors-now-has-touch-controls-1221203/).

Sources (external study, round 2 — 2026-07-01, see "External study, round 2" above):
[Deconstructor of Fun — Archero](https://www.deconstructoroffun.com/blog/2019/8/9/why-archero-banked-25m-but-leaves-25m-hanging-hlx9n),
[Scott Fine Game Design — Archero gameplay](http://scottfinegamedesign.com/design-blog/2019/7/2/archero-part-1-gameplay),
[BitTopup — Mobile Legends controls guide](https://bittopup.com/article/Mobile-Legends-Controls-Guide-2025-Pro-Settings-Setup),
[TouchTapPlay — Asphalt 9 controls guide](https://www.touchtapplay.com/asphalt-9-controls-settings-guide/),
[Pocket Gamer — Asphalt 9's stripped-back controls](https://www.pocketgamer.com/articles/076788/asphalt-9s-stripped-back-controls-are-the-best-way-to-play-the-game-heres-five-reasons-why/),
[BlueStacks — Marvel Contest of Champions battle system](https://www.bluestacks.com/blog/game-guides/marvel-contest-of-champions/battle-system-marvel-contest-of-champions-en.html),
[Apple WWDC21 session 10081 — GCVirtualController](https://developer.apple.com/videos/play/wwdc2021/10081/).
