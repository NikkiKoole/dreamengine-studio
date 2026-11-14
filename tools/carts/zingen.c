/* de:meta
{
  "slug": "zingen",
  "title": "zingen",
  "status": "active",
  "created": "2026-11-13",
  "kind": ["instrument", "probe"],
  "teaches": ["generative-melody"],
  "lineage": "The first cart on lyric.h, the sung-words layer for INSTR_VOICE (idea after JIANT FM's VOICE/WORDS, M-VAVE FM-1 firmware, GPL-3.0; ideas only, no code; the Dutch rules are ours). Follows singab, the A/B probe that motivated the de-hiss pass.",
  "description": {
    "summary": "The formant voice sings in six languages, and whatever you type: Dutch (read by spelling rules: long and short vowels, schwa, ch/g, ui/ij/eu, final devoicing), Mipomi (the playtime Mipos' language), Japanese romaji, Spanish, Swahili and Hawaiian. One syllable per note, with voice knobs and one-tap characters (chipmunk, monster, robot, alien...).",
    "detail": "lyric.h reads the text into syllables; each note starts the next one: its consonants at note-on, the vowel held while the note is, then the diphthong's second half and the final consonants chained at release ('nacht' = x then t), before the note lets go. The melody is made from the words: a word's last syllable lasts longer, every fourth word breathes. Or play it yourself: every key press sings the next syllable. The bottom line shows the phonemes it read.",
    "controls": "SPACE = auto-sing on/off · A S D F G H J K = sing the next syllable on that note · 1 2 3 = Dutch lyrics, 4 = a Mipomi lullaby, 5 Japanese, 6 Spanish, 7 Swahili, 8 Hawaiian, 9 SCAT (writes its own scat: swung runs, stutters, call and response, held notes, rolls, over a walking bass and snaps; B = band on/off; try the JAZZ voice), 0 = your words · ENTER = type your own line (TAB cycles Dutch, Mipomi, Japanese romaji, Spanish, Swahili, Hawaiian; an apostrophe is the okina), ENTER again to hear it sung (it is remembered) · V = formant model (Klatt cascade / parallel) · knobs: SIZE (giant .. child) · BUZZ (relaxed and round .. pressed and bright; also UP/DOWN) · VIBRATO · GLIDE · LIFE (a machine .. shaky) · PITCH (±12 semitones) · WHISPER (sung → whispered: no pitch, the words stay) · NASAL (open → hummed, through the nose) · GRAVEL (clean → a rasp) · characters (one tap): CLEAN, CHIPMUNK, MONSTER, ROBOT, ALIEN, PHONE, BULLHORN, JAZZ (a gravelly jazz singer that scoops into notes); each moves the knobs it needs and adds its own effects · DEFAULT = CLEAN · L = singer layer on/off (vibrato that blooms in, a living pitch, sung glides) · R = back to the start"
  }
}
de:meta */
#include "studio.h"
#include "lyric.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

// ZINGEN — INSTR_VOICE sings through lyric.h: three Dutch lyrics (our own lines) on a melody made from
// the words, and one MIPOMI song (the playtime Mipos' language, real words from mipomi-lang's
// words.tsv) on its own written lullaby melody. And YOUR words: ENTER opens a text box, type any line
// (Dutch or Mipomi, TAB switches), ENTER again and it sings it. The line is saved for next time.
#define SLOT 5
#define BASS_SLOT 6                  // scat's band: a pizzicato upright (INSTR_BOWED, the upright.c recipe)
#define SNAP_SLOT 7                  // and finger snaps (band-passed noise)

