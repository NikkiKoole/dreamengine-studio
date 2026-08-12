# iso-rooms — can we do a rotating isometric room at 2D-only runtime cost?

STATUS: BUILDING (2026-08-12) — a **probe, not a product**. Bench cart `tools/carts/isoroom.c`
plus a build-time baker `tools/voxel-bake.js`. Gates the question of whether a Sims-style life sim
is buildable here at all; the game itself is deliberately not started until this returns a verdict.
Method is the builder/critic loop of
[ADR-0034](../decisions/0034-contract-first-parallel-authoring.md), run single-builder this time
for reasons given in §6.

> **Homage:** The Sims (Maxis, 2000) for the room grammar, RollerCoaster Tycoon 2 (Chris Sawyer,
> 2002) for the baked-sprite pipeline, Theme Hospital (Bullfrog, 1997) for the low-wall alternative
> in §3. Lineage runs through this repo's own `sims` (need-decay → urgency sort → seek object),
> `dwarffort` (designation → A\* → job FSM) and `lockup` (the regime as the spine). No furniture,
> art or catalogue content is copied from any of them.

---

## 1. The question

The Sims reads as The Sims because of the **rotatable isometric view with the near wall cut down**,
not because of the needs simulation. We already own three top-down needs sims: `sims`, `dwarffort`,
`lockup`. So a fourth top-down one is not a new cart, it is the same cart with a sofa. The view *is*
the product here, which is why the renderer gets probed before the game gets written.

Two facts make this non-obvious rather than an afternoon's work:

- **Hand-drawn iso costs one sprite per object per rotation.** At 8 rotations that is 8 drawings of
  every bed, toilet, sofa, fridge and counter. That is where a project like this dies.
- **The obvious fix is out of reach.** [ADR-0024](../decisions/0024-software-canvas-is-canonical-for-2d.md)
  measured `tritex`/3D at **~89ms/frame on an iPhone SE (~10fps, GPU-only)** against **59–60fps
  (~5.6ms) for 2D**. A real 3D engine (scene graph, z-buffer, per-pixel depth) is
  [explicitly out of scope](../STATUS.md). So "render the room with triangles every frame" is a
  desktop-only answer, and the launch sequence is iOS-first.

So the actual question, and the only thing this probe exists to settle:

> **Can we get one-model-many-rotations WITHOUT paying triangles at runtime?**

**The hypothesis.** Objects are authored **once** as small voxel/box models and **baked offline**
into sprite cells, one per rotation, so the runtime is `sspr()` plus a painter's-order sort and
never touches `tritex`. This is the repo's existing bake-at-build-time pattern
(`sprite-draw.js`, `font-bake.js`, `gen-rom-font.js`), pointed at a new problem.

**There is prior art for exactly this, and it is the strongest reason to believe it works.**
RollerCoaster Tycoon and The Sims 1 both shipped by pre-rendering 3D models to sprite sets. The
pipeline being proposed here is not novel, it is 1999's, which is the correct era to borrow from
given the constraints.

If the hypothesis holds, the life sim is buildable. If it does not, we say so with the number and
ship a top-down Sims instead, having spent one cart to find out.

## 2. Eight rotations, and why that is two renderers

The maker wants **8** rotations: the four cardinals and the four 45° diagonals. This is not 4
doubled. It is **two projection families over one data model**:

| | tile footprint | what you see of a box | prior art here |
|---|---|---|---|
| the four **45°** steps | 2:1 diamond | two faces + top | `qbert`, `marble` (fixed-angle) |
| the four **cardinal** steps | axis-aligned rect | one face + top | `cityview` (parallel-oblique) |

Different tile footprint, different wall-cut geometry, different screen→tile inverse, different
sprite dimensions. Practically: two renderers sharing one data model, not one renderer with an
angle parameter. **That is the real cost of 8, and it is a design cost, not an art cost.**

**Why 8 is nevertheless the right number.** These eight are the *complete* set of angles that land
on the pixel grid. The 2:1 diamond is integer (2 across, 1 down); axis-aligned is integer;
everything between them (22.5° and friends) aliases into mush. So 8 is not an arbitrary doubling of
4, it is *every angle pixel art can hold*. Stopping at 4 leaves four free ones on the table.

**And 8 is nearly free under the baked plan.** 8 hand-drawn sprites per object is dead on arrival;
8 baked from one model is a loop counter in the baker. **So wanting 8 rotations is an argument for
the voxel pipeline, not against it** — it raises the probe's value rather than its risk.

