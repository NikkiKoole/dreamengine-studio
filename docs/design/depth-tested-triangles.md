# Depth-tested triangles + a near-plane clip

STATUS: READY — designed 2026-09-25, not built. The decision is [ADR-0036](../decisions/0036-depth-tested-triangles-not-a-3d-engine.md), which narrows the "no 3D engine" cut line to exactly this. Rollout and verification are below; open questions are marked **Q**.

## What this is, and what it is not

It adds two leaf primitives in the [ADR-0009](../decisions/0009-small-3d-leaf-helpers.md) shape:

- **A per-pixel depth test** on the triangles and sprites we already draw.
- **A near-plane clip** for camera-space polygons.

The cart still owns its whole pipeline: rotate (`rot3`), clip (new), project (`project3`), light,
choose colours, draw. The engine answers exactly one question per pixel: *is this nearer than what
is already there?*

It is **not** the "fantasy PS1" layer that was discussed first (meshes, built-in lighting, fog, a
triangle budget, a Metal backend). The evidence below is why that was cut down. The ADR says what
would bring each piece back.

## The evidence (audit, 2026-09-25)

**Method.** The audit covered every cart that draws triangles or projects points. That is the 20
that call `tritex`/`trifill`/`rot3`/`project3`, plus the 3D carts that a name search misses.

- **Detecting errors.** For each perspective cart, a hidden second rasterizer used the same edge and
  fill rules as `sw_tritex`. Per pixel, it recorded which face the painter's order drew last and
  which face is actually nearest. A pixel counted as an error when those differed by more than 1%
  of depth.
- **Proving the detector works.** With `infiniminer`'s sort switched off, the detector reports
  16.7% of pixels wrong. So a zero is a real zero, not a blind detector.
- **polyroom** already carries a cart-land depth buffer (key `X`). It was diffed sorted against
  depth-tested at the same camera angles, which is the method its own header recommends.
- **Timing.** `-O2`, `DE_NO_RAYLIB` software engine, headless, camera auto-swept, on the office
  Intel i5 Mac. The sort and `draw()` were each wrapped in a high-resolution timer.

| cart | ordering today | sort errors | authoring workarounds | sort cost (share of `draw()`) |
|---|---|---|---|---|
| `citydrive` | buildings by screen-y of centroid, walls by screen-y, roof first | **yes**: walls across their own roofs on concave/complex footprints; roof-last breaks tall towers instead, so no fixed order works | the todo admits "occasional wrong overlaps"; walls drawn flat rather than textured | 0.5% (Manhattan) – 1.8% (SF) |
| `dreamcad` | `zsort` within each object, no sort between objects | **yes**: two overlapping objects render 1–1.8% differently depending on list order | — | 0.1% |
| `polyroom` | `zsort` of ~630 subdivided triangles | **yes** without subdivision (0.8–2%, at wall bases); 0.1–0.3% with it (edge pixels, rasterizer-rule disagreement) | subdivide to a tile (+53% triangles), parts must abut, furniture redesigned, sort per face | **19%** (25% including subdivision) |
| `tenement` | its own depth buffer (cart land) | solved; interpenetration had been the single root cause of three defects | deleted the sort, the subdivision and the abut rule; 3.02 → 2.28 ms | — |
| `flyover` | terrain front-to-back behind a column y-buffer, billboards `zsort`ed and drawn after | possible by construction: a prop behind a ridge draws over it (not observed) | — | small |
| `outrun`, `racer` | segment road; sprites culled at the first crest | a sprite past a crest is dropped whole, never partly hidden | the crest cull *is* the workaround | — |
| `podracer` | segments far→near (exact); pods drawn after the canyon | pods can never be hidden (≤220 px bounding box during an overtake, not noticeable) | short segments | 0% |
| `infiniminer` | insertion sort of whole cubes by centre distance | effectively none (45 px over 600 frames, 1-px slivers at the camera) | hidden-face cull | 2.3% (62 µs @ ~555) |
| `solid3d`, `textured3d` | `zsort` per face | none; the sort only mops up back faces that the non-perspective cull lets through | 4×4 subdivision (for texture warp) | ~0% |
| `cityview` | box footprints by screen-y, decks drawn after | none observed (convex boxes make roof-first correct) | — | 0.5–1.9% |
| `cube3d`, `26-first3d`, `doom`, `mode7`, `elite`, `marble`, `trafficjam` | wireframe, single quad, or already solved (`doom` has a per-column 1D depth buffer) | none | — | 0 |
| the 10 2D `tritex` carts (`boxjelly`, `boxhuman`, `puppet`, `jelly`, …) | 2D skinning of soft bodies and rigs | not a depth question | — | — |

