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

**Blocked on missing engines** ([`instrument-engines.md`](instrument-engines.md) §8.9
owns the "which engine next" call):

- **Hand-drum (tabla/conga)** — needs **membrane**. The killer pairing: touch
  *position* on the drumhead = the engine's strike-position macro. Touch API and
  engine physics map 1:1 — the best feature showcase on this list once membrane
  lands.
- **The bellows trio** — needs **reed** (fakeable today on tango.c's drawn
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
- **Finger choir** (drag voices through vowels) + **talkbox / Dalek voice toy** —
  need **formant** (+ AM/ring for the Dalek).
- **Violin / cello** — needs **bowed**; drag speed = bow pressure, another
  gesture-to-physics 1:1.
- **Slide trombone** — needs **brass**; the slide gesture is begging for it.

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
missing small/teachable games (flappy, idle, typing). Quickest high-value win: the
**flappy-style one-button cart**.
