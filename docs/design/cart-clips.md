# cart clips — where moving images (webm/gif) live

STATUS: SHIPPED (convention locked) — exporter + path scheme landed; how-to in guides/debug-harness.md "Clip capture".

CONVENTION (locked; exporter landed). Carts have a still thumbnail today
(`editor/public/carts/<cart>.cart.png`, which also stores source/sprites/map in zTXt
chunks). The video exporter — `tools/make-gif.js` (see
[`../guides/debug-harness.md`](../guides/debug-harness.md) → "Clip capture") — produces
**moving images** of carts and writes them to the path below. This note fixes where those
files go and how they're named, so consumers (the history page first) can auto-detect them.

## The layout

A **sparse, per-cart clips folder, a sibling of `carts/`** — not nested inside it:

```
editor/public/clips/<cart>/NN-label.webm
   e.g.  clips/coaster/01-the-ride.webm
         clips/coaster/02-scream-tuning.webm
         clips/roadlab/01-flyover.webm
```

Rules:

- **One folder per cart**, created only when that cart actually has video — so `clips/`
  stays sparse (not 391 empty dirs). A cart with a single clip just has `01-demo.webm`.
- **`NN-label` filename carries order + caption**, no sidecar metadata: the leading `NN-`
  sorts the clips; the rest (dash→space) is the human caption — `02-scream-tuning.webm` →
  *"scream tuning"*. Same derive-from-structure trick the history page's tools row uses.
- **The `.cart.png` stays put** as the data store *and* the video `poster=` / fallback.
- **webm is primary**, gif the lo-fi fallback; mp4 fine too if the exporter prefers it.
  Consumers pick the first existing of `webm > mp4 > gif`.
- **Served at** `/clips/<cart>/...` by Vite (and copied into `site/` at publish for the web
  gallery). **Lazy-loaded / on-hover**, never inlined — video is far too heavy for the
  base64 data-URI trick the PNG thumbnails use.

## Why a sibling, not inside `carts/`