**Three findings drove the decision:**

1. **Milliseconds are not the argument.** The sort costs ≤2.3% everywhere except `polyroom`'s
   insertion sort.
2. **Constraints are the argument.** Subdivision, must-abut and redesigned models all exist only to
   keep a painter's sort sound.
3. **The near plane is the other half.** `citydrive` (camera inside a footprint → walls become
   screen-filling wedges), `infiniminer` (faces with a corner at z<0.05 dropped → holes) and
   `26-first3d` (the whole frame skipped when a corner goes behind the camera) all fail there. A
   depth buffer does not help. A clip does.

**One measurement contradicts a cart note.** `polyroom`'s header says its depth path is *faster*
than sorting (0.86 vs 0.93 ms). The audit's build measured it *slower* (0.38 vs 0.34 ms). The
builds differed: the note's figures most likely came from the editor profiler on the desktop GPU
build. `polyroom` draws depth-tested pixels as one-row `rectfill` runs, which an in-engine test
would not do, so neither figure predicts the engine's cost. Measure it (rollout step 2).

## API sketch

The names were checked against every cart and shelf header on 2026-09-25: no collisions.
Signatures are proposals. **Re-read `studio.h` before implementing** (the four-place rule).

```c
// depth-tested twins of trifill / tritex / sspr. (x,y) are screen pixels, as now.
// z = the view DISTANCE of that corner: the number you divided by when projecting
//     (for project3 that is focal + p.z). Bigger = farther. Must be > 0.
void trifill_z(int x1,int y1,float z1, int x2,int y2,float z2, int x3,int y3,float z3, int color);
void tritex_z (int x1,int y1,float z1,float u1,float v1,
               int x2,int y2,float z2,float u2,float v2,
               int x3,int y3,float z3,float u3,float v3);
void sspr_z(int sx,int sy,int sw,int sh, int dx,int dy,int dw,int dh, float z); // a billboard at ONE depth
void depth_clear(void);   // forget all depth: next _z draw sees an empty scene (auto at frame start)

// clip a camera-space polygon to z >= zmin (in project3's space). v/uv in, out/uvout out
// (uv may be NULL). n <= 8 in; returns the new vertex count, 0 if fully behind, at most n+1.
int  nearclip(const V3 *v, const float *uv, int n, float zmin, V3 *out, float *uvout);
```

### Semantics, each one a decision

- **Allocation.** The buffer is allocated on the first `_z` call (or `depth_clear`). It is sized
  `de_sw × de_sh` and reallocated on `de_resize`. A cart that never calls a `_z` function allocates
  nothing and runs no new per-pixel code: 2D is byte-identical by construction, and the gate
  checks it anyway.
- **Clearing.** Once allocated, the buffer clears automatically at frame start, because forgetting
  that would make everything fail after frame one. `depth_clear()` is for a second 3D pass in one
  frame, such as a 3D HUD model over the world. (Sensible default, [ADR-0028](../decisions/0028-sensible-defaults-optional-tweaks.md).)
- **What gets interpolated.** 1/z, linearly in screen space. That is exact under perspective. Plain
  z is not, and gets intersection lines visibly wrong. Textures stay **affine**, as `tritex` is
  today (the PS1 look); only depth is corrected.
- **Q1: orthographic carts** (`polyroom`, `tenement`, `isoroom`-style). Under a parallel projection
  depth is linear in screen space, so interpolating 1/z is slightly wrong between vertices, although
  it is monotonic and exact at the corners. Options:
  - (a) Accept it. The error only moves intersection lines by sub-pixel amounts on small triangles.
    Measure on `polyroom`.
  - (b) `depth_linear(bool)`, one sticky switch.
  - Recommendation: (a) first, measured. Add (b) only if the diff against `polyroom`'s exact
    cart-land buffer shows it.
