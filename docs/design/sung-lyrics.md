# Sung lyrics: type words, the voice sings them

STATUS: BUILDING (2026-11-13) — `runtime/lyric.h` reads **Dutch** by spelling rules and sings it on
`INSTR_VOICE`, one syllable per note; the `zingen` cart is the demo and runs `lyr_selfcheck()`
(23 Dutch words, `node tools/spec.js zingen`). Other languages: not yet.

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
and **x** (the Dutch g/ch), ids 22-23, and chained codas (`vox_coda_prev`).

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

## Next

- **By ear first**: are the vowel targets right (uu/eu/ui are approximations; F3 is not moved), are
  clusters intelligible (str, schr), does the coda timing feel sung or clipped.
- More languages through the same `lang` switch: Spanish is the cheapest (pure rules, JIANT shows the
  rule set), English needs a dictionary.
- A lyric generator (JIANT's templates + themed word banks, sized to the pattern's note count).
- A typed-text input in a cart, so a stranger can make it sing their own words.
