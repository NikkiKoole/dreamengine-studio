# Engine simplification backlog — duplication, missing helpers, naming

> **STATUS: ready to build** (2026-07-09). A punch-list of *quality-only*
> cleanups in the engine core (`runtime/studio.c`, `runtime/sound.h`) and the
> library headers. Every item is a **behaviour-preserving** extraction or rename —
> no feature work, no bug fixes. Found by a three-way read-only review; none of it
> is rot (no dead `#if 0`, no abandoned code), it's the same idioms hand-inlined
> in many places. Pick items independently; each names the gate that proves it.

---

## How to work this list

- These are **pure refactors** — prove each with the matching deterministic gate,
  no eyeballing needed:
  - `runtime/studio.c` primitives (draw/software raster) → `node tools/canvas-diff.js <cart>`
    (A/Bs GPU vs software + pixel-diffs; `drawall` exercises every primitive) and
    `node tools/build-all.js` (all carts still compile).
  - `runtime/sound.h` → the audio gates per CLAUDE.md: `soundcheck` for queues,
    `node tools/tune-check.js --quiet` for pitched engines, `level-check.js` /
    `fx-check.js` for levels/effects. Re-bless an *intended* change with `--save`.
  - Header widgets (`ui.h`/`radio.h`/`solo.h`) → `node tools/ui-audit.js`, plus
    bake + eyeball an affected cart.
- **`sound.h` and `studio.h` are hot shared files** — targeted `Edit`s only, never
  full-file `Write`; re-`Read` the region right before editing; confirm your change
  survived the commit (see CLAUDE.md "Hot shared source files").
- Checkbox each item as it lands. Group A (tiny helpers) is the highest
  leverage-per-risk — start there.

---

## Group A — missing tiny helpers (highest leverage, lowest risk)

The same 3–4-line idioms are hand-inlined dozens of times. One helper collapses
20–40 sites and documents intent. All pure extractions.

- [ ] **`clamp01` / `clampf` in `sound.h`.** `if (x<0) x=0; if (x>1) x=1;` (crammed
  on one line, reads like a bug) appears **40+ times** in `sound_fire_req`
  (`sound.h:4884, 4987, 4995, 5008, 5014, 5021, 5058, 5093, 5096, 5128, 5129, 5139,
  5161–5163, 5174, 5177, …`). Add `clamp01(v)` / `clampf(lo,hi,v)` and use everywhere.
  The macro-clamp at `sound.h:5675–5677` (harm/timb/mor) re-evaluates the same sum
  3× per branch — a local + `clamp01` makes each one readable line.
- [ ] **`studio.c` clamps** — `fade` (`5373`), `touch_layout` button clamp (`3320`),
  `camera_ex` zoom guard (`4446`) reimplement the existing `clamp`/`clampi`/`mid`.
  Use the helpers that already exist.
- [ ] **`voice_white(Voice *v)` in `sound.h`.** The LCG noise draw
  `noise_state = noise_state*1103515245 + 12345 & 0x7fffffff; ((noise_state>>16)&0xff)/127.5f - 1`
  is copy-pasted **~20×** (`sound.h:2331, 2427, 2635, 2913, 2988, 3014, 3055, 3158,
  3194, 3277, 3284, 3326, 3420, 3472, 3884, 3909, 3991, 4076, 4180, 4325, …`). One
  helper unifies them and documents the (currently silent) 8-bit / 256-level quirk.
- [ ] **`dc_block(&x1,&y1,in)` in `sound.h`.** The one-pole DC-blocker
  `out = in - x1 + 0.999f*y1; x1 = in; y1 = out;` is inlined ~6× (echo `5834`,
  flanger `755`, phaser, drive `5734`, guitar `4064`, reed/pipe). **Two sites use a
  different R (0.990 vs 0.999)** — a helper makes that an explicit parameter instead
  of a silent drift.
- [ ] **Named audio constants in `sound.h`.** `6.2831853f` (TWO_PI) appears **37×**
  as a literal *and* is redefined as a local `const float TWO_PI` in 4 engines
  (reed/pipe/bowed/brass, `sound.h:3036, 3180, 3313, 3450`); `3.14159265f` 13× (all
  `PI*cutoff/rate` filters), `0.78539816f` (π/4) 2×, `6.9078f` (=ln 1000) 4×. Define
  file-level `SOUND_PI` / `SOUND_TWO_PI`, drop the 4 inconsistent locals.
- [ ] **`DEG2RAD` consistency in `studio.c`.** `gradient()` (`5049–5050`) hand-writes
  `angle_deg * 3.14159265f / 180.0f` and `sw_rot_composite` (`4415`) uses the raw
  literal `0.01745329252f`; the rest of the file uses `DEG2RAD`. Use it here too.
- [ ] **`ROT_QUANT` constant in `studio.c`.** The determinism idiom
  `roundf(x*4096.f)/4096.f` with bare `4096.f` appears in `de_cpu_img_rot` (`3650`)
  and `de_cpu_rectfill_rot` (`3920`). Name it (`#define ROT_QUANT 4096.f` + a
  `quantize()` inline) so the constant lives in one place.
