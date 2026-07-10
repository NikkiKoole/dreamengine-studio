/* de:meta
{
  "slug": "spacecho",
  "title": "re-201 space echo",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling"
  ],
  "lineage": "Fourth in the classic-machine family (tb303, tr909, sh101); models the Roland RE-201 (1974) tape echo — feedback self-oscillation, Doppler pitch-bend on RATE sweep, darkening loop filter — as an instrument showcase for the echo bus.",
  "homage": "Roland RE-201 Space Echo (1974)",
  "description": "Roland's RE-201 (1974) — a free-running tape loop, a record head, and an INTENSITY knob that feeds the tape back into itself; the box that made echo an instrument. Showcase cart for THE echo bus (the engine has exactly one, by design): a guitar string (INSTR_PLUCK) plays into the chamber, and five knobs map straight onto the new API — RATE is echo() time and the headline trick: sweep it while a tail rings and the repeats PITCH-BEND, because the bus's read tap glides like varying tape speed. INT is feedback, and past the red mark (>1.0) the loop genuinely self-oscillates — a tanh inside the feedback path saturates the runaway like tape instead of exploding; crank it and play the machine itself with the RATE knob. TONE is the loop filter (every repeat passes through it once, so tails get darker pass by pass — the thing scheduled-note echo could never do), ECHO is instrument_echo() send, DRV the pushed-tube input. SPACE toggles a sparse dub phrase that THROWS one note per bar into the echo with the send cranked for that single hit — the signature dub move. ZXCVBNM,./ + SDGHJL; play, UP/DOWN octave, knobs drag or wheel, H help."
}
de:meta */
#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND
#include <stdio.h>
#include <string.h>
#include <math.h>

// ── RE-201 SPACE ECHO ─────────────────────────────────────────────────────
// Roland's RE-201 (1974): a free-running tape loop in a chamber, a record
// head, playback heads, and an INTENSITY knob that feeds the tape back into
// itself. Reggae, dub, Pink Floyd, half of every guitar record since — the
// echo wasn't an effect on the music, it WAS the music. Fourth box in the
// classic-machine family (tb303.c, tr909.c, sh101.c), and the showcase cart
// for THE echo bus (audio-notes §17 step 3): the effect is the instrument.
//
// How the real one works, and how this cart maps it onto the API:
//
//   the tape loop — one shared delay line. Here: the engine's one echo bus,
//     echo(time_ms, feedback, tone). Every repeat passes through the loop
//     filter once, so tails genuinely get darker with each pass.
//   REPEAT RATE — tape speed: the head distance is fixed, so slower tape =
//     longer delay. Sweeping it while a tail rings PITCH-BENDS the repeats
//     (the read tap glides, exactly like varying tape speed). Grab the RATE
//     knob mid-tail. That's the sound of every dub siren ever.
//   INTENSITY — feedback. Past the red mark (>1.0) the loop self-oscillates:
//     a tanh inside the loop saturates the runaway like tape instead of
//     exploding. Crank it, ride RATE, play the machine itself.
//   TONE — the loop filter, 300Hz..7kHz. Dark dub tails live at the bottom.
//   ECHO — the send: instrument_echo(SLOT, x). Dry stays put; this is how
//     much of each note feeds the tape.
//   DRV — instrument_drive() on the input: the pushed-tube front end.
//
//   KEYS    Z X C V B N M , . / white + S D G H J L ; black — a guitar
//           (INSTR_PLUCK) playing into the chamber. UP/DOWN shift octave.
//   POINTER knobs: drag vertically (or hover + wheel on desktop). Play the
//           keys too. MULTITOUCH: every finger is its own pointer — ride RATE
//           and INT with two fingers while a third plays the keys; the header
//           labels (< OCT >, DUB, H) are tap targets. The desktop mouse
//           arrives as one synthetic finger, same code path.
//   SPACE   DUB mode — a sparse minor-pentatonic phrase plays itself; every
//           few bars one note gets THROWN into the echo (send cranked for
//           that single hit — the signature dub move).
//   H       help panel.

