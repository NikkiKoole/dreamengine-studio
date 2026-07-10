# Polo & Pan radio — the blind band brief (Phase 1)

STATUS: SHIPPED — the polopan.c radio station is built (mallet/pizz/flute/vocoder band, tropical house).
The engine shopping it triggered: [`polopan-effects-wants.md`](polopan-effects-wants.md) (all shipped, all wired).

Written *before* opening any cousin cart (radio-station-howto.md firewall). Polo & Pan is an
**artist station**, not a genre: the dial must play recognizably DIFFERENT *songs of theirs*,
not one texture re-keyed. So this brief names actual tracks and what makes each one *itself*;
those become the **song archetypes** rolled in Phase 4 (stolen-playbook chord brain #4 +
bundled groove/form/pocket/tempo/lead). Reference for the artist-station discipline: `jingle.c`
(six cited Mac DeMarco charts), `citypop.c`, `yacht.c`.

## The band, from the music (not the palette)

Polo & Pan (Paul Armand-Delille + Alexandre Grynszpan) — French dream-pop / tropical-house
escapism built on a **mallet-and-percussion orchestra** (marimba, balafon, metallophone,
vibraphone, glockenspiel, kalimba), **pizzicato strings** that bounce the groove, **breathy
flutes & recorders**, **vocoded/filtered multilingual vocal hooks** (the "ah-ah" chant), a
**fat resonant analog mono-synth bass** (their Korg MS-10 — the *Tunnel* bass), lush Solina-ish
**string pads**, bright **arpeggiated synths** (a sampled Roland D-50), an **eerie glassy lead**
(ondes Martenot / Cristal Baschet / wine-glasses), and a **breezy Balearic house kit** —
four-on-the-floor kick, claps, soft swishy hats, shaker, hand percussion (congas/bongos),
woodblock/clave. Everything sunny, hypnotic, drenched in reverb.

The ideal lineup (intent-first):

| role | ideal voice | the Polo & Pan reference |
|---|---|---|
| marimba / mallet lead | warm wooden struck bar, stacked in octaves | **Canopée** outro, all over Caravelle |
| balafon / bright mallet | harder, woodier African bar | Nanga, Mexico |
| glockenspiel / celeste | bright twinkle on changes | Coeur Croisé, Dorothy |
| vibraphone | glassy motor shimmer | dreamy beds |
| pizzicato strings | plucked bounce that *is* the groove | **Canopée** bounce |
| flute / recorder | airy, pure, hypnotic | **Nanga** lead |
| vocoder voice | robotic formant chant ("ah-ah", multilingual) | **Canopée**, **Ani Kuni** chant |
| analog synth bass | fat resonant MS-10 mono, propulsive | **Tunnel** riff |
| Solina string pad | warm detuned-saw wash | **Coeur Croisé** |
| bright arp synth | sampled-D-50 16th arpeggio | Ani Kuni build |
| glassy lead | ondes/Cristal/glass — eerie, pure | Dorothy, Mexico |
| kit | breezy 4-floor + claps + swish hats + shaker + congas | all |

## The five song archetypes (the dial)

