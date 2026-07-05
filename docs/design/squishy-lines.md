# Squishy lines — a velocity-brush drawing cart that animates for almost free

STATUS: BUILDING (2026-06-30) — design settled (320×320, 2-tone first, boil is an opt-in mode).
Shipped: 8 brushes (ink/pen/fineliner/marker/chalk + Krita-style **sketch**/**spray**/**bristle**) in
a **tool dropdown**, a thickness slider, the **bevel** emboss, the **boil** living loop, and a
**7-colour pen** (incl. CMY), **dithered strokes** (Bayer density ramp via `fillp`), and **per-stroke bevel/boil**
(select-tool stage 1) + the full **select tool** (stages 2–3: pick a stroke, edit its properties
non-destructively via the bar + a bevel-size/boil-intensity strip) (`tools/carts/squishy.c`). The
cart is now a tiny vector editor. What's left is the **v2 icon-pipeline slice** (plan below):
**save/load** (stroke files — nothing persists today), **closed-shape fill** (the *vector* fill;
raster flood-fill + the persistent-layer refactor come later), the **`squishy-export` tool** + a
live snapped preview — then stamps / trace underlay / stabilizer as the delight pass, plus the
`spec()` and the boil-cache perf pass. v1 plan + progress below.

> **Update (2026-07-01) — rotation brushes, live width, drop shadow, and a coverage gate.**
> The brush table is now pure data (a `Brush` carries a width recipe + an **angle recipe** + an icon
> slot), which turned the calligraphy nib into a FAMILY of four sharing one renderer: **nib** (fixed
> angle), **brushpen** (angle + a speed swell), **reed** (per-point seeded angle chatter — a dry pen
> that still boils), **twist** (angle winds along the stroke). Width is now **live on the mouse
> wheel**, captured *per point* into the `Sample` (alongside speed) so scrolling mid-stroke tapers/
> swells the line where you did it — real pressure, not a uniform rescale (the harness `.rec`/record
> paths gained a `w` wheel tag to make this replayable — `runtime/studio.c`). Added a per-stroke
> **drop shadow** (the whole silhouette re-rendered dark, offset away from the *same* sun as the
> bevel — toggle in the SUN popover). While wiring these we found rim/fill features were applied
> inconsistently per render path (drip ignored dither, the nib family ignored outline/shadow/bevel);
> fixed by routing the nib rim features through a parameterised ribbon and wrapping drip's body in
> `fillp`. That whole bug class — **a feature that silently no-ops for one brush** — is now guarded
> by **`node tools/squishy-features.js`**: it renders the cart's built-in `SQUISHY_MATRIX` grid
> (every brush × every feature, each cell one reference stroke with that feature on) and pixel-diffs
> each cell against its baseline, failing on any disagreement with a declared support matrix.

> **Update (2026-07-02) — per-stroke gradient fill (the dpaint dithered-gradient, scoped to a brushstroke).**
> A stroke's body can now dither from the pen colour to a second colour across the stroke — the same
> Bayer-threshold trick as dpaint's gradient tool (`cgrad`), but bounded to the stroke's coverage
> instead of a dragged box. Controls: on/off, the far colour, an **angle**, and a **spread** — the
> blend-band width, which is the parameter people can't name: small spread = solid ends with a narrow
> blend (`aaaaa→bfy→zzzzz`), spread 1 = an even ramp end-to-end (`abc…xyz`). Implemented as
> `render_gradient()` (rasterise coverage into `drip_cov`, project each covered pixel onto the ramp
> axis → t, stretch by spread, Bayer-threshold picks from/to). Wired for the stamp brushes and the
> wet-paint body (drips run in the from-colour); the nib ribbon is a listed follow-up. The
> `SQUISHY_MATRIX` coverage grid + `squishy-features.js` gained a `grad` column so the fill is guarded
> like the rest. This is the "B" fork of the two considered — a per-stroke fade, not the area-fill
> gradient (which still wants the flood-fill / persistent-layer refactor).
>
> The gradient's controls live in a popover reached from an **on-screen button** (a ramp glyph) *or*
> the `G` key. Fitting it forced the toolbar to **two rows** — row 1 = brush + effects, row 2 = colour
> + fill (palette · dither · gradient · select) — the honest fix once a single row was full (cramming/
> shrinking had already bitten the drop-shadow). Two rendering fixes shipped alongside: the brush-ring
> **cursor** now shows the brush's nominal caliber (was pulsing with pointer speed as you moved — it
> read as constant resizing), and the gradient body rasterises its coverage **stepped along each
> segment** like `render_stroke` (was one disk per sample → visible blobs on the thin tapered start).

> **Update (2026-07-05) — the v2 direction: from lovely toy to icon pipeline.** A fresh look at the
> tinyjam marketing icons (the AI → `pixelsnap` outputs this doc chases) against what's shipped:
> **five of the seven character ingredients now exist** (wobbly ink outline, bevel + sun, the
> outline+fill bubble lettering, per-stroke dither, mixed weights). What actually blocks drawing
> *that icon* in squishy is more basic: **nothing persists** (no save/load — a multi-session piece
> is impossible), **no filled regions** (the icon is mostly filled blobs), **no garnish stamps**,
> and **no way out of the cart into an icon file**. That gap analysis became the v2 plan below.
> Two calls made there worth flagging: **closed-shape fill before flood-fill** (a closed stroke
> scanline-fills as a *vector* shape — stays pure `f(stroke, seed)` and inherits
> outline/bevel/dither/gradient/boil/select for free, no layer refactor needed), and the **Krita
> stance** (steal input-quality features — stabilizer, dwell pressure — never raster layers or
> brush-engine sprawl; the identity is the editable, seeded, relightable vector scene).

