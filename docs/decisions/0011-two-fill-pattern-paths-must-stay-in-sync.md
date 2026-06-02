# 0011 — Two fill-pattern paths (texture vs `plot_pat`) must stay in sync
Date: 2026-06-02 · Status: accepted

## Context
`fillp()` dithering has **two independent implementations** in `runtime/studio.c`, and
nothing in the code makes that obvious:

1. **Texture path** — `rectfill_pat()` bakes the active 4×4 pattern into a tiny POT
   texture and tiles it across the rectangle in **one** `DrawTexturePro` call with
   `TEXTURE_WRAP_REPEAT`. **Only `rectfill()` uses this.** It exists because a filled
   axis-aligned rect is a single GPU quad — going per-pixel would throw away the one
   case where the GPU can do the whole fill for free.

2. **Software path** — `plot_pat()` decides each pixel's bit from `(x,y)` and calls
   `pset`. **Everything else** routes through it: `circfill`, `ovalfill`, `circ`/oval
   outlines, `polyfill`, `rrectfill`, `ngonfill`, `starfill`, and the dithered
   gradient. These already walk pixels for coverage (concave tests, disc tests,
   rounded corners), so software dithering is the natural — and only sensible — fit.

The split is individually justified, but it is a **latent trap**: the same visual
feature lives in two places, and a change to one silently leaves the other behind.

This bit us immediately. `fillp_anchor()` (let a moving shape carry its pattern instead
of crawling over a world-pinned lattice) was first added to **only** the texture path.
`rectfill` honored the anchor; every circle/polygon/star **ignored it** — so the
katamari cart, whose stuck decorations are mostly circles, kept crawling. The fix
looked correct and tested correct on a rect, yet did nothing for the actual shapes.
A deterministic A/B under `tools/play.js` (anchored disc → 0 changed px in motion;
world-anchored disc → full inversion) is what localized it to the second path.

## Decision
**Keep both paths** — neither is wrong, and merging them would either make `rectfill`
per-pixel (slower for the common case) or bolt a texture path onto shapes that must go
per-pixel anyway. But treat the pair as **one feature with two sites**: any change to
pattern *semantics* (anchor origin, hole-color handling, bit order / pattern-bit
lookup, which global supplies the pattern) **must be mirrored in `rectfill_pat()` and
`plot_pat()` together**, with the sign/coordinate convention identical.

## Why
- The two implementations are each the right tool for their shapes; the cost is purely
  the risk of silent divergence, not the existence of two paths.
- That risk is real and already realized once — worth a frozen note rather than a
  comment that the next editor won't find.

## Consequences
- When touching `fillp` behavior, grep `runtime/studio.c` for **both** `rectfill_pat`
  and `plot_pat` and change them in lockstep. The anchor convention is now
  `(coord - fp_anc_*)` in both (`& 3` in `plot_pat`, subtracted from `src` in
  `rectfill_pat`).
- `fillp_anchor` is wired in all four places (`studio.h`, `studio.c` ×2 paths,
  `studioDocs.js`, `shell.js`) and demonstrated by the `fillp_anchor` example cart.
- Out of scope: unifying the two paths. If a future change makes them hard to keep in
  sync, that's the trigger to revisit — supersede this note, don't quietly merge.
