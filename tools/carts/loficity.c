/* de:meta
{
  "slug": "loficity",
  "title": "lofi city",
  "status": "active",
  "created": "2026-09-28",
  "kind": ["toy", "instrument"],
  "teaches": ["song-arrangement", "chord-voicing", "generative-melody", "swing-timing"],
  "homage": "Lofi Cities (loficities.com, Safa Elmali) - endless lofi composed live in the browser",
  "lineage": "A study port of Lofi Cities' arranger, verified bit-identical against the site's own planner (300 seeds x 4 chained tracks, every energy/band/city: 643,600 events to 9 decimals). The arrangement is theirs, line for line; the SOUND is ours - EPIANO Rhodes, a BOWED-pizz upright, the morphdrum kit + METAL hat, a MODAL rim, MALLET vibes / PIPE flute leads. Sibling of lofi.c (the radio station), which it shares no voices with.",
  "todo": [
    "The other eight Lofi Cities styles (lofi piano, ambient, bossa nova, synth city pop, lofi house, chill guitar, sad lofi, medieval) - each overrides progressions/grids/comps/voicing/bass/energy; the oracle harness can verify them the same way. Only jazzhop is ported.",
    "The 2-bar wow drift (their fx 'wow' events) is not ridden: tape() rebuilds its DSP. Per-track wow only.",
    "The kit bus lowpass (their kit.lp, 5-9.5 kHz) is not applied; the master tone filter covers most of it.",
    "Velocity is quantised to our 0..7 vol, so their +-8% humanise mostly vanishes; timing jitter survives intact."
  ],
  "description": {
    "summary": "Endless generated lofi, arranged exactly the way Lofi Cities arranges it - played on our own engines.",
    "detail": "Every track is one seed, and the seed plans everything the way loficities.com does: a key (each track moves to a related key - the fourth, the fifth, the relative major/minor), two jazzy progressions from their bank (A, and B or A with a ii-V turnaround), 1 or 2 bars per chord, a tempo/swing/snare-lag from the ENERGY table, a drum groove (boom, bounce, lazy, half, rim, shuf), a form (T1-T3, grown to 2.5-4 minutes) whose every section switches layers on and off - hats-only intros, drumless breaks, lead entering at the second A, an outro that holds the last chord. Per bar: fills at 4- and 8-bar boundaries, B-section open hats and ghost snares, the keys PUSHING the next chord onto the and-of-4, the bass walking the kicks with chromatic approaches, a motif lead inverted and shifted in B, and the master tone filter automated (the intro opening from 900 Hz, the break dipping, the outro closing). The screen is the arrangement made visible: the form strip with the playhead, what each part is doing in this section, the chord, fills and pushes as they happen, and what's up next.",
    "controls": "N / SPACE next track . E energy (chill/balanced/upbeat, from the next track) . B band (full / no drums / chords only, from the next bar) . C city (the words the titles are made of) . H help"
  }
}
de:meta */
// ── LOFI CITY ─────────────────────────────────────────────────────────────────
// Lofi Cities' arranger, ported line for line, played on dreamengine's engines.
//
// The ARRANGEMENT half (planTrack / planBar / barHits / phrase / voice) is a port of
// the site's own JavaScript — same RNG (their hash + rng, as uint32), the same five
// seeded streams per track and the same draw ORDER, so a seed plans the same track the
// site would. Verified bit-identical against their bundle run headless in node
// (300 seeds × 4 chained tracks × every energy/band/city, 643,600 events to 9 decimals).
// Build with -DLC_DUMP for a standalone binary that prints every planned event, which is
// how that check was made (the site's code itself is not in this repo).
// The SOUND half is ours: see "THE BAND" below.
//
// Our clock is the SOUND clock, audio_time(), and every note is booked on it with schedule_at()
// 100 ms ahead (their lookahead). Not beat() or summed dt(): those advance by the clamped frame dt, and
// a schedule_hit delay counts from whichever audio callback drains it, which swung notes by up to a
// 23 ms buffer on native (gated by tools/schedule-check).
//
//   N / SPACE next   E energy   B band   C city   H help

#define LOFI_SEED 0      // pin a seed (0 = a random one each boot)
#define LC_WOW 0.5f      // tape wow scale: 1 = their depth (+-3..12 cents at ~0.5 Hz), which read as seasick
#define LC_FLUTTER 0.03f // tape flutter: ~1.6 cents at 6 Hz, their depth (0.12 was 4x that - an audible warble)
#define LC_SAT 0.0f      // tape saturation: OFF. tape()'s curve is tanh(g*x)/tanh(g), normalised so full scale stays full
                         // scale, so even 0.02 has +2.5 dB small-signal gain and bends the whole range: the drums drove it
                         // into squashing the Rhodes/bass on every hit (mix vs sum-of-parts residual -3.7 dB; at 0: -44 dB)
#define LC_GLUE 0.25f    // bus compressor amount
#define LC_TREM 0.5f     // the Rhodes suitcase tremolo + autopan, scaled from the plan's depth (1 = theirs; it read as
                         // a sine wobble on the whole mix because the keys are the loudest part)
#ifndef LC_DUMP
#include "studio.h"
#include "ui.h"
#include "morphdrum.h"
#ifdef DE_SPEC
#include "spec.h"
#endif
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

// ── the title words (their CITY_WORDS / NOUNS / ADJS / TIMES / TEMPLATES, accents folded to ASCII 1:1) ──
static const char *LC_NOUNS[] = { "lullaby", "daydream", "reverie", "echoes", "letters", "polaroids", "headlights", "streetlights", "reflections", "window seat", "slow dance", "night shift", "coffee", "cassettes", "raindrops", "soft focus", "old photographs", "paper moons", "radio static", "afterglow", "small talk", "notebooks", "warm static", "long walks", "missed calls", "pocket change", "quiet hours", "love letters", "city lights", "sleepwalking", "goodnights", "thoughts", "memories", "footsteps", "secrets", "window light", "tape hiss", "slow motion", "homework", "blue notes", "moonlight", "daydreams", "postcards", "lamplight", "rain songs", "night walks", "loose ends", "mixtapes", "late buses", "open windows", "paper boats", "half-light", "sketches", "tangerines", "umbrellas", "wishes", "waltz", "nocturne", "cigarettes", "fireflies" };
static const char *LC_ADJS[] = { "sleepy", "velvet", "hazy", "quiet", "dusty", "faded", "soft", "slow", "warm", "blue", "lonely", "sleepless", "gentle", "grainy", "amber", "misty", "drowsy", "rainy", "humming", "flickering", "distant", "mellow", "tired", "golden", "pale", "silver", "wistful", "lazy", "secret", "little", "late", "empty", "hushed", "lost", "tender", "dreamy", "foggy", "mossy", "cozy", "moonlit", "neon", "rainy-day", "nostalgic", "unhurried", "pastel", "second-hand", "far-off", "dim", "lo-fi" };
static const char *LC_TIMES[] = { "3am", "2am", "midnight", "after hours", "closing time", "the blue hour", "dawn", "before sunrise", "late night", "sunday night", "4am", "the small hours", "half past one", "nightfall", "last call" };
static const struct { double w; const char *t; } LC_TPL[] = {
    { 10, "{a} {n} in {p}" },
    { 6, "{a} {c} in {p}" },
    { 4, "{a} {n}, {p}" },
    { 3, "{n} over {p}" },
    { 2, "last {c}, {p}" },
    { 2, "{a} {n}" },
    { 2, "{a} {c}" },
    { 1.5, "{p} at {t}" },
    { 1, "{t}, {p}" },
    { 1.5, "{c} at {t}" },
    { 2, "{n} in {p}" },
    { 1.5, "{p} {n}" },
    { 2, "{c} & {n}" },
    { 1.5, "the {a} {c}" },
    { 1, "{c} in {R}" },
    { 0.8, "{w} over {p}" },
    { 0.8, "{w} on {p}" },
    { 0.4, "{p} after dark" },
    { 0.2, "walking {p}" },
    { 0.2, "somewhere in {p}" },
    { 1.5, "{P}" },
    { 1.2, "{c}, {t}" },
    { 0.6, "{Q}" },
    { 1.5, "{n} from {p}" },
    { 2, "{P} at {t}" },
};
#define LC_NCITY 16
typedef struct { const char *id, *name; const char *p[16]; int np; const char *c[24]; int nc; const char *w[5]; int nw; } LcCity;
static const LcCity LC_CITY[LC_NCITY] = {
    { "paris", "Paris",
      { "montmartre", "the seine", "saint-germain", "le marais", "pigalle", "belleville", "the left bank", "pont neuf", "the quais", "canal saint-martin", "ile saint-louis", "the latin quarter", "rue mouffetard", "batignolles", "bastille", "the tuileries" }, 16,
      { "cafe", "accordion", "balcony", "zinc bar", "bookstall", "carousel", "pigeons", "lamplight", "chanson", "bistro", "street piano", "postcard", "croissant", "rooftops", "metro", "boat lights", "espresso", "cobblestones", "shutters", "violin" }, 20,
      { "rain", "drizzle", "puddles", "wet stone", "grey skies" }, 5 },
    { "tokyo", "Tokyo",
      { "shinjuku", "shibuya", "koenji", "shimokitazawa", "golden gai", "akihabara", "nakameguro", "the yamanote", "ginza", "asakusa", "harajuku", "ebisu", "the sumida", "ikebukuro", "kichijoji", "yanaka" }, 16,
      { "vending machine", "konbini", "neon", "umbrellas", "ramen stall", "capsule hotel", "izakaya", "cassette", "lanterns", "crosswalk", "arcade", "noodle bar", "last train", "paper cranes", "taxi lights", "rooftop", "canned coffee", "train window", "shrine bell", "city pop" }, 20,
      { "neon rain", "rain", "drizzle", "wet asphalt", "puddles" }, 5 },
    { "new-york", "New York",
      { "brooklyn", "the bowery", "harlem", "soho", "the village", "queens", "coney island", "chinatown", "the high line", "tribeca", "williamsburg", "astoria", "the east river", "grand central", "little italy", "the lower east side" }, 16,
      { "fire escape", "yellow cab", "bodega", "subway car", "steam vents", "diner", "jazz club", "water tower", "bagels", "streetlamp", "laundromat", "walk-up", "hot dog cart", "rooftop", "late train", "radiator", "crosswalk", "neon sign", "saxophone", "skyline" }, 20,
      { "rain", "steam", "sleet", "city rain", "puddles" }, 5 },
    { "london", "London",
      { "soho", "camden", "the embankment", "brixton", "shoreditch", "hackney", "the thames", "notting hill", "peckham", "covent garden", "king's cross", "the southbank", "primrose hill", "whitechapel", "bermondsey", "waterloo" }, 16,
      { "night bus", "pub", "phone box", "tube", "chip shop", "umbrella", "bookshop", "double-decker", "last orders", "record shop", "lamppost", "canal boat", "big clock", "tea", "cab lights", "bridges", "terraces", "pint glass", "underpass", "foxes" }, 20,
      { "fog", "drizzle", "rain", "grey skies", "mist" }, 5 },
    { "rio", "Rio de Janeiro",
      { "copacabana", "ipanema", "lapa", "santa teresa", "leblon", "botafogo", "sugarloaf", "the lagoon", "urca", "tijuca", "flamengo", "arpoador", "corcovado", "gavea", "the boardwalk", "the favela" }, 16,
      { "samba", "bossa nova", "cable car", "coconuts", "surfboard", "caipirinha", "hill lights", "mosaic", "tram", "hammock", "beach bar", "cavaquinho", "palms", "kiosk", "sandals", "guitar", "lighthouse", "kites", "mango", "swimsuits" }, 20,
      { "warm rain", "sea breeze", "waves", "the tide", "salt air" }, 5 },
    { "istanbul", "Istanbul",
      { "the bosphorus", "galata", "karakoy", "kadikoy", "beyoglu", "balat", "uskudar", "eminonu", "the golden horn", "cihangir", "moda", "ortakoy", "sultanahmet", "the bazaar", "the old city", "the ferry pier" }, 16,
      { "ferry", "simit", "tea glass", "minaret", "lanterns", "street cats", "hammam", "carpets", "tram", "gulls", "backgammon", "cay", "spice stall", "rooftop", "oud", "fishing lines", "bridge lights", "mosaic", "coffee cups", "call to prayer" }, 20,
      { "sea breeze", "rain", "drizzle", "harbour mist", "the current" }, 5 },
    { "hong-kong", "Hong Kong",
      { "victoria harbour", "kowloon", "tsim sha tsui", "mong kok", "central", "wan chai", "sheung wan", "the peak", "yau ma tei", "sham shui po", "causeway bay", "lan kwai fong", "temple street", "the ferry pier", "north point", "jordan" }, 16,
      { "harbour ferry", "neon signs", "junk boat", "dim sum", "milk tea", "egg tarts", "the peak tram", "mahjong", "bamboo poles", "laundry lines", "rooftops", "dai pai dong", "minibus", "ding ding tram", "light show", "harbour lights", "wonton noodles", "pineapple buns", "night market", "water tanks" }, 20,
      { "drizzle", "harbour haze", "warm rain", "humid night", "sea mist" }, 5 },
    { "sydney", "Sydney",
      { "circular quay", "the rocks", "kirribilli", "farm cove", "bennelong point", "milsons point", "barangaroo", "darling harbour", "woolloomooloo", "mrs macquarie's chair", "the botanic garden", "manly", "the heads", "surry hills", "bondi", "luna park" }, 16,
      { "jacaranda", "ferry", "harbour bridge", "opera house", "sails", "water taxi", "flying foxes", "possum", "agapanthus", "bridge climb", "train", "wharf", "park bench", "lamp post", "sandstone", "cockatoo", "lorikeet", "fig tree", "flat white", "channel marker" }, 20,
      { "heat lightning", "summer storm", "humid air", "harbour breeze", "spring rain" }, 5 },
    { "san-francisco", "San Francisco",
      { "russian hill", "hyde street", "lombard street", "north beach", "the presidio", "fort point", "aquatic park", "the marina", "telegraph hill", "nob hill", "crissy field", "sausalito", "the embarcadero", "ocean beach", "the mission", "chinatown" }, 16,
      { "cable car", "foghorn", "painted ladies", "bay window", "sea lions", "lighthouse", "container ship", "sailboat", "sourdough", "the bell", "orange towers", "steep streets", "tall ship", "turret", "streetlamp", "the pier", "headlands", "night ferry", "coffee", "rooftops" }, 20,
      { "fog", "drizzle", "marine layer", "sea breeze", "mist" }, 5 },
    { "hamburg", "Hamburg",
      { "hafencity", "the speicherstadt", "st. pauli", "altona", "the elbe", "the reeperbahn", "ottensen", "the schanze", "the alster", "blankenese", "the fish market", "wilhelmsburg", "eppendorf", "kehrwieder", "the michel", "finkenwerder" }, 16,
      { "container ship", "gantry crane", "harbour ferry", "barkasse", "foghorn", "gulls", "red brick", "copper roofs", "iron bridge", "bollard", "lifebuoy", "oilskin", "lantern", "harbour seal", "glass crown", "canal", "warehouse", "fish roll", "ship's bell", "deck lights" }, 20,
      { "rain", "gusts", "squalls", "storm", "grey skies" }, 5 },
    { "amsterdam", "Amsterdam",
      { "the jordaan", "prinsengracht", "keizersgracht", "herengracht", "the westerkerk", "the nine streets", "de pijp", "bloemgracht", "brouwersgracht", "the amstel", "oud-west", "vondelpark", "haarlemmerdijk", "the ij", "noord", "leidseplein" }, 16,
      { "houseboat", "canal bridge", "bridge lights", "brown cafe", "gables", "hoisting beam", "carillon", "bicycle bell", "cargo bike", "elm seeds", "grey heron", "canal lamps", "window plants", "moored boats", "steep stairs", "bollards", "lantern boat", "coots", "roof garden", "stroopwafel" }, 20,
      { "clear skies", "spring night", "canal mist", "mild breeze", "moonlight" }, 5 },
    { "dubai", "Dubai",
      { "downtown", "the burj lake", "the fountain boardwalk", "sheikh zayed road", "al fahidi", "the creek", "deira", "bur dubai", "jumeirah", "business bay", "the souk", "karama", "satwa", "la mer", "the marina", "al seef" }, 16,
      { "fountain jets", "silver spire", "aviation lights", "led facade", "abra", "metro train", "wind towers", "arcade lamps", "brass lanterns", "dallah", "cardamom coffee", "finjan cups", "dates", "karak tea", "sadu cushions", "shisha", "oud smoke", "date palms", "fairy lights", "street cat", "sand dunes", "camel caravan", "desert campfire" }, 23,
      { "desert breeze", "winter night", "fountain mist", "fine sand", "crescent moon" }, 5 },
    { "madrid", "Madrid",
      { "gran via", "calle de alcala", "the metropolis", "plaza de callao", "red de san luis", "puerta del sol", "chueca", "malasana", "la latina", "lavapies", "plaza de espana", "cibeles", "barrio de las letras", "huertas", "plaza mayor", "el retiro" }, 16,
      { "winged victory", "gold dome", "rooftop neon", "churros", "chocolate", "tinto de verano", "geraniums", "wrought-iron balcony", "persiana", "folding fan", "night bus", "white taxi", "street sweeper", "terrace cafe", "bell tower", "farolas", "cinema sign", "black cat", "poplar fluff", "string lights" }, 20,
      { "warm night", "dry summer air", "poplar fluff", "honey moon", "city glow" }, 5 },
    { "rome", "Rome",
      { "monti", "via cavour", "via dei serpenti", "via panisperna", "piazza della madonna dei monti", "via degli annibaldi", "the colle oppio", "the celio", "the palatine", "the forum", "trastevere", "campo de' fiori", "the pantheon", "piazza navona", "testaccio", "the aventine" }, 16,
      { "colosseum", "arches", "umbrella pines", "bell tower", "sampietrini", "nasone", "trattoria", "checked tablecloths", "candlelight", "string lights", "green shutters", "laundry line", "bougainvillea", "corner shrine", "scooter", "gelato", "street cats", "gulls", "scaffolding", "wine bar" }, 20,
      { "shooting stars", "warm night", "august heat", "perseids", "summer breeze" }, 5 },
    { "prague", "Prague",
      { "mala strana", "the old town", "kampa", "petrin hill", "hradcany", "the vltava", "charles bridge", "josefov", "vinohrady", "zizkov", "letna", "smichov", "nove mesto", "wenceslas square", "old town square", "vysehrad" }, 16,
      { "castle lights", "cathedral spires", "bridge statues", "old lamps", "weir foam", "swans", "paddle steamer", "red tram", "tram bell", "linden blossoms", "cobblestones", "copper domes", "trdelnik", "bridge towers", "metronome", "lattice tower", "river terrace", "embankment bench", "mosaic pavement", "tiled roofs" }, 20,
      { "june night", "linden air", "river breeze", "clear skies", "moonlight" }, 5 },
    { "munich", "Munich",
      { "marienplatz", "the rathaus", "the frauenkirche", "the viktualienmarkt", "schwabing", "maxvorstadt", "haidhausen", "the glockenbach", "the englischer garten", "the isar", "odeonsplatz", "sendlinger tor", "kaufingerstrasse", "the tal", "giesing", "lehel" }, 16,
      { "christmas market", "glockenspiel", "green domes", "mulled wine", "gingerbread hearts", "roasted chestnuts", "pretzels", "wooden stalls", "string lights", "christmas tree", "golden madonna", "lantern", "snowy roofs", "felt hat", "bobble hat", "dachshund", "carillon", "paper star", "candles", "town hall tower" }, 20,
      { "snow", "snowfall", "december night", "frost", "soft flurries" }, 5 },
};
// ═════════════════════════════════════════════════════════════════════════════
// THE ARRANGER — a line-for-line port of Lofi Cities' planner (js/audio/arrange.js,
// engine.js planBar, rhythm.js, melody.js, harmony.js). Same RNG, same streams, same
// draw ORDER, so a seed plans the same track they would: key, progression, form,
// every layer switch, fill, push, bass approach and lead phrase. Nothing in this block
// makes a sound — it turns a seed into timed events (seconds from the track's start).
// ═════════════════════════════════════════════════════════════════════════════

