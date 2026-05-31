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

`--record` and `--trace` flush every frame, so a live session can be tailed.

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