- **The test.** A pixel is written only if its depth is **strictly nearer** than the stored one,
  and then both colour and depth are written. On an exact tie the earlier draw wins, which is
  deterministic.
- **Coverage.** Each `_z` twin must use **the same coverage rule as its non-`_z` original**:
  `trifill_z` uses `poly_fill_cov`'s rule, and `tritex_z` uses `sw_tritex`'s edge functions and
  top-left rule. Swapping `trifill` for `trifill_z` must change occlusion and nothing else. This is
  gated (see Verification).
- **`sspr_z`** tests every pixel of the destination rectangle against one depth. That gives sprites
  inside terrain (the `flyover`/`podracer`/`outrun` case). Transparent (keyed/alpha) pixels write
  neither colour nor depth.
- **Interaction with the 2D state.** The rule is: the texel or colour goes through the same state
  as today, and **then** the depth test.
  - `pal()` recolours the source, as for every primitive.
  - `fillp` holes write nothing, so they also write no depth.
  - `camera()` and `clip()` apply as they do to `trifill`.
  - **`blend()`**: a blended `_z` draw tests depth but does **not write** it. Translucent geometry
    is drawn after the opaque geometry, sorted by the cart (the standard rule, written down).
  - **Q2**: should `fillp` holes write depth, for "screen-door" transparency? Default: no.
- **Plain 2D after 3D** ignores depth entirely. HUD over scene works with no extra call.
- **Q3: rotated camera.** On the software canvas, `camera_ex` with an angle draws the world into an
  offscreen layer and then rotate-composites it. The depth buffer must be in that layer's
  coordinates, not screen coordinates. Confirm this while building rather than designing around it.

### `nearclip`

This is a Sutherland–Hodgman pass against one plane. UVs are interpolated in camera space (before
projection), so a clipped textured quad keeps its texture mapping. Pairing it with `project3` is
the cart's job:

```c
V3 cam[4] = { rot3(a,yaw,pitch), ... };
V3 cv[5]; float cuv[10];
int n = nearclip(cam, uv, 4, 0.1f - focal, cv, cuv);   // project3's z is relative to -focal
// project each cv[i]; fan-triangulate 0,i,i+1 into trifill_z / tritex_z
```

**Q4**: `project3`'s depth convention (`focal + p.z`) makes `zmin` read oddly (`0.1f - focal`).
Either document it, or give `nearclip` the same `focal` parameter and a plain `near` distance.
Recommendation: `nearclip(v, uv, n, focal, near, out, uvout)`, so the cart says "10 cm in front of
the lens" and never does the offset arithmetic.

## Which renderer draws it

Correctness has one definition: **the software rasterizer.**

- **Software canvas** (iOS, Android, `DE_NO_RAYLIB`, `DE_SOFTWARE_CANVAS=on`). The depth test is one
  extra compare-and-store in the existing inner loops.
- **Desktop GPU path** (Raylib, the default) and, as far as we can tell, **web** (the web build
  passes no `-DSW_CANVAS_DEFAULT=1`, despite the comment at `studio.c:281`; confirm before relying
  on it). A GL depth buffer is **rejected**: GL gives no exact coverage or tie guarantees, so it
  would be parity by tolerance. The options are:
  - (a) Route `_z` draws through the CPU-raster path onto the GPU canvas. This is correct but pays
    the per-pixel plot cost the software canvas was built to kill.
  - (b) **Sticky-switch a cart to the software canvas on its first `_z` call.** This is the reverse
    of `sw_force_gpu`. The cart chooses what it draws; the platform chooses how.
  - Recommendation: (b), behind a probe, because switching mid-frame needs care. The first frame can
    simply be drawn in software from the start if the switch is decided before `draw()`.
- **Separately worth deciding:** make the web build software-canvas by default, as the `studio.c`
  comment says was intended. Then web matches iOS for 2D too. That is a different decision; this
  design only needs (b).
- **No Metal or GPU 3D backend** (ADR-0036).

## Per-instance state

The buffer is engine state, so it lives in the per-instance context. Two AUv3 instances in one
process must not share it.

- Add it to `studio_ctx.h` (pointer + size + allocated flag).
- Free it on `de_instance_destroy`.
- Reallocate it on the deferred resize path.
- Classify it for `node tools/ctx-gen.js --verify`.
- `tools/instance-check` should gain a case where two instances draw different depth scenes.

