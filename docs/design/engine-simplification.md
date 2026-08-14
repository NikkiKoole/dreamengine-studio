# Engine simplification backlog — duplication, missing helpers, naming

> **STATUS: ROUND 2 OPEN** (2026-08-14). Round 1 below is closed 33/33 and its ❌ won't-do calls were
> RE-VERIFIED this round, not assumed — see [Round 2](#round-2--after-the-per-instance-refactor-2026-08-14).
>
> ⚠ Round 1's STATUS line is preserved verbatim below for the trail.

> **STATUS (round 1): DONE** (2026-07-09; last landed 2026-07-12) —
> **33/33 closed, 0 open.** The bare "8 left" the checkbox count used to show was misleading. Of the 33
> closed: **28 landed** as behaviour-preserving refactors (incl. the final three header-dedups — `net.h`
> packet helpers, `ui_button_core()`/`ui_wid_hash()`, `radio.h` `rad_knob`/`rad_iabs`/`rad_footer` — landed
> 2026-07-12, each gated by `build-all` + `ui-audit`/`net-check`); **3 were assessed and deliberately left
> as-is** (❌ won't-do — a per-pixel indirect call or a float-rounding byte-mismatch would make them a
> perf/correctness regression, not a win; see their ⚠ notes); **2 landed their doable part** and parked a
> byte-unsafe or trace-only tail (bucket ②). Several sub-items inside the header-dedups were themselves
> left-as-is for byte-safety (noted per item: solo.h/radio.h button focus semantics, typed knob wrappers,
> lay_clamp include-safety). (Checkbox semantics: `[x]` = *closed* — landed, won't-do, or doable-part-done —
> not necessarily a refactor shipped; the per-item note says which.)
> A punch-list of *quality-only*
> cleanups in the engine core (`runtime/studio.c`, `runtime/sound.h`) and the
> library headers. Every item is a **behaviour-preserving** extraction or rename —
> no feature work, no bug fixes. Found by a three-way read-only review; none of it
> is rot (no dead `#if 0`, no abandoned code), it's the same idioms hand-inlined
> in many places. Pick items independently; each names the gate that proves it.

---

## How to work this list

- **Line numbers are a snapshot** (re-anchored 2026-07-09 after the netplay +
  `instrument_unison` commits; `studio.c` shifted ~+107 below line 900, `sound.h`
  ~+9–34 by region). They rot on the next `studio.c`/`sound.h` commit — treat the
  **function name** as the real anchor and grep the quoted idiom, not the number.
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

- [x] **`clamp01` / `clampf` in `sound.h`.** `if (x<0) x=0; if (x>1) x=1;` (crammed
  on one line, reads like a bug) appears **40+ times** throughout `sound_fire_req`
  (`sound.h:4832`+; grep `if (x < 0` for the crammed one-line form). Add `clamp01(v)`
  / `clampf(lo,hi,v)` and use everywhere. The macro-clamp for harm/timb/mor
  (`sound.h:5709`+) re-evaluates the same sum 3× per branch — a local + `clamp01`
  makes each one a readable line.
- [x] **`studio.c` clamps** — `fade` (`5480`), `touch_layout` button clamp (`3427`),
  `camera_ex` zoom guard (`4574`) reimplement the existing `clamp`/`clampi`/`mid`.
  Use the helpers that already exist.
- [x] **`voice_white(Voice *v)` in `sound.h`.** The LCG noise draw
  `noise_state = noise_state*1103515245 + 12345 & 0x7fffffff; ((noise_state>>16)&0xff)/127.5f - 1`
  is copy-pasted **~20×** (grep the `1103515245` LCG noise draw across the engines,
  e.g. `sound.h:4188, 4333`). One helper unifies them and documents the (currently
  silent) 8-bit / 256-level quirk.
- [x] **`dc_block(&x1,&y1,in)` in `sound.h`.** The one-pole DC-blocker
  `out = in - x1 + 0.999f*y1; x1 = in; y1 = out;` is inlined ~6× (echo `5892`,
  flanger `761`, drive `920`/`5778`, guitar `4073`, reed/pipe). **Two sites use a
  different R (guitar `4073` uses 0.990, the rest 0.999)** — a helper makes that an
  explicit parameter instead of a silent drift.
- [x] **Named audio constants in `sound.h`.** `6.2831853f` (TWO_PI) appears **37×**
  as a literal *and* is redefined as a local `const float TWO_PI` in 4 engines
  (reed/pipe/bowed/brass, `sound.h:3045, 3189, 3322, 3459`); `3.14159265f` 13× (all
  `PI*cutoff/rate` filters), `0.78539816f` (π/4) 2×, `6.9078f` (=ln 1000) 4×. Define
  file-level `SOUND_PI` / `SOUND_TWO_PI`, drop the 4 inconsistent locals.
- [x] **`DEG2RAD` consistency in `studio.c`.** `gradient()` (`5156–5157`) hand-writes
  `angle_deg * 3.14159265f / 180.0f` and `sw_rot_composite` (`4522`) uses the raw
  literal `0.01745329252f`; the rest of the file uses `DEG2RAD`. Use it here too.
- [x] **`ROT_QUANT` constant in `studio.c`.** The determinism idiom
  `roundf(x*4096.f)/4096.f` with bare `4096.f` appears in `de_cpu_img_rot` (`3757`)
  and `de_cpu_rectfill_rot` (`4027`). Name it (`#define ROT_QUANT 4096.f` + a
  `quantize()` inline) so the constant lives in one place.
- [x] **Named slew constants in `sound.h`.** Per-param slew block (`5599–5601`) uses
  bare `0.003 / 0.0015 / 0.002` repeated 14× — name them (`SLEW_FAST`/`SLEW_MED`/
  `SLEW_MACRO`).

---

## Group B — one point-in-rect, shared across the tree

- [x] **Unify point-in-rect.** Reimplemented in four headers with subtly different
  types/bounds: `studio.h:787 point_in_box` (int), `lay.h:128 binside` (float),
  `ui.h:202 ui_in` (int, `<` bounds), and an inline copy in `gestures.h:114–116`.
  At minimum `gestures.h` should reuse one; ideally document/unify the named variants.
  **Done (the "at minimum"):** `gestures.h swiped_in` now calls `point_in_box` (exact
  same half-open int test). The float `binside`/`ui_in` variants are genuinely
  different-typed and left distinct on purpose — not worth forcing into one.

---

## Group C — `studio.c` primitive twins (biggest structural win; needs canvas-diff)

Two full implementations kept in sync by hand. Touch the software rasterizer —
run `canvas-diff` (and `mirror-diff` where symmetry applies) before/after.

- [x] **`sw_sline`/`sw_plot_minor` (`775`/`768`) vs `de_cpu_line`/`de_cpu_plot_minor`
  (`4349`/`4342`)** are verbatim copies of the same reflection-symmetric DDA, differing
  *only* by `sw_pset` vs `pset` (~30 lines; comment at `4335` admits it).
  Parameterize one over a plot callback (or `#define`-templated body).
  **❌ WON'T-DO (assessed 2026-07-10): leave as-is.** The only difference is *which plot fn is called
  per pixel*, in a per-pixel rasterizer hot path. A plot **callback** is an un-inlinable
  indirect call per pixel → real perf regression on the line path. A `#define`-templated
  body is byte-identical + perf-neutral but is macro soup for ~30 lines of stable code —
  not worth it. Honest duplication wins here.
- [x] **`circfill` (`4164`) should delegate to `ovalfill` (`5496`)** — a circle is
  `rx==ry`, and `circfill_pat` (`4109`) already delegates to `ovalfill_pat`. ~25 lines
  vanish. (`disc_inside` at `4124` is likewise `ellipse_inside` with `rx==ry`.)
  **❌ WON'T-DO (assessed 2026-07-09): NOT byte-safe — leave as-is.** `disc_inside` is *exact*
  (`dx = int+0.5` is a half-integer, so `dx²+dy² ≤ r²` never rounds), but
  `ellipse_inside` divides first (`dx/r`) then squares — which rounds — so a boundary
  pixel can flip. Delegating would shift a few edge pixels on some circles, and also
  change negative-radius behaviour (circfill draws nothing; ovalfill abs's it) and drop
  the `UIAUDIT('c')` marker. The math is equal; the floats aren't.
- [x] **`outline_ring(bbox, inside_predicate, color)`** — the "pixel is inside AND a
  4-neighbour is outside" test is copy-pasted **6×**: `circ` (`4143`), `oval`
  (`5524`), `rrect` (`5104`), `poly_stroke_cov` (`4990`), `sector_draw` stroke
  (`4260`), `thick_draw` stroke (`5076`). One helper over a function pointer / macro
  collapses all six.
  **❌ WON'T-DO (assessed 2026-07-10): leave as-is.** Same hot-path tension as the line twin, worse:
  the inside-predicate is called **5× per pixel** across an O(r²) bbox, so a
  function-pointer helper would likely make outlines *slower*. The maker already parked
  the O(perimeter) span rewrite of these as low-leverage (see the `circ` comment +
  `docs/guides/engine-optimization.md` → "Outline strokes (parked)"). Not worth a macro.
- [x] **Retire the soak-period scaffolding.** `sw_tritex_legacy` (`898`, TODO at
  `897`) and `poly_fill_cov_legacy` (`4913`, TODO at `4912`) plus their runtime
  toggles (`blit_fast_on`, `tritex_fast_on`, `disc_fill_fast`, `poly_fill_fast`,
  `clamp_cache_on`, `pset_batch`; declared `253–272`, parsed `2849–2860`) are
  self-labelled switches — once trusted, deleting the legacy paths + flags removes a
  lot of duplicated primitive code. **Done 2026-07-10 for the four that soaked default-on
  2–3½ weeks:** `poly_fill_fast` (+ `poly_fill_cov_legacy`), `disc_fill_fast`,
  `blit_fast_on`, `clamp_cache_on` — flags, env toggles, and legacy fork/body removed;
  the fast paths are now unconditional (their in-path fallbacks for rotation/dither/
  recolor/pathological remain — those aren't scaffolding). Default render unchanged
  (canvas-diff `drawall` byte-for-byte vs HEAD). **Kept:** `tritex_fast_on` +
  `sw_tritex_legacy` (only ~1wk soak — still A/B-able) and `pset_batch` (NOT a soak flag
  — a per-platform default: `DE_BATCH_PSET_DEFAULT` is 0 native / 1 web, so retiring it
  would change platform behaviour, not remove dead code).

---

## Group D — `sound.h` near-identical engines & FX

Prove with `tune-check` (pitched) / `fx-check` / `level-check`.

- [x] **Filter pairs share a core.** `sound_ladder` (`4433`) vs `sound_diode`
  (`4492`) are ~90% identical (differ by one `tanhf` on feedback + tap stage 3 vs 4);
  same for `sound_svf` (`4408`) vs `sound_steiner` (`4463`). Share a core with a
  flag/param instead of two full copies. **Done:** `svf_step(v,in,cut,nl_res)` and
  `ladder_core(v,in,cut,diode)`; each wrapper passes a literal flag so `static inline`
  const-folds the branch (no per-sample cost). Proven byte-identical by `filter-spec`
  (all four modes low/ladder/steiner/diode identical mine-vs-HEAD).
- [x] **`sound_choke_group(cmask)`** — the choke-group voice-steal block is
  duplicated verbatim 3× (`sound.h:4849, 4863, 5452`: `SR_NOTE`, `SR_NOTE_ON`,
  `SR_HIT_AT`). **Done:** `sound_choke_group(int instr_slot)` computes the cmask *and*
  runs the steal loop (the cmask calc was duplicated too), so all three sites are one call.
- [x] **Karplus tap-read + T60→feedback helper.** `ks_tap_read` (fractional read +
  wrap + lerp) and `fb = expf(-6.9078f/(t60*f))` are duplicated between
  `sound_engine_sample` (`4342`+; `fb` at `4372`) and `sound_guitar_sample` (`4043`+;
  `fb` at `4053`), and the tap-read reappears in the echo send (`~5890`). Extract
  `ks_tap_read` / `t60_to_fb`. **Done** for the two Voice-based sites (guitar + modal
  engine). The echo send uses the same shape on `echo_buf` (different buffer + wrap) —
  not folded into the `Voice*`-typed helper.
- [x] **`ks_seed_bore(v, targetLen, noiseScale)`** — bore/string delay-line seeding
  (size ×2.5, cap at `SOUND_KS_MAX-1`, floor 4, seed noise, set `initfreq`) is
  copy-pasted across reed/pipe/bowed/brass (`sound.h:3019, 3163, 3263, 3424`),
  comments and all; only the noise scale differs. **Done — scoped narrower than the
  above claim.** The shared core is only *cap + floor + seed-noise*; the ×2.5 source
  (pipe adds `+2`) and the `initfreq` formula differ per engine, so those stay inline.
  `ks_seed_bore(v, len, scale)` clamps to `[4, KS_MAX-1]`, seeds, returns the clamped
  len — used by reed/pipe/brass. **Bowed is NOT folded in** (dual delay line + pizz/arco
  branch — structurally different, not "only the noise scale").
- [x] **Modulated-delay skeleton (chorus/flanger/tape).** Each re-writes phase
  advance, triangle LFO, delay-in-samples clamp, `moddel_hermite` read (the shared
  read at `sound.h:624`), and the `wet/dry` blend by hand (chorus `720`, flanger
  `758`, …). The blend line `dry*(1-mix)+wet*mix` recurs in nearly every `*_process`
  (e.g. `1266`) — a one-liner helper. **Partial: the blend one-liner is done** —
  `mix_wet(dry, wet, mix)` collapses the 9 blend sites. The full phase/LFO/clamp/read
  *skeleton* is left: chorus/flanger/tape differ in buffer length, stereo taps and mod
  depth, so a mechanical unify is not byte-safe — **skeleton parked (bucket ②); item closed on the blend win.**
- [x] **Master/instrument FX setter pairs.** ~10 near-identical `SR_X`/`SR_INSTR_X`
  copy-paste arms in `sound_fire_req` (dispatch guarded at `sound.h:1883–1906`), plus
  a copy-pasted "auto-place kind in `insert_order`" scan (`5156–5157`, `5189–5190`).
  Factor both into helpers. **Done (the SR_INSTR_* preamble):** `fx_instr_bus(slot)`
  (validate slot → resolve/allocate aux bus, or -1) collapses the `if(slot bad)return;
  int b=fx_bus_for` 2-liner at all 20 arms; the `if (b >= 1)` guard is unchanged, so
  byte-identical (verified all 20 arms guarded; soundcheck 900f clean). The 2-line
  `insert_order` scan (only 2 sites) left inline — below the dedup threshold.

---

## Group E — table-driven instead of parallel switches / if-ladders

- [x] **`sound_fire_req` → `switch`.** It's a ~690-line `else if` chain (`sound.h:4832`)
  on a dense `SoundReqKind` enum — a textbook `switch` (jump table + unhandled-case
  warning). Many arms are literally `decode (a/1000) → clamp → store`; a small helper
  given #1's `clamp01`. **Done:** mechanical
  transform of the 121-arm chain → `switch (r.kind)` — only the boundary lines changed
  (each `} else if (r.kind == SR_X) {` → `} break; case SR_X: {`), bodies + their
  indentation untouched. Byte-identical: compiles with no dup-case (so no dup SR_ ids),
  soundcheck 900f clean, tune-check + fx-check unchanged, filter-spec byte-for-byte vs
  pre-switch HEAD. The `-Wswitch` net now catches an unhandled new SR_*. (The suggested
  per-arm decode→clamp→store helper is a separate follow-up — not done here.)
- [x] **Engine dispatch → `switch`/table.** `sound_engine_sample` (`sound.h:4342`)
  tests `v->wave` against 13 constants sequentially before the Karplus fall-through.
  **Done:** `switch (v->wave)` over the 13 engine ids (dense → jump table), Karplus
  string preserved as the fall-through past the switch. Per-sample hot path, so a small
  perf win too. Byte-identical — tune-check all modeled engines unchanged vs baseline.
- [x] **`btn_local` keymap table.** The near-identical 8-case per-player switches in
  `btn_local` (`studio.c:3298`+) → `static const int keymap[2][BTN_COUNT]` + one
  `inp_down(keymap[player][button])`.
- [x] **Font table.** Font load (`studio.c:3070–3095`), unload (`~3105`), and the
  two lookup if-chains `cur_font()`/`cur_font_img()` (`3849`/`3856`) repeat the same
  per-font work 3 times over. A `{Font*, Image*, data, len, keyColor}` table indexed
  by font id drives all of it. **Done:** `FONT_SLOT[]`/`FONT_IMG[]` (font id → Font/CPU
  image) + a local `FONT_SRC[]` (data/len/first_char) now drive the load loop, the
  sw_print CPU-copy loop, `cur_font()`/`cur_font_img()` (one index each), and the unload
  loop — five per-font blocks → four short loops. Iterated in id order = same calls/args
  as before → byte-identical (fonts demo renders identical sha vs HEAD; build-all 479/479).
- [x] **`env_is(name, val)` helper.** 9 near-identical `getenv + strcmp` blocks
  (`studio.c:2849–2867`). (The 10th, `DE_SHOW_SIZE`, is a "set & not 0" test, not an
  `==`-match — left inline.)

---

## Group F — smaller drift-risk items

- [x] **`studio.c` shared bodies:** `pget` (`4433`) / `pget_rgb` (`4455`) share the
  world→snapshot-texel guard machinery; the palette-index match loop is duplicated in
  `pget` and `sget` (`4466`) → `palette_index_of(DeColor)`; `harness_inspect`
  state-request (`1931`+) re-implements `harness_trace`'s watch-serialization
  (`1814`+) → `write_state_json()`; `MAP_DATA` load duplicated in `de_init`
  (`2689`) and `main` (`3046`) → `de_load_map()`; `sprf` builds unused `Rectangle`
  src/dst on the software path (`3708`+).
  **Mostly done 2026-07-10:** `pget_texel()` (the shared world→snapshot-texel guard, each
  caller keeps its own empty sentinel), `palette_index_of(DeColor)` (pget+sget),
  `de_load_map()` (de_init + main — note it lives BEFORE the `#ifdef DE_NO_RAYLIB` guard so
  both entry points see it), and `sprf` now builds src/dst only on the GPU branch (dead on
  the sw path). Byte-identical (build-all 479/479; canvas-diff drawall unchanged, boom
  sprites 0px). **Left (parked):** `harness_inspect`→`write_state_json()` — trace-only (`-DDE_TRACE`),
  parked in the bucket-② set with a `play.js --trace` gate. **Item closed on the 4/5 that landed.**
- [x] **Dead `PAUSE_KEY` guard** — redefined at `studio.c:3280–3281` but already
  defined at `455–456`, so the second `#ifndef` is always skipped. Delete.
- [x] **`net.h` packet helpers** — WELCOME/HELLO builder + seed LE encode/decode
  pasted ~4× (constants `NET_PKT_HELLO`/`NET_PKT_WELCOME` at `86–87`).
  **Done 2026-07-12:** `net_hello_pkt()` (3×), `net_welcome_pkt()` (seed-LE builder, 3×),
  `net_seed_decode()` (2×) as `static inline` in the transport-agnostic core, shared by
  the web DataChannel + native UDP paths. Byte-identical (inline = no unused-symbol
  warning when one transport is built). Also fixed `net-check.js` crashing on trace
  frames with no `w` block so the gate runs; net-check PASS (echo + netdemo + relay).
- [x] **`ui_button_core()`** — `ui_button` (`ui.h`) and `ui_spr_button_styled`
  copy-paste the whole capture/press/activate/focus machinery, differing only by the
  widget-id seed. **Done 2026-07-12:** `ui_button_core(wid,x,y,w,h,&focused,&pressed,&hot)`
  + `ui_wid_hash(seed,x,y,w,h)`; each button computes its seed and draws its own face.
  Byte-identical (the seed hash commutes); build-all 493/493. **Left as-is (byte-safety):**
  `solo.h`/`radio.h` use a distinct NON-focusable in-place button pattern (`ui_reg(…,0)`,
  `&static_id` identity, immediate side-effects) — folding them onto the core would add
  focus/A-activate + reorder focus nav, a behaviour change, not a pure refactor.
- [x] **`radio.h` redundancy** — **Done 2026-07-12:** `rad_knob` (readout meter) now
  delegates to `rad_knob_face(…, hot=false)` (byte-identical); `rad_iabs` kept as a thin
  alias over `abs()` (delegates — ~6 station carts call it, so a full removal is pure cart
  churn); dead `rad_footer` (zero callers) removed. build-all 493/493. **Left as-is:**
  `rad_knob_drag` vs `ui_knob` (ui_knob is a focusable monolith — unifying changes focus/nav
  semantics); `rad_knob_int`/`_sel`/`_float` (typed int/int/float wrappers — unifying needs
  macro/void* soup); `lay_clamp` in `lay.h` (lay.h does NOT include studio.h, so `clamp`
  isn't guaranteed available — delegating would add an unsafe dependency).
- [x] **`radio.h` coord `#define`s** — panel/button coordinates are duplicated between
  draw and hit-test (`?`-button `288,172,81` at `289` + `453`; B-button `32,172,81` at
  `534` + `460`; panel `44,40,232,122` in both help + band panels). **The clickable
  area can drift from the drawn one.** Hoist to `#define`s. **Done:** `RAD_HELP_BTN_X`/
  `RAD_BAND_BTN_X`/`RAD_BTN_Y`/`RAD_BTN_HIT_R2`/`RAD_PANEL_X/Y/W` — centres + panel box
  shared by draw and hit-test (draw r=6 vs hit r=9 left as the intentional fat-finger
  pad). Pure literal→macro → byte-identical; build-all 479/479.
- [x] **`keybed.h` MIDI formula helpers** — `base + (i/7)*12 + KB_WSEMI[i%7]` recurs
  ~4× (`174, 182, 200`, and `keybed_white_midi` at `182`) and a black `KB_BSEMI`
  variant alongside; `keybed_midi_at` (`159`)/`keybed_draw` recompute inline instead
  of calling `keybed_white_midi()` / a matching `kb_black_midi(i)`. **Done:**
  `keybed_white_midi()` + new `kb_black_midi()` moved above `keybed_midi_at` and all 6
  inline sites routed through them. Byte-identical (epiano renders identical vs HEAD).
- [x] **`worldnet.h`** — `nearest_hub`/`nearest_town` (`280–304`) are the same 5×5
  search with divergence risk (share a `nearest_present(getter, …)`); stale
  `(void)ux;(void)uy;` at `217–218` (both used on the line above). Delete the casts.
  **Done:** casts deleted (2026-07-09) + `nearest_present(get, cs, excl_self, …)` now
  backs both — query-rate so the getter fn-ptr is free. Byte-identical (roadnet2
  renders identical vs HEAD).

---

## Doc-only fix (spotted during the review)

- [x] **`studio.h:401` env-count doc bug** — says mod-envelopes are "2 per slot (which
  0..1)" but `note_env` (`330`) and `instrument_env` (`410`) both say `which (0..2)`.
  One is wrong; reconcile against the `sound.h` implementation. (Also `INSTR_VOICE 24`
  at `312` is declared after `INSTR_PIPE 25` at `311` — breaks the ascending block;
  cosmetic.) **Fixed:** `SOUND_ENVS` is 3 → now reads "3 per slot (which 0..2)". The
  cosmetic `INSTR_VOICE`/`INSTR_PIPE` reorder left alone (pure `#define` shuffle, no bug).

See also [`runtime-safety-audit.md`](runtime-safety-audit.md) — the sibling sweep covering
correctness & memory-safety bugs (this one is duplication/helpers/naming).

---

## Round 2 — after the per-instance refactor (2026-08-14)

Found by four read-only agents sweeping `runtime/sound.h`, `runtime/studio.c` + the host seam,
`ios/`, and the ctx refactor as a whole. **The headline is that the code was cleaner than the brief
assumed** — `#ifdef DE_NO_RAYLIB` appears 17 times in 6606 lines of `studio.c` (the backend fork is a
1–4 line runtime branch at 34 sites, everything else absorbed by `raylib_compat.c`'s link-time shim);
`sound.h` has zero direct `de_snd` accesses, zero unused params under `-Wunused-*`, zero dead statics,
zero `#if 0`. **The residue was not sloppy code. It was half-moved state groups, and instruments that
could not see them.**

Same rules as round 1: line numbers rot, the function name is the anchor, every item names its gate.

### Landed this round

| what | where | note |
|---|---|---|
| `engine-statics.js` dropped 30 rows silently | `tools/` | counted them, returned the count, never printed it. `kv_data`/`fp_cache` lost to a stale line cursor (`RecordDecl` filtered out before it could move it); `de_data_path_v`/`uiaudit_path` lost to `isConst` treating `const char *p` as const. `--check` 13 → 18, each guard mutation-tested |
| `midi_input.h` never measured | `runtime/midi_ctx.h` | absent from `ENGINE_FILES` for the whole refactor. 14 statics → 0; `de_midi_*` name their instance |
| seam lint blind spots | `tools/lint-engine-seam.js` | check B read one file, check C required `extern`, and neither could see a seam fn that never TOOK a handle → new check D. `--selfcheck` 14 → 22 |
| the Android port could not link | `android/` | every declaration pre-refactor, `de_init(DeRenderer)` outliving its deletion. Migrated + verified on the emulator; the lint walks `android/` now |
| `sound_reset_state` reset only part of the engine | `runtime/sound.h` | 34 config members never re-set → leaked cart→cart through both restore paths. `SR_CART_SWITCH`/`SR_STATE_RESTORE` collapsed into `sound_ctx_activate()` first, so the fix landed once |
| dead iOS spike layer | `ios/history/` | `canvas.h` declared `de_framebuffer` with the wrong arity AND return type |
| `mac.sh` gated a stale binary | `ios/mac.sh` | compiled `au-transport-check` three lines AFTER the gate that runs it |
| `platform.h` framebuffer origin | `runtime/platform.h` | said top-left; it is bottom-up |
| `blend_lut` duplicated per instance | `runtime/studio.c` | 20,480 B of constant table in every engine, rebuilt by ~1M iterations per boot. Shared again: `DeVideo` 31,432 → 10,952 B |
| an instance's heap was never given back | `runtime/studio.c`, `runtime/sound.h` | `de_instance_destroy` freed the struct only. Now frees what it allocated, through the instance rather than the thread-local |
| the shared sheet + fonts were rebuilt per instance | `runtime/studio.c` | `de_init_impl` overwrote SHARED globals with a fresh `malloc` each boot — a leak, and a pointer swapped under a sibling mid-`print()`. Once per process now |
| destroy had no gate | `tools/instance-check/` | the probe called it three times and asserted nothing. New heap-meter section + a `-bypass` control; goes red against `HEAD` at ~1 MB per rack |

### Open — `studio.c`

- [x] **`fb_w`/`fb_h` are provably always `== de_sw`/`de_sh`.** LANDED 2026-08-15. 2 fields, 33 use
      sites and ~25 lines of comment describing a distinction no caller can observe, gone;
      `de_ensure_fb` is folded into `de_set_canvas`, which is now the single funnel.
      ⚠ **THIS WAS "REVERTED AS UNSAFE" ON 2026-08-14 AND THAT WAS A MEASUREMENT ERROR.** The
      superseded note is kept below because the way it was wrong is worth more than the item.
      **What actually happened:** the A/B compared a `--resize` sweep of `acidcandy` between the
      working tree and a worktree at `HEAD` — and `acidcandy` persists its entire rack to a 437 KB
      `build/saves/acidcandy/cart.blob` which it REWRITES EVERY RUN. The two trees had different
      blobs, so the two runs started from different rack state. That is the whole difference. It
      moved the audio too, which I read as "a state divergence, not a rendering one" — correct
      reasoning, wrong subject: the state that diverged was the cart's saved knobs, not the engine's.
      **Redone with the save dir controlled** (wipe `build/saves/acidcandy` before each run): the
      main tree then reproduces the worktree's hash exactly, and the removal is **byte-identical to
      `HEAD` in both frames and audio** across the same five-step sweep. Gates: `refactor-guard` 6/6,
      `canvas-diff drawall`, `build-all` 581/581, `build-nr`, `instance-check`, `present-race-check`,
      `state-check`, `det-probes`.
      **THE REAL FINDING IS THE ORACLE, NOT THE REFACTOR** — see the `refactor-guard` item under
      gates below, which had the same defect and was reporting it about a file nobody edited.

      <details><summary>superseded 2026-08-14 note, kept for the trail</summary>

      The claim as it was written here ("provably always equal")
      is false, and the falsifying window is the one the merge collapses:

      `de_ensure_fb` sets `fb_*` and calls `de_grow_gpu()` **while `de_sw`/`de_sh` still hold the
      OLD size**; the caller (`de_set_canvas`) only publishes the new ones afterwards. Instrumenting
      `HEAD` shows the two one step out of phase on every resize —
      `GROW fb=320,200 de=160,100`, `GROW fb=160,100 de=320,200`, and so on. Collapsing the pair
      necessarily makes `de_grow_gpu` see the new dimensions instead of the old.

      Measured, on `acidcandy` (resizable, and a device FACE that reflows to its own chunky size
      every frame) under `--resize 320x200,480x300,240x400,800x480,167x100`: **all five dumped
      frames differ from `HEAD`** (PSNR 37-43 dB, visually indistinguishable), **and so does the
      WAV.** Audio moving is the tell — this is a state/timing divergence, not a rendering one.
      Controls run: the sweep is deterministic run-to-run on one build (5/5 identical), plain
      `acidcandy` with no sweep is byte-identical (`refactor-guard` 6/6), and applying ONLY the
      `de_ensure_fb(…)` → `de_set_canvas(…)` call-site swap to `HEAD` reproduces `HEAD` exactly —
      so the divergence is the ordering, not the call sites and not the rename (whose diff was
      reviewed line by line and is a clean identifier substitution at all 33 sites).

      What is NOT established: whether the new ordering is actually *worse*. Both orderings end in
      the same state and no cart code runs in the window. But the whole justification for this item
      is that it changes nothing, and it demonstrably changes something, so it does not ship on
      "probably benign". **A future attempt needs to explain the divergence first** — the cheap next
      step is a `de_grow_gpu(int w, int h)` taking its size explicitly, which decouples the two
      questions; an attempt to hack that up in-place is what ran out of budget here.

      Still true: it is 2 fields and ~25 lines of comment describing a distinction that no caller
      can observe, and that ambiguity is how `pget_texel` came to bounds-check one size and flip
      against another. Worth doing, once the above is understood. Gate: `refactor-guard`,
      `canvas-diff drawall`, **and a `--resize` sweep A/B'd against a worktree at `HEAD` — the first
      two are green for this change and only the third catches it.**

      *(That last sentence is exactly the trap. The third "gate" was the only one comparing across
      two trees, so it was the only one exposed to the uncontrolled save blob — its extra
      sensitivity was noise, not power. A comparison that disagrees with every other gate deserves
      suspicion of ITSELF first.)*
      </details>
- [x] **`pget_texel` bounds-checked `de_sh` then flipped against compile-time `SCREEN_H`.** LANDED
      2026-08-14, both it and `zoom_rect`'s GPU path (= `ui.h`'s loupe). **LATENT, not live**: no
      shipped cart is currently both `resizable: true` AND a pget/loupe/zoom_rect user (checked all
      16 against all 11), so nobody had hit it — it was waiting for the first cart to combine them.
      GPU-only too: `sw_zoom_rect` already used `de_sh`, so iOS/Android were never affected. **Still
      ungated** — `canvas-diff drawall` proves fixed carts unchanged (0px) but cannot see the
      resizable case.
- [x] **`blend_lut` is 20,480 B — 58% of `DeVideo` — and byte-identical in every instance.** LANDED
      2026-08-14. Hoisted back to a file-scope static beside the blend shader handles, recorded in
      `ctx-classification.json` → `studio_c.shared`, and `blend_tables_build()` now early-returns on
      its own latch, so instances 2..N skip ~1M inner iterations instead of recomputing a constant.
      Measured at 320×200: **`DeVideo` 31,432 → 10,952 B** (−20,480 exactly, −65%). `blend_lut_ready`
      was write-only while it was per-instance — nothing read it — and is now what makes the skip
      work. Gates: `refactor-guard` (6/6 byte-identical), `canvas-diff drawall`, and a
      `DE_SOFTWARE_CANVAS=on` frame dump of **`blendfx`** against a worktree at `HEAD` — byte-identical,
      which is the one that matters, since `blend_lut` is read only on the software blend path.
      ⚠ The conditional in the original note still governs and is written at the declaration: this
      moves back the day `base_palette` becomes writable. `palette_hex()` writes only the LIVE
      `palette[]`, which stays per-instance.
- [x] **`de_instance_destroy` frees the struct and nothing it allocated** — LANDED 2026-08-14. It now
      enters the instance (on a host thread the macros name the DEFAULT engine — freeing through them
      would have freed rack #1's canvas while destroying rack #2) and releases `sw_cbuf`,
      `sw_world_buf`, `pres_buf`, `de_state_mem`, `pget_snapshot`, `smooth_rt`, plus `rec_ring` and the
      sample slots via a new `sound_free_buffers()` in `sound.h`. The documented precondition is the
      host's: `pres_buf` is what `de_copy_frame` reads, so a view still blitting is a use-after-free.
      **Chasing it turned up a second, larger leak nobody had named** — see below.
- [x] **`de_init_impl` re-decoded the SHARED sprite sheet and font tables PER INSTANCE.** LANDED
      2026-08-14, found by the new heap meter, not by reading. `game_font`/`font_*_img` and
      `spritesheet_img` are classified `studio_c.shared` ("one atlas serves every engine"), but the
      code that fills them runs per instance — so instance N's `malloc` REPLACED instance N-1's
      pointer in the shared slot and orphaned it (~146 KB per rack opened, more than the buffers the
      item above is about). Two of them are worse than a leak: publishing a fresh `recs`/`glyphs`
      pointer into a global that a sibling engine may be inside `print()` on. Both inputs are
      compile-time constants, so both are now built once per process (`de_setup_baked_fonts` takes a
      latch; the sheet decode uses `spritesheet_img.data` as its own). Same shape as `blend_lut`.
      **Gate: `tools/instance-check` grew a `▸ DESTROY GIVES THE MEMORY BACK` section** — 8
      create/destroy rounds, macOS allocator statistics, measured after a warm-up round so
      process-wide init is not counted as a leak. **Green after (+0 B/round), and RED against a
      worktree at `HEAD` (+1,002,496 B/round)**, which is the evidence that the gate sees the real
      bug and not merely a skipped call. `-bypass` is the standing control (+4.98 MB/round).
- [x] **`circfill_pat`/`ovalfill_pat` were dead** — LANDED 2026-08-14. `-Wunused-function` flagged
      `circfill_pat` in BOTH configs and `ovalfill_pat`'s only caller was `circfill_pat`. Deleted
      with their two forward decls; `build-all` 581/581, `canvas-diff drawall` 0px.
- [ ] **`sw_tritex_legacy` is 5 weeks past its own soak deadline** (labelled "temporary, 2026-07 …
      delete once the fast path is trusted"); the four sibling flags were retired after 2–3½ weeks.
      ~29 lines. A policy call, not a technical one. ⚠ `pset_batch` reads like a sibling and is NOT
      one — it is a per-platform default.
- [ ] **Every fragment shader is written twice** (web GLSL-100 / desktop GLSL-330), 102 lines of
      literal for 2 shaders, differing only in the prologue. Worse, the nearest-palette metric now
      lives in 5 places across two languages and C and GLSL must agree. ⚠ a broken shader fails
      SILENTLY (`pal_shader_ok` stays false and sprites just draw unswapped) — the gate must be run.
- [ ] **Two boot sequences** (`de_init_impl` vs `main()`'s block) do the same six steps; the sheet
      upload is the near-verbatim pair. This is the `de_process_init` split
      [`per-instance-remaining.md`](per-instance-remaining.md) already names.
- [ ] `loop_step` is 428 lines with three comment-delimited seams (pause menu, pget snapshot,
      present). Removes ~0 lines — legibility only, and the `goto draw_window` makes the pause
      extraction non-trivial. `DeRenderer`'s GPU arm is unreachable and fails silently (+3 lines to
      reject or log). The palette→float-triples loop is written 4×.

### Open — `sound.h`

- [x] **The `DRIVE_*` waveshaper switch exists twice, verbatim** — LANDED 2026-08-14. The per-voice
      path calls `drive_shape()` now, which is where the mix-bus insert already went. The insert's
      own comment ("a verbatim copy of that switch") is gone with the copy. **Coverage, because the
      probe set alone would not have earned it:** `ab-render drivemodes --set mode=0,1,2,3` renders
      all four curves and every sha matches a worktree at `HEAD` — `456d0e05ea3d` / `1fecd6fe27fc` /
      `efa77be1aa7f` / `8f89a7647d7c`. `refactor-guard` 6/6. `click-check` flags the same 64 events
      before and after (a mode flip mid-note through a hard clipper genuinely steps).
- [x] **Karplus-Strong seeding duplicated between PLUCK and GUITAR** — LANDED 2026-08-14. Extracted
      as `ks_pluck_excite(v, pick_frac)`; the single expression they differed in is now the
      parameter (PLUCK sweeps it from `morph`, GUITAR bakes 1/4 string because its `morph` carries
      the mute). Same move `ks_seed_bore` made for the bore engines. A/B vs `HEAD`: `bitcrush` +
      `bossa` (PLUCK), `air` + `aquapuss` (GUITAR), all byte-identical **and all four verified
      non-silent first** — the two obvious PLUCK carts (`chordblossom`, `afxkeys`) render `-inf`
      without input, so comparing those would have proved nothing.
- [x] **"ensure FX_X is in bus b's chain" is written 9 times and the copies diverged** — LANDED
      2026-08-14 as `fx_chain_ensure(bus, fx)`. Both divergences are now unrepresentable: the bound
      is `min(N_INSERTS, FX_ORDER_SLOTS)` (the only one right for both — 19 is the array, 16 is what
      `fx_order` can pack, and `SR_FX_ORDER` already took the min), and `insert_inst` is always
      cleared, which the two `FX_GRAINS` sites did not do. A/B vs `HEAD`: `grainstest`, `gatetest`,
      `afrobeat`, `caustics` byte-identical; `fx-check` reports the same three pre-existing warnings.
- [x] **Four `SR_*` kinds were in NEITHER classification switch** — LANDED 2026-08-14.
      `SR_INPUT_MONITOR` → `CTXK_K` (a master knob beside the other mic kinds), and
      `SR_INSTR_GLIDE`/`_GLIDE_SCALE`/`_TRIGGER` → `CTXK_KA` (a = instrument slot, exactly like
      their `SR_INSTR_TUNE`/`_DRIVE` neighbours). All 146 kinds are now classified: 30 events + 116
      keyed. Before this they fell through to `CTXK_APPEND`, so a cart riding `instrument_glide()`
      from a knob appended one log entry per call until `ctx_overflow` tripped — after which restore
      is documented as incomplete.
      **STILL OPEN — the hazard is the silent `default:`, not the four kinds.** A new `SR_*` is
      still registered in three places across two files with nothing checking it (the shape
      `lint-aux-params.js` exists for). Wanted: a `lint-sound-reqs.js` in the `--selfcheck` house
      style, or an `SR_LIST(X)` X-macro generating the enum and both switches (−50 lines). Note
      `-Wswitch` cannot do it: the two switches are only exhaustive TOGETHER, and neither may drop
      its `default:`.
- [ ] `sound_callback` is 466 lines with a 273-line per-voice body; the seam is
      `sound_render_voice()`. Removes ~0 lines — buys a readable callback, a profilable voice
      renderer, and a smaller collision target in the file CLAUDE.md says to edit only with targeted
      `Edit`s. Do it AFTER the dedups so there is less to move.
- [x] `lfo_seed_ctr` is still a function-local static and is classified **CRITICAL** — LANDED
      2026-08-14, into `DeSound` (a `#define` alias cannot reach a function-local, so the
      declaration itself moved). Its `0x12345u` initial value moved with it as a designated
      initialiser; zero would have shifted every LFO seed.
      **⚠ IT IS STILL UNGATED, and the item was right that it would be.** `refactor-guard` is green
      by construction. `instance-check` was too, so it grew a sample-exact solo-vs-interleaved
      assertion — and that is *still* green with the counter deliberately put back to a shared
      static, and green again with the seed's initial value changed to `0x99999`. Both perturbations
      were run rather than assumed. The reason is the cart: `acidcandy` reaches `LFO_SHAPE_RANDOM`
      only through `acid303.h`'s drift at 0.13/0.19 Hz, not audibly within the probe. A gate needs a
      cart that opts into `DE_CART_CTX` **and** drives a stateful LFO fast enough to step, and
      nothing on the shelf does both. The move is still obviously right — a shared modulator seed
      across instances is wrong on its face — but it rests on reading, not on a red-then-green.
- [x] **Five write-only overflow counters with no reader** — LANDED 2026-08-15, and the file's own
      culture picked the remedy: give them a reader, do not delete them. `grain_overflow`,
      `shim_overflow`, `fx_bus_overflow`, `rvb_bus_overflow` and `bow_body_overflow` had been counted
      since they were written and read by nobody, so a cart asking for a third granular tank or a
      ninth aux bus got `-1`, the caller quietly did nothing, and the effect was **silently absent** —
      no error, no silence where sound should be, just a delay that never appears. They now print one
      deduped `[sound] WARNING` each, next to the two tripwires that already existed. `soundcheck`
      stays silent, so nothing on the shelf currently exhausts a pool.
- [x] **`at_psola_slot`'s `formant` param was clamped and never used** — LANDED 2026-08-15, removed.
      Both callers passed `0.0f` and no cart-facing API exposed it, so it was dead surface that READ
      like a working knob — the same shape `lint-aux-params.js` exists to catch on the other channel.
      It was a placeholder for the formant-HOLD axis this engine deliberately parks; the comment left
      in its place says so, and says it comes back WITH an implementation. Gate: `psola-check` (no
      artifact regression), `refactor-guard` 6/6.
- [x] **`ms_samp()` for the hand-written `(x * SOUND_SAMPLE_RATE) / 1000`** — LANDED 2026-08-15
      across 15 int sites. **This one was not just tidiness: the int product leaves range at about 48
      SECONDS** at 44.1 kHz, so a cart scheduling a note a minute out got a negative sample count —
      and the tell was already in the file, since one site had been quietly widened to `long long`.
      The helper saturates instead of wrapping and truncates identically otherwise, so every
      in-range call is byte-identical. NOT folded in and said so at the declaration: the two echo
      `ms * (float)RATE / 1000.0f` sites (a float delay-line position) and the one `long long`
      declick floor whose result stays 64-bit for the comparison it feeds.
- [ ] Smaller, still open: CLAV/WURLITZER e-piano blocks are ~20 identical lines apart from their
      tables (and unlike 808-vs-909 the navkit UPSTREAM is itself parameterised, so this one is
      real) · twelve mutually-exclusive per-engine `*_on` bools that could be one `eng_armed` ·
      `sound_push_req`/`sound_push_ctrl` duplicate the lock-free publish protocol.
      *(The other three from this line landed 2026-08-15 and have their own entries above: the five
      write-only overflow counters, `at_psola_slot`'s dead `formant` param, and `ms_samp()`.)*

### Open — Swift / iOS

- [ ] **`TinyjamAU.uiTick()` is orphaned** — nothing assigns `CanvasView.onDisplayTick`. It was wired
      to fix "the panel freezes when the host is stopped", then removed when the panel owned a second
      engine; that reason died when the panel started getting the AU's own engine. Hosted touches
      enter the input ring, drained only inside `de_frame`, which on that path runs only when audio
      is pulled → **stopped host = frozen panel and every tap swallowed.** One line. No gate can see
      it: `--panel`/`--view`/`--realtime` all render audio.
- [ ] **The canvas message channel is a `static let`** — N instances share one channel and one
      `owner`, which is the exact "panel showing an engine nobody can hear" bug its own comment says
      it exists to prevent.
- [ ] **`TinyjamAU` can never deallocate**: `Thread { [weak self] in self?.workerLoop() }` takes a
      strong ref for a call that never returns, so `deinit` never fires, four manual allocations
      leak, and `de_instance_destroy` is never called.
- [ ] `AudioEngine`'s render callback does ARC and can `malloc` on the audio thread — `TinyjamAU`
      went to real trouble to avoid both · `CanvasView.tick()` copies the frame up to 4× per display
      tick, one of them a fresh `Data` allocation · `44100` is stated 4× in Swift and 0× from the
      engine · `TinyjamAUFactory` is dead and is the hazard its neighbour warns about · two shipped
      comments assert the opposite of what the engine now does.
- [ ] `ios/AUProbeKit.swift` for the probes' shared HOST scaffolding (~−55 lines). **The bright
      line: a probe may share host plumbing, never the decoder/format/constant it asserts** — the
      DEZ1 decoder, the `"dreamengineRack"` key and the probes' hardcoded `44100` are known answers
      and must stay duplicated.

### Open — gates and measurement

- [x] **`refactor-guard`'s baseline silently encoded `build/saves/<cart>/`** — FOUND AND FIXED
      2026-08-15, and it is the most consequential thing in this round because of WHICH gate it is.
      Every probe ran with the default `--save-dir saves/<cart>`: untracked, mutable, and rewritten
      by every run. So the baseline quietly recorded whatever rack state the cart had persisted, and
      a later run from a different history "drifted". `acidcandy` persists a **437 KB `cart.blob`**,
      and deleting it made the gate report `audio diverges at 0.0s … state diverges at frame 0` —
      **with identical numbers on an unmodified tree**, under a headline that reads "a state move
      that changes output is a BUG in the refactor, not a new baseline". That is the worst shape a
      gate can fail in: confident, specific, and about a file nobody edited.
      Fix: each probe now runs in an isolated `saves/.refguard/<cart>` that is WIPED before the run,
      so bless and compare both start from a known state. Verified in both directions — green with
      the live blob deleted, and green again with a foreign tree's blob restored, where the old code
      went red for each. Baseline re-blessed (the fingerprints legitimately change: the probes now
      boot from no-saves rather than from an accumulated rack).
      ⚠ **The same trap applies to any hand-rolled A/B**, which is how it was found: comparing a
      cart across two worktrees compares their save blobs too. Wipe or isolate the save dir, or use
      a cart that does not persist.

- [x] **`lint-aux-params` had been RED against the real source, and `repo-doctor` showed green.**
      LANDED 2026-08-14, found by running it while gating something else. The per-instance refactor
      moved both `float eng_p[7];` out of `sound.h` into the generated `sound_ctx.h`; the lint read
      only `sound.h`, found **zero** declarations, and so reported three findings on a healthy
      engine — `width` was `undefined`, and both bound checks compared against it. **The reason
      nobody saw it is the interesting half:** `repo-doctor` ran `--selfcheck`, which passes on a
      fixture, and never ran the lint itself. So the lint now reads both files, `repo-doctor` gained
      an `aux params` row beside the `selftest: aux params` one, and the fixture gained a `split/`
      case reproducing exactly this shape (`--selfcheck` 14 → 17, mutation-tested by hiding the ctx
      header: it reproduces all three original findings). Engine unchanged — it was healthy
      throughout; width 7, 2 declarations, 2 bounds, 7 copies, 12 modes.
      **The general rule this earns:** a `--selfcheck` row proves the checker works, only a real run
      proves it is still looking at the repo. Every lint with the first row wants the second.
- [x] **`midi-check` phase B is flaky** — DIAGNOSED AND FIXED 2026-08-14, and it was the gate, not
      the engine. **The sender's lifetime was a fixed 12 s of WALL CLOCK while the cart's start time
      is a VARIABLE**: `play.js` recompiles the engine on every invocation, and the CC arrives at the
      cart's *frame 1*, so the only thing this phase needs is for the sender to still exist when the
      cart's engine initialises. Measured compile: **3.9 s idle · 8.2 s under one concurrent
      `build-all` · 13.7 s under three.** Past ~11.3 s the cart boots after the sender has exited and
      **all six assertions fail together**, which reads exactly like a broken CC parser. This repo
      runs several agents on one working tree, so "somebody else is compiling" is the normal state.
      Reproduced on demand by running three `build-all` sweeps alongside it.
      Fix: the sender now outlives any plausible compile (180 s), and the `kill` after the cart is
      what actually ends it — the bound is a safety net, not a schedule. Plus a **new
      discriminator**: run.sh checks whether the sender was still alive when the cart exited and
      reports `THE GATE RACED, not the engine` instead of six parse failures. Verified in both
      directions — the same three-sweep load now passes, and forcing a 3 s sender fires the new
      message.
      ⚠ **CORRECTION TO THE ORIGINAL NOTE.** "Only a throwaway worktree at `HEAD` established that
      the failure was not a regression" — that inference was wrong. A worktree changes the *machine
      load at that moment*, not the code under test; it passed because the machine happened to be
      quieter, and it would have passed just as well with a genuinely broken parser. The variable
      was never the tree.
      ⚠ Phase C has the same SHAPE (a sender sized in wall clock, a receiver started after
      `sleep 12`) but ~20 s of slack rather than ~5 s, and was sized deliberately. Left as-is;
      if it ever flakes, this is the first thing to look at.
- [x] **`ctx-gen --verify` now covers FUNCTION-LOCAL statics too** — extended 2026-08-15, one day
      after it shipped, because it failed to notice a new one being added. `engine-statics --list`
      emitted only file-scope statics (function-locals escaped as a bare COUNT), so the check was
      blind to exactly the category `lfo_seed_ctr` belonged to — the one classified CRITICAL. Names
      are emitted now, tagged `scope: 'function'`, and the report names the different remedy (a
      `#define` cannot reach a function-local; the declaration has to move). ⚠ The generator is
      explicitly guarded to keep ignoring them, or it would emit an alias that silently does nothing
      to the local while renaming every other use of that name. 137 → **158 statics checked**, and
      the 21 it surfaced are now classified — including a 13-variable memoisation cache in
      `poly_clamp_scan` that is **safe to share by construction** (the cache key captures every
      input, so a hit returns what a recompute would, whoever computed it) and should NOT be
      "fixed" per-instance, plus `seen_real_touch`, which is a fact about the DEVICE and is more
      correct shared than duplicated. `--check` 26 → 28.
- [x] **`ctx-gen --verify`** — LANDED 2026-08-14, and it earned its keep on the first run. Every live
      engine static must be either moved into a context or written down in `ctx-classification.json`
      with a group saying why it stays shared; `--quiet` gates and it is a `repo-doctor` row now.
      ⚠ It asserts BOOKKEEPING, not correctness: a name under `shared` is not proven safe to share,
      it is proven to have been thought about, with the reason where the next person looks.
      **28 unclassified statics on the first run, and two of them were WHOLE FILES that had never
      been classified at all — `mic.h` (11) and `midi_output.h` (7), the same shape as the
      `midi_input.h` finding that opened this round.** The rest were benign and are now recorded:
      the instance machinery itself (`de_*_pristine`, `de_inst_next_id`), the three GLSL source
      literals, and `vox_cons_name`. `--selfcheck` 18 → 26, the new guards mutation-tested by making
      an unkeyed file read as clean — which reproduces exactly the blindness the check exists for,
      and takes it to 24/26.
- [x] ~~**`midi_out_on` is a FLAG where a shared MIDI port needs a REFCOUNT**~~ — **WITHDRAWN
      2026-08-15. I got this wrong when I first wrote it down, and the correction is the finding.**
      `midi_send_note` updates the table and sends the message *unconditionally and together*, so the
      table mirrors THE LAST NOTE MESSAGE PUT ON THE WIRE — which, with one shared port, is exactly
      the receiver's state, whichever instance sent it. A flag is the correct type. A refcount would
      be strictly WORSE: `A on, B on, A off` puts a note-off on the wire (the receiver stops), while
      the refcount would still read 1, so the table would claim a note is held that nobody is
      playing, and shutdown would send a note-off into silence.
      **The real collision is one layer up and is not a bookkeeping bug**: two racks sharing one port
      AND one channel means A's note-off genuinely cuts B's note on the receiver. Nothing this table
      can do about that — it needs per-instance channels or per-instance ports, which is its own
      design question (`engine-dylib-spike` already lists "K same-named CoreMIDI virtual sources" as
      unresolved).
      ⚠ What IS real, and narrow: the table write and the send are two steps, so two instances
      calling concurrently from different audio threads can interleave as
      `A: table=1 · B: table=0 · B: send(off) · A: send(on)` — leaving the wire ON and the table 0,
      i.e. a note that shutdown will not release. Ordering/atomicity, not a refcount. Not fixed;
      recorded because it is the actual hazard, unlike the one this item used to claim.
- [ ] **`mic_rec` is a 1.4 MB capture ring two instances would fight over** — same first run. One
      capture device makes the mic path process-wide on purpose, but two instances calling
      `mic_record_start()` race for one buffer. Nothing does yet, and duplicating 1.4 MB per rack to
      fix a case nobody has hit is the wrong trade — recorded as a race, not waived as a design.
- [x] `sw_rot_active`/`sw_rot_angle` are shared while every buffer they steer is per-instance —
      `DE_NO_RAYLIB` only, i.e. exactly the AUv3 build. `kv_data` shared while `kv_count`/`kv_loaded`
      are per-instance. Both violate the "this group moves together or not at all" warning in
      `studio_ctx.h`. **BOTH LANDED 2026-08-14**, into `DeVideo` beside the members they belong to
      (`KV_MAX`/`KV_KEYLEN` moved to `studio_ctx.h` with the table, since the member is written in
      them). The `kv_data` one is the worse of the two and is a data-corruption bug, not a tidiness
      one: instance B boots with `kv_loaded` false, `kv_ensure()` reloads B's save file over the ONE
      shared table, and A's `kv_count` then indexes B's keys — `save_int(name)` returning another
      rack's value. `sw_rot_*` would have had `de_frame` composite B's world buffer at A's angle and
      repoint B's `sw_dst`. Gates: `refactor-guard` 6/6, `instance-check`, `state-check`,
      `present-race-check`, `build-all` 581/581, `spec` 2027.
- [ ] `de_audio_input`/`de_mic_wanted`/`de_mic_set_active` are waived as process-wide (one capture
      device). Revisit if an instance ever needs its own mic routing.

### Deliberately NOT doing — re-verified this round

Round 1's three ❌ calls still hold, and I checked each rather than trusting the note: the
`sw_sline`/`de_cpu_line` merge (a plot callback is an un-inlinable indirect call **per pixel**), the
`circfill`/`ovalfill` merge (`disc_inside` is exact float arithmetic where `ellipse_inside` divides
then squares — delegating flips boundary pixels), and the 6× outline-ring predicate (it runs 5× per
pixel over an O(r²) bbox). Add to them:

- **The software rasterizer stays its own implementation.** `det-probes/` exists because it must be
  bit-identical on arm64/x86-64/wasm; any shared-primitive proposal must clear `det-probes/run.sh`,
  not just `canvas-diff`.
- **808 vs 909 and the twelve engine `*_start`/`*_sample` pairs** are different circuits and
  different physical models, not parameter sets.
- **The `X()`/`instrument_X()`/`fx_set_X()` API triples** (~20 sets) are macro-generatable and
  should not be: this is the beginner-legible public surface the north star protects, and the
  scaling genuinely varies per call.
- **The `#define name (ctx->name)` mechanism.** 548 aliases, contained (each `*_ctx.h` has exactly
  one includer), zero collisions; `tls-spike` measured the alternative as free and the repo chose
  knowingly. One narrow ask: have `ctx-gen` refuse members under ~4 chars — `#define sc (de_snd->sc)`
  is a two-character global macro.
- **`CanvasView.remoteFrame` + `TinyjamCanvasChannel`** are inert but documented as deliberately
  kept, and `au-msgchannel-spike` actively exercises the channel.
