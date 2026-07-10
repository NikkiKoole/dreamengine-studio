# Mobile web — making published carts actually playable on a phone

STATUS: LIVING — the publishing-readiness ledger: what phones exposed, the levers, which carts phone-play.

The gallery is live (<https://nikkikoole.github.io/dreamengine/> — see
[`../guides/sharing.md`](../guides/sharing.md)), which means desktop-authored
carts now meet phones. This doc collects what that exposed and the proposed
levers. [`touch-notes.md`](touch-notes.md) owns the touch *API* design; this doc
owns **publishing readiness** — which carts work on a phone at all, and how the
shell/settings/tooling should adapt. **Power/thermals** (phones running hot at a
pinned 60fps — fps cap + on-demand/reactive rendering) is its own topic in
[`frame-pacing.md`](frame-pacing.md); **responsive display** (resizable window,
scale-to-fit, rotation) is §4c below.

Source: first real-device session, 2026-06-05 (iPhone, carts from the live
gallery). Findings are folded in below; touchlab's probe row in
[`probe-carts.md`](probe-carts.md) tracks the formal checklist verdicts.

## 1. What exists today (the inventory)

- **Touch API**: `touch_count/x/y/id`, `tap()`, `tapp()` — per-finger, composing
  (see touch-notes §2). All canvas-space, already correct under CSS scaling.
- **On-screen controls**: `touch_controls(bool)` draws a stick + A/B overlay;
  `TOUCH_CONTROLS_DEFAULT` compile define sets the initial state.
  `tools/build-site.js` already passes `cfg.touchControls` from the `.cart.js`
  — **but no published cart sets it yet**, so every `btn()`-based game on the
  site is currently keyboard-only on a phone.
- **Tiny-target aids (don't shrink the layout — magnify the input):**
  - **`ui_loupe(1)`** (`ui.h`) — a draggable corner-handle 3× lens for
    precision-hitting small knobs/sliders/buttons on a `ui.h` panel. One line;
    shows the whole scene (built on the `zoom_rect` engine primitive). The fix
    `mobile-lint`'s `tiny-target` warning points to. (`uiloupe`,
    [`loupe-notes.md`](loupe-notes.md))
  - **`pinch_scale()`** (`gestures.h`) × **`camera_ex()`** — two-finger
    whole-view zoom, the right tool for a dense surface you *sweep* (drag-paint
    grid, glissando keybed) where a lens-slice won't do. Shipped + working but
    long under-used (only `modrack` wired it) — reach for it.
- **Shell** (`runtime/web_shell.html`): aspect-preserving letterbox
  (`max-width/max-height`), `touch-action: none` + callout/zoom suppression,
  pointer capture for drag-release, rotate-your-phone hint (runtime-checked,
  landscape carts in portrait only).
- **Gallery**: `site/index.html` lists every built cart; no input-capability
  badges yet — a phone visitor can't tell which carts will work for them.

## 2. Device findings (2026-06-05, iPhone)

| Finding | Detail | Consequence |
|---|---|---|
| **Touch point ceiling: 5–6** | iOS Safari caps simultaneous touches (~5 on iPhone; iPads go higher — touchlab's "ten fingers" test was written for iPad) | Design budget for phone carts is **≤5 fingers**. Engine API says `0..10`; carts must not *require* more than 5. Chord-heavy instruments (sh101's two-manual piano) can't get big chords on iPhone |
| **8px font is hard to read** | touchlab's readouts are dense 8px text; at phone size the canvas scales small | HUD/diagnostic text needs phone-aware sizing — or carts intended for phones should write less, bigger. A `print()` at 2× via existing scaling? Cart-level concern, but the static checker (§3) can warn on text-dense carts |
| **Keyboard gates kill carts** | "press Z to start" has no touch equivalent unless `touch_controls(true)` | The single biggest blocker — see §3, checks 1–2 |

## 3. The mobile static checker — `tools/mobile-lint.js` (proposed)

