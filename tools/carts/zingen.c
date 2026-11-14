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
    "summary": "The formant voice sings DUTCH, and whatever you type: pick a lyric or type your own line and it sings it, one syllable per note, reading Dutch spelling by rules (long and short vowels, schwa, ch/g, ui/ij/eu, final devoicing). Song 4 is a lullaby in Mipomi, the playtime Mipos' language.",
    "detail": "lyric.h reads the text into syllables; each note starts the next one: its consonants at note-on, the vowel held while the note is, then the diphthong's second half and the final consonants chained at release ('nacht' = x then t), before the note lets go. The melody is made from the words: a word's last syllable lasts longer, every fourth word breathes. Or play it yourself: every key press sings the next syllable. The bottom line shows the phonemes it read.",
    "controls": "SPACE = auto-sing on/off · A S D F G H J K = sing the next syllable on that note · 1 2 3 = Dutch lyrics, 4 = a Mipomi lullaby, 5 = your words · ENTER = type your own line (Dutch or Mipomi, TAB switches), ENTER again to hear it sung (it is remembered) · V = formant model (Klatt cascade / parallel) · knobs: SIZE (giant .. child) · BUZZ (relaxed and round .. pressed and bright; also UP/DOWN) · VIBRATO · GLIDE · LIFE (a machine .. shaky) · PITCH (±12 semitones) · WHISPER (sung → whispered: no pitch, the words stay) · NASAL (open → hummed, through the nose) · characters (one tap): CLEAN, CHIPMUNK, MONSTER, ROBOT, ALIEN, PHONE, MEGAPHONE; each moves the knobs it needs and adds its own effects · DEFAULT = CLEAN · L = singer layer on/off (vibrato that blooms in, a living pitch, sung glides) · R = back to the start"
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
typedef struct { const char *name; int lang; const char *text; const MNote *mel; int nmel; int step_fr; } Song;
#define USER_MAX 200
static char user_text[USER_MAX] = "typ hier je eigen woorden en ik zing ze";
static Song SONGS[5] = {
    { "zing voor jou", LYR_NL, "ik zing voor jou, de zon gaat op, de nacht is lang en ik blijf hier", 0, 0, 9 },
    { "kunst",         LYR_NL, "kunst en liefde, vrolijk en eeuwig, in het licht van de straat", 0, 0, 9 },
    { "de zee",        LYR_NL, "de zee is groen, de lucht is grijs, ik loop - naar huis door de regen", 0, 0, 9 },
    { "fala, foni (mipomi)", LYR_MIPOMI,
      "mipo fala, fala a foko, mibi i mola, mibi o jojo u jo, fono, foni a foko, "
      "mi i nini, mi i momo u jo, mipo foni, pomo, ni -",
      FALA_FONI, (int)(sizeof FALA_FONI / sizeof FALA_FONI[0]), 11 },
    { "your words", LYR_NL, user_text, 0, 0, 9 },
};
#define NSONGS 5
#define USER   4

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
enum { K_SIZE, K_BUZZ, K_VIB, K_GLIDE, K_LIFE, K_PITCH, K_WHISPER, K_NASAL, K_COUNT };
static const char *K_NAME[K_COUNT] = { "SIZE", "BUZZ", "VIBRATO", "GLIDE", "LIFE", "PITCH", "WHISPER", "NASAL" };
static const float K_DEF[K_COUNT]  = { 0.33f, 0.5f, 0.375f, 0.35f, 0.5f, 0.5f, 0.0f, 0.0f };
static float kv[K_COUNT];
static int transpose = 0;            // PITCH: semitones, -12 .. +12
static float vib_hz = 5.5f;          // vibrato rate (a character sets it; no knob)

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
}