### The collapse that has to hold

Rotating the **view** by 45° and rotating the **character** by 45° are the same operation on a
baked ring. So 8 view rotations × 8 character facings is **one ring of 8, re-indexed**, not 64
bakes. Verify this early. If it holds, the combinatorial trap everyone fears in 8-rotation iso
never fires. If it does not, 8 rotations gets expensive fast, and that is a finding worth having on
day one rather than week three.

### The √2 pop, which may be the real reason nobody ships 8

One projection formula covers both families: rotate `(x,y)` by θ about the vertical axis, then
`sx = x'`, `sy = y'·k − z·h`. θ=45° gives the 2:1 diamond, θ=0° gives the cardinal view, and both
land on integer pixels. Good news for the **baker**, which can stay uniform.

But a unit tile's *apparent width* changes by **√2** between θ=0° and θ=45°. Geometrically truthful
rotation therefore makes the room appear to zoom in and out as you rotate through the eight steps.
The fixes are (a) accept the pop, (b) give each family its own voxel scale so they match, which
means the two families are no longer the same model at the same size, or (c) scale the whole view
per family. **This is a strong candidate for why every shipping game picks one family and stays
there**, and it is a cheap thing for the bench to demonstrate: rotate through all 8 and watch.

### The honest risk: nobody ships 8

The Sims is 4. RCT is 4. Project Zomboid is **1**, a fixed camera. The likely reason is that the
**cardinal views read badly**: cut the near wall at 0° and you are looking at the far wall flat-on,
which reads as an elevation drawing rather than a room, and objects lose their 3/4 silhouette.

That is a legibility risk, not a technical one, and it is cheap to settle. **Build for 8, and make
"do the cardinal views actually read?" an explicit go/no-go with 4 as the documented fallback.** Do
not quietly drop to 4; produce the frames and the finding.

## 3. What the reference actually taught us

`docs/design/reference/iso/` holds fair-use frames for critic comparison, **gitignored** so they
never reach the published `site/`. They were fetched at 365×273, which is within a whisker of our
320×200 canvas, so they are a like-for-like benchmark rather than 1080p frames a 32-colour canvas
would be unfairly graded against (the trap ADR-0034 names). Each is also snapped to our palette
via `pixelsnap.js --palette pico32`.

Three things came out of looking at them, none of which was guessable from prose:

**(a) The premise survives palette reduction.** Sims 1 at 320×240 in 32 colours still reads: the
sofa is a sofa, the desk and monitor read, the sim reads, the wall cutdown reads. 28 of 32 colours
used. This was the cheapest possible sanity check on the whole project and it passed.

**(b) There are three wall strategies, not one.** This is the useful surprise:

1. **Sims 1** — full-height walls, **cut the nearest one only**, so you always see the back wall of
   the room the sim is in. (Not the Sims 3/4 behaviour, where every wall but the exterior drops.)
2. **Theme Hospital** — walls are **low thin stubs that never occlude anything**, so there is no
   cutting problem to solve at all.
3. **RCT2** — no interiors, so it does not answer the question.

Strategy 2 sidesteps the single hardest part of the 8-rotation problem: wall cutdown in two
different projection families. And at 320×200 with a 32px character, full-height walls may simply
eat the room. **So low walls are a first-class candidate here, not a consolation prize** — test
both, and let the frames decide.

**(c) Sims 1's floor is genuinely noisy, which bears directly on lockup's worst fault.** The floor
is heavy speckle, and the pico32 reduction pushes its contrast up further. The difference from
lockup's rejected fleck is not *whether* there is noise but its **contrast**: lockup's was measured
at a +54 luminance step. So the instruction to the critic is to **measure floor-noise contrast
against the reference**, not to judge "noisy" or "not noisy" by eye.

**The one gap, stated honestly:** no reference shows the *same scene at two rotations*, so nothing
here can judge shading consistency across the 8 snaps. That turns out not to matter, because it is
an **oracle** question rather than a taste one (see §5). Parkitect is installed on this machine and
does rotate an iso view, so it is available as live reference if the loop ever needs it.

## 4. Geometry, fixed by the maker

- **Canvas 320×200.** The default. Not lockup's 720×450.
- **Characters ~32px tall.** Everything else follows from this.
- Tile size is therefore in the 24×12 to 32×16 range. A 10×10 room at 24×12 spans
  `(10+10)×12 = 240px` wide and `(10+10)×6 = 120px` plus wall height, which fits 320×200 with room
  for a HUD. Sims 1's own character is ~40px at this canvas width, so we are running slightly
  smaller than Sims 1 proportionally. Settle the exact number against the reference.
