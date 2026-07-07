# Device matrix — the resolutions we design and ship against

STATUS: REFERENCE (2026-07-07) — a **living reference doc**, not a plan. Grow it as we get
devices and as Apple's requirements move. The one canonical place for two questions we keep
re-asking, so no other doc re-tables them.

> **Two matrices, two jobs.** They're both "lists of resolutions" but answer different questions —
> keep them apart:
> - **[§2 Layout shapes](#2--layout-matrix--the-shapes-we-design-against)** — the device *shapes* a
>   responsive cart must reflow to and stay legible/finger-comfortable on. This is what a rack's
>   layout brief designs *against* (`play.js --resize`). Ergonomics, in logical points.
> - **[§3 App Store asset sizes](#3--app-store-asset-sizes--what-we-must-ship)** — the exact *pixel*
>   canvases App Store Connect demands for screenshots + preview videos. This is what the
>   store-asset pipeline (`store-shots.js`, `make-gif.js`) renders *to*. A hard external spec.

## 1 · Shape classes — what layout actually keys off

A responsive cart reflows to a **class**, not a device (device-adaptive-layout.md's whole point:
a handful of fluid arrangements driven by finger/screen ergonomics, not N pixel layouts).

| class | when | the arrangement it wants |
|---|---|---|
| **tall** | phone portrait (W < ~300 logical) | one working strip expanded + a couple compact + rest folded; song row pinned |
| **short-wide** | phone landscape | tabs (accordions degenerate short — the acidfit finding) + one expanded strip |
| **roomy** | tablet (min side ≥ ~700pt) | everything at once — e.g. all 5 acidrack strips compact, stacked |

## 2 · Layout matrix — the shapes we design against

Logical size is **points ÷ K** with the iOS **pixel chunk K = 2** (`ios/Sources/CanvasView.swift`
`pixelChunk`): the canvas reflows to device-points-halved *logical* pixels so it stays chunky-lo-fi
+ finger-friendly instead of hi-res-tiny. **The logical column is what a cart lays out in.** K=1 =
tiny pixels; K=3 = so chunky the fixed 8px font overflows a phone width (~16 chars) — 2 is the
text-safe sweet spot.

| shape class | device | points (native orient.) | logical @ K=2 | notes |
|---|---|---|---|---|
| tall | **iPhone SE (3rd gen, 2022)** | 375 × 667 | **188 × 334** | smallest + shortest modern iPhone — the **hardest tall case**; the maker's own device |
| tall | iPhone 13 mini | 375 × 812 | 188 × 406 | narrow but tall |
| tall | iPhone 16 / 15 | 393 × 852 | 196 × 426 | the default phone |
| tall | iPhone 16 Plus | 430 × 932 | 215 × 466 | large phone |
| tall | iPhone 16 Pro Max | 440 × 956 | 220 × 478 | biggest phone |
| short-wide | iPhone SE landscape | 667 × 375 | 334 × 188 | shortest landscape |
| short-wide | iPhone 16 landscape | 852 × 393 | 426 × 196 | the default landscape |
| roomy | iPad mini (6th) | 744 × 1133 | 372 × 566 | smallest tablet |
| roomy | iPad 11" (Air / Pro) | 834 × 1194 | 417 × 597 | default tablet |
| roomy | iPad Pro 13" | 1032 × 1376 | 516 × 688 | biggest, portrait |
| roomy | iPad Pro 13" landscape | 1376 × 1032 | 688 × 516 | biggest, landscape |

**Safe areas** (notch / Dynamic Island / home indicator / rounded corners) are wired engine-side —
`de_set_safe_area` → `safe_rect()` — and consumed by transport + chain row + the app home-chip.
They read **0 in the desktop `--resize` render** (they only exist on real hardware), so a desktop
matrix over-reports usable height; confirm insets on the sim/device.

**Regenerate the layout render** (any resizable cart — `acidrack`, `respond`, `tinyjam-menu`):

```sh
node tools/play.js acidrack run \
  --resize "188x334,188x406,196x426,215x466,220x478,334x188,426x196,372x566,417x597,516x688,688x516" \
  --frames 120
montage build/.resize/acidrack/resize_*.png -filter point -geometry '200%+16+20' \
  -tile 4x3 docs/design/device-matrix.png
```

Grow the matrix = add a row + its `WxH` to the `--resize` string. There is no per-device engine
work — one resizable cart covers them all.

## 3 · App Store asset sizes — what we must ship

Different job entirely: these are the **exact pixel canvases** App Store Connect accepts for the
listing. Apple **auto-scales the biggest size down** to every smaller device in a family, so you
only upload the top of each family you support — that's the **"at least" set** below.

### Screenshots — the minimum required set

| family | Apple display class | pixels (portrait) | required? | `store-shots.js` key |
|---|---|---|---|---|
| iPhone | 6.9" | **1290 × 2796** (or 1320 × 2868) | **YES** — covers all iPhones down to SE | `iphone69` |
| iPad | 13" | **2048 × 2732** (or 2064 × 2752) | required **iff the app ships on iPad** | `ipad13` |

Optional / legacy families `store-shots.js` also knows: `iphone65` 1242×2688, `ipad11` 1668×2388.
You don't need them if you upload the 6.9" / 13" set — Apple downsizes. Up to **10** screenshots
per family; the **first 1–2** appear in search results, so they must clearly show the app in use
(Guideline 2.3.3 — see store-agents.md §"Compliance").

The two alternate sizes exist because current hardware grew: `1290×2796` is the 15 Pro Max /
16 Pro; `1320×2868` is the 16 Pro Max. Either is accepted for the 6.9" slot — `store-shots.js`
emits `1290×2796`. Same for iPad: `2048×2732` (12.9") vs `2064×2752` (M4 13").

Generate: feed a `play.js --dump` frame to
`node tools/store-shots.js --in <frame.png> --device iphone69,ipad13 --caption "…"` — it
**composites** (crisp integer-upscaled cart on a bg + caption), never stretches, solving the
cart-aspect ≠ device-aspect gap. See store-agents.md §1.

### App preview videos — stricter

App previews must be **real on-device capture** (not a marketing-composited edit) — Guideline
2.3.3 again; our `make-gif.js … --mp4` clips qualify (actual runtime + real audio). Apple accepts
specific per-family video canvases and they change; **verify the current spec in App Store Connect
before rendering** rather than trusting a number cached here. Same device families as screenshots
(6.9" iPhone + 13" iPad cover the minimum), 15–30s, up to 3 per family.

> **⚠︎ driftable.** The screenshot pixel sizes and `store-shots.js` keys in §3 are copied from
> `tools/store-shots.js`'s `DEVICES` table. If that table changes, this section drifts — re-sync it.
> Video specs are deliberately *not* pinned here (they move too often); go to the source.

## 4 · How to use this doc

- **Building a responsive cart / writing a rack layout brief?** Design against **§2**; point your
  brief here instead of re-tabling shapes (acidrack-layout-brief.md §7 does this).
- **Shipping store assets (the fastlane-ish leg)?** Use **§3** + `store-shots.js` (stills) /
  `make-gif.js --mp4` (video). The "at least" upload set is 6.9" iPhone + 13" iPad.

Related: [`device-adaptive-layout.md`](device-adaptive-layout.md) (the engine + product plan) ·
[`acidrack-layout-brief.md`](acidrack-layout-brief.md) (the first rack brief that designs against
§2) · [`store-agents.md`](store-agents.md) §1 (the screenshot/video pipeline) ·
[`design-system.md`](design-system.md) (fonts/colours/roles the layouts use).