Per-cart report card answering "how is this cart not perfect for mobile?".
Cheap source-level checks first (grep-grade, like `lint-carts.js`); the goal is
a *worklist*, not perfection. Checks, in priority order:

1. **Keyboard-only input** — cart reads `btn()/btnp()/key()/keyp()` but never
   calls any `touch_*`/`tap*`/`mouse_*` *and* doesn't enable `touch_controls`
   (no call, no `touchControls: true` in `.cart.js`). → unplayable on phone.
   The fix is usually one line (`touchControls: true`) since the stick + A/B
   overlay feeds `btn()`.
2. **Key-gated start** — even a touch-capable cart often opens with
   "press Z/X/SPACE". Heuristic: `btnp(`/`keyp(` in the first N lines of
   `update()` while a `state`/`title` variable gates everything. Weakest check
   statically — may just be a manual checklist item, or: any cart that reads
   keys at all gets flagged "verify the start gate is tappable".
3. **Touch budget > 5** — cart logic indexing touches above 4
   (`touch_x(5)`, loops to `touch_count()` needing >5 to function). Rare but
   now a known hardware ceiling (§2).
4. **Mouse-only affordances** — `mouse_x/y` *without* button reads = hover
   reliance (no touch equivalent); any right-button read (no right-click on
   touch).
5. **Touch target size** — `tap()/tapp()` rects smaller than ~16×16 canvas px.
   Apple's HIG floor is 44pt; a 320-wide canvas fullscreen on an iPhone is
   ~2.5–3 pt per canvas px, so 44pt ≈ **15–17 canvas px**. Static check is
   approximate (rect args are often variables) — flag literal small rects,
   leave the rest to a touchlab-style button-size test page.
6. **Output** — a per-cart verdict (`ok / fixable / keyboard-only`) printable
   as a table, and (later) an `"input"` tag in `index.json` — vocabulary owned
   by `lint-carts.js` as usual — so the gallery can badge carts (📱 / 🖮) and
   phone visitors don't tap into dead carts.

## 4. Screen fit policy — letterbox vs stretch (settings)

Today the shell always letterboxes (aspect preserved, page-background bars).
Proposal: a per-cart **fit** setting in `.cart.js` → `de:settings` → shell:

| `fit` | Behavior | When |
|---|---|---|
| `"letterbox"` (default) | current behavior — aspect preserved, bars | games where proportions matter (everything today) |
| `"stretch"` | fill the viewport, distort | aspect-insensitive carts — radios/ambient visuals |
| `"integer"` | snap to whole-number scale, bars | pixel-purist carts; avoids shimmer/uneven pixels |

