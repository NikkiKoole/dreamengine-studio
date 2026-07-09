# Engine simplification backlog — duplication, missing helpers, naming

> **STATUS: ready to build** (2026-07-09). A punch-list of *quality-only*
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

- [ ] **`sw_sline`/`sw_plot_minor` (`775`/`768`) vs `de_cpu_line`/`de_cpu_plot_minor`
  (`4349`/`4342`)** are verbatim copies of the same reflection-symmetric DDA, differing
  *only* by `sw_pset` vs `pset` (~30 lines; comment at `4335` admits it).
  Parameterize one over a plot callback (or `#define`-templated body).
- [ ] **`circfill` (`4164`) should delegate to `ovalfill` (`5496`)** — a circle is
  `rx==ry`, and `circfill_pat` (`4109`) already delegates to `ovalfill_pat`. ~25 lines
  vanish. (`disc_inside` at `4124` is likewise `ellipse_inside` with `rx==ry`.)
  **⚠ Assessed 2026-07-09: NOT byte-safe — leave as-is.** `disc_inside` is *exact*
  (`dx = int+0.5` is a half-integer, so `dx²+dy² ≤ r²` never rounds), but
  `ellipse_inside` divides first (`dx/r`) then squares — which rounds — so a boundary
  pixel can flip. Delegating would shift a few edge pixels on some circles, and also
  change negative-radius behaviour (circfill draws nothing; ovalfill abs's it) and drop
  the `UIAUDIT('c')` marker. The math is equal; the floats aren't.
- [ ] **`outline_ring(bbox, inside_predicate, color)`** — the "pixel is inside AND a
  4-neighbour is outside" test is copy-pasted **6×**: `circ` (`4143`), `oval`
  (`5524`), `rrect` (`5104`), `poly_stroke_cov` (`4990`), `sector_draw` stroke
  (`4260`), `thick_draw` stroke (`5076`). One helper over a function pointer / macro
  collapses all six.
- [ ] **Retire the soak-period scaffolding.** `sw_tritex_legacy` (`898`, TODO at
  `897`) and `poly_fill_cov_legacy` (`4913`, TODO at `4912`) plus their runtime
  toggles (`blit_fast_on`, `tritex_fast_on`, `disc_fill_fast`, `poly_fill_fast`,
  `clamp_cache_on`, `pset_batch`; declared `253–272`, parsed `2849–2860`) are
  self-labelled switches — once trusted, deleting the legacy paths + flags removes a
  lot of duplicated primitive code.

---

## Group D — `sound.h` near-identical engines & FX

Prove with `tune-check` (pitched) / `fx-check` / `level-check`.

- [ ] **Filter pairs share a core.** `sound_ladder` (`4433`) vs `sound_diode`
  (`4492`) are ~90% identical (differ by one `tanhf` on feedback + tap stage 3 vs 4);
  same for `sound_svf` (`4408`) vs `sound_steiner` (`4463`). Share a core with a
  flag/param instead of two full copies.
- [ ] **`sound_choke_group(cmask)`** — the choke-group voice-steal block is
  duplicated verbatim 3× (`sound.h:4849, 4863, 5452`: `SR_NOTE`, `SR_NOTE_ON`,
  `SR_HIT_AT`).
- [ ] **Karplus tap-read + T60→feedback helper.** `ks_tap_read` (fractional read +
  wrap + lerp) and `fb = expf(-6.9078f/(t60*f))` are duplicated between
  `sound_engine_sample` (`4342`+; `fb` at `4372`) and `sound_guitar_sample` (`4043`+;
  `fb` at `4053`), and the tap-read reappears in the echo send (`~5890`). Extract
  `ks_tap_read` / `t60_to_fb`.
- [ ] **`ks_seed_bore(v, targetLen, noiseScale)`** — bore/string delay-line seeding
  (size ×2.5, cap at `SOUND_KS_MAX-1`, floor 4, seed noise, set `initfreq`) is
  copy-pasted across reed/pipe/bowed/brass (`sound.h:3019, 3163, 3263, 3424`),
  comments and all; only the noise scale differs.
- [ ] **Modulated-delay skeleton (chorus/flanger/tape).** Each re-writes phase
  advance, triangle LFO, delay-in-samples clamp, `moddel_hermite` read (the shared
  read at `sound.h:624`), and the `wet/dry` blend by hand (chorus `720`, flanger
  `758`, …). The blend line `dry*(1-mix)+wet*mix` recurs in nearly every `*_process`
  (e.g. `1266`) — a one-liner helper.
- [ ] **Master/instrument FX setter pairs.** ~10 near-identical `SR_X`/`SR_INSTR_X`
  copy-paste arms in `sound_fire_req` (dispatch guarded at `sound.h:1883–1906`), plus
  a copy-pasted "auto-place kind in `insert_order`" scan (`5156–5157`, `5189–5190`).
  Factor both into helpers.

---

## Group E — table-driven instead of parallel switches / if-ladders

- [ ] **`sound_fire_req` → `switch`.** It's a ~690-line `else if` chain (`sound.h:4832`)
  on a dense `SoundReqKind` enum — a textbook `switch` (jump table + unhandled-case
  warning). Many arms are literally `decode (a/1000) → clamp → store`; a small helper
  given #1's `clamp01`.
- [ ] **Engine dispatch → `switch`/table.** `sound_engine_sample` (`sound.h:4342`)
  tests `v->wave` against 13 constants sequentially before the Karplus fall-through.
- [x] **`btn_local` keymap table.** The near-identical 8-case per-player switches in
  `btn_local` (`studio.c:3298`+) → `static const int keymap[2][BTN_COUNT]` + one
  `inp_down(keymap[player][button])`.
- [ ] **Font table.** Font load (`studio.c:3070–3095`), unload (`~3105`), and the
  two lookup if-chains `cur_font()`/`cur_font_img()` (`3849`/`3856`) repeat the same
  per-font work 3 times over. A `{Font*, Image*, data, len, keyColor}` table indexed
  by font id drives all of it.
- [x] **`env_is(name, val)` helper.** 9 near-identical `getenv + strcmp` blocks
  (`studio.c:2849–2867`). (The 10th, `DE_SHOW_SIZE`, is a "set & not 0" test, not an
  `==`-match — left inline.)

---

## Group F — smaller drift-risk items

- [ ] **`studio.c` shared bodies:** `pget` (`4433`) / `pget_rgb` (`4455`) share the
  world→snapshot-texel guard machinery; the palette-index match loop is duplicated in
  `pget` and `sget` (`4466`) → `palette_index_of(DeColor)`; `harness_inspect`
  state-request (`1931`+) re-implements `harness_trace`'s watch-serialization
  (`1814`+) → `write_state_json()`; `MAP_DATA` load duplicated in `de_init`
  (`2689`) and `main` (`3046`) → `de_load_map()`; `sprf` builds unused `Rectangle`
  src/dst on the software path (`3708`+).
- [x] **Dead `PAUSE_KEY` guard** — redefined at `studio.c:3280–3281` but already
  defined at `455–456`, so the second `#ifndef` is always skipped. Delete.
- [ ] **`net.h` packet helpers** — WELCOME/HELLO builder + seed LE encode/decode
  pasted ~4× (seed LE encode/decode at `463–464`, `472–473`, `504–505`; constants
  `NET_PKT_HELLO`/`NET_PKT_WELCOME` at `86–87`). Add `net_welcome_pkt()` /
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
  ~4× (`174, 182, 200`, and `keybed_white_midi` at `182`) and a black `KB_BSEMI`
  variant alongside; `keybed_midi_at` (`159`)/`keybed_draw` recompute inline instead
  of calling `keybed_white_midi()` / a matching `kb_black_midi(i)`.
- [ ] **`worldnet.h`** — `nearest_hub`/`nearest_town` (`280–304`) are the same 5×5
  search with divergence risk (share a `nearest_present(getter, …)`); stale
  `(void)ux;(void)uy;` at `217–218` (both used on the line above). Delete the casts.
  **Partial (2026-07-09): stale `(void)` casts deleted.** The `nearest_present` dedup
  is bucket ② (needs a getter-fn refactor + verify) — still open.

---

## Doc-only fix (spotted during the review)

- [x] **`studio.h:401` env-count doc bug** — says mod-envelopes are "2 per slot (which
  0..1)" but `note_env` (`330`) and `instrument_env` (`410`) both say `which (0..2)`.
  One is wrong; reconcile against the `sound.h` implementation. (Also `INSTR_VOICE 24`
  at `312` is declared after `INSTR_PIPE 25` at `311` — breaks the ascending block;
  cosmetic.) **Fixed:** `SOUND_ENVS` is 3 → now reads "3 per slot (which 0..2)". The
  cosmetic `INSTR_VOICE`/`INSTR_PIPE` reorder left alone (pure `#define` shuffle, no bug).
