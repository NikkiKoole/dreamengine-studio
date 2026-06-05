# Debug harness — play a cart "together"

A reproducible way to watch a cart run input-by-input: record a play session and
replay it deterministically, or script exact inputs yourself, while a per-frame
trace tells you precisely what the engine did. Built for the class of bugs you
can't see by reading source — timing, input, "why did nothing happen when I
pressed the key."

The whole thing is **off by default and native-only**. A normal run (the editor's
▶ button, the web build) touches none of it.

---

## Why it's possible

The game clock is frame-driven, not wall-clock-driven where it counts: the musical
beat advances from the `dt` fed to `sound_tick()` once per frame (`runtime/sound.h`).
So pinning the timestep makes the *entire* game — falling notes, beat, judge
windows — reproducible. The only other non-determinism is `now()`/`dt()` (the wall
clock) and the two RNGs; the harness pins all three.

---

## Runtime flags (`runtime/studio.c`)

The compiled cart binary accepts:

| Flag | Effect |
|------|--------|
| `--det` | fixed 1/60 timestep + seeded RNG → byte-for-byte reproducible |
| `--seed N` | RNG seed (default 1) |
| `--record FILE` | log live key changes as `<frame> <keycode> <down>` (one per change) |
| `--replay FILE` | drive input from a recording, deterministically (implies `--det`) |
| `--script FILE` | drive input from a hand-authored frame script (implies `--det`) |
| `--trace FILE` | one JSONL line of state per frame (see "Telemetry" below) |
| `--frames N` | stop after N frames (use for batch/headless runs) |
| `--dump DIR` | export a screenshot filmstrip |
| `--dump-every N` | … every Nth frame (default 1 with `--dump`) |
| `--headless` | hidden window (for batch replay/script) |
| `--save-dir DIR` | where `cart.sav`/`cart.kv`/`cart.blob` live (default: cwd). The editor and `play.js` pass `saves/<cart>` automatically, so every cart gets its own save folder under `build/saves/` — harness runs can't clobber (or inherit) another cart's hiscores |

`--record` and `--trace` flush every frame, so a live session can be tailed.

Note this flag is **not** harness-only — it works in any native build, like the
`.bake/` trigger files. A normal editor ▶ run uses it too.

A recording (`--record`) replays under `--replay`; both feed the same injection
engine as `--script`, so they behave identically.

---

## Telemetry: the `DE_TRACE` convention

`--trace` writes the auto fields every frame — `{"f":frame,"t":seconds,"beat":n,
"bpos":fraction,"w":{…}}` — plus every `watch()` value the cart set that frame, in
`w`. Reading a session is reading this file.

To make a cart's internals legible, wrap `watch()` calls in `#ifdef DE_TRACE`:

```c
#ifdef DE_TRACE
    watch("songpos", "%.3f", songpos);
    watch("press", "L%d e=%.3f r=%s arm=%d", L, err, rating_name, armed);
#endif
```

