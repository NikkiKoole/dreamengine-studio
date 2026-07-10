# Baked rotation atlas — fast pre-rotated sprites & a primitive body kit

STATUS: EXPLORING (scratchpad, not built) — pre-rotated atlas + the primitive body kit.

> **Genre: design exploration (scratchpad).** Rationale + proposed signatures for a
> not-yet-built subsystem. It is **not** the status ledger and **not** the decision log:
> - "What's shipped / open / cut?" → [`../STATUS.md`](../STATUS.md).
> - When the foundational primitive is actually built, the "why" gets frozen as an
>   ADR in [`../decisions/`](../decisions/README.md) (see *When this settles*, bottom).
>
> Origin: the `bones` [skeleton-animator](../guides/cart-specs/skeleton-animator.md) cart (`tools/carts/bones.c`) draws an 18-bone
> stick figure by rasterizing lines every frame. Fine for one puppet; the question was
> how to scale to **crowds, richer per-part shapes, and low-end hardware**. This note is
> the design that came out of that conversation.

---

## The idea: two requests, one machine

Two wishes came up:

1. **General "rotate & bake a sprite, then blit it hella fast"** — exactly Klik & Play /
   The Games Factory's *Directions* feature: give an object N directions, the editor
   pre-generates the rotated images, and at runtime movement picks the nearest one. Useful
   for *anything* that spins and is drawn a lot: ships, asteroids, bullets, cars — and
   crowds of skeletons.
2. **A reusable kit of pre-rotated primitive shapes** (rect, trapezoid, triangle,
   half-circle) you snap together into *any* body, not just the stick figure.

They are **the same machine** with two different sources. Once you can "bake N rotations of
*something* into an atlas and blit the nearest bucket," that *something* is either an
authored sprite (wish 1) or a procedural shape (wish 2). Wish 2 is wish 1 + a built-in
shape library + a recolor trick.

## The load-bearing insight: bake proportions, rotate-only at runtime

We chose to store parts as an **endpoint pair** (a begin→end segment). The trap: if a part
can be **any length at draw time**, then fitting a baked sprite to the segment needs a
stretch *along the part's axis* — and if it's also rotated, that's a full rotate+non-uniform
scale every frame, i.e. exactly what `sspr_ex()` already does live. So **free length at
draw time means prebaking buys almost nothing.**

The resolution, which fits the tool perfectly:

> **Bake the proportions (length + width + shape) into the atlas. Leave only rotation free
> at runtime.**

- The **RIG** view (where you set lengths/widths) is *when you re-bake* — an occasional,
  invisible cost.
- **Playback/animation** only ever *rotates* parts → runtime picks the nearest pre-rotated
  cell and **translate-blits** it. No draw-time rotate, no scale, no fill. The cheapest
  possible path — what the low-end target wants.
- **Crowds** bake once per *figure type* and blit thousands of instances from the shared
  atlas.
- **Rich shapes are free**: a textured tapered limb costs the same single blit as a 1px
  line, because the cost moved to bake time.

This is the same spirit as [ADR 0002](../decisions/0002-typed-static-pools-over-entity-system.md)
(pay once, statically) applied to drawing.

## The centerline / pivot model

Every part — however thick — is a **centerline**: a begin→end segment running down the
middle of its thickness, with the silhouette draped **symmetrically** around it. This is
exactly what forward kinematics already produces (joint → joint).

```
   line:        begin •─────────────• end          (the centerline IS the line)

   rect:        begin •═════════════• end          endpoints sit mid-thickness;
              ┌─────────────────────┐               the body is uniform around them
              └─────────────────────┘

   trapezoid:   begin •════════════• end            mid-thickness at each end;
              ▛▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▄▄                     thickness tapers along it
              ▙▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▀▀

   half-circle: begin •═══» end (dome tip)           centerline = flat-edge-mid → apex
              ╱▔▔▔╲
```

Consequences, all good:

- **No pivot enum.** Attachment point = the **begin** endpoint, full stop. To also cover
  wish 1's *center*-rotating objects (a ship), make the pivot a scalar **`pivot ∈ 0..1`
  along the centerline**: `0` = begin (limbs, default), `0.5` = center (ships/asteroids),
  `1` = end.
- **Joints close themselves.** Because both endpoints sit at *mid-thickness* and adjacent
  parts overlap where centerlines meet, thick parts (especially rounded ones) butt together
  with **no gap at the joint** — clean elbows/knees for free, no seam-filling.
- **Consistent with bake-fixed-length.** A stamp is baked so its centerline length equals
  the bone length; the baked begin lands on the joint, the baked end lands exactly where the
  next bone begins. Change a length in RIG → re-bake that part. No drift.
- **Flip.** For asymmetric shapes (trapezoid, triangle, half-circle) the "begin = thick/root
  end" convention is right for an arm (fat at shoulder → thin at elbow) but backwards for a
  tail/horn, so a per-stamp **`flip`** flag swaps which end is the root.
- **Odd widths.** Because the silhouette is symmetric around a 1px centerline, thickness comes
  in **odd** steps (1, 3, 5, 7, 9) — `r` pixels each side plus the center pixel — so there's no
  half-pixel bias. (The realtime `bones` renderer already uses this odd-width convention.)

## The foundation: an offscreen-canvas primitive (the one real engine change)

Rather than a bespoke "baked rotation" API we'd regret, add the *general* capability it's
built on — **offscreen draw targets** — and let baking live in cart/helper code where it can
iterate. Proposed (PROPOSED — not yet in `studio.h`):

```c
int  make_canvas(int w, int h);   // allocate an offscreen draw target (a RenderTexture); returns id, -1 on fail
void target(int canvas);          // route subsequent draw calls into canvas; target(-1) = back to the screen
void blit(int canvas, int sx,int sy,int sw,int sh, int dx,int dy,int dw,int dh);  // sample a sub-rect of a canvas
```

With just these three, the **entire prebake is cart/helper-level**: at `init()` (or after a
RIG edit) do `target(atlas)` → draw each part at each of N rotations into its cell with the
primitives/`spr_rot` we already have → `target(-1)`; then in `draw()` use `blit`. Offscreen
canvases are independently useful (minimaps, trails, post-effects, the sprite editor), so
the engine gains a general tool, not a narrow one.

> Naming caveat: `target`/`blit`/`canvas` become reserved cart identifiers (the usual
> shadowing rule — see CLAUDE.md). `set_target` is an alternative if `target` feels too
> grabby.

## Two helpers on top (PROPOSED, library-level — cf. [ADR 0006](../decisions/0006-library-carts-not-engine.md))

```c
// wish 1 — Klik & Play "Directions": bake N rotations of an existing sprite slot
int  rot_bake(int src_slot, int n_dirs, float pivot);     // → handle
void spr_dir(int handle, float angle, int x, int y);      // blit nearest direction at the pivot

// wish 2 — the primitive body kit: bake a procedural shape at N rotations
#define SHAPE_RECT       0
#define SHAPE_TRAPEZOID  1
#define SHAPE_TRIANGLE   2
#define SHAPE_HALFCIRCLE 3
int  shape_bake(int shape, int len, int w_root, int w_tip, int n_dirs, float pivot, bool flip);  // → handle
void draw_part(int handle, int x0, int y0, int x1, int y1);   // centerline begin→end: angle from the pair, blit at begin
```

- `draw_part` is the "store begin/end, pick the right sprite" call: `angle =
  angle_to(x0,y0,x1,y1)`, snap to nearest of N buckets, blit the cell anchored so the baked
  begin lands on `(x0,y0)`.
- `w_root`/`w_tip` express thickness at each centerline end: rect `w_root==w_tip`, triangle
  `w_tip==0`, trapezoid `w_root>w_tip`; half-circle uses `len`=radius, `w_root`=diameter.
