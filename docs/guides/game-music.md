# Generative game music — recipes

How to give a cart a soundtrack that sounds **composed**, not generated. Two worked
examples, deliberately opposite in engine shape:

- **`tools/carts/bossa.c`** ("bossa radio") — percussive: a 16th-note step clock,
  schedule-ahead plucked hits, Markov jazz harmony with forced cadences.
- **`tools/carts/ambient.c`** ("ambient radio") — beatless: held `note_on` voices
  that morph via `note_glide`, per-note LFOs for tape wow, modal drift harmony
  with no cadences at all.

Every block below exists in one or both as running, listenable code. The blocks are
designed to be **copied into your cart and tweaked** — there is deliberately no
shared music header yet. Two carts in, the empirical split is: **voice leading, the
seed/PRNG contract, and the session-history UI copied verbatim**; the clock, the
chord brain, and every instrument changed completely. The reusable core is real but
small (~80 lines) — extract it when a third cart confirms the boundary.

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

**Beatless styles don't need any of this.** When nothing is percussive, millisecond
precision is inaudible — ambient.c just compares `beat() + beat_pos()` against a
`changeAt` mark in `update()` and retargets its held voices when it passes. Reach
for the schedule-ahead clock only when onsets have attack.

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

### The other chord brain: modal drift (no cadences)

Functional harmony *pulls*; ambient music must *float*. ambient.c uses a different
generator: pick **one mode** per song (lydian/dorian/mixolydian/aeolian — the mood
knob), build 7th chords by stacking alternate degrees, and walk the degrees with
weighted steps (mostly ±3rds/4ths — neighbouring in-mode chords share tones, so
changes feel like the light shifting, not a progression progressing):

```c
static const int MODESC[4][7] = { { 0,2,4,6,7,9,11 }, ... };   // lydian, dorian, …
static const int WALK[] = { 3,3,3, -3,-3,-3, 2,2, -2,-2, 1,-1 };  // weighted steps
// chord on degree d = pitch classes of degrees d, d+2, d+4, d+6 (all in-mode)
```

No cadence forcing, no form — 16 chords, 8–16 beats each, then the next song.
Verified from trace (C lydian): `Cmaj7 D7 F#m7b5 Em7 Gmaj7 …` — every chord
in-mode, nothing resolving.

### The third chord brain: the vamp (jangle.c)

Simplest of all, and the right one for song-like background music: pick **2–4
in-mode chords once** (weighted pool, always opening on I — verified trace output:
`Bb Ab` looping, i.e. I–bVII in Bb mixolydian) and loop them the entire song.
Harmony carries no form at all; **the arrangement is the form** — layers enter and
exit per vamp-loop (intro: gtr+bass → groove: +drums → solo: +lead → outro: drums
drop). This is also the game-friendliest brain: a vamp tolerates being ducked,
paused, and intensity-swept without losing its place.

### The fourth chord brain: a stolen playbook (jingle.c)

When an artist's harmonic language has been *catalogued* (here: Reverb Machine's
"Mac DeMarco Chord Theory"), encode it as **template progressions + flavor rolls**
instead of a generative walk. jingle.c stores six progressions from cited songs as
`{degree, quality, borrowed}` rows ("Blue Boy" I–ii–vi–bVII, "Another One"
IV–iii–bIII–iii, "A Heart Like Hers" vi–bVImaj7–v–I7 …), picks one for the verse
and one for the chorus, then rolls the signature variables per song: the bVII
voiced maj / 7 / 9, the bIII voiced maj7 / m6. Three supporting rules from the
same source:

- **Chromatic bass descents** — the templates are chosen so roots walk by
  semitone; a *voice-led bass* (nearest octave copy of each root, like the chord
  voices) keeps the descent intact in every key.
- **Melodic accommodation** — over a *borrowed* chord the melody narrows to that
  chord's own tones (+9), so it never clashes a semitone with the borrowed note;
  over diatonic chords it roams the whole key.
- **Verse/chorus form** — verses draw from the diatonic-leaning templates,
  choruses from the borrowed-forward ones (the "Moonlight on the River" move:
  the chorus is where the bIII lives).

Trace-verified: verse `G D F9 C` / chorus `Bbm6 Em7 Am7 D9` — I–V–bVII9–IV into a
borrowed-bIII chorus, a plausible DeMarco chart nobody wrote.

### The fifth chord brain: the sampled loop (lowend.c)

How hip hop hears harmony — encoded from the r/musictheory analysis of ATCQ's
"Electric Relaxation" loop (`Bmaj7 D#sus G#maj9 F#sus2 Emaj9#11` — a *3-bar* loop
of modal mixture where every borrowed chord is voiced lush). Three rules:

- **Mixture, voiced pretty.** Pool = I, ii, IV, Vsus + borrowed bIII/bVI/bVII —
  all as maj7/maj9 (the bVI gets the maj9#11). The 9sus replaces the dominant.
- **Odd loop lengths.** 2/3/4 bars with 3 favoured — a 3-bar loop never squares
  with the ear, so the head keeps nodding. Let it roll over 8-bar section lines;
  the sample doesn't know about your song form.
- **Rotate the cut.** Generate the progression with a real Vsus→I cadence inside
  it, then rotate the loop by a random offset — "their sampling moved the tonic."
  The loop starts where the needle dropped, not where home is. Trace-verified:
  `Dmaj7 Amaj7 Gb9sus Bmaj9 Dmaj9 Amaj9` in B — starts on bIII, cadence mid-loop,
  exactly the Electric Relaxation shape.

Production side (The Low End Theory playbook): bass first (voice-led upright,
vol 6, leaning +12ms), minimalism (bass + drums + one element), **layered hits**
(Q-Tip stacked snares — fire a noise crack + sine thump together), the full
groove template, and **no tempo wobble** — samplers don't drift; the machine is
steady and the players lean.

**Pick the brain by what the music is for:** pull (action, songs) → functions +
cadences; float (menus, exploration, night) → one mode + drift; sit (background,
overworld, menus you stay in) → vamp + arrangement; sound like *someone* (homage,
a licensed-feel pastiche) → stolen playbook + flavor rolls; nod (hip hop, anything
loop-born) → sampled loop, cut and rotated.

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

This exact function shipped **unchanged in both carts** (bossa: 3 plucked guitar
voices; ambient: 4 held pad voices) — it's the most copy-safe block in this guide.
In ambient.c each `gv[]` move becomes a *slide*: the pads are `note_on` handles
with `note_glide(h, 900..2200)` set once, so `note_pitch(h, gv[i])` on a chord
change morphs the chord instead of retriggering it. The sound never stops; only
the fingers move.

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

**Swing**: straight 16ths, but delay every off-beat 8th by 55–62% of an 8th
instead of 50%:

```c
int swing_ms = (step % 2 == 1) ? (int)(stepMs * 2 * (swingPct - 0.50f)) : 0;
schedule_hit(dly + swing_ms, ...);    // swingPct 0.55 subtle .. 0.62 drunk
```

### Groove templates — Dilla time, laid-back, rushed

Swing is one number; a *groove* is a **per-lane timing personality**. The J Dilla /
boom-bap feel is not "more swing" — it's different lanes disagreeing about where
the beat is, consistently:

```c
// ms offsets added to dly, per lane — THE groove template
static const int PUSH_KICK  = 0;      // kick defines the grid; leave it
static const int PUSH_HAT   = -8;     // hats rush slightly (eager)
static const int PUSH_SNARE = +22;    // snare drags hard (the head-nod)
static const int PUSH_BASS  = +12;    // bass leans toward the snare's time
// + per-hit jitter: dly + PUSH_X + rnd(5) - 2
```

Numbers to steal: at 90 BPM a 16th is 167ms — useful offsets are 5–35ms (3–20% of
a 16th). Drags larger than ~40ms stop feeling laid-back and start feeling late.
Field note (lowend.c): the maximal template above read as *stumbling* in
practice — the cart shipped at half strength (−5/+12/+7, swing 54%) and that's
where the head-nod lives. Start subtle; turn it up only if the groove vanishes.
Mac DeMarco-style slacker feel = everything straight but the whole kit −0/+10ms
loose against the bass, tempo slightly unstable (`bpm()` wobbled ±1 every few bars).

**Engine note (resolved 2026-06-04):** scheduled notes now fire **sample-exact**.
Before, the delayed pen ticked per audio callback (1024 samples ≈ 23ms), which
quantized all of the above to block edges — strum staggers collapsed, drags rounded
to 0 or 23ms. If a groove feels quantized anyway, check you're adding offsets to a
schedule-ahead `dly` (section 1), not triggering on the frame. No groove API is
needed or planned: templates are cart data; `schedule_hit` is the delivery vehicle.
One real limit: the delayed pen holds 64 pending notes — keep the one-step
lookahead; don't schedule whole bars ahead.

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

- **Intensity is the game hook — but make it a SHIFT, not a gate.** Lesson
  learned the hard way (jingle.c): if the knob gates layers that the arrangement
  has already silenced (kit only exists in choruses), it does nothing audible
  for most of the song. The model that works (jingle.c/lowend.c): the
  arrangement gives each section a baseline density (intro 0 · verse 1 ·
  chorus 2) and intensity *shifts the whole curve*:
  ```c
  int lvl = sectionBase + intensity - 1;     // clamp 0..3
  // layers key off lvl, never off intensity directly
  ```
  The chorus stays fuller than the verse at every setting — the two dimensions
  stay orthogonal — and every notch changes something *immediately*, including
  in the always-on layers (note gaps, touch/velocity). Map intensity to game
  state: menu 0, explore 1, action 2, boss/final lap 3; let changes land on
  2-bar boundaries, not instantly.
- **Tone is the cheapest second axis.** A master-brightness knob (re-issue the
  filter cutoffs ×0.55..1.3, live) reads instantly everywhere and doubles as a
  game state: caves/underwater/flashback = mellow, daylight = bright.
  Prototyped in jingle.c (`T` key).
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

And the held-voice kit from ambient.c — set everything once at `note_on`, then the
engine runs it; no per-frame driving:

```c
instrument(I_PAD, INSTR_SAW, 1400, 600, 6, 2800);        // slow swell, long fade
instrument_filter(I_PAD, FILTER_LOW, 750, 2);
// per voice, once:
h = note_on(midi, I_PAD, 4);
note_glide(h, rnd_between(900, 2200));                   // chord changes = slides
note_lfo(h, 0, LFO_PITCH,  0.05f + drift, 0.04f..0.10f); // tape wow (sub-0.2Hz!)
note_lfo(h, 1, LFO_CUTOFF, 0.04f..0.09f, 350);           // filter breathing
note_pitch(h, midi + rnd_float_between(-0.06f, 0.06f));  // constant detune = width

instrument(I_WIND, INSTR_NOISE, 2000, 800, 5, 2500);     // band-passed weather
instrument_filter(I_WIND, FILTER_BAND, 700, 6);          // + slow LFO_CUTOFF roll
```

For other styles: **Rhodes/epiano** = SINE or TRI + `LFO_VOLUME` tremolo ~4.5Hz +
lowpass ~1500 + tiny `ENV_CUTOFF` bark; **vinyl crackle** = `chance(8)` per frame →
`hit(90+rnd(8), I_NOISE_HP, 1, 15)`; **dusty kick** = SINE + `ENV_PITCH` drop
(amount ~14, decay 60); **chip lead** = SQUARE + `instrument_duty(slot, 0.25f)` +
`LFO_DUTY`.

### The real string — `INSTR_PLUCK` (the first engine, wave ids 16+)

Every guitar recipe above predates the KS engine; the real thing exists now and
takes the same outboard gear (filter / LFO / env). jangle.c and bossa.c carry a
live A/B against their TRI fakes (**G** key); per-station upgrade notes live in
`docs/design/radio-instrument-options.md`:

```c
instrument(I_GTR, INSTR_PLUCK, 1, 0, 7, 180);        // a real string — it decays on its own
instrument_filter(I_GTR, FILTER_LOW, 2600, 2);       // outboard gear works as before
instrument_harmonics(I_GTR, 0.5f);                   // ring time (perceptual: 0.04s thunk → ~2min drone)
instrument_timbre(I_GTR, 0.75f);                     // pick brightness (felt thumb → hard pick)
instrument_morph(I_GTR, 0.2f);                       // pick position (bridge=full → mid-string=hollow)
instrument_lfo(I_GTR, 0, LFO_PITCH, 5.5f, 0.12f);    // chorus warble works VERBATIM — the read
                                                     //   tap is fractional, all pitch tools bend it
```

Two rules of thumb: **give it room** — the string sets its own decay, so use long
`hit()` durations or a long release (a gate end must never chop the ring); and
**roll the macros per song** — three floats per instrument is exactly what the
per-song timbre roll wants to seed. The three knobs are the same on every future
engine (audio-notes §8.1.1); the `pluck` cart is the hands-on tour (grab a string
to bend it, sweep open space to strum).

### The real bar — `INSTR_MALLET` (engine #2)

The struck-bar family — marimba / xylophone / celesta / vibraphone / glockenspiel —
is one wave id (shipped 2026-06-05; macro taste is still settling in the `mallet`
cart, which carries the five hardware names as preset knob positions). Same
rules as the string: it rings on its own, so give it room.

```c
instrument(I_VIBE, INSTR_MALLET, 1, 0, 7, 1200);     // a struck bar — rings on its own
instrument_harmonics(I_VIBE, 0.25f);                 // bar material (0 wood/marimba → 1 metal/bell)
instrument_timbre(I_VIBE, 0.5f);                     // mallet hardness (yarn thump → brass + click)
instrument_morph(I_VIBE, 0.9f);                      // ring length — the top third turns the
                                                     //   vibraphone motor tremolo ON
hit(midi, I_VIBE, 5, 2500);                          // long gate: never chop the ring
```

First customers (`radio-instrument-options.md`): lowend's lead → vibraphone (the
Nujabes sound, the single highest-value engine swap), ambient's bell, ymo's
marimba counterpoint. The roll trick: a mallet *tremolo roll* is just 5–6
`schedule_hit`s ~70ms apart on one bar, alternating vol 3/4 — see the mallet
cart's autoplay.

