# Pixel-perfect scaling at non-integer offsets ("sharp bilinear")

**Status:** SHIPPED as an opt-in engine setting. The exploration below is preserved for the
reasoning; the outcome differs from where it started (see "Update" next).

## Update — what we actually shipped

Built a demo (`tools/carts/pixelperfect.c` → the "Pixel-Perfect Scaling" cart) comparing
nearest / bilinear / two-stage / **sharp-bilinear** on scrolling 1px lines, each with a 7×
magnifier. Conclusions that revised the plan below:

- **Sharp-bilinear beats the two-stage trick** and is *simpler*: it's a SINGLE fragment-shader
  pass (no integer-prescale, no intermediate RenderTexture). It keeps each source texel a flat
  colour and confines the blend to a 1-output-pixel seam at texel edges — crisp like nearest,
  stable like bilinear. So the "possible polish (later)" at the bottom became the actual answer;
  the two-stage section is now historical.
- **Gamma-correct (linear-light) blending** of that seam is a worthwhile refinement for thin
  *bright* lines: a half-covered pixel keeps its true brightness instead of pulsing dark as it
  moves. It's the demo's `SHARP+g` and the best-looking option.
- **`fwidth` sizes the seam** to exactly one output pixel, so it's correct at any scale/rotation
  (no need to pass the scale factor as a uniform).

Wired into the engine as **settings → "scaling"** (`scaleFilter`, a machine-local `-D` flag like
`SCALE`): `0` crisp/nearest (default, unchanged) · `1` bilinear · `2` sharp · `3` sharp+gamma.
The shader (`SCALE_FS`) and present-blit hook live in `runtime/studio.c` next to `pal_shader`;
the flag flows through `editor/src/settings.js` → `editor/electron/main.cjs` (native/live/web).
It is **NOT a cart-facing API** — carts never see it. Default is off, so nothing changes unless
chosen. **Only matters at non-integer scale** (web fit-to-viewport, fullscreen, resizable); at
the editor's integer `SCALE` all four look identically crisp. Desktop only for modes 2/3 (GLSL
330 `fwidth`); web falls back to the chosen texture filter (mode 1 bilinear still works there) —
a GLSL-100 + derivatives port is the open follow-up.

**Follow-up gap, found 2026-07-01: the actual present blit is never non-integer on desktop**, so
modes 2/3 are currently idle there — see [`window-fill-scaling.md`](window-fill-scaling.md). The
original LÖVE sketch this doc is based on (quoted above) has a second, fractional scale step
(`lessPerfectScale`) that stretches the crisp intermediate to fill the *actual* window; that step
never got ported — `runtime/game_rect.h`'s `gr_place()` (built later, for [`touch-controls.md`](touch-controls.md)) always
picks an integer scale for the present blit, so there's no fractional remainder left for this
shader to smooth. Not a flaw in this doc's design — the two ideas just want the same leftover
space and were never reconciled.

## The problem

When the low-res canvas has to be scaled to the window by a **non-integer** factor, both
naive options look bad:

- **Pure nearest (POINT):** uneven pixel widths. At 2.5× some source pixels land on 2
  screen pixels, some on 3 → visible wobble / shimmer, especially when anything scrolls.
- **Pure bilinear (LINEAR):** uniform but blurry — the whole image is softened because
  bilinear is sampling straight from the tiny native texture.

dreamengine's **desktop blit is already fine**: `studio.c` presents the `canvas`
RenderTexture (POINT filter) at exactly `SCREEN_W * SCALE × SCREEN_H * SCALE`, and `SCALE`
is an integer compile-time `-D` flag → integer scale → pixel-perfect. So this is a
**non-problem on desktop at the configured size.**

It only bites where a fractional total ratio is unavoidable:
- **Web build** — fitting the canvas into an arbitrary browser viewport.
- **Mobile / fullscreen** — stretching to a screen that isn't a clean multiple of the canvas.
- **Resizable desktop window** — dragged off the exact `SCREEN_W*SCALE` size.

## The fix: two-stage scale