#define SLOT 8
#define STEPS 16

// knobs — named indices (house rule: never address param arrays with raw numbers)
enum { K_RATE, K_INT, K_ECHO, K_TONE, K_DRV, NK };
static const char *KNAME[NK] = { "RATE", "INT", "ECHO", "TONE", "DRV" };
static int knob[NK] = { 40, 48, 55, 45, 20 };   // all 0..100

static int   base = 48;            // keyboard octave root (C3)
static bool  dub  = true;          // SPACE: the self-playing phrase (on at boot, like tb303 runs)
static bool  show_help;
static int   last16 = -1, playhead = 0;

// per-finger pointer table — every finger drags its own knob or plays the keys
enum { PTR_IDLE, PTR_KNOB, PTR_KEY };
typedef struct { int id, mode, k, lastY, semi; } Ptr;   // id MUST be first (pointer.h)
static Ptr ptr[PTR_MAX];           // .id == PTR_NONE → slot free
static float vu = 0.0f;            // cosmetic echo-level meter
static float tape_ph = 0.0f;       // tape-loop animation phase
static int   thrown = -1;          // step whose hit was thrown into the echo (flash)

// ── knob value mappings ──────────────────────────────────────────────────
static int   rate_ms(void) { return 60 + knob[K_RATE] * 94 / 10; }     // 60..1000ms
static float fb_x(void)    { return knob[K_INT] * 0.011f; }            // 0..1.1 — past 1.0 = self-osc
static float send_x(void)  { return knob[K_ECHO] / 100.0f; }
static float tone_x(void)  { return knob[K_TONE] / 100.0f; }
static float drv_x(void)   { return knob[K_DRV] / 100.0f; }

static void apply_bus(void)   { echo(rate_ms(), fb_x(), tone_x()); }
static void apply_voice(void) {
    instrument(SLOT, INSTR_PLUCK, 1, 0, 7, 150);   // a real string into the chamber
    instrument_harmonics(SLOT, 0.72f);             // good ring
    instrument_timbre(SLOT, 0.55f);
    instrument_drive(SLOT, drv_x());
    instrument_echo(SLOT, send_x());
}

static void knob_changed(int k) {
    if (k == K_RATE || k == K_INT || k == K_TONE) apply_bus();
    if (k == K_ECHO) instrument_echo(SLOT, send_x());
    if (k == K_DRV)  instrument_drive(SLOT, drv_x());
}

// ── the keyboard (sh101.c's lower manual) ────────────────────────────────
static const struct { char k; int semi; } KMAP[] = {
    { 'Z', 0 }, { 'X', 2 }, { 'C', 4 }, { 'V', 5 }, { 'B', 7 }, { 'N', 9 },
    { 'M', 11 }, { ',', 12 }, { '.', 14 }, { '/', 16 },
    { 'S', 1 }, { 'D', 3 }, { 'G', 6 }, { 'H', 8 }, { 'J', 10 }, { 'L', 13 }, { ';', 15 },
};
#define NKMAP ((int)(sizeof KMAP / sizeof KMAP[0]))
#define NWHITE 10
static const int  W_SEMI[NWHITE] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16 };
static const char WLBL[NWHITE + 1] = "ZXCVBNM,./";
#define NBLACK 7
static const int  B_SEMI[NBLACK]  = { 1, 3, 6, 8, 10, 13, 15 };
static const int  B_AFTER[NBLACK] = { 0, 1, 3, 4, 5, 7, 8 };
static const char BLBL[NBLACK + 1] = "SDGHJL;";
static int key_flash[NKMAP];       // frames remaining of pressed highlight

static void play_note(int midi) {
    hit(midi, SLOT, 6, 220);
    vu += 0.2f + send_x() * 0.5f;
    if (vu > 1.0f) vu = 1.0f;
}

// ── the dub phrase — sparse minor pentatonic, one throw per pass ─────────
// '.' rest; digits index the pentatonic pool. The throw step ('!') plays the
// pool[2] note with the send cranked to 1.0 for that single hit.
static const char DUBP[STEPS + 1] = "0...3.1...!...2.";
static const int  POOL[5] = { 0, 3, 5, 7, 10 };

