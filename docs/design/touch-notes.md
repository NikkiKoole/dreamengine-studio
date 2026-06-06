# Touch — multitouch, gestures, and what a real device must answer

> **Genre: design exploration (scratchpad).** Where the touch API stands, what's
> deliberately missing (release events, gestures), and the checklist of questions
> only a physical touchscreen can answer. The on-device probe is the **`touchlab`**
> cart — pick it up the day an iPad is available.
>
> **2026-06-06: the web phantom-touch bug is root-caused (emscripten `wontfix` +
> raylib single-removal) with a concrete fix plan → §7.** gestures.h follow-ups
> (2-finger pan vs pinch, id-keyed pinch pair) → §8.
>
> Status of shipped/open items → [`../STATUS.md`](../STATUS.md). Probe verdicts →
> [`probe-carts.md`](probe-carts.md).

---

## 1. What exists today

The touch API after `touch_id()`/`tapp()` landed (commit `1d7bf50`):

| | held? | began this frame? | released this frame? |
|---|---|---|---|
| **buttons** | `btn()` | `btnp()` | `btnr()` |
| **keyboard** | `key(k)` | `keyp(k)` | `keyr(k)` |
| **mouse** | `mouse_down()` | `mouse_pressed()` | `mouse_released()` |
| **touch** | `tap(x,y,w,h)` | `tapp(x,y,w,h)` | `tapr(x,y,w,h)` (§3, shipped 2026-06-05) |

Plus the enumeration view: `touch_count()`, `touch_x(i)`, `touch_y(i)`,
`touch_id(i)` — up to 10 raylib touches per frame, canvas-space coordinates.

**The virtual touch pool** (`studio.c`, `vt_*` statics): raylib touches merged
with mouse-as-touch on desktop — the mouse LMB appears as a synthetic touch with
id `-2` (`MOUSE_TOUCH_ID`). So every touch cart is testable single-finger with a
mouse, and `tap()`/`tapp()` "just work" on desktop. Pool cap `VT_MAX = 16`. The
pool also snapshots the previous frame's **ids** (`vt_prev_id[]`) — that's what
makes `tapp()` edge-triggered.

**The web path** (how touch carts reach a device): settings → *Build for web*
(emcc + raylib-web) → the preview server binds `0.0.0.0:8765` and the build log
prints the LAN url — open it on an iPad/phone on the same wifi.
`runtime/web_shell.html` sets `touch-action: none`, `user-scalable=no`,
`overscroll-behavior: none` so iOS Safari (in theory — see §5) doesn't steal
touches for scroll/pinch/double-tap-zoom.

## 2. The identity model — why touch is the odd one out

Every other input has a stable address: `keyp('A')`'s keycode *is* the
identity, assigned by hardware, valid before, during, and after the press. A
touch is **anonymous until you give it identity** — the engine only knows
"3 contacts at these coordinates", and the *index* of a contact shuffles the
moment another finger lifts. `touch_id(i)` exposes the stable id raylib already
tracks, so a cart can follow *the same finger* across frames.

The consequence: anything per-finger (a two-hand piano, pinch, multi-finger
drag) is cart-side bookkeeping — keep last frame's `(id, x, y)` list, diff
against this frame's:

- id present now, absent before → finger **landed** (this one the engine also
  answers rect-wise via `tapp()`)
- id present in both → finger **moved** (delta = drag)
- id absent now, present before → finger **lifted** ← *now engine-answered:
  `touch_ended_count/id/x/y` + `tapr()` (§3, shipped 2026-06-05)*

`tools/carts/multitouch.c` (paint) and `tools/carts/touchlab.c` (device test
card) both implement the diff; touchlab is the reference copy.

## 3. The release API — SHIPPED 2026-06-05 (with the touch piano)

**Why it's absent:** a released touch is a *ghost* — the finger has no index
this frame, so the existing `touch_x(i)`-style API physically cannot address
it. Raylib has the same hole (`GetTouchPosition(i)` covers live contacts only).
A release API is a new *shape*: the runtime must also snapshot previous-frame
**positions** (today only ids are kept) and expose the vanished contacts as
their own little list.

**Shipped signatures** (exactly as designed — the snapshot implementation below
landed in `studio.c` unchanged):

```c
int  touch_ended_count(void);            // fingers that lifted since last frame
int  touch_ended_id(int i);              // which finger
int  touch_ended_x(int i);               // where it was last seen (canvas-space)
int  touch_ended_y(int i);
bool tapr(int x, int y, int w, int h);   // did a touch END inside this rect this frame?
```

Implementation is cheap — one more `Vector2 vt_prev_pos[VT_MAX]` snapshot in
`poll_virtual_touches()`, plus a "in prev, not in cur" scan. All five functions
read the same two snapshots.

