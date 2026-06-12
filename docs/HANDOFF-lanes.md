# HANDOFF — effects lanes / chains (fx_order, pedalboard builder)

_Session handoff, 2026-06-12. Cross-machine continuity for the reorderable-effects work._
_Everything below is committed AND pushed to `master` — on a fresh machine, `git pull` makes you current._

## TL;DR

**Effects-bus Increment A is shipped, documented, and live on the web.** Latest commit of mine: `64edd41`.
Other agents are mid-flight in `sound.h` / `studio.*` (Leslie + fonts) — **stay out of those files until they land.**

## What shipped this session (all pushed)

| Commit | What |
|---|---|
| `b004363` | **Engine + `fx_order(bus, kinds, n)` API** + `FX_*` constants. The hardcoded master/aux insert ladder is now a per-bus `insert_order[]` the audio thread walks. Default order = old ladder, each step gated on `_used[b]` → **byte-identical when unused**. 4-place wired + tcc regenerated. |
| `d5d43ba` | **Pedalboard rebuilt as a drag-and-drop chain BUILDER** — `≡ PEDALS` palette → drag to add / reorder / remove; 9 effects. |
| `e6c51f2` | **Drag polish** — lift-out reorder, live green drop caret, 5-per-row palette chips. |
| `dc4d169` | **Docs** — STATUS.md ledger, audio-notes §17 item 10, `effects-bus-architecture.md` + `sound-next-steps.md` flipped to "A shipped, C next." |
| `64edd41` | **Web publish** — pedalboard → wasm, pushed; live at the Pages site. |

## Repo state when I left — DON'T collide

Working tree is dirty with **other agents'** in-flight work (none mine — all my work is committed):

- **Leslie agent**: `sound.h`, `studio.c/.h`, `effects-recipes.md`, `decisions/0015…md`, `navkit-fx-render.c`, `organ.*`, `tools/carts/leslietest.c`. Leslie is correctly **pinned per-bus** (rotary speaker = output stage, not a reorderable pedal), reuses `moddel_hermite`, lives around `sound.h:994`.
- **Font agent**: `font-bake.js`, `gen-rom-font.js`, `fontcomic.*`, `FONT_LARGE` / `FONT_BOOT` in `studio.h`.

→ **Don't edit `sound.h` / `studio.h` until those are committed and the tree is green.**

## To resume

1. `git pull` on the new machine.
2. `make` (kills stale processes, starts the editor). Reload the **pedalboard** cart, hit `≡ PEDALS`, drag effects in / around — confirm the feel.
3. Pick up a parked chapter below.

## Next chapters (parked, not started)

- **Effects-bus Increment C** — reverb as a bus *insert* + a sparse **reverb tank pool** (multiple rooms at once). This is what makes reverb's pedalboard position *real* instead of cosmetic. Full sketch + cost ledger: [`design/effects-bus-architecture.md`](design/effects-bus-architecture.md) §5/§6. **Engine work on `sound.h`** → wait for a quiet tree.
- **Pedalboard AMP block** — a pinned "AMP/CAB" at the *end* of the chain (drive + cab-shaped EQ: roll off <80 Hz & >5 kHz + a presence bump). Likely a pure **cart-side recipe** (no engine change, no collision) per decision 0015. Maybe two voicings (Fender-clean / Vox-Marshall-crunch). Contained afternoon's work.
- **The "pinned output stage" concept** — Leslie + amp/cab + soft-clip are forming a **4th bus zone**: parallel sends → reorderable inserts → **pinned output**. Worth a doc note once Leslie lands.

## Gotchas

- `fx_order` packs kinds at **3 bits each → 0–7 full.** A 9th *reorderable* kind needs widening the packing. **Pinned** effects (Leslie, amp) sidestep this entirely — no `FX_*` kind, never touch `insert_order`.
- The three bus zones today: **parallel sends** (echo/reverb) → **reorderable inserts** (`fx_order` chain) → **pinned output** (soft-clip, now Leslie). Pinned stages run per-bus *after* the `insert_order` loop, before the master soft-clip.
- Any pinned effect must be **dormant until enabled** (e.g. `leslie_used[b]`) or it breaks the byte-identical guarantee.
- After any `sound.h` edit: `node tools/play.js soundcheck script /dev/null --headless --frames 2` (silence = pass) + sentinel-check your change survived the commit. Pre-commit hook (`core.hooksPath .githooks`) compile-gates `sound.h`/`studio.h`.
- Pedalboard internals: chain = ordered DISTINCT catalog ids (`C_BIT…C_RVB`), each → an engine `FX_*` kind (or `-1` for reverb the send). `apply_fx()` pushes every effect's state then calls `fx_order(0, kinds, n)` with the in-chain order. Reverb position is cosmetic until Increment C.
