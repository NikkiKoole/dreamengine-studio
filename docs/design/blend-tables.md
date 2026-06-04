# Blend tables — index-only translucency, glow, and shadow

> **Genre: design exploration.** The design behind [STATUS open item 18](../STATUS.md)
> ("blend tables — the substantive capability gap", from the 2026-06-03 Picotron
> comparison). A **cart-space prototype now exists** — `tools/carts/blendlab.c`
> (the `blend lab` tech-demo) — built with zero new engine API; its findings are
> recorded below. Engine work needs an ADR-weight decision first (same class as
> [decision 0007](../decisions/0007-pal-recolors-sprites.md)); nothing here is committed.

---

## The gap

dreamengine has no way to draw something that **mixes with what's already on
screen**. `pal()` remaps colors, `fillp` dithers holes into a fill — neither reads
the destination pixel. The corpus consequences:

- Every "blend" is faked with dither or banding ([`geometry-helpers.md`](geometry-helpers.md):
  *"every 'blend' in the corpus is faked with dither or banding"*).
- The juice guidance fakes tint-to-white with `fillp` overlays (CLAUDE.md).
- [`galerijflat.md`](galerijflat.md) explicitly parked an engine gap: *"lights/glow
  want some kind of blend / additive API for soft light falloff — none exists yet."*

## The mechanism

A blend table is a 32×32 array of palette indices:

```c
unsigned char blend[32][32];     // blend[src][dst] = result index
```

Drawing changes from "write `src`" to "read `dst`, write `blend[src][dst]`". The
mixing happens **once, in RGB, at table-build time** — mix the two palette entries,
snap to the nearest of the 32 — and at draw time it's a pure lookup. The framebuffer
never holds anything but the 32 indices.

