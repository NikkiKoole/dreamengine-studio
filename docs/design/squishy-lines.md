# Squishy lines — a velocity-brush drawing cart that animates for almost free

STATUS: BUILDING (2026-06-30) — design settled (320×320, 2-tone first, boil is an opt-in mode).
Shipped: 8 brushes (ink/pen/fineliner/marker/chalk + Krita-style **sketch**/**spray**/**bristle**) in
a **tool dropdown**, a thickness slider, the **bevel** emboss, the **boil** living loop, and a
**4-colour pen**, **dithered strokes** (Bayer density ramp via `fillp`), and **per-stroke bevel/boil**
(select-tool stage 1) + **select & highlight** (stage 2) (`tools/carts/squishy.c`). The core is done;
what's left: the select tool's **stage 3** (a contextual property panel to edit the selected stroke),
a real **flood-fill** (with a persistent-layer
refactor — see parking lot), the pixelsnap animated-icon export, a `spec()`, and the boil-cache perf
pass. v1 plan + progress below.

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
2. **Limited palette (shipped, 2026-06-30):** a **4-colour pen** — black / blue / red / green
   (`COLORS[]` + swatch sprites 8–11; the cursor ring shows the live colour). Each stroke stores the
   colour it was drawn in, so switching never restyles finished strokes (same as tool + thickness).
   Next palette step if wanted: a darkening ramp so marker/overlap deepens within a hue.
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
  - **Stage 2 — select + hit-test (SHIPPED 2026-07-01):** a SELECT tool (last in the dropdown,
    marquee icon). A canvas click runs `pick_stroke` (min point-to-polyline distance vs each stroke's
    half-width + slack); the nearest hit becomes `selected` and gets an accent **bounding box**;
    clicking empty space deselects. Undo/clear keep `selected` valid. Cursor switches to an arrow in
    select mode. No editing yet — that's stage 3.
  - **Stage 3 — contextual property panel (TODO):** when a stroke is selected, show editors (swatch /
    dither / bevel size+dir / boil intensity / thickness) that write back to it and re-render live.
  Turns the cart into a tiny **non-destructive vector editor** — the natural endpoint of "it's all
  just nodes." (Per-stroke bevel *size/direction* — beyond on/off — also lands with stage 3.)
- **Dithered strokes (shipped, 2026-06-30)** — the *intermediate* step before flood-fill (the maker's
  idea): the fat stamp brushes can be filled with a dpaint-style **Bayer-ordered density ramp**
  (`PATTERNS[]` = 16-bit `fillp` masks computed from the 4×4 Bayer matrix, ~12/25/50/75/87% ink; set
  around the body pass only so bevel rims stay solid; a cycle button in the bar). The swatch sprites
  decode the same masks so the button always shows the real fill. Per-stroke (stored), so it stays
  inside the pure-vector model — **no layer buffer needed**. (Directional dithers — h/v-lines,
  diagonal — deferred; the engine has them as `FILL_*` if wanted.)
- **Flood-fill (still wanted)** — the *raster* other half: flood a bounded region, lay a dither/ramp
  in it. This one genuinely needs the **persistent layer buffer** (flood-fill is a raster op; the cart
  re-renders from data each frame and `pget` reads *last* frame). Do it *with* the layer-buffer
  refactor (the same one the boil-cache perf todo needs) — one chunk that also unlocks the boil cache.
  Filling a beveled blob with a dither ramp = the tinyjam-knob look.
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

## See also

- [`editor-cart-workflow.md`](editor-cart-workflow.md) — the sprite story (two sources of truth);
  this cart's downsample-export feeds that conversation.
- [`pixelsnap`](../../tools/pixelsnap.js) (tool header) — the grid-snap + palette posterize step the
  icon pipeline reuses.
- [`cart-authoring.md`](../guides/cart-authoring.md) — sprite patterns + `sprite-draw.js` for the
  tool-palette glyphs.
- [ADR-0007](../decisions/0007-pal-recolors-sprites.md) — `pal()` recolouring (why 24-bit bypasses it).
