/* de:meta
{
  "slug": "aquapuss",
  "title": "aqua-puss analog delay",
  "status": "active",
  "created": "2026-07-19",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling"
  ],
  "lineage": "Sibling of spacecho (RE-201): both play THE tape-delay DSP as the instrument, but this one uses the IN-LINE pedal echo_insert() instead of the echo() send bus — an honest stompbox in the master fx_order chain, not a parallel send.",
  "homage": "Way Huge Aqua-Puss (MN3005 bucket-brigade analog delay pedal)",
  "description": "The Way Huge Aqua-Puss as a playable stompbox — a clean jangly guitar strummed straight through an in-line analog delay. Three knobs like the real MN3005 pedal: DELAY (opened up to 8..800ms on an exponential sweep — wider than the real 20..300ms BBD range for play: short = a resonant comb / doubler, the classic Aqua-Puss slap-to-echo window sits mid-throw, long = ambient washes; the BBD time-darkening makes the long end get darker on its own), FEEDBACK (repeat count; past the RED mark >1.0 the loop self-oscillates into a saturated space-warp howl, not a blow-up — the Aqua-Puss's famous runaway, and a tanh in the feedback loop bounds it like a real BBD), and BLEND (dry/wet mix). What makes it sound analog and not digital: every repeat passes once through a one-pole lowpass INSIDE the feedback loop, and the read tap is fractional + slews — so twist DELAY while a tail rings and the repeats PITCH-BEND like varying tape speed. The Aqua-Puss is famous for BRIGHT, jangly repeats (not the usual dark analog mush), so the base tone sits high. The real analog secret is echo_insert_bbd(): the repeats gently WOBBLE (bucket-brigade clock wow + flutter) and a longer DELAY darkens the tail — colouring ONLY the echoes, never the dry guitar. Press B to A/B that analog voice against a clean digital delay. Stomp the FOOTSWITCH (SPACE or tap) to bypass. Standard GarageBand musical-typing keybed — ASDFGHJKL white keys, WETYUO black — plays the guitar, Z/X shift octave, R toggles an auto-riff so you hear the repeats immediately, knobs drag or wheel, ? for help."
}
de:meta */
#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND
#include <stdio.h>
#include <string.h>
#include <math.h>

// ── WAY HUGE AQUA-PUSS ─────────────────────────────────────────────────────
// A bucket-brigade (MN3005 BBD) analog delay guitar pedal. Three knobs: DELAY,
// FEEDBACK, BLEND. Famous for BRIGHT, jangly repeats (unlike most dark analog
// delays) and a feedback control that self-oscillates into a "space-and-time
// warping" howl at extreme settings. The real one's delay range is 20..300ms; we
// open it to 8..800ms (exp) so it reaches from a resonant comb up to ambient washes.
//
// Sibling of spacecho.c (the RE-201 tape echo): both PLAY the engine's one
// tape-delay DSP as the instrument, but where spacecho uses the parallel SEND
// bus echo(), a stompbox is an IN-LINE pedal — so this uses echo_insert(), the
// same DSP placed IN the master fx_order chain (its own buffer). The guitar
// plays DRY into master; the insert delays the whole master mix, exactly like
// plugging a guitar into a pedal.
//
// How the pedal maps onto the API — and why it sounds analog, not digital:
//
//   DELAY    echo_insert() time_ms, 8..800ms exp (wider than the real 20..300ms
//            MN3005 for play — short = resonant comb, long = ambient). The
//            read tap is fractional and SLEWS toward its target, so twisting
//            DELAY while a tail rings PITCH-BENDS the repeats — the BBD clock
//            speeding up / slowing down, exactly like varying tape speed.
//   FEEDBACK repeat count, 0..1.1. Past the RED mark (>1.0) the loop self-
//            oscillates: a tanh INSIDE the feedback path saturates the runaway
//            into a bounded howl instead of exploding — a real BBD's behaviour.
//   BLEND    dry/wet mix. The wet repeats sit on top of the full dry signal.
//   BBD (B)  echo_insert_bbd() — the ANALOG bucket-brigade voice. The repeats
//            gently WOBBLE (clock wow + flutter) and a longer DELAY makes the
//            tail DARKER (a real BBD's clock slows as delay lengthens). Both
//            colour ONLY the repeats — the dry guitar stays put. Toggle to A/B
//            the analog voice against a clean digital delay.
//   (tone)   base tone sits HIGH — the Aqua-Puss's bright, jangly repeat; the
//            BBD voice then darkens it with delay time.
//
//   KEYS     GarageBand musical typing (keybed.h standard): A S D F G H J K L
//            white, W E T Y U O black — a clean guitar (INSTR_GUITAR) into the
//            pedal. Z / X shift the octave down / up.
//   R        auto-riff: a jangly arpeggio plays itself so the repeats are
//            audible immediately (on at boot). SPACE = stomp the footswitch.
//   POINTER  knobs: drag vertically (or hover + wheel on desktop). Multitouch:
//            ride DELAY + FEEDBACK with two fingers while a third plays.
//   H        help panel.

