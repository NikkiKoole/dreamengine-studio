# Compound blind spots — undocumented grands

> **STATUS: EXPLORING (2026-09-11).** Parking-lot hub, **not READY TO BUILD.**
> These are maker blind spots: seams that already ship, never given a scene.
> File a GitHub `grand` only when actually pursuing one. Do not copy this list
> into [`STATUS.md`](../STATUS.md) — that ledger is shipped / open / cut, not a
> wish pile. The umbrella for grands that *are* already filed is
> [#15](https://github.com/NikkiKoole/dreamengine-studio/issues/15).

A **compound grand** is a cart (or tiny tool) that *composes* capabilities we
already have. The work is the scene, not a new primitive. This page parks the
ones that never got their own design doc — so they stop living only in the
maker's head, and so nobody "unblocks" them by growing `studio.h`.

---

## The pattern

Same grain as the patch-match work and as [#15](https://github.com/NikkiKoole/dreamengine-studio/issues/15):

1. **Compose what ships.** The honest core is a *use* of existing seams, not a
   new engine family.
2. **Search / oracle / ear over invention.** If you cannot name the shipped
   pieces, it is not a compound — it is a feature request.
3. **Tiny or no new API.** Prove it in cart-land (ADR-0006) or as a file
   convention first. The four seams that might someday earn a one-liner are
   tabulated below; everything else on this page is a cart.
4. **One cart, one core** (ADR-0022). Composition is plumbing *between* focused
   carts, not a host OS, not an ECS, not a silent second engine.

Cite, don't relitigate:

| ADR | file | the rule this page leans on |
|-----|------|-----------------------------|
| 0006 | [`0006-library-carts-not-engine.md`](../decisions/0006-library-carts-not-engine.md) | richness lives in readable carts, not `studio.h` |
| 0015 | [`0015-effects-are-recipes-not-primitives.md`](../decisions/0015-effects-are-recipes-not-primitives.md) | a new FX must prove it cannot be a recipe |
| 0016 | [`0016-combo-organ-recipe-then-macro-or-engine.md`](../decisions/0016-combo-organ-recipe-then-macro-or-engine.md) | recipe first; engine only when a cart proves the recipe fails |
| 0017 | [`0017-three-macro-core-plus-engine-aux-channel.md`](../decisions/0017-three-macro-core-plus-engine-aux-channel.md) | three macros forever; no 4th |
| 0020 | [`0020-in-house-tool-curated-showcase.md`](../decisions/0020-in-house-tool-curated-showcase.md) | in-house tool + curated showcase, not a platform |
| 0022 | [`0022-collaboration-is-the-north-star.md`](../decisions/0022-collaboration-is-the-north-star.md) | one honest core per cart; beginner is critic, not audience |

---

## Already documented — skip

These already have a scene (a design doc, and often a `grand`). **Do not
re-spec them here.** Pursue them in their own file; this hub only points.

| grand | where the scene lives | GitHub |
|-------|----------------------|--------|
| Synplant cart (ear-as-loss, option B) | [`patch-matching-cart.md`](patch-matching-cart.md) — built 2026-09-13 on `patchbench` | [#16](https://github.com/NikkiKoole/dreamengine-studio/issues/16) |
| Editor patch-match drop (option A) | [`patch-matching.md`](patch-matching.md) · [`patch-matching-cart.md`](patch-matching-cart.md) | [#8](https://github.com/NikkiKoole/dreamengine-studio/issues/8) |
| Live looper on `sound_extin` | [`audio-input-frontier.md`](audio-input-frontier.md) · [`vocoder.md`](vocoder.md) | [#17](https://github.com/NikkiKoole/dreamengine-studio/issues/17) |
| `aumi` MIDI processor | [`auv3-plugin-types.md`](auv3-plugin-types.md) · [`midi-out.md`](midi-out.md) | [#18](https://github.com/NikkiKoole/dreamengine-studio/issues/18) |
| `jam.js` coprime ensemble | [`cart-os.md`](cart-os.md) | [#19](https://github.com/NikkiKoole/dreamengine-studio/issues/19) |
| Attract / idle demo / IAP storefront | [`attract-mode.md`](attract-mode.md) | — |
| Pro wall (export + shopfront) | [`pro-unlock.md`](pro-unlock.md) | [#3](https://github.com/NikkiKoole/dreamengine-studio/issues/3) · [#13](https://github.com/NikkiKoole/dreamengine-studio/issues/13) |
| Synth Secrets doing-ledger | [`synth-secrets-plan.md`](synth-secrets-plan.md) | — |
| Shared road grammar / realistic roadgen | [`roadkit.md`](roadkit.md) · [`worldgen-plan.md`](worldgen-plan.md) | — |
| **Umbrella** (filed children + anti-ideas) | this page is the *undocumented* twin | [#15](https://github.com/NikkiKoole/dreamengine-studio/issues/15) |

[#15](https://github.com/NikkiKoole/dreamengine-studio/issues/15) already names
anti-ideas (silent in-cart Genopatch / option D, live autotune-next, cart-OS
Workbench, new `INSTR_*` / 4th macro, stem-ML, compile-for-strangers, TikTok
API). Those stay out. Secondary leftovers it parked without filing
(attract-as-IAP-storefront, replay-takes gate, App Store review miner, guitar
pickup tap) are likewise not this page — attract has a doc; the rest are still
maker-only until someone writes one.

---

## Blind spots

Each entry is a *scene that was never written*: what already ships, what you
compose, what you must not invent. None of these is queued.

### 1. Spatial radio drive-by

**The scene.** You drive past a corner speaker. The whole groove — wet tail
included — pans, falls off, and Doppler-whooshes as one object.

**What ships.** Spatial v1 (per-voice) + v2 (emitter buses:
`instrument_pos` / `instrument_motion`) in [`spatial.md`](spatial.md). Showcase
`spatial` is a UFO, not a station. `radio.h` already *is* the station generator
a game cart would call.

**The snag that hid the scene.** Only one cart runs at a time. An in-world
radio is *your game cart* filling an emitter bus from `radio.h`, not "drop in
the `bossa` cart." v2 named this and then the drive-by never got built.

**Compose, don't grow.** `radio.h` + one emitter bus + `sloop` (or a tiny
drive-by toy). v3 acoustic zones stay proposed and are *not* a prerequisite.

### 2. Song-codec as universal cart documents

**The scene.** Any cart's authored state is a pasteable document — a seed when
the cart is a generator, a short blob when the player has written something —
and another cart (or the same cart tomorrow) can open it.

**What ships.** Radios already *are* a `u32` seed
([`song-codec.md`](song-codec.md)). `yachtrack`'s chart blob and `tracker`'s
`save_bytes` document are the first authored customers. `de_data_path` /
`de_dropped_file` (ADR-0025) already load a file at runtime.
[`cart-os.md`](cart-os.md) deflated "OS" to *save a file here, load it there*.

**The blind spot.** The codec doc is still "how Tinyjam shares a song." The
compound is the same envelope as a *universal cart document*: seed → lanes →
events, length-distinguished so old links never break, used by a game save, a
rack, a replay take, a road seed. Not a new filesystem. Not a Workbench.

**Compose, don't grow.** Bless the envelope in cart-land. `?seed=` boot is
already the cheap radio half; do not invent a 4th fidelity layer.

### 3. Scope + CPU-shader as rack identity

**The scene.** A rack's *face* is a living picture — `scope_read` into a CPU
shader — so the instrument is recognisable with the knobs off. Not a teaching
trilogy, not a HUD meter.

**What ships.** `scope_read` (lock-free oscilloscope feed). The shader trilogy
+ `shadermath.h` ([`cpu-shaders.md`](cpu-shaders.md): `shadelab` → `caustics` →
`raymarch`). Candy-style / [`acid-pets`](acid-pets.md) already treat a mascot as live readout.

**The blind spot.** Shaders stayed a lesson; scopes stayed a diagnostic. The
compound is identity: one honest picture that *is* the machine (a diode-ladder
ribbon, a freeze-cloud, a goniometer that is the master).

**Compose, don't grow.** `pset_rgb` + `scope_read` + `shadermath.h` inside the
rack that owns the sound. A second RGB buffer (below) is the only engine
maybe; the first cart does not need it (`pget_rgb` on the live canvas is the
stand-in `cpu-shaders.md` already uses).

### 4. Lockstep radio jam

**The scene.** Two people, two machines, one groove. Each plays; inputs travel;
both radios stay bit-identical. A missed note is a *performance*, not a
desync.

**What ships.** Input-lockstep + `net.h` ([`multiplayer-research.md`](multiplayer-research.md),
rungs 1–2 + 5). Radios are already deterministic (`radio.h` seed rule). The
harness `netdemo` gate exists.

**The blind spot.** Netplay demos are games (`netdemo`). Radios are single-player
generators. Nobody wrote the jam: lockstep the *inputs* (solo strip, a mutate,
a fill), let each side render its own audio — the OS does not have to mix
across the wire. Distinct from [#19](https://github.com/NikkiKoole/dreamengine-studio/issues/19)
(one machine, clock *file*, coprime toys) and from §5 (the file convention).

**Compose, don't grow.** A radio that already has `solo.h` / `improv.h` +
`--net-host` / `--net-join`. No new transport. Across-phone Link is *not* this
scene (that is cart-os's Ableton-Link caveat).

### 5. Shared live clock, file-first

**The scene.** Every musical cart on one machine derives `beat` from a tiny
live file `{bpm, t0, playing}` instead of running its own free clock. Kick /
snare / hat carts, a radio, a keybed — same pulse, OS mixes the audio.

**What ships.** `beat` / `bpos` as harness auto-fields; `play.js --bpm`; MIDI
clock + `sync.h` ([`external-clock-sync.md`](external-clock-sync.md)) for
*follow someone else*. [`cart-os.md`](cart-os.md) already named the file as the
only new primitive and demoted the Workbench.

**The blind spot.** [#19](https://github.com/NikkiKoole/dreamengine-studio/issues/19)
is the *coprime toy* that proves the file. The unwritten scene is the
*convention*: radios and racks opt into the same file without a `studio.h`
clock and without becoming a desktop OS. File-first; an accessor is vanity
until two real carts fail to agree on a path.

**Compose, don't grow.** A JSON/text file + `--data`. `sync_beats()` stays the
external-clock face (MIDI / AU / Link). Do not add a second public clock.

### 6. Grains-freeze product — **SHIPPED 2026-09-15** as `cloudhold`

**The scene.** Hold a chord, freeze, the room becomes a pad. Phrase-locker /
infinite sustain as a *thing you play*, not a demo knob on a teaching cart.

**What ships.** `grains` / `grains_freeze` / `grains_pitch` + instrument twins
([`audio-notes.md`](audio-notes.md); showcases `grains` + the `pedalboard`
GRAINS stomp). Capture-then-freeze sampling (`mic_record`) is a different
door.

**Shipped.** [`cloudhold`](../../tools/carts/cloudhold.c) is the product cart:
hold a chord pad → stomp FREEZE → play the keybed over the cloud → unfreeze.
Boutique-pedal identity on existing `FX_GRAINS`. Not the `grains` lab, not
`grainchop`'s sampler spike, not a new primitive (ADR-0015).
[#29](https://github.com/NikkiKoole/dreamengine-studio/issues/29) · live
`https://mipolai.com/dreamengine/cloudhold/` · Linux proof
`bash tools/clips/cloudhold/render-nr.sh`.

**Compose, don't grow.** One cart, one gesture, existing `FX_GRAINS`. A new
freeze primitive fails ADR-0015 on arrival.

### 7. Flight-recorder → cart ghosts

**The scene.** You race your last lap. You watch yourself fail the jump. A
ghost is a second entity fed a take, not a video and not a song.

**What ships.** Flight recorder v1 ([`flight-recorder.md`](flight-recorder.md):
always-on `--det --record`, keep-take → `tools/clips/`). Harness
`inject_input` for whole-session replay ([`attract-mode.md`](attract-mode.md)
wants the *handoff*). [`input-recording-looper.md`](input-recording-looper.md)
already split the idea: music loops want control events; ghosts want packed
input into a *second* body.

**The blind spot.** The recorder is a *tool* (clips, `rr`-style repro). The
ghost is a *cart-facing* consumer of the same take. Attract is the idle-demo
cousin and has its own doc; this is the in-play ghost (racer, ghost-you,
shadow dancer).

**Compose, don't grow.** Read a `.rec` / clip take inside the cart and drive a
second actor. Cart-facing inject (below) is the only engine maybe; a first
ghost can parse the take in cart-land.

### 8. `param_bind` + TCC [live-coding](../guides/live-coding.md) demo

**The scene.** A knob is a host parameter *and* a C identifier. You edit the
cart, libtcc hot-reloads, `de_state()` survives, the DAW lane still moves the
same float. The console as a live-coding instrument.

**What ships.** `param_bind` ([`host-parameters.md`](host-parameters.md) — the
parameter *is* the knob). libtcc hot-reload ([`cart-as-script.md`](cart-as-script.md);
editor live mode). `de_state()` already outlives a swap.

**The blind spot.** Each half has a doc. The compound — bind three floats,
hot-reload the shader/voice *around* them, host records the lane — never got a
demo cart. ADR-0020 still applies: this is an in-house tool scene, not
compile-strangers'-C.

**Compose, don't grow.** A tiny cart that binds what it already owns + the
existing live run mode. No cart-created engine instance (the option-D trap in
[`patch-matching-cart.md`](patch-matching-cart.md)).

### 9. MIDI-out radios

**The scene.** A radio is a MIDI generator. Its improv line, its drums, its
bass leave the machine and play someone else's piano — voice still on, pattern
alongside, never instead.

**What ships.** `runtime/midi_output.h` + `midi_send_*` ([`midi-out.md`](midi-out.md);
`acidcandy` already sends). `improv.h` / `solo.h` / `radio.h` already
*originate* notes. The doc's own "later" list names radio auto-solos and then
stops.

**The blind spot.** MIDI-out landed on the sequencer rack. Thirty-plus radios
still die at the speaker. The compound is one radio (or `radio.h`) emitting
what it already schedules.

**Compose, don't grow.** Call `midi_send_*` from the station that already has
the line. Slide encoding + AUv3 `MIDIOutputEventBlock` are [#18](https://github.com/NikkiKoole/dreamengine-studio/issues/18)'s
problem, not this cart's.

### 10. OSM / `.rvb` as game content

**The scene.** Rotterdam is the level. Missions, traffic, a drive, a chase —
the map is real, the rules are the cart's.

**What ships.** `osm-roads.js` → `.rvb` → `de_data_path` + `json.h`
([`external-data-carts.md`](external-data-carts.md), ADR-0025). Consumers today
are *viewers / drive benches* (`roadview`, `citydrive`, `sloop` on a generated
or loaded city). [`worldgen-plan.md`](worldgen-plan.md) / [`roadkit.md`](roadkit.md)
are the *synthetic* twin and already have their own grands.

**The blind spot.** External data proved "swap the file, don't rebake the
cart." It did not prove "the file is the campaign." Content ≠ render.

**Compose, don't grow.** One game cart + a committed `.rvb` + existing
`road_at` / drive seams. No engine parser. No "OSM entity system."

### 11. Glyph-puppet beat-synced faces

**The scene.** The station has a face. Glyphs swap on the kick, squash on the
snare, the mouth opens on the 303 accent — one shared bank, no baked frames,
locked to `beat`.

**What ships.** [`glyph-puppet.md`](glyph-puppet.md) + `glyphpup` (swap / nudge
/ proposed scale). fmbox's LCD dancer. Radio `every()` / `beat`. Acid-pets
already map *machine state → creature*; they are candy sprites, not the glyph
channel.

**The blind spot.** The puppet is an animation experiment. The radios have
clock. Nobody made the dancer *the station*.

**Compose, don't grow.** `glyphpup` channels driven by the same `every()` the
groove already uses. No animation API. Scale/squash stays the proposed
`sspr` channel, not a new primitive.

### 12. Mic as world sensor (ADR-0032)

**The scene.** The room is a joystick. `mic_level` is wind, crowd, occupancy;
`mic_pitch` is a weather vane or a puzzle key. Not a vocoder, not a sampler —
the world *hears*.

**What ships.** `mic_level` / `mic_pitch` as controller axes
([`mic-and-sampling.md`](mic-and-sampling.md), [`audio-input-frontier.md`](audio-input-frontier.md)).
Proof carts are instruments (`humtheremin`, `pitchscope`). Capture-then-freeze
stays deterministic; live-through is live-only
([ADR-0032](../decisions/0032-live-mic-effects-are-live-only.md)).

**The blind spot.** The ear opened three doors; door 1 (controller) was used
for *sound*. The compound is a game that treats the mic like `btn()` — and
declares the live-only carve-out in `de:meta` instead of pretending a `.rec`
will replay the air.

**Compose, don't grow.** `mic_start` + existing game input. Freeze first if
the run must `spec()`; otherwise mark live-only and do not record PCM into the
take (ADR-0032 already refused that). Not [#17](https://github.com/NikkiKoole/dreamengine-studio/issues/17)
(that's the pedal / looper on the extin ring).

---

## Tiny API that would actually help

Four one-liners, each earned only if a cart above *fails* without it. File- or
cart-land first; then the four-place `studio.h` ritual. Not a shopping list.

| maybe | for | until then |
|-------|-----|------------|
| Shared clock | §5 (and [#19](https://github.com/NikkiKoole/dreamengine-studio/issues/19)) | a live `{bpm,t0,playing}` file + `--data`; `sync.h` already covers external follow |
| 2nd RGB buffer | §3 multipass identity | `pget_rgb` on the live canvas; [`cpu-shaders.md`](cpu-shaders.md) idea #5 |
| Cart-facing ghost inject | §7 | parse a `.rec` in the cart; harness `inject_input` stays a tool |
| `de_out_path()` | §2 / [`cart-os.md`](cart-os.md) | write a known path by hand; mirror of shipped `de_data_path()` |

### Vanity — refuse these

These keep getting proposed as "the thing that would unlock the grands." They
unlock nothing on this page and they fight a settled ADR or a cut:

| vanity | why it is not the unlock |
|--------|--------------------------|
| 4th live macro | [ADR-0017](../decisions/0017-three-macro-core-plus-engine-aux-channel.md) — aux channel / per-voice controls / articulation, never a universal 4th |
| FX zoo (generic `instrument_fx`, another tank, octaver-as-primitive) | [ADR-0015](../decisions/0015-effects-are-recipes-not-primitives.md) — recipe first; no cap, but no zoo |
| Engine ECS / God-struct | [ADR-0002](../decisions/0002-typed-static-pools-over-entity-system.md) — typed static pools; `.rvb` is data, not entities |
| `hud()` | cut — one shared status bar makes every cart identical |
| `music()` pattern bank | [ADR-0013](../decisions/0013-cut-music-api.md) — radios and `note()` already won |
| Silent in-cart offline render | [`patch-matching-cart.md`](patch-matching-cart.md) option D — races the audio thread, needs a second engine instance; [#15](https://github.com/NikkiKoole/dreamengine-studio/issues/15) lists it as an anti-idea |

---

## Anti-blind-spots

This page is **not**:

- a READY-TO-BUILD queue (nothing here is specced far enough to start)
- a second [`STATUS.md`](../STATUS.md) (do not paste these into Open)
- a place to file GitHub issues in bulk (label `grand` **when pursuing**, so
  defects / Pro / gates stay the front of the board)
- a license to grow `studio.h` "so the scene is possible"
- a rewrite of [#15](https://github.com/NikkiKoole/dreamengine-studio/issues/15)
  or of the skip-table docs
- a Workbench, a platform, or compile-for-strangers (ADR-0020 / ADR-0022)

If a row wants new DSP, a 4th macro, an ECS, or a silent second engine, it has
left this page.

---

## Graduation

A blind spot leaves this hub in **one** of two ways — then, and only then, it
may appear on [`STATUS.md`](../STATUS.md):

1. **Own READY doc.** Write `docs/design/<slug>.md` with a real scene, a
   compose-list, a done-when, and `STATUS: READY` (or BUILDING). Link it from
   [`README.md`](../README.md). Delete or shrink the row here.
2. **Label `grand`.** File a GitHub issue, parent it under
   [#15](https://github.com/NikkiKoole/dreamengine-studio/issues/15) if it is
   that shape of bet, and keep the design in *its* file. This hub stays
   EXPLORING. When the cart ships, shrink the row to a pointer (that's §6
   `cloudhold` / [#29](https://github.com/NikkiKoole/dreamengine-studio/issues/29)).

Do not graduate by adding an Open bullet to the ledger alone. Do not mark this
file READY — it is a parking lot. When every row has graduated or been
honestly dropped, this page can shrink to a pointer at [#15](https://github.com/NikkiKoole/dreamengine-studio/issues/15)
and the skip table.
