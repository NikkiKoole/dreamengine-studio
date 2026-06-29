# Dynamic palettes — the index is a role, not a colour

STATUS: IDEA / thinking-in-progress (2026-06-30). A design exploration, nothing decided — the maker
is still chewing on it. Goes deep on one layer of [`palette-and-color.md`](palette-and-color.md):
its **Layer 2 ("settable palette per cart")**, and specifically the question that layer leaves open
— *what happens to the `CLR_*` colour-name constants when the palette can change underneath them.*
Companion: [`palette-and-color.md`](palette-and-color.md) (the own-vs-borrowed decision + the
release gate), [`blend-tables.md`](blend-tables.md) (Layer 3, tables from the live palette),
`tools/carts/palettelab.c` (the working probe — 11 swappable candidates today).

## The instinct

The more palettes you try on (Lospec is a deep well), the more the *fixed* 32-colour palette feels
like the wrong unit. The appeal: **a cart works in a small set of slots, but which RGB sits behind
each slot is dynamic** — swappable per cart, per scene, per mood, even procedurally.

## The three-way design space

1. **Fixed indexed** (today) — one shared 32-colour palette; `CLR_RED` is *always* `#FF004D`. Draw
   with indices.
2. **Dynamic indexed** (this note) — a small working set of slots; **which colour each slot holds is
   rebindable at runtime.**
3. **Truecolor** (`pset_rgb` / `rectfill_rgb`) — no palette, absolute 24-bit RGB.

**#2 is the historically authentic one.** VGA mode 13h (a programmable DAC), the Amiga (copper
palette rewrites mid-frame), Atari ST, EGA — all drew with indices into a *programmable* palette and
did fades, day/night, and palette-cycling by rewriting the palette registers. PICO-8's *fixed* 16 is
the unusual constraint, not the norm. So dynamic indexed is arguably *more* on-brand for the
engine's documentary/homage soul than the borrowed fixed palette is.

## The machinery already mostly exists

This is not a big engine build — it's promoting and conventionalising what's already here:

- **`palette_hex(i, 0xRRGGBB)`** (currently `EXPERIMENTAL`) already rebinds a slot's RGB at runtime.
  It is how `palettelab` swaps all 11 candidates live — **that cart is the dynamic-palette proof.**
- **`pal(c0, c1)`** already recolours sprites by index.
- **Blend tables** (`blendlab`/`palettelab`) already **rebuild AVG/ADD/MUL from the live palette on
  every swap**, cheaply (a 32×32 nearest-colour pass). So translucency/glow survive a dynamic palette.
- **`pset_rgb`** is the truecolor escape hatch when a gradient genuinely needs it.

palette-and-color.md §5 already notes the path: `palette_hex` either *"graduates to Layer 2's
`palette_set()` or is deleted."* This note argues it should graduate.

## The hard part — and the resolution: `CLR_*` become *roles*

The one genuinely hard thing is the colour-name constants. Carts draw with `CLR_RED`,
`CLR_DARK_GREEN`, `CLR_MEDIUM_GREY`. Today the name reads as a *literal hue*. If the palette can
change, the name looks like it becomes a lie.

**The resolution: stop reading `CLR_RED` as "this exact red" and read it as "this palette's red
slot." The name is a promise about a *role*, not a hex.**

And this isn't hypothetical — **`palettelab` already enforces it.** Every candidate is *role-mapped*:
its header says *"slot 8 must stay 'the red', ramps must still ramp, or every existing cart
scrambles."* So in DB32 slot 8 is DB32's red; in Rosy 42 slot 8 is Rosy's red. A cart that drew
`rectfill(..., CLR_RED)` gets the **current palette's red, automatically, with zero code changes.**

So the practical answer to "how do we use the constants":

- **You keep using `CLR_*` exactly as now.** Only the *meaning* shifts: from "a fixed colour" to "a
  role the current palette fills." Existing carts reskin for free precisely *because* they were
  speaking in roles all along — we just never named it that.
- The `CLR_*` vocabulary isn't wasted by dynamic palettes; it **becomes** the role vocabulary. The
  awkwardness felt today is that the names straddle two meanings (slot-number *and* literal-hue).
  Commit them to "role" and the tension dissolves.

### The honest residual (where roles don't fully hold)

Not clean everywhere — the seams are already visible in `palettelab`:

- **The ~16 core roles map well** (black, white, the red/orange/yellow/green/blue ramp, skin). Those
  `CLR_*` stay trustworthy across any reasonable palette.
- **The off-role / upper-16 slots don't.** Rosy 42 has no grey (slots 5/6 borrow mauve); DB32 has no
  teal (slot 19 borrows blue). A cart that leaned on `CLR_MEDIUM_GREY` for a *specific* neutral grey
  shifts under a palette that has none. Unavoidable — an arbitrary Lospec palette can't fill all 32
  pico roles.
- **The 64-slot extension** colours have *no* role names; carts reference them by number — the least
  portable across palettes.

**Rule of thumb that falls out:** draw with the *core role names* and you reskin safely; pin a
*specific hue* (an off-role grey, an exact lavender) and you must accept the drift or lock it with
`palette_hex`/`pset_rgb`. Role-mapping is a good approximation, not a guarantee.

## The cleaner variant: a cart names its own few colours