- **Single floor.** Multi-storey changes both the depth sort and the wall-cut rules substantially.
  Known deferral, not an oversight.
- **Renderer only, no simulation.** The character walks a path so occlusion can be tested, and has
  no needs. The sim half is already proven three times over on this shelf.

## 5. What the bench must show, and the oracle for each

ADR-0034's finding was that **a dimension with a deterministic oracle converges and a dimension
judged by prose does not**. That is the whole reason this probe is worth running as a loop: nearly
all of it is oracle-able, unlike "does the prison look AAA".

| # | Claim | Oracle |
|---|---|---|
| 1 | A room on a tile grid at all 8 snaps, near wall cut down, readable in **both** families | frames + critic (§3b decides which wall strategy) |
| 2 | Objects (bed, toilet, sofa, fridge, counter) baked from voxel models, depth-correct at all 8 | `spec()` on the depth-sort ordering invariant |
| 3 | One character walking the floor, correctly occluded, from all 8 | `spec()` + frames |
| 4 | The **ring collapse** of §2 holds (8 views × 8 facings = one ring of 8) | `spec()`, and check it FIRST |
| 5 | Screen→tile picking exact at all 8; the inverse differs per family so **both** need covering | `spec()` round-trip: screen→tile→screen for every tile at every rotation |
| 6 | Light does not rotate with the room | assert the lit face is the same *screen-space* direction at all 8 — this is why §3's reference gap does not bite |
| 7 | Rotation invariance of a symmetric room | `mirror-diff.js` |
| 8 | Renders identically on the software canvas (the whole point is device viability) | `canvas-diff.js`; if `blend()` shadows make zero impossible, **declare a budget in the cart**, do not hand-wave a nonzero diff |
| 9 | Frame budget, 2D only, **no `tritex` in the hot path** | live profiler (`build/.bake/profiler_request`); `ios/measure-device.sh` if a device is reachable, because a desktop-only number does not answer the question that motivated the probe |
| 10 | Atlas cost | report **total atlas pixels** for the object set |

**On the atlas, correcting an assumption worth writing down:** `spr()` is hard-locked to 16×16 over
the sprite editor's 64 slots, but **`sspr(sx,sy,sw,sh, dx,dy,dw,dh)` pulls an arbitrary sub-rect at
arbitrary size**, and `cols` is derived from the loaded sheet's real width, so the sheet can be
wider than 128px. 20 carts already draw atlas-style through `sspr`. **The budget is total atlas
pixels, not the editor's slot count.** Baked objects are not 16×16, so the cart draws with `sspr`.

## 6. How the loop runs, and why it is single-builder this time

**Do not fan out.** ADR-0034's honest ledger is that fan-out compresses *authoring*, not
*convergence*, and that every fix after integration was serial. This is one renderer whose problems
are convergence problems, so it is **one builder**.

Instead, builder-and-critic, **one fault at a time**:

- The builder builds. A **separate critic** bakes frames and judges, and **cannot edit files**.
- **Never let the builder grade itself.** The failure this prevents already happened here: a
  visual-fix agent reported in good faith that it had fixed lockup's noisy ground, and two critics
  independently measured the result and returned *"changed, not fixed."* The maker's eye agreed.
- **The critic judges the legibility grammar against the reference, and only that:** wall-cut
  height and the stub left behind; light direction and its consistency across snaps; floor-vs-wall
  value separation; silhouette readability at *our* sprite size; floor-noise contrast per §3c.
  Report each as a **measured number wherever one exists**, never as an adjective.
- **The critic is forbidden to grade whether our furniture resembles theirs, or to ask for their
  art.** We are not copying the catalogue; these objects are placeholders for a shape test.
  *"The sofa does not look like the reference sofa"* is out of scope and gets dropped. *"The sofa's
  silhouette is unreadable at 16px because it has no value break against the floor"* is in scope.
  This needs saying because a harsh critic handed reference screenshots will start demanding the
  reference's content unless told not to.
- **Loop on one fault until the critic passes it, then take the next.** No broad sweeps. Do not stop
  after N rounds: stop when the critic stops finding faults, or when a fault is shown to be
  unfixable, which is also a result. Stopping at N is
  [the exact thing the method warns against](../decisions/0034-contract-first-parallel-authoring.md), and it is
  the divergence lockup deliberately made for cost reasons.

