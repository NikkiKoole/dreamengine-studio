# Probe carts — the experiments that decide what becomes API

> **Genre: ledger (kept current).** Some carts aren't (only) games or toys — they
> were **built to answer an API question**: *should the engine own X, or can a
> cart do it?* This file is the census: which cart probes which question, and
> what the verdict was. Carts in this role carry `"probe"` in their `kind[]`
> tags (`editor/public/carts/index.json`, vocabulary in `tools/lint-carts.js`);
> the tag marks the role, **this file holds the verdict**.
>
> The method is [ADR-0006](../decisions/0006-library-carts-not-engine.md):
> prototype in cart-land first; the cart *is* the evidence. Status of the
> underlying API items → [`../STATUS.md`](../STATUS.md) (the ledger wins on
> shipped/open/cut).

## The probe lifecycle

```
question  ──▶  probe cart  ──▶  verdict ──┬─▶ cart-land suffices (stays a library/example cart)
                                          ├─▶ engine API needed  (probe becomes the demo cart)
                                          └─▶ API should be REMOVED (the reverse probe)
                                  ...optionally hardened into a regression guard
```

A probe is **resolved** when its question has a written verdict (an ADR, a
STATUS entry, or a design-doc section). Resolved probes keep their tag — the
tag answers "why does this cart exist?", which stays true forever.

---

## ✅ Resolved: cart-land suffices — no API needed