If a cart only wants ~8 working colours, it needn't borrow the 32 `CLR_*` roles at all — it can name
its own:

```c
enum { INK, PAPER, ACCENT, SHADOW /* ... */ };   // the cart's private role vocabulary
```

…and bind those to RGB dynamically. Then `CLR_*` is just the *default palette's* convenience names
for carts that don't bother. Both models coexist: `CLR_*` = the shared 32-role default; a cart's own
enum = its private small palette. This is closer to the NES sub-palette / Game Boy Color model than
to VGA-256, and it's the strongest version of the "few colours, dynamically bound" instinct.

## Where truecolor sits

Keep `pset_rgb`, but as the **exception, not the idiom.** Dynamic-indexed is the middle path that
*preserves* what makes the engine itself: the lo-fi discipline, the cheap index-only blend tables,
the free sprite reskins. Truecolor throws all of that away for unlimited colour — right for a
gradient sky or a CPU shader, wrong as the default way to draw. They already coexist; the call is
just *"dynamic-indexed is how you normally draw; truecolor is the escape hatch."*

## Back-compat / adoption (grow as we go)

~400 existing carts assume the fixed global palette and the literal-hue reading of `CLR_*`. So:

- **Keep the fixed global 32 as the default.** Old carts unchanged.
- **Make dynamic palettes opt-in** — a cart calls `palette_set(...)` / declares a palette and (if it
  wants) its own role enum. Same grow-as-we-go shape as the song codec and touch controls: the
  capability lands without a flag-day.
- **Sprites stay index-based** and reskin for free on swap — a feature, not a cost.

## How this lands against palette-and-color.md

- It's the deep version of **Layer 2** (settable palette per cart). That doc proposes the mechanism
  (`palette_set()` + a `de:palette` chunk); this note supplies the *conceptual* resolution it lacked
  — `CLR_*`-as-roles and the role-mapping contract.
- It's also a **third answer to the doc's framing question** ("our own palette *vs.* borrowed
  PICO-8"): *don't pick one fixed palette — make it per-cart and swappable.*
- It interacts with the **release gate** (no paid app on the borrowed PICO-8 palette): a dynamic
  system means each *product* cart can wear its own identity palette while the free legacy gallery
  keeps PICO-8 as homage — the own-identity requirement met per-cart instead of by one global swap.

## Open forks (for when this stops being a thinking exercise)

- **How many colours per cart? (the load-bearing fork.)** It's a spectrum, and where a cart sits on
  it changes its whole character:
  - **2 (1-bit, ink+paper)** — Playdate, *Obra Dinn*, Game Boy-mono. Dithering does the tonal work;
    unmistakable identity, trivial 2-hex reskins, tiny tables. Cost: demands dithering skill.
  - **3–4** — Game Boy (4 greens), CGA, NES sub-palettes. The strongest "very limited" band, and it
    has a perfect precedent: ***Downwell*** — its identity *is* **3-colour palettes you unlock and
    swap** (dozens of them, each just 3 hexes). That is exactly "3-colour carts where *which*
    colours is dynamic."
  - **~8–16** — PICO-8 (16), ZX Spectrum, EGA. The classic fantasy-console sweet spot.
  - **~32 (today) / 64** — ramps + variety, blends smooth, dithering optional; identity softer and
    the off-role residual worsens the more roles you demand.
  - **256 / truecolor** — discipline gone.

  **Key coupling: a small working set is what makes dynamic palettes *clean*.** The off-role residual
  (a palette can't fill all 32 pico roles) exists *only because* we ask for 32 roles. A 3-colour cart
  has 3 roles → *any* palette fills them with zero lossy role-mapping (this is why Downwell can wear
  any 3 colours). So "very limited" isn't only aesthetic — it's what lets a cart wear any Lospec
  palette honestly. The 32-role world is where dynamic gets messy.

  **The meta-fork (a values call):** (1) **one canonical size = the console's signature** — PICO-8
  *is* 16, Game Boy *is* 4; the constraint becomes the brand and the whole library coheres; vs
  (2) **size is per-cart** — max expressive range, but no single visual signature. #2 may fit
  dreamengine better than most consoles, because the north star says the identity is *"deep sim
  behind a humble lo-fi surface,"* not a colour count — 3-colour and 16-colour are both humble, so
  per-cart size doesn't dilute the brand the way it would for PICO-8. Possible split: a small
  **default** house set, carts free to widen. This one drives everything downstream — decide it first.
- **Where does a cart declare its palette?** A `palette_set()`/`palette_load()` API call, a
  `de:palette` `de:meta`/zTXt chunk, or a `.cart.js` field — likely the API for runtime swaps + a
  `de:meta` field for the cart's *default* identity palette.
- **Loading Lospec palettes directly** — a tool (or runtime loader) that role-maps a `.hex`/`.gpl`
  into the 32 slots (the same role-mapping `palettelab` does by hand for its 11 candidates).
  Role-mapping an arbitrary palette is *lossy* (the off-role residual above) — needs a defined
  fallback for missing roles.
- **Does `palette_hex` graduate to `palette_set`, and what's the slot count** (32 vs 64, Layer 1b)?
- **Accessibility / theming** as first-class swaps (colourblind-safe palette in one call).