### The DX keys — `INSTR_FM` (engine #3)

Two-operator FM (shipped 2026-06-05; taste settling in the `fm` cart). Unlike
the string and the bar it does **not** decay on its own — give it a normal
ADSR; what it does do on its own is **mellow within each note** (the FM amount
decays like a real DX strike), so comped chords sparkle on the attack and sit
back in the mix as they ring. The electric-piano gap, finally:

```c
instrument(I_EP, INSTR_FM, 2, 700, 3, 350);          // piano-ish ADSR — the engine needs one
instrument_harmonics(I_EP, 0.15f);                   // ratio detent 1:1 — the harmonic/musical zone
instrument_timbre(I_EP, 0.45f);                      // brightness (decays per note automatically)
instrument_morph(I_EP, 0.10f);                       // a touch of feedback = warmth, not growl
instrument_lfo(I_EP, 0, LFO_VOLUME, 4.5f, 0.10f);    // the classic Rhodes tremolo on top
chord(57, CHORD_MAJ7, I_EP, 4);                      // DX comping
```

The harmonics knob is **detented**, not continuous — each step is a different
instrument family (integer ratios = epiano/bass/brass; off-integers = bells;
the top detent is the DX tine). Roll *which detent* per song, not a float.
First customers: citypop's lead (the Rhodes bell overtone it always wanted),
lowend's Rhodes A/B. For bells, compare against `INSTR_MALLET` first — the
mallet's bell end is physical, FM's is glassier/DX-flavored.

### Drawn waves — `wave_set` + `INSTR_USER0..3` (the lever nobody pulled)

Beyond the five built-in waves there are four **user wave slots**: fill one with a
single cycle and it plays like any other wave. Zero of the first ten stations used
this — not because it's weak, but because this guide never mentioned it and the
recipes above never needed it. What it buys: **static timbres the basic waves can't
make** — an organ drawbar mix, a nasal clav/reed, a brassy harmonic spread.
Anything whose recipe is "this fixed blend of partials":

```c
// organ — drawbar sum 8'+4'+2⅔'+2' (formula lifted from waveed.c's seed())
float t[64];
for (int i = 0; i < 64; i++) {
    float ph = i / 64.0f;
    t[i] = 0.55f*sinf(ph* 6.2832f) + 0.28f*sinf(ph*12.566f)
         + 0.18f*sinf(ph*18.850f) + 0.12f*sinf(ph*25.133f);
}
wave_set(0, t, 64);                                  // → INSTR_USER0
instrument(I_ORGAN, INSTR_USER0, 12, 80, 6, 90);     // skank / organ-bubble voice
```

To *design* a wave instead of computing one, run the **`waveed` cart**: draw with
the mouse (a live drone morphs as you edit), press **E** to export the float array
as paste-ready C. Its `seed()` function is the current de-facto bank — sine,
rounded square, saw, organ drawbars, vocal/clav, random-walk wobble, triangle —
copy the four-line formula you want into your cart's `init()`. (The planned
endgame is a curated `runtime/waves.h` carts can `#include` instead of copying —
see audio-notes §13 for when that lands and why not sooner.)

