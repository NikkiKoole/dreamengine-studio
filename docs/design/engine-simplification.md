# Engine simplification backlog — duplication, missing helpers, naming

> **STATUS: DONE** (2026-07-09; last landed 2026-07-12) —
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
