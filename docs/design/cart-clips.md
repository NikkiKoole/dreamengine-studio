# cart clips — where moving images (webm/gif) live

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
