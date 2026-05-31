# Font rendering — playful text, beyond `print`

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
4. **`print` returns the cursor x** — lets you chain segments. Ours returns `void`.

## Options, roughly by value / effort

### High value, cheap, no architecture change
All three build on one shared *per-character draw* helper (loop the glyphs, draw each
with `print` at a computed offset/color).

- **`print_shadow` / `print_outline`** — drop-shadow is one extra dark draw at +1,+1;
  full outline is 8 neighbour draws in a dark color then the real glyph on top.
  Biggest **legibility** win over busy backgrounds (racers/shooters need this more than
  bigger text). Probably the single highest-value addition.
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
  sheet + second `Font` handle.
- **Keep integer-scale only** — resist fractional `print_scaled`; at native res with a
  point filter it shimmers. The right way to get an "in-between" size is a *different
  baked font*, not a fractional scale.

## Recommended first batch

`print_outline` / `print_shadow` + `print_wave` + `print_type`, all on one shared
per-character draw helper. Cheap, no rendering-architecture change, and they cover the
three things carts visibly lack: **legibility**, **title juice**, and **dialogue**. Each
makes a clean little tutorial cart. The inline-control-code system is the "real PICO-8"
answer and is genuinely cool, but it's a bigger commitment — better as a deliberate
follow-up once we've seen which effects carts reach for most.

## When this settles

When the first batch ships: add each function in the four places (per CLAUDE.md), fold
the signatures into [`api-notes.md`](api-notes.md) §17/§19, flip the relevant line in
[`../STATUS.md`](../STATUS.md), and ship a tutorial cart with a baked screenshot. If we
commit to inline control codes, that choice deserves its own ADR in
[`../decisions/`](../decisions/README.md).
