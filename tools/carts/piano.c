// piano — INSTR_PIANO showcase + tuning rig: a one-octave keyboard, the three engine macros,
// and six hardware presets (the acceptance tests).
//
// The struck stiff string (StifKarp, §8.8.9): a Karplus-Strong string plus a DISPERSION allpass
// chain that stretches the upper partials sharp — the inharmonic, slightly metallic shimmer your
// ear reads as a real piano (and as a dulcimer or clavichord) rather than a plain string — run
// through a grand-piano soundboard. Struck; rings down on its own. The same three 0..1 macros:
//   harmonics = stiffness (0 pure tone .. 1 stretched metallic shimmer)
//   timbre    = hammer    (0 soft felt/mellow .. 1 hard/bright plectrum)
//   morph     = pedal     (0 dry/damped staccato .. 1 long open sustain)
//
// controls: A W S E D  F T G Y H U J K  play the keyboard (white = A S D F G H J K,
//             black = W E  T Y U) — or just click/tap the keys
//           1..6  load a preset (grand/bright/harpsi/dulcimer/clavi/celesta) — the test:
//                 if "harpsichord" doesn't sound like one, the MAPPING is wrong, not the preset
//           drag a slider (auditions as you drag), or LEFT/RIGHT pick a knob + UP/DOWN turn it
//           SPACE chord   ·   M autoplay on/off ('P' is the runtime pause overlay)
//
// MULTITOUCH: every finger is its own pointer — hold a chord with one hand, drag a slider
// with another. The desktop mouse is one synthetic finger, same path.

#include "studio.h"
#include <math.h>

#define I_PNO 5
#define NKEY  13                    // one octave, C..C

// keyboard layout: 8 white keys + 5 black keys, computer-key mapped like a tracker octave
static const int  SEMI[NKEY]   = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
static const char KEYK[NKEY]   = { 'A','W','S','E','D','F','T','G','Y','H','U','J','K' };
static const bool BLACK[NKEY]  = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0 };

// row 1 = the 3 engine macros; row 2 = TUNING controls (weight/attack via eng_tune(), width via
// per-note instrument_pan()) — scaffolding to dial the sound in by ear, baked to constants after.
static const char *KNOB_NAME[6] = { "voicing", "hammer", "pedal", "weight", "attack", "width" };

#define NPRESET 6
static const char *PRESET_NAME[NPRESET] = { "grand","bright","harpsi","dulcimer","clavi","celesta" };
// harmonics = voicing detent (snaps to PIANO_V[0..5]); timbre = hammer (0.5 = voicing default);
// morph = pedal. The 6 presets are the 6 voicings — the acceptance tests against navkit.
static const float PRESET[NPRESET][3] = {
    { 0.08f, 0.50f, 0.62f },   // grand
    { 0.25f, 0.55f, 0.55f },   // bright
    { 0.42f, 0.50f, 0.20f },   // harpsichord (hard plectrum, dry/short)
    { 0.58f, 0.50f, 0.50f },   // dulcimer
    { 0.75f, 0.50f, 0.30f },   // clavichord (soft, intimate)
    { 0.92f, 0.50f, 0.60f },   // celesta
};

static int   root = 60;        // middle C
static float amp[NKEY];        // visual key-press glow, decays each frame
static int   sel = 0;
static int   preset = 0;
static bool  autoplay = true;
static int   apos = 0;

#define NPTR 10
#define NOID (-999)
enum { PTR_IDLE, PTR_DRAG, PTR_KEY };
typedef struct { int id, mode, k, key; } Ptr;
static Ptr   ptr[NPTR];

// keyboard geometry
#define KB_Y   44
#define KB_H   78
#define WKEYS  8                    // white keys across
#define WW     ((SCREEN_W - 20) / WKEYS)
// slider geometry
#define KNOB_W    88
#define KNOB_TOP  (SCREEN_H - 52)            // row 1 y; row 2 sits 26px below
#define KNOB_X(k) (14 + ((k) % 3) * 102)
#define KNOB_Y(k) (KNOB_TOP + ((k) < 3 ? 0 : 26))

