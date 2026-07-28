# The app-icon mask: knowing which pixels iOS throws away

STATUS: SHIPPED (2026-07-28), `tools/icon-mask.js` + the committed measured mask
(`tools/icon-masks/ios26-2048.png`). Born from two apps sitting in review with their corners shaved.
Sits under the store pipeline: [`store-agents.md`](store-agents.md) §1 is the *screenshot* half of
the same problem, [ADR-0026](../decisions/0026-store-pipeline-in-house-not-fastlane.md) is the
plumbing.

You hand the App Store a **square** 1024×1024 PNG. iOS shows a **rounded-rect squircle**. The
difference is 6.1% of your artwork, and you find out at review time unless you design against the
real curve. This doc is the measured curve and what follows from it.

## Don't guess the shape, Apple ships a renderer

Xcode 26 bundles Icon Composer, and inside it a CLI:

```
/Applications/Xcode26_6.app/Contents/Applications/Icon Composer.app/Contents/Executables/ictool
```

`ictool` renders a `.icon` document through the **real** iOS mask. Feed it a flat full-bleed layer
and the output's **alpha channel *is* the mask**:

```
ictool Probe.icon --export-image --output-file out.png --platform iOS \
       --rendition Default --width 1024 --height 1024 --scale 2
```

(The minimal `.icon` document is a folder: `icon.json` with `fill` / `groups[].layers[].image-name` /
`supported-platforms`, plus `Assets/<image>.png`. `fill` wants **four** comma-separated components.)

`node tools/icon-mask.js rebuild` drives exactly that, keeps the alpha at 2048², symmetrises it
8-fold, and commits the result. So the mask in this repo is measured from Apple's own renderer, not
traced off a blog post. `--check` re-derives and diffs it (skipping cleanly on a machine without
Xcode), so an Xcode update that changes the shape shows up as a failing gate instead of a surprise
rejection. Wired into `repo-doctor`.

## What the measurement says

At 1024×1024, from the committed mask:

| | |
|---|---|
| area thrown away | **6.1%** (~64k px) |
| flat part of each side | **38%** of the edge |
| corner curve run-in | the first **~316 px** of every edge is curving |
| corner square lost outright | **78×78 px** at each corner |

Three findings that change how you draw:

1. **It is not a superellipse.** The best-fit `|x|ⁿ + |y|ⁿ = 1` is **n = 4.39** and still misses the
   real boundary by **42 px at 1024**. The popular "n=5 squircle" and the "corner radius = 22.37%"
   rule are both the *wrong shape* here. Apple's curve is a continuous-curvature spline; approximate
   it and your corners land further off than the thing you were trying to align. Use the mask.
2. **iOS 26 is a strict envelope of the classic iOS 7 to 18 mask.** Checked row by row against the
   continuous rounded rect at r = 0.2237·w, the iOS 26 boundary is at or inside the old one at
   *every* angle (worst crossing: 0.0 px). So **one template covers both**: survive iOS 26 and you
   survive iOS 18. That is why the tool ships a single profile instead of a per-OS zoo.