That last property is the whole reason this fits where `rgb()`/`lerp_color` didn't:
the parked true-color idea ([`geometry-helpers.md`](geometry-helpers.md) "Smooth color
interpolation") splits the color model in two and breaks `pal`/`pget`/`colorkey`
assumptions. A blend table's **output is always a palette index** — `pget` keeps
working unchanged, the fixed-palette identity stays intact, and the slight banding of
"nearest of 32" *is the look*, not a defect.

## Prior art

This is the standard 8-bit-indexed-era technique:

- **Doom / Boom** — `COLORMAP` (light levels + invulnerability) and Boom's `TRANMAP`
  (a 256×256 50%-translucency table) are exactly this structure.
- **Allegro 4** — `create_trans_table()` / `color_map`: library-level blend tables
  for 256-color modes; every DOS-era translucent effect went through one.
- **Build engine** (Duke3D) — translucency via a transluc lookup table.
- **Picotron** — has color tables today; the Picotron API comparison is where
  STATUS item 18 came from.
- **PICO-8 / TIC-80** — notably do *not* have this; their communities fake it with
  `fillp` dither, which is exactly where dreamengine is now.

## The prototype — `blendlab.c` (shipped 2026-06-04)

A cart that builds three tables in `init()` (AVG, ADD, MUL — ~32k nearest-color
searches, instant) and blends per-pixel against a **scene function it owns**
(`night_at(x,y)` / `day_at(x,y)` return the palette index under any pixel; the same
function paints the background via run-length rows, so dst is exact by construction).

Three modes: a night street with additive streetlight glows (the galerijflat case),
a draggable glass pane + multiply cloud-shadows + a fog band over a sweep of all 32
colors, and a raw view of the 32×32 tables. `D` toggles the same shapes drawn as
today's `fillp` dither-fake for direct A/B; `P` switches dst-reads to `pget()`.

### Findings

1. **The look works.** Additive glows genuinely read as *light*: the rim brightens
   the building facade, the sky, and the street each differently, because the result
   depends on dst. That dst-awareness is precisely what the dither fake can't do —
   dither overlays the same pixels everywhere. The A/B toggle makes the difference
   obvious in one keypress.
2. **AVG translucency holds up across all 32 destinations.** The glass pane over the
   swatch band produces a distinct, plausible mix for every background color. Some
   pairs band (32 colors is few), but it reads as in-palette, not as a glitch.
   Bonus finding: AVG-fog over a light checkerboard *flattens* it toward uniform
   grey — genuinely fog-like behavior, for free.
3. **Three preset tables cover the demand list.** AVG = glass/water/shadow
   (AVG-with-black is a proper 50% drop shadow), ADD = glow/light/laser, MUL =
   fog/darken/tint. Nothing in the galerijflat/geometry-helpers wish list needs a
   fourth, though weighted-AVG (25/75) would be a cheap "opacity level".
4. **Table-build details that mattered:** nearest-match used a luma-ish weighted
   distance (`2·Δr² + 4·Δg² + 3·Δb²`); plain Euclidean picked perceptually-off
   neighbors for some warm mixes. Build cost is negligible — presets could be built
   at engine startup from `palette[]` with no shipped data.
5. **⚠ The dst-read must be the in-progress frame.** The `P` mode demonstrates why:
   `pget()` reads a *last-frame* snapshot, and last frame already contains the
   blended shapes themselves — so a stationary additive glow feeds back and blooms
   to saturation over ~a second, and a held glass pane converges to opaque. Any
   engine implementation must read what's been drawn *so far this frame*, which is
   exactly the capability the renderer doesn't have today (see crux below). A
   cart-space blend is only correct because the cart knows its own background.
6. **Perf (cart-space):** per-pixel C loops + `pset` (one batched `DrawPixel` each)
   handle several radius-30 glows + a full-width fog band at 60fps native without
   effort. Not a statement about the engine path — just that the lab is usable.

## Proposed API surface (sketch — not committed)

Follows the `pal`/`fillp` shape: sticky drawing-scope state with a reset, which
[decision 0010](../decisions/0010-fade-is-immediate-mode.md) explicitly blessed for
state that many calls read within a frame:

```c
void blend(int mode);     // all draws now mix with the screen: BLEND_AVG / BLEND_ADD / BLEND_MUL
void blend_reset(void);   // back to plain overwrite (the default)
```

- **Presets first, cart-defined later (maybe).** `BLEND_AVG`/`BLEND_ADD`/`BLEND_MUL`
  built at startup from `palette[]`. A `blend_table(const unsigned char t[32][32])`
  escape hatch is cheap to add later if a cart wants "fire over water = steam";
  don't ship it until asked for (decision 0006 instinct: small core, composition).
- **Composes with `fillp`:** pattern holes skip as today; pattern 1-bits blend.
  Dither × blend = intermediate opacity levels — strictly more expressive than
  either alone, no new concepts.
- **`pal()` order:** remap src first, then look up — `blend[pal(src)][dst]`.
- **`colorkey`/transparent pixels:** skipped entirely, as now (no blend).
- **`pget`:** unchanged semantics — the canvas still holds only palette colors.

## The implementation crux — reading dst on a GPU renderer

Everything draws through the GPU into a `RenderTexture` (`pset` *is* `DrawPixel`,
`runtime/studio.c:1740`); nothing reads the in-progress canvas. Options:

1. **Shader + canvas snapshot per blend scope** *(the lane decision 0007 opened).*
   The 32×32 table uploads as a tiny LUT texture; the fragment shader recovers the
   src index the way the `pal()` shader already does (exact-match against the 32),
   samples dst from a **copy of the canvas taken when `blend()` activates** (and on
   each `blend()` re-activation), and outputs the looked-up palette RGB.
   - Cost: one canvas copy per blend *scope*, not per call — cheap if carts blend in
     one batch after drawing the scene (the natural usage).
   - Caveat: blended shapes within one scope see the snapshot, not each other.
     That matches the prototype's semantics and is arguably the right call for
     additive glows (no double-bright where two glows overlap… or arguably wrong —
     needs a taste call. Re-snapshotting per primitive is the correct-but-slower dial.)
2. **CPU shadow index-buffer** — mirror every draw into an 8-bit index buffer.
   Would also make `pget` same-frame-exact, but every GPU primitive (sprites,
   `tritex`, `DrawTexturePro` paths) would need a CPU twin. Too invasive; no.
3. **GL framebuffer-fetch / texture barrier** — not portable to web GL ES. No.

Whatever is chosen must respect [decision 0011](../decisions/0011-two-fill-pattern-paths-must-stay-in-sync.md):
fill-pattern semantics live in **two** paths (`rectfill_pat` texture path,
`plot_pat` software path), and blend semantics would land in both, in lockstep.

## Open questions for the decision

- Sticky `blend(mode)` (as sketched) vs per-call variants — sticky matches
  `pal`/`fillp` and keeps the surface to 2 functions + 3 constants.
- Shapes-only first, or sprites too in v1? (Sprites need the shader path anyway;
  shapes-only would be a strange half-feature given glow-behind-sprite is a headline
  use. Lean: do both via the shader, like decision 0007 did.)
- Overlapping blended shapes within one scope: snapshot semantics (don't see each
  other) vs re-snapshot (do). Prototype says snapshot semantics look fine.
- Web build (GL ES) — the pal shader already ships an ES-100 flavor; the LUT +
  snapshot sampling needs the same treatment.
- Whether `BLEND_AVG` needs an opacity dial (25/50/75 as three tables) or whether
  `fillp`-composition covers it.

## Status

- **2026-06-04** — concept validated in cart-space: `blendlab.c` / `blend lab`
  tech-demo shipped, zero engine changes. Findings above. Next step: decide
  (ADR) whether to build the engine version, with option 1 (shader + scope
  snapshot) as the candidate design.