// ── core.js: hash + rng (mulberry-ish), all uint32 so JS's Math.imul maps 1:1 ──
typedef struct { uint32_t s; } Rng;
static double lc_hash(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t h = 2166136261u ^ (a * 374761393u);
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= b * 668265263u;
    h = (h ^ (h >> 15)) * 2246822519u;
    h ^= c * 3266489917u;
    h = (h ^ (h >> 13)) * 3266489917u;
    h ^= h >> 16;
    return (double)h / 4294967296.0;
}
static double rn(Rng *r) {
    r->s += 1831565813u;
    uint32_t t = r->s;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return (double)(t ^ (t >> 14)) / 4294967296.0;
}
static Rng lc_stream(uint32_t seed, uint32_t k) { Rng r = { (uint32_t)floor(lc_hash(seed, k, 0) * 4294967296.0) }; return r; }
static uint32_t lc_next_seed(uint32_t seed) { return (uint32_t)floor(lc_hash(seed, 7919, 0) * 4294967296.0); }
static int rfloor(Rng *r, int n) { return (int)floor(rn(r) * n); }
// rnd2 = +(a + r()*(b-a)).toFixed(d): printf rounds the exact binary value, as toFixed does
static double rnd2(Rng *r, double a, double b, int d) {
    char buf[64]; snprintf(buf, sizeof buf, "%.*f", d, a + rn(r) * (b - a));
    return strtod(buf, NULL);
}
static int mod12(int n) { return ((n % 12) + 12) % 12; }
static double clampd(double v, double a, double b) { return v < a ? a : v > b ? b : v; }

