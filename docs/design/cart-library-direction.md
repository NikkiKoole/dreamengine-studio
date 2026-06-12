# Cart library direction — what to build next

> **Exploratory.** A snapshot analysis (2026-06-03) of the ~201-cart library and an
> opinionated take on where the *next* carts should go. Not a committed backlog — it's
> the "stop and look at the whole shelf" memo. Re-run the counts before acting; the
> library moves.

## The shelf today (201 registered carts)

| Kind | Count | Read |
|---|---|---|
| **game** | 122 | **Saturated.** Essentially the entire retro arcade/console/strategy canon + many modern indies. |
| tech-demo | 37 | Strong, broad. |
| tutorial | 28 | **Thin for a *learning* tool** — see gap #1. |
| instrument | 12 | Distinctive, deep — a real identity. |
| toy | 7 | **Underweight vs. the brand** — see gap #2. |
| tool | 4 | Niche, fine. |

The headline: **the game shelf is essentially done.** Pac-Man, Doom, Civ, XCOM, Elite,
Lemmings, Transport Tycoon, Football Manager, *three* rhythm games (smooch lounge,
beatcrypt, geometry dash) — it's all there. A 123rd retro clone adds to the pile but
moves neither the mission nor the API. **The recommendation is explicitly NOT "more
games."**

## Direction, in priority order

### 1. Fill the tutorial on-ramp (highest leverage)

The north star (VISION.md) is "a teenager makes something that moves in under an hour and
learns real programming." But there's a **cliff**: tutorials 1–24 teach primitives (draw,
input, sprites, sound, easing, particles, juice), then the next step is a 122-game wall of
finished, complex carts. Nothing teaches a beginner how to *assemble* a complete small
game. Missing, specifically:

- **A capstone "build one complete game start-to-finish" series** (tutorials ~25–29):
  title → play → game-over states, a win/lose loop, tilemap collision, score persistence.
  This is the single biggest gap relative to the stated mission.
- **The `STATE { } / S->field` sugar** — every starter cart uses it; no tutorial teaches it.
- **The entity-pool pattern** (`Enemy e[64]; bool on;` + skip-inactive loop) — VISION.md
  calls this *the* core pattern of the whole corpus, yet it only ever appears implicitly
  inside finished games.
- **Tilemap + collision** as its own lesson (#10 builds a world, but nothing teaches
  colliding with it).

### 2. More generative / creative toys (most on-brand)

Only 7 toys, but they're the most differentiated and shareable carts (mandala, bloom,
music garden, the love parade) — the literal embodiment of "a creative toy rather than a
professional tool" and the "almost impossible to make ugly" idea. Underweight relative to
how much they define the project. Candidates:

- an **audio-reactive visualizer** (there's a world-class synth and *zero* things that
  watch the sound),
- a **cellular-automaton playground**,
- a **fractal / strange-attractor explorer**,
- a **paint-with-particles fountain**.

Bonus: these exercise the newer shipped API (`ngon`/`star`/`poly`/gradients, `fillp`) that
few carts actually use yet.

### 2b. More whimsical playable instruments (2026-06-07)

The instrument shelf (otamatone, musicalsaw, glassharmonica, hurdygurdy, stylophone,
fartsynth…) is the same "creative toy" DNA and these carts double as future rack
voices ([`tinydaws.md`](tinydaws.md)). Split by what they ride on — and note the
pattern at the bottom.

