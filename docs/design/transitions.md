# transitions — scene-to-scene wipes, from demo cart to reusable tooling

STATUS: BRAINSTORM / exploration (2026-06-22). Nothing decided, no code planned yet.
Consumer-in-waiting: [`showreel-teaser.md`](showreel-teaser.md) (how its shots would be cut).
This note captures what exists (the `transitions` cart), what it would take to make it
*usable* across carts, and the open design questions — so the thinking isn't lost.

## What exists today: the `transitions` cart

[`tools/carts/transitions.c`](../../tools/carts/transitions.c) (228 lines, tagged
`tech-demo`/`toy`) is a finished, ear-and-eye-tested **vocabulary + technique demo**. It
plays eight classic scene bridges side by side over a live world (a sunny hilltop ⇄ a
starlit cave), with a selectable easing curve:

| | transition | how the mask is drawn |
|---|---|---|
| IRIS | a circle closes onto the hero, opens on the next place (SNES Mario) | `ring()` annulus from a shrinking hole |
| CLOCK | a radial sweep wipes round like a clock hand (Star Wars) | `arcfill()` pie wedge |
| WIPE | a hard black edge slides across | `rectfill()` growing width |
| CURTAIN | top + bottom slabs slide shut, then part | two `rectfill()` slabs |
| BLINDS | venetian slats snap closed | N banded `rectfill()`s |
| DISSOLVE | ordered-dither pixel dissolve (Genesis / Sonic) | `fillp()` Bayer threshold + `rectfill()` |
| FADE | the whole screen dims to black and back | `fade(p)` |
| SLIDE | the new scene shoves the old one off-screen, no black middle | `camera()` offset, both scenes drawn |

### The load-bearing insight (worth promoting)

Seven of the eight are **one idea**: *render the scene normally, then overlay a black mask
whose shape encodes "how covered are we" (`p` = 0 clear … 1 fully black).* OUT ramps `p`
0→1, you **swap scenes at full black**, IN ramps `p` 1→0. Every transition is just a
different black mask — most a single draw primitive. The phase machine is tiny:
`IDLE → OUT → (swap) → IN → IDLE`, with `t` (0..1) eased through `ease_in/out/in_out/back`
so the motion accelerates into black and settles softly on the reveal.

**SLIDE is the deliberate odd one out** — no mask, no swap-at-black. It draws *both* scenes
co-resident and `camera()`-shifts them in one motion; because the offset is a *position*,
`ease_back`'s overshoot is physically visible (the incoming scene slides a touch too far,
then springs back). That difference is the whole crux of the reusability question below.

### Generalizing the mask: luma-mask (image-thresholded) transitions

The demo *hand-draws* each black mask with a primitive (`ring`, `arcfill`, banded
`rectfill`). One generalized mechanism does **all** shapes at once: **threshold a grayscale
image by `p`** — a pixel is "covered" (drawn black) when its gray value `< p`. The grayscale
image *is* the transition: a radial gradient → iris, an L→R ramp → wipe, a sawtooth ramp →
the saw-blade edge, a noise texture → dissolve, a heart / logo / swirl → that shape melting
in. One code path, infinite shapes — exactly how RPG Maker "imagemask" transitions and video
editors' "luma wipe" / "gradient wipe" work (see References). The appeal here: the *shape*
becomes **data** (a tiny grayscale sprite), so you draw new transitions in the sprite editor
instead of writing a new `mask()` case.

The catch — cost, and the same all-cart-land tension as SLIDE. Thresholding to black means
*filling the covered pixels*, and an arbitrary shape isn't a neat row-span you can `rectfill`.
Three cart-land-honest options, cheapest first:

- **Analytic shapes stay primitives.** Wipe / curtain / blinds / iris / sawtooth are row- or
  column-contiguous, so the demo's one-primitive-per-shape approach is already optimal — keep
  it. The generalization is only *needed* for non-analytic shapes.
