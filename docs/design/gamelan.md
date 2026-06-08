# gamelan — the bronze drift (Java × Bali) — build spec

> **Status: SHIPPED 2026-06-09** (`tools/carts/gamelan.c`, station #19) — the core
> (the bronze-bank microtonal tuning + per-village jitter, colotomic gong cycle,
> kotekan interlock, gong-pair ombak, irama feel knob, the suling float-pitch voice,
> the gong-rack window) is built and verified (compiles clean; kotekan masks
> trace-asserted disjoint+full; audio renders continuous with no clipping in both a
> Java tanggung and a Bali wiled run). This doc is now the design record + the
> open-items parking lot (see §Open questions and the gong note). The one gamelan
> entry in [`future-stations.md`](future-stations.md) (the parking lot), promoted to
> a full
> spec because it carries **the most genuinely-new pieces of any station since the
> original family** — seed-rolled microtonal tuning, colotomic time, and kotekan
> interlock, all at once — and because the foundational research confirms every one
> is buildable on the shipped API (no engine port required). Engine-side companions:
> [`../guides/game-music.md`](../guides/game-music.md) (the brain catalog + `radio.h`
> chassis), [`radio-instrument-options.md`](radio-instrument-options.md) (the
> per-song timbre roll), [`instrument-engines.md`](instrument-engines.md) §8.8.2
> (the MALLET engine), [`audio-notes.md`](audio-notes.md) §12 (the gaps ledger — see
> the **gong** note for the one place this cart approximates rather than models).
> `addis.c` is the direct ancestor — read it first: scales-as-data, the modal vamp,
> and the five-engine band are all moves this cart reuses and extends. `exotica.c`
> is the cleanest scaffold-born reference.

## The essence

A gamelan is a single tuned instrument made of many bronze parts — metallophones
(saron, gendér), kettle-gong rows (bonang), big hanging gongs (gong ageng,
kempul), drums (kendang), and a bamboo flute (suling) — playing **stratified
polyphony**: one slow core melody (balungan) elaborated at every faster density
by the higher instruments, all locked to a cycle that a gong closes. Two regional
poles: **Java** (slow, deep, meditative — court gamelan) and **Bali** (fast,
bright, interlocking — the kotekan attack). Both float on **ombak** — the shimmer
of paired bronze tuned a few cents apart, beating against itself.

Three reasons it earns the slot (more new brains than anything in the queue):

1. **It proves seed-rolled MICROTONAL tuning** — the first station whose pitch set
   is *not* 12-TET. `addis.c` left major/minor but stayed on 12 equal semitones;
   gamelan plays cent tables, and **the seed rolls the tuning itself** (no two
   village ensembles are tuned alike, so no two songs are played on the same
   bronze).
2. **It proves COLOTOMIC time** — form as nested gong cycles, not
   bar→phrase→section. A new TIME brain.
3. **It proves KOTEKAN** — melody as two interlocking voices on complementary
   onset masks summing to an unbroken stream. A new MELODY brain (#3), and
   trace-verifiable by definition.

Plus **ombak** as the genre's whole timbral identity (ambient's incidental detune
promoted to the point) and **irama** as the feel knob (density doubles as the
pulse halves — the gamelan way of "more intense").

Tempo: slow pulse, deep subdivision. The balungan moves at a walking pace
(**~50–80 "balungan BPM"**) while the elaborating voices run 2×, 4×, 8× faster —
so the *felt* tempo is set by irama, not by the gong. Machine-tight on the grid
(the bronze is struck by the cycle), with **only ombak** as the living detune —
this is the anti-`tango` on jitter, like `motorik`, but the shimmer is microtonal
rather than rhythmic.

---

## The heart #1: TUNING AS DATA (the headline new brain)

Confirmed against the API: `note_pitch(int handle, float midi)` (sound.h:2101)
takes **fractional MIDI** — the audio thread decodes it as `440·2^((midi−69)/12)`,
so any pitch between two semitones is one float away. A scale is a **cent table**,
and a pitch is `root_midi + cents/100.0f`.

```c
// sléndro: 5 roughly-equal steps spanning the octave (NOT 240¢ each — that's the
// myth; real ensembles vary wildly). Stored as cents from the gong tone (tonic).
// pélog: 7 unequal steps, most songs use a 5-note subset (a "pathet").
static float scaleCents[8];   // filled per song; [0]=0 always
static int   scaleN;          // 5 (sléndro) or 7 (pélog)

// degree (can be negative or > scaleN — wraps with octave displacement) → float MIDI
static float deg_to_midi(int deg, float rootMidi) {
    int oct = 0;
    while (deg < 0)       { deg += scaleN; oct--; }
    while (deg >= scaleN) { deg -= scaleN; oct++; }
    return rootMidi + scaleCents[deg] / 100.0f + (float)(oct * 12);
}
```

**The seed rolls the tuning.** Start from an "ideal" sléndro/pélog template, then
jitter every degree by a seeded ±15–35 cents — that's the per-village bronze. A
pinned seed plays the *same ensemble* every time; a fresh seed re-casts the metal.

```c
// per song, after picking the system:
for (int i = 0; i < scaleN; i++)
    scaleCents[i] = idealCents[i] + (srnd(71) - 35);   // ±35¢ village drift
scaleCents[0] = 0;                                       // pin the tonic
```

Two ways to *sound* a degree, both confirmed:

- **Melodic voices** (saron, bonang, gendér, suling): `note_on(60, …)` then
  immediately `note_pitch(handle, deg_to_midi(deg, root))`. Or for struck
  one-shots, hold the handle just long enough to set pitch + gate. (Mallet voices
  read `freq · pitch_mul` every sample, so the float lands cleanly — sound.h:1251.)
- **The whole rank at once**: `instrument_tune(slot, semitones)` (studio.h:336)
  bends *every* sounding voice on a slot live, fractions included — useful if a
  whole metallophone rank should sit a hair sharp of another (see ombak below).

> **Decision for the build:** tuning rolls **per song** (each broadcast is one
> ensemble), not per session. The display shows the system + a made-up village
> name keyed to the seed ("Kyai …"), the gamelan-naming convention.

## The heart #2: COLOTOMIC TIME (the new TIME brain)

Javanese form is not bar→phrase→section. It is **nested gong cycles**: the *gongan*
(one full turn of the great gong) is punctuated at fixed fractions by smaller
gongs. The arrangement curve becomes concentric circles, not a timeline.

The `radio.h` step clock already counts absolute 16th-steps (`rad_clock_step`,
radio.h:142); colotomic time is just a different **gating** of that count — no new
machinery. Pick a gongan length in steps (a power of two so the punctuation lands
clean), then mark it:

```c
// seeded per song: which colotomic form (its gongan length + punctuation density)
static int gonganSteps;     // e.g. 64 (one gongan = 4 bars) … 256 (16 bars), srnd

static void play_step(long abs, double pos) {
    long s = abs - songBase;  if (s < 0) return;
    int dly = rad_step_dly(&clk, abs, pos);
    long g   = s % gonganSteps;                 // position within the gongan
    int  half = gonganSteps / 2, qtr = gonganSteps / 4, eighth = gonganSteps / 8;

    // the punctuating gongs — nested, biggest marks the rarest event
    if (g == 0)                       gong_ageng(dly);   // closes/opens the whole cycle
    else if (g == half)               kempul(dly);       // halfway
    else if (g % qtr == 0)            kenong(dly);        // quarters
    else if (g % eighth == 0)         kethuk(dly);        // eighths (the "tick")
    // … balungan + elaboration below, on the 16th grid …
}
```

The **gong stroke is the deepest event on the dial** — everything resolves *to* it,
not *from* a downbeat. (This is also why the gong's long shimmering decay matters
so much, and why it's the one place we approximate — see the gong note in
§Instruments and audio-notes §12.)

## The heart #3: KOTEKAN (the new MELODY brain, #3)

Balinese interlock: two players (**polos** and **sangsih**) play *complementary*
onset masks that sum to an unbroken fast stream neither plays alone. This is a new
melody brain — every prior station either repeats a fixed cell or runs the
single-voice `improv.h` soloist; nobody splits one line across two hands.

`improv.h` is **single-voice** (one `onset[]`/`deg[]` buffer — improv.h:34), so
kotekan is **hand-rolled**, not borrowed. `addis.c` already shows the pattern:
two onset tables driven off the same seed (head + horn answer), played per-step in
`play_step`. Kotekan is that with a hard contract: the two masks are *disjoint* and
their union is *full*.

```c
// over a 16th grid, polos + sangsih onsets partition the stream.
// the classic "norot"/"nyog cag" figures: one voice on the offbeats, one on the
// beats, the union a continuous 1/16 run. Seeded pitch contour, fixed interlock.
static unsigned polosMask, sangsihMask;   // 16-bit onset masks per beat-group

// CONTRACT (trace-verifiable): polosMask & sangsihMask == 0      (disjoint)
//                              polosMask | sangsihMask == 0xFFFF  (full stream)
```

Build it by generating one onset stream, then **deal** its hits alternately to the
two voices (guarantees disjoint + full by construction). Pitch the two voices a
register apart (sangsih usually above), both walking the same seeded contour in
the rolled scale. The acceptance test literally asserts the two masks partition the
grid (see Verification).

> Java elaborates differently (no hard interlock — the higher instruments just
> subdivide the balungan at 2×/4×). Make kotekan the **Bali** end of the feel knob
> and plain subdivision the **Java** end (see irama, below).

## Ombak (the timbral identity, not a brain)

Paired bronze tuned a few cents apart beats — the shimmer that makes a gamelan
sound *alive* and not like a synth. Two confirmed routes; the build uses the
cheap one:

- **Slot detune (recommended):** put the two ranks of a pair on two slots and
  `instrument_tune(slotB, +0.06f)` (studio.h:336 — "0.06 = unison shimmer"). One
  call, every voice on the rank shimmers, scheduled hits included.
- **Per-voice (ambient's way):** `note_pitch(h1, m)` vs `note_pitch(h2, m+0.04f)`
  — ambient.c does `det[i]=rnd_float_between(-0.06,0.06)`. More control, costs two
  voices per note (mind `SOUND_VOICES`).

Ombak is **performance-adjacent** but should be *seeded* here (it's part of the
village's tuning identity), unlike ambient's per-frame random. Roll the beat rate
per song with the tuning.

## Irama (the feel knob)

In gamelan, "more" is not louder — it is **deeper subdivision at a slower pulse**
(irama levels: lancar → tanggung → dadi → wiled → rangkep, each halving the pulse
and doubling the elaboration density). Map the `rad_input` intensity 0..3
(radio.h:176) straight onto irama:

| intensity | irama | balungan pulse | elaboration | region lean |
|---|---|---|---|---|
| 0 | lancar | fastest | sparse (gong + balungan only) | — |
| 1 | tanggung | — | bonang doubles | Java |
| 2 | dadi | slower | gendér + kotekan enters | — |
| 3 | wiled | slowest | full interlock, suling floats | Bali |

So the feel knob **slows the pulse as it thickens the texture** — the opposite of
every other station's "intensity = density at fixed tempo". That inversion is the
genre talking. (Implementation: irama scales both the balungan step stride *and*
the elaboration masks; the `bpm()` can stay fixed while the *musical* pulse halves.)

---

## Brain pick (shop the catalog first)

| slot | brain | source | gamelan's twist |
|---|---|---|---|
| **tuning** | **TUNING AS DATA** | **NEW** (this cart) | seed-rolled microtonal cent tables — first non-12-TET station |
| **time** | **COLOTOMIC** | **NEW** (this cart) | nested gong cycles, form as concentric circles |
| **melody** | **KOTEKAN (#3)** | **NEW** (this cart) | one line split across two interlocking voices (Bali end) |
| chord | the modal vamp | `addis.c` | a tonic gong-tone drone — even less harmonic motion than addis; the cycle *is* the harmony |
| feel | **irama** | **NEW** twist on `rad_level` | slower pulse + deeper subdivision, not louder |
| timbre | ombak + per-song roll | `ambient.c` (detune), `radio-instrument-options.md` | seeded beating as identity; village bronze rolled per song |

Reused chassis (verbatim from `radio.h`): `rad_srnd` + seed/history, `RadioClock`
schedule-ahead, `rad_input`, the whole draw chassis (`rad_body`/`rad_dial`/
`rad_knob_*`/`rad_help_panel`/`rad_footer`/`rad_power_led`), the tone knob
(`RAD_TONEMUL`). **Not** reused: `rad_lead_to` (no functional harmony to voice-lead)
and `improv.h` (kotekan is two hand-rolled voices, not a soloist) — though a
**suling improviser** is a natural later add (Java's *senggakan*), one floating
single-voice `improv.h` line over the bronze, exactly motorik's "lone performed
voice" exception.

---

## The band (instruments)

Confirmed engine mapping (see [`instrument-engines.md`](instrument-engines.md)
§8.8.2 and the research in this session). **Note the 6-arg `instrument()`
signature** — `instrument(slot, wave, attack_ms, decay_ms, sustain 0..7,
release_ms)` (studio.h:287); the long sustain+release is how a struck engine fakes
a ringing gong.

```c
#define I_SARON   5   // core metallophone — balungan + saron elaboration
#define I_BONANG  6   // kettle-gong row — faster elaboration / kotekan voice A
#define I_GENDER  7   // soft metallophone — kotekan voice B (sangsih)
#define I_GONG    8   // the big hanging gongs (ageng/kempul/kenong) — long ring
#define I_KENDANG 9   // hand drum — the conductor
#define I_SULING 10   // bamboo flute — floating melody (optional later: improv.h)
// ombak pairs may need a duplicate slot (e.g. I_SARON2) at +0.06 tune
```

- **Metallophones (saron / gendér) — `INSTR_MALLET`.** The core bronze. Start from
  exotica's vibes (`h=0.12, t=0.40, m=0.72`) but **dial morph DOWN** — gamelan
  bronze has no vibraphone motor tremolo (motor lives in the top third of `morph`).
  Push `harmonics` toward the bell/inharmonic end for the metallic clang. Saron =
  brighter/harder strike; gendér = softer, longer ring.
- **Bonang (kettle-gongs) — `INSTR_MALLET`, bell end.** `h≈0.9, t≈0.7, m≈0.4` —
  tuned, struck, shorter ring than the metallophones. The fast elaborating voice
  (and one half of the Bali kotekan).
- **Gongs (ageng / kempul / kenong) — `INSTR_MALLET` + LONG release.** The one
  approximation. `instrument(I_GONG, INSTR_MALLET, 1, 0, 7, 5000)` — sustain 7,
  5 s release — full bell ratios, soft strike, max ring. Add `instrument_tune`
  shimmer for ombak *inside* the gong, and a touch of `instrument_echo()`
  (studio.h:344 — the dub-throw bus send) for pavilion air. **Why approximate:**
  there is no sustained-inharmonic-metal engine with a 10–30 s shimmering tail; the
  true fix is the deferred room/reverb layer. Full rationale + the workaround
  recipe: [`audio-notes.md`](audio-notes.md) §12, the gong note.
- **Kendang (hand drum) — `INSTR_MEMBRANE`.** The conductor — signals tempo and
  irama changes. Copy addis's congas (`instrument(SL_CONGA, INSTR_MEMBRANE, 1, 0,
  7, 200)`); two slots for the high/low (lanang/wadon) pair. `harmonics` = head
  character, `timbre` = strike position (center thump vs edge slap), `morph` =
  pitch-bend for the open-tone glide.
- **Suling (bamboo flute) — `INSTR_REED` (placeholder).** No `INSTR_FLUTE` exists;
  reed is the only continuous-blow held voice. Tune toward the hollow/breathy
  (mellow `timbre`, light `morph` breath). Floats free of the grid — the one
  un-quantized, rubato voice (and the natural home for a later `improv.h` line).

---

## The face / window art

Two candidate directions (pick one in the build):

1. **The gong rack** (recommended — most legible): the hanging gongs and the
   bronze ranks in a carved frame; **the great gong flares and ripples on each
   gongan close** (the deepest visual beat on the dial, matching the deepest
   musical event), the kettle-gongs of the bonang flash as they're struck. The
   colotomic structure is *visible* — you watch the cycle breathe toward the gong.
2. **The wayang shadow puppet** — a kulit silhouette dancing the cycle behind a
   lit screen, its gestures keyed to the gong punctuation. Prettier, more
   atmospheric, less legible as "what is the music doing."

Either way: a **cycle ring** (concentric, à la `rad_phrase_dots` but circular) is
the natural progress indicator — colotomic time wants a clock face, not a bar. The
tuning/village name and the irama level read on the display panel.

---

## Seed: composition vs performance

**Composition (`rad_srnd`, = the song):** the tuning system (sléndro/pélog), the
per-village cent jitter, the key/gong-tone, `gonganSteps` (the colotomic form), the
balungan core melody, the kotekan contour + interlock figure, the region lean, the
ombak beat rate, the per-song timbre roll (mallet macros, kendang dressing, suling
on/off), title/village name, dial freq.

**Performance (`rnd`, never seeded):** kept minimal — gamelan is ensemble-tight, so
mostly just micro-velocity on the elaborating voices. The bronze spine is
machine-invariant like motorik's. (The later suling improviser would be the framed
exception — the lone performed voice, new every listen, à la motorik's soloist.)

---

## Build plan (the checklist)

1. `tools/carts/gamelan.c` — start from a copy of `addis.c` (closest relative:
   scales-as-data + modal vamp + multi-engine band). A settings-only `.cart.js` is
   likely enough (draw the rack with primitives); add a small `.cart.js` only if
   the gong-rack art wants sprites.
2. Build + bake: `node tools/make-cart.js tools/carts/gamelan.c
   editor/public/carts/gamelan.cart.png`, then `--run` to bake the screenshot.
3. Register in `editor/public/carts/index.json` with the radio family's tags
   (match a sibling like `exotica`/`addis`: `kind` + `genre`, `homage:
   "Javanese / Balinese gamelan"`); `node tools/lint-carts.js`.
4. **After touching no `runtime/` files this is just a cart** — but if the gong
   approximation pushes an engine tweak, re-run the sound tripwire (CLAUDE.md).
5. Docs: flip the `future-stations.md` gamelan entry to ✅ (point at this spec),
   add a `game-music.md` cheat-sheet row, and mark the three new brains ✅ in the
   brain catalog (TUNING AS DATA, COLOTOMIC time, KOTEKAN / melody brain #3).

## Verification

- **Seed-compat** — the standard `radio.h` trace-diff: pin a seed, run
  `--det --seed 7 --frames 3000`, the `.w` stream must be reproducible (the village
  tuning, the balungan, the kotekan contour all re-derive identically).
- **KOTEKAN proof (the new one)** — `watch()` the two onset masks each beat-group;
  assert in the trace that `polos & sangsih == 0` (disjoint) and
  `polos | sangsih == full` (the union fills the stream). This is the brain's
  acceptance test, true by construction if the deal-alternately build is correct.
- **COLOTOMIC proof** — `watch()` the gong-stroke frames; assert gong_ageng fires
  exactly at `g == 0`, kempul at the half, kenong on quarters — monotonic, no drift
  over a long run.
- **TUNING proof** — `watch()` the rolled `scaleCents[]`; confirm a fresh seed
  re-casts them and a pinned seed reproduces them byte-for-byte. Ear-check ombak:
  the shimmer should be audible but slow (< ~6 Hz beat).
- **Sound tripwire** — `soundcheck`-style headless run, silence = PASS; watch the
  long-release gong voices don't leak the voice pool (`SOUND_VOICES`), since each
  gong holds ~5 s.
- **mobile-lint** — radios rank touch-ready via the `rad_knob_*` widgets.

## Open questions (decide at build time)

- **Tuning realism vs listenability** — how much village jitter (±35¢?) before it
  reads as "out of tune" to a Western ear rather than "alive". Start conservative,
  open it up by ear.
- **Gongan length set** — which colotomic forms to roll (lancaran, ketawang,
  ladrang have canonical lengths); how many to offer.
- **Java/Bali as the feel knob ends** vs a separate region selector (the `T` tone
  knob, or a `rad_knob_sel`). Leaning: feel knob spans irama *and* region (slow-Java
  → fast-Bali), `T` does timbre.
- **Suling improviser now or later** — ship the bronze spine first; add the
  floating flute solo (`improv.h`) as the framed performed voice in a second pass,
  like motorik's vibraphone soloist.
- **Face** — gong rack vs wayang puppet.
- **Gong tail** — how long is too long before the approximation's seams (the
  MALLET self-decay fighting the 5 s release) show; whether `instrument_echo` air
  is enough or it reads dry. The honest fallback is a shorter, more present gong
  that doesn't pretend to ring for 20 s.