Know the limit: a drawn cycle is a **snapshot, not behavior**. It cannot make the
sounds the radio family actually ran out of — epiano pickup growl, FM bell
beating, plucked-string decay (audio-notes §12). Those need engines. Reach for
`wave_set` when the wish is "this harmonic color, please"; reach for an engine
(or wait for one) when the sound has to *move inside a single note*.

### Echo is just more notes (dub.c)

Dub's defining effect needs no FX engine — `schedule_hit` already schedules the
future, so a delay line is a loop:

```c
static void echo_hit(int dly, int midi, int instr, int vol, int dur, int taps) {
    int t = (int)(stepMs * 3);                 // dotted-8th = the dub delay time
    schedule_hit(dly, midi, instr, vol, dur);
    for (int k = 1; k <= taps; k++) {
        int v = vol - 1 - k;                   // each repeat quieter
        if (v <= 0) break;
        schedule_hit(dly + k * t, midi, instr, v, dur);
    }
}
```

Two cautions: each tap occupies a slot in the engine's 64-deep delayed pen, so
keep taps modest (throws of 5–6 are the ceiling); and each sounding tap is a
voice, so echo short notes (60–80ms chops echo beautifully; pads don't).

### The desk is an instrument (dub.c)

A dub track is one riddim plus an engineer riding the faders — i.e. an
**arrangement-level performance layer**. Every 4-bar phrase, re-roll which layers
play (bass ~92%, drums ~90%, skank/organ/melodica lower), keep at least one of
bass/drums holding the floor, and when a layer *leaves*, throw its last hit into a
long echo tail. Crucially the desk is **engine `rnd()`, not the seed**: the same
plate mixes itself differently every listen, which is historically exactly what a
dubplate was. This composes with the density curve — the desk drops layers
*within* what `level_of()` allows — giving three orthogonal dimensions: what the
song is (seed), how full it runs (feel), how it's mixed right now (the desk).

## Style cheat-sheet (the parking lot)

What changes between styles is mostly **data** — the engine above carries over:

| style | tempo | grid feel | harmony bias | kit | status |
|---|---|---|---|---|---|
| **bossa** | 112–140 | straight, clave masks, anticipation | maj7/9, ii-V chains, tritone subs, iv | nylon gtr, surdo bass, shaker, rim, flute | ✅ `bossa.c` |
| **lofi jazz** | 70–85 | swing 56–60%, melody very late | m9/maj9, slower changes (2 bars/chord), pentatonic noodle | rhodes, soft kick/snare, dusty hats, crackle | idea |
| **jangle pop (DeMarco)** | 86–109 | straight + loose (kit +2..10ms vs bass), tempo wobble ±1 at bar lines | mixolydian vamp: 2–4 chords looped, NO bridge — arrangement is the form | chorus-wobble gtr (5.5Hz pitch-LFO pluck), round bass, CR-78-ish kit, legato whistle lead w/ glide | ✅ `jangle.c` |
| **dusk ballad (DeMarco theory)** | 72–92 | fingerpicked 8ths w/ gaps, soft loose kit, gentle 4.6Hz warble | stolen playbook: song templates + bVII (maj/7/9) + bIII (maj7/m6) rolls, chromatic bass descents, verse/chorus, melodic accommodation | picked add9 tri gtr, voice-led sine bass, rim/hat kit, soft sine lead | ✅ `jingle.c` |
| **boom-bap (ATCQ)** | 88–97 | full groove template (hats −8ms, snare +22ms, bass +12ms, swing 57%), NO tempo drift | sampled loop: modal-mixture maj9 pool, 9sus dominant, 2/3/4-bar cut, rotated | voice-led upright bass (the star), layered snare, dusty kick, tremolo rhodes, vinyl dust | ✅ `lowend.c` |
| **city pop (Yamashita)** | 104–118 | session-TIGHT (±2ms only); the anti-lowend | stolen playbook: royal road (IV–V–iii–vi), JTTOU loops, borrowed iv; final chorus +2 gear change | octave-pop disco bass w/ chromatic runs, 16th gtr chucks (lay out at bar turns!), saw brass anticipations, bright kit w/ open hat | ✅ `citypop.c` |
| **dub (King Tubby)** | 66–80 | one-drop/rockers/steppers; echo as scheduled notes (dotted-8th taps) | 1-2 minor chords (i, iv, bVII); the seeded pentatonic RIDDIM bassline is the song | deep sine bass, skank chops, organ bubble, echoed melodica, rim throws, siren; THE DESK rides layers per phrase (performance, not seed) | ✅ `dub.c` |
| **techno-kayō (YMO)** | 112–132 | machine-tight ±2ms, pattern strings (cr78.c format) | Sakamoto templates: chromatic mediant pull, maj7 planing, kayō minor cadence (harmonic-minor V), bright I–bVII–IV–V | Hosono bassline GENERATOR (melodic counterpoint w/ inertia + octave leaps), yonanuki pent lead, sequencer arp, CR-78 drum circuits on loan from `cr78.c` | ✅ `ymo.c` |
| **Satie (gymnopédie)** | 56–70, **3/4** | twelve-step bars; bass on 1, chord on 2, beat 3 empty; rubato in gnossienne mode | chord brain #6, the ALTERNATING PAIR: rock two chords for bars on end, drift the pair per section; gnossienne = phrygian-dominant minor | SOLO PIANO (two tri slots) — density curve shapes touch/ornament, not layers; mordents; melody enters late, stepwise, held over barlines | ✅ `satie.c` |
| **French house (Daft Punk)** | 114–126 | four-on-floor 808, claps on 2+4, machine-straight; THE VOID — last beat before every drop is dead silence | stolen playbook of the records the genre sampled: Modjo/Chic i9–bVImaj7–iv9–bVII9, Stardust's Neapolitan bIImaj7, One More Time's tonic-avoiding IVmaj7–V–iii7, Digital Love's slash bass; dorian iv→IV9 + 9sus rolls, needle-drop rotation — harmony never develops, **the FILTER is the form** | THE RIDE: `note_cutoff`/`note_res` swept on held strings per 8-bar arcs + sidechain pump (cutoff ducks at each kick, blooms across the beat); saw stabs, octave disco bass, Da Funk `note_glide` mono lead, 808 circuits on loan from `tr808.c` | ✅ `house.c` |
| **ambient** | 60 fixed, pace knob | beatless; chords hold 8–16 beats, held `note_on` voices morph via `note_glide` | one mode per song, degree walk, no cadences | 4 detuned saw pads, sine sub, band-noise wind, bell arps | ✅ `ambient.c` |
| **chiptune action** | 140–170 | straight 16ths, driving; euclid() fills | i–bVI–bVII–i loops, power chords | square lead 25% duty, tri bass, noise kit | idea |

Each future style should land as its own cart (lofi radio, ambient radio…) reusing
this guide's blocks; the copied core has now survived **ten** carts essentially
verbatim — extraction is overdue, and the plan is written up below
("`radio.h` — the shared chassis").

## `radio.h` — the shared chassis, and how to lift it out

Planned 2026-06-05. Inventory of what is verbatim (or one-rename-away) across
all ten stations:

- the **xorshift32 composition PRNG** (`rngState/srnd_u/srnd`) + the seed
  derivation in `new_song` (`rnd<<16 ^ rnd ^ frame()*2654435761u`)
- the **session history** (`hist[64]`, `fresh_song`, `[`/`]` walking)
- **nearest-tone voice leading** — house.c's generalized
  `lead_to(c, voices, n, lo, hi, &init)` is the canonical form (n voices,
  register window as parameters)
- the **schedule-ahead step clock** (`scheduled/songBase/stepMs`, the
  `while (scheduled < target)` loop)
- the **input block** (SPACE/R/[ ]/arrows/M/T/H + the mouse ?-button hit test)
- the **chassis draw**: `knob()`, the dial strip, the display window, the
  boxed-chord readout, the help-panel scaffold, power LED, footer hint
- the **density model** (`level_of = sectionBase + intensity - 1`, clamped),
  the **tone knob** (`TONENAME/TONEMUL` + an `apply_voicing` hook), the vu
  decay, `PCNAME[12]`, `iabs`

What changed every time (stays in carts): the chord brain, instruments,
`play_step` content, FORM/sections, the window art, FEEL names, help text.

**Shape: a toolkit, not a framework.** The tempting version — `radio.h` owns
`update()`/`draw()` and carts fill in callbacks — is wrong for this family:
ambient has *no step clock at all*, satie runs *twelve-step 3/4 bars*, and
the guide's own field note says "the clock, the chord brain, and every
instrument changed completely" between stations. So the header exports
**blocks the cart calls**, never the other way around:

```c
// runtime/radio.h — header-only, static fns; carts opt into pieces
typedef struct { /* hist[64], histN, histPos, rngState, seed */ } RadioSeed;
unsigned rad_new_seed(RadioSeed *r);            // derive + log to history
void     rad_replay(RadioSeed *r, unsigned s);  // R / [ / ] re-entry
int      rad_srnd(RadioSeed *r, int n);         // composition PRNG

void rad_lead_to(int root_pc, const int *iv, int *v, int n,
                 int lo, int hi, bool *init);   // the voice-leading block

typedef struct { /* scheduled, songBase, stepMs */ } RadioClock;
long rad_clock_tick(RadioClock *c, double pos); // returns steps to schedule

typedef struct { /* station name, accent color, FEEL[4], help rows,
                    tempo min/max/step, tone-knob on/off, hooks */ } RadioFace;
bool rad_input(RadioFace *f, /* &tempo, &intensity, &toneSel, ... */);
void rad_draw_chassis(const RadioFace *f, /* title, freq, seed, vu, ... */);
// the cart draws ONLY its window art + chord readout inside the callback
```

Where it lives: **`runtime/radio.h`** — already on `-I` for every build path
(editor clang, `make-cart.js`, `play.js`, and the libtcc live host). Header-
only static functions compile into the cart's own TU, so **no
`studio_tcc_symbols.h` regeneration is needed** — zero engine surface grows.

**The seed-compatibility rule (the only dangerous part):** a pinned seed IS
the song, and the composition is just the *sequence of `srnd` calls*. The
extraction must not add, remove, or reorder a single PRNG call in any
migrated cart, or every pinned `*_SEED` and noted seed breaks silently.
Acceptance test per cart, before/after each migration:

```bash
node tools/play.js <name> run --headless --trace a.jsonl --frames 3000 --seed 7
# migrate, rebuild, rerun to b.jsonl — then the chord/sect streams must be
# IDENTICAL: diff <(jq -c .w a.jsonl) <(jq -c .w b.jsonl)
```

**Migration order** (one cart per commit — parallel-agent hygiene):

1. `house.c` as pilot (newest, already has the generalized `lead_to`)
2. the synth trio `ymo.c` / `dub.c` (same skeleton, near-zero adaptation)
3. `citypop.c`, `lowend.c`, `jingle.c`, `jangle.c`, `bossa.c`
4. `ambient.c` and `satie.c` **last** — they deviate most (beatless; 3/4)
   and prove the toolkit claim: they should be able to take the PRNG,
   history, voice leading and chassis while *skipping* the clock entirely.

**Sequencing note:** do the extraction **before** the per-song timbre-roll
refit (section below) — then the roll helpers (`rad_roll_filter`, register
shift, wave pools) land once in `radio.h` and the whole family inherits the
fix instead of ten hand-copies. The estimated arithmetic: core ≈ 300–350
header lines; each cart sheds ≈ 150–220; and stations eleven+ (gamelan,
Italo, the Doors) start from a scaffold instead of a copy of dub.c.

## The same band every night — per-song timbre rolls (family-wide refit)

Station-owner feedback (2026-06-05, while listening to house radio): *"it's the
same identical instrument for every song — a bit more variation is in place."*
Correct, and it's structural: every cart calls `setup_instruments()` once with
hardcoded constants, so the seed re-rolls the **composition** (key, changes,
patterns) but the **band** never changes — same cutoffs, same waves, same
envelopes, song after song. A real station plays different *records*; ours
replays one session band forever.

The fix is cheap because instrument parameters are already just numbers: in
`new_song()`, after the chord/pattern rolls, **roll the timbre from the same
composition seed**, within style bounds:

- **register** — bass octave choice, comp/stab voicing window shifted ±3–5 st
- **filter character** — per-instrument base cutoff ×0.7..1.4, resonance ±2
- **envelope feel** — attack/decay/release ×0.6..1.6 (a plucky take vs a soft
  night take of the same instrument)
- **wave swaps from small pools** — lead: square(0.25) / square(0.38) / saw;
  bass: sine / tri; stab: saw / tri. Two or three options, not a synth menu.
- **LFO personality** — vibrato rate/depth ranges, tremolo present or absent
- **kit dressing** — hat brightness, clap vs snare vs rim, shaker on/off

Implementation gotchas, learned from the existing carts:

1. **Store the rolled bases, scale at apply time.** Carts with a tone knob
   (`apply_voicing()` in dub/ymo/house) re-issue `instrument_filter` with
   *constants* — a timbre roll must instead write per-song base values that
   the tone knob multiplies, or pressing T erases the song's personality.
2. **Roll from the composition PRNG (`srnd`), not engine `rnd()`** — a pinned
   seed should always bring its own band; that's part of what the number IS.
3. **Stay inside the style.** Bossa's nylon guitar must never become a square
   wave; 2–3 audibly-rolled knobs per instrument is plenty. The acceptance
   test: two SPACE presses should sound like **two different records**, not
   two takes of one.

New stations should build this in from day one; existing carts retrofit
opportunistically (house and ymo first — newest, and the synth genres wear
timbre variation most naturally).

## Future stations — the parking lot

Candidates ranked by **engine fit** (= the genre's essence maps onto machinery we
have, rather than needing approximation). Context for whoever builds next: the
station owner's taste profile, demonstrated across seven picks, is *warm,
harmonically lush, groove-forward, analog-nostalgic, jazz-adjacent — background
music that rewards attention; crate-digger sensibility*. They name-checked Hosono
and Sakamoto unprompted.

**The strongest candidates:**

1. ~~**Dub reggae**~~ — ✅ shipped as `dub.c`, and the engine-fit prediction held:
   echo via `schedule_hit` and the desk-as-instrument both worked first try.
   Kept here as the reference case for what "best engine fit" means.
2. ~~**YMO / techno-kayō synthpop**~~ — ✅ shipped as `ymo.c`, with the drum
   circuits on loan from `cr78.c` exactly as hoped. The native-timbre prediction
   held too; the Hosono bassline generator (counterpoint with inertia) is the
   reusable new block.
3. **Ethio-jazz (Mulatu Astatke)** — the deepest crate. The whole genre is
   **scales as data**: tizita, bati, anchihoye — pentatonic modes that sound like
   nothing else on the dial, over hypnotic minor vamps with horns and
   vibraphone-ish keys. A new chord-brain variant (modal vamp in a non-Western
   scale) and the accommodation rule doing something genuinely new.
4. **Swing / cocktail jazz trio** — walking bass as an algorithm (chord tone on
   1, approach pattern through the bar — a beautiful ~20-line generator),
   ride-cymbal swing pattern, sparse comping reusing bossa's ii–V tables at 4×
   the harmonic rate. The guide's swing math finally gets its full workout.
5. **Gamelan (Java/Bali)** — planned 2026-06-05; the first station to leave
   12-TET entirely. Four new tricks in one cart:
   - **the tuning system as data** — sléndro/pélog as cent tables fed through
     `note_pitch` floats; and since *no two real gamelans are tuned alike*
     (every village ensemble is its own instrument), **the seed rolls the
     tuning** — each song is played on slightly different bronze. Also the
     natural home of the per-song timbre-roll idea (section above).
   - **colotomic time — a new TIME brain** — form as *nested gong cycles*
     (gong ageng marks the full gongan, kempul halves, kenong quarters,
     kethuk between), not bar→phrase→section. The arrangement curve becomes
     concentric circles.
   - **kotekan — melody as interlock** — two voices (polos/sangsih) on
     *complementary* onset masks summing to an unbroken 16th stream.
     Trace-verifiable by definition: assert the union fills the grid.
   - **ombak** — paired voices a few cents apart so they BEAT; ambient's
     detune promoted to the genre's whole timbral identity. Plus **irama**
     as the feel knob: density doubles as tempo halves.
   Open questions: tuning per song vs per session; Bali (fast, bright,
   kotekan-forward) vs Java (slow, deep) — possibly the two ends of the feel
   knob; face = gong rack vs a wayang shadow puppet dancing the cycle.
   Taste-fit: the Hosono thread again (*Cochin Moon*), Nonesuch Explorer
   crate-digging.
6. **The Doors / modal psych-rock** — owner-requested 2026-06-05, with the
   theory homework already done. The vamp side we mostly own (mixolydian
   one-/two-chord drones, "L.A. Woman" A–G over an A center that never
   resolves — jangle's vamp brain, slowed and darkened; modal-mixture
   excursions for the bridges, "Crystal Ship" color). What earns the slot is
   **THE IMPROVISER — melody brain #2**: every station so far repeats a cell
   or a fixed riff; nobody can take a SOLO. Manzarek demands a phrase-based
   improvisation generator — question/answer phrases with breath rests, a
   3–4 note motif *developed* (sequenced up/down the mode, rhythm varied),
   and a tension arc across 16–32 bars (register climbs, density rises,
   peak, release). Built once, it's reusable for ethio-jazz (#3) and the
   cocktail trio (#4). Solos are pure PERFORMANCE (engine rnd) — the band
   never played it the same twice, and neither should the radio.
   The rest of the kit maps clean:
   - **major/minor blues duality** — the soloist draws from minor pentatonic
     OVER the major vamp (a seeded "how blue" knob biasing b3/b7 vs major
     tones) — deliberate clash, the opposite of jingle's accommodation rule.
   - **Rhodes piano-bass ostinato** — a seeded 1-bar left-hand riff (dub's
     riddim, swung); combo-organ held chords above it: two keyboards, one
     player. Kit rolls shuffle / latin (Densmore's "Break on Through" is
     nearly a bossa) / straight rock.
   - **Krieger drone guitar** — open-string pedal tone held UNDER a moving
     modal line (two-voice guitar), flamenco/phrygian color for "The End"-
     style excursions — connects to the gamelan/drone interest.
   - **form** — the Doors essence is head → LONG instrumental middle (organ
     solo → guitar solo, each with its own arc) → head out. The arrangement
     curve finally gets to model a JAM.

**Also very doable:**

- **Motorik / Stereolab** — one-chord pulse, strict-8ths motorik kit, Farfisa-ish
  drones on held voices, maj7 planing on top. The experimental axis.
- **Chicha / Peruvian cumbia** — wobbly surf guitar (jangle's warble at higher
  depth), minor vamps, güiro 16ths. Crate-digger royalty.
- **Afrobeat** — 2–3 interlocking single-note guitar ostinatos (`euclid()` was
  made for this), one chord for eleven minutes, horn riffs as call-and-response.
- ~~**French house / disco edits**~~ — ✅ shipped as `house.c`, and the
  prediction held to the letter: the filter ride IS the song (`note_cutoff`/
  `note_res` on held strings from an arrangement-level curve). Bonus finds: the
  sidechain pump as a *filter* gesture (cutoff ducks at each kick), and THE
  VOID — one `return` statement of silence before every drop.
- **Exotica (Martin Denny / Les Baxter)** — Hosono's actual origin story (the
  Tropical Trilogy is exotica homage). Lush vibraphone maj9/m9, lazy rubato
  lounge time, and the fun new layer: **bird calls and frog croaks as an
  aleatoric performance channel** — `chance()` per bar firing pitch-swooped
  squeals that never repeat (pure engine-`rnd`, never seeded, exactly like
  Denny's band improvising the jungle). Fewer new engine tricks than gamelan —
  mostly timbre — but maximum taste-fit.
- **Wendy Carlos / baroque counterpoint** — chord brain #7 that isn't chords
  at all: **rule-based two-voice species counterpoint** over a ground bass
  (a chaconne loop = the vamp brain's classical ancestor). Contrary motion
  preferred, no parallel fifths, dissonance only passing on weak beats —
  ~40 lines of rules generating real polyphony, played on bright Moog timbres
  (square + saw, generous portamento). *Switched-On Bach* as a station.
- **Steve Reich minimalism** — two voices, same pattern, slightly different step
  lengths → phasing. The most experimental station possible, nearly free to build.
- **Lofi hip-hop jazz** — still in the cheat-sheet above; most of its parts
  (swing, rhodes, crackle, groove template) now exist across other carts.

Build order advice: pick by which *new engine trick* the station would prove,
not just by genre appeal — that's what kept the first seven carts each earning
their place (held voices, groove template, playbook encoding, loop rotation,
scheduled echo, the desk…).

### Why citypop lands — the four conditions (the second build-order axis)

Owner observation (2026-06-05): citypop is the station that works best. The
diagnosis generalizes — it scores 4/4 on conditions the others meet only
partly, and candidate genres can be scored against them:

1. **A named formula to steal.** The royal road (王道進行) and JTTOU changes
   are published, named progressions — encoding two templates captures the
   genre's identity. (Genres that are "a vibe" — ambient, jangle — can't be
   captured this way; they work differently, by texture.)
2. **Directional gestures per layer, not static masks.** The bass runs INTO
   each change, the brass anticipates the and-of-4 BEFORE the bar, the chucks
   lay out AT bar turns, the gear change lifts the LAST chorus — every layer
   points at what's coming. Forward pull beats pattern density.
3. **A time-feel that wants the engine's native precision.** Session-tight or
   machine-tight genres (citypop, ymo, house) get authenticity for free;
   loose genres need artificial sloppiness that's hard to dose (lowend
   shipped at half groove strength).
4. **Gloss-native timbres.** Our oscillators do neon (saws, bright squares,
   clean tris) natively; they can't do smoky (breath, real horns).

Genres scoring 4/4, in taste order:

- **Yacht rock / Steely Dan & AOR** — the literal source citypop imported
  ("Plastic Love" is Tokyo's answer to Aja). Even MORE codified: the mu
  chord (μ — add9 voiced 1-2-3-5) is a *named Steely Dan invention*, the
  ii–V-over-groove language exhaustively analyzed, and they were the most
  session-perfectionist act ever recorded. Rhodes + clean gtr + tight kit,
  all doable. The deep pick.
- **Italo disco** — citypop's machine cousin: sequenced octave-arpeggio
  bass, dramatic minor formulas (i–bVI–bVII–V), bright stabs, drum-machine
  time, and the truck driver's gear change lives here too. Reuses house.c's
  808 + pump. The easy-win pick. (Its descendant **eurobeat** is the same
  recipe at 155 bpm if maximum formula is ever wanted.)
- **J-fusion (Casiopea / T-Square)** — citypop's instrumental twin, same
  Tokyo session scene — but it needs real solos, so it should wait for the
  Doors station's improviser (melody brain #2).

### Batch two: the IDM / electronic wing

The first nine stations were about **harmony brains**; this batch is about
**rhythm brains and timbre brains** — volatility, ratchets, mutation,
mistuning. The other half of the engine's expressiveness, mostly unexplored.

**Aphex Twin, decomposed** (the anchor of the batch):

1. **No repeating patterns = drums as composition.** The kit is a GRAMMAR, not
   a pattern: anchors persist (kick near 1, snare backbeat-ish), everything
   else re-rolls per bar — fills are the norm. One knob: *volatility*
   (0 = loop, 1 = never the same bar twice). The first brain for rhythm
   rather than harmony.
2. **Ratchets** — replace a hit with 3–8 sub-hits at 1/32–1/64 with vol/pitch
   ramps. Only viable since the sample-exact fix (cf68813): ~23ms spacing is
   exactly what block-quantization used to destroy. ~10 lines, like echo_hit.
3. **Soft vs hard = TWO independent density curves**, deliberately crossed
   ("Flim": angelic music-box at volatility 0 over drums at volatility 1).
   Extends the arrangement-curve model to two dimensions.
4. **Canon via echo_hit** — dub's echo with a ONE-BAR delay at +12, quieter,
   is a two-voice canon generator ("Avril 14th"). Structural reuse for free.
5. **Microtonal drift** (`note_pitch` floats — detuned wrong on purpose), odd
   meters (satie.c proved non-16 bars; 7/8 = 14 steps), and acid lines —
   `note_cutoff`/`note_res` squelch. house.c now rides those two calls as a
   slow arrangement gesture; the fast per-step acid squelch is still unclaimed.

**The rest of the wing, by engine fit:**

- **Boards of Canada** — likely the best taste-fit: systematic MISTUNING as
  warmth (inconsistent per-voice detune + slow pitch LFOs — tape memory),
  lydian/major nostalgic 2-chord vamps, slow hip-hop kits, heavy rolloff.
- **Burial** — 2-step garage swing (new groove template: shuffled kicks,
  off-grid woodblock snares), vinyl + rain, "vocal" sighs = filtered saw with
  heavy note_glide on wordless pentatonics. Sits next to lowend and ambient.
- **Squarepusher** — the ratchet engine at maximum + the Hosono bassline
  generator (ymo.c) turned virtuoso slap. Two existing blocks, pushed to 11.
- **Autechre (Tri Repetae era)** — pattern mutation applied to TIMBRE:
  percussion params (pitch, cutoff, decay) drift per hit; the drum machine as
  a slowly-decaying organism. Needs the rotating-slot trick (audio-notes §2.2).
- **Plaid / B12** — melodic IDM, arps in odd meters, bells. Gentlest entry.
- **Eno "Music for Airports"** — melody cells of different PRIME lengths
  (7/11/13 bars) phasing against each other. Nearly free; ties to the Reich
  idea above.

### Batch three: the functional-music wing

Music engineered for a PURPOSE — plants, shops, television programs that
didn't exist yet. The meta-joke is the point: these radios already ARE library
music — generative tracks with mood metadata waiting for a game to license
them. This wing makes that explicit.

- **Plantasia (Mort Garson, 1976)** — "warm earth music for plants." Early
  Moog, and the first MELODY-FORWARD station: every cart so far buries the
  lead in the arrangement; here the mono Moog lead is the protagonist — a held
  voice with note_glide portamento + vibrato (jangle's lead, promoted),
  bouncy staccato bass, celesta bells, innocent major harmony with vaudeville
  chromatic passing chords. Face gimmick: a generative houseplant that GROWS
  as the piece plays — fresh plant per seed, fully grown by the outro.
- **Muzak / sounds of the supermarket** — the Muzak Corporation's "Stimulus
  Progression" (1948): 15-minute blocks of ASCENDING INTENSITY engineered to
  pace workers — literally this guide's density curve, invented as a corporate
  product 75 years early (the cart should cite it). Vibraphone-led easy
  listening, muted brass, everything consonant and dynamically flattened, plus
  the foley layer: PA bing-bong chime, register beeps as percussion. Optional
  modern lens: mallsoft (drown it in echo_hit, pitch it down). Face:
  fluorescent aisle, PA speaker, the seed on a price display.
- **KPM library music (the green sleeves)** — Mansfield/Hawkshaw/Dale/Tew:
  big-band breakbeat funk made for unknown television, the most-sampled
  catalogue in hip hop. Two engine ideas, one big:
  - **the break** — an arrangement section that is deliberately DRUMS-ONLY as
    a feature (the bars that made these records sampleable; connects to
    lowend.c).
  - **MOOD METADATA AS THE FRONT DOOR** — KPM sold tracks by descriptor
    ("bright, confident movement", "tension: industrial"). The cart picks the
    mood FIRST and derives everything (tempo, key, template, kit) from a mood
    table, displayed on a generated green sleeve with a seeded catalogue
    number. This prototypes the API every game actually wants from generative
    music — "give me tension", "give me victory lap" — and could retrofit the
    whole radio family as a mood-keyed library.

## Verifying without ears

The debug harness makes the music inspectable (see `debug-harness.md`):
`watch()` the current bar/chord/step under `#ifdef DE_TRACE`, then

```bash
node tools/play.js bossa run --headless --trace t.jsonl --frames 3600
```

and read the chord stream out of the trace — progression, form, and loop-around
are all checkable from JSONL. Determinism of a pinned seed is two runs + a diff.