> The shower idea: we'd been making cart icons by running an AI-generated image through a
> `.cart.js` (sprite-draw + `pixelsnap`). The results are nice — but **frozen**. What if you could
> *draw* in that loose, hand-inked style from the start, and get a living, breathing version
> **almost for free**? That "almost for free" is the whole reason this doc exists: the same data
> that makes a squishy line lets you re-render it with a little wobble — so animation falls out of
> the drawing, instead of being authored on top of it.

This is an **asset-authoring tool first** (the alternative to the AI-image → `pixelsnap` route),
and a lovely toy second. The AI route gives you one still frame and no underlying structure; drawing
natively gives you *stroke data*, and stroke data is what perturbs into motion.

## The pitch in one line

A drawing cart with a small set of **velocity-sensitive brushes** (pencil / fineliner / marker /
ink / paint) whose strokes have *life* — taper, speed-driven width, organic jitter — and an opt-in
**boil** mode that turns any finished piece into a 2–3-frame living loop you can export as an
animated sprite.

## The character we're chasing — the tinyjam icon

The look that started this is the **tinyjam app icon**, and especially its `pixelsnap` outputs. The
reference set (commit these as the north-star target):
`docs/marketing/tinyjam/icons/tinyjam-icon-pixel.png` (the source) and
`docs/marketing/tinyjam/icons/snapped/tinyjam-icon-pixel.snapped-1bit.png` (the 2-tone target —
*this* is the v1 feel) + `…snapped-pico.png` (the grow-to-limited-palette target).

The "character" is seven separable ingredients — each maps to a tool/feature here:

| what gives it character | the feature that makes it |
|---|---|
| **wobbly ink outline** — thick black line that swells & tapers irregularly (the lettering, the panel border) | the **ink brush** (speed→width + taper + seeded jitter) — the hero |
| **bevel / faux-3D** — a thin light line on the top edge + darker line on the bottom, lit from above (knobs, buttons, bezel, panel) | the **bevel tool** (see below) — its own thing, *not* a line brush |
| **chunky rounded forms** — soft, bubbly, generous corners everywhere | brush stamp shape + how fills round off |
| **dither shading / paper grain** — ordered/diagonal dither in the midtones, printed/riso feel | a **texture/shade fill** mode (reuse `pixelsnap`'s dither vocab) |
| **hand-drawn garnish** — sparkle asterisks, speed-tick motion lines, a heart, speckle, notes | a **stamp/ornament** tool |
| **mixed line weights** — fat hero outline *and* thin interior detail in one piece | ink brush + fineliner, on the same canvas |
| **warm limited palette** — cream paper, black ink, a few saturated accents | the palette growth path (2-tone → limited) |

> The single best statement of the v1 target is `…snapped-1bit.png`: black ink on cream paper, wobbly
> outline, dithered shading, beveled blobs. If the cart can produce *that* by hand, it's working.

## Settled decisions (this session)

1. **Canvas is 320×320** — square, and big enough for brush dynamics to actually read (taper,
   stamp-spacing, speed→width are meaningless at icon size; see "Resolution & the pipeline").
2. **Boil is NOT the headline — it's a toggle.** The headline is *the brush feel*: gorgeous squishy
   lines you draw by hand. Boil is a mode you flip on when a piece is done, to make it live / to
   export an animation. A still squishy drawing is already the win; boil is the bonus.
3. **Start 2-tone** (ink on paper — two palette indices). That's where this aesthetic sings and it
   fits the "humble lo-fi surface" north star. **Grow from there**: richer limited palettes next,
   24-bit (`pset_rgb`) as an opt-in for painterly tools much later.

## The one architectural decision everything hangs on

**A stroke is data, never pixels.** A naive paint app commits pixels to the framebuffer and forgets
the stroke. This cart can't — because boil *re-renders the same stroke* with fresh jitter, and undo
/ edit / re-palette all need the original path.

```c
typedef struct { float x, y, speed; } Sample;      // one input sample along the path
typedef struct {
    int    tool;        // BRUSH_PENCIL / _FINELINER / _MARKER / _INK / _PAINT
    int    color;       // palette index (2-tone v1: ink or paper)
    unsigned seed;      // per-stroke seed → deterministic jitter (boil reseeds per frame)
    int    n;
    Sample pts[STROKE_MAX];
} Stroke;
```

Rendering is then a **pure function of `(Stroke, frame_seed)`** → drop in a different seed, get a
different wobble of the *same* line. That purity is what makes the boil deterministic (seeded
`rnd()`, so frame 2 always boils the same way), what makes it testable (a `spec()` can assert
identical strokes render identically given the same seed), and what makes boil-as-export trivial.

## The brush engine

Each tool is **a stamp + a spacing + a dynamics curve**. A stroke renders as a chain of stamps along
its path (stamp-spacing — how real brush engines work — *not* a fat polyline).

| tool | stamp | speed → ? | feel |
|---|---|---|---|
| pencil | tiny, hard, grainy | speed → opacity (fast skips / lightens) | dry, scratchy |
| fineliner | thin, crisp | barely any (near-constant) | clean ink |
| marker | wide, soft, flat | none; overlap darkens | even, juicy |
| ink brush | round | **speed → big width swing + end-taper** | *the squish* |
| chalk | round + dropout | drops ~40% of stamps + heavy wobble | dry, broken, grainy |
| sketch | — (threads) | each point threads to nearby earlier points (à la Krita Sketch) | hairy, gestural web |
| spray | scatter | dots scattered per point; cloud radius follows speed | airbrush / stipple |
| bristle | raked hairs | N parallel 1px hairs across the width, perpendicular | dry multi-line / oil |
| paint | wide, textured | speed → width + wet edges | gloopy (later / 24-bit) |

Three things stack to make a line feel alive:
1. **Speed → width**, via a per-tool curve. (Drag fast → thin tail; ink it slow → it fattens.)
2. **Taper envelope** over the stroke length — thin at the start and end (you lifted off).
3. **Per-stamp noise** (seeded) — a little width/offset wobble so it's never mechanical.

Velocity is DIY and trivial: `mouse_x/y` delta per frame, smoothed (an EMA so a single jittery
frame doesn't spike the width). Touch comes later — this is mouse-first for v1, but the same
delta-based speed works for a finger and would be gorgeous on iPad.

### The bevel tool (faux-3D edges) — a "brush of sorts"

The thing that makes the tinyjam knobs/buttons/bezels look *molded*: a thin **highlight on the lit
edge + shadow on the far edge**, under **one global light direction** (from the top — top/upper-left
edges catch light, bottom/lower-right edges fall into shadow). Pure DeluxePaint emboss. It's not a
line brush — it's an **edge treatment**, so it's its own mode. Two flavours, both lean on the
canvas read-back (`pget`/`pget_rgb`, `enable_pget(true)`):

- **Auto-bevel** — finish a filled shape, hit bevel: edge-detect its outline, lay a 1px highlight on
  boundary pixels whose outward normal points *up*, a 1px shadow where it points *down*. Feels magic
  (draw a blob → it pops 3D).
- **Bevel brush** — stroke *along* an edge; it paints light on one side of the stroke and dark on the
  other, picked by the stroke's normal vs. the light direction.

Bevel **composes** with the ink outline — outline gives the cartoon edge, bevel gives the volume;
together they *are* the tinyjam look. Global light direction is one setting (default: top). In
2-tone, "highlight" = paper and "shadow" = ink (so bevel reads as a thin paper rim + an ink underline
on blobs); in a limited palette it picks a lighter/darker ramp neighbour.

**As built (v1, 2026-06-30) — the offset-emboss, not edge-detect.** Both flavours above wanted
canvas read-back, but `pget`/`pget_rgb` **read *last* frame's canvas** (by design), so a same-frame
silhouette edge-detect isn't available — and the cart re-renders every stroke from data each frame,
so there's no stable framebuffer to scan anyway. The shipping bevel is therefore a **3-pass offset
emboss** done from the stroke geometry: draw a SHADOW copy offset toward the dark side (+1,+1) and a
HILITE copy offset toward the light (−1,−1), then the INK body on top — the ink covers the centre and
leaves a 1px light rim on the upper-left and a dark rim on the lower-right. Pure geometry, no
read-back, **deterministic** (so it's boil-ready). It reads as a clean DPaint emboss on bold strokes
and a domed 3D ball on a filled blob. Tones: HILITE `CLR_LIGHT_PEACH`, SHADOW `CLR_BLACK`, body
`CLR_BROWNISH_BLACK`. A true silhouette-rim auto-bevel (light/mid/dark, needs a cached layer to scan)
stays open for when fills + a 3-tone ramp land.

## Boil — the opt-in mode

Flip it on and the drawing breathes. The mechanism, and why it's "almost free":

- **Boil frames *are* a sprite-animation cycle.** Re-render every stroke 2–3 times, each with a
  fresh per-frame seed → 2–3 buffers that differ by a few pixels of jitter. Cycle them at ~6–8fps
  (slow — the classic hand-drawn boil) → the living loop. Those frames drop straight into the
  engine's existing frame-strip animation.
- **What perturbs:** small endpoint jitter + per-stamp width/offset noise (NOT the whole path
  re-flowing — that reads as redrawing, not boiling). 1–3px of wobble is the sweet spot.
- **2–3 frames** (classic squigglevision is 2–3). Default 2.
- **Two boil styles (per-stroke, 2026-07-01):** **WOBBLE** (the original per-point jitter) and
  **PULSE** — a subtle smooth grow/shrink (±`BREATHE_AMT` scale about the stroke's centroid, continuous
  sine at `BREATHE_SPEED`, per-stroke phase offset from the seed so strokes breathe independently).
  Dispatched through a small `Boil` context (`{amt, style, fseed, cx, cy, phase}`) + `boil_pt()` that
  every renderer calls per point — so adding a 3rd style is just another branch there. The boil button
  cycles off → wobble → pulse. (Wobble cycles `fseed` per beat → width animates too; pulse keeps a
  stable `fseed` so only the scale breathes.)

### Performance — the gotcha and its fix

Re-rendering every stroke every *display* frame would melt as the drawing grows. The move:
**render the N boil frames once into N cached buffers and just cycle them** — only re-render when a
stroke is added or edited. Finished strokes bake to a static layer; only the active/selected stroke
needs live re-render while you're drawing. That holds 60fps regardless of canvas density. (The boil
loop itself plays at ~6–8fps off the cached frames — cheap.)

**As built (v1, 2026-06-30).** Boil ships as a *live re-render every frame* — no cache yet. The
display frame (`frame()`) picks one of `BOIL_FRAMES` variant seeds (held `BOIL_PERIOD` frames each ≈
7.5fps); each committed stroke renders with `stroke.seed ^ VARIANT[v]`, which re-rolls both the
per-stamp width wobble *and* a small per-point positional offset (`BOIL_JIT` ≈ 1.2px, keyed per
sample so the wobble is coherent, not fizzy). The bevel passes inherit the same jitter so the rim
moves with the body. The stroke you're actively drawing doesn't boil. The cached-buffer version
above is the open perf todo (matters once a drawing is dense); the look is already right.

**Profiled (2026-07-01, 120 strokes ≈ the `MAX_STROKES` cap, headless `play.js` → `build/perf.json`,
each ~30 pts).** Everything re-renders every frame (no cache yet), so cost = strokes × passes ×
stamps, and **`circfill` (the round stamp) is the hot primitive** everywhere:

| scene (120 strokes) | avg ms/frame | circfill/frame | note |
|---|---|---|---|
| plain ink (1 pass) | **7.6** | 12.4k | the floor |
| + outline + bevel + boil + dither | **66.8** | 56.2k | **worst case** — decoration stacks *passes* |
| drip ×120 | 20.4 | 29.6k (+10.5k `pset`) | coverage-grid raster is cheap-ish; runs add stamps |
| impasto ×120 | 51.8 | 43.6k (+14.2k `line`) | 3 rim passes + raked streaks |
| everything mixed + spinning sun | 24.3 | 30.9k | lower than row 2 — many strokes are cheap 1-pass brushes |

Takeaways: (1) the killer is **passes per stroke** — plain = 1, outline +1, **bevel +2** → up to **4× the
stamps**, which is the 7.6 → 66.8 ms jump, not any single fancy brush. (2) At realistic counts (≤~30
strokes) even the worst case is fine; 120 (the cap) with full decoration is where it bites. (3) The fix
is the **cached-layer** todo above — bake finished/undecorated strokes once so only the active + boiling
strokes re-render; that erases rows 2–5 for static art. Reproduce by seeding N strokes in `init()` under
`#ifdef DE_TRACE` and reading `workMsAvg` from `build/perf.json`.

## Resolution & the pipeline (why 320×320 *and* tiny icons both work)

Brush dynamics need pixel room, but cart icons/sprites are tiny — these seem to conflict. They
don't, because the icon path reuses what we were already doing with the AI images:

> **Draw big (320×320) → boil (N frames) → `pixelsnap` each frame down to icon size + a small
> palette → animated sprite slot.**

`pixelsnap.js` already does grid-snap + posterize-to-small-palette (that's the part of the AI route
we liked). The wobble survives the downsample as a subtle, charming pixel boil — which is exactly
how good pixel-art idle animation reads. So the full story: *squishy draw → living sprite*, using the
same palette-snapping, except now it's **yours and it moves**.

So there are two outputs, same engine:
- **The full-res piece** (320×320) — a living squishy drawing in its own right.
- **The downsampled animated sprite** — boil frames through `pixelsnap` → a frame strip you can
  hand to a cart (the AI-image-icon replacement).

## Palette growth path

1. **2-tone (shipped):** ink + paper. The soul of it.
2. **Limited palette (shipped):** a **7-colour pen** — black / blue / red / green / cyan / magenta /
   yellow (`COLORS[]`; cursor ring shows the live colour). Picked from an **always-visible palette
   strip** — all seven swatches drawn with their real colour (via `rectfill`, not sprites), the active
   one wearing an accent tab under it (the same on-state language as the bvl/boil toggles). pico32 has
   no true cyan, so cyan ≈ `CLR_BLUE_GREEN` (dark teal). Each stroke stores its colour (non-destructive).
3. **24-bit (later, opt-in):** the painterly tools (`paint`, watercolour) via `pset_rgb` /
   `rectfill_rgb` (raw `0xRRGGBB`, palette-bypass). **Caveat to remember:** `pset_rgb` pixels bypass
   `pal()` recolouring and don't read back through `pget` (use `pget_rgb`), so true-colour and
   palette tools don't mix cleanly on one layer — keep 24-bit to its own mode/layer.

## Prior art to lean on (don't reinvent the chrome)

`dpaint` (Deluxe Paint–style) and `mariopaint` already solve the **canvas + tool-palette UI**
plumbing — glance there for the panel/tool-strip pattern rather than rebuilding it. The line studies
(`linerider`, `linesym`, `inkrunner`, `linecompare`) are reference for path handling. None of them do
**velocity-driven brush dynamics + boil** — that's the novel core here. For the tool-palette glyphs,
`flank` / `boom` show the code-drawn icon pattern (`tools/sprite-draw.js`), so the brush picker can be
beautiful instead of text labels.

## Open / future "sexy drawing functions" (parking lot)

The maker has more drawing ideas queued; this cart is the home they slot into. Candidates to consider
once v1 lands (not committed):

- **★ Select tool — per-stroke properties (the big one, maker's idea).** The whole drawing is already
  *vector data* (a list of strokes, each a path of nodes), so a SELECT tool is natural: click near a
  stroke → hit-test (point-to-polyline distance) → select it → a contextual property editor tweaks
  *that one stroke*. Built in three stages:
  - **Stage 1 — per-stroke bevel + boil (SHIPPED 2026-07-01).** Migrated `bevel` (bool) and `boil`
    (intensity 0..1) from global toggles to per-stroke fields, captured at draw like
    colour/tool/thickness/pattern. The toolbar toggles now set the **default for new strokes**
    (non-retroactive — drawing a plain stroke then toggling bevel leaves it plain). `draw()` animates
    each stroke by its own `boil` (still strokes use a stable seed; boiling ones cycle the variant).
    Already expressive: some strokes beveled, some boiling, some still.
  - **Stage 2 — select + hit-test (SHIPPED 2026-07-01):** a SELECT mode — promoted to a **dedicated
    top-bar toggle** (marquee icon, sprite 21) + the **S** key, *not* a dropdown brush (it's a mode,
    `selmode`, not a tool). A canvas click runs `pick_stroke` (min point-to-polyline distance vs each
    stroke's half-width + slack); the nearest hit becomes `selected` with an accent **bounding box**;
    empty deselects. Undo/clear keep `selected` valid; cursor is an arrow in select mode.
  - **Stage 3 — contextual property editing (SHIPPED 2026-07-01):** when the SELECT tool has a stroke
    picked, the bar's value controls **retarget to that stroke** and reflect its values — the colour
    palette strip, the dither strip, bevel/boil toggles, and the thickness slider all read/write
    `strokes[selected]` (live re-render); the header reads "edit". A **property strip** drops under the
    bar with **bevel-size** (0..`BEVEL_MAX`) and **boil-intensity** (0..1) sliders, plus **z-order**
    buttons (`to_front`/`to_back` reorder the `strokes[]` array — later = on top). Bevel went from a
    bool to a float *size*. Reflected sliders use stable static `float`s (ui capture is address-keyed).
    Clicks in the strip don't re-pick. → a tiny **non-destructive vector editor**. (Bevel *direction*
    — light angle — is the one deferred bit; everything else from the ask landed.)
  - **Palette:** grew to a **7-colour pen** (black/blue/red/green + cyan/magenta/yellow), shown as an
    **always-visible swatch strip** (see the palette growth path above). pico32 has no true cyan →
    cyan ≈ `CLR_BLUE_GREEN` (dark teal).
  - **Toolbar UI polish (2026-07-01):** the picker chrome caught up with the vector model. The **colour
    cycle → a 7-swatch strip** and the **dither cycle → a 6-swatch strip** (both always visible, active
    one tabbed; drawn directly — colours via `rectfill`, dither via `fillp`, so the old swatch sprites
    8–20 are now unused). The **brush dropdown opens a 2-column icon+name grid** (rows grow from
    `NTOOLS`, so new tools just slot in). SELECT became a **black button with a white marquee box**.
    Whole bar reskinned **ink-on-paper** (`#define UI_COL_*` before `ui.h` → white buttons, black text,
    grey chip borders; sliders get an outline since `ui_slider` draws none). Icon sprites use black
    (idx 0) as their background, so `colorkey(CLR_BLACK)` in `init()` finally lets the glyphs read on
    the paper cell — fixing the near-invisible **sketch** icon.
- **Dithered strokes (shipped, 2026-06-30)** — the *intermediate* step before flood-fill (the maker's
  idea): the fat stamp brushes can be filled with a dpaint-style **Bayer-ordered density ramp**
  (`PATTERNS[]` = 16-bit `fillp` masks computed from the 4×4 Bayer matrix, ~12/25/50/75/87% ink; set
  around the body pass only so bevel rims stay solid; an always-visible swatch strip in the bar, each
  cell drawn live with its own `fillp` mask so it always shows the real fill). Per-stroke (stored), so it stays
  inside the pure-vector model — **no layer buffer needed**. (Directional dithers — h/v-lines,
  diagonal — deferred; the engine has them as `FILL_*` if wanted.)
- **Drip paint (shipped, 2026-07-01 — the maker's walk idea).** A `K_DRIP` "paint" brush: a wide wet
  body, then runs that drip DOWN from **every exposed bottom edge** of the paint. `render_drip`
  rasterises the body into a coverage grid (`drip_cov`, a disk per sample within the stroke's bbox),
  then per column walks top→bottom and treats each paint→paper transition as a band bottom that can
  run. **A run stops when it meets paint again below** (`drip_cov` hit) — so a serpentine stroke drips
  off *each* of its bands: inner bands make short runs into the gaps and merge into the band below,
  while only the truly-open bottom edge falls long. Run length ∝ the band's thickness there × seeded
  jitter, capped at `DRIP_MAX_LEN`; each run is a thin tapering streak with a heavy **bead** where it
  ends, plus the odd spatter dot. Still a **pure function of (path, seed)** — no simulation, no
  persistent layer — so it re-renders identically and inherits undo / recolour / select-edit / z-order
  for free. Tunables: `DRIP_STEP`, `DRIP_MIN_THK`, `DRIP_CHANCE`, `DRIP_LEN_SCALE`, `DRIP_MAX_LEN`.
  - *First cut* collapsed to one lowest edge per column, so a serpentine only dripped off its lowest
    pass (upper bands' drips vanished as you painted down — the maker caught this immediately). The
    coverage-grid rewrite fixed it. *v1 is static* (runs reach final length the instant the stroke
    commits). **Next:** a "wet" run-DOWN animation — ease each run 0→final over ~1s via the boil/age
    timing, then settle. And *cross-stroke* pooling (a run stopping on a *different* stroke's paint, and
    paint building up where strokes overlap) — `drip_cov` only holds the current stroke; a shared layer
    would unlock it (same persistent-buffer refactor flood-fill + the boil cache want).
  - **Bevel + boil fixes (2026-07-01):** drip now takes **bevel** (via the shared `bevel_pass` — a
    raised glossy paint edge under the wet body). And the runs are **dried**: their coverage/geometry is
    computed from the *un-boiled* path (a local still `Boil`), so a boiling body wobbles but the drips
    no longer dance/flicker frame to frame (verified: drip-region PSNR = ∞ across boil beats while the
    body differs).
  - **Perf note:** the coverage raster + clear is O(bbox area) per drip stroke per frame — fine for a
    few, but it's another customer for the layer-buffer cache in the boil-perf todo.
- **Calligraphy nib (shipped, 2026-07-01).** A `K_NIB` "nib" brush — the first brush where width
  comes from **angle, not speed**. A flat broad edge is held at a fixed `nib_angle` (captured per
  stroke, default 45° italic); `render_nib` lays a ribbon of nib-wide quads (`trifill` ×2) between
  consecutive points plus the nib edge (`line`) at each vertex to close joints. Width perpendicular to
  motion = `nibW·|sin(nib − travel)|` — a hairline when you move along the nib, full width across it —
  so loops/curves get the classic broad-nib thick/thins for free from the geometry (no `sin` needed,
  the quads do it). No speed taper. `[` / `]` rotate the nib angle (the default, or a picked stroke's
  in select mode); the cursor previews the nib edge live so you can see the angle. Still pure
  `f(path, seed)`. Next: a per-stroke angle slider in the select property strip; a pressure/gap that
  lifts the nib for split strokes.
- **Oil / impasto (shipped, 2026-07-01).** A `K_IMPASTO` "oil" brush — thick paint faked for the
  2-tone/limited palette (true impasto wants alpha + per-colour light/dark we don't have). `render_impasto`
  stacks three cheap cues: an **auto-bevel rim** (`render_stroke` SHADOW +offset / HILITE −offset, the
  same trick the bevel tool uses) for the raised light-catching edge, the fat colour body, then
  **raked HILITE/SHADOW streaks** along the drag (like `render_bristle` but alternating ridge/groove
  colours) so the paint looks pushed by a stiff brush or knife. Pure `f(path, seed)`. It's a
  characterful *fake*, not physical impasto — good enough for the lo-fi surface; a 24-bit paint layer
  would let it mix real light/dark of the paint colour later.
- **Outline (shipped, 2026-07-01 — from the Tiny Jam icon study).** A per-stroke **OUT** toggle +
  its own colour: `render_stroke` gained a `grow` param, so the outline is just a fatter silhouette
  of the stroke drawn in `outline_color` UNDER the fill (and under any bevel). A black rim + coloured
  fill with the fat **ink** brush *is* the chunky bubble-lettering of the Tiny Jam logo; add **bevel**
  for the highlight and it's startlingly close. Captured per stroke (`outline` px + `outline_color`),
  editable in select mode. The toolbar chip is drawn as a **ring** (not a solid swatch) so it reads as
  "outline colour" rather than another fill swatch. Pure `f(path, seed)`. **Outline-width slider**
  (shipped 2026-07-01): added to the select property strip (`out`, 0..`OUTLINE_MAX`, 0 = off) alongside
  the bevel/boil sliders. Maybe an inner-highlight pass for the full logo look next.
- **Bevel light = a global "sun" (shipped, 2026-07-01, maker's idea).** Bevel *size* stays per-stroke,
  but the light *direction* became a single global `bevel_angle` (deg) used by every bevel rim (and the
  oil-paint rim) at render time — so rotating it **relights the whole drawing at once**, like sweeping
  the sun. `hilite` offsets toward the light (`cos/sin(angle)·size`), `shadow` away. `,` / `.` rotate it
  (±15°). Default 225° = the old up-left light, so existing art is unchanged. Because it's read live (not
  captured per stroke), it's a scene control, not a stroke property — a nice emergent lighting knob.
  **Sun colour** (shipped 2026-07-01): the lit-rim tint is a second global (`light_sel` → `LIGHTS[]`:
  warm peach → golden → neutral white → cool grey "moon"), cycled with `/`. Also global + live, so it
  recolours every bevel/oil rim at once. Shadow stays black. Default peach = the old fixed HILITE.
  **Auto-rotate** (shipped 2026-07-01): `\` toggles `sun_spin`; when on, `update()` advances
  `bevel_angle` by `SUN_SPIN_SPEED` (0.8°/frame, ~7.5s/turn) every frame in any mode, so the light
  slowly circles and every bevel/oil rim shimmers on its own — a cheap living-scene effect, purely a
  render-time read (no per-stroke state). The `boil` loop already lives; now the *lighting* can too.
- **Sun popover UI (shipped, 2026-07-01).** All the sun tweaks got on-screen controls (they were
  keyboard-only): a **SUN** button in the bar (its glyph shows the live sun colour; tabs when open or
  spinning) opens a small popover — a **light dial** you drag to aim the sun (`atan2` of the pointer
  vs the dial centre → `bevel_angle`), the four **sun-colour** swatches, and a **spin** toggle. Modal
  like the tool dropdown (blocks canvas drawing, taps-away to dismiss). The keys (`, . / \`) still work.
  Freeing bar space meant dropping the always-on tool-name label — the dropdown icon identifies the
  tool and an ACCENT ring on the header now flags edit mode (the property strip + selection box already
  signalled it).
- **Flood-fill (still wanted — but no longer first).** The *raster* other half: flood a bounded
  region, lay a dither/ramp in it. This one genuinely needs the **persistent layer buffer**
  (flood-fill is a raster op; the cart re-renders from data each frame and `pget` reads *last*
  frame). Do it *with* the layer-buffer refactor (the same one the boil-cache perf todo and
  cross-stroke drip pooling need — three customers, one chunk). Filling a beveled blob with a
  dither ramp = the tinyjam-knob look. **De-prioritised 2026-07-05:** the v2 plan's
  **closed-shape fill** (a closed stroke scanline-fills as a vector shape) covers the icon's filled
  blobs without the refactor and inherits every per-stroke feature for free — flood-fill is off the
  icon's critical path.
- pressure from dwell-time as well as speed; smoothing/stabiliser on the input path
- symmetry/mirror modes (kaleidoscope ink) — `linesym` is adjacent
- a "redraw" boil variant where the *whole path* re-flows (rougher, more animated)
- texture/grain stamps; wet-on-wet blend for the 24-bit paint mode
- export the boil as a baked clip (`make-gif.js`) for the gallery

## Two-part bar (north star)

- **Verifiable:** the render is a pure function of `(stroke, seed)`, so a `spec()` can assert
  determinism (same seed → identical pixels) and the boil's frame-to-frame jitter stays within
  bounds. The brush math (speed→width curve, taper envelope) is oracle-checkable.
- **Legible & delightful to a stranger:** pick up the mouse, scribble, and the line *feels* alive —
  no manual, no number to read. Flip boil on and it breathes. That's the soul; no oracle checks it.

## v1 plan (the buildable slice)

Shipped the first cut (`tools/carts/squishy.c`, 2026-06-30): the ink brush feels right — slow swells
fat, fast flicks thin, ends taper, seeded wobble keeps it hand-inked — plus the **bevel** toggle
(`B`) that embosses strokes into faux-3D (a filled blob domes into a ball), and a **top tool-bar**
(`ui.h`): a **tool dropdown** of 8 brushes — ink / pen / fineliner / marker / chalk (recipe rows in
the `BRUSHES` table) plus three special renderers, **sketch** (Krita-style web), **spray** (airbrush),
**bristle** (raked hairs) — a thickness slider, and bevel + undo buttons. Each stroke remembers the
tool + thickness it was drawn with. And the **boil** toggle makes a finished drawing breathe (live demo:
`editor/public/clips/squishy/06-boilface.webm` — a smiley drawn, then coming alive). Demo seeds in
`tools/clips/squishy/`.

- [x] Canvas + input: 320×320, capture a stroke as `Sample[]` with per-frame smoothed speed.
- [x] Stroke store: the `Stroke` struct + a list; undo (drop last stroke).
- [x] One brush, done well: the **ink** brush — stamp-spacing, speed→width, end-taper, seeded
      per-stamp noise. Get the squish *feeling* right before adding tools.
- [x] 2-tone palette: ink + paper; a clear button.
- [x] Tool palette UI — a top tool-bar (`ui.h`): a **tool dropdown** with **code-drawn icon glyphs**
      (sprite-draw.js in `squishy.cart.js`, on paper-bg `ui_spr_button`s), a thickness slider, a
      bevel toggle, a boil toggle, an undo button. Drawing is gated below the bar; the dropdown is
      modal (tap-away dismisses). (Icon polish — ink/chalk/sketch — is the remaining nicety.)
- [x] **Sketch brush** (à la Krita's Sketch engine) — a hairy web: a thin spine plus 1px threads
      from each point to a few seeded-random earlier points within reach. Topology keyed by the
      stroke seed (stable), positions jittered by the boil seed → the whole tangle wiggles.
- [x] **Bevel tool** — shipped as a 3-pass offset emboss (`B` toggles it): top-left HILITE rim +
      lower-right SHADOW rim, light from the top-left. See "As built (v1)" above for why emboss, not
      edge-detect. (A true silhouette-rim auto-bevel waits on fills + a 3-tone ramp.)
- [x] Boil toggle (`B`evel's neighbour, key `O`) — cycles `BOIL_FRAMES` jittered variants at
      ~7.5fps (`BOIL_PERIOD` held frames each), driven by `frame()` + a `VARIANT[]` seed table.
      Per-point coherent wobble (the line wiggles, doesn't fizz); the active stroke stays put.
      Ships as a live re-render every frame — the N-cached-frames optimization is the open todo.
- [ ] `spec()`: same-seed determinism + jitter-bounds assertions.
- [ ] The icon pipeline: boil frames → `pixelsnap` → an animated sprite strip (the AI-route
      replacement); document the recipe.
- [x] `de:meta` + bake + `lint-carts`; a screenshot for the owner to play.

## v2 plan — the icon pipeline (the next buildable slice)

*(2026-07-05, from the marketing-icon gap analysis — see the update note up top.)* The one-sentence
version: **save/load + closed-shape fill + the export tool turns squishy from a lovely toy into the
icon pipeline; stamps, the live preview, and the trace underlay make it delightful.**

### Core slice — what blocks the icon

- [ ] **Save/load — stroke files.** The enabler for everything; a real icon is a multi-session
  piece and today a drawing dies with the cart. Strokes are already pure data → `save_bytes()` of
  the `strokes[]` array into a few named slots (saves are per-cart, `build/saves/squishy/`) is a
  small change. The deeper win: a **committed stroke file is the drawing's source artifact** — the
  `tools/clips/` input-track idea applied to art. Deterministic, re-renderable at any size / palette
  / sun angle forever; the AI route's one frozen frame vs *this*. Version the blob (a magic +
  version int up front) so the `Stroke` struct can grow without orphaning old saves.
- [ ] **Closed-shape fill (the vector fill — do this BEFORE flood-fill).** The icon is mostly
  filled blobs (body, knobs, letters). A CLOSE toggle on a stroke marks it a **closed polygon**:
  the renderer closes the path and scanline-fills the interior (even-odd over the polyline), then
  runs the normal stroke pass over the boundary. Stays pure `f(stroke, seed)` — so with **zero new
  feature code** a filled shape inherits outline (the black rim), bevel (domes under the sun),
  dither + gradient (the shading), boil (the whole blob breathes), drop shadow, select-edit,
  z-order. That composition is the whole argument for the vector route; raster flood-fill still
  arrives later with the layer-buffer refactor (which has three customers — see parking lot) but is
  no longer on the icon's critical path. Add a `close` column to `SQUISHY_MATRIX` /
  `squishy-features.js` so the fill is guarded per brush like every other rim/fill feature.
- [ ] **`squishy-export` — the one-command icon mint.** The existing pipeline sentence ("draw big →
  boil N frames → `pixelsnap` each → sprite strip") as an actual tool: load a saved stroke file,
  render N boil frames headless (the `play.js --dump` / `screenshot_request` machinery exists),
  pipe each through `pixelsnap --grid 96x96 --two 0,7` / `--palette pico32`, emit stills + a frame
  strip. One command from a stroke file to exactly the artifacts in
  `docs/marketing/tinyjam/icons/snapped/`. Document the recipe in the tool header.

### Delight slice — what makes it sing

- [ ] **Live snapped preview.** A small corner panel showing the canvas box-downsampled to icon
  size (1-bit / current palette) *while you draw* — the piece reads completely differently at 96px,
  and today you only find out after leaving the cart. In-cart approximation is fine (box-filter +
  threshold/Bayer, reusing the dither vocab); `pixelsnap` stays the authoritative exporter.
- [ ] **Stamp/ornament tool — the garnish.** Sparkle asterisk, heart, note, speed-ticks, speckle,
  star, swirl, cross (~8 covers the icon's garnish). Each stamp is a **tiny stroke recipe** (a
  hand-authored path placed at the tap, usual seed) — NOT a sprite — so stamps wobble under boil,
  take bevel/colour/shadow, and are select-editable like everything else. Emergent, not
  special-cased.
- [ ] **Trace-over underlay — the bridge to the AI route.** Load a reference image dimmed/ghosted
  *under* the canvas and redraw it in strokes. Keeps AI for what it's good at (composition), ends
  with what it can't give (stroke structure — a *living* icon, not a cleaned-up still). Needs an
  image-load path (bake the reference into the sprite sheet via `.cart.js`, or a raw-RGB side file
  read at init) — decide when building.
- [ ] **Stabilizer / smoothing** (promoted from the parking lot) — Krita's most-loved input
  feature; the icon's long confident panel border wants it. A trailing-average / pull-string on the
  input path *before* sampling into the `Stroke`, so it composes with everything downstream.
  Dwell-time pressure can ride along.

### The Krita stance (what to steal, what to refuse)

Steal **input quality**: stabilizer, dwell pressure, symmetry (parking lot). Refuse **raster
layers and brush-engine sprawl** — squishy's identity, the thing Krita can't do, is that the whole
drawing stays an editable, seeded, relightable vector scene. Every feature keeping that property
compounds (see closed-shape fill above); every raster feature dilutes it. "Layers" here is the
z-order that already exists, maybe named groups later.

## See also

- [`editor-cart-workflow.md`](editor-cart-workflow.md) — the sprite story (two sources of truth);
  this cart's downsample-export feeds that conversation.
- [`pixelsnap`](../../tools/pixelsnap.js) (tool header) — the grid-snap + palette posterize step the
  icon pipeline reuses.
- [`cart-authoring.md`](../guides/cart-authoring.md) — sprite patterns + `sprite-draw.js` for the
  tool-palette glyphs.
- [ADR-0007](../decisions/0007-pal-recolors-sprites.md) — `pal()` recolouring (why 24-bit bypasses it).