- [ ] **Named slew constants in `sound.h`.** Per-param slew block (`5567–5581`) uses
  bare `0.003 / 0.0015 / 0.002` repeated 14× — name them (`SLEW_FAST`/`SLEW_MED`/
  `SLEW_MACRO`).

---

## Group B — one point-in-rect, shared across the tree

- [ ] **Unify point-in-rect.** Reimplemented in four headers with subtly different
  types/bounds: `studio.h:783 point_in_box` (int), `lay.h:128 binside` (float),
  `ui.h:202 ui_in` (int, `<` bounds), and an inline copy in `gestures.h:114–116`.
  At minimum `gestures.h` should reuse one; ideally document/unify the named variants.

---

## Group C — `studio.c` primitive twins (biggest structural win; needs canvas-diff)

Two full implementations kept in sync by hand. Touch the software rasterizer —
run `canvas-diff` (and `mirror-diff` where symmetry applies) before/after.

- [ ] **`sw_sline`/`sw_plot_minor` (`774`) vs `de_cpu_line`/`de_cpu_plot_minor`
  (`4242`)** are verbatim copies of the same reflection-symmetric DDA, differing
  *only* by `sw_pset` vs `pset` (~30 lines; comment at `4228` admits it).
  Parameterize one over a plot callback (or `#define`-templated body).
- [ ] **`circfill` (`4057`) should delegate to `ovalfill` (`5389`)** — a circle is
  `rx==ry`, and `circfill_pat` (`4002`) already delegates to `ovalfill_pat`. ~25 lines
  vanish. (`disc_inside` at `4017` is likewise `ellipse_inside` with `rx==ry`.)
- [ ] **`outline_ring(bbox, inside_predicate, color)`** — the "pixel is inside AND a
  4-neighbour is outside" test is copy-pasted **6×**: `circ` (`4048`), `oval`
  (`5422`), `rrect` (`5003`), `poly_stroke_cov` (`4887`), `sector_draw` stroke
  (`4161`), `thick_draw` stroke (`4979`). One helper over a function pointer / macro
  collapses all six.
- [ ] **Retire the soak-period scaffolding.** `sw_tritex_legacy` (`897`, TODO at
  `896`) and `poly_fill_cov_legacy` (`4806`, TODO at `4805`) plus their runtime
  toggles (`blit_fast_on`, `tritex_fast_on`, `disc_fill_fast`, `poly_fill_fast`,
  `clamp_cache_on`, `pset_batch`; declared `253–272`, parsed `2771–2782`) are
  self-labelled switches — once trusted, deleting the legacy paths + flags removes a
  lot of duplicated primitive code.

---

## Group D — `sound.h` near-identical engines & FX

Prove with `tune-check` (pitched) / `fx-check` / `level-check`.

- [ ] **Filter pairs share a core.** `sound_ladder` (`4424`) vs `sound_diode`
  (`4483`) are ~90% identical (differ by one `tanhf` on feedback + tap stage 3 vs 4);
  same for `sound_svf` (`4399`) vs `sound_steiner` (`4454`). Share a core with a
  flag/param instead of two full copies.
- [ ] **`sound_choke_group(cmask)`** — the choke-group voice-steal block is
  duplicated verbatim 3× (`sound.h:4836, 4851, 5423`: `SR_NOTE`, `SR_NOTE_ON`,
  `SR_HIT_AT`).
- [ ] **Karplus tap-read + T60→feedback helper.** `ks_tap_read` (fractional read +
  wrap + lerp) and `fb = expf(-6.9078f/(t60*f))` are duplicated between
  `sound_engine_sample` (`4351–4373`) and `sound_guitar_sample` (`4039–4057`), and
  the tap-read reappears in the echo send (`5823–5828`). Extract `ks_tap_read` /
  `t60_to_fb`.
- [ ] **`ks_seed_bore(v, targetLen, noiseScale)`** — bore/string delay-line seeding
  (size ×2.5, cap at `SOUND_KS_MAX-1`, floor 4, seed noise, set `initfreq`) is
  copy-pasted across reed/pipe/bowed/brass (`sound.h:3008, 3143, 3254, 3415`),
  comments and all; only the noise scale differs.
- [ ] **Modulated-delay skeleton (chorus/flanger/tape).** Each re-writes phase
  advance, triangle LFO, delay-in-samples clamp, `moddel_hermite` read, and the
  `wet/dry` blend by hand (`sound.h:697`, `741`, …). The blend line
  `*mixL = dry*(1-mix)+wet*mix` recurs in nearly every `*_process` (`716, 762, 1669`)
  — a one-liner helper.
- [ ] **Master/instrument FX setter pairs.** ~10 near-identical `SR_X`/`SR_INSTR_X`
  copy-paste blocks (chorus `5215`, flanger `5221`, tape `5227`, wah `5233`, …), plus
  a copy-pasted "auto-place kind in `insert_order`" scan (`5158, 5189–5191,
  5199–5201`). Factor both into helpers.

---

## Group E — table-driven instead of parallel switches / if-ladders