| Cart | Question | Verdict lives in |
|---|---|---|
| `astar`, `boids`, `sims` | engine pathfinding / flocking / particles? | [ADR-0006](../decisions/0006-library-carts-not-engine.md) — the founding case |
| `ragdoll`, `physics` | engine physics API? | STATUS "decided-against"; [`physics-notes.md`](physics-notes.md) |
| `particles` | engine particle system? | library pattern proven (ADR-0006). *The small `explode()` helper question is separate and still open — STATUS #1.* |
| `loopstation` | engine input/event recorder for live-looping? | [`input-recording-looper.md`](input-recording-looper.md) — looper core is ~70 cart lines; the ring-buffer tee stays a written-down promotion path, unneeded |
| `marble` (the ghost) | engine ghost/replay support? | same doc, stage 2 — position stream + `save_bytes()`; also settled the rule *input ghosts need a frame-stepped sim, `dt()` sims record positions* |
| `sfxed`, `sfxgen` | does sfx authoring need engine banks (`sfx_def()`)? | **leaning no** — export-as-C works (STATUS #5/#12 direction note, audio-notes §5.6). Formally still open; see below. |

## ✅ Resolved: engine API was needed — the probe became the demo

| Cart | What it proved | API that shipped |
|---|---|---|
| `blendlab` | 32-color translucency works as a lookup table, **but cart-land can't read dst correctly** — its deliberately-broken `P` mode shows the last-frame `pget` feedback bloom; dst must come from the in-progress frame | **verdict in, API not yet built** — STATUS #18, [`blend-tables.md`](blend-tables.md); next step is the ADR, **sequenced after the palette decision** ([`palette-and-color.md`](palette-and-color.md) — tables derive from the palette). The one probe that proved *need* and is awaiting the build. |
| `waveed` | drawable single-cycle waves earn their keep (live-morph drone) | `wave_set()` + `INSTR_USER0..3` |
| `platform-paint` (vs `platform-rects`) *(not tagged — shipped with the API as its teaching pair)* | level-as-painted-data needs a spritesheet read | `sget()` |
| `podracer` *(not tagged — a game that stumbled onto the cliff, not built to ask)* | software-poly perf cliff is real | off-screen bbox clamp (STATUS #14) |

*(`heldnotes`, `filterenv`, `pitchenv` are demo carts shipped **with** their APIs —
the probing happened in design docs and the instrument cabinet's ear-testing.
They are not tagged; listed here so nobody re-tags them.)*

## ✅ Resolved: API should be removed — the reverse probe

| Cart | Verdict |
|---|---|
| `16-spirograph` | sole user of turtle graphics; a turtle is `dx`/`dy` + `line()` in ~10 cart lines → **API cut** ([ADR-0008](../decisions/0008-cut-turtle-graphics-api.md)). The cheapest API is one you can delete. |

## 🔄 Open probes — verdict not yet in

| Cart | Question | Where it stands |
|---|---|---|
| `sfxed`, `sfxgen` | engine sound banks / tracker UI (STATUS #5, #12) | direction is "no banks", but explicitly conditional: *"`sfx_def()` only if the prototype proves the engine should own banks"* — the probes are still gathering evidence in use |
| `bones` | offscreen canvas / baked rotation atlas (STATUS #13, [`baked-rotation-atlas.md`](baked-rotation-atlas.md)) | the animator that hits the realtime-drawing wall; whether the buffer primitive is needed is unproven either way |
| **the instrument cabinet** — `stylophone`, `musicalsaw`, `moog`, `modrack` | is the audio surface expressive enough? Each deliberately exercises a corner "no other cart touches" (live duty, live LFO, full patching, modular routing) | open by design — this family already surfaced the mod-envelope gap (shipped); it keeps probing as the synth grows |
| `bossa` *(in flight, another session)* | likely the groove/rhythm-timing question (STATUS #8) | unverified — its author should classify it here |
| `palettelab` | which default palette replaces the lifted PICO-8 one — and at what size? ([`palette-and-color.md`](palette-and-color.md) Layer 1/1b) | contact sheet live, nine candidates: shipped-32 / ENDESGA 32 / Resurrect 64 / E32+derived / ENDESGA 64 / AAP-64 / Famicube / Journey / Jehkoba64 — role-mapped, across swatch/ramp/sunset/portrait scenes **plus blendlab's blend scenes** (night glow, glass+fog, table grid) rebuilt from the live candidate — palette choice and blend fidelity judged together. Built on the **experimental** `palette_hex()` + 64-slot palette (see below) |
| `uikit` | does the `ui.h` widget-kit shape work — per-contact capture, hit-pad inflation, opt-in focus — across mouse + touch + keyboard? ([`ui-widgets-notes.md`](ui-widgets-notes.md)) | v1 (button/slider/knob; panel/drag-from deliberately deferred for a second customer) shipped 2026-06-07 with the showcase cart + the `sfxgen` retrofit (17 sliders + 11 buttons, behavior-faithful incl. undo, verified by scripted replay). Harness-verified on desktop; the **on-device run is the open half** — fat fingers, two-knobs-at-once, the 5-touch ceiling, per the §5.3 probe-before-polish rule |
| `touchlab` | does the touch stack hold up on real hardware — and do we then need a release API (`tapr`/`touch_ended_*`) and/or gesture wrappers (raylib rgestures vs cart-land)? ([`touch-notes.md`](touch-notes.md) §3–5) | 8-point on-device test card built (test 8 = raw rgestures readout); **first device contact 2026-06-05** (iPhone, via the live gallery <https://nikkikoole.github.io/dreamengine/touchlab/>): touch ceiling ~5–6 simultaneous points (iOS Safari limit — phone design budget ≤5), 8px readout text hard to read at phone size. **Gesture sub-question RESOLVED: rgestures' one-global-gesture flaw confirmed on hardware** (a DRAG dies when a second finger taps) → never wrap rgestures; per-finger gestures go cart-land on `touch_id()`, unblocking only on the §3 release API (touch-notes §4/§5.8). Formal run of points 1–7 still pending; broader phone-readiness ledger: [`mobile-web-notes.md`](mobile-web-notes.md) §2. **Same-day follow-through: the release API + `gestures.h` + `touchpiano` shipped AND device-passed** — 5-finger chord on iPhone (the Safari ceiling), every note released by its own finger; per-finger swipes (test 9) fire while other fingers drum, where rgestures (test 8) dies. The release-API design is parked in §3 with the touch piano as its designated second customer |

## 🧪 Not probes — regression guards (don't re-tag)

`raster_test`, `trifill_stress`, `soundcheck` **defend** verdicts; they don't ask
questions. Likewise `smooch` (the DE_TRACE harness worked-example),
`classic-starter` (STATE-sugar demo), `monstermix` (sprite-draw `stamp()`
showcase — validates the *JS library*, no engine question), and `drummachine` /
`mariopaint` (toys that informally support the "no banks" lean but weren't
built to ask it).

## 📭 Probes that should exist but don't yet

- **Sequence scripts** (STATUS #17) — explicitly *"worth prototyping one
  `sequence`/`wait` helper to feel the ergonomics"*; `dialogue`/`zak`/`adventure`
  hand-roll the switch-state cutscene pattern that motivates it.
- **Events** `broadcast()`/`received()` (STATUS #3) — "confirmed demand", zero
  carts built to test whether a cart-land message array is just fine.

---

## Experimental engine surface (probes sometimes need one hook)

Most probes are pure cart-land. When a question is *unreachable* from cart code
(the palette lives behind `studio.c` statics), the probe may get **one
explicitly-experimental engine hook** rather than a full public API: declared in
`studio.h` under the `EXPERIMENTAL` banner, documented in the help tab under
**"experimental — testing only"** (discoverable, but clearly flagged as not for
games), with a written sunset — it resolves into real API or gets deleted with
its probe. Current experiments:

| Hook | Probe | Sunset |
|---|---|---|
| `palette_hex(i, hex)` | `palettelab` | becomes `palette_set()` ([`palette-and-color.md`](palette-and-color.md) Layer 2) or is deleted with the palette decision |
| `PALETTE_SIZE` 32→64 (slots 32–63 mirror 0–31 by default; byte-identical for existing carts, `raster_test`-verified) | `palettelab` | kept if a 64-color default wins Layer 1b, reverted to 32 otherwise |

## Housekeeping rules

1. New experiment cart → tag `"probe"` in `index.json` + add a row here with its
   question, **when you build it** (a probe without a written question drifts
   into being "just a tech-demo").
2. Verdict lands → move the row to the right section and link the ADR/STATUS
   entry. Keep the tag.
3. If a probe earns a regression guard, the guard is a *new* cart (or script),
   listed under "not probes".