#define SLOT 8
#define STEPS 16
#define TONE_BRIGHT 0.72f      // Aqua-Puss signature: bright, jangly repeats (loop LP sits high)

// knobs — named indices (house rule: never address param arrays with raw numbers)
enum { K_DELAY, K_FB, K_BLEND, NK };
static const char *KNAME[NK] = { "DELAY", "FEEDBACK", "BLEND" };
// all 0..100. Boots into a sustaining WASH, not slapback: a mid delay (~160ms,
// a 16th at 88bpm) so each repeat lands in the gap of the next, high feedback
// so repeats overlap and sustain instead of dying, and plenty of wet — the
// echoes fill the space and carry the flow forward. Pull DELAY down for tight
// slapback, FEEDBACK up to the red for the self-osc drone.
static int knob[NK] = { 65, 66, 58 };   // K_DELAY 65 ≈ 160ms (the wash); exp sweep, so mid-throw

static int   base = 60;            // keyboard octave root (C4 — GarageBand default octave)
static bool  bypass = false;       // the footswitch
static bool  riff   = true;        // R: the self-playing arpeggio (on at boot)
static bool  bbd    = true;        // B: the analog bucket-brigade voice (echo_insert_bbd) on/off
static bool  show_help;
static int   last16 = -1, playhead = 0;

// per-finger pointer table — every finger drags its own knob or plays the keys
enum { PTR_IDLE, PTR_KNOB, PTR_KEY };
typedef struct { int id, mode, k, lastY, semi; } Ptr;   // id MUST be first (pointer.h)
static Ptr ptr[PTR_MAX];           // .id == PTR_NONE → slot free

// the repeat visualizer: a ring of recent hits, each ticking down the chain
#define NREP 12
static float rep_age[NREP];        // 0 = just fired, counts up; <0 = empty slot
static int   rep_head = 0;

// ── knob value mappings ────────────────────────────────────────────────────
// 8..800ms, EXPONENTIAL (800/8 = 100×) so both ends stay playable: short = a
// resonant comb / doubler, the classic 20..300ms Aqua-Puss window sits mid-sweep,
// long = ambient echoes. Wider than the real MN3005 (20..300) on purpose — the
// BBD time-darkening makes the long end get darker on its own, like an overdriven
// bucket-brigade. Engine allows 1..2000ms (the 2s delay line).
static int   delay_ms(void)  { return (int)(8.0f * powf(100.0f, knob[K_DELAY] / 100.0f) + 0.5f); }
static float fb_x(void)      { return knob[K_FB] * 0.011f; }              // 0..1.1 — past 1.0 = self-osc
static float blend_x(void)   { return knob[K_BLEND] / 100.0f; }

static void apply_pedal(void) {
    // stomp OFF = mix 0 (true bypass). feedback/time keep updating so the tail
    // that was ringing when you stomped still glides if you sweep DELAY.
    echo_insert(delay_ms(), fb_x(), TONE_BRIGHT, bypass ? 0.0f : blend_x());
}
static void apply_voice(void) {
    instrument(SLOT, INSTR_GUITAR, 1, 0, 7, 900);   // a clean six-string into the pedal
    instrument_harmonics(SLOT, 0.55f);
    instrument_timbre(SLOT, 0.85f);                 // bright, jangly pick attack
    instrument_morph(SLOT, 0.15f);
    instrument_drive_mode(SLOT, DRIVE_SOFT);
    instrument_drive(SLOT, 0.10f);                  // a whisker of push, clean
}

