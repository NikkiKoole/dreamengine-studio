# aseprite/ — hand-drawn source art for the app-store icons

Aseprite sources and their PNG exports for the two shipped app icons. Small enough to live
here rather than in a pipeline; there is no build step, the exports are made by hand from
Aseprite and copied to `apps/<name>/icon.png`.

## What ships

| shipped as | is a copy of | notes |
|---|---|---|
| `apps/tinyacidjam/icon.png` | `smiley-1024.png` | melting acid-house smiley |
| `apps/pedalboard/icon.png` | `pedal-teal-1024.png` | teal stompbox |

Naming: `<subject>[-colourway]-<pixels>.png`. Every export is 48px logical art scaled to its
pixel size, so `-1024` is the same drawing as `-48`, not a redraw.

| file | what |
|---|---|
| `pedal-teal-1024.png` / `pedal-teal-48.png` | **the shipped pedal** (teal chassis, brass switch) |
| `pedal-blue-1024.png` / `pedal-blue-48.png` | the superseded blue colourway — kept because it is what `pedal.aseprite` currently exports (see caveat 1) |
| `smiley-1024.png` / `smiley-48.png` | the shipped smiley |
| `pedal.aseprite`, `acid-smiley.aseprite`, `smiley48.aseprite` | the Aseprite sources, all 48×48 |

## Caveats — two places the sources DISAGREE with what ships

1. **`pedal.aseprite` still holds the BLUE palette.** The teal colourway exists only as PNG.
   Open the source and you get blue; re-export and you overwrite teal with blue. The teal
   values, if you want to put them back into the source palette:

   | role | blue | teal |
   |---|---|---|
   | chassis | `#3f70be` | `#287571` |
   | shade | `#28466f` | `#37433e` |
   | bevel | `#92b4e3` | `#bead85` |
   | ink | `#0d0d0f` | `#140c05` |

   `pedal-teal-48.png` was not exported from Aseprite — it was derived from
   `pedal-blue-48.png` by learning the exact colour map from the blue→teal 1024 pair
   (13/13 colours mapped unambiguously). It is correct, but it is downstream of a PNG, not
   of the source.

2. **`smiley-48.png` carries 28 colours, 26 of which are near-black noise** (`#000000`,
   `#0e0d0d`, `#0d0d0d`, …) where there should be one ink. The 1024 export is clean. Not
   fixed here on purpose: patching the PNG would just diverge from the source that produced
   it. Fix the ink in `smiley48.aseprite` and re-export.

Also: there are **two** 48×48 smiley sources (`acid-smiley.aseprite` and `smiley48.aseprite`)
and they differ. Which one is canonical is not recorded — no Aseprite CLI on this machine to
render them for comparison.

## Checking an icon before it ships

An icon is not done when it looks right at 1024 — iOS masks it to a squircle and throws away
6.1% of the square, corners first, then downscales hard. Both gates:

```bash
node tools/icon-mask.js check   apps/<app>/icon.png   # per-corner: flat backdrop, or lost detail?
node tools/icon-mask.js preview apps/<app>/icon.png   # how it reads at 1024/192/128/120/87/60
```

Requirements the exports must meet: **1024×1024** and **no transparent pixels** (a fully
opaque alpha channel is fine — `actool` flattens it). Full write-up:
[`docs/design/app-icon-mask.md`](../docs/design/app-icon-mask.md).

**`apps/tinyacidjam/icon.png` currently fails `check`** on the bottom-right corner (6.6% of
that corner's cut region is detail, reaching 248px). What the squircle clips is the drop
shadow, not the face. The cause is framing: the smiley fills 88%×94% of the frame with a
21px bottom margin, against the pedal's 75%×81% and 107px — pulling it in a cell or two
fixes the warning and the size mismatch between the two icons together.
