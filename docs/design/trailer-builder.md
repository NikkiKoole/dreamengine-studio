# The trailer builder — a humble CapCut over the `.reel`

STATUS: BUILDING (2026-07-03) — the **backbone shipped** (`tools/build-app-reel.js`, proven on
Tiny Jam: a 3-rack reel); the **editor UI is designed here, not yet built**. The video-authoring
surface for a multi-cart IAP app: pick clips, order them, set cuts / trim / speed, preview. The
lever-#2 tool of [`demand-generation.md`](demand-generation.md), built on
[`transitions.md`](transitions.md)'s Layer B (`compose-clips.js`) and the
[`share-panel.md`](share-panel.md) make-clip idea.

## What ships today (the backbone)

`tools/build-app-reel.js <app>` reads the manifest's `carts[]`, bakes a clip per rack (skips racks
with no committed clip, warns), writes a committed + hand-editable `tools/reels/<app>.reel`, and
composes it via `compose-clips.js` → `editor/public/reels/<app>.webm`. So "several racks in one
video" already works from the CLI; the UI is a **thin editor over that `.reel`**, not a new engine.

## The two principles that separate us from CapCut

**1 · Non-destructive — and more so than CapCut.** Every edit (order, trim, speed, transition) is a
**parameter in the `.reel`**, applied at compose time (ffmpeg `trim`/`setpts`/`xfade` on *copies*).
The builder **never mutates a source** — not the clip `.webm`, not its recipe, not the cart. So you
can trim a clip to 3s in one trailer and 8s in another off the *same* source, and the baked reel is
a throwaway derived artifact you rebuild any time. Better than a CapCut project because the sources
are committed recipes and the edit list is committed text: **git is the undo history** — every past
cut is a diff, nothing is ever overwritten in place. This is the non-negotiable core; the UI is just
a face on it.

**2 · Manifest → bake → preview (not live).** The `.reel` is committed text, and `compose-clips`
re-bakes it *identically* after an engine change or a re-tuned cart — a trailer is a **build artifact
of committed inputs**, not a one-off export. The cost: our "preview" is a **bake step (a few
seconds), not live scrubbing**. So the builder should *look* like CapCut but must not *pretend* to be
live CapCut. (Same ethos as the rest of the repo: deterministic, reproducible, manifest is truth.)

## What we studied in CapCut (2026-07) — steal vs. skip

CapCut's editor: a horizontal multi-track **timeline** (clips end-to-end, width ∝ duration, a
playhead, live preview above); **trim** by dragging clip edges *or* a Duration (clock) field;
**speed** as a Normal slider (0.1×–100×) or a Curve ramp; **transitions** dragged onto the *join*
between two clips, then a duration slider. Plus its moat: multi-track text/stickers/effects/filters,
animated-text presets, auto-captions, templates.

| steal (maps onto `.reel` + `compose-clips`) | skip (CapCut's moat / not our ethos) |
|---|---|
| horizontal, duration-proportional timeline — *a `.reel` literally is this* | multi-track layering |
| **transitions at the joins** — click a marker between clips → type + duration (= `\| wipeleft 0.6`) | text / stickers / effects / filters, animated-text presets |
| per-clip **duration (= trim)** + **speed** fields (the honest form of CapCut's clock icon / speed slider) | AI auto-captions, templates, the flashy transition *library* |
| a **preview player** (our baked reel `<video>`) | live frame-scrubbing (needs an in-browser video engine) |

The "great" motion-graphics layer (kinetic text, effects) is **handed to CapCut**, or done
**engine-native** for the beat-synced bits only (see [`demand-generation.md`](demand-generation.md)
text/tween fork). We expose the handful of ffmpeg xfades and stay deterministic.

## Where it lives

There is **no modal/overlay framework** in the editor (the Share thing is a small positioned
popover). So: a **🎞 trailer button on the app card** opens a **"Trailer" section in the Apps tab**
— the same render-into-a-dedicated-area pattern as `#aso-lab`. The card stays a launcher; the
builder gets room below. (A true pop-out modal is nicer but a separate build — staged.)

## How it picks the parts

- **Opens pre-populated, never blank:** loads the current `tools/reels/<app>.reel`, or if none, the
  default = every rack, one clip, manifest order (what `build-app-reel` produces).
- **Library:** each cart's committed clips (`tools/clips/<cart>/`); `＋` appends to the timeline. A
  rack with a recipe but no baked `.webm` shows "needs bake" and is baked on Build.
- **Timeline = the `.reel`:** the ordered clip blocks; each join carries its transition (type +
  seconds); each block carries trim + speed. Reorder + remove. Edit here → write the manifest.
- **Build:** write `.reel` → bake any missing clips → `compose-clips` → drop the result into an
  inline `<video>`.

```
┌─ Trailer: Tiny Jam ──────────────────────────────────────┐
│ LIBRARY   acidrack[01-demo ＋]  yachtrack[01-groove ＋]    │
│           epiano[01-riff ＋]    tinyjam-menu[needs bake ＋]│
│                                                           │
│ TIMELINE  (horizontal, width ∝ duration; ◇ = transition) │
│  ┌──────────┐◇┌──────────┐◇┌────┐                        │
│  │ acidrack │ │ yachtrack│ │epia│      click block → trim/speed
│  └──────────┘ └──────────┘ └────┘      click ◇     → type/secs
│                                                           │
│  [ Build ]   ▶ (baked reel preview)                       │
└───────────────────────────────────────────────────────────┘
```

## v1 scope

- **v1 (A):** the horizontal timeline (blocks sized by duration), **transitions as clickable
  joins** (type + seconds), per-clip **trim + speed** as fields, Build → bake → inline preview.
  CapCut's *layout*, none of the fiddly drag physics. Speed is in v1 (CapCut makes it central + it
  was a maker ask) — needs a `setpts`/`trim` add to `compose-clips` + a `.reel` field.
- **Staged (B):** drag-to-trim / drag-to-reorder (the tactile feel), per-clip thumbnails, speed
  **curves**, the **9:16 social-export** toggle, and the **IAP-tease ordering** (free rack first →
  "unlock 3 more" — the montage becomes a funnel, [`demand-generation.md`](demand-generation.md) #4).

## IPC it needs (small)

- `studio:app-clips(app)` → each cart's committed clips (+ which are baked / recipe-only)
- `studio:build-reel(app, rows[])` → write `tools/reels/<app>.reel`, bake missing clips, compose,
  return the `.webm` path to preview

## Open decisions (maker's call)

- **v1 A vs B** — ship the click-to-edit timeline first (rec), or invest in drag physics up front?
- **`.reel` trim/speed syntax** — extend the line grammar (e.g. `acidrack/01-demo | fade 0.5 |
  trim 0 7 | speed 1.25`) — needs designing + a `compose-clips` bump.
- **the text/tween fork** (from [`demand-generation.md`](demand-generation.md)): beat-synced
  engine-native vs. hand-off-to-CapCut — still open; the builder is neutral to it.

## See also
- [`demand-generation.md`](demand-generation.md) — why this is lever #2; the funnel it feeds
- [`transitions.md`](transitions.md) — `compose-clips.js` (Layer B), the xfade vocab
- [`cart-clips.md`](cart-clips.md) — clip storage (`tools/clips/<cart>/`, baked `editor/public/clips/`)
- [`share-panel.md`](share-panel.md) — the make-clip button this realizes
- [`input-recording-looper.md`](input-recording-looper.md) — richer clip *authoring* (the deep end)
