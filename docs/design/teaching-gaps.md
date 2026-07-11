# Teaching gaps — what the API offers but nothing teaches

STATUS: BUILDING — audit of under-taught API areas + a prioritized tutorial attack list; 04b-mouse, camera, music-sequencing, 3D, and (2026-07-11) touch + audio-effects-chain shipped; collision track (27–33) shipped. Type-1 on-ramp holes now all closed.

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
| **Mouse input** (`mouse_*`) | 124 (32%) | ~~0~~ **1** | `04b-mouse` now teaches `mouse_x`/`mouse_y`/click directly |
| **Camera / viewport** (`camera`/`follow`/`clip`) | 66 (17%) | ~~0~~ **1** | `10-world` only used the `follow()` wrapper; `10b-camera` now teaches `camera()`/`clip()` directly |
| **Audio effects chain** (`echo`/`reverb`/`chorus`/`tape`/`wah`/`crush`/`eq`…) | 58 (15%) | ~~0~~ **1** | `22b-fxchain` (2026-07-11) toggles echo/reverb/crush on one note — teaches the bus + the set-and-hold rule |
| **Touch input** (`touch_*`/`tap`) | 64 (16%) | ~~0~~ **1** | `04c-touch` (2026-07-11) teaches `touch_count`/`touch_x`/`touch_y`/`tap`, sibling of `04b-mouse` |

## Gap type 2 — whole subsystem, no entry point

| Capability | Carts | Tutorials | Note |
|---|---|---|---|
| **3D** (`rot3`/`project3`/`tritex`/`quadfill`) | 12 (3%) | ~~0~~ **1** | demos exist (cube3d, solid3d, dreamcad) but had no "your first 3D" — `26-first3d` now teaches it |
| **Music sequencing** (`euclid`/`chord`/`strum`/`degree`) | — | ~~0~~ **1** | `06-sound` only did `bpm`/`every`; `06b-chords` now teaches scales/chords/strum/euclid |
| **Spatial audio** (`listener`/`note_pos`/`spatial_model`) | 4 (1%) | 0 | evocative but genuinely niche — low priority |
| **MIDI** (`midi_*`) | 5 (1%) | 0 | specialized — low priority |

## Gap type 3 — API nobody uses (under-*demonstrated*, not under-taught)

Only **3** studio.h functions are called by zero carts as of 2026-07-02 — down from 14 on
2026-06-22, and from 12 earlier the same day. `instrument_autopan` (`epiano`), `instrument_leslie`
(`organ`), `instrument_sync` (`moog`), `instrument_flanger` (`mistress`, `more-note-bass`), and
`zoom_rect` (`facegen`/`gnomeseek`/`uiloupe`) picked up real showcases over the preceding weeks.
Then `mixbooth` — a tiny 6-track band where each track gets its own private
`instrument_tape`/`instrument_gate`/`instrument_ringmod`/`instrument_univibe`/`instrument_shallow`
(+`instrument_grains_freeze`/`instrument_grains_pitch` for a frozen-pad mode)/`instrument_fx_mod`
effect, plus a `strum_notes()` guitar re-strum and a retrofit of the underused `ampcab.h` amp/cab
bundle onto a 6th track — cleared **all 8** remaining per-instrument variants and `strum_notes` in
one cart (the whole family shared one teaching point: "this effect touches ONE voice, not the
mix" — no reason to spread it across 8 showcase carts). `map_scale` stays a technicality-only
non-zero: its one use is `drawall.c`, the every-primitive coverage cart (CLAUDE.md rule #5), not a
teaching showcase, so it's still undersold in spirit.

What's left:

- `tapr` — the touch-release edge trigger. Don't give it a standalone cart; it belongs in the
  still-open touch tutorial (attack list #4, `tap`/`touch_*`) as one release-to-fire target.
- `watch_visible`, `paused` — dev/debug conveniences, not really teachable concepts. Probably not
  worth a showcase cart at all; a one-line mention in [`debug-harness.md`](../guides/debug-harness.md) covers it better.

(The long tail has shifted too — `print_rot_scaled` (3 carts) and `rectfill_rot` (4 carts/8
calls) have outgrown "1–2 carts"; still in that band: `arcoutline`, `ringoutline`, `pget_rgb`,
`zoom_rect`, `map_scale` (coverage-only — see above), `lfo_value`, `listener_vel`,
`note_motion`/`note_follow`/`note_sync` — see `node tools/api-usage.js` for the live list.)

## The attack list — prioritised

Mission is the teaching on-ramp (VISION.md), so **Gap type 1 first.** Suggested carts:

1. ~~**`04b-mouse`** — click / drag / hover with `mouse_*`.~~ ✅ **SHIPPED** (2026-06-22) — the
   highest-leverage gap (32% usage, 0 teaching) is now an on-ramp. `mouse_world_*` under a camera
   is deferred to the camera tutorial (#2).
2. ~~**A camera/follow tutorial** — a world bigger than the screen, `follow()` the player, `clip()` a HUD.~~
   ✅ **SHIPPED** (2026-07-01) as `10b-camera` — went with manual `camera()`/`clip()` instead of
   `follow()` (10-world already covers `follow()`), so the primitive itself gets taught, plus a
   clipped inset window as the "second view" idiom. `zoom_rect`/`map_scale` are still undemoed
   (type 3).
3. **`23-effects`** — the sequel to the `20–22` synth trilogy: send a voice through `reverb`/`echo`/`crush`,
   the set-and-hold rule (see [`../guides/effects-recipes.md`](../guides/effects-recipes.md)).
4. **A touch tutorial** — `tap`/`touch_*`, honouring the mobile goal ([`mobile-web-notes.md`](mobile-web-notes.md)).
5. ~~**"Your first 3D"** — one spinning textured quad via `rot3`/`project3`/`tritex`.~~
   ✅ **SHIPPED** (2026-07-01) as `26-first3d` — a single quad (not a solid), rotated + projected,
   filled with two `tritex()` calls; a Z-toggle wireframe overlay exposes the shared diagonal seam,
   and an X-toggle subdivision grid (1→2→4→8) teaches `textured3d`'s affine-warp fix directly,
   instead of deferring it to that cart.
6. ~~**A music-sequencing tutorial** — `chord()`/`strum()`/`euclid()`/scales beyond `06-sound`.~~
   ✅ **SHIPPED** (2026-07-01) as `06b-chords` — a I-IV-V-vi progression via `degree()`, voiced on the
   real `INSTR_PIANO` grand piano with `strum()`, gated by a `euclid()` hi-hat with a live density knob.

Type 3 was a separate sweep: showcase the orphaned capability, or cut the dead weight. Mostly
done now (`mixbooth` cleared 9 of the 12 remaining in one cart, see above) — `tapr` rides the
still-open touch tutorial (#4); `watch_visible`/`paused` are probably dead weight, not undersold
(decide per fn — `node tools/api-usage.js` flags the current zero-use list).

## See also
- [`tutorial-curriculum.md`](tutorial-curriculum.md) — the planned tutorial wave (25+); this doc
  supplies the *API-gap evidence* for what that curriculum should prioritise
- [`cart-library-direction.md`](cart-library-direction.md) — the cart-shelf companion (what to *build*)
- the ★ techniques compendium (`tools/build-compendium.js` → editor Docs tab) — what each cart *teaches*
- [`api-usage-audit.md`](api-usage-audit.md) — the API-usage findings snapshot

*The counting method + derived-number freshness convention: [`driftable-docs.md`](driftable-docs.md).*
