# Mobile web — making published carts actually playable on a phone

The gallery is live (<https://nikkikoole.github.io/dreamengine/> — see
[`../guides/sharing.md`](../guides/sharing.md)), which means desktop-authored
carts now meet phones. This doc collects what that exposed and the proposed
levers. [`touch-notes.md`](touch-notes.md) owns the touch *API* design; this doc
owns **publishing readiness** — which carts work on a phone at all, and how the
shell/settings/tooling should adapt.

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

Deliberately NOT done: device-resolution rendering. `SCREEN_W/H` are
compile-time constants carts do math against; native-res would break cart
determinism and the fantasy-console aesthetic for marginal sharpness. The open
remainder here is the `fit` policy (§4) — `"integer"` snap for fractional-scale
shimmer, `"stretch"` for aspect-insensitive carts.

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
bool touch_available(void);   // as above
int  device_class(void);      // DEV_DESKTOP / DEV_PHONE / DEV_TABLET
int  touch_ceiling(void);     // max simultaneous fingers: 5 iPhone, 10 iPad,
                              // maxTouchPoints Android, 0 desktop
```

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

## 7. Next steps

1. ~~**`tools/mobile-lint.js` v1**~~ — **SHIPPED 2026-06-05** (checks 1, 3, 4, 5
   from §3 + hover/wheel/right-click warnings). One design lesson from the first
   run: verdicts rank by the *best* phone-usable input path (touch > mouse-tap >
   btn > key), with lesser paths surfaced as `also-reads-keys`-style warnings —
   ranking by worst path branded mostly-mouse carts (ragdoll) keyboard-only.
   First `--site` run over 50 published carts: 2 keyboard-only, 5 fixable,
   34 tap-as-mouse, 3 touch-ready, 6 no-input.
2. **`fit` setting end-to-end** (§4) — `.cart.js` → `de:settings` → both build
   paths → shell. Radios are the first `"stretch"` customers.
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