This probe doubles as the **cheap experiment ADR-0034 asked for**: it is the first run here with a
real concrete benchmark in place (§3), on a mostly oracle-able target, so it is the honest test of
whether the critic loop *converges* on this codebase or merely churns.

## 7. Findings so far (2026-08-12)

**The baker works.** `tools/voxel-bake.js` (32/32 `--check`) takes ASCII voxel layers and emits
all 8 rotations. Models live in `tools/voxel-models/room.js`; 8 objects authored so far. Scale
settled: **8 voxels = one tile, `--tw 4 --zh 2`**, giving a 32×16px tile and a 16-voxel figure
standing exactly the **32px** the maker fixed.

**Two real bugs, both caught by the tool's own self-test rather than by eye.** Worth recording
because they are the argument for writing the known-answer fixture first:

1. *Edge-on faces read as front-facing.* Testing visibility by projecting the face normal called
   the ±x faces of a cube "screen-right facing" in a **cardinal** view, where they actually project
   to a zero-width line. Replaced with a test against the projector's own depth function: a face
   faces the camera exactly when stepping along its normal gets you nearer. That culls edge-on for
   free.
2. *Lavender walls.* Deriving side tones by nearest-RGB darkened neutral grey (#c2c3c7) into
   pico32's **indigo** (#83769c), because indigo genuinely is closest in raw distance. Every grey
   wall and white fridge baked out lavender. Fixed by matching on **luminance** with a 6× hue and
   chroma penalty and a strict darkening constraint. pico32 does hold a neutral ramp
   (7 → 6 → 5 → 21 → 16 → 0); the matcher simply was not looking for it.

**The √2 pop is avoidable, correcting §2.** Setting the cardinal tile width equal to the diagonal
one (`cw = tw`, now the default) makes a tile cover the **same screen footprint in both families**,
so rotating between them does not appear to zoom. It is geometrically "wrong" — a square turned 45°
really is √2 wider across its diagonal — but iso games are not geometrically truthful anyway, and
`--true-scale` keeps the honest ratio for anyone who wants to see the pop. Both directions are
pinned by assertions. **This materially de-risks the 8-rotation plan.**

**THE BLOCKER, and it is a hard one: the atlas does not fit a cart.**

| | pixels |
|---|---|
| 8 objects × 8 rotations, cells only | **67,780** |
| packed atlas (256×294, 90% efficient) | **75,264** |
| what a cart can currently ship (128×128) | **16,384** |

And it does not fit at *any* reduced scope, so long as the character stays 32px tall:

- 4 rotations instead of 8 → 33,890px, still **2× over**.
- Just `sofa` + `person` + `wall_low`, at 8 rotations → 21,948px, **still over**.

The cause is `tools/make-cart.js`: `slotsToSheetPng` hard-codes a 128×128 sheet and `genSlots`
**drops any slot index ≥ 64**. The *runtime* has no such limit — `cols` is derived from the loaded
sheet's real width and `sspr()` addresses any sub-rect, which 20 carts already rely on. So this is a
**tool** cap, not an engine one.

**Proposed unblock:** let a `.cart.js` declare a sheet size, default unchanged at 128×128, guarded
by `build-all.js` across all carts. Consequence to accept: a wide-sheet cart is no longer editable
in the in-editor sprite canvas (which assumes an 8×8 grid of 16×16), so `isoroom` becomes
generator-only. That is already the standing rule for generator carts, so it costs nothing new here.

**Three faults already visible in the first atlas**, logged for the critic loop rather than fixed
now: `fridge` and `wall_full` are indistinguishable grey slabs (the fridge's door detail is a single
near-invisible voxel line); the `toilet` is mush at this voxel scale; and the **cardinal cells
already read as flat elevation slabs**, which is early support for §2's go/no-go.

### The cart renders. Verdict on the go/no-go: **4 rotations, not 8.**

`tools/carts/isoroom.c` is up, `spec()` is 43/43, and
`tools/clips/isoroom/01-eight-rotations.script` is the committed seed that walks all eight views
(one per dumped frame). Guards: `build-all` 576/576, `lint-carts` clean at 549 carts.

**The four DIAGONAL views work, and work well.** The room reads as a room at 320×200 in 32 colours:
the bed reads as a bed, the sofa as a sofa, the fridge as a white good, the figure reads at 32px.
Objects hold position relative to the room as it turns, the depth sort is correct at every step, and
the light stays put rather than swinging around — that last one holds *by construction*, because
shading is done in screen space, which is why it needed no reference frame to check.

**The four CARDINAL views do not read, exactly as §2 predicted.** With frames now attached: they
come out as a wide flat floor plan. The 2:1 vertical squash that gives the diamond its depth cue
reads, without the 45° turn, as looking almost straight down — so objects lose their 3/4 silhouette
and the bed and sofa flatten into coloured slabs. Worse, the perimeter walls go edge-on and survive
as **detached 1–2px grey bars** floating at the room's edge, which is a genuine artifact rather than
a taste call.

So: **ship 4 rotations.** The cardinal family is not cut for being ugly, it is cut because a
projection with no X/Y mixing has no depth cue left to carry the objects. If it is ever wanted back,
the fix is not cosmetic — it needs its own `zh` and a weaker y-squash, i.e. a third set of bake
parameters, which is a different probe.

**What this costs and what it buys.** Cutting to 4 halves the atlas to ~33,900px, which still
exceeds the old 128×128 cap, so the declared-sheet-size change stays necessary either way. And the
§2 √2 finding survives intact but stops mattering, since a single family never pops.

**The frame budget: ~0.68 ms/frame, about 4% of 60fps.** Measured as the SLOPE of two headless
det-turbo runs (600 frames in 6.146s, 2400 in 7.368s → 1.222s / 1800 frames), which cancels the
compile-and-load intercept. That is the whole render: 63 `quadfill` floor tiles, 39 `sspr` cells and
the HUD. Caveats stated plainly: this is a Mac on the GPU path, and det-turbo runs flat out rather
than at 60fps, so it is a CPU-cost figure and not a vsynced one.

**It is the number that matters anyway, because it settles the regime question.** ADR-0024 measured
2D at ~5.6ms and `tritex`/3D at ~89ms on an iPhone SE. At 0.68ms of pure 2D work this sits
comfortably in the fast lane, so the bake-at-build-time hypothesis does what it was supposed to do:
it buys a rotating iso view at 2D cost. The on-device figure is still unmeasured and should be taken
before anything is built on top (`ios/measure-device.sh`).

**Still open:** the on-device number, and whether LOW walls or FULL+cutaway reads better (both are
implemented and toggle on `W`; the low-wall Theme Hospital strategy looks the stronger candidate at
this canvas size, per §3b). Neither blocks the decision to build.

### A tool gap worth recording, because it cost real time

`play.js`, the editor's re-bake path and `make-cart` each re-derived "config → sheet PNG"
independently. The moment a cart exported `atlas` instead of `sprites`, `play.js` fell through to
`makeBlankSpritePng()` and staged an **all-black sheet** — so the first run drew every piece of
furniture as a black rectangle, with no error anywhere. Now consolidated into one
`mk.sheetBufFor(cfg)` that all three call. The general shape (three copies of a resolution rule, and
the new case silently hits the fallback) is worth watching for elsewhere.

**And a third, the worst of them, because it only appears in the editor.** The sprite editor's
canvas is a fixed **128×128** (`sprite-editor.js`: `mapwidth: 128, mapheight: 128`). Loading a cart
drops its sheet into that canvas, and pressing ▶ exports the canvas back over `build/sprites.png`.
So a cart with a bigger sheet is **silently cropped to its top-left 128×128 on every run** — the
`.cart.png` holds the correct 256×286 atlas, `play.js` renders it perfectly, and the editor draws
one surviving piece of furniture plus a floor full of shadows, with no error at any layer. The
divergence between "works under the harness" and "broken in the editor" is what makes it nasty.

Fixed in `prepareCart()` (so every path gets it: run, web, app, iOS, live): a cart that declares
`atlas` or `sheet` has its sheet **re-staged from the generator**, overriding the editor's export.
The generator is already the source of truth for such carts. Slot-grid carts are untouched and keep
the sprite editor as their editing surface. **A wide-sheet cart therefore cannot be pixel-edited in
the editor** — that was the accepted cost of a declared sheet size, but note it is a stronger
statement than it first sounded: without this fix such a cart could not even be *run* from there.

Second one, same session: the live-inspection trigger files are **first-poller-wins**, and a stray
cart process was already running, so the first `profiler_request` was answered by *another cart* and
returned a plausible-looking profile that was not this cart's at all (141,793 frames, `pset` at 80M,
draw calls this cart does not make). CLAUDE.md documents the `pid:` line for exactly this; the trap
is that the wrong answer looks completely valid.

