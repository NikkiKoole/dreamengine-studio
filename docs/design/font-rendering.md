# Font rendering — playful text, beyond `print`

STATUS: EXPLORING — text-effect proposals; the two baked tiny fonts await print_small/print_tiny wiring.

> **Genre: design exploration (scratchpad).** Rationale + proposed signatures for
> not-yet-built text effects. It is **not** the status ledger and **not** the decision
> log:
> - "What's shipped / open / cut?" → [`../STATUS.md`](../STATUS.md).
> - When something here ships, fold it into the print/text section of
>   [`api-notes.md`](api-notes.md) (§17 / §19 already cover the shipped print helpers).
>
> Origin: a "are we using the font at its smallest, and can we make text more
> *playful*?" conversation. This note ferments those thoughts so we can pick a batch
> later.

---

## Where we are today

> **Note (2026-06-12):** the "one font" framing below is the *original* state. There are now
> **five** fonts via `font()` — see [The font family + the bitmap-font pipeline](#the-font-family--the-bitmap-font-pipeline-2026-06-12-session-12) below for the current roster and how to add more.

**One font, drawn two ways.** The in-game font is `dos_8x8` — a full DOS-OEM 8×8
bitmap sheet (16×16 grid), loaded via `LoadFontFromImage(..., YELLOW, 0)` with
`TEXTURE_FILTER_POINT`. It is **monospace**: `text_width = strlen × 8`.

- `print(text, x, y, color)` → `DrawTextEx(..., 8, 0, ...)` — native 8px, the
  *smallest* size.
- `print_scaled(text, x, y, color, scale)` → `DrawTextEx(..., 8.0f * scale, ...)` —
  **integer** multiples only.

Because everything renders into a native-res `RenderTexture` that the window then
scales, and the font texture uses a point filter, **all text is crisp blocky pixels**
and integer-scaled text stays perfectly sharp. There is no blur problem to fix.

**How the carts actually use it** (survey of the ~132 carts in `tools/carts/`):

| call | carts | typical use |
|---|---|---|
| `print` (8px) | 131 | *everything* functional — HUD, score, body, dialogue |
| `print_scaled` | 26 | titles / menus / combos only |

`print_scaled` scale args in the wild: **×2** (22 carts), **×3** (7), **×4** (6).

**Verdict on the original question:** we draw at the *smallest* size for everything
functional and pixel-double only for titles. That is essentially the PICO-8 model. The
font being full square monospace glyphs (vs PICO-8's skinny 3–4px ones) gives a
chunkier "terminal" feel — a deliberate aesthetic, not a bug.

## How PICO-8 does it (and where we differ)

1. **Native-res everything** — same as us; the window scale does the enlarging.
2. **Inline P8SCII control codes** — the big one. PICO-8's `print` parses the *string*:
   `\f7` switch foreground color, `\#8` background, `\^w`/`\^h` wide/tall, wavy text,
   cursor moves. One string carries color + scale + effects. **We have none of this** —
   color and scale are fixed per call.
3. **Two built-in fonts** — standard + a wide font (plus POKE-able custom glyph data).
4. **`print` returns the cursor x** — lets you chain segments. ✓ **Shipped 2026-06-01** — all `print*` functions now return `int` (x after the last char).

## Options, roughly by value / effort

### High value, cheap, no architecture change
All three build on one shared *per-character draw* helper (loop the glyphs, draw each
with `print` at a computed offset/color).

- **`print_shadow` / `print_outline`** — ✓ **Shipped 2026-06-01.** Drop-shadow at +1,+1; outline is 8-neighbour draws then the glyph on top. Both work with all three fonts via the `font()` state.
- **`print_wave`** — per-character vertical sine from `frame() + index`. Instantly juicy
  for titles and dialogue.
- **`print_type`** — typewriter reveal: draw the first N chars where N grows on a timer
  (`strncpy` into a `str()` buffer). The classic dialogue effect.

### Medium value
- **Per-character color** — once the per-glyph loop exists, rainbow text and vertical
  gradient fills fall out almost for free.
- **Bounce-in / squash titles** — "GET READY!" easing in with the existing `ease_*`
  helpers + a scale wobble.
- **`print_rot`** — `DrawTextPro` rotates; angled banners ("LEVEL 2!" tilted).
- **Letter spacing / tracking** — the `DrawTextEx` spacing arg is hardcoded `0`; expose
  it for loose retro headings.

### The big lever (more work, most payoff long-term)
- **Inline control codes, PICO-8 style** — `print("score \f9%d\f7 left", ...)` switches
  color mid-string. Subsumes wave/scale/color into string codes, but it's a parser plus
  the full four-place wiring (`studio.h` / `studio.c` / `studioDocs.js` / `shell.js`)
  *and* it complicates `text_width`. Worth it if we want *authoring* to feel magic;
  skippable if a handful of explicit `print_*` helpers is enough.

### Aesthetic forks (real asset work — only if a cart needs it)
- **A second font** — a thin **3×5 micro font** lets stats-heavy games (druglord, sims)
  pack far more text into 320px; or a **chunky bold** font for impact titles. New bitmap
  sheet + second `Font` handle. → **Decided 2026-06-01, see below** (two tiny fonts baked).
- **Keep integer-scale only** — resist fractional `print_scaled`; at native res with a
  point filter it shimmers. The right way to get an "in-between" size is a *different
  baked font*, not a fractional scale.

## Decision (2026-06-01): bake two full-ASCII "second fonts"

We're committing to the **second-font** fork above — and going *smaller*, not bolder
first. Two tiny fonts get baked as PNG atlases alongside `dos_8x8`, so stats-heavy carts
(sims, RPG menus, debug overlays) can pack far more text into 320px:

| sheet | source | license | glyph box (cell) |
|---|---|---|---|
| `editor/public/font3x5.png` | `my 3x5 tiny mono pixel font.ttf` (looks like alasseearfalas' "another tiny pixel font mono 3x5") | **confirm** — assume itch.io free/CC0 until checked | 3 inked + 1 advance × 6 (cell 4×6) |
| `editor/public/font4x6.png` | [filmote/Font4x6](https://github.com/filmote/Font4x6) (BSD-3) + ~31 hand-drawn punctuation glyphs in its style | **BSD-3** (keep notice) | 4 inked + 1 advance × 7 (cell 5×7) |

**On the 3×5 source vs filmote's 3×5:** filmote's [Font3x5](https://github.com/filmote/Font3x5)
(BSD-3) only covers `A–Z a–z 0–9 ! .` — every other ASCII slot is a gap. Because
`LoadFontFromImage` assigns codepoints **sequentially**, gaps scramble the mapping, so a
gappy font needs every slot filled anyway. The TTF already covers **all 95 printable
ASCII**, so the 3×5 needed no hand-drawing.

**On the 4×6 (chosen 2026-06-01, take 2):** went with filmote's chunky, genuinely-4-wide
4×6 (proper `#..#` letterforms, real descenders) over the earlier
[luizbills/font4x6](https://github.com/luizbills/font4x6) candidate — luizbills' is only
*3px-inked* wide, so it read almost identically to the 3×5 (not a distinct second font).
filmote's gaps were filled by **hand-drawing the ~31 missing punctuation glyphs**
(`" # $ % & ' ( ) * + , - / : ; < = > ? @ [ \ ] ^ _ \` { | } ~`) at 4×6 in its weight, so
the sheet is now full printable ASCII. Known-weak hand-drawn glyphs to revisit: **`&`**
(reads blobby), `$`/`@` (busy at this size).

**Bake format** (matches `dos_8x8` exactly — same loader, no new code):
- 16-column grid, codepoints **32–127** in order → 6 rows. `firstChar=32`.
- 1px **opaque-yellow** `(255,255,0,255)` separator lines + border = the key color.
- Glyph pixels **opaque white**; cell interior (incl. blanks like space) **transparent**.
  Transparent ≠ key, so even space is detected as a glyph and the codepoint run stays
  intact — that's the trick that lets a clean `firstChar=32` layout work.
- The bake script is Python+PIL (rasterises the TTF at size 6 / decodes luizbills' 3-byte
  nibble packing). One-shot; not yet a repo tool. **Verified** with a raylib probe:
  `LoadFontFromImage(img, YELLOW, 32)` → `glyphCount=96`, `baseSize=6`, `'A' '0' '!' '~'`
  all map to the correct codepoint.

**Shipped 2026-06-01 (session 11):** All three items below are done.
- ✓ **Wiring** — `font(FONT_SMALL/FONT_TINY)` state setter; each font has its own `Font` handle loaded at startup; all `print*` + `text_width` route through `cur_font()` / `cur_font_size()` / `MeasureTextEx`. `font(FONT_NORMAL)` resets. Demo cart: `fonts.cart.png`.
- **Punctuation polish** — still first-pass; `&`, `$`, `@` in the 4×6 font want another look. Not blocking.
- **3×5 license** — still unconfirmed; assume CC0/free until checked.

## Shipped first batch (2026-06-01)

`print_outline` / `print_shadow` ✓ shipped. `print_wave` + `print_type` remain open — still the recommended next step (title juice + dialogue). The inline-control-code system is the "real PICO-8" answer and is genuinely cool, but it's a bigger commitment — better as a deliberate follow-up once we've seen which effects carts reach for most.

## The font family + the bitmap-font pipeline (2026-06-12, session 12)

The lasting win is a **reproducible generator** (`tools/gen-rom-font.js`) so the next font is a
recipe, not archaeology.

> **Update (2026-06-12, later):** `FONT_LARGE` (MDA 9×14), `FONT_BOOT` (VGA 9×16), and
> `FONT_SMOOTH` (16×16 EPX) were **dropped** — they weren't the right fit. With them went their
> atlases/data headers, the `MDA9.F14`/`VGA9.F16` ROM dumps, and the EPX `smoothGlyphs` source.
> The family is now **five**, and the constants were renumbered contiguous.

| constant | cell | source | character |
|---|---|---|---|
| `FONT_NORMAL` | 8×8  | `dos_8x8` (the default)| the workhorse |
| `FONT_SMALL`  | 4×6  | filmote/Font4x6 bake  | compact HUD |
| `FONT_TINY`   | 3×5  | TTF bake (2026-06-01) | max density |
| `FONT_COMIC`  | 10×20| `ComicMono-Bold.ttf` rasterized @18px | friendly rounded handwriting; titles/dialogue |
| `FONT_THIN`   | 8×8  | IBM **CGA** "thin" ROM dump | narrow-stroke alternate; lighter than the default |

`tools/carts/rottext.c` is the playground: it cycles all fonts on **X** and renders each font's
name in that font (also the `print_rot` angle harness it was originally built for).

### The atlas format — the contract every font shares

All fonts load through the **one** `LoadFontFromImage(img, YELLOW, firstChar)` path in
`studio.c` (no per-font code), so each atlas PNG must be built identically:

- **16×16 grid** of glyph cells. `dos_8x8`, the CGA ROM font, and the comic TTF bake use
  **`firstChar = 0`** (cell index == codepoint); the tiny TTF-baked fonts use `32`.
- **1px opaque-yellow `(255,255,0,255)` separators + border** = the key colour. This is what
  delimits cells; even a blank cell (space) is detected because its interior is *not* key.
- **Glyph pixels opaque white; cell interiors transparent** `(0,0,0,0)`. Transparent ≠ key,
  so the cell is found; alpha 0 means the non-ink pixels simply don't draw, and the white ink
  is tinted by the `print` colour at draw time.
- Sheet size = `cells*cell + (cells+1)` per axis (the +1s are the separator lattice). A
  9×14 font → `16*9+17 = 161` wide, `16*14+17 = 241` tall.

### Two ways to fill an atlas (both in `gen-rom-font.js`)

1. **IBM ROM dump** (`FONT_THIN`). Raw `*.F08`/`*.F14`/`*.F16` files from VileR's int10h
   collection live in `tools/fonts/`. Each is 256 glyphs × H bytes, one byte per 8-pixel
   scanline (MSB = leftmost). `romGlyphs(srcFile, rows, width)` takes a **cell width**: `8` for
   a true CGA cell (edge-to-edge, what `FONT_THIN` uses), or `9` for the MDA/VGA style where the
   9th column copies the 8th over the box-drawing range **`0xC0–0xDF`** so rules connect.
2. **TTF rasterization** (`FONT_COMIC`). `ttfGlyphs(fontFile, px, gw, gh, threshold, patches)`
   reuses `font-bake.js`'s outline→coverage path (`contoursFromPath` + `rasterize`, both now
   exported) to bake a real TrueType font into fixed cells. Monospace glyphs are centred in the
   cell by advance width; the baseline is placed so descenders clear the bottom edge; printable
   ASCII 32–127 fill the atlas (firstChar 0, so cell index == codepoint). **Pick `px` from the
   font's own metrics, not by eye** — read `unitsPerEm`/cap-height/advance/descender and find the
   `px` where caps and advance land on whole pixels (Comic Mono Bold: cap 0.576em, advance 0.545em,
   box ~1.13em → px 18 gives cap≈8, advance≈8, box≈20, hence the 10×20 cell). The optional
   `patches` table (`COMIC_PATCHES`) applies per-glyph pixel set/clear *after* rasterization, so
   hand cleanups survive every re-bake — keyed by character, `[x, y, value]`, cell-local coords.
   **Only works on monospace TTFs** — a proportional font baked into fixed cells loses its spacing
   (the engine's `print` is monospace: `text_width = strlen × cell`).

### Wiring a new font in (the mechanical checklist)

`gen-rom-font.js` emits two files per font: `editor/public/font<NAME>.png` (human-viewable)
and `runtime/font<NAME>_data.h` (embedded byte array). Then, to expose it:

- **`runtime/studio.c` — five spots:** `#include` the header; add a `static Font font_x`;
  load it in `init` (`LoadImageFromMemory(".png", …_DATA, …_DATA_LEN)` →
  `LoadFontFromImage(img, YELLOW, 0)` → `SetTextureFilter(…, POINT)`); `UnloadFont` it on
  shutdown; add an `active_font_id == FONT_X` case to `cur_font()`; and add `FONT_X` to the
  `font()` validator's allowed set.
- **The usual four places** for the `FONT_X` constant: `studio.h` (`#define` + one-liner),
  `studioDocs.js`, `shell.js` (the graphics section's `keys`). (`studio.c` is covered above.)
  Re-run `tools/gen-tcc-symbols.js` after touching `studio.h`.

### Sources & licensing

ROM dumps: VileR's [int10h vga-text-mode-fonts](https://github.com/viler-int10h/vga-text-mode-fonts)
(`FONTS/PC-IBM/CGA-TH.F08` for `FONT_THIN`). The underlying IBM ROM bitmaps are public-domain;
we keep the raw `.F08` in `tools/fonts/` as the source-of-truth so the generator is reproducible.
`FONT_COMIC` is baked from `ComicMono-Bold.ttf` (MIT).

## When this settles

When the first batch ships: add each function in the four places (per CLAUDE.md), fold
the signatures into [`api-notes.md`](api-notes.md) §17/§19, flip the relevant line in
[`../STATUS.md`](../STATUS.md), and ship a tutorial cart with a baked screenshot. If we
commit to inline control codes, that choice deserves its own ADR in
[`../decisions/`](../decisions/README.md).

A downstream consumer of this pipeline: [`music-notation-glyphs.md`](music-notation-glyphs.md)
wants notation symbols baked as bitmap glyphs so a chord chart reads as music.