// a written melody: {midi, steps}; midi 0 = a rest. One note per syllable, in order.
typedef struct { unsigned char midi, len; } MNote;
// "Fala, Foni" (sun, moon), in Mipomi:
//   mipo fala, fala a foko            hello sun, the sun goes up
//   mibi i mola, mibi o jojo u jo     we are happy, we sing with you
//   fono, foni a foko                 sunset, the moon goes up
//   mi i nini, mi i momo u jo         I'm very sleepy, I'm in love with you
//   mipo foni, pomo, ni -             bye moon, thank you, sleep...
static const MNote FALA_FONI[] = {
    {67,2},{69,2},{67,2},{64,6},{0,2}, {67,2},{69,2},{72,4},{71,2},{67,6},{0,4},
    {64,2},{67,2},{69,4},{67,2},{64,6},{0,2}, {64,2},{67,2},{69,4},{72,2},{71,2},{69,2},{67,8},{0,4},
    {69,2},{65,6},{0,2}, {69,2},{72,2},{74,4},{72,2},{69,6},{0,4},
    {67,2},{64,4},{62,2},{60,6},{0,2}, {67,2},{69,4},{67,2},{64,2},{62,2},{64,8},{0,4},
    {67,2},{64,2},{62,2},{60,6},{0,2}, {64,2},{62,4},{0,2}, {64,6},{60,10},{0,12},
};
typedef struct { const char *name; int lang; const char *text; const MNote *mel; int nmel; int step_fr; int scat; } Song;
static char scat_text[1024];         // the scat song writes its own words (scat_gen)
#define USER_MAX 200
static char user_text[USER_MAX] = "typ hier je eigen woorden en ik zing ze";
static Song SONGS[] = {
    { "zing voor jou", LYR_NL, "ik zing voor jou, de zon gaat op, de nacht is lang en ik blijf hier", 0, 0, 9 },
    { "kunst",         LYR_NL, "kunst en liefde, vrolijk en eeuwig, in het licht van de straat", 0, 0, 9 },
    { "de zee",        LYR_NL, "de zee is groen, de lucht is grijs, ik loop - naar huis door de regen", 0, 0, 9 },
    { "fala, foni (mipomi)", LYR_MIPOMI,
      "mipo fala, fala a foko, mibi i mola, mibi o jojo u jo, fono, foni a foko, "
      "mi i nini, mi i momo u jo, mipo foni, pomo, ni -",
      FALA_FONI, (int)(sizeof FALA_FONI / sizeof FALA_FONI[0]), 11 },
    // three more languages, our own lines (each with its meaning):
    // "cherry blossoms bloom, they sway in the wind, I sing with you"
    { "sakura (nihongo)", LYR_JA, "sakura no hana ga saite, kaze ni yureru, kimi to utau yo", 0, 0, 9 },
    // "I sing with you under the moon, the sea and the wind dance with me"
    { "la luna (espanol)", LYR_ES, "canto contigo bajo la luna, el mar y el viento bailan conmigo", 0, 0, 9 },
    // "the sun is shining, the birds are singing, my friend, come let's play"
    { "jua (kiswahili)", LYR_SW, "jua linawaka, ndege wanaimba, rafiki yangu, njoo tucheze", 0, 0, 9 },
    // "greetings, O sun, greetings, O moon, we are singing, with love"
    { "aloha (hawaii)", LYR_HAW, "aloha e ka lā, aloha e ka mahina, ke mele nei mākou, me ke aloha", 0, 0, 9 },
    // SCAT: no words, the generator writes them (fresh phrases each time round), sung fast and swung
    { "scat!", LYR_ES, scat_text, 0, 0, 3, 1 },
    { "your words", LYR_NL, user_text, 0, 0, 9 },
};
#define NSONGS ((int)(sizeof SONGS / sizeof SONGS[0]))
#define USER   (NSONGS - 1)   // the last entry is always "your words"

// the typed line: a text box over the cart (ENTER opens it, ENTER sings it, TAB = language)
static int typing = 0, tlang = LYR_NL, bs_held = 0;
static char tbuf[USER_MAX];
typedef struct { int ver, lang; char text[USER_MAX]; } Saved;   // save_bytes: the last line you typed
static int mel_i = 0;
static const int NOTES[8] = { 60, 62, 64, 67, 69, 72, 74, 76 };   // C pentatonic-ish, two octaves of keys
static const int CONTOUR[16] = { 2, 3, 4, 3, 2, 1, 2, 4, 5, 4, 3, 2, 1, 0, 1, 2 };

static Lyric lyr;
static Singer sing;
static int which = 0, autop = 1;
static int fr = 0, note_left = 0, syl_i = 0, rest_left = 0, words = 0;
static int keyheld = -1;
static int casc = 1;                 // V: the voice's formant model, 1 = Klatt cascade (the Singer's default), 0 = parallel

static int sung = 1;                 // L: the singer layer (delayed vibrato, jitter/shimmer, legato glides) on/off, to A/B

// THE VOICE KNOBS (0..1 each; DEFAULT puts them all back). Each maps onto one Singer setting.
enum { K_SIZE, K_BUZZ, K_VIB, K_GLIDE, K_LIFE, K_PITCH, K_WHISPER, K_NASAL, K_GRAVEL, K_COUNT };
static const char *K_NAME[K_COUNT] = { "SIZE", "BUZZ", "VIBRATO", "GLIDE", "LIFE", "PITCH", "WHISPER", "NASAL", "GRAVEL" };
static const float K_DEF[K_COUNT]  = { 0.33f, 0.5f, 0.375f, 0.35f, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f };
static float kv[K_COUNT];
static int transpose = 0;            // PITCH: semitones, -12 .. +12
static float vib_hz = 5.5f;          // vibrato rate (a character sets it; no knob)
static float vib_delay = 0.25f;      // how long a note stays straight before its vibrato blooms (a character sets it)
static int scoop_st = 0;             // a character's scoop: notes after a rest slide up into place from this far below

static void apply_voice(void) {
    singer_size(&sing, kv[K_SIZE]);                       // vocal tract: 0 giant .. 1 child
    singer_buzz(&sing, kv[K_BUZZ]);                       // relaxed and round .. pressed and bright
    sing.vib    = sung ? kv[K_VIB] * 0.8f : 0.0f;         // vibrato depth (blooms in after the note starts)
    sing.vib_hz = vib_hz;
    sing.glide  = sung ? kv[K_GLIDE] * 0.2f : 0.0f;       // legato slide time, 0 .. 0.2 s
    sing.human  = sung ? kv[K_LIFE] * 2.0f : 0.0f;        // jitter + shimmer, 0 (a machine) .. 2 (shaky)
    transpose   = (int)((kv[K_PITCH] - 0.5f) * 24.0f + (kv[K_PITCH] >= 0.5f ? 0.5f : -0.5f));
    singer_whisper(&sing, kv[K_WHISPER]);                 // sung .. breathy .. whispered (no pitch, the words stay)
    singer_nasal(&sing, kv[K_NASAL]);                     // open .. hummed, through the nose
    singer_gravel(&sing, kv[K_GRAVEL]);                   // clean .. gravelly (uneven, doubled cycles + rasp)
    sing.vib_delay = vib_delay;
    sing.scoop  = sung ? scoop_st : 0;
}