void init(void) {
    bpm(74);
    PTR_CLEAR(ptr);
    apply_voice();
    apply_bus();
}

// ── layout ───────────────────────────────────────────────────────────────
static const int KX[NK] = { 36, 96, 156, 216, 276 };
#define KY 44
#define KR 12
#define KBX 10
#define KBY 138
#define KBW 30
#define KBH 54

// which semitone is under canvas point x,y? -1 = none (black keys sit on top)
static int key_at(int x, int y) {
    if (y < KBY || y >= KBY + KBH) return -1;
    for (int j = 0; j < NBLACK; j++) {
        int bx = KBX + B_AFTER[j] * KBW + KBW - 8;
        if (y < KBY + 32 && x >= bx && x < bx + 16) return B_SEMI[j];
    }
    if (x >= KBX && x < KBX + NWHITE * KBW) return W_SEMI[(x - KBX) / KBW];
    return -1;
}
static void flash_semi(int s) {
    for (int k = 0; k < NKMAP; k++)
        if (KMAP[k].semi == s) { key_flash[k] = 6; break; }
}

void update(void) {
    if (keyp(KEY_SPACE)) { dub = !dub; last16 = -1; }
    if (keyp(KEY_UP))   { base += 12; if (base > 72) base = 72; }
    if (keyp(KEY_DOWN)) { base -= 12; if (base < 24) base = 24; }

    // computer keys → the string
    for (int i = 0; i < NKMAP; i++)
        if (keyp(KMAP[i].k)) { play_note(base + KMAP[i].semi); key_flash[i] = 6; }
    for (int i = 0; i < NKMAP; i++)
        if (key_flash[i] > 0) key_flash[i]--;

    int mx = mouse_x(), my = mouse_y();

    if (show_help) {
        if (keyp('H') || tapp(0, 0, 320, 200)) show_help = false;   // any tap closes
        goto clock;
    }
    if (keyp('H') || tapp(304, 0, 16, 20)) show_help = true;

    // tappable header: < OCT > halves shift octave, DUB toggles the phrase
    if (tapp(200, 0, 32, 20)) { base -= 12; if (base < 24) base = 24; }
    if (tapp(232, 0, 30, 20)) { base += 12; if (base > 72) base = 72; }
    if (tapp(262, 0, 42, 20)) { dub = !dub; last16 = -1; }

    // touch: every finger is its own pointer — drag a knob (vertically) or
    // play the keys, all at once (the desktop mouse = one synthetic finger;
    // hover + wheel below still nudges knobs on desktop)
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;                              // pool full (>PTR_MAX fingers)
        if (fresh) {                                   // finger just landed
            *p = (Ptr){ id, PTR_IDLE, -1, ty, -1 };
            for (int k = 0; k < NK; k++) {
                int dx = tx - KX[k], dy = ty - KY;
                if (dx * dx + dy * dy <= (KR + 3) * (KR + 3)) { p->mode = PTR_KNOB; p->k = k; }
            }
            if (p->mode == PTR_IDLE) {
                int s = key_at(tx, ty);
                if (s >= 0) { play_note(base + s); flash_semi(s); p->mode = PTR_KEY; p->semi = s; }
            }
        } else if (p->mode == PTR_KNOB) {
            if (ty != p->lastY) {
                knob[p->k] += (p->lastY - ty) * 2;
                if (knob[p->k] < 0)   knob[p->k] = 0;
                if (knob[p->k] > 100) knob[p->k] = 100;
                knob_changed(p->k);
            }
        } else if (p->mode == PTR_KEY) {
            int s = key_at(tx, ty);                    // slide across keys = glissando
            if (s >= 0 && s != p->semi) { play_note(base + s); flash_semi(s); p->semi = s; }
        }
        p->lastY = ty;
    }
    for (int i = 0; i < touch_ended_count(); i++) {    // lifted fingers free their slot
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (p) p->id = PTR_NONE;
    }

    float wh = mouse_wheel();
    if (wh != 0.0f)
        for (int k = 0; k < NK; k++) {
            int dx = mx - KX[k], dy = my - KY;
            if (dx * dx + dy * dy <= (KR + 3) * (KR + 3)) {
                knob[k] += wh > 0 ? 4 : -4;
                if (knob[k] < 0)   knob[k] = 0;
                if (knob[k] > 100) knob[k] = 100;
                knob_changed(k);
            }
        }

