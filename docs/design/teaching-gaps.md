# Teaching gaps — what the API offers but nothing teaches

> **Exploratory snapshot (2026-06-22).** A "what's under-taught?" pass over the library,
> cross-referencing the API surface against what tutorials + tagged carts actually cover.
> Not a committed backlog — it's the stop-and-look memo, the teaching-side companion to
> [`cart-library-direction.md`](cart-library-direction.md) (which looks at the *cart shelf*).
> **Re-run the numbers before acting** — the library moves fast.

## How this was measured (reproducible)

Two lenses, both from existing tools:

```bash
node tools/api-usage.js        # mechanical axis: how many carts call each studio.h fn
node tools/cart-index.js --coverage   # technique coverage (families + teaches concepts)
# tutorial coverage: grep the tutorial .c sources for an area's signature calls, e.g.
#   grep -lE 'mouse_' $(tutorials)   # → 0 tutorials touch the mouse
```

A **gap** is where an API area scores high on "the library uses it" but zero on "a tutorial
teaches it" — learners copy it blindly — *or* a whole subsystem has no entry point at all.

## Gap type 1 — used a lot, taught nowhere (highest leverage)

The on-ramp holes: heavily used across the library, but **no tutorial explains them**.

| Capability | Carts using it | Tutorials | Note |
|---|---|---|---|
| **Mouse input** (`mouse_*`) | 124 (32%) | **0** | `04-buttons` is keyboard-only; a third of carts use the mouse with no lesson |
| **Camera / viewport** (`camera`/`follow`/`clip`) | 66 (17%) | **0** | the "world bigger than the screen, follow the player" concept — core, untaught |
| **Audio effects chain** (`echo`/`reverb`/`chorus`/`tape`/`wah`/`crush`/`eq`…) | 58 (15%) | **0** | `20–22` teach *voices*; the whole effects bus has no on-ramp |
| **Touch input** (`touch_*`/`tap`) | 64 (16%) | **0** | mobile is a stated project goal, yet no touch/tap tutorial |

## Gap type 2 — whole subsystem, no entry point

| Capability | Carts | Tutorials | Note |
|---|---|---|---|
| **3D** (`rot3`/`project3`/`tritex`/`quadfill`) | 12 (3%) | **0** | demos exist (cube3d, solid3d, dreamcad) but no "your first 3D" |
| **Music sequencing** (`euclid`/`chord`/`strum`/`degree`) | — | **0** | `06-sound` only does `bpm`/`every`; chords + euclidean rhythm untaught |
| **Spatial audio** (`listener`/`note_pos`/`spatial_model`) | 4 (1%) | 0 | evocative but genuinely niche — low priority |
| **MIDI** (`midi_*`) | 5 (1%) | 0 | specialized — low priority |

## Gap type 3 — API nobody uses (under-*demonstrated*, not under-taught)

14 studio.h functions are called by **zero** carts — a different problem: they exist but no
cart shows them off. Each is either *undersold* (wants one showcase cart) or *dead weight*
(a candidate to cut). As of this snapshot:

- **per-instrument effect variants** — `instrument_tape`, `instrument_gate`, `instrument_ringmod`,
  `instrument_univibe`, `instrument_shallow`, `instrument_grains_freeze`, `instrument_grains_pitch`,
  `instrument_fx_mod`, `instrument_autopan`, `instrument_flanger`, `instrument_leslie`, `instrument_sync`
- **graphics/util** — `zoom_rect`, `map_scale`, `strum_notes`, `paused`, `watch_visible`, `tapr`

(Plus a long tail used by only 1–2 carts: `arcoutline`, `ringoutline`, `pget_rgb`, `rectfill_rot`,
`print_rot_scaled`, `lfo_value`, `listener_vel`, `note_motion`/`note_follow`/`note_sync`, … — see
`node tools/api-usage.js` for the live list.)

## The attack list — prioritised

Mission is the teaching on-ramp (VISION.md), so **Gap type 1 first.** Suggested carts:

1. ~~**`04b-mouse`** — click / drag / hover with `mouse_*`.~~ ✅ **SHIPPED** (2026-06-22) — the
   highest-leverage gap (32% usage, 0 teaching) is now an on-ramp. `mouse_world_*` under a camera
   is deferred to the camera tutorial (#2).
2. **A camera/follow tutorial** — a world bigger than the screen, `follow()` the player, `clip()` a HUD.
   Every platformer needs it; nothing teaches it. (Retire or demo the dead `zoom_rect`/`map_scale`.)
3. **`23-effects`** — the sequel to the `20–22` synth trilogy: send a voice through `reverb`/`echo`/`crush`,
   the set-and-hold rule (see [`../guides/effects-recipes.md`](../guides/effects-recipes.md)).
4. **A touch tutorial** — `tap`/`touch_*`, honouring the mobile goal ([`mobile-web-notes.md`](mobile-web-notes.md)).
5. **"Your first 3D"** — one spinning textured quad via `rot3`/`project3`/`tritex`.
6. **A music-sequencing tutorial** — `chord()`/`strum()`/`euclid()`/scales beyond `06-sound`.

Type 3 is a separate sweep: one showcase cart per orphaned capability, or cut the dead weight
(decide per fn — `node tools/api-usage.js` flags the current zero-use list).

## See also
- [`tutorial-curriculum.md`](tutorial-curriculum.md) — the planned tutorial wave (25+); this doc
  supplies the *API-gap evidence* for what that curriculum should prioritise
- [`cart-library-direction.md`](cart-library-direction.md) — the cart-shelf companion (what to *build*)
- the ★ techniques compendium (`tools/build-compendium.js` → editor Docs tab) — what each cart *teaches*
- [`api-usage-audit.md`](api-usage-audit.md) — the API-usage findings snapshot
