/* de:meta
{
  "slug": "pianobar",
  "title": "piano bar",
  "status": "wip",
  "created": "2026-11-15",
  "kind": ["instrument"],
  "teaches": ["chord-voicing", "scale-quantize"],
  "lineage": "zingen's singer (lyric.h on INSTR_VOICE) sat down at grand keys' Steinway (steinway.h on INSTR_PIANO's hybrid). Piano: bitKlavier Grand Sample Library, Daniel Trueman, Princeton University (CC BY 4.0).",
  "description": {
    "summary": "Be the singer at the grand piano. Your left hand plays chords on a real Steinway, your right hand plays the tune, and every note you play sings the next syllable of the song.",
    "detail": "One key plays a whole chord in the song's key, voiced the way a pianist would: each chord moves to the nearest shape from the last one, with the root low in the bass. The melody keys only play notes of the scale, so nothing is ever a wrong note. Hold a melody key to hold the syllable; play the next note while still holding it and the voice slides over, legato. The words light up as you sing them. A MIDI keyboard works too: below middle C is the piano, from middle C up is the voice.",
    "controls": "LEFT HAND (piano): Z X C V B N M = chords I ii iii IV V7 vi bVII · SPACE = sustain pedal · RIGHT HAND (voice): A S D F G H J K L = the scale from the key note up, Q W E R T Y U I O P = an octave higher · 1-8 = pick the song · 0 or BACKSPACE = back to the first word · LEFT/RIGHT = change key · TAB = the singer (clean, warm, jazz, child) · UP/DOWN = piano loudness · touch: the chord pads, the pedal, and the keys (left of the gold line = piano, right = voice)"
  }
}
de:meta */
// PIANO BAR — the singer at the grand.
//
// THE POINT. zingen made INSTR_VOICE sing words; grand keys made a real Steinway playable. A performer
// wants both at once, one per hand, the way a singer-songwriter sits at a piano: the left hand comps,
// the right hand carries the tune, and the words come out of the right hand. So the LEFT hand gets
// whole chords on one key each (voice-led, so a I-vi-IV-V sounds like a pianist, not a chord chart),
// and the RIGHT hand gets the scale only (scale-locked: nothing to get wrong while you sing). Every
// melody note starts the next syllable (lyric.h's Singer); the words light up as they are sung.
#include "studio.h"
#define KEYBED_WHITE_KEYS ""          // the QWERTY rows are this cart's own (chords + melody), not a keybed
#define KEYBED_BLACK_KEYS ""
#define KEYBED_OCT_DOWN 0
#define KEYBED_OCT_UP   0
#include "keybed.h"
#include "steinway.h"
#include "lyric.h"
#include <stdio.h>
#include <string.h>

#define P_SLOT 5                      // the Steinway
#define V_SLOT 6                      // the voice
#define SPLIT  60                     // a MIDI / on-screen key below this is piano, from here up it is sung
#define V_VOL  5

// ── THE SONGS (our own lines; the same as zingen's, so the two carts sing the same book) ──
typedef struct { const char *name; int lang; const char *text; } Song;
static const Song SONGS[] = {
    { "zing voor jou", LYR_NL, "ik zing voor jou, de zon gaat op, de nacht is lang en ik blijf hier" },
    { "kunst",         LYR_NL, "kunst en liefde, vrolijk en eeuwig, in het licht van de straat" },
    { "de zee",        LYR_NL, "de zee is groen, de lucht is grijs, ik loop naar huis door de regen" },
    { "fala, foni",    LYR_MIPOMI, "mipo fala, fala a foko, mibi i mola, mibi o jojo u jo, fono, foni a foko, "
                                   "mi i nini, mi i momo u jo, mipo foni, pomo, ni" },
    { "sakura",        LYR_JA, "sakura no hana ga saite, kaze ni yureru, kimi to utau yo" },
    { "la luna",       LYR_ES, "canto contigo bajo la luna, el mar y el viento bailan conmigo" },
    { "jua",           LYR_SW, "jua linawaka, ndege wanaimba, rafiki yangu, njoo tucheze" },
    { "aloha",         LYR_HAW, "aloha e ka lā, aloha e ka mahina, ke mele nei mākou, me ke aloha" },
};
#define NSONGS ((int)(sizeof SONGS / sizeof SONGS[0]))

