# API design notes — what to steal from the classics

> **Genre: design exploration (scratchpad).** This is a *design-reference* doc: rationale
> and proposed signatures for the engine API, surveying what PICO-8, GameMaker, BlitzMax,
> DIV, p5.js, and Scratch offer and which abstractions earn their keep in a dreamengine
> cart. It is allowed to be messy and exploratory.
>
> It is **not** the status ledger and **not** the decision log:
> - "What's shipped / open / cut?" → [`../STATUS.md`](../STATUS.md).
> - "Why did we (not) do X?" once settled → [`../decisions/`](../decisions/README.md).
>
> The ✓ markers below are design-time notes; STATUS is authoritative for state.

---

## What dreamengine has today

For reference, the current API (all of `studio.h`) groups into:

| Group | Symbols |
|---|---|
| **Callbacks** | `update`, `draw`, `init` |
| **Input — buttons** | `btn`, `btnp`, `BTN_*` |
| **Input — touch** | `touch_count`, `touch_x`, `touch_y`, `tap`, `stick_x/y`, `touch_controls` |
| **Input — mouse + keyboard** | `mouse_x`, `mouse_y`, `mouse_down`, `mouse_pressed`, `mouse_released`, `mouse_wheel`, `key`, `keyp`, `text_input`, `KEY_*` |
| **Graphics** | `cls`, `spr`, `sprf`, `sspr`, `sspr_ex`, `spr_rot`, `pset`, `pget`, `print`, `print_centered`, `print_right`, `print_scaled`, `text_width`, `line`, `rect`, `rectfill`, `circ`, `circfill`, `oval`, `ovalfill`, `tri`, `trifill`, `tritex`, `bar`, `camera`, `clip`, `colorkey`, `map_scale`, `fillp`, `fillp_reset`, `pal`, `pal_reset`, `fade`, `shake`, `FILL_*` |
| **Map** | `mget`, `mset`, `map`, `MAP_W`, `MAP_H` |
| **Sound** | `sfx`, `music`, `note`, `hit`, `tone`, `chord`, `strum`, `schedule`, `bpm`, `beat`, `beat_pos`, `every`, `euclid`, `chance`, `degree`, `instrument`, `instrument_duty`, `instrument_lfo`, `instrument_filter`, `INSTR_*`, `LFO_*`, `FILTER_*`, `SCALE_*`, `CHORD_*` |
| **Math** | `abs`, `min`, `max`, `clamp`, `mid`, `sgn`, `lerp`, `remap`, `distance`, `length`, `angle_to`, `dx`, `dy`, `sin_deg`, `cos_deg`, `fsqrt` |
| **Collision** | `boxes_touch`, `circles_touch`, `near`, `point_in_box`, `touching_map`, `tile_at`, `touching_color`, `bounce_at_edges` |
| **Animation** | `anim`, `anim_once`, `blink` |
| **Easing** | `ease_in`, `ease_out`, `ease_in_out` |
| **Noise** | `noise`, `noise2`, `noise3` |
| **Persistence** | `save`, `load`, `save_bytes`, `load_bytes` |
| **Time** | `now`, `frame`, `dt`, `timer`, `timer_reset`, `epoch` |
| **Strings** | `str` |
| **Random** | `rnd`, `rnd_between`, `rnd_float`, `rnd_float_between` |
| **Camera** | `camera`, `follow` |
| **Turtle / pen** | `turtle_home`, `turtle_move`, `turtle_turn`, `turtle_face`, `turtle_at`, `pen_down`, `pen_up`, `pen_color`, `pen_size` |
| **Debug** | `watch`, `watch_visible`, `printh` |
| **Palette** | 32 `CLR_*` constants |

Roughly **~125 functions + ~90 constants** in `studio.h` today. Strong across the board. **Still missing: events, Strudel extras, Dilla groove timing, gamepad, and pause/fps debug.** (Since this table was first written: sprite rotation/scale `spr_rot`/`sspr_ex` (§18); the juice batch `pal`/`fade`/`shake`/`print_scaled` and `fillp` patterns (§19); the four-axis **instrument synth** `instrument`/`instrument_duty`/`instrument_lfo`/`instrument_filter` (see audio-notes §10); and `tritex` textured triangles.)

---

## The classics, briefly

### PICO-8 — minimalism as a feature

The whole API is ~80 functions, deliberately sparse. Strengths:

- Sprite system is just `spr(n, x, y, [w, h, flip_x, flip_y])`.
  `sspr` is the stretched version. No image objects, no transforms
  beyond flip. **We have this.**
- **Map**: `mget(cx, cy)` and `map(cx, cy, sx, sy, cw, ch)`. **We have this.**
- **Sound**: data-driven (`sfx(n)`, `music(n)`). Notes/octaves live in
  a tracker UI, not in the API. *We diverged here — code-first.*
- **Input**: `btn`, `btnp` with optional player. **We have this.**
- **Math helpers**: `abs, ceil, flr, max, min, mid, rnd, sin, cos,
  atan2, sqrt, sgn`. Crucially, `sin/cos/atan2` take **turns** (0..1)
  rather than radians — odd, but it makes "spin once" `+= 1/60`,
  which is genuinely friendly. **We have `mid`, `rnd`, `sgn`; the
  rest are absent.**
- **Strings**: `tostr, tonum, sub, ord, chr`. *We have nothing here.*
- **Persistence**: `dget(slot)`, `dset(slot, val)` — 64 numbers
  saved per cart. *We have nothing here.*
- **Collision**: nothing built-in. Carts roll their own with
  `mget` for tiles and AABB checks for sprites. PICO-8 chose
  minimalism over convenience.
- Pixel-level: `pset`, `pget`, `fillp` (fill pattern). **We have
  the first two.**
- "Peek/poke" memory access — too low-level for our audience,
  skip.

### GameMaker — kitchen sink, but with gems

Massive surface. The genuinely useful highlights:

- **Collision**: `place_meeting(x, y, obj)`, `collision_circle`,
  `collision_line`, `collision_rectangle`, `collision_point`,
  `point_in_rectangle`, `rectangle_in_rectangle`. Instance bounding
  boxes via `bbox_left/right/top/bottom`.
- **Trig**: `lengthdir_x(len, dir)` and `lengthdir_y(len, dir)` —
  hands-down the most useful 2D game function ever invented. Move
  any sprite at any angle in one expression:
  `x += lengthdir_x(2, dir);` Also `point_distance`,
  `point_direction`, `dcos`, `dsin` (degree-based trig).
- **Math**: `lerp(a, b, t)`, `clamp`, `sign`, `abs`, `power`,
  `lengthdir_*`.
- **Drawing transforms**: `draw_sprite_ext(spr, frame, x, y, xscale,
  yscale, angle, color, alpha)` — flip is just negative scale,
  rotation included. We have flip only (`sprf`).
- **Animation built into sprites**: `image_index` /
  `image_speed`. Every sprite carries its own frame counter that
  auto-advances. We could do this with a tiny helper.
- **Data structures**: `ds_list`, `ds_map`, `ds_grid`,
  `ds_priority`. Probably overkill for a learning console; carts
  can use C arrays.
- **Camera**: `camera_set_view_pos`, follow targets,
  `surface_create` for offscreen rendering. We have `camera()`.
- **Persistence**: `ini_open`, `ds_map_secure_save`. Useful but
  involved.
- **Particles**: a full `part_system` API. Lovely effect but a lot
  of surface area.

### BlitzMax / Blitz3D — approachable + game-y

A spiritual ancestor of dreamengine's "just write code, hit run"
vibe. Notable API choices:

- `LoadImage` / `LoadSound` — explicit resource handles. Less
  relevant for us since sprites are in the editor.
- **`ImagesCollide(img1, x1, y1, frame1, img2, x2, y2, frame2)`**
  — pixel-perfect sprite collision out of the box. Killer feature.
- `RectsOverlap(x1, y1, w1, h1, x2, y2, w2, h2)` — AABB.
- **Trig in degrees**, not radians, with `Sin(deg)` / `Cos(deg)`.
  PICO-8 went turns, BlitzMax went degrees. Either is more friendly
  than radians for beginners.
- `Rnd(min, max)` (float) and `Rand(min, max)` (int) — nice split.
- `MilliSecs()` — global millis since start. We have `now()`
  in seconds; same idea.
- File I/O was straightforward (`OpenFile`, `ReadLine`). Probably
  out of scope.