**Shipped with its designated customer: the touch piano**
(`tools/carts/touchpiano.c`) — two octaves, per-finger note_on/note_off keyed
by `touch_ended_id`, glissando on slide, lift-ripples at `touch_ended_x/y`.
Ten fingers, ids crossing keys, iOS audio — the §5 stress points in one cart.
**`gestures.h` followed the same day** (`runtime/gestures.h`): per-finger
swipes judged at lift time (`gest_update()` + `swiped(dir)` / `swiped_in(rect)`
/ `pinch_scale()`), the cart-land answer §4 called for. touchlab test 9 runs it
side-by-side against rgestures (test 8) — swipe while drumming other fingers;
9 keeps firing where 8 dies.

**DEVICE-CONFIRMED same day (iPhone, live gallery): it works.** Five
simultaneous fingers on the piano — a full chord at Safari's touch ceiling —
each note released by exactly its own finger. The release API + per-finger
model survives contact with hardware on day one.

**Device finding #3 — the ghost contact (FIXED same day):** drag two fingers,
lift one → a stationary ring lingered. Cause: mouse-as-touch (`MOUSE_TOUCH_ID`)
is a desktop affordance, but browsers *emulate* mouse events from touches — iOS
fires a compatibility mousedown around finger-release with no clean mouseup
during multitouch, so the synthetic mouse joined the pool as a stale contact.
Fix in `poll_virtual_touches()`: once any real touch has been seen, the mouse
is never synthesized again that session (a device with fingers doesn't need the
mouse fallback; its "mouse" is an emulation).

## 4. Raylib gestures — what's available, and the lean

Raylib ships a gestures module (`rgestures.h`) the engine doesn't expose:

```c
SetGesturesEnabled(unsigned int flags);   // opt-in mask
bool IsGestureDetected(int gesture);      // this frame
int  GetGestureDetected(void);            // latest of:
//   GESTURE_TAP, GESTURE_DOUBLETAP, GESTURE_HOLD, GESTURE_DRAG,
//   GESTURE_SWIPE_RIGHT / _LEFT / _UP / _DOWN,
//   GESTURE_PINCH_IN / GESTURE_PINCH_OUT
float   GetGestureHoldDuration(void);     // ms
Vector2 GetGestureDragVector(void);       // normalized drag direction
float   GetGestureDragAngle(void);        // degrees
Vector2 GetGesturePinchVector(void);      // pinch displacement
float   GetGesturePinchAngle(void);
```

So "swipe left" etc. exist one `#include` below us. **The lean is to NOT wrap
them yet**, for three reasons:

1. **Quality unknown.** rgestures is a small heuristic detector (one global
   gesture at a time, tuned thresholds, historically single-touch-pair
   oriented). Whether its swipes feel right on an iPad in a wasm build is
   exactly a touchlab question — wrapping a flaky detector bakes the flakiness
   into our API.
2. **Cart-land may suffice** (ADR-0006 instinct). Once releases land, a swipe
   is ~10 cart lines: position at `touch_began`, position at `touch_ended`,
   threshold the delta and the duration. Pinch is two `touch_id`-tracked
   fingers and a distance ratio. A `gestures.h` *library header* (like
   `improv.h`) may beat engine API.
3. **One gesture at a time is wrong for games.** rgestures reports a single
   global gesture; a game wants "this finger is dragging the slider WHILE that
   finger swipes the camera". Our per-id model composes; theirs doesn't.

If device testing shows rgestures is solid *and* carts keep re-rolling the same
detection, the wrapper would be thin: `bool swiped(int dir)` (reusing `BTN_*`
direction constants), `float pinch(void)` (scale factor this frame, 0 = no
pinch). ~~Park until evidence.~~ **Evidence arrived 2026-06-05 (§5 item 8): reason
3 confirmed on an iPhone — a live DRAG is abandoned the instant a second finger
taps. rgestures will not be wrapped; gestures are cart-land, per-finger, on
`touch_id()` (blocked only on the §3 release API).**

## 5. What only a real device can answer — the touchlab checklist

The **`touchlab`** cart (`tools/carts/touchlab.c`, kind `probe`) puts all of
these on one screen. Build for web, open the LAN url on the device, run down
the card:

1. **Coordinate accuracy under CSS scaling.** `web_shell.html` scales the
   canvas with `max-width/max-height`; emscripten's touch handler is supposed
   to compensate via `getBoundingClientRect`. *Verify:* the crosshair ring must
   sit **exactly under the finger**, including after rotating the iPad. If
   offset/drifting, the shell needs a manual scale fix.
2. **Ten fingers.** Put both hands down — `max seen` should reach 10 and stay
   smooth (fps readout on screen).