### Settled by the maker: four rotations, and half the scale

Two calls after playing it, both now in the cart:

**Four rotations, diagonals only.** Confirms the §7 verdict. The cardinal family is gone rather than
disabled — `iso_project` lost its branch entirely, a rotation is now just a quarter turn of the
world, and the cart carries 4 cells per object instead of 8.

**Half the scale: a 16px figure, 16px tiles.** This one is NOT a parameter change, and the reason is
worth keeping:

> A crisp 2:1 diamond needs one voxel step to be a whole number of pixels both across and down.
> `sx` steps `tw/2` and `sy` steps `tw/4`, so **`tw` must be a multiple of 4** — a 4×2px voxel is
> the FLOOR. At `tw=2` the rows land on half-pixels and voxels start disappearing.

So the picture cannot be shrunk by shrinking the voxels; it shrinks by using **fewer** of them.
Every model was re-authored at **4 voxels per tile** instead of 8, and the figure went from 16
voxels tall to 8. `spec()` now pins the integer-grid property directly, so nobody can quietly set
`tw=6` later. At 16px per tile there is no room for modelled detail anyway, so each object reads by
silhouette alone.

**Two consequences, one of them significant.** The room grew from 9×7 to 18×13 tiles, because at
16px tiles the old flat used barely a third of the canvas and the whole point of a smaller scale is
seeing more room. And the atlas fell from 67,780px of cells to **8,224px, packing to 128×77 — inside
a standard cart sheet.** So the declared-sheet-size change is no longer *required* by this cart. It
stays, because it is correct and guarded, and because the editor-crop fix it exposed is a real bug
either way; but the cart no longer depends on it, and a wide-sheet cart's editor limitations no
longer apply here.