## Rollout

1. **ADR + this doc.** Done 2026-09-25.
2. **`trifill_z` + `depth_clear` + the gates below**, on the software canvas only.
   - Port **`polyroom`**: swap its cart-land buffer for the engine one. Its sorted/depth toggle
     becomes an engine-vs-sort A/B, and it has an exact reference to diff against (which also
     answers **Q1**).
   - Measure the engine cost against the sort on the same scene.
3. **`tritex_z` + `nearclip`**.
   - Port **`citydrive`**: it has the roof/wall errors and the near-plane wedges, so it shows both
     fixes in one cart.
   - Check that `infiniminer`'s holes at the camera close with `nearclip`.
4. **`sspr_z`**. Give `flyover` props that hide behind ridges, and `podracer` pods that hide behind
   walls.
5. **Desktop/web routing (b)**, with `canvas-diff` confirming that no 2D cart moved.
6. **Only then** a new cart built on the primitives, and only then the question of whether lighting
   or fog repetition has appeared (the ADR-0036 promotion rule).

**Follow-up, independent of all this:** `zsort` is an insertion sort, which is 19% of `polyroom`'s
draw at n≈630. It also has to stay stable, because `tenement`'s strobing wall corners came from a
switch to the unstable `qsort`. A stable merge sort with the same signature fixes the cost for every
cart that keeps sorting. It is small, has no API change, and could land any time.

## Verification

Pixel precision here means **deterministic and correct**, and each property gets a gate that can
fail:

1. **2D untouched.** A cart that never calls `_z` must hash identically before and after
   (`refactor-guard.js` probes, `build-all.js`). The strongest version is a `drawall` frame diff.
2. **Equivalence with the originals.** Draw a scene twice, once with `trifill` and once with
   `trifill_z` using strictly decreasing depth per call, so every draw is nearer and always passes.
   The two must be **byte-identical**. The same holds for `tritex`/`tritex_z` and `sspr`/`sspr_z`.
   This proves the coverage rules did not fork.
3. **Cross-compile determinism.** A `det-probes/` probe (in the `run.sh` pattern) draws
   interpenetrating geometry and must give the same bytes on arm64, x86-64 and wasm.
4. **Known answers** (`--selfcheck`-style fixtures):
   - **Intersection line.** Two quads crossing at a computable line; every pixel on each side must
     come from the correct quad.
   - **Watertightness.** A closed mesh with a debug write counter: every covered pixel is written
     exactly once by the front surface. This catches cracks and double writes.
   - **Tie rule.** Two coplanar triangles; the earlier draw wins everywhere.
   - **Near plane.** A quad straddling `near`; the clipped output covers exactly the expected pixel
     set, with no wedge and no hole.
5. **Negative control.** Interpolating plain z instead of 1/z must **fail** fixture 4's intersection
   test. Without that, the fixture might be passing vacuously.
6. **Per-instance.** The `instance-check` case above; `ctx-gen --verify` clean.
7. **The audit as a regression.** The hidden-rasterizer error counter used for the evidence becomes
   a tool. Ported carts must report 0 error pixels where they used to report non-zero (`citydrive`,
   `dreamcad`, `polyroom` unsubdivided).

## Related

- [ADR-0009](../decisions/0009-small-3d-leaf-helpers.md): the leaf-helper precedent this extends.
- [ADR-0024](../decisions/0024-software-canvas-is-canonical-for-2d.md), plus its Update: software
  `tritex` is in budget on-device.
- [`engine-portability.md`](engine-portability.md): the renderer seam and the GPU-parity audit.
- [`determinism.md`](determinism.md): why `+ - * /` under `FP_CONTRACT OFF` is the safe basis.
- [`rasterization-consistency.md`](rasterization-consistency.md): the coverage rules the `_z` twins
  must not fork.
- [`blend-tables.md`](blend-tables.md): the table builder a future `shade()` would reuse.
- [`cpu-shaders.md`](cpu-shaders.md): `raymarch` shows per-pixel 3D without triangles, a different
  road that stays open.
- The cart-land precedent: `runtime/tenement/art.h` (the shipped depth buffer) and
  `tools/carts/polyroom.c` (the sort-vs-depth probe).
