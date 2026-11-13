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
    "summary": "The formant voice sings DUTCH. Type-free: pick a lyric and it sings it, one syllable per note, reading Dutch spelling by rules (long and short vowels, schwa, ch/g, ui/ij/eu, final devoicing). Song 4 is a lullaby in Mipomi, the playtime Mipos' language.",
    "detail": "lyric.h reads the text into syllables; each note starts the next one: its consonants at note-on, the vowel held while the note is, then the diphthong's second half and the final consonants chained at release ('nacht' = x then t), before the note lets go. The melody is made from the words: a word's last syllable lasts longer, every fourth word breathes. Or play it yourself: every key press sings the next syllable. The bottom line shows the phonemes it read.",
    "controls": "SPACE = auto-sing on/off · A S D F G H J K = sing the next syllable on that note · 1 2 3 = Dutch lyrics, 4 = a Mipomi lullaby · V = formant model (Klatt cascade / parallel) · UP/DOWN = buzz (relaxed and round .. pressed and bright) · L = singer layer on/off (vibrato that blooms in, a living pitch, sung glides) · R = back to the start"
  }
}
de:meta */
#include "studio.h"
#include "lyric.h"
#include <stdio.h>

// ZINGEN — INSTR_VOICE sings through lyric.h: three Dutch lyrics (our own lines) on a melody made from
// the words, and one MIPOMI song (the playtime Mipos' language, real words from mipomi-lang's
// words.tsv) on its own written lullaby melody.
#define SLOT 5

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
typedef struct { const char *name; int lang; const char *text; const MNote *mel; int nmel; int step_fr; float vib; } Song;
static const Song SONGS[4] = {
    { "zing voor jou", LYR_NL, "ik zing voor jou, de zon gaat op, de nacht is lang en ik blijf hier", 0, 0, 9, 0.25f },
    { "kunst",         LYR_NL, "kunst en liefde, vrolijk en eeuwig, in het licht van de straat", 0, 0, 9, 0.25f },
    { "de zee",        LYR_NL, "de zee is groen, de lucht is grijs, ik loop - naar huis door de regen", 0, 0, 9, 0.25f },
    { "fala, foni (mipomi)", LYR_MIPOMI,
      "mipo fala, fala a foko, mibi i mola, mibi o jojo u jo, fono, foni a foko, "
      "mi i nini, mi i momo u jo, mipo foni, pomo, ni -",
      FALA_FONI, (int)(sizeof FALA_FONI / sizeof FALA_FONI[0]), 11, 0.35f },
};
#define NSONGS 4
static int mel_i = 0;
static const int NOTES[8] = { 60, 62, 64, 67, 69, 72, 74, 76 };   // C pentatonic-ish, two octaves of keys
static const int CONTOUR[16] = { 2, 3, 4, 3, 2, 1, 2, 4, 5, 4, 3, 2, 1, 0, 1, 2 };

static Lyric lyr;
static Singer sing;
static int which = 0, autop = 1;
static int fr = 0, note_left = 0, syl_i = 0, rest_left = 0, words = 0;
static int keyheld = -1;
static int casc = 1;                 // V: the voice's formant model, 1 = Klatt cascade (the Singer's default), 0 = parallel

static float buzz = 0.5f;            // UP/DOWN: the voice's EFFORT macro = the cascade's BUZZ (relaxed round .. pressed bright)
static int sung = 1;                 // L: the singer layer (delayed vibrato, jitter/shimmer, legato glides) on/off, to A/B

static int sing_note(int midi) { return singer_on(&sing, midi, 6); }
static char phon[512];

static void apply_sung(void) {
    sing.vib   = sung ? SONGS[which].vib : 0.0f;
    sing.human = sung ? 1.0f : 0.0f;
    sing.glide = sung ? 0.07f : 0.0f;
}

static void pick_lyric(int k) {
    which = k;
    lyr_set(&lyr, SONGS[k].lang, SONGS[k].text);
    lyr_phonemes(&lyr, phon, sizeof phon);
    singer_init(&sing, &lyr, SLOT);
    sing.parallel = !casc;
    singer_buzz(&sing, buzz);
    apply_sung();
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
    pick_lyric(0);
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
    if (keyp(' ')) { autop = !autop; singer_off(&sing); note_left = rest_left = 0; }
    for (int k = 0; k < NSONGS; k++) if (keyp('1' + k)) pick_lyric(k);
    if (keyp('V')) { casc = !casc; sing.parallel = !casc; }
    if (keyp('L')) { sung = !sung; apply_sung(); }
    if (keyp(KEY_UP) || keyp(KEY_DOWN)) {                 // set-and-hold: only when it changes
        buzz += keyp(KEY_UP) ? 0.25f : -0.25f;
        buzz = buzz < 0.0f ? 0.0f : buzz > 1.0f ? 1.0f : buzz;
        singer_buzz(&sing, buzz);
    }
    if (keyp('R')) { singer_off(&sing); singer_restart(&sing); syl_i = words = mel_i = note_left = 0; }
    static const char KEYS[8] = { 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K' };
    for (int k = 0; k < 8; k++) {
        if (keyp(KEYS[k])) { autop = 0; sing_note(NOTES[k]); keyheld = k; }
        if (keyr(KEYS[k]) && keyheld == k) { singer_off(&sing); keyheld = -1; }
    }
    const Song *g = &SONGS[which];
    if (autop && ++fr >= g->step_fr) { fr = 0; if (g->mel) mel_step(g); else auto_step(); }
    singer_update(&sing);
}

void draw(void) {
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
        int lit = (i == sing.cur);
        if (lit) rectfill(x - 1, y - 2, w + 2, 11, CLR_DARK_PURPLE);
        print(S->txt, x, y, lit ? CLR_WHITE : CLR_LIGHT_GREY);
        x += w + 2;
    }
    font(FONT_SMALL);
    // the phonemes it read (wrapped)
    int px = 8, py = 150;
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
    print(SONGS[which].name, 8, 136, CLR_YELLOW);
    { char b[24]; snprintf(b, sizeof b, "buzz %d/4", (int)(buzz * 4.0f + 0.5f)); print(b, SCREEN_W - 150, 136, CLR_LIGHT_GREY); }
    print(sung ? "L: sung" : "L: plain", SCREEN_W - 200, 136, sung ? CLR_PINK : CLR_DARK_GREY);
    { const char *m = casc ? "V: cascade" : "V: parallel"; print(m, SCREEN_W - 8 - text_width(m), 136, casc ? CLR_ORANGE : CLR_DARK_GREY); }
    print("1-4 song  R restart  @ = schwa  x = g/ch", 8, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

#ifdef DE_SPEC
void spec(void) { lyr_selfcheck(); }
#endif