Each FIXES its lead voice, groove, tempo, key-mood and form; the seed varies key/patterns
*within* the archetype, so "the Canopée one" sounds different every time but always like
Canopée. (Chord templates encoded as degree/quality rows, stolen-playbook brain #4.)

1. **CANOPÉE** — "Canopée" · ~115 BPM · major, sunny tropical · breezy four-on-floor + shaker ·
   STAR = the bouncing **pizzicato + marimba** riff, doubled · vocoder "ah" accents · lush pad.
   Form: intro → pluck-verse → mallet-hook → the famous **stacked-mallet outro** (mallets pile
   up in octaves). Chords (bright major): I–V–vi–IV / I–iii–IV–V feel.

2. **ANI KUNI** — "Ani Kuni" · ~122 BPM · modal/minor, hypnotic · driving Balearic four-on-floor
   + claps · STAR = a **repeating chant cell** (vocoder voice + glockenspiel doubling), bright
   **arp synth** build · big terraced drop. Form: long build → drop → strip-back → rebuild.
   Chords: insistent modal vamp, i–bVII / i–bVII–bVI–bVII.

3. **NANGA** — "Nanga" · ~100 BPM · dreamy, exotic, downtempo · loping groove + congas/bongos +
   shaker · STAR = a **breathy flute** lead over **balafon/marimba** + hand percussion · no big
   drop, hypnotic A/B wander. Major-with-flat-7 / dorian colour. Chords: modal two-chord
   wander, I–bVII / i–IV (dorian).

4. **TUNNEL** — "Tunnel" · ~120 BPM · minor, dark, propulsive · tight four-on-floor, minimal ·
   STAR = the **fat resonant MS-10 synth-bass riff** (the song *is* the bass) · sparse glassy
   lead stabs · dark pad. Form: driving, terraced add/strip, no melodic chorus. Chords: minor
   one/two-chord drive, i / i–bVI.

5. **COEUR CROISÉ** — "Coeur Croisé" · ~108 BPM · warm major, romantic · gentle mid-tempo groove
   · STAR = a **lush Solina pad wash** + a **sung wordless melodic lead** (sine/voice) ·
   glockenspiel twinkle · pizzicato comp. Form: verse → chorus → verse song shape. Chords:
   warm diatonic, I–vi–IV–V / vi–IV–I–V.

## Phase 2 — palette shop (gaps = new recipes)

| voice | engine / recipe | source | new? |
|---|---|---|---|
| marimba | `INSTR_MALLET` `mallet/marimba` (h0.00 t0.45 m0.35) | mallet.c | **first station MALLET marimba** |
| balafon | `INSTR_MALLET` — harder/woodier bar — **new recipe** (`mallet/balafon`, h0.10 t0.72 m0.18) | builds on mallet.c | NEW recipe |
| glockenspiel | `INSTR_MALLET` `mallet/glockenspiel` (h0.90 t0.85 m0.60) | mallet.c | reuse |
| vibraphone | `INSTR_MALLET` `mallet/vibes` (h0.25 t0.50 m0.90, shared) | mallet.c | reuse |
| pizzicato strings | `INSTR_BOWED` `bowed/pizzicato` (h0.30 t0.42, friction off) | bowed.c | **NEW radio use — no station uses BOWED** |
| flute | `INSTR_PIPE` `pipe/flute` (h0.00 t0.38 m0.70) | pipe.c | **NEW radio use — no station uses PIPE** |
| vocoder voice | `INSTR_VOICE` `voice/sing` (vibrato-rich formant) | voxlab.c | **NEW radio use — no station uses VOICE; highest-value gap** |
| MS-10 synth bass | `INSTR_SAW` resonant LP + cut-env "wow" + drive — **new recipe** (`saw/ms10-bass`) | builds on tb303 / `saw/fat-moog` path | NEW recipe |
| Solina pad | `INSTR_SAW` `saw/string-machine` (long attack, LP ~900) | shared pad pile | reuse |
| bright arp | `INSTR_PLUCK` high KS arp, `pluck/seq-arp`-ish (A0 D200 S0 R120 LP3600 h0.45 t0.7) | italo | reuse |
| glassy lead | `INSTR_SINE` `sine/glass` (A220 D200 S7 R900 · pitch-LFO 5/0.12 · vol-LFO 0.7/0.18) | glassharmonica.c | reuse |
| kick | `drum/synth-kick` (SINE, pitch-env thump) | shared | reuse |
| clap | `drum/house-clap` (NOISE BP1100/5) | house | reuse |
| shaker | `noise/caxixi` (NOISE HP, soft attack) | bossa | reuse |
| hats | `drum/noise-hat` (HP-noise tick, S2 wash variant for the breezy feel) | shared | reuse |
| congas/bongos | `membrane/conga`/`bongo` (tabla.c MEMBRANE) | addis | reuse |
| woodblock/clave | `cr78/claves` (TRI ping LP) | cr78.c | reuse |

**Untapped engines reached: `INSTR_PIPE`, `INSTR_VOICE`, `INSTR_BOWED`** — three engines no
charted station uses — plus the first station marimba on `INSTR_MALLET`. This is exactly the
right gap for Polo & Pan: they *are* the mallet / world-percussion / vocoder / flute band, so
the most valuable thing the cart adds (the untapped shelves) is also the most genre-faithful.

**Effects wanted we lack:** big plate/hall reverb (everything P&P is drenched), tape/chorus
detune on the pads, ping-pong delay on the flute/mallet. We have a live filter cut-env (use it
for the MS-10 "wow" and breezy filter opens) and an echo send (use sparingly on the lead). Note
the missing effects; don't fake them badly. (Roster: `../design/sound-next-steps.md` §effects.)

## Brain picks (Phase 4)

- **chord brain #4 — stolen playbook**: 5 template progressions, one per archetype, rolled.
- **time feel**: per-archetype groove (rolled by archetype, not free): breezy tropical 4-floor /
  driving Balearic 4-floor / downtempo loping world / tight dark 4-floor / gentle romantic mid.
  Coupled tempo (115 / 122 / 100 / 120 / 108).
- **melody**: re-pitched cell per archetype, on the archetype's STAR lead voice (pizz+marimba /
  chant cell / flute / bass-riff / sung lead).
- **bass**: MS-10 resonant for TUNNEL/ANI KUNI; rounder for the rest.
- **form**: rolled per archetype (terraced layers + density via `rad_level`); Canopée's
  stacked-mallet outro and Ani Kuni's build→drop are form-brain business.
- **timbre roll**: within archetype, vary register/filter/mallet-macros so two CANOPÉEs differ.

Untapped engines reached: **PIPE, VOICE, BOWED** (+ first station marimba on MALLET).
