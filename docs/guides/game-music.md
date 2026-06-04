# Generative game music — recipes

How to give a cart a soundtrack that sounds **composed**, not generated. The worked
example is **`tools/carts/bossa.c`** (the "bossa radio" cart) — every block below
exists there as running, listenable code. These blocks are designed to be **copied
into your cart and tweaked** — there is deliberately no shared music header yet;
when a second style cart lands and the copying starts to hurt, that's the signal to
extract one.

The engine already has everything you need: `instrument()` + filters/LFOs/envelopes
for timbre, `schedule_hit()` for sample-accurate timing, `bpm()/beat()/beat_pos()`
for the clock, and `chord()/degree()/euclid()/chance()` as music-theory helpers.
What this guide adds is the **layer above the API**: harmony, time-feel, and
arrangement.

## The five layers

A generative soundtrack is five small machines stacked. Build them in this order —
each is independently testable by ear:

1. **Step clock** — a 16th-note grid, scheduled ahead (jitter-free timing)
2. **Chord brain** — a weighted walk over harmonic functions (the changes)
3. **Voice leading** — chords connect by nearest-tone movement (sounds composed)
4. **Time feel** — the rhythmic pattern language of the style (clave, swing…)
5. **Melody** — one rhythmic cell, re-pitched to each chord

## 1. The step clock — schedule ahead, never trigger on the frame

Triggering notes the frame you notice a step changed adds up to 16ms of frame
jitter — instantly audible as a drunk drummer. Instead, run a step counter against
`beat()`/`beat_pos()` and schedule every step **one step ahead** with
`schedule_hit()`, which fires at a sample-accurate future time:

```c
static long   scheduled = -1;          // last absolute 16th-step we scheduled
static double stepMs    = 119.0;       // 60000 / (bpm * 4)

void update(void) {
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;   // 16ths, fractional
    stepMs = 60000.0 / (tempo * 4);
    long target = (long)pos + 1;                            // one step of lookahead
    while (scheduled < target) { scheduled++; play_step(scheduled, pos); }
}

static void play_step(long abs, double pos) {
    int dly = (int)((abs - pos) * stepMs);   // ms until this step actually lands
    if (dly < 1) dly = 1;
    // ... schedule_hit(dly + offset, midi, instr, vol, dur) for everything on this step
}
```

All feel offsets (strum stagger, swing, behind-the-beat melody) are added to `dly`
in milliseconds. The pattern survives `bpm()` changes mid-song because `pos` is
derived from the engine's own beat clock.

## 2. The chord brain — functions, not chords

Don't generate chords; generate **harmonic functions** (I, ii, V7…) in a random
key, and render them to pitches later. A weighted transition table is enough to
sound like real changes — it reads like a jazz harmony cheat-sheet:

```c
// each function = scale-degree offset + chord quality
enum { F_I, F_ii, F_iii, F_IV, F_V, F_vi, F_II7, F_VI7, F_bII7, F_iv, F_bVII7, ... };
static const int F_OFF[]  = { 0, 2, 4, 5, 7, 9, 2, 9, 1, 5, 10, ... };
static const int F_QUAL[] = { Q_MAJ7, Q_MIN7, Q_MIN7, Q_MAJ7, Q_DOM7, ... };

// where can each function go? repeats = more likely
// ii→V→I; secondary dominants resolve down a fifth (VI7→ii, II7→V);
// bII7 = tritone sub of V; iv→bVII7→I = the backdoor cadence
T(T_ii,   F_V, F_V, F_V, F_V, F_V, F_bII7, F_bII7)
T(T_V,    F_I, F_I, F_I, F_I, F_I, F_iii, F_vi)      // mostly home, sometimes deceptive
T(T_VI7,  F_ii, F_ii, F_ii, F_ii, F_ii)
...
```

Two constraints turn a random walk into a **song**:

- **Force the cadence.** Whatever the walk does, hardcode the last two bars of a
  section to `ii → V` (or `ii → bII7`). The loop always pulls back home.
- **Use a form.** Generate an 8-bar A and an 8-bar B, play **AABA**. The B section
  starts away from home (IV or vi). Repetition is what makes listeners trust the
  tune; the form gives them a map.

Verified output from bossa.c (trace, key G):
`Gmaj7 E7 Am7 D7 Gmaj7 A7 Am7 D7` (A) … `Cmaj7 Cm6 Gmaj7 …` (B) — I–VI7–ii–V and
a borrowed iv, textbook changes nobody wrote.

