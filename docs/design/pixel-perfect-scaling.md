# Pixel-perfect scaling at non-integer offsets ("sharp bilinear")

**Status:** exploration / parked. Nothing wired into the engine yet. This note exists so
future-me can pick it up cold.

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
