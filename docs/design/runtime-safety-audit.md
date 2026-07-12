# Runtime safety audit — correctness & memory-safety bugs

> **STATUS: BUILDING** (2026-07-12) — a correctness/memory-safety sweep of all hand-written
> runtime C/H, the **bug-hunting complement** to [`engine-simplification.md`](engine-simplification.md)
> (which was quality-only — "no bug fixes"). Found by a 7-way read-only fan-out (one auditor per
> subsystem), every finding then re-verified against source. **3 high, 2 medium, a tail of low/latent.**
> Vendored/generated files (`stb_image.h`, `fonts_baked.h`, `*_data.h`, `raylib-web/`, `libtcc/`) were
> out of scope. Working the list top-down; each item names the gate that proves the fix.

---

## How to work this list

- These are **behaviour-*changing* fixes** (unlike the pure refactors in `engine-simplification.md`),
  so each is gated by a deterministic oracle **plus** a "no regression on the happy path" check:
  - `studio.c` draw path → `node tools/canvas-diff.js drawall` (+ any affected cart) and
    `node tools/build-all.js`.
  - `sound.h` → the audio gates per CLAUDE.md: `soundcheck` for queues, `tune-check --quiet` for
    pitched engines, `fx-check.js`/`level-check.js` for effects. A clamp that only rejects
    out-of-range input must leave every in-range render **byte-identical** (re-bless nothing).
  - `net.h` → `node tools/net-check.js` (the lockstep gate).
  - header libs (`lay.h`/`radio.h`/`roadkit.h`/…) → `node tools/build-all.js` + `ui-audit.js` +
    bake & eyeball an affected cart.
- **`sound.h` and `studio.h` are hot shared files** — targeted `Edit`s only, never full-file `Write`;
  re-`Read` the region right before editing; confirm the change survived the commit (CLAUDE.md "Hot
  shared source files"). Line numbers below are a **2026-07-12 snapshot** — treat the function name
  as the anchor and grep the idiom.
- Checkbox each item as it lands. Group 🔴 (reachable memory-safety) is highest leverage — start there.

---

## 🔴 High — reachable memory-safety

- [x] **1. `sw_blit` fast path reads the sprite atlas out of bounds.** ✅ 2026-07-12 — source rect
  now clamped against sheet dims in the fast path; `canvas-diff drawall` byte-identical (in-bounds
  no-op), studio.c compiles clean. `runtime/studio.c`
  (`sw_blit`, fast path ~`942-951`). The unscaled/axis-aligned fast path clamps only the
  *destination* (against clip + screen); the source read `srow[i] = src[(sy+j)*srcw + sx + i]`
  has **no bounds check against the sheet dims**. The slow path directly below gets this for free
  via `img_texel()` (returns blank on OOB). `sprf()` (`~3934`) feeds `sw_blit` `index/cols *
  SPRITE_SIZE` with **no clamp on `index`**, and `sspr()` passes cart coords straight through.
  → `spr(9999,0,0)`, an out-of-range `sspr`, or a `map()` tile beyond the sheet → heap over-read
  on the **default software canvas** (ADR-0024; and the *only* path on iOS/`DE_NO_RAYLIB`).
  **Fix:** clamp/skip the source rect against `spritesheet_img` width/height in the fast path
  (mirror what the slow path gets from `img_texel`). **Gate:** `canvas-diff.js drawall` (must stay
  identical — in-bounds sprites unaffected) + a probe cart drawing an out-of-range index without
  crashing.

- [x] **2. Effect-instance index OOB on the audio thread.** ✅ 2026-07-12 — `if (i<0||i>=*_INST)
  return;` added to `fx_set_tape/crush/drive/filter` (mirroring `fx_set_eq`); the `SR_FX_ORDER`
  decode clamps `inst` to `[0, TAPE_INST)`; `FX_INST` macro doc corrected to "0..1". Gates:
  `soundcheck` silent, `tune-check` PASS, `fx-check` output **byte-identical** to HEAD (no-op for
  valid instances). **Original ↓** The per-instance
  effect arrays have instance dimension **2** (`TAPE_INST`/`CRUSH_INST`/`FILT_INST`/`DRIVE_INST`,
  `~859+`), but:
  - the setters `fx_set_tape` (`~917`), `fx_set_crush` (`~948`), `fx_set_filter` (`~1118`),
    `fx_set_drive` (`~1000`) write `arr[b][i]` with **no clamp on `i`** — while `fx_set_eq`
    (`~1203`) *does* (`if (i<0||i>=EQ_INST) return;`), proving the omission;
  - `fx_order`'s `SR_FX_ORDER` decode (`~5489`) reads a 3-bit instance nibble (`(byte>>5)&7`,
    range 0..7) into `insert_inst[bus][s]` unclamped, and `apply_insert` then reads
    `tape_used[b][inst]` etc. **every sample**;
  - the public API (`tape_inst`/`crush_inst`/`filter_inst`/`drive_insert_inst`) forwards `instance`
    unclamped, and the **`FX_INST` macro doc (studio.h:677) advertises "instance 0..7"** — so a
    cart *following the docs* corrupts memory.
  → `tape_inst(2,…)` = OOB **write** on the audio thread; `FX_INST(FX_TAPE,2)` in `fx_order` = OOB
  **read** every sample. **Fix:** clamp `i` in the four setters (mirror `fx_set_eq`); clamp `inst`
  in the `SR_FX_ORDER` decode; correct the `FX_INST` macro doc + the public setter docs to the true
  range (0..1). **Gate:** `soundcheck` (silence) + `fx-check.js` (in-range renders unchanged).