// CHARACTERS: one tap turns the singer into someone else. Each sets the voice knobs (they move, so you
// see what it did and can keep turning) plus its own effects on the slot. Applied only on a tap
// (set-and-hold: every effect call reconfigures DSP).
typedef struct {
    const char *name;
    float k[K_COUNT];                    // SIZE BUZZ VIBRATO GLIDE LIFE PITCH WHISPER NASAL GRAVEL
    float vib_hz;
    int filt, cut, res;                  // instrument_filter
    float drive; int dmode;              // instrument_drive / _mode
    float ring_hz, ring_mix;             // instrument_ringmod
    float fl_rate, fl_depth, fl_fb, fl_mix;   // instrument_flanger
    float level;                         // instrument_level: evens the characters out (measured RMS vs CLEAN)
    int scoop; float vib_delay;          // slide up into notes from this many semitones below; vibrato waits this long
} Character;
static const Character CHARS[] = {
    //            SIZE   BUZZ   VIB    GLIDE  LIFE   PITCH  WHISP NASAL   vib   filter               drive        ringmod       flanger
    { "CLEAN",    { 0.33f, 0.50f, 0.375f,0.35f, 0.50f, 0.5f, 0.0f, 0.0f, 0.0f },   5.5f, FILTER_OFF, 1000, 0, 0.0f,  DRIVE_SOFT, 0, 0.0f,    0, 0, 0, 0.0f , 1.0f , 0, 0.25f },
    // pitched up an octave AND a small throat: the sped-up-tape cartoon
    { "CHIPMUNK", { 0.95f, 0.60f, 0.25f, 0.50f, 0.50f, 1.0f, 0.0f, 0.2f, 0.0f },   7.0f, FILTER_OFF, 1000, 0, 0.0f,  DRIVE_SOFT, 0, 0.0f,    0, 0, 0, 0.0f , 0.32f , 0, 0.25f },
    // down an octave, a giant's throat, pressed and rough, a little growl, the top shaved off
    { "MONSTER",  { 0.00f, 0.95f, 0.15f, 0.40f, 0.90f, 0.0f, 0.15f, 0.0f, 0.5f },   4.0f, FILTER_LOW, 2500, 2, 0.40f, DRIVE_SOFT, 0, 0.0f,    0, 0, 0, 0.0f , 1.0f , 0, 0.25f },
    // dead flat (no vibrato, glide or life), pressed, ring-modulated at 55 Hz, a metallic flanger
    { "ROBOT",    { 0.33f, 1.00f, 0.00f, 0.00f, 0.00f, 0.5f, 0.0f, 0.35f, 0.0f },   5.5f, FILTER_OFF, 1000, 0, 0.0f,  DRIVE_SOFT, 55, 0.75f,  0.1f, 0.2f, 0.6f, 0.4f , 1.0f , 0, 0.25f },
    // up a fourth, small, a deep fast warble, long glides, a high clangy ring, a swirling flanger
    { "ALIEN",    { 0.80f, 0.70f, 1.00f, 1.00f, 0.50f, 0.708f, 0.1f, 0.5f, 0.0f }, 9.0f, FILTER_OFF, 1000, 0, 0.0f,  DRIVE_SOFT, 420, 0.35f, 2.0f, 0.7f, 0.5f, 0.4f , 0.7f , 0, 0.25f },
    // a narrow band around 1.5 kHz (no lows, no highs) and a little hard grit: a cheap earpiece
    { "PHONE",    { 0.40f, 0.60f, 0.375f,0.35f, 0.50f, 0.5f, 0.0f, 0.2f, 0.0f },   5.5f, FILTER_BAND, 1500, 5, 0.30f, DRIVE_HARD, 0, 0.0f,   0, 0, 0, 0.0f , 1.0f , 0, 0.25f },
    // a tighter, ringier band and a lot of hard clipping: a bullhorn (a megaphone)
    { "BULLHORN", { 0.40f, 0.90f, 0.30f, 0.35f, 0.60f, 0.5f, 0.0f, 0.3f, 0.2f },   5.5f, FILTER_BAND, 1800, 9, 0.75f, DRIVE_HARD, 0, 0.0f,   0, 0, 0, 0.0f , 0.8f , 0, 0.25f },
    // a gravelly old jazz singer: low, big and pressed, LOTS of gravel, a slow wide vibrato that waits
    // for the end of the note, scoops up into notes from two below, a touch of warm drive
    { "JAZZ",     { 0.12f, 0.85f, 0.85f, 0.55f, 0.60f, 0.375f, 0.08f, 0.15f, 0.85f }, 4.6f, FILTER_LOW, 6000, 1, 0.18f, DRIVE_SOFT, 0, 0.0f, 0, 0, 0, 0.0f, 1.0f, 2, 0.45f },
};
#define NCHARS ((int)(sizeof CHARS / sizeof CHARS[0]))
static int who = 0;                      // the character picked

