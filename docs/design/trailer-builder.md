# The trailer builder — a humble CapCut over the `.reel`

STATUS: BUILDING (2026-07-03) — **backbone + editor UI v1 (A) SHIPPED.** Backbone:
`tools/build-app-reel.js` (proven on Tiny Jam, 3-rack reel). UI: the Apps-card **🎞 trailer** button
opens a **Trailer section** (Apps tab) — a click-to-edit timeline that loads from the `.reel`, a
clip **library** per rack, **reorder** (◀▶) + **remove**, a **transition dropdown + seconds at each
join**, and **Build → bake+compose → inline `<video>` preview** (`studio:app-clips` /
`studio:build-reel` in main.cjs). Non-destructive: it only writes the `.reel` + bakes derived clips.
**trim + speed — ENGINE SHIPPED (2026-07-03):** `compose-clips.js` now takes any number of
order-independent `| verb …` segments per line — `| trim A B` (begin/end in-out points, seconds
into the source, B clamps to length) and `| speed F` (F× faster; video `setpts`, audio `atempo`
chained for F outside 0.5–2×). Each clip's effective duration is recomputed so the xfade **overlaps
still line up**. Verified: `acidrack | trim 0 4` + `yachtrack | trim 2 6 | speed 1.5` +
`epiano | speed 2` → 8.2s (was 34s), untouched reels byte-identical. **Editor block fields SHIPPED
(2026-07-04):** each timeline block now has `trim begin/end` + `×speed` number inputs, serialized
through `studio:build-reel` and parsed back by `studio:app-clips` (round-trip verified). Same pass:
Build streams a live log panel with progress counters — a `[k/N]` bake counter for the missing
clips, then a throttled `encoding… X%` from ffmpeg's `-progress` through the otherwise-silent
encode — and closing the panel stops the preview video. The video-authoring
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

- **v1 (A) — SHIPPED:** horizontal clip blocks, ◀▶ reorder + ✕ remove, a clip **library** per rack
  (with "·bake" on recipe-only clips), **transitions as clickable joins** (type dropdown + seconds),
  Build → bake+compose → inline `<video>` preview. Loads from / writes to the `.reel`. CapCut's
  *layout*, none of the fiddly drag physics.
- **trim + speed — SHIPPED (engine + editor):** the `.reel` line-syntax + `compose-clips` bump
  (`acidrack/01-demo | fade 0.5 | trim 0 7 | speed 1.25`; `trim A B` = begin/end in-out, `speed F`
  = F× faster) AND the per-clip `trim begin/end` + `×speed` inputs in each timeline block (serialized
  by `studio:build-reel`, parsed by `studio:app-clips`). Also: a streaming build-log panel + the
  preview stops on close. (Blocks are uniform width for now; duration-proportional needs ffprobe per
  clip — a small later polish.)