## 3. Voice leading — the single biggest "sounds composed" trick

`chord()` stacks every chord from its root, so progressions leap around like a
theory exercise. Real players move each finger to the **nearest** tone of the next
chord. Keep 3 voices as state and lead them:

```c
static int gv[3];   // current voicing, midi

static void lead_voices(int f) {                      // f = harmonic function
    int pcs[3];                                       // target pitch classes:
    for (int k = 0; k < 3; k++)                       // rootless! bass owns the root,
        pcs[k] = (root_pc(f) + QVOICE[F_QUAL[f]][k]) % 12;  // these are 3rd/7th/9th
    bool used[3] = {0};
    for (int v = 0; v < 3; v++) {                     // each voice: nearest unused tone
        int bestJ = -1, bestC = 0, bestD = 99;
        for (int j = 0; j < 3; j++) {
            if (used[j]) continue;
            int d = ((pcs[j] - gv[v]) % 12 + 18) % 12 - 6;   // nearest octave copy
            if (iabs(d) < bestD) { bestD = iabs(d); bestJ = j; bestC = gv[v] + d; }
        }
        used[bestJ] = true; gv[v] = bestC;
    }
    // clamp each voice back into the instrument's register (e.g. 58..82)
}
```

Rootless voicings matter: the comping instrument plays **3rd + 7th + color** (9th,
6th…) and the bass plays the root. Quality tables from bossa.c:

| quality | voicing intervals | reads as |
|---|---|---|
| maj7 | 4, 11, 14 | 3 7 9 |
| m7   | 3, 10, 14 | b3 b7 9 |
| 7    | 4, 10, 14 | 3 b7 9 |
| m7b5 | 3, 6, 10  | b3 b5 b7 |
| m6   | 3, 9, 14  | b3 6 9 |

## 4. Time feel — the style lives here

The notes are almost interchangeable between styles; the **placement** isn't.
Three mechanisms, all just milliseconds added to `dly`:

- **Pattern masks.** The style's signature rhythm as step sets over a 2-bar
  (32-step) grid. Bossa: comping on the clave `{0,6,12,20,26}`, bass on the surdo
  (`0`=root, `8`=fifth, pickup at `14`). Write them as data, pick variants per song.
- **Anticipation.** Hits in the last 8th of a bar already play the **next** bar's
  chord (`if (s % 16 >= 14) bar++`). This one line is half of what makes bossa
  sound like bossa.
- **Humanize.** `dly + rnd(4)` on percussion, strum chords 8ms apart per voice,
  melody 10–18ms **behind** the beat. Velocity: accent the pattern's anchor step,
  drop ghost notes with `chance()`.

**Swing** (for the future lofi cart): straight 16ths, but delay every off-beat 8th
by 55–62% of an 8th instead of 50%:

```c
int swing_ms = (step % 2 == 1) ? (int)(stepMs * 2 * (swingPct - 0.50f)) : 0;
schedule_hit(dly + swing_ms, ...);    // swingPct 0.55 subtle .. 0.62 drunk
```

## 5. Melody — one cell, re-pitched (the One Note Samba trick)

Don't generate free melody — it noodles. Generate **one syncopated rhythmic cell**
(3–6 onsets in 2 bars, biased to off-beats) and repeat it for the whole song while
its **pitches re-resolve to each new chord**: pick the chord tone (or 9th) nearest
to where the melody currently sits, with a little random drift. Repetition makes it
a hook; the changing harmony keeps it alive. Rest whole cell-instances
(`chance(60)`) — the silences are what make it background music.

## Seeds — composition vs performance

Make every song a 32-bit seed through a **cart-local PRNG** (xorshift32), and split
the randomness in two:

- **Composition** (seeded): key, progression, patterns, melody cell, tempo, title.
  Same seed = same tune, always. Display the seed; let the player/agent pin it
  (bossa.c: `#define BOSSA_SEED 0x…`, plus `R` = replay, `[ ]` = session history).
- **Performance** (engine `rnd()`, unseeded): humanize jitter, ghost-note rolls,
  the melody's exact pitch path. Replaying a seed is the band playing the same
  chart again, not a tape.

This split is free to implement and turns "nice accident" into a shareable artifact.

## Wiring it into a game

- **Intensity is the game hook.** Expose one 0–3 level that gates layers
  (bossa.c: bass+comping → +shaker → +clave+melody → louder/denser). Map it to
  game state: menu 0, explore 1, action 2, boss/final lap 3. Layers enter on
  2-bar boundaries, not instantly.