static void set_character(int c) {
    const Character *C = &CHARS[c];
    who = c;
    for (int k = 0; k < K_COUNT; k++) kv[k] = C->k[k];
    vib_hz = C->vib_hz;
    vib_delay = C->vib_delay;
    scoop_st = C->scoop;
    apply_voice();
    instrument_filter(SLOT, C->filt, C->cut, C->res);
    instrument_drive_mode(SLOT, C->dmode);
    instrument_drive(SLOT, C->drive);
    if (sing.h) note_drive(sing.h, C->drive);             // the note sounding now too
    instrument_ringmod(SLOT, C->ring_hz > 0 ? C->ring_hz : 100.0f, C->ring_mix);
    instrument_flanger(SLOT, C->fl_rate > 0 ? C->fl_rate : 0.3f, C->fl_depth, C->fl_fb, C->fl_mix);
    instrument_level(SLOT, C->level);
}
static void voice_defaults(void) { set_character(0); }

static int sing_note(int midi) { return singer_on(&sing, midi + transpose, 6); }
static char phon[512];


// ── SCAT ── the generator writes phrases of one-syllable scat words (read by the Spanish rules: pure
// vowels, one vowel each, so one word = one note) as a PLAN: per note a duration in ticks, a pitch on
// the blues scale, a breath after it. Five kinds of phrase, picked at random:
//   SWING    a swung run, long-short, walking the scale, landing on bop / dop / dat
//   STUTTER  one syllable fired fast on one pitch ("ba ba ba ba bop"), sometimes climbing
//   ECHO     the phrase before, answered a step higher (call and response)
//   HOLD     two quick pickups, then a long held "yeh" where the vibrato blooms
//   ROLL     "bi di bi di" in even fast notes, climbing
// Every phrase LANDS: it is started so its last note falls on a strong beat of the 12-bar (a downbeat,
// or beat 3), and that note is moved to the nearest tone of the chord there.
// One tick = 3 frames (50 ms); a beat = 6 ticks (100 bpm, swung). Fresh phrases every time round.
// THE BAND (B toggles): a walking upright bass on a 12-bar blues in G (INSTR_BOWED pizzicato, the
// upright recipe) and finger snaps on 2 and 4, on the same tick clock.
#define SCAT_TICKS_PER_BEAT 6
static const char *const SCAT_SYL[] = { "ba", "be", "bi", "bo", "bu", "da", "di", "du", "do", "ski",
                                        "ska", "wa", "ya", "zu", "dap", "bap", "dat" };
