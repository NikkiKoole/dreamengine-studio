# Probe carts — the experiments that decide what becomes API

STATUS: LIVING (ledger, kept current) — which cart answers which API question, and the verdicts.

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
| `sfxed`, `sfxgen` | does sfx authoring need engine banks (`sfx_def()`)? | **leaning no** — export-as-C works (STATUS #5/#12 direction note, [audio-notes](audio-notes.md) §5.6). Formally still open; see below. |
| `mt70` | does the MT-70's "2 (sometimes 3) sine oscillators per key" need a second-oscillator engine, or does voice-stacking suffice? | **stacking suffices — no API needed.** All 10 presets render by stacking `note_on`s on separate slots, exact ratios via `note_pitch` floats; the cart ships with an A/B switch (`V`) against `INSTR_MALLET` on the struck presets. **Ear verdict (owner, 2026-06-07): "they sound very different, I like both."** → the two paths are *complementary, not competitors* — the A/B stays a feature, not a bake-off to settle. Soft (not blocking) signal toward eventually building the **Additive engine** (instrument-engines §8.9 names mt70 its first customer): wanted for *range*, not because stacking failed. The deferred §12 gap 2b ("second audible oscillator on the generic voice path") stays deferred — mt70 didn't need it. **Validated-by (the rules paying off, with receipts): a whole playable instrument shipped with a *zero-line* `runtime/` diff** — all 3 mt70 commits touch only `tools/carts/mt70.c` + the `.cart.png` + the registry; every symbol it calls (`note_on`/`note_pitch`/`note_vol`, `instrument_filter`, `INSTR_MALLET/SINE/NOISE`, the macros, `tapp`) pre-existed. So this is a clean worked instance of **[ADR-0006](../decisions/0006-library-carts-not-engine.md)** (richness in cart-land, not the kernel), **[decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md)** ("two oscillators" *was* a recipe — voice-stacking — exactly the "prove it can't be one" bar), and the second-customer rule (ship the recipe; let the engine wait for a real second customer). The cart didn't just avoid an engine change — it proved the change isn't *owed*. |

## ✅ Resolved: engine API was needed — the probe became the demo

| Cart | What it proved | API that shipped |
|---|---|---|
| `blendlab` | 32-color translucency works as a lookup table, **but cart-land can't read dst correctly** — its deliberately-broken `P` mode shows the last-frame `pget` feedback bloom; dst must come from the in-progress frame | **verdict in, API not yet built** — STATUS #18, [`blend-tables.md`](blend-tables.md); next step is the ADR, **sequenced after the palette decision** ([`palette-and-color.md`](palette-and-color.md) — tables derive from the palette). The one probe that proved *need* and is awaiting the build. |
| `waveed` | drawable single-cycle waves earn their keep (live-morph drone) | `wave_set()` + `INSTR_USER0..3` |
| `platform-paint` (vs `platform-rects`) *(not tagged — shipped with the API as its teaching pair)* | level-as-painted-data needs a spritesheet read | `sget()` |
| `podracer` *(not tagged — a game that stumbled onto the cliff, not built to ask)* | software-poly perf cliff is real | off-screen bbox clamp (STATUS #14) |
| `acoustics` (spatial v3) | acoustic zones: the **mechanic is cart-land** (listener→source raycast → `note_cutoff` [Hz+slewed = great], zones → `reverb(size,damping)`, materials → `(size,damping)` presets). But two conveniences dropped out: `note_vol`'s **0..7 steps audibly** under occlusion attenuation, and **set-and-hold blocks** a per-frame zone-reverb crossfade (rooms jump). | **verdict in, API not yet built** — (a) `note_gain(handle, float)`, (b) a **rideable/crossfadable zone reverb**; the bulk ships as a cart-land `acoustics.h` helper. A tilemap line-of-sight primitive is an optional generalizing bonus — now its own dedicated probe, `sightlines` (open, below). STATUS #40, [`spatial.md`](spatial.md) v3. |

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
| `dijkstra` | is a **Dijkstra / flow-field map** worth owning — the *decision* companion to `astar`'s single path ("[the incredible power of Dijkstra maps](https://www.roguebasin.com/index.php/The_Incredible_Power_of_Dijkstra_Maps)")? | the testbed, built 2026-06-15 on the same occluder grid as `sightlines`. One flood-fill (the relax/scan method) → three behaviours from the *same* field: **approach** (roll downhill), **flee** (negate ×-1.2 + re-flood → escape-route-aware retreat, the trick a naive "move away" can't do), and **hunt** (sum a seek-gold map with a dread-monster map; the monster itself rolls downhill on a distance-to-agents map = textbook monster AI). All three verified by trace 2026-06-15 (flee: mean agent→goal dist rises 16→24 & plateaus; approach: falls to ~6 then respawns; hunt: gold collected, no crash). Rendered as a live contour-ring heatmap + downhill flow arrows. **Open: the verdict** — does this earn an engine API (a `dijkstra_map()` / flow-field core) or stay a cart-land reference like `astar`/`boids` per [ADR-0006](../decisions/0006-library-carts-not-engine.md)? Lines up behind `sightlines`: both are the "grids + agents" family, both lean cart-land so far |
| `sightlines` | is a **tilemap line-of-sight / FOV primitive** worth owning — generalizing the `acoustics` raycast *beyond audio* (enemy vision, stealth, lighting, fog-of-war all want "can A see B")? | the testbed, built 2026-06-15 on a shared `is_wall(x,y)` occluder grid (so a real `los.h`/`fov.h` would take that as a callback). Two candidates side by side: **single-ray LOS** (one Bresenham walk, lifted + generalized from `rogue.c`'s FOV ray — guards light red when they can see the eye) and **field of view** (recursive shadowcasting, Björn Bergström 8-octant — reveals what the eye sees, walls cast shadows). Both proven to render correctly headless. RogueBasin survey behind the algorithm choice ([Field of Vision](https://www.roguebasin.com/index.php/Field_of_Vision), [comparative study](https://www.roguebasin.com/index.php/Comparative_study_of_field_of_view_algorithms_for_2D_grid_based_worlds)): recursive shadowcasting = popular/fast/circular but asymmetric; [symmetric shadowcasting](https://www.albertford.com/shadowcasting/) is the upgrade if symmetry bugs bite. **Open: the verdict** — does this earn an engine API (a `los()` core + the shadowcast as a cart-land `fov.h`), or stay cart-land entirely per [ADR-0006](../decisions/0006-library-carts-not-engine.md)? Ties to STATUS #40 / [`spatial.md`](spatial.md) v3, where the acoustics probe flagged it as an "optional generalizing bonus" |
| `monochord` | does **fretting-by-finger-position** feel playable WITHOUT tactile frets? (one string, fingers are movable bridges, segment length = pitch — the đàn bầu / diddley bow / monochord gesture) | built 2026-06-14 on `pointer.h` + the [held-notes](held-notes.md) API (PD oscillator, so a ringing note glides as a bridge slides). Gesture verified working in a scripted run (bridge on the octave mark → reads E3). **Early verdict (2026-06-14, desktop A-G keyboard-bridges + mouse as 2nd finger): the gesture feels good — fretting-by-position is playable, not fiddly.** Validates moving to the payoff cart; real-finger multitouch feel still to confirm on a phone. The verdict gates the **string fretboard** payoff cart (chords as finger geometry, not button lookups — the honest version of `pedalboard`'s chord buttons), per [`multitouch-and-generalization-audit.md`](multitouch-and-generalization-audit.md). Touch ceiling caveat: full 6-finger chording exceeds the iPhone 5-touch cap, so the fretboard leans desktop/iPad |
| `sfxed`, `sfxgen` | engine sound banks / tracker UI (STATUS #5, #12) | direction is "no banks", but explicitly conditional: *"`sfx_def()` only if the prototype proves the engine should own banks"* — the probes are still gathering evidence in use |
| `bones` | offscreen canvas / baked rotation atlas (STATUS #13, [`baked-rotation-atlas.md`](baked-rotation-atlas.md)) | the animator that hits the realtime-drawing wall; whether the buffer primitive is needed is unproven either way |
| **the instrument cabinet** — `stylophone`, `musicalsaw`, `moog`, `modrack` | is the audio surface expressive enough? Each deliberately exercises a corner "no other cart touches" (live duty, live LFO, full patching, modular routing) | open by design — this family already surfaced the mod-envelope gap (shipped); it keeps probing as the synth grows |
| `bossa` *(in flight, another session)* | likely the groove/rhythm-timing question (STATUS #8) | unverified — its author should classify it here |
| `palettelab` | which default palette replaces the lifted PICO-8 one — and at what size? ([`palette-and-color.md`](palette-and-color.md) Layer 1/1b) | contact sheet live, nine candidates: shipped-32 / ENDESGA 32 / Resurrect 64 / E32+derived / ENDESGA 64 / AAP-64 / Famicube / Journey / Jehkoba64 — role-mapped, across swatch/ramp/sunset/portrait scenes **plus blendlab's blend scenes** (night glow, glass+fog, table grid) rebuilt from the live candidate — palette choice and blend fidelity judged together. Built on the **experimental** `palette_hex()` + 64-slot palette (see below) |
| `uikit` | does the `ui.h` widget-kit shape work — per-contact capture, hit-pad inflation, opt-in focus — across mouse + touch + keyboard? ([`ui-widgets-notes.md`](ui-widgets-notes.md)) | v1 (button/slider/knob; panel/drag-from deliberately deferred for a second customer) shipped 2026-06-07 with the showcase cart + the `sfxgen` retrofit (17 sliders + 11 buttons, behavior-faithful incl. undo, verified by scripted replay). Harness-verified on desktop; the **on-device run is the open half** — fat fingers, two-knobs-at-once, the 5-touch ceiling, per the §5.3 probe-before-polish rule |
| `voxlab` | the formant VOICE engine (`INSTR_VOICE`, a navkit VoicForm port — glottal pulse → 4 vowel formants): **which THREE of the seven raw params become the public `harmonics`/`timbre`/`morph` macros?** | the fat prototype: all 7 raw params on sliders, driven through the **experimental** `voice_param(handle, idx, value)` by-index poke; four candidate 3-axis *layouts* on buttons (gen = vowel/size/effort · mouth = two vowel axes · creat = vowel/size/growl · sing = vowel/effort/vibrato). Engine ported + verified non-silent (voiced, 0-clip) 2026-06-08; **the listen-by-ear pass to pick the axes is the open half** — until then nothing in `studioDocs.js`/`shell.js` is wired and the 3-macro mapping is unlocked |
| `say` | the voice's FOURTH control kind — **PROSODY** (pitch + timing + stress over a phrase): how far do today's primitives get us toward SPEAKING (not droning), and which gaps are real engine API? ([`voice-engine.md`](voice-engine.md)) | a `say()`-style sequencer over `INSTR_VOICE` using ONLY existing calls. **Key finding (built + rendered 2026-06-09, 3 phrases, 0-clip, crest ~15 dB = articulated not droning): connected speech works on ONE held note** — re-firing the experimental `voice_consonant()` resets `vox_cons_t`, re-articulating a consonant mid-note on a single `note_on`, so no new mid-note API is *required* to prototype. RANDOM builds varied **V/CV/VC/CVC** simlish (codas via `voice_coda()`) + cart-land diphthong glides, not a monotonous CV string. Pitch contour = per-frame `note_pitch()` along per-syllable shapes (flat/rise/fall/peak) + phrase declination; intonation by mode (statement falls · question rises · exclaim hard); emphasis = a clicked syllable gets a pitch accent (higher+longer); character presets (normal/robot/kid/giant/whisper) bundle timbre + pitch-offset/range/rate = the terminal-`say()` 'vary character' lever. **Open (the by-ear half): does re-fired onset read as a true inter-vocalic stop, or does speech need a dedicated mid-note consonant?** Surfaces 4 candidate engine gaps — no creak/jitter (robot can't be truly sterile / no rasp), only `b d g m n l s sh` (no `h w r f v` / unvoiced `p t k`), no schwa (unstressed over-enunciate), no first-class diphthong (a cart-land vowel-lerp fakes it in `say`) |
| `voxab` | the formant VOICE engine, **completeness audit vs navkit** ([`voice-engine.md`](voice-engine.md) §"Completeness audit"): of the levers we trimmed from VoicForm + the ones in navkit's *other* voice (WAVE_VOICE), **which earn a place — by ear?** | the A/B comparator voxlab can't be: two full param states, **TAB flips the LIVE sustained tone instantly** (difference lives in the flip, not slider-hunting); **solo-sweep** ramps one param with the rest frozen; auto-retrig so attacks are heard. Drives the **experimental** `voice_param()` indices 10–16 — the audit finds wired into the kernel 2026-06-10: **buzz** (glottal↔sine source), **jitter/shimmer/creak** (the net-new roughness trio — the one gap *neither* navkit voice had), **anti-formant nasality** (navkit WAVE_VOICE's notch model, to A/B against voxlab's config-morph nasality), **schwa-reduce**, a **measured-bandwidth toggle** (navkit's Klatt/Peterson-Barney BW rows vs our derived `60+fc*0.08`), and a **diphthong glide** (vow2/glide, idx 17/18) that reaches the 5 extra vowels the table-completion added (AE AH AW UH ER) — the consonant set is now navkit-complete too (22 ids: …ng r w y dh f v z zh th p t k ch). Preset A/B pairs (BW · NASAL · BUZZ · CREAK · REDUCE · VOWEL2) load a base differing in one lever for a direct flip-test. **Open: the by-ear verdicts** — which levers ship, which nasality model wins, whether measured BW is worth the table |
| `vox4` | the formant VOICE engine's **public API shape**: is 3 macros enough, or does ONE 4th continuous axis earn breaking the uniform mould — and which? ([`voice-engine.md`](voice-engine.md) §"is three continuous macros enough?") | the proposed lean API made playable: 3 fixed macro sliders (VOWEL / SIZE / EFFORT, effort = breath+open-q+tilt as one knob) + a 4th slider whose meaning TABs through the candidates (ROUGHNESS = jitter+shimmer+creak collapsed · NASALITY anti-formant · BUZZ source-edge). Vibrato shown as the orthogonal `note_lfo` modulator, not an axis; articulation stays the `voice_consonant()`/`coda()` events. **RESOLVED 2026-06-10 — API LOCKED + WIRED:** 3 macros (harmonics=VOWEL · timbre=SIZE · morph=EFFORT) + ONE earned 4th knob `voice_nasal()` + the two articulation calls; roughness/buzz/2D-vowel cut by ear. `vox4` rewritten to drive the real macro API (no `voice_param`). Verdict + wiring in [`voice-engine.md`](voice-engine.md) "Status — SHIPPED". |
| `voxpad` | the formant VOICE engine's **articulation set** — does the navkit-complete phoneme table (10 vowels + 22 consonants) actually *sound* like its phonemes, the new ones included? | a phoneme keyboard: click a vowel (all 10, via the vow2/glide diphthong path) + click a consonant to articulate "C + vowel" through the **experimental** `voice_consonant()`/`voice_coda()`. Holds a drone (consonants re-articulate mid-note, like `say`) or fires one-shot syllables; CV/VC mode; SIZE/EFFORT for tract + effort. The rig to hear the 14 appended consonants (`ng r w y dh f v z zh th p t k ch`) and 5 appended vowels (`ae ah aw uh er`) that no other voice cart triggers. **Open: the by-ear pass** — which new phonemes read true, which need tuning (and `h`, the one navkit row we *don't* have, stays unported) |
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