**New legibility question at this size:** a sofa and a bed became nearly indistinguishable, both
reading as a pale slab with a coloured band. At 32px they were clearly different objects.

### Then settled up again: 150%, a 24px figure

Half turned out to be a step too far, so the scale went back up by half: **6 voxels per tile, a
24px figure, 24px tiles.** Third re-authoring of the model set, and worth noting that the ladder of
available sizes is coarse *by construction* — with `tw` pinned at 4, the only knob is the voxel
count, so the set has now been through 8 per tile (32px), 4 (16px) and 6 (24px). **The scale is a
data decision, not a flag**, and each move is a re-authoring job.

Two things fell out of it:

**The sofa-vs-bed confusion is fixed, and not by adding detail.** The extra height was spent on
making them differ in OUTLINE: the sofa got a tall cushioned back and arms, the bed stayed low and
flat. At this size detail is invisible but silhouette is not, which is the general rule for the whole
object set.

**The atlas is back over a standard sheet** — 19,456px of cells, packing to 128×169 against the
16,384px a 128×128 sheet holds. So the declared-sheet-size change earns its keep after all, and so
does the editor-crop fix (169 > 128, so the sprite editor would silently truncate it). Worth
recording as a near-miss: one intermediate scale made that whole capability look unnecessary.

## 8. The verdict this doc must end with

Written back into this file and the cart's `de:meta`:

- Does the baked-voxel-sprite path hold?
- At what ms/frame, on what hardware?
- At what atlas cost per object?
- 8 rotations, or 4 with the cardinal views cut? Which wall strategy from §3b?
- What it cannot do.

Then bake the cart (`make-cart.js` then `--run`) so the maker can load it in the editor and rotate
the room by hand. A "no" with a number attached is a successful probe.

## See also

- [ADR-0034](../decisions/0034-contract-first-parallel-authoring.md) — the method, and the ledger of
  what did and did not work on lockup
- [ADR-0024](../decisions/0024-software-canvas-is-canonical-for-2d.md) — why `tritex` is off the
  table for the hot path
- [ADR-0009](../decisions/0009-small-3d-leaf-helpers.md) — `V3`/`rot3`/`project3`/`zsort`/`quadfill`,
  the leaf-helpers the **baker** can use freely (it runs at build time, where cost is irrelevant)
- [pseudo-3d-city.md](pseudo-3d-city.md) — `cityview`'s parallel-oblique projection, the cardinal
  family's prior art
- [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md) — the two-part bar; the
  legible-to-a-stranger half is precisely the half no oracle checks