### DIV Game Studio — what's worth borrowing

DIV's most distinctive feature is its **process** primitive — each game
object as a coroutine with its own `loop … frame;` body. We considered
adopting it as a "Level-2" learning model and **decided against it**: the
shipped carts all work cleanly with plain typed static pools, so it's
weeks of coroutine/transformer machinery for a model the cart corpus shows
we don't need. (Decision recorded in [`../decisions/0001-cut-coroutine-process-model.md`](../decisions/0001-cut-coroutine-process-model.md); see the 2026-05-30 review below and [`../VISION.md`](../VISION.md).) That leaves
DIV's flatter *API* ideas, most of which only make sense once you *have*
per-instance state — so most are moot for us too, but noted for the record:

- **`collision(type)`** — returns the id of a colliding process
  (or 0). Tied to the process model — not applicable without instances.
- **`get_dist(id)`**, **`get_angle(id)`**, **`get_signed_angle(id)`**
  — just `point_distance` / `point_direction` from GameMaker anchored to a
  process's own coords. We cover the math with `distance` / `angle_to`.
- **`advance(speed)`** — "move forward at my current angle by N
  pixels". Equivalent to `x += dx(speed, angle); y += dy(speed, angle);`
  with our existing helpers.
- **`region`** — per-process scissor rectangle. We already have
  `clip()`; same idea, just not per-instance since we don't have
  instances.

### Scratch — collision-by-color and the kids'-API instincts

Scratch is the easiest target audience reference point — millions of
children build games in it. Its primitives are simple but several
are genuinely clever:

- **`touching color [c]`** — "is any pixel of my sprite currently
  touching a pixel of color c on the stage?" This is **collision
  via pixel color**, and it's brilliant for beginners: you don't
  need types, layers, hitboxes, or instance lists. Make the walls
  blue, ask "am I touching blue?", and you have collision. We can
  implement this cheaply on top of `pget` — walk the cells under
  the sprite's bbox and ask `pget(px, py) == color` for any of
  them.
- **`color [a] is touching color [b]`** — the same idea but
  comparing two colors on the canvas, no "self" involved. Less
  commonly useful for us, but trivial to add if we have the first.
- **`distance to mouse`** / **`distance to sprite`** — same idea
  as DIV's `get_dist`, framed differently.
- **`pen down` / `change pen color by`** — the turtle-graphics pen
  primitives. Often overlooked but really fun for procedural art.
  Probably out of scope since we have `pset/line/etc.`
- **`when I receive [message]`** — global broadcast events. Useful
  in OO-flavoured games. Maps to nothing we have today (no event
  system) — possibly worth thinking about alongside the process
  model.
- **`ask [thing] and wait`** — modal input. Probably skip.
- **`pick random N to M`** — same as GameMaker's `irandom_range`.
- **Sound: `play sound until done` / `change tempo`** — same
  shape as our `bpm()`.

Beyond `touching color`, the Scratch blocks worth pulling in:

- **`if on edge, bounce`** — Scratch's iconic DVD-logo helper.
  Flips velocity components when the sprite hits a screen edge.
  Four lines of C, big "first game" payoff. Proposed below.
- **`point towards [target]`** — same idea as our `angle_to`, but
  the *naming* makes it click for beginners. We'll keep `angle_to`
  for the angle math and add a notion of "face the target"
  conceptually when we have a sprite/actor concept.
- **`timer` + `reset timer`** — global cart-resettable stopwatch.
  We have `now()` (seconds since startup, unstoppable) but no way
  to reset. Tiny addition: `timer()` returns seconds since last
  reset, `timer_reset()` zeroes it.
- **`broadcast [msg]` + `when I receive [msg]`** — Scratch's
  lightweight global event bus. Pedagogically lovely: decouple
  "the ball was eaten" from "play the eat sound" from "increment
  score". Proposed below as `broadcast()` + `received()`.
- **`wait N seconds`** — would need a coroutine to pause inside a
  function without yielding control. That's the process model, which
  we cut (see DIV section). In our model you wait by checking `timer()`
  / `frame()` in `update()`, so we skip this.
- **`create clone of [sprite]`** — same: it wants per-instance actors
  with their own state, which the process model would have given. Carts
  do this with a typed static pool + `on` flag instead. Skip.
- **Looks effects: `ghost`, `pixelate`, `brightness`** —
  post-render filters. The `ghost` (alpha) one is genuinely
  beautiful for fade-in/out, cheap to add via a global tint. The
  others are GLSL territory.
- **Pen / turtle graphics** — `pen_down`, `pen_up`, `change pen
  color`. Different drawing paradigm — your "cursor" leaves a
  trail as it moves. Wonderful for procedural-art lessons
  (L-systems, recursive trees). Worth its own future write-up.

Scratch's biggest lesson isn't a function — it's the *naming*: it
calls things what they actually do, in plain English. "touching
color" is a much better name than "color collision check"; "wait 2
seconds" beats `usleep`. Worth keeping in mind as we name new API.

### p5.js / Processing (Dan Shiffman)

Same `setup() / draw()` skeleton as dreamengine. Its real gift to
us is a clutch of *math helpers* that turn out to be load-bearing
in real sketches:

- **`map(v, lo1, hi1, lo2, hi2)`** — remap a value from one range
  to another. Constantly useful for sliders, fades, anywhere you
  have one range and need another. *(We can't use `map` as a name
  — it's the tilemap function. Call it `remap`.)*
- **`lerp(a, b, t)`** — linear interpolation. Used everywhere from
  smooth camera follow to color blending.
- **`constrain(v, lo, hi)`** — same as our `mid(lo, v, hi)`.
- **`dist(x1, y1, x2, y2)`** — Euclidean distance.
- **`mag(x, y)`** — `dist` from origin; the `sqrt(x²+y²)` you
  always need.
- **`atan2(y, x)`** — angle of a vector.
- **`noise(x)`**, **`noise(x, y)`** — Perlin noise. *This is the
  Shiffman special*: organic-feeling motion, terrain, flow fields,
  all from one function returning a smooth 0..1 value across
  continuous inputs. Hugely fun for learners.
- **`random(low, high)`** — float random in a range.
- **`createVector(x, y)`** + chained vector ops. Hard to translate
  to C without operator overloading. Probably skip.
- `millis()`, `frameCount`, `deltaTime` — we have `now()`; could
  add `frame()` (uint frame counter).
- `text(str, x, y)`, `textSize`, `textAlign` — we have `print` at
  fixed size.

### Strudel — code-as-music idioms

We already pulled in Strudel's `every`, `euclid`, `chance`,
`degree` — the helpers that made the most sense as plain
functions. Strudel itself is more ambitious: a JS port of
TidalCycles where music is built out of **first-class patterns**
that compose with method chains. We can't have first-class
patterns in C, but the *idioms* still translate.

Things worth borrowing on a second pass (proposed in §13):

- **Note-name parsing** — Strudel's `note("c4 e4 g4")` accepts
  letter+octave strings. Beats remembering MIDI 60 = C4.
- **Probability shorthands** — `sometimes`, `often`, `rarely` are
  named points on `chance(p)`. They read like English.
- **Arpeggios** — `arp` plays one note of a chord per step.
- **Stutter** — `bd*4` in mini-notation, repeat a hit N times in a
  beat.
- **Palindrome / reverse** — pattern shape transforms.
- **Swing** — micro-timing biases that humanise the beat.
- **Off-beat scheduling** — fractional-beat events for ghost notes.

Things that need engine work (deferred): per-note filters
(`.lpf().resonance()`), reverb/delay sends, stereo/pan, and the
mini-notation parser. Real Strudel features, but they touch
`sound.h`, not `studio.h`.

---

## Proposed additions, by category

Each entry is a guess at a signature + a one-line rationale. None
are merged. We can pick whichever buckets feel like the right next
step.

### 1. Math — small, high-leverage

These are universally useful and tiny to implement. Names chosen to
read like sentences from the comment beside them.

```c
// numeric — bare math verbs
int   abs(int v);                                       // "absolute value"
int   min(int a, int b);                                // "smaller of two"
int   max(int a, int b);                                // "bigger of two"
float clamp(float v, float lo, float hi);               // "keep between two limits"
float lerp(float a, float b, float t);                  // "mix a and b" (t=0 → a, t=1 → b)
float remap(float v, float a, float b, float c, float d); // "from one range to another"

// geometry — read like sentences
float distance(int x1, int y1, int x2, int y2);         // "how far apart are these points?"
float length(int x, int y);                             // "how long is this vector?" (sqrt of x²+y²)
float angle_to(int x1, int y1, int x2, int y2);         // "what direction (degrees) from A to B?"
float dx(float steps, float degrees);                   // "how much x do I move stepping this way?"
float dy(float steps, float degrees);                   //  (GameMaker's lengthdir_x/y, renamed)

// trig — degree-based, matches angle_to + dx/dy
float sin_deg(float degrees);
float cos_deg(float degrees);
```

We expose **degree-based trig** rather than radians or turns. Two
reasons: angles in degrees are taught in school; and `dx`/`dy` plus
`angle_to` chain cleanly in degrees. Floats throughout because the
cost of int-only math is more bug than the cost of teaching floats.

Naming notes: `abs/min/max` are the universal math words; `clamp`
beats `mid` for clarity (we'll keep `mid` as an alias — see the
existing-API review at the end). `distance` over `dist`, `length`
over `mag` — full words read better than abbreviations when you're
seven. `dx`/`dy` is the physics shorthand: "delta x given a step at
an angle." Comments make the verb visible.

### 2. Collision — the missing layer

```c
// "are these two boxes touching?"
bool boxes_touch(int ax, int ay, int aw, int ah,
                 int bx, int by, int bw, int bh);

// "is this point inside this box?"
bool point_in_box(int px, int py, int bx, int by, int bw, int bh);

// "are these two circles touching?"
bool circles_touch(int ax, int ay, int ar, int bx, int by, int br);

// "are these two points within d pixels of each other?"
bool near(int ax, int ay, int bx, int by, int d);

// "is this box on top of any non-empty map cell?"
bool touching_map(int x, int y, int w, int h);     // pixel-space AABB
int  tile_at(int px, int py);                       // sample the map at pixel coords

// "is this box covering any pixel of this color?" (Scratch's idea)
bool touching_color(int x, int y, int w, int h, int color);
```

`touching_map` is the cart-friendly version of "I am a 16×16 sprite
at (x, y) — am I on top of a wall?". Internally it walks the cells
that the AABB overlaps and returns true if any is non-zero. For a
richer notion of "solid" the cart can use `tile_at` and decide for
itself.

`touching_color` is the **Scratch idea** ported in. It walks the
pixels of the bbox and asks `pget(px, py) == color`. Profoundly
beginner-friendly: paint your walls red, ask
`touching_color(x, y, 16, 16, CLR_RED)`, you have collision with no
typing system, no instance lists, no hitboxes. Pairs naturally with
the sprite editor (kids already think in colors) and with `pget`
which is already in the runtime. Implementation cost is tiny since
`pget` already reads from the previous frame's canvas snapshot.

```c
// "if I'm at an edge, bounce!" (Scratch's classic helper)
// Flips vx if x is at the left/right edge, vy at top/bottom.
// Mutates x/y/vx/vy in place.
void bounce_at_edges(int *x, int *y, int *vx, int *vy,
                     int w, int h);
```

The classic first-game helper. With one call your DVD-logo bounces.
Internally: if x < 0 or x + w >= SCREEN_W, flip vx; same for y/vy.
Operates in screen-space, not world-space, since "edges" only makes
sense relative to the visible screen.

### 3. Animation — one helper, no objects

GameMaker's `image_index` is implicit; PICO-8 makes you do
`spr(base + (t//8) % n, x, y)`. The latter is fine but not great
for absolute beginners. One helper goes a long way:

```c
// "which frame should I be on right now?" — cycles 0..n_frames-1
int anim(int n_frames, float fps);

// "which frame should I be on, given this animation started at time t?"
// — plays once; returns the last frame after the animation ends.
int anim_once(int n_frames, float fps, float start_t);
```

Usage: `spr(walk_base + anim(4, 10), x, y);` — walk cycle, 10
frames per second.

Naming note: `anim` reads well in context (`anim(4, 10)` = "4-frame
animation at 10 fps"). Shorter than `current_frame_of_animation()`
and it's already a familiar term to anyone who's used GameMaker.

### 4. Easing / interpolation

We get most of the way with `lerp`. Add a few common easings for
when the user wants something other than linear motion:

```c
// take a 0..1 value and curve it to make motion feel less robotic
float ease_in(float t);      // "start slow, end fast"
float ease_out(float t);     // "start fast, end slow"
float ease_in_out(float t);  // "slow, fast, slow" (smoothstep)
```

Composes with `lerp` and `now()` for transitions. Plain English
comments because "ease" is jargon — the comment tells you what
the curve actually does.

### 5. Noise — the Shiffman special

```c
// "smooth-random — nearby inputs give similar outputs" (0..1)
float noise(float x);                     // 1D
float noise2(float x, float y);           // 2D  — terrain, fog, flow fields
float noise3(float x, float y, float z);  // 3D — animated 2D + time
```

Value noise is ~30 lines and good enough. Perlin is ~80 and feels
better. Either is worth having — organic motion (clouds, wobble,
flow fields, drifting fog) is otherwise hard to get cheaply.

### 6. Persistence — high scores and cart state

```c
// "remember a number across runs of this cart" (64 slots)
void save(int slot, int value);   // store an int in a persistent slot
int  load(int slot);              // read it back — 0 if never saved
```

Stored alongside the cart binary as a per-cart `cart.dat` file in
`build/`. Trivial in main.cjs land.

Naming note: PICO-8 calls these `dset`/`dget` ("data set/get"); we
go with the verbs you'd actually say out loud — "save the high
score", "load the high score." We'll need to keep an eye on `save`
colliding with future cart-saving — but `save_score(s)` is the most
common use, and the slot version reads cleanly: `save(0, score)`.

### 7. String formatting — make `print` more useful

Right now you can't easily print a number. C's `snprintf` is
correct but ceremonial. A tiny helper returning a static buffer is
all most carts need:

```c
// "build a string with values in it" — printf-style, returns a static buffer
const char *str(const char *fmt, ...);
// usage: print(str("score %d", score), 4, 4, CLR_WHITE);
```

Rolling buffer means: subsequent calls overwrite. Fine for the
single-frame case; document the gotcha.

Naming note: `str` is short and reads in context (`print(str("…"),
…)`). `text(…)` was tempting but overlaps with the visual concept
of text already drawn by `print`.

### 8. Camera helpers — quality-of-life

```c
// "make the camera follow this point" — clamped to the world rect
void follow(int target_x, int target_y, int world_w, int world_h);
```

Internally calls `camera(clamp(target_x - SCREEN_W/2, 0, world_w -
SCREEN_W), clamp(target_y - SCREEN_H/2, 0, world_h - SCREEN_H))`.
The default cart's draw() already inlines this; making it a helper
saves carts five lines.

### 9. Time helpers

```c
int   frame(void);            // "what frame number are we on?"  (increments each update)
float timer(void);            // "how many seconds since I last reset the timer?"
void  timer_reset(void);      // "start the timer over from 0"
```

`now()` is unstoppable since startup — great for animation. `timer`
is the cart-controllable counterpart — perfect for "how long did
the player survive?", round timers, etc. `frame()` is for users
who want frame-based timing instead of seconds-based.

For *musical* sub-beat timing — anything finer than `beat()` — see
the **PPQ tick clock** introduced in §14. `tick()` advances 96
times per beat and stays musical across tempo changes, which is
why the Dilla-timing primitives are built on it.

### 10. Random — small additions

```c
// "pick a random number between lo and hi"  (Scratch: "pick random N to M")
int   rnd_between(int lo, int hi);          // int in [lo, hi)

// "pick a random float between 0.0 and 1.0"
float rnd_float(void);

// "pick a random float between lo and hi"
float rnd_float_between(float lo, float hi);
```

Currently `rnd(n)` only gives `[0, n)` ints. The float variants
are needed for noise/lerp/animation patterns.

### 11. Events — Scratch's broadcast / receive

```c
void broadcast(int msg_id);     // "shout this message to everyone"
bool received(int msg_id);      // "did anyone shout this message?"
                                //  (drained each frame)
```

A tiny global event bus, mirroring Scratch's
`broadcast` / `when I receive` pair. Decouples "the player died"
(`broadcast(EVT_DEATH)`) from "play the death sound"
(`if (received(EVT_DEATH)) sfx(3);`) from "increment death count".
Implementation: a small bitset of pending events; `update()` runs,
then the runtime drains the bitset at frame-end (so events posted
in this frame are visible until end-of-frame, then gone).

Carts use whatever integer IDs they like; conventionally
`#define EVT_DEATH 0` etc. at the top of the cart. ~30 lines of C
total.

### 12. Pen / turtle graphics

```c
// Scratch / Logo turtle. A single implicit turtle with position +
// heading that leaves a trail when the pen is down.

// motion (turtle position & heading) — Scratch's "Motion" category
void turtle_home(void);             // "go to the middle, face right"
void turtle_move(float steps);      // "move forward this many pixels"
void turtle_turn(float degrees);    // "turn N degrees" (positive = clockwise)
void turtle_face(float degrees);    // "point in this direction"
void turtle_at(int x, int y);       // "teleport here without drawing"
                                    // (note: `goto` is a C reserved word)

// pen (drawing) — Scratch's "Pen" extension
void pen_down(void);                // "start leaving a trail"
void pen_up(void);                  // "stop leaving a trail"
void pen_color(int palette_color);  // "use this color for the trail"
```

A separate drawing paradigm — your cursor moves through the world
and leaves a trail. Magical for procedural art lessons (L-systems,
recursive trees, Hilbert curves). Tiny in code: one `Turtle` struct
internally (x, y, heading, pen, color), `turtle_forward` calls
`line()` from old position to new. Composes with `cls`/`spr`/etc.
freely — turtle drawing just adds to the canvas.

Beginner programs that fit on a screen:

```c
void draw() {
    cls(CLR_BLACK);
    turtle_home();
    pen_color(CLR_GREEN);
    pen_down();
    for (int i = 0; i < 360; i++) {
        turtle_move(2);
        turtle_turn(91);    // 91 instead of 90 makes a spiral, not a square
    }
}
```

Could grow into a tutorial cart all by itself.

### 13. More from Strudel — code-as-music additions

We already pulled Strudel's `every`, `euclid`, `chance`, `degree`.
There's more on offer. None of it is essential, but a few of these
would expand what a beginner can express in three lines of code.

```c
// "what's the MIDI number for this note name?" — accepts c4, d#5, bb3, etc.
int pitch(const char *note_name);
```

Saves remembering that "C4" is MIDI 60. Carts read naturally:
`note(pitch("c4"), INSTR_TRI, 5)`. Strudel does this through its
`note("c e g")` parsing.

```c
// probability shorthands — Strudel's sometimes / often / rarely
bool sometimes(void);   // chance(50)
bool often(void);       // chance(80)
bool rarely(void);      // chance(20)
```

`if (every(1) && rarely()) note(...);` reads exactly like what
it does. We already have `chance(p)` — these are just named
points on the curve.

```c
// "play the nth note of this chord, cycling forever"
// — turns a chord into an arpeggio when called once per beat.
void arp(int root, int chord_type, int instr, int vol, int step);
```

Usage:
```c
if (every(1)) arp(60, CHORD_MIN7, INSTR_TRI, 4, beat());
// → cycles through C-Eb-G-Bb-C-Eb-G-Bb... one per beat
```

Strudel's `arp` is a pattern transformer; ours is a function call,
but the *idea* — chord notes as a sequence indexed by step — is
the same.

```c
// "play this note multiple times within one beat" (Strudel's bd*4)
void stutter(int midi, int instr, int vol, int times);
```

Schedules `times` notes evenly across the current beat using
`schedule()` under the hood. Snare rolls, machine-gun blips,
stuttering bass.

```c
// "go back and forth across N positions, instead of repeating"
int palindrome(int beat, int length);   // 0..length-1..0..length-1...
int rev_step(int beat, int length);     // length-1 down to 0, then repeat
```

For melodies that bounce instead of loop, or for reversed
arpeggios:
```c
if (every(1)) {
    int idx = palindrome(beat(), 5);
    note(degree(SCALE_PENTA, 4, idx), INSTR_TRI, 4);
}
// → C D E G A G E D C D E G A G E D C...
```

```c
// "play this note exactly at a fraction of the way through a beat"
// fraction is 0.0..1.0. Useful for hi-hat ghost notes, syncopation.
void off_beat(float fraction, int midi, int instr, int vol);
```

Equivalent to `schedule((int)(fraction * beat_ms()), midi, instr,
vol)` where `beat_ms` is derived from BPM. Just makes the
ghost-note pattern one call instead of three.

> Global swing (Strudel's `swingBy`) is subsumed by per-group
> swing — see §14 below. We omit a single global `swing()` here
> so we don't have two ways to set the same thing.

#### Things we'd need bigger changes for (defer)

These are real Strudel features but they touch the audio engine,
not just the API:

- **Per-note filter cutoff / resonance** (`.lpf(800).resonance(0.5)`)
  — requires per-voice SVF filters in `sound.h`. Currently we mix
  raw oscillators. Roughly 100 lines of DSP per filter type.
- **Reverb / delay effects** — global FX bus. Bigger restructure
  of the audio callback.
- **Stereo / pan** — currently mono. Doubles every voice's
  per-sample math but is otherwise straightforward.
- **Mini-notation parser** (`"bd*4 ~ sd"`) — strings of pattern
  syntax. A small parser + matching pattern-eval runtime. Probably
  out of scope for a learning console, but undeniably fun.

#### What we already have that lines up with Strudel

| Strudel | dreamengine | notes |
|---|---|---|
| `cps(n)` / BPM | `bpm(rate)` | same |
| `s("bd")` sample trigger | `sfx(n)` | data-driven |
| `note(...)` | `note(midi, instr, vol)` | direct trigger |
| `every(4, fn)` | `if (every(4)) …` | gate-based |
| `euclid(3,8)` mini-notation | `euclid(hits, steps, b)` | function form |
| `?p` random gate | `chance(p)` | function form |
| `.scale("c:minor").n(3)` | `degree(SCALE_MINOR, oct, 3)` | function form |
| `stack(a, b)` | two parallel `if`-blocks | naturally |
| `.degradeBy(0.5)` | `every(1) && chance(50)` | composable |
| `.fast(2)` | call `every(1)` twice | manual |

### 14. Dilla timing — per-group groove, swing, jitter

J Dilla's drum patterns are famous for *not* sitting on the grid —
hi-hats might be straight, the snare slightly behind, the kick
slightly ahead. Combined with a hint of random wobble, the result
feels like a person playing, not a metronome.

To do this well we need a sub-beat unit. Milliseconds aren't
right (a "5ms delay" feels different at 60 vs 180 BPM); we want
something musical.

#### Sub-beat unit: PPQ (pulses per quarter note)

Stealing the MIDI/MPC standard: **96 ticks per beat**. That
divides cleanly into 16th notes (24 ticks), triplet 8ths (32
ticks), 32nd notes (12 ticks), and triplet 16ths (16 ticks).
navkit's soundsystem uses the same 96 — keeps us future-compatible
if we ever talk to it.

```c
#define PPQ 96    // ticks per beat — public constant

int   tick(void);        // "what tick of the song are we on?"
                         //  monotonically increasing, 96 per beat
float tick_pos(void);    // 0.0..1.0 fractional position within the current tick
```

96 ticks per beat at 120 BPM = a tick every ~5.2 ms. At our 1024-
sample audio buffer (~23 ms) that's finer than the audio thread's
resolution, but it's still the right *musical* unit — we round to
the nearest sample when scheduling.

**Why expose ticks at all?** So that swing / jitter / push can be
expressed in *musical* units that survive tempo changes. "Push
snare by 8 ticks" is the same musical feel at any BPM; "push snare
by 40ms" is not.

#### The groove API

```c
// "play a note that follows this group's timing feel"
void groove(int group, int midi, int instr, int vol, int dur_ms);

// per-group config — set once, applies to all subsequent groove() calls.
// units chosen for what feels intuitive at the human end:
void groove_swing(int group, int percent);    // 0..50  "how laid-back"  (% of half-beat)
void groove_jitter(int group, int ticks);     // 0..24  "how human"      (± PPQ ticks)
void groove_push(int group, int ticks);       // -24..24 "early/late"    (PPQ ticks)
```

Picking units:
- **swing** stays as a percent because that's how musicians talk
  about it ("25% swing", "MPC swing") and it's already
  tempo-independent.
- **jitter** and **push** are in **ticks**, not ms — so the feel
  stays musical across BPM changes. We multiply by `(60_000 /
  bpm) / PPQ` internally to get the sample delay.

Eight groups, generic numbered 0..7. Carts give them meaning:

```c
#define G_KICK  0
#define G_HAT   1
#define G_SNARE 2
#define G_BASS  3
#define G_LEAD  4

void update() {
    bpm(85);

    // setup the feel — once, anywhere (idempotent)
    groove_swing (G_HAT,   28);     // jazzy hats — 28% swing
    groove_jitter(G_HAT,    3);     // ±3 ticks of human wobble (~16ms at 85bpm)
    groove_push  (G_SNARE, +6);     // snare drags by 6 ticks — Dilla classic
    groove_jitter(G_KICK,   1);     // kick is tight, just a kiss of wobble

    if (every(2))                 groove(G_KICK,  36, INSTR_TRI,   5, 60);
    if (every(2) && beat()%4==2)  groove(G_SNARE, 60, INSTR_NOISE, 5, 80);
    if (every(1))                 groove(G_HAT,   72, INSTR_NOISE, 2, 25);
}
```

The same loop, played by a person. The tick-based push/jitter
keeps feeling the same if you change `bpm(85)` to `bpm(140)`.

**How each setting fires:**

- **`groove_jitter(g, ticks)`** — easiest. Every `groove(g, ...)`
  call rolls a uniform random offset in `[-ticks, +ticks]`,
  converts to samples (`ticks * sample_rate * 60 / bpm / PPQ`),
  and feeds it to the schedule queue. Done. No state.
- **`groove_push(g, ticks)`** — also easy. Fixed offset, same
  ticks-to-samples conversion. Positive = late, negative = early.
- **`groove_swing(g, percent)`** — needs a tiny bit of state per
  group. We track which "subdivision" we're on by reading
  `tick() % PPQ` at trigger time: ticks in the 0..47 half are
  "on-beat", ticks in 48..95 are "off-beat". On the off-beat we
  add `(percent% × PPQ/2)` ticks of delay. This is the same
  formula MPCs use, applied per call. No assumption about call
  rate.

**Why grouped, not per-call:** the *feel* of a song lives in the
relationship between elements — hat swings 25%, snare pushes 6ms
late, kick is dead straight. Carts set this once at the top and
the rest of the music just calls `groove(g, …)`. PICO-8 doesn't
have anything like this. Strudel does it through pattern
transforms; we do it through groups.

**Naming notes:**

- `groove(g, …)` reads like English: "play this with the kick
  group's feel".
- `groove_swing`, `groove_jitter`, `groove_push` form a triad and
  every name says what it does (no "humanize", no "feel").
- We chose `groove` over `hit_in_group(g, …)` or `play(g, …)` —
  it conveys *micro-timing* specifically.
- Numbered groups (not enum) so carts pick their own taxonomy.

### 15. Gamepad — controller support, almost free

raylib already polls game controllers — we just don't expose it.
Adding controller support means **augmenting `btn()` internally** so
that `btn(0, BTN_A)` returns true if either the Z key *or* the
gamepad-0 A button is held. Same for player 1 mapping to
gamepad-1. No cart-facing change.

What we *do* need a new surface for is analog input:

```c
// "how far in this direction is the stick pushed?" (-1.0..1.0)
float gp_axis(int slot, int axis);

#define AXIS_LX 0   // left stick X (left-right)
#define AXIS_LY 1   // left stick Y (up = negative, down = positive)
#define AXIS_RX 2   // right stick X
#define AXIS_RY 3   // right stick Y
#define AXIS_LT 4   // left trigger  (0..1)
#define AXIS_RT 5   // right trigger (0..1)

bool gp_present(int slot);   // is a gamepad plugged in at this slot?
```

The trigger axes (`LT`/`RT`) sit between 0 and 1, not -1..1.

This pairs nicely with `stick_x()` / `stick_y()` (the touch
stick) — if we ever generalise, we could have a single
`stick_x(slot)` that reads from "whatever's connected first":
touch on iPad, gamepad on desktop, WASD fallback otherwise. For
now, keep them distinct.

### 16. Pause + debug helpers

```c
// "freeze the world" — update() stops being called, draw() keeps running
// so the cart can show a pause menu / state overlay.
void pause(bool paused);
bool paused(void);

// debug introspection
int fps(void);             // current frame rate (averaged)
int voices_active(void);   // how many sound voices are currently playing
```

`pause(true)` stops `update()` from firing; `draw()` keeps running
each frame so the screen doesn't freeze. Carts can use this for
pause menus, game-over overlays, level-transition screens. The
runtime drains the audio queue normally so sounds finish their
release tails rather than cutting out.

`fps()` and `voices_active()` are debug breadcrumbs — useful while
making the game, less useful in the finished cart. A typical use:

```c
void draw() {
    cls(CLR_BLACK);
    // ... game ...
    if (btn(0, BTN_A) && btn(0, BTN_B)) {   // both held = show debug
        print(str("%d fps", fps()), 4, 4, CLR_LIGHT_YELLOW);
        print(str("%d/8 voices", voices_active()), 4, 14, CLR_LIGHT_YELLOW);
    }
}
```

### 17. Print alignment helpers

`print(text, x, y, color)` is fixed-size top-left anchored. Two
tiny helpers cover the most common asks:

```c
// "print this text centered on the screen at height y"
void print_centered(const char *text, int y, int color);

// "print this text so it ends at right_x at height y"
void print_right(const char *text, int right_x, int y, int color);
```

Internally each computes the text width (chars × 8 px) and
calls `print` with the right offset. Useful for titles, scores,
floating damage numbers, anything where you don't want to measure
strings yourself.

### 18. Convenient sprite drawing — ✓ shipped

> **✓ Shipped (in working tree, pending commit).** Landed as two functions — a simple
> whole-sprite spin and a full source-rect transform:
> ```c
> void spr_rot(int index, int x, int y, float deg);  // spin a whole sprite `deg` around its center. (x,y) = top-left like spr(). the everyday rotate
> void sspr_ex(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, float deg, int ox, int oy); // source sub-rect → dest rect (scale), rotated `deg` around pivot (ox,oy) in dest. flip with negative sw/sh
> ```
> Pure raylib `DrawTexturePro` under the hood. `spr_rot` is the everyday case — just an
> angle, rotates around the sprite's own center. `sspr_ex` is the works: it extends
> `sspr` (sub-rect → scaled dest rect) with rotation around an arbitrary pivot, and
> flips by passing a negative `sw`/`sh`. Unblocks asteroids, racer, lander, sopwith,
> paratrooper, galaga, xevious, which all faked rotation with triangles.

---

### 19. Shipped but undocumented — things that exist but weren't in this doc

These were added during implementation without a corresponding design note here.

#### Triangles — `tri` / `trifill`

```c
void tri(int x1, int y1, int x2, int y2, int x3, int y3, int color);     // triangle border
void trifill(int x1, int y1, int x2, int y2, int x3, int y3, int color); // filled triangle (any winding)
```

Fills the obvious gap between `rect` and freeform shapes. `trifill` uses a scanline rasteriser and accepts any vertex winding order.

#### Sprite transparency — `colorkey`

```c
void colorkey(int color);  // set transparent color for sprites. -1 = no transparency.
```

Called once when the transparency color changes, not every frame. Lets carts pick a background color to treat as alpha without needing a separate alpha channel.

#### Map zoom — `map_scale`

```c
void map_scale(int n);  // integer zoom for map drawing; default 1
```

Scales cells when drawing the map — useful for zoomed-in views or chunky tile games.

#### Debug overlay — `watch` / `watch_visible` / `printh`

```c
void watch(const char *name, const char *fmt, ...);  // show a named live value in the corner
void watch_visible(bool on);                         // hide/show the overlay (F1 toggles it)
void printh(const char *fmt, ...);                   // printf to the editor's runtime log panel
```

`watch` is the fast path for debugging: call it every frame and the value updates live in the corner of the game window without polluting the canvas. `printh` sends output to the editor's build log — useful for one-shot diagnostics. Together these replace printf-debugging with something more ergonomic.

#### Juice + polish batch — `pal` / `fade` / `shake` / `print_scaled` / `text_width`

> In working tree, pending commit.

```c
void  pal(int c0, int c1);    // remap: draw color c0 as c1 (hit-flash, recolor, fades). persists until pal_reset()
void  pal_reset(void);        // undo all pal() swaps
void  fade(float amount);     // darken the whole screen toward black: 0 = normal, 1 = black. for transitions
void  shake(float amount);    // kick the screen by `amount` pixels; decays on its own. call on impacts
void  print_scaled(const char *text, int x, int y, int color, int scale); // bigger text for titles/menus
int   text_width(const char *text);  // pixel width of text at normal size (chars × 8) — for centering in your own boxes
```

The classic juice primitives, finally first-class: `pal(c, CLR_WHITE)` for the
flash-on-hit, `fade()` for state transitions and pause-dim, `shake()` replaces the
hand-rolled screenshake in `juice.c`. `print_scaled` + `text_width` cover big
titles/scores and custom HUD layout.

#### Shapes — `oval` / `ovalfill`

```c
void oval(int x, int y, int rx, int ry, int color);     // ellipse border (rx,ry = half-width/height)
void ovalfill(int x, int y, int rx, int ry, int color); // filled ellipse — squashed circles, eyes, shadows
```

#### Keyboard + text entry — `key` / `keyp` / `text_input`

```c
bool key(int k);                // true while key k is held. letters/digits: key('A'), key('5'); specials: KEY_SPACE, KEY_LEFT...
bool keyp(int k);               // true only on the frame key k was pressed
const char *text_input(void);   // characters typed this frame (for name entry / word games); "" if none
```

Raw keyboard beyond the 6-button `btn()` model, plus typed-character capture — opens
up name entry, word games, and keyboard-heavy carts. (Was not in any prior proposal.)

#### Time + persistence — `dt` / `save_bytes` / `load_bytes`

```c
float dt(void);                               // seconds since the last frame (clamped for hitches). framerate-independent motion
void  save_bytes(const void *data, int len);  // save a whole block of bytes (a struct/array) — for state the 64 int slots can't hold
int   load_bytes(void *out, int max);         // read it back into `out` (up to max bytes); returns bytes read, 0 if never saved
```

`save_bytes`/`load_bytes` generalise persistence beyond the 64 int slots — hi-score
*tables*, names, full save states. This subsumes the `save_str`/`load_str` idea that
came up while polishing carts (just `save_bytes` a struct that contains the strings).
`dt()` gives framerate-independent movement without hand-tracking `now()` deltas.

---

## Cart-pattern analysis — what the carts keep re-implementing (bottom-up)

The sections above are *top-down* — what the classics offer. This one is *bottom-up*:
patterns that recur across the actual cart sources, which is the strongest signal for
the next convenience helpers. (Priorities now live in [`STATUS.md`](../STATUS.md); this is
the evidence behind them.)

**Already well-abstracted — don't touch.** `near`, `boxes_touch`, `touching_map`,
`angle_to`, `dx`/`dy`, `lerp`, `ease_*`, `clamp`, `anim`, `follow`, `camera`, `rnd`,
`rnd_between`, `note`, `hit`, `schedule` — all solid and used consistently.

### A. HUD header bar — ~11 files

Every game draws its own version of:
```c
rectfill(0, 0, SCREEN_W, 18, CLR_DARKER_BLUE);
print("GAME", 4, 5, CLR_LIGHT_GREY);
print(str("best: %d", hi), 240, 5, CLR_YELLOW);
```
Candidate: `hud(label, score, best, lives)` — draws the standard header bar.
Files: 07-score, 12-hiscore, 14-hud, 18-invaders, 19-breakout, asteroids, frogger,
lander, platform, pong, robotron.

### B. Game over / press A to restart — ~12 files

Everyone writes their own "darken screen, print GAME OVER, wait for `btnp(A)`" block.
Varies per game (some show score, some best, some delay before accepting input).
Candidate: `game_over_screen(score, best)` returning `true` on A. Probably too rigid
given the variation — watch for a pattern to settle before committing to an interface.
Files: 09-enemies, 18-invaders, 19-breakout, asteroids, frogger, lander, platform,
pong, robotron, snake + others.

### C. Particle / explosion burst — lander, asteroids, robotron

Radial debris on death, reimplemented each time:
```c
for (int i = 0; i < 8; i++) {
    float a = i * 45.0f;
    pset((int)(x + dx(age * 0.6f, a)), (int)(y + dy(age * 0.6f, a)), CLR_ORANGE);
}
```
Candidate: `explode(x, y, age, color)` — stateless radial burst driven by a
caller-owned age value. Fits the engine's no-hidden-state style. **Top pick** — pure
visual, easy, immediately useful.

### D. Entity pool with `.on` flag — 6+ files

Array of structs + `bool on` + a loop that skips inactive slots. Hard to abstract
cleanly in C without macros. Candidate: a `POOL_FOREACH(pool, n, var)` macro. Low
priority — the pattern is simple and readable as-is, and the explicit loop *is* the
learn-C lesson. Files: 18-invaders, asteroids, frogger, platform, robotron + others.

### E. Tile-collision push-out — surveyed, low demand

A `move_and_collide(&x, &y, w, h, vx, vy)` that resolves an entity out of solid map
tiles came up while polishing platformers. A cart survey (2026-05-30) found thin demand:
- Only **`platform.c`** does the full pattern — `mget` → 4-corner `solid_at` test →
  axis-separated move-then-push-out → `on_ground`.
- `zelda.c`/`gta.c` do a related 4-corner test, but top-down and against their *own*
  world data, not `mget` — a single map-based helper wouldn't serve them cleanly.
- alleycat / pitfall / burgertime / sokoban don't use map-collision at all.

Verdict: **not worth a built-in yet.** The reusable nugget is the 4-corner "is this box
solid?" predicate, not the full move-resolve — and even that varies by data source.
Revisit if more tile-based platformers appear.

---

## External brainstorm review — DIV/MMF/sim ideas weighed against the carts (2026-05-30)

> A long external brainstorm (≈58k tokens; another LLM with no repo
> access) proposed a large "pro engine" expansion: DIV-style processes,
> memory arenas, an engine-owned entity system, MMF movement/qualifier
> engines, a PS1 ordering-table + textured 3D, built-in
> pathfinding/spatial-hash, generic data structures, and "tools-as-carts /
> fantasy OS". Weighed against the actual ~90 carts, almost all of it
> collides with two facts about how we already work: (1) every cart is
> **stateless immediate-mode** over its own **typed static pools**
> (`Enemy enes[64]; bool on;` in `robotron.c`, `Enemy en[140]` in
> `vampire.c`), and (2) this is a **learn-C** console where the algorithm
> *is* the content — `astar.c` (102 lines), `boids.c` (85), `sims.c` (232,
> a needs-driven Sims that pathfinds around player-built walls) are all
> cart-space and all teaching artifacts. Burying those in a kernel deletes
> the lesson. What actually survived:

**Keepers (engine):**

1. **`fget` / `fset` — per-sprite flags (PICO-8).** The one good idea in
   the whole "smart map" arc, and not yet anywhere else in this doc.

   ```c
   bool fget(int spr, int flag);            // is flag bit set on sprite? flag 0..7
   void fset(int spr, int flag, bool on);   // set/clear a flag bit on a sprite
   ```

   256 sprites × 8 bits = **256 bytes** of state. A map cell already
   stores a sprite index, so per-tile properties ride on the sprite:
   `platform.c`'s `solid_tile()` collapses from
   `t == TILE_BRICK || t == TILE_GRASS` to `fget(mget(cx,cy), F_SOLID)`,
   and the same 8 flags do double duty for "is tree / water / hazard /
   pickup". Pairs with a small **sprite-editor** addition: an 8-checkbox
   flag row per slot. Note this is the **per-sprite** PICO-8 model, *not*
   the per-cell "flag bank / data bank" the brainstorm pushed — `sims.c`
   shows that a game wanting genuine per-cell data just declares its own
   `cell[][]`, which is clearer than a hidden engine layer. Medium value:
   shines once a game has many distinct solid/interactive tile types;
   `touching_map`'s nonzero=solid already covers the simplest case.

2. **`broadcast` / `received`** — already specced in §11, still open. The
   brainstorm's "signal bus" independently lands on the same primitive, so
   treat it as confirmed demand. The tiny global bitset is the right
   altitude — it gives the DIV "signal" feel with **none** of the process
   model (which we cut — see the DIV section).

3. **Pattern fill across all primitives** *(chosen over `trifill_tex`)*.
   The brainstorm pushed an affine textured triangle (`trifill_tex`) as the
   one 3D-ish splurge. Decision: go the dreamengine-native route instead —
   generalize the 4×4 tiled-pattern fill we already ship as `rectfill_pat`
   (§19 / studio.h) so **every** fill primitive honours a pattern:
   `circfill`, `ovalfill`, `trifill`, and the `map` fill path. This is the
   `fillp` PICO-8 already has and we don't (noted in §47).

   Two ways to expose it, pick one:
   - **PICO-8 `fillp()` model (preferred):** a single global fill-pattern
     state — `fillp(pattern)` once, all `*fill` calls respect it until
     reset. Matches the PICO-8 vocabulary this console already leans on and
     keeps every existing fill signature unchanged. (`FILL_*` pattern
     constants would slot into `shell.js` like the other constant groups.)
   - **Per-call `_pat` variants:** `circfill_pat`, `trifill_pat`, … —
     consistent with the existing `rectfill_pat`, but multiplies the
     surface.

   Gives dithered gradients, retro shading, and cheap "texture" without a
   3D pipeline, an ordering table, or UV math — and it composes with `pal`
   for recolour. **`trifill_tex` is parked** (still under "3D anything" in
   *defer*); revisit only if the pseudo-3D carts (`mode7`, `raycaster`,
   `cube3d`, `elite`) actually demand real texture mapping.

**Route to library carts, not the engine** — this is how to get the
"Settlers/Sims power" the brainstorm chased without breaking the stateless
core or hiding the lesson. Seeds already exist:
- pathfinding header ← `astar.c`
- entity-pool + tag helpers ← `robotron.c` / `vampire.c`
- flocking / state-machine patterns ← `boids.c`

> **Priority note.** All three keepers rank *below* the cart-derived
> helpers (`hud()`, `game_over_screen()`, `explode()`, `pause()`) from the
> "Cart-pattern analysis" section above, which came from real cart-frequency
> analysis (appearing in 11–12+ files each). Finish that first; these are additive.

---

## What to defer or skip

- **Particle systems** — fun but a lot of state. Carts can write
  their own with arrays + `for` loops.
- **Pathfinding (mp_grid)** — too narrow. Ship as a library cart
  (seed: `astar.c`), not engine surface — see the 2026-05-30 review above.
- **Tweens** — `lerp` + `now()` covers 90% of cases. Real tween
  libraries are nice but big.
- **Vector struct (Vec2)** — without operator overloading the
  ergonomics aren't there; spelling out `vx, vy` is fine.
- **DS structures (lists/maps/grids)** — C arrays are the lesson.
  (The brainstorm's hashmap/dllist/spatial-hash push: same verdict. Our
  typed static pools already *are* the data-structure layer, indices are
  the "handles", and naive nearest-loops are fine at our entity counts —
  `boids.c` proves it.)
- **Engine-owned entity system** — God-struct / `SELF` global / `val[16]`
  "alterable values" / ECS / union-entity. The big rabbit hole of the
  2026-05-30 review. Per-game typed pools with *named* fields beat all of
  them for a learn-C console; `e->val[0]` is strictly worse than `e->hp`.
- **MMF movement/qualifier engines** (`move_platform`, `move_8dir`,
  `overlap_tag`) — removes the lesson; `platform.c`'s manual physics is the
  point. Cf. the "Cart-pattern analysis" §E above ("tile push-out — low demand").
- **Memory arenas** (frame/level/proc) — no cart `malloc`s; `str()` already
  gives the reusable scratch buffer arenas are pitched for.
- **PS1 ordering table / z-sort / sim-tick** — z-sort breaks immediate-mode
  draw-order; sim-tick is just `frame() % n`.
- **Tools-as-carts / VFS / fantasy-OS / peek-poke** — a different product.
  The editor is JS/Electron and carts are compiled binaries; there's no
  shared-RAM model to build on.
- **Process / coroutine model** — cut entirely. Every shipped cart works
  cleanly with plain typed static pools, so the DIV-style coroutine model
  isn't worth its weeks of machinery. See [`../decisions/0001-cut-coroutine-process-model.md`](../decisions/0001-cut-coroutine-process-model.md).
- **Pixel-perfect sprite collision** — needs to walk the sprite's
  alpha. Worth doing eventually; AABB covers 95% of cases first.
- **3D anything** — fantasy console, not a 3D engine. (`trifill_tex` was
  considered as the one exception, then parked in favour of pattern fill —
  see keeper 3 in the 2026-05-30 review.)

---

## Suggested first slice

> **✓ Shipped (2026-05-29).** All of pass 1 below is now implemented in
> `runtime/studio.{h,c}`, documented in `studioDocs.js`, and grouped into
> `math` / `collision` / `animation` / `strings` help sections (+ `timer`
> in `utility`). `abs()` is declared in the header but provided by libc.
> Angles are degrees (0 = right, 90 = down). **`dx()`/`dy()` return `float`**
> (the spec said `int`; we changed it so repeated `x += dx(...)` keeps
> sub-pixel motion when position is held in a float). Passes 2–3 are still open.

If we were to ship the next API expansion in one focused pass, I'd
pick:

1. **Math + collision basics**: `abs`, `min`, `max`, `clamp`,
   `lerp`, `remap`, `distance`, `length`, `angle_to`, `dx`, `dy`,
   `sin_deg`, `cos_deg`, `boxes_touch`, `circles_touch`, `near`,
   `point_in_box`, `touching_map`, `tile_at`, `touching_color`,
   `bounce_at_edges`.
2. **`str()`** for formatted printing.
3. **`anim()`** for animation cycles.
4. **`timer()` / `timer_reset()`** for round timers.

That's ~25 small functions, all of which are pure (no global state
beyond a single timer reference, no allocations), all of which
compose with what's already there, and together they unlock writing
real games — collision, motion at angle, score display, walking
sprites, bouncing balls, timed rounds.

### Second pass

> **✓ Mostly shipped.** The items marked ✓ below are implemented. Strudel extras and Dilla timing remain open.

- ✓ **`noise()`** / `noise2()` / `noise3()` — organic motion, terrain, animated 2D.
- ✓ **Persistence**: `save(slot, val)` / `load(slot)` for high scores.
- ✓ **Easings**: `ease_in`, `ease_out`, `ease_in_out`.
- ✓ **`follow()`** — camera follow helper.
- ✓ **`rnd_between` / `rnd_float` / `rnd_float_between`** — full random surface.
- **Events**: `broadcast` / `received` — still open. Touches main-loop drain semantics.
- **More Strudel**: `pitch("c4")`, `sometimes`/`often`/`rarely`, `arp`, `stutter`, `palindrome`, `off_beat` — still open.
- **Dilla timing**: `groove` + `groove_swing` / `groove_jitter` / `groove_push`, `tick` / `tick_pos` / `PPQ` — still open.

### Third pass (own projects)

> **Mostly shipped.** Turtle, print helpers, and sprite rotation/scale are done.
> Gamepad and pause/debug remain open.

- ✓ **Turtle graphics** — `turtle_home/move/turn/face/at`, `pen_down/up/color`, `pen_size`.
- ✓ **Print alignment**: `print_centered`, `print_right`. Plus ✓ `print_scaled` (bigger text for titles/menus).
- ✓ **Sprite rotation + scale** — shipped as `spr_rot` / `sspr_ex` (see §18).
- **Gamepad support** — `gp_axis(slot, axis)`, `gp_present(slot)`, internal augment of `btn()`/`btnp()`.
- **Pause + debug**: `pause(bool)`, `paused()`, `fps()`, `voices_active()`.
- **Sound tracker UI** — if the code-first sound path turns out not to be enough.

---

## Existing API — naming review

A pass over what we already ship, looking for names that would
read better to a beginner.

### Worth adding as aliases (keep the old name too)

| Existing | Proposed alias | Reason |
|---|---|---|
| `mid(lo, val, hi)` | `clamp(val, lo, hi)` | `mid` is a PICO-8 idiom; `clamp` is what everyone else calls it, and the argument order is more natural ("clamp this value between these limits") |
| `sgn(n)` | `sign(n)` | Same value, less abbreviation |

These would coexist — alias the new name to the old impl so no
existing code breaks. The friendlier names are what we teach;
the old names stay for compatibility and for users coming from
PICO-8.

### Keep as-is — PICO-8 vocabulary, beginners learn it fast

| Name | Why we keep it |
|---|---|
| `cls(c)` | "clear screen" — universal in fantasy consoles |
| `spr(idx, x, y)` | PICO-8 standard; carts will need to look at PICO-8 tutorials |
| `sprf` / `sspr` | Suffixes are PICO-8 conventions (`spr` + flip / sub) |
| `pset` / `pget` | PICO-8 vocab — "pixel set/get" |
| `circ` / `circfill` | Renaming to `circle` clashes with raylib's `Circle` type. We learned this the hard way once already |
| `rect` / `rectfill` | Short, universal |
| `rnd(n)` | Short, beginner-recognisable |
| `now()` | Excellent name as-is |
| `mget` / `mset` / `map(...)` | PICO-8 standard |

### Already excellent Scratch-style names

`tap`, `touch_count`, `touch_x`, `touch_y`, `stick_x`, `stick_y`,
`btn`, `btnp`, `every`, `chance` — these all read like sentences
already.

### Worth thinking about later

| Name | Concern | Possible rename |
|---|---|---|
| `every(n)` | Reads great in `if (every(4))` — possibly the best name in the whole API | (keep) |
| `bpm`, `beat`, `beat_pos` | Music vocabulary, fine for users making music | (keep) |
| `degree(scale, oct, n)` | "Degree" is music theory jargon — but the alternatives ("scale_note", "note_in_scale") aren't clearly better | (keep, but watch) |
| `euclid(hits, steps, b)` | Borrows the mathematician's name. Beautiful for those who know it; opaque to those who don't. Comment in studioDocs covers it | (keep) |
| `chord` / `strum` | Both excellent — read like sentences | (keep) |

### Naming principles we've drifted toward

Looking at what feels right vs. wrong in our current API:

1. **Verbs in plain English beat abbreviations** — `touching_color`
   beats `color_overlap`; `bounce_at_edges` beats `edge_reflect`.
2. **Read like a sentence in context** — `if (boxes_touch(...))`,
   `pen_down(); turtle_move(20);`.
3. **Question form for boolean queries** — `touching_map`,
   `boxes_touch`, `near`.
4. **Function-name = command for actions** — `move`, `turn`,
   `face`, `clear`, `bounce`.
5. **Comment as a Scratch block** — every API entry's docstring
   should be the sentence a Scratch block would be. We're already
   doing this in `studioDocs.js`.
6. **Stay consistent with PICO-8 vocabulary for the basics** —
   beginners do look at PICO-8 tutorials.
7. **When PICO-8 and Scratch disagree, lean Scratch** — except
   where PICO-8 is genuinely shorter/clearer (`cls`, `spr`).

---

## Index — every proposed name

Flat alphabetical list of all the new symbols proposed in this
doc, so you can ctrl-F. Constants are grouped at the bottom.

| Name | Kind | Category | § |
|---|---|---|---|
| `abs(v)` | function | math | 1 |
| `angle_to(x1, y1, x2, y2)` | function | math | 1 |
| `anim(n_frames, fps)` | function | animation | 3 |
| `anim_once(n_frames, fps, start_t)` | function | animation | 3 |
| `arp(root, type, instr, vol, step)` | function | sound (Strudel) | 13 |
| `bounce_at_edges(&x, &y, &vx, &vy, w, h)` | function | collision | 2 |
| `boxes_touch(ax, ay, aw, ah, bx, by, bw, bh)` | function | collision | 2 |
| `broadcast(msg_id)` | function | events | 11 |
| `circles_touch(ax, ay, ar, bx, by, br)` | function | collision | 2 |
| `clamp(v, lo, hi)` | function | math (also alias for `mid`) | 1 |
| `cos_deg(degrees)` | function | math | 1 |
| `distance(x1, y1, x2, y2)` | function | math | 1 |
| `dx(steps, degrees)` | function | math | 1 |
| `dy(steps, degrees)` | function | math | 1 |
| `ease_in(t)` | function | easing | 4 |
| `ease_in_out(t)` | function | easing | 4 |
| `ease_out(t)` | function | easing | 4 |
| `fget(spr, flag)` | function | sprite flags | review (2026-05-30) |
| `follow(target_x, target_y, world_w, world_h)` | function | camera | 8 |
| `fset(spr, flag, on)` | function | sprite flags | review (2026-05-30) |
| `fps()` | function | debug | 16 |
| `frame()` | function | time | 9 |
| `gp_axis(slot, axis)` | function | input | 15 |
| `gp_present(slot)` | function | input | 15 |
| `groove(group, midi, instr, vol, dur_ms)` | function | sound (Dilla) | 14 |
| `groove_jitter(group, ticks)` | function | sound (Dilla) | 14 |
| `groove_push(group, ticks)` | function | sound (Dilla) | 14 |
| `groove_swing(group, percent)` | function | sound (Dilla) | 14 |
| `length(x, y)` | function | math | 1 |
| `lerp(a, b, t)` | function | math | 1 |
| `load(slot)` | function | persistence | 6 |
| `max(a, b)` | function | math | 1 |
| `min(a, b)` | function | math | 1 |
| `near(ax, ay, bx, by, d)` | function | collision | 2 |
| `noise(x)` | function | noise | 5 |
| `noise2(x, y)` | function | noise | 5 |
| `noise3(x, y, z)` | function | noise | 5 |
| `off_beat(fraction, midi, instr, vol)` | function | sound (Strudel) | 13 |
| `often()` | function | sound (Strudel) | 13 |
| `palindrome(beat, length)` | function | sound (Strudel) | 13 |
| `pause(paused)` | function | runtime | 16 |
| `paused()` | function | runtime | 16 |
| `pen_color(c)` | function | turtle | 12 |
| `pen_down()` | function | turtle | 12 |
| `pen_up()` | function | turtle | 12 |
| `pitch(note_name)` | function | sound (Strudel) | 13 |
| `point_in_box(px, py, bx, by, bw, bh)` | function | collision | 2 |
| `print_centered(text, y, color)` | function | graphics | 17 |
| `print_right(text, right_x, y, color)` | function | graphics | 17 |
| `rarely()` | function | sound (Strudel) | 13 |
| `received(msg_id)` | function | events | 11 |
| `remap(v, a, b, c, d)` | function | math | 1 |
| `rev_step(beat, length)` | function | sound (Strudel) | 13 |
| `rnd_between(lo, hi)` | function | random | 10 |
| `rnd_float()` | function | random | 10 |
| `rnd_float_between(lo, hi)` | function | random | 10 |
| `save(slot, value)` | function | persistence | 6 |
| `sign(n)` | function | math (alias for `sgn`) | review |
| `sin_deg(degrees)` | function | math | 1 |
| `sometimes()` | function | sound (Strudel) | 13 |
| `spr_rot(idx, x, y, deg)` ✓ | function | graphics | 18 |
| `sspr_ex(sx, sy, sw, sh, dx, dy, dw, dh, deg, ox, oy)` ✓ | function | graphics | 18 |
| `str(fmt, ...)` | function | strings | 7 |
| `stutter(midi, instr, vol, times)` | function | sound (Strudel) | 13 |
| `tick()` | function | time | 14 |
| `tick_pos()` | function | time | 14 |
| `tile_at(px, py)` | function | collision | 2 |
| `timer()` | function | time | 9 |
| `timer_reset()` | function | time | 9 |
| `touching_color(x, y, w, h, color)` | function | collision (Scratch) | 2 |
| `touching_map(x, y, w, h)` | function | collision | 2 |
| `fillp(pattern)` | function | graphics | review (2026-05-30) |
| `turtle_at(x, y)` | function | turtle | 12 |
| `turtle_face(degrees)` | function | turtle | 12 |
| `turtle_home()` | function | turtle | 12 |
| `turtle_move(steps)` | function | turtle | 12 |
| `turtle_turn(degrees)` | function | turtle | 12 |
| `voices_active()` | function | debug | 16 |
| `AXIS_LX` / `AXIS_LY` / `AXIS_RX` / `AXIS_RY` / `AXIS_LT` / `AXIS_RT` | constants | input | 15 |
| `PPQ` | constant (96) | time | 14 |

**Totals:** ~60 new functions + ~7 constants. Roughly doubling the
current API surface — which is why this lands in three passes, not
one.