Split the zoom into an integer part (nearest) and a fractional remainder (bilinear), so
bilinear only ever touches an *already-large* intermediate, never the native low-res image.

1. `K = max(1, floor(min(winW/SCREEN_W, winH/SCREEN_H)))` — biggest integer zoom that fits.
2. Upscale `canvas` (SCREEN_W×SCREEN_H) by `K` with **nearest** into an intermediate
   `canvas_hi` (SCREEN_W*K × SCREEN_H*K). Crisp, no uneven pixels.
3. Scale `canvas_hi` the rest of the way to the window with **bilinear**, centered /
   letterboxed. The only softening happens on the sub-pixel remainder.

This is the classic "sharp bilinear" / integer-prescale trick.

### Reference implementation (LÖVE)

The whole idea fits in one LÖVE sketch — `~/Projects/vector-sketch/experiments/pixelperfect/main.lua`:

```lua
function makeBestCanvas()
    local w,h = love.graphics.getDimensions()
    local imgW, imgH = img:getDimensions()
    local perfectScale = math.max(1, math.floor(math.min(w/imgW, h/imgH)))  -- step 1

    local canvas = love.graphics.newCanvas(imgW*perfectScale, imgH*perfectScale)
    canvas:setFilter("linear", "linear")        -- step 3 filter (set on the intermediate)
    love.graphics.setCanvas(canvas)
    love.graphics.draw(img, 0,0, 0, perfectScale, perfectScale)  -- step 2: nearest prescale
    love.graphics.setCanvas()
    return canvas
end

function love.draw()
    local w,h   = love.graphics.getDimensions()
    local cw,ch = canvas:getDimensions()
    local s = math.min(w/cw, h/ch)              -- step 3: fractional remainder, bilinear
    love.graphics.draw(canvas, 0,0, 0, s, s)
end
```

(`img:setFilter("nearest")` is set on the source so step 2 stays crisp; the intermediate
canvas is `"linear"` so step 3 is the soft one. Rebuild the intermediate only on resize.)

## Porting to dreamengine (raylib)

Mirrors the LÖVE code almost line-for-line. Filters map directly:
LÖVE `"nearest"`/`"linear"` → raylib `TEXTURE_FILTER_POINT` / `TEXTURE_FILTER_BILINEAR`.

- Keep `canvas` (SCREEN_W×SCREEN_H, POINT) as-is.
- Add `canvas_hi` = `LoadRenderTexture(SCREEN_W*K, SCREEN_H*K)`, `TEXTURE_FILTER_POINT`.
  Rebuild it only when `K` (the fit-scale) changes — the analogue of `love.resize`.
- Present in two `DrawTexturePro` calls instead of one:
  1. `canvas.texture → canvas_hi` at integer `K` (POINT).
  2. `SetTextureFilter(canvas_hi.texture, TEXTURE_FILTER_BILINEAR)`; blit
     `canvas_hi → window` at the fractional scale, centered/letterboxed.

The present site to change is the final blit in `studio.c` (search `DrawTexturePro(canvas.texture`
near the `BeginDrawing()` at the end of the frame — ~line 1266). Gate the two-stage path so
it only runs when the target scale is non-integer; integer scales keep the existing single
crisp blit. Remember the RenderTexture Y-flip (`-SCREEN_H` in the source rect) the current
code already handles.

**Cost:** one extra RenderTexture, two blits instead of one, intermediate rebuilt only on
resize. Trivial. Not a global rendering change — a targeted patch on the fractional paths.

## Possible polish (later)

Plain bilinear for stage 3 is the 90% version (zero shader). The full-polish version is a
**sharp-bilinear shader** — samples with a sub-pixel-narrow ramp so edges stay tight instead
of getting the standard bilinear smear. Only worth it if the slight softness is noticeable.

## A demo would settle it fast

A tiny cart drawing one image scaled three ways side-by-side (pure nearest / pure bilinear /
two-stage) at a deliberately fractional window is the quickest way to *see* the wobble vanish.
Not built yet — flagged here as the obvious next step before committing to engine changes.