// ── harmony.js ──
enum { Q_MAJ7, Q_MAJ9, Q_6, Q_69, Q_M7, Q_M9, Q_M6, Q_M7B5, Q_7, Q_9, Q_13, Q_7B9, Q_7S9, Q_7SUS4, NQUAL };
static const struct { const char *name; int n; int iv[6]; } QUAL[NQUAL] = {
    { "maj7", 4, { 0, 4, 7, 11 } },      { "maj9", 5, { 0, 4, 7, 11, 14 } },
    { "6",    4, { 0, 4, 7, 9 } },       { "69",   5, { 0, 4, 7, 9, 14 } },
    { "m7",   4, { 0, 3, 7, 10 } },      { "m9",   5, { 0, 3, 7, 10, 14 } },
    { "m6",   4, { 0, 3, 7, 9 } },       { "m7b5", 4, { 0, 3, 6, 10 } },
    { "7",    4, { 0, 4, 7, 10 } },      { "9",    5, { 0, 4, 7, 10, 14 } },
    { "13",   5, { 0, 4, 10, 14, 21 } }, { "7b9",  5, { 0, 4, 7, 10, 13 } },
    { "7#9",  5, { 0, 4, 7, 10, 15 } },  { "7sus4",4, { 0, 5, 7, 10 } },
};
typedef struct { int root, q, beats; } ProgCh;
typedef struct { const char *id; int n; ProgCh c[5]; } Prog;
static const Prog PROG_MAJ[7] = {
    { "M1", 4, { { 2, Q_M9, 4 }, { 7, Q_13, 4 }, { 0, Q_MAJ9, 4 }, { 0, Q_MAJ9, 4 } } },
    { "M2", 4, { { 0, Q_MAJ7, 4 }, { 9, Q_M9, 4 }, { 2, Q_M9, 4 }, { 7, Q_13, 4 } } },
    { "M3", 4, { { 5, Q_MAJ7, 4 }, { 4, Q_M7, 4 }, { 2, Q_M9, 4 }, { 0, Q_MAJ9, 4 } } },
    { "M4", 4, { { 5, Q_MAJ7, 4 }, { 5, Q_M9, 4 }, { 4, Q_M7, 4 }, { 9, Q_M9, 4 } } },
    { "M5", 5, { { 5, Q_MAJ9, 4 }, { 4, Q_7B9, 4 }, { 9, Q_M9, 4 }, { 7, Q_M9, 2 }, { 0, Q_13, 2 } } },
    { "M6", 4, { { 0, Q_MAJ9, 4 }, { 0, Q_MAJ9, 4 }, { 5, Q_MAJ9, 4 }, { 5, Q_MAJ9, 4 } } },
    { "M7", 4, { { 0, Q_MAJ9, 4 }, { 4, Q_M7, 4 }, { 5, Q_MAJ7, 4 }, { 5, Q_M6, 4 } } },
};
static const Prog PROG_MIN[6] = {
    { "m1", 4, { { 0, Q_M9, 4 }, { 5, Q_M9, 4 }, { 10, Q_13, 4 }, { 3, Q_MAJ9, 4 } } },
    { "m2", 4, { { 0, Q_M9, 4 }, { 0, Q_M9, 4 }, { 5, Q_9, 4 }, { 5, Q_9, 4 } } },
    { "m3", 4, { { 8, Q_MAJ9, 4 }, { 10, Q_6, 4 }, { 0, Q_M9, 4 }, { 0, Q_M9, 4 } } },
    { "m4", 4, { { 2, Q_M7B5, 4 }, { 7, Q_7B9, 4 }, { 0, Q_M9, 4 }, { 0, Q_M9, 4 } } },
    { "m5", 4, { { 0, Q_M9, 4 }, { 3, Q_MAJ7, 4 }, { 8, Q_MAJ7, 4 }, { 7, Q_7S9, 4 } } },
    { "m6", 5, { { 0, Q_M9, 4 }, { 8, Q_MAJ7, 4 }, { 5, Q_M9, 4 }, { 7, Q_7SUS4, 2 }, { 7, Q_7B9, 2 } } },
};
static const ProgCh TURN_MAJ[2] = { { 2, Q_M9, 2 }, { 7, Q_13, 2 } };
static const ProgCh TURN_MIN[2] = { { 2, Q_M7B5, 2 }, { 7, Q_7B9, 2 } };
static const char *NOTE_NAMES[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
static const int PENTA[2][5] = { { 0, 3, 5, 7, 10 }, { 0, 2, 4, 7, 9 } };   // [major?]

typedef struct { int tonic, major; } Key;
typedef struct { int root, q; double start, beats; int onset, fin; } Chord;
typedef struct { char id[12]; Chord c[12]; int n; double beats; } Expanded;

static Expanded lc_expand(const Prog *p, int stretch, const ProgCh *turn) {
    Expanded e; memset(&e, 0, sizeof e); snprintf(e.id, sizeof e.id, "%s", p->id);
    double t = 0;
    for (int i = 0; i < p->n; i++) {
        Chord c = { p->c[i].root, p->c[i].q, t, (double)p->c[i].beats * stretch, 0, 0 };
        e.c[e.n++] = c; t += c.beats;
    }
    if (turn) {
        double cut = t - 4; int k = 0;
        for (int i = 0; i < e.n; i++) if (e.c[i].start < cut) {
            Chord c = e.c[i]; c.beats = fmin(c.beats, cut - c.start); e.c[k++] = c;
        }
        e.n = k; double s = cut;
        for (int i = 0; i < 2; i++) { Chord c = { turn[i].root, turn[i].q, s, turn[i].beats, 0, 0 }; e.c[e.n++] = c; s += turn[i].beats; }
    }
    e.beats = t;
    return e;
}
static Key related_key(Key prev, Rng *r) {
    double u = rn(r);
    if (u < 0.15) return prev;
    if (u < 0.55) { Key k = { mod12(prev.tonic + (rn(r) < 0.5 ? 5 : 7)), prev.major }; return k; }
    if (u < 0.75) {
        if (prev.major) { Key k = { mod12(prev.tonic + 9), 0 }; return k; }
        Key k = { mod12(prev.tonic + 3), 1 }; return k;
    }
    Key k; k.tonic = rfloor(r, 12); k.major = !(rn(r) < 0.55); return k;
}
static Key random_key(Rng *r) { Key k; k.tonic = rfloor(r, 12); k.major = !(rn(r) < 0.55); return k; }

// spell: the chord's colour tones (root dropped), capped at 4, a 9th added when thin
typedef struct { int root, n, iv[6]; } Spelled;
static Spelled lc_spell(Key key, const Chord *c) {
    Spelled s; s.root = mod12(key.tonic + c->root); s.n = 0;
    int has7 = 0;
    for (int i = 0; i < QUAL[c->q].n; i++) { int v = QUAL[c->q].iv[i]; if (v % 12 != 0) { s.iv[s.n++] = v; if (v == 7) has7 = 1; } }
    if (s.n > 4 && has7) { int k = 0; for (int i = 0; i < s.n; i++) if (s.iv[i] != 7) s.iv[k++] = s.iv[i]; s.n = k; }
    while (s.n > 4) s.n--;
    if (s.n < 4) { int has2 = 0; for (int i = 0; i < s.n; i++) if (s.iv[i] % 12 == 2) has2 = 1; if (!has2) s.iv[s.n++] = 14; }
    return s;
}
static void isort(int *a, int n) { for (int i = 1; i < n; i++) { int v = a[i], j = i - 1; while (j >= 0 && a[j] > v) { a[j + 1] = a[j]; j--; } a[j + 1] = v; } }
typedef struct { int n, m[6]; } Voicing;
static double vmean(const Voicing *v) { double s = 0; for (int i = 0; i < v->n; i++) s += v->m[i]; return s / v->n; }
static double lead_cost(const Voicing *a, const Voicing *prev, int reg) {
    double c = 0.5 * fabs(vmean(a) - 62 - (prev ? 0 : reg));
    if (!prev) return c;
    int n = a->n < prev->n ? a->n : prev->n;
    for (int i = 0; i < n; i++) c += abs(a->m[i] - prev->m[i]);
    return c + 2 * fmax(0, abs(a->m[a->n - 1] - prev->m[prev->n - 1]) - 4);
}
// voice: every close + drop-2 inversion in register, the cheapest by voice-leading cost
static Voicing lc_voice(const Spelled *sp, const Voicing *prev, int reg) {
    int pcs[6], n = 0;
    for (int i = 0; i < sp->n; i++) { int p = sp->iv[i] % 12, dup = 0; for (int j = 0; j < n; j++) if (pcs[j] == p) dup = 1; if (!dup) pcs[n++] = p; }
    isort(pcs, n);
    int shapes[12][6], ns = 0;
    for (int k = 0; k < n; k++) {
        int cl[6], cn = 0;
        for (int j = 0; j < n; j++) { int v = pcs[(k + j) % n]; while (cn && v <= cl[cn - 1]) v += 12; cl[cn++] = v; }
        memcpy(shapes[ns++], cl, sizeof cl);
        if (n >= 3) { int d2[6]; memcpy(d2, cl, sizeof cl); d2[n - 2] -= 12; isort(d2, n); memcpy(shapes[ns++], d2, sizeof d2); }
    }
    Voicing best = { 0 }; double bc = INFINITY; int any = 0;
    for (int s = 0; s < ns; s++) for (int o = 1; o <= 6; o++) {
        Voicing v; v.n = n;
        for (int i = 0; i < n; i++) v.m[i] = sp->root + shapes[s][i] + 12 * o;
        if (v.m[0] < 48 || v.m[0] > 60 || v.m[n - 1] > 76) continue;
        int ok = 1; for (int i = 1; i < n; i++) if (v.m[i] - v.m[i - 1] < 3 && v.m[i - 1] < 52) ok = 0;
        if (!ok) continue;
        double k = lead_cost(&v, prev, reg);
        if (!any || k < bc) { bc = k; best = v; any = 1; }
    }
    if (!any) {
        best.n = sp->n;
        for (int i = 0; i < sp->n; i++) best.m[i] = 48 + mod12(sp->root + sp->iv[i] - 48) + (sp->iv[i] >= 12 ? 12 : 0);
        isort(best.m, best.n);
    }
    return best;
}
static int bass_note(int pc, int prev) {
    int best = 33 + mod12(pc - 33), bd = 1 << 30;
    for (int n = best; n <= 47; n += 12) { int d = abs(n - prev); if (d < bd) { bd = d; best = n; } }
    return best;
}

// ── rhythm.js ──
enum { V_KICK, V_SNARE, V_HAT, V_RIM, V_CLAP, V_SHAKER, V_TOM, V_BLOCK, NDRUMV };
static const char LANE_CH[NDRUMV] = { 'K', 'S', 'H', 'R', 'C', 'X', 'T', 'W' };
static const double VEL[NDRUMV] = { 0.92, 0.88, 0.8, 0.8, 0.85, 0.7, 0.85, 0.75 };
static const double HAT_ACCENT[4] = { 1, 0.45, 0.7, 0.45 };
typedef struct { int v, s, ghost, open; double acc, vel; } Hit;   // vel < 0 = unset
typedef struct { const char *id, *name; const char *lane[NDRUMV]; } Grid;
static const Grid GRIDS[6] = {
    { "P1", "boom",   { "x.........x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", 0 } },
    { "P2", "bounce", { "x......x..x.....", "....x.......x...", "x.x.x.x.x.x.x.xx", 0 } },
    { "P3", "lazy",   { "x.....x.........", "....x..g....x...", "x.x.x.x.x.x.x.o.", 0 } },
    { "P4", "half",   { "x.....x...x.....", "........x.......", "x.x.x.x.x.x.x.x.", "............r..." } },
    { "P5", "rim",    { "x.........x.....", 0, "x...x...x...x...", "....r.......r..." } },
    { "P6", "shuf",   { "x..x......x.....", "....x..g....x.g.", "xxxxxxxxxxxxxxxx", 0 } },
};
enum { P1, P2, P3, P4, P5, P6 };
typedef struct { Hit h[48]; int n; } Hits;
static Hits parse_grid(const Grid *g) {
    Hits o; o.n = 0;
    for (int L = 0; L < NDRUMV; L++) {
        const char *s = g->lane[L]; if (!s) continue;
        for (int i = 0; s[i]; i++) {
            char c = s[i]; if (c == '.') continue;
            Hit h = { L, i, 0, 0, 0, -1 };
            if (L == V_SNARE) h.ghost = c == 'g';
            else if (L == V_HAT) h.open = c == 'o';
            else if (c == 'g') h.ghost = 1;
            if (c == 'a') h.acc = 1.2; else if (c == 's') h.acc = 0.55;
            o.h[o.n++] = h;
        }
    }
    return o;
}
// FILLS F1 roll · F2 stop · F3 kicks · F4 open — in this order (Object.keys)
enum { F_NONE = -1, F1, F2, F3, F4 };
static const char *FILL_NAME[4] = { "roll", "stop", "kicks", "open" };
enum { COMP_HOLD, COMP_CHARLESTON, COMP_PULSE, NCOMP };
static const char *COMP_NAME[NCOMP] = { "hold", "charleston", "pulse" };
static const int COMPS[NCOMP][2][2] = { { { 0, 16 }, { -1, 0 } }, { { 0, 3 }, { 6, 10 } }, { { 0, 6 }, { 8, 8 } } };
static const int COMPN[NCOMP] = { 1, 2, 2 };
static double lc_swing(int step, double sd, double pct) { return step % 2 ? ((pct - 50) / 50) * sd : 0; }

// ── melody.js ──
static const int CELLS[8][5] = { { 0, 3, 6, 8 }, { 2, 4, 7 }, { 0, 6, 10, 12, 14 }, { 0, 2, 6, 8 }, { 3, 6, 10 }, { 0, 4, 6, 10 }, { 2, 6, 10, 12 }, { 0, 3, 8, 10 } };
static const int CELLN[8] = { 4, 3, 5, 4, 3, 4, 4, 4 };
static const int TAILS[6][2] = { { 0 }, { 16 }, { 16, 19 }, { 18 }, { 16, 22 }, { 20 } };
static const int TAILN[6] = { 0, 1, 2, 1, 2, 1 };
static const int ANSWERS[6][4] = { { 0, 4, 8 }, { 2, 6, 10 }, { 0, 3, 6, 10 }, { 4, 8 }, { 0, 6, 12 }, { 2, 4, 8, 12 } };
static const int ANSN[6] = { 3, 3, 4, 2, 3, 4 };
#define MEL_LO 67
#define MEL_HI 84
typedef struct { int n, steps[8], durs[8], contour[8]; double vel[8]; } Motif;
static void mk_contour(Rng *r, int n, int *c) {
    c[0] = 0; int dir = rn(r) < 0.5 ? 1 : -1, back = 0;
    for (int i = 1; i < n; i++) {
        int mv;
        if (back) { mv = -back; back = 0; }
        else if (rn(r) < 0.75) { int a = 1 + (int)floor(rn(r) * 2); mv = a * (rn(r) < 0.65 ? dir : -dir); }
        else { mv = (2 + (int)floor(rn(r) * 2)) * dir; back = mv > 0 ? 1 : mv < 0 ? -1 : 0; }
        c[i] = c[i - 1] + mv;
        if (abs(c[i]) > 4) dir = c[i] > 0 ? -1 : 1;
    }
}
static Motif make_motif(Rng *r) {
    Motif m; int ci = rfloor(r, 8), n = 0;
    for (int i = 0; i < CELLN[ci]; i++) m.steps[n++] = CELLS[ci][i];
    int ti = rfloor(r, 6);
    for (int i = 0; i < TAILN[ti]; i++) m.steps[n++] = TAILS[ti][i];
    if (n > 6) n = 6;
    while (n < 3) { m.steps[n] = m.steps[n - 1] + 4; n++; }
    m.n = n;
    int last = 4 + rfloor(r, 5);
    for (int i = 0; i < n; i++) m.durs[i] = i < n - 1 ? (6 < m.steps[i + 1] - m.steps[i] ? 6 : m.steps[i + 1] - m.steps[i]) : last;
    mk_contour(r, n, m.contour);
    for (int i = 0; i < n; i++) m.vel[i] = i == 0 ? 0.78 : 0.62 + 0.1 * rn(r);
    return m;
}
typedef struct { int s, d, midi; double v; } LNote;
typedef struct { LNote n[16]; int cnt; } Phrase;
typedef struct { int n, pc[6]; } Pcs;
typedef Pcs (*ChordAtFn)(void *ctx, int s);
typedef struct { int scale[40], n; } Scale;
static int pcs_has(const Pcs *p, int pc) { for (int i = 0; i < p->n; i++) if (p->pc[i] == pc) return 1; return 0; }
static int s_in(const Scale *S, int i) { return i >= 0 && i < S->n && S->scale[i] >= MEL_LO && S->scale[i] <= MEL_HI; }
static int s_nearest(const Scale *S, double target, const Pcs *pcs) {
    if (!isfinite(target)) target = 74;
    int best = -1; double bd = INFINITY;
    for (int i = 0; i < S->n; i++) {
        if (!s_in(S, i) || (pcs && !pcs_has(pcs, mod12(S->scale[i])))) continue;
        double d = fabs(S->scale[i] - target); if (d < bd) { bd = d; best = i; }
    }
    if (best >= 0) return best;
    if (pcs) return s_nearest(S, target, NULL);
    for (int i = 0; i < S->n; i++) if (S->scale[i] >= MEL_LO) return i;
    return 0;
}
static int s_chord_tone(const Scale *S, int i, Pcs pcs) {
    if (i >= 0 && i < S->n && pcs_has(&pcs, mod12(S->scale[i]))) return i;
    static const int D[4] = { 1, -1, 2, -2 };
    for (int k = 0; k < 4; k++) { int j = i + D[k]; if (j >= 0 && j < S->n && s_in(S, j) && pcs_has(&pcs, mod12(S->scale[j]))) return j; }
    return i;
}
static int s_clamp(const Scale *S, double x) {
    int i = (int)floor(x + 0.5);                       // Math.round
    if (i < 0) i = 0; if (i > S->n - 1) i = S->n - 1;
    while (i > 0 && S->scale[i] > MEL_HI) i--;
    while (i < S->n - 1 && S->scale[i] < MEL_LO) i++;
    return i;
}
typedef struct { int statement, inv, shift, hasPrev, prevLast, reg; } Variant;
static Phrase lc_phrase(const Motif *mo, ChordAtFn chordAt, void *ctx, Key key, Rng *r, Variant va) {
    Scale S; S.n = 0;
    for (int m = 55; m <= 96; m++) for (int k = 0; k < 5; k++) if (mod12(key.tonic + PENTA[key.major][k]) == mod12(m)) { S.scale[S.n++] = m; break; }
    struct { int s, d, i; double v; } nt[16]; int nn = 0;
    int shift = va.shift, cont[8];
    for (int i = 0; i < mo->n; i++) cont[i] = va.inv ? -mo->contour[i] : mo->contour[i];
    double tgt = (va.hasPrev ? va.prevLast : 74 + va.reg) + (rn(r) * 4 - 2);
    Pcs p0 = chordAt(ctx, shift);
    int anchor = s_nearest(&S, tgt, &p0);
    for (int i = 0; i < mo->n; i++) {
        int s = mo->steps[i] + shift; if (s > 31) continue;
        int idx = s_clamp(&S, anchor + cont[i]);
        if (s % 16 == 0 || s % 16 == 8) idx = s_chord_tone(&S, idx, chordAt(ctx, s));
        nt[nn].s = s; nt[nn].d = mo->durs[i]; nt[nn].i = idx; nt[nn].v = mo->vel[i]; nn++;
    }
    if (va.statement && va.statement % 3 == 0 && nn > 1) {
        int L = nn - 1;
        nt[L].i = s_clamp(&S, nt[L].i + (rn(r) < 0.5 ? 2 : -1));
        nt[L].d = nt[L].d - 2 > 2 ? nt[L].d - 2 : 2;
        int s2 = nt[L].s + nt[L].d + 1; if (s2 > 31) s2 = 31;
        nt[nn].s = s2; nt[nn].d = 4; nt[nn].i = s_clamp(&S, s_chord_tone(&S, nt[L].i - 1, chordAt(ctx, 30))); nt[nn].v = 0.6; nn++;
    }
    int ai = rfloor(r, 6);
    int cur = nn ? nt[nn - 1].i : anchor;
    for (int k = 0; k < ANSN[ai]; k++) {
        int c = ANSWERS[ai][k], s = 48 + c, mv;
        if (rn(r) < 0.75) { int sg = rn(r) < 0.5 ? -1 : 1; mv = sg * (1 + (int)floor(rn(r) * 2)); }
        else mv = rn(r) < 0.5 ? -3 : 3;
        cur = s_clamp(&S, cur + mv);
        if (k == ANSN[ai] - 1 || s % 16 == 0 || s % 16 == 8) cur = s_chord_tone(&S, cur, chordAt(ctx, s));
        int d;
        if (k < ANSN[ai] - 1) { d = ANSWERS[ai][k + 1] - c; if (d > 6) d = 6; }
        else { int a = 63 - s, b = 4 + rfloor(r, 5); d = a < b ? a : b; }
        nt[nn].s = s; nt[nn].d = d > 1 ? d : 1; nt[nn].i = cur; nt[nn].v = 0.58 + 0.12 * rn(r); nn++;
    }
    int lo = 1 << 30; for (int i = 0; i < nn; i++) if (S.scale[nt[i].i] < lo) lo = S.scale[nt[i].i];
    Phrase ph; ph.cnt = 0;
    for (int i = 0; i < nn; i++) {
        int m = S.scale[nt[i].i];
        while (m > lo + 12) m -= 12;
        int found = 0; for (int j = 0; j < S.n; j++) if (S.scale[j] == m) found = 1;
        if (!found) m = S.scale[s_nearest(&S, m, NULL)];
        LNote x = { nt[i].s, nt[i].d, m, nt[i].v };
        int j = ph.cnt++;                                   // stable insert by s
        while (j > 0 && ph.n[j - 1].s > x.s) { ph.n[j] = ph.n[j - 1]; j--; }
        ph.n[j] = x;
    }
    return ph;
}

// ── arrange.js: the ENERGY table (chill / balanced / upbeat) ──
enum { EN_CHILL, EN_BALANCED, EN_UPBEAT, NENERGY };
static const char *ENERGY_NAME[NENERGY] = { "chill", "balanced", "upbeat" };
enum { BAND_FULL, BAND_NODRUMS, BAND_KEYS, NBAND };
static const char *BAND_NAME[NBAND] = { "full band", "no drums", "chords only" };
static const double GROOVES[4][6] = {                  // P1..P6 weights
    { 0.8, 0.5, 1.4, 2.6, 1.2, 0.3 },                   // chill
    { 1, 1, 1, 2.6, 0.5, 0.7 },                         // slow
    { 1.3, 1.2, 1, 0.6, 0.5, 1 },                       // fast
    { 1.4, 1.6, 0.6, 0.2, 0.2, 1.4 },                   // upbeat
};
static const int GROOVE_OF[NENERGY][2] = { { 0, 0 }, { 1, 2 }, { 3, 3 } };   // [< 74 bpm, >= 74]
typedef struct {
    double stretch; int bpm[2]; double swing[2], snareLag[2], drumLevel, kitLp[2], epLp[2], tremDepth[2], strum[2], epVel[2];
    double hold[2], charleston[2], pulse[2], push, legato, lead, softLead, tone[2], wowCents[2], dust[2];
    double introHats; int leadFrom; double hatsFirst, breakDrums, breakLead;
} Energy;
static const Energy EN[NENERGY] = {
    { 0.5,  { 60, 72 }, { 56, 64 }, { 0.012, 0.022 }, 0.85, { 5000, 7500 }, { 2000, 3600 }, { 0.1, 0.22 }, { 0.015, 0.04 }, { 0.46, 0.58 },
      { 0.7, 1.3 }, { 0.05, 0.4 }, { 0.05, 0.3 }, 0.12, 0.8, 0.5, 0.75, { 4200, 6800 }, { 6, 12 }, { 0.9, 1.6 }, 0.3, 2, 0.45, 0.35, 0.25 },
    { 0.3,  { 68, 88 }, { 54, 62 }, { 0.008, 0.018 }, 1,    { 6000, 9000 }, { 2400, 4500 }, { 0.06, 0.18 }, { 0.01, 0.03 }, { 0.5, 0.62 },
      { 0.3, 1 },   { 0.1, 0.8 },  { 0.1, 0.6 },  0.25, 0.6, 0.65, 0.6, { 5500, 9000 }, { 4, 10 }, { 0.6, 1.4 }, 0.5, 2, 0.3, 0.6, 0.4 },
    { 0.15, { 80, 94 }, { 52, 58 }, { 0.004, 0.012 }, 1,    { 7000, 9500 }, { 3000, 5200 }, { 0.04, 0.12 }, { 0.008, 0.02 }, { 0.52, 0.64 },
      { 0.1, 0.5 }, { 0.4, 1 },    { 0.4, 1 },    0.38, 0.35, 0.8, 0.45, { 7000, 10000 }, { 3, 7 }, { 0.5, 1.1 }, 0.7, 1, 0.15, 0.85, 0.55 },
};
enum { S_INTRO, S_A, S_B, S_BREAK, S_OUTRO };
static const char *SEC_NAME[5] = { "intro", "A", "B", "break", "outro" };
static const int FORMS[3][8][2] = {
    { { S_INTRO, 4 }, { S_A, 8 }, { S_A, 8 }, { S_B, 8 }, { S_BREAK, 4 }, { S_A, 8 }, { S_B, 8 }, { S_OUTRO, 4 } },
    { { S_INTRO, 2 }, { S_A, 8 }, { S_B, 8 }, { S_A, 8 }, { S_BREAK, 8 }, { S_B, 8 }, { S_A, 8 }, { S_OUTRO, 4 } },
    { { S_INTRO, 4 }, { S_A, 16 }, { S_BREAK, 4 }, { S_B, 8 }, { S_A, 16 }, { S_OUTRO, 4 }, { -1, 0 } },
};
enum { EP_COMP, EP_INTRO, EP_WHOLE, EP_OUTRO };
enum { BS_KICK, BS_LAST, BS_NONE, BS_WHOLE, BS_OUTRO };
enum { DR_PATTERN, DR_HATS2, DR_NONE, DR_P5, DR_OUTRO };
typedef struct { int ep, bass, drums, kit, lead, hatsFirst, dipLast; double level; } Layers;
enum { PR_A, PR_B, PR_FINAL };
typedef struct { int name, bars, prog; Layers L; } Section;

typedef struct { double kickF0, kickF1, kickDecay, snareDecay, hatHP, hatClosed, lp; } Kit;
#define MAXSEC 16
typedef struct {
    uint32_t seed, nextSeed;
    int energy, band, city;
    int bpm; double swing, snareLag;
    Key key;
    Expanded progA, progB; int chordBars;
    int pattern; Kit kit; double drumLevel;
    struct { double lp, tremRate, tremDepth, strum, vel, comp[NCOMP], push; } ep;
    int legato; double bassFactor;
    int hasLead, leadSoft; double leadPan; Motif motif;
    double tone, wowCents, dust;
    int reg, form;
    Section sec[MAXSEC]; int nsec, bars;
    double duration;
    char title[40];
} Plan;

// ── the title generator (makeTitle) — the city's words + templates ──
static const char *pick_s(Rng *r, const char **a, int n) { return a[rfloor(r, n)]; }
static void make_title(Rng *r, int city, char *out, int cap) {
    const LcCity *C = &LC_CITY[city];
    int NN = sizeof LC_NOUNS / sizeof *LC_NOUNS, NA = sizeof LC_ADJS / sizeof *LC_ADJS, NT = sizeof LC_TIMES / sizeof *LC_TIMES;
    int NTPL = sizeof LC_TPL / sizeof *LC_TPL; double total = 0; for (int i = 0; i < NTPL; i++) total += LC_TPL[i].w;
    for (int attempt = 0; attempt < 16; attempt++) {
        double u = rn(r) * total; const char *tpl = LC_TPL[0].t;
        for (int i = 0; i < NTPL; i++) if ((u -= LC_TPL[i].w) < 0) { tpl = LC_TPL[i].t; break; }
        char s[128]; int n = 0;
        for (const char *p = tpl; *p && n < 120; p++) {
            char tok[64] = { 0 };
            if (p[0] == '{' && p[1] && p[2] == '}') {
                const char *w;
                switch (p[1]) {
                case 'a': snprintf(tok, sizeof tok, "%s", pick_s(r, LC_ADJS, NA)); break;
                case 'n': snprintf(tok, sizeof tok, "%s", pick_s(r, LC_NOUNS, NN)); break;
                case 't': snprintf(tok, sizeof tok, "%s", pick_s(r, LC_TIMES, NT)); break;
                case 'p': snprintf(tok, sizeof tok, "%s", C->p[rfloor(r, C->np)]); break;
                case 'c': snprintf(tok, sizeof tok, "%s", C->c[rfloor(r, C->nc)]); break;
                case 'w': snprintf(tok, sizeof tok, "%s", C->w[rfloor(r, C->nw)]); break;
                case 'P': case 'Q':
                    w = p[1] == 'P' ? C->p[rfloor(r, C->np)] : C->w[rfloor(r, C->nw)];
                    { const char *a = pick_s(r, LC_ADJS, NA);
                      if (!strncmp(w, "the ", 4)) snprintf(tok, sizeof tok, "the %s %s", a, w + 4);
                      else snprintf(tok, sizeof tok, "%s %s", a, w); }
                    break;
                case 'R': w = C->w[rfloor(r, C->nw)];
                    if (!strncmp(w, "the ", 4)) snprintf(tok, sizeof tok, "%s", w); else snprintf(tok, sizeof tok, "the %s", w);
                    break;
                }
                for (int k = 0; tok[k] && n < 120; k++) s[n++] = tok[k];
                p += 2;
            } else s[n++] = *p;
        }
        s[n] = 0;
        char *ll; while ((ll = strstr(s, "last last "))) memmove(ll, ll + 5, strlen(ll + 5) + 1);
        for (char *q = s; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32;
        if ((int)strlen(s) <= 32) { snprintf(out, cap, "%s", s); return; }
    }
    snprintf(out, cap, "%.32s", C->p[rfloor(r, C->np)]);
}

static double pick_w(Rng *r, const double *w, int n, int *out) {
    double sum = 0; for (int i = 0; i < n; i++) sum += w[i];
    double u = rn(r) * sum;
    for (int i = 0; i < n; i++) if ((u -= w[i]) < 0) { *out = i; return 0; }
    *out = 0; return 0;
}

// planTrack — one seed → the whole track: key, harmony, groove, kit, keys, lead, form
static Plan plan_track(uint32_t seed, const Key *prevKey, int energy, int band, int city) {
    Plan P; memset(&P, 0, sizeof P);
    P.seed = seed; P.energy = energy; P.band = band; P.city = city;
    const Energy *E = &EN[energy];
    Rng rH = lc_stream(seed, 1), rR = lc_stream(seed, 2), rM = lc_stream(seed, 3), rA = lc_stream(seed, 4), rT = lc_stream(seed, 5);
    P.key = prevKey ? related_key(*prevKey, &rH) : random_key(&rH);
    const Prog *bank = P.key.major ? PROG_MAJ : PROG_MIN; int nb = P.key.major ? 7 : 6;
    int idA = rfloor(&rH, nb);
    P.chordBars = rn(&rH) < E->stretch ? 2 : 1;
    int idB = idA, turn = 0;
    if (rn(&rH) < 0.6) {
        int others[8], no = 0; for (int i = 0; i < nb; i++) if (i != idA) others[no++] = i;
        int k = rfloor(&rH, no); idB = k < no ? others[k] : idA;
    } else turn = 1;
    P.progA = lc_expand(&bank[idA], P.chordBars, NULL);
    P.progB = lc_expand(&bank[idB], P.chordBars, turn ? (P.key.major ? TURN_MAJ : TURN_MIN) : NULL);
    if (turn) snprintf(P.progB.id, sizeof P.progB.id, "%s+ii-V", bank[idA].id);
    P.bpm = E->bpm[0] + (int)floor(rn(&rR) * (E->bpm[1] - E->bpm[0] + 1));
    P.swing = rnd2(&rR, E->swing[0], E->swing[1], 1);
    P.snareLag = rnd2(&rR, E->snareLag[0], E->snareLag[1], 4);
    pick_w(&rR, GROOVES[GROOVE_OF[energy][P.bpm < 74 ? 0 : 1]], 6, &P.pattern);
    P.kit.kickF0 = rnd2(&rR, 150, 170, 1);
    P.kit.kickF1 = rnd2(&rR, 46, 56, 1);
    P.kit.kickDecay = rnd2(&rR, 0.11, 0.15, 3);
    P.kit.snareDecay = rnd2(&rR, 0.05, 0.08, 3);
    P.kit.hatHP = rnd2(&rR, 6000, 8500, 0);
    P.kit.hatClosed = rnd2(&rR, 0.012, 0.02, 3);
    P.kit.lp = rnd2(&rR, E->kitLp[0], E->kitLp[1], 0);
    P.ep.lp = rnd2(&rA, E->epLp[0], E->epLp[1], 0);
    P.ep.tremRate = rnd2(&rA, 3.5, 5.5, 2);
    P.ep.tremDepth = rnd2(&rA, E->tremDepth[0], E->tremDepth[1], 3);
    P.ep.strum = rnd2(&rA, E->strum[0], E->strum[1], 4);
    P.ep.vel = rnd2(&rA, E->epVel[0], E->epVel[1], 3);
    P.ep.comp[COMP_HOLD] = rnd2(&rA, E->hold[0], E->hold[1], 2);
    P.ep.comp[COMP_CHARLESTON] = rnd2(&rA, E->charleston[0], E->charleston[1], 2);
    P.ep.comp[COMP_PULSE] = rnd2(&rA, E->pulse[0], E->pulse[1], 2);
    P.ep.push = E->push;
    P.legato = rn(&rA) < E->legato;
    P.bassFactor = P.legato ? 0.92 : 0.55;
    if (rn(&rA) < E->lead) {
        P.hasLead = 1;
        P.leadSoft = rn(&rA) < E->softLead;
        P.leadPan = rnd2(&rA, -0.3, 0.3, 2);
        P.motif = make_motif(&rM);
    }
    P.tone = rnd2(&rA, E->tone[0], E->tone[1], 0);
    P.wowCents = rnd2(&rA, E->wowCents[0], E->wowCents[1], 1);
    P.dust = rnd2(&rA, E->dust[0], E->dust[1], 2);
    { Rng r8 = lc_stream(seed, 8); P.reg = (int)floor(rn(&r8) * 11) - 5; }
    double barDur = 240.0 / P.bpm;
    P.form = rfloor(&rA, 3);
    int nm[MAXSEC], nbar[MAXSEC], n = 0;
    for (int i = 0; i < 8 && FORMS[P.form][i][0] >= 0; i++) { nm[n] = FORMS[P.form][i][0]; nbar[n] = FORMS[P.form][i][1]; n++; }
#define TOTAL() ({ int t_ = 0; for (int q_ = 0; q_ < n; q_++) t_ += nbar[q_]; t_; })
#define INSERT() do { nm[n + 1] = nm[n - 1]; nbar[n + 1] = nbar[n - 1]; nm[n - 1] = S_B; nbar[n - 1] = 8; nm[n] = S_A; nbar[n] = 8; n += 2; } while (0)
    while (TOTAL() * barDur < 150 && n + 2 <= MAXSEC) INSERT();
    if (rn(&rA) < 0.35 && (TOTAL() + 16) * barDur <= 240 && n + 2 <= MAXSEC) INSERT();
    int aCount = 0;
    for (int i = 0; i < n; i++) {
        Layers L = { EP_COMP, BS_KICK, DR_PATTERN, 1, 0, 0, 0, 1 };
        int s = nm[i];
        if (s == S_INTRO) {
            L.ep = EP_INTRO;
            L.bass = rn(&rA) < 0.5 ? BS_LAST : BS_NONE;
            L.drums = rn(&rA) < E->introHats ? DR_HATS2 : DR_NONE;
        } else if (s == S_A) { aCount++; L.lead = P.hasLead && aCount >= E->leadFrom; }
        else if (s == S_B) { L.lead = P.hasLead; L.hatsFirst = rn(&rA) < E->hatsFirst; }
        else if (s == S_BREAK) {
            L.ep = EP_WHOLE;
            L.bass = rn(&rA) < 0.5 ? BS_WHOLE : BS_NONE;
            L.drums = rn(&rA) < E->breakDrums ? DR_P5 : DR_NONE;
            L.lead = P.hasLead && rn(&rA) < E->breakLead;
        } else { L.ep = EP_OUTRO; L.bass = BS_OUTRO; L.drums = DR_OUTRO; }
        if ((s == S_A || s == S_B) && i < n - 1) L.dipLast = rn(&rA) < 0.25;
        P.sec[i].name = s; P.sec[i].bars = nbar[i];
        P.sec[i].prog = s == S_B ? PR_B : s == S_OUTRO ? PR_FINAL : PR_A;
        P.sec[i].L = L;
    }
    P.nsec = n; P.bars = TOTAL();
#undef TOTAL
#undef INSERT
    P.duration = P.bars * barDur;
    P.nextSeed = lc_next_seed(seed);
    make_title(&rT, city, P.title, sizeof P.title);
    (void)E->drumLevel; P.drumLevel = E->drumLevel;
    return P;
}
// bandLayers — full band / no drums / chords only (keys lifted +3 dB)
static Layers band_layers(Layers L, int band) {
    if (band == BAND_NODRUMS) L.kit = 0;
    else if (band == BAND_KEYS) { L.kit = 0; L.bass = BS_NONE; L.lead = 0; L.level = 1.41; }
    return L;
}

// ── engine.js: the per-bar planner ──
enum { K_FX, K_EP, K_BASS, K_KICK, K_SNARE, K_HAT, K_RIM, K_CLAP, K_SHAKER, K_TOM, K_BLOCK, K_LEAD };
static const char *K_NAME[] = { "fx", "ep", "bass", "kick", "snare", "hat", "rim", "clap", "shaker", "tom", "block", "lead" };
enum { FX_TONE_, FX_VINYL_, FX_DUST_, FX_WOW_, FX_LEVEL_ };
static const char *FXN[] = { "tone", "vinyl", "dust", "wow", "level" };
typedef struct {
    double t; int k, bar;
    int notes[6], nn; double vel, dur, strum, rel;
    int midi, slide, glide, open, ghost;
    int fx; double v, tau;
} Ev;
typedef struct { int si, sec, j, n, prog, prev; Layers L; } BarInfo;
#define MAXBARS 96
typedef struct {
    BarInfo map[MAXBARS]; int nbars;
    Rng rH, rR, rM, rU, rS;
    Voicing prevVoicing; int hasPrevVoicing;
    int prevBass, pushed, nextComp;
    Phrase phrases[MAXSEC][4]; unsigned char hasPhrase[MAXSEC][4];
    int statement, lastLead, hasLastLead;
    int lastFill, lastComp, lastPush;            // for the display (not part of the port)
} BarState;
static const double VINYL_BOOST = 1.585;

static void bar_state(const Plan *P, BarState *st) {
    memset(st, 0, sizeof *st);
    for (int si = 0; si < P->nsec; si++) {
        Layers L = band_layers(P->sec[si].L, P->band);
        for (int j = 0; j < P->sec[si].bars && st->nbars < MAXBARS; j++) {
            BarInfo b = { si, P->sec[si].name, j, P->sec[si].bars, P->sec[si].prog, si ? P->sec[si - 1].name : -1, L };
            st->map[st->nbars++] = b;
        }
    }
    st->rH = lc_stream(P->seed, 11); st->rR = lc_stream(P->seed, 12); st->rM = lc_stream(P->seed, 13);
    st->rU = lc_stream(P->seed, 16); st->rS = lc_stream(P->seed, 17);
    st->prevBass = 40; st->pushed = -1; st->nextComp = -1; st->lastFill = F_NONE;
}
static int chords_in(const Expanded *pr, int j, Chord *out) {
    double b0 = fmod(j * 4.0, pr->beats); int n = 0;
    for (int i = 0; i < pr->n; i++) {
        const Chord *c = &pr->c[i];
        double s = fmax(c->start, b0), e = fmin(c->start + c->beats, b0 + 4);
        if (e > s) { Chord o = { c->root, c->q, s - b0, e - s, c->start >= b0, 0 }; out[n++] = o; }
    }
    return n;
}
static int bar_chords(const Plan *P, const BarState *st, int i, Chord *out) {
    const BarInfo *b = &st->map[i < st->nbars - 1 ? i : st->nbars - 1];
    if (b->prog == PR_FINAL) {
        if (b->j < 2) return chords_in(&P->progA, b->j, out);
        int q = !P->key.major ? Q_M9 : (P->seed % 2 ? Q_MAJ9 : Q_69);
        Chord c = { 0, q, 0, 4, b->j == 2, 1 }; out[0] = c; return 1;
    }
    return chords_in(b->prog == PR_B ? &P->progB : &P->progA, b->j, out);
}
static Pcs chord_pcs(const Plan *P, const Chord *c) {
    Pcs p; p.n = QUAL[c->q].n;
    for (int i = 0; i < p.n; i++) p.pc[i] = mod12(P->key.tonic + c->root + QUAL[c->q].iv[i]);
    return p;
}
static int same_chord(const Chord *a, const Chord *b) { return a && b && a->root == b->root && a->q == b->q; }
static int pick_comp(const double *w, Rng *r) {
    double sum = 0; for (int i = 0; i < NCOMP; i++) sum += w[i];
    double u = rn(r) * sum;
    for (int i = 0; i < NCOMP; i++) if ((u -= w[i]) < 0) return i;
    return NCOMP - 1;
}

// barHits — the drum grid for one bar, with its B-section variations and fills
typedef struct { Hit h[64]; int n, stop, fill; } BarHits;
static void hits_filter_not(BarHits *o, int (*keep)(const Hit *, int), int arg) {
    int k = 0; for (int i = 0; i < o->n; i++) if (keep(&o->h[i], arg)) o->h[k++] = o->h[i]; o->n = k;
}
static int keep_hatshaker(const Hit *h, int a) { (void)a; return h->v == V_HAT || h->v == V_SHAKER; }
static int keep_nokick(const Hit *h, int a) { (void)a; return h->v != V_KICK; }
static int keep_not_hat15(const Hit *h, int a) { (void)a; return !(h->v == V_HAT && h->s == 15); }
static int keep_not_v_ge12(const Hit *h, int v) { return !(h->v == v && h->s >= 12); }
static int keep_before(const Hit *h, int s) { return h->s < s; }
static int keep_not_hat_ge(const Hit *h, int s) { return !(h->v == V_HAT && h->s >= s); }
static BarHits bar_hits(const Plan *P, const BarInfo *b, int drums, int hatsFirst, Rng *r) {
    BarHits o; o.n = 0; o.stop = -1; o.fill = F_NONE;
    int mode = drums;
    if (mode == DR_NONE) return o;
    if (mode == DR_HATS2 && b->j < b->n - 2) return o;
    if (mode == DR_OUTRO && b->j >= 2) return o;
    Hits g = parse_grid(&GRIDS[mode == DR_P5 ? P5 : P->pattern]);
    double scale = (mode == DR_P5 ? 0.6 : 1) * P->drumLevel;
    for (int i = 0; i < g.n; i++) o.h[o.n++] = g.h[i];
    if (mode == DR_HATS2 || (hatsFirst && b->j == 0)) hits_filter_not(&o, keep_hatshaker, 0);
    if (mode == DR_OUTRO) hits_filter_not(&o, keep_nokick, 0);
    int groove = mode == DR_PATTERN || mode == DR_P5;
    if (groove && b->j > 0 && rn(r) < 0.08) hits_filter_not(&o, keep_nokick, 0);
    if (mode == DR_PATTERN && b->sec == S_B) {
        if (b->j % 2 == 1) {
            int f = -1; for (int i = 0; i < o.n; i++) if (o.h[i].v == V_HAT && o.h[i].s == 14) { f = i; break; }
            if (f >= 0) o.h[f].open = 1;
            else { Hit h = { V_HAT, 14, 0, 1, 0, -1 }; o.h[o.n++] = h; }
            hits_filter_not(&o, keep_not_hat15, 0);
        }
        static const int SS[2] = { 7, 15 };
        for (int q = 0; q < 2; q++) {
            if (rn(r) < 0.3) {
                int has = 0; for (int i = 0; i < o.n; i++) if (o.h[i].v == V_SNARE && o.h[i].s == SS[q]) has = 1;
                if (!has) { Hit h = { V_SNARE, SS[q], 1, 0, 0, -1 }; o.h[o.n++] = h; }
            }
        }
    }
    if (groove && b->j < b->n) {
        int pos = b->j + 1; double p = pos % 8 == 0 ? 0.5 : pos % 4 == 0 ? 0.2 : 0;
        if (p && rn(r) < p) {
            int id = rfloor(r, 4); o.fill = id;
            if (id == F1) {
                static const double ROLL[4][2] = { { 12, 0.9 }, { 13, 0.3 }, { 14, 0.45 }, { 15, 0.65 } };
                hits_filter_not(&o, keep_not_v_ge12, V_SNARE);
                for (int k = 0; k < 4; k++) { Hit h = { V_SNARE, (int)ROLL[k][0], 0, 0, 0, ROLL[k][1] }; o.h[o.n++] = h; }
            }
            if (id == F2) { hits_filter_not(&o, keep_before, 12); o.stop = 12; }
            if (id == F3) {
                static const int KS[2] = { 13, 15 };
                for (int k = 0; k < 2; k++) {
                    int has = 0; for (int i = 0; i < o.n; i++) if (o.h[i].v == V_KICK && o.h[i].s == KS[k]) has = 1;
                    if (!has) { Hit h = { V_KICK, KS[k], 0, 0, 0, 0.7 }; o.h[o.n++] = h; }
                }
            }
            if (id == F4) { hits_filter_not(&o, keep_not_hat_ge, 14); Hit h = { V_HAT, 14, 0, 1, 0, -1 }; o.h[o.n++] = h; }
        }
    }
    for (int i = 0; i < o.n; i++) {
        Hit *h = &o.h[i];
        double v = h->vel >= 0 ? h->vel : VEL[h->v];
        if (h->v == V_HAT) v *= h->open ? 0.95 : HAT_ACCENT[h->s % 4];
        if (h->v == V_KICK && h->s != 0 && h->vel < 0) v *= 0.9;
        if (h->acc) v *= h->acc;
        if (h->ghost) v = 0.25 + 0.1 * rn(r);
        h->vel = v * scale;
    }
    for (int i = 1; i < o.n; i++) { Hit x = o.h[i]; int j = i - 1; while (j >= 0 && o.h[j].s > x.s) { o.h[j + 1] = o.h[j]; j--; } o.h[j + 1] = x; }
    return o;
}

typedef struct { const Plan *P; const BarState *st; int startBar; } LeadCtx;
static Pcs lead_pcs_at(void *vctx, int s) {
    LeadCtx *c = vctx; Chord cs[12];
    int n = bar_chords(c->P, c->st, c->startBar + s / 16, cs);
    double beat = (s % 16) / 4.0; int k = 0;
    for (int i = 0; i < n; i++) if (beat >= cs[i].start && beat < cs[i].start + cs[i].beats) { k = i; break; }
    return chord_pcs(c->P, &cs[k]);
}

static const double SIG[NDRUMV] = { 3e-3, 4e-3, 6e-3, 4e-3, 5e-3, 5e-3, 5e-3, 5e-3 };
static int LAGGED(int v) { return v == V_SNARE || v == V_RIM || v == V_CLAP; }

typedef struct { Ev e[160]; int n; } Evs;
static Ev *push_ev(Evs *E, double t, int k, int bar) { Ev *x = &E->e[E->n++]; memset(x, 0, sizeof *x); x->t = t; x->k = k; x->bar = bar; return x; }
typedef struct { const Plan *P; BarState *st; double bar0, sd; } BarCtx;
static double gauss_(BarState *st) { return (rn(&st->rU) + rn(&st->rU) + rn(&st->rU) - 1.5) * 2; }
static double at_(BarCtx *c, int s, double sigma, double lag) {
    return fmax(0, c->bar0 + s * c->sd + lc_swing(s, c->sd, c->P->swing) + lag + gauss_(c->st) * sigma);
}
static double vh_(BarState *st, double v) { return clampd(v * (1 + (rn(&st->rU) * 2 - 1) * 0.08), 0.05, 1); }

// planBar — every event of bar i: fx automation, keys comping (+ the push), drums, bass, lead
static void plan_bar(const Plan *P, int i, BarState *st, Evs *out) {
    out->n = 0;
    const BarInfo *b = &st->map[i]; const Layers *L = &b->L;
    double sd = 60.0 / P->bpm / 4, bar0 = i * 16 * sd, barDur = 16 * sd;
    BarCtx cx = { P, st, bar0, sd };
    Chord chords[12]; int nch = bar_chords(P, st, i, chords);
    Chord nextC[12]; int nnext = i + 1 < st->nbars ? bar_chords(P, st, i + 1, nextC) : 0;
#define FX(type, val, tau_, dt_) do { Ev *x_ = push_ev(out, bar0 + (dt_), K_FX, i); x_->fx = type; x_->v = val; x_->tau = tau_; } while (0)
    if (i == 0) { FX(FX_DUST_, P->dust, 1, 0); FX(FX_WOW_, 1, 1, 0); FX(FX_VINYL_, VINYL_BOOST, 0.5, 0); FX(FX_TONE_, 900, 0.02, 0); }
    else if (L->level != st->map[i - 1].L.level) FX(FX_LEVEL_, L->level, 0.4, 0);
#define DRIFT() (P->tone * (1 + (rn(&st->rH) * 2 - 1) * 0.12))
    if (b->sec == S_INTRO) FX(FX_TONE_, 900 * pow(P->tone / 900, (b->j + 1) / (double)b->n), barDur / 3, i == 0 ? 0.12 : 0);
    else if (b->j == 0) {
        if (b->sec == S_BREAK) FX(FX_TONE_, 1800, 0.4, 0);
        else if (b->sec != S_OUTRO) { double d = DRIFT(); FX(FX_TONE_, d, b->prev == S_BREAK ? 0.8 : 0.3, 0); }
        if (b->prev == S_INTRO) FX(FX_VINYL_, 1, 2, 0);
    } else if (b->j % 4 == 0 && (b->sec == S_A || b->sec == S_B)) { double d = DRIFT(); FX(FX_TONE_, d, barDur, 0); }
    if (b->j % 2 == 0) { double w = 0.6 + rn(&st->rH) * 0.8; FX(FX_WOW_, w, 1, 0); }
    if (L->dipLast && b->j == b->n - 1) FX(FX_TONE_, 500, 0.25, 0);
    if (b->sec == S_OUTRO && b->j == b->n - 2) { FX(FX_TONE_, 700, barDur * 0.6, 0); FX(FX_VINYL_, VINYL_BOOST, 1.5, 0); }
#undef DRIFT
#undef FX
    // ── keys: the comp hits [step, len, chord, vel-scale, release] ──
    struct { double s, len; Chord c; double vs, rel; } hits[16]; int nh = 0;
#define HIT(s_, len_, c_, vs_, rel_) do { hits[nh].s = s_; hits[nh].len = len_; hits[nh].c = c_; hits[nh].vs = vs_; hits[nh].rel = rel_; nh++; } while (0)
    double vel = P->ep.vel;
    st->lastComp = -1; st->lastPush = 0;
    if (L->ep == EP_INTRO || L->ep == EP_WHOLE) {
        for (int k = 0; k < nch; k++) HIT(chords[k].start * 4, chords[k].beats * 4, chords[k], L->ep == EP_INTRO ? 0.85 : 0.8, 0.12);
    } else if (L->ep == EP_OUTRO) {
        if (b->j < 2) for (int k = 0; k < nch; k++) HIT(chords[k].start * 4, chords[k].beats * 4, chords[k], 0.9, 0.1);
        else if (b->j == 2) HIT(0, (b->n - 2) * 16, chords[0], 0.85, 0.9);
    } else if (nch > 1 || st->pushed == i) {
        for (int k = 0; k < nch; k++) HIT(chords[k].start * 4, chords[k].beats * 4, chords[k], 1, 0.08);
    } else {
        int ci = pick_comp(P->ep.comp, &st->rH); st->lastComp = ci;
        for (int k = 0; k < COMPN[ci]; k++) {
            int s = COMPS[ci][k][0], len = COMPS[ci][k][1];
            HIT(s, k == COMPN[ci] - 1 ? 16 - s : len, chords[0], k ? 0.9 : 1, 0.08);
        }
    }
    if (st->pushed == i) { if (nh) { memmove(&hits[0], &hits[1], sizeof hits[0] * (nh - 1)); nh--; } st->pushed = -1; }
    const Chord *last = &chords[nch - 1];
    if (L->ep == EP_COMP && nnext && st->map[i + 1].L.ep == EP_COMP && !same_chord(last, &nextC[0]) && rn(&st->rH) < P->ep.push) {
        if (nh && hits[nh - 1].s < 14) {
            for (int k = 0; k < nh; k++) hits[k].len = fmin(hits[k].len, 14 - hits[k].s);
            double len = 2 + nextC[0].beats * 4;
            HIT(14, len, nextC[0], 0.95, 0.08);
            st->pushed = i + 1; st->lastPush = 1;
        }
    }
#undef HIT
    for (int k = 0; k < nh; k++) {
        Spelled sp = lc_spell(P->key, &hits[k].c);
        Voicing v = lc_voice(&sp, st->hasPrevVoicing ? &st->prevVoicing : NULL, P->reg);
        st->prevVoicing = v; st->hasPrevVoicing = 1;
        double t = at_(&cx, (int)hits[k].s, 4e-3, 0);
        Ev *x = push_ev(out, t, K_EP, i);
        x->nn = v.n; memcpy(x->notes, v.m, sizeof v.m);
        x->vel = vh_(st, vel * hits[k].vs);
        x->dur = fmax(0.1, hits[k].len * sd - 0.03);
        x->strum = P->ep.strum; x->rel = hits[k].rel;
    }
    // ── drums ──
    BarHits dh = bar_hits(P, b, L->drums, L->hatsFirst, &st->rR);
    st->lastFill = dh.fill;
    for (int k = 0; k < dh.n; k++) {
        const Hit *h = &dh.h[k];
        double lag = LAGGED(h->v) ? P->snareLag : 0;
        double t = at_(&cx, h->s, SIG[h->v], lag);
        double vv = vh_(st, h->vel);
        if (L->kit) { Ev *x = push_ev(out, t, K_KICK + h->v, i); x->vel = vv; x->open = h->open; x->ghost = h->ghost; }
    }
    // ── bass: on the kicks + the changes, an approach into the next chord ──
    int bm = L->bass;
    int bassOn = bm == BS_KICK || bm == BS_WHOLE || (bm == BS_LAST && b->j == b->n - 1) || (bm == BS_OUTRO && b->j < 3);
    if (bassOn) {
        int steps[40], ns = 0, changes[12], ncg = 0;
        if (bm == BS_KICK) for (int k = 0; k < dh.n; k++) if (dh.h[k].v == V_KICK) steps[ns++] = dh.h[k].s;
        for (int k = 0; k < nch; k++) changes[ncg++] = (int)(chords[k].start * 4);
        for (int k = 0; k < ncg; k++) steps[ns++] = changes[k];
        { int u[40], nu = 0; for (int k = 0; k < ns; k++) { int d = 0; for (int q = 0; q < nu; q++) if (u[q] == steps[k]) d = 1; if (!d) u[nu++] = steps[k]; }
          isort(u, nu); memcpy(steps, u, sizeof(int) * nu); ns = nu; }
        int approach = 0;
        if (bm == BS_KICK && nnext && nextC[0].root != last->root && rn(&st->rR) < 0.5) {
            int k2 = 0; for (int k = 0; k < ns; k++) if (steps[k] < 14) steps[k2++] = steps[k]; ns = k2;
            steps[ns++] = 14; approach = 1;
        }
        if (dh.stop >= 0) { int k2 = 0; for (int k = 0; k < ns; k++) if (steps[k] < dh.stop) steps[k2++] = steps[k]; ns = k2; }
        int endStep = dh.stop >= 0 ? dh.stop : 16;
        for (int k = 0; k < ns; k++) {
            int s = steps[k];
            const Chord *c = &chords[nch - 1];
            for (int q = 0; q < nch; q++) if (s >= chords[q].start * 4 && s < (chords[q].start + chords[q].beats) * 4) { c = &chords[q]; break; }
            int rootPc = mod12(P->key.tonic + c->root);
            int root = bass_note(rootPc, st->prevBass), m = root;
            int isChange = 0; for (int q = 0; q < ncg; q++) if (changes[q] == s) isChange = 1;
            if (approach && s == 14) {
                int target = bass_note(mod12(P->key.tonic + nextC[0].root), root);
                m = rn(&st->rR) < 0.6 ? target - 1 : target + 1;
            } else if (!isChange && s % 8 != 0 && rn(&st->rR) < 0.25) {
                m = rn(&st->rR) < 0.6 ? (root + 7 <= 50 ? root + 7 : root - 5) : (root + 12 <= 52 ? root + 12 : root);
            }
            int nextS = k + 1 < ns ? steps[k + 1] : endStep;
            double dur = fmax(1, nextS - s) * sd * P->bassFactor;
            double t = at_(&cx, s, 4e-3, 3e-3);
            Ev *x = push_ev(out, t, K_BASS, i);
            x->midi = m; x->vel = vh_(st, s == 0 ? 0.92 : 0.82); x->dur = dur;
            x->slide = rn(&st->rR) < 0.15;
            st->prevBass = root;
        }
    }
    // ── lead: one motif, stated per 4-bar group, inverted + shifted in B, answered ──
    if (P->hasLead && L->lead) {
        int g = b->j / 4;
        if (!st->hasPhrase[b->si][g]) {
            int startBar = i - (b->j % 4);
            int rest = g % 2 == 1 && rn(&st->rM) < 0.3;
            Phrase ph; ph.cnt = 0;
            if (!rest) {
                LeadCtx lc = { P, st, startBar };
                Variant va = { ++st->statement, b->sec == S_B, b->sec == S_B ? 2 : 0, st->hasLastLead, st->lastLead, P->reg };
                ph = lc_phrase(&P->motif, lead_pcs_at, &lc, P->key, &st->rM, va);
            }
            if (ph.cnt) { st->lastLead = ph.n[ph.cnt - 1].midi; st->hasLastLead = 1; }
            st->phrases[b->si][g] = ph; st->hasPhrase[b->si][g] = 1;
        }
        const Phrase *ph = &st->phrases[b->si][g]; int bj = b->j % 4;
        for (int k = 0; k < ph->cnt; k++) {
            const LNote *n = &ph->n[k];
            if (n->s / 16 != bj) continue;
            const LNote *p = k ? &ph->n[k - 1] : NULL;
            int glide = p && n->s - (p->s + p->d) <= 1;
            double t = at_(&cx, n->s % 16, 8e-3, 0);
            Ev *x = push_ev(out, t, K_LEAD, i);
            x->midi = n->midi; x->vel = vh_(st, n->v); x->dur = n->d * sd * 0.95; x->glide = glide;
        }
    }
    // stable sort by time (JS Array.sort is stable)
    for (int a = 1; a < out->n; a++) { Ev x = out->e[a]; int j = a - 1; while (j >= 0 && out->e[j].t > x.t) { out->e[j + 1] = out->e[j]; j--; } out->e[j + 1] = x; }
}

#ifdef LC_DUMP
static const char *PAT[6] = { "P1","P2","P3","P4","P5","P6" };
static void pr_plan(const Plan *P) {
    printf("PLAN seed=%u next=%u title=%s key=%d/%s bpm=%d swing=%.4f lag=%.4f pat=%s form=T%d bars=%d A=%s B=%s cb=%d lead=%d soft=%d pan=%.2f reg=%d tone=%.0f wow=%.1f dust=%.2f legato=%d\n",
        P->seed, P->nextSeed, P->title, P->key.tonic, P->key.major ? "major" : "minor", P->bpm, P->swing, P->snareLag, PAT[P->pattern], P->form + 1, P->bars,
        P->progA.id, P->progB.id, P->chordBars, P->hasLead, P->leadSoft, P->leadPan + 0.0 == 0 ? 0.0 : P->leadPan, P->reg, P->tone, P->wowCents, P->dust, P->legato);
    printf("KIT %.1f %.1f %.3f %.3f %.0f %.3f %.0f EP %.0f %.2f %.3f %.4f %.3f %.2f %.2f %.2f\n", P->kit.kickF0, P->kit.kickF1, P->kit.kickDecay, P->kit.snareDecay, P->kit.hatHP, P->kit.hatClosed, P->kit.lp,
        P->ep.lp, P->ep.tremRate, P->ep.tremDepth, P->ep.strum, P->ep.vel, P->ep.comp[0], P->ep.comp[1], P->ep.comp[2]);
    printf("SECS");
    for (int i = 0; i < P->nsec; i++) { const Layers *L = &P->sec[i].L;
        printf(" %s%d[%d%d%d%d%d%d]", SEC_NAME[P->sec[i].name], P->sec[i].bars, L->ep, L->bass, L->drums, L->lead, L->hatsFirst, L->dipLast); }
    printf("\n");
}
int main(int argc, char **argv) {
    uint32_t seed = (uint32_t)strtoul(argv[1], 0, 10);
    int energy = argc > 2 ? atoi(argv[2]) : 1, band = argc > 3 ? atoi(argv[3]) : 0, city = argc > 4 ? atoi(argv[4]) : 0;
    int ntracks = argc > 5 ? atoi(argv[5]) : 1;
    Key prev; int hasPrev = 0;
    for (int tr = 0; tr < ntracks; tr++) {
        Plan P = plan_track(seed, hasPrev ? &prev : NULL, energy, band, city);
        pr_plan(&P);
        static BarState st; bar_state(&P, &st);
        static Evs ev;
        for (int i = 0; i < P.bars; i++) {
            plan_bar(&P, i, &st, &ev);
            for (int k = 0; k < ev.n; k++) { const Ev *e = &ev.e[k];
                printf("E %d %s %.9f", e->bar, K_NAME[e->k], e->t);
                if (e->k == K_FX) printf(" %s %.6f %.4f", FXN[e->fx], e->v, e->tau);
                else if (e->k == K_EP) { printf(" ["); for (int q = 0; q < e->nn; q++) printf("%s%d", q ? "," : "", e->notes[q]); printf("] %.9f %.9f %.4f %.2f", e->vel, e->dur, e->strum, e->rel); }
                else if (e->k == K_BASS) printf(" %d %.9f %.9f %d", e->midi, e->vel, e->dur, e->slide);
                else if (e->k == K_LEAD) printf(" %d %.9f %.9f %d", e->midi, e->vel, e->dur, e->glide);
                else printf(" %.9f %d %d", e->vel, e->open, e->ghost);
                printf("\n"); }
        }
        prev = P.key; hasPrev = 1; seed = P.nextSeed;
    }
    return 0;
}
#else

// ═════════════════════════════════════════════════════════════════════════════
// THE BAND — the planner's events played on our engines. This half is ours: where
// Lofi Cities hand-builds Web Audio graphs (3-op FM Rhodes, a sine+triangle bass, a
// noise-buffer kit), each part here is a modeled engine chosen fresh:
//   keys  INSTR_EPIANO Rhodes, re-voiced per track from the plan (lowpass, the suitcase
//         tremolo + autopan at the plan's rate/depth, strum, velocity)
//   bass  INSTR_BOWED pizzicato through the double-bass body — a real upright
//   kit   morphdrum.h kick + snare + INSTR_METAL hat, its knobs re-rolled per track from
//         the plan's kit (kick sweep F0→F1 + decay, snare decay, hat highpass + length)
//   rim   INSTR_MODAL — a struck bar, short and dull
//   lead  "vibes" = INSTR_MALLET with the motor on · "soft" = INSTR_PIPE breathy flute
//   mix   the plan's per-bar TONE automation rides the master lowpass (filter() is the one
//         effect built to be ridden live); tape wow/sat + echo are set per track; a vinyl
//         hiss bed + crackle ticks follow the plan's dust + vinyl swells.
// ═════════════════════════════════════════════════════════════════════════════
#define I_EP     5    // keys, short release (comping)
#define I_EPL    6    // keys, long release (the outro's held last chord)
#define I_BASS   7    // upright, plucked
#define I_BASSS  8    // upright, the slid-into note (a quick pitch scoop)
#define I_RIM    9
#define I_VIBES  10
#define I_FLUTE  11
#define I_HISS   12
#define I_CRK    13
#define KIT_BASE 20   // morphdrum slots 20..29

static MorphKit kit;
static double clk = 0;                 // our clock, seconds (summed dt — see the header)
static int    energySel = EN_BALANCED, bandSel = BAND_FULL, citySel = 1;   // tokyo
static bool   showHelp = false;
// FX TOGGLES (keys 1-5 / the buttons top-right): switch each master stage off to hear what it is doing.
// Each re-applies ONLY when flipped (set-and-hold).
enum { FXT_TONE, FXT_TAPE, FXT_BUS, FXT_TREM, FXT_VINYL, NFXT };
static const char *FXT_NAME[NFXT] = { "tone", "tape", "bus", "trem", "vinyl" };
static bool  fxOn[NFXT] = { true, true, true, true, true };
static float curWow = 0.3f;
static int   lastTone = -1;

typedef struct {
    Plan P; BarState st;
    double start;                      // clock time of bar 0
    int nextBar;
    Ev q[384]; int nq;                 // planned, not yet dispatched (time-sorted)
    bool live;
} Track;
static Track cur;
static Plan  upNext[3];                // the queue: the next three tracks of the chain
static double toneHz = 8000, toneTgt = 8000, toneTau = 0.05;
static double vinylG = 1, vinylTgt = 1, vinylTau = 0.5, dustAmt = 1;
static double epLevel = 0.71;
static int    hissH = -1;
static float  flash[12];               // per-kind hit flash for the display
static int    fillShow = F_NONE; static float fillT = 0;
static int    pushShow = 0; static float pushT = 0;
static int    songCount = 0;

static int v2vol(double v, double k) { int x = (int)lround(v * k); return x < 1 ? 1 : x > 7 ? 7 : x; }
static double ftom(double f) { return 69 + 12 * log2(f / 440.0); }
static float c01(double x) { return (float)(x < 0 ? 0 : x > 1 ? 1 : x); }

static void refresh_queue(void) {
    const Plan *prev = &cur.P;
    for (int i = 0; i < 3; i++) { upNext[i] = plan_track(prev->nextSeed, &prev->key, energySel, bandSel, citySel); prev = &upNext[i]; }
}

// per-track voicing — the planner's rolled kit/keys/lead parameters land on our engines
static void voice_track(const Plan *P) {
    // keys
    for (int s = I_EP; s <= I_EPL; s++) {
        instrument(s, INSTR_EPIANO, 2, 0, 7, s == I_EP ? 320 : 2600);
        instrument_harmonics(s, 0.10f); instrument_timbre(s, 0.36f); instrument_morph(s, 0.18f);
        instrument_filter(s, FILTER_LOW, (int)P->ep.lp, 0);
        float tr = fxOn[FXT_TREM] ? LC_TREM : 0.0f;
        instrument_lfo(s, 0, LFO_VOLUME, (float)P->ep.tremRate, (float)P->ep.tremDepth * tr);
        instrument_lfo(s, 1, LFO_PAN, (float)P->ep.tremRate, 0.25f * tr);
        instrument_reverb(s, 0.30f);
    }
    // kit: kick F0→F1 sweep + decay, snare decay, hat highpass + closed length
    float *k = kit.p[MD_KICK];
    k[MD_CHAR] = 0.2f; k[MD_LEVEL] = 1;
    k[MD_TUNE]  = c01((ftom(P->kit.kickF1) - 19) / 33.0);
    k[MD_PUNCH] = c01(12 * log2(P->kit.kickF0 / P->kit.kickF1) / 48.0);
    k[MD_SNAP]  = c01((90 - 8) / 142.0);
    k[MD_DECAY] = c01((P->kit.kickDecay * 4000 - 40) / 1060.0);
    k[MD_CUT] = 0.36f; k[MD_CLICK] = 0.22f; k[MD_SUB] = 0.22f; k[MD_DRIVE] = 0.18f;
    float *s = kit.p[MD_SNARE];
    s[MD_CHAR] = 0.5f; s[MD_LEVEL] = 1; s[MD_TUNE] = 0.32f; s[MD_DECAY] = 0.45f; s[MD_PUNCH] = 0.25f;
    s[MD_SNAP] = 0.8f; s[MD_TONE] = 0.62f; s[MD_CUT] = 0.45f; s[MD_DRIVE] = 0.1f;
    s[MD_ODEC] = c01((P->kit.snareDecay * 4000 - 30) / 390.0);
    float *h = kit.p[MD_HAT];
    h[MD_CHAR] = 0.2f; h[MD_LEVEL] = 1; h[MD_TUNE] = 0.53f; h[MD_TONE] = 0.25f; h[MD_SUB] = 0.6f; h[MD_RES] = 0.0f;
    h[MD_CUT]   = c01(log2(P->kit.hatHP / 3000.0) / 2.0);
    h[MD_DECAY] = c01((P->kit.hatClosed * 2500 - 10) / 210.0);   // their hat is a 12-20 ms tick: keep ours short
    h[MD_ODEC]  = c01((0.12 * 4000 - 80) / 720.0);
    morph_ride(&kit);
    for (int s = MDS_HC; s <= MDS_HO; s++) instrument_level(kit.base + s, 0.55f);   // after the ride: morph_apply resets hat level
    // lead
    instrument_pan(I_VIBES, (float)P->leadPan); instrument_pan(I_FLUTE, (float)P->leadPan);
    // the lead's own delay: 3 sixteenths, gentle feedback, darkened (their 3·stepDur / fb .3 / LP 2.5k)
    int dms = (int)(3 * 60000.0 / P->bpm / 4); if (dms > 1900) dms = 1900;
    echo(dms, 0.3f, 0.3f);
    // tape: the plan's wow depth (cents) → our wow; saturation + flutter held
    float wow = (float)(P->wowCents / 25.0); if (wow > 0.6f) wow = 0.6f;
    curWow = wow * LC_WOW;
    if (fxOn[FXT_TAPE]) tape(curWow, LC_FLUTTER, LC_SAT);
    dustAmt = P->dust;
    epLevel = 0.71 * band_layers(P->sec[0].L, P->band).level;
    instrument_level(I_EP, (float)epLevel); instrument_level(I_EPL, (float)epLevel);
}

static void setup_band(void) {
    morph_build(&kit, KIT_BASE);
    instrument(I_BASS, INSTR_BOWED, 3, 0, 7, 90);
    instrument(I_BASSS, INSTR_BOWED, 3, 0, 7, 90);
    for (int s = I_BASS; s <= I_BASSS; s++) {
        instrument_mode(s, MODE_BOW_PIZZ, 1.0f);
        instrument_mode(s, MODE_BOW_BODY, 0.85f);
        instrument_mode(s, MODE_BOW_SIZE, BOW_SIZE_BASS);
        instrument_harmonics(s, 0.62f); instrument_timbre(s, 0.30f); instrument_morph(s, 0.45f);
        instrument_filter(s, FILTER_LOW, 950, 0);
        instrument_level(s, 1.0f);
    }
    instrument_env(I_BASSS, 0, ENV_PITCH, 0, 60, -1.0f);     // slide: start a semitone flat, scoop up
    instrument(I_RIM, INSTR_MODAL, 0, 0, 7, 30);
    instrument_harmonics(I_RIM, 0.55f); instrument_timbre(I_RIM, 0.70f); instrument_morph(I_RIM, 0.04f);
    instrument_level(I_RIM, 0.28f);   // a MODAL strike is hot: alone it peaked at -0.3 dBFS
    instrument_filter(I_RIM, FILTER_HIGH, 300, 0);
    instrument_reverb(I_RIM, 0.30f); instrument_pan(I_RIM, -0.15f);
    instrument(I_VIBES, INSTR_MALLET, 1, 0, 7, 1200);
    instrument_harmonics(I_VIBES, 0.22f); instrument_timbre(I_VIBES, 0.45f); instrument_morph(I_VIBES, 0.85f);
    instrument_filter(I_VIBES, FILTER_LOW, 3200, 0);
    instrument(I_FLUTE, INSTR_PIPE, 14, 0, 5, 220);
    instrument_harmonics(I_FLUTE, 0.0f); instrument_timbre(I_FLUTE, 0.34f); instrument_morph(I_FLUTE, 0.68f);
    instrument_lfo(I_FLUTE, 0, LFO_PITCH, 5.0f, 0.10f);
    instrument_glide(I_FLUTE, 13);
    for (int s = I_VIBES; s <= I_FLUTE; s++) { instrument_reverb(s, 0.40f); instrument_echo(s, 0.35f); }
    instrument_reverb(KIT_BASE + MDS_SNB, 0.25f); instrument_reverb(KIT_BASE + MDS_SNN, 0.25f);
    instrument_reverb(KIT_BASE + MDS_HC, 0.08f); instrument_reverb(KIT_BASE + MDS_HO, 0.08f);
    instrument_pan(KIT_BASE + MDS_HC, 0.2f); instrument_pan(KIT_BASE + MDS_HO, 0.2f);
    // the vinyl: a pink-ish hiss bed + dust ticks
    instrument(I_HISS, INSTR_NOISE, 400, 0, 7, 400); instrument_filter(I_HISS, FILTER_HIGH, 2000, 0); instrument_level(I_HISS, 0.30f);
    instrument(I_CRK, INSTR_NOISE, 0, 5, 0, 3); instrument_filter(I_CRK, FILTER_BAND, 2600, 2); instrument_level(I_CRK, 0.60f);
    for (int s = MDS_KICK; s <= MDS_KICKS; s++) instrument_level(KIT_BASE + s, 0.55f);   // the kick sat 6 dB over the band
    reverb(0.52f, 0.55f);
    instrument_level(KIT_BASE + MDS_SNN, 0.45f); instrument_level(KIT_BASE + MDS_SNB, 0.8f);         // snare + hat transients were the mix's peaks (crest 24 dB)
    // the BUS stage (glue 0.25 + eq +1.5/+2.5 dB, the makeup their tape stage adds into a -3 dB limiter) is applied by apply_fx_toggle
}

static void apply_fx_toggle(int i) {
    switch (i) {
    case FXT_TONE:  if (fxOn[i]) lastTone = -1; else filter(FILTER_OFF, 0, 0); break;
    case FXT_TAPE:  if (fxOn[i]) tape(curWow, LC_FLUTTER, LC_SAT); else tape(0, 0, 0); break;
    case FXT_BUS:   if (fxOn[i]) { glue(0, LC_GLUE, 8, 160); eq(1.5f, 2.5f, 0.0f); } else { glue(0, 0, 8, 160); eq(0, 0, 0); } break;
    case FXT_TREM:  for (int s = I_EP; s <= I_EPL; s++) {
                        float tr = fxOn[i] ? LC_TREM : 0.0f;
                        instrument_lfo(s, 0, LFO_VOLUME, (float)cur.P.ep.tremRate, (float)cur.P.ep.tremDepth * tr);
                        instrument_lfo(s, 1, LFO_PAN, (float)cur.P.ep.tremRate, 0.25f * tr);
                    } break;
    default: break;   // vinyl is gated per frame in update()
    }
}
static void toggle_fx(int i) { fxOn[i] = !fxOn[i]; apply_fx_toggle(i); }

// the drum voices, fired at a planned velocity (morph_fire only takes a coarse boost)
static void fire_kick(double d, double vel) {
    MDRes r; md__resolve(&kit, MD_KICK, &r); int b = kit.base, v = v2vol(vel, 7.6);
    schedule_at(d, r.midi, b + MDS_KICK, v, r.dec);
    if (r.l1_vol) schedule_at(d, 60, b + MDS_KICKC, (r.l1_vol * v + 6) / 7, r.l1_dec);
    if (r.l2_vol) schedule_at(d, r.midi - 12, b + MDS_KICKS, (r.l2_vol * v + 6) / 7, r.l2_dec);
}
static void fire_snare(double d, double vel) {
    MDRes r; md__resolve(&kit, MD_SNARE, &r); int b = kit.base, v = v2vol(vel, 7.6);
    int body = (int)lround((1 - r.tone) * v * 1.5), snpy = v;   // both layers ride the velocity (morph_fire pins the noise)
    if (body > 7) body = 7;
    if (body) { schedule_at(d, r.midi, b + MDS_SNB, body, r.dec); schedule_at(d, r.midi + 10, b + MDS_SNB, body, r.dec); }
    schedule_at(d, 60, b + MDS_SNN, snpy ? snpy : 1, r.l1_dec);
}
static void fire_hat(double d, double vel, int open) {
    MDRes r; md__resolve(&kit, MD_HAT, &r); int b = kit.base, v = v2vol(vel, 7.6);
    schedule_at(d, r.midi, b + (open ? MDS_HO : MDS_HC), v, (open ? r.l2_dec : r.dec) * 6);
}

static void dispatch(Track *T, const Ev *e) {
    double at = T->start + e->t;
    double d = at;   // an ABSOLUTE audio_time(): schedule_at lands it on its sample (see the header)
    switch (e->k) {
    case K_EP: {
        int slot = e->rel > 0.5 ? I_EPL : I_EP;
        for (int k = 0; k < e->nn; k++) {
            double dt = e->nn > 1 ? k * e->strum / (e->nn - 1) : 0;
            double v = k == e->nn - 1 ? fmin(1, e->vel * 1.1) : e->vel;
            schedule_at(d + dt, e->notes[k], slot, v2vol(v, 10.5), (int)((e->dur - dt) * 1000));
        }
        flash[K_EP] = 1; break;
    }
    case K_BASS: schedule_at(d, e->midi, e->slide ? I_BASSS : I_BASS, v2vol(e->vel, 8.6), (int)(e->dur * 1000)); flash[K_BASS] = 1; break;
    case K_KICK: fire_kick(d, e->vel); flash[K_KICK] = 1; break;
    case K_SNARE: fire_snare(d, e->vel); flash[K_SNARE] = 1; break;
    case K_HAT: fire_hat(d, e->vel, e->open); flash[K_HAT] = 1; break;
    case K_RIM: schedule_at(d, 77, I_RIM, v2vol(e->vel, 7.6), 60); flash[K_RIM] = 1; break;
    case K_LEAD:
        if (T->P.hasLead) schedule_at(d, e->midi, T->P.leadSoft ? I_FLUTE : I_VIBES, v2vol(e->vel, 8.4), (int)(e->dur * 1000));
        flash[K_LEAD] = 1; break;
    case K_FX:
        switch (e->fx) {
        case FX_TONE_:  toneTgt = e->v; toneTau = e->tau; break;
        case FX_VINYL_: vinylTgt = e->v; vinylTau = e->tau; break;
        case FX_DUST_:  dustAmt = e->v; break;
        case FX_WOW_:   break;   // per-track only: tape() rebuilds its DSP, so the 2-bar wow drift is not ridden
        case FX_LEVEL_: epLevel = 0.71 * e->v; instrument_level(I_EP, (float)epLevel); instrument_level(I_EPL, (float)epLevel); break;
        }
        break;
    default: break;   // clap/shaker/tom/block — the styles use them; jazzhop's grids don't
    }
}
// the fx events land on the frame their time arrives (they are automation, not notes)
static void insert_ev(Track *T, const Ev *e) {
    if (T->nq >= (int)(sizeof T->q / sizeof *T->q)) return;
    int j = T->nq++;
    while (j > 0 && T->q[j - 1].t > e->t) { T->q[j] = T->q[j - 1]; j--; }
    T->q[j] = *e;
}
static void build_track(const Plan *P, double t0) {
    memset(&cur, 0, sizeof cur);
    cur.P = *P; bar_state(&cur.P, &cur.st);
    cur.start = t0; cur.live = true;
    voice_track(&cur.P);
    refresh_queue();
    songCount++;
}
static void start_seed(uint32_t seed, const Key *prevKey, double t0) {
    Plan P = plan_track(seed, prevKey, energySel, bandSel, citySel);
    build_track(&P, t0);
}
static void skip_next(void) {
    // their skip: fade out, dip the tone to 400 Hz, vinyl swell, the next intro at +0.6 s
    Plan nx = upNext[0];
    note_off_all(); hissH = -1;
    toneTgt = 400; toneTau = 0.13; vinylTgt = VINYL_BOOST; vinylTau = 0.3;
    build_track(&nx, clk + 0.6);
}
#define LOOK 0.1
static void advance(void) {
    if (!cur.live) return;
    double until = clk + LOOK, barDur = 16 * 60.0 / cur.P.bpm / 4;
    if (cur.nq && cur.start + cur.q[0].t < clk - 0.05) {           // a hitch: re-anchor, drop what's late
        int bar = cur.q[0].bar, k = 0;
        for (int i = 0; i < cur.nq; i++) if (cur.q[i].bar > bar) cur.q[k++] = cur.q[i];
        cur.nq = k; cur.start = clk + 0.1 - (bar + 1) * barDur;
    }
    while (cur.nextBar < cur.P.bars && cur.start + cur.nextBar * barDur < until + LOOK) {
        static Evs ev; plan_bar(&cur.P, cur.nextBar++, &cur.st, &ev);
        for (int i = 0; i < ev.n; i++) insert_ev(&cur, &ev.e[i]);
        if (cur.st.lastFill != F_NONE) { fillShow = cur.st.lastFill; fillT = 1.6f; }
        if (cur.st.lastPush) { pushShow = 1; pushT = 1.2f; }
    }
    int k = 0;
    while (k < cur.nq && cur.start + cur.q[k].t < until) dispatch(&cur, &cur.q[k++]);
    if (k) { memmove(cur.q, cur.q + k, sizeof cur.q[0] * (cur.nq - k)); cur.nq -= k; }
    if (cur.nextBar >= cur.P.bars && cur.nq == 0) {
        double end = cur.start + cur.P.bars * barDur, nextStart = end + 0.6 * barDur;
        if (nextStart < until + LOOK) { Plan nx = upNext[0]; build_track(&nx, nextStart); }
    }
}
static void set_band(int band) {
    bandSel = band; cur.P.band = band;
    for (int i = cur.nextBar; i < cur.st.nbars; i++) cur.st.map[i].L = band_layers(cur.P.sec[cur.st.map[i].si].L, band);
    refresh_queue();
}

// ── display helpers ──
static int now_bar(void) {
    double barDur = 16 * 60.0 / cur.P.bpm / 4; int b = (int)floor((clk - cur.start) / barDur);
    return b < 0 ? -1 : b >= cur.P.bars ? cur.P.bars - 1 : b;
}
static void chord_name(const Plan *P, const Chord *c, char *o, int n) {
    snprintf(o, n, "%s%s", NOTE_NAMES[mod12(P->key.tonic + c->root)], QUAL[c->q].name);
}
static const char *layer_ep(int m) { static const char *N[] = { "comp", "intro", "whole", "outro" }; return N[m]; }
static const char *layer_bass(int m) { static const char *N[] = { "on kick", "last bar", "--", "whole", "outro" }; return N[m]; }
static const char *layer_drums(const Plan *P, int m) {
    switch (m) { case DR_PATTERN: return GRIDS[P->pattern].name; case DR_HATS2: return "hats in";
    case DR_NONE: return "--"; case DR_P5: return "rim break"; default: return "outro"; }
}

void update(void) {
    static bool booted = false;
    if (!booted) {
        setup_band();
        uint32_t seed = LOFI_SEED ? LOFI_SEED : (uint32_t)(rnd(1 << 30)) * 4u + (uint32_t)rnd(4);
        clk = 0;
        start_seed(seed, NULL, 0.15);
        apply_fx_toggle(FXT_BUS);
        booted = true;
    }
    // the clock IS the sound clock: audio_time() counts the samples actually rendered, and every note
    // is booked on it with schedule_at, so a stalled frame or a callback boundary cannot move a note
    clk = audio_time();
    // ── input (their keys: N next · V vibe · C city) ──
    if (keyp('N') || keyp(KEY_SPACE)) skip_next();
    if (keyp('E')) { energySel = (energySel + 1) % NENERGY; refresh_queue(); }     // from the next track
    if (keyp('B')) set_band((bandSel + 1) % NBAND);                                  // from the next bar
    if (keyp('C')) { citySel = (citySel + 1) % LC_NCITY; refresh_queue(); }
    if (keyp('H')) showHelp = !showHelp;
    for (int i = 0; i < NFXT; i++) if (keyp('1' + i)) toggle_fx(i);
    advance();
    // the master tone + vinyl, ridden like setTargetAtTime (first-order, per frame)
    double a = 1 - exp(-dt() / fmax(0.005, toneTau)); toneHz += (toneTgt - toneHz) * a;
    int th = (int)toneHz;
    if (fxOn[FXT_TONE] && abs(th - lastTone) > 2) { filter(FILTER_LOW, (float)th, 0.05f); lastTone = th; }
    vinylG += (vinylTgt - vinylG) * (1 - exp(-dt() / fmax(0.005, vinylTau)));
    if (hissH < 0) hissH = note_on(60, I_HISS, 0);
    note_vol(hissH, fxOn[FXT_VINYL] ? (float)(2.2 * vinylG) : 0.0f);
    if (fxOn[FXT_VINYL] && rnd_float() < dustAmt * vinylG * 9.0 * dt()) schedule_hit(rnd(16), 60 + rnd(24), I_CRK, 1 + rnd(3), 3);
    for (int i = 0; i < 12; i++) flash[i] *= 0.86f;
    if (fillT > 0) fillT -= dt(); if (pushT > 0) pushT -= dt();
#ifdef DE_TRACE
    int bb = now_bar();
    watch("song", "%d", songCount);
    watch("title", "%s", cur.P.title);
    watch("bar", "%d", bb);
    watch("section", "%s", bb >= 0 ? SEC_NAME[cur.st.map[bb].sec] : "-");
    watch("tone", "%d", (int)toneHz);
#endif
}

// ── the scene: a night city by the water, lit windows breathing with the band ──
static void draw_city(float t) {
    static const int SKY[5] = { CLR_BLACK, CLR_DARKER_BLUE, CLR_DARK_BLUE, CLR_DARKER_PURPLE, CLR_DARK_PURPLE };
    for (int i = 0; i < 5; i++) rectfill(0, i * 18, 320, 18, SKY[i]);
    for (int i = 0; i < 40; i++) {                                   // stars
        int x = (i * 73 + 11) % 320, y = (i * 37 + 5) % 60;
        if (((int)(t * 1.3f) + i) % 7) pset(x, y, i % 3 ? CLR_INDIGO : CLR_LIGHT_PEACH);
    }
    // three skyline layers, far → near; windows light up with the keys + bass
    for (int layer = 0; layer < 3; layer++) {
        int base = 88 + layer * 4, col = layer == 0 ? CLR_DARKER_PURPLE : layer == 1 ? CLR_DARKER_BLUE : CLR_BLACK;
        unsigned hsh = 2166136261u ^ (unsigned)(layer * 977 + citySel * 131);
        for (int x = -4; x < 320;) {
            hsh = hsh * 16777619u + 7; int w = 10 + (int)(hsh % 18); hsh = hsh * 16777619u + 7;
            int h = 14 + (int)(hsh % (26 + layer * 12));
            rectfill(x, base - h, w, h, col);
            if (layer) for (int wy = base - h + 3; wy < base - 3; wy += 4) for (int wx = x + 2; wx < x + w - 2; wx += 3) {
                unsigned q = (unsigned)(wx * 73856093) ^ (unsigned)(wy * 19349663) ^ (unsigned)(layer * 83492791 + citySel);
                q ^= q >> 13; q *= 0x5bd1e995u; q ^= q >> 15;
                if (q % 4 == 0) {
                    float glow = (q % 3 == 0 ? flash[K_EP] : q % 3 == 1 ? flash[K_BASS] : flash[K_LEAD]);
                    pset(wx, wy, glow > 0.4f && q % 2 ? CLR_LIGHT_YELLOW : (q % 7 ? CLR_DARK_ORANGE : CLR_PEACH));
                }
            }
            x += w + 1;
        }
    }
    rectfill(0, 96, 320, 12, CLR_DARKER_BLUE);                       // the water, with reflections
    for (int i = 0; i < 24; i++) {
        int x = (i * 53 + (int)(t * 6)) % 320, y = 98 + (i * 5) % 9;
        line(x, y, x + 3 + i % 4, y, i % 3 ? CLR_DARK_ORANGE : CLR_DARK_BLUE);
    }
}

void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    float t = timer();
    draw_city(t);
    const Plan *P = &cur.P;
    int bar = now_bar(); if (bar < 0) bar = 0;   // before the first downbeat, show bar 1
    // title card
    font(FONT_NORMAL);
    print(P->title, 8, 6, CLR_LIGHT_PEACH);
    font(FONT_SMALL);
    print(str("%s  -  %s %s  -  %d bpm  -  swing %.0f%%", LC_CITY[P->city].name, NOTE_NAMES[P->key.tonic], P->key.major ? "major" : "minor", P->bpm, P->swing),
          8, 17, CLR_PEACH);
    print(str("A %s   B %s   %s", P->progA.id, P->progB.id, P->chordBars == 2 ? "2 bars/chord" : "1 bar/chord"), 8, 25, CLR_INDIGO);
    print("style: jazzhop", 312 - text_width("style: jazzhop"), 25, CLR_DARK_ORANGE);
    font(FONT_NORMAL);

    // ── the FORM strip — every section, sized by its bars, the playhead crawling through ──
    int fx = 8, fw = 304, fy = 112, fh = 14;
    rectfill(fx - 2, fy - 10, fw + 4, fh + 50, CLR_BROWNISH_BLACK);
    static const int SC[5] = { CLR_DARK_BLUE, CLR_TRUE_BLUE, CLR_MAUVE, CLR_DARK_PURPLE, CLR_DARKER_BLUE };
    int x = fx, acc = 0;
    for (int i = 0; i < P->nsec; i++) {
        int w = (int)lround((double)(acc + P->sec[i].bars) * fw / P->bars) - (int)lround((double)acc * fw / P->bars);
        bool on = bar >= 0 && cur.st.map[bar].si == i;
        rectfill(x, fy, w - 1, fh, on ? CLR_PEACH : SC[P->sec[i].name]);
        font(FONT_SMALL);
        const char *nm = P->sec[i].name == S_BREAK ? "brk" : P->sec[i].name == S_INTRO ? "in" : P->sec[i].name == S_OUTRO ? "out" : SEC_NAME[P->sec[i].name];
        if (text_width(nm) < w - 2) print(nm, x + 2, fy + 4, on ? CLR_BLACK : CLR_LIGHT_PEACH);
        font(FONT_NORMAL);
        x += w; acc += P->sec[i].bars;
    }
    if (bar >= 0) {
        double barDur = 16 * 60.0 / P->bpm / 4, pos = (clk - cur.start) / barDur;
        int px = fx + (int)(pos * fw / P->bars); if (px > fx + fw) px = fx + fw;
        line(px, fy - 3, px, fy + fh + 2, CLR_WHITE);
    }
    font(FONT_SMALL);
    print(str("bar %d/%d", bar + 1, P->bars), fx, fy - 8, CLR_INDIGO);
    print(str("form T%d", P->form + 1), fx + fw - 34, fy - 8, CLR_INDIGO);

    // ── what each part is doing right now (the section's LAYERS) ──
    if (bar >= 0) {
        const BarInfo *b = &cur.st.map[bar]; const Layers *L = &b->L;
        int ly = fy + fh + 6;
        struct { const char *lab, *val; int k; bool on; } R[4] = {
            { "keys",  layer_ep(L->ep), K_EP, true },
            { "bass",  layer_bass(L->bass), K_BASS, L->bass != BS_NONE },
            { "drums", L->kit ? layer_drums(P, L->drums) : "off", K_KICK, L->kit && L->drums != DR_NONE },
            { "lead",  !P->hasLead ? "none" : L->lead ? (P->leadSoft ? "flute" : "vibes") : "rest", K_LEAD, P->hasLead && L->lead },
        };
        for (int i = 0; i < 4; i++) {
            int cx = fx + i * 76;
            float fl = R[i].k == K_KICK ? fmaxf(flash[K_KICK], fmaxf(flash[K_SNARE], flash[K_HAT])) : flash[R[i].k];
            circfill(cx + 3, ly + 3, 2, R[i].on ? (fl > 0.3f ? CLR_LIGHT_YELLOW : CLR_DARK_ORANGE) : CLR_DARKER_PURPLE);
            print(R[i].lab, cx + 9, ly, CLR_INDIGO);
            print(R[i].val, cx + 9, ly + 8, R[i].on ? CLR_LIGHT_PEACH : CLR_DARK_PURPLE);
        }
        // the chord under the playhead + what's next
        Chord cs[12]; int n = bar_chords(P, &cur.st, bar, cs);
        double barDur = 16 * 60.0 / P->bpm / 4, beatIn = fmod((clk - cur.start) / barDur, 1.0) * 4;
        int ci = 0; for (int q = 0; q < n; q++) if (beatIn >= cs[q].start) ci = q;
        char nm[16]; chord_name(P, &cs[ci], nm, sizeof nm);
        font(FONT_NORMAL);
        print(nm, fx, ly + 22, CLR_WHITE);
        font(FONT_SMALL);
        print(str("%s  %d/%d", SEC_NAME[b->sec], b->j + 1, b->n), fx + text_width(nm) + 40, ly + 24, CLR_PEACH);
        if (fillT > 0) print(str("fill: %s", FILL_NAME[fillShow]), fx + 150, ly + 24, CLR_YELLOW);
        if (pushT > 0) print("push!", fx + 230, ly + 24, CLR_PINK);
    }

    // ── the VIBE: energy (next track) · band (next bar) · city (titles) · next ──
    int by = 178; font(FONT_SMALL);
    if (ui_button(8, by, 70, 14, str("E %s", ENERGY_NAME[energySel]))) { energySel = (energySel + 1) % NENERGY; refresh_queue(); }
    if (ui_button(82, by, 78, 14, str("B %s", BAND_NAME[bandSel]))) set_band((bandSel + 1) % NBAND);
    if (ui_button(164, by, 78, 14, str("C %s", LC_CITY[citySel].name))) { citySel = (citySel + 1) % LC_NCITY; refresh_queue(); }
    if (ui_button(246, by, 66, 14, "N next >>")) skip_next();
    {   // the fx toggles, top-right: a struck-through label = that stage is OFF
        int bx = 312 - NFXT * 32 + 2;
        for (int i = 0; i < NFXT; i++, bx += 32) {
            if (ui_button(bx, 36, 30, 11, FXT_NAME[i])) toggle_fx(i);
            if (!fxOn[i]) line(bx + 2, 41, bx + 27, 41, CLR_PINK);
        }
    }
    print(str("up next: %s  (%s %s)", upNext[0].title, NOTE_NAMES[upNext[0].key.tonic], upNext[0].key.major ? "maj" : "min"), 8, 168, CLR_INDIGO);
    font(FONT_NORMAL);
    if (showHelp) {
        rectfill(40, 40, 240, 92, CLR_BROWNISH_BLACK); rect(40, 40, 240, 92, CLR_PEACH);
        font(FONT_SMALL);
        static const char *H[] = {
            "LOFI CITY - endless generated lofi, the Lofi Cities way",
            "",
            "N / SPACE   next track (their skip: tone dips, vinyl swells)",
            "E           energy chill / balanced / upbeat (next track)",
            "B           band full / no drums / chords only (next bar)",
            "C           city - the words the titles are made of",
            "1-5         fx off/on: tone . tape . bus . trem . vinyl",
            "H           this help",
            "",
            "each track = one seed: key (moves to a related key),",
            "2 progressions, a form (T1-T3), per-section layers,",
            "fills, pushes, bass approaches, a motif lead.",
        };
        for (int i = 0; i < 11; i++) print(H[i], 46, 46 + i * 7, i == 0 ? CLR_PEACH : CLR_LIGHT_PEACH);
        font(FONT_NORMAL);
    }
    ui_end();
}

#ifdef DE_SPEC
// Known answers taken from the site's OWN planner (their bundle run headless in node,
// 2026-09-28) — so a regression in the port shows up as a different track, not a vibe.
void spec(void) {
    Plan P = plan_track(12345, NULL, EN_BALANCED, BAND_FULL, 0);
    expect(!strcmp(P.title, "the quais city lights"), "seed 12345 in paris is 'the quais city lights'");
    expect_eq(P.key.tonic, 2, "seed 12345 is in D");
    expect_eq(P.key.major, 1, "... major");
    expect_eq(P.bpm, 78, "at 78 bpm");
    expect_eq(P.bars, 52, "52 bars long");
    expect_eq(P.form, 0, "form T1");
    expect(!strcmp(P.progA.id, "M1") && !strcmp(P.progB.id, "M1+ii-V"), "A = M1, B = M1 with the ii-V turnaround");
    expect_eq(P.pattern, P6, "groove P6 (shuf)");
    expect_eq(P.nextSeed, 2748599489u, "the chain's next seed");
    BarState st; bar_state(&P, &st); Evs ev; int n = 0, eps = 0;
    for (int i = 0; i < P.bars; i++) { plan_bar(&P, i, &st, &ev); n += ev.n; for (int k = 0; k < ev.n; k++) eps += ev.e[k].k == K_EP; }
    expect_eq(n, 1264, "1264 events across the track");
    expect_eq(eps, 80, "80 keys hits");
    // a track WITH a lead, and the chain into the next (the related-key move)
    Plan L = plan_track(999, NULL, EN_BALANCED, BAND_FULL, 1);
    expect(!strcmp(L.title, "last noodle bar, shibuya"), "seed 999 in tokyo is 'last noodle bar, shibuya'");
    expect(L.hasLead && !L.leadSoft, "... with a vibes lead");
    BarState s2; bar_state(&L, &s2); int nl = 0, all = 0, firstBar = -1, firstMidi = -1;
    for (int i = 0; i < L.bars; i++) { plan_bar(&L, i, &s2, &ev); all += ev.n;
        for (int k = 0; k < ev.n; k++) if (ev.e[k].k == K_LEAD) { if (firstBar < 0) { firstBar = i; firstMidi = ev.e[k].midi; } nl++; } }
    expect_eq(all, 802, "802 events");
    expect_eq(nl, 37, "37 lead notes");
    expect_eq(firstBar, 10, "the lead enters at bar 10 (the second A)");
    expect_eq(firstMidi, 80, "on midi 80");
    Plan N = plan_track(L.nextSeed, &L.key, EN_BALANCED, BAND_FULL, 1);
    expect(!strcmp(N.title, "lost ginza at midnight"), "the next track is 'lost ginza at midnight'");
    expect(N.key.tonic == 8 && N.key.major && N.bpm == 87, "in Ab major at 87 bpm");
}
#endif
#endif
