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

> **Picking a station's instruments?** [`instrument-presets.md`](instrument-presets.md) +
> [`radio-voices.md`](radio-voices.md) catalog every existing station's voices and show which
> sound recipes are already shared across stations — start from the closest cousin's chart
> rather than from scratch. (And update both when you ship a new station.)

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
practice — half strength (−5/+12/+7, swing 54%) is where the head-nod lives.
lowend now **rolls the pocket per song** (tight / swung / drunk) rather than
shipping one fixed feel — the full Dilla drag earns its place as one option among
three. Promoting a fixed `PUSH_*` template to a per-song roll is the *pocket*
axis; see "Same song every night?" below. Start subtle; turn it up only if the
groove vanishes.
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
live A/B against their TRI fakes (**G** key); the pattern since generalized into
**THE BAND panel** (**B** — `radio.h`'s `rad_chair`/`rad_band_input`/
`rad_band_panel`, every chair's candidates cycling live mid-song; cocktail,
tango, yacht, roadhouse, exotica are chaired). Per-station upgrade notes live in
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
engine (instrument-engines §8.1.1); the `pluck` cart is the hands-on tour (grab a string
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

First customers ([`radio-instrument-options.md`](../design/radio-instrument-options.md)): lowend's lead → vibraphone (the
Nujabes sound, the single highest-value engine swap), ambient's bell, ymo's
marimba counterpoint. The roll trick: a mallet *tremolo roll* is just 5–6
`schedule_hit`s ~70ms apart on one bar, alternating vol 3/4 — see the mallet
cart's autoplay.

### The DX keys — `INSTR_FM` (engine #3)

Two-operator FM (shipped 2026-06-05; taste settling in the `fm` cart). Unlike
the string and the bar it does **not** decay on its own — give it a normal
ADSR; what it does do on its own is **mellow within each note** (the FM amount
decays like a real DX strike), so comped chords sparkle on the attack and sit
back in the mix as they ring. The electric-piano gap for the stations, for now:

> A dedicated `INSTR_EPIANO` engine (Rhodes/Wurli/Clav) shipped 2026-06-08, but
> it is **parked for radio use** — its voice leans on a provisional per-voice wah
> and the real plan is a wah-on-a-bus effect (instrument-engines.md §8.10). Until
> that lands, **FM is the station epiano** (and don't adopt the `epiano.c` wah
> recipe below into a station). See radio-instrument-options.md intro.

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

> Note: for **organ** specifically there's now a dedicated `INSTR_ORGAN` engine
> (shipped 2026-06-07) with live registration/animation macros — reach for it when
> you want the drawbars to *move* (scanner chorus, key click). The `wave_set`
> drawbar below is still the right call for a fixed, seed-rolled footage mix
> (roadhouse/tango ship it that way).

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
see [audio-notes](../design/audio-notes.md) §13 for when that lands and why not sooner.)

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

### Wah is just the filter swept (epiano.c)

> ⚠ **PROVISIONAL — do not adopt into a radio station yet.** This per-voice wah is
> a bit poopy in practice; the real plan is **a proper wah as a bus effect**
> (instrument-engines.md §8.10). The recipe below documents what `epiano.c`
> currently does, but stations should NOT wire it in (and INSTR_EPIANO, which
> relies on it, is likewise parked — see the FM section above). Revisit once the
> bus effect lands.