- **Drag-to-arrange — SHIPPED (2026-07-04):** the pool of clips is finite but the timeline isn't —
  any clip can appear any number of times, in any order, each copy independently trimmed. Drag a
  library clip onto the timeline to place it (a drop indicator shows where; dropping the same clip
  again = a repeat), drag a block to reorder (native HTML5 DnD; the trim handles / buttons are guarded
  so grabbing them doesn't start a block drag), **⧉** duplicates a part with its trim+speed, **clear**
  empties the timeline so you build a subset up from nothing. `synth1→synth2→synth1→synth3→synth1` is
  just drags. All client-side — the compose engine already handles duplicate refs (verified).
- **Staged (B) — the live-preview tier:** visually pick cut + transition points on a `<video>`-scrub
  timeline (drag-to-trim, overlap-as-transition), no bake. Full spec: **[§Live preview + visual cut
  points](#live-preview--visual-cut-points-staged-b)** below. Plus the smaller (B) polish: speed
  **curves**, the **9:16 social-export** toggle, and the **IAP-tease ordering** (free rack first →
  "unlock 3 more" — the montage becomes a funnel, [`demand-generation.md`](demand-generation.md) #4).

## Live preview + visual cut points (staged B)

STATUS for this section: **slice 1 SHIPPED (2026-07-04)** — duration-proportional blocks + a visual
trim track (draggable in/out handles) + a preview monitor with scrub-and-set-in/out and play-in→out
at speed. Slices 2–4 (overlap-as-transition, continuous sequence player) still specced. The v1 (A)
timeline edited numbers in fields then baked to see the result; this tier adds the thing real editors
have — **scrub the clips and pick cut points by eye, reflected live** — while keeping Build as export.

**The principle it rests on (same as a real NLE): one graph, two consumers.** The `.reel` stays the
single source of truth. Today only the *exporter* (`compose-clips`) reads it. This tier adds a
second reader — a **live player built from `<video>` elements** — that renders the same graph
just-in-time to the screen instead of to a file. "Live preview" and "bake at the end" become the
same edit list with a different sink (screen vs `.webm`). See
[`demand-generation.md`](demand-generation.md) and the discussion that seeded this: trim + speed are
the edits a real editor does *for free*, because our clips are already `<video>`-playable webm.

**Why it's cheap here.** No new IPC, no ffmpeg for preview, no in-browser codec:
- **Trim = live for free** — a `<video>` clamped to `[in, out]` (`currentTime` bounds); the preview
  updates the instant you drag a handle. Writes `trim A B` to the row.
- **Speed = live for free** — `video.playbackRate = F`. Writes `speed F`.
- **Duration-proportional blocks** — read `video.duration` on `loadedmetadata` (no ffprobe/IPC), lay
  out each block's width ∝ its *trimmed* length. This is what makes the timeline honest to scrub.
- **Sequence playback** — chain clips: play A from `in` to `(out − overlap)`, then the overlap region
  **cross-fades into B by animating opacity** on two stacked `<video>`s (works for `fade`/`dissolve`).

**The maker's priority — picking cut + transition points by eye:**
- **Per-clip scrub bar with two trim handles.** Each block carries a thin scrub bar (a filmstrip of
  a few `<video>`-grabbed thumbnails, or just a draggable playhead that drives the preview frame).
  Drag the ◀ / ▶ handles → sets `trim A B` live. A **"set in/out here"** button captures the current
  scrub frame's `currentTime` into the trim — the cheapest "pick it visually" affordance, no drag
  physics needed to be useful.
- **Overlap IS the transition (the DaVinci feel).** Draw adjacent blocks *overlapping* by a width
  ∝ the transition seconds (a clip sits at `offset = total − xdur` — already true in the engine).
  Drag the overlap wider/narrower → sets `xdur`; a ◇ in the overlap opens the type dropdown. So the
  transition length is a thing you *grab*, not a number you type.

**Where it stays honest (the non-negotiable core):** live preview is an *approximation*; Build is
truth. Opacity can't do `wipe*`/`pixelize`/`radial`/`circleopen` — the live player shows those as a
plain cut or a fade stand-in with a **"wipe — exact on Build"** badge. The baked reel is the only
frame-accurate, correct-audio, shareable artifact. (Real editors hide this with a background render
cache; our baked reel *is* that cache — we just add the live layer above it.)

**Build order (each slice independently useful):**
1. ✅ **SHIPPED** — duration-proportional blocks (`video.duration` probed client-side, block width ∝
   trimmed length) + per-clip trim track with draggable in/out handles + a preview monitor (click a
   block to load it; drag a handle seeks the monitor to the cut frame; ⇤/⇥ capture the scrubbed
   frame as in/out; ▶ plays just the in→out range at the clip's speed). All client-side, no new IPC.
   Delivers "pick cut points visually" on its own, no transitions involved.
2. Overlap-as-transition: overlapping block layout, drag-to-resize `xdur`, ◇ type picker in the overlap.
3. Continuous sequence preview player (chained `<video>`, opacity cross-fade for fade/dissolve;
   badge the non-opacity transitions).
4. Build unchanged — the exact exporter.

## Text cards + overlays — kinetic pixel type via a magic-colour key (staged)

STATUS for this section: SPECCED, not built (2026-07-04). Resolves the **text/tween fork** left open
in [`demand-generation.md`](demand-generation.md) (§"App-trailer pipeline") — toward **engine-native**,
not ffmpeg `drawtext` and not a hand-off to CapCut.

**The idea.** CapCut's moat is kinetic text — but it's smooth vector type that looks like every other
social clip. This engine has what CapCut doesn't: **bitmap fonts** (`dos_8x8`, `FONT_SMALL/TINY/LARGE/
BOOT/SMOOTH`) and a whole **game-feel juice** vocabulary (ease, overshoot, shake, glow, `anim()`
phase-stagger). So the on-brand move is **kinetic *pixel* type animated with the same juice as the
games** — unmistakably this engine. A text "part" is drawn by a cart and baked like any clip.

**One cart, two uses — the key insight is the magic colour:**
- **Standalone card** — the cart draws its own background (title / section beat / "unlock 3 more"
  CTA); it's just another full part in the `.reel` sequence. No compositing.
- **Overlay on gameplay** — the cart clears to a reserved **magic colour** (one palette slot nothing
  else draws with) and draws text over it; at **compose time** ffmpeg keys that colour out and
  overlays the text onto a base clip for a time window. One filtergraph pass, **no alpha codec**.

**Why it's pixel-perfect (and better than anyone else's chroma key).** Real chroma key is hard
because video is anti-aliased — soft blended edges leave fringe/halo and need despill + fuzzy
tolerance. **Pixel art has no anti-aliasing**: every pixel is an exact palette colour with a hard
edge, so we key on an *exact* colour match (near-zero tolerance) and get a literally pixel-perfect
cut with zero fringe. The thing that makes green-screen finicky doesn't exist here. It's also
philosophically native — the engine already ships `colorkey(color)` for sprite transparency; this is
the same trick one layer up, at the video level.

**Proven (2026-07-04).** A hard-edged shape on pure magenta, keyed + overlaid onto a real acidrack
clip in one pass — magenta fully gone, hard edges, zero fringe:
```
ffmpeg -i base.webm \
  -f lavfi -i "color=c=0xFF00FF:s=320x200:r=30,drawbox=…:t=fill" \
  -filter_complex "[1:v]colorkey=0xFF00FF:0.01:0.0[k];[0:v][k]overlay=0:0:enable='between(t,1,4)'[o]" \
  -map "[o]" -map 0:a …
```
This is the de-risk: overlay-on-gameplay is NOT the expensive fork it looked like — the magic-colour
key makes it as cheap as a standalone card.

**The renderer.** A reusable `titlecard` cart (candidate for a `titlecard.h` cart-land lib so it's
shared/extensible), parameterised by content · style · anim-preset · background (magic colour vs real)
· duration. Params reach it at bake time (a params file the cart reads, or a generated source per card
— settle in build). One cart drives both uses: magic-colour bg → overlay; real bg → standalone card.

**Content model — a list of sized lines, NOT templates (DECIDED 2026-07-04).** A card's content is an
**ordered list of lines, each with a role that sets its size**: `title` (big) · `sub` (medium) ·
`body` (small). This single model *is* every case, with no template to pick:
- one sentence → a single `body` (or `title`) line
- header + subheader → a `title` line + a `sub` line (the common case)
- a paragraph → several `body` lines
Lines stack top-to-bottom in the order written, auto-centre, and auto-fit (wrap / shrink when too
wide) via **`lay.h`** — roles just map to the engine's font sizes. Three roles (not two): `body` is
what makes taglines and paragraphs read right, and it costs nothing.

**Three motion layers (don't conflate them):**
1. the card's **cut** — the `xfade` bringing the part in (already have it);
2. the text's **entrance** — plays once (typewriter/pop/slide, below);
3. the text's **resting life** — *continuous* while on screen: **boil / breathe**, ported from the
   `squishy` cart. This is the "sexiness" — without it a title arrives then sits there dead.

**Entrance vocabulary** (one-shot, cheap engine-native):
- **typewriter** — chars appear one by one
- **pop** — per-letter stagger (`anim()` phase) + overshoot bounce
- **slide** — ease in from an edge
- **glow / CRT flicker** — pulse
- **impact** — shake + scale-punch, settle

**Resting life — port `squishy`'s boil (the warm, anti-CapCut signature).** Squishy renders strokes as
a pure function of (stroke, seed, frame) and "boils" them two ways; steal both at the *letter* level
(bitmap glyphs can't jitter their outline points, so jitter each letter's position instead):
- **wobble** — cycle 3 seeded variants, each held ~8 frames (**~7.5 fps** — the hand-inked-cel cadence
  that reads as drawn-by-hand; smooth 60 fps reads as digital), each letter nudged ±~1 px.
- **breathe** — a smooth ±~7% scale on a slow sine (~2.4 s/breath).
Deterministic and nearly free (seeded offsets, no simulation), so it bakes cleanly. Numbers lifted
from `squishy.c` (`BOIL_FRAMES 3` · `BOIL_PERIOD 8` · `BOIL_JIT 1.2` · `BREATHE_AMT 0.07`).
**Later, borrow squishy's rim features** — `outline` + `bevel` + `drop-shadow` are the chunky
bubble-letter look (fat-ink + outline + bevel *is* the Tiny-Jam logo, per squishy's own notes); and
for a hand-lettered app *title* specifically, draw it once in squishy and use the export as a clip.

**Two anims for slice 1 — in-animation only.** A card enters with its text anim and **exits via the
existing `xfade` cut** (no separate out-animation yet; covers ~90% of trailer cards, halves the
grammar). Out-animations are a later add if a card needs to *leave* with flair mid-shot.

**`.reel` grammar (DECIDED — anchored-to-clip overlays, sized-line content):**
```
@card 2.5 | fade 0.5 | title "TINY JAM" | sub "3 synths, one app" | anim pop | style neon
@card 2.0 | fade 0.5 | body "made on a fantasy console" | anim slide          # just one sentence
acidrack/01-demo | fade 0.5                                                   # an overlay rides THIS clip:
  over @1-3 | pos bottom | anim slide | body "new: the acid engine"           #   relative time, survives reorder/retrim
```
- **Overlay timing is anchored to its clip (relative seconds), never absolute reel time** — so
  reordering / retrimming the clip carries the overlay along and keeps it valid (absolute times would
  refight the "34s too long" fragility). Overlays hang on continuation lines under the clip (one now,
  the form allows several later).
- Text-role segments (`title`/`sub`/`body`) render in written order, top-to-bottom; `anim`/`style`/
  `pos`/`fade`/`over` are the non-text segments. `pos` = a 9-grid enum (via `lay.h`).

**Where it stays honest:**
- **Preserve nearest-neighbour scaling** so the magic colour stays an *exact* RGB after the reel's
  integer upscale (we already `scale=neighbor`) — key on the exact colour, no fringe.
- **Pick a magic colour nothing fades toward** — a glow that falls off toward the key colour could
  partially key; have glows fall to black and reserve a colour far from any effect's output.
- Reserve the palette slot; the text cart must never draw content in the magic colour.

**Staging:**
1. Standalone cards (title / CTA) — no key, the simplest slice; resolves the fork. **Include the
   resting boil/breathe from the start** — it's the signature and it's nearly free; one entrance
   preset (`pop`) is enough to begin.
2. Magic-colour overlays on clips — the proven `colorkey`+`overlay` pass + the `over …` grammar.
3. Beat-sync (cards pop on the beat; inherit the prior clip's BPM), squishy's rim features
   (outline/bevel/shadow), + more entrance presets.

**Editor:** a "＋ text card" in the library adds a card block whose content is a small **line stack**
— each row = a role dropdown (title/sub/body) + a text field, with ＋line / ✕line — plus anim/style
dropdowns. A new card defaults to a single `title` line (type one thing, done); add a `sub` for the
header+subheader case. Overlay = attach to the focused clip with a relative time window. Preview via
bake first; a client-side canvas preview of the text animation is a later nicety.

## IPC it needs (small)

- `studio:app-clips(app)` → each cart's committed clips (+ which are baked / recipe-only)
- `studio:build-reel(app, rows[])` → write `tools/reels/<app>.reel`, bake missing clips, compose,
  return the `.webm` path to preview

## Open decisions (maker's call)

- **v1 A vs B** — ship the click-to-edit timeline first (rec), or invest in drag physics up front?
- ~~**`.reel` trim/speed syntax**~~ — DECIDED (2026-07-03): `| trim A B` (A/B = begin/end in-out
  points in source seconds) `| speed F` (F× faster), order-independent segments. Engine shipped.
  The maker's frame: think DaVinci/CapCut — two clips **overlap** and the overlap region *is* the
  transition (already true under the hood: a clip sits at `offset = total − xdur`); the staged (B)
  timeline should **draw** that overlap so dragging it sets the transition length.
- **the text/tween fork** (from [`demand-generation.md`](demand-generation.md)): beat-synced
  engine-native vs. hand-off-to-CapCut — still open; the builder is neutral to it.

## See also
- [`demand-generation.md`](demand-generation.md) — why this is lever #2; the funnel it feeds
- [`transitions.md`](transitions.md) — `compose-clips.js` (Layer B), the xfade vocab
- [`cart-clips.md`](cart-clips.md) — clip storage (`tools/clips/<cart>/`, baked `editor/public/clips/`)
- [`share-panel.md`](share-panel.md) — the make-clip button this realizes
- [`input-recording-looper.md`](input-recording-looper.md) — richer clip *authoring* (the deep end)
