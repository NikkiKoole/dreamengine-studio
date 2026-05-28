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

Scratch's biggest lesson isn't a function — it's the *naming*: it
calls things what they actually do, in plain English. "touching
color" is a much better name than "color collision check". Worth
keeping in mind as we name new API.

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

---

## Proposed additions, by category

Each entry is a guess at a signature + a one-line rationale. None
are merged. We can pick whichever buckets feel like the right next
step.

### 1. Math — small, high-leverage

These are universally useful and tiny to implement.

```c
// numeric
int   abs_i(int v);
int   min_i(int a, int b);
int   max_i(int a, int b);
float lerp(float a, float b, float t);              // p5
float remap(float v, float a, float b, float c, float d);  // p5 'map' renamed
float clamp_f(float v, float lo, float hi);          // float twin of mid()

// geometry
float dist(int x1, int y1, int x2, int y2);          // p5
float mag(int x, int y);                             // p5
float angle_to(int x1, int y1, int x2, int y2);      // GameMaker point_direction; returns degrees
int   len_dir_x(float len, float deg);               // GameMaker classic
int   len_dir_y(float len, float deg);

// trig (degree-based to match angle_to + len_dir_*; matches BlitzMax)
float sin_d(float deg);
float cos_d(float deg);
```

We expose **degree-based trig** rather than radians or turns. Two
reasons: angles in degrees are taught in school; and `len_dir_*`
plus `angle_to` chain cleanly in degrees. Floats throughout because
the cost of int-only math is more bug than the cost of teaching
floats.

### 2. Collision — the missing layer

```c
// AABB rectangle overlap
bool rect_overlap(int ax, int ay, int aw, int ah,
                  int bx, int by, int bw, int bh);

// point-in-rect (we sort of have `tap`, but that's touch-only)
bool point_in_rect(int px, int py, int rx, int ry, int rw, int rh);

// circle-circle overlap
bool circle_overlap(int ax, int ay, int ar, int bx, int by, int br);

// distance check helper (squared-distance internally so we avoid sqrt)
bool within(int ax, int ay, int bx, int by, int d);

// tile-map collision: is any cell under this AABB non-empty?
bool map_solid(int x, int y, int w, int h);   // pixel-space AABB
int  map_at(int px, int py);                  // sample the map at pixel coords

// Scratch-style: is any pixel under this AABB the given palette color?
bool touching_color(int x, int y, int w, int h, int color);
```

`map_solid` is the cart-friendly version of "I am a 16×16 sprite at
(x, y) — am I on top of a wall?". Internally it walks the cells
that the AABB overlaps and returns true if any is non-zero. For a
richer notion of "solid" the cart can use `map_at` and decide for
itself.

`touching_color` is the **Scratch idea** ported in. It walks the
pixels of the bbox and asks `pget(px, py) == color`. Profoundly
beginner-friendly: paint your walls red, ask
`touching_color(x, y, 16, 16, CLR_RED)`, you have collision with no
typing system, no instance lists, no hitboxes. Pairs naturally with
the sprite editor (kids already think in colors) and with `pget`
which is already in the runtime. Implementation cost is tiny since
`pget` already reads from the previous frame's canvas snapshot.

### 3. Animation — one helper, no objects

GameMaker's `image_index` is implicit; PICO-8 makes you do
`spr(base + (t//8) % n, x, y)`. The latter is fine but not great
for absolute beginners. One helper goes a long way:

```c
// returns the current frame (0..n_frames-1) for an animation
// running at `fps` FPS, anchored to the global clock.
int anim(int n_frames, float fps);

// same, but plays once from a given start time and returns
// the last frame after it ends.
int anim_once(int n_frames, float fps, float start_t);
```

Usage: `spr(walk_base + anim(4, 10), x, y);` — walk cycle, 10
frames per second.

### 4. Easing / interpolation

We get most of the way with `lerp`. Add a few common easings for
when the user wants something other than linear motion:

```c
float ease_in(float t);     // t*t
float ease_out(float t);    // 1 - (1-t)*(1-t)
float ease_in_out(float t); // smoothstep — 3t² - 2t³
```

Composes with `lerp` and `now()` for transitions.

### 5. Noise — the Shiffman special

```c
float noise(float x);                    // 1D Perlin / value noise, returns 0..1
float noise2(float x, float y);          // 2D
float noise3(float x, float y, float z); // 3D — useful for animated terrain
```

Value noise is ~30 lines and good enough. Perlin is ~80 and feels
better. Either is worth having — organic motion (clouds, wobble,
flow fields, drifting fog) is otherwise hard to get cheaply.

### 6. Persistence — high scores and cart state

```c
int  dget(int slot);              // read int from persistent slot
void dset(int slot, int value);   // write
// total slots: 64 (PICO-8 parity)
```

Stored alongside the cart binary or in a per-cart `cart.dat` file
in `build/`. Trivial in main.cjs land.

### 7. String formatting — make `print` more useful

Right now you can't easily print a number. C's `snprintf` is
correct but ceremonial. A tiny helper returning a static buffer is
all most carts need:

```c
const char *str(const char *fmt, ...);   // sprintf into a 64-byte rolling buffer
// usage: print(str("score %d", score), 4, 4, CLR_WHITE);
```

Rolling buffer means: subsequent calls overwrite. Fine for the
single-frame case; document the gotcha.

### 8. Camera helpers — quality-of-life

```c
// follow target, clamped to a world rect
void camera_follow(int target_x, int target_y,
                   int world_w, int world_h);
```

Internally calls `camera(mid(0, target_x - SCREEN_W/2, world_w -
SCREEN_W), mid(0, target_y - SCREEN_H/2, world_h - SCREEN_H))`.
The default cart's draw() already inlines this; making it a helper
saves carts five lines.

### 9. Time helpers

```c
int frame(void);   // monotonic frame counter, increments per update()
```

Pairs nicely with `now()` (seconds). Some users want frame-based
timing instead of seconds-based.

### 10. Random — small additions

```c
int   rnd_range(int lo, int hi);    // int in [lo, hi)
float rnd_f(void);                  // float in [0.0, 1.0)
float rnd_f_range(float lo, float hi);
```

Currently `rnd(n)` only gives `[0, n)` ints. The float variants
are needed for noise/lerp/animation patterns.

### 11. Convenient sprite drawing

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

1. **Math + collision basics**: `abs_i`, `min_i`, `max_i`, `lerp`,
   `remap`, `dist`, `angle_to`, `len_dir_x/y`, `sin_d`, `cos_d`,
   `rect_overlap`, `circle_overlap`, `within`, `map_solid`,
   `map_at`, `touching_color`.
2. **`str()`** for formatted printing.
3. **`anim()`** for animation cycles.

That's ~20 small functions, all of which are pure (no global state,
no allocations), all of which compose with what's already there,
and together they unlock writing real games — collision, motion at
angle, score display, walking sprites.

`noise()`, persistence (`dget/dset`), easings, camera helpers, and
the rotated sprite (`spr_ext`) can come as a second pass once the
first lands.