static const char *const SCAT_END[] = { "bop", "dop", "dat", "wap", "bam", "zot" };
static const char *const SCAT_HOLD[] = { "yeh", "wa", "ba", "bo" };
static const int BLUES[] = { 55, 58, 60, 62, 63, 65, 66, 67, 70, 72, 74 };   // G blues + the 9th, G3..D5 (higher runs hot: F1 tracking)
#define NBLUES ((int)(sizeof BLUES / sizeof BLUES[0]))
typedef struct { const char *w; unsigned char dur, rest, first; signed char pit; int midi; } ScatNote;   // midi: a landing note snapped to the chord (0 = the scale pitch)
#define SCAT_MAX 140
static ScatNote sn[SCAT_MAX];
static int sn_n = 0, sn_i = 0, sc_wait = 0, sc_rest = 0, sc_last = 6;
static int band = 1, sc_tick = 0, sc_sched = 0;
static unsigned scat_seed = 20261115u;
static unsigned scat_rand(void) { scat_seed = scat_seed * 1664525u + 1013904223u; return scat_seed >> 8; }
#define SR(n) ((int)(scat_rand() % (unsigned)(n)))
static int sc_clamp(int p) { return p < 0 ? 0 : p >= NBLUES ? NBLUES - 1 : p; }
static int scat_add(const char *w, int dur, int pit) {
    if (sn_n >= SCAT_MAX) return 0;
    ScatNote *N = &sn[sn_n++];
    N->w = w; N->dur = (unsigned char)dur; N->rest = 0; N->first = 0; N->midi = 0; N->pit = (signed char)sc_clamp(pit);
    return 1;
}
#define SCAT_PICK(a) (a)[SR((int)(sizeof (a) / sizeof (a)[0]))]
static void scat_gen(void) {
    sn_n = 0;
    int prev0 = -1, prev1 = -1;                          // the phrase before (for ECHO)
    for (int ph = 0; ph < 8 && sn_n < SCAT_MAX - 16; ph++) {
        int start = sn_n, kind = SR(10);
        int p = 3 + SR(6);
        if (kind < 3) {                                   // SWING
            int n = 4 + SR(5);
            const char *got[12];
            for (int i = 0; i < n; i++) {
                const char *w = (i >= 2 && SR(3) == 0) ? got[i - 2] : SCAT_PICK(SCAT_SYL);
                got[i] = w;
                scat_add(w, (i % 2 == 0) ? 4 : 2, p);
                p = sc_clamp(p + SR(5) - 2);
            }
            scat_add(SCAT_PICK(SCAT_END), 4 + 2 * SR(2), p);
        } else if (kind < 5) {                            // STUTTER
            const char *w = SCAT_PICK(SCAT_SYL);
            int k = 3 + SR(4), climb = SR(2);
            for (int i = 0; i < k; i++) { scat_add(w, 2, p); if (climb) p = sc_clamp(p + 1); }
            scat_add(SCAT_PICK(SCAT_END), 4, sc_clamp(p + (SR(2) ? 2 : -2)));
        } else if (kind < 7 && prev0 >= 0 && prev1 - prev0 <= 10) {   // ECHO the phrase before
            int up = SR(3) ? 2 : -1;
            for (int i = prev0; i < prev1; i++) scat_add(sn[i].w, sn[i].dur, sn[i].pit + up);
        } else if (kind < 8) {                            // HOLD
            p = 6 + SR(5);
            scat_add(SCAT_PICK(SCAT_SYL), 2, p - 2);
            scat_add(SCAT_PICK(SCAT_SYL), 2, p - 1);
            scat_add(SCAT_PICK(SCAT_HOLD), 12 + 2 * SR(3), p);
        } else {                                          // ROLL
            static const char *const PAIRS[][2] = { { "bi", "di" }, { "ba", "da" }, { "bu", "du" }, { "ski", "ba" } };
            int pr = SR(4), n = 6 + 2 * SR(2);
            for (int i = 0; i < n; i++) { scat_add(PAIRS[pr][i & 1], 2, p); if (i & 1) p = sc_clamp(p + 1); }
            scat_add(SCAT_PICK(SCAT_END), 4, p);
        }
        if (sn_n > start) { sn[start].first = 1; sn[sn_n - 1].rest = 1; }   // phrase bounds (its last note is the LANDING)
        prev0 = start; prev1 = sn_n;
    }
    // the plan as text, one word per note: the Singer reads it (lyr_set, Spanish rules)
    int len = 0;
    scat_text[0] = 0;
    for (int i = 0; i < sn_n; i++) {
        int wl = (int)strlen(sn[i].w);
        if (len + wl + 3 >= (int)sizeof scat_text) { sn_n = i; break; }
        memcpy(scat_text + len, sn[i].w, (size_t)wl); len += wl;
        if (sn[i].rest) scat_text[len++] = ',';
        scat_text[len++] = ' ';
        scat_text[len] = 0;
    }
}

// THE BAND: one beat of walking bass + snaps. Roots of a 12-bar blues in G (G7 C7 D7), a walking line:
// root, a chord tone, another, then a half step into the next bar's root.
static const int BLUES12[12] = { 43, 48, 43, 43, 48, 48, 43, 43, 50, 48, 43, 50 };   // G C G G / C C G G / D C G D (quick-change bar 2)
#define SCAT_BAR (SCAT_TICKS_PER_BEAT * 4)
static void band_beat(int tk) {
    int bar = (tk / SCAT_BAR) % 12, b = (tk / SCAT_TICKS_PER_BEAT) % 4;
    int root = BLUES12[bar], next = BLUES12[(bar + 1) % 12];
    static const int TONES[4] = { 4, 7, 9, 10 };          // 3rd, 5th, 6th, b7 of a dominant 7
    static unsigned bseed = 7u;                           // the band's own dice: switching it off keeps the same phrases
    bseed = bseed * 1664525u + 1013904223u;
    unsigned r = bseed >> 8;
    int m = b == 0 ? root : b == 3 ? next + ((r & 1) ? 1 : -1) : root + TONES[(r >> 1) % 4];
    while (m > 55) m -= 12;
    while (m < 36) m += 12;
    hit(m, BASS_SLOT, b == 0 ? 6 : 5, 280);
    if (b == 1 || b == 3) hit(84, SNAP_SLOT, 4, 40);       // snaps on 2 and 4
}
// the chord tone (1 3 5 b7 of the bar's dominant 7) nearest a pitch, at tick tk: where a phrase LANDS
static int chord_tone_near(int midi, int tk) {
    int root = BLUES12[(tk / SCAT_BAR) % 12] % 12, best = midi, bd = 99;
    static const int CT[4] = { 0, 4, 7, 10 };
    for (int m = BLUES[0]; m <= BLUES[NBLUES - 1]; m++) {
        int pc = ((m - root) % 12 + 12) % 12;
        for (int k = 0; k < 4; k++)
            if (pc == CT[k] && (m > midi ? m - midi : midi - m) < bd) { bd = m > midi ? m - midi : midi - m; best = m; }
    }
    return best;
}