They cost nothing in a normal build (the macro isn't defined) and feed the trace
in a harness build. `runtime/studio.c`'s `watch()` already drives the on-screen F1
overlay and the crash dump; this just tees it to a file. See `tools/carts/smooch.c`
for a worked example.

---

## The driver: `tools/play.js`

Compiles a cart from its source of truth (`tools/carts/<name>.c` + optional
`.cart.js`) **with `-DDE_TRACE`**, then runs a harness mode. Beat-scripts are
compiled to frame events here, so you author in musical time, not frame counts.

```bash
node tools/play.js <name> run                  # windowed live play
node tools/play.js <name> record <out.rec>     # play live, capture inputs
node tools/play.js <name> replay <in.rec>      # replay a recording
node tools/play.js <name> beats  <in.beats>    # compile a beat-script + run (you drive)
node tools/play.js <name> script <in.script>   # run a raw frame-script as-is
```

Options pass through: `--trace --frames --dump --dump-every --headless --seed --bpm`.

### Beat-script format

```
bpm 96            # tempo for beat->frame conversion (or --bpm)
start 2           # press SPACE at this frame; beat 0 lives here (default 2)
beat 20 tap S 3   # at beat 20 press S, release 3 beats later (a hold)
beat 4  tap A     # at beat 4 tap A briefly (default 0.12 beat)
frame 900 down L  # raw frame events still work and pass through
```

### Mouse (point-and-click carts)

Record/replay capture the **mouse** too (canvas-space pointer + L/R/M buttons), and
a raw frame-script (`<name> script file.script`) can drive it:

```
move  100 160 120      # move the pointer to canvas (160,120) at frame 100
click 120 204 135      # at frame 120: move there + left-click (a 4th arg: 1=right 2=middle)
down  130 N            # keys still work alongside (open the notebook)
up    132 N
click 165 160 100      # click to close it
```

That makes mouse-driven carts (neon rain, the economy game, …) fully scriptable and
replayable — `mouse_x/y/mouse_pressed` all read the injected pointer under replay.

### Gotcha: injected keys can hit the pause overlay

Scripted input goes through the same path as real input, so the **runtime pause
overlay** reacts to it: `ENTER` opens it (freezing `update()` — your trace's
`watch()` fields go empty `{}` from that frame), and while it's open **Z/ENTER
confirm** and **UP/DOWN move the selection** — an injected `Z` meant as a cart key
will instead select *Continue* and unpause. Keys the cart itself reads are
*claimed* and never trigger pause (`P` is safe in a cart that polls `P`), but a key
the cart does **not** read is fair game for the overlay. If a scripted run
mysteriously freezes mid-trace, check the script for a stray `ENTER`.

---

## The three ways to "play together"

1. **Record → replay & inspect** — you play once, then
   `node tools/play.js <name> replay you.rec --headless --frames N --dump shots/`
   re-runs your exact session; read the trace / filmstrip to autopsy any moment.
2. **Scripted (you drive)** — author a `.beats` file to hit an exact scenario
   (a perfectly-timed hold, a 1-frame-late press) and read the result.
3. **Live tail** — `node tools/play.js <name> record live.rec` in one terminal,
   `tail -f build/<name>.trace.jsonl` in another, narrate as you play.

---

## Live inspection — on-demand snapshot

While any cart runs (any mode, any backend), you can pull a screenshot or state
snapshot at the exact moment you care about. Write the desired output path into a
trigger file; the game captures the current frame on its next tick, then deletes
the request as the handshake — **gone = fresh and ready to read.**

```bash
# screenshot at the exact moment you want
echo "/abs/path/to/frame.png"  > build/.bake/screenshot_request

# state snapshot (frame + time + all active watch() values as JSON)
echo "/abs/path/to/state.json" > build/.bake/state_request

# profiler snapshot (same schema as build/perf.json — frame budget + draw-call counts)
echo "/abs/path/to/perf.json"  > build/.bake/profiler_request
```

All three work simultaneously. The game creates `build/.bake/` on startup, so the
directory always exists. `profiler_request` works in any native build — you do not
need `-DDE_PROFILE` or the `⏱ profile` button; the counters run in every normal run.

### Before/after diff

```bash
# launch (compile takes ~1-2s before the game actually starts)
node tools/play.js <name> run --headless --frames 600 &

sleep 2.5   # wait for compilation + a few frames to run
echo "$(pwd)/build/.bake/before.json" > build/.bake/state_request

# ... let the interesting event happen ...
sleep 1.0
echo "$(pwd)/build/.bake/after.json"  > build/.bake/state_request

wait
diff build/.bake/before.json build/.bake/after.json
```

The two files have different `f` values — proof they were captured at distinct
moments in the same live run.

### Pre-staged (frame 0)

Write the request files *before* launching — they survive until the game picks them
up, so frame 0 is always captured even if compilation is slow:

```bash
echo "$(pwd)/build/.bake/snap.png"  > build/.bake/screenshot_request
echo "$(pwd)/build/.bake/snap.json" > build/.bake/state_request
node tools/play.js <name> run --headless --frames 3
```

### What the state JSON contains

```json
{"f": 38, "t": 0.6333, "beat": 2, "bpos": 0.8, "w": {"px": "160", "py": "88"}}
```

`w` holds all currently active `watch()` values — not just those set this exact
frame, but everything still alive (age < 60 frames). For carts without `#ifdef
DE_TRACE` instrumentation `w` will be empty, but `f`/`t` still land correctly.

---

## Worked example — the smooch-lounge hold mechanic

The question: *"do I press when the ribbon touches and release when it leaves?"*

```bash
printf 'bpm 96\nstart 2\nbeat 20 tap S 3\n' > /tmp/hold.beats
node tools/play.js smooch beats /tmp/hold.beats --headless --frames 900 --bpm 96
grep '"press"\|"holddone"' build/smooch.trace.jsonl
```

```
f752  press: L1 e=0.000 r=SMOOCH arm=1 hold=3.0      # head hit on the beat -> hold armed
f865  holddone: L1 tail=23.00 sp=23.013 +bonus       # held to the tail -> bonus
```

Press the head one window late instead (`beat 20.35 …`) and the trace shows
`press: L1 WHIFF` with **no `arm`, no `holddone`** — proving the hold never starts
unless the *head* lands as a hit, no matter how long you hold the key. That's a
discoverability finding you can't get by reading the source.

---

## Recipes — using this *today* to find & fix issues (for the next agent)

The harness is open-loop (you pre-bake inputs, then run), but with determinism that
is already enough to find and fix real bugs. The core loop is always the same:

> **reproduce → inspect the trace → locate in source → fix → re-run the *same*
> script and diff the trace.** Because runs are byte-for-byte deterministic, a trace
> diff is your regression test.

A few things to internalize before you start:

- **`--headless` is realtime-capped today** (hidden window, `SetTargetFPS(60)`). A
  900-frame run takes ~15 s; a full song (~5000 frames) ~80 s. Always set `--frames`
  to the smallest window that covers the moment you care about. (Speeding this up is
  Stage 1 in [`../design/headless-autoplay.md`](../design/headless-autoplay.md).)
- **beat ↔ frame:** `frame = start_frame + round(beat * 3600/bpm)`. With the default
  `start 2` and bpm 96, that's `2 + beat*37.5`. Let `play.js` do it — author in beats.
- **Each trace line shows only the watches set *that* frame.** `grep` by watch key
  (`'"press"'`, `'"miss"'`); absence of a key means that branch didn't run.
- A cart must opt into the trace with `#ifdef DE_TRACE watch(...)` (see smooch). If a
  cart you're debugging has none, **add them first** at the suspect code path.

### Recipe 1 — "X feels wrong at moment Y"
Script the moment, run headless, grep the relevant watch:
```bash
printf 'bpm 96\nstart 2\nbeat 20 tap S 3\n' > /tmp/r.beats
node tools/play.js <name> beats /tmp/r.beats --headless --frames 900
grep '"press"\|"holddone"\|"miss"' build/<name>.trace.jsonl
```
The trace tells you the engine's decision (error, verdict, whether a branch fired) —
not your guess about it.

### Recipe 2 — reproduce a human's exact session
```bash
node tools/play.js <name> record build/u.rec      # they play, then close the window
node tools/play.js <name> replay build/u.rec --headless --frames 5000 --dump build/u
```
Now you have their inputs *and* a frame-by-frame trace + filmstrip to autopsy.

### Recipe 3 — find the exact edge of a timing window (sweep)
Iterate-on-replay: vary one number, diff the verdict. Finds off-by-one window bugs.
```bash
for off in 19.90 20.00 20.10 20.28 20.30 20.35; do
  printf "bpm 96\nstart 2\nbeat %s tap S\n" "$off" > /tmp/s.beats
  node tools/play.js <name> beats /tmp/s.beats --headless --frames 800 >/dev/null 2>&1
  echo -n "offset $off -> "; grep -o '"press":"[^"]*"' build/<name>.trace.jsonl | head -1
done
```

### Recipe 4 — catch a crash
The runtime's `crash_handler` dumps the last `watch()` values to **stderr** on
SIGSEGV/SIGFPE/etc. Run *without* redirecting stderr and read the dump — the watches
are your "last known state" before the fault. Pair with `--seed N` so it's repeatable.

### Recipe 5 — detect a soft-lock / stall (the navkit move)
A stall = the clock advances but nothing the player cares about changes. Grep for a
flat-line: e.g. in `ST_PLAY`, `songpos` climbing while `score`/`combo` never move and
no `miss`/`press` fires for a long stretch. navkit does this exact before/after
snapshot in `RunHeadless` (`src/main.c`, the `=== HEADLESS RESULTS ===` block) — copy
the idea: run N frames, then assert "state changed in the way it should have."

### Recipe 6 — verify a fix without regressing
```bash
node tools/play.js <name> beats fix.beats --headless --frames N --trace /tmp/before.jsonl
# ...apply the fix...
node tools/play.js <name> beats fix.beats --headless --frames N --trace /tmp/after.jsonl
diff /tmp/before.jsonl /tmp/after.jsonl     # exactly what your change moved, nothing else
```

### Recipe 7 — a poor-man's autoplayer
You can play a chart *perfectly* by emitting a `beat <b> tap <key>` for each note in
the cart's chart (read it from the source). A flawless run that *doesn't* earn the
grade/award the code promises is a real bug. (Demonstrated on smooch's verse: 16
notes + 1 hold → combo 17, all SMOOCH — the engine delivered exactly what the chart
implied.) This is the seed of the fuzz/autoplay driver in the growth doc.

### Recipe 8 — live visual check (no filmstrip needed)
Run the cart, trigger a screenshot at the moment you're curious about, read it:
```bash
node tools/play.js <name> run &
# ... play until the suspicious frame ...
echo "$(pwd)/build/.bake/suspicious.png" > build/.bake/screenshot_request
wait
```
No `--dump` needed. One PNG, the exact frame you asked for, nothing wasted.

> If `jq` is available, it makes traces far nicer to slice, e.g.
> `jq -c 'select(.w.press) | {f, p:.w.press}' build/<name>.trace.jsonl`.

## Sound tripwire + the `soundcheck` cart

The sound engine counts every request it has to **drop** (ring buffer or delayed-pen full)
and printh-screams `[sound] WARNING: request queue overflow — N sound call(s) DROPPED` —
in the editor log, bake output, and play.js output. Dropped requests are the *silent* bug
class: notes that never play, `instrument()` defines that never land (the "every wav-knob
position sounds like square" incident, 2026-06-04).

`tools/carts/soundcheck.c` is the matching self-test: its `init()` issues the worst-case
define burst (27 slots × 11 calls — ADSR/duty/LFOs/filter/envs **plus the three engine
macros** — + 4 `wave_set` tables in one frame) and `update()` walks the whole sound API,
including the `INSTR_PLUCK` engine, the live `note_harmonics/timbre/morph` setters, and a
40-shot `schedule_hit` burst that stresses the delayed pen. **Grow this cart in the same
commit as any new sound surface** — a kind the walk never pushes is a kind the tripwire
never guards.

```bash
node tools/play.js soundcheck script /dev/null --headless --frames 900 2>&1 | grep "\[sound\]"
# no output = PASS. Any WARNING line = sound calls were lost — fix the engine, not the cart.
```

Run it after touching `runtime/sound.h` (queue sizes, request kinds, new `instrument_*`/
`wave_set`-style bulk APIs). Ears version: run the cart normally — the 10 waves it plays in
sequence (the last is the KS pluck engine) must all sound *different*.