- Both draws compose with **`pal()`** for per-instance recolor (next section).

## Recolor via `pal()` silhouettes — one atlas, infinite bodies

Bake shapes as **colorless silhouettes** (a fixed *fill* index + a fixed *outline* index),
then **recolor per instance at blit time with `pal()`**, which already recolors sprites in
this engine — that's [ADR 0007](../decisions/0007-pal-recolors-sprites.md). So the same baked
"capsule"/"trapezoid" becomes a pink arm on one critter and a blue arm on the next, with **no
extra atlas**. This is the difference between a toy and a real kit, and it's the same
"magic-color" trick the `the crowd` cart already uses — just applied to baked primitives.

Baking with two reserved indices (fill + outline) gives readable, outlined parts; the cart
`pal()`-swaps both per instance.

## Knobs & costs

| Knob | Effect | Note |
|---|---|---|
| **`N` angle buckets** | master quality/memory dial | `16` matches `bones`' existing 16-direction snapping (playback looks identical, tiny atlas); `32`–`64` for smooth crowd motion. |
| **Atlas memory** | grows with *distinct shapes* × N | the big question — see **[How big does the atlas get?](#how-big-does-the-atlas-actually-get)** below. |
| **L/R sharing** | halves part count (12 not 18) | already the model in `bones`. |
| **0–180° + flip** | halve the atlas for *symmetric* strokes | works for line/rect (mirror for the other half); **not** for asymmetric shapes (trapezoid/triangle/half-circle/a hand) — per-shape opt-in. |
| **Re-bake timing** | only on proportion change (RIG edits) | never during playback, so even a slow bake is invisible. |

## How big does the atlas actually get?

The honest worry: `parts × angles × cell²` balloons fast. Three reframes keep it bounded,
then the levers.

**It's a GPU texture, not the 64-slot sheet.** The atlas is an offscreen RenderTexture — it
does *not* consume sprite-editor slots. The real ceiling is VRAM and `GL_MAX_TEXTURE_SIZE`.
Desktop is generous; **web / low-end GL ES can cap at 2048²**, so "keep one atlas ≤ 2048²"
is the portability target.

**Bounded by distinct *shapes* × N — not by figures on screen, and not by colors.** Because
color is `pal()`-applied per instance ([ADR 0007](../decisions/0007-pal-recolors-sprites.md)),
**100 differently-colored soldiers share ONE atlas.** And you bake the **12 parts**, never
assembled figures, so the atlas stays small and composable. The only thing that grows it is
genuinely different *proportions/shapes*.

**Scope decision: cap a part at 16px** — the engine's native sprite-slot size. `bones` is
aimed at *smaller* characters, and 16px parts already stack to a ~64px-tall figure (upper +
lower torso = 32, upper + lower leg = 32) plus head and neck above — plenty. So cells are a
uniform **~16–18px square** (a 16-long part's rotation bbox fits there), the atlas is a simple
**grid** rather than a variable rect-pack, and baked parts are literally slot-sized.

**The numbers (with the 16px cap).** `12 parts × N × 16²` cells: RGBA at **N=16 ≈ 196 KB**,
N=64 ≈ 786 KB; the indexed 1-byte variant is **~49 KB at N=16**. Packed ≈ `222²` (N=16). So
even a *roster* of distinct body types fits comfortably in one ≤ 2048² texture — at this cap
the size worry essentially dissolves. (Current `bones` RIG clamps length to 40px; adopting the
baked path would lower that clamp to 16, which suits the smaller-character aim anyway.)

**Levers, ranked:**
1. **`pal()` recolor → colors are free.** The biggest one: variety is recolor, not atlas copies.
2. **Tight rect-pack + a `{u,v,w,h}` manifest** — *optional* once parts are capped at 16px
   (a uniform grid is fine then); only earns its keep if you allow mixed/larger part sizes.
3. **Indexed (1-byte) atlas instead of RGBA.** The `pal()` shader already maps by
   nearest-palette-index; bake the atlas as a single-channel **palette-index** image and the
   same shader consumes it → **4× less VRAM**. Strong, worth prototyping.
4. **`N` is linear** — default 16 (tool-identical); raise only where smooth rotation matters.
5. **L/R share** (12 not 18) — already the model.
6. **0–180° + horizontal flip** for *symmetric* strokes (line/rect) — halves those parts.
7. **Bake parts, compose figures** — never bake whole assembled bodies.
8. Offline atlases compress on disk (PNG `de:` chunk); **VRAM is the true budget**, not file size.

Bottom line: the tool case is ~half a meg. The worry only bites with a roster of many
*structurally different* bodies at high N — and even then, packing + indexed + per-shape
atlas-sharing keep it inside one ≤2048² texture.

## Runtime draw loop (sketch)

```
// once, after a RIG edit (Hybrid: runtime in-tool; offline bake for shipped carts):
target(atlas);
for each part p:  handle[p] = shape_bake(shape[p], len[p], wroot[p], wtip[p], N, pivot[p], flip[p]);
target(-1);

// every frame, per figure (cheap — translate-blit only):
FK → joints
for each bone b:
    pal(FILL, bodyFill[figure]); pal(OUTLINE, bodyOutline[figure]);   // per-instance recolor
    draw_part(handle[PART[b]], x0[b],y0[b], x1[b],y1[b]);             // nearest bucket, blit at begin
    pal_reset();
```

## Suggested build path (phased — when picked up, not now)

1. **Add `make_canvas` / `target` / `blit`** to `runtime/studio.{h,c}` (the four-places
   wiring per CLAUDE.md) **and write the ADR** — render targets are a real engine capability.
2. **Prove it in `bones`**: bake 12 parts × 16 buckets after each RIG edit; switch
   `draw_preview` from `thickline`/`circfill` to `blit`. Same visuals, validates the
   pipeline + the centerline/pivot math on one figure.
3. **Stress it**: spawn 50–100 baked skeletons → confirm the crowd win; dial `N` up for
   smoothness and watch memory.
4. **Promote** `rot_bake`/`shape_bake`/`draw_part`/`spr_dir` into a shared helper (a cart
   `.h`, or engine API if they earn it). Then decide **offline bake for shipping**:
   serialize the atlas PNG + manifest beside the cart, like the existing `de:sprites` chunk.

## Open questions

- **`N` default** — `16` (matches the tool, tiny) vs `32` (smoother crowds). Per-bake or global?
- ~~**Half-circle primitive**~~ — *resolved:* `arc`/`arcfill`/`ring` shipped, so
  `shape_bake` draws the half-circle directly with `arcfill(x, y, r, 0, 180, col)`.
- **Fill+outline bake** — fixed two-index scheme for `pal()` recolor, or single-color
  silhouettes only?
- **`target` naming** — `target` vs `set_target` (shadowing footprint).
- **Indexed vs RGBA atlas** — a 1-byte palette-index atlas is 4× cheaper in VRAM and rides
  the existing `pal()` shader, but couples the bake format to that shader; prototype it?
- **Stretch escape hatch** — keep `sspr_ex` as the documented "I need free length" fallback,
  or forbid it to keep the fast-path promise clean?

## When this settles

- Build the offscreen-canvas primitive → update **`STATUS.md`** and write a
  **`decisions/NNNN-offscreen-canvases.md`** ADR (foundational, deserves frozen rationale).
- The `rot_bake`/`shape_bake` helpers landing → note in **`STATUS.md`**; if they stay
  library-level rather than engine API, that itself is an [ADR 0006](../decisions/0006-library-carts-not-engine.md)-style call.
- Cross-link from **[`api-notes.md`](api-notes.md)** (graphics section) once signatures firm up.
