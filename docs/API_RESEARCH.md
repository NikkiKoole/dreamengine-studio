# API Research — what to steal from the classics

> **Purpose:** before we keep growing `studio.h`, look hard at what
> PICO-8, GameMaker, BlitzMax, DIV Game Studio, p5.js
> (Dan Shiffman / Processing) and Scratch offer for game-making,
> decide which of those abstractions actually earn their keep in a
> dreamengine cart, and produce a concrete list of candidate
> additions with proposed signatures.

This is a scratchpad, not a spec. Nothing in here is committed to
yet — call out which buckets you'd like me to ship first.

---

## What dreamengine has today

For reference, the current API (all of `studio.h`) groups into:

| Group | Symbols |
|---|---|
| **Callbacks** | `update`, `draw` |
| **Input — buttons** | `btn`, `btnp`, `BTN_*` |
| **Input — touch** | `touch_count`, `touch_x`, `touch_y`, `tap`, `stick_x/y`, `touch_controls` |
| **Graphics** | `cls`, `spr`, `sprf`, `sspr`, `pset`, `pget`, `print`, `line`, `rect`, `rectfill`, `circ`, `circfill`, `camera`, `clip` |
| **Map** | `mget`, `mset`, `map`, `MAP_W`, `MAP_H` |
| **Sound** | `sfx`, `music`, `note`, `hit`, `tone`, `chord`, `strum`, `schedule`, `bpm`, `beat`, `beat_pos`, `every`, `euclid`, `chance`, `degree` |
| **Utility** | `rnd`, `now`, `sgn`, `mid` |
| **Palette** | 32 `CLR_*` constants |

Roughly ~70 functions / constants. Strong on graphics, input, sound,
and tile maps. **Weak on collision, math helpers, animation, and
anything that takes time to evolve (tweens, easing, particles).**

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

### DIV Game Studio — the process model

The most distinctive thing in DIV is the **process** primitive: each
game object is a coroutine with its own local state and its own
`loop … frame;` body. This is in our `VISION.md` as the Level-2
learning model and is the biggest open vision item; it's an
architectural undertaking, not just an API addition. Out of scope
for this doc.

What's worth borrowing from DIV's *API* (separate from the process
model):

- **`collision(type)`** — returns the id of a colliding process
  (or 0). Tied to the process model.
- **`get_dist(id)`**, **`get_angle(id)`**, **`get_signed_angle(id)`**
  — these are just `point_distance` and `point_direction` from
  GameMaker but anchored to the process's own coords.
- **`advance(speed)`** — "move forward at my current angle by N
  pixels". Equivalent to `x += lengthdir_x(speed, angle); y +=
  lengthdir_y(speed, angle);`.
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
- **`wait N seconds`** — points straight at the process /
  coroutine model. You can't pause inside a function without
  yielding control. Lives in the Level-2 learning gap from VISION.
- **`create clone of [sprite]`** — same; the process model gives
  you cloned actors with their own state.
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
int   dx(float steps, float degrees);                   // "how much x do I move stepping this way?"
int   dy(float steps, float degrees);                   //  (GameMaker's lengthdir_x/y, renamed)

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

### 18. Convenient sprite drawing

If we ever want rotation: `spr_ext(idx, x, y, xscale, yscale,
angle_deg, flip_x, flip_y)`. Pure raylib `DrawTexturePro` under
the hood. Big jump in expressiveness but also in API complexity.
Probably defer until someone actually asks.

---

## What to defer or skip

- **Particle systems** — fun but a lot of state. Carts can write
  their own with arrays + `for` loops.
- **Pathfinding (mp_grid)** — too narrow.
- **Tweens** — `lerp` + `now()` covers 90% of cases. Real tween
  libraries are nice but big.
- **Vector struct (Vec2)** — without operator overloading the
  ergonomics aren't there; spelling out `vx, vy` is fine.
- **DS structures (lists/maps/grids)** — C arrays are the lesson.
- **Process model** — out of scope here; lives in `VISION.md`.
- **Pixel-perfect sprite collision** — needs to walk the sprite's
  alpha. Worth doing eventually; AABB covers 95% of cases first.
- **3D anything** — fantasy console, not a 3D engine.

---

## Suggested first slice

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
- **`noise()`** (Perlin) for organic motion.
- **Persistence**: `save(slot, val)` / `load(slot)` for high scores.
- **Easings**: `ease_in`, `ease_out`, `ease_in_out`.
- **`follow()`** — camera follow helper.
- **Events**: `broadcast` / `received` — small but worth a real
  pass since it touches the main-loop drain semantics.
- **`rnd_between` / `rnd_float` / `rnd_float_between`** — round
  out the random surface.
- **More Strudel**: `pitch("c4")` for note-name parsing,
  `sometimes`/`often`/`rarely`, `arp` for chord arpeggios,
  `stutter`, `palindrome`, `off_beat`. Pure functions throughout.
- **Dilla timing**: `groove(g, midi, instr, vol, dur_ms)` +
  `groove_swing` / `groove_jitter` / `groove_push` for per-group
  micro-timing. The thing that turns a quantized beat into a
  *song*.

### Third pass (own projects)
- **Turtle graphics** — its own paradigm, big payoff but earns a
  separate ship with example carts (`turtle_move`/`turn`/`face`/
  `home`/`at` + `pen_down`/`up`/`color`).
- **Gamepad support** — `gp_axis(slot, axis)`, `gp_present(slot)`,
  internal augment of `btn()`/`btnp()` to also read controllers.
- **Pause + debug**: `pause(bool)`, `paused()`, `fps()`,
  `voices_active()`.
- **Print alignment**: `print_centered`, `print_right`.
- **`spr_ext`** with rotation + scale — opens up effects.
- **Sound tracker UI** — if the code-first sound path turns out
  not to be enough.

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
| `follow(target_x, target_y, world_w, world_h)` | function | camera | 8 |
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
| `spr_ext(idx, x, y, xs, ys, deg, fx, fy)` | function | graphics | 18 |
| `str(fmt, ...)` | function | strings | 7 |
| `stutter(midi, instr, vol, times)` | function | sound (Strudel) | 13 |
| `tick()` | function | time | 14 |
| `tick_pos()` | function | time | 14 |
| `tile_at(px, py)` | function | collision | 2 |
| `timer()` | function | time | 9 |
| `timer_reset()` | function | time | 9 |
| `touching_color(x, y, w, h, color)` | function | collision (Scratch) | 2 |
| `touching_map(x, y, w, h)` | function | collision | 2 |
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
