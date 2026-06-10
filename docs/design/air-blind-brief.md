# AIR radio — the blind band brief (Phase 1)

Written *before* opening any cousin cart (radio-station-howto.md firewall). AIR is an
**artist station**, not a genre: the dial must play recognizably DIFFERENT *songs of
theirs*, not one texture re-keyed. So this brief names actual tracks and what makes each
one itself; those become the **song archetypes** rolled in Phase 4 (stolen-playbook
chord brain #4 + bundled groove/form/pocket/tempo/lead).

## The band, from the music (not the palette)

AIR (Nicolas Godin & J.B. Dunckel) — French dream-pop / space-age easy-listening built on
**vintage analog gear**: Moog/Minimoog leads, a **Solina/ARP string-machine** wash (THE
signature pad), Fender Rhodes & Wurlitzer electric pianos, fat fuzzy/saturated synth
basses *and* fluid melodic basslines, vocoder + breathy whispered vocals, smoky tenor sax,
nylon/acoustic guitar, flute & recorder, vibraphone twinkle, and soft vintage-drum-machine
kits (CR-78-ish swish, hand-claps, brushed ballad kit). Lush, slow, melancholy-but-sunny,
reverbed.

The ideal lineup (intent-first):

| role | ideal voice | the AIR reference |
|---|---|---|
| string-machine pad | Solina/ARP detuned-saw ensemble, slow swell | every Moon Safari track |
| electric piano | warm Rhodes / Wurlitzer | La Femme, Playground |
| fuzzy lead/riff bass | saturated Moog square-ish bass | **Sexy Boy** riff |
| melodic bass | fluid round fingered bass that walks | **La Femme d'Argent** |
| Moog lead | resonant saw/PD lead, glide | La Femme melody, Sexy hook |
| tenor sax | smoky, breathy, late | **Playground Love** |
| nylon/acoustic guitar | fingerpicked arpeggios | **Cherry Blossom Girl** |
| flute / recorder | airy, pure | Cherry Blossom Girl |
| vibraphone | glassy twinkle | Cherry / ambient beds |
| vocoder voice | robotic formant lead | **Kelly Watch the Stars**, Sexy whisper |
| drums | soft CR-78 swish, claps on 2&4, brushed ballad kit | all |

## The five song archetypes (the dial)

Each FIXES its lead voice, groove, tempo, key-mood and form; the seed varies key/patterns
*within* the archetype, so "the Sexy Boy one" sounds different every time but always like
Sexy Boy. (Chord templates encoded as degree/quality rows, stolen-playbook brain #4.)

1. **SEXY** — "Sexy Boy" · ~112 BPM · minor, moody · laid-back four-on-floor + claps ·
   STAR = the fuzzy saturated Moog bass riff · descending Moog hook · Solina chord wash ·
   vocoder accent. Form: intro → riff-verse → hook → break.
   Chords (minor): i–bVI–bVII–i / i–v–bVI–bVII feel.

2. **ARGENT** — "La Femme d'Argent" · ~95 BPM · major, sunny/jazzy · relaxed groove ·
   STAR = the rolling melodic bassline · Rhodes comping · Moog lead melody · organ-ish.
   Long terraced instrumental. Chords: jazzy major loop, e.g. Imaj7–vi7–ii7–V or
   I–IV–ii–V with passing changes.

3. **PLAYGROUND** — "Playground Love" · ~75 BPM · minor/jazzy noir · slow lounge ballad ·
   brushed kit · STAR = smoky tenor sax lead · Rhodes · soft walking-ish bass ·
   descending chromatic-leaning changes. Torch song.
   Chords: i–(chromatic descent)–bVII–bVI–V kind of fall.

4. **CHERRY** — "Cherry Blossom Girl" · ~100 BPM · major, airy organic · gentle folk-pop ·
   soft kit · STAR = fingerpicked nylon guitar arpeggios · flute line · soft strings ·
   vibe twinkle · breathy. Chords: bright diatonic major, I–V–vi–IV / I–iii–IV–V.

5. **KELLY** — "Kelly Watch the Stars" · ~120 BPM · minor, robotic · driving four-on-floor ·
   STAR = vocoder robotic lead · punchy synth bass · bright saw stabs · Solina.
   Chords: insistent minor vamp, i–bVII–bVI–bVII.

## Phase 2 — palette shop (gaps = new recipes)

| voice | engine / recipe | source | new? |
|---|---|---|---|
| Solina string pad | SAW ensemble, slow attack, detuned pair, LP — **new recipe** (saw/solina) | builds on ambient's saw pad | NEW recipe |
| Rhodes/Wurli | `INSTR_FM` 1:1 detent + LFO_VOLUME tremolo (the station epiano — EPIANO is parked) | fm/epiano, yacht's fm/rhodes | reuse engine |
| fuzzy Moog bass | SAW + `instrument_drive` + resonant LP — **new recipe** (saw/fuzz-moog-bass) | tb303 path / pd/cz-bass | NEW recipe |
| melodic bass | SAW/SINE + LP, round — loopstation `saw/bass`-ish | loopstation | reuse |
| Moog lead | `INSTR_PD` reso-lead / `saw` reso + glide | pd/reso-lead | reuse engine |
| tenor sax | `INSTR_REED` reed/tenor_sax (h0.82 t0.30 m0.62) | reed.c — **first station sax** | NEW radio use |
| nylon guitar | `INSTR_GUITAR` guitar/nylon (untapped shelf) | guitar.c | NEW radio use |
| flute | `INSTR_PIPE` pipe/flute (NO station uses PIPE) | pipe.c | NEW radio use |
| vibraphone | `INSTR_MALLET` mallet/vibraphone | mallet.c (shared) | reuse |
| vocoder voice | `INSTR_VOICE` formant (NO station uses VOICE) | voxlab voice/sing | NEW radio use — highest-value gap |
| kit | soft CR-78-ish / claps / brushed — raw waves | cr78.c, loopstation | reuse |

**Effects wanted we lack:** reverb (everything is drenched), chorus/Solina-ensemble
detune (faked with detuned saw pair), tape saturation. Note, don't fake badly.

## Brain picks (Phase 4)
- **chord brain #4 — stolen playbook**: 5 template progressions, one per archetype, rolled.
- **time feel**: per-archetype groove (rolled by archetype, not free): 4-floor laid-back /
  slow funk / brushed ballad / gentle folk 8ths / driving 4-floor electro. Coupled tempo.
- **melody**: re-pitched cell per archetype, on the archetype's lead voice. (improv optional.)
- **bass**: melodic/voice-led, with the fuzzy-riff variant for SEXY/KELLY.
- **form**: rolled per archetype (terraced layers + density curve via rad_level).
- **timbre roll**: within archetype, vary register/filter/envelope so two SEXYs differ.

Untapped engines reached: **GUITAR, PIPE, VOICE** (+ first station tenor sax on REED).
