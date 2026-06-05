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

## 6. Gestures — status unchanged, now with a device

Raylib's rgestures (swipe/pinch/hold/drag) exist one `#include` below us;
touch-notes §4 deliberately parked wrapping them ("park until evidence") —
single-global-gesture is the wrong shape for games, and detector quality on
real hardware was unknown. **The live gallery removes the testing blocker**:
the next touchlab revision can add a gesture readout page and answer the
quality question on-device in minutes. Until then the lean stands: swipes are
~10 cart lines once touch-release lands (touch-notes §3), and a `gestures.h`
library header may beat engine API (ADR-0006 instinct).

## 7. Next steps

1. **`tools/mobile-lint.js` v1** — checks 1, 3, 4 from §3 (the grep-grade
   ones); run it over the 28 published carts and use the output to pick which
   get `touchControls: true` first.
2. **`fit` setting end-to-end** (§4) — `.cart.js` → `de:settings` → both build
   paths → shell. Radios are the first `"stretch"` customers.
3. **`touch_available()`** (§5) — 4-place wiring + first self-adapting cart.
4. **touchlab on iPhone, formally** — run the §5 checklist from touch-notes,
   write verdicts into probe-carts.md; add a button-size page (how small can a
   `tapp()` rect be and stay hittable?) and a gesture readout page (§6).
5. **Gallery badges** — surface the lint verdicts on the site so phone
   visitors see what's playable before tapping.
