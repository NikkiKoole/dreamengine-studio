# 0029 — 320×200 is the base resolution (adapt outward, don't fit a device)

Date: 2026-07-08 · Status: accepted

## Context
Building the Tiny Jam app (`build-app.js tinyjam --mac`) failed with a **screen/grid mismatch**:
a multi-cart app bundle must pick ONE size and every cart must match it
([`share-panel.md`](../design/share-panel.md) next-spike #3), but the carts disagreed —
acidrack/epiano were authored at **320×240** (4:3, scale 3) and omnichord/pedalboard/otamatone at
**320×200** (16:10, scale 4). Which size should the app — and the project — standardize on?

The instinct was "pick whichever the flagships already use (240)." But 240 is the *retro-er* choice,
and it fits the wrong device. The aspect-ratio math (the trap that keeps recurring):

| res | ratio | closest real device |
|---|---|---|
| **320×200** | **16:10** (1.60) | **MacBook** (1280×800 @4× is a native Mac size); the *middle* of the range |
| 320×240 | 4:3 (1.33) | **iPad** (12.9" is exactly 4:3) |
| *iPhone* | **~19.5:9 (2.17)** | nothing at 320×N — you'd need ~320×148 |

The two target devices pull in **opposite** directions — iPad is boxy (1.33), iPhone is very tall
(2.17) — so **no single fixed resolution fits both** (and Mac is a third point). 320×240 is native to
iPad but letterboxes badly on iPhone; 320×200 is native to nothing but sits in the **middle** of the
iPad↔iPhone spread. A "which device" bet is therefore the wrong frame — the right frame is
device-adaptive reflow ([`device-adaptive-layout.md`](../design/device-adaptive-layout.md)): carts
fill the *actual* device, and the base res is just a design canvas.

## Decision
**320×200 is the canonical base resolution. Author and finalize carts *good at 320×200*, then make
them resizable to adapt outward to the actual device.**

Rationale — 320×200 wins on all three axes that matter here:
1. **Heritage** — it's the engine default (CLAUDE.md: "Screen 320×200 default, 4× scale = 1280×800")
   and DOS Mode 13h, the DIV Game Studio lineage the project comes from. It's the de-facto standard:
   ~386 of 473 carts use it (no `.cart.js` ⇒ default 200), and 31-to-9 among carts that declare a size.
2. **Modern ratio** — 16:10 is a current widescreen ratio (MacBooks are 16:10); `@4×` = 1280×800, a
   native Mac size. 320×240 (4:3) is the dated ratio.
3. **Neutral middle** — 1.60 sits between iPad (1.33) and iPhone (2.17), so "get it good at 320×200"
   is the closest single starting point to *every* target. You adapt outward from the middle, not
   from an extreme.

The device fit itself is **not** the base res's job — it's the resizable layer's
(`-DDE_RESIZABLE` + `de_reflow`, `screen_w()/screen_h()`, `lay.h`, `finger_px()`). The base is the
reference canvas; reflow fills iPhone (tall), iPad (4:3), and Mac (16:10).

## Consequences
- **Tiny Jam standardizes on 320×200 @4×.** acidrack/epiano moved down from 320×240 (their
  `.cart.js` now `screenH: 200, scale: 4`); omnichord/pedalboard/otamatone were already there. The
  `build-app` bundle constraint now passes for all five.
- **acidrack/epiano lose 40px of height** and their dense UIs (drum grids, keybed) may cramp at 200 —
  acceptable because both are **not finalized**; their layout finalization is the natural place to
  fit 200 and add resizable reflow. Bake + eyeball each at 200 before calling them done.
- **New carts default to 320×200** already (no `.cart.js` needed for size) — this ADR just makes the
  default a deliberate choice. A cart that genuinely needs another size still can, but a *bundled*
  cart must match its app's base.
- **The general guidance:** don't chase a device with a fixed resolution. Author at 320×200, make it
  resizable, let it adapt. See [`device-adaptive-layout.md`](../design/device-adaptive-layout.md) for
  the reflow mechanics and [`0022`](0022-collaboration-is-the-north-star.md)'s stranger-legibility bar.