// ── THE SINGERS (TAB): each is a handful of Singer settings, applied once on the tap ──
typedef struct { const char *name; float size, buzz, vib, vib_hz, glide, human, whisper, gravel; int scoop; } Voice;
static const Voice VOICES[] = {
    { "clean", 0.33f, 0.50f, 0.30f, 5.5f, 0.07f, 1.0f, 0.00f, 0.00f, 0 },
    { "warm",  0.25f, 0.30f, 0.35f, 5.0f, 0.09f, 1.0f, 0.15f, 0.00f, 0 },
    { "jazz",  0.30f, 0.65f, 0.45f, 5.0f, 0.08f, 1.3f, 0.00f, 0.35f, 3 },
    { "child", 0.90f, 0.55f, 0.25f, 6.0f, 0.06f, 0.8f, 0.05f, 0.00f, 0 },
};
#define NVOICES ((int)(sizeof VOICES / sizeof VOICES[0]))

static Lyric lyr;
static Singer sing;
static int song = 0, voice = 0;
static int keyc = 0;                  // the key: 0 = C .. 11 = B
static int pvel = 6;                  // piano loudness (1..7; 6 sits the chords about 5 dB under the voice, measured)
static bool ok = false, pedal = false;
static const char *NOTE_NAME[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

// ── THE PIANO: refcounted per key (a chord pad and a MIDI key may hold the same note), the pedal keeps
// a let-go key ringing until it comes up (grand keys' rule) ──
static int  phnd[128], pcount[128];
static bool psus[128];
static void piano_down(int m, int v) {
    if (m < 0 || m > 127) return;
    if (pcount[m]++ > 0) return;
    if (phnd[m] >= 0) note_off(phnd[m]);         // struck again while ringing on the pedal: a fresh strike
    phnd[m] = note_on(m, P_SLOT, v);
    if (phnd[m] >= 0) note_filter(phnd[m], FILTER_OFF);
    psus[m] = false;
}
static void piano_up(int m) {
    if (m < 0 || m > 127 || pcount[m] <= 0) return;
    if (--pcount[m] > 0) return;
    if (pedal) { psus[m] = true; return; }
    if (phnd[m] >= 0) note_off(phnd[m]);
    phnd[m] = -1;
}
static void pedal_up(void) {
    for (int m = 0; m < 128; m++)
        if (psus[m]) { if (phnd[m] >= 0) note_off(phnd[m]); phnd[m] = -1; psus[m] = false; }
}

// ── CHORDS: the seven a pop/folk song lives on, in the key. Voiced in the middle of the piano, each one
// the inversion NEAREST the last (the least total movement: how a pianist's hand moves), plus the root
// alone down in the bass. ──
typedef struct { const char *roman; int root; int third, fifth, seventh; } Chord;   // semitones over the key note
static const Chord CHORDS[7] = {
    { "I",    0, 4, 7, -1 }, { "ii",  2, 3, 7, -1 }, { "iii", 4, 3, 7, -1 }, { "IV", 5, 4, 7, -1 },
    { "V7",   7, 4, 7, 10 }, { "vi",  9, 3, 7, -1 }, { "bVII", 10, 4, 7, -1 },
};
static const char CHORD_KEYS[7] = { 'Z', 'X', 'C', 'V', 'B', 'N', 'M' };
#define VLO 53                        // the voicing's lowest note sits in F3 .. E4
#define VHI 64
#define MAXV 5
static int cnotes[7][MAXV], cn[7];    // what each held pad is sounding (so its release lets go of exactly that)
static int last_voicing[4] = { 60, 64, 67, -1 }, last_n = 3;
static int chord_shown = -1;          // the last chord struck (for the display)

static int voice_chord(int c, int *out) {
    const Chord *C = &CHORDS[c];
    int pcs[4] = { C->root, C->root + C->third, C->root + C->fifth, C->seventh >= 0 ? C->root + C->seventh : -1 };
    int np = C->seventh >= 0 ? 4 : 3;
    int best[4], bestd = 1 << 30;
    for (int inv = 0; inv < np; inv++) {          // each inversion, its lowest note placed in VLO..VHI
        int v[4], lo = (keyc + pcs[inv]) % 12;
        while (lo < VLO) lo += 12;
        while (lo > VHI) lo -= 12;
        v[0] = lo;
        for (int k = 1; k < np; k++) {
            int pc = (keyc + pcs[(inv + k) % np]) % 12, m = v[k - 1] + 1;
            while (m % 12 != pc) m++;
            v[k] = m;
        }
        int d = 0;                                // movement from the last voicing (note by note, sorted)
        for (int k = 0; k < np; k++) { int p = last_voicing[k < last_n ? k : last_n - 1]; d += v[k] > p ? v[k] - p : p - v[k]; }
        if (d < bestd) { bestd = d; memcpy(best, v, sizeof best); }
    }
    memcpy(last_voicing, best, sizeof best); last_n = np;
    int n = 0;
    int bass = (keyc + C->root) % 12 + 36;        // the root, alone, in C2 .. B2
    out[n++] = bass;
    for (int k = 0; k < np; k++) out[n++] = best[k];
    return n;
}
static void chord_down(int c) {
    if (cn[c]) return;
    cn[c] = voice_chord(c, cnotes[c]);
    for (int k = 0; k < cn[c]; k++) piano_down(cnotes[c][k], k == 0 ? pvel + 1 > 7 ? 7 : pvel + 1 : pvel);
    chord_shown = c;
}
static void chord_up(int c) {
    for (int k = 0; k < cn[c]; k++) piano_up(cnotes[c][k]);
    cn[c] = 0;
}

// ── THE MELODY: the major scale of the key, from the key note up (home row), and an octave higher
// (the row above). Every note sings the next syllable; the newest held key owns the voice. ──
static const int MAJOR[7] = { 0, 2, 4, 5, 7, 9, 11 };
static const char *HOME_ROW = "ASDFGHJKL", *TOP_ROW = "QWERTYUIOP";
static int vkey = -1;                 // the key (an id) holding the voice now, -1 = none
static int vmidi = -1;
static int tonic_midi(void) { int t = 60 + keyc; return t > 66 ? t - 12 : t; }   // keep the voice in its range
static int degree_midi(int d) { return tonic_midi() + (d / 7) * 12 + MAJOR[d % 7]; }
static void sing_down(int id, int midi) {
    singer_on(&sing, midi, V_VOL);                // legato if the last one is still held: it slides
    vkey = id; vmidi = midi;
}
static void sing_up(int id) {
    if (id != vkey) return;                       // an older key let go under a newer one: the newer one sings on
    singer_off(&sing);
    vkey = -1; vmidi = -1;
}

static void apply_voice(void) {
    const Voice *V = &VOICES[voice];
    singer_size(&sing, V->size);
    singer_buzz(&sing, V->buzz);
    singer_whisper(&sing, V->whisper);
    singer_gravel(&sing, V->gravel);
    sing.vib = V->vib; sing.vib_hz = V->vib_hz; sing.glide = V->glide; sing.human = V->human; sing.scoop = V->scoop;
}
static void pick_song(int k) {
    song = k;
    singer_stop(&sing);
    vkey = vmidi = -1;
    lyr_set(&lyr, SONGS[k].lang, SONGS[k].text);
    singer_init(&sing, &lyr, V_SLOT);
    apply_voice();
}

// MIDI keyboard + the drawn keys (keybed.h): split at middle C
static void kb_down(int m, int v) { if (m < SPLIT) piano_down(m, v); else sing_down(1000 + m, m); }
static void kb_up(int m)          { if (m < SPLIT) piano_up(m); else sing_up(1000 + m); }

// ── LAYOUT ──
#define LYR_Y   34
#define PAD_Y   106
#define PAD_H   22
#define KEY_Y   134
static int pad_w(void) { return SCREEN_W / 8; }
static int pad_x(int i) { return i * pad_w(); }          // 0..6 the chords, 7 the pedal
static bool pad_touched(int i) {
    for (int t = 0; t < touch_count(); t++) {
        int x = touch_x(t), y = touch_y(t);
        if (x >= pad_x(i) && x < pad_x(i) + pad_w() && y >= PAD_Y && y < PAD_Y + PAD_H) return true;
    }
    return false;
}
static bool chord_held[7];

void init(void) {
    for (int m = 0; m < 128; m++) phnd[m] = -1;
    instrument(P_SLOT, INSTR_PIANO, 1, 0, 7, 150);   // grand keys' voicing
    instrument_harmonics(P_SLOT, 0.08f);
    instrument_timbre(P_SLOT, 0.5f);
    instrument_morph(P_SLOT, 1.0f);
    instrument_reverb(P_SLOT, 0.22f);
    ok = steinway_load(0) == STEINWAY_ZONES;
    instrument_mode(P_SLOT, MODE_PIANO_MODEL, 1.0f);
    if (ok) steinway_use(P_SLOT, 0);
    instrument(V_SLOT, INSTR_VOICE, 5, 60, 7, 190);  // zingen's singer
    instrument_reverb(V_SLOT, 0.25f);
    reverb(0.5f, 0.45f);
    keybed_config(P_SLOT, 3, 21);                    // C3 .. B5: piano left of middle C, voice from it up
    keybed_layout(0, KEY_Y, SCREEN_W, SCREEN_H - KEY_Y);
    keybed_velocity(5);
    keybed_manage_voices(false);
    keybed_on_note(kb_down);
    keybed_on_off(kb_up);
    pick_song(0);
}

void update(void) {
    // the pedal
    bool p = key(KEY_SPACE) || midi_cc(0, 64) >= 64 || pad_touched(7);
    if (pedal && !p) { pedal = false; pedal_up(); }
    pedal = p;
    // the chords: a key or a pad
    for (int c = 0; c < 7; c++) {
        bool h = key(CHORD_KEYS[c]) || pad_touched(c);
        if (h && !chord_held[c]) chord_down(c);
        if (!h && chord_held[c]) chord_up(c);
        chord_held[c] = h;
    }
    // the melody rows
    for (int i = 0; HOME_ROW[i]; i++) { if (keyp(HOME_ROW[i])) sing_down(i, degree_midi(i)); if (keyr(HOME_ROW[i])) sing_up(i); }
    for (int i = 0; TOP_ROW[i]; i++)  { if (keyp(TOP_ROW[i])) sing_down(100 + i, degree_midi(7 + i)); if (keyr(TOP_ROW[i])) sing_up(100 + i); }
    keybed_update();
    // songs, singers, key, loudness
    for (int k = 0; k < NSONGS; k++) if (keyp('1' + k) && k != song) pick_song(k);
    if (keyp('0') || keyp(KEY_BACKSPACE)) { singer_off(&sing); vkey = -1; singer_restart(&sing); }
    if (keyp(KEY_TAB)) { voice = (voice + 1) % NVOICES; apply_voice(); }
    if (keyp(KEY_RIGHT)) keyc = (keyc + 7) % 12;     // round the circle of fifths: the next key is a neighbour
    if (keyp(KEY_LEFT))  keyc = (keyc + 5) % 12;
    if (keyp(KEY_UP)   && pvel < 7) pvel++;
    if (keyp(KEY_DOWN) && pvel > 1) pvel--;
    singer_update(&sing);
#ifdef DE_TRACE
    int n = 0; for (int m = 0; m < 128; m++) if (phnd[m] >= 0) n++;
    watch("piano", "%d", n);
    watch("pedal", "%d", pedal ? 1 : 0);
    watch("syl", "%d", sing.pos);
    watch("vmidi", "%d", vmidi);
    watch("chord", "%d", chord_shown);
#endif
}

// the lyric, karaoke: sung syllables dim, the one sounding bright, the next one waiting
static void draw_lyric(void) {
    int x = 8, y = LYR_Y, cur = sing.h || sing.rh ? sing.cur : -1, next = sing.pos >= lyr.n ? 0 : sing.pos;
    for (int i = 0; i < lyr.n; i++) {
        const LyrSyl *S = &lyr.syl[i];
        // a word's width, to wrap before it
        if (S->word0) {
            int w = 0;
            for (int j = i; j < lyr.n && (j == i || !lyr.syl[j].word0); j++) w += 8 * (int)strlen(lyr.syl[j].txt);
            if (i > 0) x += 8;
            if (x + w > SCREEN_W - 8) { x = 8; y += 12; }
        }
        int col = i == cur ? CLR_YELLOW : i == next ? CLR_WHITE : i < next ? CLR_MEDIUM_GREY : CLR_LIGHT_GREY;
        if (i == cur) rectfill(x - 1, y - 2, 8 * (int)strlen(S->txt) + 2, 11, CLR_DARK_PURPLE);
        print(S->txt, x, y, col);
        if (i == next && i != cur) line(x, y + 9, x + 8 * (int)strlen(S->txt) - 2, y + 9, CLR_PINK);
        x += 8 * (int)strlen(S->txt);
    }
}

static int key_col(int m, int base, int held) {
    if (keybed_held(m) || (m < SPLIT && pcount[m] > 0) || m == vmidi) return held;
    if (psus[m]) return CLR_PEACH;
    return base;
}

void draw(void) {
    cls(CLR_BLACK);
    rectfill(0, 0, SCREEN_W, 26, CLR_DARK_BROWN);
    print("piano bar", 8, 4, CLR_WHITE);
    char b[64];
    snprintf(b, sizeof b, "%d %s", song + 1, SONGS[song].name);
    print(b, SCREEN_W - 8 - 8 * (int)strlen(b), 4, CLR_YELLOW);
    font(FONT_SMALL);
    snprintf(b, sizeof b, "key %s (</>)   singer %s (tab)   piano %d (up/dn)   %s",
             NOTE_NAME[keyc], VOICES[voice].name, pvel, lyr_lang_name[SONGS[song].lang]);
    print(b, 8, 16, CLR_LIGHT_PEACH);
    font(FONT_NORMAL);
    draw_lyric();
    // the chord pads + the pedal
    for (int i = 0; i < 8; i++) {
        int x = pad_x(i) + 1, w = pad_w() - 2;
        bool on = i < 7 ? chord_held[i] : pedal;
        rectfill(x, PAD_Y, w, PAD_H, on ? CLR_ORANGE : i == chord_shown ? CLR_DARK_BROWN : CLR_DARKER_GREY);
        if (i < 7) {
            const Chord *C = &CHORDS[i];
            snprintf(b, sizeof b, "%s%s", NOTE_NAME[(keyc + C->root) % 12], C->third == 3 ? "m" : C->seventh >= 0 ? "7" : "");
            print(b, x + w / 2 - 4 * (int)strlen(b), PAD_Y + 3, on ? CLR_BLACK : CLR_WHITE);
            font(FONT_TINY);
            snprintf(b, sizeof b, "%c %s", CHORD_KEYS[i], C->roman);
            print(b, x + 2, PAD_Y + 14, on ? CLR_BLACK : CLR_MEDIUM_GREY);
            font(FONT_NORMAL);
        } else {
            print("ped", x + w / 2 - 12, PAD_Y + 3, on ? CLR_BLACK : CLR_WHITE);
            font(FONT_TINY); print("space", x + 2, PAD_Y + 14, on ? CLR_BLACK : CLR_MEDIUM_GREY); font(FONT_NORMAL);
        }
    }
    // the keys: piano (left of the gold line), voice (right of it, tinted, in the key's scale)
    int split_x = SCREEN_W;
    for (int k = 0; k < keybed_white_count(); k++) {
        int x, y, w, h; keybed_white_rect(k, &x, &y, &w, &h);
        int m = keybed_white_midi(k);
        bool sung_key = m >= SPLIT;
        bool in_scale = false;
        for (int s = 0; s < 7; s++) if ((m - keyc - MAJOR[s]) % 12 == 0) in_scale = true;
        int base = sung_key ? (in_scale ? CLR_LIGHT_PEACH : CLR_LIGHT_GREY) : CLR_WHITE;
        rectfill(x, y, w - 1, h, key_col(m, base, sung_key ? CLR_YELLOW : CLR_ORANGE));
        if (m == SPLIT) split_x = x - 1;
    }
    for (int k = 0; k < keybed_white_count(); k++) {
        int x, y, w, h, m; if (!keybed_black_rect(k, &x, &y, &w, &h, &m)) continue;
        rectfill(x, y, w, h, key_col(m, CLR_BROWNISH_BLACK, m >= SPLIT ? CLR_YELLOW : CLR_ORANGE));
    }
    line(split_x, KEY_Y - 3, split_x, SCREEN_H, CLR_YELLOW);
    font(FONT_TINY);
    print("piano", 4, KEY_Y - 7, CLR_MEDIUM_GREY);
    print("voice", split_x + 4, KEY_Y - 7, CLR_MEDIUM_GREY);
    if (ok) print("piano: bitKlavier Grand, D. Trueman, Princeton (CC BY 4.0)", 8, 96, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
