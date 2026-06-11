# Napoleon Dynamite radio — the blind band brief (Phase 1)

Written *before* opening any cousin cart (radio-station-howto.md firewall). This is an
**artist station** — except the "artist" is the *film's musical world*: the 2004
soundtrack (Jamiroquai, Alphaville, the White Stripes, Kip's wedding serenade) plus
**John Swihart's original score** — the deadpan, lo-fi, toy-keyboard cue that IS "the
Napoleon Dynamite sound." So the dial must play recognizably DIFFERENT *songs of theirs*,
not one texture re-keyed. This brief names actual tracks + what makes each one itself;
those become the **song archetypes** rolled in Phase 4 (stolen-playbook chord brain #4 +
bundled groove/form/pocket/tempo/lead).

## The world, from the music (not the palette)

The film's sonic identity is a deliberate clash of registers: a **deadpan lo-fi indie
score** (cheap Casio/toy-organ, plucky muted guitar, glockenspiel, a stiff little
drum-machine — twee-but-melancholy, small-town Idaho) sitting next to needle-drops that
are each a whole different genre — **acid-jazz funk** (the dance), a **70s quiet-storm
soul ballad** (Kip's serenade), **anthemic 80s synth-pop**, and **fingerpicked acoustic
folk**. The joke of the soundtrack is exactly that these don't match; the station should
celebrate that — five wildly different songs that all still feel like the movie.

The ideal lineup (intent-first), per song-world:

| role | ideal voice | the reference |
|---|---|---|
| toy/Casio organ | cheap detuned PWM/square combo-organ, slightly out of tune | Swihart score main theme |
| plucky muted guitar | short palm-muted single-note pluck | Swihart score |
| glockenspiel / music box | glassy mallet twinkle | Swihart score, sweet cues |
| stiff drum-machine | a deadpan boom-tap preset kit, no swing | Swihart score |
| funky clav / wah guitar | percussive comped clavinet, wah'd 16ths | **Canned Heat** (the dance) |
| slap/funk bass | popping fingered funk bass that struts | Canned Heat |
| brass stabs | tight horn-section hits | Canned Heat |
| disco hat/four-floor kit | crisp four-on-the-floor + open hat off-beats | Canned Heat |
| Rhodes / electric piano | warm, lush, romantic | the serenade ("Always & Forever") |
| smooth soul bass | round fingered bass, slow & deep | the serenade |
| lush string pad | quiet-storm strings swell | the serenade |
| falsetto croon voice | sweet earnest male falsetto | Kip's wedding serenade |
| big 80s saw/PWM pad | fat anthemic synth wash | **Forever Young** (80s synth-pop) |
| gated-reverb tom kit | huge 80s snare/tom hits | Forever Young |
| bright synth arpeggio | sequenced 8th/16th arp | Forever Young |
| fingerpicked nylon/steel | gentle childlike fingerpicking | **We're Going to Be Friends** |
| soft brushed/handclap kit | intimate, sparse | We're Going to Be Friends |

## The five song archetypes (the dial)

Each FIXES its lead voice, groove, tempo, key-mood and form; the seed varies key/patterns
*within* the archetype, so "the dance one" sounds different every time but always like the
dance. (Chord templates encoded as degree/quality rows — stolen-playbook brain #4.)

1. **DANCE** — "Canned Heat" (Jamiroquai), Napoleon's class-president dance · ~125 BPM ·
   minor, funky/euphoric · driving four-on-the-floor + off-beat open hats · STAR = popping
   slap-funk bass strut + wah'd clav 16ths · brass stabs on accents · falsetto "ooh"
   accents. Form: intro → groove-build → horn-stab chorus → break-down → big finish.
   Chords (minor vamp, funk): i7–IV7 vamp / i7–bVII7–IV7 with a ii–V lift.

2. **SERENADE** — Kip's wedding song to LaFawnduh ("Always & Forever") · ~68 BPM · major,
   romantic/quiet-storm · slow soul ballad, finger-snaps on 2&4 · STAR = sweet falsetto
   croon lead over lush Rhodes · round soul bass · string-pad swell. This is the user's
   "Kip waits for LaFawnduh" theme — the most tender setting. Form: slow intro → verse →
   swelling chorus. Chords (smooth-soul major7): Imaj7–iii7–vi7–ii7–V7 turnaround.

3. **SWIHART** — the original score main theme (the deadpan identity of the film) ·
   ~96 BPM · major but wonky/innocent · stiff straight-8 drum-machine, no swing · STAR =
   detuned cheap toy/combo-organ melody doubled by plucky muted guitar · glockenspiel
   counter-twinkle · deliberately naive. The "sounds like Napoleon Dynamite specifically"
   one. Form: loop with an A/B melodic phrase, terraced layers. Chords: childlike diatonic,
   I–V–vi–IV / I–IV–V, no fancy extensions (the point is innocence).

4. **FOREVER** — "Forever Young" (Alphaville) anthemic 80s synth-pop · ~108 BPM · minor→
   relative-major lift, wistful-nostalgic · gated-reverb 80s drums, half-time feel · STAR =
   fat saw/PWM pad chords + bright synth arpeggio · earnest vocal-ish lead. Small-town
   nostalgia. Form: intro arp → verse → big anthemic chorus. Chords: i–bVI–bIII–bVII
   (epic-80s) / vi–IV–I–V chorus lift.

5. **FRIENDS** — "We're Going to Be Friends" (White Stripes) gentle fingerpicked folk ·
   ~88 BPM · major, intimate/childlike · sparse brushed kit + handclaps, lots of space ·
   STAR = fingerpicked nylon/steel guitar arpeggios, single voice · almost a nursery rhyme.
   Form: verse-only, very simple, builds by adding one voice at a time. Chords: plain
   I–IV–V folk, capo'd bright.

## Brain picks (Phase 4) — preliminary

- **chord brain #4 — stolen playbook**: 5 template progressions, one per archetype, rolled.
- **time feel**: per-archetype groove (rolled by archetype, not free): driving disco /
  slow soul ballad / stiff straight-8 / half-time gated 80s / sparse folk. Coupled tempo.
- **melody**: re-pitched cell per archetype, on the archetype's STAR lead voice.
- **bass**: per-archetype (slap-funk strut / round soul / simple root / synth pulse / folk root).
- **form**: rolled per archetype (terraced layers + density curve via rad_level).
- **timbre roll**: within an archetype, vary register/filter/voicing so two DANCEs differ.

## Phase 2 — palette shop (gaps = new recipes)

Matched each imagined voice to the supply side (`instrument-recipes.md`) and the demand
side (`instrument-presets.md`). The headline: only **AIR** uses `INSTR_VOICE`, and as a
*robotic vocoder lead* — so the **sung, vibrato falsetto croon** (Kip's serenade + the
Jamiroquai "ooh") is this cart's highest-value addition: the first radio voice to use the
formant engine as an actual *singer*, not a vocoder.

| voice (archetype) | engine / recipe | source | new? |
|---|---|---|---|
| **falsetto croon / "ooh"** (SERENADE lead · DANCE accent · FRIENDS voice) | `INSTR_VOICE` voice/sing — vibrato-rich singing formant | voxlab `voice/sing`; air's is a vocoder | **NEW — first SUNG voice on `INSTR_VOICE`** (highest value) |
| funk slap bass (DANCE star) | `INSTR_TRI`/SAW round bass + hard pitch-env pop + bright cut-env | kin `tri/disco-bass` octave-pop | **NEW recipe** `tri/funk-slap` |
| wah clavinet (DANCE comp) | `INSTR_EPIANO` clav h0.85 + env-wah quack | epiano.c `epiano/clav` | NEW radio voicing (`epiano/funk-clav`) |
| horn-section stab (DANCE) | `INSTR_BRASS` bright trumpet stab | brass.c `trumpet`; afrobeat/mariachi tapped it | reuse engine |
| disco four-floor kit + open hat + clap (DANCE) | reuse `drum/french-house-kick` · `drum/house-hats` · `drum/house-clap` | house/italo | **reuse on purpose** (add napoleon to **used by**) |
| Rhodes (SERENADE keys) | `INSTR_EPIANO` rhodes h0.15 | epiano.c `epiano/rhodes`, italo `epiano/rhodes-detent` | reuse engine |
| round soul bass (SERENADE) | `INSTR_SINE` round bass, soft | `sine/round-bass` (jangle/jingle) | reuse idea |
| lush string pad (SERENADE · FOREVER) | `INSTR_SAW` string machine, slow swell | `saw/string-machine` pile (air/italo/house/motorik) | **reuse on purpose** |
| detuned toy/Casio organ (SWIHART star) | detuned dual-`INSTR_SQUARE` PWM pair, cheap & slightly out of tune | kin roadhouse `user0/combo-organ` (different approach) | **NEW recipe** `square/toy-organ` — the deadpan signature |
| plucky muted guitar (SWIHART double) | `INSTR_GUITAR` pizz h0.20 t0.60 m0.85 | guitar.c `guitar/pizz` | NEW radio voicing |
| glockenspiel (SWIHART · sweet cues) | `INSTR_MALLET` glockenspiel h0.90 t0.85 m0.60 | mallet.c `mallet/glockenspiel` | NEW radio voicing (MALLET = vibes/celesta/bronze only so far) |
| stiff straight-8 drum machine (SWIHART) | reuse `drum/synth-kick` · `drum/noise-snare` · `drum/noise-hat`, quantized hard | citypop/motorik/etc. variants | reuse, no swing |
| fat 80s saw/PWM pad (FOREVER star) | `saw/string-machine` opened up + bright | the pad pile | reuse |
| bright synth arpeggio (FOREVER) | `INSTR_PLUCK`/SAW 16th arp | `pluck/seq-arp` (italo), `tri/sequencer` (ymo) | reuse idea |
| anthemic synth lead (FOREVER) | `INSTR_SQUARE` glossy/saw lead | `square/glossy-lead` (citypop) | reuse idea |
| synth pulse bass (FOREVER) | `INSTR_SAW`/SQUARE octave pulse | `pulse/moog-bass` (motorik) | reuse idea |
| fingerpicked nylon/steel (FRIENDS star) | `INSTR_GUITAR` nylon h0.45 / steel h0.55 | guitar.c `guitar/nylon`,`/steel`; air nylon | reuse engine |
| sparse brushed kit + handclaps (FRIENDS) | reuse `kit/cocktail-brushes` sweep + `drum/house-clap` | cocktail/house | reuse, very sparse |

### New recipes this cart contributes
1. **`voice/croon`** (`INSTR_VOICE`) — the first radio voice to *sing* on the formant engine
   (AIR uses it as a vocoder, not a singer); a sweet
   vibrato falsetto. The single most valuable thing the cart adds.
2. **`square/toy-organ`** — a deliberately-cheap, slightly-detuned dual-square combo organ:
   the John Swihart deadpan-score identity. Detune-for-character (kin gamelan's ombak trick,
   used for cheapness not shimmer).
3. **`tri/funk-slap`** — the popping DANCE bass (Jamiroquai strut).
4. New *voicings* on existing engines: `epiano/funk-clav`, `mallet/glockenspiel` (first radio
   glock), `guitar/pizz` muted (first radio pizz-mute).

### Effects we lack — SAVE THE GAP (Phase 2.4)
Recorded per-cart in [`napoleon-effects-wants.md`](napoleon-effects-wants.md):
- **gated reverb** — FOREVER's huge 80s snare/toms are defined by it; faked with a long-R
  noise tail, badly. The clearest want.
- **plate/spring reverb** — SERENADE's strings/croon and the whole quiet-storm wash want it.
- **tape wow/flutter + lo-fi** — SWIHART's cheapness is partly tape; faked with pitch-LFO drift.
- **chorus** — the 80s pad & toy organ want an ensemble chorus (faked with detuned pairs).