// the analog bucket-brigade voice — echo_insert_bbd(). A real MN3005 BBD adds
// clock JITTER (wow + flutter → the repeats shimmer) and DARKENS as the delay
// lengthens (the clock slows, bandwidth drops). Both live INSIDE the delay loop
// / on the read tap, so ONLY the repeats are coloured — the dry guitar passes
// untouched (the thing the earlier master-tape() spike couldn't do). Toggle
// it (T) to A/B the clean digital delay against the analog voice by ear.
static void apply_bbd(void) {
    echo_insert_bbd(bbd ? 1.0f : 0.0f);   // 1 = full analog, 0 = clean digital (byte-identical)
}

static void knob_changed(int k) { (void)k; apply_pedal(); }   // any knob reconfigures the one pedal

// ── the keyboard (spacecho's manual) ───────────────────────────────────────
// GarageBand "musical typing" — the studio's standard QWERTY keybed (keybed.h):
// home row = white keys, the row above = black keys (gaps where a piano has none).
static const struct { char k; int semi; } KMAP[] = {
    { 'A', 0 }, { 'S', 2 }, { 'D', 4 }, { 'F', 5 }, { 'G', 7 }, { 'H', 9 },
    { 'J', 11 }, { 'K', 12 }, { 'L', 14 },
    { 'W', 1 }, { 'E', 3 }, { 'T', 6 }, { 'Y', 8 }, { 'U', 10 }, { 'O', 13 },
};
#define NKMAP ((int)(sizeof KMAP / sizeof KMAP[0]))
#define NWHITE 9
static const int  W_SEMI[NWHITE] = { 0, 2, 4, 5, 7, 9, 11, 12, 14 };
static const char WLBL[NWHITE + 1] = "ASDFGHJKL";
#define NBLACK 6
static const int  B_SEMI[NBLACK]  = { 1, 3, 6, 8, 10, 13 };
static const int  B_AFTER[NBLACK] = { 0, 1, 3, 4, 5, 7 };
static const char BLBL[NBLACK + 1] = "WETYUO";
static int key_flash[NKMAP];       // frames remaining of pressed highlight

static void fire_rep(void) {       // record a hit for the repeat visualizer
    rep_age[rep_head] = 0.0f;
    rep_head = (rep_head + 1) % NREP;
}
static void play_note(int midi) {
    hit(midi, SLOT, 6, 220);
    fire_rep();
}

// ── the auto-riff — a rolling wave that leaves the OFF-beats empty ──────────
// '.' rest; digits index the pool. Plays only on the 8ths; with the ~160ms
// delay + high feedback the repeats land on the skipped 16ths and fill them,
// so you hear a continuous cascade you didn't play — the delay carrying the
// flow. Bright major-add9 shimmer, the clean-guitar sound the pedal was for.
static const char RIFF[STEPS + 1] = "0.2.4.3.5.3.2.0.";
static const int  POOL[6] = { 0, 4, 7, 9, 11, 12 };

// ── layout ─────────────────────────────────────────────────────────────────
static const int KX[NK] = { 58, 160, 262 };
#define KY 46
#define KR 15
#define KBX 10
#define KBY 138
#define KBW 33
#define KBH 54