clock:
    // tape animation: fixed head distance, so longer delay = slower tape
    tape_ph += 14.0f / (float)rate_ms();
    if (tape_ph >= 1.0f) tape_ph -= 1.0f;

    // cosmetic VU: decays at the rate a repeat actually decays (fb per rate_ms)
    vu *= powf(fb_x() > 0.01f ? fb_x() : 0.01f, 16.7f / (float)rate_ms());
    if (fb_x() > 1.0f && vu < 0.35f) vu = 0.35f;   // self-osc floor: the loop is alive

    if (!dub) return;
    int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
    int s   = s16 & 15;
    if (s16 != last16) {
        last16 = s16;
        playhead = s;
        char c = DUBP[s];
        if (c == '!') {                       // THE THROW: this one hit, full send
            instrument_echo(SLOT, 1.0f);
            play_note(base + POOL[2] + 12);
            instrument_echo(SLOT, send_x());  // back to the knob before the next note
            thrown = 20;
        } else if (c != '.') {
            play_note(base + POOL[c - '0']);
        }
    }
    if (thrown >= 0) thrown--;
}

void draw(void) {
    char buf[40];

    // the RE-201 face: grey-green box, dark header
    cls(CLR_MEDIUM_GREY);
    rectfill(0, 0, 320, 20, CLR_BROWNISH_BLACK);
    print("ROLAND RE-201 SPACE ECHO", 8, 7, CLR_LIGHT_PEACH);
    font(FONT_SMALL);                                  // tap halves of OCT to shift it
    print("<", 203, 9, CLR_DARK_GREY);
    print(">", 259, 9, CLR_DARK_GREY);
    font(FONT_NORMAL);
    sprintf(buf, "OCT C%d", base / 12 - 1);
    print(buf, 210, 7, CLR_MEDIUM_GREY);
    print(dub ? "DUB >" : "DUB #", 264, 7, dub ? CLR_GREEN : CLR_DARK_GREY);   // tappable
    print("H", 310, 7, CLR_DARK_GREY);                                         // tappable

    // knobs
    for (int k = 0; k < NK; k++) {
        // INTENSITY: paint the self-osc zone red on the dial ring
        circfill(KX[k], KY, KR, CLR_BLACK);
        circ(KX[k], KY, KR, k == K_INT && fb_x() > 1.0f ? CLR_RED : CLR_DARK_GREY);
        float a = (-135.0f + 270.0f * knob[k] / 100.0f) * 0.0174533f - 1.5708f;
        line(KX[k], KY, KX[k] + (int)(cosf(a) * (KR - 2)), KY + (int)(sinf(a) * (KR - 2)), CLR_WHITE);
        print(KNAME[k], KX[k] - (int)strlen(KNAME[k]) * 4, KY + KR + 5, CLR_BLACK);
    }
    // knob readouts
    sprintf(buf, "%dMS", rate_ms());
    print(buf, KX[K_RATE] - 14, KY - KR - 12, CLR_BROWNISH_BLACK);
    if (fb_x() > 1.0f) print("REGEN!", KX[K_INT] - 22, KY - KR - 12, (beat() & 1) ? CLR_RED : CLR_ORANGE);

    // ── the tape chamber ─────────────────────────────────────────────────
    rectfill(10, 76, 196, 54, CLR_BROWNISH_BLACK);
    rect(10, 76, 196, 54, CLR_DARK_GREY);
    print("TAPE", 16, 81, CLR_DARK_GREY);
    // the free-running loop: a static oval path + bright splices riding it at
    // tape speed (longer delay = slower tape, like the real fixed-head machine)
    for (int i = 0; i < 48; i++) {
        float a = i / 48.0f * 6.2831853f;
        pset(108 + (int)(cosf(a) * 78.0f), 103 + (int)(sinf(a) * 16.0f), CLR_DARK_BROWN);
    }
    for (int i = 0; i < 3; i++) {
        float t = tape_ph + i / 3.0f;
        t -= (int)t;
        float a = t * 6.2831853f;
        int x = 108 + (int)(cosf(a) * 78.0f);
        int y = 103 + (int)(sinf(a) * 16.0f);
        rectfill(x - 1, y - 1, 3, 3, i == 0 ? CLR_WHITE : CLR_ORANGE);
    }
    // heads: record (left) + playback (right) — fixed; the tape moves past them
    rectfill(56, 96, 5, 12, CLR_LIGHT_GREY);   print("R", 57, 86, CLR_MEDIUM_GREY);
    rectfill(156, 96, 5, 12, CLR_LIGHT_GREY);  print("P", 157, 86, CLR_MEDIUM_GREY);

    // ── echo level VU ────────────────────────────────────────────────────
    rectfill(216, 76, 94, 54, CLR_BROWNISH_BLACK);
    rect(216, 76, 94, 54, CLR_DARK_GREY);
    print("ECHO LEVEL", 224, 81, CLR_DARK_GREY);
    int bars = (int)(vu * 10.0f + 0.5f);
    for (int i = 0; i < 10; i++) {
        int c = i >= bars ? CLR_DARKER_GREY : i >= 8 ? CLR_RED : i >= 6 ? CLR_ORANGE : CLR_GREEN;
        rectfill(224 + i * 8, 96, 6, 24, c);
    }
    if (thrown > 0) print("THROW!", 240, 122, (thrown & 2) ? CLR_YELLOW : CLR_ORANGE);

    // ── the keyboard ─────────────────────────────────────────────────────
    for (int i = 0; i < NWHITE; i++) {
        int x = KBX + i * KBW;
        bool lit = false;
        for (int k = 0; k < NKMAP; k++)
            if (key_flash[k] > 0 && KMAP[k].semi == W_SEMI[i]) lit = true;
        rectfill(x, KBY, KBW - 2, KBH, lit ? CLR_YELLOW : CLR_WHITE);
        sprintf(buf, "%c", WLBL[i]);
        print(buf, x + KBW / 2 - 4, KBY + KBH - 12, CLR_MEDIUM_GREY);
    }
    for (int j = 0; j < NBLACK; j++) {
        int x = KBX + B_AFTER[j] * KBW + KBW - 8;
        bool lit = false;
        for (int k = 0; k < NKMAP; k++)
            if (key_flash[k] > 0 && KMAP[k].semi == B_SEMI[j]) lit = true;
        rectfill(x, KBY, 16, 32, lit ? CLR_YELLOW : CLR_BLACK);
        sprintf(buf, "%c", BLBL[j]);
        print(buf, x + 5, KBY + 22, CLR_MEDIUM_GREY);
    }
    if (show_help) {
        rectfill(30, 26, 260, 150, CLR_BLACK);
        rect(30, 26, 260, 150, CLR_LIGHT_GREY);
        print("RE-201 CONTROLS", 100, 34, CLR_YELLOW);
        static const char *HL[] = {
            "KEYS      PLAY THE STRING",
            "RATE      TAPE SPEED 60..1000MS",
            "          SWEEP IT MID-TAIL: THE",
            "          REPEATS PITCH-BEND",
            "INT       FEEDBACK. PAST THE RED",
            "          MARK IT SELF-OSCILLATES",
            "ECHO      SEND INTO THE TAPE",
            "TONE      DARK..BRIGHT REPEATS",
            "DRV       INPUT TUBE GRIT",
            "SPACE     DUB PHRASE + THROWS",
            "UP/DOWN   OCTAVE (TAP < OCT >)",
        };
        for (int i = 0; i < 11; i++)
            print(HL[i], 42, 48 + i * 11, i < 9 ? CLR_WHITE : CLR_LIGHT_PEACH);
    }
}