// white-key index (0..7) → its NKEY slot; and x of white key i
static int white_slot(int wi) { int c = 0; for (int i = 0; i < NKEY; i++) if (!BLACK[i]) { if (c == wi) return i; c++; } return 0; }
static int white_x(int wi)    { return 10 + wi * WW; }

static float knob[6] = { 0.08f, 0.50f, 0.62f, 0.0f, 0.3f, 0.0f };   // grand voicing + a little hammer thump (attack)

static void push_knobs(void) {
    instrument_harmonics(I_PNO, knob[0]);
    instrument_timbre(I_PNO, knob[1]);
    instrument_morph(I_PNO, knob[2]);
    eng_tune(I_PNO, 0, knob[3]);   // TUNING: fundamental weight
    eng_tune(I_PNO, 1, knob[4]);   // TUNING: attack click
}

// gate scales with pedal (morph): dry staccato lets go fast, a held pedal rings long
static int gate_ms(void) { return 500 + (int)(knob[2] * knob[2] * 16000.0f); }

static void play_key(int slot, int vol) {
    instrument_pan(I_PNO, (SEMI[slot] / 12.0f - 0.5f) * 1.8f * knob[5]);   // TUNING: width = pan by pitch
    hit(root + SEMI[slot], I_PNO, vol, gate_ms());
    amp[slot] = 1.0f;
}

static void load_preset(int p) {
    preset  = p;
    knob[0] = PRESET[p][0]; knob[1] = PRESET[p][1]; knob[2] = PRESET[p][2];
    push_knobs();
}

void init(void) {
    instrument(I_PNO, INSTR_PIANO, 1, 0, 7, 2000);   // long release — never chop a ringing note
    push_knobs();
    for (int i = 0; i < NPTR; i++) ptr[i].id = NOID;
    bpm(96);
}