Implementation is small: `build-site.js` (and the editor's build-web) injects
the policy into the shell's CSS per build, same pattern as
`TOUCH_CONTROLS_DEFAULT`. The rotate-hint already reads the canvas at runtime
and keeps working under all three.

## 4b. Fullscreen & home-screen install — SHIPPED 2026-06-05

Two different problems wearing one name:

- **Desktop / Android / iPad**: the Fullscreen API works → the shell has a small
  ⛶ button (bottom-right, HTML — a key shortcut would collide with "a key the
  cart reads is the cart's key"). It hides itself where the API is missing.
- **iPhone**: Safari has **no Fullscreen API for canvas**. The legit path is
  **Add to Home Screen**: `apple-mobile-web-app-capable` meta in the shell + a
  per-cart `manifest.json` (generated by `build-site.js`, `display:
  "fullscreen"`, the cart's `.cart.png` as icon) → an installed cart launches
  standalone, no browser chrome. Each cart becomes its own app icon — the tr-808
  as a home-screen app is very much the spirit of the thing.

Open question noted 2026-07-02, see [`window-fill-scaling.md`](window-fill-scaling.md): this ⛶
button is per-cart-page only — whether the *gallery* list page (`build-site.js`'s `site/index.html`)
wants its own fullscreen affordance hasn't been looked at.

Deliberately NOT done: device-resolution rendering. `SCREEN_W/H` are
compile-time constants carts do math against; native-res would break cart
determinism and the fantasy-console aesthetic for marginal sharpness. The open
remainder here is the `fit` policy (§4) — `"integer"` snap for fractional-scale
shimmer, `"stretch"` for aspect-insensitive carts.

## 4c. Resizable window & dynamic scale-to-fit — "responsive", honestly split

"Responsive?" (researched 2026-06-10) is really **two** asks, and conflating them
is the trap. The dividing line is whether the **cart** sees a different size:

**(a) Display-responsive — scale the fixed canvas to whatever space exists.**
Cheap and safe, because the canvas stays a fixed-res `RenderTexture2D` and *only
the blit rectangle changes*; the cart never knows. This is the `fit` policy (§4)
plus, on native, a **resizable window**:

- **SHIPPED 2026-07-01, opt-in only:** `DE_RESIZABLE=1` sets `FLAG_WINDOW_RESIZABLE` before
  `InitWindow` (a dev/preview env var, off by default — see [`touch-controls.md`](touch-controls.md)'s RAILS work).
  The blit dest rect already reads the live window size every frame via `gr_place()`/`game_rect`
  (built for touch-controls' DECK/RAILS/OVERLAY placement, not for this), so resizing "just works"
  for placement — but the *scale* it picks is always the largest INTEGER multiple, never a
  fractional fill of the extra space. That gap — and why it matters far more once this lands on
  web/iOS than it does for desktop dev-preview — is its own doc now:
  [`window-fill-scaling.md`](window-fill-scaling.md).
- **Web is already display-responsive**: CSS `max-width/height: 100%` + flex-center
  letterboxes to the viewport and rescales touch coords through it
  (`web_shell.html`). `fit` just swaps that CSS per build — same integer-vs-fractional question
  as the native case, per `window-fill-scaling.md`.

**(b) Layout-responsive — the cart reflows to the real size, like a web app.**
This needs `SCREEN_W/H` to become **runtime variables**, which breaks the
determinism + pixel-art contract the whole engine leans on (and the ~45 carts that
do arithmetic against fixed dims). **Recommendation: do not make this global.** At
most it's an opt-in capability for a narrow class (UI tools, dashboards, radios that
want to fill the screen) — and even then it's a real project, not a setting. Keep it
behind the same wall §4b already drew for device-resolution rendering.

**The maker's call (2026-07-02, see `window-fill-scaling.md`):** for iOS and the product being
built to sell, (b) is the likely better route over pursuing (a)'s fractional-fill further — a
reflowed layout reads as a real app; a scaled-up fixed canvas reads as a scaled-up fixed canvas,
however crisp. Doesn't retire (a) (a game viewport still needs to fit itself into whatever the
reflow gives it), but it does mean `responsive-layout.md`'s graduation path is probably the
higher-value thread to pull on next, not a `gr_place()` fractional-scale extension.

**What that opt-in mode's API would look like — [`responsive-layout.md`](responsive-layout.md).**
Rather than guess, that doc picks a **four-primitive layout set** (flex / fluid
`clamp`+% / anchor+inset / aspect) and **prototypes it in cart-land first**: the
`respond` cart (`tools/carts/respond.c`) lets you drag the corner of a *fake*
screen and watch a real layout reflow (button bar flips row→column at a width
breakpoint), so we get to feel whether the set is right **before** `SCREEN_W/H`
ever become runtime vars. The candidate `runtime/lay.h` is rect-in/rect-out, so
nothing changes when it graduates from `vs` to a real `safe_rect()`.

**Phone rotation.** Because dims are compile-time, you can't swap portrait↔landscape
dims at runtime. So rotation is either *letterbox the fixed canvas* (what (a) gives
you — the existing rotate-hint already copes) or *the cart truly relayouts* (that's
(b), the hard one, and the GLFW touch shim breaks under a CSS transform anyway —
§4b). Ship (a); treat rotation-reflow as out of scope until one specific cart needs it.

## 5. Mobile vs desktop divergence — what a cart can't currently know

A cart has no way to ask "am I on a touch device?" — so it can't auto-enable
`touch_controls(true)`, swap "press Z" for "tap to start" text, or enlarge its
UI. Proposal: **`bool touch_available(void)`** — web: emscripten touch-support
check; native desktop: false. One function, then carts self-adapt:

```c
if (touch_available()) { touch_controls(true); /* bigger buttons, tap-to-start */ }
```

Needs the full 4-place wiring (studio.h / studio.c / studioDocs.js / shell.js)
plus a cart that exercises it, per CLAUDE.md. Shell-side (no API): small-screen
CSS tweaks if needed, and verifying iOS audio unlock (touch-notes §5.6) — the
radios make first-tap audio behavior very visible.

**Extension (researched 2026-06-06, after device finding #5 — the iPhone
6th-finger mass-cancel made device class actionable):** grow the one function
into a small device-facts trio, same plumbing precedent as the `Module.deTouches`
touch mirror (shell computes once at boot → `EM_JS` read):

```c
bool touch_available(void);   // as above — still open
int  device_class(void);      // DEV_DESKTOP / DEV_PHONE / DEV_TABLET — still open
int  touch_ceiling(void);     // SHIPPED 2026-06-06: 5 iPhone, 10 iPad,
                              // maxTouchPoints Android, 0 desktop
```

**`touch_ceiling()` SHIPPED 2026-06-06** (the first of the trio — built when the
6th-finger mass-cancel made it concrete): `web_shell.html` computes
`Module.deTouchCeiling` at boot using the detection facts below; `studio.c`
reads it via EM_JS under `PLATFORM_WEB` (native returns 0); full 4-place wiring
done. First customer: **touchpiano's header** prints "this device tracks max N
fingers" on touch devices (display-only — the compact-layout retrofit was
deliberately skipped for now). The other two functions stay open.

Detection facts (the Apple traps, so nobody re-derives them):

- **iPhone** is honest — UA contains `iPhone`.
- **iPad lies**: since iPadOS 13 Safari masquerades as a desktop Mac
  (`Macintosh; Intel Mac OS X`). The canonical tell: UA says Mac **and**
  `navigator.maxTouchPoints > 1` → iPad (real Macs report 0).
- **`navigator.maxTouchPoints` can't give the iOS ceiling**: it reports 5 on
  BOTH iPhone and iPad, while the iPad really delivers ~10 (touchlab-measured).
  So the ceiling is *derived*: iPhone → 5, iPad → 10, Android →
  `maxTouchPoints` (digitizers report truthfully there), desktop → 0.
- **No physical screen size exists on the web** — CSS px are roughly constant
  angular size, so the standard proxy is `min(innerWidth, innerHeight)`:
  ≲ 500 → phone-class, ~500–900 → tablet-class, above → desktop; combine with
  `matchMedia('(pointer: coarse)')` for finger-vs-mouse.

First customers: touchpiano (`touch_ceiling() <= 5` → one octave, fatter keys —
makes the 6th-finger nuke unreachable in normal play), any cart swapping
"press Z" for "tap to start", tiny-font carts choosing `FONT_NORMAL` on
phone-class screens. Native: all three compile-time (desktop / no touch / 0)
until a native touch platform exists.

**The ceiling-safeguard pattern (why `touch_ceiling()` matters).** The
6th-finger mass-cancel (touch-notes device finding #5) **cannot be blocked** —
iOS abandons the whole gesture at the OS level; no web API intercepts it, and
by the time the cart hears about it every contact is already cancelled. The
safeguard is therefore two layers, neither of which is "stop the nuke":

1. **Prevention by design** — the ceiling is a *budget* read at startup; shape
   the interaction so deliberate play stays at or under it:
   ```c
   void init(void) {
       if (touch_ceiling() > 0 && touch_ceiling() <= 5) {
           octaves = 1;   // fewer, fatter keys: chords need 3-4 fingers,
       }                  // finger #6 stops being something players reach for
   }
   ```
   Don't build the playground at the edge of the cliff.
2. **Graceful failure anyway** — the nuke still happens *accidentally*: a
   resting palm or the edge of a hand counts as contacts, so a legitimate
   5-finger chord + a grazing palm = 6 = mass-cancel. Every touch cart must
   land sane after one: all holds released, no stuck state, play resumes
   instantly (touchpiano already passes). The signature is detectable if a
   cart wants to react — all contacts ended on one frame while
   `touch_count()` sat at the ceiling — e.g. flash a "too many fingers!"
   toast instead of leaving the player wondering why their chord died.

## 5c. `pget` / canvas read-back on web — WORKS (opt-in), via our own `glReadPixels`

`pget`, `pget_rgb`, and `touching_color()` read the rendered canvas back from the GPU.
The read-back is **opt-in via `enable_pget(true)`** (off by default on both platforms — it
costs a GPU stall + memory, so only carts that read pixels pay; see studio.h). With it on:

- **Native** uses raylib's `LoadImageFromTexture` (unchanged — works, no error).
- **Web** does its *own* `glReadPixels` on the canvas FBO (`studio.c`, the `#else` branch).

**Why not `LoadImageFromTexture` on web:** it runs an ES3-only framebuffer *format probe*
(`glGetFramebufferAttachmentParameteriv`) that WebGL1 rejects with a cosmetic per-frame
`INVALID_ENUM`. The read-back itself works regardless, but the console spam is ugly. We
don't need the probe — the canvas is always RGBA8 — so the web branch binds the FBO via
`rlEnableFramebuffer(canvas.id)` and calls a plain `glReadPixels(... GL_RGBA,
GL_UNSIGNED_BYTE ...)` into a reused buffer: no probe (clean console), no per-frame alloc
(no churn), bottom-up RGBA so `pget`'s Y-flip is unchanged. `glReadPixels` is in the
universal GL subset (every backend raylib supports), and it's **web-gated** — native and
any future native target stay pure raylib, so no portability cost.

**Validated 2026-06-11:** desktop Chrome + iOS Safari both return correct pixels with a
clean console (the `pgetweb` diagnostic cart). The old "disabled on web, needs WebGL2"
note was over-cautious — readback was always functional; only the probe-spam was the issue.

**Caveats:**
- A cart must call `enable_pget(true)` or reads return empty (one-time log warning fires).
- Feedback/collision carts already in the gallery (shadelab, inkrunner, collision-lab-3,
  lemmings, splatoon, blendlab) need a **republish** to pick up this engine change before
  they work on the live web build.
- Mobile perf: it's a synchronous per-frame `glReadPixels` (a TBDR stall). Fine for the
  diagnostic; a heavy full-canvas feedback cart may run warm. A WebGL2 **PBO async read**
  (collect 1–2 frames later — matches `pget`'s already-last-frame contract) is the future
  perf upgrade if needed, but no longer a correctness blocker.

## 6. Gestures — status unchanged, now with a device

Raylib's rgestures (swipe/pinch/hold/drag) exist one `#include` below us;
touch-notes §4 deliberately parked wrapping them ("park until evidence") —
single-global-gesture is the wrong shape for games, and detector quality on
real hardware was unknown. **The live gallery removes the testing blocker**:
the next touchlab revision can add a gesture readout page and answer the
quality question on-device in minutes. Until then the lean stands: swipes are
~10 cart lines once touch-release lands (touch-notes §3), and a `gestures.h`
library header may beat engine API (ADR-0006 instinct).

## 6b. Which key() reads are actually a problem — and the keycap idea

Precision on "reads keys = bad", because the overlay changes the calculus
(verified in `studio.c:1427` — the on-screen stick + A/B feed **`btn()` only**;
`key()/keyp()/keyr()` are invisible to it, *even for arrow keycodes*):

| The cart reads | On a phone | Fix |
|---|---|---|
| `btn(0, …)` | works once the overlay is on | `touchControls: true` — one line |
| `key(KEY_LEFT/RIGHT/UP/DOWN)`, `key('Z'/'X')` | dead — overlay doesn't feed `key()` | switch those reads to `btn()`; it's what btn is for |
| `key(KEY_SPACE)`, `key('R')`, `key('[')`, … | dead, and **unsynthesizable** — no overlay can guess them | this is the real gap → keycaps, below |

*(A runtime alternative for row 2 — stick also satisfying `key()` for the six
mapped keycodes — would work, but teaching carts to use `btn()` for movement is
the cleaner fix; the API already says so.)*

### Tappable key labels — "keycaps"

> **Worklist:** [`keycap-audit.md`](keycap-audit.md) — a swarm read all 348 carts and
> bucketed which need a keycap (100, with the exact chips extracted), which just need the
> `touchControls` flag (114), which need a real redesign, and which are desktop-only. Start
> there to pick what to retrofit. **Not built yet** — `runtime/keycap.h` is still a proposal.

The unsynthesizable keys are almost always already *on screen as text*: every
radio prints `SPACE next · R again · [ ] history` in its help line. The idea:
make that label **be** the button. One call draws a keycap-styled chip and
returns true when the key is pressed **or** the chip is tapped:

```c
// draws [SPACE] next song   — returns true on keyp(KEY_SPACE) OR a tap on the chip
if (keycap(x, y, "SPACE", "next song", KEY_SPACE)) next_song();
```

Why it's the right shape:

- **Self-documenting** — the control hint and the touch target are the same
  pixels; no separate mobile UI to design, and desktop users see the same thing
  they see today.
- **Composable with the lint** — a keycap cart reads `tapp()`, so it counts as
  touch-ready automatically; no special-casing.
- **Cheap** — it's `rect` + `print` + `keyp` + `tapp`, all existing API.
  ~30 lines.
- **Sizing rule carries over** — chips must respect the ≥16 canvas-px floor
  (the lint's tiny-target check applies as-is).

Shape: start as a **cart-land helper header** (`runtime/keycap.h`, the
`radio.h`/`improv.h` precedent — ADR-0006 instinct: a library header beats
engine API until many customers prove the need). Promote to `studio.h` only if
it wants runtime privileges (key claiming, pause-overlay theming). First
customers: the radios — their help line becomes a row of caps and they jump
🟡 → 🟢 without a redesign.

### But the real policy: design touch-first, keys as accelerators

The keycap is a **retrofit** tool — the cheap bridge for the ~40 shipped carts
whose help lines name keys. For *new* carts the better idea is to not depend
on the keyboard in the first place:

- **Controls are on-screen icons/buttons**, labeled by *action*, not by key:
  `⏭` next song, `⏸` pause, `↻` again — drawn rects read with `tapp()`.
  Every action reachable by tap; the UI looks identical on desktop and phone.
- **Keys become optional accelerators** that mirror the buttons — SPACE also
  fires `⏭`, silently. Power users keep their shortcuts; the help line
  doesn't have to advertise them (or does so subtly).
- **Movement stays `btn()`** — stick overlay on phone, arrows/WASD on desktop.

The hierarchy, then: **new carts → touch-first icons** (keys mirror them);
**existing carts → keycap retrofit** (the label becomes the button);
**movement → always `btn()`**, never `key(KEY_LEFT)`. A cart designed this
way lints 🟢 with no special effort.

## 6c. Native text input — there isn't one

`text_input()` (Raylib `GetCharPressed`) only ever sees a **hardware
keyboard**. On mobile web there is no way to summon the OS virtual keyboard
from a canvas: iOS/Android only open it when a real DOM `<input>`/
`<textarea>` gains focus, and the emscripten/GLFW shim renders to a bare
`<canvas>` — no input element, no keyboard, ever. So "type your name" is
silently impossible on a phone today.

Options if a cart needs name entry on mobile:

1. **Arcade-style initials picker** — 3 slots, arrows/taps cycle A–Z. Solved
   in 1980, touch-friendly by construction, zero new machinery. The right
   default for hiscores.
2. **In-cart tap keyboard** — a drawn QWERTY grid of `tapp()` rects; pairs
   naturally with the keycap header (§6b) — it's just rows of caps.
3. **Shell-level hidden `<input>`** — `web_shell.html` focuses an invisible
   DOM input and forwards its events into the wasm key queue. The only way to
   get the *real* OS keyboard (autocorrect, emoji, IMEs), but it's per-shell
   plumbing, fights fullscreen, and the soft keyboard reflows/obscures the
   canvas. Park unless a cart genuinely needs free text on phones.

Lint angle: `text_input()` reads are now surfaced as a `text-input` warning —
dead path on any touch device.

## 6d. Web debug overlay — cable-free on-device visibility (designed 2026-06-06)

> **v1 SHIPPED 2026-06-07** exactly as designed below — `runtime/debug-overlay.js`
> (site-root copy via `build-site.js`) + the baked loader in `web_shell.html`.
> Open: `?debug=1` or triple-tap/click the top-left corner; ✕ or the same
> gesture closes. v2 (the `watch()` push + `web_tm_*` readout) still open —
> and it's a one-file republish now, no cart rebuilds.

Motivated by one iPad afternoon producing three on-device mysteries (muted
WebAudio, the §-touch-notes-7 phantom, the finding-#6 tap-death) with zero
visibility — native has the editor overlay + trigger-file inspection, web has
nothing without a Mac and a cable.

**v1 (pure shell JS, no engine changes):**
- live touch state: a dot per `Module.deTouches` contact + count/ids readout —
  makes phantom-vs-tap-death diagnosis a 5-second visual ("a dot stayed" vs
  "dots appear but the game ignores them" = mouse path)
- printh/console mirror: last ~10 `console.log` lines on screen
- JS error capture: `window.onerror` → red banner instead of a frozen canvas
- fps from the rAF loop

**v2 (small engine assist):** the cart's `watch()` values pushed once per frame
via EM_JS — the native `watch()` debug workflow, identical on a phone — plus
the `web_tm_*` mouse-synth state (now load-bearing).

**Architecture rule (the expensive lesson):** the shell is baked into every
cart at emcc time — every shell tweak costs a full-catalog rebuild (~13 min ×
263 carts, twice on 2026-06-06 alone). So: a ~5-line LOADER in the shell
(`?debug=1` → inject `<script src="../debug-overlay.js">`, silent if absent)
and all overlay logic in ONE site-root file. Iterating on the overlay then
means republishing one JS file, zero cart rebuilds. Activation: `?debug=1`, or
triple-tap the top-left corner mid-session. General principle worth extending:
the less logic baked per-cart, the cheaper web iteration gets.

**Zero-code alternative that already works:** iPad + USB cable + Mac Safari →
Develop menu → remote Web Inspector = full console into a running cart. The
overlay's value is being cable-free and game-legible.

## 6e. Web MIDI — SHIPPED 2026-06-12 (desktop browsers only)

A USB MIDI keyboard plays published carts on the web, via the same `midi_get()` API as
native. The bridge is `runtime/web_midi.js` (injected by emcc `--post-js` in both build
paths): it drives `navigator.requestMIDIAccess()` and feeds note/bend events into the same
ring the cart drains. Full design: [`midi-and-keybed.md`](midi-and-keybed.md).

Mobile/web facts worth knowing:
- **Desktop only, Chromium + Firefox.** Web MIDI exists in Chrome / Edge / Opera / Firefox.
  **Safari does NOT** ship it, and **iOS has no Web MIDI at all** (any browser) — so this is a
  desktop-laptop feature, not a phone one. The bridge no-ops cleanly where it's absent.
- **Secure context required** — `https://` or `localhost`. A cart opened as a `file://`
  won't get MIDI; the published GitHub-Pages site (https) is fine.
- **Permission prompt** — the browser asks once to allow MIDI; the bridge handles a denial.
- **Pre-connected devices** — a keyboard already plugged in at page load can enumerate late
  and may not fire a `statechange`, so the bridge polls until a device appears, then pops a
  small on-page toast naming it (and re-announces on a real hot-plug).
- Not a `mobile-lint` axis — MIDI is an *additive* input on desktop; carts stay fully
  playable by touch/keys without it.

## 7. Next steps

1. ~~**`tools/mobile-lint.js` v1**~~ — **SHIPPED 2026-06-05** (checks 1, 3, 4, 5
   from §3 + hover/wheel/right-click warnings). One design lesson from the first
   run: verdicts rank by the *best* phone-usable input path (touch > mouse-tap >
   btn > key), with lesser paths surfaced as `also-reads-keys`-style warnings —
   ranking by worst path branded mostly-mouse carts (ragdoll) keyboard-only.
   First `--site` run over 50 published carts: 2 keyboard-only, 5 fixable,
   34 tap-as-mouse, 3 touch-ready, 6 no-input.
2. **`fit` setting end-to-end** (§4) — `.cart.js` → `de:settings` → both build
   paths → shell. Radios are the first `"stretch"` customers. Resizable native
   window + dynamic scale-to-fit share this work (§4c).
2b. **Power: fps cap + reactive rendering** ([`frame-pacing.md`](frame-pacing.md)) —
   the phone-heat fix. Lever A (`-DTARGET_FPS`) is the ~5-line quick win; Lever B
   (on-demand `repaint()`) is the bigger win for the static-but-audio radios.
3. **`touch_available()`** (§5) — 4-place wiring + first self-adapting cart.
4. **touchlab on iPhone, formally** — run the §5 checklist from touch-notes,
   write verdicts into probe-carts.md; add a button-size page (how small can a
   `tapp()` rect be and stay hittable?) and a gesture readout page (§6).
5. ~~**Gallery badges**~~ — **SHIPPED 2026-06-05**: `build-site.js` requires
   `mobile-lint.js` and stamps a colored chip on every gallery card, four
   tiers: **🟢 Mobile Ready — plays perfectly** (touch/tap-driven AND zero dead
   input paths — if there's a keyboard button to press somewhere, it's not
   perfect) / **🟡 Mostly Playable — works, with rough edges** (dead *extras*:
   `also-reads-keys`, `btn-without-overlay`, right/middle, `touch>5`; plus the
   no-input radios — the music plays, the key controls don't) / **🟠 Rough on
   Mobile — technically runs, expect pain** (dead *core* interaction: hover or
   wheel — things a touch screen simply doesn't have) / **🔴 Desktop Only —
   good luck past the title screen** (fixable + keyboard-only; until
   `touchControls` actually lands, a btn()-cart still needs a keyboard). First
   run over 50 carts: 4 ready (heldnotes, multitouch, stylophone, touchlab) /
   34 mostly / 5 rough (dreamcad, modrack, spacecho, sh101, tb303 — the
   knob-and-wheel synth UIs, accurately) / 7 desktop. The static guess is
   overridable per cart: hand-test on a device, then put
   `"mobile": "ready"|"mostly"|"rough"|"desktop"` in the cart's `index.json`
   entry — manual verdict beats lint (it can't see key-gated title screens
   etc.). Iterated three times same-day: emoji-glyph chips → plain three-tier
   words → this colored-dot four-tier set.
6. **Touch-first control policy + keycap retrofit** (§6b) — write the policy
   into `guides/cart-authoring.md` (new carts: on-screen action icons via
   `tapp()`, keys as silent accelerators, movement via `btn()`); prototype
   `runtime/keycap.h` and retrofit one radio as the proof (expected 🟡 → 🟢).
7. ~~**Lint: list the keys each cart reads**~~ — **SHIPPED 2026-06-05**:
   `keys(SPACE,R,M,…)` warning on every cart that reads literal keycodes —
   the manual-testing worklist and the keycap-retrofit shopping list in one;
   `text_input()` reads surfaced as a `text-input` warning (§6c — no OS
   keyboard on mobile web, ever).