- **Stay out of the sfx register.** Keep the soundtrack mid-volume (vol 3–5) and
  mid-register; give game events vol 6–7 and the extremes. Hit-sounds read through
  a full mix fine if the music never peaks.
- **Stingers over switches.** On big events, don't swap songs — drop the melody for
  2 bars, or force `intensity = 3` for 4 bars and decay. The clock and harmony keep
  running, so it never sounds like a skip.
- **Voices are a budget.** 8 total. The bossa mix peaks at 7: 3 comping + bass +
  shaker + rim + melody. Keep chord stabs at 3 voices and decays short, or notes
  start stealing.

## Instrument recipes

From bossa.c (slots 5+; all one `instrument()` + a filter, some + env/LFO):

```c
instrument(I_GTR, INSTR_TRI, 1, 180, 1, 120);          // nylon guitar pluck
instrument_filter(I_GTR, FILTER_LOW, 2200, 3);
instrument_env(I_GTR, 0, ENV_CUTOFF, 0, 90, 1400);     // attack sparkle

instrument(I_BASS, INSTR_TRI, 2, 200, 4, 80);          // round fingered bass
instrument_filter(I_BASS, FILTER_LOW, 700, 2);

instrument(I_FLUTE, INSTR_SINE, 25, 120, 5, 140);      // breathy lead
instrument_lfo(I_FLUTE, 0, LFO_PITCH, 5.2f, 0.18f);    // vibrato
instrument_filter(I_FLUTE, FILTER_LOW, 2600, 2);

instrument(I_SHAKER, INSTR_NOISE, 1, 45, 0, 25);       // caxixi (16ths)
instrument_filter(I_SHAKER, FILTER_HIGH, 5200, 4);

instrument(I_RIM, INSTR_NOISE, 0, 28, 0, 18);          // woody cross-stick
instrument_filter(I_RIM, FILTER_BAND, 1800, 9);
instrument_env(I_RIM, 0, ENV_PITCH, 0, 20, 18);
```

For other styles: **Rhodes/epiano** = SINE or TRI + `LFO_VOLUME` tremolo ~4.5Hz +
lowpass ~1500 + tiny `ENV_CUTOFF` bark; **vinyl crackle** = `chance(8)` per frame →
`hit(90+rnd(8), I_NOISE_HP, 1, 15)`; **dusty kick** = SINE + `ENV_PITCH` drop
(amount ~14, decay 60); **tape wow** = a held pad via `note_on` with slow
`note_pitch(h, base + sinf(t*0.7f)*0.15f)` drift; **chip lead** = SQUARE +
`instrument_duty(slot, 0.25f)` + `LFO_DUTY`.

## Style cheat-sheet (the parking lot)

What changes between styles is mostly **data** — the engine above carries over:

| style | tempo | grid feel | harmony bias | kit | status |
|---|---|---|---|---|---|
| **bossa** | 112–140 | straight, clave masks, anticipation | maj7/9, ii-V chains, tritone subs, iv | nylon gtr, surdo bass, shaker, rim, flute | ✅ `bossa.c` |
| **lofi jazz** | 70–85 | swing 56–60%, melody very late | m9/maj9, slower changes (2 bars/chord), pentatonic noodle | rhodes, soft kick/snare, dusty hats, crackle | idea |
| **ambient** | 60–80 | beatless; chords every 2–4 bars via held `note_on`, voice-led with `note_glide` | maj7/sus, modal drift, no cadence forcing | 2 detuned pads, sub bass, sparse bell | idea |
| **chiptune action** | 140–170 | straight 16ths, driving; euclid() fills | i–bVI–bVII–i loops, power chords | square lead 25% duty, tri bass, noise kit | idea |

Each future style should land as its own cart (lofi radio, ambient radio…) reusing
this guide's blocks; if by the second one the copied core (clock + chord brain +
voice leading, ~150 lines) has stayed identical, extract it then.

## Verifying without ears

The debug harness makes the music inspectable (see `debug-harness.md`):
`watch()` the current bar/chord/step under `#ifdef DE_TRACE`, then

```bash
node tools/play.js bossa run --headless --trace t.jsonl --frames 3600
```

and read the chord stream out of the trace — progression, form, and loop-around
are all checkable from JSONL. Determinism of a pinned seed is two runs + a diff.