- [x] **3. Netplay: unbounded frame index → OOB write + session-kill.** ✅ 2026-07-12 — `net_store`
  now rejects `frame < 0` (overflow) and `frame > net_frame + NET_RING` (poison); `net_input_pkt`
  bounds `f0` to the same window (so the `f0+i` loop can't overflow) and decodes with `(unsigned)`
  casts (kills the signed-shift UB). Gate: `net-check` PASS (echo + netdemo + relay). **Original ↓**
  `runtime/net.h`
  (`net_input_pkt` `~215-225`, `net_store` `~188`). `f0` is read from an **untrusted** packet and
  guarded only `>= 0`, never bounded against `NET_RING`/`net_frame`, then used as `frame % NET_RING`
  with a **signed** `frame`. A crafted/corrupt datagram where `f0 + i` overflows → negative index →
  OOB **write** of wire data into BSS. Even without overflow, a large valid `f0` poisons ring slots
  and — because `net_store` "never regresses" — **permanently** blocks the peer's real input →
  barrier timeout → `exit(1)` from one bad packet. Plus signed-shift UB at the decode
  (`b[6]<<24` into the sign bit; the seed decoders correctly cast `(unsigned)`, this one doesn't).
  **Fix:** bound `f0` to a sane window around `net_frame` before storing; add `(unsigned)` casts to
  the decode. **Gate:** `node tools/net-check.js`.

## 🟠 Medium — crash from reasonable input

- [ ] **4. `json.h` unbounded recursion → stack overflow.** `runtime/json.h` (`json_span` `~271`,
  `json_get` `~288`). jsmn's tokenizer is iterative (survives nesting), but the walk helpers recurse
  one C stack frame per nesting level. A deeply-nested external data file (`[[[[…]]]]`) crashes them —
  and json.h ingests **untrusted OSM data** (roadview). **Fix:** cap recursion depth (return -1 / stop
  past a limit) or walk iteratively. **Gate:** a probe parsing a deep-nested file returns cleanly.

- [ ] **5. `lay_wrap` integer div-by-zero (SIGFPE).** `runtime/lay.h:~112`. `lay_wrap(c, n=0, …)` →
  `lay_wrap_cols` clamps `cols → 0` (its `if (cols > n) cols = n`), then `rows = (n+cols-1)/cols` =
  `-1/0` → hard crash. Reachable by laying out a **dynamically-empty list**. (`lay_grid`/`lay_aspect`
  produce inf/NaN geometry on degenerate input — no crash, lower priority.) **Fix:** early-out
  `if (n < 1)` in `lay_wrap` (and guard `lay_wrap_cols` returning 0). **Gate:** `build-all.js` +
  a cart wrapping a zero-length list without crashing.

## 🟡 Low — latent / caller-controlled / benign

- [x] **6. Negative color → negative palette index** (OOB **read**, benign wrong color, UB). ✅
  2026-07-12 — all 24 `% PALETTE_SIZE` sites in studio.c changed to `& (PALETTE_SIZE-1)` (64 is a power
  of two → identical for valid colors, negatives wrap to a safe index); note added at the define.
  `canvas-diff drawall` byte-identical.
- [x] **7. `de_state()` unchecked `realloc` + `int` size.** ✅ 2026-07-12 — `bytes <= 0` early-out;
  `realloc` into a temp and keep the old block on OOM (no NULL `memset`, no leak).
- [x] **8. `load_bytes()` no `max` guard.** ✅ 2026-07-12 — added `if (max <= 0) return 0;`, symmetric
  with `save_bytes`.
- [ ] **9. `roadkit.h` missing capacity guards** (latent — masked by caller clamps today; relevant to
  the WIP B4 interchange work). `rk_make_junction` `conns[16]` overflows at ≥6 legs (`~361`);
  `rk_field_build` no clamp on `n>8` into `RK_MAXARM=8` arrays (`~138`); the spline emitters
  (`arc/clothoid/loop/scurve_spline`, `~182+`) have no buffer-size contract (clothoid can emit ~54
  points). **Fix:** `if (out->nConns >= 16) break;`, clamp `n` to `RK_MAXARM`, document/param the
  spline max point counts.
- [ ] **10. `radio.h` robustness** (cart-misuse). Empty-chair div-by-zero in `rad_band_input`
  (`% ncand` when `ncand==0`); `rad_chair` has no `RAD_MAXCHAIR` (6) guard → a 7th chair OOB-writes
  the fixed array. Add both guards.
- [ ] **11. Misc caller-controlled div-by-zero / OOB / NULL.** `shadermath.h` `op_smin(k=0)` → NaN;
  `radio.h` `rad_srnd(n=0)`; `ampcab.h` `ampcab_apply` unbounded voicing index; `raylib_compat.c`
  `MeasureTextEx` NULL-deref on an unloaded font; `lay_grid`/`lay_aspect` inf/NaN geometry. All need
  a degenerate-input floor; none currently reachable via a documented path.
- [ ] **12. `sound.h` benign non-atomic cross-thread reads.** `sound_bpm` (`~2297`) and `ctx_overflow`
  (`~1927`) are written on the audio thread and read on the main thread without synchronization —
  worst case one frame of stale value, never a crash. Note only; not worth churn.

---

## Audited clean (for the record)

`ui.h` / `gestures.h` / `pointer.h` / `cursor.h` / `game_rect.h` (finger/contact ids are
linear-searched, **never** used as array indices — structurally eliminates the whole "id indexed
against pool size" bug class), `worldnet.h`, `citygen.h` (for its live callers), `keybed.h`,
`solo.h`, `improv.h`, `midi_input.h`, and the studio.c lifecycle surface (per-cart save-dir path
building, `watch`, `key`/`keyp`/`keyr`/injection guards, `mget`/`mset`, the WAV writer, CLI argv
parse) — all genuinely defensive.