static void scat_step(void) {
    int tk = sc_tick++;                                   // the shared clock: the band and the phrases both read it
    if (band && tk % SCAT_TICKS_PER_BEAT == 0) band_beat(tk);
    if (sc_wait > 0 && --sc_wait > 0) return;
    if (sc_rest) { singer_off(&sing); sc_rest = 0; sc_wait = 2; return; }   // a breath after the landing
    if (sn_i >= sn_n) {                                   // all sung: write new phrases
        scat_gen();
        lyr_set(&lyr, LYR_ES, scat_text);
        lyr_phonemes(&lyr, phon, sizeof phon);
        singer_restart(&sing);
        sn_i = 0;
    }
    // a new phrase: start it so its LAST note lands on a strong beat (the next bar's downbeat if it is
    // near, else beat 3), and move that landing note onto the chord sounding there
    if (sn[sn_i].first && !sc_sched) {
        int L = 0, e = sn_i;
        while (e < sn_n - 1 && !sn[e].rest) L += sn[e++].dur;       // e = the landing note, L = ticks before it
        int earliest = tk + L, t1 = (earliest + SCAT_BAR - 1) / SCAT_BAR * SCAT_BAR;
        int t3 = (earliest + SCAT_BAR / 2 - 1) / (SCAT_BAR / 2) * (SCAT_BAR / 2);
        int land = (t1 - earliest <= 8) ? t1 : t3;
        sn[e].midi = chord_tone_near(BLUES[sn[e].pit], land);
        sc_sched = 1;
        if (land - L > tk) { sc_wait = land - L - tk; return; }
    }
    sc_sched = 0;
    const ScatNote *N = &sn[sn_i++];
    sc_last = N->pit;
    sing_note(N->midi ? N->midi : BLUES[N->pit]);
#ifdef DE_TRACE
    if (N->rest) {                                        // a landing: where in the bar, and is it a chord tone
        int m = N->midi ? N->midi : BLUES[N->pit], root = BLUES12[(tk / SCAT_BAR) % 12] % 12, pc = ((m - root) % 12 + 12) % 12;
        static int nland = 0; nland++;
        watch("land", "%d %d %d %d", nland, tk % SCAT_BAR, pc, tk / SCAT_BAR);
    }
#endif
    sc_wait = N->dur;
    sc_rest = N->rest;
}
static void scat_reset(void) { sn_i = sc_wait = sc_rest = sc_tick = sc_sched = 0; }

static void pick_lyric(int k) {
    which = k;
    if (SONGS[k].scat) { scat_gen(); scat_reset(); }
    lyr_set(&lyr, SONGS[k].lang, SONGS[k].text);
    lyr_phonemes(&lyr, phon, sizeof phon);
    singer_stop(&sing);                                   // a sounding note must let go before init forgets it
    keyheld = -1;
    singer_init(&sing, &lyr, SLOT);
    sing.parallel = !casc;
    apply_voice();
    fr = note_left = rest_left = syl_i = words = mel_i = 0;
}

// a written melody: legato (a note runs into the next; the singer releases the old one as the new one
// starts), released at a rest
static void mel_step(const Song *g) {
    if (note_left > 0 && --note_left > 0) return;
    if (mel_i >= g->nmel) { mel_i = 0; singer_off(&sing); singer_restart(&sing); }
    MNote m = g->mel[mel_i++];
    note_left = m.len;
    if (!m.midi) { singer_off(&sing); return; }
    int h = sing_note(m.midi);
    (void)h;
}

void init(void) {
    instrument(SLOT, INSTR_VOICE, 5, 60, 7, 190);
    reverb(0.45f, 0.5f);
    instrument_reverb(SLOT, 0.25f);
    instrument(BASS_SLOT, INSTR_BOWED, 4, 0, 7, 260);       // the walking bass: a pizzicato upright
    instrument_mode(BASS_SLOT, MODE_BOW_PIZZ, 1.0f);
    instrument_mode(BASS_SLOT, MODE_BOW_BODY, 0.85f);
    instrument_mode(BASS_SLOT, MODE_BOW_SIZE, BOW_SIZE_BASS);
    instrument_filter(BASS_SLOT, FILTER_LOW, 1300, 3);
    instrument_harmonics(BASS_SLOT, 0.30f); instrument_timbre(BASS_SLOT, 0.30f); instrument_morph(BASS_SLOT, 0.45f);
    instrument_reverb(BASS_SLOT, 0.12f);
    instrument(SNAP_SLOT, INSTR_NOISE, 0, 40, 0, 20);       // a finger snap: a short bright click
    instrument_filter(SNAP_SLOT, FILTER_BAND, 2600, 5);
    instrument_reverb(SNAP_SLOT, 0.3f);
    Saved sv;
    if (load_bytes(&sv, sizeof sv) == (int)sizeof sv && sv.ver == 1 && sv.text[0]) {
        sv.text[USER_MAX - 1] = 0;
        memcpy(user_text, sv.text, USER_MAX);
        SONGS[USER].lang = (sv.lang >= 0 && sv.lang < LYR_NLANG) ? sv.lang : LYR_NL;
    }
    for (int k = 0; k < K_COUNT; k++) kv[k] = K_DEF[k];
    pick_lyric(0);
}

static void keys_step(void);

