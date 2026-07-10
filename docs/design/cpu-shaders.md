# CPU shaders — notes & open ideas

STATUS: SHIPPED — the teaching trilogy (shadelab → caustics → raymarch) + shadermath.h; ideas #3 and #5 parked below.

dreamengine has no GPU shader hook. A "fragment shader" here is just a C function
that returns a `0xRRGGBB` colour, painted to the canvas with the true-colour
primitives `pset_rgb` / `rectfill_rgb` (24-bit, bypassing the 32-colour palette),
optionally reading the canvas back with `pget_rgb` (`enable_pget(true)` first). It
runs on the CPU, one evaluation per `ps×ps` cell, every frame. Slower than a GPU,
but every pixel is a few readable lines of C — the *lesson* is legible in a way a
GLSL blob never is. That legibility is the whole point of this corner of the engine.

## Shipped — the teaching trilogy (+ toolkit)

A deliberate arc, each cart one concept built on the last:

1. **`shadelab`** — a pixel is a function of `(u,v,t)`. Ten worked shaders (uv, hue
   sweep, distance field, plasma, value-noise, warped checker, interference, cursor
   light/ripple) ending in **feedback** (#10): reads the previous frame back with
   `pget_rgb`, so the screen becomes its own memory (rotate+zoom+decay tunnel).
2. **`caustics`** — **displacement mapping**: warp *where* you look up a colour and
   the picture ripples. Two modes via `TAB`: FAKED (scrolling sines over a procedural
   floor — the N64 water trick) and REAL (a height-field water sim in two ping-pong
   buffers — the wave equation, the previous frame feeding the next — whose surface
   slope steers a lookup into a captured source image; the classic Hugo-Elias 2D water).
3. **`raymarch`** — 3D from a signed distance field. The whole world is a six-line
   `scene(p)`; each pixel marches a ray, normals come from the field gradient. Shows
   off **`shadermath.h`**.

**`runtime/shadermath.h`** — the cart-land GLSL-vocabulary header underneath it all:
`vec2`/`vec3` + `dot`/`normalize`/`length`/`cross`/`mix`, the scalar idioms `studio.h`
lacks (`fract`/`smoothstep`/`mix`/`step`/`sat`), colour helpers (`rgb`/`hsv`/`vrgb` +
the Iñigo-Quílez cosine `palette()`), and SDF primitives/combinators (`sd_sphere`/
`box`/`torus`/`plane`, `op_union`/`op_sub`/`op_smin`). Include **after** `studio.h`.
Table row: [`guides/cart-authoring.md`](../guides/cart-authoring.md) → "Cart-land library headers".

## Open ideas (parked — not lost)

### #3 — Make the per-pixel cost VISIBLE (the performance lesson)

The `ps` (pixel-size) knob already lets you *feel* chunkiness, but nothing shows
*why* it matters. Add a live readout to the shader carts (shadelab / caustics /
raymarch): **evals-per-frame · work-ms · FPS**. Drop `ps` from 3→1 and watch ~9× the
pixels tank the framerate — a viscerally clear lesson in what "per-pixel cost" means
and why GPUs exist. Cheap to build: the profiler already exists
(`build/.bake/profiler_request` → JSON with `workMsAvg`/`workMsMax`/`frameMsAvg`; see
[`guides/debug-harness.md`](../guides/debug-harness.md) → "Live inspection"), so the
data is there — this is mostly a small HUD line (`evals = (SCREEN_W/ps)*(SCREEN_H/ps)`,
plus ms/FPS from the engine). Could live as a toggle (a `?`/`F` key) so it doesn't
clutter the default view. Lowest-effort, high pedagogical payoff.

### #5 — A true second offscreen buffer (ping-pong multipass)

`shadelab`'s feedback shader and `caustics`' REAL mode both fake "another buffer" by
reading the live canvas back (`pget_rgb`) or by keeping cart-side arrays. A *real*
second render target would let carts teach **multi-pass pipelines** properly: render
pass A into buffer B, then pass C samples B cleanly (no same-frame read-back hazard,
no HUD-smear workarounds like caustics' clamp-to-interior). Enables blur/bloom,
separable convolutions, true ping-pong simulations sampling a clean previous state,
and "render the scene, then post-process it" as a first-class lesson.

**Why it's lower priority:** the feedback shader already delivers ~80% of the
multi-pass *intuition* on the live canvas, and a real offscreen target is an **engine**
change (a `RenderTexture2D` the cart can draw into and sample — new `studio.h` API +
the four-place wiring), not cart-land like everything else here. Only worth it if we
specifically want to teach post-processing / multi-pass as a topic. Until then the
single-canvas read-back is the pragmatic stand-in.

> **See also [software-canvas.md](software-canvas.md)** — a proposed CPU framebuffer for
> the whole 64k-px canvas (to kill `rlVertex3f`, the fleet's #1 cost) would be a *superset*
> of this: with the canvas already in RAM, a second offscreen buffer and clean ping-pong
> multipass fall out for free, and `pset_rgb`/`pget_rgb` become plain array access.

## Related

- True-colour primitives: `pset_rgb`/`rectfill_rgb`/`pget_rgb`/`enable_pget`/`palette_hex`
  declared in [`runtime/studio.h`](../../runtime/studio.h); palette/colour rules in
  [`design/palette-and-color.md`](palette-and-color.md).
- `wowflutter` also leans on the true-colour primitives (chroma fringe / RGB grain over
  a `palette_hex` sepia grade) — not a shader, but the same toolbox.