3. **The inscribed circle is entirely safe.** A circle of diameter = the full icon width sits inside
   the mask (tangent at the four edge midpoints; at 45° the mask boundary is 613 px from centre
   against the circle's 512). **Anything inside that circle survives.** Only the four corner slivers
   between circle and mask edge are at risk. That is the whole rule, and it is why the template
   draws that circle heaviest.

## The trap that actually bit us

Both apps in review draw a **device chassis**, a rounded rectangle with a coloured border, inside
the square. A hand-drawn rounded rect has *circular* corners; the mask has *continuous* ones. Even
when the chassis looks comfortably inside, its corners poke through the mask near the diagonals and
get shaved, so the border reads as broken at four points. It looks like a bug in the icon.

The fix is not "make the radius bigger". Draw the border **on an offset of the mask itself**:

```
node tools/icon-mask.js template --inset 28      # green line = the mask, eroded 28px
node tools/icon-mask.js mask --inset 28          # that curve as a plain mask, to composite
```

## Verified against a real device, so the prediction is trustworthy

A measured mask is only worth as much as its match to a phone. `icon-mask.js device <icon.png>` is
the proof, and it runs end to end: borrow a simulator `.app`, swap in the icon via `actool`, install
it into a booted iOS 26 simulator, screenshot the home screen, locate the new icon by diffing the
before/after screenshots (largest roughly-square changed region, polled until the install progress
ring stops moving), crop it and diff its silhouette against the committed mask.

Results on iOS 26.5 / iPhone 17:

- **The mask matches to mean 0.13 px, max 0.70 px** (flat probe icon, 192 rows). Essentially exact.
- **iOS applies no gloss to the flat PNG we ship.** A pure-magenta probe comes back
  rgb 254-255, 0-1, 254-255 across the whole interior: no gradient, no specular, no inner border.
  So *masking alone* is a faithful prediction, and `preview` deliberately does nothing else.
  (Careful: `ictool` *does* gloss its render, because a `.icon` document is the layered Icon Composer
  format that opts into the glass treatment. That is a property of the format, not of our asset.)
- **The home-screen icon is 192 px, not 180.** iOS 26 draws it at 64pt @3x. Worth knowing before
  hand-tuning pixel art to a size that stopped being current.
- **The downscale filter is closest to mitchell.** Swept every candidate against the real device
  render of a busy pixel-art icon at 192 px: mitchell 5.44/255 mean delta, cubic 5.90, lanczos2 5.94,
  lanczos3 6.93, nearest 11.82, all at zero pixel offset. So `preview` uses mitchell. The ~2%
  residual is filter-level difference on high-contrast pixel art, not a treatment iOS is applying,
  and no shift was needed, which confirms the crop and the mask are aligned.

## Drawing a NEW icon — the actual steps

```
node tools/icon-mask.js template --overlay          # tools/icon-masks/template-1024-overlay.png
node tools/icon-mask.js template --overlay --inset 28 --out /tmp/guide.png   # + the border curve
```

0. **If the art came out of an image generator, snap it first:** `tools/pixelsnap.js` (off-grid soft
   pixels → a real grid, gradient soup → a small palette). That is the usual upstream step for our
   icons. Its `--grid` choice interacts with the mask: coarse cells in the corners mean the cut lands
   on whole cells and reads as a chewed edge, so check afterwards, not before.
1. **Get the overlay layer.** `template --overlay` is transparent inside and red outside, so it
   floats on top of your artwork in Procreate/Photoshop as a locked top layer: red = gone. The plain
   `template` (white inside, opaque red outside) is the version to draw *under* if you'd rather.
   Both are committed at `tools/icon-masks/template-1024*.png`, so you can just open them.
2. **Draw the background full-bleed to the square edge.** All 1024×1024 of it, right into the
   corners, even though the corners get thrown away. Anything you leave blank there (white,
   transparent, a different colour) becomes a hard edge or a halo once the mask cuts.
3. **Keep everything that matters inside the big circle.** That's the heavy blue guide, and the rule
   is exact: the inscribed circle is entirely inside the mask, so content inside it cannot be cut.
4. **If you're drawing your own border or chassis, draw it on the `--inset` curve** (green), not as a
   rounded rect. A rounded rect's circular corners poke through the mask near the diagonals and the
   border reads as broken at four points. This is the mistake to know about, and it is invisible
   until you see the icon on a phone.
5. **Export 1024×1024 PNG, opaque.** No transparency anywhere, or the build fails validation. An
   unused alpha channel is harmless (actool flattens it).
6. **Check it, then look at it small:**
   ```
   node tools/icon-mask.js check   my-icon.png    # per corner: flat background (safe) or lost detail
   node tools/icon-mask.js preview my-icon.png    # all six real sizes, light + dark
   ```
   `check` wants all four corners reading ✓ or `·`. `preview` is where you find out the wordmark
   dies at 87 px — a lo-fi icon usually needs *fewer, bigger* shapes than feels right at 1024.
7. **Install it:** drop it at `apps/<app>/icon.png` and point the manifest at it
   (`"icon": "apps/<app>/icon.png"`). `build-app.js` re-prints the verdict when it stages it, and the
   editor's Apps tab has a 🎨 icon button that runs check + preview for you.
8. **Optional, when it matters:** `node tools/icon-mask.js device my-icon.png` for the real thing off
   a booted iOS 26 simulator instead of a prediction.

## The workflow

```
node tools/icon-mask.js template                 # design against this (red = gone)
node tools/icon-mask.js template --overlay       # transparent inside, float it over artwork
node tools/icon-mask.js check   apps/<app>/icon.png    # what gets cut, per corner
node tools/icon-mask.js preview apps/<app>/icon.png    # what it will LOOK like, every real size
node tools/icon-mask.js device  apps/<app>/icon.png    # what a real iOS 26 phone actually draws
```

`preview` is the everyday one: the icon masked and shrunk to 1024 (App Store product page), 192
(home screen), 128 (iPad home), 120 (Spotlight), 87 (Settings) and 60 (notifications), on a light and
a dark backdrop, smooth-scaled the way iOS scales. Only the 192 is measured; the rest are Apple's
pt×scale and are labelled as such in the sheet. It is where a lo-fi icon's fate is decided, because
the mask takes the corners but the *downscale* takes the fine detail, and at 87 px a pixel-font
wordmark turns to mush.

Two places it now runs without being asked:

- **`build-app.js`** prints the corner verdict right after it stages the icon into the asset catalog,
  the last moment the icon is still cheap to change. Advisory: a build is not the place to refuse
  over taste.
- **The editor's Apps tab** grew a per-app **🎨 icon** button (`check` + `preview`, sheet opened for
  you). See [`../guides/editor-features.md`](../guides/editor-features.md).

`check` is the oracle. Per corner it asks whether the cut region is **flat background** (safe: the
mask takes nothing but backdrop) or carries **detail** (loss), reports how far the lost ink reaches
in, and writes a 3-up proof PNG: as drawn / as iOS shows it / what got cut, in magenta. `--quiet`
exits nonzero when a corner loses detail, so it can gate a release.

It also flags an icon with **transparent pixels**, which App Store validation rejects outright.
Judge that from the raw bytes: sharp's `extractChannel(3)` does *not* reliably hand back the alpha
band, and reported min=9 on a fully opaque icon.

## Open

- **Only the iOS/iPadOS mask is measured.** `ictool --platform` also takes macOS and watchOS
  (rounded-square and circle). Same extraction, not yet run. Add profiles when we ship there.
- **Legacy vs Icon Composer authoring.** We ship a flat 1024 PNG in the asset catalog and let iOS
  mask it. Icon Composer's layered `.icon` format buys the iOS 26 glass / tinted / dark renditions
  instead of one flat image for all of them. Not adopted; it would be a real design decision, not a
  build change.
- **A `check` step in the app build.** `build-app.js --ios` stages the icon; it could run the corner
  check and refuse a build that loses detail.