3. **Id stability.** Hold one finger, tap others around it — the held finger's
   id label must never change. (Safari recycles WebKit touch ids aggressively;
   raylib remaps them, and this is the test that the mapping holds.)
4. **`tapp()` single-fire.** Drum on the TAPS button — counter must +1 per
   physical tap, even at fast tempo (the prev-id snapshot runs per frame; taps
   faster than one frame would alias — physically unlikely at 60fps, verify).
5. **Release diff bookkeeping.** Lift a finger — the LIFT readout must show the
   right id at the right last position (this is §3's logic running cart-side;
   it's also the data the future `touch_ended_*` would return).
6. **iOS audio unlock.** Each landing finger blips. If the first blip is
   swallowed, emscripten's AudioContext-resume-on-gesture isn't firing and the
   shell needs an explicit `resume()` hook.
7. **Browser gesture leak.** Pinch, double-tap, drag, swipe from edges — the
   page must not zoom, scroll, rubber-band, or navigate (tests §1's CSS).
   Also long-press: no callout/magnifier.
8. **rgestures feel** — *wired 2026-06-05*: touchlab line 8 shows the live
   `GetGestureDetected()` readout (+ hold duration, drag vector) and a log strip
   of recent gesture onsets — raw extern decls, no engine API (§4's
   probe-before-wrapping). *Verify:* swipes/flicks fire reliably in the right
   direction, pinch in/out are distinguished, HOLD doesn't false-positive on a
   resting finger, and nothing fires during normal multi-finger play (§4 reason
   3 — one global gesture at a time is the suspected flaw).
   **VERDICT (same day, iPhone): the §4-reason-3 flaw is confirmed on
   hardware** — an in-progress DRAG dies the moment a second finger taps; the
   detector abandons the gesture and re-evaluates globally. That kills rgestures
   for games regardless of how good its swipe detection is. **Decision: never
   wrap rgestures; per-finger gestures live in cart-land** on `touch_id()`
   bookkeeping (a `gestures.h` library header once the release API of §3
   lands). Swipe-reliability/pinch/HOLD-false-positive sub-questions are moot
   for engine purposes — rgestures stays available to carts via extern decls
   (touchlab shows how) for anything single-gesture.

Write verdicts into [`probe-carts.md`](probe-carts.md) (touchlab's row) and fix
what fails; items 1/6/7 are shell bugs if they fail, 2/3/4/5 are engine bugs.

## 6. Cart status

| Cart | Role | State |
|---|---|---|
| `multitouch` (paint) | first multitouch demo — `touch_id` colors, `tapp` CLEAR | shipped, untested on hardware |
| `touchlab` | the device test card (§5, now 9 points: +rgestures readout, +gestures.h side-by-side) | shipped, first iPhone contact 2026-06-05 (gesture verdict in §5.8) |
| `touchpiano` | the release API's customer (§3) | **shipped 2026-06-05**, same commit as the API |

## 7. The web phantom touch point — ROOT CAUSE FOUND (2026-06-06)

> **Fix BUILT & DEVICE-PASSED same day** (phone, live gallery: two-finger
> simultaneous releases drop the count to 0; touchpiano's lift ripples — once
> intermittent because releases were swallowed — now fire on every lift).
> Exactly the plan below:
> `web_shell.html` rebuilds `Module.deTouches` (flat `[id,x,y]` triples, canvas
> pixels, filtered to `touch.target === canvas`) from `event.touches` on every
> touch event; `studio.c`'s `poll_virtual_touches()` reads it via
> `de_web_touch_read` (EM_JS) under `PLATFORM_WEB`, falling back to raylib's
> list if the mirror is absent (returns −1 — older shells keep working).
> Native path untouched, native build verified. Run the acceptance test below
> on a phone before calling it done.

**Symptom** (observed on device, web builds only — native is clean): lift a finger
and its touch point *stays in the list* — `touch_count()` doesn't drop, a stale
contact sits frozen at the last position. Not every time, but often, and **most
reliably when two fingers release at (nearly) the same moment**. Raylib's own
"input multi touch" web example reproduces it; the "input gestures" example
doesn't (explained below). `gestures.h` can't paper over it — garbage in: the
virtual touch pool mirrors raylib's list, so the phantom never produces a
`touch_ended_*` event, lift-judged swipes never fire, and `tap()` rects read as
held forever.

**The cause is two bugs stacked, and the bottom one is `wontfix`:**

1. **Emscripten violates the Touch Events spec** —
   [emscripten#4679](https://github.com/emscripten-core/emscripten/issues/4679)
   (closed `wontfix`). Per W3C, a `touchend`'s `changedTouches` lists *only* the
   lifted fingers and `touches` lists *only* the remaining ones. Emscripten's
   `library_html5.js` instead hands C a `touches[]` that conflates
   remaining + lifted, **with `isChanged == true` on every entry**. So native
   code physically cannot tell which finger lifted from the emscripten event
   alone.
2. **Raylib's web backend removes at most ONE point per `touchend` event** —
   [raylib#4474](https://github.com/raysan5/raylib/issues/4474) ("Touches do not
   release at the correct time"), patched by
   [PR#4488](https://github.com/raysan5/raylib/pull/4488) (in raylib 5.5 — which
   is exactly what `runtime/raylib-web/` vendors) as a blunt
   `pointCount--` per touchend; current master refines it to "find the first
   `isChanged` entry, shift the array up, decrement, **`break`**". Either way:
   a single `touchend` carrying **two** genuine releases (your
   two-fingers-at-once case — the browser batches them into one event)
   decrements once. One phantom remains until some later touch event happens to
   shake it loose. Upgrading raylib does not fix this; the upstream data is
   ambiguous (bug 1) and the removal loop is single-shot by design.

**Why the "input gestures" example looks immune:** `rgestures.h` never re-reads
the raw point array — on any `TOUCH_ACTION_UP` it hard-resets all gesture state
and forces `pointCount = 0`. The phantom still exists in the array; the gestures
module just never looks at it again. (And why it turns a two-finger drag into
pinch/zoom: its `pointCount == 2` branch *is* the pinch branch — there is no
two-finger-pan gesture at all, and `MINIMUM_PINCH` is a tiny 0.005 normalized
threshold, so any two-finger motion with the slightest distance change reads as
GESTURE_PINCH_IN/OUT. Confirms the §4 never-wrap verdict from yet another angle.)

**The fix — own the touch truth on web (don't trust raylib's array there).**
The DOM's `event.touches` *is* spec-correct in every browser: it always lists
exactly the currently-active contacts. So rebuild, don't decrement:

- `runtime/web_shell.html`: add canvas `touchstart/move/end/cancel` listeners
  (`addEventListener` **coexists** with emscripten's handlers — nothing about
  raylib's plumbing changes). On *every* event, rebuild a JS mirror array
  (`identifier, x, y` per entry) from `event.touches`, canvas-space via
  `getBoundingClientRect()` (same scaling math touchlab test 1 verifies).
  `touches.length === 0` ⇒ mirror is empty, unconditionally. `touchcancel`
  handled identically to `touchend` (OS interruptions — and WebKit sometimes
  drops it, which the rebuild-every-event model shrugs off anyway).
- `studio.c`, `poll_virtual_touches()`: under `PLATFORM_WEB`/emscripten, read
  the mirror via a small `EM_JS` instead of
  `GetTouchPointCount()/GetTouchPointId()/GetTouchPosition()`. Everything
  downstream — the vt pool, `tapp/tapr`, `touch_ended_*`, gestures.h — is
  already keyed by id and just starts receiving correct data. Native path
  untouched.

This is immune to both bugs by construction: we never decrement a count, we
*replace* the active set with the browser's canonical one each event.

**Acceptance test** (touchlab, on device, web build): repeatedly press two
fingers and lift both simultaneously — `touch_count()` must return to 0 every
single time, and LIFT must report both ids. Then the §5 card top-to-bottom to
confirm no regressions (especially test 1 coordinates and test 3 id stability,
since both now flow through the mirror).

## 8. gestures.h — known gaps (work after §7 lands)

§7 is the prerequisite: until the phantom is fixed, lift-judged gestures misfire
on web no matter what this header does. Then, in rough order of value:

1. **Two-finger pan vs pinch.** `pinch_scale()` is a naive distance ratio of
   touches 0 and 1 — any two-finger drag *also* changes that distance slightly,
   and there's no pan output at all. The classifier games actually want:
   compare **centroid motion** (pan) against **inter-finger distance change**
   (pinch) with a dead-zone/hysteresis — below thresholds, report neither;
   commit to whichever dominates. Add `gest_pan_x()/gest_pan_y()` (centroid
   delta this frame, two-finger only) beside a hardened `pinch_scale()`.
2. **Pinch pair must be id-keyed.** Today the pair is whatever sits at indices
   0 and 1 — a third finger landing or an index shuffle silently swaps the pair
   and `p_dist` compares two different geometries for one frame (spike). Track
   the pinch pair by id (first two ids seen), reset cleanly when either lifts.
3. **Net-direction-only swipes** — only landing and lift positions are judged;
   a finger that hooks (left then right) records the net direction. Fine for
   now; noted so nobody mistakes it for a bug. Path tracking only if a cart
   actually needs flick-curves.
