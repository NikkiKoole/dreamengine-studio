# cart clips — where moving images (webm/gif) live

CONVENTION (proposed; exporter not landed yet). Carts have a still thumbnail today
(`editor/public/carts/<cart>.cart.png`, which also stores source/sprites/map in zTXt
chunks). A small video exporter is in the works to produce **moving images** of carts —
this note fixes where those files go and how they're named, so the exporter targets a
stable path from day one and consumers (the history page first) can auto-detect them.

## The layout

A **sparse, per-cart clips folder** — *not* the flat `carts/` dir:

```
editor/public/carts/clips/<cart>/NN-label.webm
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
- **Served at** `/carts/clips/<cart>/...` by Vite (and copied into `site/` at publish for
  the web gallery). **Lazy-loaded / on-hover**, never inlined — video is far too heavy for
  the base64 data-URI trick the PNG thumbnails use.

## Why not the flat `carts/` folder

It already holds 391 `.cart.png` + `.cart.js`, and the editor's cart picker globs
`*.cart.png` there. Dropping `coaster.webm`, `coaster-2.webm`, … alongside would clutter
it, bloat git, and give no clean home for *multiple* clips per cart. The per-cart subfolder
answers the multiple-videos question directly.

## Auto-detection (the consumer contract)

Anything surfacing clips globs `editor/public/carts/clips/<cart>/*.{webm,mp4,gif}`, sorts by
filename, and derives the caption from `NN-label`. No per-cart registration — exactly like
the still thumbnails. First consumer: **the history page** (`tools/build-history.js`) — the
hero cart, the design-note spotlight, and the research-thread carts are the natural slots
(they already show a still `.cart.png`); a clip, when present, upgrades the still in place,
PNG as poster.

## Open decisions (don't change the layout, just the plumbing)

1. **Git policy for the binaries** — direct commit (simplest, but weight, and `site/` carries
   copies), git-lfs, or keep-out-of-git / emit-into-`site/`-at-publish only. Lean: direct
   commit while clips stay short/small; revisit if volume forces lfs.
2. **Exporter output** — format (webm/gif/mp4) and whether it can write straight to
   `clips/<cart>/NN-label.webm`. If it dumps to one folder under its own names, a tiny
   `tools/` step files them into place.

> Status: waiting on the exporter. When it lands, wire auto-detection into
> `tools/build-history.js` (see [`../guides/history-page.md`](../guides/history-page.md) →
> design-note spotlight / research threads) and, later, the editor cart picker.