static void tb_add(unsigned char c) {
    int n = (int)strlen(tbuf);
    if (n < USER_MAX - 1) { tbuf[n] = (char)c; tbuf[n + 1] = 0; }
}
static void tb_back(void) {                               // one character, a whole UTF-8 sequence
    int n = (int)strlen(tbuf);
    while (n > 0 && ((unsigned char)tbuf[n - 1] & 0xC0) == 0x80) n--;
    if (n > 0) n--;
    tbuf[n] = 0;
}

// one frame of the text box
static void type_step(void) {
    for (const unsigned char *p = (const unsigned char *)text_input(); *p; p++) {
        if (*p >= 0xC0) { if ((int)strlen(tbuf) < USER_MAX - 2) { tb_add(0xC3); tb_add((unsigned char)(*p - 0x40)); } }  // Latin-1 é → UTF-8
        else if (*p >= 0x20 && *p < 0x7F) tb_add(*p);
    }
    for (const unsigned char *p = (const unsigned char *)paste(); *p; p++)
        tb_add(*p == '\n' || *p == '\r' || *p == '\t' ? ' ' : *p);
    if (keyp(KEY_BACKSPACE)) { tb_back(); bs_held = 0; }
    else if (key(KEY_BACKSPACE) && ++bs_held > 24 && bs_held % 3 == 0) tb_back();   // held: repeat
    if (keyp(KEY_TAB)) tlang = (tlang + 1) % LYR_NLANG;   // Nederlands → Mipomi → Nihongo → Espanol → Kiswahili → Hawaii
    if (keyp(KEY_ENTER)) {
        typing = 0;
        if (tbuf[0]) {                                    // sing it (an empty line just closes the box)
            memcpy(user_text, tbuf, USER_MAX);
            SONGS[USER].lang = tlang;
            Saved sv; memset(&sv, 0, sizeof sv);
            sv.ver = 1; sv.lang = tlang; memcpy(sv.text, tbuf, USER_MAX);
            save_bytes(&sv, sizeof sv);
            singer_off(&sing);
            pick_lyric(USER);
            autop = 1;
        }
    }
}

// the auto melody, one note per syllable, shaped by the words
static void auto_step(void) {
    if (note_left > 0) {
        if (--note_left == 1) singer_off(&sing);          // a step of air for the codas before the next
        return;
    }
    if (rest_left > 0) { rest_left--; return; }
    int i = syl_i % lyr.n;
    const LyrSyl *S = &lyr.syl[i];
    const LyrSyl *N = &lyr.syl[(i + 1) % lyr.n];
    int word_end = N->word0 || i + 1 == lyr.n;
    int len = word_end ? 4 : 2;
    sing_note(NOTES[CONTOUR[syl_i % 16]]);
    syl_i++;
    note_left = len;
    if (word_end) { words++; if (words % 4 == 0) rest_left = 4; }
    if (i + 1 == lyr.n) rest_left = 8;
    (void)S;
}

void update(void) {
    (void)key('P');                                        // ours, so typing a "p" never pauses the console
    if (typing) type_step();
    else if (keyp(KEY_ENTER)) {                            // open the text box on your last line
        typing = 1;
        if (keyheld >= 0) { singer_off(&sing); keyheld = -1; }   // its key-up would be missed while typing
        memcpy(tbuf, user_text, USER_MAX);
        tlang = SONGS[USER].lang;
    } else keys_step();
    const Song *g = &SONGS[which];
    if (autop && ++fr >= g->step_fr) { fr = 0; if (g->scat) scat_step(); else if (g->mel) mel_step(g); else auto_step(); }
    singer_update(&sing);
}

