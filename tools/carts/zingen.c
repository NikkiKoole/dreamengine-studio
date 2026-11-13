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
    "summary": "The formant voice sings DUTCH. Type-free: pick a lyric and it sings it, one syllable per note, reading Dutch spelling by rules (long and short vowels, schwa, ch/g, ui/ij/eu, final devoicing).",
    "detail": "lyric.h reads the text into syllables; each note starts the next one: its consonants at note-on, the vowel held while the note is, then the diphthong's second half and the final consonants chained at release ('nacht' = x then t), before the note lets go. The melody is made from the words: a word's last syllable lasts longer, every fourth word breathes. Or play it yourself: every key press sings the next syllable. The bottom line shows the phonemes it read.",
    "controls": "SPACE = auto-sing on/off · A S D F G H J K = sing the next syllable on that note · 1 2 3 = pick a lyric · R = back to the start"
  }
}
de:meta */
#include "studio.h"
#include "lyric.h"

// ZINGEN — INSTR_VOICE sings Dutch through lyric.h. The lyrics are our own lines.
#define SLOT 5
#define STEP_FR 9                        // one step = 9 frames (≈ 16ths at 100 BPM)

static const char *const LYRICS[3] = {
    "ik zing voor jou, de zon gaat op, de nacht is lang en ik blijf hier",
    "kunst en liefde, vrolijk en eeuwig, in het licht van de straat",
    "de zee is groen, de lucht is grijs, ik loop - naar huis door de regen",
};
static const int NOTES[8] = { 60, 62, 64, 67, 69, 72, 74, 76 };   // C pentatonic-ish, two octaves of keys
static const int CONTOUR[16] = { 2, 3, 4, 3, 2, 1, 2, 4, 5, 4, 3, 2, 1, 0, 1, 2 };

static Lyric lyr;
static Singer sing;
static int which = 0, autop = 1;
static int fr = 0, note_left = 0, syl_i = 0, rest_left = 0, words = 0;
static int keyheld = -1;
static char phon[512];

static void pick_lyric(int k) {
    which = k;
    lyr_set(&lyr, LYR_NL, LYRICS[k]);
    lyr_phonemes(&lyr, phon, sizeof phon);
    singer_init(&sing, &lyr, SLOT);
    fr = note_left = rest_left = syl_i = words = 0;
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
    singer_on(&sing, NOTES[CONTOUR[syl_i % 16]], 6);
    syl_i++;
    note_left = len;
    if (word_end) { words++; if (words % 4 == 0) rest_left = 4; }
    if (i + 1 == lyr.n) rest_left = 8;
    (void)S;
}

void update(void) {
    if (keyp(' ')) { autop = !autop; singer_off(&sing); note_left = rest_left = 0; }
    for (int k = 0; k < 3; k++) if (keyp('1' + k)) pick_lyric(k);
    if (keyp('R')) { singer_off(&sing); singer_restart(&sing); syl_i = words = 0; }
    static const char KEYS[8] = { 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K' };
    for (int k = 0; k < 8; k++) {
        if (keyp(KEYS[k])) { autop = 0; singer_on(&sing, NOTES[k], 6); keyheld = k; }
        if (keyr(KEYS[k]) && keyheld == k) { singer_off(&sing); keyheld = -1; }
    }
    if (autop && ++fr >= STEP_FR) { fr = 0; auto_step(); }
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
    print("1 2 3 lyric  R restart  @ = schwa  x = g/ch", 8, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

#ifdef DE_SPEC
void spec(void) { lyr_selfcheck(); }
#endif
