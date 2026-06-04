# Palette & color — own the palette before blending bakes it in

> **Genre: design exploration.** Two questions that turn out to be one decision:
> (1) dreamengine's 32-color palette is **lifted verbatim from PICO-8** — fine
> for a learning project, borrowed clothes for anything released into the world;
> (2) blend tables ([STATUS #18](../STATUS.md), [`blend-tables.md`](blend-tables.md))
> are *computed from* the palette's RGB values, so building them against the
> borrowed palette bakes it one layer deeper. This doc records how Picotron
> handles both, and a three-layer plan where each layer ships independently.
> Nothing here is committed; #18's ADR should follow at least Layer 1's decision.

---

## 1. Where the borrowed palette lives today

The PICO-8 hexes are not one constant — they're replicated in four places:

| Place | What |
|---|---|
| `runtime/studio.c:432` | `palette[0..31]` RGB literals (+ `base_palette` pristine copy for `pal_reset()`) |
| `runtime/studio.h:34` | the `CLR_*` names + hex comments ("the PICO-8 palette", per the header comment) |
| `editor/public/palettes/pico32.json` | the sprite editor's swatches |
| `tools/make-cart.js` | `DEFAULT_CHAR_MAP` ASCII↔index conventions (R=red, b=bright blue…) |

Plus one *behavioral* dependency that's easy to miss: `gradient()`/`vgradient()`/
`hgradient()` and the whole dither-fake school assume **perceptually adjacent
ramps exist** in the palette (dark blue → blue → light grey reads as one ramp).
A replacement palette isn't just 32 pretty colors — it needs the same *ramp
structure* or every dithered gradient in the corpus degrades.

Also load-bearing: the "magic recolor indices" convention — `crowd`/`monstermix`
draw clothes/bodies in indices 28/29 specifically so `pal()` can re-dress them.
Any new palette keeps a designated recolor pair (or formalizes it).

## 2. What Picotron does (read 2026-06-04)

Sources: [gfx pipeline doc](https://www.lexaloffle.com/dl/docs/picotron_gfx_pipeline.html),
[manual](https://www.lexaloffle.com/dl/docs/picotron_manual.html),
[color-tables BBS tutorial](https://www.lexaloffle.com/bbs/?tid=149249).

- **The palette is not hardwired.** 480×270 / "**64 definable colours**".
  `pal(i, 0xRRGGBB, 2)` assigns RGB to any index. Four 64-entry RGB display
  palettes live at `0x5000`, selectable **per scanline** (2 bits/scanline at
  `0x5400`). The *default* palette is PICO-8's 16 + the extended 16 (the same
  32 we copied) — but it's explicitly a default, not the console's identity.
  **Picotron's brand is "bring your own palette."**
- **Blending is a color table in the rasterizer.** Four 64×64 tables at
  `0x8000`–`0xb000`; every pixel any draw op writes goes through
  `result = table[draw_col][target_col]`; the high bits of a color select which
  table. Negligible cost — it's in the pixel loop, not a post pass.
- **The big architectural idea: transparency, `pal()` remaps, and blending are
  ONE mechanism.** Color 0's column = "keep target" = transparency. A row that
  ignores the target = an opaque `pal()` swap. A row built by RGB-mixing = glass
  / fog / additive. The color table isn't a feature *next to* the palette ops —
  it **is** all of the palette ops.
- Their practical gotchas (from the BBS): bare `pal()` resets the table;
  authors build tables as 64×64 images and `memmap()` them in.

## 3. The coupling, stated plainly

A blend table entry is `nearest_palette_index(mix(rgb[src], rgb[dst]))` — the
table is a **pure function of the palette**. Consequences:

1. If the palette is fixed forever, tables can be baked constants (what
   `blendlab` does in cart-land today).
2. If the palette is ever customizable, the **engine must build tables from the
   active palette** — at init and on every palette change. That's not a burden;
   it makes the API *better*: carts say `blend(BLEND_AVG)` and never see a table.
3. Therefore the order of work matters: **decide the palette story first**, then
   write the #18 ADR in terms of "tables derived from the active palette."
   Building #18 against the PICO-8 hexes would bake the borrowed identity into
   a second, less-visible layer.

## 4. The three-layer plan (each independently shippable)

### Layer 1 — a default palette of our own *(identity fix; smallest)*

Replace the 32 hexes with a palette that is ours, keeping the *structure*:
16 "standard" + 16 "extended", same ramp coverage, a designated recolor pair.

Candidate routes (32-slot):
- **Adopt a community palette** (Lospec, free to use): e.g. **ENDESGA 32** — a
  beloved general-purpose 32 with strong ramps; or curate 32 out of
  **Resurrect 64**. Cheap, instantly credible, license-clean (verify the
  specific palette's terms before shipping).
- **Curate our own 32**, using ENDESGA/Resurrect as references — more work,
  fully ours. Could be seeded from a community palette and drifted.

### Layer 1b — the 64-color question (raised 2026-06-04)

A fact that changes the math: **sprite sheets store RGB pixels, not palette
indices** (the editor canvas / `sprites.png` are plain RGB; indices live only in
`.cart.js` arrays, `pal()` args, and `CLR_*` constants). So 32→64 is *not* a
sheet-format migration — old carts using 0–31 keep working as a **subset** if
the first 32 slots keep their roles. Real costs: `PALETTE_SIZE`
(`studio.c:109`), the editor's swatch UI, 32 new names, charmap conventions,
and *learnability* (PICO-8's 16 are memorizable; 64 is a different relationship
with color).

Why 64 is attractive *specifically because of blend tables*: table quality is
bounded by palette density — `nearest(mix(a,b))` can only land on colors that
exist. Dense, long ramps make blended output read like real alpha; sparse
palettes band. Two 64-routes considered:

- **Resurrect 64** (Kerrie Lake, Lospec) — hand-curated 64 with long unified
  ramps; effectively *designed* for smooth mixing, proven in many shipped
  games. Trade-offs: it's a strong aesthetic of its own (moody, saturated — a
  *style* choice, though "community palette" is borrowed in a much weaker sense
  than another console's identity palette), and its hue coverage decides our
  default look wholesale.
- **ENDESGA 32 + 32 derived in-betweens** (slots 32–63 generated as ramp
  midpoints / hue bridges) — keeps a memorizable 32 as the *authoring* palette
  (swatches, names, charmap untouched), with the upper 32 existing mainly as
  **blend headroom** (and finer dithered gradients). Caveats: naive RGB
  midpoints go muddy — mix in OKLab and hand-fix; and within-ramp midpoints
  alone don't help *cross*-ramp blends (red glow on blue floor needs hue
  bridges, which curation handles and generation doesn't). Realistic version:
  generate, then curate the worst offenders.

Neither is decided; the contact-sheet probe should render **both** against the
corpus worst cases (glow over water, fog over a sunset gradient, skin tones)
next to plain ENDESGA 32. Note the dependency: if 64 wins, Layer 3's table is
64×64 (4KB — still nothing) and `blendlab`'s math ports unchanged.

Migration cost (the honest part): every existing cart's *art* shifts hue —
sprites store indices, so nothing breaks structurally, but all ~215 thumbnails
need a re-bake and some carts' color choices will read differently (the closer
the new palette's ramp structure, the smaller the damage). `CLR_*` names stay
(they describe the *default* palette's slots; names like `CLR_DARK_BLUE` remain
true if slot 1 stays a dark blue). A **palette-diff contact sheet cart** (all 32
swatches + the corpus's worst-case scenes, old vs new side by side) is the probe
to build first.

### Layer 2 — settable palette per cart *(the Picotron lesson)*

- `palette_set(i, r, g, b)` (or packed `0xRRGGBB`) + `palette_reset()` —
  the mechanism nearly exists: `pal()`-on-sprites already drives a palette
  texture through a shader ([decision 0007](../decisions/0007-pal-recolors-sprites.md));
  settable entries extend the same path. Slot count (32 vs 64) is Layer 1b's
  call — sheets are RGB so either works; whatever ships, old indices 0–31
  keep their roles.
- A **`de:palette` zTXt chunk** so a cart carries its palette exactly like it
  carries screen/scale/map settings today (without it, a custom-palette cart
  renders wrong in a fresh editor — the same class of bug `de:settings` fixed).
- Sprite editor reads the active cart's palette instead of `pico32.json`.
- Skipped on purpose: per-scanline palettes (flavor, not need) and multiple
  display palettes.

### Layer 3 — blend tables from the active palette *(STATUS #18, the ADR)*

`blendlab`'s verdict stands: the look works, and dst must be read from the
**in-progress frame** (its `P` mode demonstrates the last-frame feedback bloom),
pointing at a shader + per-scope canvas snapshot — the decision-0007 lane.
What this doc adds to the ADR's inputs:

- Tables are **built by the engine from the active palette**, rebuilt on
  `palette_set` (a 32×32 build is ~1k nearest-color searches — nothing).
- API shape: `blend(BLEND_OFF/AVG/ADD/MUL)` + maybe `blend_custom(table)` for
  the blendlab-style power user. One active mode, not Picotron's 4-table
  high-bit scheme — we don't have spare bits in a 0–31 color, and one mode at a
  time covers the corpus's wants (glow, glass, fog, shadow).
- Long-view (not for the first ADR): Picotron shows transparency + `pal()` +
  blending can unify into one table. Ours stay separate mechanisms for now —
  but the ADR should note the unification as the eventual shape, so new color
  features don't multiply mechanisms further.

## 5. Order of work

1. **Palette contact-sheet probe cart** (Layer 1's evidence; tag `probe`) —
   render candidate palettes against worst-case corpus scenes.
   **✅ Shipped** — `tools/carts/palettelab.c`: keys 1–4 swap the live palette
   (shipped PICO-8 / ENDESGA 32 / **full Resurrect 64** / **E32 + 32 derived
   in-betweens**), LEFT/RIGHT walks five scenes: 64-swatch grid + corpus
   ramps / sunset / night-glow / portrait / **blend test** (glass + shadow +
   additive glow mixed per-pixel and snapped to the nearest active color —
   blendlab's math against the live candidate). Built on two **experimental**
   runtime hooks: `palette_hex(i, hex)` (the `EXPERIMENTAL` banner at the
   bottom of `studio.h`; rides the `pal()` shader path, so existing sprite art
   recolors too; in the help tab under "experimental — testing only"; sunset
   rule: becomes Layer 2's `palette_set()` or is deleted) and **`PALETTE_SIZE` 32→64
   with slots 32–63 mirroring 0–31 by default** — byte-identical for every
   existing cart (`color % 64` lands on the copy; `pget`/shader nearest-match
   scan low-first; verified: `raster_test` 46/46 frames `mismatches:0`,
   `eq total=0`). Findings so far:
   - From the role-mapping work itself: **E32 lacks a lime, a second dark
     plum, and a near-black navy pair** (three dup slots), while the Resurrect
     cut filled every role — the curated-64 advantage showed up before any
     scene was rendered.
   - From the blend scene: **under the shipped 32, glass-over-orange snaps to
     pink and the additive glow vanishes over white** (nowhere to land);
     E32+derived tints coherently; full Resurrect 64 is smoothest. Palette
     density *is* blend fidelity — measured, not argued.
   - **v3 absorbed blendlab's scene-function architecture** (night glow with
     real additive streetlights vs the `D`-toggled fillp fake, glass + MUL
     cloud shadows + AVG fog, the raw AVG/ADD/MUL table grid) with all three
     tables rebuilt from the live candidate on every switch. This answered a
     confusion worth recording: **candidates 2 and 4 are identical in the
     dither scenes by construction** — sunset/portrait only reference indices
     0–31; derived/upper colors only appear where something *samples* them,
     i.e. in the blend paths. With the blendlab scenes in, E32 vs E32+derived
     finally separates on screen: the derived slots add intermediate glow-rim
     steps plain E32 doesn't have.
2. Decide + ship the new default (Layer 1). Re-bake thumbnails.
3. `palette_set` + `de:palette` chunk (Layer 2) — independently useful even if
   blending waited forever.
4. Write the #18 ADR with tables-from-active-palette as a requirement (Layer 3).

Open questions parked here: exact palette choice (needs the contact sheet, and
eyes); whether `DEFAULT_CHAR_MAP`'s letter mnemonics survive a palette change
(`R`=red probably should keep meaning "the red slot"); web build parity for
`palette_set`; what `pget`-based carts assume about specific indices.