- [ ] **`sound_fire_req` → `switch`.** It's a ~690-line `else if` chain (`sound.h:4816`)
  on a dense `SoundReqKind` enum — a textbook `switch` (jump table + unhandled-case
  warning). Many arms are literally `decode (a/1000) → clamp → store`; a small helper
  given #1's `clamp01`.
- [ ] **Engine dispatch → `switch`/table.** `sound_engine_sample` (`sound.h:4334`)
  tests `v->wave` against 13 constants sequentially before the Karplus fall-through.
- [ ] **`btn_local` keymap table.** Two near-identical 8-case switches (`studio.c:3209`
  player 0, `3219` player 1) → `static const int keymap[2][BTN_COUNT]` + one
  `inp_down(keymap[player][button])`.
- [ ] **Font table.** Font load (`studio.c:2962–3000`), unload (`3092–3096`), and the
  two lookup if-chains `cur_font()`/`cur_font_img()` (`3742–3755`) repeat the same
  per-font work 3 times over. A `{Font*, Image*, data, len, keyColor}` table indexed
  by font id drives all of it.
- [ ] **`env_is(name, val)` helper.** 9 near-identical `getenv + strcmp` blocks
  (`studio.c:2771–2790`).

---

## Group F — smaller drift-risk items

- [ ] **`studio.c` shared bodies:** `pget` (`4326`) / `pget_rgb` (`4348`) share the
  world→snapshot-texel guard machinery; the palette-index match loop is duplicated in
  `pget` (`4339`) and `sget` (`4366`) → `palette_index_of(DeColor)`; `harness_inspect`
  state-request (`1912–1930`) re-implements `harness_trace`'s watch-serialization
  (`1783–1798`) → `write_state_json()`; `MAP_DATA` load duplicated in `de_init`
  (`2623`) and `main` (`2940`) → `de_load_map()`; `sprf` builds unused `Rectangle`
  src/dst on the software path (`3601–3616`).
- [ ] **Dead `PAUSE_KEY` guard** — redefined at `studio.c:3173–3175` but already
  defined at `455–457`, so the second `#ifndef` is always skipped. Delete.
- [ ] **`net.h` packet helpers** — WELCOME/HELLO builder + seed LE encode/decode
  pasted ~4× (`437, 447, 460, 478, 519, 527, 711`). Add `net_welcome_pkt()` /
  `net_seed_decode()` + a `NET_HELLO` constant.
- [ ] **`ui_button_core()`** — `ui_button` (`ui.h:604–628`) and
  `ui_spr_button_styled` (`643–667`) copy-paste the whole capture/press/activate/focus
  machinery (`610–621` ≈ `650–660`); `solo.h:253–302` (4 buttons) and `radio.h` then
  reimplement it a 3rd/4th time. Also the widget-id hash magic
  (`(void*)(uintptr_t)(0x10000001u + x*131071 + …)`) is pasted at `ui.h:605`/`644` →
  `ui_wid_hash(seed, x,y,w,h)`.
- [ ] **`radio.h` redundancy** — `rad_knob` (`327`) is `rad_knob_face(…, hot=false)`;
  `rad_knob_drag` (`357–372`) re-derives `ui_knob`'s drag physics (`ui.h:578–586`);
  `rad_iabs` (`33`) duplicates `studio.h abs`, `lay_clamp` (`lay.h:43`) duplicates
  `clamp`; `rad_footer` (`465–468`) is a documented no-op to remove once callers are
  gone; `rad_knob_int`/`rad_knob_sel`/`rad_knob_float` (`375–424`) are three wrappers
  that differ only in the value↔t map.
- [ ] **`radio.h` coord `#define`s** — panel/button coordinates are duplicated between
  draw and hit-test (`?`-button `288,172,81` at `289` + `453`; B-button `32,172,81` at
  `534` + `460`; panel `44,40,232,122` in both help + band panels). **The clickable
  area can drift from the drawn one.** Hoist to `#define`s.
- [ ] **`keybed.h` MIDI formula helpers** — `base + (i/7)*12 + KB_WSEMI[i%7]` recurs
  ~5× (`171, 174, 182, 200, 262`) and the black variant ~4× (`171, 188, 202, 272`);
  `keybed_midi_at`/`keybed_draw` recompute inline instead of calling
  `keybed_white_midi()` / a matching `kb_black_midi(i)`.
- [ ] **`worldnet.h`** — `nearest_hub`/`nearest_town` (`280–304`) are the same 5×5
  search with divergence risk (share a `nearest_present(getter, …)`); stale
  `(void)ux;(void)uy;` at `217–218` (both used on the line above). Delete the casts.

---

## Doc-only fix (spotted during the review)

- [ ] **`studio.h:401` env-count doc bug** — says mod-envelopes are "2 per slot (which
  0..1)" but `note_env` (`330`) and `instrument_env` (`408`) both say `which (0..2)`.
  One is wrong; reconcile against the `sound.h` implementation. (Also `INSTR_VOICE 24`
  at `312` is declared after `INSTR_PIPE 25` at `311` — breaks the ascending block;
  cosmetic.)