Wah needs no FX engine — it's the per-voice SVF with a moving centre
([decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md): "the
4th use of the one filter"). Two things make it sound like a *wah* and not a tone
knob (learned by A/B-ing against navkit's wah):

1. **Use `FILTER_BAND`, not `FILTER_LOW`.** A bandpass moves a *resonant peak*
   through the spectrum (rejecting both lows and highs around it) — that moving
   peak IS the vowel. A lowpass just opens the top; much weaker.
2. **High resonance (10–12)** — the peak is the vowel; a gentle one reads as a tone
   control.

Then *what sweeps the centre* is which wah you get — one per modulation source.
All four are in `epiano.c`'s wah toggle (off/auto/env/touch):

```c
instrument_filter(slot, FILTER_BAND, 1300, 11);         // resonant filter — the vowel peak

// AUTO-WAH — an LFO sweeps the centre (rhythmic "wah-wah"; works on anything, even staccato).
// Keep the sweep in a MUSICAL band (~400-2200) — sweeping to the ~20Hz clamp makes a bandpass
// pass nothing, so it pulses to mud.
instrument_lfo(slot, 0, LFO_CUTOFF, 4.0f, 700.0f);      // rate Hz, depth Hz

// ENVELOPE-WAH — the FUNKY CLAV "quack": a FAST per-note cutoff snap on a resonant filter.
// The trick is SPEED — ~100ms decay so the brightness snaps shut while the body still rings
// (the cutoff LEADS the amplitude down). A slow env just tracks the decay and you hear nothing.
instrument_filter(slot, FILTER_LOW, 500, 9);
instrument_env(slot, 0, ENV_CUTOFF, 2, 110, 2400.0f);   // attack 2ms, decay 110ms, amount Hz

// TOUCH-WAH — the envelope FOLLOWER: opens from the note's OWN amplitude, "hangs" on a slow
// release. Dynamic, responds to how hard you play — great on sustained bass/leads. NOT a clav
// (it holds the filter OPEN; the clav wants the fast snap above).
instrument_follow(slot, LFO_CUTOFF, 3, 200, 1800.0f);   // attack ms, release ms, depth Hz

// PEDAL-WAH — you are the LFO: sweep it live on a held note
note_filter(h, FILTER_BAND); note_res(h, 10); note_cutoff(h, foot_hz);
```

**The funky-clav lesson (confirmed by rendering navkit, 2026-06-08):** a real
Clavinet's "wah" is *not* a wah pedal or an envelope follower — it's a resonant
voice filter with a **fast per-note envelope** that snaps the cutoff shut in ~100ms.
The filter topology (Chamberlin vs TPT SVF) turned out not to matter; the sweep
*speed/range/source* is everything. The method: `tools/navkit-render.c` renders
navkit's actual preset to a WAV, `tools/wav-envelope.js` plots amplitude+brightness
— a real wah shows brightness dropping *faster* than amplitude. Full effects-as-recipes
audit: [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md).

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
| **exotica (Denny/Baxter)** | 84–100, lazy two-feel | rim clave + conga heartbeat + shaker; vibes 15–25ms behind the beat; tremolo ROLLS on long notes | lounge loops: maj9/6-9 lush, borrowed ivm6 dusk, pagan minor (i–bVI / i–bIII–bVI); melody = re-pitched cell | vibes lead (**MALLET**, motor on), nylon comp (**PLUCK**), glass bell arps (**FM**) — all three engines; upright TRI bass; THE AVIARY: unseeded bird/frog calls per bar (the band improvising the jungle) | ✅ `exotica.c` |
| **yacht rock (Steely Dan)** | 92–114, session-tight OR Purdie half-time shuffle (swung ghosts vol 1) OR CR-78 machine — rolled per song | citypop anticipations (and-of-4 epiano push), chromatic bass runs INTO changes, +2 gear change in the last chorus | THE MU CHORD (add9 voiced 3-5-9, a named quality) in stolen-playbook loops: two-mu vamp, ii–V13, maj7 planing, dorian i–IV13; mu-ify + sus-melt rolls | **FM epiano** comp (1:1 detent — the tine pings every chord) + tremolo, PLUCK strat 9th-stabs, breathy narrow-pulse sax, soft saw strings, kit circuits swapped per groove (cr78.c on loan) | ✅ `yacht.c` |
| **modal psych-rock (the Doors)** | 96–122; kit rolls shuffle (swung + ghosts) / latin / rock; ride takes over in the solos | jam form: head → ORGAN SOLO → GUITAR SOLO → head out; **THE IMPROVISER** (melody brain #2): motif → stated/answered/sequenced/doubled per 2-bar phrase, 16-bar tension arc, breath rests — pure performance rnd, never seeded | two-chord modal vamps rocked 2 bars each: mixo I–bVII (B–A forever), dorian i–IV7, phrygian i–bII; seeded "how blue" knob = b3/b7/b5 intrusions | Vox combo organ = **wave_set drawbar cycle** (the lever finally pulled), Rhodes piano bass = **FM** epiano detent low + swung seeded ostinato, Krieger gtr = **PLUCK** line over an open-string drone | ✅ `roadhouse.c` |
| **cocktail piano trio (Guaraldi/Peterson)** | 96–108 ballads / 116–144 swing; ride "ding ding-ga-ding" + hat on 2&4, swing 62%, feathered kick, brush sweeps | trio set form: head → piano solo (16-bar arc) → **BASS SOLO** (the improviser, low register, 0.62 density — the room leans in) → piano trades back → head out | weighted ii–V–I functional walk with secondary dominants + the tritone sub; cadence FORCED into bars 6–7 of every 8; borrowed ivm6 | **THE WALKING BASS** (the promised ~20-line generator: root → motion → chromatic approach into the next root), satie's TRI piano + felt-attack env, Charleston comp cells; `improv.h` shared with roadhouse | ✅ `cocktail.c` |
| **tango (Golden Age)** | 116–136, canción 100–112 | **TEMPO AS A VOICE: the conductor rides `bpm()` live** (phrase breath, section-end rit + a-tempo snap, the final rallentando — depth seeded); marcato-in-four / síncopa 3-3-2 rolled per phrase; THE ARRASTRE (chromatic bass drag into downbeats); no kit — chicharra/golpe from the band | harmonic-minor functional walk (iiø→V7b9 forced cadence, Neapolitan bII, Andalusian-tetrachord roll) + parallel-major cantabile B; THE CHAN-CHAN — every song actually ends | bandoneón = **wave_set reed cycle** (drawn wave #2, bellows-tremble LFO), saw violin w/ vibrato + ENV_PITCH scoop, felt TRI piano, upright TRI bass | ✅ `tango.c` |
| **ambient** | 60 fixed, pace knob | beatless; chords hold 8–16 beats, held `note_on` voices morph via `note_glide` | one mode per song, degree walk, no cadences | 4 detuned saw pads, sine sub, band-noise wind, bell arps | ✅ `ambient.c` |
| **chiptune action** | 140–170 | straight 16ths, driving; euclid() fills | i–bVI–bVII–i loops, power chords | square lead 25% duty, tri bass, noise kit | idea |

Each future style should land as its own cart (lofi radio, ambient radio…) reusing
this guide's blocks; the copied core has now survived **ten** carts essentially
verbatim — extraction is overdue, and the plan is written up below
("`radio.h` — the shared chassis").

## The brain catalog — every reusable part, and how to combine them

> Added 2026-06-05, after fourteen stations made the pattern obvious: a station
> is not a monolith — it's a PICK of interchangeable brains (a chord brain + a
> time feel + a bass brain + a melody brain + a form + a band). New carts should
> shop this catalog first, and **novel combinations are first-class builds**:
> "the walking bass under the improviser playing FM brass" is a legitimate
> station nobody has made yet. The graduation rule governs reuse: a block lives
> in its home cart until a SECOND customer wants it, then it moves to a shared
> header (`radio.h` and `improv.h` are the two graduates so far).

**Chord brains** (what the harmony does):

| # | brain | home | character |
|---|---|---|---|
| 1 | functional walk + forced cadences | `bossa.c`, `cocktail.c` (jazz rate, secondary dominants, tritone sub) | pull — always comes home |
| 2 | modal drift | `ambient.c` | float — no cadences, light shifting |
| 3 | the vamp | `jangle.c`, `roadhouse.c` (2-chord modal) | sit — arrangement is the form |
| 4 | stolen playbook | `jingle.c`, `citypop.c`, `house.c`, `yacht.c`, `air.c`, `polopan.c` (the artist stations encode cited tracks as per-archetype templates; + the mu chord as a named QUALITY) | sound like *someone* |
| 5 | sampled loop, cut + rotated | `lowend.c` | nod — loop-born harmony |
| 6 | alternating pair | `satie.c` | rock two chords for bars on end |
| 7 | **no chords at all — two-voice species counterpoint** | `carlos.c` | the harmony is whatever two rule-generated lines SPELL: consonance on strong beats, passing dissonance on weak, contrary motion preferred, no parallel perfect 5ths/8ves, over a seeded chaconne ground — and the feel knob slides strict canon → free counterpoint |
| — | lounge templates + flavor rolls | `exotica.c` (borrowed-ivm6 dusk) | 4-brain's gentler cousin |

**Time brains / feels**: schedule-ahead clock (`radio.h` `RadioClock`) · groove templates (`lowend.c` push/drag lanes — now **rolled per song** as the pocket axis: tight/swung/drunk) · swing 55–62% (`cocktail.c` has the full ride pattern) · machine-tight ±2ms (`citypop.c`/`ymo.c`/`house.c`) · lazy two-feel (`exotica.c`) · Purdie half-time shuffle + ghosts (`yacht.c`) · **TEMPO AS A VOICE — the conductor (`tango.c`)**: phrase-level rubato via live `bpm()`, eased asymmetrically (rising fast = the a-tempo snap, falling slow = the band sags together), depth seeded per song; the clock survived untouched, exactly as promised · **THE DRUNK POCKET — DIALABLE (`lofi.c`)**: per-voice `schedule_hit` dly offsets (snare-LATE / lazy-kick / swung-hat) + a seeded humanize wobble, all scaled by a **pocket knob** (tight → drunk), default moderate — the off-grid Dilla feel `lowend` undersold, now done right *and* under the player's control (keep it adjustable; over-done loose timing gets seasick). · *future:* colotomic cycles (gamelan), the process form (motorik), volatility grammar (Aphex).

**Melody brains**: #1 the re-pitched cell (everywhere since bossa) · **#2 THE IMPROVISER (`runtime/improv.h`)** — phrase-based solos, any mode, any register, any density; its inputs are deliberately generic: hand it a scale, a register, a length, a density and ANY instrument slot becomes a soloist · **#3 THE SONGWRITER (`plantasia.c`)** — a *seeded* theme-and-variation generator: a singable hook invented from `rad_srnd` (so it's the song's IDENTITY, the same every replay — the opposite of #2, which is pure-performance and re-rolls), developed across an A-A'-B-A songform (state / restate-up-an-octave / contrasting bridge / recap + a final key-LIFT). It makes the LEAD the protagonist (the first melody-forward station); the held mono voice glides between its notes (`note_pitch`+`note_glide`, driven per-frame — `schedule_hit` can't portamento). *Graduation candidate: second customer (Reich, J-fusion) moves it to a header.* · **#4 INTERLOCKING ARPS (`plaid.c`)** — 3–4 short cyclic cells, each at its OWN length (5/7/8/9/11 steps) running against the bar, so they phase and the tune EMERGES from the overlap; all in one mode → every overlap is consonant. The melodic, pitched cousin of gamelan's kotekan + eno's coprime loops (a *polymeter* of melodies). · **#5 THE CALL-AND-RESPONSE (`thexx.c`)** — two voices (a low male + a higher female `INSTR_VOICE`) that TRADE a sung cell by phrase (A states, B answers) and meet in unison on the chorus; *who-sings-when, and the silence between turns, is the structure* — a duet-conversation brain, reusable by any call-and-response act, and it pairs with SPACE-AS-STRUCTURE (deliberately low density, reverb + rests carrying the weight). · *future:* kotekan interlock (gamelan).

**Bass brains**: voice-led roots (everywhere) · octave disco pop (`house.c`) · Hosono counterpoint generator (`ymo.c`) · seeded riddim/ostinato (`dub.c`, `roadhouse.c` piano bass) · **THE WALKING BASS (`cocktail.c` `walk_note()`)** — root → chord/scale motion → chromatic approach; *graduation candidate: second customer moves it to a header.* · **THE BASS AS LEAD (`squarepusher.c`)** — the bass is the protagonist, not a sub: the Hosono walker cranked to virtuoso density (chromatic approaches, octave pops, a SLAP transient on accents) for the groove, and `improv.h` put in the *bass* chair for the solos — over moving jazz-fusion changes.

**Form brains**: 8×8 sectional + density curve (`rad_level`, everywhere) · verse/chorus + gear change (`citypop.c`, `yacht.c`) · the jam — head/solos/head (`roadhouse.c`) · the trio set + bass solo (`cocktail.c`) · THE DESK — per-phrase performance mixing (`dub.c`) · the filter ride as form (`house.c`) · **the rolled form set — variable-length arrangements, not one fixed `FORM[8]` (`addis.c` sketch/set/jam, `lowend.c` tape/loop/cut), incl. the drums-only BREAK as a form feature (`lowend.c`)** · **the artist-archetype bundle (`air.c`) — a stolen-playbook cited-song chart bundled WITH its own groove, tempo, mood and LEAD-VOICE (the lead engine itself swaps per song), rolled as one ARCHETYPE: the dial plays recognizably different songs *of one artist*, not one band re-running charts. The escalation over jingle/citypop (which roll only the chart, keeping one band): the archetype reconfigures the band**. The five structural de-samifying axes (groove · form · pocket · tempo · instrumentation/lead-voice) and how to roll them: see "Same song every night?" above.

**Performance channels** (never seeded): the improviser's solos · THE AVIARY — aleatoric calls (`exotica.c`) · the desk (`dub.c`) · humanize jitter (everywhere).

**The band** (timbre is a pick too): engine voices — PLUCK strings/guitars, MALLET vibes/bells, FM epiano/bells/bass/brass (`instrument()` ADSR included: brass = slow attack!) · drawn waves (`wave_set` — `roadhouse.c`'s Vox drawbars and `tango.c`'s bandoneón reed are the worked examples) · borrowed machine circuits (`tr808.c` → house, `cr78.c` → yacht — copy the `instrument()` lines verbatim) · the per-song timbre roll (seeded band variation, `exotica.c`/`yacht.c` from day one) · **detuned pairs** (`instrument_tune` — two slots, one tuned +0.06, instant unison shimmer; same call = gamelan ombak, or a live tuning trimmer like sh101's TUNE. Reaches scheduled hits, the one pitch control that does).

**A worked combination** (the kind of build this catalog exists for): *a hard-bop brass combo* = cocktail's walking bass + swing ride, `improv.h` soloing on an `INSTR_FM` brass patch (1:1 detent, timbre 0.9, **70ms attack** — the [instrument-engines](../design/instrument-engines.md) §8.8.3 swell makes the horn speak), chord brain #1 at jazz rate, the trio-set form with trading fours. Every part exists today; the cart is ~150 lines of glue plus a face.

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
- **THE BAND panel** (`rad_chair`/`rad_band_input`/`rad_band_panel`): a chair
  registry + B-key overlay cycling each chair's instrument candidates live —
  the cart applies each swap in its own `apply_chair()`; the toolkit never
  calls `rad_srnd`, so pinned seeds survive auditions
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

Acceptance test per cart, before/after each migration — **refined by the
house.c pilot (2026-06-05), which caught two flaws in the naive recipe**:
(a) a free-roaming cart derives its seed from `frame()` at boot, which
wall-clock startup jitter makes unreproducible between runs — **pin the
cart's `*_SEED` to a fixed value for the test**; (b) even pinned, scheduling
lands ±2–6 frames apart between runs (the beat clock starts at a slightly
different wall moment), so a per-frame diff false-fails — **compare the
TRANSITION SEQUENCE of (song, sect, chord), not frames**. Control: the same
binary run twice must show the same boundary skew (it does — 38 frame
mismatches, 0 transition mismatches).

```bash
# 1. set <NAME>_SEED to a fixed value (e.g. 0xBEE71234) in the cart
node tools/play.js <name> run --headless --trace a.jsonl --frames 3000 --seed 7
# 2. migrate, rebuild, rerun to b.jsonl
# 3. dedup consecutive (song,sect,chord) tuples from each .w stream —
#    the two transition sequences must be IDENTICAL (python, not jq+diff)
# 4. restore <NAME>_SEED 0
```

**Status (2026-06-05): `runtime/radio.h` EXTRACTED; `house.c` migrated as the
pilot — transition sequences identical, 788 → ~640 lines.** The chosen path:
new stations build on the scaffold immediately; the other nine carts migrate
opportunistically (e.g. in the same visit as their engine retrofit), since an
unmigrated cart keeps working untouched. The migration recipe that worked:
`#define srnd(n) rad_srnd(&rs, (n))` keeps every PRNG call site textually
identical (zero reorder risk), and `#define stepMs (clk.stepMs)` etc. alias
the clock fields under their old names — smallest possible diff.

**Migration order** (one cart per commit — parallel-agent hygiene):

1. ~~`house.c` as pilot~~ ✅ **migrated 2026-06-05** (newest, already had the generalized `lead_to`)
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

## The jam layer — a "can't play wrong" solo over the radio (`solo.h`)

Added 2026-06. The radios *generate*; the jam layer lets you *play along* without
being able to play wrong. `runtime/solo.h` is the companion to `radio.h`: a
horizontal strip of only the safe notes, monophonic with portamento so drags
slide like a stylophone/Omnichord. The station supplies the **voice** (an
`I_SOLO` instrument tuned to sit on top of the mix); solo.h owns the
**interaction** (capture via ui.h's per-finger model, so mouse+touch for free,
glide, optional beat-quantize, octave shift, the strip + the closed "jam" tab).
Usage is one `SoloCtx` fill + one `solo_strip()` call between `ui_begin/ui_end` —
see the header.

### Two locks — match the strip to the station's harmony (not habit)

The original ribbons all hardcoded **major pentatonic** — a copy-paste, not a
decision. The lock should follow the *song*. solo.h offers two, chosen per
station by how its harmony moves:

| Lock | `chordLock` | Cells are… | Use when | Stations |
|---|---|---|---|---|
| **Scale-lock** (default) | 0 | every note of a FIXED scale; chord tones merely *lit* | the song stays in one mode/scale — pass the **song's own** scale, not a generic pentatonic | jangle (mixolydian — the b7 is the idiom), air (its archetype's mode), dub (minor-pent *is* the riddim), napoleon (per-archetype pent) |
| **Chord-lock** (Omnichord) | 1 | ONLY the live chord's tones, re-shaped every chord change | **changes-heavy** harmony where no single scale fits — the station "presses the chord button" for you, you literally can't miss, the harmony slides under your hand | citypop (Royal Road + +2 gear change), bossa (jazz ii-V-I), jingle (Mac-DeMarco borrowed chords), polopan (borrowed-chord progs, + struck) |

Chord-lock detail: a held note does **not** re-pitch when the chord changes — it
re-maps on your **next** press, so your finger only glides when *you* move (calm;
no note sliding out from under you). The strip lights the chord **root**; the
rest are its other tones. This split is what makes the ribbon "follow the song":
modal stations get their actual mode, jazzy stations get the live chord.

**`chordLock` is only the STARTING state — the player toggles it live.** The open
strip carries a small **`ch`/`sc` button** (lit accent `ch` = chord-locked, dim
`sc` = free scale play), and `SOLO_LOCK_KEY` (default `L`) does the same. So
chord-lock is never a trap: a station that feels too constraining is one tap from
the looser scale (the cart's pentatonic over the changes), and a scale station
can be tightened to chord-lock. The button only appears where there's chord
material to lock onto; `solo_locked(cx)` reports the current live state.

### Four ways to play — the articulation axis

The two locks above decide *which notes are safe*. This is the **other** axis:
*what a press actually plays*. The ribbon shape (in-key cells, can't-play-wrong)
stays the same; only the articulation changes. Read top-to-bottom it's one
gradient — **one note held → one note struck → a chord struck together → a chord
rolled** — set by two `SoloCtx` fields (`struck`) plus, for the chord modes, the
parked sibling header. **Two of the four ship today.**

| # | Mode | Notes / press | Articulation | Y controls | Feel | `SoloCtx` | Status |
|---|---|---|---|---|---|---|---|
| 1 | **Sustained lead** | mono | held + portamento glide | a `SOLO_Y_*` macro (vol / cutoff / echo send) — or nothing | stylophone single-finger lead | `struck=0` (default) | **shipped** |
| 2 | **Struck mallet** | mono | re-strikes each new cell, old notes **ring out** | usually `SOLO_Y_OFF` (you can't bend a ringing bar) | **xylophone / marimba run** | `struck=1` | **shipped** (`polopan.c`) |
| 3 | **Chord-trigger** | chord | the whole diatonic triad at once — the Juno-demo / Omnichord chord button | the **strum spread**: top = tight **block** (0ms), bottom = a **rolled strum** (~38ms harp-roll) | press a cell, get a chord; tilt for how hard you "strum" it | `chordTrig=1` | **shipped** (`citypop.c`) |
| 4 | **Strum / piano** | chord | a chord you **rake across by hand** — drag/roll over the cells to arpeggiate its tones, like a strum plate | strum direction + spread, but driven by the **motion** of the finger, not a fixed Y band | expressive Omnichord strum plate — you perform the roll | sibling header | **parked** |

**Modes 1–2 are the mono pair** and already exist: `struck` flips between a held,
gliding voice (the stylophone lead — every melodic station) and a re-struck,
ringing one (the mallet run — polopan's French-dream-pop marimba). Mode 2 *is* the
"xylophone" — perceptually polyphonic because old bars keep ringing, even though
it's one voice handle re-triggered.

**Mode 3 shipped 2026-06-22** as a `SoloCtx` flag (`chordTrig`), NOT the parked
sibling header — see the note below for why it fit in solo.h after all. It's the
literal Juno/Omnichord chord button: a press fires the **diatonic triad rooted at
the cell**, stacked in-scale (`notes[cell]` + the cells two and four up), so the
chord *quality* (maj/min/dim) falls out of the mode for free — you can pick neither
a wrong chord (the ribbon only offers in-key ones) nor a wrong voicing. Y is the
strum spread (block ↔ harp-roll). Ring-out, like struck (no held voice). First home
`citypop` — combined with `chordLock` it's "the station holds the chord, you just
strum it," a Juno comp-stab over the changes. **Mode 4 (Strum / piano)** is still
parked: Y stops being a preset and your finger's travel *is* the strum, so you rake
a chord slow or snap it tight by hand.

> **⚠ OPEN — the strum doesn't sound great yet (2026-06-22).** The mechanic works
> (triad fires, spread responds to Y) but the *voicing/timbre* is unsatisfying as
> shipped. Things to try when we come back to it: (a) the triad is always
> root-position root/3rd/5th from the pressed cell — drop-2 or spread voicings, or
> grabbing the 7th, would sound less blocky; (b) `strum_notes` rides `note()`'s
> fixed 250ms one-shot, so every stab is the same length regardless of `I_SOLO`'s
> envelope — a held/release-able version (the mode-4 voice pool) would let the comp
> breathe; (c) `SOLO_VOL=5` × 3 simultaneous notes may be slamming the bus — try a
> lower per-note vol for chords; (d) the spread range (~38ms) and direction (up vs
> down) want tuning by ear; (e) `citypop`'s `I_SOLO` is a glossy `INSTR_SQUARE`
> lead — a dedicated comp patch (softer attack, less duty-buzz) may sit better than
> reusing the lead voice. Not blocking; the feature is live, just not polished.

All four are **lenses on one ribbon** — lead, mallet, chord-button, strum-plate —
which between them cover the whole Omnichord (melody + chord buttons + strum
plate) plus the mallet toy.

**Why chord-trigger fit in solo.h after all (and why mode 4 still won't).**
solo.h is built around **one** `solo_handle`. The original plan parked *both* chord
modes in a sibling header because they seemed to need a **voice pool + staggered
`note_on`s**. But that's only true if the chord must *sustain while held and stop on
release*. Accept **ring-out** — the chord plays and decays on its own envelope, like
struck mode — and there's no voice to pool: you just fire-and-forget the notes. So
mode 3 became a third articulation flag (`chordTrig`) sitting on a tiny new sound
primitive, **`strum_notes(midis, n, instr, vol, delay_ms)`** (the explicit-notes
sibling of `strum()`; `delay_ms` = the block↔roll spread). ~6 lines in `sound.h` +
~15 in `solo.h`, no new header, no pool.

**Mode 4 (Strum / piano) still wants the sibling header** — `strum.h` / `omnichord.h`
— because raking a chord by hand and *holding* it (the sustained pad, re-voicing on a
chord change under a still finger) is the case ring-out can't cover. Build it on a
changes-heavy station; the standalone **`omnichord` instrument cart** is the
voice-pool + spread reference.

**Why this is the right shape for the toy thesis.** The radios sound good
*because you can't touch them*. An editable step-grid would let a player tap
cells and make it sound *worse* — the generation magic evaporates. The jam strip
is the opposite: **additive performance over fixed generation**, scale-locked so
every note lands. You only ever make it *more* yours, never wreck it. That
"can't play wrong" principle is the candidate answer for how the whole tinyjam racks
direction (`design/tinyjam-racks.md`) should let people interact — perform on top, not
edit underneath. A recorded/looped strip is plausibly tinyjam racks' first *lane* (the
lead — the one you'd rather perform than program).

### Making physical room — the chrome rework + the bottom-third takeover

The four play modes (and the parked chord pair) all want **vertical room**: Y now
means something (mallet has none, but chord-trigger's Y = block↔roll, strum's Y =
spread). The shipped ribbon is an 18px sliver at `y=170` sharing the bottom with a
footer hint — far too short for fat fingers on a phone. So the chassis is being
reworked to give the ribbon real estate, in two steps:

**Step 1 — chrome declutter (SHIPPED 2026-06-11, commit `6d20232`).** The bottom
**footer hint line was retired** (`rad_footer()` is now a no-op): every station's
`H help` / `B band` / `G gtr:…` text is gone, because the affordances are now
buttons. The `?` help button (bottom-right) already existed; a mirrored **`B`
band button** (bottom-left, `rad_band_button()`) now opens THE BAND panel. **All
timbre A/B routes through THE BAND** — the 4 old `G`-key toggles (bossa guitar,
jangle guitar, dub skank, lowend lead) were folded into one-chair `RadBand`s, and
the `G` key is gone. Net effect: the whole bottom strip is now free for the ribbon.

**Step 2 — the bottom-third takeover (PLANNED, next session — `solo.h` + the 8 jam
carts).** Closed, the ribbon is just its small "jam" tab (a radio looks like a
radio). **Open, it expands to fill the bottom third (~`y133–200`), the chrome
behind it dimming** — fat-finger room on a tiny screen, and the radio becomes an
instrument only when you choose to play. Decisions already locked with Nikki:
- **Extent:** bottom third (not half, not full) — his call.
- **Chrome:** dims/darkens behind the expanded ribbon (the takeover is the open
  state of the existing `solo_strip` tab, not a separate mode).
- **`B` / `?` buttons stay reachable** while the ribbon is open (don't let the
  expansion bury them).
- Owns the geometry in `radio.h`/`solo.h` so it rolls to all 8 jam carts (bossa,
  citypop, jingle, polopan, jangle, air, dub, napoleon) the way `rad_dial` did.
- Open question for that session: **animated slide-up vs. instant**, and exactly
  what "dim" looks like (overlay alpha vs. redrawn darker chrome).

### Making room — the station must yield its own lead

The design problem that bit first: most stations *also* play a melodic lead
(bossa's flute, jangle's whistle, dub's melodica). With the strip live you have
**two soloists in the same register**, and two leads at once is mud — true on a
real bandstand too. So the station owes the player room: gate its own lead
block on one of solo.h's two signals. The strip never silences the station (it
can't know which voice is the lead — only the station does), so it's a one-line
guard you add per station.

| Strategy | Signal | Feel | Used by |
|---|---|---|---|
| **Lay out** | `!solo_open()` | Open the tab → station lead stops launching notes; the sounding note rings to its end (finishes on the beat, no chop), then yields. A held/legato lead routes through its existing trail-off so it releases instead of cutting. Brief hole if you open-and-don't-play = an invitation, not a bug. The default. | bossa, citypop, jingle, jangle |
| **Trade** | `!solo_playing()` | Station lead drops out *only while a strip note is held*, fills the gaps when you rest — call-and-response / trading fours. No hole, busiest/most alive. Best where a tail bridges the handoff. | dub (the tape echo makes the trade gorgeous) |
| **Duck** | (vol) | Keep the lead but quieter while you play. Truest to a mixing desk, but our notes are scheduled-ahead at fixed vol so retro-quieting is awkward — Trade is the practical stand-in. Unused so far. | — |

**Curation rule:** only put the strip on stations whose idiom *has* a soloist to
step in for. The five above were chosen for exactly that. Beatless/textural
stations (ambient, satie) have no lead to make room, and a solo pad just fights
the texture — they get no strip by design. The make-room contract also lives in
`solo.h`'s header comment (the toolkit's own doc); this section is the "why."

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
timbre variation most naturally). The timbre roll fixes *who's playing*; its
sibling below fixes *what they play* — do both and a station plays records.

## Same song every night? — the structural axes (groove · form · pocket · tempo · instrumentation)

The timbre roll above fixes *who's playing*; this fixes *what they play*.
Station-owner feedback (2026-06-08, on addis): *"I love the sound, but all the
songs are sort of samey."* Same diagnosis shape as the timbre one — it's about
**which axes the seed rolls**.

A song's identity is the sequence of `srnd()` calls in `new_song()`. The trap is
easy to fall into: roll the **least audible** axes (pitch content) and hold the
**most audible** ones fixed. The ear locks on in this order —

> **groove → tempo → structure → instrumentation → … → pitch-class content (last)**

— so a station that rolls the mode, the key, and the melody cell but keeps one
hardcoded drum pattern, one fixed `FORM[8]`, and a ±5% tempo produces tunes that
*are* "the same song with a different doodle on top." addis (pre-2026-06-08)
rolled qñit/key/cells/timbre and held groove, form, and length fixed — and that's
exactly what it sounded like.

The fix: roll the structural axes too, from the same composition seed, within
style bounds. Ranked by payoff (= salience):

1. **Groove — the percussion feel.** The single biggest win, because the ear
   hears it first. Roll 2–3 named feels (addis: ballad / swing / drive — the kit
   pattern, density, and accent map all change). Two grooves read as two songs
   even with identical harmony. `yacht.c` already did this (session / Purdie /
   CR-78); addis and lowend now do too.
2. **Form + length — the arrangement.** Promote the fixed
   `static const int FORM[8]` to a *rolled set of templates with different
   lengths* — a short sketch, the classic, a long jam. Not every tune should be
   64 bars. A template can also carry a structural *feature*: lowend's "cut" form
   drops a drums-only **break** in the middle (bass/rhodes/lead all cut out for 8
   bars — the sampleable b-boy bars). `motorik.c` rolls a continuous length
   (48–96 bars); addis/lowend roll discrete templates.
3. **Pocket — the per-lane timing personality** (boom-bap, swing, anything
   groove-led). The `PUSH_*` lane offsets (§4 Groove templates) are usually a
   fixed `#define`; roll them per song instead (lowend: tight / swung / drunk).
   This is what lets one station be Premier-crisp on one tune and Dilla-drunk on
   the next.
4. **Tempo — coupled to the groove/pocket.** A ±5% tempo band is inaudible as "a
   different song." Widen it, and *couple* it so a ballad genuinely drags and a
   driver genuinely pushes (addis: ballad 70–85 / swing 88–105 / drive 104–122;
   lowend's tempo follows the pocket).
5. **Instrumentation / lead-voice — the band itself, per song** (`air.c` +
   `polopan.c` — the artist-station case). The per-song *timbre roll* (above) varies one band's knobs;
   this goes further and rolls **which instrument carries the song** — the lead
   *engine* swaps, not just its cutoff. `air.c` bundles this into each archetype: a
   Sexy-Boy roll plays a `INSTR_PD` Moog over a fuzz bass; a Playground-Love roll
   re-aims the *same* `I_LEAD` slot to a `INSTR_REED` tenor sax; Cherry → `INSTR_PIPE`
   flute; Kelly → `INSTR_VOICE` vocoder — so two songs sound like two *records by the
   same artist*, played on different instruments, not one band re-running a chart. The
   trick that makes it cheap: `I_BASS`/`I_LEAD` are reconfigured (`instrument()` engine
   + macros) in `new_song()` from the archetype, exactly like the timbre roll re-aims a
   filter — instrumentation is "just numbers" too, including the `INSTR_*` id. Salience
   note: instrumentation sits mid-list in the ear-order above, *below* groove/tempo —
   so it reinforces an archetype whose groove/tempo already changed; rolling the lead
   voice **without** also moving the feel reads as "same song, different patch." Couple
   it (air bundles voice + groove + tempo + chord-template into one **archetype**), as
   with tempo. **Two stations now do this independently** — `polopan.c` swaps its
   topline `I_STAR` slot between `INSTR_PIPE` (the Nanga flute), `INSTR_VOICE` (the
   Canopée/Ani-Kuni chant) and `INSTR_SINE` (Tunnel glass / Coeur sung) from the
   archetype — so the lead-engine swap has graduated from an `air` one-off to an
   established artist-station pattern (the catalog's second-customer rule). This is the
   artist-archetype bundle (see the Form-brains catalog).

### The implementation pattern

Promote the fixed structure to seed-rolled data, exactly like the timbre roll:

```c
// before: one fixed arrangement
static const int FORM[8] = { S_INTRO, S_HEAD, /* ... */ };
static int sect_of(long bar){ long x=bar/8; return x<8?FORM[x]:S_OUTRO; }

// after: a rolled set with variable length + length/section-count helpers
static const struct { int n; int s[12]; const char *name; } FORMS[] = {
    { 5,  { S_INTRO, S_HEAD, S_VSOLO, S_HEAD, S_OUTRO }, "sketch" },
    { 8,  { /* the classic */ }, "set" },
    { 11, { /* a long jam */ }, "jam" },
};
static long song_bars(void){ return (long)FORMS[sng.form].n * 8; }  // replaces the hardcoded 64
static int  sect_of(long bar){ int x=bar/8,n=FORMS[sng.form].n; return x<n?FORMS[sng.form].s[x]:S_OUTRO; }
// in new_song(): int fr=srnd(100); sng.form = fr<30?0 : fr<75?1 : 2;
```

Gotchas (the ones that bit during the addis/lowend passes):

1. **Kill every hardcoded `64`.** The song-length cap in `play_step` and the
   rollover check in `update()` both assumed 8×8 bars — both must read
   `song_bars()`. The phrase-dot count (`rad_phrase_dots`) must read the form's
   section count, or the progress display lies about the song.
2. **Solo/section lengths must be *read off* the form, not constants.** addis
   computed solo start as a literal `bar - 24`; the jam form doubles a solo to 16
   bars, so it walks the contiguous run instead (a `solo_span` helper) and feeds
   the length to `improv_begin` — the tension arc then stretches to fit.
3. **Roll from `srnd`, not engine `rnd()`** — same rule as the timbre roll; the
   structure is part of what the seed IS.
4. **Couple, don't scatter.** Tempo that follows the groove reinforces it; an
   independent tempo roll just adds noise. One axis pulling another is what makes
   the variation read as *intent* rather than randomness.

Acceptance test (same as the timbre roll): **two SPACE presses should sound like
two different records.** Seed caveat: adding `srnd()` calls reorders the stream,
so old pinned `#seeds` render different songs afterward — fine for a feature add,
and a non-issue while `<NAME>_SEED` is 0 (flag it either way).

Worked examples: **addis** (groove + form + coupled tempo), **lowend** (pocket +
form-with-break + coupled tempo). With the timbre roll above, the two refits are
what turn "one session band replaying one tune" into a station that plays records.

## Future stations → moved

The station-candidates parking lot (ranked candidates, the citypop conditions,
the new-brains-per-cart axis, the IDM + functional-music wings) **moved to
[`../design/future-stations.md`](../design/future-stations.md)** (2026-06-07) —
it's exploration/backlog, not how-to, and it had grown to ~280 lines inside this
guide. This guide keeps the *recipes*: clock, brains, voice leading, instrument
recipes, the style cheat-sheet, `radio.h`.

## Verifying without ears

The debug harness makes the music inspectable (see [`debug-harness.md`](debug-harness.md)):
`watch()` the current bar/chord/step under `#ifdef DE_TRACE`, then

```bash
node tools/play.js bossa run --headless --trace t.jsonl --frames 3600
```

and read the chord stream out of the trace — progression, form, and loop-around
are all checkable from JSONL. Determinism of a pinned seed is two runs + a diff.