void init(void) {
    bpm(88);
    PTR_CLEAR(ptr);
    for (int i = 0; i < NREP; i++) rep_age[i] = -1.0f;
    apply_voice();
    apply_pedal();
    apply_bbd();
    // the delay pedal is the only insert in the master chain; the analog BBD
    // voice lives INSIDE it (echo_insert_bbd), not as a separate pedal
    int kinds[] = { FX_ECHO };
    fx_order(0, kinds, 1);
}

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
    if (keyp(KEY_SPACE)) { bypass = !bypass; apply_pedal(); }   // stomp
    if (keyp('R'))       { riff = !riff; last16 = -1; }
    if (keyp('B'))       { bbd = !bbd; apply_bbd(); }           // A/B analog vs clean digital
    if (keyp('X'))      { base += 12; if (base > 84) base = 84; }   // GarageBand octave keys
    if (keyp('Z'))      { base -= 12; if (base < 36) base = 36; }

    // computer keys → the guitar
    for (int i = 0; i < NKMAP; i++)
        if (keyp(KMAP[i].k)) { play_note(base + KMAP[i].semi); key_flash[i] = 6; }
    for (int i = 0; i < NKMAP; i++)
        if (key_flash[i] > 0) key_flash[i]--;

    int mx = mouse_x(), my = mouse_y();

    if (show_help) {
        if (keyp('/') || tapp(0, 0, 320, 200)) show_help = false;   // any tap closes
        goto clock;
    }
    if (keyp('/') || tapp(304, 0, 16, 20)) show_help = true;

    // tappable header: < OCT > halves shift octave (touch), RIFF toggles the arpeggio
    if (tapp(196, 0, 30, 20)) { base -= 12; if (base < 36) base = 36; }
    if (tapp(226, 0, 28, 20)) { base += 12; if (base > 84) base = 84; }
    if (tapp(256, 0, 40, 20)) { riff = !riff; last16 = -1; }

    // the footswitch (right box) is a big tap target too
    if (tapp(220, 76, 90, 54)) { bypass = !bypass; apply_pedal(); }
    // BBD nuance toggle sits in the (non-interactive) REPEATS box title row
    if (tapp(112, 78, 94, 12)) { bbd = !bbd; apply_bbd(); }

    // touch: every finger is its own pointer — drag a knob or play the keys
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;                              // pool full (>PTR_MAX fingers)
        if (fresh) {                                   // finger just landed
            *p = (Ptr){ id, PTR_IDLE, -1, ty, -1 };
            for (int k = 0; k < NK; k++) {
                int dx = tx - KX[k], dy = ty - KY;
                if (dx * dx + dy * dy <= (KR + 4) * (KR + 4)) { p->mode = PTR_KNOB; p->k = k; }
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
            if (dx * dx + dy * dy <= (KR + 4) * (KR + 4)) {
                knob[k] += wh > 0 ? 4 : -4;
                if (knob[k] < 0)   knob[k] = 0;
                if (knob[k] > 100) knob[k] = 100;
                knob_changed(k);
            }
        }

clock:
    // age the repeat visualizer (in delay-time units so it reads as the echo)
    for (int i = 0; i < NREP; i++)
        if (rep_age[i] >= 0.0f) {
            rep_age[i] += 1000.0f / 60.0f / (float)delay_ms();   // +1 per delay period
            if (rep_age[i] > (float)NREP) rep_age[i] = -1.0f;
        }

    if (!riff) return;
    int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
    int s   = s16 & 15;
    if (s16 != last16) {
        last16 = s16;
        playhead = s;
        char c = RIFF[s];
        if (c != '.') play_note(base + POOL[c - '0']);
    }
}