> **Update — new-sound scoring + engine-status correction (2026-06-10).** Cross-referenced
> against the now-complete recipe palette
> ([`../guides/instrument-recipes.md`](../guides/instrument-recipes.md)). **The "blocked on
> missing engines" list below is STALE: `MEMBRANE`, `REED`, and `BOWED` have all shipped** —
> the hand-drum, the bellows trio, the violin/cello (and an **arco** upright) are *buildable
> today*. (**Brass too, as of 2026-06-10** — `INSTR_BRASS` shipped; the cart's in progress.
> *No instrument is engine-blocked anymore.*) Scored here by **new sonic
> territory** (does it make sound the *playable* carts can't?) — a different axis from the
> fun × showcase ranking below. Highest new-sound = taking an engine that today lives only in
> a showcase/radio and making it *playable* for the first time.
>
> | new-sound | instrument | engine | why |
> |---|---|---|---|
> | ★★★★★ | hand-drum (tabla/conga) | `MEMBRANE` ✅ | the drumhead engine into a **touch** instrument for the first time — touch-position = strike-position macro, 1:1 |
> | ★★★★★ | bellows / melodeon | `REED` ✅ | first *played* self-oscillating reed; **bisonoric** push/pull = different note (accordion/harmonica share the voice) |
> | ★★★★☆ | violin / cello | `BOWED` ✅ | first *played* friction string; drag = bow pressure. Also unblocks **arco upright bass** (the "waits on bowed" note below is now stale) |
> | ★★★★☆ | finger choir / talkbox / Dalek | `VOICE` ⚠️ | morphing vowels — a new sonic class; discounted because VOICE's public macros aren't locked |
> | ★★★☆☆ | **clanky pots & pans** 🆕 | `FM` clang + detuned `SQUARE` + inharmonic `MALLET`/`MEMBRANE` | a *played, per-object-tuned* junk-metal kit; the clang recipes exist scattered (`fm/clang`, `cr78/metal-beat`, `808/cowbell`/`cymbal`) but no cart assembles them into an instrument |
> | ★★★☆☆ | slide guitar | `PLUCK` + `note_glide` | glissando on a plucked string — a combo no cart uses |
> | ★★★☆☆ | **Juno** poly synth 🆕 | raw `SAW`/pulse + sub + resonant `LP` | lush **poly** pad/stab — a real gap (our synths are mono leads/basses). Caveat: osc+filter territory is dense (sh101/tb303/moog) and the signature **chorus** leans on the pending effects bus (fake meanwhile with detuned layers / `LFO_PAN`) |
> | ★★★☆☆ | **upright bass** (arco + pizz) 🆕scope | `BOWED` ✅ + `PLUCK` | arco double-bass now buildable (bowed shipped); pizz + noise-slap is the lesser-new half |
> | ★★☆☆☆ | slide whistle | `SINE` + glide overshoot/bounce | charming + maximally touch-native, but SINE glides already exist (theremin/glass) |
> | ★★☆☆☆ | music box | `TRI` + run-down | the novelty is the run-down *mechanic* (pitch/tempo sag), not the timbre |
> | ★★☆☆☆ | standing bass (pizz-only) | `PLUCK` low + noise slap | mostly a *better* version of the already-faked upright (see the arco entry instead) |
> | ★☆☆☆☆ | barrel/crank organ | `USER0` drawbars | re-treads roadhouse's combo-organ / stylophone's drawn organ; the crank wobble is the only-new bit |
> | ★★★★☆ | slide trombone / brass | `BRASS` ✅ *shipped* | first *played* lip-reed brass — the slide gesture rides the live glide; horn fakes (addis/dub/citypop/yacht) can move onto it. **Shipped 2026-06-10** as the `brass` cart (trombone slide). |
>
> **New-sound ≠ build priority.** The barrel organ scores lowest here yet remains the top
> *buildable* pick below (tinydaws gateway, great gesture). If the goal is "cover the most new
> sonic ground," build the **hand-drum, bellows, and violin** first — each makes a powerful,
> currently-showcase-only engine *playable*, the biggest untapped sound in the library (cf.
> the palette's "GUITAR/PIPE/BOWED/PIANO/VOICE have no consumers yet").
>
> **Also in the running — the Raymond Scott "pre-Roland wing"** (currently parked in
> [`../STATUS.md`](../STATUS.md), classic-machine list): the **Circle Machine** (~1959) — a
> *circular* step sequencer (rotating photocell sweeps a ring of brightness knobs) — scores
> low on *new sound* (it reuses existing oscillators) but high on **new interaction**, a
> genuinely fresh UI unlike any music cart here; the **Clavivox** (keyboard theremin) is a
> small `note_glide` instrument. Listed here so they're not lost in STATUS.
>
> **Two more candidates (2026-06-11)** — each shows off an engine move no cart leans on:
>
> | new-sound | instrument | engine | why |
> |---|---|---|---|
> | ★★★☆☆ | **vibraphone** 🆕 | `MALLET` + `LFO_VOLUME` | jazzy tap bars; the **motor tremolo** (spinning-disc resonators = `instrument_lfo(…, LFO_VOLUME, rate, depth)` with a dial-able rate) is unused by any cart. Exists only as a *preset* in `mallet.c` today — make it a played instrument. Very mobile-friendly. Low effort, copies the `mallet`/`handpan` chassis. |
> | ★★★★☆ | **Ondes Martenot** 🆕 | `SINE`/`VOICE` + live `note_pitch`/`note_vol`/`note_cutoff` + `note_glide` | the expressive ribbon synth (Radiohead / film scores): drag-ribbon = pitch, *touche d'intensité* = volume swell, timbre stops = filter. The most expressive thing buildable; a spiritual upgrade to the theremin. Extends `heldnotes.c`. No engine gap — ribbon/touche are cart-land gestures. **→ SHIPPED 2026-06-12** as the `martenot` cart (ribbon-first: X = pitch, Y = swell; touche d'intensité; combinable wave-stops; four diffuseurs; a detuned twin + slow drift for the eerie character). Its ribbon+touche chassis is the natural base for a **singing/`VOICE` instrument** — a vocal-theremin where Y morphs vowels (a strong candidate, cf. the "finger choir" row above). |
>
> **Checked and NOT new:** *Gamelan* — already shipped as **gamelan radio** (station #19,
> 2026-06-09); it already does the *ombak* detuned-pair shimmer via `instrument_tune()`.
> *Music box* — already on this backlog (rows below); refinement worth noting: glassy
> `MALLET`/glock timbre + a hand-crank whose speed = tempo, on top of the run-down mechanic.

**Buildable today** (current engines), ranked by fun × feature-showcase value:

1. **Barrel/crank organ** — the top pick. Crank it with a finger-circle gesture
   (speed = tempo, wobble = pitch warble); the tune is a **punch card you poke
   holes in**. Shows `wave_set` drawbars (roadhouse proved the voice) — and the
   punch card is secretly a baby tinydaws lane editor. Gateway drug.
2. **Hang drum / handpan** — circle of tone fields, `INSTR_MALLET`. Possibly the
   most *satisfying* touch instrument there is; shows mallet's harmonics/morph
   macros beautifully. **→ SHIPPED 2026-06-07** as the `handpan` cart: the scale
   zigzag (neighbors consonant), strike offset = per-hit timbre macro, shoulder
   tok, four tunings (kurd/celtic/hijaz/pygmy), full multitouch. Published to the
   web gallery same day.
3. **Music box** — wind it up, it plays itself and *runs down* (tempo + pitch sag
   as the spring dies). TRI + clockwork ticks; shows `bpm()`-as-a-voice (tango's
   conductor trick) in toy form.
4. **Slide guitar** — `INSTR_PLUCK` + `note_glide`, a combo no cart has used.
   Bottleneck drag = a great one-finger gesture.
5. **Standing bass (upright)** — PLUCK low register + a noise slap transient; the
   fingerboard is the touch surface. Autopilot = cocktail.c's `walk_note()`
   (already a graduation candidate in game-music.md) — *tap a chord, the bass
   walks itself*. Arco mode waits on the bowed engine.
6. **Slide whistle ("pulling whistle")** — cheapest of all: `INSTR_SINE` held
   note + `note_glide` with overshoot + bounce. One finger = one plunger — the
   most touch-native instrument imaginable; already Space-Age rack whimsy in
   tinydaws.md.
7. **MT-70 Casiotone preset keyboard** (decided 2026-06-07: cart-land first, per
   the second-customer rule — instrument-engines §8.9 corrected note has the full
   story). **→ SHIPPED 2026-06-07** as the `mt70` cart, built slightly simpler than
   sketched: **stacking is the primary implementation for all 10 presets** (1–3
   `note_on`s per key on separate slots, `note_pitch` float correction for exact
   ratios — 3.0 = +19.02 st), because the Chime's perfect-4th interval is outside
   mallet's fixed modal ratios; `INSTR_MALLET` is instead the **SRC A/B switch** on
   the five struck presets (recipe vs engine, judged by ear). Trace-verified:
   VIBES key = 2 voices, BELLS key = 3, mallet mode = 1. Plus vibrato, the
   obligatory demo tune, and a voices/16 meter with an oldest-key budget guard.
   Open tail: the owner ear pass (preset taste-tuning, A/B verdict) + the
   probe-carts.md findings entry. Doubles as the **probe cart for the Additive
   engine**: its tuned ratio/decay table is the future engine's macro homework,
   and presets re-bake as macro positions when that engine lands.

**Was blocked on missing engines** ([`instrument-engines.md`](instrument-engines.md) §8.9
owns the "which engine next" call) — ⚠️ **mostly UNBLOCKED as of 2026-06-10** (membrane,
reed, bowed, and the experimental formant voice have all shipped; see the scoring update
above) — and **brass shipped 2026-06-10** too, so *nothing here is engine-blocked anymore*.
The interaction notes still stand:

- **Hand-drum (tabla/conga)** — ✅ membrane shipped (tabla.c is the showcase). The killer pairing: touch
  *position* on the drumhead = the engine's strike-position macro. Touch API and
  engine physics map 1:1 — the best feature showcase on this list once membrane
  lands.
- **The bellows trio** — ✅ reed shipped (reed.c showcase; gamelan's suling uses it). Still fakeable on tango.c's drawn
  bandoneón `wave_set` wave; the squawk/breath character wants the engine).
  One shared voice + three interaction skins — build the bellows once:
  - **Melodeon** — THE interaction gem: **bisonoric**, the same button sounds a
    *different note* pushing vs pulling. Bellows = drag direction (or two-finger
    squeeze); direction-changes-the-note is a uniquely touch-friendly mechanic
    no desktop instrument has.
  - **Accordion** — unisonoric + Stradella chord buttons: omnichord's
    chord-button layout meets a bellows whose drag *speed* is the volume swell
    (no bellows motion, no sound).
  - **Mouth harmonica** — bisonoric again (blow/draw), 10 holes, richter tuning;
    vertical drag on a hole = the draw bend, cupped-hand wah = `note_cutoff`.
- **Finger choir** (drag voices through vowels) + **talkbox / Dalek voice toy** — ⚠️ formant voice shipped but EXPERIMENTAL (voxlab/vox; macros not locked) —
  need **formant** (+ AM/ring for the Dalek).
- **Violin / cello** — ✅ bowed shipped (bowed.c showcase; arco + pizzicato). Drag speed = bow pressure, another
  gesture-to-physics 1:1.
- **Slide trombone** — ✅ `INSTR_BRASS` shipped (2026-06-10, lip-reed waveguide that slides/glisses live); the `brass` showcase cart shipped alongside, marquee = the draggable trombone slide. The slide gesture finally has its engine.

The pattern worth noticing: the blocked ones are all *continuous-gesture*
instruments — exactly why instrument-engines §8.9 says the wind/bowed group "pairs with held notes
and live macros." The touch work on the instrument shelf is the other half of
that pairing.

### 3. The few genuinely-missing, *teachable* game types

Not "another clone" — each earns its place:

- ~~**A minimal one-button game (Flappy-style).**~~ **→ SHIPPED 2026-06-07** as `flappy`:
  the full attract→play→game-over arc (not just the mechanic), `save_int` hi-score + medal,
  programmatic sprite-draw bird with a sampled flap cycle, sspr_ex tilt+squash, feather
  particles, flash/shake/hit-stop, parallax sky/ground. One-button on every device
  (`tapp` full-screen + btnp + key) → phone-playable. The tutorial↔big-game bridge, built.
  (Also tutorial-curriculum Track C #40.)
- **An idle / incremental ("numbers go up").** Trivial to code, genuinely engaging, and
  the *first game a beginner can finish and feel proud of.* None exist.
- **A typing game** (Typing of the Dead style) — educational *and* shows off
  `text_input`/`key`. Only the type-&-save tutorial touches typing today.
- *(Softer)* a chill fishing/timing game.

## What was checked and is NOT a gap

Confirmed already covered, so don't propose these as new:

- **Rhythm games** — covered three ways: `smooch lounge` (DDR/lane), `beatcrypt`
  (Crypt-of-the-Necrodancer beat-locked roguelike), `geometry dash` (rhythm runner).
- **State machines / title screens** — present in finished games (street fighter, chess) —
  but *not taught* (that's gap #1, not a content gap).

## One-line answer

Stop cloning the canon — it's complete. Spend the next batch on **(1)** a numbered
"make a whole game" tutorial series, **(2)** a few generative toys, and **(3)** the three
missing small/teachable games (flappy, idle, typing).

> **Progress (2026-06-07):** the *quickest high-value win* — **flappy — SHIPPED** (gap #3
> above; tutorial-curriculum Track C #40), proving the whole-game arc. Gap #2 (toys) also
> largely closed since this memo: toys 7 → 29, instruments 12 → 40. **Still open and now
> the highest-leverage next move: gap #1's Track A** ("how code works" — variables, loops,
> functions, structs, the entity pool; 0 of 8 built), plus the other two teachable games
> (**idle/incremental**, **typing**). Re-run the counts — this 2026-06-03 memo is stale.