- **Cell-grid luma mask (the pragmatic cart-land version).** Sample the grayscale at a coarse
  grid (say 40×25 cells) and `rectfill` the covered cells: ~1000 fills/frame, cheap, and the
  chunky result reads as deliberately retro — on-aesthetic, not a compromise. Arbitrary
  shapes with **zero engine help**.
- **Pixel-perfect, per-row spans (also cart-land, also cheap — the earlier note was too
  pessimistic).** "Per-pixel" does *not* mean `pset` 64k times a frame (that *is* too slow —
  64k draw calls). Separate the **data** from the **drawing**: precompute the field once into a
  full-res buffer (`unsigned char lumapx[H][W]`, ~64 KB), then each frame *scan* each row for
  contiguous covered runs and `rectfill` each run. That's 64k cheap integer compares (not draw
  calls) + ~1k fills → **pixel-perfect edges at 60 fps, zero engine help.** Degrades toward
  per-pixel only for a field that's tiny disconnected specks per row (a noise dissolve); for
  swirls / irises / sawtooth / logos / gradients it's perfect and fast.
- **True `pset`-per-pixel** is the *only* genuinely slow route, and the lone case that would
  want an engine **blit-with-mask / luma-key** primitive. Not needed — per-row spans cover it.
  (Filed as a "someday, if a noise-speckle field matters" engine note, nothing more.)

Lean: **prototype the cell-grid version in the demo cart** (a new "LUMA" transition reading a
small grayscale sprite) — the cheapest way to find out if data-driven shapes feel good before
touching the per-pixel / engine question.

**BUILT & validated (2026-06-22).** Shipped in [`transitions.c`](../../tools/carts/transitions.c)
as the "**LUMA**" transition — a full-res `unsigned char lumapx[H][W]` swirl baked at `init()`,
drawn **pixel-perfect**: each frame, scan every row of the buffer for contiguous covered runs
(`gray < p`) and `rectfill` each run — 64k cheap *compares* + ~1k fills, **not** 64k `pset`s.
Smooth pixel edges, 60 fps, zero engine help.

> An earlier build shipped this as a side-by-side "LUMA A/B" (chunky 8×8 cells | pixel-perfect
> spans) to compare the two fills; the maker picked **pixel**, so the demo now ships pure
> pixel-perfect and the cell version lives in git history. The cell path stays documented above
> as the cheaper fallback.

Repro track: `tools/clips/transitions/01-luma.script`. Findings that resolve the questions
below:

- **Both are cart-land and cheap.** Pixel-perfect is *not* expensive — the cost is in per-pixel
  draw *calls*, which per-row spans avoid by precomputing the field and filling runs. So the
  §2 "elegant version wants engine support" tension does **not** bite; an engine luma-key
  primitive is unnecessary (kept only as a someday-note for noise-speckle fields).
- **Decision: per-row-span PIXEL is the default for the eventual `transitions.h`** (maker
  preference, 2026-06-22 — the smooth side won the A/B). Cells were only the comparison; the
  smooth path costs barely more (a full-res buffer + a row scan) and looks markedly better.
  Keep cells documented as the cheaper fallback, but the header ships pixel-perfect.
- **The field is the only thing that varies.** Swapping the `init()` formula (or reading a
  grayscale `sget()` sprite) is the *entire* cost of a new shape — "the shape becomes data"
  holds in practice. Next experiment if pursued: read the field from a *painted sprite*.

## Transition vocabulary — ideas to grow (cheap; mask unless noted)

The mask insight makes new transitions cheap, so the vocabulary can grow freely. Ideas beyond
the shipped eight:

- **Toothed wipe — saw or triangle** (mask) — **BUILT (2026-06-22, shipped as "TRIANGLE").**
  A wipe whose black edge is teeth. Turned out to be *also a luma field* — a left→right ramp
  plus a vertical tooth wave (`gray = x/W + amp·tooth(y)`, covered when `gray < p`) — but
  **monotonic in x**, so rather than `trifill` teeth or a span scan, each row's edge is solved
  directly: one `rectfill(0, y, edge(y), 1)` per row, pixel-perfect and ~200 fills. A clean
  demonstration that the luma generalization *subsumes* the analytic shapes. **The tooth
  waveform is the only knob** — triangle (`1−|2·ph−1|`, symmetric points, shipped) vs sawtooth
  (`ph`, slanted blades) is a one-line swap; same field, same math. (a.k.a. zigzag / serrated /
  shark-teeth wipe.)
- **Interlace / scanline** (**CO-RESIDENT**) — alternating rows show scene A then scene B,
  then resolve to one. Both scenes are visible at once → the *callback* family, not a black
  mask. (The 1-px "venetian blind" revealing *black* instead would be a mask variant.) (a.k.a.
  interlace / scanline / line wipe / comb transition.)

*(add more here as they come — tag each mask vs co-resident, and whether it's analytic or
wants the luma-mask path)*

## The gap: it's a demo, not a tool

