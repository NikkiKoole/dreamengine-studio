# HANDOFF — effects lanes / chains (fx_order, pedalboard builder)

_Session handoff, 2026-06-12. Cross-machine continuity for the reorderable-effects work._
_Everything below is committed AND pushed to `master` — on a fresh machine, `git pull` makes you current._

## TL;DR

**Effects-bus Increment A + the FULL Increment C are shipped.** Increment A (reorderable inserts +
the pedalboard builder) is live on the web. **Increment C landed 2026-06-12**: a `ReverbTank` pool
(3 tanks), `FX_REVERB` as a reorderable wet-replace insert, and the API — `reverb_bus()` /
`instrument_reverb_bus()` (**multi-reverb**, two spaces at once) **plus `reverb_bus_fx()`** (the
fast-follow, same day: **effects AFTER the reverb** — reverb→crush/eq/tape, tank-keyed so a cart never
touches the bus index). Showcase = the **reverb spaces** cart (two spaces + a CRUSH toggle + a SONG
toggle). Legacy `reverb()` bytes-identical (verified). Leslie + fonts landed, tree clean.

**C is fully cart-exposed now — nothing open on it.** The remaining parked chapters below are
*new* directions (AMP block, `reverb()`→bus migration, sidechain), not C gaps.

## What shipped this session (all pushed)

| Commit | What |
|---|---|
| `b004363` | **Engine + `fx_order(bus, kinds, n)` API** + `FX_*` constants. The hardcoded master/aux insert ladder is now a per-bus `insert_order[]` the audio thread walks. Default order = old ladder, each step gated on `_used[b]` → **byte-identical when unused**. 4-place wired + tcc regenerated. |
| `d5d43ba` | **Pedalboard rebuilt as a drag-and-drop chain BUILDER** — `≡ PEDALS` palette → drag to add / reorder / remove; 9 effects. |
| `e6c51f2` | **Drag polish** — lift-out reorder, live green drop caret, 5-per-row palette chips. |
| `dc4d169` | **Docs** — STATUS.md ledger, [audio-notes](design/audio-notes.md) §17 item 10, `effects-bus-architecture.md` + [`sound-next-steps.md`](design/sound-next-steps.md) flipped to "A shipped, C next." |
| `64edd41` | **Web publish** — pedalboard → wasm, pushed; live at the Pages site. |

## Repo state when I left — DON'T collide

Working tree is dirty with **other agents'** in-flight work (none mine — all my work is committed):

- **Leslie agent**: `sound.h`, `studio.c/.h`, [`effects-recipes.md`](guides/effects-recipes.md), `decisions/0015…md`, `navkit-fx-render.c`, `organ.*`, `tools/carts/leslietest.c`. Leslie is correctly **pinned per-bus** (rotary speaker = output stage, not a reorderable pedal), reuses `moddel_hermite`, lives around `sound.h:994`.
- **Font agent**: `font-bake.js`, `gen-rom-font.js`, `fontcomic10x20.*` / `fontthin8x8.*`, `FONT_COMIC` / `FONT_THIN` in `studio.h` (and dropped `FONT_LARGE`/`FONT_BOOT`/`FONT_SMOOTH`).

→ **Don't edit `sound.h` / `studio.h` until those are committed and the tree is green.**

## To resume

1. `git pull` on the new machine.
2. `make` (kills stale processes, starts the editor). Reload the **pedalboard** cart, hit `≡ PEDALS`, drag effects in / around — confirm the feel.
3. Pick up a parked chapter below.

## Next chapters (parked, not started)

- **✅ DONE — effects AFTER the reverb (`reverb_bus_fx`).** The fast-follow shipped same-day: the
  tank-keyed `reverb_bus_fx(tank, fx, a, b, c)` (the generic shape, not the two-function one) resolves
  tank→bus on the audio thread, configures the insert (crush/eq/tape/chorus), and appends it after
  `FX_REVERB`. The **reverb spaces** CRUSH toggle is the signature demo. Left here as a record, not a TODO.
- **Migrate `reverb()` onto the bus path** (effects-bus-architecture.md §7 open call #3) — optional cleanup
  so there's *one* reverb mechanism. Today tank 0 stays a master parallel send (deliberate, kept bytes-identical).
- **The output stage (4th zone): cabinets — amp/cab · Leslie** — now a full design section,
  [`effects-bus-architecture.md` → "Increment E"](design/effects-bus-architecture.md). Leslie shipped
  (the one tenant that exists); E generalizes it to a pinned **"pick your cabinet" slot** where a guitar
  amp is the obvious next tenant. An amp = a **cart-side recipe** (drive + cab-shaped EQ + `glue` power-sag
  + soft-clip; 0015's "amp/cab = drive + lowpass") — likely **no engine change**. ~5 named voicings sketched
  (clean/chime/crunch/high-gain/lo-fi). Also the **gain-staging cure** for long chains that clip. Build after
  Increment D's `glue()` lands + the tree is quiet.

## Gotchas

- `fx_order` packs kinds at **3 bits each → 0–7 full.** A 9th *reorderable* kind needs widening the packing. **Pinned** effects (Leslie, amp) sidestep this entirely — no `FX_*` kind, never touch `insert_order`.
- The three bus zones today: **parallel sends** (echo/reverb) → **reorderable inserts** (`fx_order` chain) → **pinned output** (soft-clip, now Leslie). Pinned stages run per-bus *after* the `insert_order` loop, before the master soft-clip.
- Any pinned effect must be **dormant until enabled** (e.g. `leslie_used[b]`) or it breaks the byte-identical guarantee.
- After any `sound.h` edit: `node tools/play.js soundcheck script /dev/null --headless --frames 2` (silence = pass) + sentinel-check your change survived the commit. Pre-commit hook (`core.hooksPath .githooks`) compile-gates `sound.h`/`studio.h`.
- Pedalboard internals: chain = ordered DISTINCT catalog ids (`C_BIT…C_RVB`), each → an engine `FX_*` kind (or `-1` for reverb the send). `apply_fx()` pushes every effect's state then calls `fx_order(0, kinds, n)` with the in-chain order. Reverb position is cosmetic until Increment C.