void update(void) {
    for (int i = 0; i < NKEY; i++) if (keyp(KEYK[i])) play_key(i, 6);

    for (int p = 0; p < NPRESET; p++)
        if (keyp('1' + p)) { load_preset(p); play_key(0, 6); play_key(4, 5); play_key(7, 5); }

    if (keyp(KEY_LEFT))  sel = (sel + 5) % 6;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % 6;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        knob[sel] = clamp(knob[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        push_knobs();
        if (frame() % 14 == 0) play_key(4, 5);
    }

    if (keyp(KEY_SPACE)) { play_key(0, 6); play_key(4, 5); play_key(7, 5); }   // a triad
    if (keyp('M')) autoplay = !autoplay;
    if (keyp('Z') && root > 24) root -= 12;   // octave down
    if (keyp('X') && root < 96) root += 12;   // octave up

    // touch: each finger plays a key or drags a slider, independently
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        Ptr *p = 0, *freeP = 0;
        for (int j = 0; j < NPTR; j++) {
            if (ptr[j].id == id) { p = &ptr[j]; break; }
            if (ptr[j].id == NOID && !freeP) freeP = &ptr[j];
        }
        if (!p) {
            if (!freeP) continue;
            p = freeP; *p = (Ptr){ id, PTR_IDLE, -1, -1 };
            if (point_in_box(tx, ty, SCREEN_W - 112, 2, 108, 12)) { autoplay = !autoplay; continue; }
            for (int k = 0; k < 6; k++)
                if (point_in_box(tx, ty, KNOB_X(k) - 2, KNOB_Y(k) - 6, KNOB_W + 4, 18)) { p->mode = PTR_DRAG; p->k = sel = k; }
            if (p->mode == PTR_IDLE && ty >= KB_Y && ty < KB_Y + KB_H) {
                // black keys sit on top (upper 60% of the keybed) — test them first
                int hitk = -1;
                for (int wi = 0; wi < WKEYS && hitk < 0; wi++) {
                    int slot = white_slot(wi);
                    if (slot + 1 < NKEY && BLACK[slot + 1] && ty < KB_Y + KB_H * 6 / 10) {
                        int bx = white_x(wi) + WW - WW / 4;
                        if (tx >= bx && tx < bx + WW / 2) hitk = slot + 1;
                    }
                }
                if (hitk < 0)                                   // else a white key
                    for (int wi = 0; wi < WKEYS; wi++)
                        if (tx >= white_x(wi) && tx < white_x(wi) + WW) hitk = white_slot(wi);
                if (hitk >= 0) { p->mode = PTR_KEY; p->key = hitk; play_key(hitk, 6); }
            }
        } else if (p->mode == PTR_DRAG) {
            knob[p->k] = clamp((float)(tx - KNOB_X(p->k)) / (float)KNOB_W, 0.0f, 1.0f);
            push_knobs();
            if (frame() % 14 == 0) play_key(4, 5);
        }
    }
    for (int i = 0; i < touch_ended_count(); i++)
        for (int j = 0; j < NPTR; j++)
            if (ptr[j].id == touch_ended_id(i)) ptr[j].id = NOID;

    // autoplay: a gentle arpeggiated noodle
    if (autoplay && every(1)) {
        static const int seq[16] = { 0,4,7,4, 9,7,4,0, 2,5,9,5, 7,4,2,0 };
        play_key(seq[apos % 16], 5);
        if (chance(30)) schedule_hit(280, root + SEMI[(seq[apos % 16] + 7) % NKEY] + 12, I_PNO, 3, 1500);
        apos++;
    }

#ifdef DE_TRACE
    watch("harm", "%.2f", knob[0]);
    watch("timb", "%.2f", knob[1]);
    watch("mor",  "%.2f", knob[2]);
#endif
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    print("PIANO", 8, 6, CLR_WHITE);
    font(FONT_SMALL);
    print("stiff-string (StifKarp) engine", 60, 8, CLR_MEDIUM_GREY);
    print_right(autoplay ? "M autoplay: on" : "M autoplay: off", SCREEN_W - 10, 8, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // white keys
    for (int wi = 0; wi < WKEYS; wi++) {
        int slot = white_slot(wi), x = white_x(wi);
        amp[slot] *= 0.90f;
        int c = amp[slot] > 0.1f ? CLR_LIGHT_YELLOW : CLR_WHITE;
        rectfill(x + 1, KB_Y, WW - 2, KB_H, c);
        rect(x, KB_Y, WW, KB_H, CLR_DARK_GREY);
        print(str("%c", KEYK[slot]), x + WW / 2 - 3, KB_Y + KB_H - 12, CLR_DARK_GREY);
    }
    // black keys on top
    for (int wi = 0; wi < WKEYS; wi++) {
        int slot = white_slot(wi);
        if (slot + 1 < NKEY && BLACK[slot + 1]) {
            int bx = white_x(wi) + WW - WW / 4, bs = slot + 1;
            amp[bs] *= 0.90f;
            int c = amp[bs] > 0.1f ? CLR_ORANGE : CLR_BLACK;
            rectfill(bx, KB_Y, WW / 2, KB_H * 6 / 10, c);
            rect(bx, KB_Y, WW / 2, KB_H * 6 / 10, CLR_DARKER_GREY);
            print(str("%c", KEYK[bs]), bx + 1, KB_Y + 3, CLR_LIGHT_GREY);
        }
    }

    // two rows of knobs: row 1 = macros, row 2 = weight / attack / width (tuning)
    for (int k = 0; k < 6; k++) {
        int x = KNOB_X(k), y = KNOB_Y(k);
        bool on = (k == sel);
        font(FONT_SMALL);
        print(KNOB_NAME[k], x, y - 8, on ? CLR_YELLOW : k < 3 ? CLR_MEDIUM_GREY : CLR_DARK_GREY);
        font(FONT_NORMAL);
        bar(x, y, KNOB_W, 6, knob[k], on ? CLR_ORANGE : k < 3 ? CLR_BROWN : CLR_DARKER_GREY, CLR_DARKER_GREY);
        if (on) print(">", x - 9, y - 1, CLR_YELLOW);
    }

    // preset row (1..6) — the named acceptance tests
    font(FONT_TINY);
    int prx = 10;
    for (int p = 0; p < NPRESET; p++)
        prx = print(str("%d %s  ", p + 1, PRESET_NAME[p]), prx, KB_Y + KB_H + 6,
                    p == preset ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    print(str("A..K play   Z/X octave (C%d)   1..6 preset   drag slider   SPACE chord", root / 12 - 1),
          10, SCREEN_H - 9, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