Everything in the cart is **local statics inside `transitions.c`** — the `mask()` function,
the phase machine, the two hard-coded scenes (`scene_day`/`scene_cave`). No other cart can
use any of it without copy-paste. To go from "look, transitions" to "every cart bridges its
own scenes," the technique has to move into a reusable home — almost certainly a **cart-land
library header**, `runtime/transitions.h`, in the `ui.h` / `gestures.h` / `keybed.h` family
([ADR-0006](../decisions/0006-library-carts-not-engine.md): capabilities the engine
deliberately doesn't own live as static headers, not `studio.h` API).

Why a header and not engine API: a transition is **inherently multi-frame stateful** — it
owns a timer and a phase across frames. That's the opposite of `fade()`, which is
deliberately *immediate-mode* ([ADR-0010](../decisions/0010-fade-is-immediate-mode.md): a
sticky `fade()` caused stale-fade bugs in 27 carts). So the held state (`phase`, `t`, `sel`,
`ease`) belongs in cart-land where `gestures.h` already keeps per-frame state — not in the
engine, which stays immediate-mode.

## The design questions to settle before any code

### 1. How does a generic cart hand its scenes to the transition?

In the demo both scenes are functions in the *same* cart. Real carts model a scene as a
*state* (`enum { TITLE, PLAY, OVER }` + a `switch` in `draw()`). For the **mask-7** this is
easy and clean: the header only ever *overlays* — the cart keeps drawing its own current
state, the header draws the black mask on top, and at the black midpoint it tells the cart
"swap now." Three plausible hookup shapes:

- **Poll** — cart asks `transition_covered()` (true at black) and flips its own state. Most
  in-keeping with the immediate-mode house style; cart stays in control of its state.
- **Callback** — cart registers a `void on_black(void)`; header calls it at the midpoint.
  Cleaner call site, but function pointers are rare in cart-land.
- **Event** — header exposes a `transition_hit_black()` one-frame edge, like `ui_grabbed`.

Lean: **poll/edge**, matching how `ui.h`/`gestures.h` already expose state.

### 2. SLIDE (and any "both scenes visible" transition) is the hard case

The mask-7 never need scene B until black — so the cart only ever draws *one* scene at a
time, which every state-machine cart can already do. SLIDE needs **both scenes drawn the
same frame** under different camera offsets. A generic cart usually *cannot* draw an
arbitrary other state on demand (its `draw()` reads live globals for the current state).
Options, roughly increasing cost:

- **(a) Two-tier API.** Ship the mask-7 as the easy, universal tier; make SLIDE/PUSH opt-in
  and require the cart to provide a `draw_scene(int which)`-style callback that can render
  either side. Honest about the cost; most carts take the easy tier.
- **(b) Snapshot the old scene to a texture.** Capture the outgoing frame once, then slide
  the *image* while the cart draws the incoming scene live. Needs an offscreen-capture
  primitive the engine may not expose to cart-land — investigate before relying on it.
- **(c) SLIDE stays demo-only.** Promote just the mask-7; leave push/slide as a pattern
  carts hand-roll when they truly need it.

This fork (does the reusable header include SLIDE, and at what API cost) is the main thing
to decide.

**Leaning (2026-06-22): keep it all in cart-land → option (a), the callback.** The header is
a cart-land `transitions.h` with no engine API. That resolves the fork: co-resident
transitions take the **callback** path — a cart that wants SLIDE provides a
`draw_scene(int which)` it can call for *either* side, and the header drives the camera
offsets. Option (b) snapshot-to-texture is **deliberately not taken** — it's the only route
that would need engine support (offscreen capture), and "do it in cart-land" rules it out.
The accepted cost is the [cart-authoring](../guides/cart-authoring.md) contract: a cart opting into a co-resident transition
must be able to draw scene A *or* B on demand (most carts refactor to a `draw_scene(which)`
cheaply; the demo already has exactly this shape). The mask-family transitions need no such
contract — they only overlay, so they stay universal.

### 3. Scope — which transitions earn promotion?

FADE is nearly free (it's just `fade(p)` already). IRIS / WIPE / CURTAIN / DISSOLVE are the
crowd-pleasers and all pure overlays. CLOCK and BLINDS are cheap riders once `mask()` is
generalized. The question is whether the header ships all seven masks or a curated subset
(the second-customer rule: promote what a real cart will actually call).

## Three distinct things — keep them separate

"Transitions for a showcase" collapses three *very* different features that are easy to
conflate. Only **A** is this doc. The distinction matters because the cost scales by an order
of magnitude at each step:

- **A — in-cart transitions (THIS doc; the original intent).** *One* cart animates a
  fade/slide *between its own scenes/parts* (title → game → over, or just touring different
  views of itself), captured as one continuous clip. Works today (hand-rolled in the demo);
  the win is a reusable `transitions.h` so every cart's scene bridges feel intentional
  instead of snapping. No external anything — the cart does the transition, the recorder just
  captures a normal run. **Prerequisite for [attract mode](attract-mode.md).** *This is the
  goal; it's the lowest-risk path and the rest of this doc is about it.*

- **B — clip stitching, build-time + passive (a small tool, NOT a player). BUILT 2026-06-22:
  `tools/compose-clips.js`.** Take *already-baked* `.webm`s and glue them with ffmpeg
  crossfades into one **video file** ("5s of `sloop`, wipe, 10s of `moog`"). Nothing live, no
  interactivity — it's video *editing*. Output is a passive reel for the gallery front page.
  A committed `.reel` manifest lists the clips + per-cut transition (xfade names: fade /
  dissolve / wipe* / circleopen…); video uses `xfade`, audio `acrossfade`, so picture and
  sound dissolve together. Clips now carry sound (Layer-A "Sound" is shipped). Serves the
  curated-showcase reel ([decision 0020](../decisions/0020-in-house-tool-curated-showcase.md)).

- **C — a LIVE multi-cart player (a separate, much bigger idea — stubbed, not specced here).**
  A meta-program that *runs* cart 1 live (or its demo loop), transitions, loads cart 2, and
  tours the catalog — a gallery kiosk / arcade carousel. This is the "external player that
  stitches multiple games together." It is **attract mode generalized across carts** rather
  than within one, and it needs real new plumbing: either the web wasm builds sequenced inside
  one page, or the native runtime taught to load/swap carts at runtime. The most exciting for
  "show people," and the heaviest by far. Tracked as its own line below; belongs with
  [attract-mode.md](attract-mode.md), not here.

How they relate: **A** makes each cart's own scenes (and each clip) look good; **B** is one
way to assemble a reel out of finished clips; **C** is a different way to assemble a showcase —
live and interactive instead of pre-rendered video. A is the foundation for both B's clips and
C's per-cart demo loops. Don't let A's small, concrete win get held hostage to C's scope.

### C — the cross-cart player, as a stub

Not designed here, just parked so it isn't reinvented. If it ever happens it's essentially:
*the attract-mode handoff, but the "next state" is the next **cart**, not the next scene of
the same cart.* The open questions are all C's, not A's — how carts are loaded/swapped (web
page sequencing the wasm builds vs a native multi-cart runner), where the transition between
two carts is drawn (neither cart owns it — a host layer does), and whether it's a fixed reel
or shuffles the curated subset. Revisit alongside [attract-mode.md](attract-mode.md) if the
native single-cart attract prototype proves the feel.

## Open questions (parked, for the decision later)

1. The hookup shape — poll vs callback vs edge (§1). Lean poll/edge.
2. ~~Whether the reusable header includes SLIDE, and via which option a/b/c (§2).~~
   **Leaning settled (2026-06-22):** all cart-land → option (a) callback; snapshot-to-texture
   (b) ruled out (would need engine support). SLIDE is in, via a `draw_scene(which)` contract.
3. Which masks earn promotion (§3) — all seven or a curated subset.
3b. ~~Is the **luma-mask generalization** worth it? Prototype the cell-grid version first.~~
   **Answered (2026-06-22): yes.** The cell-grid LUMA transition is built and looks good with
   zero engine help (see the BUILT note in §"Generalizing the mask"); per-pixel/engine-blit
   is not needed. Optional follow-on: read the grayscale field from a *painted sprite* so
   shapes are authored, not coded.
4. Is "one cart that transitions through its own scenes, recorded continuously" (A) enough
   for the showcase reel, or do we also want **B** (build-time clip stitching) and/or **C**
   (a live cross-cart player)? A is the foundation regardless; B and C are independent,
   later, and optional — C especially is a separate project (see its stub above).

## References — where to find more transitions

- **GL Transitions** — `gl-transitions.com` — the canonical open gallery (~80+ named
  transitions, live previews + GLSL source: Bowtie, Doorway, Pixelize, WindowBlinds,
  PolkaDotsCurtain, DoomScreenTransition…). The best single resource.
- **RPG Maker transition graphics** — its transition system *is* the black-mask / luma trick
  (grayscale images thresholded by progress); large free libraries of mask shapes to mine for
  ideas.
- **SMPTE wipe codes** — Wikipedia "Wipe (transition)" + the SMPTE wipe chart — the
  broadcast-TV standard catalog of (numbered) wipe shapes; a formal taxonomy of nearly every
  wipe.
- **Luma mattes** — DaVinci Resolve "Luma Wipe" / Premiere "Gradient Wipe" — the video-editor
  name for image-thresholded transitions; free matte packs to browse.
- **Classics by name** — "Doom screen melt," "Sonic dissolve," Zelda/Mario iris; the PICO-8
  BBS has fantasy-console-scale "transition" carts.
- **Search terms** — sawtooth idea → *sawtooth / jagged / zigzag / serrated wipe*; interlace
  idea → *interlace / scanline / line wipe, venetian blind transition*; the generalization →
  *luma wipe / gradient wipe / luma matte / imagemask transition*.

## Related

- [`attract-mode.md`](attract-mode.md) — a cart that plays itself; transitions are how its
  demo loop bridges scenes. Layer A is a prerequisite.
- [`cart-clips.md`](cart-clips.md) — the clip baker; an in-cart transition is captured as
  one continuous clip. Layer B (stitching) would live near this.
- [`../decisions/0010-fade-is-immediate-mode.md`](../decisions/0010-fade-is-immediate-mode.md)
  — why `fade()` is immediate-mode; the multi-frame state for a transition lives in cart-land.
- [`../decisions/0006-library-carts-not-engine.md`](../decisions/0006-library-carts-not-engine.md)
  — why this would be a `transitions.h` header, not `studio.h` API.
- [`../decisions/0020-in-house-tool-curated-showcase.md`](../decisions/0020-in-house-tool-curated-showcase.md)
  — the showcase this ultimately feeds.