`carts/` is the cart **data store** — 391 `.cart.png` + `.cart.js` + `index.json`, the
editor's picker domain (which reads `index.json`), and the target of two git-log date
scans (both filtered to `*.cart.png`). Nothing breaks if clips nest there, but keeping
`carts/` a flat, homogeneous folder of cart artifacts keeps every "just glob `carts/`" tool
correct by default. `editor/public/` is already the media root (`palettes/`, the fonts live
there), so `clips/` sits as a natural peer. The per-cart subfolder under it answers the
multiple-videos question; the flat `carts/` dir was never a candidate (no home for multiple
clips, and it'd bloat the data store).

## Auto-detection (the consumer contract)

Anything surfacing clips globs `editor/public/clips/<cart>/*.{webm,mp4,gif}`, sorts by
filename, and derives the caption from `NN-label`. No per-cart registration — exactly like
the still thumbnails. First consumer: **the history page** (`tools/build-history.js`) — the
hero cart, the design-note spotlight, and the research-thread carts are the natural slots
(they already show a still `.cart.png`); a clip, when present, upgrades the still in place,
PNG as poster.

## Reproducibility — the `tools/clips/` recipe home

The baked `.webm` under `editor/public/clips/` is an *artifact*; its **input track is the
source of truth**, committed under `tools/clips/<cart>/NN-label.{script,beats,rec}` (mirrors
the output tree, same `NN-label`). Bake with `node tools/make-gif.js <cart> --recipe
<NN-label>`; `--all` rebuilds every clip from its recipe — so `clips/` regenerates from
committed sources the same way carts rebuild from `tools/carts/`. A recipe is
self-describing (`# frames N`, `# fps N`, `# scale N`, `# crf N` comment lines travel with
the track), so each clip rebuilds at its own length. Default bake is **native res** (the
page upscales crisply via `image-rendering:pixelated`); the weight levers are
duration → `--crf` → scale, not resolution — see
[`../guides/debug-harness.md`](../guides/debug-harness.md) → "Clip capture".

> This same demo track is a candidate seed for more than the webm — a self-recorded `.rec`
> (`play.js record`), and potentially an in-cart **attract mode** (the cart plays itself
> until you touch a control). Parked, thinking in progress; the track is the shared primitive.

## Open decisions (don't change the layout, just the plumbing)

1. **Git policy for the binaries** — direct commit (simplest, but weight, and `site/` carries
   copies), git-lfs, or keep-out-of-git / emit-into-`site/`-at-publish only. Lean: direct
   commit while clips stay short/small; revisit if volume forces lfs.
2. **webm vs gif default** when a cart has both — webm wins on size/quality with the PNG
   poster, gif is the autoplay-blocked fallback; the glob accepts both (decide whether webm
   wins or both ride as `<source>`s). *(Resolved: the exporter writes straight to
   `clips/<cart>/NN-label.<ext>`, so no filing step is needed.)*

> Status: exporter landed (`tools/make-gif.js`). Next: wire auto-detection into
> `tools/build-history.js` (see [`../guides/history-page.md`](../guides/history-page.md) →
> "Moving thumbnails — clip support") and, later, the editor cart picker.

## Sound — clips carry audio (SHIPPED 2026-06-22)

webm/mp4 clips now have sound by default. `make-gif.js` renders the cart's audio to WAV **in
the same deterministic `play.js` pass** as the frames (one run → picture and sound share one
timeline) and ffmpeg-muxes it in: **webm → Opus, mp4 → AAC**. `gif`/`webp`/`apng` can't carry
audio and stay silent; `--mute` opts any clip out. Verified: `sloop/01-autodrive.webm` carries
a vp9+opus pair at matching 8.16 s durations.

How the bits fit together (for whoever touches it next):

- **Sync = one run.** Frames (`--dump`) and audio (`--wav`) come from the *same* `play.js`
  invocation, so the timelines correspond by construction. `--dump-every`/`fps` keep video
  real-time (engine 60 fps); `-shortest` trims any rounding tail; `--start n` shifts the audio
  by `n/60 s` so dropped boot frames drop the matching audio.
- **Container codecs.** webm → Opus, mp4 → AAC; gif/webp/apng silent by nature.
- **Autoplay vs. showreel are different audio *policies*, decided at PLAYBACK, not bake.** The
  baked file always has the audio track; a gallery thumbnail autoplays it **muted** (browser
  autoplay policy; many carts at once) with sound on tap/hover, while a deliberate "press play"
  showreel plays it aloud. Same file, two presentation rules — see
  [attract-mode.md](attract-mode.md) "Web manners".

This serves all three showreel layers in [transitions.md](transitions.md): an in-cart
transition clip (A), a stitched reel (B), and the live cross-cart player (C) all now have sound.

## Reels — stitching clips into one video (SHIPPED 2026-06-22)

`tools/compose-clips.js` is the Layer-B stitcher: it glues already-baked clips into ONE reel
with transitions between cuts (ffmpeg `xfade` for video, `acrossfade` for audio — picture and
sound dissolve together). Like a clip, a reel is a **committed, reproducible recipe**: a
`.reel` manifest under `tools/reels/<name>.reel` → `editor/public/reels/<name>.webm` (a sibling
of `clips/`). The manifest lists one clip per line (`<cart>/<label>` or a path) with an optional
`| <transition> <secs>` per cut and `# fps/crf/size/xfade` defaults; transitions are ffmpeg
xfade names (fade · dissolve · wipeleft/… · circleopen…). Clips of mixed sizes are letterboxed
nearest-neighbour (pixels stay crisp). Bake clips first (`make-gif.js`), then compose. The
first proof reel is `tools/reels/demo.reel` → `editor/public/reels/demo.webm`.

## Why this is now the priority (the venue is decided)

The clip system stalled not on the tool — which is done — but on the absence of a venue to
show clips in. That's resolved: [decision 0020](../decisions/0020-in-house-tool-curated-showcase.md)
makes the public surface a **curated showcase** people view and play. Clips are the cheapest,
lowest-risk way to make the gallery we *already publish* stop reading as a 400-item file
listing — a wall of moving images reads as "look what we made," a wall of stills reads as a
directory. Concretely: only `sloop/01-autodrive.webm` is baked of ~10 committed recipe
tracks; the cheap win is a `make-gif.js --all` pass + wiring the gallery/history page to
glob `clips/<cart>/`. Note the showcase is **curated** (a featured subset, not all ~400), so
clips are worth baking first for the carts that would headline.
