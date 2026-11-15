# Sung lyrics: type words, the voice sings them

STATUS: BUILDING (2026-11-15) — `runtime/lyric.h` reads **Dutch** by spelling rules and sings it on
`INSTR_VOICE`, one syllable per note, plus **Mipomi** (the playtime Mipos' language), **Japanese**
(romaji), **Spanish**, **Swahili** and **Hawaiian**; the `zingen` cart is the demo (three Dutch lyrics, "Fala, Foni",
a Mipomi lullaby on a written melody, one song in each new language) and runs
`lyr_selfcheck()` (23 Dutch words, 3 Mipomi phrases, 17 Japanese/Spanish/Swahili/Hawaiian, `node tools/spec.js zingen`). Voice knobs in zingen (SIZE,
BUZZ, VIBRATO, GLIDE, LIFE, PITCH, WHISPER, NASAL, GRAVEL + DEFAULT, and one-tap CHARACTERS: CLEAN CHIPMUNK MONSTER ROBOT ALIEN PHONE BULLHORN JAZZ (gravel + a scoop into notes), each = knob settings
+ its own slot effects (filter, drive, ring mod, flanger) + a level that evens them out; `singer_size`/`singer_buzz` + the Singer fields). Type your own line
in zingen (ENTER, TAB = Dutch/Mipomi, ENTER again: it sings it; remembered with `save_bytes`).

## Where it came from

An A/B against **JIANT FM**, an alternative firmware for the M-VAVE FM-1 (GPL-3.0, read for ideas
only, no code). Its VOICE engine is a Klatt cascade with a WORDS layer: per-language letter rules
(Spanish by rule, Japanese romaji, English via a CMU-dictionary subset), syllables advanced by the key
gate, codas sung at release. Hearing it next to ours led first to the de-hiss pass
([voice-engine.md](voice-engine.md) decision log, 2026-11-13) and then to this layer. The `singab`
cart is the A/B harness (same lyric, melody and timing as JIANT's own host render).

## The shape (what lyric.h does)

1. **Read** (`lyr_set`): text → words → tokens (a consonant or a vowel, spelling-aware) →
   syllables: onset consonants, one vowel, coda consonants, plus the letters for display.
2. **Sing** (`Singer`): `singer_on` starts the next syllable on a new note. The first onset consonant
   fires with the note (`voice_consonant`), the rest of a cluster one after another (re-firing
   `voice_consonant` mid-note). The vowel holds while the key does. `singer_off` sings the TAIL: a
   diphthong's second half (F1/F2 glide, 90 ms), then each coda (`voice_coda`; the second one CHAINS
   from the first in the engine, so "nacht" never flashes the vowel between x and t), and only after
   the last consonant has sounded does it call `note_off`. That hold-then-release is JIANT's "the coda
   at the level the note had" idea, done from cart land.
3. **Vowels as formant targets.** Each Dutch vowel is an F1/F2 pair (adult male, in the range of
   Pols, Tromp & Plomp 1973 and Adank et al. 2004). `INSTR_VOICE` has only a five-vowel path, so the
   singer picks the nearest base vowel on that path and places F1/F2 with the openness/frontness
   levers (`note_aux` 8/9). That is how the front rounded vowels (uu, eu, ui) exist at all. One base
   per diphthong keeps its glide smooth (only the levers move).

The engine pieces it needed (2026-11-13): consonants **h** (breath through the vowel's own formants)
and **x** (the Dutch g/ch), ids 22-23, and chained codas (`vox_coda_prev`). The Singer sings through the engine's **Klatt
cascade** formant model (`note_aux` 19, chosen over the parallel one by ear; `Singer.parallel = 1` for the old
one), and high notes get F1-follows-pitch ([voice-engine.md](voice-engine.md) decision log).
**The singer layer** (after JIANT FM's, ideas only), on by default: a note starts STRAIGHT and its
vibrato blooms in (`vib` depth 0.3, `vib_hz` 5.5, after `vib_delay` 0.25 s, over 0.4 s); a living pitch
(`human`: per-period jitter ±0.3 %, shimmer up to 3 %); legato notes (starting within 0.12 s of the last
release) begin on the last pitch and slide on an S-curve (`glide` 0.07 s, +6 % per semitone), driven per
frame with `note_pitch` and a one-frame `note_glide` so it never staircases. Zero a field to switch that
part off (zingen: L A/Bs the whole layer).
`singer_buzz(&sing, 0..1)` sets the voice's EFFORT, in the cascade its BUZZ: relaxed and round to pressed and
bright (zingen: UP/DOWN).

## The Dutch rules

| rule | example |
|---|---|
| digraphs read first: aa ee oo uu ie oe eu ui ij/ei au/ou, ch ng nk sch sj tj, aai ooi oei eeuw ieuw | huis → huis, mijn → mein |
| a single vowel letter is LONG in an open syllable, short in a closed one | ma-ken → maa.k@, man-nen → mAn.n@ |
| ch / ng / nk / x keep the vowel before them short (spelling never doubles them) | lachen → lA.x@ |
| syllables: one consonant between vowels opens the next; two split unless they can start one (obstruent + l/r, kw tw zw dw); ng never opens one | zingen → zIng.@, vergeten → v@r.xee.t@ |
| an unstressed single e is a schwa; stress = first syllable after be- ge- ver- te- her- ont- (a prefix only when the word is long or its last vowel is not a plain e) | geluk → x@.lUk, beter → bee.t@r |
| function words de je me we ze te ge een: schwa | de zee → d@ zee |
| -ig and -lijk reduced | eeuwig → ee.w@x, vrolijk → vroo.l@k |
| final devoicing, and a doubled coda consonant once | hond → hOnt, wordt → wOrt |
| sch = s + x, but word-final -isch / -ische = s | school → sxool, praktisch → prAk.tIs |
| the final n of -en after a schwa is dropped, as sung (`lyr.keep_en = 1` keeps it) | maken → maa.k@ |
| an accent marks the stress; ë ï ö ü split a vowel pair | café → kaa.fee |
| a lone "-" holds the previous vowel one more note; the coda waits for the last | zo - ver |

Known misses (no dictionary): words where be-/ge- is a prefix but the last vowel is a plain e
(gebed is read as gee.bEt), loanwords (computer), and stress in compounds. Dutch w uses the engine's
w glide (an English-ish w; Dutch has a labiodental ʋ) and r the approximant (Dutch r varies
anyway: tap, trill, uvular, the Gooise r).

## Japanese, Spanish, Swahili (2026-11-15)

One reader (`lyr__rule_word`) for three languages whose spelling is (nearly) phonemic: letters become
tokens (a vowel, or the consonant(s) a grapheme stands for), every vowel opens a syllable, and the
consonants between two vowels split by the language's rule. New pure vowels: e (mid, no glide), the
unrounded Japanese u ("u'"), and the Spanish diphthongs ai / oi.

| language | rules | example |
|---|---|---|
| Japanese (romaji, `LYR_JA`) | every mora a note (saite = sa.i.te); shi chi tsu, j = d+zh; the moraic n and the small tsu (a doubled consonant) close the syllable before | konnichiwa → kon.nie.chie.waa, kitte → kiet.te |
| Spanish (`LYR_ES`) | ch ll ñ qu gu/gü, c/g soft before e i (seseo, jota), h silent; rr and a word-initial r = three quick d taps (a trill); an unaccented i/u before a vowel is a glide, after a/e/o a diphthong; y alone or final = i; consonant + l/r or + a glide starts a syllable | corazón → ko.raa.son, perro → pe.dddo, agua → aa.gwaa |
| Hawaiian (`LYR_HAW`, 2026-11-15) | h k l m n p w, every syllable open; ai au ei oi ou on one note; macrons (ā ē ī ō ū, UTF-8) = a long vowel that never merges; the okina (ʻ, or a typed apostrophe) = a break, so Hawaiʻi = haa.wai.ie; w sounds as v after i/e | aloha → aa.lo.haa, mākou → maa.koo, ewa → e.vaa |
| Swahili (`LYR_SW`) | every syllable open; mb nd ng nj prenasalized onsets; ng' (the apostrophe is kept), ny, ch sh dh th gh kh; every vowel its own note (kaa = ka.a) | jambo → dzhaa.mbo, ng'ombe → ngo.mbe |

Approximations the voice forces: no dʒ (built as d + zh), no real trill (taps), the single r is the
engine's approximant. zingen: songs 5-8 (our own lines), TAB in the text box cycles all six languages.

## Scat (zingen song 9)

Not a language: a generator (`scat_gen` in zingen) writes a PLAN of one-syllable scat words (ba be di
du ski wa ya ... landing on bop / dop / dat), each with a duration in ticks (3 frames = 50 ms), a pitch on
a G blues scale (G3..D5: higher runs hot under F1 tracking) and a breath after. Five phrase kinds:
SWING (a swung long-short run), STUTTER (one syllable fired fast on one pitch, "ba ba ba ba bop"), ECHO
(the phrase before, a step higher: call and response), HOLD (two pickups and a long held "yeh" where the
vibrato blooms), ROLL ("bi di bi di", even and climbing). The words are read by the Spanish rules (one
vowel each: one word = one note), fresh phrases each time round. THE BAND (B): a walking pizzicato upright
on a 12-bar blues in G (the upright.c recipe) and finger snaps on 2 and 4, on the same tick clock with its
own dice (so switching it off keeps the phrases), ~10 dB under the voice. The JAZZ voice suits it.
Every phrase LANDS (2026-11-15): before it starts, its length is summed and it waits so its last note
falls on a strong beat of the 12-bar (the next downbeat if that is within 8 ticks, else beat 3), and that
note is moved to the nearest chord tone (1 3 5 b7 of the bar's G7 / C7 / D7). Traced over a minute: 45 of
45 landings on beat 1 or 3 (27 downbeats), 45 of 45 on chord tones. T = HALF TIME (the default): the tick runs at
6 frames instead of 3, so phrases, band and landings slow together (bass notes ring a beat long).

## Mipomi

`LYR_MIPOMI` reads the Mipos' language (`~/Projects/love/mipomi-lang/RULES.md`, words in `words.tsv`):
every syllable is one consonant + a vowel (m b p f k l t j n d s h w; a i o, + u "with"), so each vowel
opens a syllable and the consonants before it are its onset. Vowels are pure (a = Dutch aa, i = ie,
o = a round o with no glide, u = oe). The grammar's melting aspect vowel ("mi i mola" said "mii mola")
needs no rule: "i" is an onsetless syllable on the same vowel, which is that vowel held on the next
note. The paragrammar's lone whispered consonant after a sentence is not sung yet (a syllable needs a
vowel).

## Next

- **By ear first**: are the vowel targets right (uu/eu/ui are approximations; F3 is not moved), are
  clusters intelligible (str, schr), does the coda timing feel sung or clipped.
- More languages: Maori (the macron path is already in), Yoruba (tonal: the accent marks could
  steer the melody), Italian; English needs a dictionary. Click languages (Zulu, Xhosa) need a click the
  voice cannot make yet.
- A lyric generator (JIANT's templates + themed word banks, sized to the pattern's note count).