// CHARACTERS: one tap turns the singer into someone else. Each sets the voice knobs (they move, so you
// see what it did and can keep turning) plus its own effects on the slot. Applied only on a tap
// (set-and-hold: every effect call reconfigures DSP).
typedef struct {
    const char *name;
    float k[K_COUNT];                    // SIZE BUZZ VIBRATO GLIDE LIFE PITCH WHISPER NASAL
    float vib_hz;
    int filt, cut, res;                  // instrument_filter
    float drive; int dmode;              // instrument_drive / _mode
    float ring_hz, ring_mix;             // instrument_ringmod
    float fl_rate, fl_depth, fl_fb, fl_mix;   // instrument_flanger
    float level;                         // instrument_level: evens the characters out (measured RMS vs CLEAN)
} Character;
static const Character CHARS[] = {
    //            SIZE   BUZZ   VIB    GLIDE  LIFE   PITCH  WHISP NASAL   vib   filter               drive        ringmod       flanger
    { "CLEAN",    { 0.33f, 0.50f, 0.375f,0.35f, 0.50f, 0.5f, 0.0f, 0.0f },   5.5f, FILTER_OFF, 1000, 0, 0.0f,  DRIVE_SOFT, 0, 0.0f,    0, 0, 0, 0.0f , 1.0f },
    // pitched up an octave AND a small throat: the sped-up-tape cartoon
    { "CHIPMUNK", { 0.95f, 0.60f, 0.25f, 0.50f, 0.50f, 1.0f, 0.0f, 0.2f },   7.0f, FILTER_OFF, 1000, 0, 0.0f,  DRIVE_SOFT, 0, 0.0f,    0, 0, 0, 0.0f , 0.32f },
    // down an octave, a giant's throat, pressed and rough, a little growl, the top shaved off
    { "MONSTER",  { 0.00f, 0.95f, 0.15f, 0.40f, 0.90f, 0.0f, 0.15f, 0.0f },   4.0f, FILTER_LOW, 2500, 2, 0.40f, DRIVE_SOFT, 0, 0.0f,    0, 0, 0, 0.0f , 1.0f },
    // dead flat (no vibrato, glide or life), pressed, ring-modulated at 55 Hz, a metallic flanger
    { "ROBOT",    { 0.33f, 1.00f, 0.00f, 0.00f, 0.00f, 0.5f, 0.0f, 0.35f },   5.5f, FILTER_OFF, 1000, 0, 0.0f,  DRIVE_SOFT, 55, 0.75f,  0.1f, 0.2f, 0.6f, 0.4f , 1.0f },
    // up a fourth, small, a deep fast warble, long glides, a high clangy ring, a swirling flanger
    { "ALIEN",    { 0.80f, 0.70f, 1.00f, 1.00f, 0.50f, 0.708f, 0.1f, 0.5f }, 9.0f, FILTER_OFF, 1000, 0, 0.0f,  DRIVE_SOFT, 420, 0.35f, 2.0f, 0.7f, 0.5f, 0.4f , 0.7f },
    // a narrow band around 1.5 kHz (no lows, no highs) and a little hard grit: a cheap earpiece
    { "PHONE",    { 0.40f, 0.60f, 0.375f,0.35f, 0.50f, 0.5f, 0.0f, 0.2f },   5.5f, FILTER_BAND, 1500, 5, 0.30f, DRIVE_HARD, 0, 0.0f,   0, 0, 0, 0.0f , 1.0f },
    // a tighter, ringier band and a lot of hard clipping: a bullhorn
    { "MEGAPHONE",{ 0.40f, 0.90f, 0.30f, 0.35f, 0.60f, 0.5f, 0.0f, 0.3f },   5.5f, FILTER_BAND, 1800, 9, 0.75f, DRIVE_HARD, 0, 0.0f,   0, 0, 0, 0.0f , 0.8f },
};
#define NCHARS ((int)(sizeof CHARS / sizeof CHARS[0]))
static int who = 0;                      // the character picked

static void set_character(int c) {
    const Character *C = &CHARS[c];
    who = c;
    for (int k = 0; k < K_COUNT; k++) kv[k] = C->k[k];
    vib_hz = C->vib_hz;
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


static void pick_lyric(int k) {
    which = k;
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
    Saved sv;
    if (load_bytes(&sv, sizeof sv) == (int)sizeof sv && sv.ver == 1 && sv.text[0]) {
        sv.text[USER_MAX - 1] = 0;
        memcpy(user_text, sv.text, USER_MAX);
        SONGS[USER].lang = sv.lang == LYR_MIPOMI ? LYR_MIPOMI : LYR_NL;
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
    if (keyp(KEY_TAB)) tlang = tlang == LYR_NL ? LYR_MIPOMI : LYR_NL;
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
    if (autop && ++fr >= g->step_fr) { fr = 0; if (g->mel) mel_step(g); else auto_step(); }
    singer_update(&sing);
}

static void keys_step(void) {
    if (keyp(' ')) { autop = !autop; singer_off(&sing); note_left = rest_left = 0; }
    for (int k = 0; k < NSONGS; k++) if (keyp('1' + k)) pick_lyric(k);
    if (keyp('V')) { casc = !casc; sing.parallel = !casc; }
    if (keyp('L')) { sung = !sung; apply_voice(); }
    if (keyp(KEY_UP) || keyp(KEY_DOWN)) {                 // set-and-hold: only when it changes
        float b = kv[K_BUZZ] + (keyp(KEY_UP) ? 0.25f : -0.25f);
        kv[K_BUZZ] = b < 0.0f ? 0.0f : b > 1.0f ? 1.0f : b;
        apply_voice();
    }
    if (keyp('R')) { singer_off(&sing); singer_restart(&sing); syl_i = words = mel_i = note_left = 0; }
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
    const char *ln = tlang == LYR_NL ? "TAB: Nederlands" : "TAB: Mipomi";
    print(ln, bx + bw - 8 - text_width(ln), by + 6, tlang == LYR_NL ? CLR_ORANGE : CLR_PINK);
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
        changed |= ui_knob(&kv[k], 22 + k * 39, ky, lb);
    }
    if (changed) apply_voice();
    for (int c = 0; c < NCHARS; c++) {                    // the characters, one tap each
        int bx = 8 + c * 44, by = 125;
        if (ui_button(bx, by, 42, 12, CHARS[c].name)) set_character(c);
        if (c == who) rect(bx - 1, by - 1, 44, 14, CLR_YELLOW);
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
    int px = 8, py = 156;
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
    print(SONGS[which].name, 8, 144, CLR_YELLOW);
    print(sung ? "L: sung" : "L: plain", SCREEN_W - 110, 144, sung ? CLR_PINK : CLR_DARK_GREY);
    { const char *m = casc ? "V: cascade" : "V: parallel"; print(m, SCREEN_W - 8 - text_width(m), 144, casc ? CLR_ORANGE : CLR_DARK_GREY); }
    print("ENTER type your own  1-5 song  R restart  @ = schwa  x = g/ch", 8, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
    if (typing) draw_box();
    ui_end();
}

#ifdef DE_SPEC
void spec(void) { lyr_selfcheck(); }
#endif