static void keys_step(void) {
    if (keyp(' ')) { autop = !autop; singer_off(&sing); note_left = rest_left = 0; }
    for (int k = 0; k < NSONGS && k < 10; k++) if (keyp(k < 9 ? '1' + k : '0')) pick_lyric(k);   // 1..9, then 0
    if (keyp('V')) { casc = !casc; sing.parallel = !casc; }
    if (keyp('B')) band = !band;                           // scat's band on/off
    if (keyp('L')) { sung = !sung; apply_voice(); }
    if (keyp(KEY_UP) || keyp(KEY_DOWN)) {                 // set-and-hold: only when it changes
        float b = kv[K_BUZZ] + (keyp(KEY_UP) ? 0.25f : -0.25f);
        kv[K_BUZZ] = b < 0.0f ? 0.0f : b > 1.0f ? 1.0f : b;
        apply_voice();
    }
    if (keyp('R')) { singer_off(&sing); singer_restart(&sing); syl_i = words = mel_i = note_left = 0; scat_reset(); }
    static const char KEYS[8] = { 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K' };
    for (int k = 0; k < 8; k++) {
        if (keyp(KEYS[k])) { autop = 0; sing_note(NOTES[k]); keyheld = k; }
        if (keyr(KEYS[k]) && keyheld == k) { singer_off(&sing); keyheld = -1; }
    }
}

// the text box: your line wrapped over a few rows, a blinking cursor at its end
static void draw_box(void) {
    int bx = 12, by = 40, bw = SCREEN_W - 24, bh = 96;
    rectfill(bx, by, bw, bh, CLR_BLACK);
    rect(bx, by, bw, bh, CLR_YELLOW);
    print("type words to sing", bx + 8, by + 6, CLR_YELLOW);
    char ln[32]; snprintf(ln, sizeof ln, "TAB: %s", lyr_lang_name[tlang]);
    static const int LCOL[LYR_NLANG] = { CLR_ORANGE, CLR_PINK, CLR_RED, CLR_YELLOW, CLR_GREEN, CLR_BLUE };
    print(ln, bx + bw - 8 - text_width(ln), by + 6, LCOL[tlang]);
    // wrap word by word into rows that fit (a space ends a word; it is drawn as the gap)
    int x = bx + 8, y = by + 22, n = (int)strlen(tbuf), sp = text_width(" ");
    char wd[USER_MAX];
    for (int i = 0; i <= n; ) {
        int j = i;
        while (j < n && tbuf[j] != ' ') j++;
        memcpy(wd, tbuf + i, (size_t)(j - i)); wd[j - i] = 0;
        int w = text_width(wd);
        if (x > bx + 8 && x + w > bx + bw - 8) { x = bx + 8; y += 10; }
        if (y < by + bh - 22) print(wd, x, y, CLR_WHITE);
        x += w;
        if (j < n) x += sp;                               // the space after it (so the cursor sits after a typed space)
        i = j + 1;
    }
    if (((int)(now() * 2.0f)) % 2 == 0 && y < by + bh - 22) rectfill(x + 1, y, 6, 8, CLR_YELLOW);
    font(FONT_SMALL);
    print("ENTER sing it  BACKSPACE erase  empty + ENTER closes", bx + 8, by + bh - 10, CLR_LIGHT_GREY);
    font(FONT_NORMAL);
}

// the knob row, the CHARACTER row and DEFAULT; a turned knob applies at once (set-and-hold: on a change)
static void draw_knobs(void) {
    const int ky = 102;
    int changed = 0;
    font(FONT_SMALL);
    for (int k = 0; k < K_COUNT; k++) {
        char lb[16];
        if (k == K_PITCH) snprintf(lb, sizeof lb, transpose ? "%+d" : "PITCH", transpose);
        else snprintf(lb, sizeof lb, "%s", K_NAME[k]);
        changed |= ui_knob(&kv[k], 18 + k * 35, ky, lb);
    }
    if (changed) apply_voice();
    for (int c = 0; c < NCHARS; c++) {                    // the characters, one tap each
        int bx = 8 + (c % 4) * 77, by = 122 + (c / 4) * 14;   // two rows of four
        if (ui_button(bx, by, 74, 12, CHARS[c].name)) set_character(c);
        if (c == who) rect(bx - 1, by - 1, 76, 14, CLR_YELLOW);
    }
    font(FONT_NORMAL);
    if (ui_button(258, 3, 56, 12, "DEFAULT")) voice_defaults();
}

void draw(void) {
    ui_begin();
    cls(CLR_DARK_BLUE);
    print("ZINGEN", 8, 6, CLR_YELLOW);
    print(autop ? "SPACE: auto" : "SPACE: you sing (A..K)", 70, 6, CLR_LIGHT_GREY);
    // the lyric as syllables, the sounding one lit
    int x = 8, y = 26;
    for (int i = 0; i < lyr.n; i++) {
        const LyrSyl *S = &lyr.syl[i];
        int w = text_width(S->txt);
        if (S->word0 && i) x += 6;
        if (x + w > SCREEN_W - 8) { x = 8; y += 12; }
        if (y > 76) break;                                // the knobs live below
        int lit = (i == sing.cur);
        if (lit) rectfill(x - 1, y - 2, w + 2, 11, CLR_DARK_PURPLE);
        print(S->txt, x, y, lit ? CLR_WHITE : CLR_LIGHT_GREY);
        x += w + 2;
    }
    if (!typing) draw_knobs();
    font(FONT_SMALL);
    // the phonemes it read (wrapped)
    int px = 8, py = 164;
    char word[64]; int wn = 0;
    for (const char *p = phon; ; p++) {
        if (*p == ' ' || *p == 0) {
            word[wn] = 0;
            int ww = text_width(word);
            if (px + ww > SCREEN_W - 8) { px = 8; py += 8; }
            if (py < SCREEN_H - 12) print(word, px, py, CLR_INDIGO);
            px += ww + 5; wn = 0;
            if (!*p) break;
        } else if (wn < 63) word[wn++] = *p;
    }
    print(SONGS[which].name, 8, 153, CLR_YELLOW);
    print(sung ? "L: sung" : "L: plain", SCREEN_W - 110, 153, sung ? CLR_PINK : CLR_DARK_GREY);
    if (SONGS[which].scat) print(band ? "B: band" : "B: solo", SCREEN_W - 160, 153, band ? CLR_GREEN : CLR_DARK_GREY);
    { const char *m = casc ? "V: cascade" : "V: parallel"; print(m, SCREEN_W - 8 - text_width(m), 153, casc ? CLR_ORANGE : CLR_DARK_GREY); }
    print("ENTER type  1-9,0 song  R restart  @ = schwa  x = g/ch", 8, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
    if (typing) draw_box();
    ui_end();
}

#ifdef DE_SPEC
void spec(void) { lyr_selfcheck(); }
#endif
