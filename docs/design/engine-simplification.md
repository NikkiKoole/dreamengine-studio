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

- [ ] **`fb_w`/`fb_h` are provably always `== de_sw`/`de_sh`.** One write site (`de_ensure_fb`), and
      all three callers pass the values that become `de_sw`/`de_sh`. A fossil of the abandoned
      grow-only scheme: 2 fields, ~14 use sites, ~25 lines of comment explaining a distinction with
      no content. **Doing it makes the next item unrepeatable.** Gate: `refactor-guard`,
      `canvas-diff drawall`, a `--resize` run.
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
- [ ] `lfo_seed_ctr` is still a function-local static and is classified **CRITICAL** — two instances
      interleaving note starts both lose determinism, and `refactor-guard` would sit green through
      it. Belongs to the per-instance lane, not this list. ⚠ `ctx-classification.json` records its
      line as 5822; the real one is 4933.
- [ ] Smaller: CLAV/WURLITZER e-piano blocks are ~20 identical lines apart from their tables (and
      unlike 808-vs-909 the navkit UPSTREAM is itself parameterised, so this one is real) · five
      write-only overflow counters with no reader anywhere (give them a reader, don't delete — the
      file's culture is fail-loud) · `at_psola_slot`'s `formant` param is clamped and then never used
      · twelve mutually-exclusive per-engine `*_on` bools that could be one `eng_armed` ·
      `sound_push_req`/`sound_push_ctrl` duplicate the lock-free publish protocol · `ms_samp()` for
      the 18 hand-written `(x * SOUND_SAMPLE_RATE) / 1000` with inconsistent clamps.

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
- [ ] **`midi-check` phase B is flaky** — failed once and passed twice on identical code; only a
      throwaway worktree at `HEAD` established that the failure was not a regression. A gate whose
      failure is indistinguishable from a real one will eventually be believed wrongly.
- [ ] **`ctx-gen --verify`** — assert every live static in a processed target is classified or
      hand-annotated. `ctx-gen --check` self-tests the parser, not the source, and both generated
      headers are now partly hand-maintained. This closes the half-moved-group CLASS rather than its
      instances, and would have caught `kv_data` and `sw_rot_*` the day they landed.
- [ ] `sw_rot_active`/`sw_rot_angle` are shared while every buffer they steer is per-instance —
      `DE_NO_RAYLIB` only, i.e. exactly the AUv3 build. `kv_data` shared while `kv_count`/`kv_loaded`
      are per-instance. Both violate the "this group moves together or not at all" warning in
      `studio_ctx.h`.
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