void draw(void) {
    char buf[40];

    // the pedal: teal enclosure, dark header strip
    cls(CLR_BLUE_GREEN);
    rectfill(0, 0, 320, 20, CLR_BROWNISH_BLACK);
    print("WAY HUGE  AQUA-PUSS", 8, 7, CLR_LIGHT_PEACH);
    font(FONT_SMALL);                                  // tap halves of OCT to shift it
    print("<", 199, 9, CLR_DARKER_BLUE);
    print(">", 249, 9, CLR_DARKER_BLUE);
    font(FONT_NORMAL);
    sprintf(buf, "OCT C%d", base / 12 - 1);
    print(buf, 206, 7, CLR_LIGHT_GREY);
    print(riff ? "RIFF>" : "RIFF#", 256, 7, riff ? CLR_GREEN : CLR_DARK_GREY);   // tappable
    print("?", 310, 7, CLR_LIGHT_GREY);                                          // tappable (help)

    // subtitle
    font(FONT_SMALL);
    print("ANALOG DELAY", 8, 25, CLR_DARKER_BLUE);
    font(FONT_NORMAL);

    // knobs
    for (int k = 0; k < NK; k++) {
        // FEEDBACK: paint the self-osc zone red on the dial when past 1.0
        bool osc = (k == K_FB && fb_x() > 1.0f);
        circfill(KX[k], KY, KR, CLR_BROWNISH_BLACK);
        circ(KX[k], KY, KR, osc ? CLR_RED : CLR_LIGHT_GREY);
        circ(KX[k], KY, KR - 1, osc ? CLR_RED : CLR_DARKER_BLUE);
        float a = (-135.0f + 270.0f * knob[k] / 100.0f) * 0.0174533f - 1.5708f;
        line(KX[k], KY, KX[k] + (int)(cosf(a) * (KR - 3)), KY + (int)(sinf(a) * (KR - 3)), CLR_WHITE);
        print(KNAME[k], KX[k] - (int)strlen(KNAME[k]) * 4, KY + KR + 5, CLR_LIGHT_PEACH);
    }
    // knob readouts
    sprintf(buf, "%dMS", delay_ms());
    print(buf, KX[K_DELAY] - 14, KY - KR - 12, CLR_LIGHT_PEACH);
    if (fb_x() > 1.0f) print("SELF-OSC!", KX[K_FB] - 34, KY - KR - 12, (beat() & 1) ? CLR_RED : CLR_ORANGE);
    sprintf(buf, "%d%%", knob[K_BLEND]);
    print(buf, KX[K_BLEND] - 8, KY - KR - 12, CLR_LIGHT_PEACH);

    // ── the repeat chain visualizer ──────────────────────────────────────
    rectfill(10, 76, 200, 54, CLR_DARKER_BLUE);
    rect(10, 76, 200, 54, CLR_BLUE_GREEN);
    print("REPEATS", 16, 81, CLR_BLUE);
    font(FONT_SMALL);                                  // tappable analog-nuance A/B
    print(bbd ? "BBD FLUTTER: ON" : "BBD FLUTTER: OFF", 114, 82, bbd ? CLR_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);
    // each recorded hit walks left→right across the box, one step per delay
    // period, dimming as feedback would decay it — or holding full & red in
    // self-oscillation (fb>1.0). Brightness stays high (the pedal is bright).
    for (int i = 0; i < NREP; i++) {
        if (rep_age[i] < 0.0f) continue;
        float age = rep_age[i];
        int x = 20 + (int)(age * (180.0f / (float)NREP));
        if (x < 16 || x > 202) continue;
        float fb = fb_x();
        float lvl = fb > 1.0f ? 1.0f : powf(fb > 0.02f ? fb : 0.02f, age);   // decay per repeat
        int h = 6 + (int)(lvl * 34.0f);
        int c = fb > 1.0f ? ((beat() & 1) ? CLR_RED : CLR_ORANGE)
              : lvl > 0.55f ? CLR_BLUE
              : lvl > 0.25f ? CLR_TRUE_BLUE : CLR_DARK_BLUE;
        rectfill(x, 122 - h, 3, h, c);
    }

    // ── the footswitch (tap target) ──────────────────────────────────────
    rectfill(220, 76, 90, 54, CLR_DARKER_BLUE);
    rect(220, 76, 90, 54, CLR_BLUE_GREEN);
    // status LED
    circfill(233, 88, 4, bypass ? CLR_DARKER_GREY : CLR_RED);
    circ(233, 88, 4, CLR_LIGHT_GREY);
    print(bypass ? "BYPASS" : "ON", 244, 84, bypass ? CLR_DARK_GREY : CLR_LIGHT_PEACH);
    // the stomp
    circfill(265, 110, 13, CLR_LIGHT_GREY);
    circfill(265, 110, 10, CLR_MEDIUM_GREY);
    font(FONT_SMALL);
    print("STOMP", 250, 108, CLR_BROWNISH_BLACK);
    font(FONT_NORMAL);

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
        print("AQUA-PUSS CONTROLS", 92, 34, CLR_YELLOW);
        static const char *HL[] = {
            "ASDFGHJKL WHITE KEYS / WETYUO BLACK",
            "DELAY     8..800MS. SWEEP IT",
            "          MID-TAIL: REPEATS BEND",
            "FEEDBACK  REPEAT COUNT. PAST THE",
            "          RED MARK IT SELF-OSC'S",
            "BLEND     DRY/WET MIX",
            "SPACE     STOMP (BYPASS)",
            "R         AUTO-RIFF ON/OFF",
            "B         BBD ANALOG VOICE (A/B)",
            "Z / X     OCTAVE DOWN / UP",
            "/         CLOSE THIS",
        };
        for (int i = 0; i < 11; i++)
            print(HL[i], 42, 48 + i * 11, i < 6 ? CLR_WHITE : CLR_LIGHT_PEACH);
    }
}
